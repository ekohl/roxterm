/*
    roxterm - PROFILE/GTK terminal emulator with tabs
    Copyright (C) 2019 Tony Houghton <h@realh.co.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "config.h"
#include "multitext-geometry-provider.h"
#include "roxterm-profile.h"

struct _RoxtermProfile {
    GObject parent_instance;
    char *name;
    char *filename;
    GKeyFile *key_file;
};

GHashTable *roxterm_profiles = NULL;

static void roxterm_profile_weak_ref_notify(gpointer handle, GObject *obj)
{
    (void) obj;
    g_hash_table_remove(roxterm_profiles, handle);
}

static void roxterm_profile_finalize(GObject *obj)
{
    RoxtermProfile *self = ROXTERM_PROFILE(obj);
    g_free(self->name);
    self->name = NULL;
    g_free(self->filename);
    self->filename = NULL;
}

static void roxterm_profile_dispose(GObject *obj)
{
    RoxtermProfile *self = ROXTERM_PROFILE(obj);
    if (self->key_file)
    {
        g_key_file_unref(self->key_file);
        self->key_file = NULL;
    }
}

G_DEFINE_TYPE(RoxtermProfile, roxterm_profile, G_TYPE_OBJECT);

enum {
    PROP_NAME = 1,
    N_PROPS
};

static GParamSpec *roxterm_profile_props[N_PROPS] = {NULL};

static void roxterm_profile_set_name(RoxtermProfile *self, const char *name)
{
    if (self->filename)
    {
        g_free(self->filename);
        self->filename = NULL;
    }
    gboolean add_to_hash = TRUE;
    if (self->name)
    {
        // If there was already a hash entry for the old name and it doesn't
        // point to self, this is a copy and we don't want to replace the
        // old hash entry
        gpointer hashed = g_hash_table_lookup(roxterm_profiles, name);
        if (hashed && hashed != self)
            add_to_hash = FALSE;
        g_free(self->name);
    }
    if (add_to_hash)
    {
        // Make another duplicate of the name because docs don't make it clear
        // at which stage of dispose/finalize the notification is emitted, so
        // the profile's name field may be invalid by then
        char *dup_name = g_strdup(name);
        g_hash_table_insert(roxterm_profiles, dup_name, self);
        g_object_weak_ref(G_OBJECT(self), roxterm_profile_weak_ref_notify,
                dup_name);
    }
    self->name = g_strdup(name);
}

static void roxterm_profile_set_property(GObject *obj, guint prop_id,
        const GValue *value, GParamSpec *pspec)
{
    RoxtermProfile *self = ROXTERM_PROFILE(obj);
    switch (prop_id)
    {
        case PROP_NAME:
            roxterm_profile_set_name(self, g_value_get_string(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
    }
}

static void roxterm_profile_get_property(GObject *obj, guint prop_id,
        GValue *value, GParamSpec *pspec)
{
    RoxtermProfile *self = ROXTERM_PROFILE(obj);
    switch (prop_id)
    {
        case PROP_NAME:
            g_value_set_string(value, self->name);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
    }
}

enum {
    ROXTERM_PROFILE_SIGNAL_STRING_CHANGED,
    ROXTERM_PROFILE_SIGNAL_INT_CHANGED,
    ROXTERM_PROFILE_SIGNAL_BOOLEAN_CHANGED,
    ROXTERM_PROFILE_SIGNAL_FLOAT_CHANGED,
    ROXTERM_PROFILE_SIGNAL_RGBA_CHANGED,

    ROXTERM_PROFILE_N_SIGNALS,
};

static guint roxterm_profile_signals[ROXTERM_PROFILE_N_SIGNALS];

#define ROXTERM_PROFILE_DEFINE_SIGNAL(ktype, signame, rtype, gtype) \
    roxterm_profile_signals[ROXTERM_PROFILE_SIGNAL_##rtype##_CHANGED] \
        = g_signal_new(signame "-changed", ktype, \
            G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, \
            0, NULL, NULL, NULL, \
            G_TYPE_NONE, 2, G_TYPE_STRING, gtype)

#define ROXTERM_PROFILE_DEFINE_GSIGNAL(ktype, signame, rtype, gtype) \
    ROXTERM_PROFILE_DEFINE_SIGNAL(ktype, signame, rtype, G_TYPE_##gtype)

static void roxterm_profile_class_init(RoxtermProfileClass *klass)
{
    GObjectClass *oklass = G_OBJECT_CLASS(klass);
    oklass->dispose = roxterm_profile_dispose;
    oklass->finalize = roxterm_profile_finalize;
    // Accessors must be set before installing properties
    oklass->set_property = roxterm_profile_set_property;
    oklass->get_property = roxterm_profile_get_property;
    roxterm_profile_props[PROP_NAME] =
            g_param_spec_string("name", "name",
            "Profile name", "Default",
            G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
    g_object_class_install_properties(oklass, N_PROPS, roxterm_profile_props);
    GType rpt = G_TYPE_FROM_CLASS(oklass);
    ROXTERM_PROFILE_DEFINE_GSIGNAL(rpt, "string", STRING, STRING);
    ROXTERM_PROFILE_DEFINE_GSIGNAL(rpt, "int", INT, INT);
    ROXTERM_PROFILE_DEFINE_GSIGNAL(rpt, "boolean", BOOLEAN, BOOLEAN);
    ROXTERM_PROFILE_DEFINE_GSIGNAL(rpt, "float", FLOAT, DOUBLE);
    ROXTERM_PROFILE_DEFINE_SIGNAL(rpt, "rgba", RGBA, ROXTERM_TYPE_RGBA);
}

static void roxterm_profile_init(RoxtermProfile *self)
{
    (void) self;
}

RoxtermProfile *roxterm_profile_new(const char *name)
{
    GObject *obj = g_object_new(ROXTERM_TYPE_PROFILE,
            "name", name,
            NULL);
    return ROXTERM_PROFILE(obj);
}

const char *roxterm_profile_get_user_directory(void)
{
    static char *user_dir = NULL;
    if (!user_dir)
        user_dir = g_build_path(g_get_user_config_dir(), PACKAGE, NULL);
    return user_dir;
}

// Constructs dir/roxterm4/name.ini where dir is an XDG config base directory
static char *roxterm_profile_build_filename(const char *dir, const char *name)
{
    char *leafname = g_strdup_printf("%s.ini", name);
    char *filename = g_build_path(dir, PACKAGE, leafname, NULL);
    g_free(leafname);
    return filename;
}

// Returns allocated file pathname if the file dir/roxterm4/name.ini exists,
// otherwise NULL
static char *roxterm_profile_check_directory(const char *dir, const char *name)
{
    char *filename = roxterm_profile_build_filename(dir, name);
    if (!g_file_test(filename, G_FILE_TEST_EXISTS))
    {
        g_free(filename);
        filename = NULL;
    }
    return filename;
}

void roxterm_profile_load(RoxtermProfile *self)
{
    if (self->key_file)
        return;
    self->key_file = g_key_file_new();
    const char *dir = g_get_user_config_dir();
    char *filename = self->filename = roxterm_profile_check_directory(dir,
            self->name);
    if (!filename)
    {
        char const * const *dirs = g_get_system_config_dirs();
        for (int n = 0; dirs[n]; ++n)
        {
            filename = roxterm_profile_check_directory(dirs[n], self->name);
            if (filename)
                break;
        }
    }
    if (filename)
    {
        GError *error = NULL;
        if (!g_key_file_load_from_file(self->key_file, filename,
                    G_KEY_FILE_KEEP_COMMENTS, &error))
        {
            g_critical("Error loading profile from '%s': %s", filename,
                    error->message);
            g_error_free(error);
        }
        if (filename != self->filename)
            g_free(filename);
    }
}

void roxterm_profile_save(RoxtermProfile *self)
{
    if (!self->filename)
    {
        const char *dir = roxterm_profile_get_user_directory();
        if (!g_file_test(dir, G_FILE_TEST_IS_DIR))
        {
            g_mkdir_with_parents(dir, 0755);
        }
        self->filename = roxterm_profile_build_filename(g_get_user_config_dir(),
                self->name);
    }
    GError *error = NULL;
    if (!g_key_file_save_to_file(self->key_file, self->filename, &error))
    {
        // TODO: It would be better to report this in the GUI, but it's
        // difficult to find the parent window for the dialog unless we
        // create an ugly GError-pseudo-exception chain
        g_critical("Error saving profile to '%s': %s", self->filename,
                error->message);
        g_error_free(error);
    }
}

char *roxterm_profile_get_string(RoxtermProfile *self, const char *key)
{
    roxterm_profile_load(self);
    return g_key_file_get_string(self->key_file, "strings", key, NULL);
}

void roxterm_profile_set_string(RoxtermProfile *self, const char *key,
        const char *value)
{
    roxterm_profile_load(self);
    g_key_file_set_string(self->key_file, "strings", key, value);
    roxterm_profile_save(self);
    g_signal_emit(self,
            roxterm_profile_signals[ROXTERM_PROFILE_SIGNAL_STRING_CHANGED],
            0, key, value);
}

int roxterm_profile_get_int(RoxtermProfile *self, const char *key)
{
    roxterm_profile_load(self);
    return g_key_file_get_integer(self->key_file, "ints", key, NULL);
}

void roxterm_profile_set_int(RoxtermProfile *self, const char *key, int value)
{
    roxterm_profile_load(self);
    g_key_file_set_integer(self->key_file, "ints", key, value);
    roxterm_profile_save(self);
    g_signal_emit(self,
            roxterm_profile_signals[ROXTERM_PROFILE_SIGNAL_INT_CHANGED],
            0, key, value);
}

gboolean roxterm_profile_get_boolean(RoxtermProfile *self, const char *key)
{
    roxterm_profile_load(self);
    return g_key_file_get_boolean(self->key_file, "booleans", key, NULL);
}

void roxterm_profile_set_boolean(RoxtermProfile *self, const char *key,
        gboolean value)
{
    roxterm_profile_load(self);
    g_key_file_set_boolean(self->key_file, "booleans", key, value);
    roxterm_profile_save(self);
    g_signal_emit(self,
            roxterm_profile_signals[ROXTERM_PROFILE_SIGNAL_BOOLEAN_CHANGED],
            0, key, value);
}

double roxterm_profile_get_float(RoxtermProfile *self, const char *key)
{
    roxterm_profile_load(self);
    return g_key_file_get_double(self->key_file, "floats", key, NULL);
}

void roxterm_profile_set_float(RoxtermProfile *self, const char *key,
        double value)
{
    roxterm_profile_load(self);
    g_key_file_set_double(self->key_file, "floats", key, value);
    roxterm_profile_save(self);
    g_signal_emit(self,
            roxterm_profile_signals[ROXTERM_PROFILE_SIGNAL_FLOAT_CHANGED],
            0, key, value);
}

RoxtermRGBA roxterm_profile_get_rgba(RoxtermProfile *self, const char *key)
{
    roxterm_profile_load(self);
    char *s = g_key_file_get_string(self->key_file, "colours", key, NULL);
    RoxtermRGBA rrgba = roxterm_rgba_parse(s);
    g_free(s);
    return rrgba;
}

void roxterm_profile_set_rgba(RoxtermProfile *self, const char *key,
        RoxtermRGBA value)
{
    char *s = roxterm_rgba_to_string(value);
    g_key_file_set_string(self->key_file, "colours", key, s);
    g_free(s);
    g_signal_emit(self,
            roxterm_profile_signals[ROXTERM_PROFILE_SIGNAL_RGBA_CHANGED],
            0, key, value);
}

RoxtermProfile *roxterm_profile_lookup(const char *name)
{
    if (!name)
        name = "Default";
    if (!roxterm_profiles)
        roxterm_profiles = g_hash_table_new_full(g_str_hash, g_str_equal,
                g_free, NULL);
    RoxtermProfile *profile = g_hash_table_lookup(roxterm_profiles, name);
    if (profile)
        g_object_ref(profile);
    else
        profile = roxterm_profile_new(name);
    return profile;
}

gboolean roxterm_profile_has_string(RoxtermProfile *self, const char *key)
{
    roxterm_profile_load(self);
    return g_key_file_has_key(self->key_file, "strings", key, NULL);
}

gboolean roxterm_profile_has_int(RoxtermProfile *self, const char *key)
{
    roxterm_profile_load(self);
    return g_key_file_has_key(self->key_file, "ints", key, NULL);
}

gboolean roxterm_profile_has_boolean(RoxtermProfile *self, const char *key)
{
    roxterm_profile_load(self);
    return g_key_file_has_key(self->key_file, "booleans", key, NULL);
}

gboolean roxterm_profile_has_float(RoxtermProfile *self, const char *key)
{
    roxterm_profile_load(self);
    return g_key_file_has_key(self->key_file, "floats", key, NULL);
}

gboolean roxterm_profile_has_rgba(RoxtermProfile *self, const char *key)
{
    roxterm_profile_load(self);
    return g_key_file_has_key(self->key_file, "colours", key, NULL);
}
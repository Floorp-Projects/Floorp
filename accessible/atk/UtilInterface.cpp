/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ApplicationAccessibleWrap.h"
#include "mozilla/Likely.h"
#include "nsAccessibilityService.h"
#include "nsMai.h"

#include <atk/atk.h>
#include <gtk/gtk.h>
#include <string.h>

using namespace mozilla;
using namespace mozilla::a11y;

typedef AtkUtil MaiUtil;
typedef AtkUtilClass MaiUtilClass;

#define MAI_VERSION MOZILLA_VERSION
#define MAI_NAME "Gecko"

extern "C" {
static guint (*gail_add_global_event_listener)(GSignalEmissionHook listener,
                                               const gchar* event_type);
static void (*gail_remove_global_event_listener) (guint remove_listener);
static void (*gail_remove_key_event_listener) (guint remove_listener);
static AtkObject*  (*gail_get_root)();
}

struct MaiUtilListenerInfo
{
  gint key;
  guint signal_id;
  gulong hook_id;
  // For window create/destory/minimize/maximize/restore/activate/deactivate
  // events, we'll chain gail_util's add/remove_global_event_listener.
  // So we store the listenerid returned by gail's add_global_event_listener
  // in this structure to call gail's remove_global_event_listener later.
  guint gail_listenerid;
};

static GHashTable* sListener_list = nullptr;
static gint sListener_idx = 1;

extern "C" {
static guint
add_listener (GSignalEmissionHook listener,
              const gchar *object_type,
              const gchar *signal,
              const gchar *hook_data,
              guint gail_listenerid = 0)
{
    GType type;
    guint signal_id;
    gint rc = 0;

    type = g_type_from_name(object_type);
    if (type) {
        signal_id = g_signal_lookup(signal, type);
        if (signal_id > 0) {
            MaiUtilListenerInfo *listener_info;

            rc = sListener_idx;

            listener_info =  (MaiUtilListenerInfo *)
                g_malloc(sizeof(MaiUtilListenerInfo));
            listener_info->key = sListener_idx;
            listener_info->hook_id =
                g_signal_add_emission_hook(signal_id, 0, listener,
                                           g_strdup(hook_data),
                                           (GDestroyNotify)g_free);
            listener_info->signal_id = signal_id;
            listener_info->gail_listenerid = gail_listenerid;

            g_hash_table_insert(sListener_list, &(listener_info->key),
                                listener_info);
            sListener_idx++;
        }
        else {
            g_warning("Invalid signal type %s\n", signal);
        }
    }
    else {
        g_warning("Invalid object type %s\n", object_type);
    }
    return rc;
}

static guint
mai_util_add_global_event_listener(GSignalEmissionHook listener,
                                   const gchar *event_type)
{
    guint rc = 0;
    gchar **split_string;

    split_string = g_strsplit (event_type, ":", 3);

    if (split_string) {
        if (!strcmp ("window", split_string[0])) {
            guint gail_listenerid = 0;
            if (gail_add_global_event_listener) {
                // call gail's function to track gtk native window events
                gail_listenerid =
                    gail_add_global_event_listener(listener, event_type);
            }

            rc = add_listener (listener, "MaiAtkObject", split_string[1],
                               event_type, gail_listenerid);
        }
        else {
            rc = add_listener (listener, split_string[1], split_string[2],
                               event_type);
        }
        g_strfreev(split_string);
    }
    return rc;
}

static void
mai_util_remove_global_event_listener(guint remove_listener)
{
    if (remove_listener > 0) {
        MaiUtilListenerInfo *listener_info;
        gint tmp_idx = remove_listener;

        listener_info = (MaiUtilListenerInfo *)
            g_hash_table_lookup(sListener_list, &tmp_idx);

        if (listener_info != nullptr) {
            if (gail_remove_global_event_listener &&
                listener_info->gail_listenerid) {
              gail_remove_global_event_listener(listener_info->gail_listenerid);
            }

            /* Hook id of 0 and signal id of 0 are invalid */
            if (listener_info->hook_id != 0 && listener_info->signal_id != 0) {
                /* Remove the emission hook */
                g_signal_remove_emission_hook(listener_info->signal_id,
                                              listener_info->hook_id);

                /* Remove the element from the hash */
                g_hash_table_remove(sListener_list, &tmp_idx);
            }
            else {
                g_warning("Invalid listener hook_id %ld or signal_id %d\n",
                          listener_info->hook_id, listener_info->signal_id);
            }
        }
        else {
            // atk-bridge is initialized with gail (e.g. yelp)
            // try gail_remove_global_event_listener
            if (gail_remove_global_event_listener) {
                return gail_remove_global_event_listener(remove_listener);
            }

            g_warning("No listener with the specified listener id %d",
                      remove_listener);
        }
    }
    else {
        g_warning("Invalid listener_id %d", remove_listener);
    }
}

static AtkKeyEventStruct *
atk_key_event_from_gdk_event_key (GdkEventKey *key)
{
    AtkKeyEventStruct *event = g_new0(AtkKeyEventStruct, 1);
    switch (key->type) {
    case GDK_KEY_PRESS:
        event->type = ATK_KEY_EVENT_PRESS;
        break;
    case GDK_KEY_RELEASE:
        event->type = ATK_KEY_EVENT_RELEASE;
        break;
    default:
        g_assert_not_reached ();
        return nullptr;
    }
    event->state = key->state;
    event->keyval = key->keyval;
    event->length = key->length;
    if (key->string && key->string [0] &&
        (key->state & GDK_CONTROL_MASK ||
         g_unichar_isgraph (g_utf8_get_char (key->string)))) {
        event->string = key->string;
    }
    else if (key->type == GDK_KEY_PRESS ||
             key->type == GDK_KEY_RELEASE) {
        event->string = gdk_keyval_name (key->keyval);
    }
    event->keycode = key->hardware_keycode;
    event->timestamp = key->time;

    return event;
}

struct MaiKeyEventInfo
{
    AtkKeyEventStruct *key_event;
    gpointer func_data;
};

union AtkKeySnoopFuncPointer
{
    AtkKeySnoopFunc func_ptr;
    gpointer data;
};

static gboolean
notify_hf(gpointer key, gpointer value, gpointer data)
{
    MaiKeyEventInfo *info = (MaiKeyEventInfo *)data;
    AtkKeySnoopFuncPointer atkKeySnoop;
    atkKeySnoop.data = value;
    return (atkKeySnoop.func_ptr)(info->key_event, info->func_data) ? TRUE : FALSE;
}

static void
insert_hf(gpointer key, gpointer value, gpointer data)
{
    GHashTable *new_table = (GHashTable *) data;
    g_hash_table_insert (new_table, key, value);
}

static GHashTable* sKey_listener_list = nullptr;

static gint
mai_key_snooper(GtkWidget *the_widget, GdkEventKey *event, gpointer func_data)
{
    /* notify each AtkKeySnoopFunc in turn... */

    MaiKeyEventInfo *info = g_new0(MaiKeyEventInfo, 1);
    gint consumed = 0;
    if (sKey_listener_list) {
        GHashTable *new_hash = g_hash_table_new(nullptr, nullptr);
        g_hash_table_foreach (sKey_listener_list, insert_hf, new_hash);
        info->key_event = atk_key_event_from_gdk_event_key (event);
        info->func_data = func_data;
        consumed = g_hash_table_foreach_steal (new_hash, notify_hf, info);
        g_hash_table_destroy (new_hash);
        g_free(info->key_event);
    }
    g_free(info);
    return (consumed ? 1 : 0);
}

static guint sKey_snooper_id = 0;

static guint
mai_util_add_key_event_listener (AtkKeySnoopFunc listener,
                                 gpointer data)
{
  if (MOZ_UNLIKELY(!listener))
    return 0;

    static guint key=0;

    if (!sKey_listener_list) {
        sKey_listener_list = g_hash_table_new(nullptr, nullptr);
        sKey_snooper_id = gtk_key_snooper_install(mai_key_snooper, data);
    }
    AtkKeySnoopFuncPointer atkKeySnoop;
    atkKeySnoop.func_ptr = listener;
    g_hash_table_insert(sKey_listener_list, GUINT_TO_POINTER (key++),
                        atkKeySnoop.data);
    return key;
}

static void
mai_util_remove_key_event_listener (guint remove_listener)
{
    if (!sKey_listener_list) {
        // atk-bridge is initialized with gail (e.g. yelp)
        // try gail_remove_key_event_listener
        return gail_remove_key_event_listener(remove_listener);
    }

    g_hash_table_remove(sKey_listener_list, GUINT_TO_POINTER (remove_listener));
    if (g_hash_table_size(sKey_listener_list) == 0) {
        gtk_key_snooper_remove(sKey_snooper_id);
    }
}

static AtkObject*
mai_util_get_root()
{
  ApplicationAccessible* app = ApplicationAcc();
  if (app)
    return app->GetAtkObject();

  // We've shutdown, try to use gail instead
  // (to avoid assert in spi_atk_tidy_windows())
  // XXX tbsaunde then why didn't we replace the gail atk_util impl?
  if (gail_get_root)
    return gail_get_root();

  return nullptr;
}

static const gchar*
mai_util_get_toolkit_name()
{
    return MAI_NAME;
}

static const gchar*
mai_util_get_toolkit_version()
{
    return MAI_VERSION;
}

static void
_listener_info_destroy(gpointer data)
{
    g_free(data);
}

static void
window_added (AtkObject *atk_obj,
              guint     index,
              AtkObject *child)
{
  if (!IS_MAI_OBJECT(child))
      return;

  static guint id =  g_signal_lookup ("create", MAI_TYPE_ATK_OBJECT);
  g_signal_emit (child, id, 0);
}

static void
window_removed (AtkObject *atk_obj,
                guint     index,
                AtkObject *child)
{
  if (!IS_MAI_OBJECT(child))
      return;

  static guint id =  g_signal_lookup ("destroy", MAI_TYPE_ATK_OBJECT);
  g_signal_emit (child, id, 0);
}

  static void
UtilInterfaceInit(MaiUtilClass* klass)
{
    AtkUtilClass *atk_class;
    gpointer data;

    data = g_type_class_peek(ATK_TYPE_UTIL);
    atk_class = ATK_UTIL_CLASS(data);

    // save gail function pointer
    gail_add_global_event_listener = atk_class->add_global_event_listener;
    gail_remove_global_event_listener = atk_class->remove_global_event_listener;
    gail_remove_key_event_listener = atk_class->remove_key_event_listener;
    gail_get_root = atk_class->get_root;

    atk_class->add_global_event_listener =
        mai_util_add_global_event_listener;
    atk_class->remove_global_event_listener =
        mai_util_remove_global_event_listener;
    atk_class->add_key_event_listener = mai_util_add_key_event_listener;
    atk_class->remove_key_event_listener = mai_util_remove_key_event_listener;
    atk_class->get_root = mai_util_get_root;
    atk_class->get_toolkit_name = mai_util_get_toolkit_name;
    atk_class->get_toolkit_version = mai_util_get_toolkit_version;

    sListener_list = g_hash_table_new_full(g_int_hash, g_int_equal, nullptr,
                                           _listener_info_destroy);
    // Keep track of added/removed windows.
    AtkObject *root = atk_get_root ();
    g_signal_connect (root, "children-changed::add", (GCallback) window_added, nullptr);
    g_signal_connect (root, "children-changed::remove", (GCallback) window_removed, nullptr);
}
}

GType
mai_util_get_type()
{
    static GType type = 0;

    if (!type) {
        static const GTypeInfo tinfo = {
            sizeof(MaiUtilClass),
            (GBaseInitFunc) nullptr, /* base init */
            (GBaseFinalizeFunc) nullptr, /* base finalize */
            (GClassInitFunc) UtilInterfaceInit, /* class init */
            (GClassFinalizeFunc) nullptr, /* class finalize */
            nullptr, /* class data */
            sizeof(MaiUtil), /* instance size */
            0, /* nb preallocs */
            (GInstanceInitFunc) nullptr, /* instance init */
            nullptr /* value table */
        };

        type = g_type_register_static(ATK_TYPE_UTIL,
                                      "MaiUtil", &tinfo, GTypeFlags(0));
    }
    return type;
}


/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher Blizzard.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Ramiro Estrugo <ramiro@eazel.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef gtkmozembed_h
#define gtkmozembed_h

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stddef.h>
#include <gtk/gtk.h>

#ifdef MOZILLA_CLIENT
#include "nscore.h"
#else /* MOZILLA_CLIENT */
#ifndef nscore_h__
/* Because this header may be included from files which not part of the mozilla
   build system, define macros from nscore.h */

#if (__GNUC__ >= 4) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)
#define NS_HIDDEN __attribute__((visibility("hidden")))
#else
#define NS_HIDDEN
#endif

#define NS_FROZENCALL
#define NS_EXPORT_(type) type
#define NS_IMPORT_(type) type
#endif /* nscore_h__ */
#endif /* MOZILLA_CLIENT */

#ifdef XPCOM_GLUE

#define GTKMOZEMBED_API(type, name, params) \
  typedef type (NS_FROZENCALL * name##Type) params; \
  extern name##Type name NS_HIDDEN;

#else /* XPCOM_GLUE */

#ifdef _IMPL_GTKMOZEMBED
#define GTKMOZEMBED_API(type, name, params) NS_EXPORT_(type) name params;
#else
#define GTKMOZEMBED_API(type,name, params) NS_IMPORT_(type) name params;
#endif

#endif /* XPCOM_GLUE */

#define GTK_TYPE_MOZ_EMBED             (gtk_moz_embed_get_type())
#define GTK_MOZ_EMBED(obj)             G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_MOZ_EMBED, GtkMozEmbed)
#define GTK_MOZ_EMBED_CLASS(klass)     G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_MOZ_EMBED, GtkMozEmbedClass)
#define GTK_IS_MOZ_EMBED(obj)          G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_MOZ_EMBED)
#define GTK_IS_MOZ_EMBED_CLASS(klass)  G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_MOZ_EMBED)

typedef struct _GtkMozEmbed      GtkMozEmbed;
typedef struct _GtkMozEmbedClass GtkMozEmbedClass;

struct _GtkMozEmbed
{
  GtkBin    bin;
  void     *data;
};

struct _GtkMozEmbedClass
{
  GtkBinClass parent_class;

  void (* link_message)        (GtkMozEmbed *embed);
  void (* js_status)           (GtkMozEmbed *embed);
  void (* location)            (GtkMozEmbed *embed);
  void (* title)               (GtkMozEmbed *embed);
  void (* progress)            (GtkMozEmbed *embed, gint curprogress,
                                gint maxprogress);
  void (* progress_all)        (GtkMozEmbed *embed, const char *aURI,
                                gint curprogress, gint maxprogress);
  void (* net_state)           (GtkMozEmbed *embed, gint state, guint status);
  void (* net_state_all)       (GtkMozEmbed *embed, const char *aURI,
                                gint state, guint status);
  void (* net_start)           (GtkMozEmbed *embed);
  void (* net_stop)            (GtkMozEmbed *embed);
  void (* new_window)          (GtkMozEmbed *embed, GtkMozEmbed **newEmbed,
                                guint chromemask);
  void (* visibility)          (GtkMozEmbed *embed, gboolean visibility);
  void (* destroy_brsr)        (GtkMozEmbed *embed);
  gint (* open_uri)            (GtkMozEmbed *embed, const char *aURI);
  void (* size_to)             (GtkMozEmbed *embed, gint width, gint height);
  gint (* dom_key_down)        (GtkMozEmbed *embed, gpointer dom_event);
  gint (* dom_key_press)       (GtkMozEmbed *embed, gpointer dom_event);
  gint (* dom_key_up)          (GtkMozEmbed *embed, gpointer dom_event);
  gint (* dom_mouse_down)      (GtkMozEmbed *embed, gpointer dom_event);
  gint (* dom_mouse_up)        (GtkMozEmbed *embed, gpointer dom_event);
  gint (* dom_mouse_click)     (GtkMozEmbed *embed, gpointer dom_event);
  gint (* dom_mouse_dbl_click) (GtkMozEmbed *embed, gpointer dom_event);
  gint (* dom_mouse_over)      (GtkMozEmbed *embed, gpointer dom_event);
  gint (* dom_mouse_out)       (GtkMozEmbed *embed, gpointer dom_event);
  void (* security_change)     (GtkMozEmbed *embed, gpointer request,
                                guint state);
  void (* status_change)       (GtkMozEmbed *embed, gpointer request,
                                gint status, gpointer message);
  gint (* dom_activate)        (GtkMozEmbed *embed, gpointer dom_event);
  gint (* dom_focus_in)        (GtkMozEmbed *embed, gpointer dom_event);
  gint (* dom_focus_out)       (GtkMozEmbed *embed, gpointer dom_event);
};

GTKMOZEMBED_API(GType,      gtk_moz_embed_get_type,        (void))
GTKMOZEMBED_API(GtkWidget*, gtk_moz_embed_new,             (void))
GTKMOZEMBED_API(void,       gtk_moz_embed_push_startup,    (void))
GTKMOZEMBED_API(void,       gtk_moz_embed_pop_startup,     (void))

/* Tell gtkmozembed where the gtkmozembed libs live. If this is not specified,
   The MOZILLA_FIVE_HOME environment variable is checked. */
GTKMOZEMBED_API(void,       gtk_moz_embed_set_path,        (const char *aPath))

GTKMOZEMBED_API(void,       gtk_moz_embed_set_comp_path,   (const char *aPath))
GTKMOZEMBED_API(void,      gtk_moz_embed_set_profile_path, (const char *aDir,
                                                            const char *aName))
GTKMOZEMBED_API(void,      gtk_moz_embed_load_url,        (GtkMozEmbed *embed,
                                                           const char *url))
GTKMOZEMBED_API(void,      gtk_moz_embed_stop_load,       (GtkMozEmbed *embed))
GTKMOZEMBED_API(gboolean,  gtk_moz_embed_can_go_back,     (GtkMozEmbed *embed))
GTKMOZEMBED_API(gboolean,  gtk_moz_embed_can_go_forward,  (GtkMozEmbed *embed))
GTKMOZEMBED_API(void,      gtk_moz_embed_go_back,         (GtkMozEmbed *embed))
GTKMOZEMBED_API(void,      gtk_moz_embed_go_forward,      (GtkMozEmbed *embed))
GTKMOZEMBED_API(void,   gtk_moz_embed_render_data,     (GtkMozEmbed *embed, 
                                                        const char *data,
                                                        guint32 len,
                                                        const char *base_uri, 
                                                        const char *mime_type))
GTKMOZEMBED_API(void,   gtk_moz_embed_open_stream,     (GtkMozEmbed *embed,
                                                        const char *base_uri,
                                                        const char *mime_type))
GTKMOZEMBED_API(void,   gtk_moz_embed_append_data,      (GtkMozEmbed *embed,
                                                         const char *data,
                                                         guint32 len))
GTKMOZEMBED_API(void,   gtk_moz_embed_close_stream,     (GtkMozEmbed *embed))
GTKMOZEMBED_API(char*,  gtk_moz_embed_get_link_message, (GtkMozEmbed *embed))
GTKMOZEMBED_API(char*,  gtk_moz_embed_get_js_status,    (GtkMozEmbed *embed))
GTKMOZEMBED_API(char*,  gtk_moz_embed_get_title,        (GtkMozEmbed *embed))
GTKMOZEMBED_API(char*,  gtk_moz_embed_get_location,     (GtkMozEmbed *embed))
GTKMOZEMBED_API(void,   gtk_moz_embed_reload,           (GtkMozEmbed *embed,
                                                         gint32 flags))
GTKMOZEMBED_API(void,   gtk_moz_embed_set_chrome_mask,  (GtkMozEmbed *embed, 
                                                         guint32 flags))
GTKMOZEMBED_API(guint32, gtk_moz_embed_get_chrome_mask, (GtkMozEmbed *embed))

/* These are straight out of nsIWebProgressListener.h */

typedef enum
{
  GTK_MOZ_EMBED_FLAG_START = 1,
  GTK_MOZ_EMBED_FLAG_REDIRECTING = 2,
  GTK_MOZ_EMBED_FLAG_TRANSFERRING = 4,
  GTK_MOZ_EMBED_FLAG_NEGOTIATING = 8,
  GTK_MOZ_EMBED_FLAG_STOP = 16,
  
  GTK_MOZ_EMBED_FLAG_IS_REQUEST = 65536,
  GTK_MOZ_EMBED_FLAG_IS_DOCUMENT = 131072,
  GTK_MOZ_EMBED_FLAG_IS_NETWORK = 262144,
  GTK_MOZ_EMBED_FLAG_IS_WINDOW = 524288,

  GTK_MOZ_EMBED_FLAG_RESTORING = 16777216
} GtkMozEmbedProgressFlags;

/* These are from various networking headers */

typedef enum
{
  /* NS_ERROR_UNKNOWN_HOST */
  GTK_MOZ_EMBED_STATUS_FAILED_DNS     = 2152398878U,
 /* NS_ERROR_CONNECTION_REFUSED */
  GTK_MOZ_EMBED_STATUS_FAILED_CONNECT = 2152398861U,
 /* NS_ERROR_NET_TIMEOUT */
  GTK_MOZ_EMBED_STATUS_FAILED_TIMEOUT = 2152398862U,
 /* NS_BINDING_ABORTED */
  GTK_MOZ_EMBED_STATUS_FAILED_USERCANCELED = 2152398850U
} GtkMozEmbedStatusFlags;

/* These used to be straight out of nsIWebNavigation.h until the API
   changed.  Now there's a mapping table that maps these values to the
   internal values. */

typedef enum 
{
  GTK_MOZ_EMBED_FLAG_RELOADNORMAL = 0,
  GTK_MOZ_EMBED_FLAG_RELOADBYPASSCACHE = 1,
  GTK_MOZ_EMBED_FLAG_RELOADBYPASSPROXY = 2,
  GTK_MOZ_EMBED_FLAG_RELOADBYPASSPROXYANDCACHE = 3,
  GTK_MOZ_EMBED_FLAG_RELOADCHARSETCHANGE = 4
} GtkMozEmbedReloadFlags;

/* These are straight out of nsIWebBrowserChrome.h */

typedef enum
{
  GTK_MOZ_EMBED_FLAG_DEFAULTCHROME = 1U,
  GTK_MOZ_EMBED_FLAG_WINDOWBORDERSON = 2U,
  GTK_MOZ_EMBED_FLAG_WINDOWCLOSEON = 4U,
  GTK_MOZ_EMBED_FLAG_WINDOWRESIZEON = 8U,
  GTK_MOZ_EMBED_FLAG_MENUBARON = 16U,
  GTK_MOZ_EMBED_FLAG_TOOLBARON = 32U,
  GTK_MOZ_EMBED_FLAG_LOCATIONBARON = 64U,
  GTK_MOZ_EMBED_FLAG_STATUSBARON = 128U,
  GTK_MOZ_EMBED_FLAG_PERSONALTOOLBARON = 256U,
  GTK_MOZ_EMBED_FLAG_SCROLLBARSON = 512U,
  GTK_MOZ_EMBED_FLAG_TITLEBARON = 1024U,
  GTK_MOZ_EMBED_FLAG_EXTRACHROMEON = 2048U,
  GTK_MOZ_EMBED_FLAG_ALLCHROME = 4094U,
  GTK_MOZ_EMBED_FLAG_WINDOWRAISED = 33554432U,
  GTK_MOZ_EMBED_FLAG_WINDOWLOWERED = 67108864U,
  GTK_MOZ_EMBED_FLAG_CENTERSCREEN = 134217728U,
  GTK_MOZ_EMBED_FLAG_DEPENDENT = 268435456U,
  GTK_MOZ_EMBED_FLAG_MODAL = 536870912U,
  GTK_MOZ_EMBED_FLAG_OPENASDIALOG = 1073741824U,
  GTK_MOZ_EMBED_FLAG_OPENASCHROME = 2147483648U 
} GtkMozEmbedChromeFlags;

/* this is a singleton object that you can hook up to to get signals
   that are not handed out on a per widget basis. */

#define GTK_TYPE_MOZ_EMBED_SINGLE            (gtk_moz_embed_single_get_type())
#define GTK_MOZ_EMBED_SINGLE(obj)            GTK_CHECK_CAST((obj), GTK_TYPE_MOZ_EMBED_SINGLE, GtkMozEmbedSingle)
#define GTK_MOZ_EMBED_SINGLE_CLASS(klass)    GTK_CHEK_CLASS_CAST((klass), GTK_TYPE_MOZ_EMBED_SINGLE, GtkMozEmbedSingleClass)
#define GTK_IS_MOZ_EMBED_SINGLE(obj)         GTK_CHECK_TYPE((obj), GTK_TYPE_MOZ_EMBED_SINGLE)
#define GTK_IS_MOZ_EMBED_SINGLE_CLASS(klass) GTK_CHECK_CLASS_TYPE((klass), GTK_TYPE_MOZ_EMBED)
#define GTK_MOZ_EMBED_SINGLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_MOZ_EMBED_SINGLE, GtkMozEmbedSingleClass))

typedef struct _GtkMozEmbedSingle      GtkMozEmbedSingle;
typedef struct _GtkMozEmbedSingleClass GtkMozEmbedSingleClass;

struct _GtkMozEmbedSingle
{
  GtkObject  object;
  void      *data;
};

struct _GtkMozEmbedSingleClass
{
  GtkObjectClass parent_class;

  void (* new_window_orphan)   (GtkMozEmbedSingle *embed,
                                GtkMozEmbed **newEmbed,
                                guint chromemask);
};

GTKMOZEMBED_API(GType,               gtk_moz_embed_single_get_type, (void))
GTKMOZEMBED_API(GtkMozEmbedSingle *, gtk_moz_embed_single_get,      (void))

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* gtkmozembed_h */

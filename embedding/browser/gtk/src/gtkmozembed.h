/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Ramiro Estrugo <ramiro@eazel.com>
 */

#ifndef gtkmozembed_h
#define gtkmozembed_h

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <gtk/gtk.h>

#define GTK_TYPE_MOZ_EMBED             (gtk_moz_embed_get_type())
#define GTK_MOZ_EMBED(obj)             GTK_CHECK_CAST((obj), GTK_TYPE_MOZ_EMBED, GtkMozEmbed)
#define GTK_MOZ_EMBED_CLASS(klass)     GTK_CHEK_CLASS_CAST((klass), GTK_TYPE_MOZ_EMBED, GtkMozEmbedClass)
#define GTK_IS_MOZ_EMBED(obj)          GTK_CHECK_TYPE((obj), GTK_TYPE_MOZ_EMBED)
#define GTK_IS_MOZ_EMBED_CLASS(klass)  GTK_CHECK_CLASS_TYPE((klass), GTK_TYPE_MOZ_EMBED)

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

  void (* link_message) (GtkMozEmbed *embed);
  void (* js_status)    (GtkMozEmbed *embed);
  void (* location)     (GtkMozEmbed *embed);
  void (* title)        (GtkMozEmbed *embed);
  void (* progress)     (GtkMozEmbed *embed, gint maxprogress, gint curprogress);
  void (* net_status)   (GtkMozEmbed *embed, gint status);
  void (* net_start)    (GtkMozEmbed *embed);
  void (* net_stop)     (GtkMozEmbed *embed);
  void (* new_window)   (GtkMozEmbed *embed, GtkMozEmbed **newEmbed, guint chromemask);
  void (* visibility)   (GtkMozEmbed *embed, gboolean visibility);
  void (* destroy_brsr) (GtkMozEmbed *embed);
};

extern GtkType      gtk_moz_embed_get_type         (void);
extern GtkWidget   *gtk_moz_embed_new              (void);
extern void         gtk_moz_embed_load_url         (GtkMozEmbed *embed, const char *url);
extern void         gtk_moz_embed_stop_load        (GtkMozEmbed *embed);
extern gboolean     gtk_moz_embed_can_go_back      (GtkMozEmbed *embed);
extern gboolean     gtk_moz_embed_can_go_forward   (GtkMozEmbed *embed);
extern void         gtk_moz_embed_go_back          (GtkMozEmbed *embed);
extern void         gtk_moz_embed_go_forward       (GtkMozEmbed *embed);
extern void         gtk_moz_embed_render_data      (GtkMozEmbed *embed, 
						    const char *data, guint32 len,
						    const char *base_uri, const char *mime_type);
extern void         gtk_moz_embed_open_stream      (GtkMozEmbed *embed,
						    const char *base_uri, const char *mime_type);
extern void         gtk_moz_embed_append_data      (GtkMozEmbed *embed,
						    const char *data, guint32 len);
extern void         gtk_moz_embed_close_stream     (GtkMozEmbed *embed);
extern char        *gtk_moz_embed_get_link_message (GtkMozEmbed *embed);
extern char        *gtk_moz_embed_get_js_status    (GtkMozEmbed *embed);
extern char        *gtk_moz_embed_get_title        (GtkMozEmbed *embed);
extern char        *gtk_moz_embed_get_location     (GtkMozEmbed *embed);
extern void         gtk_moz_embed_reload           (GtkMozEmbed *embed, gint32 flags);
extern void         gtk_moz_embed_set_chrome_mask  (GtkMozEmbed *embed, guint32 flags);
extern guint32      gtk_moz_embed_get_chrome_mask  (GtkMozEmbed *embed);

/* These are straight out of nsIWebProgress.h */

enum { gtk_moz_embed_flag_net_start = 1,
       gtk_moz_embed_flag_net_stop = 2,
       gtk_moz_embed_flag_net_dns = 4,
       gtk_moz_embed_flag_net_connecting = 8,
       gtk_moz_embed_flag_net_redirecting = 16,
       gtk_moz_embed_flag_net_negotiating = 32,
       gtk_moz_embed_flag_net_transferring = 64,
       gtk_moz_embed_flag_net_failedDNS = 4096,
       gtk_moz_embed_flag_net_failedConnect = 8192,
       gtk_moz_embed_flag_net_failedTransfer = 16384,
       gtk_moz_embed_flag_net_failedTimeout = 32768,
       gtk_moz_embed_flag_net_userCancelled = 65536,
       gtk_moz_embed_flag_win_start = 1048576,
       gtk_moz_embed_flag_win_stop = 2097152 };

/* These are straight out of nsIWebNavigation.h */

enum { gtk_moz_embed_flag_reloadNormal = 0,
       gtk_moz_embed_flag_reloadBypassCache = 1,
       gtk_moz_embed_flag_reloadBypassProxy = 2,
       gtk_moz_embed_flag_reloadBypassProxyAndCache = 3 };

/* These are straight out of nsIWebBrowserChrome.h */

enum { gtk_moz_embed_flag_defaultChrome = 1U,
       gtk_moz_embed_flag_windowBordersOn = 2U,
       gtk_moz_embed_flag_windowCloseOn = 4U,
       gtk_moz_embed_flag_windowResizeOn = 8U,
       gtk_moz_embed_flag_menuBarOn = 16U,
       gtk_moz_embed_flag_toolBarOn = 32U,
       gtk_moz_embed_flag_locationBarOn = 64U,
       gtk_moz_embed_flag_statusBarOn = 128U,
       gtk_moz_embed_flag_personalToolBarOn = 256U,
       gtk_moz_embed_flag_scrollbarsOn = 512U,
       gtk_moz_embed_flag_titlebarOn = 1024U,
       gtk_moz_embed_flag_extraChromeOn = 2048U,
       gtk_moz_embed_flag_windowRaised = 33554432U,
       gtk_moz_embed_flag_windowLowered = 67108864U,
       gtk_moz_embed_flag_centerScreen = 134217728U,
       gtk_moz_embed_flag_dependent = 268435456U,
       gtk_moz_embed_flag_modal = 536870912U,
       gtk_moz_embed_flag_openAsDialog = 1073741824U,
       gtk_moz_embed_flag_openAsChrome = 2147483648U,
       gtk_moz_embed_flag_allChrome = 4094U };


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* gtkmozembed_h */

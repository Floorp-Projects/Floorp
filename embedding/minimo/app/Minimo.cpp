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
 * Christopher Blizzard. Portions created by Christopher Blizzard are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Doug Turner <dougt@meer.net>  Branched from TestGtkEmbed.cpp
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

#include "icons.h"
#include "gtkmozembed.h"
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// mozilla specific headers
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "prenv.h"

#ifdef NS_TRACE_MALLOC
#include "nsTraceMalloc.h"
#endif

#ifdef MOZ_JPROF
#include "jprof.h"
#endif

#define MINIMO_HOME_URL "http://www.mozilla.org/projects/minimo/home.html"
#define MINIMO_THROBBER_ICON_MAX 4

typedef struct _MinimoBrowser {
  GtkWidget  *topLevelWindow;
  GtkWidget  *topLevelVBox;
  GtkWidget  *toolbarHBox;
  GtkWidget  *toolbar;
  GtkWidget  *backButton;
  GtkWidget  *stopReloadButton;
  GtkWidget  *forwardButton;
  GtkWidget  *homeButton;
  GtkWidget  *urlEntry;
  GtkWidget  *mozEmbed;
  GtkWidget  *progressAreaHBox;
  GtkWidget  *progressBar;
  GtkWidget  *statusAlign;
  GtkWidget  *statusBar;
  GtkWidget  *backIcon;
  GtkWidget  *forwardIcon;
  GtkWidget  *homeIcon;
  GtkWidget  *reloadIcon;
  GtkWidget  *stopIcon[MINIMO_THROBBER_ICON_MAX];
  GtkWidget  *stopReloadIcon;

  const char *statusMessage;
  int         loadPercent;
  int         bytesLoaded;
  int         maxBytesLoaded;
  char       *tempMessage;
  int         throbberID;
  gboolean toolBarOn;
  gboolean locationBarOn;
  gboolean statusBarOn;
  gboolean loading;
} MinimoBrowser;

// the list of browser windows currently open
GList *browser_list = g_list_alloc();

static MinimoBrowser *new_gtk_browser    (guint32 chromeMask);
static void            set_browser_visibility (MinimoBrowser *browser,
					       gboolean visibility);

static int num_browsers = 0;

// callbacks from the UI
static void     back_clicked_cb    (GtkButton   *button, 
				    MinimoBrowser *browser);
static void     forward_clicked_cb (GtkButton   *button,
				    MinimoBrowser *browser);
static void     stop_reload_clicked_cb  (GtkButton   *button,
					 MinimoBrowser *browser);
static void     home_clicked_cb    (GtkButton   *button,
				    MinimoBrowser *browser);
static void     url_activate_cb    (GtkEditable *widget, 
				    MinimoBrowser *browser);
static gboolean delete_cb          (GtkWidget *widget, GdkEventAny *event,
				    MinimoBrowser *browser);
static void     destroy_cb         (GtkWidget *widget,
				    MinimoBrowser *browser);

// callbacks from the widget
static void location_changed_cb  (GtkMozEmbed *embed, MinimoBrowser *browser);
static void title_changed_cb     (GtkMozEmbed *embed, MinimoBrowser *browser);
static void load_started_cb      (GtkMozEmbed *embed, MinimoBrowser *browser);
static void load_finished_cb     (GtkMozEmbed *embed, MinimoBrowser *browser);
static void net_state_change_cb  (GtkMozEmbed *embed, gint flags,
				  guint status, MinimoBrowser *browser);
static void net_state_change_all_cb (GtkMozEmbed *embed, const char *uri,
				     gint flags, guint status,
				     MinimoBrowser *browser);
static void progress_change_cb   (GtkMozEmbed *embed, gint cur, gint max,
				  MinimoBrowser *browser);
static void progress_change_all_cb (GtkMozEmbed *embed, const char *uri,
				    gint cur, gint max,
				    MinimoBrowser *browser);
static void link_message_cb      (GtkMozEmbed *embed, MinimoBrowser *browser);
static void js_status_cb         (GtkMozEmbed *embed, MinimoBrowser *browser);
static void new_window_cb        (GtkMozEmbed *embed,
				  GtkMozEmbed **retval, guint chromemask,
				  MinimoBrowser *browser);
static void visibility_cb        (GtkMozEmbed *embed, 
				  gboolean visibility,
				  MinimoBrowser *browser);
static void destroy_brsr_cb      (GtkMozEmbed *embed, MinimoBrowser *browser);
static gint open_uri_cb          (GtkMozEmbed *embed, const char *uri,
				  MinimoBrowser *browser);
static void size_to_cb           (GtkMozEmbed *embed, gint width,
				  gint height, MinimoBrowser *browser);
static gint dom_key_down_cb      (GtkMozEmbed *embed, nsIDOMKeyEvent *event,
				  MinimoBrowser *browser);
static gint dom_key_press_cb     (GtkMozEmbed *embed, nsIDOMKeyEvent *event,
				  MinimoBrowser *browser);
static gint dom_key_up_cb        (GtkMozEmbed *embed, nsIDOMKeyEvent *event,
				  MinimoBrowser *browser);
static gint dom_mouse_down_cb    (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
				  MinimoBrowser *browser);
static gint dom_mouse_up_cb      (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
				  MinimoBrowser *browser);
static gint dom_mouse_click_cb   (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
				  MinimoBrowser *browser);
static gint dom_mouse_dbl_click_cb (GtkMozEmbed *embed, 
				  nsIDOMMouseEvent *event,
				  MinimoBrowser *browser);
static gint dom_mouse_over_cb    (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
				  MinimoBrowser *browser);
static gint dom_mouse_out_cb     (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
				  MinimoBrowser *browser);

// callbacks from the singleton object
static void new_window_orphan_cb (GtkMozEmbedSingle *embed,
				  GtkMozEmbed **retval, guint chromemask,
				  gpointer data);

static gboolean urlEntry_focus_in (GtkWidget *entry, GdkEventFocus *event,
				   gpointer user_data)
{
  MinimoBrowser* browser = (MinimoBrowser*) user_data;
  if (!browser->loading)
    gtk_widget_hide(browser->toolbar);
  return 0;
}
 
static gboolean urlEntry_focus_out (GtkWidget *entry, GdkEventFocus *event,
				    gpointer user_data)
{
  MinimoBrowser* browser = (MinimoBrowser*) user_data;
  gtk_widget_show(browser->toolbar);
  return 0;
 }

// some utility functions
static void update_toolbar          (MinimoBrowser *browser);
static void start_throbber          (MinimoBrowser *browser);
static void update_status_bar_text  (MinimoBrowser *browser);
static void update_temp_message     (MinimoBrowser *browser,
				     const char *message);
static void update_nav_buttons      (MinimoBrowser *browser);

int
main(int argc, char **argv)
{
#ifdef NS_TRACE_MALLOC
  argc = NS_TraceMallocStartupArgs(argc, argv);
#endif

  gtk_set_locale();
  gtk_init(&argc, &argv);

  char *home_path;
  char *full_path;
  home_path = PR_GetEnv("HOME");
  if (!home_path) {
    fprintf(stderr, "Failed to get HOME\n");
    exit(1);
  }
  
  full_path = g_strdup_printf("%s/%s", home_path, ".Minimo");
  
  gtk_moz_embed_set_profile_path(full_path, "Minimo");

  MinimoBrowser *browser = new_gtk_browser(GTK_MOZ_EMBED_FLAG_DEFAULTCHROME);

  // set our minimum size
  gtk_widget_set_usize(browser->mozEmbed, 240, 320);

  set_browser_visibility(browser, TRUE);

  if (argc > 1)
    gtk_moz_embed_load_url(GTK_MOZ_EMBED(browser->mozEmbed), argv[1]);

  // get the singleton object and hook up to its new window callback
  // so we can create orphaned windows.

  GtkMozEmbedSingle *single;

  single = gtk_moz_embed_single_get();
  if (!single) {
    fprintf(stderr, "Failed to get singleton embed object!\n");
    exit(1);
  }

  gtk_signal_connect(GTK_OBJECT(single), "new_window_orphan",
		     GTK_SIGNAL_FUNC(new_window_orphan_cb), NULL);

  gtk_main();
}

static GtkWidget*
NewIconFromXPM(GtkWidget *toolbar, gchar** icon_xpm)
{
  GdkBitmap *mask;
  GdkColormap *colormap = gtk_widget_get_colormap(toolbar);
  GtkStyle *style = gtk_widget_get_style(toolbar);

  GdkPixmap *pixmap = gdk_pixmap_colormap_create_from_xpm_d(NULL, 
							    colormap,
							    &mask, 
							    &style->bg[GTK_STATE_NORMAL], 
							    icon_xpm);
  GtkWidget* icon = gtk_pixmap_new(pixmap, mask);
  return icon;
}

static MinimoBrowser *
new_gtk_browser(guint32 chromeMask)
{
  guint32         actualChromeMask = chromeMask;
  MinimoBrowser *browser = 0;

  num_browsers++;

  browser = g_new0(MinimoBrowser, 1);

  browser_list = g_list_prepend(browser_list, browser);

  browser->toolBarOn = FALSE;
  browser->locationBarOn = FALSE;
  browser->statusBarOn = FALSE;
  browser->loading = TRUE;

  if (chromeMask == GTK_MOZ_EMBED_FLAG_DEFAULTCHROME)
    actualChromeMask = GTK_MOZ_EMBED_FLAG_ALLCHROME;

  if (actualChromeMask & GTK_MOZ_EMBED_FLAG_TOOLBARON)
  {
    browser->toolBarOn = TRUE;
  }
  if (actualChromeMask & GTK_MOZ_EMBED_FLAG_LOCATIONBARON)
  {
    browser->locationBarOn = TRUE;
  }
  if (actualChromeMask & GTK_MOZ_EMBED_FLAG_STATUSBARON)
  {
    browser->statusBarOn = TRUE;
  }

  // create our new toplevel window
  browser->topLevelWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  // new vbox
  browser->topLevelVBox = gtk_vbox_new(FALSE, 0);
  // add it to the toplevel window
  gtk_container_add(GTK_CONTAINER(browser->topLevelWindow),
		    browser->topLevelVBox);

  // create the hbox that will contain the toolbar and the url text entry bar
  browser->toolbarHBox = gtk_hbox_new(false, 0);
  // add that hbox to the vbox
  gtk_box_pack_start(GTK_BOX(browser->topLevelVBox), 
		     browser->toolbarHBox,
		     FALSE, // expand
		     FALSE, // fill
		     0);    // padding
  // new horiz toolbar with buttons + icons
#ifdef MOZ_WIDGET_GTK
  browser->toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL,
				     GTK_TOOLBAR_ICONS);
#endif /* MOZ_WIDGET_GTK */

#ifdef MOZ_WIDGET_GTK2
  browser->toolbar = gtk_toolbar_new();
  gtk_toolbar_set_orientation(GTK_TOOLBAR(browser->toolbar),
			      GTK_ORIENTATION_HORIZONTAL);
  gtk_toolbar_set_style(GTK_TOOLBAR(browser->toolbar),
			GTK_TOOLBAR_ICONS);
#endif /* MOZ_WIDGET_GTK2 */


  // setup the icons
  browser->backIcon = NewIconFromXPM(browser->toolbar, back_xpm);
  browser->forwardIcon = NewIconFromXPM(browser->toolbar, forward_xpm);
  browser->homeIcon = NewIconFromXPM(browser->toolbar, home_xpm);
  browser->reloadIcon = NewIconFromXPM(browser->toolbar, reload_xpm);
  
  browser->throbberID  = 0;
  browser->stopIcon[0] = NewIconFromXPM(browser->toolbar, stop_1_xpm);
  browser->stopIcon[1] = NewIconFromXPM(browser->toolbar, stop_2_xpm);
  browser->stopIcon[2] = NewIconFromXPM(browser->toolbar, stop_3_xpm);
  browser->stopIcon[3] = NewIconFromXPM(browser->toolbar, stop_4_xpm);

  // This is really just an allocation
  browser->stopReloadIcon = NewIconFromXPM(browser->toolbar, stop_1_xpm);

  // add it to the hbox
  gtk_box_pack_start(GTK_BOX(browser->toolbarHBox), browser->toolbar,
		     FALSE, // expand
		     FALSE, // fill
		     0);    // padding
  // new back button
  browser->backButton =
    gtk_toolbar_append_item(GTK_TOOLBAR(browser->toolbar),
			    "Back",
			    "Go Back",
			    "Go Back",
			    browser->backIcon,
			    GTK_SIGNAL_FUNC(back_clicked_cb),
			    browser);
  // new forward button
  browser->forwardButton =
    gtk_toolbar_append_item(GTK_TOOLBAR(browser->toolbar),
			    "Forward",
			    "Forward",
			    "Forward",
			    browser->forwardIcon, 
			    GTK_SIGNAL_FUNC(forward_clicked_cb),
			    browser);
  // new stop and reload button
  browser->stopReloadButton = 
    gtk_toolbar_append_item(GTK_TOOLBAR(browser->toolbar),
			    "Stop/Reload",
			    "Stop/Reload",
			    "Stop/Reload",
			    browser->stopReloadIcon,
			    GTK_SIGNAL_FUNC(stop_reload_clicked_cb),
			    browser);
  update_toolbar(browser);

  // new home button
  browser->homeButton = 
    gtk_toolbar_append_item(GTK_TOOLBAR(browser->toolbar),
			    "Home",
			    "Home",
			    "Home",
			    browser->homeIcon, 
			    GTK_SIGNAL_FUNC(home_clicked_cb),
			    browser);

  // create the url text entry
  browser->urlEntry = gtk_entry_new();
  g_signal_connect(G_OBJECT(browser->urlEntry), "focus-in-event",
		   G_CALLBACK(urlEntry_focus_in), browser);
  g_signal_connect(G_OBJECT(browser->urlEntry), "focus-out-event",
		   G_CALLBACK(urlEntry_focus_out), browser);

  // add it to the hbox
  gtk_box_pack_start(GTK_BOX(browser->toolbarHBox), browser->urlEntry,
		     TRUE, // expand
		     TRUE, // fill
		     0);    // padding
  // create our new gtk moz embed widget
  browser->mozEmbed = gtk_moz_embed_new();
  // add it to the toplevel vbox
  gtk_box_pack_start(GTK_BOX(browser->topLevelVBox), browser->mozEmbed,
		     TRUE, // expand
		     TRUE, // fill
		     0);   // padding
  // create the new hbox for the progress area
  browser->progressAreaHBox = gtk_hbox_new(FALSE, 0);

  // create our new progress bar
  browser->progressBar = gtk_progress_bar_new();

  // add it to the hbox
  gtk_box_pack_start(GTK_BOX(browser->progressAreaHBox), browser->progressBar,
		     FALSE, // expand
		     FALSE, // fill
		     0); // padding

  // create our status area and the alignment object that will keep it
  // from expanding
  browser->statusAlign = gtk_alignment_new(0, 0, 1, 1);
  gtk_widget_set_usize(browser->statusAlign, 1, -1);
  // create the status bar
  browser->statusBar = gtk_statusbar_new();

  gtk_container_add(GTK_CONTAINER(browser->statusAlign), browser->statusBar);
  // add it to the hbox
  gtk_box_pack_start(GTK_BOX(browser->progressAreaHBox), browser->statusAlign,
		     TRUE, // expand
		     TRUE, // fill
		     0);   // padding
  // by default none of the buttons are marked as sensitive.
  gtk_widget_set_sensitive(browser->backButton, FALSE);
  gtk_widget_set_sensitive(browser->forwardButton, FALSE);
  
  // catch the destruction of the toplevel window
  gtk_signal_connect(GTK_OBJECT(browser->topLevelWindow), "delete_event",
		     GTK_SIGNAL_FUNC(delete_cb), browser);

  // hook up the activate signal to the right callback
  gtk_signal_connect(GTK_OBJECT(browser->urlEntry), "activate",
		     GTK_SIGNAL_FUNC(url_activate_cb), browser);

  // hook up the location change to update the urlEntry
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "location",
		     GTK_SIGNAL_FUNC(location_changed_cb), browser);
  // hook up the title change to update the window title
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "title",
		     GTK_SIGNAL_FUNC(title_changed_cb), browser);
  // hook up the start and stop signals
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "net_start",
		     GTK_SIGNAL_FUNC(load_started_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "net_stop",
		     GTK_SIGNAL_FUNC(load_finished_cb), browser);
  // hook up to the change in network status
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "net_state",
		     GTK_SIGNAL_FUNC(net_state_change_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "net_state_all",
		     GTK_SIGNAL_FUNC(net_state_change_all_cb), browser);
  // hookup to changes in progress
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "progress",
		     GTK_SIGNAL_FUNC(progress_change_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "progress_all",
		     GTK_SIGNAL_FUNC(progress_change_all_cb), browser);
  // hookup to changes in over-link message
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "link_message",
		     GTK_SIGNAL_FUNC(link_message_cb), browser);
  // hookup to changes in js status message
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "js_status",
		     GTK_SIGNAL_FUNC(js_status_cb), browser);
  // hookup to see whenever a new window is requested
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "new_window",
		     GTK_SIGNAL_FUNC(new_window_cb), browser);
  // hookup to any requested visibility changes
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "visibility",
		     GTK_SIGNAL_FUNC(visibility_cb), browser);
  // hookup to the signal that says that the browser requested to be
  // destroyed
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "destroy_browser",
		     GTK_SIGNAL_FUNC(destroy_brsr_cb), browser);
  // hookup to the signal that is called when someone clicks on a link
  // to load a new uri
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "open_uri",
		     GTK_SIGNAL_FUNC(open_uri_cb), browser);
  // this signal is emitted when there's a request to change the
  // containing browser window to a certain height, like with width
  // and height args for a window.open in javascript
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "size_to",
		     GTK_SIGNAL_FUNC(size_to_cb), browser);
  // key event signals
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_key_down",
		     GTK_SIGNAL_FUNC(dom_key_down_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_key_press",
		     GTK_SIGNAL_FUNC(dom_key_press_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_key_up",
		     GTK_SIGNAL_FUNC(dom_key_up_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_mouse_down",
		     GTK_SIGNAL_FUNC(dom_mouse_down_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_mouse_up",
		     GTK_SIGNAL_FUNC(dom_mouse_up_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_mouse_click",
		     GTK_SIGNAL_FUNC(dom_mouse_click_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_mouse_dbl_click",
		     GTK_SIGNAL_FUNC(dom_mouse_dbl_click_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_mouse_over",
		     GTK_SIGNAL_FUNC(dom_mouse_over_cb), browser);
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "dom_mouse_out",
		     GTK_SIGNAL_FUNC(dom_mouse_out_cb), browser);
  // hookup to when the window is destroyed
  gtk_signal_connect(GTK_OBJECT(browser->mozEmbed), "destroy",
		     GTK_SIGNAL_FUNC(destroy_cb), browser);
  
  // set the chrome type so it's stored in the object
  gtk_moz_embed_set_chrome_mask(GTK_MOZ_EMBED(browser->mozEmbed),
				actualChromeMask);

  return browser;
}

void
set_browser_visibility (MinimoBrowser *browser, gboolean visibility)
{
  if (!visibility)
  {
    gtk_widget_hide(browser->topLevelWindow);
    return;
  }
  
  // since they are on the same line here...
  if (browser->toolBarOn || browser->locationBarOn)
    gtk_widget_show_all(browser->toolbarHBox);
  else 
    gtk_widget_hide_all(browser->toolbarHBox);

  if (browser->statusBarOn)
    gtk_widget_show_all(browser->progressAreaHBox);
  else
    gtk_widget_hide_all(browser->progressAreaHBox);

  gtk_widget_show(browser->mozEmbed);
  gtk_widget_show(browser->topLevelVBox);
  gtk_widget_show(browser->topLevelWindow);
}

void
back_clicked_cb (GtkButton *button, MinimoBrowser *browser)
{
  gtk_moz_embed_go_back(GTK_MOZ_EMBED(browser->mozEmbed));
}

void stop_reload_clicked_cb (GtkButton *button, MinimoBrowser *browser)
{
  if (browser->loading) {
    gtk_moz_embed_stop_load(GTK_MOZ_EMBED(browser->mozEmbed));
    return;
  }

  GdkModifierType state = (GdkModifierType)0;
  gint x, y;
  gdk_window_get_pointer(NULL, &x, &y, &state);
  
  gtk_moz_embed_reload(GTK_MOZ_EMBED(browser->mozEmbed),
		       (state & GDK_SHIFT_MASK) ?
		       GTK_MOZ_EMBED_FLAG_RELOADBYPASSCACHE : 
		       GTK_MOZ_EMBED_FLAG_RELOADNORMAL);
}

void
forward_clicked_cb (GtkButton *button, MinimoBrowser *browser)
{
  gtk_moz_embed_go_forward(GTK_MOZ_EMBED(browser->mozEmbed));
}

void
home_clicked_cb  (GtkButton *button, MinimoBrowser *browser)
{
  gtk_moz_embed_load_url(GTK_MOZ_EMBED(browser->mozEmbed), MINIMO_HOME_URL);
}
void
url_activate_cb    (GtkEditable *widget, MinimoBrowser *browser)
{
  gchar *text = gtk_editable_get_chars(widget, 0, -1);
  gtk_moz_embed_load_url(GTK_MOZ_EMBED(browser->mozEmbed), text);
  g_free(text);
}
gboolean
delete_cb(GtkWidget *widget, GdkEventAny *event, MinimoBrowser *browser)
{
  gtk_widget_destroy(widget);
  return TRUE;
}

void
destroy_cb         (GtkWidget *widget, MinimoBrowser *browser)
{
  GList *tmp_list;
  num_browsers--;
  tmp_list = g_list_find(browser_list, browser);
  browser_list = g_list_remove_link(browser_list, tmp_list);
  if (browser->tempMessage)
    g_free(browser->tempMessage);
  if (num_browsers == 0)
    gtk_main_quit();
}

void
location_changed_cb (GtkMozEmbed *embed, MinimoBrowser *browser)
{
  char *newLocation;
  int   newPosition = 0;
  newLocation = gtk_moz_embed_get_location(embed);
  if (newLocation)
  {
    gtk_editable_delete_text(GTK_EDITABLE(browser->urlEntry), 0, -1);
    gtk_editable_insert_text(GTK_EDITABLE(browser->urlEntry),
			     newLocation, strlen(newLocation), &newPosition);
    g_free(newLocation);
  }
  else
  // always make sure to clear the tempMessage.  it might have been
  // set from the link before a click and we wouldn't have gotten the
  // callback to unset it.
  update_temp_message(browser, 0);
  // update the nav buttons on a location change
  update_nav_buttons(browser);
}

void
title_changed_cb    (GtkMozEmbed *embed, MinimoBrowser *browser)
{
  char *newTitle;
  newTitle = gtk_moz_embed_get_title(embed);
  if (newTitle)
  {
    gtk_window_set_title(GTK_WINDOW(browser->topLevelWindow), newTitle);
    g_free(newTitle);
  }
  
}

void
load_started_cb     (GtkMozEmbed *embed, MinimoBrowser *browser)
{
  browser->loadPercent = 0;
  browser->bytesLoaded = 0;
  browser->maxBytesLoaded = 0;
  browser->loading = true;
  update_toolbar(browser);
  start_throbber(browser);
  update_status_bar_text(browser);
}

void
load_finished_cb    (GtkMozEmbed *embed, MinimoBrowser *browser)
{
  browser->loadPercent = 0;
  browser->bytesLoaded = 0;
  browser->maxBytesLoaded = 0;
  browser->loading = false;
  update_toolbar(browser);
  update_status_bar_text(browser);
  gtk_progress_set_percentage(GTK_PROGRESS(browser->progressBar), 0);
}


void
net_state_change_cb (GtkMozEmbed *embed, gint flags, guint status,
		     MinimoBrowser *browser)
{
  if (flags & GTK_MOZ_EMBED_FLAG_IS_REQUEST) {
    if (flags & GTK_MOZ_EMBED_FLAG_REDIRECTING)
    browser->statusMessage = "Redirecting to site...";
    else if (flags & GTK_MOZ_EMBED_FLAG_TRANSFERRING)
    browser->statusMessage = "Transferring data from site...";
    else if (flags & GTK_MOZ_EMBED_FLAG_NEGOTIATING)
    browser->statusMessage = "Waiting for authorization...";
  }

  if (status == GTK_MOZ_EMBED_STATUS_FAILED_DNS)
    browser->statusMessage = "Site not found.";
  else if (status == GTK_MOZ_EMBED_STATUS_FAILED_CONNECT)
    browser->statusMessage = "Failed to connect to site.";
  else if (status == GTK_MOZ_EMBED_STATUS_FAILED_TIMEOUT)
    browser->statusMessage = "Failed due to connection timeout.";
  else if (status == GTK_MOZ_EMBED_STATUS_FAILED_USERCANCELED)
    browser->statusMessage = "User canceled connecting to site.";

  if (flags & GTK_MOZ_EMBED_FLAG_IS_DOCUMENT) {
    if (flags & GTK_MOZ_EMBED_FLAG_START)
      browser->statusMessage = "Loading site...";
    else if (flags & GTK_MOZ_EMBED_FLAG_STOP)
      browser->statusMessage = "Done.";
  }

  update_status_bar_text(browser);
  
}

void net_state_change_all_cb (GtkMozEmbed *embed, const char *uri,
				     gint flags, guint status,
				     MinimoBrowser *browser)
{
}

void progress_change_cb   (GtkMozEmbed *embed, gint cur, gint max,
			   MinimoBrowser *browser)
{
  // avoid those pesky divide by zero errors
  if (max < 1)
  {
    gtk_progress_set_activity_mode(GTK_PROGRESS(browser->progressBar), FALSE);
    browser->loadPercent = 0;
    browser->bytesLoaded = cur;
    browser->maxBytesLoaded = 0;
    update_status_bar_text(browser);
  }
  else
  {
    browser->bytesLoaded = cur;
    browser->maxBytesLoaded = max;
    if (cur > max)
      browser->loadPercent = 100;
    else
      browser->loadPercent = (cur * 100) / max;
    update_status_bar_text(browser);
    gtk_progress_set_percentage(GTK_PROGRESS(browser->progressBar), browser->loadPercent / 100.0);
  }
  
}

void progress_change_all_cb (GtkMozEmbed *embed, const char *uri,
			     gint cur, gint max,
			     MinimoBrowser *browser)
{
}

void
link_message_cb      (GtkMozEmbed *embed, MinimoBrowser *browser)
{
  char *message;
  message = gtk_moz_embed_get_link_message(embed);
  if (!message || !*message)
    update_temp_message(browser, 0);
  else
    update_temp_message(browser, message);
  if (message)
    g_free(message);
}

void
js_status_cb (GtkMozEmbed *embed, MinimoBrowser *browser)
{
 char *message;
  message = gtk_moz_embed_get_js_status(embed);
  if (!message || !*message)
    update_temp_message(browser, 0);
  else
    update_temp_message(browser, message);
  if (message)
    g_free(message);
}

void
new_window_cb (GtkMozEmbed *embed, GtkMozEmbed **newEmbed, guint chromemask, MinimoBrowser *browser)
{
  MinimoBrowser *newBrowser = new_gtk_browser(chromemask);
  gtk_widget_set_usize(newBrowser->mozEmbed, 400, 400);
  *newEmbed = GTK_MOZ_EMBED(newBrowser->mozEmbed);
}

void
visibility_cb (GtkMozEmbed *embed, gboolean visibility, MinimoBrowser *browser)
{
  set_browser_visibility(browser, visibility);
}

void
destroy_brsr_cb      (GtkMozEmbed *embed, MinimoBrowser *browser)
{
  gtk_widget_destroy(browser->topLevelWindow);
}

gint
open_uri_cb          (GtkMozEmbed *embed, const char *uri, MinimoBrowser *browser)
{
  // don't interrupt anything
  return FALSE;
}

void
size_to_cb (GtkMozEmbed *embed, gint width, gint height,
	    MinimoBrowser *browser)
{
  gtk_widget_set_usize(browser->mozEmbed, width, height);
}

gint dom_key_down_cb      (GtkMozEmbed *embed, nsIDOMKeyEvent *event,
			   MinimoBrowser *browser)
{
  PRUint32 keyCode = 0;
  event->GetKeyCode(&keyCode);
  return NS_OK;
}

gint dom_key_press_cb     (GtkMozEmbed *embed, nsIDOMKeyEvent *event,
			   MinimoBrowser *browser)
{
  PRUint32 keyCode = 0;
  event->GetCharCode(&keyCode);
  return NS_OK;
}

gint dom_key_up_cb        (GtkMozEmbed *embed, nsIDOMKeyEvent *event,
			   MinimoBrowser *browser)
{
  PRUint32 keyCode = 0;
  event->GetKeyCode(&keyCode);
  return NS_OK;
}

gint dom_mouse_down_cb    (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
			   MinimoBrowser *browser)
{
  return NS_OK;
 }

gint dom_mouse_up_cb      (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
			   MinimoBrowser *browser)
{
  return NS_OK;
}

gint dom_mouse_click_cb   (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
			   MinimoBrowser *browser)
{
  PRUint16 button;
  event->GetButton(&button);
  return NS_OK;
}

gint dom_mouse_dbl_click_cb (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
			     MinimoBrowser *browser)
{
  return NS_OK;
}

gint dom_mouse_over_cb    (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
			   MinimoBrowser *browser)
{
  return NS_OK;
}

gint dom_mouse_out_cb     (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
			   MinimoBrowser *browser)
{
  return NS_OK;
}

void new_window_orphan_cb (GtkMozEmbedSingle *embed,
			   GtkMozEmbed **retval, guint chromemask,
			   gpointer data)
{
  MinimoBrowser *newBrowser = new_gtk_browser(chromemask);
  *retval = GTK_MOZ_EMBED(newBrowser->mozEmbed);
}

// utility functions

void update_toolbar(MinimoBrowser *browser)
{
}


static gboolean update_throbber(gpointer data)
{
  MinimoBrowser *browser = (MinimoBrowser*)data;

  GdkPixmap *val;
  GdkBitmap *mask;

  gboolean loading = browser->loading;

  if (loading)
  {
    browser->throbberID++;
    if (browser->throbberID == MINIMO_THROBBER_ICON_MAX)
      browser->throbberID = 0;
    
    gtk_pixmap_get(GTK_PIXMAP(browser->stopIcon[browser->throbberID]), &val, &mask);
  }
  else
  {
    // set the icon back to the reload icon.
    gtk_pixmap_get(GTK_PIXMAP(browser->reloadIcon), &val, &mask);
  }

  gtk_pixmap_set(GTK_PIXMAP(browser->stopReloadIcon), val, mask);
  gtk_widget_queue_draw(browser->stopReloadIcon);
  gtk_widget_show_now(browser->toolbar);
  gtk_main_iteration_do(false);    

  return loading;
}

void start_throbber(MinimoBrowser *browser)
{
  g_timeout_add(500, update_throbber, (void*)browser);
}

void
update_status_bar_text(MinimoBrowser *browser)
{
  gchar message[256];
  
  gtk_statusbar_pop(GTK_STATUSBAR(browser->statusBar), 1);
  if (browser->tempMessage)
    gtk_statusbar_push(GTK_STATUSBAR(browser->statusBar), 1, browser->tempMessage);
  else
  {
    if (browser->loadPercent)
    {
      g_snprintf(message, 255, "%s (%d%% complete, %d bytes of %d loaded)", browser->statusMessage, browser->loadPercent, browser->bytesLoaded, browser->maxBytesLoaded);
    }
    else if (browser->bytesLoaded)
    {
      g_snprintf(message, 255, "%s (%d bytes loaded)", browser->statusMessage, browser->bytesLoaded);
    }
    else if (browser->statusMessage == NULL)
    {
      g_snprintf(message, 255, " ");
    }
    else
    {
      g_snprintf(message, 255, "%s", browser->statusMessage);
    }
    gtk_statusbar_push(GTK_STATUSBAR(browser->statusBar), 1, message);
  }
}

void
update_temp_message(MinimoBrowser *browser, const char *message)
{
  if (browser->tempMessage)
    g_free(browser->tempMessage);
  if (message)
    browser->tempMessage = g_strdup(message);
  else
    browser->tempMessage = 0;
  // now that we've updated the temp message, redraw the status bar
  update_status_bar_text(browser);
}


void
update_nav_buttons      (MinimoBrowser *browser)
{
  gboolean can_go_back;
  gboolean can_go_forward;
  can_go_back = gtk_moz_embed_can_go_back(GTK_MOZ_EMBED(browser->mozEmbed));
  can_go_forward = gtk_moz_embed_can_go_forward(GTK_MOZ_EMBED(browser->mozEmbed));
  if (can_go_back)
    gtk_widget_set_sensitive(browser->backButton, TRUE);
  else
    gtk_widget_set_sensitive(browser->backButton, FALSE);
  if (can_go_forward)
    gtk_widget_set_sensitive(browser->forwardButton, TRUE);
  else
    gtk_widget_set_sensitive(browser->forwardButton, FALSE);
 }


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
 */

#include "gtkmozembed.h"
#include <gtk/gtk.h>
#include <string.h>
#include "stdlib.h"

// mozilla specific headers
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "prenv.h"

typedef struct _TestGtkBrowser {
  GtkWidget  *topLevelWindow;
  GtkWidget  *topLevelVBox;
  GtkWidget  *menuBar;
  GtkWidget  *fileMenuItem;
  GtkWidget  *fileMenu;
  GtkWidget  *fileOpenNewBrowser;
  GtkWidget  *fileStream;
  GtkWidget  *fileClose;
  GtkWidget  *fileQuit;
  GtkWidget  *toolbarHBox;
  GtkWidget  *toolbar;
  GtkWidget  *backButton;
  GtkWidget  *stopButton;
  GtkWidget  *forwardButton;
  GtkWidget  *reloadButton;
  GtkWidget  *urlEntry;
  GtkWidget  *mozEmbed;
  GtkWidget  *progressAreaHBox;
  GtkWidget  *progressBar;
  GtkWidget  *statusAlign;
  GtkWidget  *statusBar;
  const char *statusMessage;
  int         loadPercent;
  int         bytesLoaded;
  int         maxBytesLoaded;
  char       *tempMessage;
  gboolean menuBarOn;
  gboolean toolBarOn;
  gboolean locationBarOn;
  gboolean statusBarOn;

} TestGtkBrowser;

// the list of browser windows currently open
GList *browser_list = g_list_alloc();

static TestGtkBrowser *new_gtk_browser    (guint32 chromeMask);
static void            set_browser_visibility (TestGtkBrowser *browser,
					       gboolean visibility);

static int num_browsers = 0;

// callbacks from the UI
static void     back_clicked_cb    (GtkButton   *button, 
				    TestGtkBrowser *browser);
static void     stop_clicked_cb    (GtkButton   *button,
				    TestGtkBrowser *browser);
static void     forward_clicked_cb (GtkButton   *button,
				    TestGtkBrowser *browser);
static void     reload_clicked_cb  (GtkButton   *button,
				    TestGtkBrowser *browser);
static void     url_activate_cb    (GtkEditable *widget, 
				    TestGtkBrowser *browser);
static void     menu_open_new_cb   (GtkMenuItem *menuitem,
				    TestGtkBrowser *browser);
static void     menu_stream_cb     (GtkMenuItem *menuitem,
				    TestGtkBrowser *browser);
static void     menu_close_cb      (GtkMenuItem *menuitem,
				    TestGtkBrowser *browser);
static void     menu_quit_cb       (GtkMenuItem *menuitem,
				    TestGtkBrowser *browser);
static gboolean delete_cb          (GtkWidget *widget, GdkEventAny *event,
				    TestGtkBrowser *browser);
static void     destroy_cb         (GtkWidget *widget,
				    TestGtkBrowser *browser);

// callbacks from the widget
static void location_changed_cb  (GtkMozEmbed *embed, TestGtkBrowser *browser);
static void title_changed_cb     (GtkMozEmbed *embed, TestGtkBrowser *browser);
static void load_started_cb      (GtkMozEmbed *embed, TestGtkBrowser *browser);
static void load_finished_cb     (GtkMozEmbed *embed, TestGtkBrowser *browser);
static void net_state_change_cb  (GtkMozEmbed *embed, gint flags,
				  guint status, TestGtkBrowser *browser);
static void net_state_change_all_cb (GtkMozEmbed *embed, const char *uri,
				     gint flags, guint status,
				     TestGtkBrowser *browser);
static void progress_change_cb   (GtkMozEmbed *embed, gint cur, gint max,
				  TestGtkBrowser *browser);
static void progress_change_all_cb (GtkMozEmbed *embed, const char *uri,
				    gint cur, gint max,
				    TestGtkBrowser *browser);
static void link_message_cb      (GtkMozEmbed *embed, TestGtkBrowser *browser);
static void js_status_cb         (GtkMozEmbed *embed, TestGtkBrowser *browser);
static void new_window_cb        (GtkMozEmbed *embed,
				  GtkMozEmbed **retval, guint chromemask,
				  TestGtkBrowser *browser);
static void visibility_cb        (GtkMozEmbed *embed, 
				  gboolean visibility,
				  TestGtkBrowser *browser);
static void destroy_brsr_cb      (GtkMozEmbed *embed, TestGtkBrowser *browser);
static gint open_uri_cb          (GtkMozEmbed *embed, const char *uri,
				  TestGtkBrowser *browser);
static void size_to_cb           (GtkMozEmbed *embed, gint width,
				  gint height, TestGtkBrowser *browser);
static gint dom_key_down_cb      (GtkMozEmbed *embed, nsIDOMKeyEvent *event,
				  TestGtkBrowser *browser);
static gint dom_key_press_cb     (GtkMozEmbed *embed, nsIDOMKeyEvent *event,
				  TestGtkBrowser *browser);
static gint dom_key_up_cb        (GtkMozEmbed *embed, nsIDOMKeyEvent *event,
				  TestGtkBrowser *browser);
static gint dom_mouse_down_cb    (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
				  TestGtkBrowser *browser);
static gint dom_mouse_up_cb      (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
				  TestGtkBrowser *browser);
static gint dom_mouse_click_cb   (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
				  TestGtkBrowser *browser);
static gint dom_mouse_dbl_click_cb (GtkMozEmbed *embed, 
				  nsIDOMMouseEvent *event,
				  TestGtkBrowser *browser);
static gint dom_mouse_over_cb    (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
				  TestGtkBrowser *browser);
static gint dom_mouse_out_cb     (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
				  TestGtkBrowser *browser);

// callbacks from the singleton object
static void new_window_orphan_cb (GtkMozEmbedSingle *embed,
				  GtkMozEmbed **retval, guint chromemask,
				  gpointer data);

// some utility functions
static void update_status_bar_text  (TestGtkBrowser *browser);
static void update_temp_message     (TestGtkBrowser *browser,
				     const char *message);
static void update_nav_buttons      (TestGtkBrowser *browser);

int
main(int argc, char **argv)
{
  gtk_set_locale();
  gtk_init(&argc, &argv);

  char *home_path;
  char *full_path;
  home_path = PR_GetEnv("HOME");
  if (!home_path) {
    fprintf(stderr, "Failed to get HOME\n");
    exit(1);
  }
  
  full_path = g_strdup_printf("%s/%s", home_path, ".TestGtkEmbed");
  
  gtk_moz_embed_set_profile_path(full_path, "TestGtkEmbed");

  TestGtkBrowser *browser = new_gtk_browser(GTK_MOZ_EMBED_FLAG_DEFAULTCHROME);

  // set our minimum size
  gtk_widget_set_usize(browser->mozEmbed, 400, 400);

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

static TestGtkBrowser *
new_gtk_browser(guint32 chromeMask)
{
  guint32         actualChromeMask = chromeMask;
  TestGtkBrowser *browser = 0;

  num_browsers++;

  browser = g_new0(TestGtkBrowser, 1);

  browser_list = g_list_prepend(browser_list, browser);

  browser->menuBarOn = FALSE;
  browser->toolBarOn = FALSE;
  browser->locationBarOn = FALSE;
  browser->statusBarOn = FALSE;

  g_print("new_gtk_browser\n");

  if (chromeMask == GTK_MOZ_EMBED_FLAG_DEFAULTCHROME)
    actualChromeMask = GTK_MOZ_EMBED_FLAG_ALLCHROME;

  if (actualChromeMask & GTK_MOZ_EMBED_FLAG_MENUBARON)
  {
    browser->menuBarOn = TRUE;
    g_print("\tmenu bar\n");
  }
  if (actualChromeMask & GTK_MOZ_EMBED_FLAG_TOOLBARON)
  {
    browser->toolBarOn = TRUE;
    g_print("\ttool bar\n");
  }
  if (actualChromeMask & GTK_MOZ_EMBED_FLAG_LOCATIONBARON)
  {
    browser->locationBarOn = TRUE;
    g_print("\tlocation bar\n");
  }
  if (actualChromeMask & GTK_MOZ_EMBED_FLAG_STATUSBARON)
  {
    browser->statusBarOn = TRUE;
    g_print("\tstatus bar\n");
  }

  // create our new toplevel window
  browser->topLevelWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  // new vbox
  browser->topLevelVBox = gtk_vbox_new(FALSE, 0);
  // add it to the toplevel window
  gtk_container_add(GTK_CONTAINER(browser->topLevelWindow),
		    browser->topLevelVBox);
  // create our menu bar
  browser->menuBar = gtk_menu_bar_new();
  // create the file menu
  browser->fileMenuItem = gtk_menu_item_new_with_label("File");
  browser->fileMenu = gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM(browser->fileMenuItem),
			     browser->fileMenu);

  browser->fileOpenNewBrowser = 
    gtk_menu_item_new_with_label("Open New Browser");
  gtk_menu_append(GTK_MENU(browser->fileMenu),
		  browser->fileOpenNewBrowser);
  
  browser->fileStream =
    gtk_menu_item_new_with_label("Test Stream");
  gtk_menu_append(GTK_MENU(browser->fileMenu),
		  browser->fileStream);

  browser->fileClose =
    gtk_menu_item_new_with_label("Close");
  gtk_menu_append(GTK_MENU(browser->fileMenu),
		  browser->fileClose);

  browser->fileQuit =
    gtk_menu_item_new_with_label("Quit");
  gtk_menu_append(GTK_MENU(browser->fileMenu),
		  browser->fileQuit);
  
  // append it
  gtk_menu_bar_append(GTK_MENU_BAR(browser->menuBar), browser->fileMenuItem);

  // add it to the vbox
  gtk_box_pack_start(GTK_BOX(browser->topLevelVBox),
		     browser->menuBar,
		     FALSE, // expand
		     FALSE, // fill
		     0);    // padding
  // create the hbox that will contain the toolbar and the url text entry bar
  browser->toolbarHBox = gtk_hbox_new(FALSE, 0);
  // add that hbox to the vbox
  gtk_box_pack_start(GTK_BOX(browser->topLevelVBox), 
		     browser->toolbarHBox,
		     FALSE, // expand
		     FALSE, // fill
		     0);    // padding
  // new horiz toolbar with buttons + icons
  browser->toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL,
				     GTK_TOOLBAR_BOTH);
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
			    0, // XXX replace with icon
			    GTK_SIGNAL_FUNC(back_clicked_cb),
			    browser);
  // new stop button
  browser->stopButton = 
    gtk_toolbar_append_item(GTK_TOOLBAR(browser->toolbar),
			    "Stop",
			    "Stop",
			    "Stop",
			    0, // XXX replace with icon
			    GTK_SIGNAL_FUNC(stop_clicked_cb),
			    browser);
  // new forward button
  browser->forwardButton =
    gtk_toolbar_append_item(GTK_TOOLBAR(browser->toolbar),
			    "Forward",
			    "Forward",
			    "Forward",
			    0, // XXX replace with icon
			    GTK_SIGNAL_FUNC(forward_clicked_cb),
			    browser);
  // new reload button
  browser->reloadButton = 
    gtk_toolbar_append_item(GTK_TOOLBAR(browser->toolbar),
			    "Reload",
			    "Reload",
			    "Reload",
			    0, // XXX replace with icon
			    GTK_SIGNAL_FUNC(reload_clicked_cb),
			    browser);
  // create the url text entry
  browser->urlEntry = gtk_entry_new();
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
  // add it to the vbox
  gtk_box_pack_start(GTK_BOX(browser->topLevelVBox), browser->progressAreaHBox,
		     FALSE, // expand
		     FALSE, // fill
		     0);   // padding
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
  gtk_widget_set_sensitive(browser->stopButton, FALSE);
  gtk_widget_set_sensitive(browser->forwardButton, FALSE);
  gtk_widget_set_sensitive(browser->reloadButton, FALSE);
  
  // catch the destruction of the toplevel window
  gtk_signal_connect(GTK_OBJECT(browser->topLevelWindow), "delete_event",
		     GTK_SIGNAL_FUNC(delete_cb), browser);

  // hook up the activate signal to the right callback
  gtk_signal_connect(GTK_OBJECT(browser->urlEntry), "activate",
		     GTK_SIGNAL_FUNC(url_activate_cb), browser);

  // hook up to the open new browser activation
  gtk_signal_connect(GTK_OBJECT(browser->fileOpenNewBrowser), "activate",
		     GTK_SIGNAL_FUNC(menu_open_new_cb), browser);
  // hook up to the stream test
  gtk_signal_connect(GTK_OBJECT(browser->fileStream), "activate",
		     GTK_SIGNAL_FUNC(menu_stream_cb), browser);
  // close this window
  gtk_signal_connect(GTK_OBJECT(browser->fileClose), "activate",
		     GTK_SIGNAL_FUNC(menu_close_cb), browser);
  // quit the application
  gtk_signal_connect(GTK_OBJECT(browser->fileQuit), "activate",
		     GTK_SIGNAL_FUNC(menu_quit_cb), browser);

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
set_browser_visibility (TestGtkBrowser *browser, gboolean visibility)
{
  if (!visibility)
  {
    gtk_widget_hide(browser->topLevelWindow);
    return;
  }
  
  if (browser->menuBarOn)
    gtk_widget_show_all(browser->menuBar);
  else
    gtk_widget_hide_all(browser->menuBar);

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
back_clicked_cb (GtkButton *button, TestGtkBrowser *browser)
{
  gtk_moz_embed_go_back(GTK_MOZ_EMBED(browser->mozEmbed));
}

void
stop_clicked_cb (GtkButton *button, TestGtkBrowser *browser)
{
  g_print("stop_clicked_cb\n");
  gtk_moz_embed_stop_load(GTK_MOZ_EMBED(browser->mozEmbed));
}

void
forward_clicked_cb (GtkButton *button, TestGtkBrowser *browser)
{
  g_print("forward_clicked_cb\n");
  gtk_moz_embed_go_forward(GTK_MOZ_EMBED(browser->mozEmbed));
}

void
reload_clicked_cb  (GtkButton *button, TestGtkBrowser *browser)
{
  g_print("reload_clicked_cb\n");
  GdkModifierType state = (GdkModifierType)0;
  gint x, y;
  gdk_window_get_pointer(NULL, &x, &y, &state);
  
  gtk_moz_embed_reload(GTK_MOZ_EMBED(browser->mozEmbed),
		       (state & GDK_SHIFT_MASK) ?
		       GTK_MOZ_EMBED_FLAG_RELOADBYPASSCACHE : 
		       GTK_MOZ_EMBED_FLAG_RELOADNORMAL);
}

void 
stream_clicked_cb  (GtkButton   *button, TestGtkBrowser *browser)
{
  const char *data;
  const char *data2;
  data = "<html>Hi";
  data2 = " there</html>\n";
  g_print("stream_clicked_cb\n");
  gtk_moz_embed_open_stream(GTK_MOZ_EMBED(browser->mozEmbed),
			    "file://", "text/html");
  gtk_moz_embed_append_data(GTK_MOZ_EMBED(browser->mozEmbed),
			    data, strlen(data));
  gtk_moz_embed_append_data(GTK_MOZ_EMBED(browser->mozEmbed),
			    data2, strlen(data2));
  gtk_moz_embed_close_stream(GTK_MOZ_EMBED(browser->mozEmbed));
}

void
url_activate_cb    (GtkEditable *widget, TestGtkBrowser *browser)
{
  gchar *text = gtk_editable_get_chars(widget, 0, -1);
  g_print("loading url %s\n", text);
  gtk_moz_embed_load_url(GTK_MOZ_EMBED(browser->mozEmbed), text);
  g_free(text);
}

void
menu_open_new_cb   (GtkMenuItem *menuitem, TestGtkBrowser *browser)
{
  g_print("opening new browser.\n");
  TestGtkBrowser *newBrowser = 
    new_gtk_browser(GTK_MOZ_EMBED_FLAG_DEFAULTCHROME);
  gtk_widget_set_usize(newBrowser->mozEmbed, 400, 400);
  set_browser_visibility(newBrowser, TRUE);
}

void
menu_stream_cb     (GtkMenuItem *menuitem, TestGtkBrowser *browser)
{
  g_print("menu_stream_cb\n");
  const char *data;
  const char *data2;
  data = "<html>Hi";
  data2 = " there</html>\n";
  g_print("stream_clicked_cb\n");
  gtk_moz_embed_open_stream(GTK_MOZ_EMBED(browser->mozEmbed),
			    "file://", "text/html");
  gtk_moz_embed_append_data(GTK_MOZ_EMBED(browser->mozEmbed),
			    data, strlen(data));
  gtk_moz_embed_append_data(GTK_MOZ_EMBED(browser->mozEmbed),
			    data2, strlen(data2));
  gtk_moz_embed_close_stream(GTK_MOZ_EMBED(browser->mozEmbed));
}

void
menu_close_cb (GtkMenuItem *menuitem, TestGtkBrowser *browser)
{
  gtk_widget_destroy(browser->topLevelWindow);
}

void
menu_quit_cb (GtkMenuItem *menuitem, TestGtkBrowser *browser)
{
  TestGtkBrowser *tmpBrowser;
  GList *tmp_list = browser_list;
  tmpBrowser = (TestGtkBrowser *)tmp_list->data;
  while (tmpBrowser) {
    tmp_list = tmp_list->next;
    gtk_widget_destroy(tmpBrowser->topLevelWindow);
    tmpBrowser = (TestGtkBrowser *)tmp_list->data;
  }
}

gboolean
delete_cb(GtkWidget *widget, GdkEventAny *event, TestGtkBrowser *browser)
{
  g_print("delete_cb\n");
  gtk_widget_destroy(widget);
  return TRUE;
}

void
destroy_cb         (GtkWidget *widget, TestGtkBrowser *browser)
{
  GList *tmp_list;
  g_print("destroy_cb\n");
  num_browsers--;
  tmp_list = g_list_find(browser_list, browser);
  browser_list = g_list_remove_link(browser_list, tmp_list);
  if (browser->tempMessage)
    g_free(browser->tempMessage);
  if (num_browsers == 0)
    gtk_main_quit();
}

void
location_changed_cb (GtkMozEmbed *embed, TestGtkBrowser *browser)
{
  char *newLocation;
  int   newPosition = 0;
  g_print("location_changed_cb\n");
  newLocation = gtk_moz_embed_get_location(embed);
  if (newLocation)
  {
    gtk_editable_delete_text(GTK_EDITABLE(browser->urlEntry), 0, -1);
    gtk_editable_insert_text(GTK_EDITABLE(browser->urlEntry),
			     newLocation, strlen(newLocation), &newPosition);
    g_free(newLocation);
  }
  else
    g_print("failed to get location!\n");
  // always make sure to clear the tempMessage.  it might have been
  // set from the link before a click and we wouldn't have gotten the
  // callback to unset it.
  update_temp_message(browser, 0);
  // update the nav buttons on a location change
  update_nav_buttons(browser);
}

void
title_changed_cb    (GtkMozEmbed *embed, TestGtkBrowser *browser)
{
  char *newTitle;
  g_print("title_changed_cb\n");
  newTitle = gtk_moz_embed_get_title(embed);
  if (newTitle)
  {
    gtk_window_set_title(GTK_WINDOW(browser->topLevelWindow), newTitle);
    g_free(newTitle);
  }
  
}

void
load_started_cb     (GtkMozEmbed *embed, TestGtkBrowser *browser)
{
  g_print("load_started_cb\n");
  gtk_widget_set_sensitive(browser->stopButton, TRUE);
  gtk_widget_set_sensitive(browser->reloadButton, FALSE);
  browser->loadPercent = 0;
  browser->bytesLoaded = 0;
  browser->maxBytesLoaded = 0;
  update_status_bar_text(browser);
}

void
load_finished_cb    (GtkMozEmbed *embed, TestGtkBrowser *browser)
{
  g_print("load_finished_cb\n");
  gtk_widget_set_sensitive(browser->stopButton, FALSE);
  gtk_widget_set_sensitive(browser->reloadButton, TRUE);
  browser->loadPercent = 0;
  browser->bytesLoaded = 0;
  browser->maxBytesLoaded = 0;
  update_status_bar_text(browser);
  gtk_progress_set_percentage(GTK_PROGRESS(browser->progressBar), 0);
}


void
net_state_change_cb (GtkMozEmbed *embed, gint flags, guint status,
		     TestGtkBrowser *browser)
{
  g_print("net_state_change_cb %d\n", flags);
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
				     TestGtkBrowser *browser)
{
  //  g_print("net_state_change_all_cb %s %d %d\n", uri, flags, status);
}

void progress_change_cb   (GtkMozEmbed *embed, gint cur, gint max,
			   TestGtkBrowser *browser)
{
  g_print("progress_change_cb cur %d max %d\n", cur, max);

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
			     TestGtkBrowser *browser)
{
  //g_print("progress_change_all_cb %s cur %d max %d\n", uri, cur, max);
}

void
link_message_cb      (GtkMozEmbed *embed, TestGtkBrowser *browser)
{
  char *message;
  g_print("link_message_cb\n");
  message = gtk_moz_embed_get_link_message(embed);
  if (message && (strlen(message) == 0))
    update_temp_message(browser, 0);
  else
    update_temp_message(browser, message);
  if (message)
    g_free(message);
}

void
js_status_cb (GtkMozEmbed *embed, TestGtkBrowser *browser)
{
 char *message;
  g_print("js_status_cb\n");
  message = gtk_moz_embed_get_js_status(embed);
  if (message && (strlen(message) == 0))
    update_temp_message(browser, 0);
  else
    update_temp_message(browser, message);
  if (message)
    g_free(message);
}

void
new_window_cb (GtkMozEmbed *embed, GtkMozEmbed **newEmbed, guint chromemask, TestGtkBrowser *browser)
{
  g_print("new_window_cb\n");
  g_print("embed is %p chromemask is %d\n", (void *)embed, chromemask);
  TestGtkBrowser *newBrowser = new_gtk_browser(chromemask);
  gtk_widget_set_usize(newBrowser->mozEmbed, 400, 400);
  *newEmbed = GTK_MOZ_EMBED(newBrowser->mozEmbed);
  g_print("new browser is %p\n", (void *)*newEmbed);
}

void
visibility_cb (GtkMozEmbed *embed, gboolean visibility, TestGtkBrowser *browser)
{
  g_print("visibility_cb %d\n", visibility);
  set_browser_visibility(browser, visibility);
}

void
destroy_brsr_cb      (GtkMozEmbed *embed, TestGtkBrowser *browser)
{
  g_print("destroy_brsr_cb\n");
  gtk_widget_destroy(browser->topLevelWindow);
}

gint
open_uri_cb          (GtkMozEmbed *embed, const char *uri, TestGtkBrowser *browser)
{
  g_print("open_uri_cb %s\n", uri);

  // interrupt this test load
  if (!strcmp(uri, "http://people.redhat.com/blizzard/monkeys.txt"))
    return TRUE;
  // don't interrupt anything
  return FALSE;
}

void
size_to_cb (GtkMozEmbed *embed, gint width, gint height,
	    TestGtkBrowser *browser)
{
  g_print("*** size_to_cb %d %d\n", width, height);
  gtk_widget_set_usize(browser->mozEmbed, width, height);
}

gint dom_key_down_cb      (GtkMozEmbed *embed, nsIDOMKeyEvent *event,
			   TestGtkBrowser *browser)
{
  PRUint32 keyCode = 0;
  //  g_print("dom_key_down_cb\n");
  event->GetKeyCode(&keyCode);
  // g_print("key code is %d\n", keyCode);
  return NS_OK;
}

gint dom_key_press_cb     (GtkMozEmbed *embed, nsIDOMKeyEvent *event,
			   TestGtkBrowser *browser)
{
  PRUint32 keyCode = 0;
  // g_print("dom_key_press_cb\n");
  event->GetCharCode(&keyCode);
  // g_print("char code is %d\n", keyCode);
  return NS_OK;
}

gint dom_key_up_cb        (GtkMozEmbed *embed, nsIDOMKeyEvent *event,
			   TestGtkBrowser *browser)
{
  PRUint32 keyCode = 0;
  // g_print("dom_key_up_cb\n");
  event->GetKeyCode(&keyCode);
  // g_print("key code is %d\n", keyCode);
  return NS_OK;
}

gint dom_mouse_down_cb    (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
			   TestGtkBrowser *browser)
{
  //  g_print("dom_mouse_down_cb\n");
  return NS_OK;
 }

gint dom_mouse_up_cb      (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
			   TestGtkBrowser *browser)
{
  //  g_print("dom_mouse_up_cb\n");
  return NS_OK;
}

gint dom_mouse_click_cb   (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
			   TestGtkBrowser *browser)
{
  //  g_print("dom_mouse_click_cb\n");
  PRUint16 button;
  event->GetButton(&button);
  printf("button was %d\n", button);
  return NS_OK;
}

gint dom_mouse_dbl_click_cb (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
			     TestGtkBrowser *browser)
{
  //  g_print("dom_mouse_dbl_click_cb\n");
  return NS_OK;
}

gint dom_mouse_over_cb    (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
			   TestGtkBrowser *browser)
{
  //g_print("dom_mouse_over_cb\n");
  return NS_OK;
}

gint dom_mouse_out_cb     (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
			   TestGtkBrowser *browser)
{
  //g_print("dom_mouse_out_cb\n");
  return NS_OK;
}

void new_window_orphan_cb (GtkMozEmbedSingle *embed,
			   GtkMozEmbed **retval, guint chromemask,
			   gpointer data)
{
  g_print("new_window_orphan_cb\n");
  g_print("chromemask is %d\n", chromemask);
  TestGtkBrowser *newBrowser = new_gtk_browser(chromemask);
  *retval = GTK_MOZ_EMBED(newBrowser->mozEmbed);
  g_print("new browser is %p\n", (void *)*retval);
}

// utility functions

void
update_status_bar_text(TestGtkBrowser *browser)
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
update_temp_message(TestGtkBrowser *browser, const char *message)
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
update_nav_buttons      (TestGtkBrowser *browser)
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


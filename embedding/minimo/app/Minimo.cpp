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

#include "gtkmozembed.h"
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

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

#define MINIMO_HOME_URL "file:///usr/lib/mozilla-minimo/startpage/start.html"

#define MINIMO_THROBBER_COLOR_INTERVAL 100
#define MINIMO_THROBBER_COLOR_STEP     500
#define MINIMO_THROBBER_COLOR_MIN      40000
#define MINIMO_THROBBER_COLOR_MAX      65000

typedef struct _MinimoBrowser {
  GtkWidget  *topLevelWindow;
  GtkWidget  *topLevelVBox;
  GtkWidget  *toolbarVPaned;
  GtkWidget  *urlEntry;
  GtkWidget  *mozEmbed;

  bool        mouseDown;
  guint       mouseDownTimer;

  const char *statusMessage;
  int         loadPercent;
  int         bytesLoaded;
  int         maxBytesLoaded;
  char       *tempMessage;

  int         throbberID;
  int         throbberDirection;

  gboolean toolBarOn;
  gboolean locationBarOn;
  gboolean statusBarOn;
  gboolean loading;
} MinimoBrowser;

static char *g_profile_path = NULL;

// the list of browser windows currently open
GList *browser_list = g_list_alloc();

// create our auto completion object 
static GCompletion *g_AutoComplete;

// populate it.



static MinimoBrowser *new_gtk_browser    (guint32 chromeMask);
static void            set_browser_visibility (MinimoBrowser *browser,
					       gboolean visibility);
static int num_browsers = 0;

// callbacks from the UI
static void     url_activate_cb    (GtkEditable *widget, 
				    MinimoBrowser *browser);
static gboolean url_key_press_cb    (GtkEditable *widget, 
				     GdkEventKey* event, 
				     MinimoBrowser *browser);
static gboolean delete_cb          (GtkWidget *widget, 
				    GdkEventAny *event,
				    MinimoBrowser *browser);
static void     destroy_cb         (GtkWidget *widget,
				    MinimoBrowser *browser);

static gboolean context_menu_cb    (void *data);

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

// some utility functions
static void start_throbber          (MinimoBrowser *browser);
static void update_temp_message     (MinimoBrowser *browser,
				     const char *message);

static const gchar* gCommonCompletes[] =
{
  "http://",
  "https://",
  "www.",
  "http://www.",
  "https://www.",
  NULL
};

static void init_autocomplete()
{
  if (!g_AutoComplete)
    g_AutoComplete = g_completion_new(NULL);

  char* full_path = g_strdup_printf("%s/%s", g_profile_path, "autocomplete.txt");
  
  FILE *fp;
  char url[255];
  GList* list = g_list_alloc();
	 
  if((fp = fopen(full_path, "r"))) 
  {
    while(fgets(url, sizeof(url) - 1, fp))
    {
      int length = strlen(url);
      if (url[length-1] == '\n')
	url[length-1] = '\0'; 

      // should be smarter!
      list->data = g_strdup(url);
      
      g_completion_add_items(g_AutoComplete, list);
    }
    fclose(fp);
  }

  for (int i=0; gCommonCompletes[i] != NULL; i++)
  {
    list->data = g_strdup(gCommonCompletes[i]);
    g_completion_add_items(g_AutoComplete, list);
  }

  g_list_free(list);
  g_free(full_path);
  return;
}

static void add_autocomplete(const char* value)
{
  if (g_AutoComplete)
  {
      GList* list = g_list_alloc();
      list->data = g_strdup(value);
      g_completion_add_items(g_AutoComplete, list);
      g_list_free(list);
  }

  char* full_path = g_strdup_printf("%s/%s", g_profile_path, "autocomplete.txt");
  
  FILE *fp;
  if((fp = fopen(full_path, "a"))) 
  {
    fwrite(value, strlen(value), 1, fp);
    fputc('\n', fp);
    fclose(fp);
  }
  
  g_free(full_path);
  return;
}

/* This lets us open an URL by remote control.  Trying to mimic the
   behavior of mosaic like dillo remote
 */

static void handle_remote(int sig)
{
  FILE *fp;
  char url[256];
	 
  sprintf(url, "/tmp/Mosaic.%d", getpid());
  if((fp = fopen(url, "r"))) 
  {
    if(fgets(url, sizeof(url) - 1, fp))
    {
      MinimoBrowser *bw = NULL;
      if (strncmp(url, "goto", 4) == 0 && fgets(url, sizeof(url) - 1, fp)) 
      {
	GList *tmp_list = browser_list;
	bw = (MinimoBrowser *)tmp_list->data;
	if(!bw) return;
      }
      else if (strncmp(url, "newwin", 6) == 0 && fgets(url, sizeof(url) - 1, fp))
      {
	bw = new_gtk_browser(GTK_MOZ_EMBED_FLAG_DEFAULTCHROME);
	gtk_widget_set_usize(bw->mozEmbed, 240, 320);
	set_browser_visibility(bw, TRUE);
      }
      if (bw)
	gtk_moz_embed_load_url(GTK_MOZ_EMBED(bw->mozEmbed), url);
      fclose(fp);
    }
  }
  return;
}

static void init_remote()
{
  gchar *file;
  FILE *fp;

  signal(SIGUSR1, SIG_IGN);

  /* Write the pidfile : would be useful for automation process*/
  file = g_strconcat(g_get_home_dir(), "/", ".mosaicpid", NULL);
  if((fp = fopen(file, "w"))) 
  {
    fprintf (fp, "%d\n", getpid());
    fclose (fp);
    signal(SIGUSR1, handle_remote);
  }
  g_free(file);
}

static void cleanup_remote()
{
  gchar *file;

  signal(SIGUSR1, SIG_IGN);

  file = g_strconcat(g_get_home_dir(), "/", ".mosaicpid", NULL);
  unlink(file);
  g_free(file);
}

int
main(int argc, char **argv)
{
#ifdef NS_TRACE_MALLOC
  argc = NS_TraceMallocStartupArgs(argc, argv);
#endif

  gtk_set_locale();
  gtk_init(&argc, &argv);

  char *home_path;
  home_path = PR_GetEnv("HOME");
  if (!home_path) {
    fprintf(stderr, "Failed to get HOME\n");
    exit(1);
  }
  
  g_profile_path = g_strdup_printf("%s/%s", home_path, ".Minimo");
  
  gtk_moz_embed_set_profile_path(g_profile_path, "Minimo");

  init_autocomplete();

  MinimoBrowser *browser = new_gtk_browser(GTK_MOZ_EMBED_FLAG_DEFAULTCHROME);

  // set our minimum size
  gtk_widget_set_usize(browser->mozEmbed, 240, 320);

  set_browser_visibility(browser, TRUE);

  init_remote();

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

  cleanup_remote();
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

  // create the paned that will contain the url text entry bar
  browser->toolbarVPaned = gtk_vpaned_new();
  gtk_paned_set_position(GTK_PANED(browser->toolbarVPaned), 110);

  // add that paned to the vbox
  gtk_box_pack_start(GTK_BOX(browser->topLevelVBox), 
		     browser->toolbarVPaned,
		     FALSE, // expand
		     FALSE, // fill
		     0);    // padding

  // create the url text entry
  browser->urlEntry = gtk_entry_new();
  browser->throbberID  = MINIMO_THROBBER_COLOR_MAX;
  browser->throbberDirection = -1;

  // add it to the paned
  gtk_paned_pack2(GTK_PANED(browser->toolbarVPaned), 
		  browser->urlEntry,
		  true, // resize
		  true);

  // create our new gtk moz embed widget
  browser->mozEmbed = gtk_moz_embed_new();
  // add it to the toplevel vbox
  gtk_box_pack_start(GTK_BOX(browser->topLevelVBox), browser->mozEmbed,
		     TRUE, // expand
		     TRUE, // fill
		     0);   // padding

  
  // catch the destruction of the toplevel window
  gtk_signal_connect(GTK_OBJECT(browser->topLevelWindow), "delete_event",
		     GTK_SIGNAL_FUNC(delete_cb), browser);

  // hook up the activate signal to the right callback
  gtk_signal_connect(GTK_OBJECT(browser->urlEntry), "activate",
		     GTK_SIGNAL_FUNC(url_activate_cb), browser);

  // hook up autocomplete fudge
  gtk_signal_connect(GTK_OBJECT(browser->urlEntry), "key_press_event",
		     GTK_SIGNAL_FUNC(url_key_press_cb), browser);

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
    gtk_widget_show_all(browser->toolbarVPaned);
  else 
    gtk_widget_hide_all(browser->toolbarVPaned);

  gtk_widget_show(browser->mozEmbed);
  gtk_widget_show(browser->topLevelVBox);
  gtk_widget_show(browser->topLevelWindow);
}

void
back_clicked_cb (GtkButton *button, MinimoBrowser *browser)
{
  gtk_moz_embed_go_back(GTK_MOZ_EMBED(browser->mozEmbed));
}

void reload_clicked_cb (GtkButton *button, MinimoBrowser *browser)
{
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
  fprintf(stdout, "woot! %x\n", browser);
  gtk_moz_embed_load_url(GTK_MOZ_EMBED(browser->mozEmbed), MINIMO_HOME_URL);
}

void
url_activate_cb    (GtkEditable *widget, MinimoBrowser *browser)
{
  gchar *text = gtk_editable_get_chars(widget, 0, -1);
  gtk_moz_embed_load_url(GTK_MOZ_EMBED(browser->mozEmbed), text);
  add_autocomplete(text);
  g_free(text);
}

gboolean
url_key_press_cb    (GtkEditable *widget, 
		     GdkEventKey* event, 
		     MinimoBrowser *browser)
{
  gboolean result = FALSE;

  if (!g_AutoComplete)
    return result;
 
  // if the user hits backspace, let them.
  if (event->keyval == GDK_BackSpace)
    return result;

  gchar *text = gtk_editable_get_chars(widget, 0, -1);
  gint length = strlen(text);
  
  gchar *newText = (gchar*) malloc(length + 2);
  memcpy(newText, text, length);
  memcpy(newText+length, &(event->keyval), 1);
  newText[length+1] = '\0';
  
  g_free(text);
  
  gchar *completion = NULL;
  g_completion_complete(g_AutoComplete, newText, &completion);

  if (completion)
  {
    // if the completion is something more than the what is in the entry field.
    if (strcmp(completion, newText))
    {
      gtk_entry_set_text(GTK_ENTRY(widget), completion);
      gtk_editable_select_region(GTK_EDITABLE(widget), length+1, -1);
      gtk_editable_set_position(GTK_EDITABLE(widget), -1);//length+1);

      // selection is still fucked /***/
      result = TRUE;
    }
    g_free(completion);
  }
  g_free(newText);
  return result;
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

gboolean context_menu_cb(gpointer data)
{
  MinimoBrowser *browser = (MinimoBrowser*)data;
  fprintf(stdout, "foopy! %x\n", browser);

  GtkMenu *menu = (GtkMenu*) gtk_menu_new();

  GtkWidget *back_item = gtk_menu_item_new_with_label ("Back");
  GtkWidget *forward_item = gtk_menu_item_new_with_label ("Forward");
  GtkWidget *home_item = gtk_menu_item_new_with_label ("Home");
  GtkWidget *reload_item = gtk_menu_item_new_with_label ("Reload");

  gtk_menu_append(menu, home_item);
  gtk_menu_append(menu, back_item);
  gtk_menu_append(menu, forward_item);
  gtk_menu_append(menu, reload_item);
  
  gtk_widget_show (home_item);
  gtk_widget_show (back_item);
  gtk_widget_show (forward_item);
  gtk_widget_show (reload_item);

  gtk_signal_connect(GTK_OBJECT(home_item), "activate", GTK_SIGNAL_FUNC(home_clicked_cb), browser);
  gtk_signal_connect(GTK_OBJECT(back_item), "activate", GTK_SIGNAL_FUNC(back_clicked_cb), data);
  gtk_signal_connect(GTK_OBJECT(forward_item), "activate", GTK_SIGNAL_FUNC(forward_clicked_cb), data);
  gtk_signal_connect(GTK_OBJECT(reload_item), "activate", GTK_SIGNAL_FUNC(reload_clicked_cb), browser);

  gtk_menu_popup(menu,
		 NULL,
		 NULL,
		 NULL,
		 NULL,
		 0,
		 gtk_get_current_event_time());

  fprintf(stdout, "done\n");
  return false;
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
  {
    // always make sure to clear the tempMessage.  it might have been
    // set from the link before a click and we wouldn't have gotten the
    // callback to unset it.
    update_temp_message(browser, 0);
  }
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
  start_throbber(browser);
}

void
load_finished_cb    (GtkMozEmbed *embed, MinimoBrowser *browser)
{
  browser->loadPercent = 0;
  browser->bytesLoaded = 0;
  browser->maxBytesLoaded = 0;
  browser->loading = false;
}

void
net_state_change_cb (GtkMozEmbed *embed, gint flags, guint status, MinimoBrowser *browser)
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
    browser->loadPercent = 0;
    browser->bytesLoaded = cur;
    browser->maxBytesLoaded = 0;
  }
  else
  {
    browser->bytesLoaded = cur;
    browser->maxBytesLoaded = max;
    if (cur > max)
      browser->loadPercent = 100;
    else
      browser->loadPercent = (cur * 100) / max;
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
  gtk_widget_set_usize(newBrowser->mozEmbed, 240, 320);
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
  browser->mouseDownTimer = g_timeout_add(250, 
					  context_menu_cb, 
					  browser);
  return NS_OK;
 }

gint dom_mouse_up_cb      (GtkMozEmbed *embed, nsIDOMMouseEvent *event,
			   MinimoBrowser *browser)
{
  g_source_remove(browser->mouseDownTimer);
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

static gboolean update_throbber(gpointer data)
{
  MinimoBrowser *browser = (MinimoBrowser*)data;

  if (!browser)
    return false;

  gboolean loading = browser->loading;

  if (loading)
  {
    browser->throbberID += (browser->throbberDirection * MINIMO_THROBBER_COLOR_STEP);

    GdkColor myColor;
    myColor.red = myColor.green = myColor.blue = browser->throbberID;

    gtk_widget_modify_base(GTK_WIDGET(browser->urlEntry),
			   GTK_STATE_NORMAL,
			   &myColor);

    if (browser->throbberDirection < 0 && browser->throbberID <= MINIMO_THROBBER_COLOR_MIN) 
    {
      browser->throbberDirection = 1;
    }
    else if (browser->throbberDirection > 0 && browser->throbberID >= MINIMO_THROBBER_COLOR_MAX)
    {
      browser->throbberDirection = -1;
    }
  }
  else
  {
    browser->throbberDirection = -1;
    browser->throbberID = MINIMO_THROBBER_COLOR_MAX;
    gtk_widget_modify_base(GTK_WIDGET(browser->urlEntry),
			   GTK_STATE_NORMAL,
			   NULL);
  }
  return loading;
}

void start_throbber(MinimoBrowser *browser)
{
  g_timeout_add(MINIMO_THROBBER_COLOR_INTERVAL, update_throbber, (void*)browser);
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
}


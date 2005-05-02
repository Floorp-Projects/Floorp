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
 *   The 10LE Team (in alphabetical order)
 *   -------------------------------------
 *
 *    Ilias Biris       <ext-ilias.biris@indt.org.br> - Coordinator
 *    Afonso Costa      <afonso.costa@indt.org.br>
 *    Antonio Gomes     <antonio.gomes@indt.org.br>
 *    Diego Gonzalez    <diego.gonzalez@indt.org.br>
 *    Raoni Novellino   <raoni.novellino@indt.org.br>
 *    Andre Pedralho    <andre.pedralho@indt.org.br>
 *
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

#include "nsMemory.h"
#include "minimo_callbacks.h"
#include "minimo_context.h"
#include "minimo_history.h"
#include "mozilla_api.h"

/* Number of browsers */
extern int gMinimoNumBrowsers;

extern gchar *gMinimoLinkMessageURL ;

/* Emulating right button clicks */
RightButtonClick *rButtonClick;

void
set_browser_visibility(MinimoBrowser *browser, gboolean visibility) {
  
  if (!visibility) {
    gtk_widget_hide(browser->topLevelWindow);
    return;
  }
  
  gtk_widget_show(browser->mozEmbed);
  gtk_widget_show(browser->topLevelVBox);
  gtk_widget_show(browser->topLevelWindow);
}

/* callbacks handlers */
void browse_in_window_from_popup(GtkWidget *menuitem, MinimoBrowser *browser)
{
  gchar *href;
  
  href = (gchar *) g_object_get_data(G_OBJECT(menuitem), "link");
  
  minimo_load_url(href, browser);
  
  return;
}

void download_link_from_popup(GtkWidget *menuitem, MinimoBrowser *browser)
{
  
  gchar *href;
  
  href = (gchar *) g_object_get_data(G_OBJECT(menuitem), "link");
  if (!href) {
    return;
  }
  
  create_save_document(NULL, browser, href);
  
  return;
}

void minimo_load_url(gchar *url, MinimoBrowser *browser)
{
  g_return_if_fail(browser->mozEmbed != NULL);
  g_return_if_fail(url != NULL);
  
  gtk_moz_embed_load_url(GTK_MOZ_EMBED(browser->mozEmbed), url);
  
  return;
}

void make_tab_from_popup(GtkWidget *menuitem, MinimoBrowser *browser)
{
  gchar *href;
  
  href = (gchar *) g_object_get_data(G_OBJECT(menuitem), "link");
  open_new_tab_cb(NULL, browser);
  
  minimo_load_url(href, browser);
  
  return;
}

void open_new_tab_cb (GtkMenuItem *button, MinimoBrowser *browser)
{
  GtkWidget *page_label;
  gint num_pages;
  gchar *page_title;
  gint current_page;
  
  g_return_if_fail(browser->notebook != NULL);
  
  /* right button click emulation initialization */
  rButtonClick = (RightButtonClick*) malloc(sizeof(RightButtonClick));
  rButtonClick->sig_handler= 0;
  rButtonClick->is_connected= FALSE;
  
  num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (browser->notebook));
  page_title = g_strdup_printf("%d",num_pages+1);
  page_label = gtk_label_new (page_title);
  
  browser->mozEmbed = gtk_moz_embed_new();
  
  gtk_notebook_append_page (GTK_NOTEBOOK (browser->notebook), browser->mozEmbed, page_label);
  gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (browser->notebook), browser->mozEmbed, FALSE, FALSE, GTK_PACK_START);
  gtk_widget_show (browser->mozEmbed);
  
  current_page = gtk_notebook_page_num (GTK_NOTEBOOK (browser->notebook), browser->mozEmbed);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (browser->notebook), current_page);
  browser->active_page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (browser->notebook), current_page);
  
  num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (browser->notebook));
  
  browser->progressPopup = create_minimo_progress(browser);
  
  gtk_box_pack_start(GTK_BOX(browser->topLevelVBox), 
                     browser->progressPopup,
                     FALSE, // expand
                     FALSE, // fill
                     0);   // padding
  
  /* applying the Signal into the new Render Engine - MozEmbed */
  // hook up the title change to update the window title
  g_signal_connect (GTK_OBJECT  (browser->mozEmbed), "title",       G_CALLBACK (title_changed_cb), browser);
  g_signal_connect (GTK_OBJECT  (browser->mozEmbed), "location",    G_CALLBACK(location_changed_cb), page_label);
  
  // hook up the start and stop signals
  g_signal_connect (GTK_OBJECT  (browser->mozEmbed), "net_start",   G_CALLBACK(load_started_cb), browser);
  g_signal_connect (GTK_OBJECT  (browser->mozEmbed), "net_stop",    G_CALLBACK(load_finished_cb), browser);
  
  // hookup to see whenever a new window is requested
  //g_signal_connect (GTK_OBJECT  (browser->mozEmbed), "new_window",  G_CALLBACK(new_window_cb), browser);
  
  // hookup to any requested visibility changes
  g_signal_connect(GTK_OBJECT(browser->mozEmbed), "visibility",     G_CALLBACK(visibility_cb), browser);
  
  // hookup to the signal that is called when someone clicks on a link to load a new uri
  g_signal_connect (GTK_OBJECT  (browser->mozEmbed), "open_uri",    G_CALLBACK(open_uri_cb), browser);
  
  // this signal is emitted when there's a request to change the containing browser window to a certain height, like with width
  // and height args for a window.open in javascript
  g_signal_connect (GTK_OBJECT  (browser->mozEmbed), "size_to",     G_CALLBACK(size_to_cb), browser);
  g_signal_connect (GTK_OBJECT  (browser->mozEmbed), "link_message",G_CALLBACK (link_message_cb), browser);
  
  // this signals are emitted when the mouse is clicked, then we can measure the time between pressed and released for left click or right click
  g_signal_connect (GTK_OBJECT (browser->mozEmbed), "dom_mouse_down", G_CALLBACK(on_button_pressed_cb), browser);
  g_signal_connect (GTK_OBJECT (browser->mozEmbed), "dom_mouse_up", G_CALLBACK(on_button_released_cb), browser);
  
  gMinimoNumBrowsers= num_pages;
}

GtkWidget* create_minimo_progress (MinimoBrowser* browser)
{
  GtkWidget *Minimo_Progress;
  GtkWidget *dialog_vbox4;
  GtkWidget *vbox3;
  GtkWidget *hbox2;
  GtkWidget *LoadingLabel;
  GtkWidget *URLLable;
  GtkWidget *progressbar1;
  GtkWidget *Minimo_Progress_Status;
  
  Minimo_Progress = gtk_vbox_new(FALSE, 0);
  
  dialog_vbox4 = Minimo_Progress;
  
  vbox3 = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(vbox3);
  gtk_box_pack_start(GTK_BOX (dialog_vbox4), vbox3, FALSE, FALSE, 0);
  
  hbox2 = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox2);
  gtk_box_pack_start(GTK_BOX (vbox3), hbox2, FALSE, TRUE, 0);
  
  LoadingLabel = gtk_label_new("Loading:");
  gtk_widget_modify_font(LoadingLabel, getOrCreateDefaultMinimoFont());
  gtk_widget_show(LoadingLabel);
  gtk_box_pack_start(GTK_BOX (hbox2), LoadingLabel, FALSE, FALSE, 0);
  
  URLLable = gtk_label_new("...");
  gtk_widget_modify_font(URLLable, getOrCreateDefaultMinimoFont());
  gtk_widget_show(URLLable);
  gtk_box_pack_start(GTK_BOX (hbox2), URLLable, FALSE, FALSE, 0);
  gtk_label_set_justify(GTK_LABEL (URLLable), GTK_JUSTIFY_CENTER);
  
  progressbar1 = gtk_progress_bar_new ();
  gtk_widget_show(progressbar1);
  gtk_box_pack_start(GTK_BOX (vbox3), progressbar1, FALSE, FALSE, 0);
  gtk_widget_set_usize(progressbar1, 240, 20);
  
  Minimo_Progress_Status = gtk_label_new ("");
  gtk_widget_modify_font(Minimo_Progress_Status, getOrCreateDefaultMinimoFont());
  gtk_widget_show(Minimo_Progress_Status);
  gtk_box_pack_start(GTK_BOX (vbox3), Minimo_Progress_Status, TRUE, FALSE, 0);
  
  browser->progressBar = progressbar1;
  
  g_signal_connect(GTK_OBJECT(browser->mozEmbed), "net_state", G_CALLBACK(net_state_change_cb), Minimo_Progress_Status);
  g_signal_connect(GTK_OBJECT(browser->mozEmbed), "progress",  G_CALLBACK(progress_change_cb), browser);
  g_signal_connect(GTK_OBJECT(browser->mozEmbed), "location",  G_CALLBACK(location_changed_cb), URLLable);
  
  return Minimo_Progress;
}

void quick_message(gchar* message)
{
  GtkWidget *dialog, *label, *okbutton;
  
  dialog = gtk_dialog_new();
  label  = gtk_label_new(message);
  okbutton = gtk_button_new_with_label("Ok");
  
  gtk_widget_modify_font(label, getOrCreateDefaultMinimoFont());
  gtk_widget_modify_font(okbutton, getOrCreateDefaultMinimoFont());
  
  g_signal_connect_swapped(GTK_OBJECT(okbutton), "clicked", G_CALLBACK(gtk_widget_destroy), dialog);
  
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), okbutton);
  
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
  gtk_widget_show_all(dialog);
}

void net_state_change_cb(GtkMozEmbed *embed, gint flags, guint status, GtkLabel* label)
{
  char * message = "Waiting...";
  
  if(flags & GTK_MOZ_EMBED_FLAG_IS_REQUEST) 
  {
    if (flags & GTK_MOZ_EMBED_FLAG_REDIRECTING)
      message = "Redirecting to site...";
    else if (flags & GTK_MOZ_EMBED_FLAG_TRANSFERRING)
      message = "Transferring data from site...";
    else if (flags & GTK_MOZ_EMBED_FLAG_NEGOTIATING)
      message = "Waiting for authorization...";
  }
  
  if (status == GTK_MOZ_EMBED_STATUS_FAILED_DNS)
    message = "Site not found.";
  else if (status == GTK_MOZ_EMBED_STATUS_FAILED_CONNECT)
    message = "Failed to connect to site.";
  else if (status == GTK_MOZ_EMBED_STATUS_FAILED_TIMEOUT)
    message = "Failed due to connection timeout.";
  else if (status == GTK_MOZ_EMBED_STATUS_FAILED_USERCANCELED)
    message = "User canceled connecting to site.";
  
  if (flags & GTK_MOZ_EMBED_FLAG_IS_DOCUMENT) {
    if (flags & GTK_MOZ_EMBED_FLAG_START)
      message = "Loading site...";
    else if (flags & GTK_MOZ_EMBED_FLAG_STOP)
      message = "Done.";
  }
  
  gtk_label_set_text(label, message);
}

void progress_change_cb(GtkMozEmbed *embed, gint cur, gint max, MinimoBrowser* browser)
{
  browser->totalBytes += cur;
  
  if (max < 1)
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(browser->progressBar));
  
  else { 
    float tmp = (cur/max);
    
    if ( tmp <= 1 && tmp >=0)
      gtk_progress_bar_update(GTK_PROGRESS_BAR(browser->progressBar), cur/max);
  }
}


void close_current_tab_cb (GtkMenuItem *button, MinimoBrowser *browser)
{
  gint current_page, num_pages;
  
  num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (browser->notebook));
  
  if (num_pages >1) {
	
    current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (browser->notebook));
	
    gtk_notebook_remove_page (GTK_NOTEBOOK (browser->notebook), current_page);
    num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (browser->notebook));
    gMinimoNumBrowsers= num_pages;
	
    if (num_pages == 1) {
      gtk_notebook_set_show_tabs (GTK_NOTEBOOK (browser->notebook), FALSE);
    }
  }
  
  g_free(rButtonClick);
}

/* bookmark button clicked */ 
void open_bookmark_window_cb (GtkMenuItem *button,MinimoBrowser *browser)
{
  open_bookmark();
}

void back_clicked_cb(GtkButton *button, MinimoBrowser *browser)
{
  gtk_moz_embed_go_back(GTK_MOZ_EMBED(browser->mozEmbed));
}

void reload_clicked_cb(GtkButton *button, MinimoBrowser *browser)
{
  GdkModifierType state = (GdkModifierType)0;
  gint x, y;
  gdk_window_get_pointer(NULL, &x, &y, &state);
  
  gtk_moz_embed_reload(GTK_MOZ_EMBED(browser->mozEmbed),
                       (state & GDK_SHIFT_MASK) ?
                       GTK_MOZ_EMBED_FLAG_RELOADBYPASSCACHE : 
                       GTK_MOZ_EMBED_FLAG_RELOADNORMAL);
}

void forward_clicked_cb(GtkButton *button, MinimoBrowser *browser)
{
  gtk_moz_embed_go_forward(GTK_MOZ_EMBED(browser->mozEmbed));
}

void stop_clicked_cb(GtkButton *button, MinimoBrowser *browser)
{
  gtk_moz_embed_stop_load(GTK_MOZ_EMBED(browser->mozEmbed));
}

gboolean delete_cb(GtkWidget *widget, GdkEventAny *event, MinimoBrowser *browser)
{
  gtk_widget_destroy(widget);
  return TRUE;
}

/* Creates a GUI for proxy and 'block popup' settings */
void pref_clicked_cb(GtkButton *button, MinimoBrowser *browser) {
  
  build_pref_window (browser->topLevelWindow);
}

void title_changed_cb(GtkMozEmbed *embed, MinimoBrowser *browser)
{
  char *newTitle;
  
  newTitle = gtk_moz_embed_get_title(embed);
  if(newTitle) {
    gtk_window_set_title(GTK_WINDOW(browser->topLevelWindow), newTitle);
    g_free(newTitle);
  }
}

void load_started_cb(GtkMozEmbed *embed, MinimoBrowser *browser)
{
  browser->totalBytes = 0;
  gtk_widget_show_all(browser->progressPopup);
}

void load_finished_cb(GtkMozEmbed *embed, MinimoBrowser *browser)
{
  gtk_widget_hide_all(browser->progressPopup);
  nsMemory::HeapMinimize(PR_TRUE);
}
/*
  void new_window_cb(GtkMozEmbed *embed, GtkMozEmbed **newEmbed, guint chromemask, MinimoBrowser *browser)
  {
  //	open_new_tab_cb (NULL, browser);
  //	gtk_moz_embed_load_url(GTK_MOZ_EMBED(browser->mozEmbed), gMinimoLinkMessageURL );
  //	show_hide_tabs_cb (NULL, browser);
  MinimoBrowser *newBrowser = new_gtk_browser(chromemask);
  gtk_widget_set_usize(newBrowser->mozEmbed, 240, 320);
  *newEmbed = GTK_MOZ_EMBED(newBrowser->mozEmbed);
  }
*/
void location_changed_cb(GtkMozEmbed *embed, GtkLabel* label)
{
  char *newLocation;
  
  newLocation = gtk_moz_embed_get_location(embed);
  if (newLocation) {
    
    gtk_label_set_text(label, newLocation);
    add_to_history(newLocation);
    g_free(newLocation);
  }
}

/* Method that create the "save dialog" */
void create_save_document(GtkMenuItem *button, MinimoBrowser *browser, gchar *location) 
{
  GtkWidget *fs, *ok_button, *cancel_button, *hbox;
  GtkWidget *SaveDialog, *scrolled_window;
  OpenDialogParams *dialogParams;
  
  G_CONST_RETURN gchar *file_name = NULL;
  
  g_return_if_fail(browser->mozEmbed != NULL);
  
  if (location)
    location = gtk_moz_embed_get_location(GTK_MOZ_EMBED (browser->mozEmbed));
  
  if (location) file_name = g_basename(location);
  
  dialogParams = (OpenDialogParams*) malloc(sizeof(OpenDialogParams));
  
  fs = gtk_file_chooser_widget_new (GTK_FILE_CHOOSER_ACTION_SAVE);
  SaveDialog = gtk_dialog_new ();
  gtk_widget_set_size_request (SaveDialog, 240, 320);
  gtk_window_set_title (GTK_WINDOW (SaveDialog), ("Save as"));
  gtk_window_set_position (GTK_WINDOW (SaveDialog), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal (GTK_WINDOW (SaveDialog), TRUE);
  gtk_window_set_default_size (GTK_WINDOW (SaveDialog), 240, 320);
  gtk_window_set_resizable (GTK_WINDOW (SaveDialog), FALSE);
  gtk_window_set_decorated (GTK_WINDOW (SaveDialog), TRUE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (SaveDialog), TRUE);
  gtk_window_set_skip_pager_hint (GTK_WINDOW (SaveDialog), TRUE);
  gtk_window_set_type_hint (GTK_WINDOW (SaveDialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_gravity (GTK_WINDOW (SaveDialog), GDK_GRAVITY_NORTH_EAST);
  gtk_window_set_transient_for(GTK_WINDOW(SaveDialog), GTK_WINDOW(browser->topLevelWindow));
  
  scrolled_window = gtk_scrolled_window_new(NULL,NULL);
  gtk_widget_show(scrolled_window);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG (SaveDialog)->vbox),scrolled_window,TRUE,TRUE,0);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
  
  gtk_scrolled_window_add_with_viewport ( GTK_SCROLLED_WINDOW (scrolled_window) ,fs);
  
  g_object_set_data(G_OBJECT(fs), "minimo", browser);
  
  /* adding extra button into the widget -> 'Ok and Cancel' Button */
  ok_button = gtk_button_new_with_label ("Ok");
  gtk_widget_modify_font(GTK_BIN(ok_button)->child, getOrCreateDefaultMinimoFont());
  
  cancel_button = gtk_button_new_with_label ("Cancel");
  gtk_widget_modify_font(GTK_BIN(cancel_button)->child, getOrCreateDefaultMinimoFont());
  
  hbox = gtk_hbox_new(FALSE, 10);
  gtk_box_pack_start(GTK_BOX (hbox), ok_button, FALSE, TRUE, 10);
  gtk_box_pack_start(GTK_BOX (hbox), cancel_button, FALSE, TRUE, 10);
  gtk_widget_show_all (hbox);
  
  gtk_file_chooser_set_extra_widget ( GTK_FILE_CHOOSER (fs), GTK_WIDGET (hbox));
  
  dialogParams->main_combo= fs;	
  dialogParams->dialog_combo= SaveDialog;
  
  /* connecting callbacks into the extra buttons */
  g_signal_connect(G_OBJECT(ok_button), "clicked", G_CALLBACK(on_save_ok_cb), dialogParams);
  
  gtk_widget_show(fs);
  gtk_widget_show_all(SaveDialog);
  
  return;
}

void on_save_ok_cb(GtkWidget *button, OpenDialogParams *dialogParams) {
  
  MinimoBrowser *browser = NULL;
  G_CONST_RETURN gchar *filename;
  
  filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialogParams->main_combo));
  
  if (!filename || !strcmp(filename,"")) {
	
    return ;
	
  } else {
    browser = (MinimoBrowser *) g_object_get_data(G_OBJECT(dialogParams->main_combo), "minimo");
    
    if (!mozilla_save( GTK_MOZ_EMBED (browser->mozEmbed), (gchar *) filename, TRUE )) {
      //create_dialog("Error","Save failed, did you enter a name?",0,0);
    }
    
    gtk_widget_destroy(dialogParams->main_combo);
    gtk_widget_destroy(dialogParams->dialog_combo);
    
    g_free(dialogParams);
  }
  return ;
}

void save_image_from_popup(GtkWidget *menuitem, MinimoBrowser *browser)
{
  
  gchar *href, *name, *FullPath;
  
  FullPath= PR_GetEnv("HOME");
  
  href = (gchar *) g_object_get_data(G_OBJECT(menuitem), "link");
  if (!href) {
    g_print("Could not find href to save image\n");
    return;
  }
  name = (gchar *) g_object_get_data(G_OBJECT(menuitem), "name");
  
  FullPath= g_strconcat(FullPath,"/downloaded_files/",NULL);
  mkdir(FullPath,0755);
  
  FullPath= g_strconcat(FullPath,name,NULL);
  mozilla_save_image(GTK_MOZ_EMBED(browser->mozEmbed),href,FullPath);
  
  g_free(FullPath);
  g_free(name);
  g_free(href);
  return;
}

void visibility_cb(GtkMozEmbed *embed, gboolean visibility, MinimoBrowser *browser)
{
  set_browser_visibility(browser, visibility);
}

gint open_uri_cb(GtkMozEmbed *embed, const char *uri, MinimoBrowser *browser)
{
  // don't interrupt anything
  return FALSE;
}

void size_to_cb(GtkMozEmbed *embed, gint width, gint height,
                MinimoBrowser *browser)
{
  gtk_widget_set_usize(browser->mozEmbed, width, height);
}

void link_message_cb (GtkWidget *embed, MinimoBrowser *browser)
{
  gMinimoLinkMessageURL  = mozilla_get_link_message (browser->active_page);
}

/* (mouse or stylus) clicks interaction */
void on_button_clicked_cb(GtkWidget *button, MinimoBrowser *browser)
{
  return;
}

/* button pressed */
gint on_button_pressed_cb(GtkWidget *button, MinimoBrowser *browser)
{
  rButtonClick->pressing_timer= g_timer_new();
  
  return NS_OK;
}

/* button released */
gint on_button_released_cb(GtkWidget *button, gpointer *dom_event, MinimoBrowser *browser)
{
  gint button_number;
  glong type;
  gdouble timeElapsed;
  gchar *img=NULL, *href=NULL, *linktext=NULL;
  
  type = mozilla_get_context_menu_type(GTK_MOZ_EMBED(browser->mozEmbed), dom_event, &img, &href, &linktext);
  
  button_number = 1;
  timeElapsed = g_timer_elapsed(rButtonClick->pressing_timer,0);
  g_timer_destroy(rButtonClick->pressing_timer);
  
  if (timeElapsed>= 2)
    button_number = 2;
  
  switch (button_number)
  {
  case 1: /* left button - disconect signal click for normal using */
    if (rButtonClick->is_connected) {
      g_signal_handler_disconnect(GTK_OBJECT (browser->mozEmbed), rButtonClick->sig_handler);
      rButtonClick->is_connected= FALSE;;
    }
    break;
  case 2: /* right button - connect signal click for do nothing with left button */
    if (!rButtonClick->is_connected) {
      rButtonClick->sig_handler= g_signal_connect(GTK_OBJECT(browser->mozEmbed),"dom_mouse_click", G_CALLBACK(on_button_clicked_cb), browser);
      rButtonClick->is_connected= TRUE;
    }
    
    if ((type & CONTEXT_IMAGE) && (type & CONTEXT_LINK)) {
      GtkWidget *menu = get_link_image_popup_menu(browser,img,href);
      gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, GDK_BUTTON3_MASK, gtk_get_current_event_time());
    } else if (type & CONTEXT_IMAGE) {
      gtk_menu_popup(GTK_MENU(get_image_popup_menu(browser,img)), NULL, NULL, NULL, NULL, GDK_BUTTON3_MASK, gtk_get_current_event_time());
    } else if (type & CONTEXT_LINK) {
      gtk_menu_popup(GTK_MENU(get_link_popup_menu(browser,href,linktext)), NULL, NULL, NULL, NULL, GDK_BUTTON3_MASK, gtk_get_current_event_time());
    } else if (type & CONTEXT_DOCUMENT) {
      gtk_menu_popup(GTK_MENU(get_doc_popup_menu(browser,href)), NULL, NULL, NULL, NULL, GDK_BUTTON3_MASK, gtk_get_current_event_time());
    } else if (type & CONTEXT_OTHER) {
      g_print("clicked on some other components\n");
    }
    break;
  default:
    break;
  }
  return NS_OK;
}

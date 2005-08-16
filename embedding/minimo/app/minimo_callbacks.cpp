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
#include "minimo_support.h"
#include "mozilla_api.h"

/* External Global variables */
extern int gMinimoNumBrowsers;				/* Number of browsers */
extern gchar *gMinimoLinkMessageURL ;
extern MinimoBrowser *gMinimoBrowser;			/* Global Minimo Variable */
extern gchar *gMinimoLastFind;				/* store the last expression on find dialog */
extern GtkEntryCompletion *gMinimoEntryCompletion;	/* Data Struct for make possible the real autocomplete */
extern ConfigData gPreferenceConfigStruct;
extern RightButtonClick *gContextRightButtonClick;			/* Emulating right button clicks */

// Callback from widget HEADERS (used only internaly)
static void open_dialog_ok_cb(GtkButton *button, OpenDialogParams* params);
static void create_info_dialog (MinimoBrowser* browser);
static void create_open_dialog(MinimoBrowser* browser);
static void find_dialog_ok_cb (GtkWidget *w, GtkWidget *window);
static void on_open_ok_cb(GtkWidget *button, GtkWidget *fs);

// TOOLBAR'S CALLBACKS

// Open 'URL INPUT Dialog' Button
void open_url_dialog_input_cb(GtkButton *button, MinimoBrowser *browser)
{
  create_open_dialog(browser);
}

// 'URL INPUT Dialog' creation
static void create_open_dialog(MinimoBrowser* browser)
{
  if (!browser) {
    quick_message("Creating the open dialog failed.");
    return;
  }
  
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *dialog_vbox2;
  GtkWidget *dialog_label;
  GtkWidget *dialog_comboentry;
  GtkWidget *dialog_area;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;
  GtkWidget *dialog_combo;
  GtkEntry *dialog_entry;
  
  dialog = gtk_dialog_new();
  
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(browser->topLevelWindow));
  
  /* set the position of this dialog immeditate below the toolbar */
  
  gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_default_size(GTK_WINDOW(dialog), 320, 100);
  gtk_window_set_decorated (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_skip_pager_hint(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  
  gtk_window_set_title(GTK_WINDOW (dialog), "Open Dialog");
  
  dialog_vbox = GTK_DIALOG(dialog)->vbox;
  gtk_widget_show(dialog_vbox);
  
  dialog_vbox2 = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(dialog_vbox2);
  gtk_box_pack_start(GTK_BOX (dialog_vbox), dialog_vbox2, TRUE, TRUE, 0);
  
  dialog_label = gtk_label_new("Enter a url:");
  gtk_widget_modify_font(dialog_label, getOrCreateDefaultMinimoFont());
  gtk_widget_show(dialog_label);
  gtk_box_pack_start(GTK_BOX (dialog_vbox2), dialog_label, FALSE, FALSE, 0);
  
  dialog_comboentry = gtk_combo_box_entry_new_text ();
  
  dialog_combo = gtk_combo_new ();
  dialog_entry = GTK_ENTRY (GTK_COMBO (dialog_combo)->entry);
  gtk_widget_modify_font(GTK_COMBO (dialog_combo)->entry, getOrCreateDefaultMinimoFont());
  gtk_widget_show(dialog_combo);
  gtk_box_pack_start(GTK_BOX (dialog_vbox2), dialog_combo, TRUE, TRUE, 0);
  
  support_populate_autocomplete((GtkCombo*) dialog_combo);
  
  /* setting up entry completion */
  support_build_entry_completion (browser);
  gtk_entry_set_completion (dialog_entry, gMinimoEntryCompletion);
  g_object_unref (gMinimoEntryCompletion);
  
  dialog_area = GTK_DIALOG(dialog)->action_area;
  gtk_widget_show(dialog_area);
  gtk_button_box_set_layout(GTK_BUTTON_BOX (dialog_area), GTK_BUTTONBOX_END);
  
  cancelbutton = gtk_button_new_with_label("Cancel");
  gtk_widget_modify_font(GTK_BIN(cancelbutton)->child, getOrCreateDefaultMinimoFont());
  gtk_widget_show(cancelbutton);
  gtk_dialog_add_action_widget(GTK_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS(cancelbutton, GTK_CAN_DEFAULT);
  
  okbutton = gtk_button_new_with_label("OK");
  gtk_widget_modify_font(GTK_BIN(okbutton)->child, getOrCreateDefaultMinimoFont());
  gtk_widget_show(okbutton);
  gtk_dialog_add_action_widget(GTK_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS(okbutton, GTK_CAN_DEFAULT);
  
  OpenDialogParams* dialogParams = (OpenDialogParams*) malloc(sizeof(OpenDialogParams));
  
  dialogParams->dialog_combo = dialog_combo;
  //  ((MinimoBrowser *)(dialogParams->main_combo)) = browser;
  
  g_signal_connect (GTK_OBJECT (okbutton), "clicked", G_CALLBACK (open_dialog_ok_cb), dialogParams);
  g_signal_connect (GTK_OBJECT (GTK_COMBO (dialog_combo)->entry), "activate",G_CALLBACK (open_dialog_ok_cb), dialogParams);
  g_signal_connect_swapped(GTK_OBJECT(okbutton), "clicked", G_CALLBACK(gtk_widget_destroy), dialog);
  g_signal_connect_swapped(GTK_OBJECT(GTK_COMBO (dialog_combo)->entry), "activate", G_CALLBACK(gtk_widget_destroy), dialog);
  
  g_signal_connect_swapped(GTK_OBJECT(cancelbutton), "clicked", G_CALLBACK(gtk_widget_destroy), dialog);
  g_signal_connect(GTK_OBJECT(dialog), "destroy", G_CALLBACK(support_destroy_dialog_params_cb), dialogParams);
  
  gtk_widget_show_all(dialog);
}

// 'URL INPUT Dialog' Button's OK Callback
static void open_dialog_ok_cb(GtkButton *button, OpenDialogParams* params)
{
  const gchar *url = 
    gtk_editable_get_chars (GTK_EDITABLE (GTK_COMBO(params->dialog_combo)->entry), 0, -1);
  
  gtk_moz_embed_load_url(GTK_MOZ_EMBED(((MinimoBrowser *)(params->main_combo))->mozEmbed), url);
  
  support_add_autocomplete(url, params);
}

// Back button
void back_clicked_cb(GtkButton *button, MinimoBrowser *browser)
{
	gtk_moz_embed_go_back(GTK_MOZ_EMBED(browser->mozEmbed));
}

// Reaload Button
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

// Forward button
void forward_clicked_cb(GtkButton *button, MinimoBrowser *browser)
{
  gtk_moz_embed_go_forward(GTK_MOZ_EMBED(browser->mozEmbed));
}

// Stop button
void stop_clicked_cb(GtkButton *button, MinimoBrowser *browser)
{
  gtk_moz_embed_stop_load(GTK_MOZ_EMBED(browser->mozEmbed));
}

// Pref Window button (creates a dialog eneabling proxy and 'block popup' settings)
void pref_clicked_cb(GtkButton *button, MinimoBrowser *browser) {
  
	preferences_build_dialog (browser->topLevelWindow);
}


// MENU_BUTTON'S CALLBACK - METHODS THAT CREATE DIALOG FOR SPECIFICS TASK
// show / hide tabs divison
void show_hide_tabs_cb (GtkMenuItem *button, MinimoBrowser *browser)
{
  if (browser->show_tabs) {
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK(browser->notebook), FALSE);
    browser->show_tabs = FALSE;
  }
  else {
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK(browser->notebook), TRUE);
    browser->show_tabs = TRUE;
  }
}

// Create a new tab (including a new gtk_moz_embed)
void open_new_tab_cb (GtkMenuItem *button, MinimoBrowser *browser)
{
  GtkWidget *page_label;
  gint num_pages;
  gchar *page_title;
  gint current_page;
  
  g_return_if_fail(browser->notebook != NULL);
  
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

// Close the current tab
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
  
  g_free(gContextRightButtonClick);
}

// bookmark button clicked
void open_bookmark_window_cb (GtkMenuItem *button,MinimoBrowser *browser)
{
	bookmark_create_dialog();
}

// Method that create the "save dialog"
void create_save_document_dialog (GtkMenuItem *button, MinimoBrowser *browser, gchar *location) 
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

void info_clicked_cb(GtkButton *button, MinimoBrowser *browser)
{
  create_info_dialog(browser);
}

static void create_info_dialog (MinimoBrowser* browser)
{
  if (!browser) {
    quick_message("Creating the info dialog failed.");
    return;
  }
  
  char* titleString    = gtk_moz_embed_get_title((GtkMozEmbed*)browser->mozEmbed);
  char* locationString = gtk_moz_embed_get_location((GtkMozEmbed*)browser->mozEmbed);
  
  // first extract all of the interesting info out of the browser
  // object.	
  GtkWidget *InfoDialog;
  GtkWidget *dialog_vbox5;
  GtkWidget *vbox4;
  GtkWidget *frame1;
  GtkWidget *table2;
  GtkWidget *URL;
  GtkWidget *Type;
  GtkWidget *RenderMode;
  GtkWidget *Encoding;
  GtkWidget *Size;
  GtkWidget *TypeEntry;
  GtkWidget *URLEntry;
  GtkWidget *ModeEntry;
  GtkWidget *SizeEntry;
  GtkWidget *PageTitle;
  GtkWidget *frame2;
  GtkWidget *SecurityText;
  GtkWidget *label10;
  GtkWidget *dialog_action_area5;
  GtkWidget *closebutton1;
  
  InfoDialog = gtk_dialog_new ();
  gtk_widget_set_size_request (InfoDialog, 240, 320);
  gtk_window_set_title (GTK_WINDOW (InfoDialog), ("Page Info"));
  gtk_window_set_position (GTK_WINDOW (InfoDialog), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal (GTK_WINDOW (InfoDialog), TRUE);
  gtk_window_set_default_size (GTK_WINDOW (InfoDialog), 240, 320);
  gtk_window_set_resizable (GTK_WINDOW (InfoDialog), FALSE);
  gtk_window_set_decorated (GTK_WINDOW (InfoDialog), TRUE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (InfoDialog), TRUE);
  gtk_window_set_skip_pager_hint (GTK_WINDOW (InfoDialog), TRUE);
  gtk_window_set_type_hint (GTK_WINDOW (InfoDialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_gravity (GTK_WINDOW (InfoDialog), GDK_GRAVITY_NORTH_EAST);
  gtk_window_set_transient_for(GTK_WINDOW(InfoDialog), GTK_WINDOW(browser->topLevelWindow));
  
  dialog_vbox5 = GTK_DIALOG (InfoDialog)->vbox;
  gtk_widget_show (dialog_vbox5);
  
  vbox4 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox4);
  gtk_box_pack_start (GTK_BOX (dialog_vbox5), vbox4, TRUE, TRUE, 0);
  
  frame1 = gtk_frame_new (NULL);
  gtk_widget_show (frame1);
  gtk_box_pack_start (GTK_BOX (vbox4), frame1, TRUE, TRUE, 0);
  
  table2 = gtk_table_new (5, 2, FALSE);
  gtk_widget_show (table2);
  gtk_container_add (GTK_CONTAINER (frame1), table2);
  gtk_widget_set_size_request (table2, 240, -1);
  gtk_container_set_border_width (GTK_CONTAINER (table2), 2);
  gtk_table_set_row_spacings (GTK_TABLE (table2), 1);
  gtk_table_set_col_spacings (GTK_TABLE (table2), 10);
  
  URL = gtk_label_new (("URL:"));
  gtk_widget_modify_font(URL, getOrCreateDefaultMinimoFont());
  gtk_widget_show (URL);
  gtk_table_attach (GTK_TABLE (table2), URL, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (URL), 0, 0.5);
  
  Type = gtk_label_new (("Type:"));
  gtk_widget_modify_font(Type, getOrCreateDefaultMinimoFont());
  gtk_widget_show (Type);
  gtk_table_attach (GTK_TABLE (table2), Type, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (Type), 0, 0.5);
  
  RenderMode = gtk_label_new (("Mode:"));
  gtk_widget_modify_font(RenderMode, getOrCreateDefaultMinimoFont());
  gtk_widget_show (RenderMode);
  gtk_table_attach (GTK_TABLE (table2), RenderMode, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (RenderMode), 0, 0.5);
  
  Encoding = gtk_label_new (("Encoding:"));
  gtk_widget_modify_font(Encoding, getOrCreateDefaultMinimoFont());
  gtk_widget_show (Encoding);
  gtk_table_attach (GTK_TABLE (table2), Encoding, 0, 1, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (Encoding), 0, 0.5);
  
  Size = gtk_label_new (("Size:"));
  gtk_widget_modify_font(Size, getOrCreateDefaultMinimoFont());
  gtk_widget_show (Size);
  gtk_table_attach (GTK_TABLE (table2), Size, 0, 1, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (Size), 0, 0.5);
  
  TypeEntry = gtk_entry_new ();
  gtk_widget_modify_font(TypeEntry, getOrCreateDefaultMinimoFont());
  gtk_widget_show (TypeEntry);
  gtk_table_attach (GTK_TABLE (table2), TypeEntry, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_editable_set_editable (GTK_EDITABLE (TypeEntry), FALSE);
  gtk_entry_set_has_frame (GTK_ENTRY (TypeEntry), FALSE);
  
  URLEntry = gtk_entry_new ();
  gtk_widget_modify_font(URLEntry, getOrCreateDefaultMinimoFont());
  
  gtk_entry_set_text(GTK_ENTRY(URLEntry), locationString);
  
  gtk_widget_show (URLEntry);
  gtk_table_attach (GTK_TABLE (table2), URLEntry, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_editable_set_editable (GTK_EDITABLE (URLEntry), FALSE);
  gtk_entry_set_has_frame (GTK_ENTRY (URLEntry), FALSE);
  
  ModeEntry = gtk_entry_new ();
  gtk_widget_modify_font(ModeEntry, getOrCreateDefaultMinimoFont());
  gtk_widget_show (ModeEntry);
  gtk_table_attach (GTK_TABLE (table2), ModeEntry, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_editable_set_editable (GTK_EDITABLE (ModeEntry), FALSE);
  gtk_entry_set_has_frame (GTK_ENTRY (ModeEntry), FALSE);
  
  Encoding = gtk_entry_new ();
  gtk_widget_modify_font(Encoding, getOrCreateDefaultMinimoFont());
  gtk_widget_show (Encoding);
  gtk_table_attach (GTK_TABLE (table2), Encoding, 1, 2, 3, 4,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_editable_set_editable (GTK_EDITABLE (Encoding), FALSE);
  gtk_entry_set_has_frame (GTK_ENTRY (Encoding), FALSE);
  
  SizeEntry = gtk_entry_new ();
  gtk_widget_modify_font(SizeEntry, getOrCreateDefaultMinimoFont());
  gtk_widget_show (SizeEntry);
  gtk_table_attach (GTK_TABLE (table2), SizeEntry, 1, 2, 4, 5,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_editable_set_editable (GTK_EDITABLE (SizeEntry), FALSE);
  gtk_entry_set_has_frame (GTK_ENTRY (SizeEntry), FALSE);
  
  
  char buffer[50];
  sprintf(buffer, "%d bytes", browser->totalBytes);
  gtk_entry_set_text(GTK_ENTRY(SizeEntry), buffer);
  
  PageTitle = gtk_label_new ("");
  gtk_widget_modify_font(PageTitle, getOrCreateDefaultMinimoFont());
  
  if (titleString)
	gtk_label_set_text(GTK_LABEL(PageTitle), titleString);
  else
	gtk_label_set_text(GTK_LABEL(PageTitle), "Untitled Page");
  
  gtk_widget_show (PageTitle);
  gtk_frame_set_label_widget (GTK_FRAME (frame1), PageTitle);
  
  frame2 = gtk_frame_new (NULL);
  gtk_widget_show (frame2);
  gtk_box_pack_start (GTK_BOX (vbox4), frame2, TRUE, TRUE, 0);
  
  SecurityText = gtk_label_new ("");
  gtk_widget_modify_font(SecurityText, getOrCreateDefaultMinimoFont());
  gtk_widget_show (SecurityText);
  gtk_container_add (GTK_CONTAINER (frame2), SecurityText);
  
  label10 = gtk_label_new (("Security Information"));
  gtk_widget_modify_font(label10, getOrCreateDefaultMinimoFont());
  gtk_widget_show (label10);
  gtk_frame_set_label_widget (GTK_FRAME (frame2), label10);
  
  dialog_action_area5 = GTK_DIALOG (InfoDialog)->action_area;
  gtk_widget_show (dialog_action_area5);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area5), GTK_BUTTONBOX_END);
  
  closebutton1 = gtk_button_new_with_label("Close");
  gtk_widget_modify_font(GTK_BIN(closebutton1)->child, getOrCreateDefaultMinimoFont());
  gtk_widget_show (closebutton1);
  gtk_dialog_add_action_widget (GTK_DIALOG (InfoDialog), closebutton1, GTK_RESPONSE_CLOSE);
  GTK_WIDGET_SET_FLAGS (closebutton1, GTK_CAN_DEFAULT);
  
  g_signal_connect_swapped(GTK_OBJECT(closebutton1), "clicked", G_CALLBACK(gtk_widget_destroy), InfoDialog);
  
  gtk_widget_show_all(InfoDialog);
  
  free(titleString);
  free(locationString);
}

// enable find a substring in the embed 
void create_find_dialog (GtkWidget *w, MinimoBrowser *browser) {
  
  GtkWidget *window, *hbox, *entry, *label, *button, *ignore_case, *vbox_l, *hbox_tog;
  GList *glist = NULL;
  
  if (gMinimoLastFind) 
    glist = g_list_append(glist, gMinimoLastFind);
  
  window = gtk_dialog_new();
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_keep_above(GTK_WINDOW (window), TRUE);
  gtk_widget_set_size_request (window, 230, 110);
  gtk_window_set_resizable(GTK_WINDOW (window),FALSE);
  gtk_window_set_title (GTK_WINDOW(window), "Find");
  support_setup_escape_key_handler(window);
  
  g_object_set_data(G_OBJECT(window), "Minimo", browser);
  g_object_set_data(G_OBJECT(window), "Embed", browser->mozEmbed);
  
  hbox = gtk_hbox_new (FALSE, 0);
  hbox_tog = gtk_hbox_new(FALSE, 0);
  vbox_l = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER(hbox), 4);
  
  label = gtk_label_new ("Find: ");
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
  
  entry = gtk_combo_new ();
  gtk_combo_disable_activate(GTK_COMBO(entry));
  gtk_combo_set_case_sensitive(GTK_COMBO(entry), TRUE);
  if (gMinimoLastFind) 
    gtk_combo_set_popdown_strings(GTK_COMBO(entry),glist);
  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(entry)->entry), "");
  g_object_set_data(G_OBJECT(window), "entry", entry);
  g_signal_connect(G_OBJECT(GTK_COMBO(entry)->entry), "activate", G_CALLBACK(find_dialog_ok_cb), window);
  gtk_box_pack_start (GTK_BOX(hbox), entry, FALSE, FALSE, 0);
  
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->vbox), hbox, FALSE, FALSE, 0);
  
  ignore_case = gtk_check_button_new_with_label ("Ignore case");
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ignore_case), TRUE);
  g_object_set_data(G_OBJECT(window), "ignore_case", ignore_case);
  gtk_box_pack_start(GTK_BOX(vbox_l), ignore_case, FALSE, FALSE, 0);
  
  gtk_box_pack_start(GTK_BOX(hbox_tog), vbox_l, FALSE, FALSE, 0);
  
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->vbox), hbox_tog, FALSE, FALSE, 0);
  
  button = gtk_button_new_from_stock (GTK_STOCK_FIND);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(window)->action_area), button, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(find_dialog_ok_cb), window);
  
  button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(window)->action_area), button, FALSE, FALSE, 0);
  g_signal_connect_swapped(G_OBJECT(button), "clicked", G_CALLBACK(gtk_widget_destroy),
                           (gpointer)window);
  
  gtk_widget_show_all (window);
  gtk_widget_grab_focus (entry);
}

static void find_dialog_ok_cb (GtkWidget *w, GtkWidget *window)
{
  
  GtkWidget *entry = (GtkWidget *)g_object_get_data(G_OBJECT(window), "entry");
  
#define	GET_FIND_OPTION(window, name)	\
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(window), name)))
  
  gMinimoBrowser->didFind = mozilla_find( GTK_MOZ_EMBED (gMinimoBrowser->mozEmbed),
                                          gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(entry)->entry)),
                                          !GET_FIND_OPTION(window, "ignore_case"),
                                          FALSE, //GET_FIND_OPTION(window, "backwards"),
                                          TRUE, //GET_FIND_OPTION(window, "auto_wrap"),
                                          FALSE, //GET_FIND_OPTION(window, "entire_word"),
                                          TRUE, //GET_FIND_OPTION(window, "in_frame"),
                                          FALSE);
  
  //if (!browser->didFind) create_dialog("Find Results", "Expression was not found!", 0, 1);
  if (gMinimoLastFind) g_free(gMinimoLastFind);
  gMinimoLastFind = g_strdup(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(entry)->entry)));
}

/* Method that create the "Open From File .." Dialog */
void create_open_document_from_file (GtkMenuItem *button, MinimoBrowser *browser) {
  
  GtkWidget *fs, *ok_button, *cancel_button, *hbox;
  GtkWidget *OpenFromFileDialog, *scrolled_window;
  
  G_CONST_RETURN gchar *location = NULL, *file_name = NULL;
  
  g_return_if_fail(browser->mozEmbed != NULL);
  
  location = gtk_moz_embed_get_location(GTK_MOZ_EMBED (browser->mozEmbed));
  if (location) 
    file_name = g_basename(location);
  
  fs = gtk_file_chooser_widget_new (GTK_FILE_CHOOSER_ACTION_OPEN);
  
  OpenFromFileDialog = gtk_dialog_new ();
  gtk_widget_set_size_request (OpenFromFileDialog, 240, 320);
  gtk_window_set_title (GTK_WINDOW (OpenFromFileDialog), ("Open URL From File"));
  gtk_window_set_position (GTK_WINDOW (OpenFromFileDialog), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal (GTK_WINDOW (OpenFromFileDialog), TRUE);
  gtk_window_set_default_size (GTK_WINDOW (OpenFromFileDialog), 240, 320);
  gtk_window_set_resizable (GTK_WINDOW (OpenFromFileDialog), FALSE);
  gtk_window_set_decorated (GTK_WINDOW (OpenFromFileDialog), TRUE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (OpenFromFileDialog), TRUE);
  gtk_window_set_skip_pager_hint (GTK_WINDOW (OpenFromFileDialog), TRUE);
  gtk_window_set_type_hint (GTK_WINDOW (OpenFromFileDialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_gravity (GTK_WINDOW (OpenFromFileDialog), GDK_GRAVITY_NORTH_EAST);
  gtk_window_set_transient_for(GTK_WINDOW(OpenFromFileDialog), GTK_WINDOW(browser->topLevelWindow));
  
  scrolled_window = gtk_scrolled_window_new(NULL,NULL);
  gtk_widget_show(scrolled_window);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG (OpenFromFileDialog)->vbox),scrolled_window,TRUE,TRUE,0);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
  
  gtk_scrolled_window_add_with_viewport ( GTK_SCROLLED_WINDOW (scrolled_window) ,fs);
  
  g_object_set_data(G_OBJECT(fs), "minimo", browser);
  g_object_set_data(G_OBJECT(fs), "open_dialog", OpenFromFileDialog);
  
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
  
  /* connecting callbacks into the extra buttons */
  g_signal_connect(G_OBJECT(ok_button), "clicked", G_CALLBACK(on_open_ok_cb), fs);
  g_signal_connect_swapped(G_OBJECT(cancel_button), "clicked", G_CALLBACK(gtk_widget_destroy), (gpointer) OpenFromFileDialog		);
  
  gtk_widget_show(fs);
  gtk_widget_show_all(OpenFromFileDialog);
  
  return;
}

static void on_open_ok_cb (GtkWidget *button, GtkWidget *fs) {
  MinimoBrowser *browser = NULL;
  G_CONST_RETURN gchar *filename;
  gchar *converted;
  
  filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fs));
  
  if (!filename || !strcmp(filename,"")) {
	
    return ;
  } else {
	
    converted = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
    
    browser = (MinimoBrowser *) g_object_get_data(G_OBJECT(fs), "minimo");
	
    if (converted != NULL) {
      
      gtk_moz_embed_load_url(GTK_MOZ_EMBED(browser->mozEmbed), converted);
      gtk_widget_grab_focus (GTK_WIDGET(browser->mozEmbed));
    }
    
    g_free (converted);
    g_free ((void *) filename);
    gtk_widget_destroy((GtkWidget *) g_object_get_data(G_OBJECT(fs), "open_dialog"));
  }
  return ;
}

void increase_font_size_cb (GtkMenuItem *button, MinimoBrowser *browser) {	
  
  char tmpstr[128];
  
  gPreferenceConfigStruct.current_font_size += 3;
  
  g_snprintf (tmpstr, 1024, "font.size.variable.%s", lang_font_item[0]);
  mozilla_preference_set_int(tmpstr,gPreferenceConfigStruct.current_font_size);
  g_snprintf(tmpstr, 1024, "font.min-size.variable.%s", lang_font_item[0]);
  mozilla_preference_set_int(tmpstr, gPreferenceConfigStruct.current_font_size);
  
  fprintf(stderr, "increasing %d\n", gPreferenceConfigStruct.current_font_size);
}

/* Method that decrease the Minimo's font size */
 
void decrease_font_size_cb (GtkMenuItem *button, MinimoBrowser *browser) {
  
  char tmpstr[128];
  
  gPreferenceConfigStruct.current_font_size -= 3;
  
  g_snprintf (tmpstr, 1024, "font.size.variable.%s", lang_font_item[0]);
  mozilla_preference_set_int(tmpstr,gPreferenceConfigStruct.current_font_size);
  g_snprintf(tmpstr, 1024, "font.min-size.variable.%s", lang_font_item[0]);
  mozilla_preference_set_int(tmpstr, gPreferenceConfigStruct.current_font_size);
  
  fprintf(stderr, "decreasing %d\n", gPreferenceConfigStruct.current_font_size);
}

/* Method that show the "Full Screen" Minimo's Mode */
void full_screen_cb (GtkMenuItem *button, MinimoBrowser* browser) {
  
  gtk_window_fullscreen (GTK_WINDOW (browser->topLevelWindow));
  g_signal_connect (GTK_OBJECT (button), "activate", G_CALLBACK(unfull_screen_cb), browser);
  gtk_tooltips_set_tip (gtk_tooltips_new (), GTK_WIDGET (button), "UnFull Screen", NULL);
}

/* Method that show the "Normal Screen" Minimo's Mode */
void unfull_screen_cb (GtkMenuItem *button, MinimoBrowser* browser) {
  
  gtk_window_unfullscreen (GTK_WINDOW (browser->topLevelWindow));
  g_signal_connect (GTK_OBJECT (button), "activate", G_CALLBACK(full_screen_cb), browser);  
  gtk_tooltips_set_tip (gtk_tooltips_new (), GTK_WIDGET (button), "Full Screen", NULL);
}

/* Method that show History Window */
void open_history_window (GtkButton *button, MinimoBrowser* browser){
  history_create_dialog (browser->mozEmbed);
}

// ENIGNE SIGNALS' CALLBACKS
gint open_uri_cb(GtkMozEmbed *embed, const char *uri, MinimoBrowser *browser)
{
  // don't interrupt anything
  return FALSE;
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
	g_timer_start(gContextRightButtonClick->pressing_timer);

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
	timeElapsed = g_timer_elapsed(gContextRightButtonClick->pressing_timer,0);
	g_timer_reset(gContextRightButtonClick->pressing_timer);

	if (timeElapsed>= 2)
		button_number = 2;

	switch (button_number)
	{
		case 1: /* left button - disconect signal click for normal using */
			if (g_signal_handler_is_connected (GTK_OBJECT (browser->mozEmbed), gContextRightButtonClick->sig_handler)) {
				g_signal_handler_disconnect(GTK_OBJECT (browser->mozEmbed), gContextRightButtonClick->sig_handler);
			}
			break;
		case 2: /* right button - connect signal click for do nothing with left button */
			if (!g_signal_handler_is_connected (GTK_OBJECT (browser->mozEmbed), gContextRightButtonClick->sig_handler)) {
				gContextRightButtonClick->sig_handler= g_signal_connect(GTK_OBJECT(browser->mozEmbed),"dom_mouse_click", G_CALLBACK(on_button_clicked_cb), browser);
			}
			
			if ((type & CONTEXT_IMAGE) && (type & CONTEXT_LINK)) {
				GtkWidget *menu = context_click_link_image (browser,img,href);
				gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, GDK_BUTTON3_MASK, gtk_get_current_event_time());
			} else if (type & CONTEXT_IMAGE) {
				gtk_menu_popup(GTK_MENU(context_click_on_image (browser,img)), NULL, NULL, NULL, NULL, GDK_BUTTON3_MASK, gtk_get_current_event_time());
			} else if (type & CONTEXT_LINK) {
				gtk_menu_popup(GTK_MENU(context_click_on_link (browser,href,linktext)), NULL, NULL, NULL, NULL, GDK_BUTTON3_MASK, gtk_get_current_event_time());
			} else if (type & CONTEXT_DOCUMENT) {
				gtk_menu_popup(GTK_MENU(context_click_doc (browser,href)), NULL, NULL, NULL, NULL, GDK_BUTTON3_MASK, gtk_get_current_event_time());
			} else if (type & CONTEXT_OTHER) {
				g_print("clicked on some other components\n");
			}
			break;
		default:
			break;
	}
	return NS_OK;
}

void visibility_cb(GtkMozEmbed *embed, gboolean visibility, MinimoBrowser *browser)
{
  set_browser_visibility(browser, visibility);
}

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

/* CALLBACK FROM CONTEXT SENSITIVE MENUS */
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
  
  create_save_document_dialog (NULL, browser, href);
  
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

// SUPPORT METHODS
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

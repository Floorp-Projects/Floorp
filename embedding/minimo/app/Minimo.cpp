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

#include "Minimo.h"
/* History */
#include "minimo_history.h"
/* Minimo's callbacks */
#include "minimo_callbacks.h"
/* Minimo's support functions */
#include "minimo_support.h"

/* Global variables */
MinimoBrowser *gMinimoBrowser;			/* Global Minimo Variable */
gint  gMinimoNumBrowsers = 0;			/* Number of browsers */
gchar *gMinimoProfilePath = NULL;		/* Minimo's PATH - probably "~/home/current_user/.Minimo" */
gchar *gMinimoLastFind = NULL;			/* store the last expression on find dialog */
gchar *gMinimoUserHomePath = NULL;		/* */
gchar *gMinimoLinkMessageURL;			/* Last clicked URL */
GList *gMinimoBrowserList = g_list_alloc();	/* the list of browser windows currently open */

static GtkWidget *populate_menu_button(MinimoBrowser* );
static void switch_page_cb (GtkNotebook *, GtkNotebookPage *, guint , MinimoBrowser *);
/* callbacks from the singleton object */
static void new_window_orphan_cb(GtkMozEmbedSingle *, GtkMozEmbed **, guint , gpointer );

int
main(int argc, char **argv) {
  
#ifdef NS_TRACE_MALLOC
	argc = NS_TraceMallocStartupArgs(argc, argv);
#endif
  
	gtk_set_locale();
	gtk_init(&argc, &argv);
	
	gMinimoUserHomePath = PR_GetEnv("HOME");

	if (!gMinimoUserHomePath) {
		fprintf(stderr, "Failed to get HOME\n");
		exit(1);
	}
	
	gchar *dir = g_strconcat(gMinimoUserHomePath,"/.Minimo",NULL);
	/* this is fine, as it will silently error if it already exists*/
	mkdir(dir,0755);
	g_free(dir);   
	
	/* Minimo's config folder - hidden */  
	gMinimoProfilePath = g_strdup_printf("%s/%s", gMinimoUserHomePath, ".Minimo");
	gtk_moz_embed_set_profile_path(gMinimoProfilePath, "Minimo");
	
	gMinimoBrowser = new_gtk_browser(GTK_MOZ_EMBED_FLAG_DEFAULTCHROME);
	
	/* set our minimum size */
	gtk_widget_set_usize(gMinimoBrowser->mozEmbed, 250, 320);
	
	/* Load the configuration files */
	preferences_read_config ();
	
	set_browser_visibility(gMinimoBrowser, TRUE);
	
	bookmark_moz_embed_initialize (gMinimoBrowser->mozEmbed);
	
	context_initialize_timer ();
	support_init_remote();
	support_init_autocomplete();
	
	if (argc > 1)
		gtk_moz_embed_load_url(GTK_MOZ_EMBED(gMinimoBrowser->mozEmbed), argv[1]);
	
	// get the singleton object and hook up to its new window callback
	// so we can create orphaned windows.
	GtkMozEmbedSingle *single;
	
	single = gtk_moz_embed_single_get();
	if (!single) {
		fprintf(stderr, "Failed to get singleton embed object!\n");
		exit(1);
	}
	
	g_signal_connect(GTK_OBJECT(single), "new_window_orphan", G_CALLBACK(new_window_orphan_cb), NULL);
	
	gtk_main ();
	
	support_cleanup_remote ();
	support_cleanup_autocomplete ();
}

/* Build the Minimo Window */
MinimoBrowser * new_gtk_browser(guint32 chromeMask) {

	//MinimoBrowser *browser = 0;
	gMinimoNumBrowsers++;	
	gMinimoBrowser = g_new0(MinimoBrowser, 1);

	gMinimoBrowserList = g_list_prepend(gMinimoBrowserList, gMinimoBrowser);

	// create our new toplevel window
	gMinimoBrowser->topLevelWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_resizable (GTK_WINDOW(gMinimoBrowser->topLevelWindow),TRUE);
	gtk_window_set_title (GTK_WINDOW (gMinimoBrowser->topLevelWindow), "MINIMO - Embedded Mozilla");
	
	// new vbox
	gMinimoBrowser->topLevelVBox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show (gMinimoBrowser->topLevelVBox);
	
	// add it to the toplevel window
	gtk_container_add(GTK_CONTAINER(gMinimoBrowser->topLevelWindow), gMinimoBrowser->topLevelVBox);
	
	setup_toolbar(gMinimoBrowser);
	
	gtk_box_pack_start(GTK_BOX(gMinimoBrowser->topLevelVBox), 
			gMinimoBrowser->toolbar.toolbar,
			FALSE, // expand
			FALSE, // fill
			0);    // padding
	
	gMinimoBrowser->notebook = gtk_notebook_new ();
	gtk_box_pack_start (GTK_BOX (gMinimoBrowser->topLevelVBox), gMinimoBrowser->notebook, TRUE, TRUE, 0);
	gMinimoBrowser->show_tabs = FALSE;
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK(gMinimoBrowser->notebook), gMinimoBrowser->show_tabs);
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (gMinimoBrowser->notebook), GTK_POS_LEFT);
	gtk_widget_show (gMinimoBrowser->notebook);
	
	open_new_tab_cb (NULL, gMinimoBrowser);
	
	// catch the destruction of the toplevel window
	g_signal_connect(GTK_OBJECT(gMinimoBrowser->topLevelWindow), "delete_event", 
			G_CALLBACK(delete_cb), gMinimoBrowser);
	
	// hookup to when the window is destroyed
	g_signal_connect(GTK_OBJECT(gMinimoBrowser->topLevelWindow), "destroy", G_CALLBACK(destroy_cb), gMinimoBrowser);
	
	g_signal_connect (GTK_OBJECT (gMinimoBrowser->notebook), "switch_page", 
			G_CALLBACK (switch_page_cb), gMinimoBrowser);
	
	// set the chrome type so it's stored in the object
	// commented for scrollbars fixing
	//gtk_moz_embed_set_chrome_mask(GTK_MOZ_EMBED(gMinimoBrowser->mozEmbed), chromeMask);
	
	gMinimoBrowser->active_page = get_active_tab (gMinimoBrowser);
	
	return gMinimoBrowser;
}

void setup_toolbar(MinimoBrowser* browser) {
  
	MinimoToolbar* toolbar = &browser->toolbar;
	GtkWidget *menu;
	
#ifdef MOZ_WIDGET_GTK
	toolbar->toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);
#endif
	
#ifdef MOZ_WIDGET_GTK2
	toolbar->toolbar = gtk_toolbar_new();
	gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar->toolbar),GTK_ORIENTATION_HORIZONTAL);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar->toolbar),GTK_TOOLBAR_ICONS);
#endif /* MOZ_WIDGET_GTK2 */
	
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar->toolbar), GTK_ICON_SIZE_SMALL_TOOLBAR);
	
	gtk_widget_show(toolbar->toolbar);
	
	toolbar->OpenButton =(GtkWidget*) gtk_tool_button_new_from_stock ("gtk-open");
	gtk_widget_show(toolbar->OpenButton);
	gtk_container_add(GTK_CONTAINER (toolbar->toolbar), toolbar->OpenButton);
	g_signal_connect(GTK_OBJECT(toolbar->OpenButton), "clicked", G_CALLBACK(open_url_dialog_input_cb), browser);
	
	toolbar->BackButton = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-go-back");
	gtk_widget_show(toolbar->BackButton);
	gtk_container_add(GTK_CONTAINER (toolbar->toolbar), toolbar->BackButton);
	g_signal_connect(GTK_OBJECT(toolbar->BackButton), "clicked", G_CALLBACK(back_clicked_cb), browser);
	
	toolbar->ReloadButton = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-refresh");
	gtk_widget_show(toolbar->ReloadButton);
	gtk_container_add(GTK_CONTAINER (toolbar->toolbar), toolbar->ReloadButton);
	g_signal_connect(GTK_OBJECT(toolbar->ReloadButton), "clicked", G_CALLBACK(reload_clicked_cb), browser);
	
	toolbar->ForwardButton = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-go-forward");
	gtk_widget_show(toolbar->ForwardButton);
	gtk_container_add(GTK_CONTAINER (toolbar->toolbar), toolbar->ForwardButton);
	g_signal_connect(GTK_OBJECT(toolbar->ForwardButton), "clicked", G_CALLBACK(forward_clicked_cb), browser);
	
	toolbar->StopButton =(GtkWidget*) gtk_tool_button_new_from_stock ("gtk-stop");
	gtk_widget_show (toolbar->StopButton);
	gtk_container_add(GTK_CONTAINER (toolbar->toolbar), toolbar->StopButton);
	g_signal_connect(GTK_OBJECT(toolbar->StopButton), "clicked", G_CALLBACK(stop_clicked_cb), browser);
	
	toolbar->PrefsButton = (GtkWidget*) gtk_menu_tool_button_new_from_stock("gtk-preferences");
	gtk_widget_show (toolbar->PrefsButton);
	gtk_container_add(GTK_CONTAINER (toolbar->toolbar), toolbar->PrefsButton);
	g_signal_connect(GTK_OBJECT(toolbar->PrefsButton), "clicked", G_CALLBACK(pref_clicked_cb), browser);
	menu = populate_menu_button (browser);
	gtk_menu_tool_button_set_menu ((GtkMenuToolButton *) toolbar->PrefsButton, menu);
	
	toolbar->InfoButton = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-dialog-info");
	gtk_widget_show(toolbar->InfoButton);
	gtk_container_add(GTK_CONTAINER (toolbar->toolbar), toolbar->InfoButton);
	g_signal_connect(GTK_OBJECT(toolbar->InfoButton), "clicked", G_CALLBACK(info_clicked_cb), browser);
	
	toolbar->QuitButton = (GtkWidget*) gtk_tool_button_new_from_stock ("gtk-quit");
	gtk_widget_show(toolbar->QuitButton);
	gtk_container_add(GTK_CONTAINER (toolbar->toolbar), toolbar->QuitButton);  
	g_signal_connect(GTK_OBJECT(toolbar->QuitButton), "clicked", G_CALLBACK(destroy_cb), browser);
}

static GtkWidget *populate_menu_button(MinimoBrowser* browser){
	
	GtkWidget *menu, *menu_item;
	GtkWidget *image;
	
	menu = gtk_menu_new();
	
	menu_item = gtk_image_menu_item_new_with_label ("Show/Hide Tabs");
	gtk_menu_shell_append  ((GtkMenuShell *)(menu),menu_item);
	g_signal_connect(GTK_OBJECT(menu_item), "activate", G_CALLBACK(show_hide_tabs_cb), browser);
	image = gtk_image_new_from_stock(GTK_STOCK_DND_MULTIPLE, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
	
	menu_item = gtk_image_menu_item_new_with_label ("Add Tab");
	gtk_menu_shell_append  ((GtkMenuShell *)(menu),menu_item);
	g_signal_connect(GTK_OBJECT(menu_item), "activate", G_CALLBACK(open_new_tab_cb), browser);
	image = gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
	
	menu_item = gtk_image_menu_item_new_with_label ("Close Tab");
	gtk_menu_shell_append  ((GtkMenuShell *)(menu),menu_item);
	g_signal_connect(GTK_OBJECT(menu_item), "activate", G_CALLBACK(close_current_tab_cb), browser);
	image = gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
	
	menu_item = gtk_image_menu_item_new_with_label ("Open Bookmark Window");
	gtk_menu_shell_append  ((GtkMenuShell *)(menu),menu_item);
	g_signal_connect(GTK_OBJECT(menu_item), "activate", G_CALLBACK(open_bookmark_window_cb), browser);
	image = gtk_image_new_from_stock(GTK_STOCK_HARDDISK, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
	
	menu_item = gtk_image_menu_item_new_with_label ("Add Bookmark");
	gtk_menu_shell_append  ((GtkMenuShell *)(menu),menu_item);
	g_signal_connect(GTK_OBJECT(menu_item), "activate", G_CALLBACK(bookmark_add_url_directly_cb), browser);
	image = gtk_image_new_from_stock(GTK_STOCK_YES, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
	
	menu_item = gtk_image_menu_item_new_with_label ("Find");
	gtk_menu_shell_append  ((GtkMenuShell *)(menu),menu_item);
	g_signal_connect(GTK_OBJECT(menu_item), "activate", G_CALLBACK(create_find_dialog), browser);
	image = gtk_image_new_from_stock(GTK_STOCK_FIND, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
	
	menu_item = gtk_image_menu_item_new_with_label ("Save As");
	gtk_menu_shell_append  ((GtkMenuShell *)(menu),menu_item);
	g_signal_connect(GTK_OBJECT(menu_item), "activate", G_CALLBACK(create_save_document_dialog), browser);
	image = gtk_image_new_from_stock(GTK_STOCK_SAVE_AS, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
	
	menu_item = gtk_image_menu_item_new_with_label ("Open From File");
	gtk_menu_shell_append  ((GtkMenuShell *)(menu),menu_item);
	g_signal_connect(GTK_OBJECT(menu_item), "activate", G_CALLBACK(create_open_document_from_file), browser);
	image = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
	
	menu_item = gtk_image_menu_item_new_with_label ("Increase Font Size");
	gtk_menu_shell_append  ((GtkMenuShell *)(menu),menu_item);
	g_signal_connect(GTK_OBJECT(menu_item), "activate", G_CALLBACK(increase_font_size_cb), browser);
	image = gtk_image_new_from_stock(GTK_STOCK_ZOOM_IN, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
	
	menu_item = gtk_image_menu_item_new_with_label ("Decrease Font Size");
	gtk_menu_shell_append  ((GtkMenuShell *)(menu),menu_item);
	g_signal_connect(GTK_OBJECT(menu_item), "activate", G_CALLBACK(decrease_font_size_cb), browser);
	image = gtk_image_new_from_stock(GTK_STOCK_ZOOM_OUT, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
	
	menu_item = gtk_image_menu_item_new_with_label ("Full Screen");
	gtk_menu_shell_append  ((GtkMenuShell *)(menu),menu_item);
	g_signal_connect(GTK_OBJECT(menu_item), "activate", G_CALLBACK(full_screen_cb), browser);
	image = gtk_image_new_from_stock(GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
	
	menu_item = gtk_image_menu_item_new_with_label ("History");
	gtk_menu_shell_append  ((GtkMenuShell *)(menu),menu_item);
	g_signal_connect(GTK_OBJECT(menu_item), "activate", G_CALLBACK(open_history_window), browser);
	
	gtk_widget_show_all(menu);
	
	return (menu);
}

GtkWidget* create_minimo_progress (MinimoBrowser* browser) {
  
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

static void new_window_orphan_cb(GtkMozEmbedSingle *embed,
                                 GtkMozEmbed **retval, guint chromemask,
                                 gpointer data)
{
	MinimoBrowser *newBrowser = new_gtk_browser(chromemask);
	*retval = GTK_MOZ_EMBED(newBrowser->mozEmbed);
}

/* METHODS TO MANIPULATE TABS */
GtkWidget *get_active_tab (MinimoBrowser *browser)
{
	GtkWidget *active_page;
	gint current_page;
	
	current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (browser->notebook));
	active_page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (browser->notebook), current_page);
	
	return active_page;
}


/* CALLBACKS METHODS ... */

void
destroy_cb(GtkWidget *widget, MinimoBrowser *browser)
{
	GList *tmp_list;
	gMinimoNumBrowsers--;
	tmp_list = g_list_find(gMinimoBrowserList, browser);
	gMinimoBrowserList = g_list_remove_link(gMinimoBrowserList, tmp_list);
	
	//  if (gMinimoNumBrowsers == 0)  
	gtk_main_quit();
}

/* OTHERS METHODS */

void update_ui (MinimoBrowser *browser)
{
	gchar *tmp;
	int x=0;
	char *url;
	
	tmp = gtk_moz_embed_get_title (GTK_MOZ_EMBED (browser->active_page));
	
	if (tmp)
		x = strlen (tmp);
	if (x == 0)
		tmp = "Minimo";
	gtk_window_set_title (GTK_WINDOW(browser->topLevelWindow), tmp);
	url = gtk_moz_embed_get_location (GTK_MOZ_EMBED (browser->active_page));
	
	browser->progressPopup = create_minimo_progress(browser);
}


static void switch_page_cb (GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, MinimoBrowser *browser)
{
	browser->active_page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (browser->notebook), page_num);
	browser->mozEmbed = browser->active_page;
	update_ui (browser);
	bookmark_moz_embed_initialize (browser->mozEmbed);
}


gboolean delete_cb(GtkWidget *widget, GdkEventAny *event, MinimoBrowser *browser)
{
	gtk_widget_destroy(widget);
	return TRUE;
}

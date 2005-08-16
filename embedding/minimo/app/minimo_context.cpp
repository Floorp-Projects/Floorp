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

#include "minimo_context.h"
#include "minimo_callbacks.h"
#include "minimo_bookmark.h"

RightButtonClick *gContextRightButtonClick;	/* Emulating right button clicks */

void context_initialize_timer()
{
	/* right button click emulation initialization */
	gContextRightButtonClick = g_new0(RightButtonClick,1);
	gContextRightButtonClick->sig_handler= 0;
	gContextRightButtonClick->pressing_timer= g_timer_new();
	g_timer_reset(gContextRightButtonClick->pressing_timer);
}

GtkWidget * context_click_on_link (MinimoBrowser *browser, gchar *href, gchar *linktext)
{
	GtkWidget *menu, *m1, *m2, *m3;
	
	if (browser->link_menu) gtk_widget_destroy(browser->link_menu);
	menu = gtk_menu_new();
	g_object_set_data(G_OBJECT(menu), "freehref", href);
	g_object_set_data(G_OBJECT(menu), "freelinktext", linktext);
	
	/* Open in this Window */
	m1 = gtk_image_menu_item_new_with_mnemonic("_Open link ...");
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(m1),
					gtk_image_new_from_stock(GTK_STOCK_OPEN,GTK_ICON_SIZE_MENU));
	g_signal_connect(G_OBJECT(m1), "activate", G_CALLBACK(browse_in_window_from_popup), browser);
	g_object_set_data(G_OBJECT(m1), "link", href);
	gtk_menu_append(GTK_MENU(menu), m1);
	
	/* Open in a new Tab */
	m2 = gtk_image_menu_item_new_with_mnemonic("Open in a new _tab");
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(m2),
					gtk_image_new_from_stock(GTK_STOCK_NEW,GTK_ICON_SIZE_MENU));
	gtk_menu_append(GTK_MENU(menu), m2);
	g_object_set_data(G_OBJECT(m2), "link", href);
	g_signal_connect(G_OBJECT(m2), "activate", G_CALLBACK(make_tab_from_popup), browser);		
	
	/* Download link */
	m3  = gtk_image_menu_item_new_with_mnemonic("_Save link as ...");
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(m3),
					gtk_image_new_from_stock(GTK_STOCK_SAVE, GTK_ICON_SIZE_MENU));
	g_signal_connect(G_OBJECT(m3), "activate", G_CALLBACK(download_link_from_popup), browser);
	g_object_set_data(G_OBJECT(m3), "link", href);
	gtk_menu_append(GTK_MENU(menu), m3);
	
	gtk_widget_show_all(menu);
	browser->link_menu = menu;
	return (menu);
  
}

GtkWidget * context_click_on_image (MinimoBrowser *browser, gchar *img)
{
	GtkWidget *menu, *m1, *m2, *m3, *m4, *sep;
	gchar *lbl, *name, *aux_string;
	G_CONST_RETURN gchar *img_basename;
	
	if (browser->image_menu)
	gtk_widget_destroy(browser->image_menu);
	
	aux_string = (gchar *)g_malloc(1024);
	strcpy (aux_string, "");
	img_basename = g_basename(img);
	name = g_strdup(img_basename);
	menu = gtk_menu_new();
	g_object_set_data(G_OBJECT(menu), "freeimg", img);
	g_object_set_data(G_OBJECT(menu), "freename", name);
	
	/* View Image */
	m1 = gtk_image_menu_item_new_with_mnemonic ("_View image");
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(m1),
					gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU));
	g_signal_connect(G_OBJECT(m1), "activate", G_CALLBACK(browse_in_window_from_popup), browser);
	g_object_set_data(G_OBJECT(m1), "link", img);
	gtk_menu_append(GTK_MENU(menu), m1);
	
	/* Separator */
	sep = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), sep);
	
	/* Save as */
	m2  = gtk_image_menu_item_new_with_mnemonic ("_Save image as ...");
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(m2),
					gtk_image_new_from_stock(GTK_STOCK_SAVE, GTK_ICON_SIZE_MENU));
	g_signal_connect(G_OBJECT(m2), "activate", G_CALLBACK(download_link_from_popup), browser);
	g_object_set_data(G_OBJECT(m2), "link", img);
	g_object_set_data(G_OBJECT(m2), "name", aux_string);
	gtk_menu_append(GTK_MENU(menu), m2);
	
	/* Save */
	lbl = g_strdup_printf("%s %s","Sa_ve image as ",img_basename);
	m3  = gtk_image_menu_item_new_with_mnemonic (lbl);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(m3),
					gtk_image_new_from_stock(GTK_STOCK_SAVE, GTK_ICON_SIZE_MENU));
	g_signal_connect(G_OBJECT(m3), "activate", G_CALLBACK(save_image_from_popup), browser);
	g_object_set_data(G_OBJECT(m3), "link", img);
	g_object_set_data(G_OBJECT(m3), "name", name);
	gtk_menu_append(GTK_MENU(menu), m3);
	
	/* Separator */
	sep = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), sep);
	
	/* Save page as */
	m4 = gtk_image_menu_item_new_with_mnemonic("Save page _as ... ");
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(m4),
					gtk_image_new_from_stock(GTK_STOCK_COPY, GTK_ICON_SIZE_MENU));	
	g_signal_connect(G_OBJECT(m4), "activate", G_CALLBACK(download_link_from_popup), browser);
	g_object_set_data(G_OBJECT(m4), "link", gtk_moz_embed_get_location(GTK_MOZ_EMBED (browser->mozEmbed)));
	gtk_menu_append(GTK_MENU(menu), m4);
	gtk_widget_show_all(menu);
	
	gtk_widget_show_all(menu);
	
	g_free(aux_string);	
	g_free(lbl);
	
	browser->image_menu = menu;
	return (menu);
}

GtkWidget * context_click_doc (MinimoBrowser *browser, gchar *href)
{
	GtkWidget *menu, *m1,*m2,*m3, *m4, *m5, *m6, *sep;
	
	if (browser->doc_menu) gtk_widget_destroy(browser->doc_menu);
	menu = gtk_menu_new();
	g_object_set_data(G_OBJECT(menu), "freehref", href);
	
	m1 =  gtk_image_menu_item_new_from_stock(GTK_STOCK_GO_BACK,NULL);
	g_signal_connect(G_OBJECT(m1), "activate", G_CALLBACK(back_clicked_cb), browser);
	gtk_widget_set_sensitive(m1, gtk_moz_embed_can_go_back(GTK_MOZ_EMBED(browser->mozEmbed)));
	gtk_menu_append(GTK_MENU(menu), m1);
	
	m2 = gtk_image_menu_item_new_from_stock(GTK_STOCK_GO_FORWARD,NULL);
	gtk_menu_append(GTK_MENU(menu), m2);
	g_signal_connect(G_OBJECT(m2), "activate", G_CALLBACK(forward_clicked_cb), browser);
	gtk_widget_set_sensitive(m2, gtk_moz_embed_can_go_forward(GTK_MOZ_EMBED(browser->mozEmbed)));
	
	m3  = gtk_image_menu_item_new_from_stock(GTK_STOCK_REFRESH,NULL);     
	g_signal_connect(G_OBJECT(m3), "activate", G_CALLBACK(reload_clicked_cb), browser);
	gtk_menu_append(GTK_MENU(menu), m3);
	
	m4 = gtk_image_menu_item_new_from_stock(GTK_STOCK_STOP,NULL);
	g_signal_connect(G_OBJECT(m4), "activate", G_CALLBACK(stop_clicked_cb), browser);
	gtk_menu_append(GTK_MENU(menu), m4);
	
	/* Separator */
	sep = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), sep);
	
	m5 = gtk_image_menu_item_new_with_mnemonic ("_Save page as ... ");
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(m5),
					gtk_image_new_from_stock(GTK_STOCK_SAVE, GTK_ICON_SIZE_MENU));
	g_signal_connect(G_OBJECT(m5), "activate", G_CALLBACK(download_link_from_popup), browser);
	g_object_set_data(G_OBJECT(m5), "link", gtk_moz_embed_get_location(GTK_MOZ_EMBED (browser->mozEmbed)));
	gtk_menu_append(GTK_MENU(menu), m5);
	
	m6 = gtk_image_menu_item_new_with_mnemonic ("_Bookmark this page");
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(m6),
					gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU));
	g_signal_connect(G_OBJECT(m6), "activate", G_CALLBACK(bookmark_add_url_directly_cb), browser);
	gtk_menu_append(GTK_MENU(menu), m6);
	
	gtk_widget_show_all(menu);
	browser->doc_menu = menu;
	
	return ( menu );	
}

GtkWidget * context_click_link_image (MinimoBrowser *browser, gchar *img, gchar *href)
{
	GtkWidget *menu, *m1, *m2, *m3, *m4, *m5, *sep;
	gchar *lbl, *name, *aux_string;
	G_CONST_RETURN gchar *img_basename;
	
	if (browser->image_link_menu) 
	gtk_widget_destroy(browser->image_link_menu);
	
	aux_string= (gchar *)g_malloc(1024);
	strcpy (aux_string, "link image menu");
	
	img_basename = g_basename(img);
	menu = gtk_menu_new();
	name = g_strdup(img_basename);
	g_object_set_data(G_OBJECT(menu), "freehref", href);
	g_object_set_data(G_OBJECT(menu), "freeimg", img);
	g_object_set_data(G_OBJECT(menu), "freename", name);
	g_signal_connect(G_OBJECT(menu), "destroy", G_CALLBACK(context_destroy_popup), aux_string);
	
	/* Open Link in the current Window */
	m1 = gtk_image_menu_item_new_with_mnemonic ("_Open link ...");
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(m1),gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU));
	g_signal_connect(G_OBJECT(m1), "activate", G_CALLBACK(browse_in_window_from_popup), browser);
	g_object_set_data(G_OBJECT(m1), "link", href);
	gtk_menu_append(GTK_MENU(menu), m1);
	
	/* Open Link in a new tab */		
	m2 = gtk_image_menu_item_new_with_mnemonic ("Open link in a new _Tab");
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(m2), 
					gtk_image_new_from_stock(GTK_STOCK_NEW, GTK_ICON_SIZE_MENU));
	gtk_menu_append(GTK_MENU(menu), m2);
	g_object_set_data(G_OBJECT(m2), "link", href);
	g_signal_connect(G_OBJECT(m2), "activate", G_CALLBACK(make_tab_from_popup), browser);
	
	/* Separator */
	sep = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), sep);
	
	/* View Image */
	m3 = gtk_image_menu_item_new_with_mnemonic ("_View image");
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(m3),
					gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU));
	g_signal_connect(G_OBJECT(m3), "activate", G_CALLBACK(browse_in_window_from_popup), browser);
	g_object_set_data(G_OBJECT(m3), "link", img);	     
	gtk_menu_append(GTK_MENU(menu), m3);
	
	/* Separator */
	sep = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), sep);
	
	m4 = gtk_image_menu_item_new_with_mnemonic ("_Save link as ...");
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(m4),gtk_image_new_from_stock(GTK_STOCK_SAVE,
											GTK_ICON_SIZE_MENU));
	g_signal_connect(G_OBJECT(m4), "activate", G_CALLBACK(download_link_from_popup), browser);
	g_object_set_data(G_OBJECT(m4), "link", href);
	gtk_menu_append(GTK_MENU(menu), m4);
	
	lbl = g_strdup_printf("%s %s","Save image _as ",img_basename);
	m5  = gtk_image_menu_item_new_with_mnemonic (lbl);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(m5),gtk_image_new_from_stock(GTK_STOCK_SAVE,
											GTK_ICON_SIZE_MENU));
	g_signal_connect(G_OBJECT(m5), "activate", G_CALLBACK(save_image_from_popup), browser);
	g_object_set_data(G_OBJECT(m5), "link", img);
	g_object_set_data(G_OBJECT(m5), "name", name);
	gtk_menu_append(GTK_MENU(menu), m5);
	
	gtk_widget_show_all(menu);
	browser->image_link_menu = menu;
	
	g_free(aux_string);
	
	return ( menu );
}

void context_destroy_popup (GtkWidget *menu, gchar *name)
{
	gchar *free_href, *free_img, *free_linktext, *free_name;
	
	free_href = (gchar *) g_object_get_data(G_OBJECT(menu), "freehref");

	if (free_href) g_free(free_href);
		free_img = (gchar *) g_object_get_data(G_OBJECT(menu), "freeimg");
	if (free_img) g_free(free_img);
		free_linktext = (gchar *) g_object_get_data(G_OBJECT(menu), "freelinktext");
	if (free_linktext) g_free(free_linktext);
		free_name = (gchar *) g_object_get_data(G_OBJECT(menu), "freename");
	if (free_name) g_free(free_name);
		return;
}

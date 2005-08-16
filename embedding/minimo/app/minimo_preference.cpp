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


#include "minimo_preference.h"
#include "minimo_support.h"

extern gchar *gMinimoUserHomePath;

ConfigData gPreferenceConfigStruct;
PrefWindow *gPreferenceWinStruct;

PrefWindow * preferences_build_dialog (GtkWidget *topLevelWindow) {
  
	gPreferenceWinStruct = g_new0(PrefWindow,1);
	
	gPreferenceWinStruct->dialog = gtk_dialog_new ();
	
	gtk_window_set_title (GTK_WINDOW (gPreferenceWinStruct->dialog), ("Prefs Window"));
	gtk_window_set_position (GTK_WINDOW (gPreferenceWinStruct->dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_modal (GTK_WINDOW (gPreferenceWinStruct->dialog), TRUE);
	gtk_window_set_resizable (GTK_WINDOW (gPreferenceWinStruct->dialog), FALSE);
	gtk_window_set_decorated (GTK_WINDOW (gPreferenceWinStruct->dialog), TRUE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (gPreferenceWinStruct->dialog), TRUE);
	gtk_window_set_skip_pager_hint (GTK_WINDOW (gPreferenceWinStruct->dialog), TRUE);
	gtk_window_set_type_hint (GTK_WINDOW (gPreferenceWinStruct->dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_gravity (GTK_WINDOW (gPreferenceWinStruct->dialog), GDK_GRAVITY_NORTH_EAST);
	gtk_window_set_transient_for(GTK_WINDOW(gPreferenceWinStruct->dialog), GTK_WINDOW(topLevelWindow));
	gtk_widget_set_size_request (gPreferenceWinStruct->dialog, 240, 220);
	
	/* Creating "Connection Frame" */
	gPreferenceWinStruct->fr_connection = gtk_frame_new (NULL);
	gtk_widget_show (gPreferenceWinStruct->fr_connection);
	gtk_container_add (GTK_CONTAINER(GTK_DIALOG (gPreferenceWinStruct->dialog)->vbox), gPreferenceWinStruct->fr_connection);
	
	/* Connection Frame's VBOX */
	gPreferenceWinStruct->vbox_connection = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (gPreferenceWinStruct->vbox_connection);
	gtk_container_add (GTK_CONTAINER (gPreferenceWinStruct->fr_connection), gPreferenceWinStruct->vbox_connection);
	
	gPreferenceWinStruct->hbox_direct_connection = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (gPreferenceWinStruct->hbox_direct_connection);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->vbox_connection), gPreferenceWinStruct->hbox_direct_connection, TRUE, TRUE, 0);
	
	/* Radio Buttons - Direct/Manual Connetion */
	gPreferenceWinStruct->rb_direct_connection = gtk_radio_button_new_with_mnemonic (NULL, "Direct connection to the Internet");
	gtk_widget_show (gPreferenceWinStruct->rb_direct_connection);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->hbox_direct_connection), gPreferenceWinStruct->rb_direct_connection, FALSE, FALSE, 0);
	gPreferenceWinStruct->rb_direct_connection_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (gPreferenceWinStruct->rb_direct_connection));
	
	gPreferenceWinStruct->hbox_manual_connection = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (gPreferenceWinStruct->hbox_manual_connection);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->vbox_connection), gPreferenceWinStruct->hbox_manual_connection, TRUE, TRUE, 0);
	
	gPreferenceWinStruct->vbox_manual = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (gPreferenceWinStruct->vbox_manual);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->hbox_manual_connection), gPreferenceWinStruct->vbox_manual, TRUE, TRUE, 0);
	
	gPreferenceWinStruct->rb_manual_connection = gtk_radio_button_new_with_mnemonic (NULL, "Manual proxy configuration");
	gtk_widget_show (gPreferenceWinStruct->rb_manual_connection);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->vbox_manual), gPreferenceWinStruct->rb_manual_connection, FALSE, FALSE, 0);
	
	gPreferenceWinStruct->rb_manual_connection_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (gPreferenceWinStruct->rb_manual_connection));
	gtk_radio_button_set_group (GTK_RADIO_BUTTON (gPreferenceWinStruct->rb_manual_connection), gPreferenceWinStruct->rb_direct_connection_group);
	
	gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gPreferenceWinStruct->rb_direct_connection));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gPreferenceWinStruct->rb_direct_connection), gPreferenceConfigStruct.direct_connection);
	
	/* HTTP Widget HBOX */
	gPreferenceWinStruct->hbox_http = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (gPreferenceWinStruct->hbox_http);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->vbox_manual), gPreferenceWinStruct->hbox_http, TRUE, TRUE, 0);
	
	gPreferenceWinStruct->lb_http = gtk_label_new ("HTTP Proxy:  ");
	gtk_widget_show (gPreferenceWinStruct->lb_http);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->hbox_http), gPreferenceWinStruct->lb_http, FALSE, FALSE, 0);
	
	gPreferenceWinStruct->en_http_proxy = gtk_entry_new ();
	gtk_widget_show (gPreferenceWinStruct->en_http_proxy);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->hbox_http), gPreferenceWinStruct->en_http_proxy, TRUE, TRUE, 0);
	gtk_widget_modify_font(gPreferenceWinStruct->en_http_proxy, getOrCreateDefaultMinimoFont());
	gtk_widget_set_size_request (gPreferenceWinStruct->en_http_proxy, 60, -1);
	
	gPreferenceWinStruct->lb_port_http = gtk_label_new (" Port: ");
	gtk_widget_show (gPreferenceWinStruct->lb_port_http);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->hbox_http), gPreferenceWinStruct->lb_port_http, FALSE, FALSE, 0);
	
	gPreferenceWinStruct->en_http_port = gtk_entry_new ();
	gtk_widget_show (gPreferenceWinStruct->en_http_port);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->hbox_http), gPreferenceWinStruct->en_http_port, TRUE, TRUE, 0);
	gtk_widget_set_size_request (gPreferenceWinStruct->en_http_port, 30, -1);
	
	/* SSL Widget HBOX */
	gPreferenceWinStruct->hbox_ssl = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (gPreferenceWinStruct->hbox_ssl);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->vbox_manual), gPreferenceWinStruct->hbox_ssl, TRUE, TRUE, 0);
	
	gPreferenceWinStruct->lb_ssl = gtk_label_new ("SSL Proxy:    ");
	gtk_widget_show (gPreferenceWinStruct->lb_ssl);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->hbox_ssl), gPreferenceWinStruct->lb_ssl, FALSE, FALSE, 0);
	
	gPreferenceWinStruct->en_ssl = gtk_entry_new ();
	gtk_widget_show (gPreferenceWinStruct->en_ssl);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->hbox_ssl), gPreferenceWinStruct->en_ssl, TRUE, TRUE, 0);
	gtk_widget_set_size_request (gPreferenceWinStruct->en_ssl, 60, -1);
	
	gPreferenceWinStruct->lb_ssl_port = gtk_label_new (" Port: ");
	gtk_widget_show (gPreferenceWinStruct->lb_ssl_port);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->hbox_ssl), gPreferenceWinStruct->lb_ssl_port, FALSE, FALSE, 0);
	
	gPreferenceWinStruct->en_ssl_port = gtk_entry_new ();
	gtk_widget_show (gPreferenceWinStruct->en_ssl_port);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->hbox_ssl), gPreferenceWinStruct->en_ssl_port, TRUE, TRUE, 0);
	gtk_widget_modify_font(gPreferenceWinStruct->en_ssl, getOrCreateDefaultMinimoFont());
	gtk_widget_set_size_request (gPreferenceWinStruct->en_ssl_port, 30, -1);
	
	/* FTP Widgets HBOX */
	gPreferenceWinStruct->hbox_ftp = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (gPreferenceWinStruct->hbox_ftp);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->vbox_manual), gPreferenceWinStruct->hbox_ftp, TRUE, TRUE, 0);
	
	gPreferenceWinStruct->lb_ftp = gtk_label_new ("FTP Proxy:    ");
	gtk_widget_show (gPreferenceWinStruct->lb_ftp);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->hbox_ftp), gPreferenceWinStruct->lb_ftp, FALSE, FALSE, 0);
	
	gPreferenceWinStruct->en_ftp_proxy = gtk_entry_new ();
	gtk_widget_show (gPreferenceWinStruct->en_ftp_proxy);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->hbox_ftp), gPreferenceWinStruct->en_ftp_proxy, TRUE, TRUE, 0);
	gtk_widget_set_size_request (gPreferenceWinStruct->en_ftp_proxy, 60, -1);
	
	gPreferenceWinStruct->lb_ftp_port = gtk_label_new (" Port: ");
	gtk_widget_show (gPreferenceWinStruct->lb_ftp_port);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->hbox_ftp), gPreferenceWinStruct->lb_ftp_port, FALSE, FALSE, 0);
	
	gPreferenceWinStruct->en_ftp_port = gtk_entry_new ();
	gtk_widget_show (gPreferenceWinStruct->en_ftp_port);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->hbox_ftp), gPreferenceWinStruct->en_ftp_port, TRUE, TRUE, 0);
	gtk_widget_modify_font(gPreferenceWinStruct->en_ftp_proxy, getOrCreateDefaultMinimoFont());
	gtk_widget_set_size_request (gPreferenceWinStruct->en_ftp_port, 30, -1);
	
	gPreferenceWinStruct->hbox_noproxy = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (gPreferenceWinStruct->hbox_noproxy);
	gtk_box_pack_start (GTK_BOX (gPreferenceWinStruct->vbox_connection), gPreferenceWinStruct->hbox_noproxy, TRUE, TRUE, 0);
	
	if (gPreferenceConfigStruct.direct_connection == 0) {		
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gPreferenceWinStruct->rb_direct_connection), FALSE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gPreferenceWinStruct->rb_manual_connection), TRUE);
	
	} else if (gPreferenceConfigStruct.direct_connection == 1) {
	
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gPreferenceWinStruct->rb_direct_connection), TRUE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gPreferenceWinStruct->rb_manual_connection), FALSE);
	}
	
	gtk_entry_set_text (GTK_ENTRY (gPreferenceWinStruct->en_http_proxy), gPreferenceConfigStruct.http_proxy);
	gtk_entry_set_text (GTK_ENTRY (gPreferenceWinStruct->en_http_port), gPreferenceConfigStruct.http_proxy_port);
	gtk_entry_set_text (GTK_ENTRY (gPreferenceWinStruct->en_ssl), gPreferenceConfigStruct.ssl_proxy);
	gtk_entry_set_text (GTK_ENTRY (gPreferenceWinStruct->en_ssl_port), gPreferenceConfigStruct.ssl_proxy_port);
	gtk_entry_set_text (GTK_ENTRY (gPreferenceWinStruct->en_ftp_proxy), gPreferenceConfigStruct.ftp_proxy);
	gtk_entry_set_text (GTK_ENTRY (gPreferenceWinStruct->en_ftp_port), gPreferenceConfigStruct.ftp_proxy_port);
	
	/* Creating "Popup Frame" */
	gPreferenceWinStruct->fr_popup = gtk_frame_new (NULL);
	gtk_widget_show (gPreferenceWinStruct->fr_popup);
	gtk_container_add (GTK_CONTAINER(GTK_DIALOG (gPreferenceWinStruct->dialog)->vbox), gPreferenceWinStruct->fr_popup);
	
	/* Proxy Frame's label */
	gPreferenceWinStruct->lb_connection = gtk_label_new ("Configure Proxies:");
	gtk_widget_show (gPreferenceWinStruct->lb_connection);
	gtk_frame_set_label_widget (GTK_FRAME (gPreferenceWinStruct->fr_connection), gPreferenceWinStruct->lb_connection);
	
	gPreferenceWinStruct->vbox_popup = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (gPreferenceWinStruct->vbox_popup);
	gtk_container_add (GTK_CONTAINER (gPreferenceWinStruct->fr_popup), gPreferenceWinStruct->vbox_popup);
	
	gPreferenceWinStruct->block_popup = gtk_check_button_new_with_label ("Block Popup Windows");
	if (gPreferenceConfigStruct.disable_popups == 1) {
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(gPreferenceWinStruct->block_popup), TRUE);
	} else 
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(gPreferenceWinStruct->block_popup), FALSE);
	
	gtk_container_add(GTK_CONTAINER(gPreferenceWinStruct->vbox_popup), gPreferenceWinStruct->block_popup);
	
	gPreferenceWinStruct->lb_popup = gtk_label_new ("Block Popups");
	gtk_widget_show (gPreferenceWinStruct->lb_popup);
	gtk_frame_set_label_widget (GTK_FRAME (gPreferenceWinStruct->fr_popup), gPreferenceWinStruct->lb_popup);
	
	/* Ok and Cancel Button */
	gPreferenceWinStruct->cancelbutton = gtk_button_new_with_label("Cancel");
	gtk_widget_modify_font(GTK_BIN(gPreferenceWinStruct->cancelbutton)->child, getOrCreateDefaultMinimoFont());
	gtk_widget_show (gPreferenceWinStruct->cancelbutton);
	gtk_dialog_add_action_widget (GTK_DIALOG (gPreferenceWinStruct->dialog), gPreferenceWinStruct->cancelbutton, GTK_RESPONSE_CANCEL);
	gtk_widget_set_size_request (gPreferenceWinStruct->cancelbutton, 10, -1);
	
	gPreferenceWinStruct->okbutton = gtk_button_new_with_label("Ok");
	gtk_widget_modify_font(GTK_BIN(gPreferenceWinStruct->okbutton)->child, getOrCreateDefaultMinimoFont());
	gtk_widget_show (gPreferenceWinStruct->okbutton);
	gtk_dialog_add_action_widget (GTK_DIALOG (gPreferenceWinStruct->dialog), gPreferenceWinStruct->okbutton, GTK_RESPONSE_OK);
	gtk_widget_set_size_request (gPreferenceWinStruct->okbutton, 10, -1);
	
	/* Setting font styles */
	gtk_widget_modify_font(gPreferenceWinStruct->rb_manual_connection, getOrCreateDefaultMinimoFont());
	gtk_widget_modify_font(gPreferenceWinStruct->rb_direct_connection, getOrCreateDefaultMinimoFont());
	gtk_widget_modify_font(gPreferenceWinStruct->lb_http, getOrCreateDefaultMinimoFont());  
	gtk_widget_modify_font(gPreferenceWinStruct->lb_port_http, getOrCreateDefaultMinimoFont());
	gtk_widget_modify_font(gPreferenceWinStruct->en_http_port, getOrCreateDefaultMinimoFont());
	gtk_widget_modify_font(gPreferenceWinStruct->lb_ssl, getOrCreateDefaultMinimoFont());  
	gtk_widget_modify_font(gPreferenceWinStruct->en_ssl, getOrCreateDefaultMinimoFont());
	gtk_widget_modify_font(gPreferenceWinStruct->en_ssl_port, getOrCreateDefaultMinimoFont());
	gtk_widget_modify_font(gPreferenceWinStruct->lb_ssl_port, getOrCreateDefaultMinimoFont());
	gtk_widget_modify_font(gPreferenceWinStruct->lb_ftp, getOrCreateDefaultMinimoFont());
	gtk_widget_modify_font(gPreferenceWinStruct->en_ftp_proxy, getOrCreateDefaultMinimoFont());
	gtk_widget_modify_font(gPreferenceWinStruct->lb_ftp_port, getOrCreateDefaultMinimoFont());
	gtk_widget_modify_font(gPreferenceWinStruct->en_ftp_port, getOrCreateDefaultMinimoFont());
	gtk_widget_modify_font(gPreferenceWinStruct->lb_connection, getOrCreateDefaultMinimoFont());
	gtk_widget_modify_font(gPreferenceWinStruct->lb_popup, getOrCreateDefaultMinimoFont());
	gtk_widget_modify_font(gPreferenceWinStruct->block_popup, getOrCreateDefaultMinimoFont());
	
	gtk_widget_show_all(gPreferenceWinStruct->dialog);
	
	/* connection singnals to buttons */
	gtk_signal_connect_object(GTK_OBJECT(gPreferenceWinStruct->cancelbutton), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), gPreferenceWinStruct->dialog);
	gtk_signal_connect_object(GTK_OBJECT(gPreferenceWinStruct->okbutton), "clicked", GTK_SIGNAL_FUNC(preferences_ok_cb), gPreferenceWinStruct);
	
	return gPreferenceWinStruct;
}

void preferences_save_cb (GtkButton *button, PrefWindow *pref) {
  
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gPreferenceWinStruct->rb_direct_connection)) == TRUE) 
		gPreferenceConfigStruct.direct_connection = 1;
	else gPreferenceConfigStruct.direct_connection = 0;
  
	gPreferenceConfigStruct.http_proxy = g_strdup (gtk_entry_get_text (GTK_ENTRY (gPreferenceWinStruct->en_http_proxy)));
	gPreferenceConfigStruct.http_proxy_port = g_strdup (gtk_entry_get_text (GTK_ENTRY (gPreferenceWinStruct->en_http_port)));
	gPreferenceConfigStruct.ftp_proxy = g_strdup (gtk_entry_get_text (GTK_ENTRY (gPreferenceWinStruct->en_ftp_proxy)));
	gPreferenceConfigStruct.ftp_proxy_port = g_strdup (gtk_entry_get_text (GTK_ENTRY (gPreferenceWinStruct->en_ftp_port)));
	gPreferenceConfigStruct.ssl_proxy = g_strdup (gtk_entry_get_text (GTK_ENTRY (gPreferenceWinStruct->en_ssl)));
	gPreferenceConfigStruct.ssl_proxy_port = g_strdup (gtk_entry_get_text (GTK_ENTRY (gPreferenceWinStruct->en_ssl_port)));
	gPreferenceConfigStruct.disable_popups = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gPreferenceWinStruct->block_popup));
	
	preferences_write_config ();
}

void preferences_ok_cb (GtkButton *button, PrefWindow *pref) {

	preferences_save_cb (button, pref);
	preferences_close_cb (button, pref);
}

void preferences_close_cb (GtkButton *button, PrefWindow *pref) {

	gtk_widget_destroy (gPreferenceWinStruct->dialog);
	g_free (pref);
}

/* Read the current 'minimo's preferences and configs' into his file: $home/.Minimo/config */
void preferences_read_config (void) 
{
	FILE *config_file;
	gchar *file, *oldfile;
	gchar *line, tmpstr[1024];
  
	gint i, temp;
  
	gMinimoUserHomePath = g_strconcat(g_get_home_dir(),NULL);
  
	oldfile = g_strconcat(gMinimoUserHomePath,"/.minimorc", NULL);
	file = g_strconcat(gMinimoUserHomePath,"/.Minimo/config", NULL);
	if (access(oldfile, F_OK) == 0 && access(file, F_OK) == -1 && errno == ENOENT) {
		if (rename(oldfile, file) == 0) {
			g_print("Note: .minimorc was moved to %s\n", file);
			g_free(oldfile);
		} else {
      			g_warning("Error moving .minimorc to %s: %s\n", file, strerror(errno));
			/* we can still read the old file  */
			g_free(file); file = oldfile;
		}
	} else {
		g_free(oldfile);
	}
  
	/* defaults */   	
	gPreferenceConfigStruct.home = g_strdup("http://www.minimo.org");
	gPreferenceConfigStruct.xsize = 700;
	gPreferenceConfigStruct.ysize = 500;
	gPreferenceConfigStruct.layout = 0;
	gPreferenceConfigStruct.mailer = g_strdup("pronto %t %s %c");
	gPreferenceConfigStruct.max_go = 15;
	gPreferenceConfigStruct.maxpopupitems = 15;
	gPreferenceConfigStruct.java = TRUE;
	gPreferenceConfigStruct.javascript = TRUE;
	gPreferenceConfigStruct.http_proxy = g_strdup("");
	gPreferenceConfigStruct.ftp_proxy = g_strdup("");
	gPreferenceConfigStruct.ftp_proxy_port = g_strdup("");
	gPreferenceConfigStruct.http_proxy_port = g_strdup("");
	gPreferenceConfigStruct.ssl_proxy = g_strdup("");
	gPreferenceConfigStruct.ssl_proxy_port = g_strdup("");
	gPreferenceConfigStruct.no_proxy_for = g_strdup("localhost");
	gPreferenceConfigStruct.direct_connection = 1;
	gPreferenceConfigStruct.popup_in_new_window = 0;
	gPreferenceConfigStruct.disable_popups = 0;
	gPreferenceConfigStruct.current_font_size = DEFAULT_FONT_SIZE;
	for (i=0; i < LANG_FONT_NUM; i++) {
		gPreferenceConfigStruct.min_font_size [i]= DEFAULT_MIN_FONT_SIZE;
	}
  
	config_file = fopen(file,"r");
	if (config_file == NULL) {
		preferences_write_config ();
	}
  
	config_file = fopen(file,"r");
  
	line = (gchar *)g_malloc(1024);
  
	while(fgets(line,1024,config_file) != NULL) { 
		line[strlen(line)-1] = '\0';
		if (g_strncasecmp(line,"home=",5) == 0) {
			gPreferenceConfigStruct.home = g_strdup(line+5);
			if (g_strcasecmp(gPreferenceConfigStruct.home,"") == 0) {
				g_free(gPreferenceConfigStruct.home);
			gPreferenceConfigStruct.home = g_strdup("about:blank");
			}
		} else if (g_strncasecmp(line,"xsize=",6) == 0) {
			gPreferenceConfigStruct.xsize = atoi(line+6);
		} else if (g_strncasecmp(line,"ysize=",6) == 0) {
			gPreferenceConfigStruct.ysize = atoi(line+6);
		} else if (g_strncasecmp(line,"layout=",7) == 0) {
			gPreferenceConfigStruct.layout = atoi(line+7);
		} else if (g_strncasecmp(line,"mailer=",7) == 0) {
			g_free(gPreferenceConfigStruct.mailer);      gPreferenceConfigStruct.mailer = g_strdup(line+7);
		} else if (g_strncasecmp(line,"maxpopup=",9) == 0) {
			gPreferenceConfigStruct.maxpopupitems = atoi(line+9);
		} else if (g_strncasecmp(line,"http_proxy=",11) == 0) {
			g_free(gPreferenceConfigStruct.http_proxy);
			gPreferenceConfigStruct.http_proxy = g_strdup(line+11);
		} else if (g_strncasecmp(line,"http_proxy_port=",16) == 0) {
			g_free(gPreferenceConfigStruct.http_proxy_port);
			gPreferenceConfigStruct.http_proxy_port = g_strdup(line+16);
		} else if (g_strncasecmp(line,"ftp_proxy=",10) == 0) {
			g_free(gPreferenceConfigStruct.ftp_proxy);
			gPreferenceConfigStruct.ftp_proxy = g_strdup(line+10);
		} else if (g_strncasecmp(line,"ftp_proxy_port=",15) == 0) {
			g_free(gPreferenceConfigStruct.ftp_proxy_port);
			gPreferenceConfigStruct.ftp_proxy_port = g_strdup(line+15);		
		} else if (g_strncasecmp(line,"no_proxy_for=",13) == 0) {
			g_free(gPreferenceConfigStruct.no_proxy_for);
			gPreferenceConfigStruct.no_proxy_for = g_strdup(line+13);
		} else if (g_strncasecmp(line,"ssl_proxy=",10) == 0) {
			g_free(gPreferenceConfigStruct.ssl_proxy);
			gPreferenceConfigStruct.ssl_proxy = g_strdup(line+10);
		} else if (g_strncasecmp(line,"ssl_proxy_port=",15) == 0) {
			g_free(gPreferenceConfigStruct.ssl_proxy_port);
			gPreferenceConfigStruct.ssl_proxy_port = g_strdup(line+15);
		} else if (g_strncasecmp(line,"direct_connection=",18) == 0) {
			gPreferenceConfigStruct.direct_connection = atoi(line+18);
		} else if (g_strncasecmp(line,"max_go=",7) == 0) {
			gPreferenceConfigStruct.max_go = atoi(line+7);
		} else if (g_strncasecmp(line,"popup_in_new_window=",20) == 0) {
			gPreferenceConfigStruct.popup_in_new_window = atoi (line + 20);
		} else if (g_strncasecmp(line,"disable_popups=",15) == 0) {
			gPreferenceConfigStruct.disable_popups = atoi(line+15);
		} else if (g_strncasecmp(line,"fontsize_",9) == 0) {
			for (i = 0; i < LANG_FONT_NUM; i++) {
				g_snprintf (tmpstr, 1024, "%s=", lang_font_item[i]);
				if (g_strncasecmp(line+9,tmpstr,strlen(tmpstr)))
					continue;
        			gPreferenceConfigStruct.font_size[i] = atoi(line+9+strlen(tmpstr));
			};
		} else if (g_strncasecmp(line,"min_fontsize_",13) == 0) {
			for (i=0; i < LANG_FONT_NUM; i++) {
        			g_snprintf(tmpstr,1024,"%s=",lang_font_item[i]);
				if (g_strncasecmp(line+13,tmpstr,strlen(tmpstr)))
					continue;
				gPreferenceConfigStruct.min_font_size[i] = atoi(line+13+strlen(tmpstr));
			}
		} else if (g_strncasecmp(line,"java=",5) == 0) {
			temp = atoi(line+5);
			if (temp) {
				gPreferenceConfigStruct.java = TRUE;
			} else {
				gPreferenceConfigStruct.java = FALSE;
			}
		} else if (g_strncasecmp(line,"javascript=",11) == 0) {
			temp = atoi(line+11);
			if (temp) {
				gPreferenceConfigStruct.javascript = TRUE;
			} else {
				gPreferenceConfigStruct.javascript = FALSE;
			}
		}
	}
	fflush (config_file);
	fclose(config_file);
	g_free(line);
	g_free(file);
}

/* Write the current 'minimo's configs' into his file: $home/.Minimo/config */
void preferences_write_config () 
{
	FILE *config_file;
	gchar *file;
	
	file = g_strconcat(gMinimoUserHomePath,"/.Minimo/config",NULL);
	config_file = fopen(file,"w");
	if (config_file == NULL) {
		g_error ("Cannot write history file!");
		return ;
	}
	
	fprintf(config_file,"home=%s\n",gPreferenceConfigStruct.home);
	fprintf(config_file,"xsize=%d\n",gPreferenceConfigStruct.xsize);
	fprintf(config_file,"ysize=%d\n",gPreferenceConfigStruct.ysize);
	fprintf(config_file,"layout=%d\n",gPreferenceConfigStruct.layout);
	fprintf(config_file,"mailer=%s\n",gPreferenceConfigStruct.mailer);
	fprintf(config_file,"maxpopup=%d\n",gPreferenceConfigStruct.maxpopupitems);
	fprintf(config_file,"http_proxy=%s\n",gPreferenceConfigStruct.http_proxy);
	fprintf(config_file,"http_proxy_port=%s\n",gPreferenceConfigStruct.http_proxy_port);
	fprintf(config_file,"ftp_proxy=%s\n",gPreferenceConfigStruct.ftp_proxy);
	fprintf(config_file,"ftp_proxy_port=%s\n",gPreferenceConfigStruct.ftp_proxy_port);
	fprintf(config_file,"ssl_proxy=%s\n",gPreferenceConfigStruct.ssl_proxy);
	fprintf(config_file,"ssl_proxy_port=%s\n",gPreferenceConfigStruct.ssl_proxy_port);
	fprintf(config_file,"no_proxy_for=%s\n",gPreferenceConfigStruct.no_proxy_for);
	fprintf(config_file,"direct_connection=%d\n",gPreferenceConfigStruct.direct_connection);
	fprintf(config_file,"max_go=%d\n",gPreferenceConfigStruct.max_go);
	fprintf(config_file,"popup_in_new_window=%d\n",gPreferenceConfigStruct.popup_in_new_window);
	fprintf(config_file,"disable_popups=%d\n",gPreferenceConfigStruct.disable_popups);
  
	if (gPreferenceConfigStruct.javascript == TRUE) {      
		fprintf(config_file,"javascript=1\n");
	} else {
		fprintf(config_file,"javascript=0\n");
	} 
	
	if (gPreferenceConfigStruct.java == TRUE) {
		fprintf(config_file,"java=1\n");
	} else {
		fprintf(config_file,"java=0\n");
	}
	
	fclose(config_file);
	g_free(file);
	preferences_set_mozilla_config ();
}

void preferences_set_mozilla_config (void)
{
	gint disk_cache= 0, mem_cache= 0;
	gchar *cachedir;
  
	mozilla_preference_set_boolean ("security.warn_entering_secure", FALSE);
	mozilla_preference_set_boolean ("security.warn_leaving_secure", FALSE);
	mozilla_preference_set_boolean ("security.warn_submit_insecure", FALSE);
	mozilla_preference_set_boolean ("security.warn_viewing_mixed", FALSE);
  
	if (gPreferenceConfigStruct.disable_popups) 
		mozilla_preference_set_boolean ("dom.disable_open_during_load", TRUE);
	else 
		mozilla_preference_set_boolean ("dom.disable_open_during_load", FALSE);
	
	mozilla_preference_set_boolean("nglayout.widget.gfxscrollbars", TRUE);
	
	/*for (i = 0; i < LANG_FONT_NUM; i++) {
		
		g_snprintf(tmpstr, 1024, "font.min-size.variable.", "");
		mozilla_preference_set_int(tmpstr, gPreferenceConfigStruct.current_font_size);
		
		g_snprintf(tmpstr, 1024, "font.min-size.variable.x-western", "");
		mozilla_preference_set_int(tmpstr, 2);
		
		g_snprintf(tmpstr, 1024, "font.min-size.fixed.x-western", "");
		mozilla_preference_set_int(tmpstr, 2);
		
		g_snprintf (tmpstr, 1024, "font.size.variable.%s", lang_font_item[i]);
		mozilla_preference_set_int(tmpstr,gPreferenceConfigStruct.current_font_size);
		
		g_snprintf(tmpstr, 1024, "font.min-size.variable.%s", lang_font_item[i]);
		mozilla_preference_set_int(tmpstr, gPreferenceConfigStruct.current_font_size);
		//mozilla_preference_set_int(tmpstr, DEFAULT_MIN_FONT_SIZE);
	}*/
	mozilla_preference_set_boolean ("security.enable_java", gPreferenceConfigStruct.java);
	mozilla_preference_set_boolean ("javascript.enabled", gPreferenceConfigStruct.javascript);
	mozilla_preference_set_boolean ("browser.view_source.syntax_highlight", TRUE);
	cachedir = g_strconcat(gMinimoUserHomePath,"/.Minimo/cache",NULL);
	mkdir(cachedir,0755);
	mozilla_preference_set("browser.cache.directory",cachedir);
	g_free(cachedir);
	if ((disk_cache > 0) || (mem_cache > 0)) {
		mozilla_preference_set_boolean("browser.cache.disk.enable",TRUE);
		mozilla_preference_set_boolean("browser.cache.enable", TRUE);
	}
	else
	{
		mozilla_preference_set_boolean("browser.cache.disk.enable",FALSE);
		mozilla_preference_set_boolean("browser.cache.enable", FALSE);
	}
	if (disk_cache > 0) {
		mozilla_preference_set_boolean("browser.cache.disk.enable",TRUE);
		mozilla_preference_set_int("browser.cache.disk_cache_size",disk_cache);
		mozilla_preference_set_int("browser.cache.disk.capacity",disk_cache);
	}
	else 
	{
		mozilla_preference_set_boolean("browser.cache.disk.enable",FALSE);
		mozilla_preference_set_int("browser.cache.disk_cache_size", 0);
		mozilla_preference_set_int("browser.cache.disk.capacity",0);
	}
	
	if (mem_cache > 0)  {
		mozilla_preference_set_int("browser.cache.memory_cache_size",mem_cache);
		mozilla_preference_set_int("browser.cache.memory.capacity",mem_cache);
	} else {
		mozilla_preference_set_int("browser.cache.memory_cache_size", 0);   
		mozilla_preference_set_int("browser.cache.memory.capacity",0);
	}
	
	/* set proxy stuff */
	preferences_set_mozilla_proxy_config ();
	
	mozilla_save_prefs ();
}

void preferences_set_mozilla_proxy_config(void)
{
	gint network_type = 0;
  
	if (gPreferenceConfigStruct.direct_connection) {
		mozilla_preference_set("network.proxy.http","");
		mozilla_preference_set("network.proxy.ssl","");
		mozilla_preference_set("network.proxy.ftp","");
		mozilla_preference_set("network.proxy.no_proxies_on", " ");
		mozilla_preference_set_int("network.proxy.type",network_type);
	} else {
    
		if (strlen(gPreferenceConfigStruct.http_proxy) != 0 && strcmp(gPreferenceConfigStruct.http_proxy,"") != 0 && strlen(gPreferenceConfigStruct.http_proxy_port) > 0 && strcmp(gPreferenceConfigStruct.http_proxy_port,"") != 0) {
      
			mozilla_preference_set_int ("network.proxy.type", 1);
			network_type=1;
			mozilla_preference_set ("network.proxy.http", gPreferenceConfigStruct.http_proxy);
			mozilla_preference_set_int ("network.proxy.http_port", atoi(gPreferenceConfigStruct.http_proxy_port));		
		}
    
		if (strlen(gPreferenceConfigStruct.ftp_proxy) != 0 && strcmp(gPreferenceConfigStruct.ftp_proxy,"") != 0 && strlen(gPreferenceConfigStruct.ftp_proxy_port) > 0 && strcmp(gPreferenceConfigStruct.http_proxy_port,"") != 0) {
			if (!network_type)
				mozilla_preference_set_int("network.proxy.type", 1);
			mozilla_preference_set ("network.proxy.ftp", gPreferenceConfigStruct.ftp_proxy);
			mozilla_preference_set_int ("network.proxy.ftp_port", atoi(gPreferenceConfigStruct.ftp_proxy_port));
		}
    
		if (strlen(gPreferenceConfigStruct.ssl_proxy) != 0 && strcmp(gPreferenceConfigStruct.ssl_proxy,"") != 0 && strlen(gPreferenceConfigStruct.ssl_proxy_port) > 0 && strcmp	(gPreferenceConfigStruct.ssl_proxy_port,"") != 0) {
			if (!network_type)
				mozilla_preference_set_int("network.proxy.type", 1);
			mozilla_preference_set ("network.proxy.ssl", gPreferenceConfigStruct.ssl_proxy);
			mozilla_preference_set_int ("network.proxy.ssl_port", atoi(gPreferenceConfigStruct.ssl_proxy_port));
    		}
	
		if (strlen(gPreferenceConfigStruct.no_proxy_for) != 0)
			mozilla_preference_set("network.proxy.no_proxies_on", gPreferenceConfigStruct.no_proxy_for);
		else
			mozilla_preference_set("network.proxy.no_proxies_on", " ");
	}
}

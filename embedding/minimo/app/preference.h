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

#include "mozilla_api.h"

typedef struct _PrefWindow 		/* configuration data struct */
{
  GtkWidget *dialog;
  GtkWidget *okbutton;
  GtkWidget *cancelbutton;
  GtkWidget *applybutton;
  GtkWidget *block_popup;
  GtkWidget *vbox_popup;
  GtkWidget *fr_connection;
  GtkWidget *vbox_connection;
  GtkWidget *hbox_connection;
  GtkWidget *hbox_direct_connection;
  GtkWidget *fr_popup;
  GtkWidget *lb_popup;
  
  GtkWidget *vbox_manual;
  GtkWidget *rb_manual_connection;
  GtkWidget *hbox_http;
  GtkWidget *lb_http;
  GtkWidget *en_http_proxy;
  GtkWidget *box_popup;
  
  GtkWidget *lb_port_http;
  GtkWidget *en_http_port;
  GtkWidget *hbox_ssl;
  GtkWidget *lb_ssl;
  GtkWidget *en_ssl;
  GtkWidget *lb_ssl_port;
  GtkWidget *en_ssl_port;
  GtkWidget *hbox_ftp;
  
  GtkWidget *lb_ftp;
  GtkWidget *en_ftp_proxy;
  GtkWidget *lb_ftp_port;
  GtkWidget *en_ftp_port;
  GtkWidget *hbox_noproxy;
  GtkWidget *lb_connection;
  
  GSList *rb_direct_connection_group;
  GSList *rb_manual_connection_group;
  GtkWidget *hbox_manual_connection;
  GtkWidget *rb_direct_connection;
  
} PrefWindow;

ConfigData config;
PrefWindow *prefWindow;

/* Callbacks */
void close_preferences_cb (GtkButton *button, PrefWindow *pref);
void ok_preferences_cb (GtkButton *button, PrefWindow *pref);

/* Support methods */
void read_minimo_config(void);
void write_minimo_config(void);
void set_mozilla_prefs(void);
void minimo_set_proxy_prefs(void);

/* UI Methods */
PrefWindow * build_pref_window (GtkWidget *topLevelWindow);

PrefWindow * build_pref_window (GtkWidget *topLevelWindow) {
  
  prefWindow = g_new0(PrefWindow,1);
  
  prefWindow->dialog = gtk_dialog_new ();
  
  gtk_window_set_title (GTK_WINDOW (prefWindow->dialog), ("Prefs Window"));
  gtk_window_set_position (GTK_WINDOW (prefWindow->dialog), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal (GTK_WINDOW (prefWindow->dialog), TRUE);
  gtk_window_set_resizable (GTK_WINDOW (prefWindow->dialog), FALSE);
  gtk_window_set_decorated (GTK_WINDOW (prefWindow->dialog), TRUE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (prefWindow->dialog), TRUE);
  gtk_window_set_skip_pager_hint (GTK_WINDOW (prefWindow->dialog), TRUE);
  gtk_window_set_type_hint (GTK_WINDOW (prefWindow->dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_gravity (GTK_WINDOW (prefWindow->dialog), GDK_GRAVITY_NORTH_EAST);
  gtk_window_set_transient_for(GTK_WINDOW(prefWindow->dialog), GTK_WINDOW(topLevelWindow));
  gtk_widget_set_size_request (prefWindow->dialog, 240, 220);
  
  /* Creating "Connection Frame" */
  prefWindow->fr_connection = gtk_frame_new (NULL);
  gtk_widget_show (prefWindow->fr_connection);
  gtk_container_add (GTK_CONTAINER(GTK_DIALOG (prefWindow->dialog)->vbox), prefWindow->fr_connection);
  
  /* Connection Frame's VBOX */
  prefWindow->vbox_connection = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (prefWindow->vbox_connection);
  gtk_container_add (GTK_CONTAINER (prefWindow->fr_connection), prefWindow->vbox_connection);
  
  prefWindow->hbox_direct_connection = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (prefWindow->hbox_direct_connection);
  gtk_box_pack_start (GTK_BOX (prefWindow->vbox_connection), prefWindow->hbox_direct_connection, TRUE, TRUE, 0);
  
  /* Radio Buttons - Direct/Manual Connetion */
  prefWindow->rb_direct_connection = gtk_radio_button_new_with_mnemonic (NULL, "Direct connection to the Internet");
  gtk_widget_show (prefWindow->rb_direct_connection);
  gtk_box_pack_start (GTK_BOX (prefWindow->hbox_direct_connection), prefWindow->rb_direct_connection, FALSE, FALSE, 0);
  prefWindow->rb_direct_connection_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefWindow->rb_direct_connection));
  
  prefWindow->hbox_manual_connection = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (prefWindow->hbox_manual_connection);
  gtk_box_pack_start (GTK_BOX (prefWindow->vbox_connection), prefWindow->hbox_manual_connection, TRUE, TRUE, 0);
  
  prefWindow->vbox_manual = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (prefWindow->vbox_manual);
  gtk_box_pack_start (GTK_BOX (prefWindow->hbox_manual_connection), prefWindow->vbox_manual, TRUE, TRUE, 0);
  
  prefWindow->rb_manual_connection = gtk_radio_button_new_with_mnemonic (NULL, "Manual proxy configuration");
  gtk_widget_show (prefWindow->rb_manual_connection);
  gtk_box_pack_start (GTK_BOX (prefWindow->vbox_manual), prefWindow->rb_manual_connection, FALSE, FALSE, 0);
  
  prefWindow->rb_manual_connection_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (prefWindow->rb_manual_connection));
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (prefWindow->rb_manual_connection), prefWindow->rb_direct_connection_group);
  
  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (prefWindow->rb_direct_connection));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefWindow->rb_direct_connection), config.direct_connection);
  
  /* HTTP Widget HBOX */
  prefWindow->hbox_http = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (prefWindow->hbox_http);
  gtk_box_pack_start (GTK_BOX (prefWindow->vbox_manual), prefWindow->hbox_http, TRUE, TRUE, 0);
  
  prefWindow->lb_http = gtk_label_new ("HTTP Proxy:  ");
  gtk_widget_show (prefWindow->lb_http);
  gtk_box_pack_start (GTK_BOX (prefWindow->hbox_http), prefWindow->lb_http, FALSE, FALSE, 0);
  
  prefWindow->en_http_proxy = gtk_entry_new ();
  gtk_widget_show (prefWindow->en_http_proxy);
  gtk_box_pack_start (GTK_BOX (prefWindow->hbox_http), prefWindow->en_http_proxy, TRUE, TRUE, 0);
  gtk_widget_modify_font(prefWindow->en_http_proxy, getOrCreateDefaultMinimoFont());
  gtk_widget_set_size_request (prefWindow->en_http_proxy, 60, -1);
  
  prefWindow->lb_port_http = gtk_label_new (" Port: ");
  gtk_widget_show (prefWindow->lb_port_http);
  gtk_box_pack_start (GTK_BOX (prefWindow->hbox_http), prefWindow->lb_port_http, FALSE, FALSE, 0);
  
  prefWindow->en_http_port = gtk_entry_new ();
  gtk_widget_show (prefWindow->en_http_port);
  gtk_box_pack_start (GTK_BOX (prefWindow->hbox_http), prefWindow->en_http_port, TRUE, TRUE, 0);
  gtk_widget_set_size_request (prefWindow->en_http_port, 30, -1);
  
  /* SSL Widget HBOX */
  prefWindow->hbox_ssl = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (prefWindow->hbox_ssl);
  gtk_box_pack_start (GTK_BOX (prefWindow->vbox_manual), prefWindow->hbox_ssl, TRUE, TRUE, 0);
  
  prefWindow->lb_ssl = gtk_label_new ("SSL Proxy:    ");
  gtk_widget_show (prefWindow->lb_ssl);
  gtk_box_pack_start (GTK_BOX (prefWindow->hbox_ssl), prefWindow->lb_ssl, FALSE, FALSE, 0);
  
  prefWindow->en_ssl = gtk_entry_new ();
  gtk_widget_show (prefWindow->en_ssl);
  gtk_box_pack_start (GTK_BOX (prefWindow->hbox_ssl), prefWindow->en_ssl, TRUE, TRUE, 0);
  gtk_widget_set_size_request (prefWindow->en_ssl, 60, -1);
  
  prefWindow->lb_ssl_port = gtk_label_new (" Port: ");
  gtk_widget_show (prefWindow->lb_ssl_port);
  gtk_box_pack_start (GTK_BOX (prefWindow->hbox_ssl), prefWindow->lb_ssl_port, FALSE, FALSE, 0);
  
  prefWindow->en_ssl_port = gtk_entry_new ();
  gtk_widget_show (prefWindow->en_ssl_port);
  gtk_box_pack_start (GTK_BOX (prefWindow->hbox_ssl), prefWindow->en_ssl_port, TRUE, TRUE, 0);
  gtk_widget_modify_font(prefWindow->en_ssl, getOrCreateDefaultMinimoFont());
  gtk_widget_set_size_request (prefWindow->en_ssl_port, 30, -1);
  
  /* FTP Widgets HBOX */
  prefWindow->hbox_ftp = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (prefWindow->hbox_ftp);
  gtk_box_pack_start (GTK_BOX (prefWindow->vbox_manual), prefWindow->hbox_ftp, TRUE, TRUE, 0);
  
  prefWindow->lb_ftp = gtk_label_new ("FTP Proxy:    ");
  gtk_widget_show (prefWindow->lb_ftp);
  gtk_box_pack_start (GTK_BOX (prefWindow->hbox_ftp), prefWindow->lb_ftp, FALSE, FALSE, 0);
  
  prefWindow->en_ftp_proxy = gtk_entry_new ();
  gtk_widget_show (prefWindow->en_ftp_proxy);
  gtk_box_pack_start (GTK_BOX (prefWindow->hbox_ftp), prefWindow->en_ftp_proxy, TRUE, TRUE, 0);
  gtk_widget_set_size_request (prefWindow->en_ftp_proxy, 60, -1);
  
  prefWindow->lb_ftp_port = gtk_label_new (" Port: ");
  gtk_widget_show (prefWindow->lb_ftp_port);
  gtk_box_pack_start (GTK_BOX (prefWindow->hbox_ftp), prefWindow->lb_ftp_port, FALSE, FALSE, 0);
  
  prefWindow->en_ftp_port = gtk_entry_new ();
  gtk_widget_show (prefWindow->en_ftp_port);
  gtk_box_pack_start (GTK_BOX (prefWindow->hbox_ftp), prefWindow->en_ftp_port, TRUE, TRUE, 0);
  gtk_widget_modify_font(prefWindow->en_ftp_proxy, getOrCreateDefaultMinimoFont());
  gtk_widget_set_size_request (prefWindow->en_ftp_port, 30, -1);
  
  prefWindow->hbox_noproxy = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (prefWindow->hbox_noproxy);
  gtk_box_pack_start (GTK_BOX (prefWindow->vbox_connection), prefWindow->hbox_noproxy, TRUE, TRUE, 0);
  
  if (config.direct_connection == 0) {		
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefWindow->rb_direct_connection), FALSE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefWindow->rb_manual_connection), TRUE);
    
  } else if (config.direct_connection == 1) {
    
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefWindow->rb_direct_connection), TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefWindow->rb_manual_connection), FALSE);
  }
  
  gtk_entry_set_text (GTK_ENTRY (prefWindow->en_http_proxy), config.http_proxy);
  gtk_entry_set_text (GTK_ENTRY (prefWindow->en_http_port), config.http_proxy_port);
  gtk_entry_set_text (GTK_ENTRY (prefWindow->en_ssl), config.ssl_proxy);
  gtk_entry_set_text (GTK_ENTRY (prefWindow->en_ssl_port), config.ssl_proxy_port);
  gtk_entry_set_text (GTK_ENTRY (prefWindow->en_ftp_proxy), config.ftp_proxy);
  gtk_entry_set_text (GTK_ENTRY (prefWindow->en_ftp_port), config.ftp_proxy_port);
  
  /* Creating "Popup Frame" */
  prefWindow->fr_popup = gtk_frame_new (NULL);
  gtk_widget_show (prefWindow->fr_popup);
  gtk_container_add (GTK_CONTAINER(GTK_DIALOG (prefWindow->dialog)->vbox), prefWindow->fr_popup);
  
  /* Proxy Frame's label */
  prefWindow->lb_connection = gtk_label_new ("Configure Proxies:");
  gtk_widget_show (prefWindow->lb_connection);
  gtk_frame_set_label_widget (GTK_FRAME (prefWindow->fr_connection), prefWindow->lb_connection);
  
  prefWindow->vbox_popup = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (prefWindow->vbox_popup);
  gtk_container_add (GTK_CONTAINER (prefWindow->fr_popup), prefWindow->vbox_popup);
  
  prefWindow->block_popup = gtk_check_button_new_with_label ("Block Popup Windows");
  if (config.disable_popups == 1) {
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(prefWindow->block_popup), TRUE);
  } else gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(prefWindow->block_popup), FALSE);
  
  gtk_container_add(GTK_CONTAINER(prefWindow->vbox_popup), prefWindow->block_popup);
  
  prefWindow->lb_popup = gtk_label_new ("Block Popups");
  gtk_widget_show (prefWindow->lb_popup);
  gtk_frame_set_label_widget (GTK_FRAME (prefWindow->fr_popup), prefWindow->lb_popup);
  
  /* Ok and Cancel Button */
  prefWindow->cancelbutton = gtk_button_new_with_label("Cancel");
  gtk_widget_modify_font(GTK_BIN(prefWindow->cancelbutton)->child, getOrCreateDefaultMinimoFont());
  gtk_widget_show (prefWindow->cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (prefWindow->dialog), prefWindow->cancelbutton, GTK_RESPONSE_CANCEL);
  gtk_widget_set_size_request (prefWindow->cancelbutton, 10, -1);
  
  prefWindow->okbutton = gtk_button_new_with_label("Ok");
  gtk_widget_modify_font(GTK_BIN(prefWindow->okbutton)->child, getOrCreateDefaultMinimoFont());
  gtk_widget_show (prefWindow->okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (prefWindow->dialog), prefWindow->okbutton, GTK_RESPONSE_OK);
  gtk_widget_set_size_request (prefWindow->okbutton, 10, -1);
  
  /* Setting font styles */
  gtk_widget_modify_font(prefWindow->rb_manual_connection, getOrCreateDefaultMinimoFont());
  gtk_widget_modify_font(prefWindow->rb_direct_connection, getOrCreateDefaultMinimoFont());
  gtk_widget_modify_font(prefWindow->lb_http, getOrCreateDefaultMinimoFont());  
  gtk_widget_modify_font(prefWindow->lb_port_http, getOrCreateDefaultMinimoFont());
  gtk_widget_modify_font(prefWindow->en_http_port, getOrCreateDefaultMinimoFont());
  gtk_widget_modify_font(prefWindow->lb_ssl, getOrCreateDefaultMinimoFont());  
  gtk_widget_modify_font(prefWindow->en_ssl, getOrCreateDefaultMinimoFont());
  gtk_widget_modify_font(prefWindow->en_ssl_port, getOrCreateDefaultMinimoFont());
  gtk_widget_modify_font(prefWindow->lb_ssl_port, getOrCreateDefaultMinimoFont());
  gtk_widget_modify_font(prefWindow->lb_ftp, getOrCreateDefaultMinimoFont());
  gtk_widget_modify_font(prefWindow->en_ftp_proxy, getOrCreateDefaultMinimoFont());
  gtk_widget_modify_font(prefWindow->lb_ftp_port, getOrCreateDefaultMinimoFont());
  gtk_widget_modify_font(prefWindow->en_ftp_port, getOrCreateDefaultMinimoFont());
  gtk_widget_modify_font(prefWindow->lb_connection, getOrCreateDefaultMinimoFont());
  gtk_widget_modify_font(prefWindow->lb_popup, getOrCreateDefaultMinimoFont());
  gtk_widget_modify_font(prefWindow->block_popup, getOrCreateDefaultMinimoFont());
  
  gtk_widget_show_all(prefWindow->dialog);
  
  /* connection singnals to buttons */
  gtk_signal_connect_object(GTK_OBJECT(prefWindow->cancelbutton), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), prefWindow->dialog);
  gtk_signal_connect_object(GTK_OBJECT(prefWindow->okbutton), "clicked", GTK_SIGNAL_FUNC(ok_preferences_cb), prefWindow);
  
  return prefWindow;
}

void save_preferences_cb (GtkButton *button, PrefWindow *pref) {
  
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (prefWindow->rb_direct_connection)) == TRUE) 
    config.direct_connection = 1;
  else config.direct_connection = 0;
  
  config.http_proxy = g_strdup (gtk_entry_get_text (GTK_ENTRY (prefWindow->en_http_proxy)));
  config.http_proxy_port = g_strdup (gtk_entry_get_text (GTK_ENTRY (prefWindow->en_http_port)));
  config.ftp_proxy = g_strdup (gtk_entry_get_text (GTK_ENTRY (prefWindow->en_ftp_proxy)));
  config.ftp_proxy_port = g_strdup (gtk_entry_get_text (GTK_ENTRY (prefWindow->en_ftp_port)));
  config.ssl_proxy = g_strdup (gtk_entry_get_text (GTK_ENTRY (prefWindow->en_ssl)));
  config.ssl_proxy_port = g_strdup (gtk_entry_get_text (GTK_ENTRY (prefWindow->en_ssl_port)));
  config.disable_popups = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (prefWindow->block_popup));
  
  write_minimo_config();
}

void ok_preferences_cb (GtkButton *button, PrefWindow *pref) {
  
  save_preferences_cb (button, pref);
  close_preferences_cb (button, pref);
}

void close_preferences_cb (GtkButton *button, PrefWindow *pref) {
  gtk_widget_destroy (prefWindow->dialog);
  g_free (pref);
}

/* Read the current 'minimo config' into his file: $home/.Minimo/config */
void read_minimo_config(void) 
{
  FILE *config_file;
  gchar *file, *oldfile;
  gchar *line, tmpstr[1024];
  
  gint i, temp;
  
  user_home = g_strconcat(g_get_home_dir(),NULL);
  
  oldfile = g_strconcat(user_home,"/.minimorc", NULL);
  file = g_strconcat(user_home,"/.Minimo/config", NULL);
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
  config.home = g_strdup("http://www.minimo.org");
  config.xsize = 700;
  config.ysize = 500;
  config.layout = 0;
  config.mailer = g_strdup("pronto %t %s %c");
  config.max_go = 15;
  config.maxpopupitems = 15;
  config.java = TRUE;
  config.javascript = TRUE;
  config.http_proxy = g_strdup("");
  config.ftp_proxy = g_strdup("");
  config.ftp_proxy_port = g_strdup("");
  config.http_proxy_port = g_strdup("");
  config.ssl_proxy = g_strdup("");
  config.ssl_proxy_port = g_strdup("");
  config.no_proxy_for = g_strdup("localhost");
  config.direct_connection = 1;
  config.popup_in_new_window = 0;
  config.disable_popups = 0;
  config.current_font_size = DEFAULT_FONT_SIZE;
  
  config_file = fopen(file,"r");
  if (config_file == NULL) {
    write_minimo_config();
  }
  
  config_file = fopen(file,"r");
  
  line = (gchar *)g_malloc(1024);
  
  while(fgets(line,1024,config_file) != NULL) { 
    line[strlen(line)-1] = '\0';
    if (g_strncasecmp(line,"home=",5) == 0) {
      config.home = g_strdup(line+5);
      if (g_strcasecmp(config.home,"") == 0) {
        g_free(config.home);
        config.home = g_strdup("about:blank");
      }
    } else if (g_strncasecmp(line,"xsize=",6) == 0) {
      config.xsize = atoi(line+6);
    } else if (g_strncasecmp(line,"ysize=",6) == 0) {
      config.ysize = atoi(line+6);
    } else if (g_strncasecmp(line,"layout=",7) == 0) {
      config.layout = atoi(line+7);
    } else if (g_strncasecmp(line,"mailer=",7) == 0) {
      g_free(config.mailer);
      config.mailer = g_strdup(line+7);
    } else if (g_strncasecmp(line,"maxpopup=",9) == 0) {
      config.maxpopupitems = atoi(line+9);
    } else if (g_strncasecmp(line,"http_proxy=",11) == 0) {
      g_free(config.http_proxy);
      config.http_proxy = g_strdup(line+11);
    } else if (g_strncasecmp(line,"http_proxy_port=",16) == 0) {
      g_free(config.http_proxy_port);
      config.http_proxy_port = g_strdup(line+16);
    } else if (g_strncasecmp(line,"ftp_proxy=",10) == 0) {
      g_free(config.ftp_proxy);
      config.ftp_proxy = g_strdup(line+10);
    } else if (g_strncasecmp(line,"ftp_proxy_port=",15) == 0) {
      g_free(config.ftp_proxy_port);
      config.ftp_proxy_port = g_strdup(line+15);		
    } else if (g_strncasecmp(line,"no_proxy_for=",13) == 0) {
      g_free(config.no_proxy_for);
      config.no_proxy_for = g_strdup(line+13);
    } else if (g_strncasecmp(line,"ssl_proxy=",10) == 0) {
      g_free(config.ssl_proxy);
      config.ssl_proxy = g_strdup(line+10);
    } else if (g_strncasecmp(line,"ssl_proxy_port=",15) == 0) {
      g_free(config.ssl_proxy_port);
      config.ssl_proxy_port = g_strdup(line+15);
    } else if (g_strncasecmp(line,"no_proxy_for=",13) == 0) {
      g_free(config.no_proxy_for);
      config.no_proxy_for = g_strdup(line+13);
    } else if (g_strncasecmp(line,"direct_connection=",18) == 0) {
      config.direct_connection = atoi(line+18);
    } else if (g_strncasecmp(line,"max_go=",7) == 0) {
      config.max_go = atoi(line+7);
    } else if (g_strncasecmp(line,"popup_in_new_window=",20) == 0) {
      config.popup_in_new_window = atoi (line + 20);
    } else if (g_strncasecmp(line,"disable_popups=",15) == 0) {
      config.disable_popups = atoi(line+15);
    } else if (g_strncasecmp(line,"fontsize_",9) == 0) {
      for (i = 0; i < LANG_FONT_NUM; i++) {
        g_snprintf (tmpstr, 1024, "%s=", lang_font_item[i]);
        if (g_strncasecmp(line+9,tmpstr,strlen(tmpstr)))
          continue;
        config.font_size[i] = atoi(line+9+strlen(tmpstr));
      };
    } else if (g_strncasecmp(line,"min_fontsize_",13) == 0) {
      for (i=0; i < LANG_FONT_NUM; i++) {
        g_snprintf(tmpstr,1024,"%s=",lang_font_item[i]);
        if (g_strncasecmp(line+13,tmpstr,strlen(tmpstr)))
          continue;
        config.min_font_size[i] = atoi(line+13+strlen(tmpstr));
      }
    } else if (g_strncasecmp(line,"java=",5) == 0) {
      temp = atoi(line+5);
      if (temp) {
        config.java = TRUE;
      } else {
        config.java = FALSE;
      }
    } else if (g_strncasecmp(line,"javascript=",11) == 0) {
      temp = atoi(line+11);
      if (temp) {
        config.javascript = TRUE;
      } else {
        config.javascript = FALSE;
      }
    }
  }
  fflush (config_file);
  fclose(config_file);
  g_free(line);
  g_free(file);
}

/* Write the current 'minimo config' into his file: $home/.Minimo/config */
void write_minimo_config() 
{
  FILE *config_file;
  gchar *file;
  
  file = g_strconcat(user_home,"/.Minimo/config",NULL);
  config_file = fopen(file,"w");
  if (config_file == NULL) {
    g_error ("Cannot write config file!");
    return ;
  }
  
  fprintf(config_file,"home=%s\n",config.home);
  fprintf(config_file,"xsize=%d\n",config.xsize);
  fprintf(config_file,"ysize=%d\n",config.ysize);
  fprintf(config_file,"layout=%d\n",config.layout);
  fprintf(config_file,"mailer=%s\n",config.mailer);
  fprintf(config_file,"maxpopup=%d\n",config.maxpopupitems);
  fprintf(config_file,"http_proxy=%s\n",config.http_proxy);
  fprintf(config_file,"http_proxy_port=%s\n",config.http_proxy_port);
  fprintf(config_file,"ftp_proxy=%s\n",config.ftp_proxy);
  fprintf(config_file,"ftp_proxy_port=%s\n",config.ftp_proxy_port);
  fprintf(config_file,"ssl_proxy=%s\n",config.ssl_proxy);
  fprintf(config_file,"ssl_proxy_port=%s\n",config.ssl_proxy_port);
  fprintf(config_file,"no_proxy_for=%s\n",config.no_proxy_for);
  fprintf(config_file,"direct_connection=%d\n",config.direct_connection);
  fprintf(config_file,"max_go=%d\n",config.max_go);
  fprintf(config_file,"popup_in_new_window=%d\n",config.popup_in_new_window);
  fprintf(config_file,"disable_popups=%d\n",config.disable_popups);
  
  if (config.javascript == TRUE) {      
    fprintf(config_file,"javascript=1\n");
  } else {
    fprintf(config_file,"javascript=0\n");
  } 
  
  if (config.java == TRUE) {
    fprintf(config_file,"java=1\n");
  } else {
    fprintf(config_file,"java=0\n");
  }
  
  fclose(config_file);
  g_free(file);
  set_mozilla_prefs ();
}

void set_mozilla_prefs(void)
{
  gint i, disk_cache= 0, mem_cache= 0;
  gchar *cachedir, tmpstr[1024];
  
  mozilla_preference_set_boolean ("security.warn_entering_secure", FALSE);
  mozilla_preference_set_boolean ("security.warn_leaving_secure", FALSE);
  mozilla_preference_set_boolean ("security.warn_submit_insecure", FALSE);
  mozilla_preference_set_boolean ("security.warn_viewing_mixed", FALSE);
  
  if (config.disable_popups)
    mozilla_preference_set_boolean ("dom.disable_open_during_load", TRUE);
  else 
    mozilla_preference_set_boolean ("dom.disable_open_during_load", FALSE);
  
  mozilla_preference_set_boolean("nglayout.widget.gfxscrollbars", TRUE);
  
  for (i = 0; i < LANG_FONT_NUM; i++) {
    g_snprintf (tmpstr, 1024, "font.size.variable.%s", lang_font_item[i]);
    mozilla_preference_set_int(tmpstr,DEFAULT_FONT_SIZE);
    
    g_snprintf(tmpstr, 1024, "font.min-size.variable.%s", lang_font_item[i]);
    mozilla_preference_set_int(tmpstr, DEFAULT_MIN_FONT_SIZE);
  }
  
  mozilla_preference_set_boolean ("security.enable_java", config.java);
  mozilla_preference_set_boolean ("javascript.enabled", config.javascript);
  mozilla_preference_set_boolean ("browser.view_source.syntax_highlight", TRUE);
  cachedir = g_strconcat(user_home,"/.Minimo/cache",NULL);
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
  minimo_set_proxy_prefs();
  
  mozilla_save_prefs ();
}

void minimo_set_proxy_prefs(void)
{
  gint network_type = 0;
  
  if (config.direct_connection) {
    mozilla_preference_set("network.proxy.http","");
    mozilla_preference_set("network.proxy.ssl","");
    mozilla_preference_set("network.proxy.ftp","");
    mozilla_preference_set("network.proxy.no_proxies_on", " ");
    mozilla_preference_set_int("network.proxy.type",network_type);
  } else {
    
    if (strlen(config.http_proxy) != 0 && strcmp(config.http_proxy,"") != 0 && strlen(config.http_proxy_port) > 0 && strcmp(config.http_proxy_port,"") != 0) {
      
      mozilla_preference_set_int ("network.proxy.type", 1);
      network_type=1;
      mozilla_preference_set ("network.proxy.http", config.http_proxy);
      mozilla_preference_set_int ("network.proxy.http_port", atoi(config.http_proxy_port));		
    }
    
    if (strlen(config.ftp_proxy) != 0 && strcmp(config.ftp_proxy,"") != 0 && strlen(config.ftp_proxy_port) > 0 && strcmp(config.http_proxy_port,"") != 0) {
      if (!network_type)
        mozilla_preference_set_int("network.proxy.type", 1);
      mozilla_preference_set ("network.proxy.ftp", config.ftp_proxy);
      mozilla_preference_set_int ("network.proxy.ftp_port", atoi(config.ftp_proxy_port));
    }
    
    if (strlen(config.ssl_proxy) != 0 && strcmp(config.ssl_proxy,"") != 0 && strlen(config.ssl_proxy_port) > 0 && strcmp	(config.ssl_proxy_port,"") != 0) {
      if (!network_type)
        mozilla_preference_set_int("network.proxy.type", 1);
      mozilla_preference_set ("network.proxy.ssl", config.ssl_proxy);
      mozilla_preference_set_int ("network.proxy.ssl_port", atoi(config.ssl_proxy_port));
    }
	
    if (strlen(config.no_proxy_for) != 0)
      mozilla_preference_set("network.proxy.no_proxies_on", config.no_proxy_for);
    else
      mozilla_preference_set("network.proxy.no_proxies_on", " ");
  }
}

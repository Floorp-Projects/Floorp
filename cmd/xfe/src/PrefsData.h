/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* */
/*
   PrefsData.h -- definitions for preferences data
   Created: Linda Wei <lwei@netscape.com>, 18-Sep-96.
 */


#ifndef _xfe_prefsdata_h
#define _xfe_prefsdata_h

#include "Outliner.h"

// ---------- Appearance ----------

struct PrefsDataGeneralAppearance
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	// Launch

	Widget     navigator_toggle;
	Widget     messenger_toggle;
	Widget     collabra_toggle;
	Widget     composer_toggle;
	Widget     conference_toggle;
	Widget     netcaster_toggle;
	Widget     calendar_toggle;

	// Show Toolbar as

	Widget     pic_and_text_toggle;
	Widget     pic_only_toggle;
	Widget     text_only_toggle;
	Widget     show_tooltips_toggle;
};

struct PrefsDataGeneralFonts
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	// Font

	Widget     encoding_label;
	Widget     proportional_label;
	Widget     fixed_label;
	Widget     encoding_menu;
	Widget     prop_size_label;
	Widget     fixed_size_label;
	Widget     use_font_label;
	Widget     prop_family_option;
	Widget     fixed_family_option;
	Widget     prop_size_option;
	Widget     fixed_size_option;
	Widget     prop_size_field;
	Widget     fixed_size_field;
	Widget     prop_size_toggle;
	Widget     fixed_size_toggle;
	Widget     use_my_font_toggle;
	Widget     use_doc_font_selective_toggle;
	Widget     use_doc_font_whenever_toggle;
	int        fonts_changed;
	int        *encoding_menu_csid;
	int        selected_encoding;
};

struct PrefsDataGeneralColors
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	// Colors

	Widget     text_color_button;
	Widget     bg_color_button;
	Widget     links_color_button;
	Widget     vlinks_color_button;
	Widget     use_default_colors_button;
	Widget     underline_links_toggle;
	Widget     use_my_color_toggle;
};

// ---------- Advanced ----------

struct PrefsDataGeneralAdvanced
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	Widget     show_image_toggle;
#ifdef JAVA
	Widget     enable_java_toggle;
#endif
	Widget     enable_js_toggle;
	Widget     enable_style_sheet_toggle;
	Widget     auto_install_toggle;
	Widget     email_anonftp_toggle;

	Widget     always_accept_cookie_toggle;
	Widget     no_foreign_cookie_toggle;
	Widget     never_accept_cookie_toggle;
	Widget     warn_cookie_toggle;
};

struct PrefsDataGeneralCache
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	Widget     cache_dir_text;
	Widget     browse_button;
	Widget     mem_text;
	Widget     disk_text;
	Widget     once_toggle;
	Widget     every_toggle;
	Widget     never_toggle;
};

struct PrefsDataGeneralProxies
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	Widget     direct_toggle;
	Widget     manual_toggle;
	Widget     auto_toggle;
	Widget     config_url_text;
	Widget     reload_button;
	Widget     view_button;
};

struct PrefsDataDiskSpace
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	Widget     max_msg_size_toggle;
	Widget     max_msg_size_text;
	Widget     ask_threshold_toggle;
	Widget     threshold_text;
	Widget     keep_all_news_toggle;
	Widget     keep_news_by_age_toggle;
	Widget     keep_news_by_count_toggle;
	Widget     keep_news_days_text;
	Widget     keep_news_count_text;
	Widget     keep_unread_news_toggle;
};

#ifdef PREFS_UNSUPPORTED
struct PrefsDataHelpFiles
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	Widget     netscape_toggle;
	Widget     install_toggle;
	Widget     custom_toggle;
	Widget     custom_url_text;
	Widget     browse_button;
};
#endif /* PREFS_UNSUPPORTED */

// ---------- Browser ----------

struct PrefsDataBrowser
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	Widget     blank_page_toggle;
	Widget     home_page_toggle;
	Widget     last_page_toggle;

	Widget     browse_button;
	Widget     use_current_button;

	Widget     home_page_text;
	Widget     expire_days_text;
};

struct PrefsDataGeneralAppl
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	Widget     helpers_list;
	Widget     folder_text;
	Widget     browse_button;
	
	int        helpers_changed;
	int        static_apps_count;
};

struct PrefsDataBrowserLang
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	XFE_Outliner  *lang_outliner;
	Widget         lang_list;
	Widget         up_button;
	Widget         down_button;
	Widget         add_button;
	Widget         delete_button;
	char         **pref_lang_regs;
	int            pref_lang_count;
};

// ---------- Mail News ----------

struct PrefsDataMailNews
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	Widget     quoted_text_size_pulldown;
	Widget     quoted_text_size_option;
	Widget     quoted_text_style_pulldown;
	Widget     quoted_text_style_option;
	Widget     quoted_text_color_button;
	Widget     fixed_width_font_toggle;
	Widget     var_width_font_toggle;

	Widget     reuse_thread_window_toggle;
	Widget     reuse_message_window_toggle;
	Widget     message_in_thread_toggle;
};

struct PrefsDataMailNewsIdentity
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	Widget     name_text;
	Widget     email_addr_text;
	Widget     reply_to_addr_text;
	Widget     org_text;
	Widget     sig_file_text;
	Widget     browse_button;
	Widget     attach_card_toggle;
#if 0
	Widget     edit_card_button;
#endif
};

struct PrefsDataMailNewsComposition
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	Widget     mail_email_self_toggle;
	Widget     mail_email_other_text;
	Widget     news_email_self_toggle;
	Widget     news_email_other_text;
	Widget     mail_fcc_toggle;
	Widget     mail_fcc_combo;
	Widget     news_fcc_toggle;
	Widget     news_fcc_combo;
	Widget     auto_quote_toggle;
	Widget     send_html_msg_toggle;
	Widget     wrap_length_text;

	XP_List   *mail_fcc_folders;
	XP_List   *news_fcc_folders;
	XP_List   *mail_fcc_paths;
	XP_List   *news_fcc_paths;
};

struct PrefsDataMailNewsMserver
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	Widget     pop_user_name_text;
	Widget     outgoing_mail_server_text;
	Widget     incoming_mail_server_text;
	Widget     pop3_toggle;
	Widget     movemail_toggle;
	Widget     imap_toggle;
	//Widget     inbox_toggle;
	Widget     leave_msg_on_server_toggle;
	//Widget     imap_local_copies_toggle;
	Widget     delete_is_move_to_trash_toggle;
	Widget     imap_server_ssl_toggle;
	Widget     built_in_app_toggle;
	Widget     external_app_toggle;
	Widget     external_app_text;
	Widget     external_app_browse_button;
};

struct PrefsDataMailNewsNserver
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	Widget     nntp_server_text;
	Widget     news_dir_text;
	Widget     notify_toggle;
	Widget     browse_button;
	Widget     msg_size_text;
	Widget     port_text;
	Widget     secure_toggle;
};

struct PrefsDataMailNewsAddrBook
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	XFE_Outliner  *dir_outliner;
	Widget         dir_list;
	Widget         up_button;
	Widget         down_button;
	Widget         add_button;
	Widget         delete_button;
	Widget         edit_button;
	Widget         first_last_toggle;
	Widget         last_first_toggle;

	XP_List       *directories;
	int            num_directories;
	XP_List       *deleted_directories;
};

struct PrefsDataApplEdit
{
	MWContext *context;

	Widget     static_desc_label;
	Widget     mime_types_desc_text;
	Widget     mime_types_text;
	Widget     mime_types_suffix_text;
 
	Widget     navigator_toggle;
	Widget     plugin_toggle;
	Widget     plugin_combo;
	Widget     app_toggle;
	Widget     app_text;
	Widget     app_browse;
	Widget     save_toggle;
	Widget     unknown_toggle;

	// Data 

	Boolean    static_app;
	int        pos;
	NET_cdataStruct  *cd;
	char     **plugins;
};

// ---------- Editor ----------

struct PrefsDataEditor
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

    Widget     author;
    Widget     html_editor;
    Widget     html_browse;
    Widget     image_editor;
    Widget     image_browse;
    Widget     tmplate;
    Widget     template_restore;
	Widget     autosave_toggle;
	Widget     autosave_text;

    unsigned   changed;
};

struct PrefsDataEditorAppearance
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	fe_EditorDocumentAppearancePropertiesStruct appearance_data;
};

struct PrefsDataEditorPublish
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

    Widget     maintain_links;
    Widget     keep_images;
    Widget     publish_text;
    Widget     browse_text;
    Widget     username_text;
    Widget     password_text;
    Widget     save_password;
    unsigned   changed;
};

#ifdef PREFS_UNSUPPORTED
// ---------- Offline ----------

struct PrefsDataOffline
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	Widget     online_toggle;
	Widget     offline_toggle;
	Widget     ask_toggle;
};

// ---------- OfflineNews ----------

struct PrefsDataOfflineNews
{
	MWContext *context;
	Widget     prompt_dialog;
	Widget     page;

	Widget     download_by_date_toggle;
	Widget     download_date_from_toggle;
	Widget     download_date_since_toggle;
	Widget     msg_pulldown;
	Widget     msg_option;
	Widget     from_pulldown;
	Widget     from_option;
	Widget     num_days_text;

	Widget    *msg_menu_items;
	Widget    *from_menu_items;
};
#endif /* PREFS_UNSUPPORTED */

#endif /* _xfe_prefsdata_h */

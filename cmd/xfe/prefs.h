/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#ifndef _PREFS_H_
#define _PREFS_H_

#include "xp_core.h"
#include "msgcom.h"

/* prefs version */

#define PREFS_CURRENT_VERSION "1.0"

/* browser startup page */
#define BROWSER_STARTUP_BLANK  0
#define BROWSER_STARTUP_HOME   1
#define BROWSER_STARTUP_LAST   2

/* mail server type */
#define MAIL_SERVER_POP3       0
#define MAIL_SERVER_IMAP       1
#define MAIL_SERVER_MOVEMAIL   2
#define MAIL_SERVER_INBOX      3

/* toolbar style */
#define BROWSER_TOOLBAR_ICONS_ONLY     0
#define BROWSER_TOOLBAR_TEXT_ONLY      1
#define BROWSER_TOOLBAR_ICONS_AND_TEXT 2

/* news keep method */
#define KEEP_ALL_NEWS          0
#define KEEP_NEWS_BY_AGE       1
#define KEEP_NEWS_BY_COUNT     2

/* offline startup mode */
#define OFFLINE_STARTUP_ONLINE      0
#define OFFLINE_STARTUP_OFFLINE     1
#define OFFLINE_STARTUP_ASKME       2

/* offline news download increments */
#define OFFLINE_NEWS_DL_ALL         0
#define OFFLINE_NEWS_DL_UNREAD_ONLY 1

/* offline news download increments */
#define OFFLINE_NEWS_DL_YESTERDAY    0
#define OFFLINE_NEWS_DL_1_WK_AGO     1
#define OFFLINE_NEWS_DL_2_WKS_AGO    2
#define OFFLINE_NEWS_DL_1_MONTH_AGO  3
#define OFFLINE_NEWS_DL_6_MONTHS_AGO 4
#define OFFLINE_NEWS_DL_1_YEAR_AGO   5

/* use document fonts */
#define DOC_FONTS_NEVER        0
#define DOC_FONTS_QUICK        1
#define DOC_FONTS_ALWAYS       2     

/* help file sites */
#define HELPFILE_SITE_NETSCAPE  0
#define HELPFILE_SITE_INSTALLED 1
#define HELPFILE_SITE_CUSTOM    2

/* default link expiration for 'never expired' option */
#define LINK_NEVER_EXPIRE_DAYS  180

/* default mail html action */
#define HTML_ACTION_ASK        0
#define HTML_ACTION_TEXT       1
#define HTML_ACTION_HTML       2
#define HTML_ACTION_BOTH       3

/* global preferences structure.
 */
typedef struct _XFE_GlobalPrefs 
{
	/* 
	 *  ----- preferences.js version -----
	 */

	char    *version_number;         
	int      prefs_need_upgrade; /* 0 = no change, 1 = upgrade, -1 = downgrade */

	/*
	 *  ----- Appearance -----
	 */

	/*  launch on startup */

	XP_Bool startup_browser_p;    /* new for 4.0 */
	XP_Bool startup_mail_p;       /* new for 4.0 */
	XP_Bool startup_news_p;       /* new for 4.0 */
	XP_Bool startup_editor_p;     /* new for 4.0 */
	XP_Bool startup_conference_p; /* new for 4.0 */
	XP_Bool startup_netcaster_p;  /* new for 4.0 */
	XP_Bool startup_calendar_p;   /* new for 4.0 */

	int     startup_mode;         /* browser, mail or news; 3.0 only; 
								   * replaced by items above 
								   */

	/* show toolbar as */

	int     toolbar_style;        /* new for 4.0
								   * BROWSER_TOOLBAR_ICONS_ONLY     0
								   * BROWSER_TOOLBAR_TEXT_ONLY      1
								   * BROWSER_TOOLBAR_ICONS_AND_TEXT 2
								   * will update toolbar_icons_p and toolbar_text_p
								   * when reading in toolbar_style
								   */
	XP_Bool toolbar_icons_p;
	XP_Bool toolbar_text_p;

	/*
	 *  ----- Appearance/Colors -----
	 */

	XP_Bool  underline_links_p;
	XP_Bool  use_doc_colors;
	LO_Color text_color;
	LO_Color background_color;
	LO_Color links_color;
	LO_Color vlinks_color;

	/*
	 *  ----- Advanced -----
	 */

	/* enabler */

	XP_Bool autoload_images_p;
#ifdef JAVA
	XP_Bool enable_java;
#endif
	XP_Bool enable_javascript;
	XP_Bool enable_style_sheet;
	XP_Bool auto_install;
	XP_Bool email_anonftp;
	XP_Bool warn_accept_cookie;

	int accept_cookie;	          /* 0:disable, 1:enable-nowarn, else:enable-warn */

	/*
	 *  ----- Advanced/Password -----
	 */

	XP_Bool use_password;         /* new for 4.0 */

	int     ask_password;
	int     password_timeout;

	/*
	 *  ----- Advanced/Cache -----
	 */

	int     memory_cache_size;
	int     disk_cache_size;
	int     verify_documents;
	char   *cache_dir;

	/*
	 *  ----- Advanced/Proxies -----
	 */

	int     proxy_mode;

	char   *proxy_url;

	char   *socks_host;
	int    socks_host_port;

	char   *ftp_proxy;
	int    ftp_proxy_port;
	char   *http_proxy;
	int    http_proxy_port;
	char   *gopher_proxy;
	int    gopher_proxy_port;
	char   *wais_proxy;
	int    wais_proxy_port;
#ifndef NO_SECURITY
	char   *https_proxy;
	int    https_proxy_port;
#endif
	char   *no_proxy;

	/*
	 *  ----- Advanced/Disk Space -----
	 */

	XP_Bool pop3_msg_size_limit_p;
	int     pop3_msg_size_limit;
	XP_Bool msg_prompt_purge_threshold;
	int     msg_purge_threshold;
	int     news_keep_method;    /* new for 4.0
								  * KEEP_ALL_NEWS          0
								  * KEEP_NEWS_BY_AGE       1
								  * KEEP_NEWS_BY_COUNT     2
								  */
   	int     news_keep_days;
	int     news_keep_count;
	XP_Bool news_keep_only_unread;
	XP_Bool news_remove_bodies_by_age;
	int     news_remove_bodies_days;

	/*
	 *  ----- Advanced/Help Files -----
	 */

	char   *help_source_url;
	int     help_source_site;     /*
								   * HELPFILE_SITE_NETSCAPE  0
								   * HELPFILE_SITE_INSTALLED 1
								   * HELPFILE_SITE_CUSTOM    2
								   */

	/*
	 *  ----- Browser -----
	 */

	char   *home_document;		  /* "" means start blank; overloaded in 3.0 */

	int     browser_startup_page; /* new for 4.0 
								   * BROWSER_STARTUP_BLANK  0
								   * BROWSER_STARTUP_HOME   1
								   * BROWSER_STARTUP_LAST   2
								   */

	int     global_history_expiration;	/* days */

	XP_Bool show_toolbar_p;

	XP_Bool ssl2_enable;
	XP_Bool ssl3_enable;

	/*
	 *  ----- Browser/Fonts -----
	 */

	int     use_doc_fonts;        /*
								   * DOC_FONTS_ALWAYS       0     
								   * DOC_FONTS_QUICK        1
								   * DOC_FONTS_NEVER        2
								   */

	XP_Bool enable_webfonts;      
	/*
	 *  ----- Browser/Applications -----
	 */

	char   *global_mime_types_file;
	char   *private_mime_types_file;
	char   *global_mailcap_file;
	char   *private_mailcap_file;
	char   *tmp_dir;

	/*
	 *  ----- Mail & News -----
	 */

	char   *citation_color;
	MSG_FONT citation_font;
	XP_Bool fixed_message_font_p;
	MSG_CITATION_SIZE citation_size;

	/* if these are true, then the default gesture (dbl click or selecting from
     a menu) reuses existing windows, and the alternate gesture (alt-dbl click)
     opens a new one.  If they are false, the gestures switch behavior.

     Note:  If the thing you're opening already is on the screen somewhere, that
     window is brought to the front and is reused.  There is no way to turn off this
     behavior. */

	XP_Bool reuse_thread_window;
	XP_Bool reuse_msg_window;
	XP_Bool msg_in_thread_window;

	/*
	 *  ----- Mail & News/Identity -----
	 */

	char   *real_name;
	char   *email_address;
	char   *organization;
	char   *signature_file;
	char   *reply_to_address;
	XP_Bool attach_address_card;

	/*
	 *  ----- Mail & News/Messages -----
	 */

	XP_Bool qp_p;
	XP_Bool file_attach_binary;
	XP_Bool mailbccself_p;
	XP_Bool newsbccself_p;
	XP_Bool mailfcc_p;
	XP_Bool newsfcc_p;
	XP_Bool autoquote_reply;
	XP_Bool send_html_msg;

	int     msg_wrap_length;
	int     html_def_action;

	char   *mail_bcc;
	char   *news_bcc;
	char   *mail_fcc;
	char   *news_fcc;

	/*
	 *  ----- Mail & News/Mail Server -----
	 */

	XP_Bool use_movemail_p;
	XP_Bool builtin_movemail_p;
	XP_Bool auto_check_mail;
	XP_Bool pop3_leave_mail_on_server;
	XP_Bool imap_local_copies;
	XP_Bool imap_server_ssl;
	XP_Bool imap_delete_is_move_to_trash;
	XP_Bool rememberPswd;
	XP_Bool support_skey;
	XP_Bool use_ns_mapi_server;
	XP_Bool expand_addr_nicknames_only;
	XP_Bool enable_biff;          /* new for 4.0 */

	char   *mailhost;
	char   *movemail_program;
	char   *pop3_host;
	char   *pop3_user_id;
	char   *mail_directory;
	char   *imap_mail_directory;
	char   *imap_mail_local_directory;

	int     biff_interval;

	int     mail_server_type;    /* new for 4.0
								  *	MAIL_SERVER_POP3        0
								  * MAIL_SERVER_IMAP        1
								  * MAIL_SERVER_MOVEMAIL    2
								  * MAIL_SERVER_INBOX       3
								  */

	int     reply_on_top;
	int     reply_with_extra_lines;

	/*
	 *  ----- Mail & News/News Server -----
	 */

	char   *newshost;
	char   *newsrc_directory;

	XP_Bool news_notify_on;
	XP_Bool news_server_secure;

	int     news_max_articles;
	int     news_server_port;

	/*
	 *  ----- Mail & News/Directory -----
	 */

	XP_Bool addr_book_lastname_first;

	/*
	 *  ----- Editor -----
	 */

	XP_Bool  editor_character_toolbar;
	XP_Bool  editor_paragraph_toolbar;

	char*    editor_author_name;
	char*    editor_html_editor;
	char*    editor_image_editor;
	char*    editor_document_template;
	int32    editor_autosave_period;
  
	XP_Bool  editor_custom_colors;
	LO_Color editor_background_color;
	LO_Color editor_normal_color;
	LO_Color editor_link_color;
	LO_Color editor_active_color;
	LO_Color editor_followed_color;
	char*    editor_background_image;

	XP_Bool  editor_maintain_links;
	XP_Bool  editor_keep_images;
	char*    editor_publish_location;
	char*    editor_publish_username;
	char*    editor_publish_password;
	XP_Bool  editor_save_publish_password;
	char*    editor_browse_location;

	XP_Bool  editor_copyright_hint;

	/* to to add publish stuff */

	/*
	 *  ----- Offline -----
	 */

	int      offline_startup_mode;       /*
										  * OFFLINE_STARTUP_ONLINE      0
										  * OFFLINE_STARTUP_OFFLINE     1
										  * OFFLINE_STARTUP_ASKME       2
										  */

	/*
	 *  ----- Offline/News -----
	 */

	XP_Bool  offline_news_download_unread;
	XP_Bool  offline_news_download_by_date;
	XP_Bool  offline_news_download_use_days;
	int      offline_news_download_days;
	int      offline_news_download_inc;   /* 
										   * OFFLINE_NEWS_DL_YESTERDAY   1
										   * OFFLINE_NEWS_DL_1_WK_AGO    2
										   * OFFLINE_NEWS_DL_2_WKS_AGO   3
										   * OFFLINE_NEWS_DL_1_MONTH_AGO 4
										   * OFFLINE_NEWS_DL_1_YEAR_AGO  5
										   */

	/*
	 *  ----- Miscellaneous -----
	 */

	/* OPTIONS MENU
	 */

	XP_Bool show_url_p;
	XP_Bool show_directory_buttons_p;
	XP_Bool show_menubar_p;
	XP_Bool show_bottom_status_bar_p;
	XP_Bool fancy_ftp_p;
#ifndef NO_SECURITY
	XP_Bool show_security_bar_p;
#endif

	/* APPLICATIONS */
    /* spider begin */
	/* LID CACHE */

	char   *sar_cache_dir;
    /* spider end */

	char   *tn3270_command;
	char   *telnet_command;
	char   *rlogin_command;
	char   *rlogin_user_command;

	/* CACHE
	 */

	XP_Bool cache_ssl_p;

	/* COLORS
	 */

	/* COMPOSITION
	 */
	XP_Bool queue_for_later_p;

	/* DIRECTORIES
	 */

	char *bookmark_file;

	/*####*/  char *history_file;

	/* FONTS
	 */

	/* DEFAULT MIME CSID - used if unspecified in HTTP header */
	int doc_csid;
	char* font_charset;
	char* font_spec_list; /* list of comma separated fonts */

	/* PREFERED LANGUAGES/REGIONS
	 */

	char *lang_regions;

	/*
	 * This will break on machines where int is 2 bytes.
	 * I did this in order to make reading preferences easier.
	 */

	int signature_date;

	/* IMAGES
	 */

	char *dither_images;
	XP_Bool streaming_images;

	/* MAIL
	 */

	char* mail_folder_columns;
	char* mail_message_columns;
	char* mail_sash_geometry;
	XP_Bool movemail_warn;

	/* NETWORK
	 */

	int max_connections;
	int network_buffer_size;

	/* PROTOCOLS
	 */

	XP_Bool email_submit;
	
	/* NEWS
	 */

	XP_Bool news_cache_xover;
	XP_Bool show_first_unread_p;
	char    *news_folder_columns;
	char    *news_message_columns;
	char    *news_sash_geometry;

	/* SECURITY
	 */

	XP_Bool enter_warn;
	XP_Bool leave_warn;
	XP_Bool mixed_warn;
	XP_Bool submit_warn;
	char *cipher;
	char *def_user_cert;

#ifdef FORTEZZA
	int fortezza_toggle;
	int fortezza_timeout;
#endif

	/* STYLES 1
	 */
	XP_Bool toolbar_tips_p;


	/* STYLES 2
	 */

	/* Mail and News Organization
	 */

	/*XXX*/  XP_Bool emptyTrash;
	char *pop3_password;
	XP_Bool mail_thread_p;
	XP_Bool news_thread_p;

#define FE_PANES_NORMAL     0
#define FE_PANES_STACKED    1
#define FE_PANES_HORIZONTAL 2

	int mail_pane_style;
	int news_pane_style;

	int mail_sort_style;
	int news_sort_style;

	/* BOOKMARK
	 */

    XP_Bool has_toolbar_folder;
    char *personal_toolbar_folder;

	/* PRINT
	 */

	char *print_command;
	XP_Bool print_reversed;
	XP_Bool print_color;
	XP_Bool print_landscape;
	int print_paper_size;

	/* Lawyer nonsense */
	char *license_accepted;

	/* USER HISTORY */
	char *user_history_file;


    /* Task Bar */
    XP_Bool		task_bar_floating;
    XP_Bool		task_bar_horizontal;
    XP_Bool		task_bar_ontop;
    int			task_bar_x;
    int			task_bar_y;

	/* Toolbars */
	int32		browser_navigation_toolbar_position;
	XP_Bool		browser_navigation_toolbar_showing;
	XP_Bool		browser_navigation_toolbar_open;

	int32		browser_location_toolbar_position;
	XP_Bool		browser_location_toolbar_showing;
	XP_Bool		browser_location_toolbar_open;

	int32		browser_personal_toolbar_position;
	XP_Bool		browser_personal_toolbar_showing;
	XP_Bool		browser_personal_toolbar_open;

	int32		messenger_navigation_toolbar_position;
	XP_Bool		messenger_navigation_toolbar_showing;
	XP_Bool		messenger_navigation_toolbar_open;

	int32		messenger_location_toolbar_position;
	XP_Bool		messenger_location_toolbar_showing;
	XP_Bool		messenger_location_toolbar_open;

	int32		messages_navigation_toolbar_position;
	XP_Bool		messages_navigation_toolbar_showing;
	XP_Bool		messages_navigation_toolbar_open;

	int32		messages_location_toolbar_position;
	XP_Bool		messages_location_toolbar_showing;
	XP_Bool		messages_location_toolbar_open;

	int32		folders_navigation_toolbar_position;
	XP_Bool		folders_navigation_toolbar_showing;
	XP_Bool		folders_navigation_toolbar_open;

	int32		folders_location_toolbar_position;
	XP_Bool		folders_location_toolbar_showing;
	XP_Bool		folders_location_toolbar_open;

	int32		address_book_address_book_toolbar_position;
	XP_Bool		address_book_address_book_toolbar_showing;
	XP_Bool		address_book_address_book_toolbar_open;

	int32		compose_message_message_toolbar_position;
	XP_Bool		compose_message_message_toolbar_showing;
	XP_Bool		compose_message_message_toolbar_open;

	int32		composer_composition_toolbar_position;
	XP_Bool		composer_composition_toolbar_showing;
	XP_Bool		composer_composition_toolbar_open;

	int32		composer_formatting_toolbar_position;
	XP_Bool		composer_formatting_toolbar_showing;
	XP_Bool		composer_formatting_toolbar_open;

	int32		browser_win_width;
	int32		browser_win_height;

	int32		mail_compose_win_width;
	int32		mail_compose_win_height;

	int32		editor_win_width;
	int32		editor_win_height;

	int32		mail_folder_win_width;
	int32		mail_folder_win_height;

	int32		mail_msg_win_width;
	int32		mail_msg_win_height;

	int32		mail_thread_win_width;
	int32		mail_thread_win_height;

} XFE_GlobalPrefs;


#define xfe_PREFS_ALL		-1

/* General */
#define xfe_GENERAL_OFFSET	0
#define xfe_GENERAL(which) (which-xfe_GENERAL_OFFSET)

#define xfe_PREFS_STYLES	0
#define xfe_PREFS_FONTS		1
#define xfe_PREFS_APPS		2
#define xfe_PREFS_HELPERS	3
#define xfe_PREFS_IMAGES	4
#define xfe_PREFS_LANG_REGIONS	5

/* Mail/News */
#define xfe_MAILNEWS_OFFSET	10
#define xfe_MAILNEWS(which) (which-xfe_MAILNEWS_OFFSET)

#define xfe_PREFS_APPEARANCE	10
#define xfe_PREFS_COMPOSITION	11
#define xfe_PREFS_SERVERS	12
#define xfe_PREFS_IDENTITY	13
#define xfe_PREFS_ORGANIZATION	14

/* Network */
#define xfe_NETWORK_OFFSET	20
#define xfe_NETWORK(which) (which-xfe_NETWORK_OFFSET)

#define xfe_PREFS_CACHE		20
#define xfe_PREFS_NETWORK	21
#define xfe_PREFS_PROXIES	22
#define xfe_PREFS_PROTOCOLS	23
#define xfe_PREFS_LANG		24

/* Security */
#define xfe_SECURITY_OFFSET	30
#define xfe_SECURITY(which) (which-xfe_SECURITY_OFFSET)

#define xfe_PREFS_SEC_GENERAL	30
#define xfe_PREFS_SEC_PASSWORDS	31
#define xfe_PREFS_SEC_PERSONAL	32
#define xfe_PREFS_SEC_SITE	33
/* Editor Text item from the Properties pulldown menu 26FEB96RCJ */
#define xfe_PROPERTY_CHARACTER	34	/* added 26FEB96RCJ */
#define xfe_PROPERTY_LINK	35	/* added 26FEB96RCJ */
#define xfe_PROPERTY_PARAGRAPH	36	/* added 26FEB96RCJ */

/*
#define xfe_PREFS_OPTIONS
#define xfe_PREFS_PRINT	
*/

XP_BEGIN_PROTOS

/* Fills in the default preferences */
extern void XFE_DefaultPrefs(XFE_GlobalPrefs *prefs);

/* reads in the global preferences.
 *
 * returns True on success and FALSE
 * on failure (unable to open prefs file)
 *
 * the prefs structure must be zero'd at creation since
 * this function will free any existing char pointers
 * passed in and will malloc new ones.
 */
extern Bool XFE_ReadPrefs(char * filename, XFE_GlobalPrefs *prefs);

/* saves out the global preferences.
 *
 * returns True on success and FALSE
 * on failure (unable to open prefs file)
 */
extern Bool XFE_SavePrefs(char * filename, XFE_GlobalPrefs *prefs);
extern Bool fe_CheckVersionAndSavePrefs(char * filename, XFE_GlobalPrefs *prefs);

/* Upgrades the preferences to the Javascript file. */
extern Bool XFE_UpgradePrefs(char* filename, XFE_GlobalPrefs* prefs);

extern void fe_upgrade_prefs(XFE_GlobalPrefs* prefs);
extern void fe_check_prefs_version(XFE_GlobalPrefs* prefs);

/* Set the sorting behavior on the given mail/news context. */
extern void fe_SetMailNewsSortBehavior(MWContext* context, XP_Bool thread,
				       int sortcode);

/* Register a callback with libpref so that fe_globalPrefs will stay in sync */
extern void FE_register_pref_callbacks(void);

XP_END_PROTOS

#endif /* _PREFS_H_ */

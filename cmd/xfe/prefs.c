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

/**********************************************************************
 prefs.c
 By Daniel Malmer
 11/8/96

 Unix implementation of cross-platform preferences.
 The preferences file is now written in Javascript.
 Users will be able to download new preferences over the net.

 If you want to add a new preference, add an entry to the pref_map
 array, defined below.  If you want to specify a default value, you
 will have to edit one of the Javascript files in ns/modules/libpref:

 ns/modules/libpref/src/init/all.js:  preferences for all platforms
 ns/modules/libpref/src/unix.js: preferences only on Unix
 
 The eventual goal is to eliminate most of this file.  Rather than
 other parts of the XFE accessing the fe_globalPrefs structure, they
 will make calls to PREF_GetCharPref(), PREF_GetIntPref(), etc.

**********************************************************************/


#include <stdio.h>
#include <stddef.h>
#include <pwd.h>

#include "mozilla.h"
#include "xfe.h"
#include "fonts.h"
#include "libi18n.h"
#include "xpgetstr.h"
#include "prefapi.h"
#include "prefs.h"
#include "net.h"

/* 
 * prefs version
 */

#define PREFS_INIT_VERSION    "1.0"

/*
 * external declarations
 */
extern int XFE_TILDE_USER_SYNTAX_DISALLOWED;
extern int XFE_UNKNOWN_EMAIL_ADDR;
extern int XFE_PREFS_UPGRADE;

/*
 * global definitions
 */
/* This should probably be defined someplace else... */
MSG_Prefs* fe_mailNewsPrefs = NULL;

/*
 * typedefs
 */
typedef void (*io_routine_type)(char*, void*);

/*
 * Maps a Javascript name to an offset in the preferences
 * structure.  We also need to keep track of the type of the
 * field, and a post-processing routine to fix the field.
 */
struct pref_map {
    char* name;
    size_t offset;
    io_routine_type read_routine;
    io_routine_type write_routine;
	Bool upgrade_only;        /* saved only when upgrading from 3.0 to 4.0 */
};

/* 
 * Declarations for static functions.
 */
static void fix_path(void*);
static Bool XFE_OldReadPrefs(char * filename, XFE_GlobalPrefs *prefs);
static Bool read_old_prefs_file(XFE_GlobalPrefs* prefs);
static void split_all_proxies(XFE_GlobalPrefs* prefs);
static void split_proxy(char** proxy, int* port, int default_port);
static void xfe_prefs_from_environment(XFE_GlobalPrefs* prefs);
static char* xfe_organization(void);
static void read_str(char* name, void* ptr);
static void write_str(char* name, void* ptr);
static void read_int(char* name, void* ptr);
static void write_int(char* name, void* ptr);
static void read_bool(char* name, void* ptr);
static void write_bool(char* name, void* ptr);
static void read_path(char* name, void* ptr);
static void write_path(char* name, void* ptr);
static void read_char_set(char* name, void* ptr);
static void write_char_set(char* name, void* ptr);
static void read_font_spec(char* name, void* ptr);
static void write_font_spec(char* name, void* ptr);
static void read_color(char* name, void* ptr);
static void write_color(char* name, void* ptr);
static void tweaks(XFE_GlobalPrefs* prefs);

static int def = 0;

/*
 * macros
 */
#define FIELD_OFFSET(field) offsetof(struct _XFE_GlobalPrefs, field)
#define GET_FIELD(map, prefs) ( ((char*) prefs) + map.offset )
#define REPLACE_STRING(slot, string) if ( slot ) free(slot); \
                                     slot = strdup(string);

/*
 * static definitions
 */
static struct pref_map pref_map[] = {

{"browser.xfe.prefs_version", FIELD_OFFSET(version_number), read_str, write_str},

{"applications.tn3270", FIELD_OFFSET(tn3270_command), read_str, write_str},
{"applications.telnet", FIELD_OFFSET(telnet_command), read_str, write_str},
{"applications.rlogin", FIELD_OFFSET(rlogin_command), read_str, write_str},
{"applications.rlogin_with_user", FIELD_OFFSET(rlogin_user_command), read_str, write_str},

{"autoupdate.enabled", FIELD_OFFSET(auto_install), read_bool, write_bool},

{"browser.download_directory", FIELD_OFFSET(tmp_dir), read_path, write_path, },
{"browser.chrome.show_toolbar", FIELD_OFFSET(show_toolbar_p), read_bool, write_bool},
{"browser.chrome.show_url_bar", FIELD_OFFSET(show_url_p), read_bool, write_bool},
{"browser.chrome.show_directory_buttons", FIELD_OFFSET(show_directory_buttons_p), read_bool, write_bool},
{"browser.chrome.show_menubar", FIELD_OFFSET(show_menubar_p), read_bool, write_bool},
{"browser.chrome.show_status_bar", FIELD_OFFSET(show_bottom_status_bar_p), read_bool, write_bool},
{"browser.fancy_ftp", FIELD_OFFSET(fancy_ftp_p), read_bool, write_bool},
#ifndef NO_SECURITY
{"browser.chrome.show_security_bar", FIELD_OFFSET(show_security_bar_p), read_bool, write_bool},
#endif
{"browser.cache.memory_cache_size", FIELD_OFFSET(memory_cache_size), read_int, write_int},
{"browser.cache.disk_cache_size", FIELD_OFFSET(disk_cache_size), read_int, write_int},
{"browser.cache.directory", FIELD_OFFSET(cache_dir), read_path, write_path, },
{"browser.cache.check_doc_frequency", FIELD_OFFSET(verify_documents), read_int, write_int},
/* spider begin */
{"browser.sarcache.directory", FIELD_OFFSET(sar_cache_dir), read_path, write_path, },
/* spider end */
{"browser.cache.disk_cache_ssl", FIELD_OFFSET(cache_ssl_p), read_bool, write_bool},
{"browser.bookmark_file", FIELD_OFFSET(bookmark_file), read_path, write_path, },
{"browser.history_file", FIELD_OFFSET(history_file), read_path, write_path, },
{"browser.chrome.toolbar_style", FIELD_OFFSET(toolbar_style), read_int, write_int},
{"browser.chrome.toolbar_tips", FIELD_OFFSET(toolbar_tips_p), read_bool, write_bool},
{"browser.startup.page", FIELD_OFFSET(browser_startup_page), read_int, write_int},
{"browser.startup.homepage", FIELD_OFFSET(home_document), read_str, write_str},
{"browser.underline_anchors", FIELD_OFFSET(underline_links_p), read_bool, write_bool},
{"browser.link_expiration", FIELD_OFFSET(global_history_expiration), read_int, write_int},
{"browser.startup.license_accepted", FIELD_OFFSET(license_accepted), read_str, write_str},
{"browser.user_history_file", FIELD_OFFSET(user_history_file), read_path, write_path, },
{"browser.use_document_fonts", FIELD_OFFSET(use_doc_fonts), read_int, write_int},
{"browser.enable_webfonts", FIELD_OFFSET(enable_webfonts), read_bool, write_bool},
{"browser.foreground_color", FIELD_OFFSET(text_color), read_color, write_color},
{"browser.background_color", FIELD_OFFSET(background_color), read_color, write_color},
{"browser.anchor_color", FIELD_OFFSET(links_color), read_color, write_color},
{"browser.visited_color", FIELD_OFFSET(vlinks_color), read_color, write_color},
{"browser.use_document_colors", FIELD_OFFSET(use_doc_colors), read_bool, write_bool},
{"browser.enable_style_sheets", FIELD_OFFSET(enable_style_sheet), read_bool, write_bool},

#ifdef EDITOR
/* The editor preferences haven't all been defined yet */
{"editor.show_character_toolbar", FIELD_OFFSET(editor_character_toolbar), read_bool, write_bool},
{"editor.show_paragraph_toolbar", FIELD_OFFSET(editor_paragraph_toolbar), read_bool, write_bool},
{"editor.author", FIELD_OFFSET(editor_author_name), read_str, write_str},
{"editor.html_editor", FIELD_OFFSET(editor_html_editor), read_path, write_path, },
{"editor.image_editor", FIELD_OFFSET(editor_image_editor), read_path, write_path, },
{"editor.template_location", FIELD_OFFSET(editor_document_template), read_str, write_str},
{"editor.auto_save_delay", FIELD_OFFSET(editor_autosave_period), read_int, write_int},
{"editor.use_custom_colors", FIELD_OFFSET(editor_custom_colors), read_bool, write_bool},
{"editor.background_color", FIELD_OFFSET(editor_background_color), read_color, write_color},
{"editor.text_color", FIELD_OFFSET(editor_normal_color), read_color, write_color},
{"editor.link_color", FIELD_OFFSET(editor_link_color), read_color, write_color},
{"editor.active_link_color", FIELD_OFFSET(editor_active_color), read_color, write_color},
{"editor.followed_link_color", FIELD_OFFSET(editor_followed_color), read_color, write_color},
{"editor.background_image", FIELD_OFFSET(editor_background_image), read_path, write_path, },
{"editor.publish_keep_links", FIELD_OFFSET(editor_maintain_links), read_bool, write_bool},
{"editor.publish_keep_images", FIELD_OFFSET(editor_keep_images), read_bool, write_bool},
{"editor.publish_location", FIELD_OFFSET(editor_publish_location), read_str, write_str},
{"editor.publish_username", FIELD_OFFSET(editor_publish_username), read_str, write_str},
{"editor.publish_password", FIELD_OFFSET(editor_publish_password), read_str, write_str},
{"editor.publish_save_password", FIELD_OFFSET(editor_save_publish_password), read_bool, write_bool},
{"editor.publish_browse_location", FIELD_OFFSET(editor_browse_location), read_str, write_str},
{"editor.show_copyright", FIELD_OFFSET(editor_copyright_hint), read_bool, write_bool},
#endif

#ifdef FORTEZZA
{"fortezza.toggle", FIELD_OFFSET(fortezza_toggle), read_int, write_int},
{"fortezza.timeout", FIELD_OFFSET(fortezza_timeout), read_int, write_int},
#endif

{"general.startup.browser", FIELD_OFFSET(startup_browser_p), read_bool, write_bool},
{"general.startup.mail", FIELD_OFFSET(startup_mail_p), read_bool, write_bool},
{"general.startup.news", FIELD_OFFSET(startup_news_p), read_bool, write_bool},
{"general.startup.editor", FIELD_OFFSET(startup_editor_p), read_bool, write_bool},
{"general.startup.conference", FIELD_OFFSET(startup_conference_p), read_bool, write_bool},
{"general.startup.netcaster", FIELD_OFFSET(startup_netcaster_p), read_bool, write_bool},
{"general.startup.calendar", FIELD_OFFSET(startup_calendar_p), read_bool, write_bool},
{"general.always_load_images", FIELD_OFFSET(autoload_images_p), read_bool, write_bool},
{"general.help_source.site", FIELD_OFFSET(help_source_site), read_int, write_int},
{"general.help_source.url", FIELD_OFFSET(help_source_url), read_str, write_str},

{"helpers.global_mime_types_file", FIELD_OFFSET(global_mime_types_file), read_path, write_path, },
{"helpers.private_mime_types_file", FIELD_OFFSET(private_mime_types_file), read_path, write_path, },
{"helpers.global_mailcap_file", FIELD_OFFSET(global_mailcap_file), read_path, write_path, },
{"helpers.private_mailcap_file", FIELD_OFFSET(private_mailcap_file), read_path, write_path, },

{"images.dither", FIELD_OFFSET(dither_images), read_str, write_str},
{"images.incremental_display", FIELD_OFFSET(streaming_images), read_bool, write_bool},

{"intl.character_set", FIELD_OFFSET(doc_csid), read_int, write_int},
{"intl.font_charset", FIELD_OFFSET(font_charset), read_char_set, write_char_set},
{"intl.font_spec_list", FIELD_OFFSET(font_spec_list), read_font_spec, write_font_spec},
{"intl.accept_languages", FIELD_OFFSET(lang_regions), read_str, write_str},

{"mail.play_sound", FIELD_OFFSET(enable_biff), read_bool, write_bool},
{"mail.strictly_mime", FIELD_OFFSET(qp_p), read_bool, write_bool},
{"mail.file_attach_binary", FIELD_OFFSET(file_attach_binary), read_bool, write_bool},
{"mail.deliver_immediately", FIELD_OFFSET(queue_for_later_p), read_bool, write_bool},
{"mail.default_cc", FIELD_OFFSET(mail_bcc), read_str, write_str},
{"mail.default_fcc", FIELD_OFFSET(mail_fcc), read_path, write_path, },
{"mail.cc_self", FIELD_OFFSET(mailbccself_p), read_bool, write_bool},
{"mail.use_fcc", FIELD_OFFSET(mailfcc_p), read_bool, write_bool},
{"mail.auto_quote", FIELD_OFFSET(autoquote_reply), read_bool, write_bool},
{"mail.html_compose", FIELD_OFFSET(send_html_msg), read_bool, write_bool},
/* K&R says that enum types are int's... */
{"mail.quoted_style", FIELD_OFFSET(citation_font), read_int, write_int},
{"mail.quoted_size", FIELD_OFFSET(citation_size), read_int, write_int},
{"mail.citation_color", FIELD_OFFSET(citation_color), read_str, write_str},
{"mail.identity.username", FIELD_OFFSET(real_name), read_str, write_str},
{"mail.identity.useremail", FIELD_OFFSET(email_address), read_str, write_str},
{"mail.identity.organization", FIELD_OFFSET(organization), read_str, write_str},
{"mail.identity.reply_to", FIELD_OFFSET(reply_to_address), read_str, write_str, },
{"mail.signature_file", FIELD_OFFSET(signature_file), read_path, write_path, },
{"mail.attach_vcard", FIELD_OFFSET(attach_address_card), read_bool, write_bool},
{"mail.addr_book.lastnamefirst", FIELD_OFFSET(addr_book_lastname_first), read_bool, write_bool},
/*
 * This is no good on machines where ints are less than four bytes.
 * In those cases, signature_date will have to be a time_t.
 */
{"mail.signature_date", FIELD_OFFSET(signature_date), read_int, write_int},
{"mail.use_movemail", FIELD_OFFSET(use_movemail_p), read_bool, write_bool},
{"mail.use_builtin_movemail", FIELD_OFFSET(builtin_movemail_p), read_bool, write_bool},
{"mail.movemail_program", FIELD_OFFSET(movemail_program), read_path, write_path, },
{"mail.directory", FIELD_OFFSET(mail_directory), read_path, write_path, },
{"mail.imap.server_sub_directory", FIELD_OFFSET(imap_mail_directory), read_path, write_path, },
{"mail.imap.root_dir", FIELD_OFFSET(imap_mail_local_directory), read_path, write_path, },
{"mail.check_time", FIELD_OFFSET(biff_interval), read_int, write_int},
{"mail.check_new_mail", FIELD_OFFSET(auto_check_mail), read_bool, write_bool},
{"mail.leave_on_server", FIELD_OFFSET(pop3_leave_mail_on_server), read_bool, write_bool},
{"mail.imap.local_copies", FIELD_OFFSET(imap_local_copies), read_bool, write_bool},
{"mail.imap.server_ssl", FIELD_OFFSET(imap_server_ssl), read_bool, write_bool},
{"mail.limit_message_size", FIELD_OFFSET(pop3_msg_size_limit_p), read_bool, write_bool},
{"mail.max_size", FIELD_OFFSET(pop3_msg_size_limit), read_int, write_int},
{"mail.prompt_purge_threshhold", FIELD_OFFSET(msg_prompt_purge_threshold), read_bool, write_bool},
{"mail.purge_threshhold", FIELD_OFFSET(msg_purge_threshold), read_int, write_int},
{"mail.use_mapi_server", FIELD_OFFSET(use_ns_mapi_server), read_bool, write_bool},
{"mail.movemail_warn", FIELD_OFFSET(movemail_warn), read_bool, write_bool},
{"mail.server_type", FIELD_OFFSET(mail_server_type), read_int, write_int},
{"mail.fixed_width_messages", FIELD_OFFSET(fixed_message_font_p), read_bool, write_bool},
{"mail.empty_trash", FIELD_OFFSET(emptyTrash), read_bool, write_bool},
{"mail.remember_password", FIELD_OFFSET(rememberPswd), read_bool, write_bool},
{"mail.support_skey", FIELD_OFFSET(support_skey), read_bool, write_bool},
{"mail.pop_password", FIELD_OFFSET(pop3_password), read_str, write_str},
{"mail.thread_mail", FIELD_OFFSET(mail_thread_p), read_bool, write_bool},
{"mail.pane_config", FIELD_OFFSET(mail_pane_style), read_int, write_int},
{"mail.sort_by", FIELD_OFFSET(mail_sort_style), read_int, write_int},
{"mail.default_html_action", FIELD_OFFSET(html_def_action), read_int, write_int},
{"mail.imap.delete_is_move_to_trash", FIELD_OFFSET(imap_delete_is_move_to_trash), read_bool, write_bool},

{"mailnews.reuse_message_window", FIELD_OFFSET(reuse_msg_window), read_bool, write_bool},
{"mailnews.reuse_thread_window", FIELD_OFFSET(reuse_thread_window), read_bool, write_bool},
{"mailnews.message_in_thread_window", FIELD_OFFSET(msg_in_thread_window), read_bool, write_bool},
{"mailnews.wraplength", FIELD_OFFSET(msg_wrap_length), read_int, write_int},
{"mailnews.nicknames_only", FIELD_OFFSET(expand_addr_nicknames_only), read_bool, write_bool},
{"mailnews.reply_on_top", FIELD_OFFSET(reply_on_top), read_int, write_int},
{"mailnews.reply_with_extra_lines", FIELD_OFFSET(reply_with_extra_lines), read_int, write_int},

{"network.cookie.warnAboutCookies", FIELD_OFFSET(warn_accept_cookie), read_bool, write_bool},
{"network.cookie.cookieBehavior", FIELD_OFFSET(accept_cookie), read_int, write_int},
{"network.hosts.smtp_server", FIELD_OFFSET(mailhost), read_str, write_str},
{"network.max_connections", FIELD_OFFSET(max_connections), read_int, write_int},
{"network.tcpbufsize", FIELD_OFFSET(network_buffer_size), read_int, write_int},
{"network.hosts.nntp_server", FIELD_OFFSET(newshost), read_str, write_str},
{"network.hosts.socks_server", FIELD_OFFSET(socks_host), read_str, write_str},
{"network.hosts.socks_serverport", FIELD_OFFSET(socks_host_port), read_int, write_int},
{"network.proxy.ftp", FIELD_OFFSET(ftp_proxy), read_str, write_str},
{"network.proxy.ftp_port", FIELD_OFFSET(ftp_proxy_port), read_int, write_int},
{"network.proxy.http", FIELD_OFFSET(http_proxy), read_str, write_str},
{"network.proxy.http_port", FIELD_OFFSET(http_proxy_port), read_int, write_int},
{"network.proxy.gopher", FIELD_OFFSET(gopher_proxy), read_str, write_str},
{"network.proxy.gopher_port", FIELD_OFFSET(gopher_proxy_port), read_int, write_int},
{"network.proxy.wais", FIELD_OFFSET(wais_proxy), read_str, write_str},
{"network.proxy.wais_port", FIELD_OFFSET(wais_proxy_port), read_int, write_int},
#ifndef NO_SECURITY
{"network.proxy.ssl", FIELD_OFFSET(https_proxy), read_str, write_str},
{"network.proxy.ssl_port", FIELD_OFFSET(https_proxy_port), read_int, write_int},
#endif
{"network.proxy.no_proxies_on", FIELD_OFFSET(no_proxy), read_str, write_str},
{"network.proxy.type", FIELD_OFFSET(proxy_mode), read_int, write_int},
{"network.proxy.autoconfig_url", FIELD_OFFSET(proxy_url), read_str, write_str},

{"news.default_cc", FIELD_OFFSET(news_bcc), read_str, write_str},
{"news.default_fcc", FIELD_OFFSET(news_fcc), read_path, write_path, },
{"news.cc_self", FIELD_OFFSET(newsbccself_p), read_bool, write_bool},
{"news.use_fcc", FIELD_OFFSET(newsfcc_p), read_bool, write_bool},
{"news.directory", FIELD_OFFSET(newsrc_directory), read_path, write_path, },
{"news.notify.on", FIELD_OFFSET(news_notify_on), read_bool, write_bool},
{"news.max_articles", FIELD_OFFSET(news_max_articles), read_int, write_int},
{"news.cache_xover", FIELD_OFFSET(news_cache_xover), read_bool, write_bool},
{"news.show_first_unread", FIELD_OFFSET(show_first_unread_p), read_bool, write_bool},
{"news.sash_geometry", FIELD_OFFSET(news_sash_geometry), read_str, write_str},
{"news.thread_news", FIELD_OFFSET(news_thread_p), read_bool, write_bool},
{"news.pane_config", FIELD_OFFSET(news_pane_style), read_int, write_int},
{"news.sort_by", FIELD_OFFSET(news_sort_style), read_int, write_int},
{"news.keep.method", FIELD_OFFSET(news_keep_method), read_int, write_int},
{"news.keep.days", FIELD_OFFSET(news_keep_days), read_int, write_int},
{"news.keep.count", FIELD_OFFSET(news_keep_count), read_int, write_int},
{"news.keep.only_unread", FIELD_OFFSET(news_keep_only_unread), read_bool, write_bool},
{"news.remove_bodies.by_age", FIELD_OFFSET(news_remove_bodies_by_age), read_bool, write_bool},
{"news.remove_bodies.days", FIELD_OFFSET(news_remove_bodies_days), read_int, write_int},
{"news.server_port", FIELD_OFFSET(news_server_port), read_int, write_int},
{"news.server_is_secure", FIELD_OFFSET(news_server_secure), read_bool, write_bool},

{"offline.startup_mode", FIELD_OFFSET(offline_startup_mode), read_int, write_int},
{"offline.news.download.unread_only", FIELD_OFFSET(offline_news_download_unread), read_bool, write_bool},
{"offline.news.download.by_date", FIELD_OFFSET(offline_news_download_by_date), read_bool, write_bool},
{"offline.news.download.use_days", FIELD_OFFSET(offline_news_download_use_days), read_bool, write_bool},
{"offline.news.download.days", FIELD_OFFSET(offline_news_download_days), read_int, write_int},
{"offline.news.download.increments", FIELD_OFFSET(offline_news_download_inc), read_int, write_int},

{"security.email_as_ftp_password", FIELD_OFFSET(email_anonftp), read_bool, write_bool},
{"security.submit_email_forms", FIELD_OFFSET(email_submit), read_bool, write_bool},
{"security.warn_entering_secure", FIELD_OFFSET(enter_warn), read_bool, write_bool, True},
{"security.warn_leaving_secure", FIELD_OFFSET(leave_warn), read_bool, write_bool, True},
{"security.warn_viewing_mixed", FIELD_OFFSET(mixed_warn), read_bool, write_bool, True},
{"security.warn_submit_insecure", FIELD_OFFSET(submit_warn), read_bool, write_bool, True},
#ifdef JAVA
{"security.enable_java", FIELD_OFFSET(enable_java), read_bool, write_bool},
#endif
{"javascript.enabled", FIELD_OFFSET(enable_javascript), read_bool, write_bool},
{"security.enable_ssl2", FIELD_OFFSET(ssl2_enable), read_bool, write_bool, True},
{"security.enable_ssl3", FIELD_OFFSET(ssl3_enable), read_bool, write_bool, True},
{"security.ciphers", FIELD_OFFSET(cipher), read_str, write_str, True},
{"security.default_personal_cert", FIELD_OFFSET(def_user_cert), read_str, write_str, True},
{"security.use_password", FIELD_OFFSET(use_password), read_bool, write_bool, True},
{"security.ask_for_password", FIELD_OFFSET(ask_password), read_int, write_int, True},
{"security.password_lifetime", FIELD_OFFSET(password_timeout), read_int, write_int, True},
{"custtoolbar.has_toolbar_folder", FIELD_OFFSET(has_toolbar_folder), read_bool, write_bool},
{"custtoolbar.personal_toolbar_folder", FIELD_OFFSET(personal_toolbar_folder), read_str, write_str},
{"print.print_command", FIELD_OFFSET(print_command), read_str, write_str},
{"print.print_reversed", FIELD_OFFSET(print_reversed), read_bool, write_bool},
{"print.print_color", FIELD_OFFSET(print_color), read_bool, write_bool},
{"print.print_landscape", FIELD_OFFSET(print_landscape), read_bool, write_bool},
{"print.print_paper_size", FIELD_OFFSET(print_paper_size), read_int, write_int},


{"taskbar.floating", FIELD_OFFSET(task_bar_floating), read_bool, write_bool},
{"taskbar.horizontal", FIELD_OFFSET(task_bar_horizontal), read_bool, write_bool},
{"taskbar.ontop", FIELD_OFFSET(task_bar_ontop), read_bool, write_bool},
{"taskbar.x", FIELD_OFFSET(task_bar_x), read_int, write_int},
{"taskbar.y", FIELD_OFFSET(task_bar_y), read_int, write_int},

{"custtoolbar.Browser.Navigation_Toolbar.position", FIELD_OFFSET(browser_navigation_toolbar_position), read_int, write_int},
{"custtoolbar.Browser.Navigation_Toolbar.showing", FIELD_OFFSET(browser_navigation_toolbar_showing), read_bool, write_bool},
{"custtoolbar.Browser.Navigation_Toolbar.open", FIELD_OFFSET(browser_navigation_toolbar_open), read_bool, write_bool},

{"custtoolbar.Browser.Location_Toolbar.position", FIELD_OFFSET(browser_location_toolbar_position), read_int, write_int},
{"custtoolbar.Browser.Location_Toolbar.showing", FIELD_OFFSET(browser_location_toolbar_showing), read_bool, write_bool},
{"custtoolbar.Browser.Location_Toolbar.open", FIELD_OFFSET(browser_location_toolbar_open), read_bool, write_bool},

{"custtoolbar.Browser.Personal_Toolbar.position", FIELD_OFFSET(browser_personal_toolbar_position), read_int, write_int},
{"custtoolbar.Browser.Personal_Toolbar.showing", FIELD_OFFSET(browser_personal_toolbar_showing), read_bool, write_bool},
{"custtoolbar.Browser.Personal_Toolbar.open", FIELD_OFFSET(browser_personal_toolbar_open), read_bool, write_bool},

{"custtoolbar.Messenger.Navigation_Toolbar.position", FIELD_OFFSET(messenger_navigation_toolbar_position), read_int, write_int},
{"custtoolbar.Messenger.Navigation_Toolbar.showing", FIELD_OFFSET(messenger_navigation_toolbar_showing), read_bool, write_bool},
{"custtoolbar.Messenger.Navigation_Toolbar.open", FIELD_OFFSET(messenger_navigation_toolbar_open), read_bool, write_bool},

{"custtoolbar.Messenger.Location_Toolbar.position", FIELD_OFFSET(messenger_location_toolbar_position), read_int, write_int},
{"custtoolbar.Messenger.Location_Toolbar.showing", FIELD_OFFSET(messenger_location_toolbar_showing), read_bool, write_bool},
{"custtoolbar.Messenger.Location_Toolbar.open", FIELD_OFFSET(messenger_location_toolbar_open), read_bool, write_bool},

{"custtoolbar.Messages.Navigation_Toolbar.position", FIELD_OFFSET(messages_navigation_toolbar_position), read_int, write_int},
{"custtoolbar.Messages.Navigation_Toolbar.showing", FIELD_OFFSET(messages_navigation_toolbar_showing), read_bool, write_bool},
{"custtoolbar.Messages.Navigation_Toolbar.open", FIELD_OFFSET(messages_navigation_toolbar_open), read_bool, write_bool},

{"custtoolbar.Messages.Location_Toolbar.position", FIELD_OFFSET(messages_location_toolbar_position), read_int, write_int},
{"custtoolbar.Messages.Location_Toolbar.showing", FIELD_OFFSET(messages_location_toolbar_showing), read_bool, write_bool},
{"custtoolbar.Messages.Location_Toolbar.open", FIELD_OFFSET(messages_location_toolbar_open), read_bool, write_bool},

{"custtoolbar.Folders.Navigation_Toolbar.position", FIELD_OFFSET(folders_navigation_toolbar_position), read_int, write_int},
{"custtoolbar.Folders.Navigation_Toolbar.showing", FIELD_OFFSET(folders_navigation_toolbar_showing), read_bool, write_bool},
{"custtoolbar.Folders.Navigation_Toolbar.open", FIELD_OFFSET(folders_navigation_toolbar_open), read_bool, write_bool},

{"custtoolbar.Folders.Location_Toolbar.position", FIELD_OFFSET(folders_location_toolbar_position), read_int, write_int},
{"custtoolbar.Folders.Location_Toolbar.showing", FIELD_OFFSET(folders_location_toolbar_showing), read_bool, write_bool},
{"custtoolbar.Folders.Location_Toolbar.open", FIELD_OFFSET(folders_location_toolbar_open), read_bool, write_bool},

{"custtoolbar.Address_Book.Address_Book_Toolbar.position", FIELD_OFFSET(address_book_address_book_toolbar_position), read_int, write_int},
{"custtoolbar.Address_Book.Address_Book_Toolbar.showing", FIELD_OFFSET(address_book_address_book_toolbar_showing), read_bool, write_bool},
{"custtoolbar.Address_Book.Address_Book_Toolbar.open", FIELD_OFFSET(address_book_address_book_toolbar_open), read_bool, write_bool},

{"custtoolbar.Compose_Message.Message_Toolbar.position", FIELD_OFFSET(compose_message_message_toolbar_position), read_int, write_int},
{"custtoolbar.Compose_Message.Message_Toolbar.showing", FIELD_OFFSET(compose_message_message_toolbar_showing), read_bool, write_bool},
{"custtoolbar.Compose_Message.Message_Toolbar.open", FIELD_OFFSET(compose_message_message_toolbar_open), read_bool, write_bool},

{"custtoolbar.Composer.Composition_Toolbar.position", FIELD_OFFSET(composer_composition_toolbar_position), read_int, write_int},
{"custtoolbar.Composer.Composition_Toolbar.showing", FIELD_OFFSET(composer_composition_toolbar_showing), read_bool, write_bool},
{"custtoolbar.Composer.Composition_Toolbar.open", FIELD_OFFSET(composer_composition_toolbar_open), read_bool, write_bool},

{"custtoolbar.Composer.Formatting_Toolbar.position", FIELD_OFFSET(composer_formatting_toolbar_position), read_int, write_int},
{"custtoolbar.Composer.Formatting_Toolbar.showing", FIELD_OFFSET(composer_formatting_toolbar_showing), read_bool, write_bool},
{"custtoolbar.Composer.Formatting_Toolbar.open", FIELD_OFFSET(composer_formatting_toolbar_open), read_bool, write_bool},

{"browser.win_width", FIELD_OFFSET(browser_win_width), read_int, write_int},
{"browser.win_height",FIELD_OFFSET(browser_win_height), read_int, write_int},

{"mail.compose.win_width", FIELD_OFFSET(mail_compose_win_width), read_int, write_int},
{"mail.compose.win_height",FIELD_OFFSET(mail_compose_win_height), read_int, write_int},

{"editor.win_width", FIELD_OFFSET(editor_win_width), read_int, write_int},
{"editor.win_height",FIELD_OFFSET(editor_win_height), read_int, write_int},

{"mail.folder.win_width", FIELD_OFFSET(mail_folder_win_width), read_int, write_int},
{"mail.folder.win_height",FIELD_OFFSET(mail_folder_win_height), read_int, write_int},

{"mail.msg.win_width", FIELD_OFFSET(mail_msg_win_width), read_int, write_int},
{"mail.msg.win_height",FIELD_OFFSET(mail_msg_win_height), read_int, write_int},

{"mail.thread.win_width", FIELD_OFFSET(mail_thread_win_width), read_int, write_int},
{"mail.thread.win_height",FIELD_OFFSET(mail_thread_win_height), read_int, write_int},

};

static int num_prefs = sizeof(pref_map) / sizeof(pref_map[0]);

/*
 * XFE_UpgradePrefs
 * Upgrades the preferences to the Javascript file.
 */
Bool
XFE_UpgradePrefs(char* filename, XFE_GlobalPrefs* prefs)
{
    int i;
    char* tmp = NULL;
    void* field;
    Bool status;
    XP_Bool passwordProtectLocalCache;

    PREF_GetBoolPref("mail.password_protect_local_cache",
                     &passwordProtectLocalCache);
    /*
     * If we're not supposed to remember the password, replace
     * the password with blank, and then restore it after saving.
     */
    if ( !prefs->rememberPswd && !passwordProtectLocalCache) {
        tmp = prefs->pop3_password;
        prefs->pop3_password = "";
    }

    split_all_proxies(prefs);

    for ( i = 0; i < num_prefs; i++ ) {
        field = GET_FIELD(pref_map[i], prefs);
        pref_map[i].write_routine(pref_map[i].name, field);
    }

    status = ( PREF_SavePrefFileAs(filename) == PREF_NOERROR ) ? TRUE : FALSE;

    if ( !prefs->rememberPswd && !passwordProtectLocalCache) {
        prefs->pop3_password = tmp;
    }

    return status;
}

/*
 * XFE_SavePrefs
 * Save the preferences to the Javascript file.
 */
Bool
XFE_SavePrefs(char* filename, XFE_GlobalPrefs* prefs)
{
    int i;
    char* tmp = NULL;
    void* field;
    Bool status;
    XP_Bool passwordProtectLocalCache;
	
    PREF_GetBoolPref("mail.password_protect_local_cache",
                     &passwordProtectLocalCache);

    /*
     * If we're not supposed to remember the password, replace
     * the password with blank, and then restore it after saving.
     */
    if ( !prefs->rememberPswd && !passwordProtectLocalCache) {
        tmp = prefs->pop3_password;
        prefs->pop3_password = "";
    }

    /* split_all_proxies(prefs); */

    for ( i = 0; i < num_prefs; i++ ) {
		if (! pref_map[i].upgrade_only) {
			field = GET_FIELD(pref_map[i], prefs);
			pref_map[i].write_routine(pref_map[i].name, field);
		}
    }

    status = ( PREF_SavePrefFileAs(filename) == PREF_NOERROR ) ? TRUE : FALSE;

    if ( !prefs->rememberPswd && !passwordProtectLocalCache) {
        prefs->pop3_password = tmp;
    }

    return status;
}


/*
 * read_old_prefs_file
 * Tries to read all of the old preferences files that might be on
 * disk.  Starts at most recent naming convention, and works its
 * way back.
 */
static Bool
read_old_prefs_file(XFE_GlobalPrefs* prefs)
{
    int i;
    struct stat st;
    char buf[1024];
    char* home_dir;
    char* filenames[] = {"%s/.netscape/preferences",
                         "%s/.netscape-preferences",
                         "%s/.MCOM-preferences",};

    /* Code stolen from mozilla.c */
    home_dir = getenv ("HOME");
    if (!home_dir || !*home_dir) {
        /* Default to "/" in case a root shell is running in dire straits. */
        struct passwd *pw = getpwuid(getuid());
        home_dir = pw ? pw->pw_dir : "/";
    } else {
        char *slash;
        /* Trim trailing slashes just to be fussy. */
        while ((slash = strrchr(home_dir, '/')) && slash[1] == '\0')
        *slash = '\0';
    }

    for ( i = 0; i < sizeof(filenames)/sizeof(filenames[0]); i++ ) {
        PR_snprintf(buf, sizeof(buf), filenames[i], home_dir);
        if ( stat(buf, &st) == 0 ) {
            return XFE_OldReadPrefs(buf, prefs);
        }
    }

    return FALSE;
}


/*
 * XFE_ReadPrefs
 * Read preferences from the Javascript file.
 */
Bool
XFE_ReadPrefs(char* filename, XFE_GlobalPrefs* prefs)
{
    int i;
    void* field;
    struct stat st;

    XFE_DefaultPrefs(prefs);
    xfe_prefs_from_environment(prefs);

    if ( stat(filename, &st) == -1 ) {
        if ( read_old_prefs_file(prefs) == FALSE ) {
            return FALSE;
        }
    }

    def = 0; /* Indicate that we want to read real prefs, not defaults */
    for ( i = 0; i < num_prefs; i++ ) {
        field = GET_FIELD(pref_map[i], prefs);
        pref_map[i].read_routine(pref_map[i].name, field);
    }

	tweaks(prefs);

    return TRUE;
}


/*
 * XFE_DefaultPrefs
 */
void
XFE_DefaultPrefs(XFE_GlobalPrefs* prefs)
{
    int i;
    void* field;
    char* email_addr;
    char* real_name;
    char* organization;

    for ( i = 0; i < num_prefs; i++ ) {
        field = GET_FIELD(pref_map[i], prefs);
        pref_map[i].read_routine(pref_map[i].name, field);
    }

    prefs->doc_csid = XFE_GetDefaultCSID();
    fe_DefaultUserInfo(&email_addr, &real_name, False);
    organization = xfe_organization();

    REPLACE_STRING(prefs->email_address, email_addr ? 
                                         email_addr : 
                                         XP_GetString(XFE_UNKNOWN_EMAIL_ADDR));
    REPLACE_STRING(prefs->email_address, email_addr);
    REPLACE_STRING(prefs->real_name, real_name);
    REPLACE_STRING(prefs->organization, organization);

    if ( organization ) free(organization);

	tweaks(prefs);
}


/*
 * fix_path
 */
static void
fix_path(void* arg)
{
    char** path = (char**) arg;
    char* home = getenv ("HOME");

    if ( !home ) home = "";
 
    if ( *path && **path == '~') {
        if ( (*path)[1] != '/' && (*path)[1] != 0) {
            fprintf (stderr, XP_GetString(XFE_TILDE_USER_SYNTAX_DISALLOWED),
                     XP_AppName);
        } else {
            char* new = (char*) malloc ( strlen(*path) + strlen(home) + 3 );
            if ( new == NULL ) return;
            strcpy(new, home);
            if ( home[strlen(home)-1] != '/' ) strcat (new, "/");
            if ( (*path)[1] && (*path)[2] ) strcat (new, (*path) + 2);
            free ( *path );
            *path = new;
        }
    }
}


/*
 * split_all_proxies
 * Trims the :port off the end of all of the proxies.
 */
static void
split_all_proxies(XFE_GlobalPrefs* prefs)
{
    split_proxy(&prefs->socks_host, &prefs->socks_host_port, 0);
    split_proxy(&prefs->ftp_proxy, &prefs->ftp_proxy_port, 0);
    split_proxy(&prefs->http_proxy, &prefs->http_proxy_port, 0);
    split_proxy(&prefs->gopher_proxy, &prefs->gopher_proxy_port, 0);
    split_proxy(&prefs->wais_proxy, &prefs->wais_proxy_port, 0);
#ifndef NO_SECURITY
    split_proxy(&prefs->https_proxy, &prefs->https_proxy_port, 0);
#endif
}


/*
 * split_proxy
 * Takes a proxy of the form "host:port".
 * Trims the port off the end.
 */
static void
split_proxy(char** proxy, int* port, int default_port)
{
    char* tmp;

    assert(port != NULL);

    if (tmp = strchr(*proxy, ':') ) {
        *tmp = '\0';
        *port = atoi(tmp+1);
    } else {
		/* If *port is set, we're upgrading from
		   a beta so don't overwrite with default_port */
		if(!*port) {
			*port = default_port;
		}
    }
}


/*
 * xfe_prefs_from_environment
 */
static void
xfe_prefs_from_environment (XFE_GlobalPrefs *prefs)
{
  /* Set some slots in the prefs structure based on the environment.
   */
  char *env;
  if ((env = getenv ("WWW_HOME")))     StrAllocCopy (prefs->home_document,env);
  if ((env = getenv ("NNTPSERVER")))   StrAllocCopy (prefs->newshost, env);
  if ((env = getenv ("TMPDIR")))       StrAllocCopy (prefs->tmp_dir, env);

  if ((env = getenv ("NEWSRC")))
    {
      char *s;
      StrAllocCopy (prefs->newsrc_directory, env);
      s = strrchr (prefs->newsrc_directory, '/');
      if (s) *s = 0;
    }

  /* These damn proxy env vars *should* be set to "host:port" but apparently
     for historical reasons they tend to be set to "http://host:port/proxy/"
     so if there are any slashes in the name, extract the host and port as
     if it were a URL.

     And, as extra BS, we need to map "http://host/" to "host:80".  FMH.
   */
#define PARSE_PROXY_ENV(NAME,VAR)				\
    if ((env = getenv (NAME)))					\
      {								\
	char *new = (strchr (env, '/')				\
		     ? NET_ParseURL (env, GET_HOST_PART)	\
		     : strdup (env));				\
        if (! strchr (new, ':'))				\
	  {							\
	    char *new2 = (char *) malloc (strlen (new) + 10);	\
	    strcpy (new2, new);					\
	    strcat (new2, ":80");				\
	    free (new);						\
	    new = new2;						\
	  }							\
	if (VAR) free (VAR);					\
	VAR = new;						\
      }
  PARSE_PROXY_ENV ("ftp_proxy", prefs->ftp_proxy)
  PARSE_PROXY_ENV ("http_proxy", prefs->http_proxy)
#ifndef NO_SECURITY
  PARSE_PROXY_ENV ("https_proxy", prefs->https_proxy)
#endif
  PARSE_PROXY_ENV ("gopher_proxy", prefs->gopher_proxy)
  PARSE_PROXY_ENV ("wais_proxy", prefs->wais_proxy)
  /* #### hack commas in some way? */
  PARSE_PROXY_ENV ("no_proxy", prefs->no_proxy)
#undef PARSE_PROXY_ENV

  if (prefs->organization) free (prefs->organization);
  prefs->organization = xfe_organization ();
}


/* Hack yet another damned environment variable.
   Returns a string for the organization, or "" if none.
   Consults the ORGANIZATION environment variable, which
   may be a string (the organization) or 
 */
static char *
xfe_organization (void)
{
  char *e;

  e = getenv ("NEWSORG");	/* trn gives this priority. */
  if (!e || !*e)
    e = getenv ("ORGANIZATION");

  if (!e || !*e)
    {
      /* GNUS does this so it must be right... */
      char *home = getenv ("HOME");
      if (!home) home = "";
      e = (char *) malloc (strlen (home) + 20);
      strcpy (e, home);
      strcat (e, "/.organization");
    }
  else
    {
      e = strdup (e);
    }

  if (*e == '/')
    {
      FILE *f = fopen (e, "r");
      if (f)
	{
	  char buf [1024];
	  char *s = buf;
	  int left = sizeof (buf) - 5;
	  int read;
	  *s = 0;
	  while (1)
	    {
	      if (fgets (s, left, f))
		read = strlen (s);
	      else
		break;
	      left -= read;
	      s += read;
	      *s++ = '\t';
	    }
	  *s = 0;
	  fclose (f);
	  free (e);
	  e = strdup (fe_StringTrim (buf));
	}
      else
	{
	  /* File unreadable - set e to "" */
	  *e = 0;
	}
    }

  if (! e) abort ();
  return e;
}


/*
 * XFE_GetDefaultCSID
 */
int16
XFE_GetDefaultCSID(void)
{
  int16 csid;
 
  csid = 0;
 
  if ((fe_globalData.default_url_charset != NULL)
      && (fe_globalData.default_url_charset[0] != 0))
    csid = (INTL_CharSetNameToID((char *)fe_globalData.default_url_charset));
 
  if (csid)
  {
    if (csid == CS_JIS)
    {
        csid = CS_JIS_AUTO;
    }
    return csid;
  }
 
  return (CS_LATIN1);
}


/*
 * read_str
 */
static void
read_str(char* name, void* field)
{
    char buf[4096];
    int len = sizeof(buf);

    memset(buf, 0, len);

    if ( def ) {
        PREF_GetDefaultCharPref(name, buf, &len);
    } else {
        PREF_GetCharPref(name, buf, &len);
    }

    *(char**) field = strdup(buf);
}


/*
 * write_str
 */
static void
write_str(char* name, void* field)
{
    PREF_SetCharPref(name, *(char**) field ? *(char**) field : "");
}


/*
 * read_bool
 */
static void
read_bool(char* name, void* field)
{
    if ( def ) {
        PREF_GetDefaultBoolPref(name, (Bool*) field);
    } else {
        PREF_GetBoolPref(name, (Bool*) field);
    }
}


/*
 * write_bool
 */
static void
write_bool(char* name, void* field)
{
    PREF_SetBoolPref(name, *(Bool*) field);
}


/*
 * read_int
 */
static void
read_int(char* name, void* field)
{
    if ( def ) {
        PREF_GetDefaultIntPref(name, (int32*) field);
    } else {
        PREF_GetIntPref(name, (int32*) field);
    }
}


/*
 * write_int
 */
static void
write_int(char* name, void* field)
{
    PREF_SetIntPref(name, *(int32*) field);
}


/*
 * Paths
 * If there's a tilde in the path, we expand it to the user's
 * home directory.
 */

/*
 * read_path
 */
static void
read_path(char* name, void* field)
{
    read_str(name, field);
    fix_path(field);
}


/*
 * write_path
 */
static void
write_path(char* name, void* field)
{
    fix_path(field);
    write_str(name, field);
}


/*
 * Editor colors
 * LO_Color is a structure that has red, blue and green values.  This
 * value is stored in the prefs file as #rrggbb.
 */

/*
 * read_color
 */
static void
read_color(char* name, void* field)
{
    LO_Color* color = (LO_Color*) field;

    if ( def ) {
        PREF_GetDefaultColorPref(name, &color->red, &color->green, &color->blue);
    } else {
        PREF_GetColorPref(name, &color->red, &color->green, &color->blue);
    }
}


/*
 * write_color
 */
static void
write_color(char* name, void* field)
{
    LO_Color* color = (LO_Color*) field;

    PREF_SetColorPref(name, color->red, color->green, color->blue);
}


/*
 * Character sets
 * It's not clear that this belongs in 4.0, but it was there
 * for 2.0, 3.0, so we'll carry it forward for the time being.
 */

/*
 * read_char_set
 */
static void
read_char_set(char* name, void* field)
{
    char* char_set = *(char**) field;

    if ( def ) return;

    read_str(name, &char_set);

    if ( char_set && *char_set ) {
        fe_ReadFontCharSet(char_set);
    }
}


/*
 * write_char_set
 */
static void
write_char_set(char* name, void* field)
{
    char* char_set = *(char**) field;

    REPLACE_STRING(char_set, fe_GetFontCharSetSetting());
    write_str(name, &char_set);
}


/*
 * Font specs
 * In the 2.0/3.0 Navigator, each FONT_SPEC was written out a line
 * at a time.  It's not clear that it belongs in the preferences file
 * anymore, but we write it out as a comma-separated list for the time
 * being.
 */

/*
 * read_font_spec
 */
static void
read_font_spec(char* name, void* field)
{
    char* font_spec = *(char**) field;

    if ( def ) return;

    read_str(name, &font_spec);

    if ( font_spec && *font_spec ) {
        char* copy = strdup(font_spec);
        char* tmp = strtok(copy, ",");
 
        do {
            fe_ReadFontSpec(tmp);
        } while ( (tmp = strtok(NULL, ",")) != NULL );
        free(copy);
    }
}


/*
 * write_font_spec
 */
static void
write_font_spec(char* name, void* field)
{
    char* font_spec = *(char**) field;
    fe_FontSettings* next;
    fe_FontSettings* set;
    char buf[4096];

    set = fe_GetFontSettings();
    next = set;
    buf[0] = '\0';
    while ( next ) {
        if ( strchr(next->spec, '\n') ) {
            *strchr(next->spec, '\n') = '\0';
        }
        strcat(buf, next->spec);
        strcat(buf, ",");
        next = next->next;
    } 
    REPLACE_STRING(font_spec, buf);
    fe_FreeFontSettings(set);

    write_str(name, &font_spec);
}


/*
 * prefs_parse_color
 */
static Boolean
prefs_parse_color(LO_Color* color, char* string)
{
    if (!LO_ParseRGB(string, &color->red, &color->green, &color->blue)) {
    color->red = color->blue = color->green = 0; /* black */
    return FALSE;
    }
    return TRUE;
}


/*
 * xfe_prefs_get_line
 * Read a line of preferences
 * handling multiline values
 */
static char *
xfe_prefs_get_line(char *s, int n, FILE *fp)
{
  char *p, *q;
  int len;
 
  /*
   * get a line
   */
  p = fgets(s, n, fp);
  if (p == NULL)
    return p;
 
  /*
   * Check for multiline
   */
  q = p;
  while (q) {
    len = strlen(q);
    /* must have at least two chars: backslash and new-line */
    if (len < 2)
      break;
    /* check if the line ends with a '\\' and a '\n' */
    if ((*(q+len-1) != '\n') || (*(q+len-2) != '\\'))
      break;
    /* drop the slash and retain the new-line */
    *(q+len-2) = '\n';
    *(q+len-1) = '\0';
    n -= len - 1; /* less space left in buffer */
    /* stop if no space left */
    if (n <= 0)
      break;
    /* get the next part */
    q = fgets(q+len-1, n, fp);
  }
  return p;
}


/* reads in the global preferences.
 * 
 * returns True on success and FALSE
 * on failure (unable to open prefs file)
 *
 * the prefs structure must be zero'd at creation since
 * this function will free any existing char pointers 
 * passed in and will malloc new ones.
 */
static Bool
XFE_OldReadPrefs(char * filename, XFE_GlobalPrefs *prefs)
{
	FILE * fp;
	char buffer [4096];
	char *name, *value, *colon;

  /* First set all slots to the builtin defaults.
     Then, let the environment override that.
     Then, let the file override the environment.

     This means that, generally, the environment variables are only
     consulted when there is no preferences file on disk - once the
     preferences get saved, the prevailing env. var values will have
     been captured in it.
   */

	XFE_DefaultPrefs(prefs);
	xfe_prefs_from_environment (prefs);

	fp = fopen (filename, "r");

	if (!fp)
		return(FALSE);

	if (! xfe_prefs_get_line(buffer, 4096, fp))
		*buffer = 0;

	while (xfe_prefs_get_line (buffer, 4096, fp)) {
		name = XP_StripLine (buffer);

		if (*name == '#')
			continue;  /* comment */

		colon = strchr (name, ':');
		
		if (!colon)
			continue;  /* not a name/value pair */

		*colon= '\0';  /* terminate */

		value = XP_StripLine (colon+1);

# define BOOLP(x) (XP_STRCASECMP((x),"TRUE") ? FALSE : TRUE)

		/* OPTIONS MENU
		 */
		if (!XP_STRCASECMP("SHOW_TOOLBAR", name))
			prefs->show_toolbar_p = BOOLP (value);
		else if (!XP_STRCASECMP("SHOW_URL", name))
			prefs->show_url_p = BOOLP (value);
		else if (!XP_STRCASECMP("SHOW_DIRECTORY_BUTTONS", name))
			prefs->show_directory_buttons_p = BOOLP (value);
		else if (!XP_STRCASECMP("SHOW_MENUBAR", name))
			prefs->show_menubar_p = BOOLP (value);
		else if (!XP_STRCASECMP("SHOW_BOTTOM_STATUS_BAR", name))
			prefs->show_bottom_status_bar_p = BOOLP (value);
		else if (!XP_STRCASECMP("AUTOLOAD_IMAGES", name))
			prefs->autoload_images_p = BOOLP (value);
		else if (!XP_STRCASECMP("FTP_FILE_INFO", name))
			prefs->fancy_ftp_p = BOOLP (value);
#ifndef NO_SECURITY
		else if (!XP_STRCASECMP("SHOW_SECURITY_BAR", name))
			prefs->show_security_bar_p = BOOLP (value);
#endif


		/* APPLICATIONS
		 */
		else if (!XP_STRCASECMP("TN3270", name))
			StrAllocCopy (prefs->tn3270_command, value);
		else if (!XP_STRCASECMP("TELNET", name))
			StrAllocCopy (prefs->telnet_command, value);
		else if (!XP_STRCASECMP("RLOGIN", name))
			StrAllocCopy (prefs->rlogin_command, value);
		else if (!XP_STRCASECMP("RLOGIN_USER", name))
			StrAllocCopy (prefs->rlogin_user_command, value);

		/* CACHE
		 */
		else if (!XP_STRCASECMP("MEMORY_CACHE_SIZE", name))
			prefs->memory_cache_size = atoi (value);
		else if (!XP_STRCASECMP("DISK_CACHE_SIZE", name))
			prefs->disk_cache_size = atoi (value);
		else if (!XP_STRCASECMP("CACHE_DIR", name))
			StrAllocCopy (prefs->cache_dir, value);
		else if (!XP_STRCASECMP("VERIFY_DOCUMENTS", name))
			prefs->verify_documents = atoi (value);
		else if (!XP_STRCASECMP("CACHE_SSL_PAGES", name))
			prefs->cache_ssl_p = BOOLP (value);

		/* COLORS
		 */
	
		/* COMPOSITION
		 */
		else if (!XP_STRCASECMP("8BIT_MAIL_AND_NEWS", name))
			prefs->qp_p = !BOOLP (value);
		else if (!XP_STRCASECMP("QUEUE_FOR_LATER", name))
			prefs->queue_for_later_p = BOOLP (value);
		else if (!XP_STRCASECMP("MAIL_BCC_SELF", name))
			prefs->mailbccself_p = BOOLP (value);
		else if (!XP_STRCASECMP("NEWS_BCC_SELF", name))
			prefs->newsbccself_p = BOOLP (value);
		else if (!XP_STRCASECMP("MAIL_BCC", name))
			StrAllocCopy (prefs->mail_bcc, value);
		else if (!XP_STRCASECMP("NEWS_BCC", name))
			StrAllocCopy (prefs->news_bcc, value);
		else if (!XP_STRCASECMP("MAIL_FCC", name)) {
			StrAllocCopy (prefs->mail_fcc, value);
			/* 4.0 prefs */
			prefs->mailfcc_p = TRUE;
		}
		else if (!XP_STRCASECMP("NEWS_FCC", name)) {
			StrAllocCopy (prefs->news_fcc, value);
			/* 4.0 prefs */
			prefs->newsfcc_p = TRUE;
		}
		else if (!XP_STRCASECMP("MAIL_AUTOQUOTE_REPLY", name))
			prefs->autoquote_reply = BOOLP (value);

		/* DIRECTORIES
		 */
		else if (!XP_STRCASECMP("TMPDIR", name))
			StrAllocCopy (prefs->tmp_dir, value);
		else if (!XP_STRCASECMP("BOOKMARKS_FILE", name))
			StrAllocCopy (prefs->bookmark_file, value);
		else if (!XP_STRCASECMP("HISTORY_FILE", name))
			StrAllocCopy (prefs->history_file, value);
		else if (!XP_STRCASECMP("USER_HISTORY_FILE", name)) /* add to save user typing*/
			StrAllocCopy (prefs->user_history_file, value);

		/* FONTS
		 */
		else if (!XP_STRCASECMP("DOC_CSID", name))
			prefs->doc_csid = atoi (value);
		else if (!XP_STRCASECMP("FONT_CHARSET", name))
			fe_ReadFontCharSet(value);
		else if (!XP_STRCASECMP("FONT_SPEC", name))
			fe_ReadFontSpec(value);
		else if (!XP_STRCASECMP("CITATION_FONT", name))
			prefs->citation_font = (MSG_FONT) atoi (value);
		else if (!XP_STRCASECMP("CITATION_SIZE", name))
			prefs->citation_size = (MSG_CITATION_SIZE) atoi (value);
		else if (!XP_STRCASECMP("CITATION_COLOR", name))
			StrAllocCopy (prefs->citation_color, value);

		/* PREFERED LANGUAGE/REGION
		 */
		else if (!XP_STRCASECMP("PREFERED_LANG_REGIONS", name))
			StrAllocCopy (prefs->lang_regions, value);

		/* HELPERS
		 */
		else if (!XP_STRCASECMP("MIME_TYPES", name))
			StrAllocCopy (prefs->global_mime_types_file, value);
		else if (!XP_STRCASECMP("PERSONAL_MIME_TYPES", name))
			StrAllocCopy (prefs->private_mime_types_file, value);
		else if (!XP_STRCASECMP("MAILCAP", name))
			StrAllocCopy (prefs->global_mailcap_file, value);
		else if (!XP_STRCASECMP("PERSONAL_MAILCAP", name))
			StrAllocCopy (prefs->private_mailcap_file, value);

		/* IDENTITY
		 */
		else if (!XP_STRCASECMP("REAL_NAME", name))
			StrAllocCopy (prefs->real_name, value);
		else if (!XP_STRCASECMP("EMAIL_ADDRESS", name))
			StrAllocCopy (prefs->email_address, value);
		else if (!XP_STRCASECMP("ORGANIZATION", name))
			StrAllocCopy (prefs->organization, value);
		else if (!XP_STRCASECMP("SIGNATURE_FILE", name))
			StrAllocCopy (prefs->signature_file, value);
		else if (!XP_STRCASECMP("SIGNATURE_DATE", name))
			prefs->signature_date = (time_t) atol (value);

		/* IMAGES
       */
		else if (!XP_STRCASECMP("DITHER_IMAGES", name))
			StrAllocCopy (prefs->dither_images, value);
		else if (!XP_STRCASECMP("STREAMING_IMAGES", name))
			prefs->streaming_images = BOOLP (value);

      /* MAIL
       */
		else if (!XP_STRCASECMP("MAILHOST", name))
			StrAllocCopy (prefs->mailhost, value);
		else if (!XP_STRCASECMP("MAIL_DIR", name))
			/*####*/	StrAllocCopy (prefs->mail_directory, value);
		else if (!XP_STRCASECMP("BIFF_INTERVAL", name))
			prefs->biff_interval = atol(value);
		else if (!XP_STRCASECMP("AUTO_CHECK_MAIL", name))
			prefs->auto_check_mail = BOOLP (value);
		else if (!XP_STRCASECMP("USE_MOVEMAIL", name)) {
			/* 3.0 prefs */
			prefs->use_movemail_p = BOOLP (value);
		}
		else if (!XP_STRCASECMP("BUILTIN_MOVEMAIL", name))
			prefs->builtin_movemail_p = BOOLP (value);
		else if (!XP_STRCASECMP("MOVEMAIL_PROGRAM", name))
			StrAllocCopy (prefs->movemail_program, value);
		/* else if (!XP_STRCASECMP("POP3_HOST", name)) */
        /* StrAllocCopy (prefs->pop3_host, value); */
        /*		else if (!XP_STRCASECMP("POP3_USER_ID", name)) */
        /* StrAllocCopy (prefs->pop3_user_id, value); */
		else if (!XP_STRCASECMP("POP3_LEAVE_ON_SERVER", name))
			prefs->pop3_leave_mail_on_server = BOOLP (value);
		else if (!XP_STRCASECMP("POP3_MSG_SIZE_LIMIT", name))
			prefs->pop3_msg_size_limit = atol (value);

		else if (!XP_STRCASECMP("MAIL_SASH_GEOMETRY", name))
			StrAllocCopy(prefs->mail_sash_geometry, value);
		else if (!XP_STRCASECMP("AUTO_EMPTY_TRASH", name))
			prefs->emptyTrash = BOOLP (value);
		else if (!XP_STRCASECMP("REMEMBER_MAIL_PASSWORD", name))
			prefs->rememberPswd = BOOLP (value);
		else if (!XP_STRCASECMP("MAIL_PASSWORD", name))
			StrAllocCopy (prefs->pop3_password, value);
		else if (!XP_STRCASECMP("THREAD_MAIL", name))
			prefs->mail_thread_p = BOOLP (value);
		else if (!XP_STRCASECMP("MAIL_PANE_STYLE", name))
			prefs->mail_pane_style = atoi (value);
		else if (!XP_STRCASECMP("MAIL_SORT_STYLE", name))
			prefs->mail_sort_style = atol (value);
		else if (!XP_STRCASECMP("MOVEMAIL_WARN", name))
			prefs->movemail_warn = BOOLP (value);

		/* NETWORK
       */
		else if (!XP_STRCASECMP("MAX_CONNECTIONS", name))
			prefs->max_connections = atoi (value);
		else if (!XP_STRCASECMP("SOCKET_BUFFER_SIZE", name))
			/* The unit for tcp buffer size in 3.0 prefs is kbypes
			 * The unit for tcp buffer size in 4.0 prefs is bytes
			 */
			prefs->network_buffer_size = atoi (value) * 1024;

      /* PROTOCOLS
       */
		else if (!XP_STRCASECMP("USE_EMAIL_FOR_ANON_FTP", name))
			prefs->email_anonftp = BOOLP(value);
		else if (!XP_STRCASECMP("ALERT_EMAIL_FORM_SUBMIT", name))
			prefs->email_submit = BOOLP(value);
		else if (!XP_STRCASECMP("ACCEPT_COOKIE", name)) {
			if (atoi(value) == 0) {
				/* do not show an alert before accepting a cookie */
				prefs->accept_cookie = NET_Accept;
				prefs->warn_accept_cookie = FALSE;
			}
			else {
				/* show an alert before accepting a cookie */
				prefs->accept_cookie = NET_Accept;
				prefs->warn_accept_cookie = TRUE;
			}
		}

      /* NEWS
       */
		else if (!XP_STRCASECMP("NNTPSERVER", name))
			StrAllocCopy (prefs->newshost, value);
		else if (!XP_STRCASECMP("NEWSRC_DIR", name))
			StrAllocCopy (prefs->newsrc_directory, value);
		else if (!XP_STRCASECMP("NEWS_MAX_ARTICLES", name))
			prefs->news_max_articles = atol (value);
		else if (!XP_STRCASECMP("NEWS_CACHE_HEADERS", name))
			prefs->news_cache_xover = BOOLP (value);
		else if (!XP_STRCASECMP("SHOW_FIRST_UNREAD", name))
			prefs->show_first_unread_p = BOOLP (value);
		else if (!XP_STRCASECMP("NEWS_SASH_GEOMETRY", name))
			StrAllocCopy(prefs->news_sash_geometry, value);
		else if (!XP_STRCASECMP("THREAD_NEWS", name))
			prefs->news_thread_p = BOOLP (value);
		else if (!XP_STRCASECMP("NEWS_PANE_STYLE", name))
			prefs->news_pane_style = atoi (value);
		else if (!XP_STRCASECMP("NEWS_SORT_STYLE", name))
			prefs->news_sort_style = atol (value);

      /* PROXIES
       */
		else if (!XP_STRCASECMP("PROXY_URL", name))
			StrAllocCopy (prefs->proxy_url, value);
		else if (!XP_STRCASECMP("PROXY_MODE", name))
			prefs->proxy_mode = atol (value);
		else if (!XP_STRCASECMP("SOCKS_HOST", name))
			StrAllocCopy (prefs->socks_host, value);
		else if (!XP_STRCASECMP("FTP_PROXY", name))
			StrAllocCopy (prefs->ftp_proxy, value);
		else if (!XP_STRCASECMP("HTTP_PROXY", name))
			StrAllocCopy (prefs->http_proxy, value);
#ifndef NO_SECURITY
		else if (!XP_STRCASECMP("HTTPS_PROXY", name))
			StrAllocCopy (prefs->https_proxy, value);
#endif
		else if (!XP_STRCASECMP("GOPHER_PROXY", name))
			StrAllocCopy (prefs->gopher_proxy, value);
		else if (!XP_STRCASECMP("WAIS_PROXY", name))
			StrAllocCopy (prefs->wais_proxy, value);
		else if (!XP_STRCASECMP("NO_PROXY", name))
			StrAllocCopy (prefs->no_proxy, value);

      /* SECURITY
       */
#ifndef NO_SECURITY
		else if (!XP_STRCASECMP("ASK_PASSWORD", name))
			prefs->ask_password = atoi (value);
		else if (!XP_STRCASECMP("PASSWORD_TIMEOUT", name))
			prefs->password_timeout = atoi (value);
#ifdef FORTEZZA
		else if (!XP_STRCASECMP("FORTEZZA_TIMEOUT_ON", name))
			prefs->fortezza_toggle = atoi (value);
		else if (!XP_STRCASECMP("FORTEZZA_TIMEOUT", name))
			prefs->fortezza_timeout = atoi (value);
#endif

		else if (!XP_STRCASECMP("WARN_ENTER_SECURE", name))
			prefs->enter_warn = BOOLP (value);
		else if (!XP_STRCASECMP("WARN_LEAVE_SECURE", name))
			prefs->leave_warn = BOOLP (value);
		else if (!XP_STRCASECMP("WARN_MIXED_SECURE", name))
			prefs->mixed_warn = BOOLP (value);
		else if (!XP_STRCASECMP("WARN_SUBMIT_INSECURE", name))
			prefs->submit_warn = BOOLP (value);
#ifdef JAVA
		else if (!XP_STRCASECMP("DISABLE_JAVA", name))
			prefs->enable_java = !BOOLP (value);
#endif
		else if (!XP_STRCASECMP("DISABLE_JAVASCRIPT", name))
			prefs->enable_javascript = !BOOLP (value);

		else if (!XP_STRCASECMP("DEFAULT_USER_CERT", name))
			StrAllocCopy (prefs->def_user_cert, value);
#endif /* ! NO_SECURITY */
		else if (!XP_STRCASECMP("ENABLE_SSL2", name))
			prefs->ssl2_enable = BOOLP (value);
		else if (!XP_STRCASECMP("ENABLE_SSL3", name))
			prefs->ssl3_enable = BOOLP (value);
		else if (!XP_STRCASECMP("CIPHER", name))
			StrAllocCopy (prefs->cipher, value);
		else if (!XP_STRCASECMP("LICENSE_ACCEPTED", name))
			StrAllocCopy (prefs->license_accepted, value);

      /* STYLES (GENERAL APPEARANCE)
       */
		else if (!XP_STRCASECMP("TOOLBAR_ICONS", name))
			prefs->toolbar_icons_p = BOOLP (value);
		else if (!XP_STRCASECMP("TOOLBAR_TEXT", name))
			prefs->toolbar_text_p = BOOLP (value);
		else if (!XP_STRCASECMP("TOOLBAR_TIPS", name))
			prefs->toolbar_tips_p = BOOLP (value);
		else if (!XP_STRCASECMP("STARTUP_MODE", name)) 
			prefs->startup_mode = atoi (value); 
		else if (!XP_STRCASECMP("HOME_DOCUMENT", name))
			StrAllocCopy (prefs->home_document, value);
		else if (!XP_STRCASECMP("UNDERLINE_LINKS", name))
			prefs->underline_links_p = BOOLP (value);
		else if (!XP_STRCASECMP("HISTORY_EXPIRATION", name))
			prefs->global_history_expiration = atoi (value);
		else if (!XP_STRCASECMP("FIXED_MESSAGE_FONT", name))
			prefs->fixed_message_font_p = BOOLP (value);
		/*** no longer valid. we need one each for mail and news - dp
		  else if (!XP_STRCASECMP("SORT_MESSAGES", name))
		  prefs->msg_sort_style = atol (value);
		  else if (!XP_STRCASECMP("THREAD_MESSAGES", name))
		  prefs->msg_thread_p = BOOLP (value);
***/

      /* BOOKMARK
       */

		/* PRINT
       */
		else if (!XP_STRCASECMP("PRINT_COMMAND", name))
			StrAllocCopy (prefs->print_command, value);
		else if (!XP_STRCASECMP("PRINT_REVERSED", name))
			prefs->print_reversed = BOOLP (value);
		else if (!XP_STRCASECMP("PRINT_COLOR", name))
			prefs->print_color = BOOLP (value);
		else if (!XP_STRCASECMP("PRINT_LANDSCAPE", name))
			prefs->print_landscape = BOOLP (value);
		else if (!XP_STRCASECMP("PRINT_PAPER", name))
			prefs->print_paper_size = atoi (value);

#ifdef EDITOR
		/* EDITOR */
		else if (!XP_STRCASECMP("EDITOR_CHARACTER_TOOLBAR", name))
			prefs->editor_character_toolbar = BOOLP(value);
		else if (!XP_STRCASECMP("EDITOR_PARAGRAPH_TOOLBAR", name))
			prefs->editor_paragraph_toolbar = BOOLP(value);
		else if (!XP_STRCASECMP("EDITOR_AUTHOR_NAME", name))
			StrAllocCopy (prefs->editor_author_name, value);
		else if (!XP_STRCASECMP("EDITOR_HTML_EDITOR", name))
			StrAllocCopy (prefs->editor_html_editor, value);
		else if (!XP_STRCASECMP("EDITOR_IMAGE_EDITOR", name))
			StrAllocCopy (prefs->editor_image_editor, value);
		else if (!XP_STRCASECMP("EDITOR_NEW_DOCUMENT_TEMPLATE", name))
			StrAllocCopy (prefs->editor_document_template, value);
		else if (!XP_STRCASECMP("EDITOR_AUTOSAVE_PERIOD", name))
			prefs->editor_autosave_period = atoi(value);
		else if (!XP_STRCASECMP("EDITOR_CUSTOM_COLORS", name))
			prefs->editor_custom_colors = BOOLP(value);
		else if (!XP_STRCASECMP("EDITOR_BACKGROUND_COLOR", name))
			prefs_parse_color(&prefs->editor_background_color, value);
		else if (!XP_STRCASECMP("EDITOR_NORMAL_COLOR", name))
			prefs_parse_color(&prefs->editor_normal_color, value);
		else if (!XP_STRCASECMP("EDITOR_LINK_COLOR", name))
			prefs_parse_color(&prefs->editor_link_color, value);
		else if (!XP_STRCASECMP("EDITOR_ACTIVE_COLOR", name))
			prefs_parse_color(&prefs->editor_active_color, value);
		else if (!XP_STRCASECMP("EDITOR_FOLLOWED_COLOR", name))
			prefs_parse_color(&prefs->editor_followed_color, value);
		else if (!XP_STRCASECMP("EDITOR_BACKGROUND_IMAGE", name))
			StrAllocCopy (prefs->editor_background_image, value);
		else if (!XP_STRCASECMP("EDITOR_MAINTAIN_LINKS", name))
			prefs->editor_maintain_links = BOOLP(value);
		else if (!XP_STRCASECMP("EDITOR_KEEP_IMAGES", name))
			prefs->editor_keep_images = BOOLP(value);
		else if (!XP_STRCASECMP("EDITOR_PUBLISH_LOCATION", name))
			StrAllocCopy (prefs->editor_publish_location, value);
		else if (!XP_STRCASECMP("EDITOR_BROWSE_LOCATION", name))
			StrAllocCopy (prefs->editor_browse_location, value);
		else if (!XP_STRCASECMP("EDITOR_PUBLISH_USERNAME", name))
			StrAllocCopy (prefs->editor_publish_username, value);
		else if (!XP_STRCASECMP("EDITOR_PUBLISH_PASSWORD", name))
			StrAllocCopy (prefs->editor_publish_password, value);
		else if (!XP_STRCASECMP("EDITOR_COPYRIGHT_HINT", name))
			prefs->editor_copyright_hint = BOOLP(value);
		/* to to add publish stuff */
#endif /*EDITOR*/
    }

	fclose (fp);

	prefs->pop3_msg_size_limit_p = (prefs->pop3_msg_size_limit > 0) ? True : False;
	prefs->pop3_msg_size_limit = abs (prefs->pop3_msg_size_limit);

	/* change default proxy mode to 3 */
	if (prefs->proxy_mode == 0) prefs->proxy_mode = PROXY_STYLE_NONE;

	/* Include the current version number in global prefs when upgrading from 3.0 to 4.0 */
	StrAllocCopy(prefs->version_number, PREFS_CURRENT_VERSION);

	/* toolbar icons/text */

	if (prefs->toolbar_icons_p && prefs->toolbar_text_p) {
		prefs->toolbar_style = BROWSER_TOOLBAR_ICONS_AND_TEXT;
	}
	else if (prefs->toolbar_icons_p) { 
		prefs->toolbar_style = BROWSER_TOOLBAR_ICONS_ONLY;
	}		
	else {
		prefs->toolbar_style = BROWSER_TOOLBAR_TEXT_ONLY;
	}

	/* movemail -> mail server type */

	if (prefs->use_movemail_p)
		prefs->mail_server_type = MAIL_SERVER_MOVEMAIL;
	else
		prefs->mail_server_type = MAIL_SERVER_POP3;

	/* start up mode */

	prefs->startup_browser_p = 
		prefs->startup_mail_p = 
		prefs->startup_news_p = 
		prefs->startup_editor_p = 
		prefs->startup_conference_p = 
		prefs->startup_netcaster_p = 
		prefs->startup_calendar_p = False;
	if (prefs->startup_mode == 0)
		prefs->startup_browser_p = True;
	else if (prefs->startup_mode == 1)
		prefs->startup_mail_p = True;
	else if (prefs->startup_mode == 2)
		prefs->startup_news_p = True;

	/* home document */

	if (prefs->home_document &&  *prefs->home_document)
		prefs->browser_startup_page = BROWSER_STARTUP_HOME;
	else
		prefs->browser_startup_page = BROWSER_STARTUP_BLANK;

	/* link expiration
	 * In 3.0 you can make link never expire, i.e. global_history_expiration < 0
	 * For 4.0, need to convert it to some large number because there is
	 * no option for never expire
	 */

	if (prefs->global_history_expiration < 0) 
		prefs->global_history_expiration = LINK_NEVER_EXPIRE_DAYS;

	if (!prefs->dither_images ||
		!XP_STRCASECMP(prefs->dither_images, "TRUE") ||
		!XP_STRCASECMP(prefs->dither_images, "FALSE"))
		StrAllocCopy (prefs->dither_images, "Auto");  {
		char *home = getenv ("HOME");
		if (!home) home = "";

#define FROB(SLOT)							\
    if (prefs->SLOT && *prefs->SLOT == '~')				\
    {									\
	if (prefs->SLOT[1] != '/' && prefs->SLOT[1] != 0)		\
	  {								\
	    fprintf (stderr,	XP_GetString( XFE_TILDE_USER_SYNTAX_DISALLOWED ), \
		     XP_AppName);					\
	  }								\
	else								\
	  {								\
	    char *new = (char *)					\
	      malloc (strlen (prefs->SLOT) + strlen (home) + 3);	\
	    strcpy (new, home);						\
	    if (home[strlen(home)-1] != '/')				\
	      strcat (new, "/");					\
	    if (prefs->SLOT[1] && prefs->SLOT[2])			\
	      strcat (new, prefs->SLOT + 2);				\
	    free (prefs->SLOT);						\
	    prefs->SLOT = new;						\
	  }								\
    }

    FROB(cache_dir)
    FROB(mail_fcc)
    FROB(news_fcc)
    FROB(tmp_dir)
    FROB(bookmark_file)
    FROB(history_file)
    FROB(user_history_file)
    FROB(global_mime_types_file)
    FROB(private_mime_types_file)
    /* spider begin */
    FROB(sar_cache_dir)
    /* spider end */
    FROB(global_mailcap_file)
    FROB(private_mailcap_file)
    FROB(signature_file)
    FROB(mail_directory)
    FROB(movemail_program)
    FROB(newsrc_directory)

#ifdef EDITOR
    FROB(editor_html_editor);
    FROB(editor_image_editor);
    FROB(editor_background_image);
#endif
	}

	return (TRUE);
}

/*
 * tweaks
 * Some tweaking...
 */
static void
tweaks(XFE_GlobalPrefs* prefs)
{
	/* movemail */
	if ((prefs->mail_server_type == MAIL_SERVER_IMAP) ||
		(prefs->mail_server_type == MAIL_SERVER_INBOX) ||
		(prefs->mail_server_type == MAIL_SERVER_POP3))
		prefs->use_movemail_p = FALSE;

	/* toolbar style */
	if (prefs->toolbar_style == BROWSER_TOOLBAR_ICONS_ONLY) {
		prefs->toolbar_icons_p = True;
		prefs->toolbar_text_p = False;
	}
	else if (prefs->toolbar_style == BROWSER_TOOLBAR_TEXT_ONLY) {
		prefs->toolbar_icons_p = False;
		prefs->toolbar_text_p = True;
	}
	else if (prefs->toolbar_style == BROWSER_TOOLBAR_ICONS_AND_TEXT) {
		prefs->toolbar_icons_p = True;
		prefs->toolbar_text_p = True;
	}
}


/*
 * pref_callback
 */
static int
pref_callback(const char* name, void* data)
{
    int i = (int) data;
    void* field = GET_FIELD(pref_map[i], (&fe_globalPrefs));

    def = 0; /* Indicate that we want to read real prefs, not defaults */
    pref_map[i].read_routine(pref_map[i].name, field);

    return 0;
}


/*
 * FE_register_pref_callbacks
 */
void
FE_register_pref_callbacks(void)
{
    int i;

    for ( i = 0; i < num_prefs; i++ ) {
        PREF_RegisterCallback(pref_map[i].name, pref_callback, (void*) i);
    }
}

/* compares two version numbers.
 * returns > 0  if version1 > version2
 *         = 0 if version1 == version2
 *         < 0 if version1 < version2
 */
static Bool compare_version(char *version_str_1, 
							char *version_str_2) 
{
	/* The prefs version number is something like 4.0, 4.1...
	 * For now, we can get away with using strcmp...
	 */

	char *version_1 = (XP_STRLEN(version_str_1)) == 0 ? PREFS_INIT_VERSION : version_str_1;
	char *version_2 = (XP_STRLEN(version_str_2)) == 0 ? PREFS_INIT_VERSION : version_str_2;

	return XP_STRCMP(version_1, version_2);
}

/* Returns true if we have upgraded the prefs */
void fe_upgrade_prefs(XFE_GlobalPrefs* prefs) 
{
	/* See if we need to upgrade the user */
	if (compare_version(fe_globalPrefs.version_number, PREFS_CURRENT_VERSION) < 0) {
		/* Here's the place to do upgrades... */

		/* Change version number */
		XP_FREEIF(prefs->version_number);
		StrAllocCopy(prefs->version_number, PREFS_CURRENT_VERSION);
	}
}

/* Checks if we need to upgrade prefs. prefs->prefs_need_upgrade will be updated by
 * this routine.
 */
void fe_check_prefs_version(XFE_GlobalPrefs* prefs) 
{
	/* See if we need to upgrade the user */
 
	prefs->prefs_need_upgrade = compare_version(PREFS_CURRENT_VERSION, prefs->version_number);
}

Bool fe_CheckVersionAndSavePrefs(char            *filename,
								 XFE_GlobalPrefs *prefs) 
{
	if (prefs->prefs_need_upgrade > 0) {
		Bool result;
		Bool confirm;
		char *msg = PR_smprintf(XP_GetString(XFE_PREFS_UPGRADE),
								prefs->version_number,
								PREFS_CURRENT_VERSION);
	    confirm = fe_Confirm_2(FE_GetToplevelWidget(), msg);
		if (confirm) fe_upgrade_prefs(prefs);
		result = XFE_SavePrefs ((char *) fe_globalData.user_prefs_file,	prefs);
		if (result && confirm) prefs->prefs_need_upgrade = 0;
		XP_FREE(msg);
		return result;
	}
	else {
		return (XFE_SavePrefs ((char *) fe_globalData.user_prefs_file, prefs));
	}
}

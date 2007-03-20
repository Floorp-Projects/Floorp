/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Mosedale <dan.mosedale@oracle.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// this is a hidden preference setting, see bugzilla bug 80035
// (http://bugzilla.mozilla.org/show_bug.cgi?id=80035)
//
// the default value for this setting is true which means when migrating from
// Netscape 4.x, mozilla will copy all the contents of Local Folders and Imap
// Folder to the newly created subfolders of migrated mozilla profile
// when this value is set to false, mozilla will not copy these contents and
// still share them with Netscape 4.x
//
// Advantages of forbidding copy operation:
//     reduce the disk usage
//     quick migration
// Disadvantage of forbidding copy operation:
//     without perfect lock mechamism, there is possibility of data corruption
//     when Netscape 4.x and mozilla run at the same time and access the same
//     mail file at the same time
pref("mail.migration.copyMailFiles", true);

//mailnews.timeline_is_enabled should be set to true ONLY for perf measurement-timeline builds.
pref("mailnews.timeline_is_enabled", false);

pref("mailnews.logComposePerformance", false);

pref("mail.wrap_long_lines",                true);
pref("news.wrap_long_lines",                true);
pref("mail.inline_attachments",             true);
pref("mailnews.auto_unzip_saved_attachments", false);

// hidden pref for controlling if the user agent string
// is displayed in the message pane or not...
pref("mailnews.headers.showUserAgent",       false);

// hidden pref for controlling if the organization string
// is displayed in the message pane or not...
pref("mailnews.headers.showOrganization",    false);

// space-delimited list of extra headers to show in msg header display area.
pref("mailnews.headers.extraExpandedHeaders", "");

// default sort order settings (when creating new folder views)
// sort_order is an int value reflecting nsMsgViewSortOrder values
//   as defined in nsIMsgDBView.idl (ascending = 1, descending = 2)
// sort_type is an int value reflecting nsMsgViewSortType values
//   as defined in nsIMsgDBView.idl (byDate = 18, byId = 21 etc.)
pref("mailnews.default_sort_order", 1); // for Mail/RSS/... (nsMsgDatabase)
pref("mailnews.default_sort_type", 18); //
pref("mailnews.default_news_sort_order", 1); // for News (nsNewsDatabase)
pref("mailnews.default_news_sort_type", 21); //

// default view flags for new folders
// both flags are int values reflecting nsMsgViewFlagsType values
// as defined in nsIMsgDBView.idl (kNone = 0, kThreadedDisplay = 1 etc.)
pref("mailnews.default_view_flags", 0); // for Mail/RSS/... (nsMsgDatabase)
pref("mailnews.default_news_view_flags", 1); // for News (nsNewsDatabase)

// If true, delete will use the direction of the sort order
// in determining the next message to select. 
pref("mail.delete_matches_sort_order", false);

// mailnews tcp read+write timeout in seconds.
pref("mailnews.tcptimeout", 60);

pref("mailnews.headers.showSender", false);

// Mail server preferences, pop by default
pref("mail.server_type",	0); 	// 0 pop, 1 imap,
					// (Unix only:)
					// 2 movemail, 3 inbox          
pref("mail.auth_login", true);

pref("mail.default_drafts", "");    // empty string use default Drafts name;
pref("mail.default_templates", ""); // empty string use default Templates name

// check all folders for new mail
pref("mail.check_all_imap_folders_for_new", false);

pref("mail.imap.server_sub_directory",      "");
pref("mail.imap.max_cached_connections",    10);
pref("mail.imap.fetch_by_chunks",           true);
pref("mail.imap.chunk_size",                10240);
pref("mail.imap.min_chunk_size_threshold",  15360);
pref("mail.imap.max_chunk_size",            40960);
pref("mail.imap.chunk_fast",                2);
pref("mail.imap.chunk_ideal",               4);
pref("mail.imap.chunk_add",                 2048);
pref("mail.imap.hide_other_users",          false);
pref("mail.imap.hide_unused_namespaces",    true);
pref("mail.imap.new_mail_get_headers",      true);
pref("mail.imap.auto_unsubscribe_from_noselect_folders",    true);
pref("mail.imap.cleanup_inbox_on_exit",     false);
pref("mail.imap.mime_parts_on_demand",      true);
pref("mail.imap.mime_parts_on_demand_max_depth", 15);
pref("mail.imap.mime_parts_on_demand_threshold", 30000);
pref("mail.imap.use_literal_plus",          true);
pref("mail.imap.expunge_after_delete",      false);
pref("mail.imap.check_deleted_before_expunge", false);
// if true, we assume that a user access a folder in the other users namespace
// is acting as a delegate for that folder, and wishes to use the other users
// identity when acting on messages in other users folders.
pref("mail.imap.delegateOtherUsersFolders", false);
pref("mail.thread_without_re",	            true);  // if false, only thread by subject if Re:
pref("mail.strict_threading",               false); // if true, don't thread by suject at all
pref("mail.leave_on_server",                false);
pref("mail.default_cc",                     "");
pref("mail.default_fcc",                    ""); // maibox:URL or Imap://Host/OnLineFolderName
pref("mail.check_new_mail",                 false);
pref("mail.pop3_gets_new_mail",             false);
pref("mail.check_time",                     10);
pref("mail.pop_name",                       "");
pref("mail.pop3.deleteFromServerOnMove",    false);
pref("mail.remember_password",              false);
pref("mail.pop_password",                   "");
pref("mail.fixed_width_messages",           true);
pref("mail.citation_color",                 "#000000"); // quoted color
pref("mail.quoted_style",                   0); // 0=plain, 1=bold, 2=italic, 3=bolditalic
pref("mail.quoted_size",                    0); // 0=normal, 1=big, 2=small
pref("mail.quoted_graphical",               true); // use HTML-style quoting for displaying plain text
pref("mail.quoteasblock",                   true); // use HTML-style quoting for quoting plain text
pref("mail.identity.organization",          "");
pref("mail.identity.reply_to",              "");
pref("mail.identity.username",              "");
pref("mail.identity.useremail",             "");
pref("mail.use_fcc",                        true);
pref("mail.cc_self",                        false);
pref("mail.strictly_mime",                  false);
pref("mail.strictly_mime_headers",          true);
// 0/1 (RFC 2047), 2(RFC 2231), 3/4(RFC 2231, but name param is encoded by RFC 2047)
// 0/3 the name param is never separated to multiple lines.
pref("mail.strictly_mime.parm_folding",     3);  
pref("mail.label_ascii_only_mail_as_us_ascii", false); 
pref("mail.file_attach_binary",             false);
pref("mail.show_headers",                   1); // some
pref("mail.pane_config",                    0);
pref("mail.addr_book.mapit_url.format", "chrome://messenger-region/locale/region.properties");
#ifdef MOZ_SUITE
pref("mailnews.start_page.url", "chrome://messenger-region/locale/region.properties");
pref("mail.addr_book.im.onlineCheckAllowed", false);
pref("messenger.throbber.url","chrome://messenger-region/locale/region.properties");
pref("compose.throbber.url","chrome://messenger-region/locale/region.properties");
pref("addressbook.throbber.url","chrome://messenger-region/locale/region.properties");
pref("mail.accountwizard.deferstorage", false);
#endif

// the format for "mail.addr_book.quicksearchquery.format" is:
// @V == the escaped value typed in the quick search bar in the addressbook
//
// note, changing this might require a change to SearchNameOrEmail.label
// in messenger.dtd
pref("mail.addr_book.quicksearchquery.format", "chrome://messenger/locale/messenger.properties");

// values for "mail.addr_book.lastnamefirst" are:
//0=displayname, 1=lastname first, 2=firstname first
pref("mail.addr_book.lastnamefirst", 0); 
pref("mail.addr_book.displayName.autoGeneration", true);
pref("mail.addr_book.displayName.lastnamefirst", "chrome://messenger/locale/messenger.properties");
pref("mail.addr_book.show_phonetic_fields", "chrome://messenger/locale/messenger.properties");
pref("mail.attach_vcard",                   false);
pref("mail.html_compose",                   true);
// you can specify multiple, option headers
// this will show up in the address picker in the compose window
// examples: "X-Face" or "Approved,X-No-Archive"
pref("mail.compose.other.header",	    "");
pref("mail.compose.autosave", true);
pref("mail.compose.autosaveinterval", 5); // in minutes
pref("mail.fcc_folder",                     "");
pref("mail.encrypt_outgoing_mail",          false);
pref("mail.crypto_sign_outgoing_mail",      false);
pref("mail.default_html_action", 0);          // 0=ask, 1=plain, 2=html, 3=both
pref("mail.smtp.ssl",0);                      // 0 = no, 1 = try, 2 = must use SSL

pref("mail.mdn.report.not_in_to_cc", 2);               // 0: Never 1: Always 2: Ask me
pref("mail.mdn.report.outside_domain", 2);             // 0: Never 1: Always 2: Ask me
pref("mail.mdn.report.other", 2);                      // 0: Never 1: Always 2: Ask me 3: Denial

pref("mail.incorporate.return_receipt", 0);            // 0: Inbox/filter 1: Sent folder
pref("mail.request.return_receipt", 2);                // 1: DSN 2: MDN 3: Both
pref("mail.receipt.request_header_type", 0);           // 0: MDN-DNT header  1: RRT header 2: Both (MC)
pref("mail.receipt.request_return_receipt_on", false);
pref("mail.mdn.report.enabled", true);                 // false: Never send true: Send sometimes

pref("mail.showPreviewText", false);

pref("news.default_cc",                     "");
pref("news.default_fcc",                    ""); // mailbox:URL or Imap://Host/OnlineFolderName
pref("news.use_fcc",                        true);
pref("news.cc_self",                        false);
pref("news.fcc_folder",                     "");
pref("news.notify.on",                      true);
pref("news.max_articles",                   500);
pref("news.mark_old_read",                  false);
pref("news.show_size_in_lines",             true);
pref("news.update_unread_on_expand",        true);
pref("news.get_messages_on_select",         true);

pref("mailnews.wraplength",                 72);
pref("mail.compose.wrap_to_window_width",   false);

// 0=no header, 1="<author> wrote:", 2="On <date> <author> wrote:", 3="<author> wrote On <date>:", 4=user specified
pref("mailnews.reply_header_type",          1);
// locale which affects date format, set empty string to use application default locale
pref("mailnews.reply_header_locale",        "");
pref("mailnews.reply_header_authorwrote",   "chrome://messenger/locale/messengercompose/composeMsgs.properties");
pref("mailnews.reply_header_ondate",        "chrome://messenger/locale/messengercompose/composeMsgs.properties");

// separator to separate between date and author
pref("mailnews.reply_header_separator",     ", ");
pref("mailnews.reply_header_colon",         ":");
pref("mailnews.reply_header_originalmessage",   "chrome://messenger/locale/messengercompose/composeMsgs.properties");

pref("mailnews.reply_to_self_check_all_ident", false);

pref("mail.purge_threshhold",              100);
pref("mail.prompt_purge_threshhold",       false);
pref("mail.purge.ask",                     true);

pref("mailnews.offline_sync_mail",         false);
pref("mailnews.offline_sync_news",         false);
pref("mailnews.offline_sync_send_unsent",  true);
pref("mailnews.offline_sync_work_offline", false);   
pref("mailnews.force_ascii_search",        false);

pref("mailnews.send_default_charset",       "chrome://messenger/locale/messenger.properties");
pref("mailnews.view_default_charset",       "chrome://messenger/locale/messenger.properties");
pref("mailnews.force_charset_override",     false);
pref("mailnews.reply_in_default_charset",   false);

pref("mailnews.search_date_format",        "chrome://messenger/locale/messenger.properties");
pref("mailnews.search_date_separator",     "chrome://messenger/locale/messenger.properties");
pref("mailnews.search_date_leading_zeros", "chrome://messenger/locale/messenger.properties");

pref("mailnews.language_sensitive_font",    true);

pref("mailnews.quotingPrefs.version",       0);  // used to decide whether to migrate global quoting prefs

// the first time, we'll warn the user about the blind send, and they can disable the warning if they want.
pref("mapi.blind-send.enabled",             true);  

pref("offline.autoDetect",                  false); // automatically move the user offline or online based on the network connection
pref("offline.news.download.unread_only",   true);
pref("offline.news.download.by_date",       true);
pref("offline.news.download.days",          30);    // days
pref("offline.news.download.increments",    3); // 0-yesterday, 1-1 wk ago, 2-2 wk ago,
                                                // 3-1 month ago, 4-6 month ago, 5-1 year ago

pref("ldap_1.number_of_directories", 6);

pref("ldap_1.directory1.description", "Personal Address Book");
pref("ldap_1.directory1.dirType", 2);
pref("ldap_1.directory1.isOffline", false);

pref("ldap_1.directory2.description", "Four11 Directory");
pref("ldap_1.directory2.serverName", "ldap.four11.com");

pref("ldap_1.directory3.description", "InfoSpace Directory");
pref("ldap_1.directory3.serverName", "ldap.infospace.com");

pref("ldap_1.directory4.description", "WhoWhere Directory");
pref("ldap_1.directory4.serverName", "ldap.whowhere.com");

pref("ldap_1.directory5.description", "Bigfoot Directory");
pref("ldap_1.directory5.serverName", "ldap.bigfoot.com");

pref("ldap_1.directory6.description", "Switchboard Directory");
pref("ldap_1.directory6.serverName", "ldap.switchboard.com");
pref("ldap_1.directory6.searchBase", "c=US");
pref("ldap_1.directory6.attributes.telephoneNumber", "Phone Number:homephone");
pref("ldap_1.directory6.attributes.street", "State:st");
pref("ldap_1.directory6.filter1.repeatFilterForWords", false);

pref("ldap_2.autoComplete.interval", 650);
pref("ldap_2.autoComplete.enabled", true);
pref("ldap_2.autoComplete.useDirectory", false);
pref("ldap_2.autoComplete.useAddressBooks", true);
pref("ldap_2.autoComplete.skipDirectoryIfLocalMatchFound", true);
pref("ldap_2.autoComplete.directoryServer", "");

pref("ldap_2.servers.pab.position",								1);
pref("ldap_2.servers.pab.description",							"chrome://messenger/locale/addressbook/addressBook.properties");
pref("ldap_2.servers.pab.dirType",								2);
pref("ldap_2.servers.pab.filename",                             "abook.mab");
pref("ldap_2.servers.pab.isOffline",							false);

pref("ldap_2.servers.history.position",							2);
pref("ldap_2.servers.history.description",						"chrome://messenger/locale/addressbook/addressBook.properties");
pref("ldap_2.servers.history.dirType",							2);
pref("ldap_2.servers.history.filename",                         "history.mab");
pref("ldap_2.servers.history.isOffline",						false);

// default mapping of addressbook properties to ldap attributes
pref("ldap_2.servers.default.attrmap.FirstName", "givenName");
pref("ldap_2.servers.default.attrmap.LastName", "sn,surname");
pref("ldap_2.servers.default.attrmap.DisplayName", "cn,commonname");
pref("ldap_2.servers.default.attrmap.NickName", "mozillaNickname,xmozillanickname");
pref("ldap_2.servers.default.attrmap.PrimaryEmail", "mail");
pref("ldap_2.servers.default.attrmap.SecondEmail", "mozillaSecondEmail,xmozillasecondemail");
pref("ldap_2.servers.default.attrmap.WorkPhone", "telephoneNumber");
pref("ldap_2.servers.default.attrmap.HomePhone", "homePhone");
pref("ldap_2.servers.default.attrmap.FaxNumber", "fax,facsimiletelephonenumber");
pref("ldap_2.servers.default.attrmap.PagerNumber", "pager,pagerphone");
pref("ldap_2.servers.default.attrmap.CellularNumber", "mobile,cellphone,carphone");
pref("ldap_2.servers.default.attrmap.WorkAddress", "street,streetaddress,postOfficeBox");
pref("ldap_2.servers.default.attrmap.HomeAddress", "mozillaHomeStreet");
pref("ldap_2.servers.default.attrmap.WorkAddress2", "mozillaWorkStreet2");
pref("ldap_2.servers.default.attrmap.HomeAddress2", "mozillaHomeStreet2");
pref("ldap_2.servers.default.attrmap.WorkCity", "l,locality");
pref("ldap_2.servers.default.attrmap.HomeCity", "mozillaHomeLocalityName");
pref("ldap_2.servers.default.attrmap.WorkState", "st,region");
pref("ldap_2.servers.default.attrmap.HomeState", "mozillaHomeState");
pref("ldap_2.servers.default.attrmap.WorkZipCode", "postalCode,zip");
pref("ldap_2.servers.default.attrmap.HomeZipCode", "mozillaHomePostalCode");
pref("ldap_2.servers.default.attrmap.WorkCountry", "c,countryname");
pref("ldap_2.servers.default.attrmap.HomeCountry", "mozillaHomeCountryName");
pref("ldap_2.servers.default.attrmap.JobTitle", "title");
pref("ldap_2.servers.default.attrmap.Department", "ou,department,departmentnumber,orgunit");
pref("ldap_2.servers.default.attrmap.Company", "o,company");
pref("ldap_2.servers.default.attrmap._AimScreenName", "nsAIMid,nscpaimscreenname");
pref("ldap_2.servers.default.attrmap.WebPage1", "mozillaWorkUrl,workurl");
pref("ldap_2.servers.default.attrmap.WebPage2", "mozillaHomeUrl,homeurl");
pref("ldap_2.servers.default.attrmap.BirthYear", "birthyear");
pref("ldap_2.servers.default.attrmap.Custom1", "mozillaCustom1,custom1");
pref("ldap_2.servers.default.attrmap.Custom2", "mozillaCustom2,custom2");
pref("ldap_2.servers.default.attrmap.Custom3", "mozillaCustom3,custom3");
pref("ldap_2.servers.default.attrmap.Custom4", "mozillaCustom4,custom4");
pref("ldap_2.servers.default.attrmap.Notes", "description,notes");
pref("ldap_2.servers.default.attrmap.PreferMailFormat", "mozillaUseHtmlMail,xmozillausehtmlmail");
pref("ldap_2.servers.default.attrmap.LastModifiedDate", "modifytimestamp");


// A position of zero is a special value that indicates the directory is deleted.
// These entries are provided to keep the (obsolete) Four11 directory and the
// WhoWhere, Bigfoot and Switchboard directories from being migrated.
pref("ldap_2.servers.four11.position",						0);
pref("ldap_2.servers.four11.description",						"Four11 Directory");
pref("ldap_2.servers.four11.serverName",						"ldap.four11.com");

pref("ldap_2.servers.whowhere.position",						0);             
pref("ldap_2.servers.whowhere.description",						"WhoWhere Directory");
pref("ldap_2.servers.whowhere.serverName",						"ldap.whowhere.com");

pref("ldap_2.servers.bigfoot.position",							0);             
pref("ldap_2.servers.bigfoot.description",						"Bigfoot Directory");
pref("ldap_2.servers.bigfoot.serverName",                       "ldap.bigfoot.com");
                                                                                 
pref("ldap_2.servers.switchboard.position",						0);             
pref("ldap_2.servers.switchboard.description",					"Switchboard Directory");
pref("ldap_2.servers.switchboard.serverName",					"ldap.switchboard.com");

pref("ldap_2.user_id",											0);
pref("ldap_2.version",											3); /* Update kCurrentListVersion in include/dirprefs.h if you change this */
pref("ldap_2.prefs_migrated",      false);

pref("mailnews.confirm.moveFoldersToTrash", true);

// space-delimited list of extra headers to add to .msf file
pref("mailnews.customDBHeaders", "");

pref("mailnews.reuse_message_window", true);
pref("mailnews.reuse_thread_window2", false);
pref("mailnews.open_window_warning", 10); // warn user if they attempt to open more than this many messages at once

pref("mailnews.start_page.enabled", true);

pref("mailnews.remember_selected_message", true);
pref("mailnews.scroll_to_new_message", true);

/* file, print, and stop hidden by default.  
   see http://bugzilla.mozilla.org/show_bug.cgi?id=197729#c3 */
pref("mail.toolbars.showbutton.file", false);
pref("mail.toolbars.showbutton.next", true);
pref("mail.toolbars.showbutton.junk", true);
pref("mail.toolbars.showbutton.print",false);
pref("mail.toolbars.showbutton.stop", false);

pref("mailnews.thread_pane_column_unthreads", true);

pref("mailnews.account_central_page.url", "chrome://messenger/locale/messenger.properties");

/* default prefs for Mozilla 5.0 */
pref("mail.identity.default.compose_html", true);
pref("mail.identity.default.valid", true);
pref("mail.identity.default.fcc",true);
pref("mail.identity.default.fcc_folder","mailbox://nobody@Local%20Folders/Sent");
pref("mail.identity.default.fcc_reply_follows_parent", false);
pref("mail.identity.default.autocompleteToMyDomain", false);

// keep these defaults for backwards compatibility and migration

// but .doBcc and .doBccList are the right ones from now on.
pref("mail.identity.default.bcc_self",false);
pref("mail.identity.default.bcc_others",false);
pref("mail.identity.default.bcc_list","");

pref("mail.identity.default.draft_folder","mailbox://nobody@Local%20Folders/Drafts");
pref("mail.identity.default.stationery_folder","mailbox://nobody@Local%20Folders/Templates");
pref("mail.identity.default.directoryServer","");
pref("mail.identity.default.overrideGlobal_Pref", false);
pref("mail.identity.default.auto_quote", true);
pref("mail.identity.default.reply_on_top", 0); // 0=bottom 1=top 2=select
pref("mail.identity.default.sig_bottom", true); // true=below quoted false=above quoted
// Headers to always add to outgoing mail
// examples: "header1,header2"
// pref("mail.identity.id1.headers", "header1");
// user_pref("mail.identity.id1.header.header1", "X-Mozilla-Rocks: True")
pref("mail.identity.default.headers", "");

// by default, only collect addresses the user sends to (outgoing)
// incoming is all spam anyways
pref("mail.collect_email_address_incoming", false);
pref("mail.collect_email_address_outgoing", true);
pref("mail.collect_email_address_newsgroup", false);

// by default, use the Personal Addressbook for collection
pref("mail.collect_addressbook","moz-abmdbdirectory://abook.mab"); // the Personal addressbook.

pref("mail.default_sendlater_uri","mailbox://nobody@Local%20Folders/Unsent%20Messages");

pref("mail.server.default.port", -1);
pref("mail.server.default.offline_support_level", -1);
pref("mail.server.default.leave_on_server", false);
pref("mail.server.default.download_on_biff", false);
pref("mail.server.default.check_time", 10);
pref("mail.server.default.delete_by_age_from_server", false);
pref("mail.server.default.num_days_to_leave_on_server", 7);
// "mail.server.default.check_new_mail" now lives in the protocol info
pref("mail.server.default.dot_fix", true);
pref("mail.server.default.limit_offline_message_size", false);
pref("mail.server.default.max_size", 50);
pref("mail.server.default.auth_login", true);
pref("mail.server.default.delete_mail_left_on_server", false);
pref("mail.server.default.valid", true);
pref("mail.server.default.abbreviate",true);
pref("mail.server.default.isSecure", false);
pref("mail.server.default.useSecAuth", false);
pref("mail.server.default.socketType", 0);
pref("mail.server.default.override_namespaces", true);
pref("mail.server.default.deferred_to_account", "");

pref("mail.server.default.delete_model", 1);
pref("mail.server.default.fetch_by_chunks", true);
pref("mail.server.default.mime_parts_on_demand", true);

pref("mail.server.default.always_authenticate",false);
pref("mail.server.default.singleSignon", true);
pref("mail.server.default.max_articles", 500);
pref("mail.server.default.notify.on", true);
pref("mail.server.default.mark_old_read", false);
pref("mail.server.default.empty_trash_on_exit", false);
// 0 = Keep Dupes, leave them alone
// 1 = delete dupes
// 2 = Move Dupes to trash
// 3 = Mark Dupes as Read
pref("mail.server.default.dup_action", 0);

pref("mail.server.default.using_subscription", true);
pref("mail.server.default.dual_use_folders", true);
pref("mail.server.default.canDelete", false);
pref("mail.server.default.login_at_startup", false);
pref("mail.server.default.allows_specialfolders_usage", true);
pref("mail.server.default.canCreateFolders", true);
pref("mail.server.default.canFileMessages", true);
pref("mail.server.default.logon_fallback", true);

// special enhancements for IMAP servers
pref("mail.server.default.store_read_mail_in_pfc", false);  
pref("mail.server.default.store_sent_mail_in_pfc", false);  
pref("mail.server.default.use_idle", true); 
// for spam
pref("mail.server.default.spamLevel",100);  // 0 off, 100 on.  not doing bool since we might have real levels one day.
pref("mail.server.default.moveOnSpam",false);
pref("mail.server.default.moveTargetMode",0); // 0 == "Junk" on server, 1 == specific folder
pref("mail.server.default.spamActionTargetAccount","");
pref("mail.server.default.spamActionTargetFolder","");
pref("mail.server.default.useWhiteList",true);
pref("mail.server.default.whiteListAbURI","moz-abmdbdirectory://abook.mab");  // the Personal addressbook.
pref("mail.server.default.useServerFilter", false);
pref("mail.server.default.serverFilterName", "SpamAssasin");
pref("mail.server.default.serverFilterTrustFlags", 1); // 1 == trust positives, 2 == trust negatives, 3 == trust both
pref("mail.server.default.purgeSpam",false);
pref("mail.server.default.purgeSpamInterval",14); // 14 days

// the probablilty threshold over which messages are classified as junk
// this number is divided by 100 before it is used. The classifier can be fine tuned
// by changing this pref. Typical values are .99, .95, .90, .5, etc. 
pref("mail.adaptivefilters.junk_threshold", 90); 
pref("mail.spam.version", 0); // used to determine when to migrate global spam settings
pref("mail.spam.logging.enabled", false);
pref("mail.spam.manualMark", false);
pref("mail.spam.markAsReadOnSpam", false);
pref("mail.spam.manualMarkMode", 0); // 0 == "move to junk folder", 1 == "delete"

pref("mail.autoComplete.highlightNonMatches", true);

// if true, we'll use the password from an incoming server with
// matching username and domain
pref("mail.smtp.useMatchingDomainServer", false);

// if true, we'll use the password from an incoming server with
// matching username and host name
pref("mail.smtp.useMatchingHostNameServer", false);

pref("mail.smtpserver.default.auth_method", 1); // auth any
pref("mail.smtpserver.default.trySecAuth", true);
pref("mail.smtpserver.default.try_ssl", 0);

// For the next 3 prefs, see <http://www.bucksch.org/1/projects/mozilla/16507>
pref("mail.display_glyph", true);   // TXT->HTML :-) etc. in viewer
pref("mail.display_struct", true);  // TXT->HTML *bold* etc. in viewer; ditto
pref("mail.send_struct", false);   // HTML->HTML *bold* etc. during Send; ditto
pref("mailnews.display.original_date", false);   // display date string from mail headers without interpreting
// For the next 4 prefs, see <http://www.bucksch.org/1/projects/mozilla/108153>
pref("mailnews.display.prefer_plaintext", false);  // Ignore HTML parts in multipart/alternative
pref("mailnews.display.html_as", 0);  // How to display HTML parts. 0 = Render the sender's HTML; 1 = HTML->TXT->HTML; 2 = Show HTML source; 3 = Sanitize HTML
pref("mailnews.display.html_sanitizer.allowed_tags", "html head title body p br div(lang,title) h1 h2 h3 h4 h5 h6 ul(type,compact) ol(type,compact,start) li(type,value) dl dt dd blockquote(type,cite) pre noscript noframes strong em sub sup span(lang,title) acronym(title) abbr(title) del(title,cite,datetime) ins(title,cite,datetime) q(cite) a(href,name,title) img(alt,title,longdesc,src) base(href) area(alt) applet(alt) object(alt) var samp dfn address kbd code cite s strike tt b i table(align) caption tr(align,valign) td(rowspan,colspan,align,valign) th(rowspan,colspan,align,valign)");
pref("mailnews.display.disallow_mime_handlers", 0);  /* Let only a few classes process incoming data. This protects from bugs (e.g. buffer overflows) and from security loopholes (e.g. allowing unchecked HTML in some obscure classes, although the user has html_as > 0).
This option is mainly for the UI of html_as.
0 = allow all available classes
1 = Use hardcoded blacklist to avoid rendering (incoming) HTML
2 = ... and inline images
3 = ... and some other uncommon content types
100 = Use hardcoded whitelist to avoid even more bugs(buffer overflows).
      This mode will limit the features available (e.g. uncommon
      attachment types and inline images) and is for paranoid users.
*/

pref("mail.forward_message_mode", 0); // 0=default as attachment 2=forward as inline with attachments, (obsolete 4.x value)1=forward as quoted (mapped to 2 in mozilla)

pref("mail.startup.enabledMailCheckOnce", false);

pref("mailnews.send_plaintext_flowed", true); // RFC 2646=======
pref("mailnews.display.disable_format_flowed_support", false);
pref("mailnews.nav_crosses_folders", 1); // prompt user when crossing folders

// these two news.cancel.* prefs are for use by QA for automated testing.  see bug #31057
pref("news.cancel.confirm",true);
pref("news.cancel.alert_on_success",true);
pref("mail.SpellCheckBeforeSend",false);
pref("mail.spellcheck.inline",true);
pref("mail.phishing.detection.enabled", true); // enable / disable phishing detection for link clicks
pref("mail.warn_on_send_accel_key", true);
pref("mail.enable_autocomplete",true);
pref("mailnews.html_domains","");
pref("mailnews.plaintext_domains","");
pref("mailnews.global_html_domains.version",1);

/////////////////////////////////////////////////////////////////
// Privacy Controls for Handling Remote Content
///////////////////////////////////////////////////////////////// 
pref("mailnews.message_display.allow.plugins", false); // disable plugins by default
pref("mailnews.message_display.disable_remote_image", true);

/////////////////////////////////////////////////////////////////
// Trusted Mail Domains
//
// Specific domains can be white listed to bypass various privacy controls in Thunderbird
// such as blocking remote images, the phishing detector, etc. This is particularly
// useful for business deployments where images or links reference servers inside a 
// corporate intranet. For multiple domains, separate them with a comma. i.e.
// pref("mail.trusteddomains", "mozilla.org,mozillafoundation.org");
///////////////////////////////////////////////////////////////// 
pref("mail.trusteddomains", "");

pref("mail.imap.use_status_for_biff", true);

pref("mail.quota.mainwindow_threshold.show", 75); // in percent. when the quota meter starts showing up at all. decrease this for it to be more than a warning.
pref("mail.quota.mainwindow_threshold.warning", 80); // when it gets yellow
pref("mail.quota.mainwindow_threshold.critical", 95); // when it gets red

// Pref controlling confirmation of folder deletion on empty trash
pref("mail.imap.confirm_emptyTrashFolderDeletion", false);
// Pref controlling the updates on the pre-configured accounts.
// In order to add new pre-configured accounts (after a version),
// increase the following version number besides updating the
// pref mail.accountmanager.appendaccounts 
pref("mailnews.append_preconfig_accounts.version",1);

// Pref controlling the updates on the pre-configured smtp servers.
// In order to add new pre-configured smtp servers (after a version),
// increase the following version number besides updating the
// pref mail.smtpservers.appendsmtpservers
pref("mail.append_preconfig_smtpservers.version",1);

pref("mail.biff.play_sound",true);
// 0 == default system sound, 1 == user specified wav
pref("mail.biff.play_sound.type",0);
// _moz_mailbeep is a magic key, for the default sound.
// otherwise, this needs to be a file url
pref("mail.biff.play_sound.url","");
pref("mail.biff.show_alert", true);
pref("mail.biff.show_tray_icon", true); // currently Windows-only
pref("mail.biff.animate_dock_icon", false);

pref("mail.content_disposition_type", 0);

pref("mailnews.show_send_progress", true); //Will show a progress dialog when saving or sending a message
pref("mail.server.default.retainBy", 1);

pref("mailnews.ui.junk.firstuse", true);
pref("mailnews.ui.junk.manualMarkAsJunkMarksRead", true);

// for manual upgrades of certain UI features.
// 1 -> 2 is for the folder pane tree landing, to hide the
// unread and total columns, see msgMail3PaneWindow.js
pref("mail.ui.folderpane.version", 1);                                          

// for manual upgrades of certain UI features.
// 1 -> 2 is for the thread pane tree landing, to hide the
// labels column, see msgMail3PaneWindow.js
// 2 -> 3 is for the junk status column
pref("mailnews.ui.threadpane.version", 1);
// for manual upgrades of certain UI features.
// 1 -> 2 is for the ab results pane tree landing
// to hide the non default columns in the addressbook dialog
// see abCommon.js and addressbook.js
pref("mailnews.ui.addressbook_results.version", 1);                                          
// for manual upgrades of certain UI features.
// 1 -> 2 is for the ab results pane tree landing
// to hide the non default columns in the addressbook sidebar panel
// see abCommon.js and addressbook-panel.js
pref("mailnews.ui.addressbook_panel_results.version", 1);                                          
// for manual upgrades of certain UI features.
// 1 -> 2 is for the ab results pane tree landing
// to hide the non default columns in the select addresses dialog
// see abCommon.js and abSelectAddressesDialog.js
pref("mailnews.ui.select_addresses_results.version", 1); 
// for manual upgrades of certain UI features.
// 1 -> 2 is for the ab results pane
// to hide the non default columns in the advanced directory search dialog
// see abCommon.js and ABSearchDialog.js
pref("mailnews.ui.advanced_directory_search_results.version", 1);                                         
//If set to a number greater than 0, msg compose windows will be recycled in order to open them quickly
pref("mail.compose.max_recycled_windows", 1); 

// default description and color prefs for tags
// (we keep the .labels. names for backwards compatibility)
pref("mailnews.labels.description.1", "chrome://messenger/locale/messenger.properties");
pref("mailnews.labels.description.2", "chrome://messenger/locale/messenger.properties");
pref("mailnews.labels.description.3", "chrome://messenger/locale/messenger.properties");
pref("mailnews.labels.description.4", "chrome://messenger/locale/messenger.properties");
pref("mailnews.labels.description.5", "chrome://messenger/locale/messenger.properties");
pref("mailnews.labels.color.1", "#FF0000"); // default: red
pref("mailnews.labels.color.2", "#FF9900"); // default: orange
pref("mailnews.labels.color.3", "#009900"); // default: green
pref("mailnews.labels.color.4", "#3333FF"); // default: blue
pref("mailnews.labels.color.5", "#993399"); // default: purple

//default null headers
//example "X-Warn: XReply", list of hdrs separated by ": "
pref("mailnews.customHeaders", ""); 

pref("mailnews.fakeaccount.show", false);
pref("mailnews.fakeaccount.server", "");

// default msg compose font prefs
pref("msgcompose.font_face",                "");
pref("msgcompose.font_size",                "medium");
pref("msgcompose.text_color",               "#000000");
pref("msgcompose.background_color",         "#FFFFFF");

// When there is no disclosed recipients (only bcc), we should address the message to empty group
// to prevent some mail server to disclose the bcc recipients
pref("mail.compose.add_undisclosed_recipients", true);

// Set this preference to true to tell mail not to attach the source of a link to a local
// network file (file://///<network name>/<path>/<file name>). Windows only
pref("mail.compose.dont_attach_source_of_local_network_links", false);

// these prefs (in minutes) are here to help QA test this feature
// "mail.purge.min_delay", never purge a junk folder more than once every 480 minutes (60 mins/hour * 8 hours)
// "mail.purge.timer_interval", fire the purge timer every 5 minutes, starting 5 minutes after we load accounts
pref("mail.purge.min_delay",480);
pref("mail.purge.timer_interval",5); 

// set to true if viewing a message should mark it as read only if the msg is viewed for a specified time interval in seconds
pref("mailnews.mark_message_read.delay", false); 
pref("mailnews.mark_message_read.delay.interval", 5); // measured in seconds

// require a password before showing imap or local headers in thread pane
pref("mail.password_protect_local_cache", false);
// to reduce forking in the js / C++
// overridden by stand alone mail
pref("mail.standalone", false);

// import option to skip the first record, recorded so that we can save
// the users last used preference.
pref("mailnews.import.text.skipfirstrecord", true);

// automatically scale attached images that are displayed inline
pref("mail.enable_automatic_image_resizing", true);

#ifdef XP_MACOSX
pref("mail.notification.sound",             "");
pref("mail.close_message_window.on_delete", true);
pref("mail.close_message_window.on_file", true);

pref("mail.server_type_on_restart",         -1);
#endif

#ifndef XP_MACOSX
#ifdef XP_UNIX
pref("mail.empty_trash", false);

pref("mail.check_new_mail", true);
pref("mail.signature_file", "~/.signature");
pref("mail.default_fcc", "~/nsmail/Sent");
pref("news.default_fcc", "~/nsmail/Sent");
pref("mailnews.reply_with_extra_lines", 0);
pref("mail.use_builtin_movemail", true);
pref("mail.movemail_program", "");
pref("mail.movemail_warn", false);
pref("mail.sash_geometry", "");
pref("news.cache_xover", false);
pref("news.show_first_unread", false);
pref("news.sash_geometry", "");
pref("mail.signature_date", 0);

# XP_UNIX
#endif
#endif

#ifdef XP_OS2
pref("mail.compose.max_recycled_windows", 0);
#endif

// For the Empty Junk confirmation dialog
pref("mailnews.emptyJunk.dontAskAgain", false);

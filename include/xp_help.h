/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* xp_help.h */


/*
 *                                                                  *
 * Revision history:                                                *
 *   Author: Edwin Aoki                                             *
 *   Extensive Revision: Kevin Driscoll 3/19/97                     *
 *   Updated ONLY Mail/News entries: Kevin Driscoll 3/21/97         *
 *   Updated to add discussion IDs: Kevin Driscoll 4/22/97          *
 *   Added missing/modified IDs:  Gina Cariga 4/22/97               *
 *   Added offline & help_edit_dict IDs: Kevin Driscoll 4/28/97     *
 *   Added 2 HELP_HTML_MAIL IDs: Kevin Driscoll 4/29/97             *
 *   Fixed 5 wrong component names in helpIDs: Kevin Driscoll 5/7/97*
 *   Fixed 1 and added 1 ID: Kevin Driscoll 5/9/97                  *
 *   Corrected 7 helpside IDs as per Melton: Kevin Driscoll 5/12/97 *
 *   Added 1 new ID for conference: Kevin Driscoll 5/12/97          *
 *   Corrected 2 IDs for offine download: Kevin Driscoll 5/21/97    *
 *******************************************************************/

#ifndef XP_HELP_H
#define XP_HELP_H


/* The main entry point into help for most folks.  This function takes a string
   which represents the component and topic name for a Communicator help topic.
   It prepends the netscape vendor to create a fully-qualified topic name.  If
   an MWContext which represents the current window is available, pass that in,
   otherwise, specify NULL and NetHelp will locate an appropriate context to use.
   In either event, XP_NetHelp then calls NET_LoadNetHelpTopic, below. */

extern void
XP_NetHelp(MWContext *pContext, const char *topic);


/* Called by FEs to load a fully-qualified topic.  This function is implemented in
   libnet/mkhelp.c, but it's extracted here so unrelated FE parts don't have
   to include mkhelp.h */

extern void
NET_LoadNetHelpTopic(MWContext *pContext, const char *topic);


/* These defines correspond to help tags that can be passed to
   XP_NetHelp, above, to invoke nethelp. */


/* Main product help */

#define	HELP_COMMUNICATOR			"home:start_here"

/* Address Book Dialogs */

#define	HELP_ADDRESS_BOOK				"messengr:ADDRESS_BOOK"
#define	HELP_ADD_LIST_MAILING_LIST		"messengr:ADD_LIST_MAILING_LIST"
#define	HELP_ADD_USER_NETSCAPE_COOLTALK	"messengr:ADD_USER_NETSCAPE_COOLTALK"
#define	HELP_ADD_USER_PROPS			"messengr:ADD_USER_PROPERTIES"
#define	HELP_ADD_USER_SECURITY			"messengr:ADD_USER_SECURITY"
#define	HELP_ADD_USER_CONTACT			"messengr:ADD_USER_CONTACT"
#define	HELP_ADD_USER_NETSCAPE_COOLTALK	"messengr:ADD_USER_NETSCAPE_COOLTALK"
#define	HELP_LDAP_SERVER_PROPS			"messengr:LDAP_SERVER_PROPERTIES"
#define	HELP_EDIT_USER_CALLPOINT		"messengr:ADD_USER_NETSCAPE_COOLTALK"
#define	HELP_EDIT_USER_CONTACT			"messengr:ADD_USER_CONTACT"
#define	HELP_EDIT_USER				"messengr:ADD_USER_PROPERTIES"
#define	HELP_EDIT_USER_SECURITY			"messengr:EDIT_USER_SECURITY"
#define	HELP_MAIL_LIST_PROPS			"messengr:ADD_LIST_MAILING_LIST"
#define	HELP_SEARCH_MAILNEWS			"messengr:SEARCH_MAILNEWS"
#define	HELP_SEARCH_LDAP				"messengr:SEARCH_LDAP"
#define	HELP_SELECT_ADDRESSES			"messengr:SELECT_ADDRESSES"
#define	HELP_SEARCH_ADDRESS_BOOKS		"messengr:SEARCH_ADDRESS_BOOKS"

/* Bookmark Dialogs */

#define	HELP_BOOKMARK_PROPERTIES	"navigatr:BOOKMARK_PROPERTIES"
#define	HELP_FIND_IN_BOOKMARKS		"navigatr:FIND_IN_BOOKMARKS"

/* Browser Dialogs */

#define	HELP_OPEN_PAGE			"navigatr:OPEN_PAGE"
#define	HELP_SEARCH_HISTORY_LIST	"navigatr:SEARCH_HISTORY_LIST"

/* Conference Dialogs */

#define	HELP_ADD_TO_SENDLIST		"confernc:ADD_TO_SENDLIST"
#define	HELP_CANVAS_SIZE			"confernc:CANVAS_SIZE"
#define	HELP_CHAT_FILE_SAVE		"confernc:CHAT_FILE_SAVE"
#define	HELP_COLLAB_BROWSER		"confernc:COLLAB_BROWSER"
#define	HELP_CHAT_ABOUT			"confernc:CONF_CHAT_ABOUT"
#define	HELP_CHAT_EDITLOG			"confernc:CONF_CHAT_EDITLOG"
#define	HELP_CHAT_EDITPAD			"confernc:CONF_CHAT_EDITPAD"
#define	HELP_CONF_FILEX			"confernc:CONF_FILEX_ABOUT"
#define	HELP_CONF_FILERCV			"confernc:CONF_FILEX_FILERCV"
#define	HELP_CONF_FILESND			"confernc:CONF_FILEX_FILESND"
#define	HELP_CONF_WB_ABOUT		"confernc:CONF_WB_ABOUT"
#define	HELP_DIRECT_CALL			"confernc:DIRECT_CALL"
#define	HELP_FILE_INCLUDE			"confernc:FILE_INCLUDE"
#define	HELP_FILE_OPEN			"confernc:FILE_OPEN"
#define	HELP_PROPS_AUDIO			"confernc:PROPERTIES_AUDIO"
#define	HELP_PROPS_AUDIO_ADVANCED	"confernc:PROPERTIES_AUDIO_ADVANCED"
#define	HELP_PROPS_BUSINESS_CARD	"confernc:PROPERTIES_BUSINESS_CARD"
#define	HELP_PROPS_CALL			"confernc:PROPERTIES_CALL"
#define	HELP_PROPS_SPEED_DIAL		"confernc:PROPERTIES_SPEED_DIAL"
#define	HELP_RECORD_VOICEMAIL		"confernc:RECORD_VOICEMAIL"
#define	HELP_SEND_VOICEMAIL		"confernc:SEND_VOICEMAIL"
#define	HELP_WB_FILE_SAVE			"confernc:WB_FILE_SAVE"

/* Editor Dialogs */

#define	HELP_DOC_PROPS_ADVANCED		"composer:DOCUMENT_PROPERTIES_ADVANCED"
#define	HELP_DOC_PROPS_APPEARANCE	"composer:DOCUMENT_PROPERTIES_APPEARANCE"
#define	HELP_DOC_PROPS_GENERAL		"composer:DOCUMENT_PROPERTIES_GENERAL"
#define	HELP_HTML_TAG			"composer:HTML_TAG"
#define	HELP_NEW_TABLE_PROPS		"composer:NEW_TABLE_PROPERTIES"
#define	HELP_PROPS_CHARACTER		"composer:PROPERTIES_CHARACTER"
#define	HELP_PROPS_HRULE			"composer:PROPERTIES_HRULE"
#define	HELP_PROPS_IMAGE			"composer:PROPERTIES_IMAGE"
#define	HELP_PROPS_IMAGE_ALT		"composer:PROPERTIES_IMAGE_ALT"
#define	HELP_PROPS_LINK			"composer:PROPERTIES_LINK"
#define	HELP_PROPS_PARAGRAPH		"composer:PROPERTIES_PARAGRAPH"
#define	HELP_PROPS_TARGET			"composer:PROPERTIES_TARGET"
#define	HELP_PUBLISH_FILES		"composer:PUBLISH_FILES"
#define	HELP_TABLE_PROPS_CELL		"composer:TABLE_PROPERTIES_CELL"
#define	HELP_TABLE_PROPS_ROW		"composer:TABLE_PROPERTIES_ROW"
#define	HELP_TABLE_PROPS_TABLE		"composer:TABLE_PROPERTIES_TABLE"
#define	HELP_SPELL_CHECK			"composer:SPELL_CHECK"
#define	HELP_IMAGE_CONVERSION		"composer:IMAGE_CONVERSION"
#define	HELP_EXTRA_HTML			"composer:EXTRA_HTML"
#define	HELP_COLOR_PICKER  			"composer:COLOR_PICKER"

/* Mail Dialogs */

#define	HELP_FILTER_RULES				"messengr:FILTER_RULES"
#define	HELP_MAIL_FILTERS				"messengr:MAIL_FILTERS"
#define	HELP_MAIL_FOLDER_PROPS_GENERAL	"messengr:MAIL_FOLDER_PROPERTIES_GENERAL"
#define	HELP_MAIL_FOLDER				"messengr:MAIL_FOLDER"			
#define	HELP_MAIL_NEWS_WIZARD			"messengr:MAIL_NEWS_WIZARD"
#define	HELP_MESSAGE_LIST_WINDOW		"messengr:MESSAGE_LIST_WINDOW"
#define	HELP_MAIL_MESSAGE_WINDOW		"messengr:MAIL_MESSAGE_WINDOW"

#define	HELP_HTML_MAIL_QUESTION			"messengr:HTML_MAIL_QUESTION"
#define	HELP_HTML_MAIL_QUESTION_RECIPIENT	"messengr:HTML_MAIL_QUESTION_RECIPIENT"

/* Main Preferences: Appearance */

#define	HELP_PREFS_APPEARANCE			"navigatr:PREFERENCES_APPEARANCE"
#define	HELP_PREFS_APPEARANCE_FONTS		"navigatr:PREFERENCES_APPEARANCE_FONTS"
#define	HELP_PREFS_APPEARANCE_COLORS		"navigatr:PREFERENCES_APPEARANCE_COLORS"

/* Main Preferences: Browser */

#define	HELP_PREFS_BROWSER			"navigatr:PREFERENCES_NAVIGATOR"
#define	HELP_PREFS_BROWSER_LANGUAGES		"navigatr:PREFERENCES_NAVIGATOR_LANGUAGES"
#define	HELP_PREFS_BROWSER_APPLICATIONS	"navigatr:PREFERENCES_NAVIGATOR_APPLICATIONS"

/* Main Preferences: Mail and Groups */

#define	HELP_PREFS_MAILNEWS_MAIN_PANE		"messengr:PREFERENCES_MAILNEWS_MAIN_PANE"
#define	HELP_PREFS_MAILNEWS_IDENTITY		"messengr:PREFERENCES_MAILNEWS_IDENTITY"
#define	HELP_PREFS_MAILNEWS_MESSAGES		"messengr:PREFERENCES_MAILNEWS_MESSAGES"
#define	HELP_PREFS_MAILNEWS_MAILSERVER	"messengr:PREFERENCES_MAILNEWS_MAILSERVER"
#define	HELP_PREFS_MAILNEWS_GROUPSERVER	"messengr:PREFERENCES_MAILNEWS_GROUPSERVER"
#define	HELP_PREFS_MAILNEWS_DIRECTORY		"messengr:PREFERENCES_MAILNEWS_DIRECTORY"
#define	HELP_MAILNEWS_EDIT_CARD			"messengr:MAILNEWS_EDIT_CARD"
#define	HELP_MAILNEWS_EDIT_CARD_NAME_TAB	"messengr:ADD_USER_PROPERTIES"
#define	HELP_MAILNEWS_EDIT_CARD_CONTACT_TAB	"messengr:ADD_USER_CONTACT"
#define	HELP_MAILNEWS_EDIT_CARD_CONFERENCE_CARD	"messengr:ADD_USER_NETSCAPE_COOLTALK"

/* Main Preferences: LI */

#define	HELP_PREFS_LI_LOGIN			"navigatr:PREFERENCES_NAVIGATOR"
#define	HELP_PREFS_LI_SERVER		"navigatr:PREFERENCES_NAVIGATOR_LANGUAGES"
#define	HELP_PREFS_LI_FILES	"navigatr:PREFERENCES_NAVIGATOR_APPLICATIONS"

#ifndef MOZ_MAIL_NEWS
#define  HELP_PREFS_IDENTITY              "navigatr:PREFERENCES_IDENTITY"
#endif /* MOZ_MAIL_NEWS */

/* Main Preferences: Composer */

#ifdef XP_MAC
#define	HELP_PREFS_COMPOSER				"composer:PREFERENCES_EDITOR_GENERAL"
#define	HELP_PREFS_COMPOSER_PUBLISHING	"composer:PREFERENCES_EDITOR_PUBLISH"
#else
#define	HELP_PREFS_COMPOSER			"messengr:PREFERENCES_COMPOSER"
#define	HELP_PREFS_COMPOSER_PUBLISHING	"messengr:PREFERENCES_COMPOSER_PUBLISHING"
#endif

/* Main Preferences: Offline */

#define	HELP_PREFS_OFFLINE			"navigatr:PREFERENCES_OFFLINE"
#define	HELP_PREFS_OFFLINE_GROUPS		"navigatr:PREFERENCES_OFFLINE_GROUPS"

/* Main Preferences: Advanced */

#define	HELP_PREFS_ADVANCED			"navigatr:PREFERENCES_ADVANCED"
#define	HELP_PREFS_ADVANCED_CACHE		"navigatr:PREFERENCES_ADVANCED_CACHE"
#define	HELP_PREFS_ADVANCED_PROXIES		"navigatr:PREFERENCES_ADVANCED_PROXIES"
#define	HELP_PREFS_ADVANCED_DISK_SPACE	"navigatr:PREFERENCES_ADVANCED_DISK_SPACE"
#define	HELP_PREFS_ADVANCED_SMARTUPDATE	"navigatr:PREFERENCES_ADVANCED_SMARTUPDATE"

/* Main Preferences: Conference */

#define	HELP_CONF_PREFS_PROPS_CALL		"home:PROPERTIES_CALL"
#define	HELP_CONF_PREFS_PROPS_AUDIO		"home:PROPERTIES_AUDIO"
#define	HELP_CONF_PREFS_PROPS_BUSINESS_CARD	"home:PROPERTIES_BUSINESS_CARD"

/* Editor Preferences */

#define	HELP_EDIT_PREFS_EDITOR_APPEARANCE		"composer:PREFERENCES_EDITOR_APPEARANCE"
#define	HELP_EDIT_PREFS_EDITOR_GENERAL		"composer:PREFERENCES_EDITOR_GENERAL"
#define	HELP_EDIT_PREFS_EDITOR_PUBLISH		"composer:PREFERENCES_EDITOR_PUBLISH"
#define	HELP_EDIT_DICTIONARY				"composer:EDIT_DICTIONARY"

/* Security Preferences */

#define	HELP_SEC_PREFS_SEC_GENERAL			"home:PREFERENCES_SECURITY_GENERAL"
#define	HELP_SEC_PREFS_SEC_PASSWORDS			"home:PREFERENCES_SECURITY_PASSWORDS"
#define	HELP_SEC_PREFS_SEC_PERSONAL_CERTIFICATES	"home:PREFERENCES_SECURITY_PERSONAL_CERTIFICATES"
#define	HELP_SEC_PREFS_SEC_SITE_CERTIFICATES	"home:PREFERENCES_SECURITY_SITE_CERTIFICATES"


/* Security Advisor Dialogs */

#define	HELP_SEC_PASS			"home:SECURITY_ADVISOR_PASSWORDS"
#define	HELP_SEC_PCERT			"home:SECURITY_ADVISOR_PERSONAL_CERTIFICATES"
#define	HELP_SEC_ADV			"home:SECURITY_ADVISOR_SECURITY_ADVISOR"
#define	HELP_SEC_SCERT			"home:SECURITY_ADVISOR_SITE_CERTIFICATES"

/* Discussion Dialogs */

#define	HELP_NEWS_DISCUSION_GENERAL			"collabra:NEWS_DISCUSSION_GENERAL"
#define	HELP_NEWS_DISCUSION_DOWNLOAD			"collabra:NEWS_DISCUSSION_DOWNLOAD"
#define	HELP_NEWS_DISCUSION_DISKSPACE			"collabra:NEWS_DISCUSSION_DISKSPACE"
#define	HELP_NEWS_NEW_GROUP_SERVER			"collabra:NEWS_NEW_GROUP_SERVER"
#define	HELP_NEWS_ADD_DIRECTORY_SERVER		"collabra:NEWS_ADD_DIRECTORY_SERVER"
#define	HELP_NEWS_DIRECTORY_SERVER_PROPERTY		"collabra:NEWS_DIRECTORY_SERVER_PROPERTY"
#define	HELP_DISCUSSION_HOST_PROPERTIES		"collabra:DISCUSSION_HOST_PROPERTIES"
#define	HELP_SUBSCRIBE_SEARCH				"collabra:SUBSCRIBE_SEARCH"
#define	HELP_SUBSCRIBE_LIST_NEW				"collabra:SUBSCRIBE_LIST_NEW"
#define	HELP_SUBSCRIBE_LIST_ALL				"collabra:SUBSCRIBE_LIST_ALL"
#define	HELP_ADD_SERVER					"collabra:ADD_SERVER"


#define	HELP_OFFLINE_DOWNLOAD				"collabra:NEWS_DISCUSSION_DOWNLOAD_OFFLINE"
#define	HELP_OFFLINE_DISCUSSION_GROUPS		"collabra:NEWS_DISCUSSION_GROUPS"

#endif

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

#include "uprefd.h"

// macfe
#include "ufilemgr.h"				// CFileMgr
#include "macutil.h"				// CStringListRsrc
#include "resgui.h"					// res_Strings1, res_Strings2, res_StringsBoolean
#include "mregistr.h"				// CNotifierRegistry
#include "uerrmgr.h"				// ErrorManager
#include "uapp.h"					// FILE_TYPE_PROFILES, STARTUP_TYPE_NETPROFILE
#include "macfeprefs.h"				// prototypes for FindGutsFolder, FindNetscapeFolder, FindJavaDownloadsFolder
#include "CToolTipAttachment.h"
#include "InternetConfig.h"			// CInternetConfigInterface
#include "CSpinningN.h"
#include "profile.h"				// kProfileStrings

#include <LBroadcaster.h>

// Netscape
#include "glhist.h"					// GH_SetGlobalHistoryTimeout
#include "net.h"					// PROXY_STYLE_AUTOMATIC
#include "ntypes.h"
#include "shistele.h"
#include "proto.h"
#ifndef _XP_H_
//#include "xp_mcom.h"
#endif
#include "msgcom.h"					// MSG_NormalSize, MSG_PlainFont
#include "mime.h"					// MIME_ConformToStandard
#include "fe_proto.h"				// prototype for FE_GetNetHelpDir
#include "java.h"					// LJ_SetJavaEnabled

#include "prefapi.h"		// ns/modules/libpref

/*********************************************************************************
 * DOGBERT XP PREFERENCES / MULTI-USER PROFILES
 *
 * New preference file format: XP text file parsed by JavaScript via libpref
 *	(includes all user strings, bools, ints, and colors).
 * Some prefs (see below) are still written in old resource format.
 * Both old and new prefs are read & written to the existing Netscape Prefs file.
 *
 * Primary changes to this file:
 *	- added GetUserProfile() and InitPrefsFolder() to prompt user for desired
 *	  profile (if necessary) and use that to point to base Preferences folder.
 *	- changed order of PrefEnum entries and added XP prefName strings.
 *	- ReadPreference, if it detects new format, only needs to call 
 *	  PostProcessPrefChange for each pref to propagate the new value.
 *	  Calls to back-end will GO AWAY as modules fetch their own pref values.
 *	- ReadPreference otherwise reads old-format file.
 *	- WritePreference always writes new format.
 *	- CPrefs Get/Setters use libpref calls.
 *	- removed CPrefs storage of strings, bools, ints, and colors.
 *	- file/folder aliases accessed as libpref binary types but still stored
 *	  in CPrefs via sFileIDs.
 *
 * Some prefs are not yet supported in XP format.  
 *	- font encodings
 *	- window/pane positions
 *	- protocol handlers, etc.
 * We at least should support the first in XP format for use by Admin Kit.
 *********************************************************************************/

const Int16		refNum_Undefined	= -1;	// Really defined in LFile.cp

static LBroadcaster * gPrefBroadcaster = NULL;

//===============================================================================
// Resource definitions
// definitions of type and resource location of all preferences
//===============================================================================

#define NOTSAVED	'NONE'
#define STRINGLIST	'STR#'
#define ALIASREC	'ALIS'
#define PRINTRECORD	'PrRc'
#define REVISION	'PRev'
#define WINDOWREC	'WPSt'	// Starts off with a rect, followed by variable data for each window

#define REVISION_RES_ID 10000
// Resource IDs of all resources used in prefs
// are defined in resgui.h

enum PrefType	{
	eStringType,
	eBooleanType,
	eFileSpecType,
	eLongType,
	eColorType,
	ePrintRecordType,
	eNoType
};

struct PrefLoc {
	CPrefs::PrefEnum	prefID;	// What's my id
	OSType		resType;		// What kind of resource are we reading
	PrefType	prefType;	// How should we store this pref natively
	short		resID;		// Resource ID
	short		refCon;		// For STRINGLIST this is the index into the resource
							// For ALIASREC, 1 to pop up alert when not found, 0 no alert
	char*		prefName;	// XP pref name
};

// This array defines what kind of preference each PrefEnum is,
// and where it is stored.

static PrefLoc	prefLoc[] =
{
// Folders -- need to be before anything else, because we do not have the accumulator
	{	CPrefs::DownloadFolder,		ALIASREC,	eFileSpecType, 129, 1,
		"browser.download_directory"},
	{	CPrefs::NetscapeFolder,		NOTSAVED,	eFileSpecType, 0, 1},
	{	CPrefs::MainFolder,			NOTSAVED,	eFileSpecType, 0, 1},
	{	CPrefs::UsersFolder,		NOTSAVED,	eFileSpecType, 0, 1},
	{	CPrefs::DiskCacheFolder,	ALIASREC,	eFileSpecType, 133, 1,
		"browser.cache.directory"},
	{	CPrefs::SignatureFile,		ALIASREC,	eFileSpecType, 134, 1,
		"mail.signature_file"},
	{	CPrefs::GIFBackdropFile,	ALIASREC,	eFileSpecType, 135, 1 },
	{	CPrefs::MailFolder,			ALIASREC,	eFileSpecType, 136, 1,
		"mail.directory"},
	{	CPrefs::NewsFolder,			NOTSAVED,	eFileSpecType, 0, 1 },
	{	CPrefs::SecurityFolder,		NOTSAVED,	eFileSpecType, 0, 1 },
	{	CPrefs::MailCCFile,			ALIASREC,	eFileSpecType, 137, 1,
		"mail.default_fcc"},
	{	CPrefs::NewsCCFile,			ALIASREC,	eFileSpecType, 138, 1,
		"news.default_fcc"},
	{	CPrefs::HTMLEditor,			ALIASREC,	eFileSpecType, 139, 1,
		"editor.html_editor"},
	{	CPrefs::ImageEditor,		ALIASREC,	eFileSpecType, 140, 1,
		"editor.image_editor"},
        {       CPrefs::RequiredGutsFolder,     NOTSAVED,       eFileSpecType, 0, 1},

// TJ & ROB
        {       CPrefs::SARCacheFolder,         NOTSAVED,       eFileSpecType, 0, 0},
// EA
	{	CPrefs::NetHelpFolder,		NOTSAVED,	eFileSpecType,	0, 0},

// Strings
	{	CPrefs::HomePage, 			STRINGLIST, eStringType,  res_Strings1, 1,
			"browser.startup.homepage" },
	{	CPrefs::NewsHost,			STRINGLIST, eStringType,  res_Strings1, 2,
			"network.hosts.nntp_server" },
	{	CPrefs::FTPProxy,			STRINGLIST, eStringType,  res_Strings1, 3,
			"network.proxy.ftp" },
	{	CPrefs::GopherProxy,		STRINGLIST, eStringType,  res_Strings1, 4,
			"network.proxy.gopher" },
	{	CPrefs::HTTPProxy,			STRINGLIST, eStringType,  res_Strings1, 5,
			"network.proxy.http" },
	{	/*unused*/CPrefs::NewsProxy,			STRINGLIST, eStringType,  res_Strings1, 6 },
	{	CPrefs::WAISProxy,			STRINGLIST, eStringType,  res_Strings1, 7,
			"network.proxy.wais" },
	{	CPrefs::FTPProxyPort,		STRINGLIST, eStringType,  res_Strings1, 8,
			"network.proxy.ftp_port" },
	{	CPrefs::GopherProxyPort,	STRINGLIST, eStringType,  res_Strings1, 9,
			"network.proxy.gopher_port" },
	{	CPrefs::HTTPProxyPort,		STRINGLIST, eStringType,  res_Strings1, 10,
			"network.proxy.http_port" },
	{	/*unused*/CPrefs::NewsProxyPort,		STRINGLIST, eStringType,  res_Strings1, 11 },
	{	CPrefs::WAISProxyPort,		STRINGLIST, eStringType,  res_Strings1, 12,
			"network.proxy.wais_port" },
	{	CPrefs::NoProxies,			STRINGLIST, eStringType,  res_Strings1, 13,
			"network.proxy.no_proxies_on" },
	{	CPrefs::UserName,			STRINGLIST, eStringType,  res_Strings1, 14,
		"mail.identity.username" },
	{	CPrefs::UserEmail,			STRINGLIST, eStringType,  res_Strings1, 15,
		"mail.identity.useremail" },
	{	CPrefs::SMTPHost, 			STRINGLIST, eStringType,  res_Strings1, 16,
		"network.hosts.smtp_server" },
	{	CPrefs::SOCKSHost, 			STRINGLIST, eStringType,  res_Strings2, 1,
		"network.hosts.socks_server" },
	{	CPrefs::SOCKSPort, 			STRINGLIST, eStringType,  res_Strings2, 2,
		"network.hosts.socks_serverport" },
	{	CPrefs::Organization, 		STRINGLIST, eStringType,  res_Strings2, 3,
		"mail.identity.organization" },
	{	CPrefs::SSLProxy, 			STRINGLIST, eStringType,  res_Strings2, 4,
		"network.proxy.ssl" },
	{	CPrefs::SSLProxyPort,	 	STRINGLIST, eStringType,  res_Strings2, 5,
		"network.proxy.ssl_port" },
	{	CPrefs::PopHost,	 		STRINGLIST, eStringType,  res_Strings2, 6,
		"network.hosts.pop_server" },
	{	CPrefs::AcceptLanguage,	 	STRINGLIST, eStringType,  res_Strings2, 7,
		"intl.accept_languages" },
	{	CPrefs::DefaultMailCC,		STRINGLIST, eStringType,  res_Strings2, 8,
		"mail.default_cc" },
	{	CPrefs::DefaultNewsCC,		STRINGLIST, eStringType,  res_Strings2, 9,
		"news.default_cc" },
	{	CPrefs::ReplyTo,			STRINGLIST, eStringType,  res_Strings2, 10,
		"mail.identity.reply_to" },
	{	CPrefs::PopID,				STRINGLIST, eStringType,  res_Strings2, 11,
		"mail.pop_name" },
	{	CPrefs::AutoProxyURL,		STRINGLIST, eStringType,  res_Strings2, 12,
		"network.proxy.autoconfig_url" },
	{	CPrefs::MailPassword,		STRINGLIST, eStringType,  res_Strings2, 13,
		"mail.pop_password" },
	{ 	CPrefs::Ciphers,			STRINGLIST,	eStringType,	res_Strings2, 14,
		"security.ciphers" },	
	{ 	CPrefs::EditorAuthor,					STRINGLIST,	eStringType,	res_Strings2, 15,
		"editor.author" },	
	{ 	CPrefs::EditorNewDocTemplateLocation,	STRINGLIST,	eStringType,	res_Strings2, 16,
		"editor.template_location" },	
	{ 	CPrefs::EditorBackgroundImage,			STRINGLIST,	eStringType,	res_Strings2, 17,
		"editor.background_image" },	
	{ 	CPrefs::PublishLocation,				STRINGLIST,	eStringType,	res_Strings2, 18,
		"editor.publish_location" },	
	{ 	CPrefs::PublishBrowseLocation,			STRINGLIST,	eStringType,	res_Strings2, 19,
		"editor.publish_browse_location" },	
	{ 	CPrefs::PublishHistory0,					STRINGLIST,	eStringType,	res_Strings2, 20,
		"editor.publish_history_0" },	
	{ 	CPrefs::PublishHistory1,					STRINGLIST,	eStringType,	res_Strings2, 21,
		"editor.publish_history_1" },	
	{ 	CPrefs::PublishHistory2,					STRINGLIST,	eStringType,	res_Strings2, 22,
		"editor.publish_history_2" },	
	{ 	CPrefs::PublishHistory3,					STRINGLIST,	eStringType,	res_Strings2, 23,
		"editor.publish_history_3" },	
	{ 	CPrefs::PublishHistory4,					STRINGLIST,	eStringType,	res_Strings2, 24,
		"editor.publish_history_4" },	
	{ 	CPrefs::PublishHistory5,					STRINGLIST,	eStringType,	res_Strings2, 25,
		"editor.publish_history_5" },	
	{ 	CPrefs::PublishHistory6,					STRINGLIST,	eStringType,	res_Strings2, 26,
		"editor.publish_history_6" },	
	{ 	CPrefs::PublishHistory7,					STRINGLIST,	eStringType,	res_Strings2, 27,
		"editor.publish_history_7" },	
	{ 	CPrefs::PublishHistory8,					STRINGLIST,	eStringType,	res_Strings2, 28,
		"editor.publish_history_8" },	
	{ 	CPrefs::PublishHistory9,					STRINGLIST,	eStringType,	res_Strings2, 29,
		"editor.publish_history_9" },	
	{ 	CPrefs::DefaultPersonalCertificate,		STRINGLIST,	eStringType,	res_Strings2, 30,
		"security.default_personal_cert" },	
// Booleans
	{	CPrefs::DelayImages,		STRINGLIST, eBooleanType,  res_StringsBoolean, 1,
		"browser.delay_images"},
	{	CPrefs::AnchorUnderline,	STRINGLIST, eBooleanType,  res_StringsBoolean, 2,
		"browser.underline_anchors"},
	{	CPrefs::ShowAllNews,		STRINGLIST, eBooleanType,  res_StringsBoolean, 3},
	{	CPrefs::UseFancyFTP,		STRINGLIST, eBooleanType,  res_StringsBoolean, 4},
	{	CPrefs::UseFancyNews,		STRINGLIST, eBooleanType,  res_StringsBoolean, 5,
		"news.fancy_listing" },
	{	CPrefs::ShowToolbar,		STRINGLIST, eBooleanType,  res_StringsBoolean, 6,
		"browser.chrome.show_toolbar"},
	{	CPrefs::ShowStatus,			STRINGLIST, eBooleanType,  res_StringsBoolean, 7,
		"browser.chrome.show_status_bar"},
	{	CPrefs::ShowURL,			STRINGLIST, eBooleanType,  res_StringsBoolean, 8,
		"browser.chrome.show_url_bar"},
	{	CPrefs::LoadHomePage,		STRINGLIST, eBooleanType,  res_StringsBoolean, 9, 
		"browser.startup.autoload_homepage" },
	{	CPrefs::ExpireNever,		STRINGLIST, eBooleanType,  res_StringsBoolean, 10,
		"browser.never_expire"},
	{	CPrefs::DisplayWhileLoading, STRINGLIST, eBooleanType,  res_StringsBoolean,11,
		"browser.display_while_loading"},
	{	CPrefs::CustomLinkColors,	STRINGLIST, eBooleanType,  res_StringsBoolean, 12,
		"browser.custom_link_color"},
	{	CPrefs::ShowDirectory,		STRINGLIST, eBooleanType,  res_StringsBoolean, 13,
		"browser.chrome.show_directory_buttons" },
	{	CPrefs::AgreedToLicense,	STRINGLIST, eBooleanType,  res_StringsBoolean, 14,
		"browser.startup.agreed_to_licence" },
	{	CPrefs::EnteringSecure,		STRINGLIST, eBooleanType,  res_StringsBoolean, 15,
		"security.warn_entering_secure"},
	{	CPrefs::LeavingSecure,		STRINGLIST, eBooleanType,  res_StringsBoolean, 16,
		"security.warn_leaving_secure"},
	{	CPrefs::ViewingMixed,		STRINGLIST, eBooleanType,  res_StringsBoolean, 17,
		"security.warn_viewing_mixed"},
	{	CPrefs::SubmittingInsecure,	STRINGLIST, eBooleanType,  res_StringsBoolean, 18,
		"security.warn_submit_insecure"},
	{	CPrefs::ShowSecurity,		STRINGLIST, eBooleanType,  res_StringsBoolean, 19,
		"browser.chrome.show_security_bar"},
	{	CPrefs::CustomVisitedColors,STRINGLIST, eBooleanType,  res_StringsBoolean, 20,
		"browser.custom_visited_color"},
	{	CPrefs::CustomTextColors,	STRINGLIST, eBooleanType,  res_StringsBoolean, 21,
		"browser.custom_text_color"},
	{	CPrefs::UseDocumentColors,	STRINGLIST, eBooleanType,  res_StringsBoolean, 22,
		"browser.use_document_colors"},
	{	CPrefs::AutoDetectEncoding,	STRINGLIST, eBooleanType,  res_StringsBoolean, 23,
		"intl.auto_detect_encoding"},
	{	CPrefs::UseSigFile,			STRINGLIST, eBooleanType,  res_StringsBoolean, 24,
		"mail.use_signature_file"},
	{	CPrefs::StrictlyMIME,		STRINGLIST, eBooleanType,  res_StringsBoolean, 25,
		"mail.strictly_mime" },
	{	CPrefs::UseUtilityBackground, STRINGLIST, eBooleanType,  res_StringsBoolean, 26,
		"browser.mac.use_utility_pattern"},
	{	CPrefs::MailUseFixedWidthFont, STRINGLIST, eBooleanType,  res_StringsBoolean, 27,
		"mail.fixed_width_messages"},
	{	CPrefs::UseMailFCC,			STRINGLIST, eBooleanType,  res_StringsBoolean, 28,
		"mail.use_fcc"},
	{	CPrefs::UseNewsFCC,			STRINGLIST, eBooleanType,  res_StringsBoolean, 29,
		"news.use_fcc"},
	{	CPrefs::LimitMessageSize,	STRINGLIST, eBooleanType,  res_StringsBoolean, 30,
		"mail.limit_message_size"},
	{	CPrefs::LeaveMailOnServer,	STRINGLIST, eBooleanType,  res_StringsBoolean, 31,
		"mail.leave_on_server" },
	{	CPrefs::MailCCSelf,			STRINGLIST, eBooleanType,  res_StringsBoolean, 32,
		"mail.cc_self"},
	{	CPrefs::NewsCCSelf,			STRINGLIST, eBooleanType,  res_StringsBoolean, 33,
		"news.cc_self"},
	{	CPrefs::BiffOn,				STRINGLIST, eBooleanType,  res_StringsBoolean, 34,
		"mail.check_new_mail"},
	{   CPrefs::UseMozPassword,		STRINGLIST, eBooleanType,  res_StringsBoolean, 35},
	{	CPrefs::ThreadMail,			STRINGLIST, eBooleanType,  res_StringsBoolean, 36,
		"mail.thread_mail"},
	{	CPrefs::ThreadNews,			STRINGLIST, eBooleanType,  res_StringsBoolean, 37,
		"news.thread_news"},
	{	CPrefs::UseInlineViewSource,STRINGLIST, eBooleanType,  res_StringsBoolean, 38 },
	{	CPrefs::AutoQuoteOnReply,	STRINGLIST, eBooleanType,  res_StringsBoolean, 39,
		"mail.auto_quote"},
	{	CPrefs::RememberMailPassword,	STRINGLIST, eBooleanType,  res_StringsBoolean, 40,
		"mail.remember_password"},
	{	CPrefs::DefaultMailDelivery,	STRINGLIST, eBooleanType,  res_StringsBoolean, 41,
		"mail.deliver_immediately"},
	{	CPrefs::EnableJavascript,			STRINGLIST,	eBooleanType,	res_StringsBoolean,	42,
		"javascript.enabled" },
	{	CPrefs::ShowToolTips,			STRINGLIST, eBooleanType,  res_StringsBoolean, 43,
		"browser.mac.show_tool_tips"},
	{	CPrefs::UseInternetConfig,		STRINGLIST, eBooleanType,  res_StringsBoolean, 44,
		"browser.mac.use_internet_config"},
	{	CPrefs::EnableJava,		STRINGLIST,	eBooleanType,	res_StringsBoolean,	45,
		"security.enable_java" },
	{	CPrefs::AcceptCookies,		STRINGLIST,	eBooleanType,	res_StringsBoolean,	46,
		"network.cookie.warnAboutCookies" },
	{	CPrefs::UseEmailAsPassword,	STRINGLIST,	eBooleanType,	res_StringsBoolean,	47,
		"security.email_as_ftp_password" },
	{	CPrefs::SubmitFormsByEmail,	STRINGLIST,	eBooleanType,	res_StringsBoolean,	48,
		"security.submit_email_forms" },
	{ 	CPrefs::AllowSSLDiskCaching,	STRINGLIST,	eBooleanType,	res_StringsBoolean,	49,
		"browser.cache.disk_cache_ssl" },
	{	CPrefs::EnableSSLv2,		STRINGLIST,	eBooleanType,	res_StringsBoolean,	50,
		"security.enable_ssl2" },
	{	CPrefs::EnableSSLv3,		STRINGLIST,	eBooleanType,	res_StringsBoolean,	51,
		"security.enable_ssl3" },
	{	CPrefs::EnableActiveScrolling, STRINGLIST,	eBooleanType,	res_StringsBoolean,	52,
		"browser.mac.active_scrolling" },
#ifdef FORTEZZA
	{	CPrefs::FortezzaTimeoutOn,		STRINGLIST, eBooleanType,	res_StringsBoolean, 0 },
#endif
#ifdef EDITOR
	{	CPrefs::EditorUseCustomColors,	STRINGLIST,	eBooleanType,	res_StringsBoolean, 53,
		"editor.use_custom_colors" },
	{	CPrefs::EditorUseBackgroundImage,	STRINGLIST,	eBooleanType,	res_StringsBoolean, 54,
		"editor.use_background_image" },
	{	CPrefs::PublishMaintainLinks,	STRINGLIST,	eBooleanType,	res_StringsBoolean, 55,
		"editor.publish_keep_links" },
	{	CPrefs::PublishKeepImages,		STRINGLIST,	eBooleanType,	res_StringsBoolean, 56,
		"editor.publish_keep_images" },
	{	CPrefs::ShowCopyright,			STRINGLIST,	eBooleanType,	res_StringsBoolean, 57,
		"editor.show_copyright" },
	{	CPrefs::ShowFileEditToolbar,	STRINGLIST,	eBooleanType,	res_StringsBoolean, 58 },
	{	CPrefs::ShowCharacterToolbar,	STRINGLIST,	eBooleanType,	res_StringsBoolean, 59,
		"editor.show_character_toolbar" },
	{	CPrefs::ShowParagraphToolbar,	STRINGLIST,	eBooleanType,	res_StringsBoolean, 60,
		"editor.show_paragraph_toolbar" },
#endif EDITOR
	// WARNING: If you add stuff here, the custom.r file (in projector for Browser and MailNews) must match!
// Longs
	{	CPrefs::DiskCacheSize,		STRINGLIST, eLongType,  res_StringsLong, 1,
		"browser.cache.disk_cache_size"},
	{	CPrefs::FileSortMethod,		STRINGLIST, eLongType,  res_StringsLong, 2,
		"network.file_sort_method"},
	{	CPrefs::ToolbarStyle,		STRINGLIST, eLongType,  res_StringsLong, 3,
		"browser.chrome.toolbar_style"},
	{	CPrefs::DaysTilExpire,		STRINGLIST, eLongType,  res_StringsLong, 4,
		"browser.link_expiration"},
	{	CPrefs::Connections,		STRINGLIST, eLongType,  res_StringsLong, 5,
		"network.max_connections"},
	{	CPrefs::BufferSize,			STRINGLIST, eLongType,  res_StringsLong, 6,
		"network.tcpbufsize"},
	{	/*unused*/CPrefs::PrintFlags,			STRINGLIST, eLongType,  res_StringsLong, 7},
	{	CPrefs::NewsArticlesMax,	STRINGLIST, eLongType,  res_StringsLong, 8,
		"news.max_articles"},
	{	CPrefs::CheckDocuments,		STRINGLIST, eLongType,  res_StringsLong, 9,
		"browser.cache.check_doc_frequency"},
	{	CPrefs::DefaultCharSetID,	STRINGLIST, eLongType,  res_StringsLong, 10,
		"intl.character_set"},
	{	CPrefs::DefaultFontEncoding,STRINGLIST, eLongType,  res_StringsLong, 11,
		"intl.font_encoding"},
	{	CPrefs::BackgroundColors,	STRINGLIST, eLongType,  res_StringsLong, 12,
		"browser.background_option" },
	{	CPrefs::LicenseVersion,		STRINGLIST, eLongType,  res_StringsLong, 13,
		"browser.startup.license_version"},
	{	CPrefs::MessageFontStyle,	STRINGLIST, eLongType,  res_StringsLong, 14,
		"mail.quoted_style"},
	{	CPrefs::MessageFontSize,	STRINGLIST, eLongType,  res_StringsLong, 15,
		"mail.quoted_size"},
	{	CPrefs::MaxMessageSize,		STRINGLIST, eLongType,  res_StringsLong, 16,
		"mail.max_size" },
	{	CPrefs::TopLeftHeader,		STRINGLIST,	eLongType,	res_StringsLong, 17,
		"browser.mac.print_header_topleft" },
	{	CPrefs::TopMidHeader,		STRINGLIST,	eLongType,	res_StringsLong, 31,
		"browser.mac.print_header_topmid" },
	{	CPrefs::TopRightHeader,		STRINGLIST,	eLongType,	res_StringsLong, 18,
		"browser.mac.print_header_topright" },
	{	CPrefs::BottomLeftFooter,	STRINGLIST,	eLongType,	res_StringsLong, 19,
		"browser.mac.print_header_botleft" },
	{	CPrefs::BottomMidFooter,	STRINGLIST,	eLongType,	res_StringsLong, 20,
		"browser.mac.print_header_botmid" },
	{	CPrefs::BottomRightFooter,	STRINGLIST,	eLongType,	res_StringsLong, 21,
		"browser.mac.print_header_botright" },
	{	CPrefs::PrintBackground,	STRINGLIST,	eLongType,	res_StringsLong, 22,
		"browser.mac.print_background" },
	{	CPrefs::ProxyConfig,		STRINGLIST,	eLongType,	res_StringsLong, 23,
		"network.proxy.type" },
	{	CPrefs::StartupAsWhat,		STRINGLIST,	eLongType,	res_StringsLong, 24,
		"browser.startup.default_window"},
	{	CPrefs::StartupBitfield,	STRINGLIST,	eLongType,	res_StringsLong, 25,
		"browser.mac.startup_bitfield"},
	{	CPrefs::BiffTimeout,		STRINGLIST,	eLongType,	res_StringsLong, 26,
		"mail.check_time"},
	{	CPrefs::AskMozPassword,		STRINGLIST,	eLongType,	res_StringsLong, 27,
		"security.ask_for_password"},
	{	CPrefs::AskMozPassFrequency,STRINGLIST,	eLongType,	res_StringsLong, 28,
		"security.password_lifetime"},
	{	CPrefs::SortMailBy,			STRINGLIST,	eLongType,	res_StringsLong, 29,
		"mail.sort_by"},
	{	CPrefs::SortNewsBy,			STRINGLIST,	eLongType,	res_StringsLong, 30,
		"news.sort_by"},
	{	CPrefs::NewsPaneConfig,		STRINGLIST,	eLongType,	res_StringsLong, 32,
		"news.pane_config"},
	{	CPrefs::MailPaneConfig,		STRINGLIST,	eLongType,	res_StringsLong, 33,
		"mail.pane_config"},
	{	CPrefs::MailHeaderDisplay,	STRINGLIST,	eLongType,	res_StringsLong, 34,
		"mail.show_headers"},
	{	CPrefs::NewsHeaderDisplay,	STRINGLIST,	eLongType,	res_StringsLong, 35,
		"news.show_headers"},
	{	CPrefs::AutoSaveTimeDelay,	STRINGLIST,	eLongType,	res_StringsLong, 36,
		"editor.auto_save_delay"},
#ifdef FORTEZZA
	{	CPrefs::FortezzaTimeout,	STRINGLIST,	eLongType,	res_StringsLong, 37,
		"security.fortezza_lifetime" },
#endif

// Colors
	{	CPrefs::Black,				NOTSAVED,	eColorType, 0, 1},
	{	CPrefs::White,				NOTSAVED,	eColorType, 0, 1},
	{	CPrefs::Blue,				NOTSAVED,	eColorType, 0, 1},
	{	CPrefs::Magenta,			NOTSAVED,	eColorType, 0, 1},
	{	CPrefs::WindowBkgnd,		NOTSAVED,	eColorType, 0, 1},	// --was res_StringsColor, 1
	{	CPrefs::Anchor,				STRINGLIST, eColorType, res_StringsColor, 2,
		"browser.anchor_color"},
	{	CPrefs::Visited,			STRINGLIST, eColorType, res_StringsColor, 3,
		"browser.visited_color"},
	{	CPrefs::TextFore,			STRINGLIST,	eColorType, res_StringsColor, 4,
		"browser.foreground_color"},
	{	CPrefs::TextBkgnd,			STRINGLIST, eColorType, res_StringsColor, 5,
		"browser.background_color"},
	{	CPrefs::EditorText,			STRINGLIST,	eColorType, res_StringsColor, 6,
		"editor.text_color"},
	{	CPrefs::EditorLink,			STRINGLIST,	eColorType, res_StringsColor, 7,
		"editor.link_color"},
	{	CPrefs::EditorActiveLink,	STRINGLIST,	eColorType, res_StringsColor, 8,
		"editor.active_link_color"},
	{	CPrefs::EditorFollowedLink,	STRINGLIST,	eColorType, res_StringsColor, 9,
		"editor.followed_link_color"},
	{	CPrefs::EditorBackground,	STRINGLIST,	eColorType, res_StringsColor, 10,
		"editor.background_color"},
	{ 	CPrefs::Citation,			STRINGLIST,	eColorType,	res_StringsColor, 11,
		"mail.citation_color"},
// Print record
	{	CPrefs::PrintRecord,		PRINTRECORD, ePrintRecordType, res_PrintRecord, 0 },
// Terminator
	{	CPrefs::LastPref,			NOTSAVED,	eNoType, 0,	0}
};

// 
//	Class variables
//
RGBColor * 	CPrefs::sColors = NULL;
CFolderSpec ** CPrefs::sFileIDs = NULL;
THPrint		CPrefs::sPrintRec = NULL;
CMimeList 	CPrefs::sMimeTypes;
char *		CPrefs::sCachePath = NULL;

FSSpec		CPrefs::sTemporaryFolder;
Boolean		CPrefs::sRealTemporaryFolder = false;
Boolean		CPrefs::sViewSourceInline = true;
// 0 = no user preference file
// 3 = preferences in resource fork
// 4 = preferences in data fork
short		CPrefs::sPrefFileVersion = 3;

//
// Static variables
//

// Preference file related
static		LFile * sPrefFile = NULL;
static		short 	sPrefFileResID = refNum_Undefined;
static		short	sAppFileResID = refNum_Undefined;
static		Boolean	sReading = FALSE;	 // Are we reading the prefs right now?
static		Boolean sDirty = FALSE;		
static		Boolean sDirtyOnRead = FALSE;	// Used to signal saving of preferences 
											// if we need to touch them while reading
// 
// Utility routines
//
static UInt32 CountPrefsOfType(PrefType type);
static LFile* GetFileForPreferences();
/*static*/ Boolean FindDefaultPreferencesFolder( FSSpec& prefFolder, short folderNameID,
	Boolean createIt, Boolean resolveIt = true );
static OSErr AssertPrefsFileOpen(Int16 privileges);
static LO_Color MakeLOColor( const RGBColor& rgbColor );
static OSErr AssurePreferencesSubfolder(pref_Strings whichFolder, FSSpec & folderSpec);

// Counts how many preferences of particular type do we have
UInt32 CountPrefsOfType(PrefType type)
{
	UInt32 howMany = 0;
	int i = 0;
	while(prefLoc[i].prefType != eNoType)
		if (prefLoc[i++].prefType == type)
			howMany++;
	return howMany;
}

// FindDefaultPreferencesFolder searches the disk for a preferences folder.
// If it does not find one, it tries to create it.
// It returns false upon complete failure (could not find or create)
// Folder lookup sequence: Preferences, Home directory, Desktop
// Algorithm:
// Look for a folder in folder lookup sequence.
// If not found try to create it in the same sequence.
Boolean FindDefaultPreferencesFolder( FSSpec& prefFolder, short folderNameID,
	Boolean createIt, Boolean resolveIt )
{
	short     prefRefNum, deskRefNum;
	long      prefDirID, deskDirID;
	FSSpec netscapeFolder = CPrefs::GetFilePrototype(CPrefs::NetscapeFolder);

	OSErr err = FindFolder( kOnSystemDisk, kPreferencesFolderType, kCreateFolder, &prefRefNum, &prefDirID );// Get Pref directory
	OSErr err2 = FindFolder( kOnSystemDisk, kDesktopFolderType, kCreateFolder, &deskRefNum, &deskDirID );
	
	// ¥ check for presence of MCC folderÉ
	Boolean foundFolder = FALSE;
	CStr255 folderName;
	GetIndString( folderName, 300, folderNameID );
	LString::CopyPStr( folderName, prefFolder.name, 32 );

	// ¥ in the Preferences folder
	if ( !foundFolder &&  ( err == noErr ) &&
		( CFileMgr::FindWFolderInFolder(	prefRefNum, prefDirID,
											folderName,
											&prefFolder.vRefNum, &prefFolder.parID) == noErr ) )
		foundFolder = TRUE;

	// ¥Êin the applications folder
	if ( !foundFolder && 
		 ( CFileMgr::FindWFolderInFolder(	netscapeFolder.vRefNum, netscapeFolder.parID,
											folderName,
											&prefFolder.vRefNum, &prefFolder.parID) == noErr ) )
		foundFolder = TRUE;

	// ¥Êand on the desktop
	if ( ( !foundFolder ) && ( err2 == noErr ) && 
		( CFileMgr::FindWFolderInFolder(	deskRefNum, deskDirID,
											folderName,
											&prefFolder.vRefNum, &prefFolder.parID) == noErr ) )
		foundFolder = TRUE;

	// ¥Êcreate a folder if one does not exist. Go in Folder lookup sequence
	if ( !foundFolder && createIt )	
	{
		// ¥ get the name of the folder out of resources
		CStr255 folderName;
		GetIndString( folderName, 300, folderNameID );
		OSErr createError;

		// ¥Êtry the Preferences folder
		createError = CFileMgr::CreateFolderInFolder( prefRefNum, prefDirID,
											folderName, &prefFolder.vRefNum, &prefFolder.parID);

		if ( createError == noErr )
			foundFolder = TRUE;
		else	
		{
			// ¥Êtry the application's folder
			createError = CFileMgr::CreateFolderInFolder( netscapeFolder.vRefNum, netscapeFolder.parID,
											folderName, &prefFolder.vRefNum, &prefFolder.parID);			
			if ( createError == noErr )
				foundFolder = TRUE;
			else
			{
				// ¥ try the Desktop
				createError = CFileMgr::CreateFolderInFolder(deskRefNum, deskDirID,
											folderName, &prefFolder.vRefNum, &prefFolder.parID);			
				if ( createError == noErr )
					foundFolder = TRUE;
				else
					foundFolder = FALSE;
			}
		}
	}

	if (foundFolder && resolveIt)
		CFileMgr::FolderSpecFromFolderID(prefFolder.vRefNum, prefFolder.parID, prefFolder);

	return foundFolder;
}

inline Boolean CPrefs::IsNewPrefFormat(short id)
{
	return prefLoc[id].prefName != nil;
}

// Returns LFile for the preferences
// File might not exist
LFile* GetFileForPreferences()
{
	FSSpec		prefFileSpec;
	
	// ¥Êget the default preferences file name from the application rsrc fork
	::GetIndString( prefFileSpec.name, 300, prefFileName );
	
	if ( prefFileSpec.name[0] > 31 )
		prefFileSpec.name[0] = 31;
		
	FSSpec folderSpec = CPrefs::GetFilePrototype(CPrefs::MainFolder);
	prefFileSpec.vRefNum = folderSpec.vRefNum;
	prefFileSpec.parID = folderSpec.parID;

	CPrefs::sPrefFileVersion = CFileMgr::FileExists(prefFileSpec) ? 3 : 0;
	
	// ¥ return a fully specified LFile
	return new LFile( prefFileSpec );
}

// Creates the file if it does not exist
static OSErr AssertPrefsFileOpen(Int16 privileges)
{
	Try_
	{
		ThrowIfNil_(sPrefFile);
		Try_
		{
			sPrefFile->OpenResourceFork(privileges);
		}
		Catch_(inErr)
		{
			// Create the resource fork if it doesn't exist
			if (inErr == fnfErr || inErr == eofErr) {
				sPrefFile->CreateNewFile(emSignature, emPrefsType);
				
				sPrefFile->OpenResourceFork(privileges);
			}
		
			// Non-fatal error. We are trying to open prefs twice in a row
			else if (!((inErr == opWrErr) &&
					(sPrefFile->GetResourceForkRefNum() != refNum_Undefined)))
				Throw_(inErr);
		}
		EndCatch_
		sPrefFileResID = sPrefFile->GetResourceForkRefNum();
	}
	Catch_(inErr)
	{
		return inErr;
	}
	EndCatch_
	return noErr;
}

static void ClosePrefsFile()
{
	Try_
	{
		if (sPrefFile)
		{
			if (sPrefFile->GetResourceForkRefNum() != refNum_Undefined)
				sPrefFile->CloseResourceFork();
			if (sPrefFile->GetDataForkRefNum() != refNum_Undefined)
				sPrefFile->CloseDataFork();
		}
	}
	Catch_(inErr)
	{
		Assert_(FALSE);
	}
	EndCatch_
	sPrefFileResID = refNum_Undefined;
	
	//
	// Now that the prefs are closed, make sure weÕre
	// not using some random file (like a plug-in).
	//
	CPrefs::UseApplicationResFile();		
}


#define NON_EXISTENT_NAME		"\pNONEXISTENTsj8432903849"

// ¥ Makes sure that the folder with a given spec exists inside preferences.
//   if it is not found, it creates one.
static OSErr AssurePreferencesSubfolder(pref_Strings whichFolder, FSSpec & folderSpec)
{
	FSSpec mainSpec = CPrefs::GetFilePrototype(CPrefs::MainFolder);
	// Find a folder. Create one if necessary
	::GetIndString( folderSpec.name, 300, whichFolder );
	OSErr err = noErr;
	if ( CFileMgr::FindWFolderInFolder(mainSpec.vRefNum, mainSpec.parID,
									folderSpec.name, 
									&folderSpec.vRefNum, &folderSpec.parID) != noErr )
	// Not found, must create it
		err = CFileMgr::CreateFolderInFolder( mainSpec.vRefNum, mainSpec.parID,
									 folderSpec.name, 
									 &folderSpec.vRefNum, &folderSpec.parID );
		if ( err != noErr )	// Also try application folder for lab settings
		{
			mainSpec = CPrefs::GetFilePrototype( CPrefs::NetscapeFolder );
			err = CFileMgr::FindWFolderInFolder( mainSpec.vRefNum, mainSpec.parID,
									folderSpec.name, 
									&folderSpec.vRefNum, &folderSpec.parID );
			if ( err != noErr )
				err = CFileMgr::CreateFolderInFolder( mainSpec.vRefNum, mainSpec.parID,
									 folderSpec.name, 
									 &folderSpec.vRefNum, &folderSpec.parID );
		}
		err = CFileMgr::FolderSpecFromFolderID( folderSpec.vRefNum, folderSpec.parID, folderSpec );
	return err;
}

static CStr255 BuildProxyString(CStr255 host, CStr255 port)
{
	CStr255 ret = host;
	if ((host.Length() > 0) && (port.Length() > 0))
	{
		ret += ":";
		ret += port;
	}
	return ret;
}

static LO_Color MakeLOColor( const RGBColor& rgbColor )
{
	LO_Color		temp;
	temp.red = rgbColor.red >> 8;
	temp.green = rgbColor.green >> 8;
	temp.blue = rgbColor.blue >> 8;
	return temp;
}

// Returns TRUE if resource existed
// If it does not exist, creates a resource of 0 length
static Boolean AssertResourceExist(ResType type, short id)
{
	Handle r = ::Get1Resource(type, id);
	if (r == NULL)
	{
		Handle h = ::NewHandle(0);
		ThrowIfNil_(h);
		::AddResource(h, type, id, CStr255::sEmptyString);
		ThrowIfResError_();
		
		SInt8 flags = ::HGetState(h);
		::HNoPurge(h);
		
		::SetResAttrs(h, ::GetResAttrs(h) | resPurgeable);	// Pref resources are purgeable
		if (type == STRINGLIST)
		{
			CStringListRsrc stringList(id);
			stringList.ClearAll();
		}
		::HSetState(h, flags);
		
		return FALSE;
	}
	else
		return TRUE;
}

/*****************************************************************************
 * struct CFolderSpec
 * Contains specs for the folder and a prototype spec for the file to be 
 * created inside this folder
 *****************************************************************************/ 

CFolderSpec::CFolderSpec()
{
	fFolder.vRefNum = fFolder.parID = refNum_Undefined;
	fFilePrototype.vRefNum = fFilePrototype.parID = refNum_Undefined;
}

// Sets the specs for this folder. Must figure out the prototype file specs too
OSErr CFolderSpec::SetFolderSpec(FSSpec newSpec, int folderID)
	/*
		This routine is called for two different sorts of folders, those whose locations are saved in
		the preferences, and those that are not.  For the former, |newSpec| is constructed from the saved
		location.  For the latter, it is uninitialized.
	*/
{
	FSSpec		debugSpec;
	OSErr		settingError;
	settingError = FSMakeFSSpec( newSpec.vRefNum, newSpec.parID, newSpec.name, &debugSpec);

	if ( settingError != noErr )
	{
		sDirtyOnRead = TRUE;
		Try_
		{
			switch ( (CPrefs::PrefEnum)folderID )
			{
				case CPrefs::DownloadFolder:
					ThrowIfOSErr_( FindFolder( kOnSystemDisk, kDesktopFolderType,
						kCreateFolder, &newSpec.vRefNum, &newSpec.parID ));
					ThrowIfOSErr_( CFileMgr::FolderSpecFromFolderID(newSpec.vRefNum, newSpec.parID, newSpec));
				break;
				case CPrefs::NewsFolder:
				{
					// OSErr err = AssurePreferencesSubfolder(newsFolderName, newSpec);
					OSErr err = fnfErr;		// Brutal hard coded err for a world without mail/news
					if ( err != noErr )
					{
						fFolder.vRefNum = fFolder.parID = fFilePrototype.vRefNum = fFilePrototype.parID = 0;
						return err;
					}
				}
				break;	
				case CPrefs::SecurityFolder:
				{
					OSErr err = AssurePreferencesSubfolder(securityFolderName, newSpec);
					if ( err != noErr )
					{
						fFolder.vRefNum = fFolder.parID = fFilePrototype.vRefNum = fFilePrototype.parID = 0;
						return err;
					}
				}
				break;	
				case CPrefs::DiskCacheFolder:
					// Caching folder. By default it is inside the main folder.
				{
					OSErr err = AssurePreferencesSubfolder(cacheFolderName, newSpec);
					if ( err != noErr )
					{
						fFolder.vRefNum = fFolder.parID = fFilePrototype.vRefNum = fFilePrototype.parID = 0;
						return err;
					}
				}
				break;
				
				case CPrefs::SARCacheFolder:
					// Archive Cache folder. By default it is inside the main folder.
				{
					OSErr err = AssurePreferencesSubfolder(sarCacheFolderName, newSpec);
					if ( err != noErr )
					{
						fFolder.vRefNum = fFolder.parID = fFilePrototype.vRefNum = fFilePrototype.parID = 0;
						return err;
					}
				}
				break;
				
				case CPrefs::MailFolder:
				{
					// OSErr err = AssurePreferencesSubfolder(mailFolderName, newSpec);
					OSErr err = fnfErr;		// Brutal hard coded err for a world without mail/news
					if ( err != noErr )
					{
							fFolder.vRefNum = fFolder.parID = fFilePrototype.vRefNum = fFilePrototype.parID = 0;
							return err;
					}
				}
				break;
				case CPrefs::NetHelpFolder:
				{
						/*
							[scc:] Essentially this same code appears in uapp.cp for resolving the "Essential Files" folder.
							It should be broken out into a separate static member function of |CFileMgr| a la FindWFolderInFolder.
						*/
		      FSSpec	netscapeFolderSpec = CPrefs::GetFolderSpec(CPrefs::NetscapeFolder);
						// Build a partial path to the guts folder starting from a folder we know (the Netscape folder)
		      Str255	partialPath;

					{
							// Get the name of the guts folder
						Str255 helpFolderName;
						GetIndString(helpFolderName, 300, nethelpFolderName);	

							// partialPath = ":" + netscapeFolderSpec.name + ":" + gutsFolderName;
							//	( this may _look_ cumbersome, but it's really the most space and time efficient way to catentate 4 pstrings )
						int dest=0;
						partialPath[++dest] = ':';
						for ( int src=0; src<netscapeFolderSpec.name[0]; )
							partialPath[++dest] = netscapeFolderSpec.name[++src];
						partialPath[++dest] = ':';
						for ( int src=0; src<helpFolderName[0]; )
							partialPath[++dest] = helpFolderName[++src];
						partialPath[0] = dest;
					}

						// Use the partial path to construct an FSSpec identifying the required guts folder
					if ( FSMakeFSSpec(netscapeFolderSpec.vRefNum, netscapeFolderSpec.parID, partialPath, &newSpec) == noErr )
						{	// Ensure that the folder exists (even if pointed to by an alias) and actually _is_ a folder
							Boolean targetIsFolder, targetWasAliased;
							ResolveAliasFile(&newSpec, true, &targetIsFolder, &targetWasAliased);
						}
				}
				break;
				case CPrefs::MailCCFile:
				{	
					CStr255 defaultName;
					newSpec = CPrefs::GetFilePrototype(CPrefs::MailFolder);
					GetIndString( defaultName, 300, mailCCfile );
					LString::CopyPStr(defaultName, newSpec.name, 32);
				}
				break;
				case CPrefs::NewsCCFile:
				{	
					CStr255 defaultName;
					newSpec = CPrefs::GetFilePrototype(CPrefs::MailFolder);
					GetIndString( defaultName, 300, newsCCfile );
					LString::CopyPStr(defaultName, newSpec.name, 32);
				}
				break;
				case CPrefs::MainFolder:	// These are all created dynamically			
				case CPrefs::NetscapeFolder:
				case CPrefs::RequiredGutsFolder:
				return noErr;
			}
		}
		Catch_( err )
		{	// Not much of an error handling, just assign 0 to all, which places files in Netscape folder
			newSpec.vRefNum = 0;
			newSpec.parID = 0;
		}
		EndCatch_
	}
	fFolder = newSpec;	// Assign the spec
	
	// New folder is the parent of the prototype file. Find the file ID of the new folder
	// and make it a parent of the prototype file
	(CStr63&)fFilePrototype.name = fFolder.name;
	CInfoPBRec cinfo;
	DirInfo	*dipb=(DirInfo *)&cinfo;
	dipb->ioNamePtr = (StringPtr)&fFilePrototype.name;
	dipb->ioVRefNum = fFolder.vRefNum;
	dipb->ioFDirIndex = 0;
	dipb->ioDrDirID = fFolder.parID;
	OSErr err = PBGetCatInfoSync(&cinfo);
	fFilePrototype.name[0] = 0; // there are places where a path is made from the raw prototype!
	if (err == noErr)
	{
		fFilePrototype.vRefNum = dipb->ioVRefNum;
		fFilePrototype.parID = dipb->ioDrDirID;
	}
	else
	{
		fFilePrototype.vRefNum = 0;
		fFilePrototype.parID = 0;
	}
	return settingError;
}

Boolean CFolderSpec::Exists()
{
	CStr63		name;
	CStr63		doesntExist( NON_EXISTENT_NAME );
	
	name = fFolder.name;
	
	return ( name != doesntExist );
}

//----------------------------------------------------------------------------------------
void CPrefs::Initialize()
// Initialize all statics
// This must be called before any other preference routines.
// OK, so would somebody please add a comment as to why this can't be done in the
// constructor?
//----------------------------------------------------------------------------------------
{
	UInt32 howMany = CountPrefsOfType(eFileSpecType);
	sFileIDs = (CFolderSpec **)XP_CALLOC(1, howMany * sizeof(CFolderSpec *));
	ThrowIfNil_( sFileIDs );
	for (int i=0; i<howMany; i++)
		sFileIDs[i] = new CFolderSpec;

	howMany = CountPrefsOfType(eColorType);
	sColors = (RGBColor *)XP_CALLOC(1, howMany * sizeof(RGBColor));
	ThrowIfNil_( sColors );
	
	fCharSetFonts = new LArray( sizeof( CCharSet ) );
	ThrowIfNil_( fCharSetFonts );
	

	// We will attempt to use the system's temporary items folder
	// for all of our temp files.

	// The folder is invisible to the user and is automatically 
	// emptied on system startup.
	
	// On entry, the static member sRealTemporaryFolder is false.  We only set it to
	// true if the system temp folder is satisfactory.  Otherwise, the download folder
	// will be used.

	if (noErr != ::FindFolder(
	 	kOnSystemDisk, 
		kTemporaryFolderType, 
		kCreateFolder, 
		&sTemporaryFolder.vRefNum, 
		&sTemporaryFolder.parID))
	{
		// Note: sTemporaryFolder is not the spec of anything, it is a prototype
		// for a file WITHIN the temporary folder.
		return;
	}
	
	// Bug #105473
	// 	Customer running "At Ease" cannot use mail because the temporary items folder
	//	does not have write access.  So check for write access by trying to create a file.
	//	If the attempt fails, fall back on our "Download folder" strategy.
	//		- jrm 98/02/10
	FSSpec trialSpec = sTemporaryFolder;
	*(CStr63*)&trialSpec.name = "Foobar";
	if (noErr != ::FSpCreate(&trialSpec, 'GDAY', 'GDAY', 0))
		return;
	// Well, if it passed that test, it's hunky dory.  Clean up, and then record the fact
	// that the system temp folder is the one to use.
	::FSpDelete(&trialSpec);
	sRealTemporaryFolder = true;
}

//----------------------------------------------------------------------------------------
FSSpec CPrefs::GetTempFilePrototype()
//	Just as well the ::FindFolder() call above seldom fails. In fact, many clients of this routine
//	are expecting it to return the temporary folder PROTOTYPE, not the spec.  This
//	was what was happening if sRealTemporaryFolder was true.  Thus, if we fall back on
//	the download folder, we need to have the prototype, and not the spec of the folder.
//	As of 98/02/11, there was confusion on this point.  Fortunately, all of our routines
//	that convert a spec into a path will tolerate a "prototype" whose name is set to
//	an empty string.  That's why callers who were using the result this way were succeeding
//	for the temporary folder case (the string was empty) but in the download-folder case we were
//	previously returning GetFolderSpec(DownloadFolder), which was wrong.  It failed, in particular,
//	when the download folder was the root of a volume.
//----------------------------------------------------------------------------------------
{
	if (sRealTemporaryFolder)
		return sTemporaryFolder;
	return GetFilePrototype(MainFolder);
}

//----------------------------------------------------------------------------------------
int CPrefs::ColorPrefCallback(const char */*prefString*/, void */*data*/)
//----------------------------------------------------------------------------------------
{
	LO_Color loColor = MakeLOColor(GetColor(TextFore));
	LO_SetDefaultColor( LO_COLOR_FG , loColor.red, loColor.green, loColor.blue);
	loColor = MakeLOColor(GetColor(TextBkgnd));
	LO_SetDefaultColor( LO_COLOR_BG, loColor.red, loColor.green, loColor.blue);
	loColor = MakeLOColor(GetColor(Anchor));
	LO_SetDefaultColor( LO_COLOR_LINK, loColor.red, loColor.green, loColor.blue);
	loColor = MakeLOColor(GetColor(Visited));
	LO_SetDefaultColor( LO_COLOR_VLINK, loColor.red, loColor.green, loColor.blue);
	LO_SetUserOverride(!GetBoolean(UseDocumentColors));

	return 0;	// insert complaint about this later	
}

int
CPrefs::FSSpecPrefCallback(const char *prefString, void *enumValue)
{
	PrefEnum	thePref = (PrefEnum)enumValue;

	AliasHandle aliasH = NULL;
	int size;
	void* alias;
	if (PREF_CopyBinaryPref(prefString, &alias, &size ) == 0)
	{
		PtrToHand(alias, &(Handle)aliasH, size);
		XP_FREE(alias);
	}

	FSSpec	target;
	Boolean	wasChanged;
	OSErr	iErr = ResolveAlias(nil, aliasH, &target, &wasChanged);
	DisposeHandle((Handle)aliasH);
	SetFolderSpec(target, thePref);
	return 0;	// You don't even want to know my opinion of this!
}

void
CPrefs::RegisterPrefCallbacks()
{
	PREF_RegisterCallback(	"browser.download_directory", FSSpecPrefCallback,
							(void *)DownloadFolder);
//	PREF_RegisterCallback("", FSSpecPrefCallback, (void *)NetscapeFolder);
//	PREF_RegisterCallback("", FSSpecPrefCallback, (void *)MainFolder);
//	PREF_RegisterCallback("", FSSpecPrefCallback, (void *)UsersFolder);
	PREF_RegisterCallback(	"browser.cache.directory", FSSpecPrefCallback,
							(void *)DiskCacheFolder);
	PREF_RegisterCallback(	"mail.signature_file", FSSpecPrefCallback,
							(void *)SignatureFile);
//	PREF_RegisterCallback(	"browser.gif_backdrop_file", FSSpecPrefCallback,
//							(void *)GIFBackdropFile);
	PREF_RegisterCallback(	"mail.directory", FSSpecPrefCallback,
							(void *)MailFolder);
	PREF_RegisterCallback(	"news.directory", FSSpecPrefCallback,
							(void *)NewsFolder);
//	PREF_RegisterCallback("", FSSpecPrefCallback, (void *)SecurityFolder);
	PREF_RegisterCallback(	"mail.cc_file", FSSpecPrefCallback,
							(void *)MailCCFile);
	PREF_RegisterCallback(	"news.cc_file", FSSpecPrefCallback,
							(void *)NewsCCFile);
	PREF_RegisterCallback(	"editor.html_editor", FSSpecPrefCallback,
							(void *)HTMLEditor);
	PREF_RegisterCallback(	"editor.image_editor", FSSpecPrefCallback,
							(void *)ImageEditor);
//	PREF_RegisterCallback("", FSSpecPrefCallback, (void *)RequiredGutsFolder);
//	PREF_RegisterCallback("", FSSpecPrefCallback, (void *)SARCacheFolder);
//	PREF_RegisterCallback("", FSSpecPrefCallback, (void *)NetHelpFolder);

	// register color pref callbacks too
	PREF_RegisterCallback("browser.foreground_color", ColorPrefCallback, nil);
	PREF_RegisterCallback("browser.anchor_color", ColorPrefCallback, nil);
	PREF_RegisterCallback("browser.visited_color", ColorPrefCallback, nil);
	PREF_RegisterCallback("browser.background_color", ColorPrefCallback, nil);
	PREF_RegisterCallback("browser.use_document_colors", ColorPrefCallback, nil);
}

// Returns true iff startup should be aborted
CPrefs::PrefErr CPrefs::DoRead( LFile * file, short fileType )
{
	if ( sPrefFile )
	{
		ErrorManager::PlainAlert( mPREFS_CANNOT_OPEN_SECOND_ALERT );
		return CPrefs::eAbort;
	}
	sPrefFile = file;
	
	CPrefs::PrefErr result = InitPrefsFolder( fileType );
	if ( result == CPrefs::eAbort )
		return result;
	
	// fix-me must handle a return code here (if it returns false, we need to quit or do
	// something intelligent
		
	if ( !sPrefFile ) {
		sPrefFile = GetFileForPreferences();
	}
	
	FSSpec prefSpec;
	sPrefFile->GetSpecifier(prefSpec);
	char* prefFile = CFileMgr::PathNameFromFSSpec(prefSpec, true);
	
	// Note: we call PREF_Init(NULL) in main() in uapp.cp;
	// the call below does not re-initialize but loads the user prefs file.
	if ( PREF_Init(prefFile) == JS_TRUE ) {
		sPrefFileVersion = 4;
	}
	XP_FREE(prefFile);
		
	// Read optional profile.cfg (per-profile configuration) file
	FSSpec profileConfig;
	profileConfig.vRefNum = prefSpec.vRefNum;
	profileConfig.parID = prefSpec.parID;
	LString::CopyPStr("\pprofile.cfg", profileConfig.name, 32);
	if (CFileMgr::FileExists(profileConfig)) {
		PREF_ReadLockFile( CFileMgr::PathNameFromFSSpec(profileConfig, true) );
	}
	
	// Read the lock/config file, looking in both the 
	// Users folder and the Essential Files folder
	Boolean lockLoaded = false;
	FSSpec lockSpec = CPrefs::GetFilePrototype(CPrefs::RequiredGutsFolder);		
	GetIndString(lockSpec.name, 300, configFile);
	
	if (!CFileMgr::FileExists(lockSpec)) {
		lockSpec = CPrefs::GetFilePrototype(CPrefs::UsersFolder);
		GetIndString(lockSpec.name, 300, configFile);
	}
	if (CFileMgr::FileExists(lockSpec))
	{
		char* lockFile = CFileMgr::PathNameFromFSSpec(lockSpec, true);
	
		if (lockFile) {
			int lockErr = PREF_ReadLockFile(lockFile);
			lockLoaded = (lockErr == PREF_NOERROR);
			XP_FREE(lockFile);
			
			if (lockErr == PREF_BAD_LOCKFILE) {
				CStr255 errStr;
				GetIndString(errStr, CUserProfile::kProfileStrings, CUserProfile::kInvalidConfigFile);
				ErrorManager::PlainAlert(errStr);
				return eAbort;
			}
		}
	}
	
	// The presence of a 'Lock' application resource means
	// we should abort if the lock file failed to load
	if ( !lockLoaded && ::GetResource('Lock', 128) != nil )
	{
		CStr255 errStr;
		GetIndString(errStr, CUserProfile::kProfileStrings, CUserProfile::kConfigFileError);
		ErrorManager::PlainAlert(errStr);
		return eAbort;
	}

	OpenAnimationFile(lockSpec, profileConfig);

	ReadAllPreferences();
	
	{	// Connect to Internet Config, passing path of current
		// profile directory.  This allows each profile to
		// contain its own IC Preferences file.
		
		// We do not want a stupid break to source debugger every time
		StValueChanger<EDebugAction> okayToFail(gDebugThrow, debugAction_Nothing);
		CInternetConfigInterface::ConnectToInternetConfig(/*&prefSpec*/);
	}
	
	RegisterPrefCallbacks();

	// ¥Êhandle the upgrade path that involves us telling the user
	// that we're creating "Netscape Users", etc.
	// (I don't know why this is handled here)
	if ( result == eNeedUpgrade )
	{
		FSSpec		prefsFolder;
		FSSpec		oldPrefsFolder;
		ProfileErr	profResult;
		
		oldPrefsFolder = CPrefs::GetFolderSpec( CPrefs::MainFolder );
		profResult = CUserProfile::HandleUpgrade( prefsFolder, &oldPrefsFolder );
		if ( profResult == eOK || profResult == eRunAccountSetup )
		{
			// ¥ user OK'ed it, so we set the prefs folder to point to the new
			// place now
			CPrefs::SetFolderSpec( prefsFolder, CPrefs::MainFolder );
		}
		else
			// ¥ user chose to Quit
			return eAbort;
		if ( profResult == eRunAccountSetup )
			result = eRunAccountSetup;
	}

	// --EKit: Prevent failover if config is locked on auto-proxy
	if ( IsLocked(ProxyConfig) && GetLong(ProxyConfig) == PROXY_STYLE_AUTOMATIC ) {
		NET_SetNoProxyFailover();
	}
	
	return result;
}

void CPrefs::DoWrite()
{
	if (sPrefFile == NULL)
		sPrefFile = GetFileForPreferences();
	if (sPrefFile == NULL)
		return;
	if (!sDirty)
		return;
	WriteAllPreferences();
}

// Reads in all the preferences
void CPrefs::ReadAllPreferences()
{
	sReading = TRUE;
	Try_	
	{
		sDirtyOnRead = FALSE;
		AssertPrefsFileOpen(fsRdPerm);

		int i = FIRSTPREF;
		while(prefLoc[i].prefID != LastPref)
			ReadPreference((PrefEnum)i++);
		ReadMimeTypes();
		
		ReadCharacterEncodings();
		
		CNotifierRegistry::ReadProtocolHandlers();
		sDirty = sDirtyOnRead;
		// Needs to be done after all prefs have been read
		NET_SelectProxyStyle( (NET_ProxyStyle) GetLong( ProxyConfig) );
	}
	Catch_(inErr)
	{		
		ClosePrefsFile();
		sReading = FALSE;
		Throw_(inErr);
	}
	EndCatch_
	sReading = FALSE;
	ClosePrefsFile();
}

void CPrefs::WriteAllPreferences()
{
	Try_
	{
		ThrowIfOSErr_(AssertPrefsFileOpen(fsRdWrPerm));

		// --xp prefs 
		FSSpec prefSpec;
		sPrefFile->GetSpecifier(prefSpec);
		char* prefFile = CFileMgr::PathNameFromFSSpec(prefSpec, true);
		
		int err = PREF_SavePrefFileAs(prefFile);
		ThrowIfOSErr_(err);
		
		// -- WritePreference now skips all prefs except for print record 
		WritePreference( CPrefs::PrintRecord );

		CNotifierRegistry::WriteProtocolHandlers();
		AssertResourceExist(REVISION, REVISION_RES_ID);
		Handle	res = ::Get1Resource( REVISION, REVISION_RES_ID );
		if (res && *res)
		{
			SInt8 flags = ::HGetState(res);
			::HNoPurge(res);
			if ((::GetHandleSize(res) < sizeof(long)) || (*(long *)*res != PREF_REVISION_NUMBER))
			{
				::SetHandleSize(res, sizeof(long));
				ThrowIfMemError_();
				*(long *)*res = PREF_REVISION_NUMBER;
				::ChangedResource(res);
				::WriteResource(res);
			}
			::HSetState(res, flags);
		}
			
	}
	Catch_(inErr)
	{
		ClosePrefsFile();
		ErrorManager::PlainAlert( mPREFS_CANNOT_WRITE );
		ErrorManager::ErrorNotify(inErr, GetPString(THE_ERROR_WAS));
	}
	EndCatch_
	ClosePrefsFile();
}

// Gets the right resource
// First tries the pref file resource fork,
// On fail, try the application resource fork
// If they both fail, throw
void CPrefs::ReadPreference(short index)
{
	switch(prefLoc[index].resType)	{
	case NOTSAVED:
		InitializeUnsavedPref(index);
		break;
	case STRINGLIST:
		{
			CStr255 s;
			
			if ( sPrefFileVersion != 3 && IsNewPrefFormat(index) )
			{
				// --xp prefs temp: Do this to send init prefs values to
				// the right modules--
				// to be REPLACED by modules initing own prefs via callbacks
				PostInitializePref(prefLoc[index].prefID, true);
				
				return;
			}
			
			// Otherwise, read from old-format Prefs file
			if (UsePreferencesResFile())
			{
				Handle stringListHandle = ::Get1Resource(STRINGLIST, prefLoc[index].resID);
				
				if (stringListHandle && *stringListHandle)
				{
					if (::GetHandleSize(stringListHandle) >= sizeof(short))
					{
						CStringListRsrc stringList(prefLoc[index].resID);
						
						if (stringList.CountStrings() >= prefLoc[index].refCon &&
							prefLoc[index].refCon > 0)	// We are OK, we have a string
						{
							stringList.GetString(prefLoc[index].refCon, s);
							goto postprocessstring;
						}
					}
					else
					{
						::RemoveResource(stringListHandle);
						::DisposeHandle(stringListHandle);
					}
				}
			}
			// drop through
			sDirtyOnRead = TRUE;
			
	postprocessstring:
			switch (prefLoc[index].prefType)	{
			case eStringType:
				// Convert 3.0 ports from strings to ints
				switch (prefLoc[index].prefID) {
					case FTPProxyPort:
					case GopherProxyPort:
					case HTTPProxyPort:
					case WAISProxyPort:
					case SSLProxyPort:
					case SOCKSPort:
						Int32 value;
						if (sscanf(s, "%ld", &value) == 1)
							SetLong(value, prefLoc[index].prefID);
						break;
					
					default:					
						SetString(s, prefLoc[index].prefID);
				}
				break;
			case eBooleanType:
				Boolean bvalue;
				if (strcasecomp(s, "FALSE") == 0)
					bvalue = false;
				else if (strcasecomp(s, "TRUE") == 0)
					bvalue = true;
				else
					break;
				switch (prefLoc[index].prefID) {
					case EnableJava:
					case EnableJavascript:
						bvalue = ! bvalue;
						break;
					case UseInlineViewSource:
						sViewSourceInline = bvalue;
						break;
				}
				SetBoolean(bvalue, prefLoc[index].prefID);
				break;
			case eLongType:
				Int32 value;
				if (sscanf(s, "%ld", &value) != 1)
					break;
					
				// Magic conversions of 3.0 prefs to 4.0 units
				if (prefLoc[index].prefID == PrintBackground)
					SetBoolean (value != 0, PrintBackground);	// was a long, now a boolean
				else {
					switch (prefLoc[index].prefID) {
						case DiskCacheSize:
							value = value / DISK_CACHE_SCALE;	// was in bytes, now in K
							break;
						case BufferSize:
							value = value * BUFFER_SCALE;		// was in K, now in bytes
							break;
						case MailHeaderDisplay:
							switch (value) {					// see msgprefs.cpp
								case SHOW_ALL_HEADERS:
									value = 2;
									break;
								case SHOW_SOME_HEADERS:
									value = 1;
									break;
								case SHOW_MICRO_HEADERS:
									value = 0;
									break;
							}
							break;
					}
					SetLong(value, prefLoc[index].prefID);
				}
				break;
			case eColorType:
				long r,g,b;
				RGBColor c;
				if (sscanf(s, "%ld %ld %ld", &r, &g, &b) != 3)
					break;
				c.red = r;
				c.green = g;
				c.blue = b;

				// Conversion from 3.0 colors. Use default color if
				// the corresponding "use custom" box wasn't checked.
				switch (prefLoc[index].prefID) {
					case TextBkgnd:
						if (GetLong(BackgroundColors) != CUSTOM_BACKGROUND)
							c = GetColor(WindowBkgnd);
						break;
					case TextFore:
						if (!GetBoolean(CustomTextColors))
							c = GetColor(Black);
						break;
					case Anchor:
						if (!GetBoolean(CustomLinkColors))
							c = GetColor(Blue);
						break;
					case Visited:
						if (!GetBoolean(CustomVisitedColors))
							c = GetColor(Magenta);
						break;
				}

				SetColor(c, prefLoc[index].prefID);
				break;
			default:
				Assert_(FALSE);;
			}
		}
		break;
	case ALIASREC:
		{
			// Read old alias pref from resource
			FSSpec folder;
			folder.vRefNum = folder.parID = refNum_Undefined;
			LString::CopyPStr(NON_EXISTENT_NAME, folder.name, 32);
			AliasHandle a = NULL;
			Boolean gotAlias = false;
			Boolean fromResource = false;
			
			// XP prefs: Read alias as binary type
			if ( sPrefFileVersion != 3 && IsNewPrefFormat(index) )
			{
				int size;
				void* alias;
				if (PREF_CopyBinaryPref( prefLoc[index].prefName, &alias, &size ) == 0)
				{
					PtrToHand(alias, &(Handle) a, size);
					XP_FREE(alias);
				}
			}
			else if (UsePreferencesResFile())
			{
				a = (AliasHandle)::Get1Resource( ALIASREC, prefLoc[index].resID );
				fromResource = true;
			}

			if (a && *a) {
				Boolean changed;
				SInt8 flags = ::HGetState((Handle) a);
				::HNoPurge((Handle )a);
				OSErr err = ::ResolveAlias( NULL, a, &folder, &changed );
				gotAlias = (err == noErr);				
				if (!gotAlias)
					folder.vRefNum = folder.parID = refNum_Undefined;
				::HSetState((Handle) a, flags);

				if (fromResource) {
					if (gotAlias) {
						// 3.0 format alias: convert to an xp preference
						::HLock((Handle) a);
						PREF_SetBinaryPref( prefLoc[index].prefName, (void*) *a,
							GetHandleSize((Handle) a) );
						::HUnlock((Handle) a);
					}
				}
				else {
					::DisposeHandle((Handle) a);
				}
			}

			SetFolderSpec(folder, prefLoc[index].prefID);
			
			// If there is no user preference value for the alias, or the alias
			// is invalid, then SetFolderSpec fills in the default location.
			// We need to reflect it into a default xp pref:
			if (!gotAlias) {
				folder = GetFolderSpec(prefLoc[index].prefID);
				AliasHandle	aliasH;
				OSErr err = NewAlias(nil, &folder, &aliasH);
				if (err == noErr) {
					Size bytes = GetHandleSize((Handle) aliasH);
					HLock((Handle) aliasH);
					PREF_SetDefaultBinaryPref( prefLoc[index].prefName, *aliasH, bytes );
					DisposeHandle((Handle) aliasH);
				}
			}
		}
		break;
	case PRINTRECORD:
		{
			if (UsePreferencesResFile())
			{
				Handle printHandle = ::Get1Resource( PRINTRECORD, prefLoc[index].resID);
				if ( printHandle )
				{
					if ( !*printHandle )
						::LoadResource( printHandle );
					if ( *printHandle && (::GetHandleSize(printHandle) >= sizeof(TPrint)))
					{
						::HNoPurge( printHandle );
						::DetachResource( printHandle );
						sPrintRec = (THPrint)printHandle;
					}
				}
			}
			if (sPrintRec == NULL)
			Try_	{
				sPrintRec = UPrintingMgr::GetDefaultPrintRecord();
			}
			Catch_(inErr)	{
			}
			EndCatch_
		}
		break;
	default:
		Assert_(FALSE);;
	}
}

// Writes the resource
// Make sure that we are only saving into pref file resource fork,
void CPrefs::WritePreference(short index)
{
	ThrowIf_(!UsePreferencesResFile());
	switch(prefLoc[index].resType)	{
	case ALIASREC:
		{
			// --ML ?? this comment:
			// Do not save TemporaryFolder special case. Do not save it if
			// it points to the temp directory
		}
		break;
	case PRINTRECORD:
		{
			if (sPrintRec)
			{
				AssertResourceExist(PRINTRECORD, prefLoc[index].resID);
				Handle printHandle = ::Get1Resource( PRINTRECORD, prefLoc[index].resID );
				ThrowIfNil_(printHandle);
				ThrowIfNil_(*printHandle);
				SInt8 flags = ::HGetState(printHandle);
				::HNoPurge(printHandle);
				StHandleLocker lock((Handle)sPrintRec);
				ThrowIfOSErr_(::PtrToXHand(*sPrintRec, printHandle, sizeof(TPrint)));
				::ChangedResource( printHandle );
				::WriteResource(printHandle);
				::HSetState(printHandle, flags);
			}
		}
		break;
	default:
		break;
	}
}

// NOTE: the caller of this method is responsible for disposing of the
// returned window data handle

Handle CPrefs::ReadWindowData( short resID )
{
	Handle theWindowData = NULL;
	try {
		AssertPrefsFileOpen( fsRdPerm );
		if (UsePreferencesResFile()) {		
			theWindowData = ::Get1Resource(WINDOWREC, resID);
			if (theWindowData) {
				::HNoPurge(theWindowData);
				::DetachResource(theWindowData);
				ThrowIfResError_();
			}
		}
	}
	catch (...) {
		if (theWindowData != NULL)
			::ReleaseResource(theWindowData);
		
		theWindowData = NULL;		
	}

	ClosePrefsFile();		
	return theWindowData;
}

void CPrefs::WriteWindowData( Handle data, short resID )
{
	try
		{
		ThrowIfOSErr_(AssertPrefsFileOpen(fsRdWrPerm));
		UsePreferencesResFile();
		AssertResourceExist( WINDOWREC, resID );

		Handle		resHandle = ::Get1Resource( WINDOWREC, resID );
		ThrowIfNil_( resHandle );
		ThrowIfNil_( *resHandle );
		SInt8 flags = ::HGetState(resHandle);
		::HNoPurge(resHandle);

		long		size;
		size = ::GetHandleSize( data );

		::SetHandleSize( resHandle, size );
		ThrowIfMemError_();

		::BlockMoveData( *data, *resHandle, size );
		::ChangedResource(resHandle);
		::WriteResource(resHandle);
		::HSetState(resHandle, flags);
		}
	catch (...)
		{
		
		}

	ClosePrefsFile();		
}
  
Boolean CPrefs::UsePreferencesResFile()
{
	if (sPrefFileResID != refNum_Undefined)
	{
		::UseResFile(sPrefFileResID);
		return TRUE;
	}
	return FALSE;
}

Boolean CPrefs::UseApplicationResFile()
{
	::UseResFile(LMGetCurApRefNum());
	return TRUE;
}

// Find the Preference folder -- Three cases:
//  1. User launched a specific prefs file. Its parent is the prefs folder.
//  2. Single profile found. Prefs folder is the default.
//  3. Multiple profiles. Prompt user for prefs folder.
CPrefs::PrefErr CPrefs::InitPrefsFolder(short fileType)
{
	PrefErr result = eOK;
	Boolean showProfiles = false;
	FSSpec prefsFolder, prefSpec;
	
	CUserProfile::InitUserProfiles();
	
	// A "profile" fileType means the Profile Manager was launched,
	// so always show the profile selection dialog.
	if (fileType == FILE_TYPE_PROFILES)
		showProfiles = true;
	
	// Actually for the time being we always want the Profile Manager to appear so just set
	// showProfiles to true.  Fix things when we get the external app to invoke the Profile
	// Manager back into the public source tree.
	showProfiles = true;
	
	FSSpec usersFolder;
	Boolean foundPrefs = FindDefaultPreferencesFolder(usersFolder, usersFolderName, true);
	if (!foundPrefs) {
		ErrorManager::PlainAlert(mPREFS_CANNOT_CREATE_PREFS_FOLDER);
		return eAbort;
	}
	
	CPrefs::SetFolderSpec( usersFolder, CPrefs::UsersFolder );
		
	if ( sPrefFile )
	{
		sPrefFile->GetSpecifier( prefSpec );
		ThrowIfOSErr_( CFileMgr::FolderSpecFromFolderID(
			prefSpec.vRefNum, prefSpec.parID, prefsFolder ) );
	}
	else
	
	{
		ProfileErr		err;
		
		usersFolder = CPrefs::GetFilePrototype( CPrefs::UsersFolder );
		if ( fileType != STARTUP_TYPE_NETPROFILE)
			// ¥Êhandle user profile here
			err = CUserProfile::GetUserProfile( usersFolder, prefsFolder, showProfiles, fileType );
		else
			err = CUserProfile::CreateNetProfile(usersFolder, prefsFolder);
		
		// ¥ first time through we get the "eNeedUpgrade" error
		if ( err == eNeedUpgrade )
		{
			result = eNeedUpgrade;

			// ¥Êpoint to the old 3.0 Netscape Ä folder 
			// (but don't create it if it doesn't exist).
			if ( !FindDefaultPreferencesFolder( prefsFolder, prefFolderName, false ) )
			{
				// ¥ no Netscape Ä, so do the upgrade right now
				// !! if returns error then need to Quit...
				sPrefFileVersion = 0;
				err = CUserProfile::HandleUpgrade( prefsFolder );
				
				if ( err == eOK )
					result = eOK;
				else if ( err == eRunAccountSetup )
					result = eRunAccountSetup; // ¥Êare we having fun yet?
				else
					result = eAbort;
			}
		}
		else if ( err == eRunAccountSetup )
			result = eRunAccountSetup;
		else if ( (err == eUserCancelled) || (err == eUnknownError) )
			result = eAbort;
	}
	
	CPrefs::SetFolderSpec( prefsFolder, CPrefs::MainFolder );
	return result;
}

/*	
	FindRequiredGutsFolder
	
	Algorithm (given definitons of usersFolder, mainFolder, netscapeFolder) 
	inLaunchWithPrefs is true if user double clicked on a preferences folder.
	
	if inLaunchWithPrefs
		try to find in the mainFolder (parent of folder user clicked on)
		try to find in the netscapeFolder (where app is)
		try to find in the usersFolder (where the user prefs directories are)
	
	if !inLaunchWithPrefs
		try to find in the app folder
		try to find in the Netscape Ä folder (ie prefs)	
*/

Boolean CPrefs::FindRequiredGutsFolder(Boolean inLaunchWithPrefs)
{
	FSSpec	usersFolder;		// always the "Netscape Users" in the preferences folder
	FSSpec	mainFolder;				// if inLaunchWithPrefs, parent of prefs folder, otherwise "Netscape Ä"
	FSSpec	netscapeFolder;			// the folder containing the app
	FSSpec	gutsFolder;
	OSErr	tempErr;
	
	::GetIndString( gutsFolder.name, 300, prefRequiredGutsName );

	// really get directory IDs for these folders (not parIDs)
	netscapeFolder = GetFilePrototype(NetscapeFolder);
	usersFolder = GetFilePrototype(UsersFolder);
	mainFolder = GetFilePrototype(MainFolder);
	
	// see note below, but basically we are using parID of the gutsFolder
	// and of the parent foldres to mean the IDs of the folders
	if (inLaunchWithPrefs)
	{
		tempErr = CFileMgr::FindWFolderInFolder(mainFolder.vRefNum, mainFolder.parID, gutsFolder.name ,&gutsFolder.vRefNum, &gutsFolder.parID);
		if (noErr != tempErr)
		{
			tempErr = CFileMgr::FindWFolderInFolder(netscapeFolder.vRefNum, netscapeFolder.parID, gutsFolder.name ,&gutsFolder.vRefNum, &gutsFolder.parID);
			if (noErr != tempErr)
			{
				tempErr = CFileMgr::FindWFolderInFolder(usersFolder.vRefNum, usersFolder.parID, gutsFolder.name ,&gutsFolder.vRefNum, &gutsFolder.parID);
			}
		}	
	}
	else
	{
		tempErr = CFileMgr::FindWFolderInFolder(netscapeFolder.vRefNum, netscapeFolder.parID, gutsFolder.name ,&gutsFolder.vRefNum, &gutsFolder.parID);
		if (noErr != tempErr)
		{
			tempErr = CFileMgr::FindWFolderInFolder(mainFolder.vRefNum, mainFolder.parID, gutsFolder.name ,&gutsFolder.vRefNum, &gutsFolder.parID);
		}
	}
	
	// return false in case of error
	if (noErr == tempErr)
	{
		FSSpec toSet;
		
		// so what we really got from FindWFolderInFolder is the directory ID
		// (a shorthand reference to a specification).  Now lets convert it
		// into a proper specification for a folder
		CFileMgr::FolderSpecFromFolderID(gutsFolder.vRefNum, gutsFolder.parID, toSet);

		SetFolderSpec(toSet, CPrefs::RequiredGutsFolder);
		return (true);
	}
	else
	{
		return (false);
	}
}


// All preferences designated "NOTSAVED" are initialized here
void CPrefs::InitializeUnsavedPref(short index)
{
	switch(prefLoc[index].prefID)	{
		case NetscapeFolder:
			break;		// Initialized automatically on startup
		case MainFolder:
		case UsersFolder:
		case RequiredGutsFolder:
			break;		// Moved to InitPrefsFolder
		case NewsFolder:
		case SecurityFolder:
		case SARCacheFolder:
        case NetHelpFolder:
                        FSSpec folder;
                        folder.vRefNum = folder.parID = refNum_Undefined;
                        SetFolderSpec(folder, prefLoc[index].prefID);
                        break;
		case WindowBkgnd:
			RGBColor c;
			c.red = c.green = c.blue = 0xC0C0;
			SetColor(c, prefLoc[index].prefID);
			break;
		case Black:
			c.red = c.green = c.blue = 0;
			SetColor(c, prefLoc[index].prefID);
			break;		
		case White:
			c.red = c.green = c.blue = 0xFFFF;
			SetColor(c, prefLoc[index].prefID);
			break;
		case Blue:
			c.red = c.green = 0;
			c.blue = 0xFFFF;
			SetColor(c, prefLoc[index].prefID);
			break;
		case Magenta:
			c.red = 0x5500;
			c.green = 0x1A00;
			c.blue = 0x8B00;
			SetColor(c, prefLoc[index].prefID);
			break;
		default:
			 Assert_(FALSE);;	// Unsaved prefs need to be initialized statically
	}
}

void CPrefs::SetModified()
{
	sDirty = TRUE;
}

// Strings
Boolean CPrefs::SetString(const char* newString, PrefEnum id)
{
	Boolean changed = TRUE;

	if ( IsNewPrefFormat(id) )
	{
		changed = PREF_SetCharPref( prefLoc[id].prefName, newString );
	}

	PostInitializePref(id, changed);
	return changed;
}

// Booleans
Boolean CPrefs::SetBoolean(Boolean b, PrefEnum id)
{
	Boolean changed = FALSE;
	
	if ( IsNewPrefFormat(id) )
	{
		changed = PREF_SetBoolPref( prefLoc[id].prefName, b );
	}
	PostInitializePref(id, changed);
	return changed;
}

// Longs
Boolean CPrefs::SetLong(Int32 value, PrefEnum id)
{
	Boolean changed = FALSE;
	switch(id)	// Constrain the values
	{
		case Connections:
			ConstrainTo( CONNECTIONS_MIN, CONNECTIONS_MAX, value );
			break;
		case BufferSize:
			ConstrainTo( BUFFER_MIN, BUFFER_MAX, value );
			break;
		case DiskCacheSize:	
			unsigned long		maxSize;
			FSSpec				diskCacheSpec;
			diskCacheSpec = GetFolderSpec(DiskCacheFolder );
			maxSize = GetFreeSpaceInBytes( diskCacheSpec.vRefNum ) / DISK_CACHE_SCALE;
			ConstrainTo( DISK_CACHE_MIN, maxSize, value );
			break;
		case DaysTilExpire:
			if (value!= -1)
				ConstrainTo( EXPIRE_MIN, EXPIRE_MAX, value );
			break;
		case PrintFlags:
			ConstrainTo( PRINT_CROPPED, PRINT_RESIZED, value );
			break;
		case NewsArticlesMax:
			ConstrainTo( NEWS_ARTICLES_MIN, NEWS_ARTICLES_MAX, value );
			break;
		case CheckDocuments:
			ConstrainTo( (long)CU_CHECK_PER_SESSION, (long)CU_NEVER_CHECK,value );
			break;
		case BackgroundColors:
			break;
		case MessageFontStyle:
			ConstrainTo(MSG_PlainFont, MSG_BoldItalicFont, value);
			break;
		case MessageFontSize:
			ConstrainTo(MSG_NormalSize, MSG_Smaller, value);
			break;
		case ProxyConfig:
			ConstrainTo(PROXY_STYLE_MANUAL, PROXY_STYLE_NONE, value);
			break;
		case StartupAsWhat:
			ConstrainTo(STARTUP_BROWSER, STARTUP_VISIBLE, value);
			break;
		case BiffTimeout:
			ConstrainTo(1, 1200, value);
			break;
		case AskMozPassword:
			ConstrainTo(-1, 1, value);	// See secnav.h, SECNAV_SetPasswordPrefs
			break;
		case SortMailBy:
		case SortNewsBy:
			ConstrainTo(MAIL_SORT_BY_DATE, MAIL_SORT_BY_SENDER, value);	// See secnav.h, SECNAV_SetPasswordPrefs
			break;
		default:
			break;
	}
	if ( IsNewPrefFormat(id) )
	{
		changed = PREF_SetIntPref( prefLoc[id].prefName, value );
	}
	PostInitializePref(id, changed);
	return changed;
}

// RGB color
Boolean CPrefs::SetColor(const RGBColor& newColor, PrefEnum id)
{
	Boolean changed = FALSE;

	if ( IsNewPrefFormat(id) )
	{
		changed = PREF_SetColorPref( prefLoc[id].prefName,
			newColor.red >> 8, newColor.green >> 8, newColor.blue >> 8 );
	}
	else {
		if ((sColors[id - FIRSTCOLOR].red != newColor.red) ||
			(sColors[id - FIRSTCOLOR].green != newColor.green) ||
			(sColors[id - FIRSTCOLOR].blue != newColor.blue))
			changed = TRUE;
		sColors[id - FIRSTCOLOR] = newColor;
	}
	PostInitializePref(id, changed);	
	return changed;
}

extern const char* CacheFilePrefix;
Boolean CPrefs::SetFolderSpec( const FSSpec& newSpec , PrefEnum id )
{
	Boolean changed;
	
	FSSpec	oldSpec = sFileIDs[id - FIRSTFOLDER]->GetFolderSpec();
	CStr63	oldName( oldSpec.name );
	CStr63	newName( newSpec.name );
	changed = 	!( oldSpec.vRefNum == newSpec.vRefNum ) &&
							( oldSpec.parID == newSpec.parID ) &&
							( newName == oldName );
	if (changed)
	if ((id == DiskCacheFolder) && !sReading)
	{
		NET_SetDiskCacheSize( 0 );	// A hack to clean up the old cache directory
		NET_CleanupCacheDirectory( "", CacheFilePrefix );
		NET_SetDiskCacheSize( CPrefs::DiskCacheSize );
	}
	sFileIDs[id - FIRSTFOLDER]->SetFolderSpec(newSpec, id);
	PostInitializePref(id, changed);
	
	return changed;
}

// Called from mdmac.c to find the Java component folder
OSErr FindGutsFolder(FSSpec* outSpec)
{
	*outSpec = CPrefs::GetFolderSpec(CPrefs::RequiredGutsFolder);

	return noErr;
}

// Called from mdmac.c to find the Java component folder
OSErr FindNetscapeFolder(FSSpec* outSpec)
{
	*outSpec = CPrefs::GetFolderSpec(CPrefs::NetscapeFolder);

	return noErr;
}

// Called from mdmac.c to find the Java component folder
OSErr FindJavaDownloadsFolder(FSSpec* outSpec)
{
	*outSpec = CPrefs::GetFilePrototype(CPrefs::NetscapeFolder);
	CStr255 downName;
	::GetIndString( downName, 14000, 5 );
#ifdef DEBUG
	if (downName.Length() < 4 )
		XP_ASSERT(false);	// Someone blew away our string
#endif
	LString::CopyPStr( downName, outSpec->name, 32 );
	return noErr;
}

// Called from mkhelp.c to get the standard location of the NetHelp folder as a URL
char * FE_GetNetHelpDir()
{
	FSSpec		nethelpFolder = CPrefs::GetFolderSpec(CPrefs::NetHelpFolder);
	
	return CFileMgr::GetURLFromFileSpec(nethelpFolder);
}

Boolean CPrefs::GetBoolean(PrefEnum id)
{
	if ( IsNewPrefFormat(id) ) {
		XP_Bool value;
		PREF_GetBoolPref( prefLoc[id].prefName, &value );
		return value;
	}
	else return false;
}

Int32	CPrefs::GetLong(PrefEnum id)
{
	if ( IsNewPrefFormat(id) ) {
		int32 value;
		PREF_GetIntPref( prefLoc[id].prefName, &value );
		return (Int32) value;
	}
	else return 0;
}

char *	CPrefs::GetStaticString()
{
	static char		cStrings[kStaticStrCount][kStaticStrLen];
	static short	currentCString = 0;

	currentCString = (currentCString + 1) % kStaticStrCount;
	char* strbuffer = cStrings[currentCString];

	return (strbuffer);
}
		
CStr255	CPrefs::GetString(PrefEnum id)
{
	// --xp prefs Note:
	// Be aware of performance impact here.  For some frequently
	// accessed prefs, we might want to cache value locally
	// (e.g. HomePage is queried every time menu/toolbar is updated).
	if ( IsNewPrefFormat(id) ) {
		int len = kStaticStrLen;
		char * value = GetStaticString();
		
		// special case to handle "one-time homepage".  If a homepage
		// override is requested and the user hasn't seen it yet,
		// load the override page instead.
		if (id == HomePage) {
			XP_Bool override = false;
			PREF_GetBoolPref("browser.startup.homepage_override", &override);
			if (override) {
				char* url = NULL;
				PREF_CopyConfigString("startup.homepage_override_url", &url);
				PREF_SetBoolPref("browser.startup.homepage_override", false);
				if (url && *url) {
					strncpy(value, url, len);
					XP_FREE(url);
					return value;
				}
			}		
		}
		
		PREF_GetCharPref( prefLoc[id].prefName, value, &len );
		return value;
	}
	else return CStr255::sEmptyString;
}
		
char* CPrefs::GetCharPtr( PrefEnum id )
{
	if ( IsNewPrefFormat(id) )
	{
		char* strbuffer = GetStaticString();
		int len = kStaticStrLen;
		PREF_GetCharPref( prefLoc[id].prefName, strbuffer, &len );
		return strbuffer;
	}
	else return CStr255::sEmptyString;
}

// --ML no longer inline
const RGBColor& CPrefs::GetColor( PrefEnum id )
{
	if ( IsNewPrefFormat(id) )
	{
		uint8 r, g, b;
		PREF_GetColorPref( prefLoc[id].prefName, &r, &g, &b );
		static RGBColor color;
		color.red = r << 8;
		color.green = g << 8;
		color.blue = b << 8;
		return color;
	}

	return sColors[ id - FIRSTCOLOR ];
}

FSSpec	CPrefs::GetFolderSpec( PrefEnum id )
{
	Assert_(sFileIDs[id - FIRSTFILESPEC]);
	if (sFileIDs[id - FIRSTFILESPEC] == NULL)
	{
		FSSpec dummy;
		dummy.vRefNum = dummy.parID = 0;
		return dummy;
	}
	else
		return sFileIDs[id - FIRSTFILESPEC]->GetFolderSpec(); 
}

FSSpec CPrefs::GetFilePrototype(PrefEnum id)
{
//	Assert_((id >= FIRSTFILESPEC) && (id <= LastFolder));
	Assert_(sFileIDs[id - FIRSTFILESPEC]);
	if (sFileIDs[id - FIRSTFILESPEC] == NULL)
	{	
		FSSpec dummy;
		dummy.vRefNum = dummy.parID = 0;
		return dummy;
	}
	else
		return sFileIDs[id - FIRSTFILESPEC]->GetFilePrototype(); 
}

char * CPrefs::GetCachePath()
{
	if (sCachePath == NULL)
	{
		FSSpec cacheFolder = GetFilePrototype( DiskCacheFolder );
		sCachePath = CFileMgr::PathNameFromFSSpec(cacheFolder, FALSE);
	}
	return sCachePath;
}

THPrint	CPrefs::GetPrintRecord()
{

	/*		If we have already loaded a print record, whatever its origin, use that.
		If not, create the default one.  Note that the standard "get our print record"
		code already loads the default one, if necessary.  Checking again right here
		is useful only for dynamically detecting when a printer has recently been
		chosen on a machine for which no printer has ever before been chosen.
		(It was a bug; don't hunt me down for this one.)
	*/

	if (!sPrintRec)
		try {
			sPrintRec = UPrintingMgr::GetDefaultPrintRecord();
		} catch(...) {
		}
	return sPrintRec;
}

// --Admin Kit support. Animation file must live in Essential Files
// or the user's profile folder.
void CPrefs::OpenAnimationFile(FSSpec& preferredAnim, FSSpec& secondaryAnim)
{
	char* animFileName;
	if ( PREF_CopyConfigString("mac_animation_file", &animFileName) == PREF_NOERROR )
	{
		Try_ {
			LFile* animFile = nil;
			LString::CopyPStr( (CStr32) animFileName, preferredAnim.name, 32 );
			if (CFileMgr::FileExists(preferredAnim)) {
				animFile = new LFile(preferredAnim);
			} else {
				LString::CopyPStr( (CStr32) animFileName, secondaryAnim.name, 32 );
				if (CFileMgr::FileExists(secondaryAnim)) {
					animFile = new LFile(secondaryAnim);
				}
			}
			XP_FREE(animFileName);
			if (animFile) {
				animFile->OpenResourceFork(fsRdPerm);
				CSpinningN::AnimResFile() = animFile->GetResourceForkRefNum();
				
				MoveResourceMapBelowApp();
			}
		}
		Catch_(inErr)
		{	// file not found or something. Just ignore it.
		}
	}
}

Boolean CPrefs::HasCoBrand()
{
	// Show co-brand if using a custom animation file
	// (No longer true: or logo URL is changed.)
	return ( CSpinningN::AnimResFile() != refNum_Undefined );
	  /* || ( PREF_CopyConfigString("toolbar.logo.url", nil) == PREF_NOERROR );*/
}

Boolean CPrefs::IsLocked(PrefEnum id)
{
	if ( IsNewPrefFormat(id) ) {
		return (Boolean) PREF_PrefIsLocked( prefLoc[id].prefName );
	}
	else {
		return false;
	}
}

char* CPrefs::Concat(const char* base, const char* suffix)
{
	const size_t kBufLength = 256;
	static char buf[kBufLength];
	size_t blen = strlen(base);
	size_t slen = strlen(suffix);
	Assert_(blen+slen < kBufLength);
	memcpy(buf, base, blen);
	memcpy(&buf[blen], suffix, slen);
	buf[blen + slen] = '\0';
	return buf;
}

void CPrefs::PostInitializePref(PrefEnum id, Boolean changed)
{
	if (changed && !sReading)
	{
		sDirty = TRUE;
		if (gPrefBroadcaster != NULL)
			gPrefBroadcaster->BroadcastMessage(msg_PrefsChanged, &id);	
	}
	
	switch(id)	{
// Booleans
		case ThreadMail:
		case ThreadNews:
		case ShowToolbar:
		case DelayImages:
		case ShowStatus:
		case ShowURL:
		case DisplayWhileLoading:
			break;
		case MailUseFixedWidthFont:
			break;
		case LeaveMailOnServer:
			break;
	
		case MailCCSelf:
			break;
	
		case NewsCCSelf:
			break;
		case UseFancyNews:
		case ShowAllNews:	
			break;			
		case AnchorUnderline:
		case UseFancyFTP:
		case LoadHomePage:
		case ExpireNever:
		case CustomLinkColors:
		case ShowDirectory:
		case AgreedToLicense:
		case EnteringSecure:	
		case LeavingSecure:
		case ViewingMixed:
		case SubmittingInsecure:
		case ShowSecurity:
		case CustomVisitedColors:
		case CustomTextColors:

		case DefaultMailDelivery:
		case EnableActiveScrolling:
#ifdef EDITOR
		case EditorUseCustomColors:
		case EditorUseBackgroundImage:
		case PublishMaintainLinks:
		case PublishKeepImages:
		case ShowCopyright:
		case ShowFileEditToolbar:
		case ShowCharacterToolbar:
		case ShowParagraphToolbar:
#endif
			break;
			
		case UseDocumentColors:
			break;
		case UseSigFile:
			break;
		case AutoDetectEncoding:
		case StrictlyMIME:
#ifdef MOZ_MAIL_NEWS
			MIME_ConformToStandard(GetBoolean(id));
#endif // MOZ_MAIL_NEWS
			break;
		case UseUtilityBackground:
			break;
		case UseInlineViewSource:
			break;
			
		case ShowToolTips:
			CToolTipAttachment::Enable(GetBoolean(ShowToolTips));
			break;
			
		case UseInternetConfig:	
			break;

		case EnableJava:
#if defined (JAVA)
			LJ_SetJavaEnabled( (PRBool)CPrefs::GetBoolean( CPrefs::EnableJava ) );
#endif /* defined (JAVA) */
			break;
// Longs
		case LicenseVersion:	
		case TopLeftHeader:
		case TopMidHeader:
		case TopRightHeader:
		case BottomLeftFooter:
		case BottomMidFooter:
		case BottomRightFooter:
		case PrintBackground:
		case StartupAsWhat:
		case StartupBitfield:
		case SortMailBy:
		case SortNewsBy:
		case NewsPaneConfig:
		case MailPaneConfig:
		case Ciphers:
		case MailHeaderDisplay:
		case NewsHeaderDisplay:
		case AutoSaveTimeDelay:
		// NOPs
			break;
		case DaysTilExpire:
			if (GetLong(id) != -1)
				GH_SetGlobalHistoryTimeout( SECONDS_PER_DAY * GetLong(id) );
			else
				GH_SetGlobalHistoryTimeout( -1 );
			break;
		case Connections:
			NET_ChangeMaxNumberOfConnectionsPerContext(GetLong(id));
			break;
		case BufferSize:
			NET_ChangeSocketBufferSize( GetLong(id) );
			break;
		case PrintFlags:
		case NewsArticlesMax:
#ifdef MOZ_MAIL_NEWS
			NET_SetNumberOfNewsArticlesInListing(GetLong(id));
#endif
			break;
		case CheckDocuments:
			NET_SetCacheUseMethod( (CacheUseEnum)GetLong(id) );
			break;
		case FileSortMethod:
		case ToolbarStyle:
		case DefaultCharSetID:
		case DefaultFontEncoding:
			break;
		case BackgroundColors:
			LO_Color loColor;
			loColor = MakeLOColor(GetColor(TextBkgnd ));
			LO_SetDefaultColor( LO_COLOR_BG, loColor.red, loColor.green, loColor.blue);
			break;
		case MessageFontStyle:
		case MessageFontSize:
			break;
// Strings
		case AcceptLanguage:
			break;
		case HomePage:
			break;
		case NewsHost:
#ifdef MOZ_MAIL_NEWS
			NET_SetNewsHost( GetCharPtr(id) );
#endif
			break;
		case UserName:
			break;
		case UserEmail:
			break;
		case SMTPHost:
#if defined(MOZ_MAIL_COMPOSE) || defined(MOZ_MAIL_NEWS)
			NET_SetMailRelayHost( GetCharPtr(id));
#endif // MOZ_MAIL_COMPOSE || MOZ_MAIL_NEWS
			break;
		case SOCKSHost:
		case SOCKSPort:
			CStr31 numstr;
			::NumToString(GetLong(SOCKSPort), numstr);
			NET_SetSocksHost(BuildProxyString(GetString(SOCKSHost), numstr));
			break;
		case Organization:
			// Should we reset mail window here too?
			break;
		case PopHost:
#ifdef MOZ_MAIL_NEWS
			MSG_SetPopHost( GetCharPtr(id) );
			if ( GetBoolean( RememberMailPassword ) )
				NET_SetPopPassword( GetCharPtr( MailPassword) );
			else
				NET_SetPopPassword(NULL);
#endif
			break;
			
		case DefaultMailCC:
			break;
		case DefaultNewsCC:
			break;
		case ReplyTo:
			break;
		case PopID:
#ifdef MOZ_MAIL_NEWS
			NET_SetPopUsername(GetCharPtr(PopID));
#endif
			break;

		case AcceptCookies:
			break;
		case UseEmailAsPassword:
			NET_SendEmailAddressAsFTPPassword( CPrefs::GetBoolean( CPrefs::UseEmailAsPassword ) );
			break;
		case SubmitFormsByEmail:
			NET_WarnOnMailtoPost( CPrefs::GetBoolean( CPrefs::SubmitFormsByEmail ) ? PR_TRUE : PR_FALSE );
			break;
		case DefaultPersonalCertificate:
			break;
// Folders
		case DownloadFolder:
		case NetscapeFolder:
		case MainFolder:
		case SignatureFile:
		case HTMLEditor:
		case ImageEditor:
			break;
		case DiskCacheFolder:
			if (sCachePath)
				free(sCachePath);		
			sCachePath = NULL;
			break;
		case GIFBackdropFile:
			break;				
		case MailFolder:
			break;
		case NewsFolder:
			break;
		case SecurityFolder:
			break;
// Colors
		case TextFore:
			loColor = MakeLOColor(GetColor(TextFore ));
			LO_SetDefaultColor( LO_COLOR_FG , loColor.red, loColor.green, loColor.blue);
			break;
		case TextBkgnd:
			loColor = MakeLOColor(GetColor(TextBkgnd ));
			LO_SetDefaultColor( LO_COLOR_BG, loColor.red, loColor.green, loColor.blue);
			break;
		case Anchor:
			loColor = MakeLOColor(GetColor(Anchor ));
			LO_SetDefaultColor( LO_COLOR_LINK, loColor.red, loColor.green, loColor.blue);
			break;
		case Visited:
			loColor = MakeLOColor(GetColor(Visited ));
			LO_SetDefaultColor( LO_COLOR_VLINK, loColor.red, loColor.green, loColor.blue);
			break;
		case WindowBkgnd:
		case EditorText:
		case EditorLink:
		case EditorActiveLink:
		case EditorFollowedLink:
		case EditorBackground:
			break;
// Print record
		case PrintRecord:
			break;
// Misc. Together, because settings from different categories need to be combined
		case UseMailFCC:
		case MailCCFile:
			break;
		case UseNewsFCC:
		case NewsCCFile:
			break;
			
		case LimitMessageSize:
		case MaxMessageSize:
			break;
		case UseMozPassword:
		case AskMozPassword:
		case AskMozPassFrequency:
			break;
#ifdef FORTEZZA
		case FortezzaTimeoutOn:
		case FortezzaTimeout:
			if (!GetBoolean(FortezzaTimeoutOn))
				FortezzaSetTimeout(0);
			else {
				int timeout = GetLong(FortezzaTimeout);

				if (timeout <= 0) timeout = 1;
				FortezzaSetTimeout(timeout);
			}
			break;
#endif
			
			
		case BiffOn:
		case BiffTimeout:
		case AutoQuoteOnReply:
			break;	
		case MailPassword:
#ifdef MOZ_MAIL_NEWS
			NET_SetPopPassword( GetCharPtr( MailPassword) );
#endif
			break;
		case RememberMailPassword:
#ifdef MOZ_MAIL_NEWS
		// This depends on the loading order
		// Should be solved by the accumulator
			if ( GetBoolean( RememberMailPassword ) )
				NET_SetPopPassword( GetCharPtr( MailPassword) );
			else
				NET_SetPopPassword(NULL);
#endif
			break;
		default:
//			Assert_(FALSE);
			break;
	}
}


void CPrefs::Read1MimeTypes()
{
	// If the Prefs file is 3.0-format read it first
	CMimeMapper*	newMap;
	if ( sPrefFileVersion == 3 && sReading && UsePreferencesResFile() )
	{
		#define DYNAMIC_MIME_RES_TYPE 	'MIME'
		#define STATIC_MIME_RES_TYPE  	'SMIM'
		short howMany = ::Count1Resources( DYNAMIC_MIME_RES_TYPE );
		
		// ¥ handle the case of "first launch" so that we can read all
		//		of the MIME types out of the application's resource fork
		//		(and then write them out to prefs when we quit, so that
		//		next time we launch, they'll be in prefs)
		short i;
		Handle res;
		for ( i = 0; i < howMany; i++ )
		{
			res = ::Get1Resource( DYNAMIC_MIME_RES_TYPE, MIME_PREFS_FIRST_RESID + i );
			if ( res && *res ) {
				newMap = CMimeMapper::CreateMapperForRes(res);
				sMimeTypes.InsertItemsAt( 1, LArray::index_Last, &newMap );
			}
		}
		howMany = ::Count1Resources( STATIC_MIME_RES_TYPE );
		for ( i = 0; i < howMany; i++ )
		{
			res = ::GetResource( STATIC_MIME_RES_TYPE, i + 1 );
			if ( res && *res ) {
				newMap = CMimeMapper::CreateMapperForRes(res);
				sMimeTypes.InsertItemsAt( 1, LArray::index_Last, &newMap );
			}
		}
	}
	
	// Always read xp mime default & user prefs
	char* children;	
	if ( PREF_CreateChildList("mime", &children) == 0 )
	{	
		int index = 0;
		while (char* child = PREF_NextChild(children, &index)) {
			newMap = CMimeMapper::CreateMapperFor(child, sPrefFileVersion == 4);
			if (newMap)
				sMimeTypes.InsertItemsAt( 1, LArray::index_Last, &newMap );
		}
		XP_FREE(children);
	}
}

// Try reading in from the preferences file
// If it does not work, use the app file
void CPrefs::ReadMimeTypes()
{
	Try_
	{
		Read1MimeTypes();
	}
	Catch_(inErr)
	{
	}
	EndCatch_
}

// Creates a mapper for an unknown type, and adds it to the list. Returns NULL on failure
CMimeMapper* CPrefs::CreateDefaultUnknownMapper(const CStr255& mimeType, Boolean doInsert)
{
	CStr255 appName;
	::GetIndString( appName, 300, unknownAppName );
	CMimeMapper * newMapper = new CMimeMapper(CMimeMapper::Unknown, 
							mimeType,
							appName,
							CStr255::sEmptyString,
							'????',
							'TEXT');
	if (newMapper != NULL && doInsert)
	{
		sMimeTypes.InsertItemsAt( 1, LArray::index_Last, &newMapper);
		sDirty = TRUE;
		newMapper->WriteMimePrefs();
	}

	return newMapper;
}

// Like CreateDefaultUnknownMapper, except that it uses the application SIG
CMimeMapper* CPrefs::CreateDefaultAppMapper(FSSpec &fileSpec,const char * mimeType, Boolean doInsert)
{
	FInfo finderInfo;
	OSErr err = FSpGetFInfo(&fileSpec, &finderInfo );	
	CMimeMapper * newMapper = new CMimeMapper(CMimeMapper::Launch, 
											CStr255(mimeType),
											CStr255(fileSpec.name),
											CStr255::sEmptyString,
											finderInfo.fdCreator,	// Signature
											'TEXT'); // File type
	if (newMapper != NULL && doInsert)
	{
		sMimeTypes.InsertItemsAt( 1, LArray::index_Last, &newMapper);
		sDirty = TRUE;
		newMapper->WriteMimePrefs();
	}
	return newMapper;
}

void CPrefs::SubscribeToPrefChanges( LListener *listener )
{
	if (gPrefBroadcaster == NULL)
		gPrefBroadcaster = new LBroadcaster;
		
	if (gPrefBroadcaster != NULL)
		gPrefBroadcaster->AddListener(listener);
}


void			
CPrefs::UnsubscribeToPrefChanges( LListener *listener )
{
	if (gPrefBroadcaster != NULL)
		gPrefBroadcaster->RemoveListener(listener);
}

void FE_RememberPopPassword(MWContext * /* context */, const char * password)
{
	CPrefs::SetString( password, CPrefs::MailPassword );
}

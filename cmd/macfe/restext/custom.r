 /*
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
#undef BETA
#undef ALPHA
#include "SysTypes.r"

#define		APPLICATION_NAME		"Mozilla"
#define		TRADEMARK_NAME			APPLICATION_NAME "ª "

#define		VERSION_CORP_STR		"Mozilla.Org"

#define     APPLICATION_LANGUAGE    "en"

#define		VERSION_MAJOR_STR		"5.0 Development"
#define		VERSION_MINOR_STR		""
#define		VERSION_STRING			VERSION_MAJOR_STR VERSION_MINOR_STR
#define 	VERSION_LANG			"en"	// e.g. en, ja, de, fr
#define 	VERSION_COUNTRY			"_US"		// e.g.,  _JP, _DE, _FR, _US
#define 	VERSION_LOCALE			VERSION_LANG VERSION_COUNTRY
#define 	VERSION_MAJOR			5
#define		VERSION_MINOR			0		// Actually minor and bug rev in BCD format
#define 	VERSION_MICRO			0		// Actually internal non release rev #
#define		VERSION_KIND			development
#define		COPYRIGHT_STRING		", © Netscape Communications Corporation 1995-8"


//#define		GETINFO_DESC			TRADEMARK_NAME VERSION_STRING
#define		GETINFO_DESC			VERSION_CORP_STR

#define		GETINFO_VERSION			VERSION_STRING COPYRIGHT_STRING

#ifdef MOZ_NAV_BUILD_PREFIX
#define		USER_AGENT_PPC_STRING		"(Macintosh; %s; PPC, Nav)"
#define		USER_AGENT_68K_STRING		"(Macintosh; %s; 68K, Nav)"
#else
#define		USER_AGENT_PPC_STRING		"(Macintosh; %s; PPC)"
#define		USER_AGENT_68K_STRING		"(Macintosh; %s; 68K)"
#endif

#define		USER_AGENT_NAME			"Mozilla"

#ifdef	powerc
#define		USER_AGENT_VERSION		VERSION_STRING " [" VERSION_LOCALE "] " USER_AGENT_PPC_STRING
#else
#define		USER_AGENT_VERSION		VERSION_STRING " [" VERSION_LOCALE "] " USER_AGENT_68K_STRING
#endif

/*-----------------------------------------------------------------------------
	Version Numbers
-----------------------------------------------------------------------------*/

resource 'vers' (1, "Program") {
	VERSION_MAJOR,
	VERSION_MINOR,
	VERSION_KIND,
	VERSION_MICRO,
	verUS,
	USER_AGENT_VERSION,
	GETINFO_VERSION
};

resource 'vers' (2, "Suite") {
	VERSION_MAJOR,
	VERSION_MINOR,
	VERSION_KIND,
	VERSION_MICRO,
	verUS,
	USER_AGENT_VERSION,
	GETINFO_DESC
};

resource 'STR#' ( ID_STRINGS, "IDs", purgeable) {{
	APPLICATION_NAME
,	VERSION_MAJOR_STR			// Actually the user agent string - was VERSION_STRING
,	USER_AGENT_NAME
,	USER_AGENT_68K_STRING
,	USER_AGENT_PPC_STRING
,   APPLICATION_LANGUAGE
}};

#define		NETSCAPE_LOCATION		"http://www.netscape.com"
#define		CGI_LOCATION			"http://cgi.netscape.com/"
#define		ENG_LOCATION			NETSCAPE_LOCATION "eng/mozilla/" VERSION_MAJOR_STR "/"
#define		HOME_LOCATION			NETSCAPE_LOCATION "home/"

#define		WELCOME_LOCATION		"welcome.html"

#define		WHATSNEW_LOCATION		HOME_LOCATION "whats-new.html"
#define		WHATSCOOL_LOCATION		HOME_LOCATION "whats-cool.html"
#define		NEWSRC_LOCATION			"news:"
#define		MARKETPLACE_LOCATION	HOME_LOCATION "netscape-galleria.html"
#define		DIRECTORY_LOCATION		HOME_LOCATION "internet-directory.html"
#define		SEARCH_LOCATION			HOME_LOCATION "internet-search.html"
#define		WHITEPAGES_LOCATION		HOME_LOCATION "internet-white-pages.html"
#define		ABOUTINTERNET_LOCATION	HOME_LOCATION "about-the-internet.html"

#define		ABOUT_LOCATION			"about:"
#define		ABOUT_PLUGINS_LOCATIONS	"about:plugins"
#define		REGISTRATION_LOCATION	CGI_LOCATION "cgi-bin/reginfo.cgi"
#define     MANUAL_LOCATION         ENG_LOCATION "handbook/"		
#define		ESCAPES_LOCATION		NETSCAPE_LOCATION "escapes/index.html"
#define 	VERSION_LOCATION		ENG_LOCATION "relnotes/mac-" VERSION_STRING ".html"
#define		FAQ_LOCATION			"http://help.netscape.com/faqs.html"
#define		HELPONSEC_LOCATION		NETSCAPE_LOCATION "info/security-doc.html"
#define		HELPONNEWS_LOCATION		ENG_LOCATION "news/news.html"
#define		FEEDBACK_LOCATION		"http://cgi.netscape.com/cgi-bin/auto_bug.cgi"
#define		SUPPORT_LOCATION		"http://help.netscape.com"
#define		WEBSERVICES_LOCATION	HOME_LOCATION "how-to-create-web-services.html"
#define		SOFTWARE_LOCATION		NETSCAPE_LOCATION "comprod/upgrades/index.html"

// Note: moved NEWDOC_TEMPLATE_LOCATION and WIZARD_LOCATION to config.js

#define		ABOUT_PLUGINS_LOCATION		"about:plugins"
#define		ABOUT_SOFTWARE_LOCATION		NETSCAPE_LOCATION	"comprod/upgrades/index.html"
#define		WEB_PAGE_STARTER_LOCATION	HOME_LOCATION 		"starter.html"

#define		ESSENTIALS_FOLDER		"Essential Files"
#define		CONFIG_FILENAME			"mozilla.cfg"

// Note: moved DIR_BUTTON_BASE and DIR_MENU_BASE lists of directory URLs to config.js

resource 'STR ' ( LOGO_BUTTON_URL_RESID, "Logo Button URL", purgeable ) {
	NETSCAPE_LOCATION
};

resource 'STR#' ( 300, "Pref file names", purgeable ) {{
	PREFS_FOLDER_NAME;						// 1
	PREFS_FILE_NAME;						// 2
	"Global History";						// 3 Global history
	"Cache Ä";								// 4 Cache folder name
	"CCache log";							// 5 Cache file allocation table name
	"newsrc";								// 6 Subscribed Newsgroups
	"Bookmarks.html";						// 7 Bookmarks
	"";										// 8 MIME types, not used
	"MagicCookie";							// 9 Magic cookie file
	"socks.conf";							// 10 SOCKS file, inside preferences
	"Newsgroups";							// 11 Newsgroup listings
	"NewsFAT";								// 12 Mappings of newsrc files
	"Certificates";							// 13 Certificates
	"Certificate Index";					// 14 Certificate index
	"Mail";									// 15 Mail folder
	"News";									// 16 News folder
	"Security";								// 17 Security folder
	".snm";									// 18 Mail index extension
	":Filter Rules";						// 19 aka Mail sort file
	"Pop State";							// 20 Mail pop state
	"Proxy Config";							// 21 Proxy configuration
	"Key Database";							// 22 Key db
	"Headers cache";						// 23 Headers xover cache
	"abook.nab";							// 24 Address Book
	"Sent";									// 25 Sent mail
	"Sent";									// 26 Posted news articles
	"External Cache";						// 27 External cache FAT
	".sbd";									// 28 Subdirectory extension
	"News Host DB";							// 29 News Host DB (really.)
	CONFIG_FILENAME;						// 30 Local config/lock file
	"User Profiles";						// 31 User profiles database
	":Mail Filter Log";						// 32 Mail Filter Log
	"Navigator Registry";						// 33 Netscape registry
	ESSENTIALS_FOLDER;						// 34 Essential files (where java stuff is, and other required stuff)
	"archive";								// 35
	"archive.fat";							// 36
	IBM3270_FOLDER;							// 37 IBM 3270 folder
	IBM3270_FILE;							// 38 IBM 3270 file
	"addressbook.html";						// 39
	".vcf";									// 40 Extension for vcard files.
	".ldi";									// 41 Extension for LDIF files.
	".ldif";								// 42 Another extension for LDIFF files.
	"Security Modules";						// 43 secModuleDb
	"Navigator Users";						// 44 Folder containing user profiles
	"Unknown";								// 45 App name for unknown mime type
	"Account Setup";						// 46 Name of the magic "Account Setup" launcher
	"NetHelp";								// 47 Folder containing nethelp files
	"Folder Cache";							// 48 Cache of last known folder pane state
	"Defaults";								// 49 Template folder for new profiles
	"moz40p3";								// 50 Cryptographic policy file
	"SignedAppletDB";						// 51 Signed applet file
}};

resource 'STR#' ( BUTTON_STRINGS_RESID, "Button Names", purgeable ) {{
	"Back";					// 1 Main
	"Forward";				// 2 Main
	"Home";					// 3 Main
	"Reload";				// 4 Main
	"Images";				// 5 Main
	"Open";					// 6 Main
	"Print";				// 7 Main
	"Find";					// 8 Main
	"Stop";					// 9 Main
	"WhatÕs New?";			// 10 Directory button
	"WhatÕs Cool?";			// 11 Directory button
	"Destinations";			// 12 Directory button
	"Net Search";			// 13 Directory button
	"People";		// 14 Directory button
	"Software";				// 15 Directory button
	"Get Mail";				// 16 Mail: Get New Mail
	"To: Mail";				// 17 Mail: Mail New
	"Delete";				// 18 Mail: Delete Message
	"Re: Mail";				// 19 Mail: Reply to Sender
	"Re: All";				// 20 Mail: Reply to All
	"Forward";				// 21 Mail and News: Forward Message
	"Previous";				// 22 Mail and News: Previous Unread Message
	"Next";					// 23 Mail and News: Next Unread Message
	"New";					// 24 (not used)
	"Read";					// 25 (not used)
	"Send Now";				// 26 Compose: Send Now
	"Attach";				// 27 Compose: Attach
	"Address";				// 28 Compose: Bring up Address Book
	"Send Later";			// 29 Compose: Send Later
	"Quote";				// 30 Compose: Quote Original Message
	"To: News";				// 31 News: Post to News
	"To: Mail";				// 32 News: Send Mail
	"Re: Mail";				// 33 News: Reply via Mail
	"Re: News";				// 34 News: Reply via News
	"Re: Both";				// 35 News: Reply via Mail and News
	"Thread";				// 36 News: Mark Thread Read
	"Group";				// 37 News: Mark Group Read
	"Mozilla";				// 38 Main: Co-brand button
	"New";					// 39 Editor; File/Edit: New Document
	"Open";					// 40 Editor; File/Edit: Open
	"Save";					// 41 Editor; File/Edit: Save
	"Browse";				// 42 Editor; File/Edit: Browse Document
	"Cut";					// 43 Editor; File/Edit: Cut
	"Copy";					// 44 Editor; File/Edit: Copy
	"Paste";				// 45 Editor; File/Edit: Paste
	"Print";				// 46 Editor; File/Edit: Print
	"Find";					// 47 Editor; File/Edit: Find
	"Edit";					// 48 Edit
	"Publish";				// 49 Editor; File/Edit: Publish
	"";						// 50 Editor; Decrease font size
	"";						// 51 Editor; Increase font size
	"";						// 52 Editor; Bold
	"";						// 53 Editor; Italic
	"";						// 54 Editor; Fixed Width
	"";						// 55 Editor; Font Color
	"";						// 56 Editor; Make Link
	"";						// 57 Editor; Clear All Styles
	"";						// 58 Editor; Insert Target (Named Anchor)
	"";						// 59 Editor; Insert Image
	"";						// 60 Editor; Insert Horiz. Line
	"";						// 61 Editor; Table
	"";						// 62 Editor; Object Properties
	"";						// 63 Editor; Bullet List
	"";						// 64 Editor; Numbered List
	"";						// 65 Editor; Decrease Indent
	"";						// 66 Editor; Increase Indent
	"";						// 67 Editor; Align Left
	"";						// 68 Editor; Center
	"";						// 69 Editor; Align Right
}};

#if 0 /* these have moved elsewhere */
resource 'STR#' ( BUTTON_TOOLTIPS_RESID, "", purgeable ) {{
	"Back";					// 1 Main
	"Forward";				// 2 Main
	"Go To Home Page";		// 3 Main
	"Reload Page";			// 4 Main
	"Load Images";			// 5 Main
	"Open Location";		// 6 Main
	"Print";				// 7 Main
	"Find Text On Page";	// 8 Main
	"Stop Loading";			// 9 Main
	"";						// 10 What's New ?
	"";						// 11 Directory button
	"";						// 12 Handbook
	"";						// 13 Net Search
	"";						// 14 Net Directory
	"";						// 15 Software
	"Get New Mail";			// 16 Mail: Get New Mail
	"Compose New Mail Message";		// 17 Mail: Mail New
	"Delete Selected Messages";		// 18 Mail: Delete Message
	"Reply To Sender";				// 19 Mail: Reply to Sender
	"Reply To All";					// 20 Mail: Reply to All
	"Forward Message";				// 21 Mail and News: Forward Message
	"Previous Unread Message";		// 22 Mail and News: Previous Unread Message
	"Next Unread Message";			// 23 Mail and News: Next Unread Message
	"";								// 24 (not used)
	"";								// 25 (not used)
	"Send Mail Now";			// 26 Compose: Send Now
	"Attach";					// 27 Compose: Attach
	"Open Address Book";		// 28 Compose: Bring up Address Book
	"Place Message In Outbox";	// 29 Compose: Send Later
	"Quote Original Message";	// 30 Compose: Quote Original Message
	"Post News Message";		// 31 News: Post to News
	"Compose New Mail Message";	// 32 News: Send Mail
	"Reply via Mail";			// 33 News: Reply via Mail
	"Reply via News";			// 34 News: Reply via News
	"Reply via Mail and News";	// 35 News: Reply via Mail and News
	"Mark Thread Read";			// 36 News: Mark Thread Read
	"Mark Group Read";			// 37 News: Mark Group Read
	"";							// 38 Main: Co-brand button
	"New Document";				// 39 Editor; File/Edit: New Document
	"Open File To Edit";		// 40 Editor; File/Edit: Open
	"Save";						// 41 Editor; File/Edit: Save
	"View in Browser";			// 42 Editor; File/Edit: Browse Document
	"Cut";						// 43 Editor; File/Edit: Cut
	"Copy";						// 44 Editor; File/Edit: Copy
	"Paste";					// 45 Editor; File/Edit: Paste
	"Print";					// 46 Editor; File/Edit: Print
	"Find";						// 47 Editor; File/Edit: Find
	"Edit";						// 48 Edit
	"Publish";					// 49 Editor; File/Edit: Publish
	"Decrease font size";		// 50 Editor; Decrease font size
	"Increase font size";		// 51 Editor; Increase font size
	"Bold";						// 52 Editor; Bold
	"Italic";					// 53 Editor; Italic
	"Fixed Width";				// 54 Editor; Fixed Width
	"Font Color";				// 55 Editor; Font Color
	"Make Link";				// 56 Editor; Make Link
	"Clear All Styles";			// 57 Editor; Clear All Styles
	"Insert Target (Named Anchor)";		// 58 Editor; Insert Target (Named Anchor)
	"Insert Image";				// 59 Editor; Insert Image
	"Insert Horiz. Line";		// 60 Editor; Insert Horiz. Line
	"Table";					// 61 Editor; Table
	"Object Properties";		// 62 Editor; Object Properties
	"Bullet List";				// 63 Editor; Bullet List
	"Numbered List";			// 64 Editor; Numbered List
	"Decrease Indent";			// 65 Editor; Decrease Indent
	"Increase Indent";			// 66 Editor; Increase Indent
	"Align Left";				// 67 Editor; Align Left
	"Center";					// 68 Editor; Center
	"Align Right";				// 69 Editor; Align Right
}};
#endif 0

/******************************************************************
 * PREFERENCES
 ******************************************************************/

/* -- 4.0 xp prefs --
 * Default string/int/bool values have been removed from here
 * and moved to xp initializers in libpref/src/init/all.js
 * To add or edit default values, edit all.js and make sure
 * your prefs have corresponding xp keys in uprefd.cp.
 */



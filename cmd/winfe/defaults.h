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
//
// Default options
//
// This file is only needed by netscape.cpp so this information should
//   not really be included in netscape.h since everyone includes that
//

// Defaults             
#define DEF_TOOLBAR                 TOOL_MIXED  // pictures and text
#ifdef XP_WIN16
#define DEF_MEM_CACHE               600   // in K
#else
#define DEF_MEM_CACHE               1024  // in K
#endif
#define DEF_MAX_CONNECT             4
#define DEF_FONT_SIZE               XP_MEDIUM  // medium

#ifdef XP_WIN16
#define DEF_TCPBUFFER               6*1024
#else
#define DEF_TCPBUFFER               32*1024
#endif /* XP_WIN16 */

#define DEF_AUTOLOAD_HOME           "yes"
#define CACHE_PREFIX                "M"
#define DEF_SHOW_TOOLBAR            "yes"
#define DEF_SHOW_DIRECTORY_BUTTONS  "yes"
#define DEF_AUTOLOAD_IMAGES         "yes"
#define DEF_FANCY_FTP               "yes"
#define DEF_SOCKS_CONF              "c:\\windows\\socks.cnf"
#define DEF_NEWS_DIR                            "c:\\netscape\\news"
#define DEF_BLINKING                "yes"
#define DEF_DITHER                  "yes"

#define DEF_CHECK_SERVER            1 // check cached documents vs the server once per session

#define DEF_MIME_POSTING            "no"

#define DEF_PROPORTIONAL_FONT       "Times New Roman"
#define DEF_FIXED_FONT              "Courier New"

#ifdef MCI
#define DEF_LINK_EXPR           1                                  // expiration time
#define DEF_UNDERLINE           "no"                               // underline links
#define DEF_NEWSRC              "c:\\imci\\viewer\\newsrc"
#define DEF_BOOKMARK            "c:\\imci\\viewer\\bookmark.htm"
#define DEF_NEWS_SERVER         "News.internetMCI.com"
#define DEF_MAIL_SERVER         "mail.internetMCI.com"
#define DEF_HOME_PAGE           "http://internetMCI.com/welcome"   // this doesn't look right...
#define DEF_FANCY_NEWS          "yes"
#define DEF_TEMP_DIR            "c:\\imci\\temp"
#define DEF_CACHE_DIR           "c:\\imci\\cache"
#define DEF_COOKIE_FILE         "c:\\imci\\viewer\\cookies.txt"
#define DEF_HISTORY_FILE        "c:\\imci\\viewer\\imci.hst"
#define DEF_SHOW_LOCATION       "no"
#else
#define DEF_LINK_EXPR           9                                  // expiration time
#define DEF_UNDERLINE           "yes"                              // underline links
#define DEF_NEWSRC              "c:\\newsrc"
#define DEF_BOOKMARK            "c:\\netscape\\bookmark.htm"
#define DEF_NEWS_SERVER         "news"
#define DEF_MAIL_SERVER         "mail"
#define DEF_HOME_PAGE           szLoadString(IDS_DIRECT_HOME)
#define DEF_FANCY_NEWS          "no"
#define DEF_TEMP_DIR            BOGUS_TEMP_DIR
#define DEF_CACHE_DIR           "c:\\netscape\\cache"
#define DEF_COOKIE_FILE         "c:\\netscape\\cookies.txt"
#define DEF_HISTORY_FILE        "c:\\netscape\\netscape.hst"
#define DEF_SHOW_LOCATION       "yes"
#define DEF_CERT_DB             "c:\\netscape\\cert.db"
#define DEF_CERT_NAME_INDEX     "c:\\netscape\\certni.db"
#endif

//  Some commonly used strings
#define SZ_NO "No"
#define SZ_YES "Yes"
#define SZ_WINASSOC "application/x-unknown-content-type-"
#define SZ_IN_GOD_WE_TRUST "User Trusted External Applications"
#define SZ_EMPTY ""
#define SZ_DEBUG "Debug"
#define SZ_TWIPS "Twips"

//  Stuff everywhere
#define BLINKING_RULEZ

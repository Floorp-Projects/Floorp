/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
*/

#ifndef NS_NEWSUTILS_H
#define NS_NEWSUTILS_H

#include "nsFileSpec.h"
#include "nsString.h"

static const char kNewsRootURI[] = "news:/";
static const char kNewsMessageRootURI[] = "news_message:/";

#define kNewsRootURILen 6
#define kNewsMessageRootURILen 14

/*
 * some platforms (like Windows and Mac) use a map file, because of
 * file name length limitations.
 */
#ifndef XP_UNIX
#define USE_NEWSRC_MAP_FILE

#if defined(XP_PC)
#define NEWS_FAT_FILE_NAME "fat"
/*
 * on the PC, the fat file stores absolute paths to the newsrc files
 * on the Mac, the fat file stores relative paths to the newsrc files
 */
#define NEWS_FAT_STORES_ABSOLUTE_NEWSRC_FILE_PATHS 1
#elif defined(XP_MAC)
#define NEWS_FAT_FILE_NAME "NewsFAT"
#else
#error dont_know_what_your_fat_file_is
#endif

#endif /* ! XP_UNIX */

extern nsresult
nsGetNewsRoot(const char* hostname, nsFileSpec &result);

extern nsresult
nsGetNewsHostName(const char *rootURI, const char *uriStr, char **hostName);

extern nsresult
nsNewsURI2Path(const char* rootURI, const char* uriStr, nsFileSpec& pathResult);

extern nsresult
nsNewsURI2Name(const char* rootURI, const char* uriStr, nsString& name);

extern nsresult
nsParseNewsMessageURI(const char* uri, nsString& messageUriWithoutKey, PRUint32 *key);

extern nsresult 
nsBuildNewsMessageURI(const char *baseURI, PRUint32 key, char** uri);


#endif //NS_NEWSUTILS_H


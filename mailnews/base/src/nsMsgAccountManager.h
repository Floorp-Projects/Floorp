/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nscore.h"
#include "nsIMsgAccountManager.h"

/*
 * some platforms (like Windows and Mac) use a map file, because of
 * file name length limitations.
 */
#if !defined(XP_UNIX) && !defined(XP_BEOS)
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
#error dont_know_what_your_news_fat_file_is
#endif

#endif /* ! XP_UNIX && ! XP_BEOS */


NS_BEGIN_EXTERN_C

nsresult
NS_NewMsgAccountManager(const nsIID& iid, void **result);

NS_END_EXTERN_C

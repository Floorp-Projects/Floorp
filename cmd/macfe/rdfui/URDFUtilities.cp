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
// Miscellaneous RDF utility methods
//

#include "URDFUtilities.h"

#include "xp_mem.h"
#include "ufilemgr.h" // for CFileMgr::GetURLFromFileSpec
#include "uprefd.h" // for CPrefs::GetFolderSpec & globHistoryName
#include "PascalString.h" // for CStr255


Boolean URDFUtilities::sIsInited = false;


//
// StartupRDF
//
// Initialize RDF if it has not already been initialized. This method can be
// called over and over again without bad side effects because it protects the init
// code with a static variable.
//
void URDFUtilities::StartupRDF()
{
	if (!sIsInited)
	{		
		// Point RDF to user's "Navigator Ä" preference folder and turn the FSSpec into
		// a URL
		FSSpec prefFolder = CPrefs::GetFolderSpec(CPrefs::MainFolder);
		char* url = CFileMgr::GetURLFromFileSpec(prefFolder);
		CStr255 urlStr(url);
		XP_FREE(url);
		
		// fill in RDF params structure with appropriate file paths
		CStr255 str;
		RDF_InitParamsStruct initParams;
		initParams.profileURL = (char*)urlStr;
		::GetIndString(str, 300, bookmarkFile);
		initParams.bookmarksURL = (char*)(urlStr + "/" + str);
		::GetIndString(str, 300, globHistoryName);
		initParams.globalHistoryURL = (char*)(urlStr + "/" + str);
		
//		url = NET_UnEscape(url);
	    RDF_Init(&initParams);
	    sIsInited = true;
	}
}


//
// ShutdownRDF
//
// Opposite of above routine. Again, can be called multiple times w/out bad side
// effects.
//
void URDFUtilities::ShutdownRDF()
{
	if (sIsInited)
	{
	    RDF_Shutdown();
	    sIsInited = false;
	}
}

Uint32 URDFUtilities::GetContainerSize(HT_Resource container)
{
	Uint32 result = 0;
	// trust but verify...
	if (HT_IsContainer(container))
	{
		HT_Cursor cursor = HT_NewCursor(container);
		if (cursor)
		{
			HT_Resource node = HT_GetNextItem(cursor);
			while (node != NULL)
			{
				++result;
				if (HT_IsContainer(node) && HT_IsContainerOpen(node))
					result += GetContainerSize(node);
				
				node = HT_GetNextItem(cursor);
			}
		}
	}
	return result;
}
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

#include "pluginIncludes.h"



	static	_fileCachePtr		theCache=NULL;



void
disposeCache(Boolean justPruneFlag)
{
		_fileCachePtr		*cacheTree,tempItem;

SETUP_PLUGIN_TRACE("\p disposeCache entered");

	if (theCache)	{
		cacheTree=&theCache;
		while(*cacheTree)	{
			if (justPruneFlag==FALSE || (*cacheTree)->dirtyFlag == TRUE)	{
				tempItem=*cacheTree;
				*cacheTree=(*cacheTree)->next;
				if (tempItem->dataH)	{
					DisposeHandle(tempItem->dataH);
					tempItem->dataH=NULL;
					}
				DisposePtr((Ptr)tempItem);
				}
			else	{
				cacheTree=&((*cacheTree)->next);
				}
			}
		theCache=NULL;
		}

SETUP_PLUGIN_TRACE("\p disposeCache exiting");
}

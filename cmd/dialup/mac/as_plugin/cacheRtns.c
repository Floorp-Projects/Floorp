/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

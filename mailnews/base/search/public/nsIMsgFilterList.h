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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _nsIMsgFilterList_H_
#define _nsIMsgFilterList_H_

#include "nscore.h"
#include "nsISupports.h"
#include "nsMsgFilterCore.h"

class nsIOFileStream;

////////////////////////////////////////////////////////////////////////////////////////
// The Msg Filter List is an interface designed to make accessing filter lists
// easier. Clients typically open a filter list and either enumerate the filters,
// or add new filters, or change the order around...
//
////////////////////////////////////////////////////////////////////////////////////////
// 08ecbcb4-0493-11d3-a50a-0060b0fc04b7

#define NS_IMSGFILTERLIST_IID                         \
{ 0x08ecbcb4, 0x0493, 0x11d3,                 \
    { 0xa5, 0x0a, 0x0, 0x60, 0xb0, 0xfc, 0x04, 0xb7 } }

class nsIMsgFilter;

class nsIMsgFilterList : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IMSGFILTERLIST_IID; return iid; }

	NS_IMETHOD GetFolderForFilterList(nsIMsgFolder **aFolder)= 0;
	NS_IMETHOD GetFilterCount(PRUint32 *pCount)= 0;
	NS_IMETHOD GetFilterAt(PRUint32 filterIndex, nsIMsgFilter **filter)= 0;
	/* these methods don't delete filters - they just change the list. FE still must
		call MSG_DestroyFilter to delete a filter.
	*/
	NS_IMETHOD SetFilterAt(PRUint32 filterIndex, nsIMsgFilter *filter)= 0;
	NS_IMETHOD RemoveFilterAt(PRUint32 filterIndex)= 0;
	NS_IMETHOD MoveFilterAt(PRUint32 filterIndex, nsMsgFilterMotion motion)= 0;
	NS_IMETHOD InsertFilterAt(PRUint32 filterIndex, nsIMsgFilter *filter)= 0;

	NS_IMETHOD EnableLogging(PRBool enable)= 0;
	NS_IMETHOD IsLoggingEnabled(PRBool *aResult)= 0;

	NS_IMETHOD CreateFilter(char *name,	nsIMsgFilter **result)= 0;
	NS_IMETHOD SaveToFile(nsIOFileStream *stream) = 0;
};

#endif




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

#ifndef _nsMsgFilterList_H_
#define _nsMsgFilterList_H_

#include "nscore.h"
#include "nsISupports.h"

////////////////////////////////////////////////////////////////////////////////////////
// The Msg Filter List is an interface designed to make accessing filter lists
// easier. Clients typically open a filter list and either enumerate the filters,
// or add new filters, or change the order around...
//
////////////////////////////////////////////////////////////////////////////////////////


class nsIMsgFilterList : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IMSGFILTERLIST_IID; return iid; }

NS_IMETHOD GetFolderForFilterList(nsIMsgFolder **aFolder);
NS_IMETHOD GetFilterCount(PRInt32 *pCount);
NS_IMETHOD GetFilterAt(nsMsgFilterIndex filterIndex, nsIMsgFilter **filter);
/* these methods don't delete filters - they just change the list. FE still must
	call MSG_DestroyFilter to delete a filter.
*/
NS_IMETHOD SetFilterAt(nsMsgFilterIndex filterIndex, nsIMsgFilter *filter);
NS_IMETHOD RemoveFilterAt(nsMsgFilterIndex filterIndex);
NS_IMETHOD MoveFilterAt(nsMsgFilterIndex filterIndex, nsMsgFilterMotion motion);
NS_IMETHOD InsertFilterAt(nsMsgFilterIndex filterIndex, nsMsgFilter *filter);

NS_IMETHOD EnableLogging(PRBool enable);
NS_IMETHOD IsLoggingEnabled(PRBool *aResult);


};

#endif




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

#ifndef __nsMsgResultElement_h
#define __nsMsgResultElement_h

#include "nsMsgSearchCore.h"

// nsMsgResultElement specifies a single search hit.

//---------------------------------------------------------------------------
// nsMsgResultElement is a list of attribute/value pairs which are used to
// represent a search hit without requiring a message header or server connection
//---------------------------------------------------------------------------



class nsMsgResultElement
{
public:
	nsMsgResultElement (nsIMsgSearchAdapter *);
	virtual ~nsMsgResultElement ();

	static nsresult AssignValues (nsMsgSearchValue *src, nsMsgSearchValue *dst);
	nsresult GetValue (nsMsgSearchAttribute, nsMsgSearchValue **) const;
	nsresult AddValue (nsMsgSearchValue*);

	nsresult GetPrettyName (nsMsgSearchValue**);


	const nsMsgSearchValue *GetValueRef (nsMsgSearchAttribute) const;
	nsresult Open (void *window);

	// added as part of the search as view capabilities...
	static int CompareByFolderInfoPtrs (const void *, const void *);  

	static int Compare (const void *, const void *);
	static nsresult DestroyValue (nsMsgSearchValue *value);

	nsMsgSearchValueArray m_valueList;
	nsIMsgSearchAdapter *m_adapter;

protected:
};

#endif

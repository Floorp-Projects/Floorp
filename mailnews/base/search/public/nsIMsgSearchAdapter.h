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

#ifndef _nsIMsgSearchAdapter_H_
#define _nsIMsgSearchAdapter_H_

#include "nscore.h"
#include "nsISupports.h"
#include "nsMsgSearchCore.h"

//66f4b80c-0fb5-11d3-a515-0060b0fc04b7
#define NS_IMSGSEARCHADAPTOR_IID                         \
{ 0x66f4b80c, 0x0fb5, 0x11d3,                 \
    { 0xa5, 0x15, 0x0, 0x60, 0xb0, 0xfc, 0x04, 0xb7 } }



class nsIMsgSearchAdapter : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IMSGSEARCHADAPTOR_IID; return iid; }
	NS_IMETHOD Init(nsMsgSearchScopeTerm* scope, nsMsgSearchTermArray *terms) = 0;
	NS_IMETHOD ValidateTerms () = 0;
	NS_IMETHOD Search () = 0;
	NS_IMETHOD SendUrl () = 0;
	NS_IMETHOD OpenResultElement (nsMsgResultElement *) = 0;
	NS_IMETHOD ModifyResultElement (nsMsgResultElement*, nsMsgSearchValue*) = 0;
	NS_IMETHOD GetEncoding (const char **encoding) = 0;
	NS_IMETHOD FindTargetFolder (const nsMsgResultElement *, nsIMsgFolder **aFolder) = 0;
	NS_IMETHOD Abort () = 0;
};


#endif


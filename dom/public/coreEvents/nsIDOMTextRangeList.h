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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMTextRangeList_h__
#define nsIDOMTextRangeList_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMTextRange;

#define NS_IDOMTEXTRANGELIST_IID \
 { 0x1ee9d531, 0x2a79, 0x11d3, \
	{ 0x9e, 0xa4, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b} } 

class nsIDOMTextRangeList : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMTEXTRANGELIST_IID; return iid; }

  NS_IMETHOD    GetLength(PRUint16* aLength)=0;

  NS_IMETHOD    Item(PRUint16 aIndex, nsIDOMTextRange** aReturn)=0;
};


#define NS_DECL_IDOMTEXTRANGELIST   \
  NS_IMETHOD    GetLength(PRUint16* aLength);  \
  NS_IMETHOD    Item(PRUint16 aIndex, nsIDOMTextRange** aReturn);  \



#define NS_FORWARD_IDOMTEXTRANGELIST(_to)  \
  NS_IMETHOD    GetLength(PRUint16* aLength) { return _to GetLength(aLength); } \
  NS_IMETHOD    Item(PRUint16 aIndex, nsIDOMTextRange** aReturn) { return _to Item(aIndex, aReturn); }  \


#endif // nsIDOMTextRangeList_h__

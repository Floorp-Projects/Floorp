/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMStyleSheetList_h__
#define nsIDOMStyleSheetList_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMStyleSheet;

#define NS_IDOMSTYLESHEETLIST_IID \
 { 0xa6cf9081, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class NS_NO_VTABLE nsIDOMStyleSheetList : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOMSTYLESHEETLIST_IID)

  NS_IMETHOD    GetLength(PRUint32* aLength)=0;

  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMStyleSheet** aReturn)=0;
};


#define NS_DECL_IDOMSTYLESHEETLIST   \
  NS_IMETHOD    GetLength(PRUint32* aLength);  \
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMStyleSheet** aReturn);  \



#define NS_FORWARD_IDOMSTYLESHEETLIST(_to)  \
  NS_IMETHOD    GetLength(PRUint32* aLength) { return _to GetLength(aLength); } \
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMStyleSheet** aReturn) { return _to Item(aIndex, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitStyleSheetListClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptStyleSheetList(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMStyleSheetList_h__

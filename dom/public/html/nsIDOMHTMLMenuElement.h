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

#ifndef nsIDOMHTMLMenuElement_h__
#define nsIDOMHTMLMenuElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLMENUELEMENT_IID \
 { 0xa6cf909d, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLMenuElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLMENUELEMENT_IID; return iid; }

  NS_IMETHOD    GetCompact(PRBool* aCompact)=0;
  NS_IMETHOD    SetCompact(PRBool aCompact)=0;
};


#define NS_DECL_IDOMHTMLMENUELEMENT   \
  NS_IMETHOD    GetCompact(PRBool* aCompact);  \
  NS_IMETHOD    SetCompact(PRBool aCompact);  \



#define NS_FORWARD_IDOMHTMLMENUELEMENT(_to)  \
  NS_IMETHOD    GetCompact(PRBool* aCompact) { return _to GetCompact(aCompact); } \
  NS_IMETHOD    SetCompact(PRBool aCompact) { return _to SetCompact(aCompact); } \


extern "C" NS_DOM nsresult NS_InitHTMLMenuElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLMenuElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLMenuElement_h__

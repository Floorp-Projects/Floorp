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

#ifndef nsIDOMHTMLOptGroupElement_h__
#define nsIDOMHTMLOptGroupElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLOPTGROUPELEMENT_IID \
 { 0xa6cf9091, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLOptGroupElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLOPTGROUPELEMENT_IID; return iid; }

  NS_IMETHOD    GetDisabled(PRBool* aDisabled)=0;
  NS_IMETHOD    SetDisabled(PRBool aDisabled)=0;

  NS_IMETHOD    GetLabel(nsString& aLabel)=0;
  NS_IMETHOD    SetLabel(const nsString& aLabel)=0;
};


#define NS_DECL_IDOMHTMLOPTGROUPELEMENT   \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled);  \
  NS_IMETHOD    SetDisabled(PRBool aDisabled);  \
  NS_IMETHOD    GetLabel(nsString& aLabel);  \
  NS_IMETHOD    SetLabel(const nsString& aLabel);  \



#define NS_FORWARD_IDOMHTMLOPTGROUPELEMENT(_to)  \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled) { return _to GetDisabled(aDisabled); } \
  NS_IMETHOD    SetDisabled(PRBool aDisabled) { return _to SetDisabled(aDisabled); } \
  NS_IMETHOD    GetLabel(nsString& aLabel) { return _to GetLabel(aLabel); } \
  NS_IMETHOD    SetLabel(const nsString& aLabel) { return _to SetLabel(aLabel); } \


extern "C" NS_DOM nsresult NS_InitHTMLOptGroupElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLOptGroupElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLOptGroupElement_h__

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

#ifndef nsIDOMHTMLImageElement_h__
#define nsIDOMHTMLImageElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLIMAGEELEMENT_IID \
 { 0xa6cf90ab, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLImageElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLIMAGEELEMENT_IID; return iid; }

  NS_IMETHOD    GetLowSrc(nsAWritableString& aLowSrc)=0;
  NS_IMETHOD    SetLowSrc(const nsAReadableString& aLowSrc)=0;

  NS_IMETHOD    GetName(nsAWritableString& aName)=0;
  NS_IMETHOD    SetName(const nsAReadableString& aName)=0;

  NS_IMETHOD    GetAlign(nsAWritableString& aAlign)=0;
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign)=0;

  NS_IMETHOD    GetAlt(nsAWritableString& aAlt)=0;
  NS_IMETHOD    SetAlt(const nsAReadableString& aAlt)=0;

  NS_IMETHOD    GetBorder(nsAWritableString& aBorder)=0;
  NS_IMETHOD    SetBorder(const nsAReadableString& aBorder)=0;

  NS_IMETHOD    GetHeight(nsAWritableString& aHeight)=0;
  NS_IMETHOD    SetHeight(const nsAReadableString& aHeight)=0;

  NS_IMETHOD    GetHspace(nsAWritableString& aHspace)=0;
  NS_IMETHOD    SetHspace(const nsAReadableString& aHspace)=0;

  NS_IMETHOD    GetIsMap(PRBool* aIsMap)=0;
  NS_IMETHOD    SetIsMap(PRBool aIsMap)=0;

  NS_IMETHOD    GetLongDesc(nsAWritableString& aLongDesc)=0;
  NS_IMETHOD    SetLongDesc(const nsAReadableString& aLongDesc)=0;

  NS_IMETHOD    GetSrc(nsAWritableString& aSrc)=0;
  NS_IMETHOD    SetSrc(const nsAReadableString& aSrc)=0;

  NS_IMETHOD    GetVspace(nsAWritableString& aVspace)=0;
  NS_IMETHOD    SetVspace(const nsAReadableString& aVspace)=0;

  NS_IMETHOD    GetWidth(nsAWritableString& aWidth)=0;
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth)=0;

  NS_IMETHOD    GetUseMap(nsAWritableString& aUseMap)=0;
  NS_IMETHOD    SetUseMap(const nsAReadableString& aUseMap)=0;
};


#define NS_DECL_IDOMHTMLIMAGEELEMENT   \
  NS_IMETHOD    GetLowSrc(nsAWritableString& aLowSrc);  \
  NS_IMETHOD    SetLowSrc(const nsAReadableString& aLowSrc);  \
  NS_IMETHOD    GetName(nsAWritableString& aName);  \
  NS_IMETHOD    SetName(const nsAReadableString& aName);  \
  NS_IMETHOD    GetAlign(nsAWritableString& aAlign);  \
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign);  \
  NS_IMETHOD    GetAlt(nsAWritableString& aAlt);  \
  NS_IMETHOD    SetAlt(const nsAReadableString& aAlt);  \
  NS_IMETHOD    GetBorder(nsAWritableString& aBorder);  \
  NS_IMETHOD    SetBorder(const nsAReadableString& aBorder);  \
  NS_IMETHOD    GetHeight(nsAWritableString& aHeight);  \
  NS_IMETHOD    SetHeight(const nsAReadableString& aHeight);  \
  NS_IMETHOD    GetHspace(nsAWritableString& aHspace);  \
  NS_IMETHOD    SetHspace(const nsAReadableString& aHspace);  \
  NS_IMETHOD    GetIsMap(PRBool* aIsMap);  \
  NS_IMETHOD    SetIsMap(PRBool aIsMap);  \
  NS_IMETHOD    GetLongDesc(nsAWritableString& aLongDesc);  \
  NS_IMETHOD    SetLongDesc(const nsAReadableString& aLongDesc);  \
  NS_IMETHOD    GetSrc(nsAWritableString& aSrc);  \
  NS_IMETHOD    SetSrc(const nsAReadableString& aSrc);  \
  NS_IMETHOD    GetVspace(nsAWritableString& aVspace);  \
  NS_IMETHOD    SetVspace(const nsAReadableString& aVspace);  \
  NS_IMETHOD    GetWidth(nsAWritableString& aWidth);  \
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth);  \
  NS_IMETHOD    GetUseMap(nsAWritableString& aUseMap);  \
  NS_IMETHOD    SetUseMap(const nsAReadableString& aUseMap);  \



#define NS_FORWARD_IDOMHTMLIMAGEELEMENT(_to)  \
  NS_IMETHOD    GetLowSrc(nsAWritableString& aLowSrc) { return _to GetLowSrc(aLowSrc); } \
  NS_IMETHOD    SetLowSrc(const nsAReadableString& aLowSrc) { return _to SetLowSrc(aLowSrc); } \
  NS_IMETHOD    GetName(nsAWritableString& aName) { return _to GetName(aName); } \
  NS_IMETHOD    SetName(const nsAReadableString& aName) { return _to SetName(aName); } \
  NS_IMETHOD    GetAlign(nsAWritableString& aAlign) { return _to GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign) { return _to SetAlign(aAlign); } \
  NS_IMETHOD    GetAlt(nsAWritableString& aAlt) { return _to GetAlt(aAlt); } \
  NS_IMETHOD    SetAlt(const nsAReadableString& aAlt) { return _to SetAlt(aAlt); } \
  NS_IMETHOD    GetBorder(nsAWritableString& aBorder) { return _to GetBorder(aBorder); } \
  NS_IMETHOD    SetBorder(const nsAReadableString& aBorder) { return _to SetBorder(aBorder); } \
  NS_IMETHOD    GetHeight(nsAWritableString& aHeight) { return _to GetHeight(aHeight); } \
  NS_IMETHOD    SetHeight(const nsAReadableString& aHeight) { return _to SetHeight(aHeight); } \
  NS_IMETHOD    GetHspace(nsAWritableString& aHspace) { return _to GetHspace(aHspace); } \
  NS_IMETHOD    SetHspace(const nsAReadableString& aHspace) { return _to SetHspace(aHspace); } \
  NS_IMETHOD    GetIsMap(PRBool* aIsMap) { return _to GetIsMap(aIsMap); } \
  NS_IMETHOD    SetIsMap(PRBool aIsMap) { return _to SetIsMap(aIsMap); } \
  NS_IMETHOD    GetLongDesc(nsAWritableString& aLongDesc) { return _to GetLongDesc(aLongDesc); } \
  NS_IMETHOD    SetLongDesc(const nsAReadableString& aLongDesc) { return _to SetLongDesc(aLongDesc); } \
  NS_IMETHOD    GetSrc(nsAWritableString& aSrc) { return _to GetSrc(aSrc); } \
  NS_IMETHOD    SetSrc(const nsAReadableString& aSrc) { return _to SetSrc(aSrc); } \
  NS_IMETHOD    GetVspace(nsAWritableString& aVspace) { return _to GetVspace(aVspace); } \
  NS_IMETHOD    SetVspace(const nsAReadableString& aVspace) { return _to SetVspace(aVspace); } \
  NS_IMETHOD    GetWidth(nsAWritableString& aWidth) { return _to GetWidth(aWidth); } \
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth) { return _to SetWidth(aWidth); } \
  NS_IMETHOD    GetUseMap(nsAWritableString& aUseMap) { return _to GetUseMap(aUseMap); } \
  NS_IMETHOD    SetUseMap(const nsAReadableString& aUseMap) { return _to SetUseMap(aUseMap); } \


extern "C" NS_DOM nsresult NS_InitHTMLImageElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLImageElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLImageElement_h__

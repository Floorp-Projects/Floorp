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

#ifndef nsIDOMHTMLBodyElement_h__
#define nsIDOMHTMLBodyElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLBODYELEMENT_IID \
 { 0xa6cf908e, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLBodyElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLBODYELEMENT_IID; return iid; }

  NS_IMETHOD    GetALink(nsAWritableString& aALink)=0;
  NS_IMETHOD    SetALink(const nsAReadableString& aALink)=0;

  NS_IMETHOD    GetBackground(nsAWritableString& aBackground)=0;
  NS_IMETHOD    SetBackground(const nsAReadableString& aBackground)=0;

  NS_IMETHOD    GetBgColor(nsAWritableString& aBgColor)=0;
  NS_IMETHOD    SetBgColor(const nsAReadableString& aBgColor)=0;

  NS_IMETHOD    GetLink(nsAWritableString& aLink)=0;
  NS_IMETHOD    SetLink(const nsAReadableString& aLink)=0;

  NS_IMETHOD    GetText(nsAWritableString& aText)=0;
  NS_IMETHOD    SetText(const nsAReadableString& aText)=0;

  NS_IMETHOD    GetVLink(nsAWritableString& aVLink)=0;
  NS_IMETHOD    SetVLink(const nsAReadableString& aVLink)=0;
};


#define NS_DECL_IDOMHTMLBODYELEMENT   \
  NS_IMETHOD    GetALink(nsAWritableString& aALink);  \
  NS_IMETHOD    SetALink(const nsAReadableString& aALink);  \
  NS_IMETHOD    GetBackground(nsAWritableString& aBackground);  \
  NS_IMETHOD    SetBackground(const nsAReadableString& aBackground);  \
  NS_IMETHOD    GetBgColor(nsAWritableString& aBgColor);  \
  NS_IMETHOD    SetBgColor(const nsAReadableString& aBgColor);  \
  NS_IMETHOD    GetLink(nsAWritableString& aLink);  \
  NS_IMETHOD    SetLink(const nsAReadableString& aLink);  \
  NS_IMETHOD    GetText(nsAWritableString& aText);  \
  NS_IMETHOD    SetText(const nsAReadableString& aText);  \
  NS_IMETHOD    GetVLink(nsAWritableString& aVLink);  \
  NS_IMETHOD    SetVLink(const nsAReadableString& aVLink);  \



#define NS_FORWARD_IDOMHTMLBODYELEMENT(_to)  \
  NS_IMETHOD    GetALink(nsAWritableString& aALink) { return _to GetALink(aALink); } \
  NS_IMETHOD    SetALink(const nsAReadableString& aALink) { return _to SetALink(aALink); } \
  NS_IMETHOD    GetBackground(nsAWritableString& aBackground) { return _to GetBackground(aBackground); } \
  NS_IMETHOD    SetBackground(const nsAReadableString& aBackground) { return _to SetBackground(aBackground); } \
  NS_IMETHOD    GetBgColor(nsAWritableString& aBgColor) { return _to GetBgColor(aBgColor); } \
  NS_IMETHOD    SetBgColor(const nsAReadableString& aBgColor) { return _to SetBgColor(aBgColor); } \
  NS_IMETHOD    GetLink(nsAWritableString& aLink) { return _to GetLink(aLink); } \
  NS_IMETHOD    SetLink(const nsAReadableString& aLink) { return _to SetLink(aLink); } \
  NS_IMETHOD    GetText(nsAWritableString& aText) { return _to GetText(aText); } \
  NS_IMETHOD    SetText(const nsAReadableString& aText) { return _to SetText(aText); } \
  NS_IMETHOD    GetVLink(nsAWritableString& aVLink) { return _to GetVLink(aVLink); } \
  NS_IMETHOD    SetVLink(const nsAReadableString& aVLink) { return _to SetVLink(aVLink); } \


extern "C" NS_DOM nsresult NS_InitHTMLBodyElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLBodyElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLBodyElement_h__

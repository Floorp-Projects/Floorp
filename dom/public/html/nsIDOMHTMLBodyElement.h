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

  NS_IMETHOD    GetALink(nsString& aALink)=0;
  NS_IMETHOD    SetALink(const nsString& aALink)=0;

  NS_IMETHOD    GetBackground(nsString& aBackground)=0;
  NS_IMETHOD    SetBackground(const nsString& aBackground)=0;

  NS_IMETHOD    GetBgColor(nsString& aBgColor)=0;
  NS_IMETHOD    SetBgColor(const nsString& aBgColor)=0;

  NS_IMETHOD    GetLink(nsString& aLink)=0;
  NS_IMETHOD    SetLink(const nsString& aLink)=0;

  NS_IMETHOD    GetText(nsString& aText)=0;
  NS_IMETHOD    SetText(const nsString& aText)=0;

  NS_IMETHOD    GetVLink(nsString& aVLink)=0;
  NS_IMETHOD    SetVLink(const nsString& aVLink)=0;
};


#define NS_DECL_IDOMHTMLBODYELEMENT   \
  NS_IMETHOD    GetALink(nsString& aALink);  \
  NS_IMETHOD    SetALink(const nsString& aALink);  \
  NS_IMETHOD    GetBackground(nsString& aBackground);  \
  NS_IMETHOD    SetBackground(const nsString& aBackground);  \
  NS_IMETHOD    GetBgColor(nsString& aBgColor);  \
  NS_IMETHOD    SetBgColor(const nsString& aBgColor);  \
  NS_IMETHOD    GetLink(nsString& aLink);  \
  NS_IMETHOD    SetLink(const nsString& aLink);  \
  NS_IMETHOD    GetText(nsString& aText);  \
  NS_IMETHOD    SetText(const nsString& aText);  \
  NS_IMETHOD    GetVLink(nsString& aVLink);  \
  NS_IMETHOD    SetVLink(const nsString& aVLink);  \



#define NS_FORWARD_IDOMHTMLBODYELEMENT(_to)  \
  NS_IMETHOD    GetALink(nsString& aALink) { return _to GetALink(aALink); } \
  NS_IMETHOD    SetALink(const nsString& aALink) { return _to SetALink(aALink); } \
  NS_IMETHOD    GetBackground(nsString& aBackground) { return _to GetBackground(aBackground); } \
  NS_IMETHOD    SetBackground(const nsString& aBackground) { return _to SetBackground(aBackground); } \
  NS_IMETHOD    GetBgColor(nsString& aBgColor) { return _to GetBgColor(aBgColor); } \
  NS_IMETHOD    SetBgColor(const nsString& aBgColor) { return _to SetBgColor(aBgColor); } \
  NS_IMETHOD    GetLink(nsString& aLink) { return _to GetLink(aLink); } \
  NS_IMETHOD    SetLink(const nsString& aLink) { return _to SetLink(aLink); } \
  NS_IMETHOD    GetText(nsString& aText) { return _to GetText(aText); } \
  NS_IMETHOD    SetText(const nsString& aText) { return _to SetText(aText); } \
  NS_IMETHOD    GetVLink(nsString& aVLink) { return _to GetVLink(aVLink); } \
  NS_IMETHOD    SetVLink(const nsString& aVLink) { return _to SetVLink(aVLink); } \


extern "C" NS_DOM nsresult NS_InitHTMLBodyElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLBodyElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLBodyElement_h__

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

#ifndef nsIDOMHTMLTableCellElement_h__
#define nsIDOMHTMLTableCellElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLTABLECELLELEMENT_IID \
 { 0xa6cf90b7, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLTableCellElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLTABLECELLELEMENT_IID; return iid; }

  NS_IMETHOD    GetCellIndex(PRInt32* aCellIndex)=0;
  NS_IMETHOD    SetCellIndex(PRInt32 aCellIndex)=0;

  NS_IMETHOD    GetAbbr(nsString& aAbbr)=0;
  NS_IMETHOD    SetAbbr(const nsString& aAbbr)=0;

  NS_IMETHOD    GetAlign(nsString& aAlign)=0;
  NS_IMETHOD    SetAlign(const nsString& aAlign)=0;

  NS_IMETHOD    GetAxis(nsString& aAxis)=0;
  NS_IMETHOD    SetAxis(const nsString& aAxis)=0;

  NS_IMETHOD    GetBgColor(nsString& aBgColor)=0;
  NS_IMETHOD    SetBgColor(const nsString& aBgColor)=0;

  NS_IMETHOD    GetCh(nsString& aCh)=0;
  NS_IMETHOD    SetCh(const nsString& aCh)=0;

  NS_IMETHOD    GetChOff(nsString& aChOff)=0;
  NS_IMETHOD    SetChOff(const nsString& aChOff)=0;

  NS_IMETHOD    GetColSpan(PRInt32* aColSpan)=0;
  NS_IMETHOD    SetColSpan(PRInt32 aColSpan)=0;

  NS_IMETHOD    GetHeaders(nsString& aHeaders)=0;
  NS_IMETHOD    SetHeaders(const nsString& aHeaders)=0;

  NS_IMETHOD    GetHeight(nsString& aHeight)=0;
  NS_IMETHOD    SetHeight(const nsString& aHeight)=0;

  NS_IMETHOD    GetNoWrap(PRBool* aNoWrap)=0;
  NS_IMETHOD    SetNoWrap(PRBool aNoWrap)=0;

  NS_IMETHOD    GetRowSpan(PRInt32* aRowSpan)=0;
  NS_IMETHOD    SetRowSpan(PRInt32 aRowSpan)=0;

  NS_IMETHOD    GetScope(nsString& aScope)=0;
  NS_IMETHOD    SetScope(const nsString& aScope)=0;

  NS_IMETHOD    GetVAlign(nsString& aVAlign)=0;
  NS_IMETHOD    SetVAlign(const nsString& aVAlign)=0;

  NS_IMETHOD    GetWidth(nsString& aWidth)=0;
  NS_IMETHOD    SetWidth(const nsString& aWidth)=0;
};


#define NS_DECL_IDOMHTMLTABLECELLELEMENT   \
  NS_IMETHOD    GetCellIndex(PRInt32* aCellIndex);  \
  NS_IMETHOD    SetCellIndex(PRInt32 aCellIndex);  \
  NS_IMETHOD    GetAbbr(nsString& aAbbr);  \
  NS_IMETHOD    SetAbbr(const nsString& aAbbr);  \
  NS_IMETHOD    GetAlign(nsString& aAlign);  \
  NS_IMETHOD    SetAlign(const nsString& aAlign);  \
  NS_IMETHOD    GetAxis(nsString& aAxis);  \
  NS_IMETHOD    SetAxis(const nsString& aAxis);  \
  NS_IMETHOD    GetBgColor(nsString& aBgColor);  \
  NS_IMETHOD    SetBgColor(const nsString& aBgColor);  \
  NS_IMETHOD    GetCh(nsString& aCh);  \
  NS_IMETHOD    SetCh(const nsString& aCh);  \
  NS_IMETHOD    GetChOff(nsString& aChOff);  \
  NS_IMETHOD    SetChOff(const nsString& aChOff);  \
  NS_IMETHOD    GetColSpan(PRInt32* aColSpan);  \
  NS_IMETHOD    SetColSpan(PRInt32 aColSpan);  \
  NS_IMETHOD    GetHeaders(nsString& aHeaders);  \
  NS_IMETHOD    SetHeaders(const nsString& aHeaders);  \
  NS_IMETHOD    GetHeight(nsString& aHeight);  \
  NS_IMETHOD    SetHeight(const nsString& aHeight);  \
  NS_IMETHOD    GetNoWrap(PRBool* aNoWrap);  \
  NS_IMETHOD    SetNoWrap(PRBool aNoWrap);  \
  NS_IMETHOD    GetRowSpan(PRInt32* aRowSpan);  \
  NS_IMETHOD    SetRowSpan(PRInt32 aRowSpan);  \
  NS_IMETHOD    GetScope(nsString& aScope);  \
  NS_IMETHOD    SetScope(const nsString& aScope);  \
  NS_IMETHOD    GetVAlign(nsString& aVAlign);  \
  NS_IMETHOD    SetVAlign(const nsString& aVAlign);  \
  NS_IMETHOD    GetWidth(nsString& aWidth);  \
  NS_IMETHOD    SetWidth(const nsString& aWidth);  \



#define NS_FORWARD_IDOMHTMLTABLECELLELEMENT(_to)  \
  NS_IMETHOD    GetCellIndex(PRInt32* aCellIndex) { return _to GetCellIndex(aCellIndex); } \
  NS_IMETHOD    SetCellIndex(PRInt32 aCellIndex) { return _to SetCellIndex(aCellIndex); } \
  NS_IMETHOD    GetAbbr(nsString& aAbbr) { return _to GetAbbr(aAbbr); } \
  NS_IMETHOD    SetAbbr(const nsString& aAbbr) { return _to SetAbbr(aAbbr); } \
  NS_IMETHOD    GetAlign(nsString& aAlign) { return _to GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsString& aAlign) { return _to SetAlign(aAlign); } \
  NS_IMETHOD    GetAxis(nsString& aAxis) { return _to GetAxis(aAxis); } \
  NS_IMETHOD    SetAxis(const nsString& aAxis) { return _to SetAxis(aAxis); } \
  NS_IMETHOD    GetBgColor(nsString& aBgColor) { return _to GetBgColor(aBgColor); } \
  NS_IMETHOD    SetBgColor(const nsString& aBgColor) { return _to SetBgColor(aBgColor); } \
  NS_IMETHOD    GetCh(nsString& aCh) { return _to GetCh(aCh); } \
  NS_IMETHOD    SetCh(const nsString& aCh) { return _to SetCh(aCh); } \
  NS_IMETHOD    GetChOff(nsString& aChOff) { return _to GetChOff(aChOff); } \
  NS_IMETHOD    SetChOff(const nsString& aChOff) { return _to SetChOff(aChOff); } \
  NS_IMETHOD    GetColSpan(PRInt32* aColSpan) { return _to GetColSpan(aColSpan); } \
  NS_IMETHOD    SetColSpan(PRInt32 aColSpan) { return _to SetColSpan(aColSpan); } \
  NS_IMETHOD    GetHeaders(nsString& aHeaders) { return _to GetHeaders(aHeaders); } \
  NS_IMETHOD    SetHeaders(const nsString& aHeaders) { return _to SetHeaders(aHeaders); } \
  NS_IMETHOD    GetHeight(nsString& aHeight) { return _to GetHeight(aHeight); } \
  NS_IMETHOD    SetHeight(const nsString& aHeight) { return _to SetHeight(aHeight); } \
  NS_IMETHOD    GetNoWrap(PRBool* aNoWrap) { return _to GetNoWrap(aNoWrap); } \
  NS_IMETHOD    SetNoWrap(PRBool aNoWrap) { return _to SetNoWrap(aNoWrap); } \
  NS_IMETHOD    GetRowSpan(PRInt32* aRowSpan) { return _to GetRowSpan(aRowSpan); } \
  NS_IMETHOD    SetRowSpan(PRInt32 aRowSpan) { return _to SetRowSpan(aRowSpan); } \
  NS_IMETHOD    GetScope(nsString& aScope) { return _to GetScope(aScope); } \
  NS_IMETHOD    SetScope(const nsString& aScope) { return _to SetScope(aScope); } \
  NS_IMETHOD    GetVAlign(nsString& aVAlign) { return _to GetVAlign(aVAlign); } \
  NS_IMETHOD    SetVAlign(const nsString& aVAlign) { return _to SetVAlign(aVAlign); } \
  NS_IMETHOD    GetWidth(nsString& aWidth) { return _to GetWidth(aWidth); } \
  NS_IMETHOD    SetWidth(const nsString& aWidth) { return _to SetWidth(aWidth); } \


extern "C" NS_DOM nsresult NS_InitHTMLTableCellElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLTableCellElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLTableCellElement_h__

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

  NS_IMETHOD    GetAbbr(nsAWritableString& aAbbr)=0;
  NS_IMETHOD    SetAbbr(const nsAReadableString& aAbbr)=0;

  NS_IMETHOD    GetAlign(nsAWritableString& aAlign)=0;
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign)=0;

  NS_IMETHOD    GetAxis(nsAWritableString& aAxis)=0;
  NS_IMETHOD    SetAxis(const nsAReadableString& aAxis)=0;

  NS_IMETHOD    GetBgColor(nsAWritableString& aBgColor)=0;
  NS_IMETHOD    SetBgColor(const nsAReadableString& aBgColor)=0;

  NS_IMETHOD    GetCh(nsAWritableString& aCh)=0;
  NS_IMETHOD    SetCh(const nsAReadableString& aCh)=0;

  NS_IMETHOD    GetChOff(nsAWritableString& aChOff)=0;
  NS_IMETHOD    SetChOff(const nsAReadableString& aChOff)=0;

  NS_IMETHOD    GetColSpan(PRInt32* aColSpan)=0;
  NS_IMETHOD    SetColSpan(PRInt32 aColSpan)=0;

  NS_IMETHOD    GetHeaders(nsAWritableString& aHeaders)=0;
  NS_IMETHOD    SetHeaders(const nsAReadableString& aHeaders)=0;

  NS_IMETHOD    GetHeight(nsAWritableString& aHeight)=0;
  NS_IMETHOD    SetHeight(const nsAReadableString& aHeight)=0;

  NS_IMETHOD    GetNoWrap(PRBool* aNoWrap)=0;
  NS_IMETHOD    SetNoWrap(PRBool aNoWrap)=0;

  NS_IMETHOD    GetRowSpan(PRInt32* aRowSpan)=0;
  NS_IMETHOD    SetRowSpan(PRInt32 aRowSpan)=0;

  NS_IMETHOD    GetScope(nsAWritableString& aScope)=0;
  NS_IMETHOD    SetScope(const nsAReadableString& aScope)=0;

  NS_IMETHOD    GetVAlign(nsAWritableString& aVAlign)=0;
  NS_IMETHOD    SetVAlign(const nsAReadableString& aVAlign)=0;

  NS_IMETHOD    GetWidth(nsAWritableString& aWidth)=0;
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth)=0;
};


#define NS_DECL_IDOMHTMLTABLECELLELEMENT   \
  NS_IMETHOD    GetCellIndex(PRInt32* aCellIndex);  \
  NS_IMETHOD    SetCellIndex(PRInt32 aCellIndex);  \
  NS_IMETHOD    GetAbbr(nsAWritableString& aAbbr);  \
  NS_IMETHOD    SetAbbr(const nsAReadableString& aAbbr);  \
  NS_IMETHOD    GetAlign(nsAWritableString& aAlign);  \
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign);  \
  NS_IMETHOD    GetAxis(nsAWritableString& aAxis);  \
  NS_IMETHOD    SetAxis(const nsAReadableString& aAxis);  \
  NS_IMETHOD    GetBgColor(nsAWritableString& aBgColor);  \
  NS_IMETHOD    SetBgColor(const nsAReadableString& aBgColor);  \
  NS_IMETHOD    GetCh(nsAWritableString& aCh);  \
  NS_IMETHOD    SetCh(const nsAReadableString& aCh);  \
  NS_IMETHOD    GetChOff(nsAWritableString& aChOff);  \
  NS_IMETHOD    SetChOff(const nsAReadableString& aChOff);  \
  NS_IMETHOD    GetColSpan(PRInt32* aColSpan);  \
  NS_IMETHOD    SetColSpan(PRInt32 aColSpan);  \
  NS_IMETHOD    GetHeaders(nsAWritableString& aHeaders);  \
  NS_IMETHOD    SetHeaders(const nsAReadableString& aHeaders);  \
  NS_IMETHOD    GetHeight(nsAWritableString& aHeight);  \
  NS_IMETHOD    SetHeight(const nsAReadableString& aHeight);  \
  NS_IMETHOD    GetNoWrap(PRBool* aNoWrap);  \
  NS_IMETHOD    SetNoWrap(PRBool aNoWrap);  \
  NS_IMETHOD    GetRowSpan(PRInt32* aRowSpan);  \
  NS_IMETHOD    SetRowSpan(PRInt32 aRowSpan);  \
  NS_IMETHOD    GetScope(nsAWritableString& aScope);  \
  NS_IMETHOD    SetScope(const nsAReadableString& aScope);  \
  NS_IMETHOD    GetVAlign(nsAWritableString& aVAlign);  \
  NS_IMETHOD    SetVAlign(const nsAReadableString& aVAlign);  \
  NS_IMETHOD    GetWidth(nsAWritableString& aWidth);  \
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth);  \



#define NS_FORWARD_IDOMHTMLTABLECELLELEMENT(_to)  \
  NS_IMETHOD    GetCellIndex(PRInt32* aCellIndex) { return _to GetCellIndex(aCellIndex); } \
  NS_IMETHOD    SetCellIndex(PRInt32 aCellIndex) { return _to SetCellIndex(aCellIndex); } \
  NS_IMETHOD    GetAbbr(nsAWritableString& aAbbr) { return _to GetAbbr(aAbbr); } \
  NS_IMETHOD    SetAbbr(const nsAReadableString& aAbbr) { return _to SetAbbr(aAbbr); } \
  NS_IMETHOD    GetAlign(nsAWritableString& aAlign) { return _to GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign) { return _to SetAlign(aAlign); } \
  NS_IMETHOD    GetAxis(nsAWritableString& aAxis) { return _to GetAxis(aAxis); } \
  NS_IMETHOD    SetAxis(const nsAReadableString& aAxis) { return _to SetAxis(aAxis); } \
  NS_IMETHOD    GetBgColor(nsAWritableString& aBgColor) { return _to GetBgColor(aBgColor); } \
  NS_IMETHOD    SetBgColor(const nsAReadableString& aBgColor) { return _to SetBgColor(aBgColor); } \
  NS_IMETHOD    GetCh(nsAWritableString& aCh) { return _to GetCh(aCh); } \
  NS_IMETHOD    SetCh(const nsAReadableString& aCh) { return _to SetCh(aCh); } \
  NS_IMETHOD    GetChOff(nsAWritableString& aChOff) { return _to GetChOff(aChOff); } \
  NS_IMETHOD    SetChOff(const nsAReadableString& aChOff) { return _to SetChOff(aChOff); } \
  NS_IMETHOD    GetColSpan(PRInt32* aColSpan) { return _to GetColSpan(aColSpan); } \
  NS_IMETHOD    SetColSpan(PRInt32 aColSpan) { return _to SetColSpan(aColSpan); } \
  NS_IMETHOD    GetHeaders(nsAWritableString& aHeaders) { return _to GetHeaders(aHeaders); } \
  NS_IMETHOD    SetHeaders(const nsAReadableString& aHeaders) { return _to SetHeaders(aHeaders); } \
  NS_IMETHOD    GetHeight(nsAWritableString& aHeight) { return _to GetHeight(aHeight); } \
  NS_IMETHOD    SetHeight(const nsAReadableString& aHeight) { return _to SetHeight(aHeight); } \
  NS_IMETHOD    GetNoWrap(PRBool* aNoWrap) { return _to GetNoWrap(aNoWrap); } \
  NS_IMETHOD    SetNoWrap(PRBool aNoWrap) { return _to SetNoWrap(aNoWrap); } \
  NS_IMETHOD    GetRowSpan(PRInt32* aRowSpan) { return _to GetRowSpan(aRowSpan); } \
  NS_IMETHOD    SetRowSpan(PRInt32 aRowSpan) { return _to SetRowSpan(aRowSpan); } \
  NS_IMETHOD    GetScope(nsAWritableString& aScope) { return _to GetScope(aScope); } \
  NS_IMETHOD    SetScope(const nsAReadableString& aScope) { return _to SetScope(aScope); } \
  NS_IMETHOD    GetVAlign(nsAWritableString& aVAlign) { return _to GetVAlign(aVAlign); } \
  NS_IMETHOD    SetVAlign(const nsAReadableString& aVAlign) { return _to SetVAlign(aVAlign); } \
  NS_IMETHOD    GetWidth(nsAWritableString& aWidth) { return _to GetWidth(aWidth); } \
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth) { return _to SetWidth(aWidth); } \


extern "C" NS_DOM nsresult NS_InitHTMLTableCellElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLTableCellElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLTableCellElement_h__

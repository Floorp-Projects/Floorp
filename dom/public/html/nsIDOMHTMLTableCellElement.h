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
{ 0x6f76531e,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMHTMLTableCellElement : public nsIDOMHTMLElement {
public:

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



#define NS_FORWARD_IDOMHTMLTABLECELLELEMENT(superClass)  \
  NS_IMETHOD    GetCellIndex(PRInt32* aCellIndex) { return superClass::GetCellIndex(aCellIndex); } \
  NS_IMETHOD    SetCellIndex(PRInt32 aCellIndex) { return superClass::SetCellIndex(aCellIndex); } \
  NS_IMETHOD    GetAbbr(nsString& aAbbr) { return superClass::GetAbbr(aAbbr); } \
  NS_IMETHOD    SetAbbr(const nsString& aAbbr) { return superClass::SetAbbr(aAbbr); } \
  NS_IMETHOD    GetAlign(nsString& aAlign) { return superClass::GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsString& aAlign) { return superClass::SetAlign(aAlign); } \
  NS_IMETHOD    GetAxis(nsString& aAxis) { return superClass::GetAxis(aAxis); } \
  NS_IMETHOD    SetAxis(const nsString& aAxis) { return superClass::SetAxis(aAxis); } \
  NS_IMETHOD    GetBgColor(nsString& aBgColor) { return superClass::GetBgColor(aBgColor); } \
  NS_IMETHOD    SetBgColor(const nsString& aBgColor) { return superClass::SetBgColor(aBgColor); } \
  NS_IMETHOD    GetCh(nsString& aCh) { return superClass::GetCh(aCh); } \
  NS_IMETHOD    SetCh(const nsString& aCh) { return superClass::SetCh(aCh); } \
  NS_IMETHOD    GetChOff(nsString& aChOff) { return superClass::GetChOff(aChOff); } \
  NS_IMETHOD    SetChOff(const nsString& aChOff) { return superClass::SetChOff(aChOff); } \
  NS_IMETHOD    GetColSpan(PRInt32* aColSpan) { return superClass::GetColSpan(aColSpan); } \
  NS_IMETHOD    SetColSpan(PRInt32 aColSpan) { return superClass::SetColSpan(aColSpan); } \
  NS_IMETHOD    GetHeaders(nsString& aHeaders) { return superClass::GetHeaders(aHeaders); } \
  NS_IMETHOD    SetHeaders(const nsString& aHeaders) { return superClass::SetHeaders(aHeaders); } \
  NS_IMETHOD    GetHeight(nsString& aHeight) { return superClass::GetHeight(aHeight); } \
  NS_IMETHOD    SetHeight(const nsString& aHeight) { return superClass::SetHeight(aHeight); } \
  NS_IMETHOD    GetNoWrap(PRBool* aNoWrap) { return superClass::GetNoWrap(aNoWrap); } \
  NS_IMETHOD    SetNoWrap(PRBool aNoWrap) { return superClass::SetNoWrap(aNoWrap); } \
  NS_IMETHOD    GetRowSpan(PRInt32* aRowSpan) { return superClass::GetRowSpan(aRowSpan); } \
  NS_IMETHOD    SetRowSpan(PRInt32 aRowSpan) { return superClass::SetRowSpan(aRowSpan); } \
  NS_IMETHOD    GetScope(nsString& aScope) { return superClass::GetScope(aScope); } \
  NS_IMETHOD    SetScope(const nsString& aScope) { return superClass::SetScope(aScope); } \
  NS_IMETHOD    GetVAlign(nsString& aVAlign) { return superClass::GetVAlign(aVAlign); } \
  NS_IMETHOD    SetVAlign(const nsString& aVAlign) { return superClass::SetVAlign(aVAlign); } \
  NS_IMETHOD    GetWidth(nsString& aWidth) { return superClass::GetWidth(aWidth); } \
  NS_IMETHOD    SetWidth(const nsString& aWidth) { return superClass::SetWidth(aWidth); } \


extern nsresult NS_InitHTMLTableCellElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLTableCellElement(nsIScriptContext *aContext, nsIDOMHTMLTableCellElement *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLTableCellElement_h__

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

#ifndef nsIDOMHTMLTableRowElement_h__
#define nsIDOMHTMLTableRowElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"

class nsIDOMHTMLElement;
class nsIDOMHTMLCollection;

#define NS_IDOMHTMLTABLEROWELEMENT_IID \
 { 0xa6cf90b6, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLTableRowElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLTABLEROWELEMENT_IID; return iid; }

  NS_IMETHOD    GetRowIndex(PRInt32* aRowIndex)=0;
  NS_IMETHOD    SetRowIndex(PRInt32 aRowIndex)=0;

  NS_IMETHOD    GetSectionRowIndex(PRInt32* aSectionRowIndex)=0;
  NS_IMETHOD    SetSectionRowIndex(PRInt32 aSectionRowIndex)=0;

  NS_IMETHOD    GetCells(nsIDOMHTMLCollection** aCells)=0;
  NS_IMETHOD    SetCells(nsIDOMHTMLCollection* aCells)=0;

  NS_IMETHOD    GetAlign(nsAWritableString& aAlign)=0;
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign)=0;

  NS_IMETHOD    GetBgColor(nsAWritableString& aBgColor)=0;
  NS_IMETHOD    SetBgColor(const nsAReadableString& aBgColor)=0;

  NS_IMETHOD    GetCh(nsAWritableString& aCh)=0;
  NS_IMETHOD    SetCh(const nsAReadableString& aCh)=0;

  NS_IMETHOD    GetChOff(nsAWritableString& aChOff)=0;
  NS_IMETHOD    SetChOff(const nsAReadableString& aChOff)=0;

  NS_IMETHOD    GetVAlign(nsAWritableString& aVAlign)=0;
  NS_IMETHOD    SetVAlign(const nsAReadableString& aVAlign)=0;

  NS_IMETHOD    InsertCell(PRInt32 aIndex, nsIDOMHTMLElement** aReturn)=0;

  NS_IMETHOD    DeleteCell(PRInt32 aIndex)=0;
};


#define NS_DECL_IDOMHTMLTABLEROWELEMENT   \
  NS_IMETHOD    GetRowIndex(PRInt32* aRowIndex);  \
  NS_IMETHOD    SetRowIndex(PRInt32 aRowIndex);  \
  NS_IMETHOD    GetSectionRowIndex(PRInt32* aSectionRowIndex);  \
  NS_IMETHOD    SetSectionRowIndex(PRInt32 aSectionRowIndex);  \
  NS_IMETHOD    GetCells(nsIDOMHTMLCollection** aCells);  \
  NS_IMETHOD    SetCells(nsIDOMHTMLCollection* aCells);  \
  NS_IMETHOD    GetAlign(nsAWritableString& aAlign);  \
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign);  \
  NS_IMETHOD    GetBgColor(nsAWritableString& aBgColor);  \
  NS_IMETHOD    SetBgColor(const nsAReadableString& aBgColor);  \
  NS_IMETHOD    GetCh(nsAWritableString& aCh);  \
  NS_IMETHOD    SetCh(const nsAReadableString& aCh);  \
  NS_IMETHOD    GetChOff(nsAWritableString& aChOff);  \
  NS_IMETHOD    SetChOff(const nsAReadableString& aChOff);  \
  NS_IMETHOD    GetVAlign(nsAWritableString& aVAlign);  \
  NS_IMETHOD    SetVAlign(const nsAReadableString& aVAlign);  \
  NS_IMETHOD    InsertCell(PRInt32 aIndex, nsIDOMHTMLElement** aReturn);  \
  NS_IMETHOD    DeleteCell(PRInt32 aIndex);  \



#define NS_FORWARD_IDOMHTMLTABLEROWELEMENT(_to)  \
  NS_IMETHOD    GetRowIndex(PRInt32* aRowIndex) { return _to GetRowIndex(aRowIndex); } \
  NS_IMETHOD    SetRowIndex(PRInt32 aRowIndex) { return _to SetRowIndex(aRowIndex); } \
  NS_IMETHOD    GetSectionRowIndex(PRInt32* aSectionRowIndex) { return _to GetSectionRowIndex(aSectionRowIndex); } \
  NS_IMETHOD    SetSectionRowIndex(PRInt32 aSectionRowIndex) { return _to SetSectionRowIndex(aSectionRowIndex); } \
  NS_IMETHOD    GetCells(nsIDOMHTMLCollection** aCells) { return _to GetCells(aCells); } \
  NS_IMETHOD    SetCells(nsIDOMHTMLCollection* aCells) { return _to SetCells(aCells); } \
  NS_IMETHOD    GetAlign(nsAWritableString& aAlign) { return _to GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign) { return _to SetAlign(aAlign); } \
  NS_IMETHOD    GetBgColor(nsAWritableString& aBgColor) { return _to GetBgColor(aBgColor); } \
  NS_IMETHOD    SetBgColor(const nsAReadableString& aBgColor) { return _to SetBgColor(aBgColor); } \
  NS_IMETHOD    GetCh(nsAWritableString& aCh) { return _to GetCh(aCh); } \
  NS_IMETHOD    SetCh(const nsAReadableString& aCh) { return _to SetCh(aCh); } \
  NS_IMETHOD    GetChOff(nsAWritableString& aChOff) { return _to GetChOff(aChOff); } \
  NS_IMETHOD    SetChOff(const nsAReadableString& aChOff) { return _to SetChOff(aChOff); } \
  NS_IMETHOD    GetVAlign(nsAWritableString& aVAlign) { return _to GetVAlign(aVAlign); } \
  NS_IMETHOD    SetVAlign(const nsAReadableString& aVAlign) { return _to SetVAlign(aVAlign); } \
  NS_IMETHOD    InsertCell(PRInt32 aIndex, nsIDOMHTMLElement** aReturn) { return _to InsertCell(aIndex, aReturn); }  \
  NS_IMETHOD    DeleteCell(PRInt32 aIndex) { return _to DeleteCell(aIndex); }  \


extern "C" NS_DOM nsresult NS_InitHTMLTableRowElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLTableRowElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLTableRowElement_h__

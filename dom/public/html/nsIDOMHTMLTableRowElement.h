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

  NS_IMETHOD    GetAlign(nsString& aAlign)=0;
  NS_IMETHOD    SetAlign(const nsString& aAlign)=0;

  NS_IMETHOD    GetBgColor(nsString& aBgColor)=0;
  NS_IMETHOD    SetBgColor(const nsString& aBgColor)=0;

  NS_IMETHOD    GetCh(nsString& aCh)=0;
  NS_IMETHOD    SetCh(const nsString& aCh)=0;

  NS_IMETHOD    GetChOff(nsString& aChOff)=0;
  NS_IMETHOD    SetChOff(const nsString& aChOff)=0;

  NS_IMETHOD    GetVAlign(nsString& aVAlign)=0;
  NS_IMETHOD    SetVAlign(const nsString& aVAlign)=0;

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
  NS_IMETHOD    GetAlign(nsString& aAlign);  \
  NS_IMETHOD    SetAlign(const nsString& aAlign);  \
  NS_IMETHOD    GetBgColor(nsString& aBgColor);  \
  NS_IMETHOD    SetBgColor(const nsString& aBgColor);  \
  NS_IMETHOD    GetCh(nsString& aCh);  \
  NS_IMETHOD    SetCh(const nsString& aCh);  \
  NS_IMETHOD    GetChOff(nsString& aChOff);  \
  NS_IMETHOD    SetChOff(const nsString& aChOff);  \
  NS_IMETHOD    GetVAlign(nsString& aVAlign);  \
  NS_IMETHOD    SetVAlign(const nsString& aVAlign);  \
  NS_IMETHOD    InsertCell(PRInt32 aIndex, nsIDOMHTMLElement** aReturn);  \
  NS_IMETHOD    DeleteCell(PRInt32 aIndex);  \



#define NS_FORWARD_IDOMHTMLTABLEROWELEMENT(_to)  \
  NS_IMETHOD    GetRowIndex(PRInt32* aRowIndex) { return _to GetRowIndex(aRowIndex); } \
  NS_IMETHOD    SetRowIndex(PRInt32 aRowIndex) { return _to SetRowIndex(aRowIndex); } \
  NS_IMETHOD    GetSectionRowIndex(PRInt32* aSectionRowIndex) { return _to GetSectionRowIndex(aSectionRowIndex); } \
  NS_IMETHOD    SetSectionRowIndex(PRInt32 aSectionRowIndex) { return _to SetSectionRowIndex(aSectionRowIndex); } \
  NS_IMETHOD    GetCells(nsIDOMHTMLCollection** aCells) { return _to GetCells(aCells); } \
  NS_IMETHOD    SetCells(nsIDOMHTMLCollection* aCells) { return _to SetCells(aCells); } \
  NS_IMETHOD    GetAlign(nsString& aAlign) { return _to GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsString& aAlign) { return _to SetAlign(aAlign); } \
  NS_IMETHOD    GetBgColor(nsString& aBgColor) { return _to GetBgColor(aBgColor); } \
  NS_IMETHOD    SetBgColor(const nsString& aBgColor) { return _to SetBgColor(aBgColor); } \
  NS_IMETHOD    GetCh(nsString& aCh) { return _to GetCh(aCh); } \
  NS_IMETHOD    SetCh(const nsString& aCh) { return _to SetCh(aCh); } \
  NS_IMETHOD    GetChOff(nsString& aChOff) { return _to GetChOff(aChOff); } \
  NS_IMETHOD    SetChOff(const nsString& aChOff) { return _to SetChOff(aChOff); } \
  NS_IMETHOD    GetVAlign(nsString& aVAlign) { return _to GetVAlign(aVAlign); } \
  NS_IMETHOD    SetVAlign(const nsString& aVAlign) { return _to SetVAlign(aVAlign); } \
  NS_IMETHOD    InsertCell(PRInt32 aIndex, nsIDOMHTMLElement** aReturn) { return _to InsertCell(aIndex, aReturn); }  \
  NS_IMETHOD    DeleteCell(PRInt32 aIndex) { return _to DeleteCell(aIndex); }  \


extern "C" NS_DOM nsresult NS_InitHTMLTableRowElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLTableRowElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLTableRowElement_h__

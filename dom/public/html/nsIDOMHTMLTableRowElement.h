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
{ 0x6f765321,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMHTMLTableRowElement : public nsIDOMHTMLElement {
public:

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



#define NS_FORWARD_IDOMHTMLTABLEROWELEMENT(superClass)  \
  NS_IMETHOD    GetRowIndex(PRInt32* aRowIndex) { return superClass::GetRowIndex(aRowIndex); } \
  NS_IMETHOD    SetRowIndex(PRInt32 aRowIndex) { return superClass::SetRowIndex(aRowIndex); } \
  NS_IMETHOD    GetSectionRowIndex(PRInt32* aSectionRowIndex) { return superClass::GetSectionRowIndex(aSectionRowIndex); } \
  NS_IMETHOD    SetSectionRowIndex(PRInt32 aSectionRowIndex) { return superClass::SetSectionRowIndex(aSectionRowIndex); } \
  NS_IMETHOD    GetCells(nsIDOMHTMLCollection** aCells) { return superClass::GetCells(aCells); } \
  NS_IMETHOD    SetCells(nsIDOMHTMLCollection* aCells) { return superClass::SetCells(aCells); } \
  NS_IMETHOD    GetAlign(nsString& aAlign) { return superClass::GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsString& aAlign) { return superClass::SetAlign(aAlign); } \
  NS_IMETHOD    GetBgColor(nsString& aBgColor) { return superClass::GetBgColor(aBgColor); } \
  NS_IMETHOD    SetBgColor(const nsString& aBgColor) { return superClass::SetBgColor(aBgColor); } \
  NS_IMETHOD    GetCh(nsString& aCh) { return superClass::GetCh(aCh); } \
  NS_IMETHOD    SetCh(const nsString& aCh) { return superClass::SetCh(aCh); } \
  NS_IMETHOD    GetChOff(nsString& aChOff) { return superClass::GetChOff(aChOff); } \
  NS_IMETHOD    SetChOff(const nsString& aChOff) { return superClass::SetChOff(aChOff); } \
  NS_IMETHOD    GetVAlign(nsString& aVAlign) { return superClass::GetVAlign(aVAlign); } \
  NS_IMETHOD    SetVAlign(const nsString& aVAlign) { return superClass::SetVAlign(aVAlign); } \
  NS_IMETHOD    InsertCell(PRInt32 aIndex, nsIDOMHTMLElement** aReturn) { return superClass::InsertCell(aIndex, aReturn); }  \
  NS_IMETHOD    DeleteCell(PRInt32 aIndex) { return superClass::DeleteCell(aIndex); }  \


extern nsresult NS_InitHTMLTableRowElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLTableRowElement(nsIScriptContext *aContext, nsIDOMHTMLTableRowElement *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLTableRowElement_h__

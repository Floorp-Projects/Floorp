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
class nsIDOMHTMLTableRowElement;
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

extern nsresult NS_InitHTMLTableRowElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLTableRowElement(nsIScriptContext *aContext, nsIDOMHTMLTableRowElement *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLTableRowElement_h__

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

#ifndef nsIDOMHTMLTableElement_h__
#define nsIDOMHTMLTableElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"

class nsIDOMHTMLElement;
class nsIDOMHTMLTableElement;
class nsIDOMHTMLTableCaptionElement;
class nsIDOMHTMLTableSectionElement;
class nsIDOMHTMLCollection;

#define NS_IDOMHTMLTABLEELEMENT_IID \
{ 0x6f765320,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMHTMLTableElement : public nsIDOMHTMLElement {
public:

  NS_IMETHOD    GetCaption(nsIDOMHTMLTableCaptionElement** aCaption)=0;
  NS_IMETHOD    SetCaption(nsIDOMHTMLTableCaptionElement* aCaption)=0;

  NS_IMETHOD    GetTHead(nsIDOMHTMLTableSectionElement** aTHead)=0;
  NS_IMETHOD    SetTHead(nsIDOMHTMLTableSectionElement* aTHead)=0;

  NS_IMETHOD    GetTFoot(nsIDOMHTMLTableSectionElement** aTFoot)=0;
  NS_IMETHOD    SetTFoot(nsIDOMHTMLTableSectionElement* aTFoot)=0;

  NS_IMETHOD    GetRows(nsIDOMHTMLCollection** aRows)=0;
  NS_IMETHOD    SetRows(nsIDOMHTMLCollection* aRows)=0;

  NS_IMETHOD    GetTBodies(nsIDOMHTMLCollection** aTBodies)=0;
  NS_IMETHOD    SetTBodies(nsIDOMHTMLCollection* aTBodies)=0;

  NS_IMETHOD    GetAlign(nsString& aAlign)=0;
  NS_IMETHOD    SetAlign(const nsString& aAlign)=0;

  NS_IMETHOD    GetBgColor(nsString& aBgColor)=0;
  NS_IMETHOD    SetBgColor(const nsString& aBgColor)=0;

  NS_IMETHOD    GetBorder(nsString& aBorder)=0;
  NS_IMETHOD    SetBorder(const nsString& aBorder)=0;

  NS_IMETHOD    GetCellPadding(nsString& aCellPadding)=0;
  NS_IMETHOD    SetCellPadding(const nsString& aCellPadding)=0;

  NS_IMETHOD    GetCellSpacing(nsString& aCellSpacing)=0;
  NS_IMETHOD    SetCellSpacing(const nsString& aCellSpacing)=0;

  NS_IMETHOD    GetFrame(nsString& aFrame)=0;
  NS_IMETHOD    SetFrame(const nsString& aFrame)=0;

  NS_IMETHOD    GetRules(nsString& aRules)=0;
  NS_IMETHOD    SetRules(const nsString& aRules)=0;

  NS_IMETHOD    GetSummary(nsString& aSummary)=0;
  NS_IMETHOD    SetSummary(const nsString& aSummary)=0;

  NS_IMETHOD    GetWidth(nsString& aWidth)=0;
  NS_IMETHOD    SetWidth(const nsString& aWidth)=0;

  NS_IMETHOD    CreateTHead(nsIDOMHTMLElement** aReturn)=0;

  NS_IMETHOD    DeleteTHead()=0;

  NS_IMETHOD    CreateTFoot(nsIDOMHTMLElement** aReturn)=0;

  NS_IMETHOD    DeleteTFoot()=0;

  NS_IMETHOD    CreateCaption(nsIDOMHTMLElement** aReturn)=0;

  NS_IMETHOD    DeleteCaption()=0;

  NS_IMETHOD    InsertRow(PRInt32 aIndex, nsIDOMHTMLElement** aReturn)=0;

  NS_IMETHOD    DeleteRow(PRInt32 aIndex)=0;
};

extern nsresult NS_InitHTMLTableElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLTableElement(nsIScriptContext *aContext, nsIDOMHTMLTableElement *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLTableElement_h__

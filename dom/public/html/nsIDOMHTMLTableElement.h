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
class nsIDOMHTMLTableCaptionElement;
class nsIDOMHTMLTableSectionElement;
class nsIDOMHTMLCollection;

#define NS_IDOMHTMLTABLEELEMENT_IID \
 { 0xa6cf90b2, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLTableElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLTABLEELEMENT_IID; return iid; }

  NS_IMETHOD    GetCaption(nsIDOMHTMLTableCaptionElement** aCaption)=0;
  NS_IMETHOD    SetCaption(nsIDOMHTMLTableCaptionElement* aCaption)=0;

  NS_IMETHOD    GetTHead(nsIDOMHTMLTableSectionElement** aTHead)=0;
  NS_IMETHOD    SetTHead(nsIDOMHTMLTableSectionElement* aTHead)=0;

  NS_IMETHOD    GetTFoot(nsIDOMHTMLTableSectionElement** aTFoot)=0;
  NS_IMETHOD    SetTFoot(nsIDOMHTMLTableSectionElement* aTFoot)=0;

  NS_IMETHOD    GetRows(nsIDOMHTMLCollection** aRows)=0;

  NS_IMETHOD    GetTBodies(nsIDOMHTMLCollection** aTBodies)=0;

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


#define NS_DECL_IDOMHTMLTABLEELEMENT   \
  NS_IMETHOD    GetCaption(nsIDOMHTMLTableCaptionElement** aCaption);  \
  NS_IMETHOD    SetCaption(nsIDOMHTMLTableCaptionElement* aCaption);  \
  NS_IMETHOD    GetTHead(nsIDOMHTMLTableSectionElement** aTHead);  \
  NS_IMETHOD    SetTHead(nsIDOMHTMLTableSectionElement* aTHead);  \
  NS_IMETHOD    GetTFoot(nsIDOMHTMLTableSectionElement** aTFoot);  \
  NS_IMETHOD    SetTFoot(nsIDOMHTMLTableSectionElement* aTFoot);  \
  NS_IMETHOD    GetRows(nsIDOMHTMLCollection** aRows);  \
  NS_IMETHOD    GetTBodies(nsIDOMHTMLCollection** aTBodies);  \
  NS_IMETHOD    GetAlign(nsString& aAlign);  \
  NS_IMETHOD    SetAlign(const nsString& aAlign);  \
  NS_IMETHOD    GetBgColor(nsString& aBgColor);  \
  NS_IMETHOD    SetBgColor(const nsString& aBgColor);  \
  NS_IMETHOD    GetBorder(nsString& aBorder);  \
  NS_IMETHOD    SetBorder(const nsString& aBorder);  \
  NS_IMETHOD    GetCellPadding(nsString& aCellPadding);  \
  NS_IMETHOD    SetCellPadding(const nsString& aCellPadding);  \
  NS_IMETHOD    GetCellSpacing(nsString& aCellSpacing);  \
  NS_IMETHOD    SetCellSpacing(const nsString& aCellSpacing);  \
  NS_IMETHOD    GetFrame(nsString& aFrame);  \
  NS_IMETHOD    SetFrame(const nsString& aFrame);  \
  NS_IMETHOD    GetRules(nsString& aRules);  \
  NS_IMETHOD    SetRules(const nsString& aRules);  \
  NS_IMETHOD    GetSummary(nsString& aSummary);  \
  NS_IMETHOD    SetSummary(const nsString& aSummary);  \
  NS_IMETHOD    GetWidth(nsString& aWidth);  \
  NS_IMETHOD    SetWidth(const nsString& aWidth);  \
  NS_IMETHOD    CreateTHead(nsIDOMHTMLElement** aReturn);  \
  NS_IMETHOD    DeleteTHead();  \
  NS_IMETHOD    CreateTFoot(nsIDOMHTMLElement** aReturn);  \
  NS_IMETHOD    DeleteTFoot();  \
  NS_IMETHOD    CreateCaption(nsIDOMHTMLElement** aReturn);  \
  NS_IMETHOD    DeleteCaption();  \
  NS_IMETHOD    InsertRow(PRInt32 aIndex, nsIDOMHTMLElement** aReturn);  \
  NS_IMETHOD    DeleteRow(PRInt32 aIndex);  \



#define NS_FORWARD_IDOMHTMLTABLEELEMENT(_to)  \
  NS_IMETHOD    GetCaption(nsIDOMHTMLTableCaptionElement** aCaption) { return _to GetCaption(aCaption); } \
  NS_IMETHOD    SetCaption(nsIDOMHTMLTableCaptionElement* aCaption) { return _to SetCaption(aCaption); } \
  NS_IMETHOD    GetTHead(nsIDOMHTMLTableSectionElement** aTHead) { return _to GetTHead(aTHead); } \
  NS_IMETHOD    SetTHead(nsIDOMHTMLTableSectionElement* aTHead) { return _to SetTHead(aTHead); } \
  NS_IMETHOD    GetTFoot(nsIDOMHTMLTableSectionElement** aTFoot) { return _to GetTFoot(aTFoot); } \
  NS_IMETHOD    SetTFoot(nsIDOMHTMLTableSectionElement* aTFoot) { return _to SetTFoot(aTFoot); } \
  NS_IMETHOD    GetRows(nsIDOMHTMLCollection** aRows) { return _to GetRows(aRows); } \
  NS_IMETHOD    GetTBodies(nsIDOMHTMLCollection** aTBodies) { return _to GetTBodies(aTBodies); } \
  NS_IMETHOD    GetAlign(nsString& aAlign) { return _to GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsString& aAlign) { return _to SetAlign(aAlign); } \
  NS_IMETHOD    GetBgColor(nsString& aBgColor) { return _to GetBgColor(aBgColor); } \
  NS_IMETHOD    SetBgColor(const nsString& aBgColor) { return _to SetBgColor(aBgColor); } \
  NS_IMETHOD    GetBorder(nsString& aBorder) { return _to GetBorder(aBorder); } \
  NS_IMETHOD    SetBorder(const nsString& aBorder) { return _to SetBorder(aBorder); } \
  NS_IMETHOD    GetCellPadding(nsString& aCellPadding) { return _to GetCellPadding(aCellPadding); } \
  NS_IMETHOD    SetCellPadding(const nsString& aCellPadding) { return _to SetCellPadding(aCellPadding); } \
  NS_IMETHOD    GetCellSpacing(nsString& aCellSpacing) { return _to GetCellSpacing(aCellSpacing); } \
  NS_IMETHOD    SetCellSpacing(const nsString& aCellSpacing) { return _to SetCellSpacing(aCellSpacing); } \
  NS_IMETHOD    GetFrame(nsString& aFrame) { return _to GetFrame(aFrame); } \
  NS_IMETHOD    SetFrame(const nsString& aFrame) { return _to SetFrame(aFrame); } \
  NS_IMETHOD    GetRules(nsString& aRules) { return _to GetRules(aRules); } \
  NS_IMETHOD    SetRules(const nsString& aRules) { return _to SetRules(aRules); } \
  NS_IMETHOD    GetSummary(nsString& aSummary) { return _to GetSummary(aSummary); } \
  NS_IMETHOD    SetSummary(const nsString& aSummary) { return _to SetSummary(aSummary); } \
  NS_IMETHOD    GetWidth(nsString& aWidth) { return _to GetWidth(aWidth); } \
  NS_IMETHOD    SetWidth(const nsString& aWidth) { return _to SetWidth(aWidth); } \
  NS_IMETHOD    CreateTHead(nsIDOMHTMLElement** aReturn) { return _to CreateTHead(aReturn); }  \
  NS_IMETHOD    DeleteTHead() { return _to DeleteTHead(); }  \
  NS_IMETHOD    CreateTFoot(nsIDOMHTMLElement** aReturn) { return _to CreateTFoot(aReturn); }  \
  NS_IMETHOD    DeleteTFoot() { return _to DeleteTFoot(); }  \
  NS_IMETHOD    CreateCaption(nsIDOMHTMLElement** aReturn) { return _to CreateCaption(aReturn); }  \
  NS_IMETHOD    DeleteCaption() { return _to DeleteCaption(); }  \
  NS_IMETHOD    InsertRow(PRInt32 aIndex, nsIDOMHTMLElement** aReturn) { return _to InsertRow(aIndex, aReturn); }  \
  NS_IMETHOD    DeleteRow(PRInt32 aIndex) { return _to DeleteRow(aIndex); }  \


extern "C" NS_DOM nsresult NS_InitHTMLTableElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLTableElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLTableElement_h__

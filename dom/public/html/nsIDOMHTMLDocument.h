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

#ifndef nsIDOMHTMLDocument_h__
#define nsIDOMHTMLDocument_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMDocument.h"
#include "jsapi.h"

class nsIDOMElement;
class nsIDOMHTMLElement;
class nsIDOMHTMLDocument;
class nsIDOMHTMLCollection;
class nsIDOMNodeList;

#define NS_IDOMHTMLDOCUMENT_IID \
{ 0x6f7652fb,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMHTMLDocument : public nsIDOMDocument {
public:

  NS_IMETHOD    GetTitle(nsString& aTitle)=0;
  NS_IMETHOD    SetTitle(const nsString& aTitle)=0;

  NS_IMETHOD    GetReferrer(nsString& aReferrer)=0;

  NS_IMETHOD    GetFileSize(nsString& aFileSize)=0;

  NS_IMETHOD    GetFileCreatedDate(nsString& aFileCreatedDate)=0;

  NS_IMETHOD    GetFileModifiedDate(nsString& aFileModifiedDate)=0;

  NS_IMETHOD    GetFileUpdatedDate(nsString& aFileUpdatedDate)=0;

  NS_IMETHOD    GetDomain(nsString& aDomain)=0;

  NS_IMETHOD    GetURL(nsString& aURL)=0;

  NS_IMETHOD    GetBody(nsIDOMHTMLElement** aBody)=0;
  NS_IMETHOD    SetBody(nsIDOMHTMLElement* aBody)=0;

  NS_IMETHOD    GetImages(nsIDOMHTMLCollection** aImages)=0;

  NS_IMETHOD    GetApplets(nsIDOMHTMLCollection** aApplets)=0;

  NS_IMETHOD    GetLinks(nsIDOMHTMLCollection** aLinks)=0;

  NS_IMETHOD    GetForms(nsIDOMHTMLCollection** aForms)=0;

  NS_IMETHOD    GetAnchors(nsIDOMHTMLCollection** aAnchors)=0;

  NS_IMETHOD    GetCookie(nsString& aCookie)=0;
  NS_IMETHOD    SetCookie(const nsString& aCookie)=0;

  NS_IMETHOD    Open(JSContext *cx, jsval *argv, PRUint32 argc)=0;

  NS_IMETHOD    Close()=0;

  NS_IMETHOD    Write(JSContext *cx, jsval *argv, PRUint32 argc)=0;

  NS_IMETHOD    Writeln(JSContext *cx, jsval *argv, PRUint32 argc)=0;

  NS_IMETHOD    GetElementById(const nsString& aElementId, nsIDOMElement** aReturn)=0;

  NS_IMETHOD    GetElementsByName(const nsString& aElementName, nsIDOMNodeList** aReturn)=0;
};

extern nsresult NS_InitHTMLDocumentClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLDocument(nsIScriptContext *aContext, nsIDOMHTMLDocument *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLDocument_h__

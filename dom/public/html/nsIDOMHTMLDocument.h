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
class nsIDOMHTMLCollection;
class nsIDOMNodeList;

#define NS_IDOMHTMLDOCUMENT_IID \
 { 0xa6cf9084, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLDocument : public nsIDOMDocument {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLDOCUMENT_IID; return iid; }

  NS_IMETHOD    GetTitle(nsString& aTitle)=0;
  NS_IMETHOD    SetTitle(const nsString& aTitle)=0;

  NS_IMETHOD    GetReferrer(nsString& aReferrer)=0;

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


#define NS_DECL_IDOMHTMLDOCUMENT   \
  NS_IMETHOD    GetTitle(nsString& aTitle);  \
  NS_IMETHOD    SetTitle(const nsString& aTitle);  \
  NS_IMETHOD    GetReferrer(nsString& aReferrer);  \
  NS_IMETHOD    GetDomain(nsString& aDomain);  \
  NS_IMETHOD    GetURL(nsString& aURL);  \
  NS_IMETHOD    GetBody(nsIDOMHTMLElement** aBody);  \
  NS_IMETHOD    SetBody(nsIDOMHTMLElement* aBody);  \
  NS_IMETHOD    GetImages(nsIDOMHTMLCollection** aImages);  \
  NS_IMETHOD    GetApplets(nsIDOMHTMLCollection** aApplets);  \
  NS_IMETHOD    GetLinks(nsIDOMHTMLCollection** aLinks);  \
  NS_IMETHOD    GetForms(nsIDOMHTMLCollection** aForms);  \
  NS_IMETHOD    GetAnchors(nsIDOMHTMLCollection** aAnchors);  \
  NS_IMETHOD    GetCookie(nsString& aCookie);  \
  NS_IMETHOD    SetCookie(const nsString& aCookie);  \
  NS_IMETHOD    Open(JSContext *cx, jsval *argv, PRUint32 argc);  \
  NS_IMETHOD    Close();  \
  NS_IMETHOD    Write(JSContext *cx, jsval *argv, PRUint32 argc);  \
  NS_IMETHOD    Writeln(JSContext *cx, jsval *argv, PRUint32 argc);  \
  NS_IMETHOD    GetElementById(const nsString& aElementId, nsIDOMElement** aReturn);  \
  NS_IMETHOD    GetElementsByName(const nsString& aElementName, nsIDOMNodeList** aReturn);  \



#define NS_FORWARD_IDOMHTMLDOCUMENT(_to)  \
  NS_IMETHOD    GetTitle(nsString& aTitle) { return _to GetTitle(aTitle); } \
  NS_IMETHOD    SetTitle(const nsString& aTitle) { return _to SetTitle(aTitle); } \
  NS_IMETHOD    GetReferrer(nsString& aReferrer) { return _to GetReferrer(aReferrer); } \
  NS_IMETHOD    GetDomain(nsString& aDomain) { return _to GetDomain(aDomain); } \
  NS_IMETHOD    GetURL(nsString& aURL) { return _to GetURL(aURL); } \
  NS_IMETHOD    GetBody(nsIDOMHTMLElement** aBody) { return _to GetBody(aBody); } \
  NS_IMETHOD    SetBody(nsIDOMHTMLElement* aBody) { return _to SetBody(aBody); } \
  NS_IMETHOD    GetImages(nsIDOMHTMLCollection** aImages) { return _to GetImages(aImages); } \
  NS_IMETHOD    GetApplets(nsIDOMHTMLCollection** aApplets) { return _to GetApplets(aApplets); } \
  NS_IMETHOD    GetLinks(nsIDOMHTMLCollection** aLinks) { return _to GetLinks(aLinks); } \
  NS_IMETHOD    GetForms(nsIDOMHTMLCollection** aForms) { return _to GetForms(aForms); } \
  NS_IMETHOD    GetAnchors(nsIDOMHTMLCollection** aAnchors) { return _to GetAnchors(aAnchors); } \
  NS_IMETHOD    GetCookie(nsString& aCookie) { return _to GetCookie(aCookie); } \
  NS_IMETHOD    SetCookie(const nsString& aCookie) { return _to SetCookie(aCookie); } \
  NS_IMETHOD    Open(JSContext *cx, jsval *argv, PRUint32 argc) { return _to Open(cx, argv, argc); }  \
  NS_IMETHOD    Close() { return _to Close(); }  \
  NS_IMETHOD    Write(JSContext *cx, jsval *argv, PRUint32 argc) { return _to Write(cx, argv, argc); }  \
  NS_IMETHOD    Writeln(JSContext *cx, jsval *argv, PRUint32 argc) { return _to Writeln(cx, argv, argc); }  \
  NS_IMETHOD    GetElementById(const nsString& aElementId, nsIDOMElement** aReturn) { return _to GetElementById(aElementId, aReturn); }  \
  NS_IMETHOD    GetElementsByName(const nsString& aElementName, nsIDOMNodeList** aReturn) { return _to GetElementsByName(aElementName, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitHTMLDocumentClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLDocument(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLDocument_h__

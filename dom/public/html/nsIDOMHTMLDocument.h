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

#ifndef nsIDOMHTMLDocument_h__
#define nsIDOMHTMLDocument_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMDocument.h"

class nsIDOMHTMLElement;
class nsIDOMHTMLCollection;
class nsIDOMNodeList;

#define NS_IDOMHTMLDOCUMENT_IID \
 { 0xa6cf9084, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLDocument : public nsIDOMDocument {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLDOCUMENT_IID; return iid; }

  NS_IMETHOD    GetTitle(nsAWritableString& aTitle)=0;
  NS_IMETHOD    SetTitle(const nsAReadableString& aTitle)=0;

  NS_IMETHOD    GetReferrer(nsAWritableString& aReferrer)=0;

  NS_IMETHOD    GetDomain(nsAWritableString& aDomain)=0;
  NS_IMETHOD    SetDomain(const nsAReadableString& aDomain)=0;

  NS_IMETHOD    GetURL(nsAWritableString& aURL)=0;

  NS_IMETHOD    GetBody(nsIDOMHTMLElement** aBody)=0;
  NS_IMETHOD    SetBody(nsIDOMHTMLElement* aBody)=0;

  NS_IMETHOD    GetImages(nsIDOMHTMLCollection** aImages)=0;

  NS_IMETHOD    GetApplets(nsIDOMHTMLCollection** aApplets)=0;

  NS_IMETHOD    GetLinks(nsIDOMHTMLCollection** aLinks)=0;

  NS_IMETHOD    GetForms(nsIDOMHTMLCollection** aForms)=0;

  NS_IMETHOD    GetAnchors(nsIDOMHTMLCollection** aAnchors)=0;

  NS_IMETHOD    GetCookie(nsAWritableString& aCookie)=0;
  NS_IMETHOD    SetCookie(const nsAReadableString& aCookie)=0;

  NS_IMETHOD    Open()=0;

  NS_IMETHOD    Close()=0;

  NS_IMETHOD    Write(const nsAReadableString& aText)=0;

  NS_IMETHOD    Writeln(const nsAReadableString& aText)=0;

  NS_IMETHOD    GetElementsByName(const nsAReadableString& aElementName, nsIDOMNodeList** aReturn)=0;
};


#define NS_DECL_IDOMHTMLDOCUMENT   \
  NS_IMETHOD    GetTitle(nsAWritableString& aTitle);  \
  NS_IMETHOD    SetTitle(const nsAReadableString& aTitle);  \
  NS_IMETHOD    GetReferrer(nsAWritableString& aReferrer);  \
  NS_IMETHOD    GetDomain(nsAWritableString& aDomain);  \
  NS_IMETHOD    SetDomain(const nsAReadableString& aDomain);  \
  NS_IMETHOD    GetURL(nsAWritableString& aURL);  \
  NS_IMETHOD    GetBody(nsIDOMHTMLElement** aBody);  \
  NS_IMETHOD    SetBody(nsIDOMHTMLElement* aBody);  \
  NS_IMETHOD    GetImages(nsIDOMHTMLCollection** aImages);  \
  NS_IMETHOD    GetApplets(nsIDOMHTMLCollection** aApplets);  \
  NS_IMETHOD    GetLinks(nsIDOMHTMLCollection** aLinks);  \
  NS_IMETHOD    GetForms(nsIDOMHTMLCollection** aForms);  \
  NS_IMETHOD    GetAnchors(nsIDOMHTMLCollection** aAnchors);  \
  NS_IMETHOD    GetCookie(nsAWritableString& aCookie);  \
  NS_IMETHOD    SetCookie(const nsAReadableString& aCookie);  \
  NS_IMETHOD    Open();  \
  NS_IMETHOD    Close();  \
  NS_IMETHOD    Write(const nsAReadableString& aText);  \
  NS_IMETHOD    Writeln(const nsAReadableString& aText);  \
  NS_IMETHOD    GetElementsByName(const nsAReadableString& aElementName, nsIDOMNodeList** aReturn);  \



#define NS_FORWARD_IDOMHTMLDOCUMENT(_to)  \
  NS_IMETHOD    GetTitle(nsAWritableString& aTitle) { return _to GetTitle(aTitle); } \
  NS_IMETHOD    SetTitle(const nsAReadableString& aTitle) { return _to SetTitle(aTitle); } \
  NS_IMETHOD    GetReferrer(nsAWritableString& aReferrer) { return _to GetReferrer(aReferrer); } \
  NS_IMETHOD    GetDomain(nsAWritableString& aDomain) { return _to GetDomain(aDomain); } \
  NS_IMETHOD    SetDomain(const nsAReadableString& aDomain) { return _to SetDomain(aDomain); } \
  NS_IMETHOD    GetURL(nsAWritableString& aURL) { return _to GetURL(aURL); } \
  NS_IMETHOD    GetBody(nsIDOMHTMLElement** aBody) { return _to GetBody(aBody); } \
  NS_IMETHOD    SetBody(nsIDOMHTMLElement* aBody) { return _to SetBody(aBody); } \
  NS_IMETHOD    GetImages(nsIDOMHTMLCollection** aImages) { return _to GetImages(aImages); } \
  NS_IMETHOD    GetApplets(nsIDOMHTMLCollection** aApplets) { return _to GetApplets(aApplets); } \
  NS_IMETHOD    GetLinks(nsIDOMHTMLCollection** aLinks) { return _to GetLinks(aLinks); } \
  NS_IMETHOD    GetForms(nsIDOMHTMLCollection** aForms) { return _to GetForms(aForms); } \
  NS_IMETHOD    GetAnchors(nsIDOMHTMLCollection** aAnchors) { return _to GetAnchors(aAnchors); } \
  NS_IMETHOD    GetCookie(nsAWritableString& aCookie) { return _to GetCookie(aCookie); } \
  NS_IMETHOD    SetCookie(const nsAReadableString& aCookie) { return _to SetCookie(aCookie); } \
  NS_IMETHOD    Open() { return _to Open(); }  \
  NS_IMETHOD    Close() { return _to Close(); }  \
  NS_IMETHOD    Write(const nsAReadableString& aText) { return _to Write(aText); }  \
  NS_IMETHOD    Writeln(const nsAReadableString& aText) { return _to Writeln(aText); }  \
  NS_IMETHOD    GetElementsByName(const nsAReadableString& aElementName, nsIDOMNodeList** aReturn) { return _to GetElementsByName(aElementName, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitHTMLDocumentClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLDocument(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLDocument_h__

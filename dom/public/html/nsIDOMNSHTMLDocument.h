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

#ifndef nsIDOMNSHTMLDocument_h__
#define nsIDOMNSHTMLDocument_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "jsapi.h"

class nsIDOMEvent;
class nsIDOMHTMLCollection;

#define NS_IDOMNSHTMLDOCUMENT_IID \
 { 0xa6cf90c5, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMNSHTMLDocument : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMNSHTMLDOCUMENT_IID; return iid; }

  NS_IMETHOD    GetWidth(PRInt32* aWidth)=0;

  NS_IMETHOD    GetHeight(PRInt32* aHeight)=0;

  NS_IMETHOD    GetAlinkColor(nsString& aAlinkColor)=0;
  NS_IMETHOD    SetAlinkColor(const nsString& aAlinkColor)=0;

  NS_IMETHOD    GetLinkColor(nsString& aLinkColor)=0;
  NS_IMETHOD    SetLinkColor(const nsString& aLinkColor)=0;

  NS_IMETHOD    GetVlinkColor(nsString& aVlinkColor)=0;
  NS_IMETHOD    SetVlinkColor(const nsString& aVlinkColor)=0;

  NS_IMETHOD    GetBgColor(nsString& aBgColor)=0;
  NS_IMETHOD    SetBgColor(const nsString& aBgColor)=0;

  NS_IMETHOD    GetFgColor(nsString& aFgColor)=0;
  NS_IMETHOD    SetFgColor(const nsString& aFgColor)=0;

  NS_IMETHOD    GetLastModified(nsString& aLastModified)=0;

  NS_IMETHOD    GetEmbeds(nsIDOMHTMLCollection** aEmbeds)=0;

  NS_IMETHOD    GetSelection(nsString& aReturn)=0;

  NS_IMETHOD    NamedItem(JSContext* cx, jsval* argv, PRUint32 argc, jsval* aReturn)=0;

  NS_IMETHOD    Open(JSContext* cx, jsval* argv, PRUint32 argc)=0;

  NS_IMETHOD    Write(JSContext* cx, jsval* argv, PRUint32 argc)=0;

  NS_IMETHOD    Writeln(JSContext* cx, jsval* argv, PRUint32 argc)=0;

  NS_IMETHOD    Clear(JSContext* cx, jsval* argv, PRUint32 argc)=0;

  NS_IMETHOD    CaptureEvents(PRInt32 aEventFlags)=0;

  NS_IMETHOD    ReleaseEvents(PRInt32 aEventFlags)=0;

  NS_IMETHOD    RouteEvent(nsIDOMEvent* aEvt)=0;
};


#define NS_DECL_IDOMNSHTMLDOCUMENT   \
  NS_IMETHOD    GetWidth(PRInt32* aWidth);  \
  NS_IMETHOD    GetHeight(PRInt32* aHeight);  \
  NS_IMETHOD    GetAlinkColor(nsString& aAlinkColor);  \
  NS_IMETHOD    SetAlinkColor(const nsString& aAlinkColor);  \
  NS_IMETHOD    GetLinkColor(nsString& aLinkColor);  \
  NS_IMETHOD    SetLinkColor(const nsString& aLinkColor);  \
  NS_IMETHOD    GetVlinkColor(nsString& aVlinkColor);  \
  NS_IMETHOD    SetVlinkColor(const nsString& aVlinkColor);  \
  NS_IMETHOD    GetBgColor(nsString& aBgColor);  \
  NS_IMETHOD    SetBgColor(const nsString& aBgColor);  \
  NS_IMETHOD    GetFgColor(nsString& aFgColor);  \
  NS_IMETHOD    SetFgColor(const nsString& aFgColor);  \
  NS_IMETHOD    GetLastModified(nsString& aLastModified);  \
  NS_IMETHOD    GetEmbeds(nsIDOMHTMLCollection** aEmbeds);  \
  NS_IMETHOD    GetSelection(nsString& aReturn);  \
  NS_IMETHOD    NamedItem(JSContext* cx, jsval* argv, PRUint32 argc, jsval* aReturn);  \
  NS_IMETHOD    Open(JSContext* cx, jsval* argv, PRUint32 argc);  \
  NS_IMETHOD    Write(JSContext* cx, jsval* argv, PRUint32 argc);  \
  NS_IMETHOD    Writeln(JSContext* cx, jsval* argv, PRUint32 argc);  \
  NS_IMETHOD    Clear(JSContext* cx, jsval* argv, PRUint32 argc);  \
  NS_IMETHOD    CaptureEvents(PRInt32 aEventFlags);  \
  NS_IMETHOD    ReleaseEvents(PRInt32 aEventFlags);  \
  NS_IMETHOD    RouteEvent(nsIDOMEvent* aEvt);  \



#define NS_FORWARD_IDOMNSHTMLDOCUMENT(_to)  \
  NS_IMETHOD    GetWidth(PRInt32* aWidth) { return _to GetWidth(aWidth); } \
  NS_IMETHOD    GetHeight(PRInt32* aHeight) { return _to GetHeight(aHeight); } \
  NS_IMETHOD    GetAlinkColor(nsString& aAlinkColor) { return _to GetAlinkColor(aAlinkColor); } \
  NS_IMETHOD    SetAlinkColor(const nsString& aAlinkColor) { return _to SetAlinkColor(aAlinkColor); } \
  NS_IMETHOD    GetLinkColor(nsString& aLinkColor) { return _to GetLinkColor(aLinkColor); } \
  NS_IMETHOD    SetLinkColor(const nsString& aLinkColor) { return _to SetLinkColor(aLinkColor); } \
  NS_IMETHOD    GetVlinkColor(nsString& aVlinkColor) { return _to GetVlinkColor(aVlinkColor); } \
  NS_IMETHOD    SetVlinkColor(const nsString& aVlinkColor) { return _to SetVlinkColor(aVlinkColor); } \
  NS_IMETHOD    GetBgColor(nsString& aBgColor) { return _to GetBgColor(aBgColor); } \
  NS_IMETHOD    SetBgColor(const nsString& aBgColor) { return _to SetBgColor(aBgColor); } \
  NS_IMETHOD    GetFgColor(nsString& aFgColor) { return _to GetFgColor(aFgColor); } \
  NS_IMETHOD    SetFgColor(const nsString& aFgColor) { return _to SetFgColor(aFgColor); } \
  NS_IMETHOD    GetLastModified(nsString& aLastModified) { return _to GetLastModified(aLastModified); } \
  NS_IMETHOD    GetEmbeds(nsIDOMHTMLCollection** aEmbeds) { return _to GetEmbeds(aEmbeds); } \
  NS_IMETHOD    GetSelection(nsString& aReturn) { return _to GetSelection(aReturn); }  \
  NS_IMETHOD    NamedItem(JSContext* cx, jsval* argv, PRUint32 argc, jsval* aReturn) { return _to NamedItem(cx, argv, argc, aReturn); }  \
  NS_IMETHOD    Open(JSContext* cx, jsval* argv, PRUint32 argc) { return _to Open(cx, argv, argc); }  \
  NS_IMETHOD    Write(JSContext* cx, jsval* argv, PRUint32 argc) { return _to Write(cx, argv, argc); }  \
  NS_IMETHOD    Writeln(JSContext* cx, jsval* argv, PRUint32 argc) { return _to Writeln(cx, argv, argc); }  \
  NS_IMETHOD    Clear(JSContext* cx, jsval* argv, PRUint32 argc) { return _to Clear(cx, argv, argc); }  \
  NS_IMETHOD    CaptureEvents(PRInt32 aEventFlags) { return _to CaptureEvents(aEventFlags); }  \
  NS_IMETHOD    ReleaseEvents(PRInt32 aEventFlags) { return _to ReleaseEvents(aEventFlags); }  \
  NS_IMETHOD    RouteEvent(nsIDOMEvent* aEvt) { return _to RouteEvent(aEvt); }  \


#endif // nsIDOMNSHTMLDocument_h__

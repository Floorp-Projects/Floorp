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

#ifndef nsIDOMNSHTMLDocument_h__
#define nsIDOMNSHTMLDocument_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMElement;
class nsIDOMHTMLCollection;

#define NS_IDOMNSHTMLDOCUMENT_IID \
 { 0xa6cf90c5, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMNSHTMLDocument : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMNSHTMLDOCUMENT_IID; return iid; }

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

  NS_IMETHOD    GetLayers(nsIDOMHTMLCollection** aLayers)=0;

  NS_IMETHOD    GetPlugins(nsIDOMHTMLCollection** aPlugins)=0;

  NS_IMETHOD    GetSelection(nsString& aReturn)=0;

  NS_IMETHOD    NamedItem(const nsString& aName, nsIDOMElement** aReturn)=0;
};


#define NS_DECL_IDOMNSHTMLDOCUMENT   \
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
  NS_IMETHOD    GetLayers(nsIDOMHTMLCollection** aLayers);  \
  NS_IMETHOD    GetPlugins(nsIDOMHTMLCollection** aPlugins);  \
  NS_IMETHOD    GetSelection(nsString& aReturn);  \
  NS_IMETHOD    NamedItem(const nsString& aName, nsIDOMElement** aReturn);  \



#define NS_FORWARD_IDOMNSHTMLDOCUMENT(_to)  \
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
  NS_IMETHOD    GetLayers(nsIDOMHTMLCollection** aLayers) { return _to GetLayers(aLayers); } \
  NS_IMETHOD    GetPlugins(nsIDOMHTMLCollection** aPlugins) { return _to GetPlugins(aPlugins); } \
  NS_IMETHOD    GetSelection(nsString& aReturn) { return _to GetSelection(aReturn); }  \
  NS_IMETHOD    NamedItem(const nsString& aName, nsIDOMElement** aReturn) { return _to NamedItem(aName, aReturn); }  \


#endif // nsIDOMNSHTMLDocument_h__

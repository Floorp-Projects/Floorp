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

#ifndef nsIDOMDocument_h__
#define nsIDOMDocument_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMDocumentFragment.h"

class nsIDOMElement;
class nsIDOMProcessingInstruction;
class nsIDOMNamedNodeMap;
class nsIDOMAttribute;
class nsIDOMNode;
class nsIDOMText;
class nsIDOMDocumentType;
class nsIDOMDocumentFragment;
class nsIDOMComment;
class nsIDOMNodeList;

#define NS_IDOMDOCUMENT_IID \
{ 0x6f7652e4,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMDocument : public nsIDOMDocumentFragment {
public:

  NS_IMETHOD    GetDocumentType(nsIDOMDocumentType** aDocumentType)=0;

  NS_IMETHOD    GetProlog(nsIDOMNodeList** aProlog)=0;

  NS_IMETHOD    GetEpilog(nsIDOMNodeList** aEpilog)=0;

  NS_IMETHOD    GetDocumentElement(nsIDOMElement** aDocumentElement)=0;

  NS_IMETHOD    CreateElement(const nsString& aTagName, nsIDOMNamedNodeMap* aAttributes, nsIDOMElement** aReturn)=0;

  NS_IMETHOD    CreateDocumentFragment(nsIDOMDocumentFragment** aReturn)=0;

  NS_IMETHOD    CreateTextNode(const nsString& aData, nsIDOMText** aReturn)=0;

  NS_IMETHOD    CreateComment(const nsString& aData, nsIDOMComment** aReturn)=0;

  NS_IMETHOD    CreateProcessingInstruction(const nsString& aTarget, const nsString& aData, nsIDOMProcessingInstruction** aReturn)=0;

  NS_IMETHOD    CreateAttribute(const nsString& aName, nsIDOMNode* aValue, nsIDOMAttribute** aReturn)=0;

  NS_IMETHOD    GetElementsByTagName(const nsString& aTagname, nsIDOMNodeList** aReturn)=0;
};


#define NS_DECL_IDOMDOCUMENT   \
  NS_IMETHOD    GetDocumentType(nsIDOMDocumentType** aDocumentType);  \
  NS_IMETHOD    GetProlog(nsIDOMNodeList** aProlog);  \
  NS_IMETHOD    GetEpilog(nsIDOMNodeList** aEpilog);  \
  NS_IMETHOD    GetDocumentElement(nsIDOMElement** aDocumentElement);  \
  NS_IMETHOD    CreateElement(const nsString& aTagName, nsIDOMNamedNodeMap* aAttributes, nsIDOMElement** aReturn);  \
  NS_IMETHOD    CreateDocumentFragment(nsIDOMDocumentFragment** aReturn);  \
  NS_IMETHOD    CreateTextNode(const nsString& aData, nsIDOMText** aReturn);  \
  NS_IMETHOD    CreateComment(const nsString& aData, nsIDOMComment** aReturn);  \
  NS_IMETHOD    CreateProcessingInstruction(const nsString& aTarget, const nsString& aData, nsIDOMProcessingInstruction** aReturn);  \
  NS_IMETHOD    CreateAttribute(const nsString& aName, nsIDOMNode* aValue, nsIDOMAttribute** aReturn);  \
  NS_IMETHOD    GetElementsByTagName(const nsString& aTagname, nsIDOMNodeList** aReturn);  \



#define NS_FORWARD_IDOMDOCUMENT(_to)  \
  NS_IMETHOD    GetDocumentType(nsIDOMDocumentType** aDocumentType) { return _to##GetDocumentType(aDocumentType); } \
  NS_IMETHOD    GetProlog(nsIDOMNodeList** aProlog) { return _to##GetProlog(aProlog); } \
  NS_IMETHOD    GetEpilog(nsIDOMNodeList** aEpilog) { return _to##GetEpilog(aEpilog); } \
  NS_IMETHOD    GetDocumentElement(nsIDOMElement** aDocumentElement) { return _to##GetDocumentElement(aDocumentElement); } \
  NS_IMETHOD    CreateElement(const nsString& aTagName, nsIDOMNamedNodeMap* aAttributes, nsIDOMElement** aReturn) { return _to##CreateElement(aTagName, aAttributes, aReturn); }  \
  NS_IMETHOD    CreateDocumentFragment(nsIDOMDocumentFragment** aReturn) { return _to##CreateDocumentFragment(aReturn); }  \
  NS_IMETHOD    CreateTextNode(const nsString& aData, nsIDOMText** aReturn) { return _to##CreateTextNode(aData, aReturn); }  \
  NS_IMETHOD    CreateComment(const nsString& aData, nsIDOMComment** aReturn) { return _to##CreateComment(aData, aReturn); }  \
  NS_IMETHOD    CreateProcessingInstruction(const nsString& aTarget, const nsString& aData, nsIDOMProcessingInstruction** aReturn) { return _to##CreateProcessingInstruction(aTarget, aData, aReturn); }  \
  NS_IMETHOD    CreateAttribute(const nsString& aName, nsIDOMNode* aValue, nsIDOMAttribute** aReturn) { return _to##CreateAttribute(aName, aValue, aReturn); }  \
  NS_IMETHOD    GetElementsByTagName(const nsString& aTagname, nsIDOMNodeList** aReturn) { return _to##GetElementsByTagName(aTagname, aReturn); }  \


extern nsresult NS_InitDocumentClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptDocument(nsIScriptContext *aContext, nsIDOMDocument *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMDocument_h__

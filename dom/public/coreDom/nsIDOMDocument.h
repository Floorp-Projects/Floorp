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
class nsIDOMDocument;
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

extern nsresult NS_InitDocumentClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptDocument(nsIScriptContext *aContext, nsIDOMDocument *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMDocument_h__

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

class nsIDOMAttributeList;
class nsIDOMElement;
class nsIDOMPI;
class nsIDOMNodeIterator;
class nsIDOMDocument;
class nsIDOMTreeIterator;
class nsIDOMAttribute;
class nsIDOMNode;
class nsIDOMText;
class nsIDOMDocumentContext;
class nsIDOMComment;

#define NS_IDOMDOCUMENT_IID \
{ 0x6f7652e3,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMDocument : public nsIDOMDocumentFragment {
public:

  NS_IMETHOD    GetDocumentType(nsIDOMNode** aDocumentType)=0;
  NS_IMETHOD    SetDocumentType(nsIDOMNode* aDocumentType)=0;

  NS_IMETHOD    GetDocumentElement(nsIDOMElement** aDocumentElement)=0;
  NS_IMETHOD    SetDocumentElement(nsIDOMElement* aDocumentElement)=0;

  NS_IMETHOD    GetDocumentContext(nsIDOMDocumentContext** aDocumentContext)=0;
  NS_IMETHOD    SetDocumentContext(nsIDOMDocumentContext* aDocumentContext)=0;

  NS_IMETHOD    CreateDocumentContext(nsIDOMDocumentContext** aReturn)=0;

  NS_IMETHOD    CreateElement(nsString& aTagName, nsIDOMAttributeList* aAttributes, nsIDOMElement** aReturn)=0;

  NS_IMETHOD    CreateTextNode(nsString& aData, nsIDOMText** aReturn)=0;

  NS_IMETHOD    CreateComment(nsString& aData, nsIDOMComment** aReturn)=0;

  NS_IMETHOD    CreatePI(nsString& aName, nsString& aData, nsIDOMPI** aReturn)=0;

  NS_IMETHOD    CreateAttribute(nsString& aName, nsIDOMNode* aValue, nsIDOMAttribute** aReturn)=0;

  NS_IMETHOD    CreateAttributeList(nsIDOMAttributeList** aReturn)=0;

  NS_IMETHOD    CreateTreeIterator(nsIDOMNode* aNode, nsIDOMTreeIterator** aReturn)=0;

  NS_IMETHOD    GetElementsByTagName(nsString& aTagname, nsIDOMNodeIterator** aReturn)=0;
};

extern nsresult NS_InitDocumentClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM NS_NewScriptDocument(nsIScriptContext *aContext, nsIDOMDocument *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMDocument_h__

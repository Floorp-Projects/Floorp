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
#include "nsIDOMNode.h"

class nsIDOMElement;
class nsIDOMProcessingInstruction;
class nsIDOMAttr;
class nsIDOMCDATASection;
class nsIDOMText;
class nsIDOMDOMImplementation;
class nsIDOMDocumentType;
class nsIDOMEntityReference;
class nsIDOMDocumentFragment;
class nsIDOMComment;
class nsIDOMNodeList;

#define NS_IDOMDOCUMENT_IID \
 { 0xa6cf9075, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMDocument : public nsIDOMNode {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMDOCUMENT_IID; return iid; }

  NS_IMETHOD    GetDoctype(nsIDOMDocumentType** aDoctype)=0;

  NS_IMETHOD    GetImplementation(nsIDOMDOMImplementation** aImplementation)=0;

  NS_IMETHOD    GetDocumentElement(nsIDOMElement** aDocumentElement)=0;

  NS_IMETHOD    CreateElement(const nsString& aTagName, nsIDOMElement** aReturn)=0;

  NS_IMETHOD    CreateDocumentFragment(nsIDOMDocumentFragment** aReturn)=0;

  NS_IMETHOD    CreateTextNode(const nsString& aData, nsIDOMText** aReturn)=0;

  NS_IMETHOD    CreateComment(const nsString& aData, nsIDOMComment** aReturn)=0;

  NS_IMETHOD    CreateCDATASection(const nsString& aData, nsIDOMCDATASection** aReturn)=0;

  NS_IMETHOD    CreateProcessingInstruction(const nsString& aTarget, const nsString& aData, nsIDOMProcessingInstruction** aReturn)=0;

  NS_IMETHOD    CreateAttribute(const nsString& aName, nsIDOMAttr** aReturn)=0;

  NS_IMETHOD    CreateEntityReference(const nsString& aName, nsIDOMEntityReference** aReturn)=0;

  NS_IMETHOD    GetElementsByTagName(const nsString& aTagname, nsIDOMNodeList** aReturn)=0;
};


#define NS_DECL_IDOMDOCUMENT   \
  NS_IMETHOD    GetDoctype(nsIDOMDocumentType** aDoctype);  \
  NS_IMETHOD    GetImplementation(nsIDOMDOMImplementation** aImplementation);  \
  NS_IMETHOD    GetDocumentElement(nsIDOMElement** aDocumentElement);  \
  NS_IMETHOD    CreateElement(const nsString& aTagName, nsIDOMElement** aReturn);  \
  NS_IMETHOD    CreateDocumentFragment(nsIDOMDocumentFragment** aReturn);  \
  NS_IMETHOD    CreateTextNode(const nsString& aData, nsIDOMText** aReturn);  \
  NS_IMETHOD    CreateComment(const nsString& aData, nsIDOMComment** aReturn);  \
  NS_IMETHOD    CreateCDATASection(const nsString& aData, nsIDOMCDATASection** aReturn);  \
  NS_IMETHOD    CreateProcessingInstruction(const nsString& aTarget, const nsString& aData, nsIDOMProcessingInstruction** aReturn);  \
  NS_IMETHOD    CreateAttribute(const nsString& aName, nsIDOMAttr** aReturn);  \
  NS_IMETHOD    CreateEntityReference(const nsString& aName, nsIDOMEntityReference** aReturn);  \
  NS_IMETHOD    GetElementsByTagName(const nsString& aTagname, nsIDOMNodeList** aReturn);  \



#define NS_FORWARD_IDOMDOCUMENT(_to)  \
  NS_IMETHOD    GetDoctype(nsIDOMDocumentType** aDoctype) { return _to GetDoctype(aDoctype); } \
  NS_IMETHOD    GetImplementation(nsIDOMDOMImplementation** aImplementation) { return _to GetImplementation(aImplementation); } \
  NS_IMETHOD    GetDocumentElement(nsIDOMElement** aDocumentElement) { return _to GetDocumentElement(aDocumentElement); } \
  NS_IMETHOD    CreateElement(const nsString& aTagName, nsIDOMElement** aReturn) { return _to CreateElement(aTagName, aReturn); }  \
  NS_IMETHOD    CreateDocumentFragment(nsIDOMDocumentFragment** aReturn) { return _to CreateDocumentFragment(aReturn); }  \
  NS_IMETHOD    CreateTextNode(const nsString& aData, nsIDOMText** aReturn) { return _to CreateTextNode(aData, aReturn); }  \
  NS_IMETHOD    CreateComment(const nsString& aData, nsIDOMComment** aReturn) { return _to CreateComment(aData, aReturn); }  \
  NS_IMETHOD    CreateCDATASection(const nsString& aData, nsIDOMCDATASection** aReturn) { return _to CreateCDATASection(aData, aReturn); }  \
  NS_IMETHOD    CreateProcessingInstruction(const nsString& aTarget, const nsString& aData, nsIDOMProcessingInstruction** aReturn) { return _to CreateProcessingInstruction(aTarget, aData, aReturn); }  \
  NS_IMETHOD    CreateAttribute(const nsString& aName, nsIDOMAttr** aReturn) { return _to CreateAttribute(aName, aReturn); }  \
  NS_IMETHOD    CreateEntityReference(const nsString& aName, nsIDOMEntityReference** aReturn) { return _to CreateEntityReference(aName, aReturn); }  \
  NS_IMETHOD    GetElementsByTagName(const nsString& aTagname, nsIDOMNodeList** aReturn) { return _to GetElementsByTagName(aTagname, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitDocumentClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptDocument(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMDocument_h__

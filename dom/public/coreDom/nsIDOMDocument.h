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

#ifndef nsIDOMDocument_h__
#define nsIDOMDocument_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMNode.h"

class nsIDOMElement;
class nsIDOMAttr;
class nsIDOMProcessingInstruction;
class nsIDOMNode;
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

  NS_IMETHOD    CreateElement(const nsAReadableString& aTagName, nsIDOMElement** aReturn)=0;

  NS_IMETHOD    CreateDocumentFragment(nsIDOMDocumentFragment** aReturn)=0;

  NS_IMETHOD    CreateTextNode(const nsAReadableString& aData, nsIDOMText** aReturn)=0;

  NS_IMETHOD    CreateComment(const nsAReadableString& aData, nsIDOMComment** aReturn)=0;

  NS_IMETHOD    CreateCDATASection(const nsAReadableString& aData, nsIDOMCDATASection** aReturn)=0;

  NS_IMETHOD    CreateProcessingInstruction(const nsAReadableString& aTarget, const nsAReadableString& aData, nsIDOMProcessingInstruction** aReturn)=0;

  NS_IMETHOD    CreateAttribute(const nsAReadableString& aName, nsIDOMAttr** aReturn)=0;

  NS_IMETHOD    CreateEntityReference(const nsAReadableString& aName, nsIDOMEntityReference** aReturn)=0;

  NS_IMETHOD    GetElementsByTagName(const nsAReadableString& aTagname, nsIDOMNodeList** aReturn)=0;

  NS_IMETHOD    ImportNode(nsIDOMNode* aImportedNode, PRBool aDeep, nsIDOMNode** aReturn)=0;

  NS_IMETHOD    CreateElementNS(const nsAReadableString& aNamespaceURI, const nsAReadableString& aQualifiedName, nsIDOMElement** aReturn)=0;

  NS_IMETHOD    CreateAttributeNS(const nsAReadableString& aNamespaceURI, const nsAReadableString& aQualifiedName, nsIDOMAttr** aReturn)=0;

  NS_IMETHOD    GetElementsByTagNameNS(const nsAReadableString& aNamespaceURI, const nsAReadableString& aLocalName, nsIDOMNodeList** aReturn)=0;

  NS_IMETHOD    GetElementById(const nsAReadableString& aElementId, nsIDOMElement** aReturn)=0;
};


#define NS_DECL_IDOMDOCUMENT   \
  NS_IMETHOD    GetDoctype(nsIDOMDocumentType** aDoctype);  \
  NS_IMETHOD    GetImplementation(nsIDOMDOMImplementation** aImplementation);  \
  NS_IMETHOD    GetDocumentElement(nsIDOMElement** aDocumentElement);  \
  NS_IMETHOD    CreateElement(const nsAReadableString& aTagName, nsIDOMElement** aReturn);  \
  NS_IMETHOD    CreateDocumentFragment(nsIDOMDocumentFragment** aReturn);  \
  NS_IMETHOD    CreateTextNode(const nsAReadableString& aData, nsIDOMText** aReturn);  \
  NS_IMETHOD    CreateComment(const nsAReadableString& aData, nsIDOMComment** aReturn);  \
  NS_IMETHOD    CreateCDATASection(const nsAReadableString& aData, nsIDOMCDATASection** aReturn);  \
  NS_IMETHOD    CreateProcessingInstruction(const nsAReadableString& aTarget, const nsAReadableString& aData, nsIDOMProcessingInstruction** aReturn);  \
  NS_IMETHOD    CreateAttribute(const nsAReadableString& aName, nsIDOMAttr** aReturn);  \
  NS_IMETHOD    CreateEntityReference(const nsAReadableString& aName, nsIDOMEntityReference** aReturn);  \
  NS_IMETHOD    GetElementsByTagName(const nsAReadableString& aTagname, nsIDOMNodeList** aReturn);  \
  NS_IMETHOD    ImportNode(nsIDOMNode* aImportedNode, PRBool aDeep, nsIDOMNode** aReturn);  \
  NS_IMETHOD    CreateElementNS(const nsAReadableString& aNamespaceURI, const nsAReadableString& aQualifiedName, nsIDOMElement** aReturn);  \
  NS_IMETHOD    CreateAttributeNS(const nsAReadableString& aNamespaceURI, const nsAReadableString& aQualifiedName, nsIDOMAttr** aReturn);  \
  NS_IMETHOD    GetElementsByTagNameNS(const nsAReadableString& aNamespaceURI, const nsAReadableString& aLocalName, nsIDOMNodeList** aReturn);  \
  NS_IMETHOD    GetElementById(const nsAReadableString& aElementId, nsIDOMElement** aReturn);  \



#define NS_FORWARD_IDOMDOCUMENT(_to)  \
  NS_IMETHOD    GetDoctype(nsIDOMDocumentType** aDoctype) { return _to GetDoctype(aDoctype); } \
  NS_IMETHOD    GetImplementation(nsIDOMDOMImplementation** aImplementation) { return _to GetImplementation(aImplementation); } \
  NS_IMETHOD    GetDocumentElement(nsIDOMElement** aDocumentElement) { return _to GetDocumentElement(aDocumentElement); } \
  NS_IMETHOD    CreateElement(const nsAReadableString& aTagName, nsIDOMElement** aReturn) { return _to CreateElement(aTagName, aReturn); }  \
  NS_IMETHOD    CreateDocumentFragment(nsIDOMDocumentFragment** aReturn) { return _to CreateDocumentFragment(aReturn); }  \
  NS_IMETHOD    CreateTextNode(const nsAReadableString& aData, nsIDOMText** aReturn) { return _to CreateTextNode(aData, aReturn); }  \
  NS_IMETHOD    CreateComment(const nsAReadableString& aData, nsIDOMComment** aReturn) { return _to CreateComment(aData, aReturn); }  \
  NS_IMETHOD    CreateCDATASection(const nsAReadableString& aData, nsIDOMCDATASection** aReturn) { return _to CreateCDATASection(aData, aReturn); }  \
  NS_IMETHOD    CreateProcessingInstruction(const nsAReadableString& aTarget, const nsAReadableString& aData, nsIDOMProcessingInstruction** aReturn) { return _to CreateProcessingInstruction(aTarget, aData, aReturn); }  \
  NS_IMETHOD    CreateAttribute(const nsAReadableString& aName, nsIDOMAttr** aReturn) { return _to CreateAttribute(aName, aReturn); }  \
  NS_IMETHOD    CreateEntityReference(const nsAReadableString& aName, nsIDOMEntityReference** aReturn) { return _to CreateEntityReference(aName, aReturn); }  \
  NS_IMETHOD    GetElementsByTagName(const nsAReadableString& aTagname, nsIDOMNodeList** aReturn) { return _to GetElementsByTagName(aTagname, aReturn); }  \
  NS_IMETHOD    ImportNode(nsIDOMNode* aImportedNode, PRBool aDeep, nsIDOMNode** aReturn) { return _to ImportNode(aImportedNode, aDeep, aReturn); }  \
  NS_IMETHOD    CreateElementNS(const nsAReadableString& aNamespaceURI, const nsAReadableString& aQualifiedName, nsIDOMElement** aReturn) { return _to CreateElementNS(aNamespaceURI, aQualifiedName, aReturn); }  \
  NS_IMETHOD    CreateAttributeNS(const nsAReadableString& aNamespaceURI, const nsAReadableString& aQualifiedName, nsIDOMAttr** aReturn) { return _to CreateAttributeNS(aNamespaceURI, aQualifiedName, aReturn); }  \
  NS_IMETHOD    GetElementsByTagNameNS(const nsAReadableString& aNamespaceURI, const nsAReadableString& aLocalName, nsIDOMNodeList** aReturn) { return _to GetElementsByTagNameNS(aNamespaceURI, aLocalName, aReturn); }  \
  NS_IMETHOD    GetElementById(const nsAReadableString& aElementId, nsIDOMElement** aReturn) { return _to GetElementById(aElementId, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitDocumentClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptDocument(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMDocument_h__

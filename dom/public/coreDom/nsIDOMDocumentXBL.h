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

#ifndef nsIDOMDocumentXBL_h__
#define nsIDOMDocumentXBL_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMElement;
class nsIDOMDocument;
class nsIDOMNode;
class nsIDOMNodeList;

#define NS_IDOMDOCUMENTXBL_IID \
 { 0xc7c0ae9b, 0xa0ba, 0x4f4e, \
  { 0x9f, 0x2c, 0xc1, 0x8d, 0xeb, 0x62, 0xee, 0x8b } } 

class nsIDOMDocumentXBL : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMDOCUMENTXBL_IID; return iid; }

  NS_IMETHOD    GetAnonymousNodes(nsIDOMElement* aElt, nsIDOMNodeList** aReturn)=0;

  NS_IMETHOD    AddBinding(nsIDOMElement* aElt, const nsAReadableString& aBindingURL)=0;

  NS_IMETHOD    RemoveBinding(nsIDOMElement* aElt, const nsAReadableString& aBindingURL)=0;

  NS_IMETHOD    GetBindingParent(nsIDOMNode* aNode, nsIDOMElement** aReturn)=0;

  NS_IMETHOD    LoadBindingDocument(const nsAReadableString& aDocumentURL, nsIDOMDocument** aReturn)=0;
};


#define NS_DECL_IDOMDOCUMENTXBL   \
  NS_IMETHOD    GetAnonymousNodes(nsIDOMElement* aElt, nsIDOMNodeList** aReturn);  \
  NS_IMETHOD    AddBinding(nsIDOMElement* aElt, const nsAReadableString& aBindingURL);  \
  NS_IMETHOD    RemoveBinding(nsIDOMElement* aElt, const nsAReadableString& aBindingURL);  \
  NS_IMETHOD    GetBindingParent(nsIDOMNode* aNode, nsIDOMElement** aReturn);  \
  NS_IMETHOD    LoadBindingDocument(const nsAReadableString& aDocumentURL, nsIDOMDocument** aReturn);  \



#define NS_FORWARD_IDOMDOCUMENTXBL(_to)  \
  NS_IMETHOD    GetAnonymousNodes(nsIDOMElement* aElt, nsIDOMNodeList** aReturn) { return _to GetAnonymousNodes(aElt, aReturn); }  \
  NS_IMETHOD    AddBinding(nsIDOMElement* aElt, const nsAReadableString& aBindingURL) { return _to AddBinding(aElt, aBindingURL); }  \
  NS_IMETHOD    RemoveBinding(nsIDOMElement* aElt, const nsAReadableString& aBindingURL) { return _to RemoveBinding(aElt, aBindingURL); }  \
  NS_IMETHOD    GetBindingParent(nsIDOMNode* aNode, nsIDOMElement** aReturn) { return _to GetBindingParent(aNode, aReturn); }  \
  NS_IMETHOD    LoadBindingDocument(const nsAReadableString& aDocumentURL, nsIDOMDocument** aReturn) { return _to LoadBindingDocument(aDocumentURL, aReturn); }  \


#endif // nsIDOMDocumentXBL_h__

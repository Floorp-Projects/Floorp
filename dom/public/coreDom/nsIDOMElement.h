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

#ifndef nsIDOMElement_h__
#define nsIDOMElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMNode.h"

class nsIDOMAttr;
class nsIDOMNodeList;

#define NS_IDOMELEMENT_IID \
 { 0xa6cf9078, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMElement : public nsIDOMNode {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMELEMENT_IID; return iid; }

  NS_IMETHOD    GetTagName(nsString& aTagName)=0;

  NS_IMETHOD    GetAttribute(const nsString& aName, nsString& aReturn)=0;

  NS_IMETHOD    SetAttribute(const nsString& aName, const nsString& aValue)=0;

  NS_IMETHOD    RemoveAttribute(const nsString& aName)=0;

  NS_IMETHOD    GetAttributeNode(const nsString& aName, nsIDOMAttr** aReturn)=0;

  NS_IMETHOD    SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn)=0;

  NS_IMETHOD    RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn)=0;

  NS_IMETHOD    GetElementsByTagName(const nsString& aName, nsIDOMNodeList** aReturn)=0;

  NS_IMETHOD    GetAttributeNS(const nsString& aNamespaceURI, const nsString& aLocalName, nsString& aReturn)=0;

  NS_IMETHOD    SetAttributeNS(const nsString& aNamespaceURI, const nsString& aQualifiedName, const nsString& aValue)=0;

  NS_IMETHOD    RemoveAttributeNS(const nsString& aNamespaceURI, const nsString& aLocalName)=0;

  NS_IMETHOD    GetAttributeNodeNS(const nsString& aNamespaceURI, const nsString& aLocalName, nsIDOMAttr** aReturn)=0;

  NS_IMETHOD    SetAttributeNodeNS(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn)=0;

  NS_IMETHOD    GetElementsByTagNameNS(const nsString& aNamespaceURI, const nsString& aLocalName, nsIDOMNodeList** aReturn)=0;

  NS_IMETHOD    HasAttribute(const nsString& aName, PRBool* aReturn)=0;

  NS_IMETHOD    HasAttributeNS(const nsString& aNamespaceURI, const nsString& aLocalName, PRBool* aReturn)=0;
};


#define NS_DECL_IDOMELEMENT   \
  NS_IMETHOD    GetTagName(nsString& aTagName);  \
  NS_IMETHOD    GetAttribute(const nsString& aName, nsString& aReturn);  \
  NS_IMETHOD    SetAttribute(const nsString& aName, const nsString& aValue);  \
  NS_IMETHOD    RemoveAttribute(const nsString& aName);  \
  NS_IMETHOD    GetAttributeNode(const nsString& aName, nsIDOMAttr** aReturn);  \
  NS_IMETHOD    SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn);  \
  NS_IMETHOD    RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn);  \
  NS_IMETHOD    GetElementsByTagName(const nsString& aName, nsIDOMNodeList** aReturn);  \
  NS_IMETHOD    GetAttributeNS(const nsString& aNamespaceURI, const nsString& aLocalName, nsString& aReturn);  \
  NS_IMETHOD    SetAttributeNS(const nsString& aNamespaceURI, const nsString& aQualifiedName, const nsString& aValue);  \
  NS_IMETHOD    RemoveAttributeNS(const nsString& aNamespaceURI, const nsString& aLocalName);  \
  NS_IMETHOD    GetAttributeNodeNS(const nsString& aNamespaceURI, const nsString& aLocalName, nsIDOMAttr** aReturn);  \
  NS_IMETHOD    SetAttributeNodeNS(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn);  \
  NS_IMETHOD    GetElementsByTagNameNS(const nsString& aNamespaceURI, const nsString& aLocalName, nsIDOMNodeList** aReturn);  \
  NS_IMETHOD    HasAttribute(const nsString& aName, PRBool* aReturn);  \
  NS_IMETHOD    HasAttributeNS(const nsString& aNamespaceURI, const nsString& aLocalName, PRBool* aReturn);  \



#define NS_FORWARD_IDOMELEMENT(_to)  \
  NS_IMETHOD    GetTagName(nsString& aTagName) { return _to GetTagName(aTagName); } \
  NS_IMETHOD    GetAttribute(const nsString& aName, nsString& aReturn) { return _to GetAttribute(aName, aReturn); }  \
  NS_IMETHOD    SetAttribute(const nsString& aName, const nsString& aValue) { return _to SetAttribute(aName, aValue); }  \
  NS_IMETHOD    RemoveAttribute(const nsString& aName) { return _to RemoveAttribute(aName); }  \
  NS_IMETHOD    GetAttributeNode(const nsString& aName, nsIDOMAttr** aReturn) { return _to GetAttributeNode(aName, aReturn); }  \
  NS_IMETHOD    SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn) { return _to SetAttributeNode(aNewAttr, aReturn); }  \
  NS_IMETHOD    RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn) { return _to RemoveAttributeNode(aOldAttr, aReturn); }  \
  NS_IMETHOD    GetElementsByTagName(const nsString& aName, nsIDOMNodeList** aReturn) { return _to GetElementsByTagName(aName, aReturn); }  \
  NS_IMETHOD    GetAttributeNS(const nsString& aNamespaceURI, const nsString& aLocalName, nsString& aReturn) { return _to GetAttributeNS(aNamespaceURI, aLocalName, aReturn); }  \
  NS_IMETHOD    SetAttributeNS(const nsString& aNamespaceURI, const nsString& aQualifiedName, const nsString& aValue) { return _to SetAttributeNS(aNamespaceURI, aQualifiedName, aValue); }  \
  NS_IMETHOD    RemoveAttributeNS(const nsString& aNamespaceURI, const nsString& aLocalName) { return _to RemoveAttributeNS(aNamespaceURI, aLocalName); }  \
  NS_IMETHOD    GetAttributeNodeNS(const nsString& aNamespaceURI, const nsString& aLocalName, nsIDOMAttr** aReturn) { return _to GetAttributeNodeNS(aNamespaceURI, aLocalName, aReturn); }  \
  NS_IMETHOD    SetAttributeNodeNS(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn) { return _to SetAttributeNodeNS(aNewAttr, aReturn); }  \
  NS_IMETHOD    GetElementsByTagNameNS(const nsString& aNamespaceURI, const nsString& aLocalName, nsIDOMNodeList** aReturn) { return _to GetElementsByTagNameNS(aNamespaceURI, aLocalName, aReturn); }  \
  NS_IMETHOD    HasAttribute(const nsString& aName, PRBool* aReturn) { return _to HasAttribute(aName, aReturn); }  \
  NS_IMETHOD    HasAttributeNS(const nsString& aNamespaceURI, const nsString& aLocalName, PRBool* aReturn) { return _to HasAttributeNS(aNamespaceURI, aLocalName, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMElement_h__

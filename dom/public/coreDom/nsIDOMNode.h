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

#ifndef nsIDOMNode_h__
#define nsIDOMNode_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMDocument;
class nsIDOMNamedNodeMap;
class nsIDOMNode;
class nsIDOMNodeList;

#define NS_IDOMNODE_IID \
 { 0xa6cf907c, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMNode : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMNODE_IID; return iid; }
  enum {
    ELEMENT_NODE = 1,
    ATTRIBUTE_NODE = 2,
    TEXT_NODE = 3,
    CDATA_SECTION_NODE = 4,
    ENTITY_REFERENCE_NODE = 5,
    ENTITY_NODE = 6,
    PROCESSING_INSTRUCTION_NODE = 7,
    COMMENT_NODE = 8,
    DOCUMENT_NODE = 9,
    DOCUMENT_TYPE_NODE = 10,
    DOCUMENT_FRAGMENT_NODE = 11,
    NOTATION_NODE = 12
  };

  NS_IMETHOD    GetNodeName(nsString& aNodeName)=0;

  NS_IMETHOD    GetNodeValue(nsString& aNodeValue)=0;
  NS_IMETHOD    SetNodeValue(const nsString& aNodeValue)=0;

  NS_IMETHOD    GetNodeType(PRUint16* aNodeType)=0;

  NS_IMETHOD    GetParentNode(nsIDOMNode** aParentNode)=0;

  NS_IMETHOD    GetChildNodes(nsIDOMNodeList** aChildNodes)=0;

  NS_IMETHOD    GetFirstChild(nsIDOMNode** aFirstChild)=0;

  NS_IMETHOD    GetLastChild(nsIDOMNode** aLastChild)=0;

  NS_IMETHOD    GetPreviousSibling(nsIDOMNode** aPreviousSibling)=0;

  NS_IMETHOD    GetNextSibling(nsIDOMNode** aNextSibling)=0;

  NS_IMETHOD    GetAttributes(nsIDOMNamedNodeMap** aAttributes)=0;

  NS_IMETHOD    GetOwnerDocument(nsIDOMDocument** aOwnerDocument)=0;

  NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)=0;

  NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)=0;

  NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)=0;

  NS_IMETHOD    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)=0;

  NS_IMETHOD    HasChildNodes(PRBool* aReturn)=0;

  NS_IMETHOD    CloneNode(PRBool aDeep, nsIDOMNode** aReturn)=0;
};


#define NS_DECL_IDOMNODE   \
  NS_IMETHOD    GetNodeName(nsString& aNodeName);  \
  NS_IMETHOD    GetNodeValue(nsString& aNodeValue);  \
  NS_IMETHOD    SetNodeValue(const nsString& aNodeValue);  \
  NS_IMETHOD    GetNodeType(PRUint16* aNodeType);  \
  NS_IMETHOD    GetParentNode(nsIDOMNode** aParentNode);  \
  NS_IMETHOD    GetChildNodes(nsIDOMNodeList** aChildNodes);  \
  NS_IMETHOD    GetFirstChild(nsIDOMNode** aFirstChild);  \
  NS_IMETHOD    GetLastChild(nsIDOMNode** aLastChild);  \
  NS_IMETHOD    GetPreviousSibling(nsIDOMNode** aPreviousSibling);  \
  NS_IMETHOD    GetNextSibling(nsIDOMNode** aNextSibling);  \
  NS_IMETHOD    GetAttributes(nsIDOMNamedNodeMap** aAttributes);  \
  NS_IMETHOD    GetOwnerDocument(nsIDOMDocument** aOwnerDocument);  \
  NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn);  \
  NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn);  \
  NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn);  \
  NS_IMETHOD    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn);  \
  NS_IMETHOD    HasChildNodes(PRBool* aReturn);  \
  NS_IMETHOD    CloneNode(PRBool aDeep, nsIDOMNode** aReturn);  \



#define NS_FORWARD_IDOMNODE(_to)  \
  NS_IMETHOD    GetNodeName(nsString& aNodeName) { return _to GetNodeName(aNodeName); } \
  NS_IMETHOD    GetNodeValue(nsString& aNodeValue) { return _to GetNodeValue(aNodeValue); } \
  NS_IMETHOD    SetNodeValue(const nsString& aNodeValue) { return _to SetNodeValue(aNodeValue); } \
  NS_IMETHOD    GetNodeType(PRUint16* aNodeType) { return _to GetNodeType(aNodeType); } \
  NS_IMETHOD    GetParentNode(nsIDOMNode** aParentNode) { return _to GetParentNode(aParentNode); } \
  NS_IMETHOD    GetChildNodes(nsIDOMNodeList** aChildNodes) { return _to GetChildNodes(aChildNodes); } \
  NS_IMETHOD    GetFirstChild(nsIDOMNode** aFirstChild) { return _to GetFirstChild(aFirstChild); } \
  NS_IMETHOD    GetLastChild(nsIDOMNode** aLastChild) { return _to GetLastChild(aLastChild); } \
  NS_IMETHOD    GetPreviousSibling(nsIDOMNode** aPreviousSibling) { return _to GetPreviousSibling(aPreviousSibling); } \
  NS_IMETHOD    GetNextSibling(nsIDOMNode** aNextSibling) { return _to GetNextSibling(aNextSibling); } \
  NS_IMETHOD    GetAttributes(nsIDOMNamedNodeMap** aAttributes) { return _to GetAttributes(aAttributes); } \
  NS_IMETHOD    GetOwnerDocument(nsIDOMDocument** aOwnerDocument) { return _to GetOwnerDocument(aOwnerDocument); } \
  NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn) { return _to InsertBefore(aNewChild, aRefChild, aReturn); }  \
  NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn) { return _to ReplaceChild(aNewChild, aOldChild, aReturn); }  \
  NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn) { return _to RemoveChild(aOldChild, aReturn); }  \
  NS_IMETHOD    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn) { return _to AppendChild(aNewChild, aReturn); }  \
  NS_IMETHOD    HasChildNodes(PRBool* aReturn) { return _to HasChildNodes(aReturn); }  \
  NS_IMETHOD    CloneNode(PRBool aDeep, nsIDOMNode** aReturn) { return _to CloneNode(aDeep, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitNodeClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptNode(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMNode_h__

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

class nsIDOMNamedNodeMap;
class nsIDOMNode;
class nsIDOMNodeList;

#define NS_IDOMNODE_IID \
{ 0x6f7652eb,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMNode : public nsISupports {
public:
  enum {
    DOCUMENT = 1,
    ELEMENT = 2,
    ATTRIBUTE = 3,
    PROCESSING_INSTRUCTION = 4,
    COMMENT = 5,
    TEXT = 6,
    CDATA_SECTION = 7,
    DOCUMENT_FRAGMENT = 8,
    ENTITY_DECLARATION = 9,
    ENTITY_REFERENCE = 10
  };

  NS_IMETHOD    GetNodeName(nsString& aNodeName)=0;

  NS_IMETHOD    GetNodeValue(nsString& aNodeValue)=0;
  NS_IMETHOD    SetNodeValue(const nsString& aNodeValue)=0;

  NS_IMETHOD    GetNodeType(PRInt32* aNodeType)=0;

  NS_IMETHOD    GetParentNode(nsIDOMNode** aParentNode)=0;

  NS_IMETHOD    GetChildNodes(nsIDOMNodeList** aChildNodes)=0;

  NS_IMETHOD    GetHasChildNodes(PRBool* aHasChildNodes)=0;

  NS_IMETHOD    GetFirstChild(nsIDOMNode** aFirstChild)=0;

  NS_IMETHOD    GetLastChild(nsIDOMNode** aLastChild)=0;

  NS_IMETHOD    GetPreviousSibling(nsIDOMNode** aPreviousSibling)=0;

  NS_IMETHOD    GetNextSibling(nsIDOMNode** aNextSibling)=0;

  NS_IMETHOD    GetAttributes(nsIDOMNamedNodeMap** aAttributes)=0;

  NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)=0;

  NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)=0;

  NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)=0;

  NS_IMETHOD    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)=0;

  NS_IMETHOD    CloneNode(nsIDOMNode** aReturn)=0;

  NS_IMETHOD    Equals(nsIDOMNode* aNode, PRBool aDeep, PRBool* aReturn)=0;
};


#define NS_DECL_IDOMNODE   \
  NS_IMETHOD    GetNodeName(nsString& aNodeName);  \
  NS_IMETHOD    GetNodeValue(nsString& aNodeValue);  \
  NS_IMETHOD    SetNodeValue(const nsString& aNodeValue);  \
  NS_IMETHOD    GetNodeType(PRInt32* aNodeType);  \
  NS_IMETHOD    GetParentNode(nsIDOMNode** aParentNode);  \
  NS_IMETHOD    GetChildNodes(nsIDOMNodeList** aChildNodes);  \
  NS_IMETHOD    GetHasChildNodes(PRBool* aHasChildNodes);  \
  NS_IMETHOD    GetFirstChild(nsIDOMNode** aFirstChild);  \
  NS_IMETHOD    GetLastChild(nsIDOMNode** aLastChild);  \
  NS_IMETHOD    GetPreviousSibling(nsIDOMNode** aPreviousSibling);  \
  NS_IMETHOD    GetNextSibling(nsIDOMNode** aNextSibling);  \
  NS_IMETHOD    GetAttributes(nsIDOMNamedNodeMap** aAttributes);  \
  NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn);  \
  NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn);  \
  NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn);  \
  NS_IMETHOD    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn);  \
  NS_IMETHOD    CloneNode(nsIDOMNode** aReturn);  \
  NS_IMETHOD    Equals(nsIDOMNode* aNode, PRBool aDeep, PRBool* aReturn);  \



#define NS_FORWARD_IDOMNODE(_to)  \
  NS_IMETHOD    GetNodeName(nsString& aNodeName) { return _to##GetNodeName(aNodeName); } \
  NS_IMETHOD    GetNodeValue(nsString& aNodeValue) { return _to##GetNodeValue(aNodeValue); } \
  NS_IMETHOD    SetNodeValue(const nsString& aNodeValue) { return _to##SetNodeValue(aNodeValue); } \
  NS_IMETHOD    GetNodeType(PRInt32* aNodeType) { return _to##GetNodeType(aNodeType); } \
  NS_IMETHOD    GetParentNode(nsIDOMNode** aParentNode) { return _to##GetParentNode(aParentNode); } \
  NS_IMETHOD    GetChildNodes(nsIDOMNodeList** aChildNodes) { return _to##GetChildNodes(aChildNodes); } \
  NS_IMETHOD    GetHasChildNodes(PRBool* aHasChildNodes) { return _to##GetHasChildNodes(aHasChildNodes); } \
  NS_IMETHOD    GetFirstChild(nsIDOMNode** aFirstChild) { return _to##GetFirstChild(aFirstChild); } \
  NS_IMETHOD    GetLastChild(nsIDOMNode** aLastChild) { return _to##GetLastChild(aLastChild); } \
  NS_IMETHOD    GetPreviousSibling(nsIDOMNode** aPreviousSibling) { return _to##GetPreviousSibling(aPreviousSibling); } \
  NS_IMETHOD    GetNextSibling(nsIDOMNode** aNextSibling) { return _to##GetNextSibling(aNextSibling); } \
  NS_IMETHOD    GetAttributes(nsIDOMNamedNodeMap** aAttributes) { return _to##GetAttributes(aAttributes); } \
  NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn) { return _to##InsertBefore(aNewChild, aRefChild, aReturn); }  \
  NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn) { return _to##ReplaceChild(aNewChild, aOldChild, aReturn); }  \
  NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn) { return _to##RemoveChild(aOldChild, aReturn); }  \
  NS_IMETHOD    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn) { return _to##AppendChild(aNewChild, aReturn); }  \
  NS_IMETHOD    CloneNode(nsIDOMNode** aReturn) { return _to##CloneNode(aReturn); }  \
  NS_IMETHOD    Equals(nsIDOMNode* aNode, PRBool aDeep, PRBool* aReturn) { return _to##Equals(aNode, aDeep, aReturn); }  \


extern nsresult NS_InitNodeClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptNode(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMNode_h__

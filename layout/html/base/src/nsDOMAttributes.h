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

#ifndef nsDOMAttributes_h__
#define nsDOMAttributes_h__

#include "nsIDOMAttribute.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIScriptObjectOwner.h"

class nsIContent;
class nsIHTMLContent;

class nsDOMAttribute : public nsIDOMAttribute, public nsIScriptObjectOwner {
public:
  nsDOMAttribute(const nsString &aName, const nsString &aValue);
  virtual ~nsDOMAttribute();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

  // nsIDOMAttribute interface
  NS_IMETHOD    GetSpecified(PRBool* aSpecified);
  NS_IMETHOD    SetSpecified(PRBool aSpecified);
  NS_IMETHOD    GetName(nsString& aReturn);
  NS_IMETHOD    GetValue(nsString& aReturn);

  // nsIDOMNode interface
  NS_IMETHOD    GetNodeName(nsString& aNodeName);
  NS_IMETHOD    GetNodeValue(nsString& aNodeValue);
  NS_IMETHOD    SetNodeValue(const nsString& aNodeValue);
  NS_IMETHOD    GetNodeType(PRInt32* aNodeType);
  NS_IMETHOD    GetParentNode(nsIDOMNode** aParentNode);
  NS_IMETHOD    GetChildNodes(nsIDOMNodeList** aChildNodes);
  NS_IMETHOD    GetHasChildNodes(PRBool* aHasChildNodes);
  NS_IMETHOD    GetFirstChild(nsIDOMNode** aFirstChild);
  NS_IMETHOD    GetLastChild(nsIDOMNode** aLastChild);
  NS_IMETHOD    GetPreviousSibling(nsIDOMNode** aPreviousSibling);
  NS_IMETHOD    GetNextSibling(nsIDOMNode** aNextSibling);
  NS_IMETHOD    GetAttributes(nsIDOMNamedNodeMap** aAttributes);
  NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn);
  NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn);
  NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn);
  NS_IMETHOD    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn);
  NS_IMETHOD    CloneNode(nsIDOMNode** aReturn);
  NS_IMETHOD    Equals(nsIDOMNode* aNode, PRBool aDeep, PRBool* aReturn);

private:
  nsString *mName;
  nsString *mValue;
  void *mScriptObject;
};


class nsDOMAttributeMap : public nsIDOMNamedNodeMap, public nsIScriptObjectOwner {
public:
  nsDOMAttributeMap(nsIHTMLContent &aContent);
  virtual ~nsDOMAttributeMap();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

  // nsIDOMNamedNodeMap interface
  NS_IMETHOD    GetLength(PRUint32* aSize);
  NS_IMETHOD    GetNamedItem(const nsString& aName, nsIDOMNode** aReturn);
  NS_IMETHOD    SetNamedItem(nsIDOMNode* aNode);
  NS_IMETHOD    RemoveNamedItem(const nsString& aName, nsIDOMNode** aReturn);
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn);

private:
  nsIHTMLContent &mContent;
  void *mScriptObject;
};


#endif


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

class nsIDOMNodeIterator;
class nsIDOMNode;

#define NS_IDOMNODE_IID \
{ 0x6f7652e9,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMNode : public nsISupports {
public:
  enum {
    DOCUMENT = 1,
    ELEMENT = 2,
    ATTRIBUTE = 3,
    PI = 4,
    COMMENT = 5,
    TEXT = 6
  };

  NS_IMETHOD    GetNodeType(PRInt32* aReturn)=0;

  NS_IMETHOD    GetParentNode(nsIDOMNode** aReturn)=0;

  NS_IMETHOD    GetChildNodes(nsIDOMNodeIterator** aReturn)=0;

  NS_IMETHOD    HasChildNodes(PRBool* aReturn)=0;

  NS_IMETHOD    GetFirstChild(nsIDOMNode** aReturn)=0;

  NS_IMETHOD    GetPreviousSibling(nsIDOMNode** aReturn)=0;

  NS_IMETHOD    GetNextSibling(nsIDOMNode** aReturn)=0;

  NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild)=0;

  NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild)=0;

  NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild)=0;
};

extern nsresult NS_InitNodeClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM NS_NewScriptNode(nsIScriptContext *aContext, nsIDOMNode *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMNode_h__

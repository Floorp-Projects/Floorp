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

#ifndef nsIDOMNodeIterator_h__
#define nsIDOMNodeIterator_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMNodeIterator;
class nsIDOMNode;

#define NS_IDOMNODEITERATOR_IID \
{ 0x6f7652e9,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMNodeIterator : public nsISupports {
public:

  NS_IMETHOD    GetLength(PRUint32* aReturn)=0;

  NS_IMETHOD    GetCurrentNode(nsIDOMNode** aReturn)=0;

  NS_IMETHOD    GetNextNode(nsIDOMNode** aReturn)=0;

  NS_IMETHOD    GetPreviousNode(nsIDOMNode** aReturn)=0;

  NS_IMETHOD    ToFirst(nsIDOMNode** aReturn)=0;

  NS_IMETHOD    ToLast(nsIDOMNode** aReturn)=0;

  NS_IMETHOD    MoveTo(PRInt32 aN, nsIDOMNode** aReturn)=0;
};

extern nsresult NS_InitNodeIteratorClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM NS_NewScriptNodeIterator(nsIScriptContext *aContext, nsIDOMNodeIterator *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMNodeIterator_h__

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

#ifndef nsIDOMText_h__
#define nsIDOMText_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMNode.h"

class nsIDOMElement;
class nsIDOMText;

#define NS_IDOMTEXT_IID \
{ 0x6f7652ec,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMText : public nsIDOMNode {
public:

  NS_IMETHOD    GetData(nsString& aData)=0;
  NS_IMETHOD    SetData(nsString& aData)=0;

  NS_IMETHOD    Append(nsString& aData)=0;

  NS_IMETHOD    Insert(PRInt32 aOffset, nsString& aData)=0;

  NS_IMETHOD    Delete(PRInt32 aOffset, PRInt32 aCount)=0;

  NS_IMETHOD    Replace(PRInt32 aOffset, PRInt32 aCount, nsString& aData)=0;

  NS_IMETHOD    Splice(nsIDOMElement* aElement, PRInt32 aOffset, PRInt32 aCount)=0;
};

extern nsresult NS_InitTextClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM NS_NewScriptText(nsIScriptContext *aContext, nsIDOMText *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMText_h__

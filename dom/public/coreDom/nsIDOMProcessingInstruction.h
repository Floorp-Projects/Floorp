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

#ifndef nsIDOMProcessingInstruction_h__
#define nsIDOMProcessingInstruction_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMNode.h"


#define NS_IDOMPROCESSINGINSTRUCTION_IID \
 { 0xa6cf907f, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMProcessingInstruction : public nsIDOMNode {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMPROCESSINGINSTRUCTION_IID; return iid; }

  NS_IMETHOD    GetTarget(nsString& aTarget)=0;

  NS_IMETHOD    GetData(nsString& aData)=0;
  NS_IMETHOD    SetData(const nsString& aData)=0;
};


#define NS_DECL_IDOMPROCESSINGINSTRUCTION   \
  NS_IMETHOD    GetTarget(nsString& aTarget);  \
  NS_IMETHOD    GetData(nsString& aData);  \
  NS_IMETHOD    SetData(const nsString& aData);  \



#define NS_FORWARD_IDOMPROCESSINGINSTRUCTION(_to)  \
  NS_IMETHOD    GetTarget(nsString& aTarget) { return _to GetTarget(aTarget); } \
  NS_IMETHOD    GetData(nsString& aData) { return _to GetData(aData); } \
  NS_IMETHOD    SetData(const nsString& aData) { return _to SetData(aData); } \


extern "C" NS_DOM nsresult NS_InitProcessingInstructionClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptProcessingInstruction(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMProcessingInstruction_h__

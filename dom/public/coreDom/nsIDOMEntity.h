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

#ifndef nsIDOMEntity_h__
#define nsIDOMEntity_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMNode.h"


#define NS_IDOMENTITY_IID \
 { 0xa6cf9079, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMEntity : public nsIDOMNode {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMENTITY_IID; return iid; }

  NS_IMETHOD    GetPublicId(nsString& aPublicId)=0;

  NS_IMETHOD    GetSystemId(nsString& aSystemId)=0;

  NS_IMETHOD    GetNotationName(nsString& aNotationName)=0;
};


#define NS_DECL_IDOMENTITY   \
  NS_IMETHOD    GetPublicId(nsString& aPublicId);  \
  NS_IMETHOD    GetSystemId(nsString& aSystemId);  \
  NS_IMETHOD    GetNotationName(nsString& aNotationName);  \



#define NS_FORWARD_IDOMENTITY(_to)  \
  NS_IMETHOD    GetPublicId(nsString& aPublicId) { return _to GetPublicId(aPublicId); } \
  NS_IMETHOD    GetSystemId(nsString& aSystemId) { return _to GetSystemId(aSystemId); } \
  NS_IMETHOD    GetNotationName(nsString& aNotationName) { return _to GetNotationName(aNotationName); } \


extern "C" NS_DOM nsresult NS_InitEntityClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptEntity(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMEntity_h__

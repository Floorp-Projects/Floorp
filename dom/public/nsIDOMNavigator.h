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

#ifndef nsIDOMNavigator_h__
#define nsIDOMNavigator_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMNavigator;

#define NS_IDOMNAVIGATOR_IID \
{ 0x6f7652e8,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMNavigator : public nsISupports {
public:

  NS_IMETHOD    GetUserAgent(nsString& aUserAgent)=0;

  NS_IMETHOD    GetAppCodeName(nsString& aAppCodeName)=0;

  NS_IMETHOD    GetAppVersion(nsString& aAppVersion)=0;

  NS_IMETHOD    GetAppName(nsString& aAppName)=0;

  NS_IMETHOD    GetLanguage(nsString& aLanguage)=0;

  NS_IMETHOD    GetPlatform(nsString& aPlatform)=0;

  NS_IMETHOD    GetSecurityPolicy(nsString& aSecurityPolicy)=0;

  NS_IMETHOD    JavaEnabled(PRBool* aReturn)=0;
};

extern nsresult NS_InitNavigatorClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM NS_NewScriptNavigator(nsIScriptContext *aContext, nsIDOMNavigator *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMNavigator_h__

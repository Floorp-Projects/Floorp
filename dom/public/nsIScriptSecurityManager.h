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

#ifndef nsIScriptSecurityManager_h__
#define nsIScriptSecurityManager_h__

#include "nscore.h"
#include "nsISupports.h"

class nsIScriptContext;

/*
 * Event listener interface.
 */

#define NS_ISCRIPTSECURITYMANAGER_IID \
{ /* 58df5780-8006-11d2-bd91-00805f8ae3f4 */ \
0x58df5780, 0x8006, 0x11d2, \
{0xbd, 0x91, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

class nsIScriptSecurityManager : public nsISupports {

public:
 /**
  * Inits the security manager
  */
  NS_IMETHOD Init() = 0;

 /**
  * Checks script access to the property/method in question
  */
  NS_IMETHOD CheckScriptAccess(nsIScriptContext* aContext, void* aObj, const char* aProp, PRBool* aResult) = 0;

};

//Security flags
#define SCRIPT_SECURITY_NO_ACCESS           0x0001
#define SCRIPT_SECURITY_SAME_DOMAIN_ACCESS  0x0002
//XXX expand this flag out once we know the privileges we'll support
#define SCRIPT_SECURITY_SIGNED_ACCESS       0x0004

extern "C" NS_DOM nsresult NS_NewScriptSecurityManager(nsIScriptSecurityManager ** aInstancePtrResult);

#endif // nsIScriptSecurityManager_h__

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

#ifndef nsIJSNativeInitializer_h__
#define nsIJSNativeInitializer_h__

#include "nsISupports.h"
#include "jsapi.h"

#define NS_IJSNATIVEINITIALIZER_IID \
{0xa6cf90f4, 0x15b3, 0x11d2,        \
 {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

/**
 * A JavaScript specific interface used to initialize new
 * native objects, created as a result of calling a
 * JavaScript constructor. The arguments are passed in
 * their raw form as jsval's.
 */

class nsIJSNativeInitializer : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IJSNATIVEINITIALIZER_IID; return iid; }

  /**
   * Intialize a newly created native instance using the parameters
   * passed into the JavaScript constructor.
   */
  NS_IMETHOD Initialize(JSContext* cx, PRUint32 argc, jsval *argv) = 0;
};


#endif // nsIJSNativeInitializer_h__

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

#ifndef nsIDOMNativeObjectRegistry_h__
#define nsIDOMNativeObjectRegistry_h__

#include "nsISupports.h"
#include "nsString.h"

#define NS_IDOM_NATIVE_OBJECT_REGISTRY_IID   \
{ 0xa6cf9067, 0x15b3, 0x11d2,               \
 { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

class nsIDOMNativeObjectRegistry : public nsISupports {
public:  
  static const nsIID& GetIID() { static nsIID iid = NS_IDOM_NATIVE_OBJECT_REGISTRY_IID; return iid; }

  /**
   * Register a class ID for a factory to create the native object
   * associated with a specific DOM class e.g. "HTMLImageElement".
   *
   * @param aClassName - DOM class associated with the factory
   * @param aCID - (in param) Class ID for the factory.
   */
  NS_IMETHOD RegisterFactory(const nsString& aClassName, const nsIID& aCID)=0;

  /*
   * Obtain the class ID for a factory t create the native object
   * associated with a sepcific DOM class
   *
   * @param aClassName - DOM class associated with the factory
   * @param aCID - (out param) Class ID for the factory.
   */
  NS_IMETHOD GetFactoryCID(const nsString& aClassName, nsIID& aCID)=0;
};

#endif /* nsIDOMNativeObjectRegistry_h__ */

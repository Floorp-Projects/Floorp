/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsIScriptExternalNameSet_h__
#define nsIScriptExternalNameSet_h__

#include "nscore.h"
#include "nsString.h"
#include "nsISupports.h"

#define NS_ISCRIPTEXTERNALNAMESET_IID \
  {0xa6cf90da, 0x15b3, 0x11d2,        \
  {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

class nsIScriptContext;

/**
 * This interface represents a set of names or symbols
 * that should be added to the "global" namespace of a
 * script context. A name set is registered with the
 * name set registry (see <code>nsIScriptNameSetRegistry</code>)
 * at application initialization time. The 
 * <code>AddNameSet</code> method is invoked for each
 * new script context so that a name set can add its
 * global symbols to the name space manager
 * (see <code>nsIScriptNameSpaceManager</code>) of the
 * script context.
 */
class nsIScriptExternalNameSet : public nsISupports {
public:
  /**
   * Called to tell the name set to do any class initialization
   * it needs to
   */
  NS_IMETHOD InitializeClasses(nsIScriptContext* aScriptContext) = 0;

  /**
   * Called to tell the name set to add its names to the
   * namespace manager of the context.
   */
  NS_IMETHOD AddNameSet(nsIScriptContext* aScriptContext) = 0;
};

#endif /* nsIScriptExternalNameSet_h__ */

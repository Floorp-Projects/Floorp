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

#ifndef nsIScriptNameSetRegistry_h__
#define nsIScriptNameSetRegistry_h__

#include "nscore.h"
#include "nsString.h"
#include "nsISupports.h"

#define NS_ISCRIPTNAMESETREGISTRY_IID                   \
 {0xa6cf90d9, 0x15b3, 0x11d2,                           \
 {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

class nsIScriptExternalNameSet;

/**
 * Interface used to add and remove name sets. These name sets
 * are asked to register themselves with a namespace registry
 * for every new script context.
 */
class nsIScriptNameSetRegistry : public nsISupports {
public:
  /** 
   * Add a new name set to the list. Each name set will be asked
   * to register itself with a namespace registry for each new
   * script context.
   *
   * @param aNameSet the name set to register
   * @result NS_OK if successful
   */
  NS_IMETHOD AddExternalNameSet(nsIScriptExternalNameSet* aNameSet) = 0;

  /*
   * Remove a name set from the manager's list.
   *
   * @param aNameSet the name set to unregister.
   * @result NS_OK if successful
   */
  NS_IMETHOD RemoveExternalNameSet(nsIScriptExternalNameSet* aNameSet) = 0;
};

#endif /* nsIScriptNameSetRegistry_h__ */

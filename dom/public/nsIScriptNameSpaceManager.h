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

#ifndef nsIScriptNameSpaceManager_h__
#define nsIScriptNameSpaceManager_h__

#include "nscore.h"
#include "nsString.h"
#include "nsISupports.h"

#define NS_ISCRIPTNAMESPACEMANAGER_IID   \
  {0xa6cf90db, 0x15b3, 0x11d2,           \
  {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

/**
 * A per script context construct. The namespace manager maintains
 * a mapping between a global symbol or name and a class ID to
 * obtain a factory to use to instantiate a native object.
 */
class nsIScriptNameSpaceManager : public nsISupports {
public:
  /**
   * Used to register a single global symbol or name. The class ID
   * is used to obtain a factory which is used to instantiate
   * the the corresponding native object. The name can represent
   * either an object or a constructor.
   * XXX How do we indicate clashes in registered names?
   * 
   * @param aName the name to reflect in the global namespace
   * @param aCID the class ID used to obtain a factory
   * @param aIsConstructor set to PR_TRUE if the name represents
   *        a constructor. PR_FALSE if it is a global object.
   * @result NS_OK if successful
   */
  NS_IMETHOD RegisterGlobalName(const nsString& aName, 
                                const nsIID& aCID,
                                PRBool aIsConstructor) = 0;

  /**
   * Used to remove a global symbol or name from the manager.
   * 
   * @param aName the name to remove
   * @result NS_OK if successful
   */
  NS_IMETHOD UnregisterGlobalName(const nsString& aName) = 0;

  /**
   * Used to look up the manager using a name as a key. The
   * return value is a class ID that can be used to obtain
   * a factory.
   * 
   * @param aName the name to use as a key for the lookup
   * @param aIsConstructor PR_TRUE if we're looking for
   *        a constructor. PR_FALSE if we're looking for 
   *        a global symbol.
   * @param aCID out parameter that returns the class ID
   *        that corresponds to the name
   * @result NS_OK if the lookup succeeded. NS_COMFALSE
   *         if the lookup failed.
   */
  NS_IMETHOD LookupName(const nsString& aName, 
                        PRBool aIsConstructor,
                        nsIID& aCID) = 0;
};

extern "C" NS_DOM nsresult NS_NewScriptNameSpaceManager(nsIScriptNameSpaceManager** aInstancePtr);

#endif /* nsIScriptNameSpaceManager_h__ */

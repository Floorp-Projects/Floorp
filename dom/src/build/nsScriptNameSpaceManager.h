/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

#ifndef nsScriptNameSetRegistry_h__
#define nsScriptNameSetRegistry_h__

#include "nsIScriptNameSpaceManager.h"
#include "plhash.h"

class nsScriptNameSpaceManager : public nsIScriptNameSpaceManager {
 public:
  nsScriptNameSpaceManager();
  virtual ~nsScriptNameSpaceManager();

  NS_DECL_ISUPPORTS

  NS_IMETHOD RegisterGlobalName(const nsString& aName, 
                                const nsIID& aCID,
                                PRBool aIsConstructor);
  NS_IMETHOD UnregisterGlobalName(const nsString& aName);
  NS_IMETHOD LookupName(const nsString& aName, 
                        PRBool aIsConstructor,
                        nsIID& aCID);
  
 protected:
  static PRIntn PR_CALLBACK RemoveNames(PLHashEntry *he, PRIntn i, void *arg);

  PLHashTable* mGlobalNames;
};

#endif /* nsScriptNameSetRegistry_h__ */

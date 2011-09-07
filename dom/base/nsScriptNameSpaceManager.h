/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
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

#ifndef nsScriptNameSpaceManager_h__
#define nsScriptNameSpaceManager_h__

#include "nsIScriptNameSpaceManager.h"
#include "nsString.h"
#include "nsID.h"
#include "pldhash.h"
#include "nsDOMClassInfo.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"


struct nsGlobalNameStruct
{
  struct ConstructorAlias
  {
    nsCID mCID;
    nsString mProtoName;
    nsGlobalNameStruct* mProto;    
  };

  enum nametype {
    eTypeNotInitialized,
    eTypeInterface,
    eTypeProperty,
    eTypeNavigatorProperty,
    eTypeExternalConstructor,
    eTypeStaticNameSet,
    eTypeDynamicNameSet,
    eTypeClassConstructor,
    eTypeClassProto,
    eTypeExternalClassInfoCreator,
    eTypeExternalClassInfo,
    eTypeExternalConstructorAlias
  } mType;

  PRBool mChromeOnly;
  PRBool mDisabled;

  union {
    PRInt32 mDOMClassInfoID; // eTypeClassConstructor
    nsIID mIID; // eTypeInterface, eTypeClassProto
    nsExternalDOMClassInfoData* mData; // eTypeExternalClassInfo
    ConstructorAlias* mAlias; // eTypeExternalConstructorAlias
    nsCID mCID; // All other types...
  };

private:

  // copy constructor
};


class nsIScriptContext;
class nsICategoryManager;
class GlobalNameMapEntry;


class nsScriptNameSpaceManager : public nsIObserver,
                                 public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsScriptNameSpaceManager();
  virtual ~nsScriptNameSpaceManager();

  nsresult Init();
  nsresult InitForContext(nsIScriptContext *aContext);

  // Returns a nsGlobalNameStruct for aName, or null if one is not
  // found. The returned nsGlobalNameStruct is only guaranteed to be
  // valid until the next call to any of the methods in this class.
  // It also returns a pointer to the string buffer of the classname
  // in the nsGlobalNameStruct.
  nsresult LookupName(const nsAString& aName,
                      const nsGlobalNameStruct **aNameStruct,
                      const PRUnichar **aClassName = nsnull);
  // Returns a nsGlobalNameStruct for the navigator property aName, or
  // null if one is not found. The returned nsGlobalNameStruct is only
  // guaranteed to be valid until the next call to any of the methods
  // in this class.
  nsresult LookupNavigatorName(const nsAString& aName,
                               const nsGlobalNameStruct **aNameStruct);

  nsresult RegisterClassName(const char *aClassName,
                             PRInt32 aDOMClassInfoID,
                             PRBool aPrivileged,
                             PRBool aDisabled,
                             const PRUnichar **aResult);

  nsresult RegisterClassProto(const char *aClassName,
                              const nsIID *aConstructorProtoIID,
                              PRBool *aFoundOld);

  nsresult RegisterExternalInterfaces(PRBool aAsProto);

  nsresult RegisterExternalClassName(const char *aClassName,
                                     nsCID& aCID);

  // Register the info for an external class. aName must be static
  // data, it will not be deleted by the DOM code.
  nsresult RegisterDOMCIData(const char *aName,
                             nsDOMClassInfoExternalConstructorFnc aConstructorFptr,
                             const nsIID *aProtoChainInterface,
                             const nsIID **aInterfaces,
                             PRUint32 aScriptableFlags,
                             PRBool aHasClassInterface,
                             const nsCID *aConstructorCID);

  nsGlobalNameStruct* GetConstructorProto(const nsGlobalNameStruct* aStruct);

protected:
  // Adds a new entry to the hash and returns the nsGlobalNameStruct
  // that aKey will be mapped to. If mType in the returned
  // nsGlobalNameStruct is != eTypeNotInitialized, an entry for aKey
  // already existed.
  nsGlobalNameStruct *AddToHash(PLDHashTable *aTable, const char *aKey,
                                const PRUnichar **aClassName = nsnull);

  nsresult FillHash(nsICategoryManager *aCategoryManager,
                    const char *aCategory);
  nsresult FillHashWithDOMInterfaces();
  nsresult RegisterInterface(const char* aIfName,
                             const nsIID *aIfIID,
                             PRBool* aFoundOld);

  /**
   * Add a new category entry into the hash table.
   * Only some categories can be added (see the beginning of the definition).
   * The other ones will be ignored.
   *
   * @aCategoryManager Instance of the category manager service.
   * @aCategory        Category where the entry comes from.
   * @aEntry           The entry that should be added.
   */
  nsresult AddCategoryEntryToHash(nsICategoryManager* aCategoryManager,
                                  const char* aCategory,
                                  nsISupports* aEntry);

  PLDHashTable mGlobalNames;
  PLDHashTable mNavigatorNames;

  PRPackedBool mIsInitialized;
};

#endif /* nsScriptNameSpaceManager_h__ */

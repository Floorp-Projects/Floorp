/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
    eTypeNewDOMBinding,
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

  bool mChromeOnly;
  bool mDisabled;

  union {
    PRInt32 mDOMClassInfoID; // eTypeClassConstructor
    nsIID mIID; // eTypeInterface, eTypeClassProto
    nsExternalDOMClassInfoData* mData; // eTypeExternalClassInfo
    ConstructorAlias* mAlias; // eTypeExternalConstructorAlias
    nsCID mCID; // All other types except eTypeNewDOMBinding
  };

  // For new style DOM bindings.
  mozilla::dom::binding::DefineInterface mDefineDOMInterface;

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
  const nsGlobalNameStruct* LookupName(const nsAString& aName,
                                       const PRUnichar **aClassName = nsnull)
  {
    return LookupNameInternal(aName, aClassName);
  }

  // Returns a nsGlobalNameStruct for the navigator property aName, or
  // null if one is not found. The returned nsGlobalNameStruct is only
  // guaranteed to be valid until the next call to any of the methods
  // in this class.
  nsresult LookupNavigatorName(const nsAString& aName,
                               const nsGlobalNameStruct **aNameStruct);

  nsresult RegisterClassName(const char *aClassName,
                             PRInt32 aDOMClassInfoID,
                             bool aPrivileged,
                             bool aDisabled,
                             const PRUnichar **aResult);

  nsresult RegisterClassProto(const char *aClassName,
                              const nsIID *aConstructorProtoIID,
                              bool *aFoundOld);

  nsresult RegisterExternalInterfaces(bool aAsProto);

  nsresult RegisterExternalClassName(const char *aClassName,
                                     nsCID& aCID);

  // Register the info for an external class. aName must be static
  // data, it will not be deleted by the DOM code.
  nsresult RegisterDOMCIData(const char *aName,
                             nsDOMClassInfoExternalConstructorFnc aConstructorFptr,
                             const nsIID *aProtoChainInterface,
                             const nsIID **aInterfaces,
                             PRUint32 aScriptableFlags,
                             bool aHasClassInterface,
                             const nsCID *aConstructorCID);

  nsGlobalNameStruct* GetConstructorProto(const nsGlobalNameStruct* aStruct);

  void RegisterDefineDOMInterface(const nsAFlatString& aName,
    mozilla::dom::binding::DefineInterface aDefineDOMInterface);

private:
  // Adds a new entry to the hash and returns the nsGlobalNameStruct
  // that aKey will be mapped to. If mType in the returned
  // nsGlobalNameStruct is != eTypeNotInitialized, an entry for aKey
  // already existed.
  nsGlobalNameStruct *AddToHash(PLDHashTable *aTable, const nsAString *aKey,
                                const PRUnichar **aClassName = nsnull);
  nsGlobalNameStruct *AddToHash(PLDHashTable *aTable, const char *aKey,
                                const PRUnichar **aClassName = nsnull)
  {
    NS_ConvertASCIItoUTF16 key(aKey);
    return AddToHash(aTable, &key, aClassName);
  }

  nsresult FillHash(nsICategoryManager *aCategoryManager,
                    const char *aCategory);
  nsresult FillHashWithDOMInterfaces();
  nsresult RegisterInterface(const char* aIfName,
                             const nsIID *aIfIID,
                             bool* aFoundOld);

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

  nsGlobalNameStruct* LookupNameInternal(const nsAString& aName,
                                         const PRUnichar **aClassName = nsnull);

  PLDHashTable mGlobalNames;
  PLDHashTable mNavigatorNames;

  bool mIsInitialized;
};

#endif /* nsScriptNameSpaceManager_h__ */

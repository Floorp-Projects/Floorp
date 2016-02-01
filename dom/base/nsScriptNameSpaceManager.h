/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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

#include "mozilla/MemoryReporting.h"
#include "nsBaseHashtable.h"
#include "nsIMemoryReporter.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsString.h"
#include "nsID.h"
#include "PLDHashTable.h"
#include "nsDOMClassInfo.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "xpcpublic.h"


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
    eTypeClassConstructor,
    eTypeClassProto,
    eTypeExternalClassInfoCreator,
    eTypeExternalClassInfo,
    eTypeExternalConstructorAlias
  } mType;

  // mChromeOnly is only used for structs that define non-WebIDL things
  // (possibly in addition to WebIDL ones).  In particular, it's not even
  // initialized for eTypeNewDOMBinding structs.
  bool mChromeOnly : 1;
  bool mAllowXBL : 1;

  union {
    int32_t mDOMClassInfoID; // eTypeClassConstructor
    nsIID mIID; // eTypeInterface, eTypeClassProto
    nsExternalDOMClassInfoData* mData; // eTypeExternalClassInfo
    ConstructorAlias* mAlias; // eTypeExternalConstructorAlias
    nsCID mCID; // All other types except eTypeNewDOMBinding
  };

  // For new style DOM bindings.
  union {
    mozilla::dom::DefineInterface mDefineDOMInterface; // for window
    mozilla::dom::ConstructNavigatorProperty mConstructNavigatorProperty; // for navigator
  };
  // May be null if enabled unconditionally
  mozilla::dom::ConstructorEnabled* mConstructorEnabled;
};

class GlobalNameMapEntry : public PLDHashEntryHdr
{
public:
  // Our hash table ops don't care about the order of these members.
  nsString mKey;
  nsGlobalNameStruct mGlobalName;

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    // Measurement of the following members may be added later if DMD finds it
    // is worthwhile:
    // - mGlobalName
    return mKey.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }
};

class nsICategoryManager;

class nsScriptNameSpaceManager : public nsIObserver,
                                 public nsSupportsWeakReference,
                                 public nsIMemoryReporter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIMEMORYREPORTER

  nsScriptNameSpaceManager();

  nsresult Init();

  // Returns a nsGlobalNameStruct for aName, or null if one is not
  // found. The returned nsGlobalNameStruct is only guaranteed to be
  // valid until the next call to any of the methods in this class.
  // It also returns a pointer to the string buffer of the classname
  // in the nsGlobalNameStruct.
  const nsGlobalNameStruct* LookupName(const nsAString& aName,
                                       const char16_t **aClassName = nullptr)
  {
    return LookupNameInternal(aName, aClassName);
  }

  // Returns a nsGlobalNameStruct for the navigator property aName, or
  // null if one is not found. The returned nsGlobalNameStruct is only
  // guaranteed to be valid until the next call to any of the methods
  // in this class.
  const nsGlobalNameStruct* LookupNavigatorName(const nsAString& aName);

  nsresult RegisterClassName(const char *aClassName,
                             int32_t aDOMClassInfoID,
                             bool aPrivileged,
                             bool aXBLAllowed,
                             const char16_t **aResult);

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
                             uint32_t aScriptableFlags,
                             bool aHasClassInterface,
                             const nsCID *aConstructorCID);

  nsGlobalNameStruct* GetConstructorProto(const nsGlobalNameStruct* aStruct);

  void RegisterDefineDOMInterface(const nsAFlatString& aName,
    mozilla::dom::DefineInterface aDefineDOMInterface,
    mozilla::dom::ConstructorEnabled* aConstructorEnabled);
  template<size_t N>
  void RegisterDefineDOMInterface(const char16_t (&aKey)[N],
    mozilla::dom::DefineInterface aDefineDOMInterface,
    mozilla::dom::ConstructorEnabled* aConstructorEnabled)
  {
    nsLiteralString key(aKey);
    return RegisterDefineDOMInterface(key, aDefineDOMInterface,
                                      aConstructorEnabled);
  }

  void RegisterNavigatorDOMConstructor(const nsAFlatString& aName,
    mozilla::dom::ConstructNavigatorProperty aNavConstructor,
    mozilla::dom::ConstructorEnabled* aConstructorEnabled);
  template<size_t N>
  void RegisterNavigatorDOMConstructor(const char16_t (&aKey)[N],
    mozilla::dom::ConstructNavigatorProperty aNavConstructor,
    mozilla::dom::ConstructorEnabled* aConstructorEnabled)
  {
    nsLiteralString key(aKey);
    return RegisterNavigatorDOMConstructor(key, aNavConstructor,
                                           aConstructorEnabled);
  }

  class NameIterator : public PLDHashTable::Iterator
  {
  public:
    typedef PLDHashTable::Iterator Base;
    explicit NameIterator(PLDHashTable* aTable) : Base(aTable) {}
    NameIterator(NameIterator&& aOther) : Base(mozilla::Move(aOther.mTable)) {}

    const GlobalNameMapEntry* Get() const
    {
      return static_cast<const GlobalNameMapEntry*>(Base::Get());
    }

  private:
    NameIterator() = delete;
    NameIterator(const NameIterator&) = delete;
    NameIterator& operator=(const NameIterator&) = delete;
    NameIterator& operator=(const NameIterator&&) = delete;
  };

  NameIterator GlobalNameIter()    { return NameIterator(&mGlobalNames); }
  NameIterator NavigatorNameIter() { return NameIterator(&mNavigatorNames); }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  virtual ~nsScriptNameSpaceManager();

  // Adds a new entry to the hash and returns the nsGlobalNameStruct
  // that aKey will be mapped to. If mType in the returned
  // nsGlobalNameStruct is != eTypeNotInitialized, an entry for aKey
  // already existed.
  nsGlobalNameStruct *AddToHash(PLDHashTable *aTable, const nsAString *aKey,
                                const char16_t **aClassName = nullptr);
  nsGlobalNameStruct *AddToHash(PLDHashTable *aTable, const char *aKey,
                                const char16_t **aClassName = nullptr)
  {
    NS_ConvertASCIItoUTF16 key(aKey);
    return AddToHash(aTable, &key, aClassName);
  }
  // Removes an existing entry from the hash.
  void RemoveFromHash(PLDHashTable *aTable, const nsAString *aKey);

  nsresult FillHash(nsICategoryManager *aCategoryManager,
                    const char *aCategory);
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

  /**
   * Remove an existing category entry from the hash table.
   * Only some categories can be removed (see the beginning of the definition).
   * The other ones will be ignored.
   *
   * @aCategory        Category where the entry will be removed from.
   * @aEntry           The entry that should be removed.
   */
  nsresult RemoveCategoryEntryFromHash(nsICategoryManager* aCategoryManager,
                                       const char* aCategory,
                                       nsISupports* aEntry);

  // common helper for AddCategoryEntryToHash and RemoveCategoryEntryFromHash
  nsresult OperateCategoryEntryHash(nsICategoryManager* aCategoryManager,
                                    const char* aCategory,
                                    nsISupports* aEntry,
                                    bool aRemove);

  nsGlobalNameStruct* LookupNameInternal(const nsAString& aName,
                                         const char16_t **aClassName = nullptr);

  PLDHashTable mGlobalNames;
  PLDHashTable mNavigatorNames;
};

#endif /* nsScriptNameSpaceManager_h__ */

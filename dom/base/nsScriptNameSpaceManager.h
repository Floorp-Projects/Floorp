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
  enum nametype {
    eTypeNotInitialized,
    eTypeProperty,
    eTypeExternalConstructor,
    eTypeClassConstructor,
    eTypeClassProto,
  } mType;

  bool mChromeOnly : 1;
  bool mAllowXBL : 1;

  union {
    int32_t mDOMClassInfoID; // eTypeClassConstructor
    nsIID mIID; // eTypeClassProto
    nsCID mCID; // All other types
  };
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
                                       const char16_t **aClassName = nullptr);

  nsresult RegisterClassName(const char *aClassName,
                             int32_t aDOMClassInfoID,
                             bool aPrivileged,
                             bool aXBLAllowed,
                             const char16_t **aResult);

  nsresult RegisterClassProto(const char *aClassName,
                              const nsIID *aConstructorProtoIID,
                              bool *aFoundOld);

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

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  virtual ~nsScriptNameSpaceManager();

  // Adds a new entry to the hash and returns the nsGlobalNameStruct
  // that aKey will be mapped to. If mType in the returned
  // nsGlobalNameStruct is != eTypeNotInitialized, an entry for aKey
  // already existed.
  nsGlobalNameStruct *AddToHash(const char *aKey,
                                const char16_t **aClassName = nullptr);

  // Removes an existing entry from the hash.
  void RemoveFromHash(const nsAString *aKey);

  nsresult FillHash(nsICategoryManager *aCategoryManager,
                    const char *aCategory);

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

  PLDHashTable mGlobalNames;
};

#endif /* nsScriptNameSpaceManager_h__ */

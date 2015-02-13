/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcom-config.h"
#include <new>    // for placement new
#include "nscore.h"
#include "nsCRT.h"

#include "nsCommandParams.h"
#include "mozilla/HashFunctions.h"

using namespace mozilla;

const PLDHashTableOps nsCommandParams::sHashOps =
{
  HashKey,
  HashMatchEntry,
  HashMoveEntry,
  HashClearEntry
};

NS_IMPL_ISUPPORTS(nsCommandParams, nsICommandParams)

nsCommandParams::nsCommandParams()
{
  // init the hash table later
}

nsCommandParams::~nsCommandParams()
{
  PL_DHashTableFinish(&mValuesHash);
}

nsresult
nsCommandParams::Init()
{
  PL_DHashTableInit(&mValuesHash, &sHashOps, sizeof(HashEntry), 2);
  return NS_OK;
}

NS_IMETHODIMP
nsCommandParams::GetValueType(const char* aName, int16_t* aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);

  HashEntry* foundEntry = GetNamedEntry(aName);
  if (foundEntry) {
    *aRetVal = foundEntry->mEntryType;
    return NS_OK;
  }
  *aRetVal = eNoType;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCommandParams::GetBooleanValue(const char* aName, bool* aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);

  HashEntry* foundEntry = GetNamedEntry(aName);
  if (foundEntry && foundEntry->mEntryType == eBooleanType) {
    *aRetVal = foundEntry->mData.mBoolean;
    return NS_OK;
  }
  *aRetVal = false;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCommandParams::GetLongValue(const char* aName, int32_t* aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);

  HashEntry* foundEntry = GetNamedEntry(aName);
  if (foundEntry && foundEntry->mEntryType == eLongType) {
    *aRetVal = foundEntry->mData.mLong;
    return NS_OK;
  }
  *aRetVal = false;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCommandParams::GetDoubleValue(const char* aName, double* aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);

  HashEntry* foundEntry = GetNamedEntry(aName);
  if (foundEntry && foundEntry->mEntryType == eDoubleType) {
    *aRetVal = foundEntry->mData.mDouble;
    return NS_OK;
  }
  *aRetVal = 0.0;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCommandParams::GetStringValue(const char* aName, nsAString& aRetVal)
{
  HashEntry* foundEntry = GetNamedEntry(aName);
  if (foundEntry && foundEntry->mEntryType == eWStringType) {
    NS_ASSERTION(foundEntry->mData.mString, "Null string");
    aRetVal.Assign(*foundEntry->mData.mString);
    return NS_OK;
  }
  aRetVal.Truncate();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCommandParams::GetCStringValue(const char* aName, char** aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);

  HashEntry* foundEntry = GetNamedEntry(aName);
  if (foundEntry && foundEntry->mEntryType == eStringType) {
    NS_ASSERTION(foundEntry->mData.mCString, "Null string");
    *aRetVal = ToNewCString(*foundEntry->mData.mCString);
    return NS_OK;
  }
  *aRetVal = nullptr;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCommandParams::GetISupportsValue(const char* aName, nsISupports** aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);

  HashEntry* foundEntry = GetNamedEntry(aName);
  if (foundEntry && foundEntry->mEntryType == eISupportsType) {
    NS_IF_ADDREF(*aRetVal = foundEntry->mISupports.get());
    return NS_OK;
  }
  *aRetVal = nullptr;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCommandParams::SetBooleanValue(const char* aName, bool value)
{
  HashEntry* foundEntry = GetOrMakeEntry(aName, eBooleanType);
  if (!foundEntry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  foundEntry->mData.mBoolean = value;
  return NS_OK;
}

NS_IMETHODIMP
nsCommandParams::SetLongValue(const char* aName, int32_t value)
{
  HashEntry* foundEntry = GetOrMakeEntry(aName, eLongType);
  if (!foundEntry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  foundEntry->mData.mLong = value;
  return NS_OK;
}

NS_IMETHODIMP
nsCommandParams::SetDoubleValue(const char* aName, double value)
{
  HashEntry* foundEntry = GetOrMakeEntry(aName, eDoubleType);
  if (!foundEntry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  foundEntry->mData.mDouble = value;
  return NS_OK;
}

NS_IMETHODIMP
nsCommandParams::SetStringValue(const char* aName, const nsAString& value)
{
  HashEntry* foundEntry = GetOrMakeEntry(aName, eWStringType);
  if (!foundEntry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  foundEntry->mData.mString = new nsString(value);
  return NS_OK;
}

NS_IMETHODIMP
nsCommandParams::SetCStringValue(const char* aName, const char* value)
{
  HashEntry* foundEntry = GetOrMakeEntry(aName, eStringType);
  if (!foundEntry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  foundEntry->mData.mCString = new nsCString(value);
  return NS_OK;
}

NS_IMETHODIMP
nsCommandParams::SetISupportsValue(const char* aName, nsISupports* value)
{
  HashEntry* foundEntry = GetOrMakeEntry(aName, eISupportsType);
  if (!foundEntry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  foundEntry->mISupports = value;   // addrefs
  return NS_OK;
}

NS_IMETHODIMP
nsCommandParams::RemoveValue(const char* aName)
{
  // PL_DHashTableRemove doesn't tell us if the entry was really removed, so we
  // return NS_OK unconditionally.
  (void)PL_DHashTableRemove(&mValuesHash, (void *)aName);
  return NS_OK;
}

nsCommandParams::HashEntry*
nsCommandParams::GetNamedEntry(const char* aName)
{
  return (HashEntry *)PL_DHashTableSearch(&mValuesHash, (void *)aName);
}

nsCommandParams::HashEntry*
nsCommandParams::GetOrMakeEntry(const char* aName, uint8_t entryType)
{
  HashEntry *foundEntry =
    (HashEntry *)PL_DHashTableSearch(&mValuesHash, (void *)aName);
  if (foundEntry) { // reuse existing entry
    foundEntry->Reset(entryType);
    return foundEntry;
  }

  foundEntry = static_cast<HashEntry*>
    (PL_DHashTableAdd(&mValuesHash, (void *)aName, fallible));
  if (!foundEntry) {
    return nullptr;
  }

  // Use placement new. Our ctor does not clobber keyHash, which is important.
  new (foundEntry) HashEntry(entryType, aName);
  return foundEntry;
}

PLDHashNumber
nsCommandParams::HashKey(PLDHashTable *aTable, const void *aKey)
{
  return HashString((const char *)aKey);
}

bool
nsCommandParams::HashMatchEntry(PLDHashTable *aTable,
                                const PLDHashEntryHdr *aEntry, const void *aKey)
{
  const char* keyString = (const char*)aKey;
  const HashEntry* thisEntry = static_cast<const HashEntry*>(aEntry);
  return thisEntry->mEntryName.Equals(keyString);
}

void
nsCommandParams::HashMoveEntry(PLDHashTable *aTable,
                               const PLDHashEntryHdr *aFrom,
                               PLDHashEntryHdr *aTo)
{
  const HashEntry* fromEntry = static_cast<const HashEntry*>(aFrom);
  HashEntry* toEntry = static_cast<HashEntry*>(aTo);

  new (toEntry) HashEntry(*fromEntry);

  fromEntry->~HashEntry();      // call dtor explicitly
}

void
nsCommandParams::HashClearEntry(PLDHashTable *aTable, PLDHashEntryHdr *aEntry)
{
  HashEntry* thisEntry = static_cast<HashEntry*>(aEntry);
  thisEntry->~HashEntry();      // call dtor explicitly
}


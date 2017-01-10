/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcom-config.h"
#include <new>
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
  : mValuesHash(&sHashOps, sizeof(HashEntry), 2)
{
}

nsCommandParams::~nsCommandParams()
{
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
nsCommandParams::SetBooleanValue(const char* aName, bool aValue)
{
  HashEntry* foundEntry = GetOrMakeEntry(aName, eBooleanType);
  if (!foundEntry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  foundEntry->mData.mBoolean = aValue;
  return NS_OK;
}

NS_IMETHODIMP
nsCommandParams::SetLongValue(const char* aName, int32_t aValue)
{
  HashEntry* foundEntry = GetOrMakeEntry(aName, eLongType);
  if (!foundEntry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  foundEntry->mData.mLong = aValue;
  return NS_OK;
}

NS_IMETHODIMP
nsCommandParams::SetDoubleValue(const char* aName, double aValue)
{
  HashEntry* foundEntry = GetOrMakeEntry(aName, eDoubleType);
  if (!foundEntry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  foundEntry->mData.mDouble = aValue;
  return NS_OK;
}

NS_IMETHODIMP
nsCommandParams::SetStringValue(const char* aName, const nsAString& aValue)
{
  HashEntry* foundEntry = GetOrMakeEntry(aName, eWStringType);
  if (!foundEntry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  foundEntry->mData.mString = new nsString(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsCommandParams::SetCStringValue(const char* aName, const char* aValue)
{
  HashEntry* foundEntry = GetOrMakeEntry(aName, eStringType);
  if (!foundEntry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  foundEntry->mData.mCString = new nsCString(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsCommandParams::SetISupportsValue(const char* aName, nsISupports* aValue)
{
  HashEntry* foundEntry = GetOrMakeEntry(aName, eISupportsType);
  if (!foundEntry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  foundEntry->mISupports = aValue; // addrefs
  return NS_OK;
}

NS_IMETHODIMP
nsCommandParams::RemoveValue(const char* aName)
{
  mValuesHash.Remove((void*)aName);
  return NS_OK;
}

nsCommandParams::HashEntry*
nsCommandParams::GetNamedEntry(const char* aName)
{
  return static_cast<HashEntry*>(mValuesHash.Search((void*)aName));
}

nsCommandParams::HashEntry*
nsCommandParams::GetOrMakeEntry(const char* aName, uint8_t aEntryType)
{
  auto foundEntry = static_cast<HashEntry*>(mValuesHash.Search((void*)aName));
  if (foundEntry) { // reuse existing entry
    foundEntry->Reset(aEntryType);
    return foundEntry;
  }

  foundEntry = static_cast<HashEntry*>(mValuesHash.Add((void*)aName, fallible));
  if (!foundEntry) {
    return nullptr;
  }

  // Use placement new. Our ctor does not clobber keyHash, which is important.
  new (foundEntry) HashEntry(aEntryType, aName);
  return foundEntry;
}

PLDHashNumber
nsCommandParams::HashKey(const void* aKey)
{
  return HashString((const char*)aKey);
}

bool
nsCommandParams::HashMatchEntry(const PLDHashEntryHdr* aEntry, const void* aKey)
{
  const char* keyString = (const char*)aKey;
  const HashEntry* thisEntry = static_cast<const HashEntry*>(aEntry);
  return thisEntry->mEntryName.Equals(keyString);
}

void
nsCommandParams::HashMoveEntry(PLDHashTable* aTable,
                               const PLDHashEntryHdr* aFrom,
                               PLDHashEntryHdr* aTo)
{
  const HashEntry* fromEntry = static_cast<const HashEntry*>(aFrom);
  HashEntry* toEntry = static_cast<HashEntry*>(aTo);

  new (toEntry) HashEntry(*fromEntry);

  fromEntry->~HashEntry();
}

void
nsCommandParams::HashClearEntry(PLDHashTable* aTable, PLDHashEntryHdr* aEntry)
{
  HashEntry* thisEntry = static_cast<HashEntry*>(aEntry);
  thisEntry->~HashEntry();
}

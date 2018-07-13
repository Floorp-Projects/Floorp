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

  ErrorResult error;
  *aRetVal = GetBool(aName, error);
  return error.StealNSResult();
}

bool
nsCommandParams::GetBool(const char* aName, ErrorResult& aRv) const
{
  MOZ_ASSERT(!aRv.Failed());

  HashEntry* foundEntry = GetNamedEntry(aName);
  if (foundEntry && foundEntry->mEntryType == eBooleanType) {
    return foundEntry->mData.mBoolean;
  }
  aRv.Throw(NS_ERROR_FAILURE);
  return false;
}

NS_IMETHODIMP
nsCommandParams::GetLongValue(const char* aName, int32_t* aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);

  ErrorResult error;
  *aRetVal = GetInt(aName, error);
  return error.StealNSResult();
}

int32_t
nsCommandParams::GetInt(const char* aName, ErrorResult& aRv) const
{
  MOZ_ASSERT(!aRv.Failed());

  HashEntry* foundEntry = GetNamedEntry(aName);
  if (foundEntry && foundEntry->mEntryType == eLongType) {
    return foundEntry->mData.mLong;
  }
  aRv.Throw(NS_ERROR_FAILURE);
  return 0;
}

NS_IMETHODIMP
nsCommandParams::GetDoubleValue(const char* aName, double* aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);

  ErrorResult error;
  *aRetVal = GetDouble(aName, error);
  return error.StealNSResult();
}

double
nsCommandParams::GetDouble(const char* aName, ErrorResult& aRv) const
{
  MOZ_ASSERT(!aRv.Failed());

  HashEntry* foundEntry = GetNamedEntry(aName);
  if (foundEntry && foundEntry->mEntryType == eDoubleType) {
    return foundEntry->mData.mDouble;
  }
  aRv.Throw(NS_ERROR_FAILURE);
  return 0.0;
}

NS_IMETHODIMP
nsCommandParams::GetStringValue(const char* aName, nsAString& aRetVal)
{
  return GetString(aName, aRetVal);
}

nsresult
nsCommandParams::GetString(const char* aName, nsAString& aRetVal) const
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
nsCommandParams::GetCStringValue(const char* aName, nsACString& aRetVal)
{
  return GetCString(aName, aRetVal);
}

nsresult
nsCommandParams::GetCString(const char* aName, nsACString& aRetVal) const
{
  HashEntry* foundEntry = GetNamedEntry(aName);
  if (foundEntry && foundEntry->mEntryType == eStringType) {
    NS_ASSERTION(foundEntry->mData.mCString, "Null string");
    aRetVal.Assign(*foundEntry->mData.mCString);
    return NS_OK;
  }
  aRetVal.Truncate();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCommandParams::GetISupportsValue(const char* aName, nsISupports** aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);

  ErrorResult error;
  nsCOMPtr<nsISupports> result = GetISupports(aName, error);
  if (result) {
    result.forget(aRetVal);
  } else {
    *aRetVal = nullptr;
  }
  return error.StealNSResult();
}

already_AddRefed<nsISupports>
nsCommandParams::GetISupports(const char* aName, ErrorResult& aRv) const
{
  MOZ_ASSERT(!aRv.Failed());

  HashEntry* foundEntry = GetNamedEntry(aName);
  if (foundEntry && foundEntry->mEntryType == eISupportsType) {
    nsCOMPtr<nsISupports> result = foundEntry->mISupports;
    return result.forget();
  }
  aRv.Throw(NS_ERROR_FAILURE);
  return nullptr;
}

NS_IMETHODIMP
nsCommandParams::SetBooleanValue(const char* aName, bool aValue)
{
  return SetBool(aName, aValue);
}

nsresult
nsCommandParams::SetBool(const char* aName, bool aValue)
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
  return SetInt(aName, aValue);
}

nsresult
nsCommandParams::SetInt(const char* aName, int32_t aValue)
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
  return SetDouble(aName, aValue);
}

nsresult
nsCommandParams::SetDouble(const char* aName, double aValue)
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
  return SetString(aName, aValue);
}

nsresult
nsCommandParams::SetString(const char* aName, const nsAString& aValue)
{
  HashEntry* foundEntry = GetOrMakeEntry(aName, eWStringType);
  if (!foundEntry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  foundEntry->mData.mString = new nsString(aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsCommandParams::SetCStringValue(const char* aName, const nsACString& aValue)
{
  return SetCString(aName, aValue);
}

nsresult
nsCommandParams::SetCString(const char* aName, const nsACString& aValue)
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
  return SetISupports(aName, aValue);
}

nsresult
nsCommandParams::SetISupports(const char* aName, nsISupports* aValue)
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
nsCommandParams::GetNamedEntry(const char* aName) const
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

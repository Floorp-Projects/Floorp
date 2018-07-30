/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCommandParams_h
#define nsCommandParams_h

#include "mozilla/ErrorResult.h"
#include "nsString.h"
#include "nsICommandParams.h"
#include "nsCOMPtr.h"
#include "PLDHashTable.h"

class nsCommandParams : public nsICommandParams
{
  typedef mozilla::ErrorResult ErrorResult;
  typedef mozilla::IgnoredErrorResult IgnoredErrorResult;

public:
  nsCommandParams();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICOMMANDPARAMS

  bool GetBool(const char* aName, ErrorResult& aRv) const;
  inline bool GetBool(const char* aName) const
  {
    IgnoredErrorResult error;
    return GetBool(aName, error);
  }
  int32_t GetInt(const char* aName, ErrorResult& aRv) const;
  inline int32_t GetInt(const char* aName) const
  {
    IgnoredErrorResult error;
    return GetInt(aName, error);
  }
  double GetDouble(const char* aName, ErrorResult& aRv) const;
  inline double GetDouble(const char* aName) const
  {
    IgnoredErrorResult error;
    return GetDouble(aName, error);
  }
  nsresult GetString(const char* aName, nsAString& aValue) const;
  nsresult GetCString(const char* aName, nsACString& aValue) const;
  already_AddRefed<nsISupports>
  GetISupports(const char* aName, ErrorResult& aRv) const;
  inline already_AddRefed<nsISupports> GetISupports(const char* aName) const
  {
    IgnoredErrorResult error;
    return GetISupports(aName, error);
  }

  nsresult SetBool(const char* aName, bool aValue);
  nsresult SetInt(const char* aName, int32_t aValue);
  nsresult SetDouble(const char* aName, double aValue);
  nsresult SetString(const char* aName, const nsAString& aValue);
  nsresult SetCString(const char* aName, const nsACString& aValue);
  nsresult SetISupports(const char* aName, nsISupports* aValue);

protected:
  virtual ~nsCommandParams();

  struct HashEntry : public PLDHashEntryHdr
  {
    nsCString mEntryName;

    uint8_t mEntryType;
    union
    {
      bool mBoolean;
      int32_t mLong;
      double mDouble;
      nsString* mString;
      nsCString* mCString;
    } mData;

    nsCOMPtr<nsISupports> mISupports;

    HashEntry(uint8_t aType, const char* aEntryName)
      : mEntryName(aEntryName)
      , mEntryType(aType)
      , mData()
    {
      Reset(mEntryType);
    }

    HashEntry(const HashEntry& aRHS)
      : mEntryType(aRHS.mEntryType)
    {
      Reset(mEntryType);
      switch (mEntryType) {
        case eBooleanType:
          mData.mBoolean = aRHS.mData.mBoolean;
          break;
        case eLongType:
          mData.mLong = aRHS.mData.mLong;
          break;
        case eDoubleType:
          mData.mDouble = aRHS.mData.mDouble;
          break;
        case eWStringType:
          NS_ASSERTION(aRHS.mData.mString, "Source entry has no string");
          mData.mString = new nsString(*aRHS.mData.mString);
          break;
        case eStringType:
          NS_ASSERTION(aRHS.mData.mCString, "Source entry has no string");
          mData.mCString = new nsCString(*aRHS.mData.mCString);
          break;
        case eISupportsType:
          mISupports = aRHS.mISupports.get();
          break;
        default:
          NS_ERROR("Unknown type");
      }
    }

    ~HashEntry() { Reset(eNoType); }

    void Reset(uint8_t aNewType)
    {
      switch (mEntryType) {
        case eNoType:
          break;
        case eBooleanType:
          mData.mBoolean = false;
          break;
        case eLongType:
          mData.mLong = 0;
          break;
        case eDoubleType:
          mData.mDouble = 0.0;
          break;
        case eWStringType:
          delete mData.mString;
          mData.mString = nullptr;
          break;
        case eISupportsType:
          mISupports = nullptr;
          break;
        case eStringType:
          delete mData.mCString;
          mData.mCString = nullptr;
          break;
        default:
          NS_ERROR("Unknown type");
      }
      mEntryType = aNewType;
    }
  };

  HashEntry* GetNamedEntry(const char* aName) const;
  HashEntry* GetOrMakeEntry(const char* aName, uint8_t aEntryType);

protected:
  static PLDHashNumber HashKey(const void* aKey);

  static bool HashMatchEntry(const PLDHashEntryHdr* aEntry, const void* aKey);

  static void HashMoveEntry(PLDHashTable* aTable, const PLDHashEntryHdr* aFrom,
                            PLDHashEntryHdr* aTo);

  static void HashClearEntry(PLDHashTable* aTable, PLDHashEntryHdr* aEntry);

  PLDHashTable mValuesHash;

  static const PLDHashTableOps sHashOps;
};

nsCommandParams*
nsICommandParams::AsCommandParams()
{
  return static_cast<nsCommandParams*>(this);
}

const nsCommandParams*
nsICommandParams::AsCommandParams() const
{
  return static_cast<const nsCommandParams*>(this);
}

#endif // nsCommandParams_h

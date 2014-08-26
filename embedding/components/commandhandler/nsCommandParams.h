/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef nsCommandParams_h__
#define nsCommandParams_h__

#include "nsString.h"
#include "nsICommandParams.h"
#include "nsCOMPtr.h"
#include "pldhash.h"



class nsCommandParams : public nsICommandParams
{
public:

                      nsCommandParams();
  
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOMMANDPARAMS

  nsresult            Init();
  
protected:
  virtual             ~nsCommandParams();

  struct HashEntry : public PLDHashEntryHdr
  {
    nsCString      mEntryName;

    uint8_t       mEntryType;
    union {
    
      bool                    mBoolean;
      int32_t                 mLong;
      double                  mDouble;
      nsString*               mString;
      nsCString*              mCString;
    } mData;

    nsCOMPtr<nsISupports>   mISupports;    
    
    HashEntry(uint8_t inType, const char * inEntryName)
    : mEntryName(inEntryName)
    , mEntryType(inType)
    {
      Reset(mEntryType);
    }

    HashEntry(const HashEntry& inRHS)
    : mEntryType(inRHS.mEntryType)
    {
      Reset(mEntryType);
      switch (mEntryType)
      {
        case eBooleanType:      mData.mBoolean = inRHS.mData.mBoolean;  break;
        case eLongType:         mData.mLong    = inRHS.mData.mLong;     break;
        case eDoubleType:       mData.mDouble  = inRHS.mData.mDouble;   break;
        case eWStringType:
          NS_ASSERTION(inRHS.mData.mString, "Source entry has no string");
          mData.mString = new nsString(*inRHS.mData.mString);
          break;      
        case eStringType:
          NS_ASSERTION(inRHS.mData.mCString, "Source entry has no string");
          mData.mCString = new nsCString(*inRHS.mData.mCString);
          break;      
        case eISupportsType:
          mISupports = inRHS.mISupports.get();    // additional addref
          break;
        default:
          NS_ERROR("Unknown type");
      }
    }
    
    ~HashEntry()
    {
      Reset(eNoType);
    }
    
    void Reset(uint8_t inNewType)
    {
      switch (mEntryType)
      {
        case eNoType:                                       break;
        case eBooleanType:      mData.mBoolean = false;  break;
        case eLongType:         mData.mLong = 0;            break;
        case eDoubleType:       mData.mDouble = 0.0;        break;
        case eWStringType:      delete mData.mString; mData.mString = nullptr;     break;
        case eISupportsType:    mISupports = nullptr;        break;    // clear the nsCOMPtr
        case eStringType:       delete mData.mCString; mData.mCString = nullptr;   break;
        default:
          NS_ERROR("Unknown type");
      }
      
      mEntryType = inNewType;
    }
    
  };


  HashEntry*          GetNamedEntry(const char * name);
  HashEntry*          GetIndexedEntry(int32_t index);
  uint32_t            GetNumEntries();

  HashEntry*          GetOrMakeEntry(const char * name, uint8_t entryType);

protected:

  static PLDHashNumber HashKey(PLDHashTable *table, const void *key);

  static bool          HashMatchEntry(PLDHashTable *table,
                                      const PLDHashEntryHdr *entry, const void *key);
  
  static void          HashMoveEntry(PLDHashTable *table, const PLDHashEntryHdr *from,
                                     PLDHashEntryHdr *to);
  
  static void          HashClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry);
  
                                  
protected:
  
  enum {
    eNumEntriesUnknown      = -1
  };

  // this is going to have to use a pldhash, because we need to
  // be able to iterate through entries, passing names back
  // to the caller, which means that we need to store the names
  // internally.
  
  PLDHashTable    mValuesHash;
  
  // enumerator data
  int32_t         mCurEntry;
  int32_t         mNumEntries;      // number of entries at start of enumeration (-1 indicates not known)
    
  static const PLDHashTableOps    sHashOps;
};


#endif // nsCommandParams_h__

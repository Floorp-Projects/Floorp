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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */


#ifndef nsCommandParams_h__
#define nsCommandParams_h__

#include "nsString.h"

#include "nsICommandParams.h"

#include "nsCOMPtr.h"
#include "nsHashtable.h"

#include "pldhash.h"

// {f7fa4581-238e-11d5-a73c-ab64fb68f2bc}
#define NS_COMMAND_PARAMS_CID \
 { 0xf7fa4581, 0x238e, 0x11d5, { 0xa7, 0x3c, 0xab, 0x64, 0xfb, 0x68, 0xf2, 0xbc } }

#define NS_COMMAND_PARAMS_CONTRACTID \
 "@mozilla.org/embedcomp/command-params;1"


class nsCommandParams : public nsICommandParams
{
public:

                      nsCommandParams();
  virtual             ~nsCommandParams();
  
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOMMANDPARAMS

  nsresult            Init();
  
protected:

  struct HashEntry : public PLDHashEntryHdr
  {
    nsString      mEntryName;

    PRUint8       mEntryType;
    union {
    
      PRBool                  mBoolean;
      PRInt32                 mLong;
      double                  mDouble;
    } mData;

    // put these outside the union to avoid clobbering other fields
    nsString*               mString;
    nsCOMPtr<nsISupports>   mISupports;    
    
    HashEntry(PRUint8 inType, const nsAReadableString& inEntryName)
    : mEntryType(inType)
    , mEntryName(inEntryName)
    , mString(nsnull)
    {
      mData.mDouble = 0.0;
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
          NS_ASSERTION(inRHS.mString, "Source entry has no string");
          mString = new nsString(*inRHS.mString);
          break;      
        case eISupportsType:
          mISupports = inRHS.mISupports.get();    // additional addref
          break;
        default:
          NS_ASSERTION(0, "Unknown type");
      }
    }
    
    ~HashEntry()
    {
      if (mEntryType == eWStringType)
        delete mString;
    }
    
    void Reset(PRUint8 inNewType)
    {
      switch (mEntryType)
      {
        case eNoType:                                       break;
        case eBooleanType:      mData.mBoolean = PR_FALSE;  break;
        case eLongType:         mData.mLong = 0;            break;
        case eDoubleType:       mData.mDouble = 0.0;        break;
        case eWStringType:      delete mString; mString = nsnull;     break;
        case eISupportsType:    mISupports = nsnull;        break;    // clear the nsCOMPtr
        default:
          NS_ASSERTION(0, "Unknown type");
      }
      
      mEntryType = inNewType;
    }
    
  };


  HashEntry*          GetNamedEntry(const nsAReadableString& name);
  HashEntry*          GetIndexedEntry(PRInt32 index);
  PRUint32            GetNumEntries();
  
  nsresult            GetOrMakeEntry(const nsAReadableString& name, PRUint8 entryType, HashEntry*& outEntry);
  
protected:

  static const void *       HashGetKey(PLDHashTable *table, PLDHashEntryHdr *entry);

  static PLDHashNumber      HashKey(PLDHashTable *table, const void *key);

  static PRBool             HashMatchEntry(PLDHashTable *table,
                                  const PLDHashEntryHdr *entry, const void *key);
  
  static void               HashMoveEntry(PLDHashTable *table, const PLDHashEntryHdr *from,
                                  PLDHashEntryHdr *to);
  
  static void               HashClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry);
  
                                  
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
  PRInt32         mCurEntry;
  PRInt32         mNumEntries;      // number of entries at start of enumeration (-1 indicates not known)
    
  static PLDHashTableOps    sHashOps;
};


#endif // nsCommandParams_h__

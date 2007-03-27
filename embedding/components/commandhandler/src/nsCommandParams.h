/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef nsCommandParams_h__
#define nsCommandParams_h__

#include "nsString.h"

#include "nsICommandParams.h"

#include "nsCOMPtr.h"
#include "nsHashtable.h"

#include "pldhash.h"



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
    nsCString      mEntryName;

    PRUint8       mEntryType;
    union {
    
      PRBool                  mBoolean;
      PRInt32                 mLong;
      double                  mDouble;
      nsString*               mString;
      nsCString*              mCString;
    } mData;

    nsCOMPtr<nsISupports>   mISupports;    
    
    HashEntry(PRUint8 inType, const char * inEntryName)
    : mEntryName(inEntryName)
    , mEntryType(inType)
    {
      memset(&mData, 0, sizeof(mData));
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
          NS_ASSERTION(0, "Unknown type");
      }
    }
    
    ~HashEntry()
    {
      if (mEntryType == eWStringType)
        delete mData.mString;
      else if (mEntryType == eStringType)
        delete mData.mCString;
    }
    
    void Reset(PRUint8 inNewType)
    {
      switch (mEntryType)
      {
        case eNoType:                                       break;
        case eBooleanType:      mData.mBoolean = PR_FALSE;  break;
        case eLongType:         mData.mLong = 0;            break;
        case eDoubleType:       mData.mDouble = 0.0;        break;
        case eWStringType:      delete mData.mString; mData.mString = nsnull;     break;
        case eISupportsType:    mISupports = nsnull;        break;    // clear the nsCOMPtr
        case eStringType:       delete mData.mCString; mData.mCString = nsnull;   break;
        default:
          NS_ASSERTION(0, "Unknown type");
      }
      
      mEntryType = inNewType;
    }
    
  };


  HashEntry*          GetNamedEntry(const char * name);
  HashEntry*          GetIndexedEntry(PRInt32 index);
  PRUint32            GetNumEntries();
  
  nsresult            GetOrMakeEntry(const char * name, PRUint8 entryType, HashEntry*& outEntry);
  
protected:

  static PLDHashNumber PR_CALLBACK HashKey(PLDHashTable *table, const void *key);

  static PRBool PR_CALLBACK        HashMatchEntry(PLDHashTable *table,
                                                  const PLDHashEntryHdr *entry, const void *key);
  
  static void PR_CALLBACK          HashMoveEntry(PLDHashTable *table, const PLDHashEntryHdr *from,
                                                 PLDHashEntryHdr *to);
  
  static void PR_CALLBACK          HashClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry);
  
                                  
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

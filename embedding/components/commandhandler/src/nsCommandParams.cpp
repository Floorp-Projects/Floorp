/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcom-config.h"
#include NEW_H    // for placement new
#include "nscore.h"
#include "nsCRT.h"

#include "nsCommandParams.h"
#include "mozilla/HashFunctions.h"

using namespace mozilla;

PLDHashTableOps nsCommandParams::sHashOps =
{
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    HashKey,
    HashMatchEntry,
    HashMoveEntry,
    HashClearEntry,
    PL_DHashFinalizeStub
};


NS_IMPL_ISUPPORTS1(nsCommandParams, nsICommandParams)

nsCommandParams::nsCommandParams()
: mCurEntry(0)
, mNumEntries(eNumEntriesUnknown)
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
  if (!PL_DHashTableInit(&mValuesHash, &sHashOps, (void *)this, sizeof(HashEntry), 4))
    return NS_ERROR_FAILURE;
    
  return NS_OK;
}

#if 0
#pragma mark -
#endif

/* short getValueType (in string name); */
NS_IMETHODIMP nsCommandParams::GetValueType(const char * name, PRInt16 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = eNoType;
  HashEntry*  foundEntry = GetNamedEntry(name);
  if (foundEntry)
  {
    *_retval = foundEntry->mEntryType;
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}

/* boolean getBooleanValue (in AString name); */
NS_IMETHODIMP nsCommandParams::GetBooleanValue(const char * name, bool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = false;

  HashEntry*  foundEntry = GetNamedEntry(name);
  if (foundEntry && foundEntry->mEntryType == eBooleanType)
  {
    *_retval = foundEntry->mData.mBoolean;
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}

/* long getLongValue (in AString name); */
NS_IMETHODIMP nsCommandParams::GetLongValue(const char * name, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = false;

  HashEntry*  foundEntry = GetNamedEntry(name);
  if (foundEntry && foundEntry->mEntryType == eLongType)
  {
    *_retval = foundEntry->mData.mLong;
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}

/* double getDoubleValue (in AString name); */
NS_IMETHODIMP nsCommandParams::GetDoubleValue(const char * name, double *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = 0.0;

  HashEntry*  foundEntry = GetNamedEntry(name);
  if (foundEntry && foundEntry->mEntryType == eDoubleType)
  {
    *_retval = foundEntry->mData.mDouble;
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}

/* AString getStringValue (in AString name); */
NS_IMETHODIMP nsCommandParams::GetStringValue(const char *name, nsAString & _retval)
{
  _retval.Truncate();
  HashEntry*  foundEntry = GetNamedEntry(name);
  if (foundEntry && foundEntry->mEntryType == eWStringType)
  {
    NS_ASSERTION(foundEntry->mData.mString, "Null string");
    _retval.Assign(*foundEntry->mData.mString);
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}

/* AString getStringValue (in AString name); */
NS_IMETHODIMP nsCommandParams::GetCStringValue(const char * name, char **_retval)
{
  HashEntry*  foundEntry = GetNamedEntry(name);
  if (foundEntry && foundEntry->mEntryType == eStringType)
  {
    NS_ASSERTION(foundEntry->mData.mCString, "Null string");
    *_retval = ToNewCString(*foundEntry->mData.mCString);
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}

/* nsISupports getISupportsValue (in AString name); */
NS_IMETHODIMP nsCommandParams::GetISupportsValue(const char * name, nsISupports **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  HashEntry*  foundEntry = GetNamedEntry(name);
  if (foundEntry && foundEntry->mEntryType == eISupportsType)
  {
    NS_IF_ADDREF(*_retval = foundEntry->mISupports.get());
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}

#if 0
#pragma mark -
#endif

/* void setBooleanValue (in AString name, in boolean value); */
NS_IMETHODIMP nsCommandParams::SetBooleanValue(const char * name, bool value)
{
  HashEntry*  foundEntry;
  GetOrMakeEntry(name, eBooleanType, foundEntry);
  if (!foundEntry) return NS_ERROR_OUT_OF_MEMORY;
  
  foundEntry->mData.mBoolean = value;
  
  return NS_OK;
}

/* void setLongValue (in AString name, in long value); */
NS_IMETHODIMP nsCommandParams::SetLongValue(const char * name, PRInt32 value)
{
  HashEntry*  foundEntry;
  GetOrMakeEntry(name, eLongType, foundEntry);
  if (!foundEntry) return NS_ERROR_OUT_OF_MEMORY;
  
  foundEntry->mData.mLong = value;
  return NS_OK;
}

/* void setDoubleValue (in AString name, in double value); */
NS_IMETHODIMP nsCommandParams::SetDoubleValue(const char * name, double value)
{
  HashEntry*  foundEntry;
  GetOrMakeEntry(name, eDoubleType, foundEntry);
  if (!foundEntry) return NS_ERROR_OUT_OF_MEMORY;
  
  foundEntry->mData.mDouble = value;
  return NS_OK;
}

/* void setStringValue (in AString name, in AString value); */
NS_IMETHODIMP nsCommandParams::SetStringValue(const char * name, const nsAString & value)
{
  HashEntry*  foundEntry;
  GetOrMakeEntry(name, eWStringType, foundEntry);
  if (!foundEntry) return NS_ERROR_OUT_OF_MEMORY;
  
  foundEntry->mData.mString = new nsString(value);
  return NS_OK;
}

/* void setCStringValue (in string name, in string value); */
NS_IMETHODIMP nsCommandParams::SetCStringValue(const char * name, const char * value)
{
  HashEntry*  foundEntry;
  GetOrMakeEntry(name, eStringType, foundEntry);
  if (!foundEntry)
    return NS_ERROR_OUT_OF_MEMORY;
  foundEntry->mData.mCString = new nsCString(value);
  return NS_OK;
}

/* void setISupportsValue (in AString name, in nsISupports value); */
NS_IMETHODIMP nsCommandParams::SetISupportsValue(const char * name, nsISupports *value)
{
  HashEntry*  foundEntry;
  GetOrMakeEntry(name, eISupportsType, foundEntry);
  if (!foundEntry) return NS_ERROR_OUT_OF_MEMORY;
  
  foundEntry->mISupports = value;   // addrefs
  return NS_OK;
}

/* void removeValue (in AString name); */
NS_IMETHODIMP
nsCommandParams::RemoveValue(const char * name)
{
  // PL_DHASH_REMOVE doesn't tell us if the entry was really removed, so we return
  // NS_OK unconditionally.
  (void)PL_DHashTableOperate(&mValuesHash, (void *)name, PL_DHASH_REMOVE);

  // inval the number of entries
  mNumEntries = eNumEntriesUnknown;
  return NS_OK;
}

#if 0
#pragma mark -
#endif

nsCommandParams::HashEntry*
nsCommandParams::GetNamedEntry(const char * name)
{
  HashEntry *foundEntry = (HashEntry *)PL_DHashTableOperate(&mValuesHash, (void *)name, PL_DHASH_LOOKUP);

  if (PL_DHASH_ENTRY_IS_BUSY(foundEntry))
    return foundEntry;
   
  return nsnull;
}


nsCommandParams::HashEntry*
nsCommandParams::GetIndexedEntry(PRInt32 index)
{
  HashEntry*  entry = reinterpret_cast<HashEntry*>(mValuesHash.entryStore);
  HashEntry*  limit = entry + PL_DHASH_TABLE_SIZE(&mValuesHash);
  PRUint32    entryCount = 0;
  
  do
  {  
    if (!PL_DHASH_ENTRY_IS_LIVE(entry))
      continue;

    if ((PRInt32)entryCount == index)
      return entry;
    
    entryCount ++;
  } while (++entry < limit);

  return nsnull;
}


PRUint32
nsCommandParams::GetNumEntries()
{
  HashEntry*  entry = reinterpret_cast<HashEntry*>(mValuesHash.entryStore);
  HashEntry*  limit = entry + PL_DHASH_TABLE_SIZE(&mValuesHash);
  PRUint32    entryCount = 0;
  
  do
  {  
    if (PL_DHASH_ENTRY_IS_LIVE(entry))
      entryCount ++;
  } while (++entry < limit);

  return entryCount;
}

nsresult
nsCommandParams::GetOrMakeEntry(const char * name, PRUint8 entryType, HashEntry*& outEntry)
{

  HashEntry *foundEntry = (HashEntry *)PL_DHashTableOperate(&mValuesHash, (void *)name, PL_DHASH_LOOKUP);
  if (PL_DHASH_ENTRY_IS_BUSY(foundEntry))   // reuse existing entry  
  {
    foundEntry->Reset(entryType);
    foundEntry->mEntryName.Assign(name);
    outEntry = foundEntry;
    return NS_OK;
  }

  foundEntry = (HashEntry *)PL_DHashTableOperate(&mValuesHash, (void *)name, PL_DHASH_ADD);
  if (!foundEntry) return NS_ERROR_OUT_OF_MEMORY;
  
  // placement new that sucker. Our ctor does not clobber keyHash, which is important.
  outEntry = new (foundEntry) HashEntry(entryType, name);  
  return NS_OK;
}

#if 0
#pragma mark -
#endif

PLDHashNumber
nsCommandParams::HashKey(PLDHashTable *table, const void *key)
{
  return HashString((const char *)key);
}

bool
nsCommandParams::HashMatchEntry(PLDHashTable *table,
                                const PLDHashEntryHdr *entry, const void *key)
{
  const char*   keyString = (const char*)key;
  const HashEntry*   thisEntry = static_cast<const HashEntry*>(entry);
  
  return thisEntry->mEntryName.Equals(keyString);
}

void
nsCommandParams::HashMoveEntry(PLDHashTable *table, const PLDHashEntryHdr *from,
                                PLDHashEntryHdr *to)
{
  const HashEntry*   fromEntry  = static_cast<const HashEntry*>(from);
  HashEntry*         toEntry    = static_cast<HashEntry*>(to);
  
  *toEntry = *fromEntry;
  // we leave from dirty, but that's OK
}

void
nsCommandParams::HashClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  HashEntry*    thisEntry = static_cast<HashEntry*>(entry);
  thisEntry->~HashEntry();      // call dtor explicitly
  memset(thisEntry, 0, sizeof(HashEntry));    // and clear out
}

#if 0
#pragma mark -
#endif

/* boolean hasMoreElements (); */
NS_IMETHODIMP
nsCommandParams::HasMoreElements(bool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  if (mNumEntries == eNumEntriesUnknown)
    mNumEntries = GetNumEntries();
  
  *_retval = mCurEntry < mNumEntries;
  return NS_OK;
}

/* void first (); */
NS_IMETHODIMP
nsCommandParams::First()
{
  mCurEntry = 0;
  return NS_OK;
}

/* AString getNext (); */
NS_IMETHODIMP
nsCommandParams::GetNext(char **_retval)
{
  HashEntry*    thisEntry = GetIndexedEntry(mCurEntry);
  if (!thisEntry)
    return NS_ERROR_FAILURE;
  
  *_retval = ToNewCString(thisEntry->mEntryName);
  mCurEntry++;
  return NS_OK;
}

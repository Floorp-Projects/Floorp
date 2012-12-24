/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "nsReadableUtils.h"
#include "pratom.h"
#include "prtypes.h"
#include "nsISupports.h"
#include "nsILocale.h"
#include "nsLocale.h"
#include "nsLocaleCID.h"
#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsMemory.h"
#include "nsCRT.h"

#define LOCALE_HASH_SIZE  0xFF


/* nsILocale */
NS_IMPL_THREADSAFE_ISUPPORTS1(nsLocale, nsILocale)

nsLocale::nsLocale(void)
:  fHashtable(nullptr), fCategoryCount(0)
{
  fHashtable = PL_NewHashTable(LOCALE_HASH_SIZE,&nsLocale::Hash_HashFunction,
                               &nsLocale::Hash_CompareNSString,
                               &nsLocale::Hash_CompareNSString,
                               nullptr, nullptr);
  NS_ASSERTION(fHashtable, "nsLocale: failed to allocate PR_Hashtable");
}

nsLocale::~nsLocale(void)
{
  // enumerate all the entries with a delete function to
  // safely delete all the keys and values
  PL_HashTableEnumerateEntries(fHashtable, &nsLocale::Hash_EnumerateDelete,
                               nullptr);

  PL_HashTableDestroy(fHashtable);
}

NS_IMETHODIMP
nsLocale::GetCategory(const nsAString& category, nsAString& result)
{
  const PRUnichar *value = (const PRUnichar*) 
    PL_HashTableLookup(fHashtable, PromiseFlatString(category).get());

  if (value)
  {
    result.Assign(value);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocale::AddCategory(const nsAString &category, const nsAString &value)
{
  PRUnichar* newKey = ToNewUnicode(category);
  if (!newKey)
    return NS_ERROR_OUT_OF_MEMORY;

  PRUnichar* newValue = ToNewUnicode(value);
  if (!newValue) {
    nsMemory::Free(newKey);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!PL_HashTableAdd(fHashtable, newKey, newValue)) {
    nsMemory::Free(newKey);
    nsMemory::Free(newValue);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}


PLHashNumber
nsLocale::Hash_HashFunction(const void* key)
{
  const PRUnichar* ptr = (const PRUnichar *) key;
  PLHashNumber hash;

  hash = (PLHashNumber)0;

  while (*ptr)
    hash += (PLHashNumber) *ptr++;

  return hash;
}


int
nsLocale::Hash_CompareNSString(const void* s1, const void* s2)
{
  if (s1 && s2) {
    return !NS_strcmp((const PRUnichar *) s1, (const PRUnichar *) s2);
  }
  if (s1) {        //s2 must have been null
    return -1;
  }
  if (s2) {        //s1 must have been null
      return 1;
  }
  return 0;
}


int
nsLocale::Hash_EnumerateDelete(PLHashEntry *he, int hashIndex, void *arg)
{
  // delete an entry
  nsMemory::Free((PRUnichar *)he->key);
  nsMemory::Free((PRUnichar *)he->value);

  return (HT_ENUMERATE_NEXT | HT_ENUMERATE_REMOVE);
}


/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "nsISupports.h"
#include "nsILocale.h"
#include "nsLocale.h"
#include "nsMemory.h"
#include "nsCRT.h"

#define LOCALE_HASH_SIZE  0xFF


/* nsILocale */
NS_IMPL_ISUPPORTS(nsLocale, nsILocale)

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
  const char16_t *value = (const char16_t*) 
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
  char16_t* newKey = ToNewUnicode(category);
  if (!newKey)
    return NS_ERROR_OUT_OF_MEMORY;

  char16_t* newValue = ToNewUnicode(value);
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
  const char16_t* ptr = (const char16_t *) key;
  PLHashNumber hash;

  hash = (PLHashNumber)0;

  while (*ptr)
    hash += (PLHashNumber) *ptr++;

  return hash;
}


int
nsLocale::Hash_CompareNSString(const void* s1, const void* s2)
{
  return !nsCRT::strcmp((const char16_t *) s1, (const char16_t *) s2);
}


int
nsLocale::Hash_EnumerateDelete(PLHashEntry *he, int hashIndex, void *arg)
{
  // delete an entry
  nsMemory::Free((char16_t *)he->key);
  nsMemory::Free((char16_t *)he->value);

  return (HT_ENUMERATE_NEXT | HT_ENUMERATE_REMOVE);
}


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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
:  fHashtable(nsnull), fCategoryCount(0)
{
  fHashtable = PL_NewHashTable(LOCALE_HASH_SIZE,&nsLocale::Hash_HashFunction,
                               &nsLocale::Hash_CompareNSString,
                               &nsLocale::Hash_CompareNSString, NULL, NULL);
  NS_ASSERTION(fHashtable, "nsLocale: failed to allocate PR_Hashtable");
}

nsLocale::nsLocale(nsLocale* other) : fHashtable(nsnull), fCategoryCount(0)
{
  fHashtable = PL_NewHashTable(LOCALE_HASH_SIZE,&nsLocale::Hash_HashFunction,
                               &nsLocale::Hash_CompareNSString,
                               &nsLocale::Hash_CompareNSString, NULL, NULL);
  NS_ASSERTION(fHashtable, "nsLocale: failed to allocate PR_Hashtable");

  //
  // enumerate Hash and copy
  //
  PL_HashTableEnumerateEntries(other->fHashtable, 
                               &nsLocale::Hash_EnumerateCopy, fHashtable);
}


nsLocale::nsLocale(const nsStringArray& categoryList, 
                   const nsStringArray& valueList) 
                  : fHashtable(NULL), fCategoryCount(0)
{
  PRInt32 i;
  PRUnichar* key, *value;

  fHashtable = PL_NewHashTable(LOCALE_HASH_SIZE,&nsLocale::Hash_HashFunction,
                               &nsLocale::Hash_CompareNSString,
                               &nsLocale::Hash_CompareNSString,
                               NULL, NULL);
  NS_ASSERTION(fHashtable, "nsLocale: failed to allocate PR_Hashtable");

  if (fHashtable)
  {
    for(i=0; i < categoryList.Count(); ++i) 
    {
      key = ToNewUnicode(*categoryList.StringAt(i));
      NS_ASSERTION(key, "nsLocale: failed to allocate internal hash key");
      value = ToNewUnicode(*valueList.StringAt(i));
      NS_ASSERTION(value, "nsLocale: failed to allocate internal hash value");
      if (!PL_HashTableAdd(fHashtable,key,value)) {
          nsMemory::Free(key);
          nsMemory::Free(value);
      }
    }
  }
}

nsLocale::~nsLocale(void)
{
  // enumerate all the entries with a delete function to
  // safely delete all the keys and values
  PL_HashTableEnumerateEntries(fHashtable, &nsLocale::Hash_EnumerateDelete,
                               NULL);

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


PRIntn
nsLocale::Hash_CompareNSString(const void* s1, const void* s2)
{
  return !nsCRT::strcmp((const PRUnichar *) s1, (const PRUnichar *) s2);
}


PRIntn
nsLocale::Hash_EnumerateDelete(PLHashEntry *he, PRIntn hashIndex, void *arg)
{
  // delete an entry
  nsMemory::Free((PRUnichar *)he->key);
  nsMemory::Free((PRUnichar *)he->value);

  return (HT_ENUMERATE_NEXT | HT_ENUMERATE_REMOVE);
}

PRIntn
nsLocale::Hash_EnumerateCopy(PLHashEntry *he, PRIntn hashIndex, void* arg)
{
  PRUnichar* newKey = ToNewUnicode(nsDependentString((PRUnichar *)he->key));
  if (!newKey) 
    return HT_ENUMERATE_STOP;

  PRUnichar* newValue = ToNewUnicode(nsDependentString((PRUnichar *)he->value));
  if (!newValue) {
    nsMemory::Free(newKey);
    return HT_ENUMERATE_STOP;
  }

  if (!PL_HashTableAdd((PLHashTable*)arg, newKey, newValue)) {
    nsMemory::Free(newKey);
    nsMemory::Free(newValue);
    return HT_ENUMERATE_STOP;
  }

  return (HT_ENUMERATE_NEXT);
}


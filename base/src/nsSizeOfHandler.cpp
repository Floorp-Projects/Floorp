/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsISizeOfHandler.h"
#include "plhash.h"

static NS_DEFINE_IID(kISizeOfHandlerIID, NS_ISIZEOF_HANDLER_IID);

class nsSizeOfHandler : public nsISizeOfHandler {
public:
  nsSizeOfHandler();
  virtual ~nsSizeOfHandler();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsISizeOfHandler
  NS_IMETHOD Add(size_t aSize);
  virtual PRBool HaveSeen(void* anObject);
  NS_IMETHOD GetSize(PRUint32& aResult);

protected:
  PRUint32 mTotalSize;
  PLHashTable* mTable;
};

static PLHashNumber
HashKey(void* key)
{
  return (PLHashNumber) key;
}

static PRIntn
CompareKeys(void* key1, void* key2)
{
  return key1 == key2;
}

nsSizeOfHandler::nsSizeOfHandler()
{
  NS_INIT_REFCNT();
  mTotalSize = 0;
  mTable = PL_NewHashTable(8, (PLHashFunction) HashKey,
                           (PLHashComparator) CompareKeys,
                           (PLHashComparator) nsnull,
                           nsnull, nsnull);
}

nsSizeOfHandler::~nsSizeOfHandler()
{
  if (nsnull != mTable) {
    PL_HashTableDestroy(mTable);
  }
}

NS_IMPL_ISUPPORTS(nsSizeOfHandler, kISizeOfHandlerIID)

NS_IMETHODIMP
nsSizeOfHandler::Add(size_t aSize)
{
  mTotalSize += aSize;
  return NS_OK;
}

PRBool
nsSizeOfHandler::HaveSeen(void* anObject)
{
  if (nsnull == mTable) {
    // When we run out of memory, HaveSeen returns PR_TRUE to stop
    // wasting time.
    return PR_TRUE;
  }
    
  if (nsnull != anObject) {
    PRInt32 hashCode = (PRInt32) anObject;
    PLHashEntry** hep = PL_HashTableRawLookup(mTable, hashCode, anObject);
    PLHashEntry* he = *hep;
    if (nsnull != he) {
      return PR_TRUE;
    }
    he = PL_HashTableRawAdd(mTable, hep, hashCode, anObject, anObject);
    if (nsnull == he) {
      // When we run out of memory, HaveSeen returns PR_TRUE to stop
      // wasting time.
      return PR_TRUE;
    }
    return PR_FALSE;
  }
  return PR_TRUE;
}

NS_IMETHODIMP
nsSizeOfHandler::GetSize(PRUint32& aResult)
{
  aResult = mTotalSize;
  return NS_OK;
}

NS_BASE nsresult
NS_NewSizeOfHandler(nsISizeOfHandler** aInstancePtrResult)
{
  nsISizeOfHandler *it = new nsSizeOfHandler();
  if (it == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kISizeOfHandlerIID, (void **) aInstancePtrResult);
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsScriptNameSpaceManager.h"
#include "prmem.h"

typedef struct {
  nsIID mCID;
  PRBool mIsConstructor;
} nsGlobalNameStruct;

nsScriptNameSpaceManager::nsScriptNameSpaceManager()
{
  NS_INIT_REFCNT();
  mGlobalNames = nsnull;
}

PRIntn 
nsScriptNameSpaceManager::RemoveNames(PLHashEntry *he, PRIntn i, void *arg)
{
  char *name = (char*)he->key;
  nsGlobalNameStruct* gn = (nsGlobalNameStruct*)he->value;

  nsCRT::free(name);
  PR_DELETE(gn);

  return HT_ENUMERATE_REMOVE;
}

nsScriptNameSpaceManager::~nsScriptNameSpaceManager()
{  
  if (nsnull != mGlobalNames) {
    PL_HashTableEnumerateEntries(mGlobalNames, RemoveNames, nsnull);
    PL_HashTableDestroy(mGlobalNames);
    mGlobalNames = nsnull;
  }
}

static NS_DEFINE_IID(kIScriptNameSpaceManagerIID, NS_ISCRIPTNAMESPACEMANAGER_IID);

NS_IMPL_ISUPPORTS(nsScriptNameSpaceManager, kIScriptNameSpaceManagerIID);

NS_IMETHODIMP 
nsScriptNameSpaceManager::RegisterGlobalName(const nsString& aName, 
                                             const nsIID& aCID,
                                             PRBool aIsConstructor)
{
  if (nsnull == mGlobalNames) {
    mGlobalNames = PL_NewHashTable(4, PL_HashString, PL_CompareStrings,
                                   PL_CompareValues, nsnull, nsnull);
  }
  
  char* name = aName.ToNewCString();
  nsGlobalNameStruct* gn = (nsGlobalNameStruct*)PR_NEW(nsGlobalNameStruct);
  if (nsnull == gn) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  gn->mCID = aCID;
  gn->mIsConstructor = aIsConstructor;

  PL_HashTableAdd(mGlobalNames, name, (void *)gn);

  return NS_OK;
}

NS_IMETHODIMP 
nsScriptNameSpaceManager::UnregisterGlobalName(const nsString& aName)
{
  if (nsnull != mGlobalNames) {
    char* name = aName.ToNewCString();
    PLHashNumber hn = PL_HashString(name);
    PLHashEntry** hep = PL_HashTableRawLookup(mGlobalNames,
                                              hn,
                                              name);
    PLHashEntry* entry = *hep;

    if (nsnull != entry) {  
      nsGlobalNameStruct* gn = (nsGlobalNameStruct*)entry->value;
      char* hname = (char*)entry->key;;

      delete gn;
      PL_HashTableRemove(mGlobalNames, name);
      nsCRT::free(hname);
    }

    nsCRT::free(name);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsScriptNameSpaceManager::LookupName(const nsString& aName, 
                                     PRBool aIsConstructor,
                                     nsIID& aCID)
{
  if (nsnull != mGlobalNames) {
    char* name = aName.ToNewCString();
    nsGlobalNameStruct* gn = (nsGlobalNameStruct*)PL_HashTableLookup(mGlobalNames, name);
    nsCRT::free(name);

    if ((nsnull != gn) && (gn->mIsConstructor == aIsConstructor)) {
      aCID = gn->mCID;
      return NS_OK;
    }
  }

  return NS_COMFALSE;
}

extern "C" NS_DOM nsresult 
NS_NewScriptNameSpaceManager(nsIScriptNameSpaceManager** aInstancePtr)
{
  if (nsnull == aInstancePtr) {  
    return NS_ERROR_NULL_POINTER;  
  }  

  *aInstancePtr = NULL;  
  
  nsScriptNameSpaceManager *manager = new nsScriptNameSpaceManager();
  if (nsnull == manager) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return manager->QueryInterface(kIScriptNameSpaceManagerIID, (void **)aInstancePtr);

}


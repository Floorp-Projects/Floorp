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

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsTArray.h"
#include "nsISimpleEnumerator.h"
#include "nsXPCOM.h"
#include "nsSupportsPrimitives.h"
#include "nsIComponentManager.h"

#include "nsCommandGroup.h"
#include "nsIControllerCommand.h"
#include "nsCRT.h"


class nsGroupsEnumerator : public nsISimpleEnumerator
{
public:
              nsGroupsEnumerator(nsHashtable& inHashTable);
  virtual     ~nsGroupsEnumerator();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR

protected:

  static bool HashEnum(nsHashKey *aKey, void *aData, void* aClosure);

  nsresult      Initialize();

protected:

  nsHashtable&  mHashTable;
  PRInt32       mIndex;
  char **       mGroupNames;        // array of pointers to PRUnichar* in the hash table
  bool          mInitted;
  
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsGroupsEnumerator, nsISimpleEnumerator)

nsGroupsEnumerator::nsGroupsEnumerator(nsHashtable& inHashTable)
: mHashTable(inHashTable)
, mIndex(-1)
, mGroupNames(nsnull)
, mInitted(PR_FALSE)
{
  /* member initializers and constructor code */
}

nsGroupsEnumerator::~nsGroupsEnumerator()
{
  delete [] mGroupNames;    // ok on null pointer
}

/* boolean hasMoreElements (); */
NS_IMETHODIMP
nsGroupsEnumerator::HasMoreElements(bool *_retval)
{
  nsresult  rv = NS_OK;
  
  NS_ENSURE_ARG_POINTER(_retval);

  if (!mInitted) {
    rv = Initialize();
    if (NS_FAILED(rv)) return rv;
  }
  
  *_retval = (mIndex < mHashTable.Count() - 1); 
  return NS_OK;
}

/* nsISupports getNext (); */
NS_IMETHODIMP
nsGroupsEnumerator::GetNext(nsISupports **_retval)
{
  nsresult  rv = NS_OK;
  
  NS_ENSURE_ARG_POINTER(_retval);

  if (!mInitted) {
    rv = Initialize();
    if (NS_FAILED(rv)) return rv;
  }
  
  mIndex ++;
  if (mIndex >= mHashTable.Count())
    return NS_ERROR_FAILURE;

  char *thisGroupName = mGroupNames[mIndex];
  
  nsCOMPtr<nsISupportsCString> supportsString = do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  supportsString->SetData(nsDependentCString(thisGroupName));
  return CallQueryInterface(supportsString, _retval);
}

/* static */
/* return false to stop */
bool
nsGroupsEnumerator::HashEnum(nsHashKey *aKey, void *aData, void* aClosure)
{
  nsGroupsEnumerator*   groupsEnum = reinterpret_cast<nsGroupsEnumerator *>(aClosure);
  nsCStringKey*         stringKey = static_cast<nsCStringKey*>(aKey);
  
  groupsEnum->mGroupNames[groupsEnum->mIndex] = (char*)stringKey->GetString();
  groupsEnum->mIndex ++;
  return PR_TRUE;
}

nsresult
nsGroupsEnumerator::Initialize()
{
  if (mInitted) return NS_OK;
  
  mGroupNames = new char*[mHashTable.Count()];
  if (!mGroupNames) return NS_ERROR_OUT_OF_MEMORY;
  
  mIndex = 0; 
  mHashTable.Enumerate(HashEnum, (void*)this);

  mIndex = -1;
  mInitted = PR_TRUE;
  return NS_OK;
}

#if 0
#pragma mark -
#endif

class nsNamedGroupEnumerator : public nsISimpleEnumerator
{
public:
              nsNamedGroupEnumerator(nsTArray<char*>* inArray);
  virtual     ~nsNamedGroupEnumerator();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR

protected:

  nsTArray<char*>* mGroupArray;
  PRInt32          mIndex;
  
};

nsNamedGroupEnumerator::nsNamedGroupEnumerator(nsTArray<char*>* inArray)
: mGroupArray(inArray)
, mIndex(-1)
{
}

nsNamedGroupEnumerator::~nsNamedGroupEnumerator()
{
}

NS_IMPL_ISUPPORTS1(nsNamedGroupEnumerator, nsISimpleEnumerator)

/* boolean hasMoreElements (); */
NS_IMETHODIMP
nsNamedGroupEnumerator::HasMoreElements(bool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  
  PRInt32 arrayLen = mGroupArray ? mGroupArray->Length() : 0;
  *_retval = (mIndex < arrayLen - 1); 
  return NS_OK;
}

/* nsISupports getNext (); */
NS_IMETHODIMP
nsNamedGroupEnumerator::GetNext(nsISupports **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  if (!mGroupArray)
    return NS_ERROR_FAILURE;

  mIndex ++;
  if (mIndex >= PRInt32(mGroupArray->Length()))
    return NS_ERROR_FAILURE;
    
  PRUnichar   *thisGroupName = (PRUnichar*)mGroupArray->ElementAt(mIndex);
  NS_ASSERTION(thisGroupName, "Bad Element in mGroupArray");
  
  nsresult rv;
  nsCOMPtr<nsISupportsString> supportsString = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  supportsString->SetData(nsDependentString(thisGroupName));
  return CallQueryInterface(supportsString, _retval);
}

#if 0
#pragma mark -
#endif


/* Implementation file */
NS_IMPL_ISUPPORTS1(nsControllerCommandGroup, nsIControllerCommandGroup)

nsControllerCommandGroup::nsControllerCommandGroup()
{
}

nsControllerCommandGroup::~nsControllerCommandGroup()
{
  ClearGroupsHash();
}

void
nsControllerCommandGroup::ClearGroupsHash()
{
    mGroupsHash.Reset(ClearEnumerator, (void *)this);
}

#if 0
#pragma mark -
#endif

/* void addCommandToGroup (in DOMString aCommand, in DOMString aGroup); */
NS_IMETHODIMP
nsControllerCommandGroup::AddCommandToGroup(const char * aCommand, const char *aGroup)
{
  nsCStringKey   groupKey(aGroup);  
  nsTArray<char*>* commandList;
  if ((commandList = (nsTArray<char*> *)mGroupsHash.Get(&groupKey)) == nsnull)
  {
    // make this list
    commandList = new nsAutoTArray<char*, 8>;
    mGroupsHash.Put(&groupKey, (void *)commandList);
  }
  // add the command to the list. Note that we're not checking for duplicates here
  char* commandString = NS_strdup(aCommand); // we store allocated PRUnichar* in the array
  if (!commandString) return NS_ERROR_OUT_OF_MEMORY;
  
#ifdef DEBUG
  char** appended =
#endif
    commandList->AppendElement(commandString);
  NS_ASSERTION(appended, "Append failed");

  return NS_OK;
}

/* void removeCommandFromGroup (in DOMString aCommand, in DOMString aGroup); */
NS_IMETHODIMP
nsControllerCommandGroup::RemoveCommandFromGroup(const char * aCommand, const char * aGroup)
{
  nsCStringKey   groupKey(aGroup);
  nsTArray<char*>* commandList = (nsTArray<char*> *)mGroupsHash.Get(&groupKey);
  if (!commandList) return NS_OK;     // no group

  PRUint32 numEntries = commandList->Length();
  for (PRUint32 i = 0; i < numEntries; i ++)
  {
    char*  commandString = commandList->ElementAt(i);
    if (!nsCRT::strcmp(aCommand,commandString))
    {
      commandList->RemoveElementAt(i);
      nsMemory::Free(commandString);
      break;
    }
  }

  return NS_OK;
}

/* boolean isCommandInGroup (in DOMString aCommand, in DOMString aGroup); */
NS_IMETHODIMP
nsControllerCommandGroup::IsCommandInGroup(const char * aCommand, const char * aGroup, bool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  
  nsCStringKey   groupKey(aGroup);
  nsTArray<char*>* commandList = (nsTArray<char*> *)mGroupsHash.Get(&groupKey);
  if (!commandList) return NS_OK;     // no group
  
  PRUint32 numEntries = commandList->Length();
  for (PRUint32 i = 0; i < numEntries; i ++)
  {
    char*  commandString = commandList->ElementAt(i);
    if (!nsCRT::strcmp(aCommand,commandString))
    {
      *_retval = PR_TRUE;
      break;
    }
  }
  return NS_OK;
}

/* nsISimpleEnumerator getGroupsEnumerator (); */
NS_IMETHODIMP
nsControllerCommandGroup::GetGroupsEnumerator(nsISimpleEnumerator **_retval)
{
  nsGroupsEnumerator*   groupsEnum = new nsGroupsEnumerator(mGroupsHash);
  if (!groupsEnum) return NS_ERROR_OUT_OF_MEMORY;

  return groupsEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)_retval);
}

/* nsISimpleEnumerator getEnumeratorForGroup (in DOMString aGroup); */
NS_IMETHODIMP
nsControllerCommandGroup::GetEnumeratorForGroup(const char * aGroup, nsISimpleEnumerator **_retval)
{
  nsCStringKey   groupKey(aGroup);  
  nsTArray<char*>* commandList = (nsTArray<char*> *)mGroupsHash.Get(&groupKey); // may be null

  nsNamedGroupEnumerator*   theGroupEnum = new nsNamedGroupEnumerator(commandList);
  if (!theGroupEnum) return NS_ERROR_OUT_OF_MEMORY;

  return theGroupEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)_retval);
}

#if 0
#pragma mark -
#endif
 
bool nsControllerCommandGroup::ClearEnumerator(nsHashKey *aKey, void *aData, void* closure)
{
  nsTArray<char*>* commandList = (nsTArray<char*> *)aData;
  if (commandList)
  {  
    PRUint32 numEntries = commandList->Length();
    for (PRUint32 i = 0; i < numEntries; i ++)
    {
      char* commandString = commandList->ElementAt(i);
      nsMemory::Free(commandString);
    }
    
    delete commandList;
  }

  return PR_TRUE;
}

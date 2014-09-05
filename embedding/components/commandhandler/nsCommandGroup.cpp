/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  explicit nsGroupsEnumerator(nsControllerCommandGroup::GroupsHashtable &inHashTable);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR

protected:
  virtual ~nsGroupsEnumerator();

  static PLDHashOperator HashEnum(const nsACString &aKey, nsTArray<nsCString> *aData, void *aClosure);
  nsresult Initialize();

protected:

  nsControllerCommandGroup::GroupsHashtable &mHashTable;
  int32_t mIndex;
  char **mGroupNames;        // array of pointers to char16_t* in the hash table
  bool mInitted;
  
};

/* Implementation file */
NS_IMPL_ISUPPORTS(nsGroupsEnumerator, nsISimpleEnumerator)

nsGroupsEnumerator::nsGroupsEnumerator(nsControllerCommandGroup::GroupsHashtable &inHashTable)
: mHashTable(inHashTable)
, mIndex(-1)
, mGroupNames(nullptr)
, mInitted(false)
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
  nsresult rv = NS_OK;
  
  NS_ENSURE_ARG_POINTER(_retval);

  if (!mInitted) {
    rv = Initialize();
    if (NS_FAILED(rv)) return rv;
  }
  
  *_retval = (mIndex < static_cast<int32_t>(mHashTable.Count()) - 1);
  return NS_OK;
}

/* nsISupports getNext (); */
NS_IMETHODIMP
nsGroupsEnumerator::GetNext(nsISupports **_retval)
{
  nsresult rv = NS_OK;
  
  NS_ENSURE_ARG_POINTER(_retval);

  if (!mInitted) {
    rv = Initialize();
    if (NS_FAILED(rv)) return rv;
  }
  
  mIndex ++;
  if (mIndex >= static_cast<int32_t>(mHashTable.Count()))
    return NS_ERROR_FAILURE;

  char *thisGroupName = mGroupNames[mIndex];
  
  nsCOMPtr<nsISupportsCString> supportsString = do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  supportsString->SetData(nsDependentCString(thisGroupName));
  return CallQueryInterface(supportsString, _retval);
}

/* static */
/* return false to stop */
PLDHashOperator
nsGroupsEnumerator::HashEnum(const nsACString &aKey, nsTArray<nsCString> *aData, void *aClosure)
{
  nsGroupsEnumerator *groupsEnum = static_cast<nsGroupsEnumerator*>(aClosure);
  groupsEnum->mGroupNames[groupsEnum->mIndex] = (char*)aKey.Data();
  groupsEnum->mIndex++;
  return PL_DHASH_NEXT;
}

nsresult
nsGroupsEnumerator::Initialize()
{
  if (mInitted) return NS_OK;
  
  mGroupNames = new char*[mHashTable.Count()];
  if (!mGroupNames) return NS_ERROR_OUT_OF_MEMORY;
  
  mIndex = 0; 
  mHashTable.EnumerateRead(HashEnum, this);

  mIndex = -1;
  mInitted = true;
  return NS_OK;
}

#if 0
#pragma mark -
#endif

class nsNamedGroupEnumerator : public nsISimpleEnumerator
{
public:
  explicit nsNamedGroupEnumerator(nsTArray<nsCString> *inArray);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR

protected:
  virtual ~nsNamedGroupEnumerator();

  nsTArray<nsCString> *mGroupArray;
  int32_t mIndex;
};

nsNamedGroupEnumerator::nsNamedGroupEnumerator(nsTArray<nsCString> *inArray)
: mGroupArray(inArray)
, mIndex(-1)
{
}

nsNamedGroupEnumerator::~nsNamedGroupEnumerator()
{
}

NS_IMPL_ISUPPORTS(nsNamedGroupEnumerator, nsISimpleEnumerator)

/* boolean hasMoreElements (); */
NS_IMETHODIMP
nsNamedGroupEnumerator::HasMoreElements(bool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  
  int32_t arrayLen = mGroupArray ? mGroupArray->Length() : 0;
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

  mIndex++;
  if (mIndex >= int32_t(mGroupArray->Length()))
    return NS_ERROR_FAILURE;
    
  const nsCString& thisGroupName = mGroupArray->ElementAt(mIndex);
  
  nsresult rv;
  nsCOMPtr<nsISupportsCString> supportsString = do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  supportsString->SetData(thisGroupName);
  return CallQueryInterface(supportsString, _retval);
}

#if 0
#pragma mark -
#endif


/* Implementation file */
NS_IMPL_ISUPPORTS(nsControllerCommandGroup, nsIControllerCommandGroup)

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
  mGroupsHash.Clear();
}

#if 0
#pragma mark -
#endif

/* void addCommandToGroup (in DOMString aCommand, in DOMString aGroup); */
NS_IMETHODIMP
nsControllerCommandGroup::AddCommandToGroup(const char *aCommand, const char *aGroup)
{
  nsDependentCString groupKey(aGroup);
  nsTArray<nsCString> *commandList;
  if ((commandList = mGroupsHash.Get(groupKey)) == nullptr)
  {
    // make this list
    commandList = new nsAutoTArray<nsCString, 8>;
    mGroupsHash.Put(groupKey, commandList);
  }

#ifdef DEBUG
  nsCString *appended =
#endif
  commandList->AppendElement(aCommand);
  NS_ASSERTION(appended, "Append failed");

  return NS_OK;
}

/* void removeCommandFromGroup (in DOMString aCommand, in DOMString aGroup); */
NS_IMETHODIMP
nsControllerCommandGroup::RemoveCommandFromGroup(const char *aCommand, const char *aGroup)
{
  nsDependentCString groupKey(aGroup);
  nsTArray<nsCString> *commandList = mGroupsHash.Get(groupKey);
  if (!commandList) return NS_OK; // no group

  uint32_t numEntries = commandList->Length();
  for (uint32_t i = 0; i < numEntries; i++)
  {
    nsCString commandString = commandList->ElementAt(i);
    if (nsDependentCString(aCommand) != commandString)
    {
      commandList->RemoveElementAt(i);
      break;
    }
  }
  return NS_OK;
}

/* boolean isCommandInGroup (in DOMString aCommand, in DOMString aGroup); */
NS_IMETHODIMP
nsControllerCommandGroup::IsCommandInGroup(const char *aCommand, const char *aGroup, bool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = false;
  
  nsDependentCString groupKey(aGroup);
  nsTArray<nsCString> *commandList = mGroupsHash.Get(groupKey);
  if (!commandList) return NS_OK; // no group
  
  uint32_t numEntries = commandList->Length();
  for (uint32_t i = 0; i < numEntries; i++)
  {
    nsCString commandString = commandList->ElementAt(i);
    if (nsDependentCString(aCommand) != commandString)
    {
      *_retval = true;
      break;
    }
  }
  return NS_OK;
}

/* nsISimpleEnumerator getGroupsEnumerator (); */
NS_IMETHODIMP
nsControllerCommandGroup::GetGroupsEnumerator(nsISimpleEnumerator **_retval)
{
  nsGroupsEnumerator *groupsEnum = new nsGroupsEnumerator(mGroupsHash);
  if (!groupsEnum) return NS_ERROR_OUT_OF_MEMORY;

  return groupsEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)_retval);
}

/* nsISimpleEnumerator getEnumeratorForGroup (in DOMString aGroup); */
NS_IMETHODIMP
nsControllerCommandGroup::GetEnumeratorForGroup(const char *aGroup, nsISimpleEnumerator **_retval)
{
  nsDependentCString groupKey(aGroup);
  nsTArray<nsCString> *commandList = mGroupsHash.Get(groupKey); // may be null

  nsNamedGroupEnumerator *theGroupEnum = new nsNamedGroupEnumerator(commandList);
  if (!theGroupEnum) return NS_ERROR_OUT_OF_MEMORY;

  return theGroupEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)_retval);
}

#if 0
#pragma mark -
#endif

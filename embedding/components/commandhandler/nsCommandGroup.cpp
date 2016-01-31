/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
  explicit nsGroupsEnumerator(
    nsControllerCommandGroup::GroupsHashtable& aInHashTable);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR

protected:
  virtual ~nsGroupsEnumerator();

  nsresult Initialize();

protected:
  nsControllerCommandGroup::GroupsHashtable& mHashTable;
  int32_t mIndex;
  const char** mGroupNames;  // array of pointers to char16_t* in the hash table
  bool mInitted;
};

/* Implementation file */
NS_IMPL_ISUPPORTS(nsGroupsEnumerator, nsISimpleEnumerator)

nsGroupsEnumerator::nsGroupsEnumerator(
      nsControllerCommandGroup::GroupsHashtable& aInHashTable)
  : mHashTable(aInHashTable)
  , mIndex(-1)
  , mGroupNames(nullptr)
  , mInitted(false)
{
}

nsGroupsEnumerator::~nsGroupsEnumerator()
{
  delete[] mGroupNames;
}

NS_IMETHODIMP
nsGroupsEnumerator::HasMoreElements(bool* aResult)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG_POINTER(aResult);

  if (!mInitted) {
    rv = Initialize();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  *aResult = (mIndex < static_cast<int32_t>(mHashTable.Count()) - 1);
  return NS_OK;
}

NS_IMETHODIMP
nsGroupsEnumerator::GetNext(nsISupports** aResult)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG_POINTER(aResult);

  if (!mInitted) {
    rv = Initialize();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  mIndex++;
  if (mIndex >= static_cast<int32_t>(mHashTable.Count())) {
    return NS_ERROR_FAILURE;
  }

  const char* thisGroupName = mGroupNames[mIndex];

  nsCOMPtr<nsISupportsCString> supportsString =
    do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  supportsString->SetData(nsDependentCString(thisGroupName));
  return CallQueryInterface(supportsString, aResult);
}

nsresult
nsGroupsEnumerator::Initialize()
{
  if (mInitted) {
    return NS_OK;
  }

  mGroupNames = new const char*[mHashTable.Count()];
  if (!mGroupNames) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mIndex = 0;
  for (auto iter = mHashTable.Iter(); !iter.Done(); iter.Next()) {
    mGroupNames[mIndex] = iter.Key().Data();
    mIndex++;
  }

  mIndex = -1;
  mInitted = true;
  return NS_OK;
}

class nsNamedGroupEnumerator : public nsISimpleEnumerator
{
public:
  explicit nsNamedGroupEnumerator(nsTArray<nsCString>* aInArray);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR

protected:
  virtual ~nsNamedGroupEnumerator();

  nsTArray<nsCString>* mGroupArray;
  int32_t mIndex;
};

nsNamedGroupEnumerator::nsNamedGroupEnumerator(nsTArray<nsCString>* aInArray)
  : mGroupArray(aInArray)
  , mIndex(-1)
{
}

nsNamedGroupEnumerator::~nsNamedGroupEnumerator()
{
}

NS_IMPL_ISUPPORTS(nsNamedGroupEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
nsNamedGroupEnumerator::HasMoreElements(bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  int32_t arrayLen = mGroupArray ? mGroupArray->Length() : 0;
  *aResult = (mIndex < arrayLen - 1);
  return NS_OK;
}

NS_IMETHODIMP
nsNamedGroupEnumerator::GetNext(nsISupports** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  if (!mGroupArray) {
    return NS_ERROR_FAILURE;
  }

  mIndex++;
  if (mIndex >= int32_t(mGroupArray->Length())) {
    return NS_ERROR_FAILURE;
  }

  const nsCString& thisGroupName = mGroupArray->ElementAt(mIndex);

  nsresult rv;
  nsCOMPtr<nsISupportsCString> supportsString =
    do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  supportsString->SetData(thisGroupName);
  return CallQueryInterface(supportsString, aResult);
}

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

NS_IMETHODIMP
nsControllerCommandGroup::AddCommandToGroup(const char* aCommand,
                                            const char* aGroup)
{
  nsDependentCString groupKey(aGroup);
  nsTArray<nsCString>* commandList = mGroupsHash.Get(groupKey);
  if (!commandList) {
    // make this list
    commandList = new AutoTArray<nsCString, 8>;
    mGroupsHash.Put(groupKey, commandList);
  }

#ifdef DEBUG
  nsCString* appended =
#endif
  commandList->AppendElement(aCommand);
  NS_ASSERTION(appended, "Append failed");

  return NS_OK;
}

NS_IMETHODIMP
nsControllerCommandGroup::RemoveCommandFromGroup(const char* aCommand,
                                                 const char* aGroup)
{
  nsDependentCString groupKey(aGroup);
  nsTArray<nsCString>* commandList = mGroupsHash.Get(groupKey);
  if (!commandList) {
    return NS_OK; // no group
  }

  uint32_t numEntries = commandList->Length();
  for (uint32_t i = 0; i < numEntries; i++) {
    nsCString commandString = commandList->ElementAt(i);
    if (nsDependentCString(aCommand) != commandString) {
      commandList->RemoveElementAt(i);
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsControllerCommandGroup::IsCommandInGroup(const char* aCommand,
                                           const char* aGroup, bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = false;

  nsDependentCString groupKey(aGroup);
  nsTArray<nsCString>* commandList = mGroupsHash.Get(groupKey);
  if (!commandList) {
    return NS_OK; // no group
  }

  uint32_t numEntries = commandList->Length();
  for (uint32_t i = 0; i < numEntries; i++) {
    nsCString commandString = commandList->ElementAt(i);
    if (nsDependentCString(aCommand) != commandString) {
      *aResult = true;
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsControllerCommandGroup::GetGroupsEnumerator(nsISimpleEnumerator** aResult)
{
  RefPtr<nsGroupsEnumerator> groupsEnum = new nsGroupsEnumerator(mGroupsHash);

  groupsEnum.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsControllerCommandGroup::GetEnumeratorForGroup(const char* aGroup,
                                                nsISimpleEnumerator** aResult)
{
  nsDependentCString groupKey(aGroup);
  nsTArray<nsCString>* commandList = mGroupsHash.Get(groupKey); // may be null

  RefPtr<nsNamedGroupEnumerator> theGroupEnum =
    new nsNamedGroupEnumerator(commandList);

  theGroupEnum.forget(aResult);
  return NS_OK;
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSHTransaction.h"
#include "nsISHEntry.h"

nsSHTransaction::nsSHTransaction()
  : mPersist(true)
  , mPrev(nullptr)
{
}

nsSHTransaction::~nsSHTransaction()
{
}

NS_IMPL_ADDREF(nsSHTransaction)
NS_IMPL_RELEASE(nsSHTransaction)

NS_INTERFACE_MAP_BEGIN(nsSHTransaction)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISHTransaction)
  NS_INTERFACE_MAP_ENTRY(nsISHTransaction)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsSHTransaction::Create(nsISHEntry* aSHEntry, nsISHTransaction* aPrev)
{
  SetSHEntry(aSHEntry);
  if (aPrev) {
    aPrev->SetNext(this);
  }

  SetPrev(aPrev);
  return NS_OK;
}

NS_IMETHODIMP
nsSHTransaction::GetSHEntry(nsISHEntry** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mSHEntry;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSHTransaction::SetSHEntry(nsISHEntry* aSHEntry)
{
  mSHEntry = aSHEntry;
  return NS_OK;
}

NS_IMETHODIMP
nsSHTransaction::GetNext(nsISHTransaction** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mNext;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSHTransaction::SetNext(nsISHTransaction* aNext)
{
  if (aNext) {
    NS_ENSURE_SUCCESS(aNext->SetPrev(this), NS_ERROR_FAILURE);
  }

  mNext = aNext;
  return NS_OK;
}

NS_IMETHODIMP
nsSHTransaction::SetPrev(nsISHTransaction* aPrev)
{
  /* This is weak reference to parent. Do not Addref it */
  mPrev = aPrev;
  return NS_OK;
}

nsresult
nsSHTransaction::GetPrev(nsISHTransaction** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mPrev;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSHTransaction::SetPersist(bool aPersist)
{
  mPersist = aPersist;
  return NS_OK;
}

NS_IMETHODIMP
nsSHTransaction::GetPersist(bool* aPersist)
{
  NS_ENSURE_ARG_POINTER(aPersist);

  *aPersist = mPersist;
  return NS_OK;
}

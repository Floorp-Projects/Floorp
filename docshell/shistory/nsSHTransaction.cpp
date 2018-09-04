/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSHTransaction.h"
#include "nsISHEntry.h"

nsSHTransaction::nsSHTransaction(nsISHEntry* aSHEntry, bool aPersist)
  : mSHEntry(aSHEntry)
  , mPersist(aPersist)
{
  MOZ_ASSERT(aSHEntry);
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
  MOZ_ASSERT(aSHEntry);
  mSHEntry = aSHEntry;
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

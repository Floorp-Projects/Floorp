/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleHyperLink.h"

#include "Accessible-inl.h"

using namespace mozilla::a11y;

NS_IMETHODIMP
xpcAccessibleHyperLink::GetStartIndex(int32_t* aStartIndex)
{
  NS_ENSURE_ARG_POINTER(aStartIndex);
  *aStartIndex = 0;

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  *aStartIndex = static_cast<Accessible*>(this)->StartOffset();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperLink::GetEndIndex(int32_t* aEndIndex)
{
  NS_ENSURE_ARG_POINTER(aEndIndex);
  *aEndIndex = 0;

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  *aEndIndex = static_cast<Accessible*>(this)->EndOffset();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperLink::GetAnchorCount(int32_t* aAnchorCount)
{
  NS_ENSURE_ARG_POINTER(aAnchorCount);
  *aAnchorCount = 0;

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  *aAnchorCount = static_cast<Accessible*>(this)->AnchorCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperLink::GetURI(int32_t aIndex, nsIURI** aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);

  Accessible* thisAcc = static_cast<Accessible*>(this);
  if (thisAcc->IsDefunct())
    return NS_ERROR_FAILURE;

  if (aIndex < 0 || aIndex >= static_cast<int32_t>(thisAcc->AnchorCount()))
    return NS_ERROR_INVALID_ARG;

  nsRefPtr<nsIURI>(thisAcc->AnchorURIAt(aIndex)).forget(aURI);
  return NS_OK;
}


NS_IMETHODIMP
xpcAccessibleHyperLink::GetAnchor(int32_t aIndex, nsIAccessible** aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nullptr;

  Accessible* thisAcc = static_cast<Accessible*>(this);
  if (thisAcc->IsDefunct())
    return NS_ERROR_FAILURE;

  if (aIndex < 0 || aIndex >= static_cast<int32_t>(thisAcc->AnchorCount()))
    return NS_ERROR_INVALID_ARG;

  NS_IF_ADDREF(*aAccessible = thisAcc->AnchorAt(aIndex));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperLink::GetValid(bool* aValid)
{
  NS_ENSURE_ARG_POINTER(aValid);
  *aValid = false;

  if (static_cast<Accessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  *aValid = static_cast<Accessible*>(this)->IsLinkValid();
  return NS_OK;
}

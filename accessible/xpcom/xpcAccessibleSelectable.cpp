/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Accessible-inl.h"
#include "xpcAccessibleDocument.h"

#include "nsIMutableArray.h"

using namespace mozilla::a11y;

NS_IMETHODIMP
xpcAccessibleSelectable::GetSelectedItems(nsIArray** aSelectedItems)
{
  NS_ENSURE_ARG_POINTER(aSelectedItems);
  *aSelectedItems = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;
  NS_PRECONDITION(Intl()->IsSelect(), "Called on non selectable widget!");

  AutoTArray<Accessible*, 10> items;
  Intl()->SelectedItems(&items);

  uint32_t itemCount = items.Length();
  if (itemCount == 0)
    return NS_OK;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> xpcItems =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  for (uint32_t idx = 0; idx < itemCount; idx++)
    xpcItems->AppendElement(static_cast<nsIAccessible*>(ToXPC(items[idx])), false);

  NS_ADDREF(*aSelectedItems = xpcItems);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleSelectable::GetSelectedItemCount(uint32_t* aSelectionCount)
{
  NS_ENSURE_ARG_POINTER(aSelectionCount);
  *aSelectionCount = 0;

  if (!Intl())
    return NS_ERROR_FAILURE;
  NS_PRECONDITION(Intl()->IsSelect(), "Called on non selectable widget!");

  *aSelectionCount = Intl()->SelectedItemCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleSelectable::GetSelectedItemAt(uint32_t aIndex,
                                           nsIAccessible** aSelected)
{
  NS_ENSURE_ARG_POINTER(aSelected);
  *aSelected = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;
  NS_PRECONDITION(Intl()->IsSelect(), "Called on non selectable widget!");

  *aSelected = ToXPC(Intl()->GetSelectedItem(aIndex));
  if (*aSelected) {
    NS_ADDREF(*aSelected);
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
xpcAccessibleSelectable::IsItemSelected(uint32_t aIndex, bool* aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = false;

  if (!Intl())
    return NS_ERROR_FAILURE;
  NS_PRECONDITION(Intl()->IsSelect(), "Called on non selectable widget!");

  *aIsSelected = Intl()->IsItemSelected(aIndex);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleSelectable::AddItemToSelection(uint32_t aIndex)
{
  if (!Intl())
    return NS_ERROR_FAILURE;
  NS_PRECONDITION(Intl()->IsSelect(), "Called on non selectable widget!");

  return Intl()->AddItemToSelection(aIndex) ? NS_OK : NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
xpcAccessibleSelectable::RemoveItemFromSelection(uint32_t aIndex)
{
  if (!Intl())
    return NS_ERROR_FAILURE;
  NS_PRECONDITION(Intl()->IsSelect(), "Called on non selectable widget!");

  return Intl()->RemoveItemFromSelection(aIndex) ? NS_OK : NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
xpcAccessibleSelectable::SelectAll(bool* aIsMultiSelect)
{
  NS_ENSURE_ARG_POINTER(aIsMultiSelect);
  *aIsMultiSelect = false;

  if (!Intl())
    return NS_ERROR_FAILURE;
  NS_PRECONDITION(Intl()->IsSelect(), "Called on non selectable widget!");

  *aIsMultiSelect = Intl()->SelectAll();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleSelectable::UnselectAll()
{
  if (!Intl())
    return NS_ERROR_FAILURE;
  NS_PRECONDITION(Intl()->IsSelect(), "Called on non selectable widget!");

  Intl()->UnselectAll();
  return NS_OK;
}

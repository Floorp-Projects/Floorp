/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleSelectable.h"

#include "Accessible-inl.h"

using namespace mozilla::a11y;

NS_IMETHODIMP
xpcAccessibleSelectable::GetSelectedItems(nsIArray** aSelectedItems)
{
  NS_ENSURE_ARG_POINTER(aSelectedItems);
  *aSelectedItems = nullptr;

  Accessible* acc = static_cast<Accessible*>(this);
  if (acc->IsDefunct())
    return NS_ERROR_FAILURE;
  NS_PRECONDITION(acc->IsSelect(), "Called on non selectable widget!");

  nsCOMPtr<nsIArray> items = acc->SelectedItems();
  if (items) {
    uint32_t length = 0;
    items->GetLength(&length);
    if (length)
      items.swap(*aSelectedItems);
  }

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleSelectable::GetSelectedItemCount(uint32_t* aSelectionCount)
{
  NS_ENSURE_ARG_POINTER(aSelectionCount);
  *aSelectionCount = 0;

  Accessible* acc = static_cast<Accessible*>(this);
  if (acc->IsDefunct())
    return NS_ERROR_FAILURE;
  NS_PRECONDITION(acc->IsSelect(), "Called on non selectable widget!");

  *aSelectionCount = acc->SelectedItemCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleSelectable::GetSelectedItemAt(uint32_t aIndex,
                                           nsIAccessible** aSelected)
{
  NS_ENSURE_ARG_POINTER(aSelected);
  *aSelected = nullptr;

  Accessible* acc = static_cast<Accessible*>(this);
  if (acc->IsDefunct())
    return NS_ERROR_FAILURE;
  NS_PRECONDITION(acc->IsSelect(), "Called on non selectable widget!");

  *aSelected = acc->GetSelectedItem(aIndex);
  if (*aSelected) {
    NS_ADDREF(*aSelected);
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
xpcAccessibleSelectable::ScriptableIsItemSelected(uint32_t aIndex,
                                                  bool* aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = false;

  Accessible* acc = static_cast<Accessible*>(this);
  if (acc->IsDefunct())
    return NS_ERROR_FAILURE;
  NS_PRECONDITION(acc->IsSelect(), "Called on non selectable widget!");

  *aIsSelected = acc->IsItemSelected(aIndex);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleSelectable::ScriptableAddItemToSelection(uint32_t aIndex)
{
  Accessible* acc = static_cast<Accessible*>(this);
  if (acc->IsDefunct())
    return NS_ERROR_FAILURE;
  NS_PRECONDITION(acc->IsSelect(), "Called on non selectable widget!");

  return acc->AddItemToSelection(aIndex) ? NS_OK : NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
xpcAccessibleSelectable::ScriptableRemoveItemFromSelection(uint32_t aIndex)
{
  Accessible* acc = static_cast<Accessible*>(this);
  if (acc->IsDefunct())
    return NS_ERROR_FAILURE;
  NS_PRECONDITION(acc->IsSelect(), "Called on non selectable widget!");

  return acc->RemoveItemFromSelection(aIndex) ? NS_OK : NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
xpcAccessibleSelectable::ScriptableSelectAll(bool* aIsMultiSelect)
{
  NS_ENSURE_ARG_POINTER(aIsMultiSelect);
  *aIsMultiSelect = false;

  Accessible* acc = static_cast<Accessible*>(this);
  if (acc->IsDefunct())
    return NS_ERROR_FAILURE;
  NS_PRECONDITION(acc->IsSelect(), "Called on non selectable widget!");

  *aIsMultiSelect = acc->SelectAll();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleSelectable::ScriptableUnselectAll()
{
  Accessible* acc = static_cast<Accessible*>(this);
  if (acc->IsDefunct())
    return NS_ERROR_FAILURE;
  NS_PRECONDITION(acc->IsSelect(), "Called on non selectable widget!");

  acc->UnselectAll();
  return NS_OK;
}

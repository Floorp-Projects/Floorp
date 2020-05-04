/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDocShellEnumerator.h"

#include "nsDocShell.h"

using namespace mozilla;

nsDocShellEnumerator::nsDocShellEnumerator(
    nsDocShellEnumerator::EnumerationDirection aDirection,
    int32_t aDocShellType, nsDocShell& aRootItem)
    : mRootItem(&aRootItem),
      mDocShellType(aDocShellType),
      mDirection(aDirection) {}

nsresult nsDocShellEnumerator::BuildDocShellArray(
    nsTArray<RefPtr<nsIDocShell>>& aItemArray) {
  MOZ_ASSERT(mRootItem);

  aItemArray.Clear();

  if (mDirection == EnumerationDirection::Forwards) {
    return BuildArrayRecursiveForwards(mRootItem, aItemArray);
  }
  MOZ_ASSERT(mDirection == EnumerationDirection::Backwards);
  return BuildArrayRecursiveBackwards(mRootItem, aItemArray);
}

nsresult nsDocShellEnumerator::BuildArrayRecursiveForwards(
    nsDocShell* aItem, nsTArray<RefPtr<nsIDocShell>>& aItemArray) {
  nsresult rv;

  // add this item to the array
  if (mDocShellType == nsIDocShellTreeItem::typeAll ||
      aItem->ItemType() == mDocShellType) {
    if (!aItemArray.AppendElement(aItem, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  int32_t numChildren = aItem->ChildCount();

  for (int32_t i = 0; i < numChildren; ++i) {
    RefPtr<nsDocShell> curChild = aItem->GetInProcessChildAt(i);
    MOZ_ASSERT(curChild);

    rv = BuildArrayRecursiveForwards(curChild, aItemArray);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult nsDocShellEnumerator::BuildArrayRecursiveBackwards(
    nsDocShell* aItem, nsTArray<RefPtr<nsIDocShell>>& aItemArray) {
  nsresult rv;

  uint32_t numChildren = aItem->ChildCount();

  for (int32_t i = numChildren - 1; i >= 0; --i) {
    RefPtr<nsDocShell> curChild = aItem->GetInProcessChildAt(i);
    MOZ_ASSERT(curChild);

    rv = BuildArrayRecursiveBackwards(curChild, aItemArray);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // add this item to the array
  if (mDocShellType == nsIDocShellTreeItem::typeAll ||
      aItem->ItemType() == mDocShellType) {
    if (!aItemArray.AppendElement(aItem, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
}

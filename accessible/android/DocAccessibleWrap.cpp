/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessibleWrap.h"
#include "nsIDocShell.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// DocAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

DocAccessibleWrap::DocAccessibleWrap(nsIDocument* aDocument,
                                     nsIPresShell* aPresShell)
  : DocAccessible(aDocument, aPresShell)
{
  nsCOMPtr<nsIDocShellTreeItem> treeItem(aDocument->GetDocShell());

  nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;
  treeItem->GetParent(getter_AddRefs(parentTreeItem));

  if (treeItem->ItemType() == nsIDocShellTreeItem::typeContent &&
      (!parentTreeItem ||
       parentTreeItem->ItemType() == nsIDocShellTreeItem::typeChrome)) {
    // The top-level content document gets this special ID.
    mID = kNoID;
  } else {
    mID = AcquireID();
  }
}

DocAccessibleWrap::~DocAccessibleWrap() {}

AccessibleWrap*
DocAccessibleWrap::GetAccessibleByID(int32_t aID) const
{
  if (AccessibleWrap* acc = mIDToAccessibleMap.Get(aID)) {
    return acc;
  }

  // If the ID is not in the hash table, check the IDs of the child docs.
  for (uint32_t i = 0; i < ChildDocumentCount(); i++) {
    auto childDoc = reinterpret_cast<AccessibleWrap*>(GetChildDocumentAt(i));
    if (childDoc->VirtualViewID() == aID) {
      return childDoc;
    }
  }

  return nullptr;
}

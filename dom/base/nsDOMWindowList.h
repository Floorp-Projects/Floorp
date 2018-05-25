/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsDOMWindowList_h___
#define nsDOMWindowList_h___

#include "nsCOMPtr.h"
#include <stdint.h>
#include "nsIDocShell.h"

class nsIDocShell;
class nsIDOMWindow;

class nsDOMWindowList final
{
public:
  explicit nsDOMWindowList(nsIDocShell* aDocShell);

  NS_INLINE_DECL_REFCOUNTING(nsDOMWindowList)

  uint32_t GetLength();
  already_AddRefed<nsPIDOMWindowOuter> IndexedGetter(uint32_t aIndex);
  already_AddRefed<nsPIDOMWindowOuter> NamedItem(const nsAString& aName);

  //local methods
  void SetDocShell(nsIDocShell* aDocShell);
  already_AddRefed<nsIDocShellTreeItem> GetDocShellTreeItemAt(uint32_t aIndex)
  {
    EnsureFresh();
    nsCOMPtr<nsIDocShellTreeItem> item;
    if (mDocShellNode) {
      mDocShellNode->GetChildAt(aIndex, getter_AddRefs(item));
    }
    return item.forget();
  }

protected:
  ~nsDOMWindowList();

  // Note: this function may flush and cause mDocShellNode to become null.
  void EnsureFresh();

  nsIDocShell* mDocShellNode; //Weak Reference
};

#endif // nsDOMWindowList_h___

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsDOMWindowList_h___
#define nsDOMWindowList_h___

#include "nsCOMPtr.h"
#include "nsIDOMWindowCollection.h"
#include <stdint.h>
#include "nsIDocShellTreeItem.h"

class nsIDocShell;
class nsIDOMWindow;

class nsDOMWindowList : public nsIDOMWindowCollection
{
public:
  nsDOMWindowList(nsIDocShell *aDocShell);
  virtual ~nsDOMWindowList();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMWINDOWCOLLECTION

  uint32_t GetLength();
  already_AddRefed<nsIDOMWindow> IndexedGetter(uint32_t aIndex, bool& aFound);

  //local methods
  NS_IMETHOD SetDocShell(nsIDocShell* aDocShell);
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
  // Note: this function may flush and cause mDocShellNode to become null.
  void EnsureFresh();

  nsIDocShellTreeNode* mDocShellNode; //Weak Reference
};

#endif // nsDOMWindowList_h___

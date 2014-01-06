/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Local Includes
#include "nsDOMWindowList.h"

// Helper classes
#include "nsCOMPtr.h"

// Interfaces needed
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIWebNavigation.h"

nsDOMWindowList::nsDOMWindowList(nsIDocShell *aDocShell)
{
  SetDocShell(aDocShell);
}

nsDOMWindowList::~nsDOMWindowList()
{
}

NS_IMPL_ADDREF(nsDOMWindowList)
NS_IMPL_RELEASE(nsDOMWindowList)

NS_INTERFACE_MAP_BEGIN(nsDOMWindowList)
   NS_INTERFACE_MAP_ENTRY(nsIDOMWindowCollection)
   NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsDOMWindowList::SetDocShell(nsIDocShell* aDocShell)
{
  mDocShellNode = aDocShell; // Weak Reference

  return NS_OK;
}

void
nsDOMWindowList::EnsureFresh()
{
  nsCOMPtr<nsIWebNavigation> shellAsNav = do_QueryInterface(mDocShellNode);

  if (shellAsNav) {
    nsCOMPtr<nsIDOMDocument> domdoc;
    shellAsNav->GetDocument(getter_AddRefs(domdoc));

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);

    if (doc) {
      doc->FlushPendingNotifications(Flush_ContentAndNotify);
    }
  }
}

uint32_t
nsDOMWindowList::GetLength()
{
  EnsureFresh();

  NS_ENSURE_TRUE(mDocShellNode, 0);

  int32_t length;
  nsresult rv = mDocShellNode->GetChildCount(&length);
  NS_ENSURE_SUCCESS(rv, 0);

  return uint32_t(length);
}

NS_IMETHODIMP 
nsDOMWindowList::GetLength(uint32_t* aLength)
{
  *aLength = GetLength();
  return NS_OK;
}

already_AddRefed<nsIDOMWindow>
nsDOMWindowList::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  aFound = false;

  nsCOMPtr<nsIDocShellTreeItem> item = GetDocShellTreeItemAt(aIndex);
  if (!item) {
    return nullptr;
  }

  nsCOMPtr<nsIDOMWindow> window = do_GetInterface(item);
  MOZ_ASSERT(window);

  aFound = true;
  return window.forget();
}

NS_IMETHODIMP 
nsDOMWindowList::Item(uint32_t aIndex, nsIDOMWindow** aReturn)
{
  bool found;
  nsCOMPtr<nsIDOMWindow> window = IndexedGetter(aIndex, found);
  window.forget(aReturn);
  return NS_OK;
}

NS_IMETHODIMP 
nsDOMWindowList::NamedItem(const nsAString& aName, nsIDOMWindow** aReturn)
{
  nsCOMPtr<nsIDocShellTreeItem> item;

  *aReturn = nullptr;

  EnsureFresh();

  if (mDocShellNode) {
    mDocShellNode->FindChildWithName(PromiseFlatString(aName).get(),
                                     false, false, nullptr,
                                     nullptr, getter_AddRefs(item));

    nsCOMPtr<nsIScriptGlobalObject> globalObject(do_GetInterface(item));
    if (globalObject) {
      CallQueryInterface(globalObject.get(), aReturn);
    }
  }

  return NS_OK;
}

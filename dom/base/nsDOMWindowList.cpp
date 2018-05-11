/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMWindowList.h"

#include "FlushType.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIWebNavigation.h"

using namespace mozilla;

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
    nsCOMPtr<nsIDocument> doc;
    shellAsNav->GetDocument(getter_AddRefs(doc));

    if (doc) {
      doc->FlushPendingNotifications(FlushType::ContentAndNotify);
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

already_AddRefed<nsPIDOMWindowOuter>
nsDOMWindowList::IndexedGetter(uint32_t aIndex)
{
  nsCOMPtr<nsIDocShellTreeItem> item = GetDocShellTreeItemAt(aIndex);
  if (!item) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = item->GetWindow();
  MOZ_ASSERT(window);

  return window.forget();
}

NS_IMETHODIMP
nsDOMWindowList::Item(uint32_t aIndex, mozIDOMWindowProxy** aReturn)
{
  nsCOMPtr<nsPIDOMWindowOuter> window = IndexedGetter(aIndex);
  window.forget(aReturn);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowList::NamedItem(const nsAString& aName, mozIDOMWindowProxy** aReturn)
{
  nsCOMPtr<nsIDocShellTreeItem> item;

  *aReturn = nullptr;

  EnsureFresh();

  if (mDocShellNode) {
    mDocShellNode->FindChildWithName(aName, false, false, nullptr,
                                     nullptr, getter_AddRefs(item));

    nsCOMPtr<nsIScriptGlobalObject> globalObject(do_GetInterface(item));
    if (globalObject) {
      CallQueryInterface(globalObject.get(), aReturn);
    }
  }

  return NS_OK;
}

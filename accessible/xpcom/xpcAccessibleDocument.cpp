/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleDocument.h"
#include "xpcAccessibleImage.h"
#include "xpcAccessibleTable.h"
#include "xpcAccessibleTableCell.h"

#include "nsAccUtils.h"
#include "DocAccessible-inl.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_QUERY_INTERFACE_INHERITED(xpcAccessibleDocument, xpcAccessibleHyperText,
                                  nsIAccessibleDocument)
NS_IMPL_ADDREF_INHERITED(xpcAccessibleDocument, xpcAccessibleHyperText)
NS_IMETHODIMP_(MozExternalRefCountType) xpcAccessibleDocument::Release(void) {
  nsrefcnt r = xpcAccessibleHyperText::Release();
  NS_LOG_RELEASE(this, r, "xpcAccessibleDocument");

  // The only reference to the xpcAccessibleDocument is in DocManager's cache.
  if (r == 1 && !!mIntl && mCache.Count() == 0) {
    if (mIntl->IsLocal()) {
      GetAccService()->RemoveFromXPCDocumentCache(mIntl->AsLocal()->AsDoc());
    } else {
      GetAccService()->RemoveFromRemoteXPCDocumentCache(
          mIntl->AsRemote()->AsDoc());
    }
  }
  return r;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleDocument

NS_IMETHODIMP
xpcAccessibleDocument::GetURL(nsAString& aURL) {
  if (!mIntl) return NS_ERROR_FAILURE;

  nsAccUtils::DocumentURL(mIntl, aURL);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetTitle(nsAString& aTitle) {
  if (!Intl()) return NS_ERROR_FAILURE;

  nsAutoString title;
  Intl()->Title(title);
  aTitle = title;
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetMimeType(nsAString& aType) {
  if (!mIntl) return NS_ERROR_FAILURE;

  nsAccUtils::DocumentMimeType(mIntl, aType);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetDocType(nsAString& aType) {
  if (!Intl()) return NS_ERROR_FAILURE;

  Intl()->DocType(aType);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetDOMDocument(dom::Document** aDOMDocument) {
  NS_ENSURE_ARG_POINTER(aDOMDocument);
  *aDOMDocument = nullptr;

  if (!Intl()) return NS_ERROR_FAILURE;

  if (Intl()->DocumentNode()) NS_ADDREF(*aDOMDocument = Intl()->DocumentNode());

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetWindow(mozIDOMWindowProxy** aDOMWindow) {
  NS_ENSURE_ARG_POINTER(aDOMWindow);
  *aDOMWindow = nullptr;

  if (!Intl()) return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aDOMWindow = Intl()->DocumentNode()->GetWindow());
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetParentDocument(nsIAccessibleDocument** aDocument) {
  NS_ENSURE_ARG_POINTER(aDocument);
  *aDocument = nullptr;

  if (!Intl()) return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aDocument = ToXPCDocument(Intl()->ParentDocument()));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetChildDocumentCount(uint32_t* aCount) {
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (!Intl()) return NS_ERROR_FAILURE;

  *aCount = Intl()->ChildDocumentCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetChildDocumentAt(uint32_t aIndex,
                                          nsIAccessibleDocument** aDocument) {
  NS_ENSURE_ARG_POINTER(aDocument);
  *aDocument = nullptr;

  if (!Intl()) return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aDocument = ToXPCDocument(Intl()->GetChildDocumentAt(aIndex)));
  return *aDocument ? NS_OK : NS_ERROR_INVALID_ARG;
}

////////////////////////////////////////////////////////////////////////////////
// xpcAccessibleDocument

xpcAccessibleGeneric* xpcAccessibleDocument::GetAccessible(
    Accessible* aAccessible) {
  if (aAccessible->IsLocal() &&
      ToXPCDocument(aAccessible->AsLocal()->Document()) != this) {
    NS_ERROR(
        "This XPCOM document is not related with given internal accessible!");
    return nullptr;
  }

  if (aAccessible->IsRemote() &&
      ToXPCDocument(aAccessible->AsRemote()->Document()) != this) {
    NS_ERROR(
        "This XPCOM document is not related with given internal accessible!");
    return nullptr;
  }

  if (aAccessible->IsDoc()) return this;

  return mCache.LookupOrInsertWith(aAccessible, [&]() -> xpcAccessibleGeneric* {
    if (aAccessible->IsImage()) {
      return new xpcAccessibleImage(aAccessible);
    }
    if (aAccessible->IsTable()) {
      return new xpcAccessibleTable(aAccessible);
    }
    if (aAccessible->IsTableCell()) {
      return new xpcAccessibleTableCell(aAccessible);
    }
    if (aAccessible->IsHyperText()) {
      return new xpcAccessibleHyperText(aAccessible);
    }

    return new xpcAccessibleGeneric(aAccessible);
  });
}

void xpcAccessibleDocument::Shutdown() {
  for (auto iter = mCache.Iter(); !iter.Done(); iter.Next()) {
    iter.Data()->Shutdown();
    iter.Remove();
  }
  xpcAccessibleGeneric::Shutdown();
}

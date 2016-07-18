/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleDocument.h"
#include "xpcAccessibleImage.h"
#include "xpcAccessibleTable.h"
#include "xpcAccessibleTableCell.h"

#include "mozilla/a11y/DocAccessibleParent.h"
#include "DocAccessible-inl.h"
#include "nsIDOMDocument.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsISupports and cycle collection

NS_IMPL_CYCLE_COLLECTION_CLASS(xpcAccessibleDocument)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(xpcAccessibleDocument,
                                                  xpcAccessibleGeneric)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCache)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(xpcAccessibleDocument,
                                                xpcAccessibleGeneric)
  tmp->mCache.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(xpcAccessibleDocument)
  NS_INTERFACE_MAP_ENTRY(nsIAccessibleDocument)
NS_INTERFACE_MAP_END_INHERITING(xpcAccessibleHyperText)

NS_IMPL_ADDREF_INHERITED(xpcAccessibleDocument, xpcAccessibleHyperText)
NS_IMPL_RELEASE_INHERITED(xpcAccessibleDocument, xpcAccessibleHyperText)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleDocument

NS_IMETHODIMP
xpcAccessibleDocument::GetURL(nsAString& aURL)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->URL(aURL);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetTitle(nsAString& aTitle)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  nsAutoString title;
  Intl()->Title(title);
  aTitle = title;
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetMimeType(nsAString& aType)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->MimeType(aType);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetDocType(nsAString& aType)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->DocType(aType);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetDOMDocument(nsIDOMDocument** aDOMDocument)
{
  NS_ENSURE_ARG_POINTER(aDOMDocument);
  *aDOMDocument = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  if (Intl()->DocumentNode())
    CallQueryInterface(Intl()->DocumentNode(), aDOMDocument);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetWindow(mozIDOMWindowProxy** aDOMWindow)
{
  NS_ENSURE_ARG_POINTER(aDOMWindow);
  *aDOMWindow = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aDOMWindow = Intl()->DocumentNode()->GetWindow());
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetParentDocument(nsIAccessibleDocument** aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  *aDocument = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aDocument = ToXPCDocument(Intl()->ParentDocument()));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetChildDocumentCount(uint32_t* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (!Intl())
    return NS_ERROR_FAILURE;

  *aCount = Intl()->ChildDocumentCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetChildDocumentAt(uint32_t aIndex,
                                          nsIAccessibleDocument** aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  *aDocument = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aDocument = ToXPCDocument(Intl()->GetChildDocumentAt(aIndex)));
  return *aDocument ? NS_OK : NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetVirtualCursor(nsIAccessiblePivot** aVirtualCursor)
{
  NS_ENSURE_ARG_POINTER(aVirtualCursor);
  *aVirtualCursor = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aVirtualCursor = Intl()->VirtualCursor());
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// xpcAccessibleDocument

xpcAccessibleGeneric*
xpcAccessibleDocument::GetAccessible(Accessible* aAccessible)
{
  MOZ_ASSERT(!mRemote);
  if (ToXPCDocument(aAccessible->Document()) != this) {
    NS_ERROR("This XPCOM document is not related with given internal accessible!");
    return nullptr;
  }

  if (aAccessible->IsDoc())
    return this;

  xpcAccessibleGeneric* xpcAcc = mCache.GetWeak(aAccessible);
  if (xpcAcc)
    return xpcAcc;

  if (aAccessible->IsImage())
    xpcAcc = new xpcAccessibleImage(aAccessible);
  else if (aAccessible->IsTable())
    xpcAcc = new xpcAccessibleTable(aAccessible);
  else if (aAccessible->IsTableCell())
    xpcAcc = new xpcAccessibleTableCell(aAccessible);
  else if (aAccessible->IsHyperText())
    xpcAcc = new xpcAccessibleHyperText(aAccessible);
  else
    xpcAcc = new xpcAccessibleGeneric(aAccessible);

  mCache.Put(aAccessible, xpcAcc);
  return xpcAcc;
}

xpcAccessibleGeneric*
xpcAccessibleDocument::GetXPCAccessible(ProxyAccessible* aProxy)
{
  MOZ_ASSERT(mRemote);
  MOZ_ASSERT(aProxy->Document() == mIntl.AsProxy());
  if (aProxy->IsDoc()) {
    return this;
  }

  xpcAccessibleGeneric* acc = mCache.GetWeak(aProxy);
  if (acc) {
    return acc;
  }

  // XXX support exposing optional interfaces.
  uint8_t interfaces = 0;
  if (aProxy->mHasValue) {
    interfaces |= eValue;
  }
  
  if (aProxy->mIsHyperLink) {
    interfaces |= eHyperLink;
  }

  if (aProxy->mIsHyperText) {
    interfaces |= eText;
    acc = new xpcAccessibleHyperText(aProxy, interfaces);
    mCache.Put(aProxy, acc);

    return acc;
  }

  acc = new xpcAccessibleGeneric(aProxy, interfaces);
  mCache.Put(aProxy, acc);

  return acc;
}

void
xpcAccessibleDocument::Shutdown()
{
  for (auto iter = mCache.Iter(); !iter.Done(); iter.Next()) {
    iter.Data()->Shutdown();
    iter.Remove();
  }
  xpcAccessibleGeneric::Shutdown();
}

xpcAccessibleGeneric*
a11y::ToXPC(AccessibleOrProxy aAcc)
{
  if (aAcc.IsNull()) {
    return nullptr;
  }

  if (aAcc.IsAccessible()) {
    return ToXPC(aAcc.AsAccessible());
  }

 xpcAccessibleDocument* doc = ToXPCDocument(aAcc.AsProxy()->Document());
 return doc->GetXPCAccessible(aAcc.AsProxy());
}

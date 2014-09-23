/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleDocument.h"

#include "DocAccessible-inl.h"
#include "nsIDOMDocument.h"

using namespace mozilla::a11y;

NS_IMETHODIMP
xpcAccessibleDocument::GetURL(nsAString& aURL)
{
  if (static_cast<DocAccessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  static_cast<DocAccessible*>(this)->URL(aURL);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetTitle(nsAString& aTitle)
{
  if (static_cast<DocAccessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  nsAutoString title;
  static_cast<DocAccessible*>(this)->Title(title);
  aTitle = title;
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetMimeType(nsAString& aType)
{
  if (static_cast<DocAccessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  static_cast<DocAccessible*>(this)->MimeType(aType);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetDocType(nsAString& aType)
{
  if (static_cast<DocAccessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  static_cast<DocAccessible*>(this)->DocType(aType);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetDOMDocument(nsIDOMDocument** aDOMDocument)
{
  NS_ENSURE_ARG_POINTER(aDOMDocument);
  *aDOMDocument = nullptr;

  if (static_cast<DocAccessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  if (static_cast<DocAccessible*>(this)->DocumentNode())
    CallQueryInterface(static_cast<DocAccessible*>(this)->DocumentNode(), aDOMDocument);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetWindow(nsIDOMWindow** aDOMWindow)
{
  NS_ENSURE_ARG_POINTER(aDOMWindow);
  *aDOMWindow = nullptr;

  if (static_cast<DocAccessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aDOMWindow = static_cast<DocAccessible*>(this)->DocumentNode()->GetWindow());
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetParentDocument(nsIAccessibleDocument** aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  *aDocument = nullptr;

  if (static_cast<DocAccessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aDocument = static_cast<DocAccessible*>(this)->ParentDocument());
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetChildDocumentCount(uint32_t* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (static_cast<DocAccessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  *aCount = static_cast<DocAccessible*>(this)->ChildDocumentCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleDocument::ScriptableGetChildDocumentAt(uint32_t aIndex,
                                                    nsIAccessibleDocument** aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  *aDocument = nullptr;

  if (static_cast<DocAccessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aDocument = static_cast<DocAccessible*>(this)->GetChildDocumentAt(aIndex));
  return *aDocument ? NS_OK : NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
xpcAccessibleDocument::GetVirtualCursor(nsIAccessiblePivot** aVirtualCursor)
{
  NS_ENSURE_ARG_POINTER(aVirtualCursor);
  *aVirtualCursor = nullptr;

  if (static_cast<DocAccessible*>(this)->IsDefunct())
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aVirtualCursor = static_cast<DocAccessible*>(this)->VirtualCursor());
  return NS_OK;
}

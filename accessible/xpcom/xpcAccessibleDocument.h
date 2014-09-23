/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessibleDocument_h_
#define mozilla_a11y_xpcAccessibleDocument_h_

#include "nsIAccessibleDocument.h"

namespace mozilla {
namespace a11y {

class xpcAccessibleDocument : public nsIAccessibleDocument
{
public:
  NS_IMETHOD GetURL(nsAString& aURL) MOZ_FINAL;
  NS_IMETHOD GetTitle(nsAString& aTitle) MOZ_FINAL;
  NS_IMETHOD GetMimeType(nsAString& aType) MOZ_FINAL;
  NS_IMETHOD GetDocType(nsAString& aType) MOZ_FINAL;
  NS_IMETHOD GetDOMDocument(nsIDOMDocument** aDOMDocument) MOZ_FINAL;
  NS_IMETHOD GetWindow(nsIDOMWindow** aDOMWindow) MOZ_FINAL;
  NS_IMETHOD GetParentDocument(nsIAccessibleDocument** aDocument) MOZ_FINAL;
  NS_IMETHOD GetChildDocumentCount(uint32_t* aCount) MOZ_FINAL;
  NS_IMETHOD ScriptableGetChildDocumentAt(uint32_t aIndex,
                                          nsIAccessibleDocument** aDocument) MOZ_FINAL;
  NS_IMETHOD GetVirtualCursor(nsIAccessiblePivot** aVirtualCursor) MOZ_FINAL;

private:
  friend class DocAccessible;

  xpcAccessibleDocument() { }

  xpcAccessibleDocument(const xpcAccessibleDocument&) MOZ_DELETE;
  xpcAccessibleDocument& operator =(const xpcAccessibleDocument&) MOZ_DELETE;
};

} // namespace a11y
} // namespace mozilla

#endif

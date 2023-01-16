/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XULPersist_h
#define mozilla_dom_XULPersist_h

#include "nsStubDocumentObserver.h"

#ifndef MOZ_NEW_XULSTORE
#  include "nsCOMPtr.h"
class nsIXULStore;
#endif

template <typename T>
class nsCOMArray;

namespace mozilla::dom {

class XULPersist final : public nsStubDocumentObserver {
 public:
  NS_DECL_ISUPPORTS

  explicit XULPersist(Document* aDocument);
  void Init();
  void DropDocumentReference();

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED

 protected:
  void Persist(mozilla::dom::Element* aElement, nsAtom* aAttribute);

 private:
  ~XULPersist();
  nsresult ApplyPersistentAttributes();
  nsresult ApplyPersistentAttributesToElements(const nsAString& aID,
                                               const nsAString& aDocURI,
                                               nsCOMArray<Element>& aElements);

  nsCOMPtr<nsIXULStore> mLocalStore;

  // A weak pointer to our document. Nulled out by DropDocumentReference.
  Document* MOZ_NON_OWNING_REF mDocument;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_XULPersist_h

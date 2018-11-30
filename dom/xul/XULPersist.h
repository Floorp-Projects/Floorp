/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XULPersist_h
#define mozilla_dom_XULPersist_h

class nsIXULStore;

namespace mozilla {
namespace dom {

class XULPersist final : public nsStubDocumentObserver {
 public:
  NS_DECL_ISUPPORTS

  explicit XULPersist(nsIDocument* aDocument);
  void Init();
  void DropDocumentReference();

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED

 protected:
  void Persist(mozilla::dom::Element* aElement, int32_t aNameSpaceID,
               nsAtom* aAttribute);

 private:
  ~XULPersist();
  nsresult ApplyPersistentAttributes();
  nsresult ApplyPersistentAttributesInternal();
  nsresult ApplyPersistentAttributesToElements(const nsAString& aID,
                                               nsCOMArray<Element>& aElements);

  nsCOMPtr<nsIXULStore> mLocalStore;
  // A weak pointer to our document. Nulled out by DropDocumentReference.
  nsIDocument* MOZ_NON_OWNING_REF mDocument;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_XULPersist_h

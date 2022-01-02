/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLAllCollection_h
#define mozilla_dom_HTMLAllCollection_h

#include "mozilla/dom/Document.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "nsRefPtrHashtable.h"
#include "nsWrapperCache.h"

#include <stdint.h>

class nsContentList;
class nsINode;

namespace mozilla {
namespace dom {

class Document;
class Element;
class OwningHTMLCollectionOrElement;
template <typename>
struct Nullable;
template <typename>
class Optional;

class HTMLAllCollection final : public nsISupports, public nsWrapperCache {
  ~HTMLAllCollection();

 public:
  explicit HTMLAllCollection(mozilla::dom::Document* aDocument);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(HTMLAllCollection)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  nsINode* GetParentObject() const;

  uint32_t Length();
  Element* IndexedGetter(uint32_t aIndex, bool& aFound) {
    Element* result = Item(aIndex);
    aFound = !!result;
    return result;
  }

  void NamedItem(const nsAString& aName,
                 Nullable<OwningHTMLCollectionOrElement>& aResult) {
    bool found = false;
    NamedGetter(aName, found, aResult);
  }
  void NamedGetter(const nsAString& aName, bool& aFound,
                   Nullable<OwningHTMLCollectionOrElement>& aResult);
  void GetSupportedNames(nsTArray<nsString>& aNames);

  void Item(const Optional<nsAString>& aNameOrIndex,
            Nullable<OwningHTMLCollectionOrElement>& aResult);

  void LegacyCall(JS::Handle<JS::Value>,
                  const Optional<nsAString>& aNameOrIndex,
                  Nullable<OwningHTMLCollectionOrElement>& aResult) {
    Item(aNameOrIndex, aResult);
  }

 private:
  nsContentList* Collection();

  /**
   * Returns the HTMLCollection for document.all[aID], or null if there isn't
   * one.
   */
  nsContentList* GetDocumentAllList(const nsAString& aID);

  /**
   * Helper for indexed getter and spec Item() method.
   */
  Element* Item(uint32_t aIndex);

  RefPtr<mozilla::dom::Document> mDocument;
  RefPtr<nsContentList> mCollection;
  nsRefPtrHashtable<nsStringHashKey, nsContentList> mNamedMap;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_HTMLAllCollection_h

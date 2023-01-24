/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HighlightRegistry_h
#define mozilla_dom_HighlightRegistry_h

#include "mozilla/Attributes.h"
#include "mozilla/CompactPair.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsHashKeys.h"
#include "nsTHashMap.h"
#include "nsHashtablesFwd.h"
#include "nsWrapperCache.h"

class nsFrameSelection;

namespace mozilla {
class ErrorResult;
}
namespace mozilla::dom {

class Document;
class Highlight;

/**
 * @brief HighlightRegistry manages all `Highlight`s available to a `Document`.
 *
 * This class is exposed via `HighlightRegistry.webidl` and used to
 * add or remove `Highlight` instances to a document and binding it
 * to a highlight name.
 *
 * The HighlightRegistry idl interface defines this class to be a `maplike`.
 * To be able to access the members of the maplike without proper support
 * for iteration from C++, the insertion and deletion operations are
 * overridden and the data is also held inside of this class.
 *
 * @see https://drafts.csswg.org/css-highlight-api-1/#registration
 */
class HighlightRegistry final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(HighlightRegistry)

 public:
  explicit HighlightRegistry(Document* aDocument);

 protected:
  ~HighlightRegistry();

 public:
  /**
   * @brief Adds selections for all highlights to the `FrameSelection`.
   *
   * This method is called if highlights are added to the registry before
   * a `FrameSelection` is available.
   */
  MOZ_CAN_RUN_SCRIPT void AddHighlightSelectionsToFrameSelection(
      ErrorResult& aRv);

  /**
   * @brief Propagates changes to a highlight to the `FrameSelection`.
   */
  MOZ_CAN_RUN_SCRIPT void HighlightPropertiesChanged(Highlight& aHighlight,
                                                     ErrorResult& aRv);

  // WebIDL interface

  Document* GetParentObject() const { return mDocument; };

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  /**
   * @brief Adds a new `Highlight` to `this` using `aKey` as highlight name.
   *
   * Highlight instances are ordered by insertion.
   *
   * This call registers `this` and `aHighlightName` in the highlight given in
   * `aValue`.
   *
   * If a `FrameSelection` is present, a highlight selection is created.
   */
  MOZ_CAN_RUN_SCRIPT void Set(const nsAString& aKey, Highlight& aValue,
                              ErrorResult& aRv);

  /**
   * @brief Removes all highlights from this registry.
   *
   * If a `FrameSelection` is present, all highlight selections are removed.
   */
  void Clear(ErrorResult& aRv);

  /**
   * @brief Removes the highlight named `aKey` from the registry.
   *
   * This call removes the combination of `this` and `aKey` from the highlight.
   * If a `FrameSelection` is present, the highlight selection is removed.
   */
  void Delete(const nsAString& aKey, ErrorResult& aRv);

 private:
  /**
   * @brief Get the `FrameSelection` object if available. Can return nullptr.
   */
  RefPtr<nsFrameSelection> GetFrameSelection();

  /**
   * Parent document.
   */
  RefPtr<Document> mDocument;

  /**
   * Highlight instances are stored as array of name-value tuples
   * instead of a hashmap in order to preserve the insertion order.
   *
   * This is done
   *    a) to keep the order in sync with the underlying
   *       data structure of the `maplike` interface and
   *    b) because the insertion order defines the stacking order of
   *       of highlights that have the same priority.
   */
  nsTArray<CompactPair<RefPtr<const nsAtom>, RefPtr<Highlight>>>
      mHighlightsOrdered;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_HighlightRegistry_h

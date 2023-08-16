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

class AbstractRange;
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
  MOZ_CAN_RUN_SCRIPT void AddHighlightSelectionsToFrameSelection();

  /**
   * @brief Adds the Range to the Highlight Selection if it belongs to the same
   * Document.
   *
   * If no Highlight Selection for this highlight exists, it will be created.
   * This may occur when a Highlight is added to the Registry after the
   * nsFrameSelection is created.
   */
  MOZ_CAN_RUN_SCRIPT void MaybeAddRangeToHighlightSelection(
      AbstractRange& aRange, Highlight& aHighlight);

  /**
   * @brief Removes the Range from the Highlight Selection if it belongs to the
   * same Document.
   *
   * @note If the last range of a highlight selection is removed, the selection
   * itself is *not* removed.
   */
  MOZ_CAN_RUN_SCRIPT void MaybeRemoveRangeFromHighlightSelection(
      AbstractRange& aRange, Highlight& aHighlight);

  /**
   * @brief Removes the highlight selections associated with the highlight.
   *
   * This method is called when the Highlight is cleared
   * (i.e., all Ranges are removed).
   */
  MOZ_CAN_RUN_SCRIPT void RemoveHighlightSelection(Highlight& aHighlight);

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
  MOZ_CAN_RUN_SCRIPT void Clear(ErrorResult& aRv);

  /**
   * @brief Removes the highlight named `aKey` from the registry.
   *
   * This call removes the combination of `this` and `aKey` from the highlight.
   * If a `FrameSelection` is present, the highlight selection is removed.
   *
   * @return true if `aKey` existed and was deleted.
   */
  MOZ_CAN_RUN_SCRIPT bool Delete(const nsAString& aKey, ErrorResult& aRv);

  /**
   * @brief Get the `FrameSelection` object if available. Can return nullptr.
   */
  RefPtr<nsFrameSelection> GetFrameSelection();

  /**
   * @brief Get the registry name-value tuples.
   */
  nsTArray<CompactPair<RefPtr<nsAtom>, RefPtr<Highlight>>> const&
  HighlightsOrdered() {
    return mHighlightsOrdered;
  }

 private:
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
  nsTArray<CompactPair<RefPtr<nsAtom>, RefPtr<Highlight>>> mHighlightsOrdered;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_HighlightRegistry_h

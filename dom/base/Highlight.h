/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Highlight_h
#define mozilla_dom_Highlight_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/HighlightBinding.h"

#include "nsCycleCollectionParticipant.h"
#include "nsAtomHashKeys.h"
#include "nsTHashSet.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

class nsFrameSelection;
class nsPIDOMWindowInner;
namespace mozilla {
class ErrorResult;
}

namespace mozilla::dom {
class AbstractRange;
class Document;
class HighlightRegistry;
class Selection;

/**
 * @brief Collection of all data of a highlight instance.
 *
 * This struct is intended to be allocated on the stack and passed on
 * to the `nsFrameSelection` and layout code.
 */
struct HighlightSelectionData {
  RefPtr<nsAtom> mHighlightName;
  RefPtr<Highlight> mHighlight;
};

/**
 * @brief Representation of a custom `Highlight`.
 *
 * A `Highlight` is defined in JS as a collection of `AbstractRange`s.
 * Furthermore, a custom highlight contains the highlight type and priority.
 *
 * A highlight is added to a document using the `HighlightRegistry` interface.
 * A highlight can be added to a document using different names as well as to
 * multiple `HighlightRegistries`.
 * To propagate runtime changes of the highlight to its registries, an
 * observer pattern is implemented.
 *
 * The spec defines this class as a `setlike`. To allow access and iteration
 * of the setlike contents from C++, the insertion and deletion operations are
 * overridden and the Ranges are also stored internally in an Array.
 *
 * @see https://drafts.csswg.org/css-highlight-api-1/#creation
 */
class Highlight final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(Highlight)

 protected:
  MOZ_CAN_RUN_SCRIPT Highlight(
      const Sequence<OwningNonNull<AbstractRange>>& aInitialRanges,
      nsPIDOMWindowInner* aWindow, ErrorResult& aRv);
  ~Highlight() = default;

 public:
  /**
   * @brief Adds `this` to `aHighlightRegistry`.
   *
   * Highlights must know of all registry objects which contain them, so that
   * the registries can be notified when a property of the Highlight changes.
   *
   * Since a Highlight can be part of a registry using different names,
   * the name has to be provided as well.
   */
  void AddToHighlightRegistry(HighlightRegistry& aHighlightRegistry,
                              nsAtom& aHighlightName);

  /**
   * @brief Removes `this` from `aHighlightRegistry`.
   */
  void RemoveFromHighlightRegistry(HighlightRegistry& aHighlightRegistry,
                                   nsAtom& aHighlightName);

  /**
   * @brief Creates a Highlight Selection using the given ranges.
   */
  MOZ_CAN_RUN_SCRIPT already_AddRefed<Selection> CreateHighlightSelection(
      nsAtom* aHighlightName, nsFrameSelection* aFrameSelection);

  // WebIDL interface
  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static MOZ_CAN_RUN_SCRIPT_BOUNDARY already_AddRefed<Highlight> Constructor(
      const GlobalObject& aGlobal,
      const Sequence<OwningNonNull<AbstractRange>>& aInitialRanges,
      ErrorResult& aRv);

  /**
   * @brief Priority of this highlight.
   *
   * Priority is used to stack overlapping highlights.
   */
  int32_t Priority() const { return mPriority; }

  /**
   * @brief Set the priority of this Highlight.
   *
   * Priority is used to stack overlapping highlights.
   */
  void SetPriority(int32_t aPriority) { mPriority = aPriority; }

  /**
   * @brief The HighlightType of this Highlight (Highlight, Spelling Error,
   * Grammar Error)
   */
  HighlightType Type() const { return mHighlightType; }

  /**
   * @brief Sets the HighlightType (Highlight, Spelling Error, Grammar Error)
   */
  void SetType(HighlightType aHighlightType) {
    mHighlightType = aHighlightType;
  }

  /**
   * @brief This mirrors the `size` property in JS world (_not_ exposed via
   * webIDL)
   */
  uint32_t Size() const { return mRanges.Length(); }

  /**
   * @brief Adds a `Range` to this highlight.
   *
   * This adds `aRange` both to the setlike data storage and the internal one
   * needed for iteration, if it is not yet present.
   *
   * Also notifies all `HighlightRegistry` instances.
   */
  MOZ_CAN_RUN_SCRIPT void Add(AbstractRange& aRange, ErrorResult& aRv);

  /**
   * @brief Removes all ranges from this highlight.
   *
   * This removes all highlights from the setlike data structure as well as from
   * the internal one.
   *
   * Also notifies all `HighlightRegistry` instances.
   */
  MOZ_CAN_RUN_SCRIPT void Clear(ErrorResult& aRv);

  /**
   * @brief Removes `aRange` from this highlight.
   *
   * This removes `aRange` from the setlike data structure as well as from the
   * internal one.
   *
   * Also notifies all `HighlightRegistry` instances.
   *
   * @return As per spec, returns true if the range was deleted.
   */
  MOZ_CAN_RUN_SCRIPT bool Delete(AbstractRange& aRange, ErrorResult& aRv);

 private:
  RefPtr<nsPIDOMWindowInner> mWindow;

  /**
   * All Range objects contained in this highlight.
   */
  nsTArray<RefPtr<AbstractRange>> mRanges;

  /**
   * Type of this highlight.
   * @see HighlightType
   */
  HighlightType mHighlightType{HighlightType::Highlight};

  /**
   * Priority of this highlight.
   *
   * If highlights are overlapping, the priority can
   * be used to prioritize. If the priorities of all
   * Highlights involved are equal, the highlights are
   * stacked in order of ther insertion into the
   * `HighlightRegistry`.
   */
  int32_t mPriority{0};

  /**
   * All highlight registries that contain this Highlight.
   *
   * A highlight can be included in several registries
   * using several names.
   *
   * Note: Storing `HighlightRegistry` as raw pointer is safe here
   * because it unregisters itself from `this` when it is destroyed/CC'd
   */
  nsTHashMap<nsPtrHashKey<HighlightRegistry>, nsTHashSet<RefPtr<nsAtom>>>
      mHighlightRegistries;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_Highlight_h

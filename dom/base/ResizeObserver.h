/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ResizeObserver_h
#define mozilla_dom_ResizeObserver_h

#include "js/TypeDecls.h"
#include "mozilla/AppUnits.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/LinkedList.h"
#include "mozilla/WritingModes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ResizeObserverBinding.h"
#include "nsCoord.h"
#include "nsCycleCollectionParticipant.h"
#include "nsRefPtrHashtable.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class Element;

}  // namespace dom
}  // namespace mozilla

namespace mozilla {
namespace dom {

// For the internal implementation in ResizeObserver. Normally, this is owned by
// ResizeObserver.
class ResizeObservation final : public LinkedListElement<ResizeObservation> {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(ResizeObservation)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(ResizeObservation)

  ResizeObservation(Element& aTarget, ResizeObserverBoxOptions aBox,
                    const WritingMode aWM)
      : mTarget(&aTarget),
        mObservedBox(aBox),
        // This starts us with a 0,0 last-reported-size:
        mLastReportedSize(aWM),
        mLastReportedWM(aWM) {
    MOZ_ASSERT(mTarget, "Need a non-null target element");
  }

  Element* Target() const { return mTarget; }

  ResizeObserverBoxOptions BoxOptions() const { return mObservedBox; }

  /**
   * Returns whether the observed target element size differs from the saved
   * mLastReportedSize.
   */
  bool IsActive() const;

  /**
   * Update current mLastReportedSize with size from aSize.
   */
  void UpdateLastReportedSize(const nsSize& aSize);

 protected:
  ~ResizeObservation() = default;

  nsCOMPtr<Element> mTarget;

  const ResizeObserverBoxOptions mObservedBox;

  // The latest recorded size of observed target.
  // Per the spec, observation.lastReportedSize should be entry.borderBoxSize
  // or entry.contentBoxSize (i.e. logical size), instead of entry.contentRect
  // (i.e. physical rect), so we store this as LogicalSize.
  LogicalSize mLastReportedSize;
  WritingMode mLastReportedWM;
};

/**
 * ResizeObserver interfaces and algorithms are based on
 * https://drafts.csswg.org/resize-observer/#api
 */
class ResizeObserver final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ResizeObserver)

  ResizeObserver(already_AddRefed<nsPIDOMWindowInner>&& aOwner,
                 ResizeObserverCallback& aCb)
      : mOwner(aOwner), mCallback(&aCb) {
    MOZ_ASSERT(mOwner, "Need a non-null owner window");
  }

  nsISupports* GetParentObject() const { return mOwner; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return ResizeObserver_Binding::Wrap(aCx, this, aGivenProto);
  }

  static already_AddRefed<ResizeObserver> Constructor(
      const GlobalObject& aGlobal, ResizeObserverCallback& aCb,
      ErrorResult& aRv);

  void Observe(Element& aTarget, const ResizeObserverOptions& aOptions,
               ErrorResult& aRv);

  void Unobserve(Element& target, ErrorResult& aRv);

  void Disconnect();

  /**
   * Gather all observations which have an observed target with size changed
   * since last BroadcastActiveObservations() in this ResizeObserver.
   * An observation will be skipped if the depth of its observed target is less
   * or equal than aDepth. All gathered observations will be added to
   * mActiveTargets.
   */
  void GatherActiveObservations(uint32_t aDepth);

  /**
   * Returns whether this ResizeObserver has any active observations
   * since last GatherActiveObservations().
   */
  bool HasActiveObservations() const { return !mActiveTargets.IsEmpty(); }

  /**
   * Returns whether this ResizeObserver has any skipped observations
   * since last GatherActiveObservations().
   */
  bool HasSkippedObservations() const { return mHasSkippedTargets; }

  /**
   * Invoke the callback function in JavaScript for all active observations
   * and pass the sequence of ResizeObserverEntry so JavaScript can access them.
   * The active observations' mLastReportedSize fields will be updated, and
   * mActiveTargets will be cleared. It also returns the shallowest depth of
   * elements from active observations or numeric_limits<uint32_t>::max() if
   * there are not any active observations.
   */
  MOZ_CAN_RUN_SCRIPT uint32_t BroadcastActiveObservations();

 protected:
  ~ResizeObserver() { mObservationList.clear(); }

  nsCOMPtr<nsPIDOMWindowInner> mOwner;
  RefPtr<ResizeObserverCallback> mCallback;
  nsTArray<RefPtr<ResizeObservation>> mActiveTargets;
  // The spec uses a list to store the skipped targets. However, it seems what
  // we want is to check if there are any skipped targets (i.e. existence).
  // Therefore, we use a boolean value to represent the existence of skipped
  // targets.
  bool mHasSkippedTargets;

  // Combination of HashTable and LinkedList so we can iterate through
  // the elements of HashTable in order of insertion time, so we can deliver
  // observations in the correct order
  // FIXME: it will be nice if we have our own data structure for this in the
  // future, and mObservationMap should be considered the "owning" storage for
  // the observations, so it'd be better to drop mObservationList later.
  nsRefPtrHashtable<nsPtrHashKey<Element>, ResizeObservation> mObservationMap;
  LinkedList<ResizeObservation> mObservationList;
};

/**
 * ResizeObserverEntry is the entry that contains the information for observed
 * elements. This object is the one that's visible to JavaScript in callback
 * function that is fired by ResizeObserver.
 */
class ResizeObserverEntry final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ResizeObserverEntry)

  ResizeObserverEntry(nsISupports* aOwner, Element& aTarget)
      : mOwner(aOwner), mTarget(&aTarget) {
    MOZ_ASSERT(mOwner, "Need a non-null owner");
    MOZ_ASSERT(mTarget, "Need a non-null target element");
  }

  nsISupports* GetParentObject() const { return mOwner; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return ResizeObserverEntry_Binding::Wrap(aCx, this, aGivenProto);
  }

  static already_AddRefed<ResizeObserverEntry> Constructor(
      const GlobalObject& global, Element& target, ErrorResult& aRv);

  Element* Target() const { return mTarget; }

  /**
   * Returns the DOMRectReadOnly of target's content rect so it can be
   * accessed from JavaScript in callback function of ResizeObserver.
   */
  DOMRectReadOnly* ContentRect() const { return mContentRect; }

  /**
   * Returns target's logical border-box size and content-box size as
   * ResizeObserverSize.
   */
  ResizeObserverSize* BorderBoxSize() const { return mBorderBoxSize; }
  ResizeObserverSize* ContentBoxSize() const { return mContentBoxSize; }

  // Set borderBoxSize.
  void SetBorderBoxSize(const nsSize& aSize);
  // Set contentRect and contentBoxSize.
  void SetContentRectAndSize(const nsSize& aSize);

 protected:
  ~ResizeObserverEntry() = default;

  nsCOMPtr<nsISupports> mOwner;
  nsCOMPtr<Element> mTarget;

  RefPtr<DOMRectReadOnly> mContentRect;
  RefPtr<ResizeObserverSize> mBorderBoxSize;
  RefPtr<ResizeObserverSize> mContentBoxSize;
};

class ResizeObserverSize final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ResizeObserverSize)

  // Note: the unit of |aSize| is app unit, and we convert it into css pixel in
  // the public JS APIs.
  ResizeObserverSize(nsISupports* aOwner, const nsSize& aSize,
                     const WritingMode aWM)
      : mOwner(aOwner), mSize(aWM, aSize), mWM(aWM) {
    MOZ_ASSERT(mOwner, "Need a non-null owner");
  }

  nsISupports* GetParentObject() const { return mOwner; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return ResizeObserverSize_Binding::Wrap(aCx, this, aGivenProto);
  }

  double InlineSize() const {
    return NSAppUnitsToDoublePixels(mSize.ISize(mWM), AppUnitsPerCSSPixel());
  }

  double BlockSize() const {
    return NSAppUnitsToDoublePixels(mSize.BSize(mWM), AppUnitsPerCSSPixel());
  }

 protected:
  ~ResizeObserverSize() = default;

  nsCOMPtr<nsISupports> mOwner;
  const LogicalSize mSize;
  const WritingMode mWM;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ResizeObserver_h

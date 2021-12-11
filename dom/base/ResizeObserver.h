/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ResizeObserver_h
#define mozilla_dom_ResizeObserver_h

#include "gfxPoint.h"
#include "js/TypeDecls.h"
#include "mozilla/AppUnits.h"
#include "mozilla/Attributes.h"
#include "mozilla/LinkedList.h"
#include "mozilla/WritingModes.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ResizeObserverBinding.h"
#include "nsCoord.h"
#include "nsCycleCollectionParticipant.h"
#include "nsRefPtrHashtable.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

// XXX Avoid including this here by moving function bodies to the cpp file
#include "nsPIDOMWindow.h"

namespace mozilla {
class ErrorResult;

namespace dom {

class Element;

// The logical size in pixels.
// Note: if LogicalPixelSize have usages other than ResizeObserver in the
// future, it might be better to change LogicalSize into a template class, and
// use it to implement LogicalPixelSize.
class LogicalPixelSize {
 public:
  LogicalPixelSize() = default;
  LogicalPixelSize(WritingMode aWM, const gfx::Size& aSize) {
    mSize = aSize;
    if (aWM.IsVertical()) {
      std::swap(mSize.width, mSize.height);
    }
  }

  bool operator==(const LogicalPixelSize& aOther) const {
    return mSize == aOther.mSize;
  }
  bool operator!=(const LogicalPixelSize& aOther) const {
    return !(*this == aOther);
  }

  float ISize() const { return mSize.width; }
  float BSize() const { return mSize.height; }
  float& ISize() { return mSize.width; }
  float& BSize() { return mSize.height; }

 private:
  // |mSize.width| represents inline-size and |mSize.height| represents
  // block-size.
  gfx::Size mSize;
};

// For the internal implementation in ResizeObserver. Normally, this is owned by
// ResizeObserver.
class ResizeObservation final : public LinkedListElement<ResizeObservation> {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(ResizeObservation)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(ResizeObservation)

  ResizeObservation(Element&, ResizeObserver&, ResizeObserverBoxOptions,
                    WritingMode);

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
  void UpdateLastReportedSize(const gfx::Size& aSize);

  enum class RemoveFromObserver : bool { No, Yes };
  void Unlink(RemoveFromObserver);

 protected:
  ~ResizeObservation() { Unlink(RemoveFromObserver::No); };

  nsCOMPtr<Element> mTarget;

  // Weak, observer always outlives us.
  ResizeObserver* mObserver;

  const ResizeObserverBoxOptions mObservedBox;

  // The latest recorded of observed target.
  // This will be CSS pixels for border-box/content-box, or device pixels for
  // device-pixel-content-box.
  // Note: We use default constructor for this because we want to start with a
  // (0, 0) size, per the spec.
  LogicalPixelSize mLastReportedSize;
};

/**
 * ResizeObserver interfaces and algorithms are based on
 * https://drafts.csswg.org/resize-observer/#api
 */
class ResizeObserver final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ResizeObserver)

  ResizeObserver(nsCOMPtr<nsPIDOMWindowInner>&& aOwner, Document* aDocument,
                 ResizeObserverCallback& aCb)
      : mOwner(std::move(aOwner)), mDocument(aDocument), mCallback(&aCb) {
    MOZ_ASSERT(mOwner, "Need a non-null owner window");
    MOZ_ASSERT(mDocument, "Need a non-null doc");
    MOZ_ASSERT(mDocument == mOwner->GetExtantDoc());
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
  ~ResizeObserver() { Disconnect(); }

  nsCOMPtr<nsPIDOMWindowInner> mOwner;
  // The window's document at the time of ResizeObserver creation.
  RefPtr<Document> mDocument;
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

  ResizeObserverEntry(nsISupports* aOwner, Element& aTarget,
                      const gfx::Size& aBorderBoxSize,
                      const gfx::Size& aContentBoxSize,
                      const gfx::Size& aDevicePixelContentBoxSize)
      : mOwner(aOwner), mTarget(&aTarget) {
    MOZ_ASSERT(mOwner, "Need a non-null owner");
    MOZ_ASSERT(mTarget, "Need a non-null target element");

    SetBorderBoxSize(aBorderBoxSize);
    SetContentRectAndSize(aContentBoxSize);
    SetDevicePixelContentSize(aDevicePixelContentBoxSize);
  }

  nsISupports* GetParentObject() const { return mOwner; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return ResizeObserverEntry_Binding::Wrap(aCx, this, aGivenProto);
  }

  Element* Target() const { return mTarget; }

  /**
   * Returns the DOMRectReadOnly of target's content rect so it can be
   * accessed from JavaScript in callback function of ResizeObserver.
   */
  DOMRectReadOnly* ContentRect() const { return mContentRect; }

  /**
   * Returns target's logical border-box size, content-box size, and
   * device-pixel-content-box as an array of ResizeObserverSize.
   */
  void GetBorderBoxSize(nsTArray<RefPtr<ResizeObserverSize>>& aRetVal) const;
  void GetContentBoxSize(nsTArray<RefPtr<ResizeObserverSize>>& aRetVal) const;
  void GetDevicePixelContentBoxSize(
      nsTArray<RefPtr<ResizeObserverSize>>& aRetVal) const;

 private:
  ~ResizeObserverEntry() = default;

  // Set borderBoxSize.
  void SetBorderBoxSize(const gfx::Size& aSize);
  // Set contentRect and contentBoxSize.
  void SetContentRectAndSize(const gfx::Size& aSize);
  // Set devicePixelContentBoxSize.
  void SetDevicePixelContentSize(const gfx::Size& aSize);

  nsCOMPtr<nsISupports> mOwner;
  nsCOMPtr<Element> mTarget;

  RefPtr<DOMRectReadOnly> mContentRect;
  RefPtr<ResizeObserverSize> mBorderBoxSize;
  RefPtr<ResizeObserverSize> mContentBoxSize;
  RefPtr<ResizeObserverSize> mDevicePixelContentBoxSize;
};

class ResizeObserverSize final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ResizeObserverSize)

  ResizeObserverSize(nsISupports* aOwner, const gfx::Size& aSize,
                     const WritingMode aWM)
      : mOwner(aOwner), mSize(aWM, aSize) {
    MOZ_ASSERT(mOwner, "Need a non-null owner");
  }

  nsISupports* GetParentObject() const { return mOwner; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return ResizeObserverSize_Binding::Wrap(aCx, this, aGivenProto);
  }

  double InlineSize() const { return mSize.ISize(); }
  double BlockSize() const { return mSize.BSize(); }

 protected:
  ~ResizeObserverSize() = default;

  nsCOMPtr<nsISupports> mOwner;
  // The logical size value:
  // 1. content-box/border-box: in CSS pixels.
  // 2. device-pixel-content-box: in device pixels.
  const LogicalPixelSize mSize;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ResizeObserver_h

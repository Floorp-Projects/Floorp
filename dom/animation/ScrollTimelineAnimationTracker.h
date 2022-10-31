/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ScrollTimelineAnimationTracker_h
#define mozilla_ScrollTimelineAnimationTracker_h

#include "mozilla/dom/Animation.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTHashSet.h"

namespace mozilla {

namespace dom {
class Document;
}

/**
 * Handle the pending animations which use scroll timeline while playing or
 * pausing.
 */
class ScrollTimelineAnimationTracker final {
 public:
  explicit ScrollTimelineAnimationTracker(dom::Document* aDocument)
      : mDocument(aDocument) {}

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(
      ScrollTimelineAnimationTracker)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(ScrollTimelineAnimationTracker)

  void AddPending(dom::Animation& aAnimation) {
    mPendingSet.Insert(&aAnimation);
  }

  void RemovePending(dom::Animation& aAnimation) {
    mPendingSet.Remove(&aAnimation);
  }

  bool HasPendingAnimations() const { return mPendingSet.Count() > 0; }

  bool IsWaiting(const dom::Animation& aAnimation) const {
    return mPendingSet.Contains(const_cast<dom::Animation*>(&aAnimation));
  }

  void TriggerPendingAnimations();

 private:
  ~ScrollTimelineAnimationTracker() = default;

  nsTHashSet<nsRefPtrHashKey<dom::Animation>> mPendingSet;
  RefPtr<dom::Document> mDocument;
};

}  // namespace mozilla

#endif  // mozilla_ScrollTimelineAnimationTracker_h

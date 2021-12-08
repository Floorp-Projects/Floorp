/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScrollTimeline_h
#define mozilla_dom_ScrollTimeline_h

#include "mozilla/dom/AnimationTimeline.h"
#include "mozilla/ServoStyleConsts.h"

namespace mozilla {
namespace dom {

class ScrollTimeline final : public AnimationTimeline {
 public:
  ScrollTimeline() = delete;
  ScrollTimeline(Document* aDocument, Element* aScroller);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(ScrollTimeline,
                                                         AnimationTimeline)

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    // FIXME: Bug 1676794: Implement ScrollTimeline interface.
    return nullptr;
  }

  // AnimationTimeline methods.
  Nullable<TimeDuration> GetCurrentTimeAsDuration() const override {
    // FIXME: We will update this function in the patch series.
    return nullptr;
  }
  bool TracksWallclockTime() const override { return false; }
  Nullable<TimeDuration> ToTimelineTime(
      const TimeStamp& aTimeStamp) const override {
    // It's unclear to us what should we do for this function now, so return
    // nullptr.
    return nullptr;
  }
  TimeStamp ToTimeStamp(const TimeDuration& aTimelineTime) const override {
    // It's unclear to us what should we do for this function now, so return
    // zero time.
    return {};
  }
  Document* GetDocument() const override { return mDocument; }

 protected:
  virtual ~ScrollTimeline() = default;

 private:
  RefPtr<Document> mDocument;

  // FIXME: We will use these data members in the following patches.
  RefPtr<Element> mSource;
  StyleScrollDirection mDirection;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ScrollTimeline_h

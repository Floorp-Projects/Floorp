/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_TimelineManager_h
#define mozilla_TimelineManager_h

#include "mozilla/Assertions.h"
#include "mozilla/LinkedList.h"
#include "mozilla/TimelineCollection.h"
#include "nsStyleAutoArray.h"

class nsPresContext;

namespace mozilla {
class ComputedStyle;
enum class PseudoStyleType : uint8_t;

namespace dom {
class Element;
class ScrollTimeline;
class ViewTimeline;
}  // namespace dom

class TimelineManager {
 public:
  explicit TimelineManager(nsPresContext* aPresContext)
      : mPresContext(aPresContext) {}

  ~TimelineManager() {
    MOZ_ASSERT(!mPresContext, "Disconnect should have been called");
  }

  void Disconnect() {
    while (auto* head = mScrollTimelineCollections.getFirst()) {
      head->Destroy();
    }
    while (auto* head = mViewTimelineCollections.getFirst()) {
      head->Destroy();
    }

    mPresContext = nullptr;
  }

  enum class ProgressTimelineType : uint8_t {
    Scroll,
    View,
  };
  void UpdateTimelines(dom::Element* aElement, PseudoStyleType aPseudoType,
                       const ComputedStyle* aComputedStyle,
                       ProgressTimelineType aType);

  void UpdateHiddenByContentVisibilityForAnimations();

 private:
  template <typename StyleType, typename TimelineType>
  void DoUpdateTimelines(nsPresContext* aPresContext, dom::Element* aElement,
                         PseudoStyleType aPseudoType,
                         const nsStyleAutoArray<StyleType>& aStyleTimelines,
                         size_t aTimelineCount);

  template <typename T>
  void AddTimelineCollection(TimelineCollection<T>* aCollection);

  LinkedList<TimelineCollection<dom::ScrollTimeline>>
      mScrollTimelineCollections;
  LinkedList<TimelineCollection<dom::ViewTimeline>> mViewTimelineCollections;
  nsPresContext* mPresContext;
};

template <>
inline void TimelineManager::AddTimelineCollection(
    TimelineCollection<dom::ScrollTimeline>* aCollection) {
  mScrollTimelineCollections.insertBack(aCollection);
}

template <>
inline void TimelineManager::AddTimelineCollection(
    TimelineCollection<dom::ViewTimeline>* aCollection) {
  mViewTimelineCollections.insertBack(aCollection);
}

}  // namespace mozilla

#endif  // mozilla_TimelineManager_h

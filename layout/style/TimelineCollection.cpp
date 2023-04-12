/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TimelineCollection.h"

#include "mozilla/ElementAnimationData.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ScrollTimeline.h"
#include "mozilla/dom/ViewTimeline.h"
#include <type_traits>

namespace mozilla {

template <class TimelineType>
TimelineCollection<TimelineType>::~TimelineCollection() {
  MOZ_COUNT_DTOR(TimelineCollection);

  // FIXME: Bug 1774060. We have to restyle the animations which use the
  // timelines associtated with this TimelineCollection.

  LinkedListElement<SelfType>::remove();
}

template <class TimelineType>
void TimelineCollection<TimelineType>::Destroy() {
  auto* data = mElement.GetAnimationData();
  MOZ_ASSERT(data);
  if constexpr (std::is_same_v<TimelineType, dom::ScrollTimeline>) {
    MOZ_ASSERT(data->GetScrollTimelineCollection(mPseudo) == this);
    data->ClearScrollTimelineCollectionFor(mPseudo);
  } else if constexpr (std::is_same_v<TimelineType, dom::ViewTimeline>) {
    MOZ_ASSERT(data->GetViewTimelineCollection(mPseudo) == this);
    data->ClearViewTimelineCollectionFor(mPseudo);
  } else {
    MOZ_ASSERT_UNREACHABLE("Unsupported TimelienType");
  }
}

template <class TimelineType>
/* static */ TimelineCollection<TimelineType>*
TimelineCollection<TimelineType>::Get(const dom::Element* aElement,
                                      const PseudoStyleType aPseudoType) {
  MOZ_ASSERT(aElement);
  auto* data = aElement->GetAnimationData();
  if (!data) {
    return nullptr;
  }

  if constexpr (std::is_same_v<TimelineType, dom::ScrollTimeline>) {
    return data->GetScrollTimelineCollection(aPseudoType);
  }

  if constexpr (std::is_same_v<TimelineType, dom::ViewTimeline>) {
    return data->GetViewTimelineCollection(aPseudoType);
  }

  return nullptr;
}

// Explicit class instantiations
template class TimelineCollection<dom::ScrollTimeline>;
template class TimelineCollection<dom::ViewTimeline>;

}  // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TimelineCollection_h
#define mozilla_TimelineCollection_h

#include "mozilla/Assertions.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "nsAtomHashKeys.h"
#include "nsTHashMap.h"

class nsAtom;

namespace mozilla {
namespace dom {
class Element;
}
enum class PseudoStyleType : uint8_t;

// The collection of ScrollTimeline or ViewTimeline. We use the template class
// to share the implementation for these two timeline types.
template <class TimelineType>
class TimelineCollection final
    : public LinkedListElement<TimelineCollection<TimelineType>> {
 public:
  using SelfType = TimelineCollection<TimelineType>;
  using TimelineMap = nsTHashMap<RefPtr<nsAtom>, RefPtr<TimelineType>>;

  TimelineCollection(dom::Element& aElement, PseudoStyleType aPseudoType)
      : mElement(aElement), mPseudo(aPseudoType) {
    MOZ_COUNT_CTOR(TimelineCollection);
  }

  ~TimelineCollection();

  already_AddRefed<TimelineType> Lookup(nsAtom* aName) const {
    return mTimelines.Get(aName).forget();
  }

  already_AddRefed<TimelineType> Extract(nsAtom* aName) {
    Maybe<RefPtr<TimelineType>> timeline = mTimelines.Extract(aName);
    return timeline ? timeline->forget() : nullptr;
  }

  void Swap(TimelineMap& aValue) { mTimelines.SwapElements(aValue); }

  void Destroy();

  // Get the collection of timelines for the given |aElement| and or create it
  // if it does not already exist.
  static TimelineCollection* Get(const dom::Element* aElement,
                                 PseudoStyleType aPseudoType);

 private:
  // The element. Weak reference is fine since it owns us.
  dom::Element& mElement;
  const PseudoStyleType mPseudo;

  TimelineMap mTimelines;
};

}  // namespace mozilla

#endif  // mozilla_TimelineCollection_h

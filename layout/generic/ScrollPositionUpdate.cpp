/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollPositionUpdate.h"

#include "mozilla/Assertions.h"

namespace mozilla {

ScrollPositionUpdate::ScrollPositionUpdate()
    : mScrollGeneration(0),
      mType(ScrollUpdateType::Absolute),
      mScrollMode(ScrollMode::Normal),
      mScrollOrigin(ScrollOrigin::None) {}

/*static*/
ScrollPositionUpdate ScrollPositionUpdate::NewScrollframe(
    uint32_t aGeneration, nsPoint aInitialPosition) {
  ScrollPositionUpdate ret;
  ret.mScrollGeneration = aGeneration;
  ret.mScrollMode = ScrollMode::Instant;
  ret.mDestination = CSSPoint::FromAppUnits(aInitialPosition);
  return ret;
}

/*static*/
ScrollPositionUpdate ScrollPositionUpdate::NewScroll(uint32_t aGeneration,
                                                     ScrollOrigin aOrigin,
                                                     nsPoint aDestination) {
  MOZ_ASSERT(aOrigin != ScrollOrigin::NotSpecified);
  MOZ_ASSERT(aOrigin != ScrollOrigin::None);

  ScrollPositionUpdate ret;
  ret.mScrollGeneration = aGeneration;
  ret.mType = ScrollUpdateType::Absolute;
  ret.mScrollMode = ScrollMode::Instant;
  ret.mScrollOrigin = aOrigin;
  ret.mDestination = CSSPoint::FromAppUnits(aDestination);
  return ret;
}

/*static*/
ScrollPositionUpdate ScrollPositionUpdate::NewRelativeScroll(
    uint32_t aGeneration, nsPoint aSource, nsPoint aDestination) {
  ScrollPositionUpdate ret;
  ret.mScrollGeneration = aGeneration;
  ret.mType = ScrollUpdateType::Relative;
  ret.mScrollMode = ScrollMode::Instant;
  ret.mScrollOrigin = ScrollOrigin::Relative;
  ret.mDestination = CSSPoint::FromAppUnits(aDestination);
  ret.mSource = CSSPoint::FromAppUnits(aSource);
  return ret;
}

/*static*/
ScrollPositionUpdate ScrollPositionUpdate::NewSmoothScroll(
    uint32_t aGeneration, ScrollOrigin aOrigin, nsPoint aDestination) {
  MOZ_ASSERT(aOrigin != ScrollOrigin::NotSpecified);
  MOZ_ASSERT(aOrigin != ScrollOrigin::None);

  ScrollPositionUpdate ret;
  ret.mScrollGeneration = aGeneration;
  ret.mType = ScrollUpdateType::Absolute;
  ret.mScrollMode = ScrollMode::SmoothMsd;
  ret.mScrollOrigin = aOrigin;
  ret.mDestination = CSSPoint::FromAppUnits(aDestination);
  return ret;
}

/*static*/
ScrollPositionUpdate ScrollPositionUpdate::NewPureRelativeScroll(
    uint32_t aGeneration, ScrollOrigin aOrigin, ScrollMode aMode,
    const nsPoint& aDelta) {
  MOZ_ASSERT(aOrigin != ScrollOrigin::NotSpecified);
  MOZ_ASSERT(aOrigin != ScrollOrigin::None);

  ScrollPositionUpdate ret;
  ret.mScrollGeneration = aGeneration;
  ret.mType = ScrollUpdateType::PureRelative;
  ret.mScrollMode = aMode;
  ret.mScrollOrigin = aOrigin;
  ret.mDelta = CSSPoint::FromAppUnits(aDelta);
  return ret;
}

bool ScrollPositionUpdate::operator==(
    const ScrollPositionUpdate& aOther) const {
  // instances are immutable, and all the fields are set when the generation
  // is set. So if the generation matches, these instances are identical.
  return mScrollGeneration == aOther.mScrollGeneration;
}

uint32_t ScrollPositionUpdate::GetGeneration() const {
  return mScrollGeneration;
}

ScrollUpdateType ScrollPositionUpdate::GetType() const { return mType; }

ScrollMode ScrollPositionUpdate::GetMode() const { return mScrollMode; }

ScrollOrigin ScrollPositionUpdate::GetOrigin() const { return mScrollOrigin; }

CSSPoint ScrollPositionUpdate::GetDestination() const {
  MOZ_ASSERT(mType == ScrollUpdateType::Absolute ||
             mType == ScrollUpdateType::Relative);
  return mDestination;
}

CSSPoint ScrollPositionUpdate::GetSource() const {
  MOZ_ASSERT(mType == ScrollUpdateType::Relative);
  return mSource;
}

CSSPoint ScrollPositionUpdate::GetDelta() const {
  MOZ_ASSERT(mType == ScrollUpdateType::PureRelative);
  return mDelta;
}

void ScrollPositionUpdate::AppendToString(std::stringstream& aStream) const {
  aStream << "ScrollPositionUpdate(gen=" << mScrollGeneration
          << ", type=" << (int)mType << ", mode=" << (int)mScrollMode
          << ", origin=" << (int)mScrollOrigin << ", dst=" << mDestination.x
          << "," << mDestination.y << ", src=" << mSource.x << "," << mSource.y
          << ", delta=" << mDelta.x << "," << mDelta.y << ")";
}

}  // namespace mozilla

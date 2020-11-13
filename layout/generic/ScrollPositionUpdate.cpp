/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollPositionUpdate.h"

#include <ostream>

#include "mozilla/Assertions.h"

namespace mozilla {

uint64_t ScrollGeneration::sCounter = 0;

ScrollGeneration ScrollGeneration::New() {
  uint64_t value = ++sCounter;
  return ScrollGeneration(value);
}

ScrollGeneration::ScrollGeneration() : mValue(0) {}

ScrollGeneration::ScrollGeneration(uint64_t aValue) : mValue(aValue) {}

bool ScrollGeneration::operator<(const ScrollGeneration& aOther) const {
  return mValue < aOther.mValue;
}

bool ScrollGeneration::operator==(const ScrollGeneration& aOther) const {
  return mValue == aOther.mValue;
}

bool ScrollGeneration::operator!=(const ScrollGeneration& aOther) const {
  return !(*this == aOther);
}

std::ostream& operator<<(std::ostream& aStream, const ScrollGeneration& aGen) {
  return aStream << aGen.mValue;
}

ScrollPositionUpdate::ScrollPositionUpdate()
    : mType(ScrollUpdateType::Absolute),
      mScrollMode(ScrollMode::Normal),
      mScrollOrigin(ScrollOrigin::None) {}

/*static*/
ScrollPositionUpdate ScrollPositionUpdate::NewScrollframe(
    nsPoint aInitialPosition) {
  ScrollPositionUpdate ret;
  ret.mScrollGeneration = ScrollGeneration::New();
  ret.mScrollMode = ScrollMode::Instant;
  ret.mDestination = CSSPoint::FromAppUnits(aInitialPosition);
  return ret;
}

/*static*/
ScrollPositionUpdate ScrollPositionUpdate::NewScroll(ScrollOrigin aOrigin,
                                                     nsPoint aDestination) {
  MOZ_ASSERT(aOrigin != ScrollOrigin::NotSpecified);
  MOZ_ASSERT(aOrigin != ScrollOrigin::None);

  ScrollPositionUpdate ret;
  ret.mScrollGeneration = ScrollGeneration::New();
  ret.mType = ScrollUpdateType::Absolute;
  ret.mScrollMode = ScrollMode::Instant;
  ret.mScrollOrigin = aOrigin;
  ret.mDestination = CSSPoint::FromAppUnits(aDestination);
  return ret;
}

/*static*/
ScrollPositionUpdate ScrollPositionUpdate::NewRelativeScroll(
    nsPoint aSource, nsPoint aDestination) {
  ScrollPositionUpdate ret;
  ret.mScrollGeneration = ScrollGeneration::New();
  ret.mType = ScrollUpdateType::Relative;
  ret.mScrollMode = ScrollMode::Instant;
  ret.mScrollOrigin = ScrollOrigin::Relative;
  ret.mDestination = CSSPoint::FromAppUnits(aDestination);
  ret.mSource = CSSPoint::FromAppUnits(aSource);
  return ret;
}

/*static*/
ScrollPositionUpdate ScrollPositionUpdate::NewSmoothScroll(
    ScrollOrigin aOrigin, nsPoint aDestination) {
  MOZ_ASSERT(aOrigin != ScrollOrigin::NotSpecified);
  MOZ_ASSERT(aOrigin != ScrollOrigin::None);

  ScrollPositionUpdate ret;
  ret.mScrollGeneration = ScrollGeneration::New();
  ret.mType = ScrollUpdateType::Absolute;
  ret.mScrollMode = ScrollMode::SmoothMsd;
  ret.mScrollOrigin = aOrigin;
  ret.mDestination = CSSPoint::FromAppUnits(aDestination);
  return ret;
}

/*static*/
ScrollPositionUpdate ScrollPositionUpdate::NewPureRelativeScroll(
    ScrollOrigin aOrigin, ScrollMode aMode, const nsPoint& aDelta) {
  MOZ_ASSERT(aOrigin != ScrollOrigin::NotSpecified);
  MOZ_ASSERT(aOrigin != ScrollOrigin::None);

  ScrollPositionUpdate ret;
  ret.mScrollGeneration = ScrollGeneration::New();
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

ScrollGeneration ScrollPositionUpdate::GetGeneration() const {
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

std::ostream& operator<<(std::ostream& aStream,
                         const ScrollPositionUpdate& aUpdate) {
  aStream << "{ gen=" << aUpdate.mScrollGeneration
          << ", type=" << (int)aUpdate.mType
          << ", mode=" << (int)aUpdate.mScrollMode
          << ", origin=" << (int)aUpdate.mScrollOrigin
          << ", dst=" << aUpdate.mDestination << ", src=" << aUpdate.mSource
          << ", delta=" << aUpdate.mDelta << " }";
  return aStream;
}

}  // namespace mozilla

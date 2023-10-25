/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollPositionUpdate.h"

#include <ostream>

#include "mozilla/Assertions.h"

namespace mozilla {

static ScrollGenerationCounter sGenerationCounter;

ScrollPositionUpdate::ScrollPositionUpdate()
    : mType(ScrollUpdateType::Absolute),
      mScrollMode(ScrollMode::Normal),
      mScrollOrigin(ScrollOrigin::None),
      mTriggeredByScript(ScrollTriggeredByScript::No) {}

/*static*/
ScrollPositionUpdate ScrollPositionUpdate::NewScrollframe(
    nsPoint aInitialPosition) {
  ScrollPositionUpdate ret;
  ret.mScrollGeneration = sGenerationCounter.NewMainThreadGeneration();
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
  ret.mScrollGeneration = sGenerationCounter.NewMainThreadGeneration();
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
  ret.mScrollGeneration = sGenerationCounter.NewMainThreadGeneration();
  ret.mType = ScrollUpdateType::Relative;
  ret.mScrollMode = ScrollMode::Instant;
  ret.mScrollOrigin = ScrollOrigin::Relative;
  ret.mDestination = CSSPoint::FromAppUnits(aDestination);
  ret.mSource = CSSPoint::FromAppUnits(aSource);
  return ret;
}

/*static*/
ScrollPositionUpdate ScrollPositionUpdate::NewSmoothScroll(
    ScrollMode aMode, ScrollOrigin aOrigin, nsPoint aDestination,
    ScrollTriggeredByScript aTriggeredByScript,
    UniquePtr<ScrollSnapTargetIds> aSnapTargetIds) {
  MOZ_ASSERT(aOrigin != ScrollOrigin::NotSpecified);
  MOZ_ASSERT(aOrigin != ScrollOrigin::None);
  MOZ_ASSERT(aMode == ScrollMode::Smooth || aMode == ScrollMode::SmoothMsd);

  ScrollPositionUpdate ret;
  ret.mScrollGeneration = sGenerationCounter.NewMainThreadGeneration();
  ret.mType = ScrollUpdateType::Absolute;
  ret.mScrollMode = aMode;
  ret.mScrollOrigin = aOrigin;
  ret.mDestination = CSSPoint::FromAppUnits(aDestination);
  ret.mTriggeredByScript = aTriggeredByScript;
  if (aSnapTargetIds) {
    ret.mSnapTargetIds = *aSnapTargetIds;
  }
  return ret;
}

/*static*/
ScrollPositionUpdate ScrollPositionUpdate::NewPureRelativeScroll(
    ScrollOrigin aOrigin, ScrollMode aMode, const nsPoint& aDelta) {
  MOZ_ASSERT(aOrigin != ScrollOrigin::NotSpecified);
  MOZ_ASSERT(aOrigin != ScrollOrigin::None);

  ScrollPositionUpdate ret;
  ret.mScrollGeneration = sGenerationCounter.NewMainThreadGeneration();
  ret.mType = ScrollUpdateType::PureRelative;
  ret.mScrollMode = aMode;
  ret.mScrollOrigin = aOrigin;
  ret.mDelta = CSSPoint::FromAppUnits(aDelta);
  return ret;
}

/*static*/
ScrollPositionUpdate ScrollPositionUpdate::NewMergeableScroll(
    ScrollOrigin aOrigin, nsPoint aDestination) {
  MOZ_ASSERT(aOrigin == ScrollOrigin::AnchorAdjustment);

  ScrollPositionUpdate ret;
  ret.mScrollGeneration = sGenerationCounter.NewMainThreadGeneration();
  ret.mType = ScrollUpdateType::MergeableAbsolute;
  ret.mScrollMode = ScrollMode::Instant;
  ret.mScrollOrigin = aOrigin;
  ret.mDestination = CSSPoint::FromAppUnits(aDestination);
  return ret;
}

bool ScrollPositionUpdate::operator==(
    const ScrollPositionUpdate& aOther) const {
  // instances are immutable, and all the fields are set when the generation
  // is set. So if the generation matches, these instances are identical.
  return mScrollGeneration == aOther.mScrollGeneration;
}

MainThreadScrollGeneration ScrollPositionUpdate::GetGeneration() const {
  return mScrollGeneration;
}

ScrollUpdateType ScrollPositionUpdate::GetType() const { return mType; }

ScrollMode ScrollPositionUpdate::GetMode() const { return mScrollMode; }

ScrollOrigin ScrollPositionUpdate::GetOrigin() const { return mScrollOrigin; }

CSSPoint ScrollPositionUpdate::GetDestination() const {
  MOZ_ASSERT(mType == ScrollUpdateType::Absolute ||
             mType == ScrollUpdateType::MergeableAbsolute ||
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
          << ", delta=" << aUpdate.mDelta
          << ", triggered by script=" << aUpdate.WasTriggeredByScript() << " }";
  return aStream;
}

}  // namespace mozilla

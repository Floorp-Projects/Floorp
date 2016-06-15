/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTrackConstraints.h"

#include <limits>
#include <algorithm>
#include <iterator>

namespace mozilla {

template<class ValueType>
template<class ConstrainRange>
void
NormalizedConstraintSet::Range<ValueType>::SetFrom(const ConstrainRange& aOther)
{
  if (aOther.mIdeal.WasPassed()) {
    mIdeal.emplace(aOther.mIdeal.Value());
  }
  if (aOther.mExact.WasPassed()) {
    mMin = aOther.mExact.Value();
    mMax = aOther.mExact.Value();
  } else {
    if (aOther.mMin.WasPassed()) {
      mMin = aOther.mMin.Value();
    }
    if (aOther.mMax.WasPassed()) {
      mMax = aOther.mMax.Value();
    }
  }
}

// The Range code works surprisingly well for bool, except when averaging ideals.
template<>
bool
NormalizedConstraintSet::Range<bool>::Merge(const Range& aOther) {
  if (!Intersects(aOther)) {
    return false;
  }
  Intersect(aOther);

  // To avoid "unsafe use of type 'bool'", we keep counter in mMergeDenominator
  uint32_t counter = mMergeDenominator >> 16;
  uint32_t denominator = mMergeDenominator & 0xffff;

  if (aOther.mIdeal.isSome()) {
    if (mIdeal.isNothing()) {
      mIdeal.emplace(aOther.mIdeal.value());
      counter = aOther.mIdeal.value();
      denominator = 1;
    } else {
      if (!denominator) {
        counter = mIdeal.value();
      }
      counter += aOther.mIdeal.value();
      denominator = std::max(2U, denominator + 1);
    }
  }
  mMergeDenominator = ((counter & 0xffff) << 16) + (denominator & 0xffff);
  return true;
}

template<>
void
NormalizedConstraintSet::Range<bool>::FinalizeMerge()
{
  if (mMergeDenominator) {
    uint32_t counter = mMergeDenominator >> 16;
    uint32_t denominator = mMergeDenominator & 0xffff;

    *mIdeal = !!(counter / denominator);
    mMergeDenominator = 0;
  }
}

NormalizedConstraintSet::LongRange::LongRange(
    const dom::OwningLongOrConstrainLongRange& aOther, bool advanced)
: Range<int32_t>(1 + INT32_MIN, INT32_MAX) // +1 avoids Windows compiler bug
{
  if (aOther.IsLong()) {
    if (advanced) {
      mMin = mMax = aOther.GetAsLong();
    } else {
      mIdeal.emplace(aOther.GetAsLong());
    }
  } else {
    SetFrom(aOther.GetAsConstrainLongRange());
  }
}

NormalizedConstraintSet::DoubleRange::DoubleRange(
    const dom::OwningDoubleOrConstrainDoubleRange& aOther, bool advanced)
: Range<double>(-std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::infinity())
{
  if (aOther.IsDouble()) {
    if (advanced) {
      mMin = mMax = aOther.GetAsDouble();
    } else {
      mIdeal.emplace(aOther.GetAsDouble());
    }
  } else {
    SetFrom(aOther.GetAsConstrainDoubleRange());
  }
}

NormalizedConstraintSet::BooleanRange::BooleanRange(
    const dom::OwningBooleanOrConstrainBooleanParameters& aOther, bool advanced)
: Range<bool>(false, true)
{
  if (aOther.IsBoolean()) {
    if (advanced) {
      mMin = mMax = aOther.GetAsBoolean();
    } else {
      mIdeal.emplace(aOther.GetAsBoolean());
    }
  } else {
    const ConstrainBooleanParameters& r = aOther.GetAsConstrainBooleanParameters();
    if (r.mIdeal.WasPassed()) {
      mIdeal.emplace(r.mIdeal.Value());
    }
    if (r.mExact.WasPassed()) {
      mMin = r.mExact.Value();
      mMax = r.mExact.Value();
    }
  }
}

NormalizedConstraintSet::StringRange::StringRange(
    const dom::OwningStringOrStringSequenceOrConstrainDOMStringParameters& aOther,
    bool advanced)
{
  if (aOther.IsString()) {
    if (advanced) {
      mExact.insert(aOther.GetAsString());
    } else {
      mIdeal.insert(aOther.GetAsString());
    }
  } else if (aOther.IsStringSequence()) {
    if (advanced) {
      mExact.clear();
      for (auto& str : aOther.GetAsStringSequence()) {
        mExact.insert(str);
      }
    } else {
      mIdeal.clear();
      for (auto& str : aOther.GetAsStringSequence()) {
        mIdeal.insert(str);
      }
    }
  } else {
    SetFrom(aOther.GetAsConstrainDOMStringParameters());
  }
}

void
NormalizedConstraintSet::StringRange::SetFrom(
    const ConstrainDOMStringParameters& aOther)
{
  if (aOther.mIdeal.WasPassed()) {
    mIdeal.clear();
    if (aOther.mIdeal.Value().IsString()) {
      mIdeal.insert(aOther.mIdeal.Value().GetAsString());
    } else {
      for (auto& str : aOther.mIdeal.Value().GetAsStringSequence()) {
        mIdeal.insert(str);
      }
    }
  }
  if (aOther.mExact.WasPassed()) {
    mExact.clear();
    if (aOther.mExact.Value().IsString()) {
      mExact.insert(aOther.mExact.Value().GetAsString());
    } else {
      for (auto& str : aOther.mExact.Value().GetAsStringSequence()) {
        mIdeal.insert(str);
      }
    }
  }
}

auto
NormalizedConstraintSet::StringRange::Clamp(const ValueType& n) const -> ValueType
{
  if (!mExact.size()) {
    return n;
  }
  ValueType result;
  for (auto& entry : n) {
    if (mExact.find(entry) != mExact.end()) {
      result.insert(entry);
    }
  }
  return result;
}

bool
NormalizedConstraintSet::StringRange::Intersects(const StringRange& aOther) const
{
  if (!mExact.size() || !aOther.mExact.size()) {
    return true;
  }

  ValueType intersection;
  set_intersection(mExact.begin(), mExact.end(),
                   aOther.mExact.begin(), aOther.mExact.end(),
                   std::inserter(intersection, intersection.begin()));
  return !!intersection.size();
}

void
NormalizedConstraintSet::StringRange::Intersect(const StringRange& aOther)
{
  if (!aOther.mExact.size()) {
    return;
  }

  ValueType intersection;
  set_intersection(mExact.begin(), mExact.end(),
                   aOther.mExact.begin(), aOther.mExact.end(),
                   std::inserter(intersection, intersection.begin()));
  mExact = intersection;
}

bool
NormalizedConstraintSet::StringRange::Merge(const StringRange& aOther)
{
  if (!Intersects(aOther)) {
    return false;
  }
  Intersect(aOther);

  ValueType unioned;
  set_union(mIdeal.begin(), mIdeal.end(),
            aOther.mIdeal.begin(), aOther.mIdeal.end(),
            std::inserter(unioned, unioned.begin()));
  mIdeal = unioned;
  return true;
}

NormalizedConstraints::NormalizedConstraints(const dom::MediaTrackConstraints& aOther)
: NormalizedConstraintSet(aOther, false), mBadConstraint(nullptr)
{
  if (aOther.mAdvanced.WasPassed()) {
    for (auto& entry : aOther.mAdvanced.Value()) {
      mAdvanced.AppendElement(NormalizedConstraintSet(entry, true));
    }
  }
}

// Merge constructor. Create net constraints out of merging a set of others.

NormalizedConstraints::NormalizedConstraints(
    const nsTArray<const NormalizedConstraints*>& aOthers)
  : NormalizedConstraintSet(*aOthers[0])
  , mBadConstraint(nullptr)
{
  // Do intersection of all required constraints, and average of ideals.

  for (uint32_t i = 1; i < aOthers.Length(); i++) {
    auto& set = *aOthers[i];

    if (!mWidth.Merge(set.mWidth)) {
      mBadConstraint = "width";
      return;
    }
    if (!mHeight.Merge(set.mHeight)) {
      mBadConstraint = "height";
      return;
    }
    if (!mFrameRate.Merge(set.mFrameRate)) {
      mBadConstraint = "frameRate";
      return;
    }
    if (!mFacingMode.Merge(set.mFacingMode)) {
      mBadConstraint = "facingMode";
      return;
    }
    if (mMediaSource != set.mMediaSource) {
      mBadConstraint = "mediaSource";
      return;
    }
    if (mBrowserWindow != set.mBrowserWindow) {
      mBadConstraint = "browserWindow";
      return;
    }
    if (!mViewportOffsetX.Merge(set.mViewportOffsetX)) {
      mBadConstraint = "viewportOffsetX";
      return;
    }
    if (!mViewportOffsetY.Merge(set.mViewportOffsetY)) {
      mBadConstraint = "viewportOffsetY";
      return;
    }
    if (!mViewportWidth.Merge(set.mViewportWidth)) {
      mBadConstraint = "viewportWidth";
      return;
    }
    if (!mViewportHeight.Merge(set.mViewportHeight)) {
      mBadConstraint = "viewportHeight";
      return;
    }
    if (!mEchoCancellation.Merge(set.mEchoCancellation)) {
      mBadConstraint = "echoCancellation";
      return;
    }
    if (!mMozNoiseSuppression.Merge(set.mMozNoiseSuppression)) {
      mBadConstraint = "mozNoiseSuppression";
      return;
    }
    if (!mMozAutoGainControl.Merge(set.mMozAutoGainControl)) {
      mBadConstraint = "mozAutoGainControl";
      return;
    }

    for (auto& entry : set.mAdvanced) {
      mAdvanced.AppendElement(entry);
    }
  }
  mWidth.FinalizeMerge();
  mHeight.FinalizeMerge();
  mFrameRate.FinalizeMerge();
  mFacingMode.FinalizeMerge();
  mViewportOffsetX.FinalizeMerge();
  mViewportOffsetY.FinalizeMerge();
  mViewportWidth.FinalizeMerge();
  mViewportHeight.FinalizeMerge();
  mEchoCancellation.FinalizeMerge();
  mMozNoiseSuppression.FinalizeMerge();
  mMozAutoGainControl.FinalizeMerge();
}

FlattenedConstraints::FlattenedConstraints(const NormalizedConstraints& aOther)
: NormalizedConstraintSet(aOther)
{
  for (auto& set : aOther.mAdvanced) {
    // Must only apply compatible i.e. inherently non-overconstraining sets
    // This rule is pretty much why this code is centralized here.
    if (mWidth.Intersects(set.mWidth) &&
        mHeight.Intersects(set.mHeight) &&
        mFrameRate.Intersects(set.mFrameRate)) {
      mWidth.Intersect(set.mWidth);
      mHeight.Intersect(set.mHeight);
      mFrameRate.Intersect(set.mFrameRate);
    }
    if (mEchoCancellation.Intersects(set.mEchoCancellation)) {
        mEchoCancellation.Intersect(set.mEchoCancellation);
    }
    if (mMozNoiseSuppression.Intersects(set.mMozNoiseSuppression)) {
        mMozNoiseSuppression.Intersect(set.mMozNoiseSuppression);
    }
    if (mMozAutoGainControl.Intersects(set.mMozAutoGainControl)) {
        mMozAutoGainControl.Intersect(set.mMozAutoGainControl);
    }
  }
}

// MediaEngine helper
//
// The full algorithm for all devices. Sources that don't list capabilities
// need to fake it and hardcode some by populating mHardcodedCapabilities above.
//
// Fitness distance returned as integer math * 1000. Infinity = UINT32_MAX

// First, all devices have a minimum distance based on their deviceId.
// If you have no other constraints, use this one. Reused by all device types.

uint32_t
MediaConstraintsHelper::GetMinimumFitnessDistance(
    const dom::MediaTrackConstraintSet &aConstraints,
    bool aAdvanced,
    const nsString& aDeviceId)
{
  NormalizedConstraintSet ns(aConstraints, aAdvanced);

  return FitnessDistance(aDeviceId, ns.mDeviceId);
}

template<class ValueType, class NormalizedRange>
/* static */ uint32_t
MediaConstraintsHelper::FitnessDistance(ValueType aN,
                                        const NormalizedRange& aRange)
{
  if (aRange.mMin > aN || aRange.mMax < aN) {
    return UINT32_MAX;
  }
  if (aN == aRange.mIdeal.valueOr(aN)) {
    return 0;
  }
  return uint32_t(ValueType((std::abs(aN - aRange.mIdeal.value()) * 1000) /
                            std::max(std::abs(aN), std::abs(aRange.mIdeal.value()))));
}

// Fitness distance returned as integer math * 1000. Infinity = UINT32_MAX

/* static */ uint32_t
MediaConstraintsHelper::FitnessDistance(
    nsString aN,
    const NormalizedConstraintSet::StringRange& aParams)
{
  if (aParams.mExact.size() && aParams.mExact.find(aN) == aParams.mExact.end()) {
    return UINT32_MAX;
  }
  if (aParams.mIdeal.size() && aParams.mIdeal.find(aN) == aParams.mIdeal.end()) {
    return 1000;
  }
  return 0;
}

}

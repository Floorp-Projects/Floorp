/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTrackConstraints.h"

#include <limits>
#include <algorithm>
#include <iterator>

#include "MediaEngineSource.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/MediaManager.h"

#ifdef MOZ_WEBRTC
extern mozilla::LazyLogModule gMediaManagerLog;
#else
static mozilla::LazyLogModule gMediaManagerLog("MediaManager");
#endif
#define LOG(...) MOZ_LOG(gMediaManagerLog, LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {

using dom::CallerType;
using dom::ConstrainBooleanParameters;

template <class ValueType>
template <class ConstrainRange>
void NormalizedConstraintSet::Range<ValueType>::SetFrom(
    const ConstrainRange& aOther) {
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

// The Range code works surprisingly well for bool, except when averaging
// ideals.
template <>
bool NormalizedConstraintSet::Range<bool>::Merge(const Range& aOther) {
  if (!Intersects(aOther)) {
    return false;
  }
  Intersect(aOther);

  // To avoid "unsafe use of type 'bool'", we keep counter in mMergeDenominator
  uint32_t counter = mMergeDenominator >> 16;
  uint32_t denominator = mMergeDenominator & 0xffff;

  if (aOther.mIdeal.isSome()) {
    if (mIdeal.isNothing()) {
      mIdeal.emplace(aOther.Get(false));
      counter = aOther.Get(false);
      denominator = 1;
    } else {
      if (!denominator) {
        counter = Get(false);
        denominator = 1;
      }
      counter += aOther.Get(false);
      denominator++;
    }
  }
  mMergeDenominator = ((counter & 0xffff) << 16) + (denominator & 0xffff);
  return true;
}

template <>
void NormalizedConstraintSet::Range<bool>::FinalizeMerge() {
  if (mMergeDenominator) {
    uint32_t counter = mMergeDenominator >> 16;
    uint32_t denominator = mMergeDenominator & 0xffff;

    *mIdeal = !!(counter / denominator);
    mMergeDenominator = 0;
  }
}

NormalizedConstraintSet::LongRange::LongRange(
    LongPtrType aMemberPtr, const char* aName,
    const dom::Optional<dom::OwningLongOrConstrainLongRange>& aOther,
    bool advanced, nsTArray<MemberPtrType>* aList)
    : Range<int32_t>((MemberPtrType)aMemberPtr, aName, 1 + INT32_MIN,
                     INT32_MAX,  // +1 avoids Windows compiler bug
                     aList) {
  if (!aOther.WasPassed()) {
    return;
  }
  auto& other = aOther.Value();
  if (other.IsLong()) {
    if (advanced) {
      mMin = mMax = other.GetAsLong();
    } else {
      mIdeal.emplace(other.GetAsLong());
    }
  } else {
    SetFrom(other.GetAsConstrainLongRange());
  }
}

NormalizedConstraintSet::LongLongRange::LongLongRange(
    LongLongPtrType aMemberPtr, const char* aName, const long long& aOther,
    nsTArray<MemberPtrType>* aList)
    : Range<int64_t>((MemberPtrType)aMemberPtr, aName, 1 + INT64_MIN,
                     INT64_MAX,  // +1 avoids Windows compiler bug
                     aList) {
  mIdeal.emplace(aOther);
}

NormalizedConstraintSet::DoubleRange::DoubleRange(
    DoublePtrType aMemberPtr, const char* aName,
    const dom::Optional<dom::OwningDoubleOrConstrainDoubleRange>& aOther,
    bool advanced, nsTArray<MemberPtrType>* aList)
    : Range<double>((MemberPtrType)aMemberPtr, aName,
                    -std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::infinity(), aList) {
  if (!aOther.WasPassed()) {
    return;
  }
  auto& other = aOther.Value();
  if (other.IsDouble()) {
    if (advanced) {
      mMin = mMax = other.GetAsDouble();
    } else {
      mIdeal.emplace(other.GetAsDouble());
    }
  } else {
    SetFrom(other.GetAsConstrainDoubleRange());
  }
}

NormalizedConstraintSet::BooleanRange::BooleanRange(
    BooleanPtrType aMemberPtr, const char* aName,
    const dom::Optional<dom::OwningBooleanOrConstrainBooleanParameters>& aOther,
    bool advanced, nsTArray<MemberPtrType>* aList)
    : Range<bool>((MemberPtrType)aMemberPtr, aName, false, true, aList) {
  if (!aOther.WasPassed()) {
    return;
  }
  auto& other = aOther.Value();
  if (other.IsBoolean()) {
    if (advanced) {
      mMin = mMax = other.GetAsBoolean();
    } else {
      mIdeal.emplace(other.GetAsBoolean());
    }
  } else {
    auto& r = other.GetAsConstrainBooleanParameters();
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
    StringPtrType aMemberPtr, const char* aName,
    const dom::Optional<
        dom::OwningStringOrStringSequenceOrConstrainDOMStringParameters>&
        aOther,
    bool advanced, nsTArray<MemberPtrType>* aList)
    : BaseRange((MemberPtrType)aMemberPtr, aName, aList) {
  if (!aOther.WasPassed()) {
    return;
  }
  auto& other = aOther.Value();
  if (other.IsString()) {
    if (advanced) {
      mExact.insert(other.GetAsString());
    } else {
      mIdeal.insert(other.GetAsString());
    }
  } else if (other.IsStringSequence()) {
    if (advanced) {
      mExact.clear();
      for (auto& str : other.GetAsStringSequence()) {
        mExact.insert(str);
      }
    } else {
      mIdeal.clear();
      for (auto& str : other.GetAsStringSequence()) {
        mIdeal.insert(str);
      }
    }
  } else {
    SetFrom(other.GetAsConstrainDOMStringParameters());
  }
}

void NormalizedConstraintSet::StringRange::SetFrom(
    const dom::ConstrainDOMStringParameters& aOther) {
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
        mExact.insert(str);
      }
    }
  }
}

auto NormalizedConstraintSet::StringRange::Clamp(const ValueType& n) const
    -> ValueType {
  if (mExact.empty()) {
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

bool NormalizedConstraintSet::StringRange::Intersects(
    const StringRange& aOther) const {
  if (mExact.empty() || aOther.mExact.empty()) {
    return true;
  }

  ValueType intersection;
  set_intersection(mExact.begin(), mExact.end(), aOther.mExact.begin(),
                   aOther.mExact.end(),
                   std::inserter(intersection, intersection.begin()));
  return !intersection.empty();
}

void NormalizedConstraintSet::StringRange::Intersect(
    const StringRange& aOther) {
  if (aOther.mExact.empty()) {
    return;
  }

  ValueType intersection;
  set_intersection(mExact.begin(), mExact.end(), aOther.mExact.begin(),
                   aOther.mExact.end(),
                   std::inserter(intersection, intersection.begin()));
  mExact = intersection;
}

bool NormalizedConstraintSet::StringRange::Merge(const StringRange& aOther) {
  if (!Intersects(aOther)) {
    return false;
  }
  Intersect(aOther);

  ValueType unioned;
  set_union(mIdeal.begin(), mIdeal.end(), aOther.mIdeal.begin(),
            aOther.mIdeal.end(), std::inserter(unioned, unioned.begin()));
  mIdeal = unioned;
  return true;
}

NormalizedConstraints::NormalizedConstraints(
    const dom::MediaTrackConstraints& aOther, nsTArray<MemberPtrType>* aList)
    : NormalizedConstraintSet(aOther, false, aList), mBadConstraint(nullptr) {
  if (aOther.mAdvanced.WasPassed()) {
    for (auto& entry : aOther.mAdvanced.Value()) {
      mAdvanced.push_back(NormalizedConstraintSet(entry, true));
    }
  }
}

FlattenedConstraints::FlattenedConstraints(const NormalizedConstraints& aOther)
    : NormalizedConstraintSet(aOther) {
  for (auto& set : aOther.mAdvanced) {
    // Must only apply compatible i.e. inherently non-overconstraining sets
    // This rule is pretty much why this code is centralized here.
    if (mWidth.Intersects(set.mWidth) && mHeight.Intersects(set.mHeight) &&
        mFrameRate.Intersects(set.mFrameRate)) {
      mWidth.Intersect(set.mWidth);
      mHeight.Intersect(set.mHeight);
      mFrameRate.Intersect(set.mFrameRate);
    }
    if (mEchoCancellation.Intersects(set.mEchoCancellation)) {
      mEchoCancellation.Intersect(set.mEchoCancellation);
    }
    if (mNoiseSuppression.Intersects(set.mNoiseSuppression)) {
      mNoiseSuppression.Intersect(set.mNoiseSuppression);
    }
    if (mAutoGainControl.Intersects(set.mAutoGainControl)) {
      mAutoGainControl.Intersect(set.mAutoGainControl);
    }
    if (mChannelCount.Intersects(set.mChannelCount)) {
      mChannelCount.Intersect(set.mChannelCount);
    }
  }
}

// MediaEngine helper
//
// The full algorithm for all devices.
//
// Fitness distance returned as integer math * 1000. Infinity = UINT32_MAX

// First, all devices have a minimum distance based on their deviceId.
// If you have no other constraints, use this one. Reused by all device types.

/* static */
bool MediaConstraintsHelper::SomeSettingsFit(
    const NormalizedConstraints& aConstraints,
    const nsTArray<RefPtr<MediaDevice>>& aDevices) {
  nsTArray<const NormalizedConstraintSet*> sets;
  sets.AppendElement(&aConstraints);

  MOZ_ASSERT(!aDevices.IsEmpty());
  for (auto& device : aDevices) {
    auto distance = device->GetBestFitnessDistance(sets, CallerType::NonSystem);
    if (distance != UINT32_MAX) {
      return true;
    }
  }
  return false;
}

template <class ValueType, class NormalizedRange>
/* static */
uint32_t MediaConstraintsHelper::FitnessDistance(
    ValueType aN, const NormalizedRange& aRange) {
  if (aRange.mMin > aN || aRange.mMax < aN) {
    return UINT32_MAX;
  }
  if (aN == aRange.mIdeal.valueOr(aN)) {
    return 0;
  }
  return uint32_t(
      ValueType((std::abs(aN - aRange.mIdeal.value()) * 1000) /
                std::max(std::abs(aN), std::abs(aRange.mIdeal.value()))));
}

template <class ValueType, class NormalizedRange>
/* static */
uint32_t MediaConstraintsHelper::FeasibilityDistance(
    ValueType aN, const NormalizedRange& aRange) {
  if (aRange.mMin > aN) {
    return UINT32_MAX;
  }
  // We prefer larger resolution because now we support downscaling
  if (aN == aRange.mIdeal.valueOr(aN)) {
    return 0;
  }

  if (aN > aRange.mIdeal.value()) {
    return uint32_t(
        ValueType((std::abs(aN - aRange.mIdeal.value()) * 1000) /
                  std::max(std::abs(aN), std::abs(aRange.mIdeal.value()))));
  }

  return 10000 + uint32_t(ValueType(
                     (std::abs(aN - aRange.mIdeal.value()) * 1000) /
                     std::max(std::abs(aN), std::abs(aRange.mIdeal.value()))));
}

// Fitness distance returned as integer math * 1000. Infinity = UINT32_MAX

/* static */
uint32_t MediaConstraintsHelper::FitnessDistance(
    const Maybe<nsString>& aN,
    const NormalizedConstraintSet::StringRange& aParams) {
  if (!aParams.mExact.empty() &&
      (aN.isNothing() || aParams.mExact.find(*aN) == aParams.mExact.end())) {
    return UINT32_MAX;
  }
  if (!aParams.mIdeal.empty() &&
      (aN.isNothing() || aParams.mIdeal.find(*aN) == aParams.mIdeal.end())) {
    return 1000;
  }
  return 0;
}

/* static */ const char* MediaConstraintsHelper::SelectSettings(
    const NormalizedConstraints& aConstraints,
    nsTArray<RefPtr<MediaDevice>>& aDevices, CallerType aCallerType) {
  auto& c = aConstraints;
  LogConstraints(c);

  // First apply top-level constraints.

  // Stack constraintSets that pass, starting with the required one, because the
  // whole stack must be re-satisfied each time a capability-set is ruled out
  // (this avoids storing state or pushing algorithm into the lower-level code).
  nsTArray<RefPtr<MediaDevice>> unsatisfactory;
  nsTArray<const NormalizedConstraintSet*> aggregateConstraints;
  aggregateConstraints.AppendElement(&c);

  std::multimap<uint32_t, RefPtr<MediaDevice>> ordered;

  for (uint32_t i = 0; i < aDevices.Length();) {
    uint32_t distance =
        aDevices[i]->GetBestFitnessDistance(aggregateConstraints, aCallerType);
    if (distance == UINT32_MAX) {
      unsatisfactory.AppendElement(std::move(aDevices[i]));
      aDevices.RemoveElementAt(i);
    } else {
      ordered.insert(std::make_pair(distance, aDevices[i]));
      ++i;
    }
  }
  if (aDevices.IsEmpty()) {
    return FindBadConstraint(c, unsatisfactory);
  }

  // Order devices by shortest distance
  for (auto& ordinal : ordered) {
    aDevices.RemoveElement(ordinal.second);
    aDevices.AppendElement(ordinal.second);
  }

  // Then apply advanced constraints.

  for (const auto& advanced : c.mAdvanced) {
    aggregateConstraints.AppendElement(&advanced);
    nsTArray<RefPtr<MediaDevice>> rejects;
    for (uint32_t j = 0; j < aDevices.Length();) {
      uint32_t distance = aDevices[j]->GetBestFitnessDistance(
          aggregateConstraints, aCallerType);
      if (distance == UINT32_MAX) {
        rejects.AppendElement(std::move(aDevices[j]));
        aDevices.RemoveElementAt(j);
      } else {
        ++j;
      }
    }
    if (aDevices.IsEmpty()) {
      aDevices.AppendElements(std::move(rejects));
      aggregateConstraints.RemoveLastElement();
    }
  }
  return nullptr;
}

/* static */ const char* MediaConstraintsHelper::FindBadConstraint(
    const NormalizedConstraints& aConstraints,
    const nsTArray<RefPtr<MediaDevice>>& aDevices) {
  // The spec says to report a constraint that satisfies NONE
  // of the sources. Unfortunately, this is a bit laborious to find out, and
  // requires updating as new constraints are added!
  auto& c = aConstraints;
  dom::MediaTrackConstraints empty;

  if (aDevices.IsEmpty() ||
      !SomeSettingsFit(NormalizedConstraints(empty), aDevices)) {
    return "";
  }
  {
    NormalizedConstraints fresh(empty);
    fresh.mDeviceId = c.mDeviceId;
    if (!SomeSettingsFit(fresh, aDevices)) {
      return "deviceId";
    }
  }
  {
    NormalizedConstraints fresh(empty);
    fresh.mGroupId = c.mGroupId;
    if (!SomeSettingsFit(fresh, aDevices)) {
      return "groupId";
    }
  }
  {
    NormalizedConstraints fresh(empty);
    fresh.mWidth = c.mWidth;
    if (!SomeSettingsFit(fresh, aDevices)) {
      return "width";
    }
  }
  {
    NormalizedConstraints fresh(empty);
    fresh.mHeight = c.mHeight;
    if (!SomeSettingsFit(fresh, aDevices)) {
      return "height";
    }
  }
  {
    NormalizedConstraints fresh(empty);
    fresh.mFrameRate = c.mFrameRate;
    if (!SomeSettingsFit(fresh, aDevices)) {
      return "frameRate";
    }
  }
  {
    NormalizedConstraints fresh(empty);
    fresh.mFacingMode = c.mFacingMode;
    if (!SomeSettingsFit(fresh, aDevices)) {
      return "facingMode";
    }
  }
  return "";
}

/* static */ const char* MediaConstraintsHelper::FindBadConstraint(
    const NormalizedConstraints& aConstraints,
    const RefPtr<MediaEngineSource>& aMediaEngineSource) {
  NormalizedConstraints c(aConstraints);
  NormalizedConstraints empty((dom::MediaTrackConstraints()));
  c.mDeviceId = empty.mDeviceId;
  c.mGroupId = empty.mGroupId;
  AutoTArray<RefPtr<MediaDevice>, 1> devices;
  devices.AppendElement(MakeRefPtr<MediaDevice>(aMediaEngineSource,
                                                aMediaEngineSource->GetName(),
                                                u""_ns, u""_ns, u""_ns));
  return FindBadConstraint(c, devices);
}

static void LogConstraintStringRange(
    const NormalizedConstraintSet::StringRange& aRange) {
  if (aRange.mExact.size() <= 1 && aRange.mIdeal.size() <= 1) {
    LOG("  %s: { exact: [%s], ideal: [%s] }", aRange.mName,
        (aRange.mExact.empty()
             ? ""
             : NS_ConvertUTF16toUTF8(*aRange.mExact.begin()).get()),
        (aRange.mIdeal.empty()
             ? ""
             : NS_ConvertUTF16toUTF8(*aRange.mIdeal.begin()).get()));
  } else {
    LOG("  %s: { exact: [", aRange.mName);
    for (auto& entry : aRange.mExact) {
      LOG("      %s,", NS_ConvertUTF16toUTF8(entry).get());
    }
    LOG("    ], ideal: [");
    for (auto& entry : aRange.mIdeal) {
      LOG("      %s,", NS_ConvertUTF16toUTF8(entry).get());
    }
    LOG("    ]}");
  }
}

template <typename T>
static void LogConstraintRange(
    const NormalizedConstraintSet::Range<T>& aRange) {
  if (aRange.mIdeal.isSome()) {
    LOG("  %s: { min: %d, max: %d, ideal: %d }", aRange.mName, aRange.mMin,
        aRange.mMax, aRange.mIdeal.valueOr(0));
  } else {
    LOG("  %s: { min: %d, max: %d }", aRange.mName, aRange.mMin, aRange.mMax);
  }
}

template <>
void LogConstraintRange(const NormalizedConstraintSet::Range<double>& aRange) {
  if (aRange.mIdeal.isSome()) {
    LOG("  %s: { min: %f, max: %f, ideal: %f }", aRange.mName, aRange.mMin,
        aRange.mMax, aRange.mIdeal.valueOr(0));
  } else {
    LOG("  %s: { min: %f, max: %f }", aRange.mName, aRange.mMin, aRange.mMax);
  }
}

/* static */
void MediaConstraintsHelper::LogConstraints(
    const NormalizedConstraintSet& aConstraints) {
  auto& c = aConstraints;
  LOG("Constraints: {");
  LOG("%s", [&]() {
    LogConstraintRange(c.mWidth);
    LogConstraintRange(c.mHeight);
    LogConstraintRange(c.mFrameRate);
    LogConstraintStringRange(c.mMediaSource);
    LogConstraintStringRange(c.mFacingMode);
    LogConstraintStringRange(c.mDeviceId);
    LogConstraintStringRange(c.mGroupId);
    LogConstraintRange(c.mEchoCancellation);
    LogConstraintRange(c.mAutoGainControl);
    LogConstraintRange(c.mNoiseSuppression);
    LogConstraintRange(c.mChannelCount);
    return "}";
  }());
}

}  // namespace mozilla

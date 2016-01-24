/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file should not be included by other includes, as it contains code

#ifndef MEDIATRACKCONSTRAINTS_H_
#define MEDIATRACKCONSTRAINTS_H_

#include "mozilla/Attributes.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/MediaTrackConstraintSetBinding.h"
#include "mozilla/dom/MediaTrackSupportedConstraintsBinding.h"

#include <map>

namespace mozilla {

template<class EnumValuesStrings, class Enum>
static const char* EnumToASCII(const EnumValuesStrings& aStrings, Enum aValue) {
  return aStrings[uint32_t(aValue)].value;
}

template<class EnumValuesStrings, class Enum>
static Enum StringToEnum(const EnumValuesStrings& aStrings,
                         const nsAString& aValue, Enum aDefaultValue) {
  for (size_t i = 0; aStrings[i].value; i++) {
    if (aValue.EqualsASCII(aStrings[i].value)) {
      return Enum(i);
    }
  }
  return aDefaultValue;
}

// Helper classes for orthogonal constraints without interdependencies.
// Instead of constraining values, constrain the constraints themselves.

struct NormalizedConstraintSet
{
  template<class ValueType>
  struct Range
  {
    ValueType mMin, mMax;
    dom::Optional<ValueType> mIdeal;

    Range(ValueType aMin, ValueType aMax) : mMin(aMin), mMax(aMax) {}

    template<class ConstrainRange>
    void SetFrom(const ConstrainRange& aOther);
    ValueType Clamp(ValueType n) const { return std::max(mMin, std::min(n, mMax)); }
    ValueType Get(ValueType defaultValue) const {
      return Clamp(mIdeal.WasPassed() ? mIdeal.Value() : defaultValue);
    }
    bool Intersects(const Range& aOther) const {
      return mMax >= aOther.mMin && mMin <= aOther.mMax;
    }
    void Intersect(const Range& aOther) {
      MOZ_ASSERT(Intersects(aOther));
      mMin = std::max(mMin, aOther.mMin);
      mMax = std::min(mMax, aOther.mMax);
    }
  };

  struct LongRange : public Range<int32_t>
  {
    LongRange(const dom::OwningLongOrConstrainLongRange& aOther, bool advanced);
  };

  struct DoubleRange : public Range<double>
  {
    DoubleRange(const dom::OwningDoubleOrConstrainDoubleRange& aOther,
                bool advanced);
  };

  struct BooleanRange : public Range<bool>
  {
    BooleanRange(const dom::OwningBooleanOrConstrainBooleanParameters& aOther,
                 bool advanced);
  };

  // Do you need to add your constraint here? Only if your code uses flattening
  LongRange mWidth, mHeight;
  DoubleRange mFrameRate;
  LongRange mViewportOffsetX, mViewportOffsetY, mViewportWidth, mViewportHeight;
  BooleanRange mEchoCancellation, mMozNoiseSuppression, mMozAutoGainControl;

  NormalizedConstraintSet(const dom::MediaTrackConstraintSet& aOther,
                          bool advanced)
  : mWidth(aOther.mWidth, advanced)
  , mHeight(aOther.mHeight, advanced)
  , mFrameRate(aOther.mFrameRate, advanced)
  , mViewportOffsetX(aOther.mViewportOffsetX, advanced)
  , mViewportOffsetY(aOther.mViewportOffsetY, advanced)
  , mViewportWidth(aOther.mViewportWidth, advanced)
  , mViewportHeight(aOther.mViewportHeight, advanced)
  , mEchoCancellation(aOther.mEchoCancellation, advanced)
  , mMozNoiseSuppression(aOther.mMozNoiseSuppression, advanced)
  , mMozAutoGainControl(aOther.mMozAutoGainControl, advanced) {}
};

struct FlattenedConstraints : public NormalizedConstraintSet
{
  explicit FlattenedConstraints(const dom::MediaTrackConstraints& aOther);
};

// A helper class for MediaEngines

class MediaConstraintsHelper
{
protected:
  template<class ValueType, class ConstrainRange>
  static uint32_t FitnessDistance(ValueType aN, const ConstrainRange& aRange);
  static uint32_t FitnessDistance(int32_t aN,
      const dom::OwningLongOrConstrainLongRange& aConstraint, bool aAdvanced);
  static uint32_t FitnessDistance(double aN,
      const dom::OwningDoubleOrConstrainDoubleRange& aConstraint, bool aAdvanced);
  static uint32_t FitnessDistance(nsString aN,
    const dom::OwningStringOrStringSequenceOrConstrainDOMStringParameters& aConstraint,
    bool aAdvanced);
  static uint32_t FitnessDistance(nsString aN,
      const dom::ConstrainDOMStringParameters& aParams);

  static uint32_t
  GetMinimumFitnessDistance(const dom::MediaTrackConstraintSet &aConstraints,
                            bool aAdvanced,
                            const nsString& aDeviceId);

  template<class DeviceType>
  static bool
  SomeSettingsFit(const dom::MediaTrackConstraints &aConstraints,
                  nsTArray<RefPtr<DeviceType>>& aSources)
  {
    nsTArray<const dom::MediaTrackConstraintSet*> aggregateConstraints;
    aggregateConstraints.AppendElement(&aConstraints);

    MOZ_ASSERT(aSources.Length());
    for (auto& source : aSources) {
      if (source->GetBestFitnessDistance(aggregateConstraints) != UINT32_MAX) {
        return true;
      }
    }
    return false;
  }

public:
  // Apply constrains to a supplied list of sources (removes items from the list)

  template<class DeviceType>
  static const char*
  SelectSettings(const dom::MediaTrackConstraints &aConstraints,
                 nsTArray<RefPtr<DeviceType>>& aSources)
  {
    auto& c = aConstraints;

    // First apply top-level constraints.

    // Stack constraintSets that pass, starting with the required one, because the
    // whole stack must be re-satisfied each time a capability-set is ruled out
    // (this avoids storing state or pushing algorithm into the lower-level code).
    nsTArray<RefPtr<DeviceType>> unsatisfactory;
    nsTArray<const dom::MediaTrackConstraintSet*> aggregateConstraints;
    aggregateConstraints.AppendElement(&c);

    std::multimap<uint32_t, RefPtr<DeviceType>> ordered;

    for (uint32_t i = 0; i < aSources.Length();) {
      uint32_t distance = aSources[i]->GetBestFitnessDistance(aggregateConstraints);
      if (distance == UINT32_MAX) {
        unsatisfactory.AppendElement(aSources[i]);
        aSources.RemoveElementAt(i);
      } else {
        ordered.insert(std::pair<uint32_t, RefPtr<DeviceType>>(distance,
                                                                 aSources[i]));
        ++i;
      }
    }
    if (!aSources.Length()) {
      // None selected. The spec says to report a constraint that satisfies NONE
      // of the sources. Unfortunately, this is a bit laborious to find out, and
      // requires updating as new constraints are added!

      if (!unsatisfactory.Length() ||
          !SomeSettingsFit(dom::MediaTrackConstraints(), unsatisfactory)) {
        return "";
      }
      if (c.mDeviceId.IsConstrainDOMStringParameters()) {
        dom::MediaTrackConstraints fresh;
        fresh.mDeviceId = c.mDeviceId;
        if (!SomeSettingsFit(fresh, unsatisfactory)) {
          return "deviceId";
        }
      }
      if (c.mWidth.IsConstrainLongRange()) {
        dom::MediaTrackConstraints fresh;
        fresh.mWidth = c.mWidth;
        if (!SomeSettingsFit(fresh, unsatisfactory)) {
          return "width";
        }
      }
      if (c.mHeight.IsConstrainLongRange()) {
        dom::MediaTrackConstraints fresh;
        fresh.mHeight = c.mHeight;
        if (!SomeSettingsFit(fresh, unsatisfactory)) {
          return "height";
        }
      }
      if (c.mFrameRate.IsConstrainDoubleRange()) {
        dom::MediaTrackConstraints fresh;
        fresh.mFrameRate = c.mFrameRate;
        if (!SomeSettingsFit(fresh, unsatisfactory)) {
          return "frameRate";
        }
      }
      if (c.mFacingMode.IsConstrainDOMStringParameters()) {
        dom::MediaTrackConstraints fresh;
        fresh.mFacingMode = c.mFacingMode;
        if (!SomeSettingsFit(fresh, unsatisfactory)) {
          return "facingMode";
        }
      }
      return "";
    }

    // Order devices by shortest distance
    for (auto& ordinal : ordered) {
      aSources.RemoveElement(ordinal.second);
      aSources.AppendElement(ordinal.second);
    }

    // Then apply advanced constraints.

    if (c.mAdvanced.WasPassed()) {
      auto &array = c.mAdvanced.Value();

      for (int i = 0; i < int(array.Length()); i++) {
        aggregateConstraints.AppendElement(&array[i]);
        nsTArray<RefPtr<DeviceType>> rejects;
        for (uint32_t j = 0; j < aSources.Length();) {
          if (aSources[j]->GetBestFitnessDistance(aggregateConstraints) == UINT32_MAX) {
            rejects.AppendElement(aSources[j]);
            aSources.RemoveElementAt(j);
          } else {
            ++j;
          }
        }
        if (!aSources.Length()) {
          aSources.AppendElements(Move(rejects));
          aggregateConstraints.RemoveElementAt(aggregateConstraints.Length() - 1);
        }
      }
    }
    return nullptr;
  }
};

} // namespace mozilla

#endif /* MEDIATRACKCONSTRAINTS_H_ */

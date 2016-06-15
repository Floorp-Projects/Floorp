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
#include <set>

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
  class Range
  {
  public:
    ValueType mMin, mMax;
    Maybe<ValueType> mIdeal;

    Range(ValueType aMin, ValueType aMax)
      : mMin(aMin), mMax(aMax), mMergeDenominator(0) {}

    template<class ConstrainRange>
    void SetFrom(const ConstrainRange& aOther);
    ValueType Clamp(ValueType n) const { return std::max(mMin, std::min(n, mMax)); }
    ValueType Get(ValueType defaultValue) const {
      return Clamp(mIdeal.valueOr(defaultValue));
    }
    bool Intersects(const Range& aOther) const {
      return mMax >= aOther.mMin && mMin <= aOther.mMax;
    }
    void Intersect(const Range& aOther) {
      MOZ_ASSERT(Intersects(aOther));
      mMin = std::max(mMin, aOther.mMin);
      mMax = std::min(mMax, aOther.mMax);
    }
    bool Merge(const Range& aOther) {
      if (!Intersects(aOther)) {
        return false;
      }
      Intersect(aOther);

      if (aOther.mIdeal.isSome()) {
        if (mIdeal.isNothing()) {
          mIdeal.emplace(aOther.mIdeal.value());
          mMergeDenominator = 1;
        } else {
          *mIdeal += aOther.mIdeal.value();
          mMergeDenominator = std::max(2U, mMergeDenominator + 1);
        }
      }
      return true;
    }
    void FinalizeMerge()
    {
      if (mMergeDenominator) {
        *mIdeal /= mMergeDenominator;
        mMergeDenominator = 0;
      }
    }
  private:
    uint32_t mMergeDenominator;
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

  struct StringRange
  {
    typedef std::set<nsString> ValueType;
    ValueType mExact, mIdeal;

    StringRange(
        const dom::OwningStringOrStringSequenceOrConstrainDOMStringParameters& aOther,
        bool advanced);

    void SetFrom(const dom::ConstrainDOMStringParameters& aOther);
    ValueType Clamp(const ValueType& n) const;
    ValueType Get(const ValueType& defaultValue) const {
      return Clamp(mIdeal.size() ? mIdeal : defaultValue);
    }
    bool Intersects(const StringRange& aOther) const;
    void Intersect(const StringRange& aOther);
    bool Merge(const StringRange& aOther);
    void FinalizeMerge() {}
  };

  // All new constraints should be added here whether they use flattening or not
  LongRange mWidth, mHeight;
  DoubleRange mFrameRate;
  StringRange mFacingMode;
  nsString mMediaSource;
  long long mBrowserWindow;
  bool mScrollWithPage;
  StringRange mDeviceId;
  LongRange mViewportOffsetX, mViewportOffsetY, mViewportWidth, mViewportHeight;
  BooleanRange mEchoCancellation, mMozNoiseSuppression, mMozAutoGainControl;

  NormalizedConstraintSet(const dom::MediaTrackConstraintSet& aOther,
                          bool advanced)
  : mWidth(aOther.mWidth, advanced)
  , mHeight(aOther.mHeight, advanced)
  , mFrameRate(aOther.mFrameRate, advanced)
  , mFacingMode(aOther.mFacingMode, advanced)
  , mMediaSource(aOther.mMediaSource)
  , mBrowserWindow(aOther.mBrowserWindow.WasPassed() ?
                   aOther.mBrowserWindow.Value() : 0)
  , mScrollWithPage(aOther.mScrollWithPage.WasPassed() ?
                    aOther.mScrollWithPage.Value() : false)
  , mDeviceId(aOther.mDeviceId, advanced)
  , mViewportOffsetX(aOther.mViewportOffsetX, advanced)
  , mViewportOffsetY(aOther.mViewportOffsetY, advanced)
  , mViewportWidth(aOther.mViewportWidth, advanced)
  , mViewportHeight(aOther.mViewportHeight, advanced)
  , mEchoCancellation(aOther.mEchoCancellation, advanced)
  , mMozNoiseSuppression(aOther.mMozNoiseSuppression, advanced)
  , mMozAutoGainControl(aOther.mMozAutoGainControl, advanced) {}
};

template<> bool NormalizedConstraintSet::Range<bool>::Merge(const Range& aOther);
template<> void NormalizedConstraintSet::Range<bool>::FinalizeMerge();

// Used instead of MediaTrackConstraints in lower-level code.
struct NormalizedConstraints : public NormalizedConstraintSet
{
  explicit NormalizedConstraints(const dom::MediaTrackConstraints& aOther);
  explicit NormalizedConstraints(
      const nsTArray<const NormalizedConstraints*>& aOthers);

  nsTArray<NormalizedConstraintSet> mAdvanced;
  const char* mBadConstraint;
};

// Flattened version is used in low-level code with orthogonal constraints only.
struct FlattenedConstraints : public NormalizedConstraintSet
{
  explicit FlattenedConstraints(const NormalizedConstraints& aOther);

  explicit FlattenedConstraints(const dom::MediaTrackConstraints& aOther)
    : FlattenedConstraints(NormalizedConstraints(aOther)) {}
};

// A helper class for MediaEngines

class MediaConstraintsHelper
{
protected:
  template<class ValueType, class NormalizedRange>
  static uint32_t FitnessDistance(ValueType aN, const NormalizedRange& aRange);
  static uint32_t FitnessDistance(nsString aN,
      const NormalizedConstraintSet::StringRange& aConstraint);

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

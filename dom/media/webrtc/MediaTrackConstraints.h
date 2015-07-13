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

  // Do you need to add your constraint here? Only if your code uses flattening
  LongRange mWidth, mHeight;
  DoubleRange mFrameRate;

  NormalizedConstraintSet(const dom::MediaTrackConstraintSet& aOther,
                          bool advanced)
  : mWidth(aOther.mWidth, advanced)
  , mHeight(aOther.mHeight, advanced)
  , mFrameRate(aOther.mFrameRate, advanced) {}
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
};

} // namespace mozilla

#endif /* MEDIATRACKCONSTRAINTS_H_ */

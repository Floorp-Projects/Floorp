/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file should not be included by other includes, as it contains code

#ifndef MEDIATRACKCONSTRAINTS_H_
#define MEDIATRACKCONSTRAINTS_H_

#include <map>
#include <set>
#include <vector>

#include "mozilla/Attributes.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/MediaTrackSupportedConstraintsBinding.h"

namespace mozilla {

class MediaDevice;
class MediaEngineSource;

template <class EnumValuesStrings, class Enum>
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
class NormalizedConstraintSet {
 protected:
  class BaseRange {
   protected:
    typedef BaseRange NormalizedConstraintSet::*MemberPtrType;

    BaseRange(MemberPtrType aMemberPtr, const char* aName,
              nsTArray<MemberPtrType>* aList)
        : mName(aName) {
      if (aList) {
        aList->AppendElement(aMemberPtr);
      }
    }
    virtual ~BaseRange() = default;

   public:
    virtual bool Merge(const BaseRange& aOther) = 0;
    virtual void FinalizeMerge() = 0;

    const char* mName;
  };

  typedef BaseRange NormalizedConstraintSet::*MemberPtrType;

 public:
  template <class ValueType>
  class Range : public BaseRange {
   public:
    ValueType mMin, mMax;
    Maybe<ValueType> mIdeal;

    Range(MemberPtrType aMemberPtr, const char* aName, ValueType aMin,
          ValueType aMax, nsTArray<MemberPtrType>* aList)
        : BaseRange(aMemberPtr, aName, aList),
          mMin(aMin),
          mMax(aMax),
          mMergeDenominator(0) {}
    virtual ~Range() = default;

    template <class ConstrainRange>
    void SetFrom(const ConstrainRange& aOther);
    ValueType Clamp(ValueType n) const {
      return std::max(mMin, std::min(n, mMax));
    }
    ValueType Get(ValueType defaultValue) const {
      return Clamp(mIdeal.valueOr(defaultValue));
    }
    bool Intersects(const Range& aOther) const {
      return mMax >= aOther.mMin && mMin <= aOther.mMax;
    }
    void Intersect(const Range& aOther) {
      mMin = std::max(mMin, aOther.mMin);
      if (Intersects(aOther)) {
        mMax = std::min(mMax, aOther.mMax);
      } else {
        // If there is no intersection, we will down-scale or drop frame
        mMax = std::max(mMax, aOther.mMax);
      }
    }
    bool Merge(const Range& aOther) {
      if (strcmp(mName, "width") != 0 && strcmp(mName, "height") != 0 &&
          strcmp(mName, "frameRate") != 0 && !Intersects(aOther)) {
        return false;
      }
      Intersect(aOther);

      if (aOther.mIdeal.isSome()) {
        // Ideal values, as stored, may be outside their min max range, so use
        // clamped values in averaging, to avoid extreme outliers skewing
        // results.
        if (mIdeal.isNothing()) {
          mIdeal.emplace(aOther.Get(0));
          mMergeDenominator = 1;
        } else {
          if (!mMergeDenominator) {
            *mIdeal = Get(0);
            mMergeDenominator = 1;
          }
          *mIdeal += aOther.Get(0);
          mMergeDenominator++;
        }
      }
      return true;
    }
    void FinalizeMerge() override {
      if (mMergeDenominator) {
        *mIdeal /= mMergeDenominator;
        mMergeDenominator = 0;
      }
    }
    void TakeHighestIdeal(const Range& aOther) {
      if (aOther.mIdeal.isSome()) {
        if (mIdeal.isNothing()) {
          mIdeal.emplace(aOther.Get(0));
        } else {
          *mIdeal = std::max(Get(0), aOther.Get(0));
        }
      }
    }

   private:
    bool Merge(const BaseRange& aOther) override {
      return Merge(static_cast<const Range&>(aOther));
    }

    uint32_t mMergeDenominator;
  };

  struct LongRange : public Range<int32_t> {
    typedef LongRange NormalizedConstraintSet::*LongPtrType;

    LongRange(LongPtrType aMemberPtr, const char* aName,
              const dom::Optional<dom::OwningLongOrConstrainLongRange>& aOther,
              bool advanced, nsTArray<MemberPtrType>* aList);
  };

  struct LongLongRange : public Range<int64_t> {
    typedef LongLongRange NormalizedConstraintSet::*LongLongPtrType;

    LongLongRange(LongLongPtrType aMemberPtr, const char* aName,
                  const long long& aOther, nsTArray<MemberPtrType>* aList);
  };

  struct DoubleRange : public Range<double> {
    typedef DoubleRange NormalizedConstraintSet::*DoublePtrType;

    DoubleRange(
        DoublePtrType aMemberPtr, const char* aName,
        const dom::Optional<dom::OwningDoubleOrConstrainDoubleRange>& aOther,
        bool advanced, nsTArray<MemberPtrType>* aList);
  };

  struct BooleanRange : public Range<bool> {
    typedef BooleanRange NormalizedConstraintSet::*BooleanPtrType;

    BooleanRange(
        BooleanPtrType aMemberPtr, const char* aName,
        const dom::Optional<dom::OwningBooleanOrConstrainBooleanParameters>&
            aOther,
        bool advanced, nsTArray<MemberPtrType>* aList);

    BooleanRange(BooleanPtrType aMemberPtr, const char* aName,
                 const bool& aOther, nsTArray<MemberPtrType>* aList)
        : Range<bool>((MemberPtrType)aMemberPtr, aName, false, true, aList) {
      mIdeal.emplace(aOther);
    }
  };

  struct StringRange : public BaseRange {
    typedef std::set<nsString> ValueType;
    ValueType mExact, mIdeal;

    typedef StringRange NormalizedConstraintSet::*StringPtrType;

    StringRange(
        StringPtrType aMemberPtr, const char* aName,
        const dom::Optional<
            dom::OwningStringOrStringSequenceOrConstrainDOMStringParameters>&
            aOther,
        bool advanced, nsTArray<MemberPtrType>* aList);

    StringRange(StringPtrType aMemberPtr, const char* aName,
                const dom::Optional<nsString>& aOther,
                nsTArray<MemberPtrType>* aList)
        : BaseRange((MemberPtrType)aMemberPtr, aName, aList) {
      if (aOther.WasPassed()) {
        mIdeal.insert(aOther.Value());
      }
    }

    ~StringRange() = default;

    void SetFrom(const dom::ConstrainDOMStringParameters& aOther);
    ValueType Clamp(const ValueType& n) const;
    ValueType Get(const ValueType& defaultValue) const {
      return Clamp(mIdeal.empty() ? defaultValue : mIdeal);
    }
    bool Intersects(const StringRange& aOther) const;
    void Intersect(const StringRange& aOther);
    bool Merge(const StringRange& aOther);
    void FinalizeMerge() override {}

   private:
    bool Merge(const BaseRange& aOther) override {
      return Merge(static_cast<const StringRange&>(aOther));
    }
  };

  // All new constraints should be added here whether they use flattening or not
  LongRange mWidth, mHeight;
  DoubleRange mFrameRate;
  StringRange mFacingMode;
  StringRange mMediaSource;
  LongLongRange mBrowserWindow;
  StringRange mDeviceId;
  StringRange mGroupId;
  LongRange mViewportOffsetX, mViewportOffsetY, mViewportWidth, mViewportHeight;
  BooleanRange mEchoCancellation, mNoiseSuppression, mAutoGainControl;
  LongRange mChannelCount;

 private:
  typedef NormalizedConstraintSet T;

 public:
  NormalizedConstraintSet(const dom::MediaTrackConstraintSet& aOther,
                          bool advanced,
                          nsTArray<MemberPtrType>* aList = nullptr)
      : mWidth(&T::mWidth, "width", aOther.mWidth, advanced, aList),
        mHeight(&T::mHeight, "height", aOther.mHeight, advanced, aList),
        mFrameRate(&T::mFrameRate, "frameRate", aOther.mFrameRate, advanced,
                   aList),
        mFacingMode(&T::mFacingMode, "facingMode", aOther.mFacingMode, advanced,
                    aList),
        mMediaSource(&T::mMediaSource, "mediaSource", aOther.mMediaSource,
                     aList),
        mBrowserWindow(&T::mBrowserWindow, "browserWindow",
                       aOther.mBrowserWindow.WasPassed()
                           ? aOther.mBrowserWindow.Value()
                           : 0,
                       aList),
        mDeviceId(&T::mDeviceId, "deviceId", aOther.mDeviceId, advanced, aList),
        mGroupId(&T::mGroupId, "groupId", aOther.mGroupId, advanced, aList),
        mViewportOffsetX(&T::mViewportOffsetX, "viewportOffsetX",
                         aOther.mViewportOffsetX, advanced, aList),
        mViewportOffsetY(&T::mViewportOffsetY, "viewportOffsetY",
                         aOther.mViewportOffsetY, advanced, aList),
        mViewportWidth(&T::mViewportWidth, "viewportWidth",
                       aOther.mViewportWidth, advanced, aList),
        mViewportHeight(&T::mViewportHeight, "viewportHeight",
                        aOther.mViewportHeight, advanced, aList),
        mEchoCancellation(&T::mEchoCancellation, "echoCancellation",
                          aOther.mEchoCancellation, advanced, aList),
        mNoiseSuppression(&T::mNoiseSuppression, "noiseSuppression",
                          aOther.mNoiseSuppression, advanced, aList),
        mAutoGainControl(&T::mAutoGainControl, "autoGainControl",
                         aOther.mAutoGainControl, advanced, aList),
        mChannelCount(&T::mChannelCount, "channelCount", aOther.mChannelCount,
                      advanced, aList) {}
};

template <>
bool NormalizedConstraintSet::Range<bool>::Merge(const Range& aOther);
template <>
void NormalizedConstraintSet::Range<bool>::FinalizeMerge();

// Used instead of MediaTrackConstraints in lower-level code.
struct NormalizedConstraints : public NormalizedConstraintSet {
  explicit NormalizedConstraints(const dom::MediaTrackConstraints& aOther,
                                 nsTArray<MemberPtrType>* aList = nullptr);

  std::vector<NormalizedConstraintSet> mAdvanced;
  const char* mBadConstraint;
};

// Flattened version is used in low-level code with orthogonal constraints only.
struct FlattenedConstraints : public NormalizedConstraintSet {
  explicit FlattenedConstraints(const NormalizedConstraints& aOther);

  explicit FlattenedConstraints(const dom::MediaTrackConstraints& aOther)
      : FlattenedConstraints(NormalizedConstraints(aOther)) {}
};

// A helper class for MediaEngineSources
class MediaConstraintsHelper {
 public:
  template <class ValueType, class NormalizedRange>
  static uint32_t FitnessDistance(ValueType aN, const NormalizedRange& aRange) {
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
  static uint32_t FeasibilityDistance(ValueType aN,
                                      const NormalizedRange& aRange) {
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

    return 10000 +
           uint32_t(ValueType(
               (std::abs(aN - aRange.mIdeal.value()) * 1000) /
               std::max(std::abs(aN), std::abs(aRange.mIdeal.value()))));
  }

  static uint32_t FitnessDistance(
      const Maybe<nsString>& aN,
      const NormalizedConstraintSet::StringRange& aParams);

 protected:
  static bool SomeSettingsFit(const NormalizedConstraints& aConstraints,
                              const nsTArray<RefPtr<MediaDevice>>& aDevices);

 public:
  static uint32_t GetMinimumFitnessDistance(
      const NormalizedConstraintSet& aConstraints, const nsString& aDeviceId,
      const nsString& aGroupId);

  // Apply constrains to a supplied list of devices (removes items from the
  // list)
  static const char* SelectSettings(const NormalizedConstraints& aConstraints,
                                    nsTArray<RefPtr<MediaDevice>>& aDevices,
                                    dom::CallerType aCallerType);

  static const char* FindBadConstraint(
      const NormalizedConstraints& aConstraints,
      const nsTArray<RefPtr<MediaDevice>>& aDevices);

  static const char* FindBadConstraint(
      const NormalizedConstraints& aConstraints,
      const RefPtr<MediaEngineSource>& aMediaEngineSource);

  static void LogConstraints(const NormalizedConstraintSet& aConstraints);
};

}  // namespace mozilla

#endif /* MEDIATRACKCONSTRAINTS_H_ */

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
#include <vector>

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

class NormalizedConstraintSet
{
protected:
  class BaseRange
  {
  protected:
    typedef BaseRange NormalizedConstraintSet::* MemberPtrType;

    BaseRange(MemberPtrType aMemberPtr, const char* aName,
              nsTArray<MemberPtrType>* aList) : mName(aName) {
      if (aList) {
        aList->AppendElement(aMemberPtr);
      }
    }
    virtual ~BaseRange() {}
  public:
    virtual bool Merge(const BaseRange& aOther) = 0;
    virtual void FinalizeMerge() = 0;

    const char* mName;
  };

  typedef BaseRange NormalizedConstraintSet::* MemberPtrType;

public:
  template<class ValueType>
  class Range : public BaseRange
  {
  public:
    ValueType mMin, mMax;
    Maybe<ValueType> mIdeal;

    Range(MemberPtrType aMemberPtr, const char* aName, ValueType aMin,
          ValueType aMax, nsTArray<MemberPtrType>* aList)
      : BaseRange(aMemberPtr, aName, aList)
      , mMin(aMin), mMax(aMax), mMergeDenominator(0) {}
    virtual ~Range() {};

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
        // Ideal values, as stored, may be outside their min max range, so use
        // clamped values in averaging, to avoid extreme outliers skewing results.
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
    void FinalizeMerge() override
    {
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

  struct LongRange : public Range<int32_t>
  {
    typedef LongRange NormalizedConstraintSet::* LongPtrType;

    LongRange(LongPtrType aMemberPtr, const char* aName,
              const dom::OwningLongOrConstrainLongRange& aOther, bool advanced,
              nsTArray<MemberPtrType>* aList);
  };

  struct LongLongRange : public Range<int64_t>
  {
    typedef LongLongRange NormalizedConstraintSet::* LongLongPtrType;

    LongLongRange(LongLongPtrType aMemberPtr, const char* aName,
                  const long long& aOther,
                  nsTArray<MemberPtrType>* aList);
  };

  struct DoubleRange : public Range<double>
  {
    typedef DoubleRange NormalizedConstraintSet::* DoublePtrType;

    DoubleRange(DoublePtrType aMemberPtr,
                const char* aName,
                const dom::OwningDoubleOrConstrainDoubleRange& aOther,
                bool advanced,
                nsTArray<MemberPtrType>* aList);
  };

  struct BooleanRange : public Range<bool>
  {
    typedef BooleanRange NormalizedConstraintSet::* BooleanPtrType;

    BooleanRange(BooleanPtrType aMemberPtr, const char* aName,
                 const dom::OwningBooleanOrConstrainBooleanParameters& aOther,
                 bool advanced,
                 nsTArray<MemberPtrType>* aList);

    BooleanRange(BooleanPtrType aMemberPtr, const char* aName, const bool& aOther,
                 nsTArray<MemberPtrType>* aList)
      : Range<bool>((MemberPtrType)aMemberPtr, aName, false, true, aList) {
      mIdeal.emplace(aOther);
    }
  };

  struct StringRange : public BaseRange
  {
    typedef std::set<nsString> ValueType;
    ValueType mExact, mIdeal;

    typedef StringRange NormalizedConstraintSet::* StringPtrType;

    StringRange(StringPtrType aMemberPtr,  const char* aName,
        const dom::OwningStringOrStringSequenceOrConstrainDOMStringParameters& aOther,
        bool advanced,
        nsTArray<MemberPtrType>* aList);

    StringRange(StringPtrType aMemberPtr, const char* aName,
                const nsString& aOther, nsTArray<MemberPtrType>* aList)
      : BaseRange((MemberPtrType)aMemberPtr, aName, aList) {
      mIdeal.insert(aOther);
    }

    ~StringRange() {}

    void SetFrom(const dom::ConstrainDOMStringParameters& aOther);
    ValueType Clamp(const ValueType& n) const;
    ValueType Get(const ValueType& defaultValue) const {
      return Clamp(mIdeal.size() ? mIdeal : defaultValue);
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
  BooleanRange mScrollWithPage;
  StringRange mDeviceId;
  LongRange mViewportOffsetX, mViewportOffsetY, mViewportWidth, mViewportHeight;
  BooleanRange mEchoCancellation, mNoiseSuppression, mAutoGainControl;
  LongRange mChannelCount;
private:
  typedef NormalizedConstraintSet T;
public:
  NormalizedConstraintSet(const dom::MediaTrackConstraintSet& aOther,
                          bool advanced,
                          nsTArray<MemberPtrType>* aList = nullptr)
  : mWidth(&T::mWidth, "width", aOther.mWidth, advanced, aList)
  , mHeight(&T::mHeight, "height", aOther.mHeight, advanced, aList)
  , mFrameRate(&T::mFrameRate, "frameRate", aOther.mFrameRate, advanced, aList)
  , mFacingMode(&T::mFacingMode, "facingMode", aOther.mFacingMode, advanced, aList)
  , mMediaSource(&T::mMediaSource, "mediaSource", aOther.mMediaSource, aList)
  , mBrowserWindow(&T::mBrowserWindow, "browserWindow",
                   aOther.mBrowserWindow.WasPassed() ?
                   aOther.mBrowserWindow.Value() : 0, aList)
  , mScrollWithPage(&T::mScrollWithPage, "scrollWithPage",
                    aOther.mScrollWithPage.WasPassed() ?
                    aOther.mScrollWithPage.Value() : false, aList)
  , mDeviceId(&T::mDeviceId, "deviceId", aOther.mDeviceId, advanced, aList)
  , mViewportOffsetX(&T::mViewportOffsetX, "viewportOffsetX",
                     aOther.mViewportOffsetX, advanced, aList)
  , mViewportOffsetY(&T::mViewportOffsetY, "viewportOffsetY",
                     aOther.mViewportOffsetY, advanced, aList)
  , mViewportWidth(&T::mViewportWidth, "viewportWidth",
                   aOther.mViewportWidth, advanced, aList)
  , mViewportHeight(&T::mViewportHeight, "viewportHeight",
                    aOther.mViewportHeight, advanced, aList)
  , mEchoCancellation(&T::mEchoCancellation, "echoCancellation",
                      aOther.mEchoCancellation, advanced, aList)
  , mNoiseSuppression(&T::mNoiseSuppression, "noiseSuppression",
                      aOther.mNoiseSuppression,
                      advanced, aList)
  , mAutoGainControl(&T::mAutoGainControl, "autoGainControl",
                     aOther.mAutoGainControl, advanced, aList)
  , mChannelCount(&T::mChannelCount, "channelCount",
                  aOther.mChannelCount, advanced, aList) {}
};

template<> bool NormalizedConstraintSet::Range<bool>::Merge(const Range& aOther);
template<> void NormalizedConstraintSet::Range<bool>::FinalizeMerge();

// Used instead of MediaTrackConstraints in lower-level code.
struct NormalizedConstraints : public NormalizedConstraintSet
{
  explicit NormalizedConstraints(const dom::MediaTrackConstraints& aOther,
                        nsTArray<MemberPtrType>* aList = nullptr);

  // Merge constructor
  explicit NormalizedConstraints(
      const nsTArray<const NormalizedConstraints*>& aOthers);

  std::vector<NormalizedConstraintSet> mAdvanced;
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
  GetMinimumFitnessDistance(const NormalizedConstraintSet &aConstraints,
                            const nsString& aDeviceId);

  template<class DeviceType>
  static bool
  SomeSettingsFit(const NormalizedConstraints &aConstraints,
                  nsTArray<RefPtr<DeviceType>>& aDevices)
  {
    nsTArray<const NormalizedConstraintSet*> sets;
    sets.AppendElement(&aConstraints);

    MOZ_ASSERT(aDevices.Length());
    for (auto& device : aDevices) {
      if (device->GetBestFitnessDistance(sets, false) != UINT32_MAX) {
        return true;
      }
    }
    return false;
  }

public:
  // Apply constrains to a supplied list of devices (removes items from the list)

  template<class DeviceType>
  static const char*
  SelectSettings(const NormalizedConstraints &aConstraints,
                 nsTArray<RefPtr<DeviceType>>& aDevices,
                 bool aIsChrome)
  {
    auto& c = aConstraints;

    // First apply top-level constraints.

    // Stack constraintSets that pass, starting with the required one, because the
    // whole stack must be re-satisfied each time a capability-set is ruled out
    // (this avoids storing state or pushing algorithm into the lower-level code).
    nsTArray<RefPtr<DeviceType>> unsatisfactory;
    nsTArray<const NormalizedConstraintSet*> aggregateConstraints;
    aggregateConstraints.AppendElement(&c);

    std::multimap<uint32_t, RefPtr<DeviceType>> ordered;

    for (uint32_t i = 0; i < aDevices.Length();) {
      uint32_t distance = aDevices[i]->GetBestFitnessDistance(aggregateConstraints,
                                                              aIsChrome);
      if (distance == UINT32_MAX) {
        unsatisfactory.AppendElement(aDevices[i]);
        aDevices.RemoveElementAt(i);
      } else {
        ordered.insert(std::pair<uint32_t, RefPtr<DeviceType>>(distance,
                                                               aDevices[i]));
        ++i;
      }
    }
    if (!aDevices.Length()) {
      return FindBadConstraint(c, unsatisfactory);
    }

    // Order devices by shortest distance
    for (auto& ordinal : ordered) {
      aDevices.RemoveElement(ordinal.second);
      aDevices.AppendElement(ordinal.second);
    }

    // Then apply advanced constraints.

    for (int i = 0; i < int(c.mAdvanced.size()); i++) {
      aggregateConstraints.AppendElement(&c.mAdvanced[i]);
      nsTArray<RefPtr<DeviceType>> rejects;
      for (uint32_t j = 0; j < aDevices.Length();) {
        if (aDevices[j]->GetBestFitnessDistance(aggregateConstraints,
                                                aIsChrome) == UINT32_MAX) {
          rejects.AppendElement(aDevices[j]);
          aDevices.RemoveElementAt(j);
        } else {
          ++j;
        }
      }
      if (!aDevices.Length()) {
        aDevices.AppendElements(Move(rejects));
        aggregateConstraints.RemoveElementAt(aggregateConstraints.Length() - 1);
      }
    }
    return nullptr;
  }

  template<class DeviceType>
  static const char*
  FindBadConstraint(const NormalizedConstraints& aConstraints,
                    nsTArray<RefPtr<DeviceType>>& aDevices)
  {
    // The spec says to report a constraint that satisfies NONE
    // of the sources. Unfortunately, this is a bit laborious to find out, and
    // requires updating as new constraints are added!
    auto& c = aConstraints;
    dom::MediaTrackConstraints empty;

    if (!aDevices.Length() ||
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

  template<class MediaEngineSourceType>
  static const char*
  FindBadConstraint(const NormalizedConstraints& aConstraints,
                    const MediaEngineSourceType& aMediaEngineSource,
                    const nsString& aDeviceId);

  // Warn on and convert use of deprecated constraints to new ones

  static void
  ConvertOldWithWarning(
      const dom::OwningBooleanOrConstrainBooleanParameters& old,
      dom::OwningBooleanOrConstrainBooleanParameters& to,
      const char* aMessageName,
      nsPIDOMWindowInner* aWindow);
};

} // namespace mozilla

#endif /* MEDIATRACKCONSTRAINTS_H_ */

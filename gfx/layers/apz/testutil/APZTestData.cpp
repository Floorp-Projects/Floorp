/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZTestData.h"
#include "mozilla/dom/APZTestDataBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsString.h"

namespace mozilla {
namespace layers {

struct APZTestDataToJSConverter {
  template <typename Key, typename Value, typename KeyValuePair>
  static void ConvertMap(const std::map<Key, Value>& aFrom,
                         dom::Sequence<KeyValuePair>& aOutTo,
                         void (*aElementConverter)(const Key&, const Value&,
                                                   KeyValuePair&)) {
    for (auto it = aFrom.begin(); it != aFrom.end(); ++it) {
      if (!aOutTo.AppendElement(fallible)) {
        // XXX(Bug 1632090) Instead of extending the array 1-by-1 (which might
        // involve multiple reallocations) and potentially crashing here,
        // SetCapacity could be called outside the loop once.
        mozalloc_handle_oom(0);
      }
      aElementConverter(it->first, it->second, aOutTo.LastElement());
    }
  }

  template <typename Src, typename Target>
  static void ConvertList(const nsTArray<Src>& aFrom,
                          dom::Sequence<Target>& aOutTo,
                          void (*aElementConverter)(const Src&, Target&)) {
    for (auto it = aFrom.begin(); it != aFrom.end(); ++it) {
      if (!aOutTo.AppendElement(fallible)) {
        // XXX(Bug 1632090) Instead of extending the array 1-by-1 (which might
        // involve multiple reallocations) and potentially crashing here,
        // SetCapacity could be called outside the loop once.
        mozalloc_handle_oom(0);
      }
      aElementConverter(*it, aOutTo.LastElement());
    }
  }

  static void ConvertAPZTestData(const APZTestData& aFrom,
                                 dom::APZTestData& aOutTo) {
    ConvertMap(aFrom.mPaints, aOutTo.mPaints.Construct(), ConvertBucket);
    ConvertMap(aFrom.mRepaintRequests, aOutTo.mRepaintRequests.Construct(),
               ConvertBucket);
    ConvertList(aFrom.mHitResults, aOutTo.mHitResults.Construct(),
                ConvertHitResult);
    ConvertList(aFrom.mSampledResults, aOutTo.mSampledResults.Construct(),
                ConvertSampledResult);
    ConvertMap(aFrom.mAdditionalData, aOutTo.mAdditionalData.Construct(),
               ConvertAdditionalDataEntry);
  }

  static void ConvertBucket(const SequenceNumber& aKey,
                            const APZTestData::Bucket& aValue,
                            dom::APZBucket& aOutKeyValuePair) {
    aOutKeyValuePair.mSequenceNumber.Construct() = aKey;
    ConvertMap(aValue, aOutKeyValuePair.mScrollFrames.Construct(),
               ConvertScrollFrameData);
  }

  static void ConvertScrollFrameData(const APZTestData::ViewID& aKey,
                                     const APZTestData::ScrollFrameData& aValue,
                                     dom::ScrollFrameData& aOutKeyValuePair) {
    aOutKeyValuePair.mScrollId.Construct() = aKey;
    ConvertMap(aValue, aOutKeyValuePair.mEntries.Construct(), ConvertEntry);
  }

  static void ConvertEntry(const std::string& aKey, const std::string& aValue,
                           dom::ScrollFrameDataEntry& aOutKeyValuePair) {
    ConvertString(aKey, aOutKeyValuePair.mKey.Construct());
    ConvertString(aValue, aOutKeyValuePair.mValue.Construct());
  }

  static void ConvertAdditionalDataEntry(
      const std::string& aKey, const std::string& aValue,
      dom::AdditionalDataEntry& aOutKeyValuePair) {
    ConvertString(aKey, aOutKeyValuePair.mKey.Construct());
    ConvertString(aValue, aOutKeyValuePair.mValue.Construct());
  }

  static void ConvertString(const std::string& aFrom, nsString& aOutTo) {
    CopyUTF8toUTF16(aFrom, aOutTo);
  }

  static void ConvertHitResult(const APZTestData::HitResult& aResult,
                               dom::APZHitResult& aOutHitResult) {
    aOutHitResult.mScreenX.Construct() = aResult.point.x;
    aOutHitResult.mScreenY.Construct() = aResult.point.y;
    static_assert(MaxEnumValue<gfx::CompositorHitTestInfo::valueType>::value <
                      std::numeric_limits<uint16_t>::digits,
                  "CompositorHitTestFlags MAX value have to be less than "
                  "number of bits in uint16_t");
    aOutHitResult.mHitResult.Construct() =
        static_cast<uint16_t>(aResult.result.serialize());
    aOutHitResult.mLayersId.Construct() = aResult.layersId.mId;
    aOutHitResult.mScrollId.Construct() = aResult.scrollId;
  }

  static void ConvertSampledResult(const APZTestData::SampledResult& aResult,
                                   dom::APZSampledResult& aOutSampledResult) {
    aOutSampledResult.mScrollOffsetX.Construct() = aResult.scrollOffset.x;
    aOutSampledResult.mScrollOffsetY.Construct() = aResult.scrollOffset.y;
    aOutSampledResult.mLayersId.Construct() = aResult.layersId.mId;
    aOutSampledResult.mScrollId.Construct() = aResult.scrollId;
    aOutSampledResult.mSampledTimeStamp.Construct() = aResult.sampledTimeStamp;
  }
};

bool APZTestData::ToJS(JS::MutableHandleValue aOutValue,
                       JSContext* aContext) const {
  dom::APZTestData result;
  APZTestDataToJSConverter::ConvertAPZTestData(*this, result);
  return dom::ToJSValue(aContext, result, aOutValue);
}

}  // namespace layers
}  // namespace mozilla

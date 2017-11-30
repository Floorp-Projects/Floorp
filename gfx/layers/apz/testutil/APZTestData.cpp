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
                               void (*aElementConverter)(const Key&, const Value&, KeyValuePair&)) {
    for (auto it = aFrom.begin(); it != aFrom.end(); ++it) {
      aOutTo.AppendElement(fallible);
      aElementConverter(it->first, it->second, aOutTo.LastElement());
    }
  }

  template <typename Src, typename Target>
  static void ConvertList(const nsTArray<Src>& aFrom,
                          dom::Sequence<Target>& aOutTo,
                          void (*aElementConverter)(const Src&, Target&)) {
    for (auto it = aFrom.begin(); it != aFrom.end(); ++it) {
      aOutTo.AppendElement(fallible);
      aElementConverter(*it, aOutTo.LastElement());
    }
  }

  static void ConvertAPZTestData(const APZTestData& aFrom,
                                 dom::APZTestData& aOutTo) {
    ConvertMap(aFrom.mPaints, aOutTo.mPaints.Construct(), ConvertBucket);
    ConvertMap(aFrom.mRepaintRequests, aOutTo.mRepaintRequests.Construct(), ConvertBucket);
    ConvertList(aFrom.mHitResults, aOutTo.mHitResults.Construct(), ConvertHitResult);
  }

  static void ConvertBucket(const SequenceNumber& aKey,
                            const APZTestData::Bucket& aValue,
                            dom::APZBucket& aOutKeyValuePair) {
    aOutKeyValuePair.mSequenceNumber.Construct() = aKey;
    ConvertMap(aValue, aOutKeyValuePair.mScrollFrames.Construct(), ConvertScrollFrameData);
  }

  static void ConvertScrollFrameData(const APZTestData::ViewID& aKey,
                                     const APZTestData::ScrollFrameData& aValue,
                                     dom::ScrollFrameData& aOutKeyValuePair) {
    aOutKeyValuePair.mScrollId.Construct() = aKey;
    ConvertMap(aValue, aOutKeyValuePair.mEntries.Construct(), ConvertEntry);
  }

  static void ConvertEntry(const std::string& aKey,
                           const std::string& aValue,
                           dom::ScrollFrameDataEntry& aOutKeyValuePair) {
    ConvertString(aKey, aOutKeyValuePair.mKey.Construct());
    ConvertString(aValue, aOutKeyValuePair.mValue.Construct());
  }

  static void ConvertString(const std::string& aFrom, nsString& aOutTo) {
    aOutTo = NS_ConvertUTF8toUTF16(aFrom.c_str(), aFrom.size());
  }

  static void ConvertHitResult(const APZTestData::HitResult& aResult,
                               dom::APZHitResult& aOutHitResult) {
    aOutHitResult.mScreenX.Construct() = aResult.point.x;
    aOutHitResult.mScreenY.Construct() = aResult.point.y;
    static_assert(sizeof(aResult.result) == sizeof(uint16_t),
        "Expected CompositorHitTestInfo to be 16-bit");
    aOutHitResult.mHitResult.Construct() = static_cast<uint16_t>(aResult.result);
    aOutHitResult.mScrollId.Construct() = aResult.scrollId;
  }
};

bool
APZTestData::ToJS(JS::MutableHandleValue aOutValue, JSContext* aContext) const
{
  dom::APZTestData result;
  APZTestDataToJSConverter::ConvertAPZTestData(*this, result);
  return dom::ToJSValue(aContext, result, aOutValue);
}

} // namespace layers
} // namespace mozilla

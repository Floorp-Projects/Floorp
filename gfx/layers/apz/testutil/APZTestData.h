/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZTestData_h
#define mozilla_layers_APZTestData_h

#include <map>

#include "nsDebug.h"  // for NS_WARNING
#include "nsTArray.h"
#include "mozilla/Assertions.h"       // for MOZ_ASSERT
#include "mozilla/DebugOnly.h"        // for DebugOnly
#include "mozilla/GfxMessageUtils.h"  // for ParamTraits specializations
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/ToString.h"  // for ToString
#include "mozilla/gfx/CompositorHitTestInfo.h"
#include "mozilla/layers/LayersMessageUtils.h"  // for ParamTraits specializations
#include "mozilla/layers/ScrollableLayerGuid.h"
#include "ipc/IPCMessageUtils.h"
#include "js/TypeDecls.h"

namespace mozilla {
namespace layers {

typedef uint32_t SequenceNumber;

/**
 * This structure is used to store information logged by various gecko
 * components for later examination by test code.
 * It contains a bucket for every paint (initiated on the client side),
 * and every repaint request (initiated on the compositor side by
 * AsyncPanZoomController::RequestContentRepait), which are identified by
 * sequence numbers, and within that, a set of arbitrary string key/value
 * pairs for every scrollable frame, identified by a scroll id.
 * There are two instances of this data structure for every content thread:
 * one on the client side and one of the compositor side.
 * It also contains a list of hit-test results for MozMouseHittest events
 * dispatched during testing. This list is only populated on the compositor
 * instance of this class.
 */
// TODO(botond):
//  - Improve warnings/asserts.
//  - Add ability to associate a repaint request triggered during a layers
//    update with the sequence number of the paint that caused the layers
//    update.
class APZTestData {
  typedef ScrollableLayerGuid::ViewID ViewID;
  friend struct IPC::ParamTraits<APZTestData>;
  friend struct APZTestDataToJSConverter;

 public:
  void StartNewPaint(SequenceNumber aSequenceNumber) {
    // We should never get more than one paint with the same sequence number.
    MOZ_ASSERT(mPaints.find(aSequenceNumber) == mPaints.end());
    mPaints.insert(DataStore::value_type(aSequenceNumber, Bucket()));
  }
  void LogTestDataForPaint(SequenceNumber aSequenceNumber, ViewID aScrollId,
                           const std::string& aKey, const std::string& aValue) {
    LogTestDataImpl(mPaints, aSequenceNumber, aScrollId, aKey, aValue);
  }

  void StartNewRepaintRequest(SequenceNumber aSequenceNumber) {
    typedef std::pair<DataStore::iterator, bool> InsertResultT;
    DebugOnly<InsertResultT> insertResult = mRepaintRequests.insert(
        DataStore::value_type(aSequenceNumber, Bucket()));
    MOZ_ASSERT(((InsertResultT&)insertResult).second,
               "Already have a repaint request with this sequence number");
  }
  void LogTestDataForRepaintRequest(SequenceNumber aSequenceNumber,
                                    ViewID aScrollId, const std::string& aKey,
                                    const std::string& aValue) {
    LogTestDataImpl(mRepaintRequests, aSequenceNumber, aScrollId, aKey, aValue);
  }
  void RecordHitResult(const ScreenPoint& aPoint,
                       const mozilla::gfx::CompositorHitTestInfo& aResult,
                       const LayersId& aLayersId, const ViewID& aScrollId) {
    mHitResults.AppendElement(HitResult{aPoint, aResult, aLayersId, aScrollId});
  }
  void RecordAdditionalData(const std::string& aKey,
                            const std::string& aValue) {
    mAdditionalData[aKey] = aValue;
  }

  // Convert this object to a JS representation.
  bool ToJS(JS::MutableHandleValue aOutValue, JSContext* aContext) const;

  // Use dummy derived structures wrapping the typedefs to work around a type
  // name length limit in MSVC.
  typedef std::map<std::string, std::string> ScrollFrameDataBase;
  struct ScrollFrameData : ScrollFrameDataBase {};
  typedef std::map<ViewID, ScrollFrameData> BucketBase;
  struct Bucket : BucketBase {};
  typedef std::map<SequenceNumber, Bucket> DataStoreBase;
  struct DataStore : DataStoreBase {};
  struct HitResult {
    ScreenPoint point;
    mozilla::gfx::CompositorHitTestInfo result;
    LayersId layersId;
    ViewID scrollId;
  };

 private:
  DataStore mPaints;
  DataStore mRepaintRequests;
  nsTArray<HitResult> mHitResults;
  // Additional free-form data that's not grouped paint or scroll frame.
  std::map<std::string, std::string> mAdditionalData;

  void LogTestDataImpl(DataStore& aDataStore, SequenceNumber aSequenceNumber,
                       ViewID aScrollId, const std::string& aKey,
                       const std::string& aValue) {
    auto bucketIterator = aDataStore.find(aSequenceNumber);
    if (bucketIterator == aDataStore.end()) {
      MOZ_ASSERT(false,
                 "LogTestDataImpl called with nonexistent sequence number");
      return;
    }
    Bucket& bucket = bucketIterator->second;
    ScrollFrameData& scrollFrameData =
        bucket[aScrollId];  // create if doesn't exist
    MOZ_ASSERT(scrollFrameData.find(aKey) == scrollFrameData.end() ||
               scrollFrameData[aKey] == aValue);
    scrollFrameData.insert(ScrollFrameData::value_type(aKey, aValue));
  }
};

// A helper class for logging data for a paint.
class APZPaintLogHelper {
 public:
  APZPaintLogHelper(APZTestData* aTestData, SequenceNumber aPaintSequenceNumber)
      : mTestData(aTestData), mPaintSequenceNumber(aPaintSequenceNumber) {
    MOZ_ASSERT(!aTestData || StaticPrefs::apz_test_logging_enabled(),
               "don't call me");
  }

  template <typename Value>
  void LogTestData(ScrollableLayerGuid::ViewID aScrollId,
                   const std::string& aKey, const Value& aValue) const {
    if (mTestData) {  // avoid stringifying if mTestData == nullptr
      LogTestData(aScrollId, aKey, ToString(aValue));
    }
  }

  void LogTestData(ScrollableLayerGuid::ViewID aScrollId,
                   const std::string& aKey, const std::string& aValue) const {
    if (mTestData) {
      mTestData->LogTestDataForPaint(mPaintSequenceNumber, aScrollId, aKey,
                                     aValue);
    }
  }

 private:
  APZTestData* mTestData;
  SequenceNumber mPaintSequenceNumber;
};

}  // namespace layers
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::layers::APZTestData> {
  typedef mozilla::layers::APZTestData paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mPaints);
    WriteParam(aMsg, aParam.mRepaintRequests);
    WriteParam(aMsg, aParam.mHitResults);
    WriteParam(aMsg, aParam.mAdditionalData);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mPaints) &&
            ReadParam(aMsg, aIter, &aResult->mRepaintRequests) &&
            ReadParam(aMsg, aIter, &aResult->mHitResults) &&
            ReadParam(aMsg, aIter, &aResult->mAdditionalData));
  }
};

template <>
struct ParamTraits<mozilla::layers::APZTestData::ScrollFrameData>
    : ParamTraits<mozilla::layers::APZTestData::ScrollFrameDataBase> {};

template <>
struct ParamTraits<mozilla::layers::APZTestData::Bucket>
    : ParamTraits<mozilla::layers::APZTestData::BucketBase> {};

template <>
struct ParamTraits<mozilla::layers::APZTestData::DataStore>
    : ParamTraits<mozilla::layers::APZTestData::DataStoreBase> {};

template <>
struct ParamTraits<mozilla::layers::APZTestData::HitResult> {
  typedef mozilla::layers::APZTestData::HitResult paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.point);
    WriteParam(aMsg, aParam.result);
    WriteParam(aMsg, aParam.layersId);
    WriteParam(aMsg, aParam.scrollId);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->point) &&
            ReadParam(aMsg, aIter, &aResult->result) &&
            ReadParam(aMsg, aIter, &aResult->layersId) &&
            ReadParam(aMsg, aIter, &aResult->scrollId));
  }
};

}  // namespace IPC

#endif /* mozilla_layers_APZTestData_h */

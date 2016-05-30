/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZTestData_h
#define mozilla_layers_APZTestData_h

#include <map>

#include "FrameMetrics.h"
#include "nsDebug.h"             // for NS_WARNING
#include "mozilla/Assertions.h"  // for MOZ_ASSERT
#include "mozilla/DebugOnly.h"   // for DebugOnly
#include "mozilla/ToString.h"    // for ToString
#include "ipc/IPCMessageUtils.h"
#include "js/TypeDecls.h"

namespace mozilla {
namespace layers {

typedef uint32_t SequenceNumber;

/**
 * This structure is used to store information logged by various gecko
 * components for later examination by test code.
 * It consists of a bucket for every paint (initiated on the client side),
 * and every repaint request (initiated on the compositor side by
 * AsyncPanZoomController::RequestContentRepait), which are identified by
 * sequence numbers, and within that, a set of arbitrary string key/value
 * pairs for every scrollable frame, identified by a scroll id.
 * There are two instances of this data structure for every content thread:
 * one on the client side and one of the compositor side.
 */
// TODO(botond):
//  - Improve warnings/asserts.
//  - Add ability to associate a repaint request triggered during a layers update
//    with the sequence number of the paint that caused the layers update.
class APZTestData {
  typedef FrameMetrics::ViewID ViewID;
  friend struct IPC::ParamTraits<APZTestData>;
  friend struct APZTestDataToJSConverter;
public:
  void StartNewPaint(SequenceNumber aSequenceNumber) {
    // We should never get more than one paint with the same sequence number.
    MOZ_ASSERT(mPaints.find(aSequenceNumber) == mPaints.end());
    mPaints.insert(DataStore::value_type(aSequenceNumber, Bucket()));
  }
  void LogTestDataForPaint(SequenceNumber aSequenceNumber,
                           ViewID aScrollId,
                           const std::string& aKey,
                           const std::string& aValue) {
    LogTestDataImpl(mPaints, aSequenceNumber, aScrollId, aKey, aValue);
  }

  void StartNewRepaintRequest(SequenceNumber aSequenceNumber) {
    typedef std::pair<DataStore::iterator, bool> InsertResultT;
    DebugOnly<InsertResultT> insertResult = mRepaintRequests.insert(DataStore::value_type(aSequenceNumber, Bucket()));
    MOZ_ASSERT(((InsertResultT&)insertResult).second, "Already have a repaint request with this sequence number");
  }
  void LogTestDataForRepaintRequest(SequenceNumber aSequenceNumber,
                                    ViewID aScrollId,
                                    const std::string& aKey,
                                    const std::string& aValue) {
    LogTestDataImpl(mRepaintRequests, aSequenceNumber, aScrollId, aKey, aValue);
  }

  // Convert this object to a JS representation.
  bool ToJS(JS::MutableHandleValue aOutValue, JSContext* aContext) const;

  // Use dummy derived structures wrapping the tyepdefs to work around a type
  // name length limit in MSVC.
  typedef std::map<std::string, std::string> ScrollFrameDataBase;
  struct ScrollFrameData : ScrollFrameDataBase {};
  typedef std::map<ViewID, ScrollFrameData> BucketBase;
  struct Bucket : BucketBase {};
  typedef std::map<SequenceNumber, Bucket> DataStoreBase;
  struct DataStore : DataStoreBase {};
private:
  DataStore mPaints;
  DataStore mRepaintRequests;

  void LogTestDataImpl(DataStore& aDataStore,
                       SequenceNumber aSequenceNumber,
                       ViewID aScrollId,
                       const std::string& aKey,
                       const std::string& aValue) {
    auto bucketIterator = aDataStore.find(aSequenceNumber);
    if (bucketIterator == aDataStore.end()) {
      MOZ_ASSERT(false, "LogTestDataImpl called with nonexistent sequence number");
      return;
    }
    Bucket& bucket = bucketIterator->second;
    ScrollFrameData& scrollFrameData = bucket[aScrollId];  // create if doesn't exist
    MOZ_ASSERT(scrollFrameData.find(aKey) == scrollFrameData.end()
            || scrollFrameData[aKey] == aValue);
    scrollFrameData.insert(ScrollFrameData::value_type(aKey, aValue));
  }
};

// A helper class for logging data for a paint.
class APZPaintLogHelper {
public:
  APZPaintLogHelper(APZTestData* aTestData, SequenceNumber aPaintSequenceNumber)
    : mTestData(aTestData),
      mPaintSequenceNumber(aPaintSequenceNumber)
  {}

  template <typename Value>
  void LogTestData(FrameMetrics::ViewID aScrollId,
                   const std::string& aKey,
                   const Value& aValue) const {
    if (mTestData) {  // avoid stringifying if mTestData == nullptr
      LogTestData(aScrollId, aKey, ToString(aValue));
    }
  }

  void LogTestData(FrameMetrics::ViewID aScrollId,
                   const std::string& aKey,
                   const std::string& aValue) const {
    if (mTestData) {
      mTestData->LogTestDataForPaint(mPaintSequenceNumber, aScrollId, aKey, aValue);
    }
  }
private:
  APZTestData* mTestData;
  SequenceNumber mPaintSequenceNumber;
};

} // namespace layers
} // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::layers::APZTestData>
{
  typedef mozilla::layers::APZTestData paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mPaints);
    WriteParam(aMsg, aParam.mRepaintRequests);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mPaints) &&
            ReadParam(aMsg, aIter, &aResult->mRepaintRequests));
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

} // namespace IPC


#endif /* mozilla_layers_APZTestData_h */

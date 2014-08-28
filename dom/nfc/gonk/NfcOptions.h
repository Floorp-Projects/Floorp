/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NfcOptions_h
#define NfcOptions_h

#include "mozilla/dom/NfcOptionsBinding.h"

namespace mozilla {

struct NDEFRecordStruct
{
  uint8_t mTnf;
  nsTArray<uint8_t> mType;
  nsTArray<uint8_t> mId;
  nsTArray<uint8_t> mPayload;
};

struct CommandOptions
{
  CommandOptions(const mozilla::dom::NfcCommandOptions& aOther) {

#define COPY_FIELD(prop) prop = aOther.prop;

#define COPY_OPT_FIELD(prop, defaultValue)            \
  if (aOther.prop.WasPassed()) {                      \
    prop = aOther.prop.Value();                       \
  } else {                                            \
    prop = defaultValue;                              \
  }

    COPY_FIELD(mType)
    COPY_FIELD(mRequestId)
    COPY_OPT_FIELD(mSessionId, 0)
    COPY_OPT_FIELD(mPowerLevel, 0)
    COPY_OPT_FIELD(mTechType, 0)

    if (!aOther.mRecords.WasPassed()) {
      return;
    }

    mozilla::dom::Sequence<mozilla::dom::NDEFRecord> const & currentValue = aOther.mRecords.InternalValue();
    int count = currentValue.Length();
    for (uint32_t i = 0; i < count; i++) {
      NDEFRecordStruct record;
      record.mTnf = currentValue[i].mTnf.Value();

      if (currentValue[i].mType.WasPassed()) {
        currentValue[i].mType.Value().ComputeLengthAndData();
        record.mType.AppendElements(currentValue[i].mType.Value().Data(),
                                    currentValue[i].mType.Value().Length());
      }

      if (currentValue[i].mId.WasPassed()) {
        currentValue[i].mId.Value().ComputeLengthAndData();
        record.mId.AppendElements(currentValue[i].mId.Value().Data(),
                                  currentValue[i].mId.Value().Length());
      }

      if (currentValue[i].mPayload.WasPassed()) {
        currentValue[i].mPayload.Value().ComputeLengthAndData();
        record.mPayload.AppendElements(currentValue[i].mPayload.Value().Data(),
                                       currentValue[i].mPayload.Value().Length());
      }

      mRecords.AppendElement(record);
    }

#undef COPY_FIELD
#undef COPY_OPT_FIELD
  }

  nsString mType;
  int32_t mSessionId;
  nsString mRequestId;
  int32_t mPowerLevel;
  int32_t mTechType;
  nsTArray<NDEFRecordStruct> mRecords;
};

struct EventOptions
{
  EventOptions()
    : mType(EmptyString()), mStatus(-1), mSessionId(-1), mRequestId(EmptyString()), mMajorVersion(-1), mMinorVersion(-1),
      mIsReadOnly(-1), mCanBeMadeReadOnly(-1), mMaxSupportedLength(-1), mPowerLevel(-1), mOrigin(EmptyString()),
      mOriginType(-1), mOriginIndex(-1)
  {}

  nsString mType;
  int32_t mStatus;
  int32_t mSessionId;
  nsString mRequestId;
  int32_t mMajorVersion;
  int32_t mMinorVersion;
  nsTArray<uint8_t> mTechList;
  nsTArray<NDEFRecordStruct> mRecords;
  int32_t mIsReadOnly;
  int32_t mCanBeMadeReadOnly;
  int32_t mMaxSupportedLength;
  int32_t mPowerLevel;

  nsString mOrigin;
  int32_t mOriginType;
  int32_t mOriginIndex;
  nsTArray<uint8_t> mAid;
  nsTArray<uint8_t> mPayload;
};

} // namespace mozilla

#endif

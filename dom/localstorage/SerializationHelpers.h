/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_SerializationHelpers_h
#define mozilla_dom_localstorage_SerializationHelpers_h

#include <string>
#include "chrome/common/ipc_message_utils.h"
#include "ipc/EnumSerializer.h"
#include "mozilla/dom/LSSnapshot.h"
#include "mozilla/dom/LSValue.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::LSSnapshot::LoadState>
    : public ContiguousEnumSerializer<
          mozilla::dom::LSSnapshot::LoadState,
          mozilla::dom::LSSnapshot::LoadState::Initial,
          mozilla::dom::LSSnapshot::LoadState::EndGuard> {};

template <>
struct ParamTraits<mozilla::dom::LSValue::CompressionType>
    : public ContiguousEnumSerializer<
          mozilla::dom::LSValue::CompressionType,
          mozilla::dom::LSValue::CompressionType::UNCOMPRESSED,
          mozilla::dom::LSValue::CompressionType::NUM_TYPES> {};

static_assert(
    0u == static_cast<uint8_t>(mozilla::dom::LSValue::ConversionType::NONE));
template <>
struct ParamTraits<mozilla::dom::LSValue::ConversionType>
    : public ContiguousEnumSerializer<
          mozilla::dom::LSValue::ConversionType,
          mozilla::dom::LSValue::ConversionType::NONE,
          mozilla::dom::LSValue::ConversionType::NUM_TYPES> {};

template <>
struct ParamTraits<mozilla::dom::LSValue> {
  typedef mozilla::dom::LSValue paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mBuffer);
    WriteParam(aWriter, aParam.mUTF16Length);
    WriteParam(aWriter, aParam.mConversionType);
    WriteParam(aWriter, aParam.mCompressionType);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mBuffer) &&
           ReadParam(aReader, &aResult->mUTF16Length) &&
           ReadParam(aReader, &aResult->mConversionType) &&
           ReadParam(aReader, &aResult->mCompressionType);
  }
};

}  // namespace IPC

#endif  // mozilla_dom_localstorage_SerializationHelpers_h

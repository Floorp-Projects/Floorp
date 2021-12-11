/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IPC_ErrorIPCUtils_h
#define IPC_ErrorIPCUtils_h

#include <utility>

#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorResult.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::ErrNum>
    : public ContiguousEnumSerializer<
          mozilla::dom::ErrNum, mozilla::dom::ErrNum(0),
          mozilla::dom::ErrNum(mozilla::dom::Err_Limit)> {};

template <>
struct ParamTraits<mozilla::ErrorResult> {
  typedef mozilla::ErrorResult paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    // It should be the case that mMightHaveUnreportedJSException can only be
    // true when we're expecting a JS exception.  We cannot send such messages
    // over the IPC channel since there is no sane way of transferring the JS
    // value over to the other side.  Callers should never do that.
    MOZ_ASSERT_IF(aParam.IsJSException(),
                  aParam.mMightHaveUnreportedJSException);
    if (aParam.IsJSException()
#ifdef DEBUG
        || aParam.mMightHaveUnreportedJSException
#endif
    ) {
      MOZ_CRASH(
          "Cannot encode an ErrorResult representing a Javascript exception");
    }

    WriteParam(aMsg, aParam.mResult);
    WriteParam(aMsg, aParam.IsErrorWithMessage());
    WriteParam(aMsg, aParam.IsDOMException());
    if (aParam.IsErrorWithMessage()) {
      aParam.SerializeMessage(aMsg);
    } else if (aParam.IsDOMException()) {
      aParam.SerializeDOMExceptionInfo(aMsg);
    }
  }

  static void Write(Message* aMsg, paramType&& aParam) {
    Write(aMsg, static_cast<const paramType&>(aParam));
    aParam.SuppressException();
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    paramType readValue;
    if (!ReadParam(aMsg, aIter, &readValue.mResult)) {
      return false;
    }
    bool hasMessage = false;
    if (!ReadParam(aMsg, aIter, &hasMessage)) {
      return false;
    }
    bool hasDOMExceptionInfo = false;
    if (!ReadParam(aMsg, aIter, &hasDOMExceptionInfo)) {
      return false;
    }
    if (hasMessage && hasDOMExceptionInfo) {
      // Shouldn't have both!
      return false;
    }
    if (hasMessage && !readValue.DeserializeMessage(aMsg, aIter)) {
      return false;
    } else if (hasDOMExceptionInfo &&
               !readValue.DeserializeDOMExceptionInfo(aMsg, aIter)) {
      return false;
    }
    *aResult = std::move(readValue);
    return true;
  }
};

template <>
struct ParamTraits<mozilla::CopyableErrorResult> {
  typedef mozilla::CopyableErrorResult paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    ParamTraits<mozilla::ErrorResult>::Write(aMsg, aParam);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    // We can't cast *aResult to ErrorResult&, so cheat and just cast
    // to ErrorResult*.
    return ParamTraits<mozilla::ErrorResult>::Read(
        aMsg, aIter, reinterpret_cast<mozilla::ErrorResult*>(aResult));
  }
};

}  // namespace IPC

#endif

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TABMESSAGE_UTILS_H
#define TABMESSAGE_UTILS_H

#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/AudioChannelBinding.h"
#include "nsIDOMEvent.h"
#include "nsCOMPtr.h"

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

namespace mozilla {
namespace dom {
struct RemoteDOMEvent
{
  // Make sure to set the owner after deserializing.
  nsCOMPtr<nsIDOMEvent> mEvent;
};

bool ReadRemoteEvent(const IPC::Message* aMsg, void** aIter,
                     mozilla::dom::RemoteDOMEvent* aResult);

#ifdef MOZ_CRASHREPORTER
typedef CrashReporter::ThreadId NativeThreadId;
#else
// unused in this case
typedef int32_t NativeThreadId;
#endif

}
}

namespace IPC {

template<>
struct ParamTraits<mozilla::dom::RemoteDOMEvent>
{
  typedef mozilla::dom::RemoteDOMEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aParam.mEvent->Serialize(aMsg, true);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return mozilla::dom::ReadRemoteEvent(aMsg, aIter, aResult);
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
  }
};

template<>
struct ParamTraits<mozilla::dom::AudioChannel>
{
  typedef mozilla::dom::AudioChannel paramType;

  static bool IsLegalValue(const paramType &aValue) {
    return aValue <= mozilla::dom::AudioChannel::Publicnotification;
  }

  static void Write(Message* aMsg, const paramType& aValue) {
    MOZ_ASSERT(IsLegalValue(aValue));
    WriteParam(aMsg, (uint32_t)aValue);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult) {
    uint32_t value;
    if(!ReadParam(aMsg, aIter, &value) ||
       !IsLegalValue(paramType(value))) {
      return false;
    }
    *aResult = paramType(value);
    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
  }
};

template <>
struct ParamTraits<nsEventStatus>
  : public ContiguousEnumSerializer<nsEventStatus,
                                    nsEventStatus_eIgnore,
                                    nsEventStatus_eSentinel>
{ };

}

#endif

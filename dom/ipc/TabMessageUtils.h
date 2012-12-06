/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TABMESSAGE_UTILS_H
#define TABMESSAGE_UTILS_H

#include "AudioChannelCommon.h"
#include "ipc/IPCMessageUtils.h"
#include "nsIDOMEvent.h"
#include "nsCOMPtr.h"

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

namespace mozilla {
namespace dom {
struct RemoteDOMEvent
{
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

template <>
struct ParamTraits<mozilla::dom::AudioChannelType>
  : public EnumSerializer<mozilla::dom::AudioChannelType,
                          mozilla::dom::AUDIO_CHANNEL_NORMAL,
                          mozilla::dom::AUDIO_CHANNEL_LAST>
{ };

}


#endif

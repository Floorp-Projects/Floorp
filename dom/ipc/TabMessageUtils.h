/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TABMESSAGE_UTILS_H
#define TABMESSAGE_UTILS_H

#include "ipc/IPCMessageUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/Event.h"
#include "nsExceptionHandler.h"
#include "nsIRemoteTab.h"
#include "nsPIDOMWindow.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {
class Event;

struct RemoteDOMEvent {
  // Make sure to set the owner after deserializing.
  RefPtr<Event> mEvent;
};

bool ReadRemoteEvent(const IPC::Message* aMsg, PickleIterator* aIter,
                     mozilla::dom::RemoteDOMEvent* aResult);

typedef CrashReporter::ThreadId NativeThreadId;

}  // namespace dom
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::RemoteDOMEvent> {
  typedef mozilla::dom::RemoteDOMEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    aParam.mEvent->Serialize(aMsg, true);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return mozilla::dom::ReadRemoteEvent(aMsg, aIter, aResult);
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {}
};

template <>
struct ParamTraits<nsEventStatus>
    : public ContiguousEnumSerializer<nsEventStatus, nsEventStatus_eIgnore,
                                      nsEventStatus_eSentinel> {};

template <>
struct ParamTraits<nsSizeMode>
    : public ContiguousEnumSerializer<nsSizeMode, nsSizeMode_Normal,
                                      nsSizeMode_Invalid> {};

template <>
struct ParamTraits<UIStateChangeType>
    : public ContiguousEnumSerializer<UIStateChangeType,
                                      UIStateChangeType_NoChange,
                                      UIStateChangeType_Invalid> {};

template <>
struct ParamTraits<nsIRemoteTab::NavigationType>
    : public ContiguousEnumSerializerInclusive<
          nsIRemoteTab::NavigationType,
          nsIRemoteTab::NavigationType::NAVIGATE_BACK,
          nsIRemoteTab::NavigationType::NAVIGATE_URL> {};

}  // namespace IPC

#endif  // TABMESSAGE_UTILS_H

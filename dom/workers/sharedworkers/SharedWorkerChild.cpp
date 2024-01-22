/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedWorkerChild.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/ErrorEventBinding.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/SecurityPolicyViolationEvent.h"
#include "mozilla/dom/SecurityPolicyViolationEventBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/SharedWorker.h"
#include "mozilla/dom/WebTransport.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WorkerError.h"
#include "mozilla/dom/locks/LockManagerChild.h"

namespace mozilla {

using namespace ipc;

namespace dom {

SharedWorkerChild::SharedWorkerChild() : mParent(nullptr), mActive(true) {}

SharedWorkerChild::~SharedWorkerChild() = default;

void SharedWorkerChild::ActorDestroy(ActorDestroyReason aWhy) {
  mActive = false;
}

void SharedWorkerChild::SendClose() {
  if (mActive) {
    // This is the last message.
    mActive = false;
    PSharedWorkerChild::SendClose();
  }
}

void SharedWorkerChild::SendSuspend() {
  if (mActive) {
    PSharedWorkerChild::SendSuspend();
  }
}

void SharedWorkerChild::SendResume() {
  if (mActive) {
    PSharedWorkerChild::SendResume();
  }
}

void SharedWorkerChild::SendFreeze() {
  if (mActive) {
    PSharedWorkerChild::SendFreeze();
  }
}

void SharedWorkerChild::SendThaw() {
  if (mActive) {
    PSharedWorkerChild::SendThaw();
  }
}

IPCResult SharedWorkerChild::RecvError(const ErrorValue& aValue) {
  if (!mParent) {
    return IPC_OK();
  }

  if (aValue.type() == ErrorValue::Tnsresult) {
    mParent->ErrorPropagation(aValue.get_nsresult());
    return IPC_OK();
  }

  nsPIDOMWindowInner* window = mParent->GetOwner();
  uint64_t innerWindowId = window ? window->WindowID() : 0;

  if (aValue.type() == ErrorValue::TCSPViolation) {
    SecurityPolicyViolationEventInit violationEventInit;
    if (NS_WARN_IF(
            !violationEventInit.Init(aValue.get_CSPViolation().json()))) {
      return IPC_OK();
    }

    if (NS_WARN_IF(!window)) {
      return IPC_OK();
    }

    RefPtr<EventTarget> eventTarget = window->GetExtantDoc();
    if (NS_WARN_IF(!eventTarget)) {
      return IPC_OK();
    }

    RefPtr<Event> event = SecurityPolicyViolationEvent::Constructor(
        eventTarget, u"securitypolicyviolation"_ns, violationEventInit);
    event->SetTrusted(true);

    eventTarget->DispatchEvent(*event);
    return IPC_OK();
  }

  if (aValue.type() == ErrorValue::TErrorData &&
      aValue.get_ErrorData().isWarning()) {
    // Don't fire any events for warnings. Just log to console.
    WorkerErrorReport::LogErrorToConsole(aValue.get_ErrorData(), innerWindowId);
    return IPC_OK();
  }

  AutoJSAPI jsapi;
  jsapi.Init();

  RefPtr<Event> event;
  if (aValue.type() == ErrorValue::TErrorData) {
    const ErrorData& errorData = aValue.get_ErrorData();
    RootedDictionary<ErrorEventInit> errorInit(jsapi.cx());
    errorInit.mBubbles = false;
    errorInit.mCancelable = true;
    errorInit.mMessage = errorData.message();
    errorInit.mFilename = errorData.filename();
    errorInit.mLineno = errorData.lineNumber();
    errorInit.mColno = errorData.columnNumber();

    event = ErrorEvent::Constructor(mParent, u"error"_ns, errorInit);
  } else {
    event = Event::Constructor(mParent, u"error"_ns, EventInit());
  }
  event->SetTrusted(true);

  ErrorResult res;
  bool defaultActionEnabled =
      mParent->DispatchEvent(*event, CallerType::System, res);
  if (res.Failed()) {
    ThrowAndReport(window, res.StealNSResult());
    return IPC_OK();
  }

  if (aValue.type() != ErrorValue::TErrorData) {
    MOZ_ASSERT(aValue.type() == ErrorValue::Tvoid_t);
    return IPC_OK();
  }

  if (defaultActionEnabled) {
    WorkerErrorReport::LogErrorToConsole(aValue.get_ErrorData(), innerWindowId);
  }

  return IPC_OK();
}

IPCResult SharedWorkerChild::RecvNotifyLock(bool aCreated) {
  if (!mParent) {
    return IPC_OK();
  }

  locks::LockManagerChild::NotifyBFCacheOnMainThread(mParent->GetOwner(),
                                                     aCreated);

  return IPC_OK();
}

IPCResult SharedWorkerChild::RecvNotifyWebTransport(bool aCreated) {
  if (!mParent) {
    return IPC_OK();
  }

  WebTransport::NotifyBFCacheOnMainThread(mParent->GetOwner(), aCreated);

  return IPC_OK();
}

IPCResult SharedWorkerChild::RecvTerminate() {
  if (mParent) {
    mParent->Close();
  }

  return IPC_OK();
}

}  // namespace dom
}  // namespace mozilla

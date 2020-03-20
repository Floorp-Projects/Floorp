/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedWorkerChild.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/ErrorEventBinding.h"
#include "mozilla/dom/WorkerError.h"

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

  if (aValue.type() == ErrorValue::TErrorData &&
      aValue.get_ErrorData().isWarning()) {
    // Don't fire any events anywhere.  Just log to console.
    // XXXbz should we log to all the consoles of all the relevant windows?
    WorkerErrorReport::LogErrorToConsole(aValue.get_ErrorData(), 0);
    return IPC_OK();
  }

  // May be null.
  nsPIDOMWindowInner* window = mParent->GetOwner();

  RefPtr<Event> event;

  AutoJSAPI jsapi;
  jsapi.Init();

  if (aValue.type() == ErrorValue::TErrorData) {
    const ErrorData& errorData = aValue.get_ErrorData();
    RootedDictionary<ErrorEventInit> errorInit(jsapi.cx());
    errorInit.mBubbles = false;
    errorInit.mCancelable = true;
    errorInit.mMessage = errorData.message();
    errorInit.mFilename = errorData.filename();
    errorInit.mLineno = errorData.lineNumber();
    errorInit.mColno = errorData.columnNumber();

    event =
        ErrorEvent::Constructor(mParent, NS_LITERAL_STRING("error"), errorInit);
  } else {
    event =
        Event::Constructor(mParent, NS_LITERAL_STRING("error"), EventInit());
  }

  if (!event) {
    ThrowAndReport(window, NS_ERROR_UNEXPECTED);
    return IPC_OK();
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

  if (!defaultActionEnabled) {
    return IPC_OK();
  }

  bool shouldLogErrorToConsole = true;

  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(window);
  MOZ_ASSERT(sgo);

  const ErrorData& errorData = aValue.get_ErrorData();

  MOZ_ASSERT(NS_IsMainThread());
  RootedDictionary<ErrorEventInit> errorInit(jsapi.cx());
  errorInit.mLineno = errorData.lineNumber();
  errorInit.mColno = errorData.columnNumber();
  errorInit.mFilename = errorData.filename();
  errorInit.mMessage = errorData.message();
  errorInit.mCancelable = true;
  errorInit.mBubbles = true;

  nsEventStatus status = nsEventStatus_eIgnore;
  if (!sgo->HandleScriptError(errorInit, &status)) {
    ThrowAndReport(window, NS_ERROR_UNEXPECTED);
    return IPC_OK();
  }

  if (status == nsEventStatus_eConsumeNoDefault) {
    shouldLogErrorToConsole = false;
  }

  // Finally log a warning in the console if no window tried to prevent it.
  if (shouldLogErrorToConsole) {
    WorkerErrorReport::LogErrorToConsole(aValue.get_ErrorData(), 0);
  }

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

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MIDIPlatformRunnables.h"
#include "mozilla/dom/MIDIPlatformService.h"
#include "mozilla/dom/MIDIPortParent.h"
#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla::dom {

NS_IMETHODIMP
MIDIBackgroundRunnable::Run() {
  MIDIPlatformService::AssertThread();
  if (!MIDIPlatformService::IsRunning()) {
    return NS_OK;
  }
  RunInternal();
  return NS_OK;
}

void ReceiveRunnable::RunInternal() {
  MIDIPlatformService::Get()->CheckAndReceive(mPortId, mMsgs);
}

void AddPortRunnable::RunInternal() {
  MIDIPlatformService::Get()->AddPortInfo(mPortInfo);
}

void RemovePortRunnable::RunInternal() {
  MIDIPlatformService::Get()->RemovePortInfo(mPortInfo);
}

void SetStatusRunnable::RunInternal() {
  MIDIPlatformService::Get()->UpdateStatus(mPort, mState, mConnection);
}

void SendPortListRunnable::RunInternal() {
  // Unlike other runnables, SendPortListRunnable should just exit quietly if
  // the service has died.
  if (!MIDIPlatformService::IsRunning()) {
    return;
  }
  MIDIPlatformService::Get()->SendPortList();
}

}  // namespace mozilla::dom

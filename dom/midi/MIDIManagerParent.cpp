/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MIDIManagerParent.h"
#include "mozilla/dom/MIDIPlatformService.h"

namespace mozilla::dom {

void MIDIManagerParent::ActorDestroy(ActorDestroyReason aWhy) {
  if (MIDIPlatformService::IsRunning()) {
    MIDIPlatformService::Get()->RemoveManager(this);
  }
}

mozilla::ipc::IPCResult MIDIManagerParent::RecvRefresh() {
  MIDIPlatformService::Get()->Refresh();
  return IPC_OK();
}

mozilla::ipc::IPCResult MIDIManagerParent::RecvShutdown() {
  // The two-step shutdown process here is the standard way to ensure that the
  // child receives any messages sent by the server (since either sending or
  // receiving __delete__ prevents any further messages from being received).
  // This was necessary before bug 1547085 when discarded messages would
  // trigger a crash, and is probably unnecessary now, but we leave it in place
  // just in case.
  Close();
  return IPC_OK();
}

}  // namespace mozilla::dom

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MIDIManagerChild.h"
#include "mozilla/dom/MIDIAccessManager.h"

using namespace mozilla::dom;

MIDIManagerChild::MIDIManagerChild() : mShutdown(false) {}

mozilla::ipc::IPCResult MIDIManagerChild::RecvMIDIPortListUpdate(
    const MIDIPortList& aPortList) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mShutdown) {
    return IPC_OK();
  }
  MOZ_ASSERT(MIDIAccessManager::IsRunning());
  MIDIAccessManager::Get()->Update(aPortList);
  return IPC_OK();
}

void MIDIManagerChild::SetActorAlive() {
  // IPC Channels for MIDIManagers are created and managed by MIDIAccessManager,
  // so once the actor is created, we'll need to add a reference to keep it
  // alive until BackgroundChildImpl kills it.
  AddRef();
}

void MIDIManagerChild::Shutdown() {
  MOZ_ASSERT(!mShutdown);
  mShutdown = true;
  SendShutdown();
}

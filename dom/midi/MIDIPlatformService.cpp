/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MIDIPlatformService.h"
#include "MIDIMessageQueue.h"
#include "TestMIDIPlatformService.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/MIDIManagerParent.h"
#include "mozilla/dom/MIDIPlatformRunnables.h"
#include "mozilla/dom/MIDIUtils.h"
#include "mozilla/dom/PMIDIManagerParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/dom/MIDIPortParent.h"

using namespace mozilla;
using namespace mozilla::dom;

MIDIPlatformService::MIDIPlatformService()
    : mHasSentPortList(false),
      mMessageQueueMutex("MIDIPlatformServce::mMessageQueueMutex") {}

MIDIPlatformService::~MIDIPlatformService() = default;

void MIDIPlatformService::CheckAndReceive(const nsAString& aPortId,
                                          const nsTArray<MIDIMessage>& aMsgs) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  for (auto& port : mPorts) {
    // TODO Clean this up when we split input/output port arrays
    if (port->MIDIPortInterface::Id() != aPortId ||
        port->Type() != MIDIPortType::Input ||
        port->ConnectionState() != MIDIPortConnectionState::Open) {
      continue;
    }
    if (!port->SysexEnabled()) {
      nsTArray<MIDIMessage> msgs;
      for (auto& msg : aMsgs) {
        if (!MIDIUtils::IsSysexMessage(msg)) {
          msgs.AppendElement(msg);
        }
      }
      Unused << port->SendReceive(msgs);
    } else {
      Unused << port->SendReceive(aMsgs);
    }
  }
}

void MIDIPlatformService::AddPort(MIDIPortParent* aPort) {
  MOZ_ASSERT(aPort);
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  mPorts.AppendElement(aPort);
}

void MIDIPlatformService::RemovePort(MIDIPortParent* aPort) {
  // This should only be called from the background thread, when a MIDIPort
  // actor has been destroyed.
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_ASSERT(aPort);
  mPorts.RemoveElement(aPort);
  MaybeStop();
}

void MIDIPlatformService::BroadcastState(const MIDIPortInfo& aPortInfo,
                                         const MIDIPortDeviceState& aState) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  for (auto& p : mPorts) {
    if (p->MIDIPortInterface::Id() == aPortInfo.id() &&
        p->DeviceState() != aState) {
      p->SendUpdateStatus(aState, p->ConnectionState());
    }
  }
}

void MIDIPlatformService::QueueMessages(const nsAString& aId,
                                        nsTArray<MIDIMessage>& aMsgs) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  {
    MutexAutoLock lock(mMessageQueueMutex);
    MIDIMessageQueue* msgQueue = mMessageQueues.GetOrInsertNew(aId);
    msgQueue->Add(aMsgs);
    ScheduleSend(aId);
  }
}

void MIDIPlatformService::SendPortList() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  mHasSentPortList = true;
  MIDIPortList l;
  for (auto& el : mPortInfo) {
    l.ports().AppendElement(el);
  }
  for (auto& mgr : mManagers) {
    Unused << mgr->SendMIDIPortListUpdate(l);
  }
}

void MIDIPlatformService::Clear(MIDIPortParent* aPort) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_ASSERT(aPort);
  {
    MutexAutoLock lock(mMessageQueueMutex);
    MIDIMessageQueue* msgQueue =
        mMessageQueues.Get(aPort->MIDIPortInterface::Id());
    if (msgQueue) {
      msgQueue->Clear();
    }
  }
}

void MIDIPlatformService::AddPortInfo(MIDIPortInfo& aPortInfo) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());

  mPortInfo.AppendElement(aPortInfo);

  // ORDER MATTERS HERE.
  //
  // When MIDI hardware is disconnected, all open MIDIPort objects revert to a
  // "pending" state, and they are removed from the port maps of MIDIAccess
  // objects. We need to send connection updates to all living ports first, THEN
  // we can send port list updates to all of the live MIDIAccess objects. We
  // have to go in this order because if a port object is still held live but is
  // disconnected, it needs to readd itself to its originating MIDIAccess
  // object. Running SendPortList first would cause MIDIAccess to create a new
  // MIDIPort object, which would conflict (i.e. old disconnected object != new
  // object in port map, which is against spec).
  for (auto& port : mPorts) {
    if (port->MIDIPortInterface::Id() == aPortInfo.id()) {
      port->SendUpdateStatus(MIDIPortDeviceState::Connected,
                             port->ConnectionState());
    }
  }
  if (mHasSentPortList) {
    SendPortList();
  }
}

void MIDIPlatformService::RemovePortInfo(MIDIPortInfo& aPortInfo) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  mPortInfo.RemoveElement(aPortInfo);
  BroadcastState(aPortInfo, MIDIPortDeviceState::Disconnected);
  if (mHasSentPortList) {
    SendPortList();
  }
}

StaticRefPtr<MIDIPlatformService> gMIDIPlatformService;

// static
bool MIDIPlatformService::IsRunning() {
  return gMIDIPlatformService != nullptr;
}

void MIDIPlatformService::Close(mozilla::dom::MIDIPortParent* aPort) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  {
    MutexAutoLock lock(mMessageQueueMutex);
    MIDIMessageQueue* msgQueue =
        mMessageQueues.Get(aPort->MIDIPortInterface::Id());
    if (msgQueue) {
      msgQueue->ClearAfterNow();
    }
  }
  // Send all messages before sending a close request
  ScheduleSend(aPort->MIDIPortInterface::Id());
  // TODO We should probably have the send function schedule closing
  ScheduleClose(aPort);
}

// static
MIDIPlatformService* MIDIPlatformService::Get() {
  // We should never touch the platform service in a child process.
  MOZ_ASSERT(XRE_IsParentProcess());
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  if (!IsRunning()) {
    ErrorResult rv;
    // Uncomment once we have an actual platform library to test.
    //
    // bool useTestService = false;
    // rv = Preferences::GetRootBranch()->GetBoolPref("midi.testing",
    // &useTestService);
    gMIDIPlatformService = new TestMIDIPlatformService();
    gMIDIPlatformService->Init();
  }
  return gMIDIPlatformService;
}

void MIDIPlatformService::MaybeStop() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  if (!IsRunning()) {
    // Service already stopped or never started. Exit.
    return;
  }
  // If we have any ports or managers left, we should still be alive.
  if (!mPorts.IsEmpty() || !mManagers.IsEmpty()) {
    return;
  }
  Stop();
  gMIDIPlatformService = nullptr;
}

void MIDIPlatformService::AddManager(MIDIManagerParent* aManager) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  mManagers.AppendElement(aManager);
  // Managers add themselves during construction. We have to wait for the
  // protocol construction to finish before we send them a port list. The
  // runnable calls SendPortList, which iterates through the live manager list,
  // so this saves us from having to worry about Manager pointer validity at
  // time of runnable execution.
  nsCOMPtr<nsIRunnable> r(new SendPortListRunnable());
  NS_DispatchToCurrentThread(r);
}

void MIDIPlatformService::RemoveManager(MIDIManagerParent* aManager) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  mManagers.RemoveElement(aManager);
  MaybeStop();
}

void MIDIPlatformService::UpdateStatus(
    const nsAString& aPortId, const MIDIPortDeviceState& aDeviceState,
    const MIDIPortConnectionState& aConnectionState) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  for (auto port : mPorts) {
    if (port->MIDIPortInterface::Id() == aPortId) {
      port->SendUpdateStatus(aDeviceState, aConnectionState);
    }
  }
}

void MIDIPlatformService::GetMessages(const nsAString& aPortId,
                                      nsTArray<MIDIMessage>& aMsgs) {
  // Can run on either background thread or platform specific IO Thread.
  {
    MutexAutoLock lock(mMessageQueueMutex);
    MIDIMessageQueue* msgQueue;
    if (!mMessageQueues.Get(aPortId, &msgQueue)) {
      return;
    }
    msgQueue->GetMessages(aMsgs);
  }
}

void MIDIPlatformService::GetMessagesBefore(const nsAString& aPortId,
                                            const TimeStamp& aTimeStamp,
                                            nsTArray<MIDIMessage>& aMsgs) {
  // Can run on either background thread or platform specific IO Thread.
  {
    MutexAutoLock lock(mMessageQueueMutex);
    MIDIMessageQueue* msgQueue;
    if (!mMessageQueues.Get(aPortId, &msgQueue)) {
      return;
    }
    msgQueue->GetMessagesBefore(aTimeStamp, aMsgs);
  }
}

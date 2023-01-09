/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDIPlatformService_h
#define mozilla_dom_MIDIPlatformService_h

#include "nsClassHashtable.h"
#include "mozilla/Mutex.h"
#include "mozilla/dom/MIDIPortBinding.h"
#include "nsHashKeys.h"

// XXX Avoid including this here by moving function implementations to the cpp
// file.
#include "mozilla/dom/MIDIMessageQueue.h"

namespace mozilla::dom {

class MIDIManagerParent;
class MIDIPortParent;
class MIDIMessage;
class MIDIMessageQueue;
class MIDIPortInfo;

/**
 * Base class for platform specific MIDI implementations. Handles aggregation of
 * IPC service objects, as well as sending/receiving updates about port
 * connection events and messages.
 *
 */
class MIDIPlatformService {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MIDIPlatformService);
  // Adds info about MIDI Port that has been connected.
  void AddPortInfo(MIDIPortInfo& aPortInfo);

  // Removes info of MIDI Port that has been disconnected.
  void RemovePortInfo(MIDIPortInfo& aPortInfo);

  // Adds a newly created manager protocol object to manager array.
  void AddManager(MIDIManagerParent* aManager);

  // Removes a deleted manager protocol object from manager array.
  void RemoveManager(MIDIManagerParent* aManager);

  // Adds a newly created port protocol object to port array.
  void AddPort(MIDIPortParent* aPort);

  // Removes a deleted port protocol object from port array.
  void RemovePort(MIDIPortParent* aPort);

  // Platform specific init function.
  virtual void Init() = 0;

  // Forces the implementation to refresh the port list.
  virtual void Refresh() = 0;

  // Platform specific MIDI port opening function.
  virtual void Open(MIDIPortParent* aPort) = 0;

  // Clears all queued MIDI messages for a port.
  void Clear(MIDIPortParent* aPort);

  // Puts in a request to destroy the singleton MIDIPlatformService object.
  // Object will only be destroyed if there are no more MIDIManager and MIDIPort
  // protocols left to communicate with.
  void MaybeStop();

  // Initializes statics on startup.
  static void InitStatics();

  // Returns the MIDI Task Queue.
  static nsISerialEventTarget* OwnerThread();

  // Asserts that we're on the above task queue.
  static void AssertThread() {
    MOZ_DIAGNOSTIC_ASSERT(OwnerThread()->IsOnCurrentThread());
  }

  // True if service is live.
  static bool IsRunning();

  // Returns a pointer to the MIDIPlatformService object, creating it and
  // starting the platform specific service if it is not currently running.
  static MIDIPlatformService* Get();

  // Sends a list of all currently connected ports in order to populate a new
  // MIDIAccess object.
  void SendPortList();

  // Receives a new set of messages from an MIDI Input Port, and checks their
  // validity.
  void CheckAndReceive(const nsAString& aPortID,
                       const nsTArray<MIDIMessage>& aMsgs);

  // Sends connection/disconnect/open/closed/etc status updates about a MIDI
  // Port to all port listeners.
  void UpdateStatus(MIDIPortParent* aPort,
                    const MIDIPortDeviceState& aDeviceState,
                    const MIDIPortConnectionState& aConnectionState);

  // Adds outgoing messages to the sorted message queue, for sending later.
  void QueueMessages(const nsAString& aId, nsTArray<MIDIMessage>& aMsgs);

  // Clears all messages later than now, sends all outgoing message scheduled
  // before/at now, and schedules MIDI Port connection closing.
  void Close(MIDIPortParent* aPort);

  // Returns whether there are currently any MIDI devices.
  bool HasDevice() { return !mPortInfo.IsEmpty(); }

 protected:
  MIDIPlatformService();
  virtual ~MIDIPlatformService();
  // Platform specific MIDI service shutdown method.
  virtual void Stop() = 0;

  // When device state of a MIDI Port changes, broadcast to all IPC port
  // objects.
  void BroadcastState(const MIDIPortInfo& aPortInfo,
                      const MIDIPortDeviceState& aState);

  // Platform specific MIDI port closing function. Named "Schedule" due to the
  // fact that it needs to happen in the context of the I/O thread for the
  // platform MIDI implementation, and therefore will happen async.
  virtual void ScheduleClose(MIDIPortParent* aPort) = 0;

  // Platform specific MIDI message sending function. Named "Schedule" due to
  // the fact that it needs to happen in the context of the I/O thread for the
  // platform MIDI implementation, and therefore will happen async.
  virtual void ScheduleSend(const nsAString& aPortId) = 0;

  // Allows platform specific IO Threads to retrieve all messages to be sent.
  // Handles mutex locking.
  void GetMessages(const nsAString& aPortId, nsTArray<MIDIMessage>& aMsgs);

  // Allows platform specific IO Threads to retrieve all messages to be sent
  // before a certain timestamp. Handles mutex locking.
  void GetMessagesBefore(const nsAString& aPortId, const TimeStamp& aTimeStamp,
                         nsTArray<MIDIMessage>& aMsgs);

 private:
  // When the MIDIPlatformService is created, we need to know whether or not the
  // corresponding IPC MIDIManager objects have received the MIDIPort list after
  // it is populated. This is set to True when that is done, so we don't
  // constantly spam MIDIManagers with port lists.
  bool mHasSentPortList;

  // Array of MIDIManager IPC objects. This array manages the lifetime of
  // MIDIManager objects in the parent process, and IPC will call
  // RemoveManager() end lifetime when IPC channel is destroyed.
  nsTArray<RefPtr<MIDIManagerParent>> mManagers;

  // Array of information for currently connected Ports
  nsTArray<MIDIPortInfo> mPortInfo;

  // Array of MIDIPort IPC objects. May contain ports not listed in mPortInfo,
  // as we can hold port objects even after they are disconnected.
  //
  // TODO Split this into input and output ports. Will make life easier.
  nsTArray<RefPtr<MIDIPortParent>> mPorts;

  // Per-port message queue hashtable. Handles scheduling messages for sending.
  nsClassHashtable<nsStringHashKey, MIDIMessageQueue> mMessageQueues;

  // Mutex for managing access to message queue objects.
  Mutex mMessageQueueMutex MOZ_UNANNOTATED;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_MIDIPlatformService_h

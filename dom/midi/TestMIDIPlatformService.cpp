/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestMIDIPlatformService.h"
#include "mozilla/dom/MIDIPort.h"
#include "mozilla/dom/MIDITypes.h"
#include "mozilla/dom/MIDIPortInterface.h"
#include "mozilla/dom/MIDIPortParent.h"
#include "mozilla/dom/MIDIPlatformRunnables.h"
#include "mozilla/dom/MIDIUtils.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/Unused.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

/**
 * Runnable used for making sure ProcessMessages only happens on the IO thread.
 *
 */
class ProcessMessagesRunnable : public mozilla::Runnable {
 public:
  explicit ProcessMessagesRunnable(const nsAString& aPortID)
      : Runnable("ProcessMessagesRunnable"), mPortID(aPortID) {}
  ~ProcessMessagesRunnable() = default;
  NS_IMETHOD Run() {
    // If service is no longer running, just exist without processing.
    if (!MIDIPlatformService::IsRunning()) {
      return NS_OK;
    }
    TestMIDIPlatformService* srv =
        static_cast<TestMIDIPlatformService*>(MIDIPlatformService::Get());
    srv->ProcessMessages(mPortID);
    return NS_OK;
  }

 private:
  nsString mPortID;
};

/**
 * Runnable used for allowing IO thread to queue more messages for processing,
 * since it can't access the service object directly.
 *
 */
class QueueMessagesRunnable : public MIDIBackgroundRunnable {
 public:
  QueueMessagesRunnable(const nsAString& aPortID,
                        const nsTArray<MIDIMessage>& aMsgs)
      : MIDIBackgroundRunnable("QueueMessagesRunnable"),
        mPortID(aPortID),
        mMsgs(aMsgs.Clone()) {}
  ~QueueMessagesRunnable() = default;
  virtual void RunInternal() {
    AssertIsOnBackgroundThread();
    MIDIPlatformService::Get()->QueueMessages(mPortID, mMsgs);
  }

 private:
  nsString mPortID;
  nsTArray<MIDIMessage> mMsgs;
};

TestMIDIPlatformService::TestMIDIPlatformService()
    : mBackgroundThread(NS_GetCurrentThread()),
      mControlInputPort(
          NS_LITERAL_STRING("b744eebe-f7d8-499b-872b-958f63c8f522"),
          NS_LITERAL_STRING("Test Control MIDI Device Input Port"),
          NS_LITERAL_STRING("Test Manufacturer"), NS_LITERAL_STRING("1.0.0"),
          static_cast<uint32_t>(MIDIPortType::Input)),
      mControlOutputPort(
          NS_LITERAL_STRING("ab8e7fe8-c4de-436a-a960-30898a7c9a3d"),
          NS_LITERAL_STRING("Test Control MIDI Device Output Port"),
          NS_LITERAL_STRING("Test Manufacturer"), NS_LITERAL_STRING("1.0.0"),
          static_cast<uint32_t>(MIDIPortType::Output)),
      mStateTestInputPort(
          NS_LITERAL_STRING("a9329677-8588-4460-a091-9d4a7f629a48"),
          NS_LITERAL_STRING("Test State MIDI Device Input Port"),
          NS_LITERAL_STRING("Test Manufacturer"), NS_LITERAL_STRING("1.0.0"),
          static_cast<uint32_t>(MIDIPortType::Input)),
      mStateTestOutputPort(
          NS_LITERAL_STRING("478fa225-b5fc-4fa6-a543-d32d9cb651e7"),
          NS_LITERAL_STRING("Test State MIDI Device Output Port"),
          NS_LITERAL_STRING("Test Manufacturer"), NS_LITERAL_STRING("1.0.0"),
          static_cast<uint32_t>(MIDIPortType::Output)),
      mAlwaysClosedTestOutputPort(
          NS_LITERAL_STRING("f87d0c76-3c68-49a9-a44f-700f1125c07a"),
          NS_LITERAL_STRING("Always Closed MIDI Device Output Port"),
          NS_LITERAL_STRING("Test Manufacturer"), NS_LITERAL_STRING("1.0.0"),
          static_cast<uint32_t>(MIDIPortType::Output)),
      mIsInitialized(false) {
  AssertIsOnBackgroundThread();
}

TestMIDIPlatformService::~TestMIDIPlatformService() {
  AssertIsOnBackgroundThread();
}

void TestMIDIPlatformService::Init() {
  AssertIsOnBackgroundThread();

  if (mIsInitialized) {
    return;
  }
  mIsInitialized = true;

  // Treat all of our special ports as always connected. When the service comes
  // up, prepopulate the port list with them.
  MIDIPlatformService::Get()->AddPortInfo(mControlInputPort);
  MIDIPlatformService::Get()->AddPortInfo(mControlOutputPort);
  MIDIPlatformService::Get()->AddPortInfo(mAlwaysClosedTestOutputPort);
  nsCOMPtr<nsIRunnable> r(new SendPortListRunnable());

  // Start the IO Thread.
  NS_DispatchToCurrentThread(r);
}

void TestMIDIPlatformService::Open(MIDIPortParent* aPort) {
  MOZ_ASSERT(aPort);
  MIDIPortConnectionState s = MIDIPortConnectionState::Open;
  if (aPort->MIDIPortInterface::Id() == mAlwaysClosedTestOutputPort.id()) {
    // If it's the always closed testing port, act like it's already opened
    // exclusively elsewhere.
    s = MIDIPortConnectionState::Closed;
  }
  // Connection events are just simulated on the background thread, no need to
  // push to IO thread.
  nsCOMPtr<nsIRunnable> r(new SetStatusRunnable(aPort->MIDIPortInterface::Id(),
                                                aPort->DeviceState(), s));
  NS_DispatchToCurrentThread(r);
}

void TestMIDIPlatformService::ScheduleClose(MIDIPortParent* aPort) {
  MOZ_ASSERT(aPort);
  if (aPort->ConnectionState() == MIDIPortConnectionState::Open) {
    // Connection events are just simulated on the background thread, no need to
    // push to IO thread.
    nsCOMPtr<nsIRunnable> r(new SetStatusRunnable(
        aPort->MIDIPortInterface::Id(), aPort->DeviceState(),
        MIDIPortConnectionState::Closed));
    NS_DispatchToCurrentThread(r);
  }
}

void TestMIDIPlatformService::Stop() { AssertIsOnBackgroundThread(); }

void TestMIDIPlatformService::ScheduleSend(const nsAString& aPortId) {
  nsCOMPtr<nsIRunnable> r(new ProcessMessagesRunnable(aPortId));
  NS_DispatchToCurrentThread(r);
}

void TestMIDIPlatformService::ProcessMessages(const nsAString& aPortId) {
  nsTArray<MIDIMessage> msgs;
  GetMessagesBefore(aPortId, TimeStamp::Now(), msgs);

  for (MIDIMessage msg : msgs) {
    // receiving message from test control port
    if (aPortId == mControlOutputPort.id()) {
      switch (msg.data()[0]) {
        // Hit a note, get a test!
        case 0x90:
          switch (msg.data()[1]) {
            // Echo data/timestamp back through output port
            case 0x00: {
              nsCOMPtr<nsIRunnable> r(
                  new ReceiveRunnable(mControlInputPort.id(), msg));
              mBackgroundThread->Dispatch(r, NS_DISPATCH_NORMAL);
              break;
            }
            // Cause control test ports to connect
            case 0x01: {
              nsCOMPtr<nsIRunnable> r1(
                  new AddPortRunnable(mStateTestInputPort));
              mBackgroundThread->Dispatch(r1, NS_DISPATCH_NORMAL);
              break;
            }
            // Cause control test ports to disconnect
            case 0x02: {
              nsCOMPtr<nsIRunnable> r1(
                  new RemovePortRunnable(mStateTestInputPort));
              mBackgroundThread->Dispatch(r1, NS_DISPATCH_NORMAL);
              break;
            }
            // Test for packet timing
            case 0x03: {
              // Append a few echo command packets in reverse timing order,
              // should come out in correct order on other end.
              nsTArray<MIDIMessage> newMsgs;
              nsTArray<uint8_t> msg;
              msg.AppendElement(0x90);
              msg.AppendElement(0x00);
              msg.AppendElement(0x00);
              // PR_Now() returns nanosecods, and we need a double with
              // fractional milliseconds.
              TimeStamp currentTime = TimeStamp::Now();
              for (int i = 0; i <= 5; ++i) {
                // Insert messages with timestamps in reverse order, to make
                // sure we're sorting correctly.
                newMsgs.AppendElement(MIDIMessage(
                    msg, currentTime - TimeDuration::FromMilliseconds(i * 2)));
              }
              nsCOMPtr<nsIRunnable> r(
                  new QueueMessagesRunnable(aPortId, newMsgs));
              mBackgroundThread->Dispatch(r, NS_DISPATCH_NORMAL);
              break;
            }
            default:
              NS_WARNING("Unknown Test MIDI message received!");
          }
          break;
          // Sysex tests
        case 0xF0:
          switch (msg.data()[1]) {
              // Echo data/timestamp back through output port
            case 0x00: {
              nsCOMPtr<nsIRunnable> r(
                  new ReceiveRunnable(mControlInputPort.id(), msg));
              mBackgroundThread->Dispatch(r, NS_DISPATCH_NORMAL);
              break;
            }
            // Test for system real time messages in the middle of sysex
            // messages.
            case 0x01: {
              nsTArray<uint8_t> msgs;
              const uint8_t msg[] = {0xF0, 0x01, 0xF8, 0x02, 0x03,
                                     0x04, 0xF9, 0x05, 0xF7};
              // Can't use AppendElements on an array here, so just do range
              // based loading.
              for (auto& s : msg) {
                msgs.AppendElement(s);
              }
              nsTArray<MIDIMessage> newMsgs;
              MIDIUtils::ParseMessages(msgs, TimeStamp::Now(), newMsgs);
              nsCOMPtr<nsIRunnable> r(
                  new ReceiveRunnable(mControlInputPort.id(), newMsgs));
              mBackgroundThread->Dispatch(r, NS_DISPATCH_NORMAL);
              break;
            }
            default:
              NS_WARNING("Unknown Test Sysex MIDI message received!");
          }
          break;
      }
    }
  }
}

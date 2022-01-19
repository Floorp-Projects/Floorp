/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "midirMIDIPlatformService.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/MIDIPort.h"
#include "mozilla/dom/MIDITypes.h"
#include "mozilla/dom/MIDIPortInterface.h"
#include "mozilla/dom/MIDIPortParent.h"
#include "mozilla/dom/MIDIPlatformRunnables.h"
#include "mozilla/dom/MIDIUtils.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/Unused.h"
#include "nsIThread.h"
#include "mozilla/Logging.h"
#include "MIDILog.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

static_assert(sizeof(TimeStamp) == sizeof(GeckoTimeStamp));

/**
 * Runnable used for to send messages asynchronously on the I/O thread.
 */
class SendRunnable : public MIDIBackgroundRunnable {
 public:
  explicit SendRunnable(const nsAString& aPortID)
      : MIDIBackgroundRunnable("SendRunnable"), mPortID(aPortID) {}
  ~SendRunnable() = default;
  virtual void RunInternal() {
    AssertIsOnBackgroundThread();
    midirMIDIPlatformService* srv =
        static_cast<midirMIDIPlatformService*>(MIDIPlatformService::Get());
    srv->SendMessages(mPortID);
  }

 private:
  nsString mPortID;
  nsTArray<MIDIMessage> mMsgs;
};

// static
StaticMutex midirMIDIPlatformService::gBackgroundThreadMutex;

// static
nsCOMPtr<nsIThread> midirMIDIPlatformService::gBackgroundThread;

midirMIDIPlatformService::midirMIDIPlatformService()
    : mImplementation(nullptr) {
  StaticMutexAutoLock lock(gBackgroundThreadMutex);
  gBackgroundThread = NS_GetCurrentThread();
}

midirMIDIPlatformService::~midirMIDIPlatformService() {
  LOG("midir_impl_shutdown");
  midir_impl_shutdown(mImplementation);
  StaticMutexAutoLock lock(gBackgroundThreadMutex);
  gBackgroundThread = nullptr;
}

// static
void midirMIDIPlatformService::AddPort(const nsString* aId,
                                       const nsString* aName, bool aInput) {
  MIDIPortType type = aInput ? MIDIPortType::Input : MIDIPortType::Output;
  MIDIPortInfo port(*aId, *aName, u""_ns, u""_ns, static_cast<uint32_t>(type));
  MIDIPlatformService::Get()->AddPortInfo(port);
}

void midirMIDIPlatformService::Init() {
  if (mImplementation) {
    return;
  }

  mImplementation = midir_impl_init(AddPort);

  if (mImplementation) {
    MIDIPlatformService::Get()->SendPortList();
  } else {
    LOG("midir_impl_init failure");
  }
}

// static
void midirMIDIPlatformService::CheckAndReceive(const nsString* aId,
                                               const uint8_t* aData,
                                               size_t aLength,
                                               const GeckoTimeStamp* aTimeStamp,
                                               uint64_t aMicros) {
  nsTArray<uint8_t> data;
  for (size_t i = 0; i < aLength; i++) {
    data.AppendElement(aData[i]);
  }
  const TimeStamp* openTime = reinterpret_cast<const TimeStamp*>(aTimeStamp);
  TimeStamp timestamp =
      *openTime + TimeDuration::FromMicroseconds(static_cast<double>(aMicros));
  MIDIMessage message(data, timestamp);
  nsTArray<MIDIMessage> messages;
  messages.AppendElement(message);

  LogMIDIMessage(messages, *aId, MIDIPortType::Input);

  nsCOMPtr<nsIRunnable> r(new ReceiveRunnable(*aId, messages));
  StaticMutexAutoLock lock(gBackgroundThreadMutex);
  if (gBackgroundThread) {
    gBackgroundThread->Dispatch(r, NS_DISPATCH_NORMAL);
  }
}

void midirMIDIPlatformService::Open(MIDIPortParent* aPort) {
  MOZ_ASSERT(aPort);
  nsString id = aPort->MIDIPortInterface::Id();
  TimeStamp openTimeStamp = TimeStamp::Now();
  if (midir_impl_open_port(mImplementation, &id,
                           reinterpret_cast<GeckoTimeStamp*>(&openTimeStamp),
                           CheckAndReceive)) {
    LOG("MIDI port open: %s at t=%lf", NS_ConvertUTF16toUTF8(id).get(),
        (openTimeStamp - TimeStamp::ProcessCreation()).ToSeconds());
    nsCOMPtr<nsIRunnable> r(new SetStatusRunnable(
        aPort, aPort->DeviceState(), MIDIPortConnectionState::Open));
    NS_DispatchToCurrentThread(r);
  } else {
    LOG("MIDI port open failed: %s", NS_ConvertUTF16toUTF8(id).get());
  }
}

void midirMIDIPlatformService::Stop() {
  // Nothing to do here AFAIK
}

void midirMIDIPlatformService::ScheduleSend(const nsAString& aPortId) {
  LOG("MIDI port schedule open %s", NS_ConvertUTF16toUTF8(aPortId).get());
  nsCOMPtr<nsIRunnable> r(new SendRunnable(aPortId));
  StaticMutexAutoLock lock(gBackgroundThreadMutex);
  if (gBackgroundThread) {
    gBackgroundThread->Dispatch(r, NS_DISPATCH_NORMAL);
  }
}

void midirMIDIPlatformService::ScheduleClose(MIDIPortParent* aPort) {
  MOZ_ASSERT(aPort);
  nsString id = aPort->MIDIPortInterface::Id();
  LOG("MIDI port schedule close %s", NS_ConvertUTF16toUTF8(id).get());
  if (aPort->ConnectionState() == MIDIPortConnectionState::Open) {
    midir_impl_close_port(mImplementation, &id);
    nsCOMPtr<nsIRunnable> r(new SetStatusRunnable(
        aPort, aPort->DeviceState(), MIDIPortConnectionState::Closed));
    NS_DispatchToCurrentThread(r);
  }
}

void midirMIDIPlatformService::SendMessages(const nsAString& aPortId) {
  LOG("MIDI port send message on %s", NS_ConvertUTF16toUTF8(aPortId).get());
  nsTArray<MIDIMessage> messages;
  GetMessages(aPortId, messages);
  LogMIDIMessage(messages, aPortId, MIDIPortType::Output);
  for (const auto& message : messages) {
    midir_impl_send(mImplementation, &aPortId, &message.data());
  }
}

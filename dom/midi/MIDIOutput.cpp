/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MIDIOutput.h"
#include "mozilla/dom/MIDIPortChild.h"
#include "mozilla/dom/MIDITypes.h"
#include "mozilla/dom/MIDIOutputBinding.h"
#include "mozilla/dom/MIDIUtils.h"
#include "nsDOMNavigationTiming.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/Performance.h"

using namespace mozilla;
using namespace mozilla::dom;

MIDIOutput::MIDIOutput(nsPIDOMWindowInner* aWindow, MIDIAccess* aMIDIAccessParent)
  : MIDIPort(aWindow, aMIDIAccessParent)
{

}

//static
MIDIOutput*
MIDIOutput::Create(nsPIDOMWindowInner* aWindow, MIDIAccess* aMIDIAccessParent,
                   const MIDIPortInfo& aPortInfo, const bool aSysexEnabled)
{
  MOZ_ASSERT(static_cast<MIDIPortType>(aPortInfo.type()) == MIDIPortType::Output);
  auto port = new MIDIOutput(aWindow, aMIDIAccessParent);
  if (NS_WARN_IF(!port->Initialize(aPortInfo, aSysexEnabled))) {
    return nullptr;
  }
  return port;
}

JSObject*
MIDIOutput::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MIDIOutput_Binding::Wrap(aCx, this, aGivenProto);
}

void
MIDIOutput::Send(const Sequence<uint8_t>& aData, const Optional<double>& aTimestamp, ErrorResult& aRv)
{
  if (mPort->DeviceState() == MIDIPortDeviceState::Disconnected) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  // The timestamp passed to us is a DOMHighResTimestamp, which is in relation
  // to the start of navigation timing. This needs to be turned into a
  // TimeStamp before it hits the platform specific MIDI service.
  //
  // If timestamp is either not set or zero, set timestamp to now and send the
  // message ASAP.
  TimeStamp timestamp;
  if (aTimestamp.WasPassed() && aTimestamp.Value() != 0) {
    nsCOMPtr<nsIDocument> doc = GetOwner()->GetDoc();
    if (!doc) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }
    TimeDuration ts_diff = TimeDuration::FromMilliseconds(aTimestamp.Value());
    timestamp =
      GetOwner()->GetPerformance()->GetDOMTiming()->GetNavigationStartTimeStamp() + ts_diff;
  } else {
    timestamp = TimeStamp::Now();
  }

  nsTArray<MIDIMessage> msgArray;
  MIDIUtils::ParseMessages(aData, timestamp, msgArray);
  // Our translation of the spec is that invalid messages in a multi-message
  // sequence will be thrown out, but that valid messages will still be used.
  if (msgArray.IsEmpty()) {
    aRv.Throw(NS_ERROR_DOM_TYPE_ERR);
    return;
  }

  // TODO Move this check back to parse message so we don't have to iterate
  // twice.
  if (!SysexEnabled()) {
    for (auto& msg : msgArray) {
      if (MIDIUtils::IsSysexMessage(msg)) {
        aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
        return;
      }
    }
  }
  mPort->SendSend(msgArray);
}

void
MIDIOutput::Clear()
{
  if (mPort->ConnectionState() == MIDIPortConnectionState::Closed) {
    return;
  }
  mPort->SendClear();
}

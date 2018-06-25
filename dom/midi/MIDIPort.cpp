/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MIDIPort.h"
#include "mozilla/dom/MIDIPortChild.h"
#include "mozilla/dom/MIDIAccess.h"
#include "mozilla/dom/MIDITypes.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/MIDITypes.h"
#include "mozilla/Unused.h"
#include "nsISupportsImpl.h" // for MOZ_COUNT_CTOR, MOZ_COUNT_DTOR

using namespace mozilla::ipc;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(MIDIPort,
                                   DOMEventTargetHelper,
                                   mOpeningPromise,
                                   mClosingPromise)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(MIDIPort,
                                               DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MIDIPort)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MIDIPort, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MIDIPort, DOMEventTargetHelper)

MIDIPort::MIDIPort(nsPIDOMWindowInner* aWindow, MIDIAccess* aMIDIAccessParent) :
  DOMEventTargetHelper(aWindow),
  mMIDIAccessParent(aMIDIAccessParent)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aMIDIAccessParent);
}

MIDIPort::~MIDIPort()
{
  if (mMIDIAccessParent) {
    mMIDIAccessParent->RemovePortListener(this);
    mMIDIAccessParent = nullptr;
  }
  if (mPort) {
    // If the IPC port channel is still alive at this point, it means we're
    // probably CC'ing this port object. Send the shutdown message to also clean
    // up the IPC channel.
    mPort->SendShutdown();
    // This will unset the IPC Port pointer. Don't call anything after this.
    mPort->Teardown();
  }
}

bool
MIDIPort::Initialize(const MIDIPortInfo& aPortInfo, bool aSysexEnabled)
{
  RefPtr<MIDIPortChild> port = new MIDIPortChild(aPortInfo, aSysexEnabled, this);
  PBackgroundChild* b = BackgroundChild::GetForCurrentThread();
  MOZ_ASSERT(b, "Should always have a valid BackgroundChild when creating a port object!");
  if (!b->SendPMIDIPortConstructor(port, aPortInfo, aSysexEnabled)) {
    return false;
  }
  mPort = port;
  // Make sure to increase the ref count for the port, so it can be cleaned up
  // by the IPC manager.
  mPort->SetActorAlive();
  return true;
}

void
MIDIPort::UnsetIPCPort()
{
  mPort = nullptr;
}

JSObject*
MIDIPort::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MIDIPort_Binding::Wrap(aCx, this, aGivenProto);
}

void
MIDIPort::GetId(nsString& aRetVal) const
{
  MOZ_ASSERT(mPort);
  aRetVal = mPort->MIDIPortInterface::Id();
}

void
MIDIPort::GetManufacturer(nsString& aRetVal) const
{
  MOZ_ASSERT(mPort);
  aRetVal = mPort->Manufacturer();
}

void
MIDIPort::GetName(nsString& aRetVal) const
{
  MOZ_ASSERT(mPort);
  aRetVal = mPort->Name();
}

void
MIDIPort::GetVersion(nsString& aRetVal) const
{
  MOZ_ASSERT(mPort);
  aRetVal = mPort->Version();
}

MIDIPortType
MIDIPort::Type() const
{
  MOZ_ASSERT(mPort);
  return mPort->Type();
}

MIDIPortConnectionState
MIDIPort::Connection() const
{
  MOZ_ASSERT(mPort);
  return mPort->ConnectionState();
}

MIDIPortDeviceState
MIDIPort::State() const
{
  MOZ_ASSERT(mPort);
  return mPort->DeviceState();
}

bool
MIDIPort::SysexEnabled() const
{
  MOZ_ASSERT(mPort);
  return mPort->SysexEnabled();
}

already_AddRefed<Promise>
MIDIPort::Open()
{
  MOZ_ASSERT(mPort);
  RefPtr<Promise> p;
  if (mOpeningPromise) {
    p = mOpeningPromise;
    return p.forget();
  }
  ErrorResult rv;
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(GetOwner());
  p = Promise::Create(go, rv);
  if (rv.Failed()) {
    return nullptr;
  }
  mOpeningPromise = p;
  mPort->SendOpen();
  return p.forget();
}

already_AddRefed<Promise>
MIDIPort::Close()
{
  MOZ_ASSERT(mPort);
  RefPtr<Promise> p;
  if (mClosingPromise) {
    p = mClosingPromise;
    return p.forget();
  }
  ErrorResult rv;
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(GetOwner());
  p = Promise::Create(go, rv);
  if (rv.Failed()) {
    return nullptr;
  }
  mClosingPromise = p;
  mPort->SendClose();
  return p.forget();
}

void
MIDIPort::Notify(const void_t& aVoid)
{
  // If we're getting notified, it means the MIDIAccess parent object is dead. Nullify our copy.
  mMIDIAccessParent = nullptr;
}

void
MIDIPort::FireStateChangeEvent()
{
  MOZ_ASSERT(mPort);
  if (mPort->ConnectionState() == MIDIPortConnectionState::Open ||
      mPort->ConnectionState() == MIDIPortConnectionState::Pending) {
    if (mOpeningPromise) {
      mOpeningPromise->MaybeResolve(this);
      mOpeningPromise = nullptr;
    }
  } else if (mPort->ConnectionState() == MIDIPortConnectionState::Closed) {
    if (mOpeningPromise) {
      mOpeningPromise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
      mOpeningPromise = nullptr;
    }
    if (mClosingPromise) {
      mClosingPromise->MaybeResolve(this);
      mClosingPromise = nullptr;
    }
  }
  if (mPort->DeviceState() == MIDIPortDeviceState::Connected &&
      mPort->ConnectionState() == MIDIPortConnectionState::Pending) {
    mPort->SendOpen();
  }
  // Fire MIDIAccess events first so that the port is no longer in the port maps.
  if (mMIDIAccessParent) {
    mMIDIAccessParent->FireConnectionEvent(this);
  }

  MIDIConnectionEventInit init;
  init.mPort = this;
  RefPtr<MIDIConnectionEvent> event(
    MIDIConnectionEvent::Constructor(this, NS_LITERAL_STRING("statechange"), init));
  DispatchTrustedEvent(event);
}

void
MIDIPort::Receive(const nsTArray<MIDIMessage>& aMsg)
{
  MOZ_CRASH("We should never get here!");
}

}
}

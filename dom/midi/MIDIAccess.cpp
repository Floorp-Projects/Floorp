/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MIDIAccess.h"
#include "mozilla/dom/MIDIAccessManager.h"
#include "mozilla/dom/MIDIPort.h"
#include "mozilla/dom/MIDIAccessBinding.h"
#include "mozilla/dom/MIDIConnectionEvent.h"
#include "mozilla/dom/MIDIOptionsBinding.h"
#include "mozilla/dom/MIDIOutputMapBinding.h"
#include "mozilla/dom/MIDIInputMapBinding.h"
#include "mozilla/dom/MIDIOutputMap.h"
#include "mozilla/dom/MIDIInputMap.h"
#include "mozilla/dom/MIDIOutput.h"
#include "mozilla/dom/MIDIInput.h"
#include "mozilla/dom/MIDITypes.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PContent.h"
#include "nsIRunnable.h"
#include "nsIDocument.h"
#include "nsPIDOMWindow.h"
#include "nsContentPermissionHelper.h"
#include "nsISupportsImpl.h" // for MOZ_COUNT_CTOR, MOZ_COUNT_DTOR
#include "IPCMessageUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(MIDIAccess)
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(MIDIAccess,
                                               DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MIDIAccess,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInputMap)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOutputMap)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAccessPromise)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MIDIAccess,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInputMap)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOutputMap)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAccessPromise)
  tmp->Shutdown();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MIDIAccess)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MIDIAccess, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MIDIAccess, DOMEventTargetHelper)

MIDIAccess::MIDIAccess(nsPIDOMWindowInner* aWindow, bool aSysexEnabled,
                       Promise* aPromise) :
  DOMEventTargetHelper(aWindow),
  mInputMap(new MIDIInputMap(aWindow)),
  mOutputMap(new MIDIOutputMap(aWindow)),
  mSysexEnabled(aSysexEnabled),
  mAccessPromise(aPromise),
  mHasShutdown(false)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aPromise);
}

MIDIAccess::~MIDIAccess()
{
  Shutdown();
}

void
MIDIAccess::Shutdown()
{
  if (mHasShutdown) {
    return;
  }
  mDestructionObservers.Broadcast(void_t());
  if (MIDIAccessManager::IsRunning()) {
    MIDIAccessManager::Get()->RemoveObserver(this);
  }
  mHasShutdown = true;
}

void
MIDIAccess::FireConnectionEvent(MIDIPort* aPort)
{
  MOZ_ASSERT(aPort);
  MIDIConnectionEventInit init;
  init.mPort = aPort;
  nsAutoString id;
  aPort->GetId(id);
  ErrorResult rv;
  if (aPort->State() == MIDIPortDeviceState::Disconnected) {
    if (aPort->Type() == MIDIPortType::Input &&
        MIDIInputMap_Binding::MaplikeHelpers::Has(mInputMap, id, rv)) {
      MIDIInputMap_Binding::MaplikeHelpers::Delete(mInputMap, id, rv);
    } else if (aPort->Type() == MIDIPortType::Output &&
               MIDIOutputMap_Binding::MaplikeHelpers::Has(mOutputMap, id, rv)) {
      MIDIOutputMap_Binding::MaplikeHelpers::Delete(mOutputMap, id, rv);
    }
    // Check to make sure Has()/Delete() calls haven't failed.
    if(NS_WARN_IF(rv.Failed())) {
      return;
    }
  } else {
    // If we receive an event from a port that is not in one of our port maps,
    // this means a port that was disconnected has been reconnected, with the port
    // owner holding the object during that time, and we should add that port
    // object to our maps again.
    if (aPort->Type() == MIDIPortType::Input &&
        !MIDIInputMap_Binding::MaplikeHelpers::Has(mInputMap, id, rv)) {
      if(NS_WARN_IF(rv.Failed())) {
        return;
      }
      MIDIInputMap_Binding::MaplikeHelpers::Set(mInputMap,
                                               id,
                                               *(static_cast<MIDIInput*>(aPort)),
                                               rv);
      if(NS_WARN_IF(rv.Failed())) {
        return;
      }
    } else if (aPort->Type() == MIDIPortType::Output &&
               !MIDIOutputMap_Binding::MaplikeHelpers::Has(mOutputMap, id, rv)) {
      if(NS_WARN_IF(rv.Failed())) {
        return;
      }
      MIDIOutputMap_Binding::MaplikeHelpers::Set(mOutputMap,
                                                id,
                                                *(static_cast<MIDIOutput*>(aPort)),
                                                rv);
      if(NS_WARN_IF(rv.Failed())) {
        return;
      }
    }
  }
  RefPtr<MIDIConnectionEvent> event =
    MIDIConnectionEvent::Constructor(this, NS_LITERAL_STRING("statechange"), init);
  DispatchTrustedEvent(event);
}

void
MIDIAccess::MaybeCreateMIDIPort(const MIDIPortInfo& aInfo, ErrorResult& aRv)
{
  nsAutoString id(aInfo.id());
  MIDIPortType type = static_cast<MIDIPortType>(aInfo.type());
  RefPtr<MIDIPort> port;
  if (type == MIDIPortType::Input) {
    bool hasPort = MIDIInputMap_Binding::MaplikeHelpers::Has(mInputMap, id, aRv);
    if (hasPort || NS_WARN_IF(aRv.Failed())) {
      // We already have the port in our map.
      return;
    }
    port = MIDIInput::Create(GetOwner(), this, aInfo, mSysexEnabled);
    if (NS_WARN_IF(!port)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    MIDIInputMap_Binding::MaplikeHelpers::Set(mInputMap,
                                             id,
                                             *(static_cast<MIDIInput*>(port.get())),
                                             aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  } else if (type == MIDIPortType::Output) {
    bool hasPort = MIDIOutputMap_Binding::MaplikeHelpers::Has(mOutputMap, id, aRv);
    if (hasPort || NS_WARN_IF(aRv.Failed())) {
      // We already have the port in our map.
      return;
    }
    port = MIDIOutput::Create(GetOwner(), this, aInfo, mSysexEnabled);
    if (NS_WARN_IF(!port)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    MIDIOutputMap_Binding::MaplikeHelpers::Set(mOutputMap,
                                              id,
                                              *(static_cast<MIDIOutput*>(port.get())),
                                              aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  } else {
    // If we hit this, then we have some port that is neither input nor output.
    // That is bad.
    MOZ_CRASH("We shouldn't be here!");
  }
  // Set up port to listen for destruction of this access object.
  mDestructionObservers.AddObserver(port);

  // If we haven't resolved the promise for handing the MIDIAccess object to
  // content, this means we're still populating the list of already connected
  // devices. Don't fire events yet.
  if (!mAccessPromise) {
    FireConnectionEvent(port);
  }
}

// For the MIDIAccess object, only worry about new connections, where we create
// MIDIPort objects. When a port is removed and the MIDIPortRemove event is
// received, that will be handled by the MIDIPort object itself, and it will
// request removal from MIDIAccess's maps.
void
MIDIAccess::Notify(const MIDIPortList& aEvent)
{
  ErrorResult rv;
  for (auto& port : aEvent.ports()) {
    // Something went very wrong. Warn and return.
    MaybeCreateMIDIPort(port, rv);
    if (rv.Failed()) {
      if (!mAccessPromise) {
        return;
      }
      mAccessPromise->MaybeReject(rv);
      mAccessPromise = nullptr;
    }
  }
  if (!mAccessPromise) {
    return;
  }
  mAccessPromise->MaybeResolve(this);
  mAccessPromise = nullptr;
}

JSObject*
MIDIAccess::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MIDIAccess_Binding::Wrap(aCx, this, aGivenProto);
}

void
MIDIAccess::RemovePortListener(MIDIAccessDestructionObserver* aObs)
{
  mDestructionObservers.RemoveObserver(aObs);
}

} // namespace dom
} // namespace mozilla

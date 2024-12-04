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
#include "mozilla/dom/Document.h"
#include "nsPIDOMWindow.h"
#include "nsContentPermissionHelper.h"
#include "nsISupportsImpl.h"  // for MOZ_COUNT_CTOR, MOZ_COUNT_DTOR
#include "ipc/IPCMessageUtils.h"
#include "MIDILog.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(MIDIAccess)
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(MIDIAccess, DOMEventTargetHelper)
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_PTR
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MIDIAccess)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MIDIAccess, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MIDIAccess, DOMEventTargetHelper)

MIDIAccess::MIDIAccess(nsPIDOMWindowInner* aWindow, bool aSysexEnabled,
                       Promise* aAccessPromise)
    : DOMEventTargetHelper(aWindow),
      mInputMap(new MIDIInputMap(aWindow)),
      mOutputMap(new MIDIOutputMap(aWindow)),
      mSysexEnabled(aSysexEnabled),
      mAccessPromise(aAccessPromise),
      mHasShutdown(false) {
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aAccessPromise);
  KeepAliveIfHasListenersFor(nsGkAtoms::onstatechange);
}

MIDIAccess::~MIDIAccess() { Shutdown(); }

void MIDIAccess::Shutdown() {
  LOG("MIDIAccess::Shutdown");
  if (mHasShutdown) {
    return;
  }
  if (MIDIAccessManager::IsRunning()) {
    MIDIAccessManager::Get()->RemoveObserver(this);
  }
  mHasShutdown = true;
}

void MIDIAccess::FireConnectionEvent(MIDIPort* aPort) {
  MOZ_ASSERT(aPort);
  MIDIConnectionEventInit init;
  init.mPort = aPort;
  nsAutoString id;
  aPort->GetId(id);
  ErrorResult rv;
  if (aPort->State() == MIDIPortDeviceState::Disconnected) {
    if (aPort->Type() == MIDIPortType::Input && mInputMap->Has(id)) {
      MIDIInputMap_Binding::MaplikeHelpers::Delete(mInputMap, aPort->StableId(),
                                                   rv);
      mInputMap->Remove(id);
    } else if (aPort->Type() == MIDIPortType::Output && mOutputMap->Has(id)) {
      MIDIOutputMap_Binding::MaplikeHelpers::Delete(mOutputMap,
                                                    aPort->StableId(), rv);
      mOutputMap->Remove(id);
    }
    // Check to make sure Has()/Delete() calls haven't failed.
    if (NS_WARN_IF(rv.Failed())) {
      LOG("Inconsistency during FireConnectionEvent");
      return;
    }
  } else {
    // If we receive an event from a port that is not in one of our port maps,
    // this means a port that was disconnected has been reconnected, with the
    // port owner holding the object during that time, and we should add that
    // port object to our maps again.
    if (aPort->Type() == MIDIPortType::Input && !mInputMap->Has(id)) {
      if (NS_WARN_IF(rv.Failed())) {
        LOG("Input port not found");
        return;
      }
      MIDIInputMap_Binding::MaplikeHelpers::Set(
          mInputMap, aPort->StableId(), *(static_cast<MIDIInput*>(aPort)), rv);
      if (NS_WARN_IF(rv.Failed())) {
        LOG("Map Set failed for input port");
        return;
      }
      mInputMap->Insert(id, aPort);
    } else if (aPort->Type() == MIDIPortType::Output && mOutputMap->Has(id)) {
      if (NS_WARN_IF(rv.Failed())) {
        LOG("Output port not found");
        return;
      }
      MIDIOutputMap_Binding::MaplikeHelpers::Set(
          mOutputMap, aPort->StableId(), *(static_cast<MIDIOutput*>(aPort)),
          rv);
      if (NS_WARN_IF(rv.Failed())) {
        LOG("Map set failed for output port");
        return;
      }
      mOutputMap->Insert(id, aPort);
    }
  }
  RefPtr<MIDIConnectionEvent> event =
      MIDIConnectionEvent::Constructor(this, u"statechange"_ns, init);
  DispatchTrustedEvent(event);
}

void MIDIAccess::MaybeCreateMIDIPort(const MIDIPortInfo& aInfo,
                                     ErrorResult& aRv) {
  nsAutoString id(aInfo.id());
  MIDIPortType type = static_cast<MIDIPortType>(aInfo.type());
  RefPtr<MIDIPort> port;
  if (type == MIDIPortType::Input) {
    if (mInputMap->Has(id) || NS_WARN_IF(aRv.Failed())) {
      // We already have the port in our map.
      return;
    }
    port = MIDIInput::Create(GetOwner(), this, aInfo, mSysexEnabled);
    if (NS_WARN_IF(!port)) {
      LOG("Couldn't create input port");
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    MIDIInputMap_Binding::MaplikeHelpers::Set(
        mInputMap, port->StableId(), *(static_cast<MIDIInput*>(port.get())),
        aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      LOG("Coudld't set input port in map");
      return;
    }
    mInputMap->Insert(id, port);
  } else if (type == MIDIPortType::Output) {
    if (mOutputMap->Has(id) || NS_WARN_IF(aRv.Failed())) {
      // We already have the port in our map.
      return;
    }
    port = MIDIOutput::Create(GetOwner(), this, aInfo, mSysexEnabled);
    if (NS_WARN_IF(!port)) {
      LOG("Couldn't create output port");
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    MIDIOutputMap_Binding::MaplikeHelpers::Set(
        mOutputMap, port->StableId(), *(static_cast<MIDIOutput*>(port.get())),
        aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      LOG("Coudld't set output port in map");
      return;
    }
    mOutputMap->Insert(id, port);
  } else {
    // If we hit this, then we have some port that is neither input nor output.
    // That is bad.
    MOZ_CRASH("We shouldn't be here!");
  }

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
void MIDIAccess::Notify(const MIDIPortList& aEvent) {
  LOG("MIDIAcess::Notify");
  if (!GetOwner()) {
    // Do nothing if we've already been disconnected from the document.
    return;
  }

  for (const auto& port : aEvent.ports()) {
    // Something went very wrong. Warn and return.
    ErrorResult rv;
    MaybeCreateMIDIPort(port, rv);
    if (rv.Failed()) {
      if (!mAccessPromise) {
        // We can't reject the promise so let's suppress the error instead
        rv.SuppressException();
        return;
      }
      mAccessPromise->MaybeReject(std::move(rv));
      mAccessPromise = nullptr;
    }
  }
  if (!mAccessPromise) {
    return;
  }
  mAccessPromise->MaybeResolve(this);
  mAccessPromise = nullptr;
}

JSObject* MIDIAccess::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return MIDIAccess_Binding::Wrap(aCx, this, aGivenProto);
}

void MIDIAccess::DisconnectFromOwner() {
  IgnoreKeepAliveIfHasListenersFor(nsGkAtoms::onstatechange);

  DOMEventTargetHelper::DisconnectFromOwner();
  MIDIAccessManager::Get()->SendRefresh();
}

}  // namespace mozilla::dom

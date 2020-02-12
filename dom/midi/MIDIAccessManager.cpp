/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MIDIAccessManager.h"
#include "mozilla/dom/MIDIAccess.h"
#include "mozilla/dom/MIDIManagerChild.h"
#include "mozilla/dom/MIDIPermissionRequest.h"
#include "mozilla/dom/FeaturePolicyUtils.h"
#include "mozilla/dom/Promise.h"
#include "nsIGlobalObject.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/Preferences.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace dom {

namespace {
// Singleton object for MIDIAccessManager
StaticRefPtr<MIDIAccessManager> gMIDIAccessManager;
}  // namespace

MIDIAccessManager::MIDIAccessManager() : mHasPortList(false), mChild(nullptr) {}

MIDIAccessManager::~MIDIAccessManager() {}

// static
MIDIAccessManager* MIDIAccessManager::Get() {
  if (!gMIDIAccessManager) {
    gMIDIAccessManager = new MIDIAccessManager();
    ClearOnShutdown(&gMIDIAccessManager);
  }
  return gMIDIAccessManager;
}

// static
bool MIDIAccessManager::IsRunning() { return !!gMIDIAccessManager; }

already_AddRefed<Promise> MIDIAccessManager::RequestMIDIAccess(
    nsPIDOMWindowInner* aWindow, const MIDIOptions& aOptions,
    ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(aWindow);
  RefPtr<Promise> p = Promise::Create(go, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  nsCOMPtr<Document> doc = aWindow->GetDoc();
  if (NS_WARN_IF(!doc)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (!FeaturePolicyUtils::IsFeatureAllowed(doc, NS_LITERAL_STRING("midi"))) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIRunnable> permRunnable =
      new MIDIPermissionRequest(aWindow, p, aOptions);
  aRv = NS_DispatchToMainThread(permRunnable);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  return p.forget();
}

bool MIDIAccessManager::AddObserver(Observer<MIDIPortList>* aObserver) {
  // Add observer before we start the service, otherwise we can end up with
  // device lists being received before we have observers to send them to.
  mChangeObservers.AddObserver(aObserver);
  // If we don't currently have a port list, that means this is a new
  // AccessManager and we possibly need to start the MIDI Service.
  if (!mChild) {
    // Otherwise we must begin the PBackground initialization process and
    // wait for the async ActorCreated() callback.
    MOZ_ASSERT(NS_IsMainThread());
    ::mozilla::ipc::PBackgroundChild* actor =
        BackgroundChild::GetOrCreateForCurrentThread();
    if (NS_WARN_IF(!actor)) {
      return false;
    }
    RefPtr<MIDIManagerChild> mgr(new MIDIManagerChild());
    PMIDIManagerChild* constructedMgr = actor->SendPMIDIManagerConstructor(mgr);

    if (NS_WARN_IF(!constructedMgr)) {
      return false;
    }
    MOZ_ASSERT(constructedMgr == mgr);
    mChild = mgr.forget();
    // Add a ref to mChild here, that will be deref'd by
    // BackgroundChildImpl::DeallocPMIDIManagerChild on IPC cleanup.
    mChild->SetActorAlive();
  }
  return true;
}

void MIDIAccessManager::RemoveObserver(Observer<MIDIPortList>* aObserver) {
  mChangeObservers.RemoveObserver(aObserver);
  if (mChangeObservers.Length() == 0) {
    // If we're out of listeners, go ahead and shut down. Make sure to cleanup
    // the IPDL protocol also.
    if (mChild) {
      mChild->Shutdown();
      mChild = nullptr;
    }
    gMIDIAccessManager = nullptr;
  }
}

void MIDIAccessManager::CreateMIDIAccess(nsPIDOMWindowInner* aWindow,
                                         bool aNeedsSysex, Promise* aPromise) {
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aPromise);
  RefPtr<MIDIAccess> a(new MIDIAccess(aWindow, aNeedsSysex, aPromise));
  if (NS_WARN_IF(!AddObserver(a))) {
    aPromise->MaybeReject(NS_ERROR_FAILURE);
    return;
  }
  if (!mHasPortList) {
    // Hold the access object until we get a connected device list.
    mAccessHolder.AppendElement(a);
  } else {
    // If we already have a port list, just send it to the MIDIAccess object now
    // so it can prepopulate its device list and resolve the promise.
    a->Notify(mPortList);
  }
}

void MIDIAccessManager::Update(const MIDIPortList& aPortList) {
  mPortList = aPortList;
  mChangeObservers.Broadcast(aPortList);
  if (!mHasPortList) {
    mHasPortList = true;
    // Now that we've broadcast the already-connected port list, content
    // should manage the lifetime of the MIDIAccess object, so we can clear the
    // keep-alive array.
    mAccessHolder.Clear();
  }
}

}  // namespace dom
}  // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PresentationConnectionList.h"

#include "nsContentUtils.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/PresentationConnectionAvailableEvent.h"
#include "mozilla/dom/PresentationConnectionListBinding.h"
#include "mozilla/dom/Promise.h"
#include "PresentationConnection.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(PresentationConnectionList,
                                   DOMEventTargetHelper,
                                   mGetConnectionListPromise, mConnections)

NS_IMPL_ADDREF_INHERITED(PresentationConnectionList, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(PresentationConnectionList, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PresentationConnectionList)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

PresentationConnectionList::PresentationConnectionList(
    nsPIDOMWindowInner* aWindow, Promise* aPromise)
    : DOMEventTargetHelper(aWindow), mGetConnectionListPromise(aPromise) {
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aPromise);
}

/* virtual */
JSObject* PresentationConnectionList::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return PresentationConnectionList_Binding::Wrap(aCx, this, aGivenProto);
}

void PresentationConnectionList::GetConnections(
    nsTArray<RefPtr<PresentationConnection>>& aConnections) const {
  if (nsContentUtils::ShouldResistFingerprinting()) {
    aConnections.Clear();
    return;
  }

  aConnections = mConnections.Clone();
}

nsresult PresentationConnectionList::DispatchConnectionAvailableEvent(
    PresentationConnection* aConnection) {
  if (nsContentUtils::ShouldResistFingerprinting()) {
    return NS_OK;
  }

  PresentationConnectionAvailableEventInit init;
  init.mConnection = aConnection;

  RefPtr<PresentationConnectionAvailableEvent> event =
      PresentationConnectionAvailableEvent::Constructor(
          this, NS_LITERAL_STRING("connectionavailable"), init);

  if (NS_WARN_IF(!event)) {
    return NS_ERROR_FAILURE;
  }
  event->SetTrusted(true);

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(this, event);
  return asyncDispatcher->PostDOMEvent();
}

PresentationConnectionList::ConnectionArrayIndex
PresentationConnectionList::FindConnectionById(const nsAString& aId) {
  for (ConnectionArrayIndex i = 0; i < mConnections.Length(); i++) {
    nsAutoString id;
    mConnections[i]->GetId(id);
    if (id == aId) {
      return i;
    }
  }

  return ConnectionArray::NoIndex;
}

void PresentationConnectionList::NotifyStateChange(
    const nsAString& aSessionId, PresentationConnection* aConnection) {
  if (!aConnection) {
    MOZ_ASSERT(false, "PresentationConnection can not be null.");
    return;
  }

  bool connectionFound =
      FindConnectionById(aSessionId) != ConnectionArray::NoIndex;

  PresentationConnectionList_Binding::ClearCachedConnectionsValue(this);
  switch (aConnection->State()) {
    case PresentationConnectionState::Connected:
      if (!connectionFound) {
        mConnections.AppendElement(aConnection);
        if (mGetConnectionListPromise) {
          if (!nsContentUtils::ShouldResistFingerprinting()) {
            mGetConnectionListPromise->MaybeResolve(this);
          }
          mGetConnectionListPromise = nullptr;
          return;
        }
      }
      DispatchConnectionAvailableEvent(aConnection);
      break;
    case PresentationConnectionState::Terminated:
      if (connectionFound) {
        mConnections.RemoveElement(aConnection);
      }
      break;
    default:
      break;
  }
}

}  // namespace dom
}  // namespace mozilla

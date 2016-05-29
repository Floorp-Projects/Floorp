/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PresentationConnectionList.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/PresentationConnectionAvailableEvent.h"
#include "mozilla/dom/PresentationConnectionListBinding.h"
#include "mozilla/dom/Promise.h"
#include "PresentationConnection.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(PresentationConnectionList, DOMEventTargetHelper,
                                   mGetConnectionListPromise,
                                   mConnections)

NS_IMPL_ADDREF_INHERITED(PresentationConnectionList, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(PresentationConnectionList, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PresentationConnectionList)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

PresentationConnectionList::PresentationConnectionList(nsPIDOMWindowInner* aWindow,
                                                       Promise* aPromise)
  : DOMEventTargetHelper(aWindow)
  , mGetConnectionListPromise(aPromise)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aPromise);
}

/* virtual */ JSObject*
PresentationConnectionList::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto)
{
  return PresentationConnectionListBinding::Wrap(aCx, this, aGivenProto);
}

void
PresentationConnectionList::GetConnections(
  nsTArray<RefPtr<PresentationConnection>>& aConnections) const
{
  aConnections = mConnections;
}

nsresult
PresentationConnectionList::DispatchConnectionAvailableEvent(
  PresentationConnection* aConnection)
{
  PresentationConnectionAvailableEventInit init;
  init.mConnection = aConnection;

  RefPtr<PresentationConnectionAvailableEvent> event =
    PresentationConnectionAvailableEvent::Constructor(
      this,
      NS_LITERAL_STRING("connectionavailable"),
      init);

  if (NS_WARN_IF(!event)) {
    return NS_ERROR_FAILURE;
  }
  event->SetTrusted(true);

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, event);
  return asyncDispatcher->PostDOMEvent();
}

PresentationConnectionList::ConnectionArrayIndex
PresentationConnectionList::FindConnectionById(
  const nsAString& aId)
{
  for (ConnectionArrayIndex i = 0; i < mConnections.Length(); i++) {
    nsAutoString id;
    mConnections[i]->GetId(id);
    if (id == nsAutoString(aId)) {
      return i;
    }
  }

  return mConnections.NoIndex;
}

void
PresentationConnectionList::NotifyStateChange(const nsAString& aSessionId,
                                              PresentationConnection* aConnection)
{
  if (!aConnection) {
    MOZ_ASSERT(false, "PresentationConnection can not be null.");
    return;
  }

  bool connectionFound =
    FindConnectionById(aSessionId) != mConnections.NoIndex ? true : false;

  PresentationConnectionListBinding::ClearCachedConnectionsValue(this);
  switch (aConnection->State()) {
    case PresentationConnectionState::Connected:
      if (!connectionFound) {
        mConnections.AppendElement(aConnection);
        if (mGetConnectionListPromise) {
          mGetConnectionListPromise->MaybeResolve(this);
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

} // namespace dom
} // namespace mozilla

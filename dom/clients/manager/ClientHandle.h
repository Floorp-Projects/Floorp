/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientHandle_h
#define _mozilla_dom_ClientHandle_h

#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/ClientOpPromise.h"
#include "mozilla/dom/ClientThing.h"
#include "mozilla/MozPromise.h"

#ifdef XP_WIN
#  undef PostMessage
#endif

namespace mozilla {

namespace dom {

class ClientManager;
class ClientHandleChild;
class ClientOpConstructorArgs;
class PClientManagerChild;
class ServiceWorkerDescriptor;
enum class CallerType : uint32_t;

namespace ipc {
class StructuredCloneData;
}

// The ClientHandle allows code to take a simple ClientInfo struct and
// convert it into a live actor-backed object attached to a particular
// ClientSource somewhere in the browser.  If the ClientSource is
// destroyed then the ClientHandle will simply begin to reject operations.
// We do not currently provide a way to be notified when the ClientSource
// is destroyed, but this could be added in the future.
class ClientHandle final : public ClientThing<ClientHandleChild> {
  friend class ClientManager;
  friend class ClientHandleChild;

  RefPtr<ClientManager> mManager;
  nsCOMPtr<nsISerialEventTarget> mSerialEventTarget;
  RefPtr<GenericPromise::Private> mDetachPromise;
  ClientInfo mClientInfo;

  ~ClientHandle();

  void Shutdown();

  void StartOp(const ClientOpConstructorArgs& aArgs,
               const ClientOpCallback&& aResolveCallback,
               const ClientOpCallback&& aRejectCallback);

  // ClientThing interface
  void OnShutdownThing() override;

  // Private methods called by ClientHandleChild
  void ExecutionReady(const ClientInfo& aClientInfo);

  // Private methods called by ClientManager
  ClientHandle(ClientManager* aManager,
               nsISerialEventTarget* aSerialEventTarget,
               const ClientInfo& aClientInfo);

  void Activate(PClientManagerChild* aActor);

 public:
  const ClientInfo& Info() const;

  // Mark the ClientSource attached to this handle as controlled by the
  // given service worker.  The promise will resolve true if the ClientSource
  // is successfully marked or reject if the operation could not be completed.
  RefPtr<GenericErrorResultPromise> Control(
      const ServiceWorkerDescriptor& aServiceWorker);

  // Focus the Client if possible.  If successful the promise will resolve with
  // a new ClientState snapshot after focus has completed.  If focusing fails
  // for any reason then the promise will reject.
  RefPtr<ClientStatePromise> Focus(CallerType aCallerType);

  // Send a postMessage() call to the target Client.  Currently this only
  // supports sending from a ServiceWorker source and the MessageEvent is
  // dispatched to the Client's navigator.serviceWorker event target.  The
  // returned promise will resolve if the MessageEvent is dispatched or if
  // it triggers an error handled in the Client's context.  Other errors
  // will result in the promise rejecting.
  RefPtr<GenericErrorResultPromise> PostMessage(
      ipc::StructuredCloneData& aData, const ServiceWorkerDescriptor& aSource);

  // Return a Promise that resolves when the ClientHandle object is detached
  // from its remote actors.  This will happen if the ClientSource is destroyed
  // and triggers the cleanup of the handle actors.  It will also naturally
  // happen when the ClientHandle is de-referenced and tears down its own
  // actors.
  //
  // Note: This method can only be called on the ClientHandle owning thread,
  //       but the MozPromise lets you Then() to another thread.
  RefPtr<GenericPromise> OnDetach();

  NS_INLINE_DECL_REFCOUNTING(ClientHandle);
};

}  // namespace dom
}  // namespace mozilla

#endif  // _mozilla_dom_ClientHandle_h

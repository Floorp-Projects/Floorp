/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientSource_h
#define _mozilla_dom_ClientSource_h

#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/ClientOpPromise.h"
#include "mozilla/dom/ClientThing.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/Variant.h"

class nsIDocShell;
class nsISerialEventTarget;
class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class ClientClaimArgs;
class ClientControlledArgs;
class ClientFocusArgs;
class ClientManager;
class ClientSourceChild;
class ClientSourceConstructorArgs;
class ClientSourceExecutionReadyArgs;
class PClientManagerChild;

namespace workers {
class WorkerPrivate;
} // workers namespace

// ClientSource is an RAII style class that is designed to be held via
// a UniquePtr<>.  When created ClientSource will register the existence
// of a client in the cross-process ClientManagerService.  When the
// ClientSource is destroyed then client entry will be removed.  Code
// that represents globals or browsing environments, such as nsGlobalWindow
// or WorkerPrivate, should use ClientManager to create a ClientSource.
class ClientSource final : public ClientThing<ClientSourceChild>
{
  friend class ClientManager;

  NS_DECL_OWNINGTHREAD

  RefPtr<ClientManager> mManager;
  nsCOMPtr<nsISerialEventTarget> mEventTarget;

  Variant<Nothing,
          RefPtr<nsPIDOMWindowInner>,
          nsCOMPtr<nsIDocShell>,
          mozilla::dom::workers::WorkerPrivate*> mOwner;

  ClientInfo mClientInfo;
  Maybe<ServiceWorkerDescriptor> mController;

  void
  Shutdown();

  void
  ExecutionReady(const ClientSourceExecutionReadyArgs& aArgs);

  mozilla::dom::workers::WorkerPrivate*
  GetWorkerPrivate() const;

  nsIDocShell*
  GetDocShell() const;

  void
  MaybeCreateInitialDocument();

  nsresult
  SnapshotWindowState(ClientState* aStateOut);

  // Private methods called by ClientManager
  ClientSource(ClientManager* aManager,
               nsISerialEventTarget* aEventTarget,
               const ClientSourceConstructorArgs& aArgs);

  void
  Activate(PClientManagerChild* aActor);

public:
  ~ClientSource();

  nsPIDOMWindowInner*
  GetInnerWindow() const;

  void
  WorkerExecutionReady(mozilla::dom::workers::WorkerPrivate* aWorkerPrivate);

  nsresult
  WindowExecutionReady(nsPIDOMWindowInner* aInnerWindow);

  nsresult
  DocShellExecutionReady(nsIDocShell* aDocShell);

  void
  Freeze();

  void
  Thaw();

  const ClientInfo&
  Info() const;

  // Trigger a synchronous IPC ping to the parent process to confirm that
  // the ClientSource actor has been created.  This should only be used
  // by the WorkerPrivate startup code to deal with a ClientHandle::Control()
  // call racing on the main thread.  Do not call this in other circumstances!
  void
  WorkerSyncPing(mozilla::dom::workers::WorkerPrivate* aWorkerPrivate);

  // Synchronously mark the ClientSource as controlled by the given service
  // worker.  This can happen as a result of a remote operation or directly
  // by local code.  For example, if a client's initial network load is
  // intercepted by a controlling service worker then this should be called
  // immediately.
  //
  // Note, there is no way to clear the controlling service worker because
  // the specification does not allow that operation.
  void
  SetController(const ServiceWorkerDescriptor& aServiceWorker);

  // Mark the ClientSource as controlled using the remote operation arguments.
  // This will in turn call SetController().
  RefPtr<ClientOpPromise>
  Control(const ClientControlledArgs& aArgs);

  // Get the ClientSource's current controlling service worker, if one has
  // been set.
  const Maybe<ServiceWorkerDescriptor>&
  GetController() const;

  RefPtr<ClientOpPromise>
  Focus(const ClientFocusArgs& aArgs);

  RefPtr<ClientOpPromise>
  Claim(const ClientClaimArgs& aArgs);

  RefPtr<ClientOpPromise>
  GetInfoAndState(const ClientGetInfoAndStateArgs& aArgs);

  nsresult
  SnapshotState(ClientState* aStateOut);

  nsISerialEventTarget*
  EventTarget() const;

  void
  Traverse(nsCycleCollectionTraversalCallback& aCallback,
           const char* aName,
           uint32_t aFlags);
};

inline void
ImplCycleCollectionUnlink(UniquePtr<ClientSource>& aField)
{
  aField.reset();
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            UniquePtr<ClientSource>& aField,
                            const char* aName,
                            uint32_t aFlags)
{
  if (aField) {
    aField->Traverse(aCallback, aName, aFlags);
  }
}

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ClientSource_h

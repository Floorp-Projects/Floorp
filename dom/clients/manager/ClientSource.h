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
#include "mozilla/ResultVariant.h"
#include "mozilla/Variant.h"

#ifdef XP_WIN
#  undef PostMessage
#endif

class nsIContentSecurityPolicy;
class nsIDocShell;
class nsIGlobalObject;
class nsISerialEventTarget;
class nsPIDOMWindowInner;

namespace mozilla {
class ErrorResult;

namespace dom {

class ClientControlledArgs;
class ClientFocusArgs;
class ClientGetInfoAndStateArgs;
class ClientManager;
class ClientPostMessageArgs;
class ClientSourceChild;
class ClientSourceConstructorArgs;
class ClientSourceExecutionReadyArgs;
class ClientState;
class ClientWindowState;
class PClientManagerChild;
class WorkerPrivate;

// ClientSource is an RAII style class that is designed to be held via
// a UniquePtr<>.  When created ClientSource will register the existence
// of a client in the cross-process ClientManagerService.  When the
// ClientSource is destroyed then client entry will be removed.  Code
// that represents globals or browsing environments, such as nsGlobalWindow
// or WorkerPrivate, should use ClientManager to create a ClientSource.
class ClientSource final : public ClientThing<ClientSourceChild> {
  friend class ClientManager;

  NS_DECL_OWNINGTHREAD

  RefPtr<ClientManager> mManager;
  nsCOMPtr<nsISerialEventTarget> mEventTarget;

  Variant<Nothing, RefPtr<nsPIDOMWindowInner>, nsCOMPtr<nsIDocShell>,
          WorkerPrivate*>
      mOwner;

  ClientInfo mClientInfo;
  Maybe<ServiceWorkerDescriptor> mController;
  Maybe<nsCOMPtr<nsIPrincipal>> mPrincipal;

  // Contained a de-duplicated list of ServiceWorker scope strings
  // for which this client has called navigator.serviceWorker.register().
  // Typically there will be either be zero or one scope strings, but
  // there could be more.  We keep this list until the client is closed.
  AutoTArray<nsCString, 1> mRegisteringScopeList;

  void Shutdown();

  void ExecutionReady(const ClientSourceExecutionReadyArgs& aArgs);

  WorkerPrivate* GetWorkerPrivate() const;

  nsIDocShell* GetDocShell() const;

  nsIGlobalObject* GetGlobal() const;

  Result<bool, ErrorResult> MaybeCreateInitialDocument();

  Result<ClientState, ErrorResult> SnapshotWindowState();

  // Private methods called by ClientManager
  ClientSource(ClientManager* aManager, nsISerialEventTarget* aEventTarget,
               const ClientSourceConstructorArgs& aArgs);

  void Activate(PClientManagerChild* aActor);

 public:
  ~ClientSource();

  nsPIDOMWindowInner* GetInnerWindow() const;

  void WorkerExecutionReady(WorkerPrivate* aWorkerPrivate);

  nsresult WindowExecutionReady(nsPIDOMWindowInner* aInnerWindow);

  nsresult DocShellExecutionReady(nsIDocShell* aDocShell);

  void Freeze();

  void Thaw();

  void EvictFromBFCache();

  RefPtr<ClientOpPromise> EvictFromBFCacheOp();

  const ClientInfo& Info() const;

  // Trigger a synchronous IPC ping to the parent process to confirm that
  // the ClientSource actor has been created.  This should only be used
  // by the WorkerPrivate startup code to deal with a ClientHandle::Control()
  // call racing on the main thread.  Do not call this in other circumstances!
  void WorkerSyncPing(WorkerPrivate* aWorkerPrivate);

  // Synchronously mark the ClientSource as controlled by the given service
  // worker.  This can happen as a result of a remote operation or directly
  // by local code.  For example, if a client's initial network load is
  // intercepted by a controlling service worker then this should be called
  // immediately.
  //
  // Note, there is no way to clear the controlling service worker because
  // the specification does not allow that operation.
  void SetController(const ServiceWorkerDescriptor& aServiceWorker);

  // Mark the ClientSource as controlled using the remote operation arguments.
  // This will in turn call SetController().
  RefPtr<ClientOpPromise> Control(const ClientControlledArgs& aArgs);

  // Inherit the controller from a local parent client.  This requires both
  // setting our immediate controller field and also updating the parent-side
  // data structure.
  void InheritController(const ServiceWorkerDescriptor& aServiceWorker);

  // Get the ClientSource's current controlling service worker, if one has
  // been set.
  const Maybe<ServiceWorkerDescriptor>& GetController() const;

  // Note that the client has reached DOMContentLoaded.  Only applies to window
  // clients.
  void NoteDOMContentLoaded();

  // TODO: Convert Focus() to MOZ_CAN_RUN_SCRIPT
  MOZ_CAN_RUN_SCRIPT_BOUNDARY RefPtr<ClientOpPromise> Focus(
      const ClientFocusArgs& aArgs);

  RefPtr<ClientOpPromise> PostMessage(const ClientPostMessageArgs& aArgs);

  RefPtr<ClientOpPromise> GetInfoAndState(
      const ClientGetInfoAndStateArgs& aArgs);

  Result<ClientState, ErrorResult> SnapshotState();

  nsISerialEventTarget* EventTarget() const;

  void SetCsp(nsIContentSecurityPolicy* aCsp);
  void SetPreloadCsp(nsIContentSecurityPolicy* aPreloadCSP);
  void SetCspInfo(const mozilla::ipc::CSPInfo& aCSPInfo);
  const Maybe<mozilla::ipc::CSPInfo>& GetCspInfo();

  void SetAgentClusterId(const nsID& aId) {
    mClientInfo.SetAgentClusterId(aId);
  }

  void Traverse(nsCycleCollectionTraversalCallback& aCallback,
                const char* aName, uint32_t aFlags);

  void NoteCalledRegisterForServiceWorkerScope(const nsACString& aScope);

  bool CalledRegisterForServiceWorkerScope(const nsACString& aScope);

  nsIPrincipal* GetPrincipal();
};

inline void ImplCycleCollectionUnlink(UniquePtr<ClientSource>& aField) {
  aField.reset();
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    UniquePtr<ClientSource>& aField, const char* aName, uint32_t aFlags) {
  if (aField) {
    aField->Traverse(aCallback, aName, aFlags);
  }
}

}  // namespace dom
}  // namespace mozilla

#endif  // _mozilla_dom_ClientSource_h

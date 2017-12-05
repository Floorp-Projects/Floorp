/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientSource_h
#define _mozilla_dom_ClientSource_h

#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/ClientThing.h"

class nsIDocShell;
class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

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

  void
  Shutdown();

  void
  ExecutionReady(const ClientSourceExecutionReadyArgs& aArgs);

  mozilla::dom::workers::WorkerPrivate*
  GetWorkerPrivate() const;

  nsIDocShell*
  GetDocShell() const;

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

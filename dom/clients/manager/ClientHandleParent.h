/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientHandleParent_h
#define _mozilla_dom_ClientHandleParent_h

#include "mozilla/dom/PClientHandleParent.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {
namespace dom {

class ClientManagerService;
class ClientSourceParent;

typedef MozPromise<ClientSourceParent*, CopyableErrorResult,
                   /* IsExclusive = */ false>
    SourcePromise;

class ClientHandleParent final : public PClientHandleParent {
  RefPtr<ClientManagerService> mService;
  ClientSourceParent* mSource;

  nsID mClientId;
  PrincipalInfo mPrincipalInfo;

  // A promise for HandleOps that want to access our ClientSourceParent.
  // Resolved once FoundSource is called and we have a ClientSourceParent
  // available.
  RefPtr<SourcePromise::Private> mSourcePromise;

  // PClientHandleParent interface
  mozilla::ipc::IPCResult RecvTeardown() override;

  void ActorDestroy(ActorDestroyReason aReason) override;

  PClientHandleOpParent* AllocPClientHandleOpParent(
      const ClientOpConstructorArgs& aArgs) override;

  bool DeallocPClientHandleOpParent(PClientHandleOpParent* aActor) override;

  mozilla::ipc::IPCResult RecvPClientHandleOpConstructor(
      PClientHandleOpParent* aActor,
      const ClientOpConstructorArgs& aArgs) override;

 public:
  ClientHandleParent();
  ~ClientHandleParent();

  void Init(const IPCClientInfo& aClientInfo);

  void FoundSource(ClientSourceParent* aSource);

  ClientSourceParent* GetSource() const;

  RefPtr<SourcePromise> EnsureSource();
};

}  // namespace dom
}  // namespace mozilla

#endif  // _mozilla_dom_ClientHandleParent_h

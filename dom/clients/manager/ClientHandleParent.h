/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientHandleParent_h
#define _mozilla_dom_ClientHandleParent_h

#include "mozilla/dom/PClientHandleParent.h"

namespace mozilla::dom {

class ClientManagerService;
class ClientSourceParent;

using SourcePromise =
    MozPromise<bool, CopyableErrorResult, /* IsExclusive = */ false>;

class ClientHandleParent final : public PClientHandleParent {
  RefPtr<ClientManagerService> mService;

  // mSource and mSourcePromiseHolder are mutually exclusive.
  ClientSourceParent* mSource;

  // Operations will wait on this promise while mSource is null.
  MozPromiseHolder<SourcePromise> mSourcePromiseHolder;

  MozPromiseRequestHolder<SourcePromise> mSourcePromiseRequestHolder;

  nsID mClientId;
  mozilla::ipc::PrincipalInfo mPrincipalInfo;

  ~ClientHandleParent();

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
  NS_INLINE_DECL_REFCOUNTING(ClientHandleParent, override)

  ClientHandleParent();

  void Init(const IPCClientInfo& aClientInfo);

  void FoundSource(ClientSourceParent* aSource);

  // Should be called only once EnsureSource() has resolved. May return nullptr.
  ClientSourceParent* GetSource() const;

  RefPtr<SourcePromise> EnsureSource();
};

}  // namespace mozilla::dom

#endif  // _mozilla_dom_ClientHandleParent_h

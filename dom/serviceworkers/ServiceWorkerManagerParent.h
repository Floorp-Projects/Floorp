/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ServiceWorkerManagerParent_h
#define mozilla_dom_ServiceWorkerManagerParent_h

#include "mozilla/dom/PServiceWorkerManagerParent.h"

namespace mozilla {

class OriginAttributes;

namespace ipc {
class BackgroundParentImpl;
}  // namespace ipc

namespace dom {

class ServiceWorkerManagerService;

class ServiceWorkerManagerParent final : public PServiceWorkerManagerParent {
  friend class mozilla::ipc::BackgroundParentImpl;
  friend class PServiceWorkerManagerParent;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ServiceWorkerManagerParent)

  uint64_t ID() const { return mID; }

 private:
  ServiceWorkerManagerParent();
  ~ServiceWorkerManagerParent();

  mozilla::ipc::IPCResult RecvRegister(
      const ServiceWorkerRegistrationData& aData);

  mozilla::ipc::IPCResult RecvUnregister(const PrincipalInfo& aPrincipalInfo,
                                         const nsString& aScope);

  mozilla::ipc::IPCResult RecvPropagateSoftUpdate(
      const OriginAttributes& aOriginAttributes, const nsString& aScope);

  mozilla::ipc::IPCResult RecvPropagateUnregister(
      const PrincipalInfo& aPrincipalInfo, const nsString& aScope);

  mozilla::ipc::IPCResult RecvPropagateRemove(const nsCString& aHost);

  mozilla::ipc::IPCResult RecvPropagateRemoveAll();

  mozilla::ipc::IPCResult RecvShutdown();

  PServiceWorkerUpdaterParent* AllocPServiceWorkerUpdaterParent(
      const OriginAttributes& aOriginAttributes, const nsCString& aScope);

  virtual mozilla::ipc::IPCResult RecvPServiceWorkerUpdaterConstructor(
      PServiceWorkerUpdaterParent* aActor,
      const OriginAttributes& aOriginAttributes,
      const nsCString& aScope) override;

  bool DeallocPServiceWorkerUpdaterParent(PServiceWorkerUpdaterParent* aActor);

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  RefPtr<ServiceWorkerManagerService> mService;

  // We use this ID in the Service in order to avoid the sending of messages to
  // ourself.
  uint64_t mID;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ServiceWorkerManagerParent_h

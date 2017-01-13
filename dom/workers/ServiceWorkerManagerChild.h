/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ServiceWorkerManagerChild_h
#define mozilla_dom_ServiceWorkerManagerChild_h

#include "mozilla/dom/PServiceWorkerManagerChild.h"
#include "mozilla/ipc/BackgroundUtils.h"

namespace mozilla {

class OriginAttributes;

namespace ipc {
class BackgroundChildImpl;
} // namespace ipc

namespace dom {
namespace workers {

class ServiceWorkerManagerChild final : public PServiceWorkerManagerChild
{
  friend class mozilla::ipc::BackgroundChildImpl;

public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerManagerChild)

  void ManagerShuttingDown()
  {
    mShuttingDown = true;
  }

  virtual mozilla::ipc::IPCResult RecvNotifyRegister(const ServiceWorkerRegistrationData& aData)
                                                                       override;

  virtual mozilla::ipc::IPCResult RecvNotifySoftUpdate(const OriginAttributes& aOriginAttributes,
                                                       const nsString& aScope) override;

  virtual mozilla::ipc::IPCResult RecvNotifyUnregister(const PrincipalInfo& aPrincipalInfo,
                                                       const nsString& aScope) override;

  virtual mozilla::ipc::IPCResult RecvNotifyRemove(const nsCString& aHost) override;

  virtual mozilla::ipc::IPCResult RecvNotifyRemoveAll() override;

private:
  ServiceWorkerManagerChild()
    : mShuttingDown(false)
  {}

  ~ServiceWorkerManagerChild() {}

  bool mShuttingDown;
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ServiceWorkerManagerChild_h

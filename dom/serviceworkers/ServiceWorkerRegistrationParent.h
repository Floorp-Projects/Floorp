/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkerregistrationparent_h__
#define mozilla_dom_serviceworkerregistrationparent_h__

#include "mozilla/dom/PServiceWorkerRegistrationParent.h"

namespace mozilla::dom {

class IPCServiceWorkerRegistrationDescriptor;
class ServiceWorkerRegistrationProxy;

class ServiceWorkerRegistrationParent final
    : public PServiceWorkerRegistrationParent {
  RefPtr<ServiceWorkerRegistrationProxy> mProxy;
  bool mDeleteSent;

  ~ServiceWorkerRegistrationParent();

  // PServiceWorkerRegistrationParent
  void ActorDestroy(ActorDestroyReason aReason) override;

  mozilla::ipc::IPCResult RecvTeardown() override;

  mozilla::ipc::IPCResult RecvUnregister(
      UnregisterResolver&& aResolver) override;

  mozilla::ipc::IPCResult RecvUpdate(const nsACString& aNewestWorkerScriptUrl,
                                     UpdateResolver&& aResolver) override;

  mozilla::ipc::IPCResult RecvSetNavigationPreloadEnabled(
      const bool& aEnabled,
      SetNavigationPreloadEnabledResolver&& aResolver) override;

  mozilla::ipc::IPCResult RecvSetNavigationPreloadHeader(
      const nsACString& aHeader,
      SetNavigationPreloadHeaderResolver&& aResolver) override;

  mozilla::ipc::IPCResult RecvGetNavigationPreloadState(
      GetNavigationPreloadStateResolver&& aResolver) override;

 public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerRegistrationParent, override);

  ServiceWorkerRegistrationParent();

  void Init(const IPCServiceWorkerRegistrationDescriptor& aDescriptor);

  void MaybeSendDelete();
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_serviceworkerregistrationparent_h__

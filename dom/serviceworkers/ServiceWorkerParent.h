/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkerparent_h__
#define mozilla_dom_serviceworkerparent_h__

#include "mozilla/dom/PServiceWorkerParent.h"

namespace mozilla::dom {

class IPCServiceWorkerDescriptor;
class ServiceWorkerProxy;

class ServiceWorkerParent final : public PServiceWorkerParent {
  RefPtr<ServiceWorkerProxy> mProxy;
  bool mDeleteSent;

  ~ServiceWorkerParent();

  // PServiceWorkerParent
  void ActorDestroy(ActorDestroyReason aReason) override;

  mozilla::ipc::IPCResult RecvTeardown() override;

  mozilla::ipc::IPCResult RecvPostMessage(
      const ClonedOrErrorMessageData& aClonedData,
      const ClientInfoAndState& aSource) override;

 public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerParent, override);

  ServiceWorkerParent();

  void Init(const IPCServiceWorkerDescriptor& aDescriptor);

  void MaybeSendDelete();
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_serviceworkerparent_h__

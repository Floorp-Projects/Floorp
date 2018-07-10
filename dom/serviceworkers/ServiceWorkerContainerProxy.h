/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef moz_dom_ServiceWorkerContainerProxy_h
#define moz_dom_ServiceWorkerContainerProxy_h

namespace mozilla {
namespace dom {

class ServiceWorkerContainerParent;

class ServiceWorkerContainerProxy final
{
  // Background thread only
  ServiceWorkerContainerParent* mActor;

  ~ServiceWorkerContainerProxy();

public:
  explicit ServiceWorkerContainerProxy(ServiceWorkerContainerParent* aActor);

  void
  RevokeActor(ServiceWorkerContainerParent* aActor);

  RefPtr<ServiceWorkerRegistrationPromise>
  Register(const ClientInfo& aClientInfo, const nsCString& aScopeURL,
           const nsCString& aScriptURL,
           ServiceWorkerUpdateViaCache aUpdateViaCache);

  RefPtr<ServiceWorkerRegistrationPromise>
  GetRegistration(const ClientInfo& aClientInfo, const nsCString& aURL);

  RefPtr<ServiceWorkerRegistrationListPromise>
  GetRegistrations(const ClientInfo& aClientInfo);

  RefPtr<ServiceWorkerRegistrationPromise>
  GetReady(const ClientInfo& aClientInfo);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ServiceWorkerContainerProxy);
};

} // namespace dom
} // namespace mozilla

#endif // moz_dom_ServiceWorkerContainerProxy_h

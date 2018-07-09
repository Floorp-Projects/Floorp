/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef moz_dom_ServiceWorkerProxy_h
#define moz_dom_ServiceWorkerProxy_h

#include "nsProxyRelease.h"
#include "ServiceWorkerDescriptor.h"

namespace mozilla {
namespace dom {

class ServiceWorkerInfo;
class ServiceWorkerParent;

class ServiceWorkerProxy final
{
  // Background thread only
  ServiceWorkerParent* mActor;

  // Written on background thread and read on main thread
  nsCOMPtr<nsISerialEventTarget> mEventTarget;

  // Main thread only
  ServiceWorkerDescriptor mDescriptor;
  nsMainThreadPtrHandle<ServiceWorkerInfo> mInfo;

  ~ServiceWorkerProxy();

  // Background thread methods
  void
  MaybeShutdownOnBGThread();

  void
  SetStateOnBGThread(ServiceWorkerState aState);

  // Main thread methods
  void
  InitOnMainThread();

  void
  MaybeShutdownOnMainThread();

  void
  StopListeningOnMainThread();

public:
  explicit ServiceWorkerProxy(const ServiceWorkerDescriptor& aDescriptor);

  void
  Init(ServiceWorkerParent* aActor);

  void
  RevokeActor(ServiceWorkerParent* aActor);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ServiceWorkerProxy);
};

} // namespace dom
} // namespace mozilla

#endif // moz_dom_ServiceWorkerProxy_h

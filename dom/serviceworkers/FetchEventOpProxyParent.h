/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fetcheventopproxyparent_h__
#define mozilla_dom_fetcheventopproxyparent_h__

#include "mozilla/RefPtr.h"
#include "mozilla/dom/PFetchEventOpProxyParent.h"
#include "mozilla/dom/ServiceWorkerOpPromise.h"

namespace mozilla::dom {

class FetchEventOpParent;
class PRemoteWorkerParent;
class ParentToParentServiceWorkerFetchEventOpArgs;

/**
 * FetchEventOpProxyParent owns a FetchEventOpParent in order to propagate
 * the respondWith() value by directly calling SendRespondWith on the
 * FetchEventOpParent, but the call to Send__delete__ is handled via MozPromise.
 * This is done because this actor may only be created after its managing
 * PRemoteWorker is created, which is asynchronous and may fail.  We take on
 * responsibility for the promise once we are created, but we may not be created
 * if the RemoteWorker is never successfully launched.
 */
class FetchEventOpProxyParent final : public PFetchEventOpProxyParent {
  friend class PFetchEventOpProxyParent;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FetchEventOpProxyParent, override);

  static void Create(
      PRemoteWorkerParent* aManager,
      RefPtr<ServiceWorkerFetchEventOpPromise::Private>&& aPromise,
      const ParentToParentServiceWorkerFetchEventOpArgs& aArgs,
      RefPtr<FetchEventOpParent> aReal, nsCOMPtr<nsIInputStream> aBodyStream);

 private:
  FetchEventOpProxyParent(
      RefPtr<FetchEventOpParent>&& aReal,
      RefPtr<ServiceWorkerFetchEventOpPromise::Private>&& aPromise);

  ~FetchEventOpProxyParent();

  mozilla::ipc::IPCResult RecvAsyncLog(const nsCString& aScriptSpec,
                                       const uint32_t& aLineNumber,
                                       const uint32_t& aColumnNumber,
                                       const nsCString& aMessageName,
                                       nsTArray<nsString>&& aParams);

  mozilla::ipc::IPCResult RecvRespondWith(
      const ChildToParentFetchEventRespondWithResult& aResult);

  mozilla::ipc::IPCResult Recv__delete__(
      const ServiceWorkerFetchEventOpResult& aResult);

  void ActorDestroy(ActorDestroyReason) override;

  RefPtr<FetchEventOpParent> mReal;
  RefPtr<ServiceWorkerFetchEventOpPromise::Private> mLifetimePromise;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_fetcheventopproxyparent_h__

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fetcheventopchild_h__
#define mozilla_dom_fetcheventopchild_h__

#include "nsCOMPtr.h"

#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/PFetchEventOpChild.h"
#include "mozilla/dom/ServiceWorkerOpArgs.h"

class nsIInterceptedChannel;

namespace mozilla {
namespace dom {

class KeepAliveToken;
class PRemoteWorkerControllerChild;
class ServiceWorkerRegistrationInfo;

/**
 * FetchEventOpChild represents an in-flight FetchEvent operation.
 */
class FetchEventOpChild final : public PFetchEventOpChild {
  friend class PFetchEventOpChild;

 public:
  static RefPtr<GenericPromise> Create(
      PRemoteWorkerControllerChild* aManager,
      ServiceWorkerFetchEventOpArgs&& aArgs,
      nsCOMPtr<nsIInterceptedChannel> aInterceptedChannel,
      RefPtr<ServiceWorkerRegistrationInfo> aRegistrationInfo,
      RefPtr<KeepAliveToken>&& aKeepAliveToken);

  ~FetchEventOpChild();

 private:
  FetchEventOpChild(ServiceWorkerFetchEventOpArgs&& aArgs,
                    nsCOMPtr<nsIInterceptedChannel>&& aInterceptedChannel,
                    RefPtr<ServiceWorkerRegistrationInfo>&& aRegistrationInfo,
                    RefPtr<KeepAliveToken>&& aKeepAliveToken);

  mozilla::ipc::IPCResult RecvAsyncLog(const nsCString& aScriptSpec,
                                       const uint32_t& aLineNumber,
                                       const uint32_t& aColumnNumber,
                                       const nsCString& aMessageName,
                                       nsTArray<nsString>&& aParams);

  mozilla::ipc::IPCResult RecvRespondWith(
      IPCFetchEventRespondWithResult&& aResult);

  mozilla::ipc::IPCResult Recv__delete__(
      const ServiceWorkerFetchEventOpResult& aResult) override;

  void ActorDestroy(ActorDestroyReason) override;

  nsresult StartSynthesizedResponse(IPCSynthesizeResponseArgs&& aArgs);

  void SynthesizeResponse(IPCSynthesizeResponseArgs&& aArgs);

  void ResetInterception();

  void CancelInterception(nsresult aStatus);

  void MaybeScheduleRegistrationUpdate() const;

  const ServiceWorkerFetchEventOpArgs mArgs;
  nsCOMPtr<nsIInterceptedChannel> mInterceptedChannel;
  RefPtr<ServiceWorkerRegistrationInfo> mRegistration;
  RefPtr<KeepAliveToken> mKeepAliveToken;
  bool mInterceptedChannelHandled = false;
  MozPromiseHolder<GenericPromise> mPromiseHolder;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_fetcheventopchild_h__

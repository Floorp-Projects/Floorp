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
#include "mozilla/dom/FetchService.h"
#include "mozilla/dom/PFetchEventOpChild.h"
#include "mozilla/dom/ServiceWorkerOpArgs.h"

class nsIInterceptedChannel;

namespace mozilla::dom {

class KeepAliveToken;
class PRemoteWorkerControllerChild;
class ServiceWorkerRegistrationInfo;

/**
 * FetchEventOpChild represents an in-flight FetchEvent operation.
 */
class FetchEventOpChild final : public PFetchEventOpChild {
  friend class PFetchEventOpChild;

 public:
  static RefPtr<GenericPromise> SendFetchEvent(
      PRemoteWorkerControllerChild* aManager,
      ParentToParentServiceWorkerFetchEventOpArgs&& aArgs,
      nsCOMPtr<nsIInterceptedChannel> aInterceptedChannel,
      RefPtr<ServiceWorkerRegistrationInfo> aRegistrationInfo,
      RefPtr<FetchServicePromises>&& aPreloadResponseReadyPromises,
      RefPtr<KeepAliveToken>&& aKeepAliveToken);

  ~FetchEventOpChild();

 private:
  FetchEventOpChild(
      ParentToParentServiceWorkerFetchEventOpArgs&& aArgs,
      nsCOMPtr<nsIInterceptedChannel>&& aInterceptedChannel,
      RefPtr<ServiceWorkerRegistrationInfo>&& aRegistrationInfo,
      RefPtr<FetchServicePromises>&& aPreloadResponseReadyPromises,
      RefPtr<KeepAliveToken>&& aKeepAliveToken);

  mozilla::ipc::IPCResult RecvAsyncLog(const nsCString& aScriptSpec,
                                       const uint32_t& aLineNumber,
                                       const uint32_t& aColumnNumber,
                                       const nsCString& aMessageName,
                                       nsTArray<nsString>&& aParams);

  mozilla::ipc::IPCResult RecvRespondWith(
      ParentToParentFetchEventRespondWithResult&& aResult);

  mozilla::ipc::IPCResult Recv__delete__(
      const ServiceWorkerFetchEventOpResult& aResult);

  void ActorDestroy(ActorDestroyReason) override;

  nsresult StartSynthesizedResponse(
      ParentToParentSynthesizeResponseArgs&& aArgs);

  void SynthesizeResponse(ParentToParentSynthesizeResponseArgs&& aArgs);

  void ResetInterception(bool aBypass);

  void CancelInterception(nsresult aStatus);

  void MaybeScheduleRegistrationUpdate() const;

  ParentToParentServiceWorkerFetchEventOpArgs mArgs;
  nsCOMPtr<nsIInterceptedChannel> mInterceptedChannel;
  RefPtr<ServiceWorkerRegistrationInfo> mRegistration;
  RefPtr<KeepAliveToken> mKeepAliveToken;
  bool mInterceptedChannelHandled = false;
  MozPromiseHolder<GenericPromise> mPromiseHolder;
  bool mWasSent = false;
  MozPromiseRequestHolder<FetchServiceResponseAvailablePromise>
      mPreloadResponseAvailablePromiseRequestHolder;
  MozPromiseRequestHolder<FetchServiceResponseTimingPromise>
      mPreloadResponseTimingPromiseRequestHolder;
  MozPromiseRequestHolder<FetchServiceResponseEndPromise>
      mPreloadResponseEndPromiseRequestHolder;
  RefPtr<FetchServicePromises> mPreloadResponseReadyPromises;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_fetcheventopchild_h__

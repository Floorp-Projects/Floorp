/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_ActorsChild_h
#define mozilla_dom_quota_ActorsChild_h

#include <cstdint>
#include "ErrorList.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/quota/PQuotaChild.h"
#include "mozilla/dom/quota/PQuotaRequestChild.h"
#include "mozilla/dom/quota/PQuotaUsageRequestChild.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "nsCOMPtr.h"
#include "nsStringFwd.h"
#include "nsTArray.h"

class nsIEventTarget;

namespace mozilla {
namespace ipc {

class BackgroundChildImpl;

}  // namespace ipc

namespace dom::quota {

class QuotaManagerService;
class Request;
class UsageRequest;

class QuotaChild final : public PQuotaChild {
  friend class mozilla::ipc::BackgroundChildImpl;
  friend class QuotaManagerService;

  QuotaManagerService* mService;

#ifdef DEBUG
  nsCOMPtr<nsIEventTarget> mOwningThread;
#endif

 public:
  NS_INLINE_DECL_REFCOUNTING(QuotaChild, override)

  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

 private:
  // Only created by QuotaManagerService.
  explicit QuotaChild(QuotaManagerService* aService);

  ~QuotaChild();

  // IPDL methods are only called by IPDL.
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual PQuotaUsageRequestChild* AllocPQuotaUsageRequestChild(
      const UsageRequestParams& aParams) override;

  virtual bool DeallocPQuotaUsageRequestChild(
      PQuotaUsageRequestChild* aActor) override;

  virtual PQuotaRequestChild* AllocPQuotaRequestChild(
      const RequestParams& aParams) override;

  virtual bool DeallocPQuotaRequestChild(PQuotaRequestChild* aActor) override;
};

class QuotaUsageRequestChild final : public PQuotaUsageRequestChild {
  friend class QuotaChild;
  friend class QuotaManagerService;

  RefPtr<UsageRequest> mRequest;

 public:
  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

 private:
  // Only created by QuotaManagerService.
  explicit QuotaUsageRequestChild(UsageRequest* aRequest);

  // Only destroyed by QuotaChild.
  ~QuotaUsageRequestChild();

  void HandleResponse(nsresult aResponse);

  void HandleResponse(const nsTArray<OriginUsage>& aResponse);

  void HandleResponse(const OriginUsageResponse& aResponse);

  // IPDL methods are only called by IPDL.
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual mozilla::ipc::IPCResult Recv__delete__(
      const UsageRequestResponse& aResponse) override;
};

class QuotaRequestChild final : public PQuotaRequestChild {
  friend class QuotaChild;
  friend class QuotaManagerService;

  RefPtr<Request> mRequest;

 public:
  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

 private:
  // Only created by QuotaManagerService.
  explicit QuotaRequestChild(Request* aRequest);

  // Only destroyed by QuotaChild.
  ~QuotaRequestChild();

  void HandleResponse(nsresult aResponse);

  void HandleResponse();

  void HandleResponse(bool aResponse);

  void HandleResponse(const nsAString& aResponse);

  void HandleResponse(const EstimateResponse& aResponse);

  void HandleResponse(const nsTArray<nsCString>& aResponse);

  void HandleResponse(const GetFullOriginMetadataResponse& aResponse);

  // IPDL methods are only called by IPDL.
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual mozilla::ipc::IPCResult Recv__delete__(
      const RequestResponse& aResponse) override;
};

}  // namespace dom::quota
}  // namespace mozilla

#endif  // mozilla_dom_quota_ActorsChild_h

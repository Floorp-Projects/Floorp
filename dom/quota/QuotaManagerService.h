/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_QuotaManagerService_h
#define mozilla_dom_quota_QuotaManagerService_h

#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/dom/battery/Types.h"
#include "mozilla/Observer.h"
#include "nsAutoPtr.h"
#include "nsIObserver.h"
#include "nsIQuotaManagerService.h"

#define QUOTAMANAGER_SERVICE_CONTRACTID \
  "@mozilla.org/dom/quota-manager-service;1"

namespace mozilla {
namespace ipc {

class PBackgroundChild;

} // namespace ipc

namespace hal {
class BatteryInformation;
}

namespace dom {
namespace quota {

class QuotaChild;
class QuotaManager;

class QuotaManagerService final
  : public nsIQuotaManagerService
  , public nsIObserver
  , public BatteryObserver
{
  typedef mozilla::ipc::PBackgroundChild PBackgroundChild;

  class BackgroundCreateCallback;
  class PendingRequestInfo;
  class UsageRequestInfo;
  class RequestInfo;
  class IdleMaintenanceInfo;

  nsCOMPtr<nsIEventTarget> mBackgroundThread;

  nsTArray<nsAutoPtr<PendingRequestInfo>> mPendingRequests;

  QuotaChild* mBackgroundActor;

  bool mBackgroundActorFailed;
  bool mIdleObserverRegistered;

public:
  // Returns a non-owning reference.
  static QuotaManagerService*
  GetOrCreate();

  // Returns a non-owning reference.
  static QuotaManagerService*
  Get();

  // Returns an owning reference! No one should call this but the factory.
  static QuotaManagerService*
  FactoryCreate();

  void
  ClearBackgroundActor();

  void
  NoteLiveManager(QuotaManager* aManager);

  void
  NoteShuttingDownManager();

  // Called when a process is being shot down. Aborts any running operations
  // for the given process.
  void
  AbortOperationsForProcess(ContentParentId aContentParentId);

private:
  QuotaManagerService();
  ~QuotaManagerService();

  nsresult
  Init();

  void
  Destroy();

  nsresult
  InitiateRequest(nsAutoPtr<PendingRequestInfo>& aInfo);

  nsresult
  BackgroundActorCreated(PBackgroundChild* aBackgroundActor);

  void
  BackgroundActorFailed();

  void
  PerformIdleMaintenance();

  void
  RemoveIdleObserver();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIQUOTAMANAGERSERVICE
  NS_DECL_NSIOBSERVER

  // BatteryObserver override
  void
  Notify(const hal::BatteryInformation& aBatteryInfo) override;
};

} // namespace quota
} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_quota_QuotaManagerService_h */

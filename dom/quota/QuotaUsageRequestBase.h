/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_QUOTAUSAGEREQUESTBASE_H_
#define DOM_QUOTA_QUOTAUSAGEREQUESTBASE_H_

#include "NormalOriginOperationBase.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/dom/quota/PQuotaUsageRequestParent.h"

class nsIFile;

namespace mozilla {

template <typename V, typename E>
class Result;

namespace dom::quota {

struct OriginMetadata;
class UsageInfo;
class UsageRequestResponse;

class QuotaUsageRequestBase : public NormalOriginOperationBase,
                              public PQuotaUsageRequestParent {
 protected:
  QuotaUsageRequestBase(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                        const char* aName)
      : NormalOriginOperationBase(std::move(aQuotaManager), aName) {}

  mozilla::Result<UsageInfo, nsresult> GetUsageForOrigin(
      QuotaManager& aQuotaManager, PersistenceType aPersistenceType,
      const OriginMetadata& aOriginMetadata);

  // Subclasses use this override to set the IPDL response value.
  virtual void GetResponse(UsageRequestResponse& aResponse) = 0;

 private:
  mozilla::Result<UsageInfo, nsresult> GetUsageForOriginEntries(
      QuotaManager& aQuotaManager, PersistenceType aPersistenceType,
      const OriginMetadata& aOriginMetadata, nsIFile& aDirectory,
      bool aInitialized);

  void SendResults() override;

  // IPDL methods.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvCancel() final;
};

}  // namespace dom::quota
}  // namespace mozilla

#endif  // DOM_QUOTA_QUOTAUSAGEREQUESTBASE_H_

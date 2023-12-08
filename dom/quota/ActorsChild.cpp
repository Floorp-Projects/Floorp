/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsChild.h"

// Local includes
#include "QuotaManagerService.h"
#include "QuotaRequests.h"
#include "QuotaResults.h"

// Global includes
#include <new>
#include <utility>
#include "mozilla/Assertions.h"
#include "mozilla/dom/quota/PQuotaRequest.h"
#include "mozilla/dom/quota/PQuotaUsageRequest.h"
#include "nsError.h"
#include "nsID.h"
#include "nsIEventTarget.h"
#include "nsIQuotaResults.h"
#include "nsISupports.h"
#include "nsIVariant.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsVariant.h"

namespace mozilla::dom::quota {

/*******************************************************************************
 * QuotaChild
 ******************************************************************************/

QuotaChild::QuotaChild(QuotaManagerService* aService)
    : mService(aService)
#ifdef DEBUG
      ,
      mOwningThread(GetCurrentSerialEventTarget())
#endif
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aService);

  MOZ_COUNT_CTOR(quota::QuotaChild);
}

QuotaChild::~QuotaChild() {
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(quota::QuotaChild);
}

#ifdef DEBUG

void QuotaChild::AssertIsOnOwningThread() const {
  MOZ_ASSERT(mOwningThread);

  bool current;
  MOZ_ASSERT(NS_SUCCEEDED(mOwningThread->IsOnCurrentThread(&current)));
  MOZ_ASSERT(current);
}

#endif  // DEBUG

void QuotaChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  if (mService) {
    mService->ClearBackgroundActor();
#ifdef DEBUG
    mService = nullptr;
#endif
  }
}

PQuotaUsageRequestChild* QuotaChild::AllocPQuotaUsageRequestChild(
    const UsageRequestParams& aParams) {
  AssertIsOnOwningThread();

  MOZ_CRASH("PQuotaUsageRequestChild actors should be manually constructed!");
}

bool QuotaChild::DeallocPQuotaUsageRequestChild(
    PQuotaUsageRequestChild* aActor) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);

  delete static_cast<QuotaUsageRequestChild*>(aActor);
  return true;
}

PQuotaRequestChild* QuotaChild::AllocPQuotaRequestChild(
    const RequestParams& aParams) {
  AssertIsOnOwningThread();

  MOZ_CRASH("PQuotaRequestChild actors should be manually constructed!");
}

bool QuotaChild::DeallocPQuotaRequestChild(PQuotaRequestChild* aActor) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);

  delete static_cast<QuotaRequestChild*>(aActor);
  return true;
}

/*******************************************************************************
 * QuotaUsageRequestChild
 ******************************************************************************/

QuotaUsageRequestChild::QuotaUsageRequestChild(UsageRequest* aRequest)
    : mRequest(aRequest) {
  AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(quota::QuotaUsageRequestChild);
}

QuotaUsageRequestChild::~QuotaUsageRequestChild() {
  // Can't assert owning thread here because the request is cleared.

  MOZ_COUNT_DTOR(quota::QuotaUsageRequestChild);
}

#ifdef DEBUG

void QuotaUsageRequestChild::AssertIsOnOwningThread() const {
  MOZ_ASSERT(mRequest);
  mRequest->AssertIsOnOwningThread();
}

#endif  // DEBUG

void QuotaUsageRequestChild::HandleResponse(nsresult aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResponse));
  MOZ_ASSERT(mRequest);

  mRequest->SetError(aResponse);
}

void QuotaUsageRequestChild::HandleResponse(
    const nsTArray<OriginUsage>& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  RefPtr<nsVariant> variant = new nsVariant();

  if (aResponse.IsEmpty()) {
    variant->SetAsEmptyArray();
  } else {
    nsTArray<RefPtr<UsageResult>> usageResults(aResponse.Length());

    for (const auto& originUsage : aResponse) {
      usageResults.AppendElement(MakeRefPtr<UsageResult>(
          originUsage.origin(), originUsage.persisted(), originUsage.usage(),
          originUsage.lastAccessed()));
    }

    variant->SetAsArray(nsIDataType::VTYPE_INTERFACE_IS,
                        &NS_GET_IID(nsIQuotaUsageResult), usageResults.Length(),
                        static_cast<void*>(usageResults.Elements()));
  }

  mRequest->SetResult(variant);
}

void QuotaUsageRequestChild::HandleResponse(
    const OriginUsageResponse& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  RefPtr<OriginUsageResult> result =
      new OriginUsageResult(aResponse.usageInfo());

  RefPtr<nsVariant> variant = new nsVariant();
  variant->SetAsInterface(NS_GET_IID(nsIQuotaOriginUsageResult), result);

  mRequest->SetResult(variant);
}

void QuotaUsageRequestChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  if (mRequest) {
    mRequest->ClearBackgroundActor();
#ifdef DEBUG
    mRequest = nullptr;
#endif
  }
}

mozilla::ipc::IPCResult QuotaUsageRequestChild::Recv__delete__(
    const UsageRequestResponse& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  switch (aResponse.type()) {
    case UsageRequestResponse::Tnsresult:
      HandleResponse(aResponse.get_nsresult());
      break;

    case UsageRequestResponse::TAllUsageResponse:
      HandleResponse(aResponse.get_AllUsageResponse().originUsages());
      break;

    case UsageRequestResponse::TOriginUsageResponse:
      HandleResponse(aResponse.get_OriginUsageResponse());
      break;

    default:
      MOZ_CRASH("Unknown response type!");
  }

  return IPC_OK();
}

/*******************************************************************************
 * QuotaRequestChild
 ******************************************************************************/

QuotaRequestChild::QuotaRequestChild(Request* aRequest) : mRequest(aRequest) {
  AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(quota::QuotaRequestChild);
}

QuotaRequestChild::~QuotaRequestChild() {
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(quota::QuotaRequestChild);
}

#ifdef DEBUG

void QuotaRequestChild::AssertIsOnOwningThread() const {
  MOZ_ASSERT(mRequest);
  mRequest->AssertIsOnOwningThread();
}

#endif  // DEBUG

void QuotaRequestChild::HandleResponse(nsresult aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResponse));
  MOZ_ASSERT(mRequest);

  mRequest->SetError(aResponse);
}

void QuotaRequestChild::HandleResponse() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  RefPtr<nsVariant> variant = new nsVariant();
  variant->SetAsVoid();

  mRequest->SetResult(variant);
}

void QuotaRequestChild::HandleResponse(bool aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  RefPtr<nsVariant> variant = new nsVariant();
  variant->SetAsBool(aResponse);

  mRequest->SetResult(variant);
}

void QuotaRequestChild::HandleResponse(const nsAString& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  RefPtr<nsVariant> variant = new nsVariant();
  variant->SetAsAString(aResponse);

  mRequest->SetResult(variant);
}

void QuotaRequestChild::HandleResponse(const EstimateResponse& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  RefPtr<EstimateResult> result =
      new EstimateResult(aResponse.usage(), aResponse.limit());

  RefPtr<nsVariant> variant = new nsVariant();
  variant->SetAsInterface(NS_GET_IID(nsIQuotaEstimateResult), result);

  mRequest->SetResult(variant);
}

void QuotaRequestChild::HandleResponse(const nsTArray<nsCString>& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  RefPtr<nsVariant> variant = new nsVariant();

  if (aResponse.IsEmpty()) {
    variant->SetAsEmptyArray();
  } else {
    nsTArray<const char*> stringPointers(aResponse.Length());
    std::transform(aResponse.cbegin(), aResponse.cend(),
                   MakeBackInserter(stringPointers),
                   std::mem_fn(&nsCString::get));

    variant->SetAsArray(nsIDataType::VTYPE_CHAR_STR, nullptr,
                        stringPointers.Length(), stringPointers.Elements());
  }

  mRequest->SetResult(variant);
}

void QuotaRequestChild::HandleResponse(
    const GetFullOriginMetadataResponse& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  RefPtr<nsVariant> variant = new nsVariant();

  if (aResponse.maybeFullOriginMetadata()) {
    RefPtr<FullOriginMetadataResult> result =
        new FullOriginMetadataResult(*aResponse.maybeFullOriginMetadata());

    variant->SetAsInterface(NS_GET_IID(nsIQuotaFullOriginMetadataResult),
                            result);

  } else {
    variant->SetAsVoid();
  }

  mRequest->SetResult(variant);
}

void QuotaRequestChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();
}

mozilla::ipc::IPCResult QuotaRequestChild::Recv__delete__(
    const RequestResponse& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  switch (aResponse.type()) {
    case RequestResponse::Tnsresult:
      HandleResponse(aResponse.get_nsresult());
      break;

    case RequestResponse::TStorageNameResponse:
      HandleResponse(aResponse.get_StorageNameResponse().name());
      break;

    case RequestResponse::TResetOriginResponse:
    case RequestResponse::TPersistResponse:
      HandleResponse();
      break;

    case RequestResponse::TInitializePersistentOriginResponse:
      HandleResponse(
          aResponse.get_InitializePersistentOriginResponse().created());
      break;

    case RequestResponse::TInitializeTemporaryOriginResponse:
      HandleResponse(
          aResponse.get_InitializeTemporaryOriginResponse().created());
      break;

    case RequestResponse::TGetFullOriginMetadataResponse:
      HandleResponse(aResponse.get_GetFullOriginMetadataResponse());
      break;

    case RequestResponse::TPersistedResponse:
      HandleResponse(aResponse.get_PersistedResponse().persisted());
      break;

    case RequestResponse::TEstimateResponse:
      HandleResponse(aResponse.get_EstimateResponse());
      break;

    case RequestResponse::TListOriginsResponse:
      HandleResponse(aResponse.get_ListOriginsResponse().origins());
      break;

    default:
      MOZ_CRASH("Unknown response type!");
  }

  return IPC_OK();
}

}  // namespace mozilla::dom::quota

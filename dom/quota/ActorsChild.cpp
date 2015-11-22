/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsChild.h"

#include "QuotaManagerService.h"
#include "QuotaRequests.h"

namespace mozilla {
namespace dom {
namespace quota {

/*******************************************************************************
 * QuotaChild
 ******************************************************************************/

QuotaChild::QuotaChild(QuotaManagerService* aService)
  : mService(aService)
#ifdef DEBUG
  , mOwningThread(NS_GetCurrentThread())
#endif
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aService);

  MOZ_COUNT_CTOR(quota::QuotaChild);
}

QuotaChild::~QuotaChild()
{
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(quota::QuotaChild);
}

#ifdef DEBUG

void
QuotaChild::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(mOwningThread);

  bool current;
  MOZ_ASSERT(NS_SUCCEEDED(mOwningThread->IsOnCurrentThread(&current)));
  MOZ_ASSERT(current);
}

#endif // DEBUG

void
QuotaChild::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  if (mService) {
    mService->ClearBackgroundActor();
#ifdef DEBUG
    mService = nullptr;
#endif
  }
}

PQuotaUsageRequestChild*
QuotaChild::AllocPQuotaUsageRequestChild(const UsageRequestParams& aParams)
{
  AssertIsOnOwningThread();

  MOZ_CRASH("PQuotaUsageRequestChild actors should be manually constructed!");
}

bool
QuotaChild::DeallocPQuotaUsageRequestChild(PQuotaUsageRequestChild* aActor)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);

  delete static_cast<QuotaUsageRequestChild*>(aActor);
  return true;
}

PQuotaRequestChild*
QuotaChild::AllocPQuotaRequestChild(const RequestParams& aParams)
{
  AssertIsOnOwningThread();

  MOZ_CRASH("PQuotaRequestChild actors should be manually constructed!");
}

bool
QuotaChild::DeallocPQuotaRequestChild(PQuotaRequestChild* aActor)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);

  delete static_cast<QuotaRequestChild*>(aActor);
  return true;
}

/*******************************************************************************
 * QuotaUsageRequestChild
 ******************************************************************************/

QuotaUsageRequestChild::QuotaUsageRequestChild(UsageRequest* aRequest)
  : mRequest(aRequest)
{
  AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(quota::QuotaUsageRequestChild);
}

QuotaUsageRequestChild::~QuotaUsageRequestChild()
{
  // Can't assert owning thread here because the request is cleared.

  MOZ_COUNT_DTOR(quota::QuotaUsageRequestChild);
}

#ifdef DEBUG

void
QuotaUsageRequestChild::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(mRequest);
  mRequest->AssertIsOnOwningThread();
}

#endif // DEBUG

void
QuotaUsageRequestChild::HandleResponse(nsresult aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResponse));
  MOZ_ASSERT(mRequest);

  mRequest->SetError(aResponse);
}

void
QuotaUsageRequestChild::HandleResponse(const UsageResponse& aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  mRequest->SetResult(aResponse.usage(), aResponse.fileUsage());
}

void
QuotaUsageRequestChild::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  if (mRequest) {
    mRequest->ClearBackgroundActor();
#ifdef DEBUG
    mRequest = nullptr;
#endif
  }
}

bool
QuotaUsageRequestChild::Recv__delete__(const UsageRequestResponse& aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  switch (aResponse.type()) {
    case UsageRequestResponse::Tnsresult:
      HandleResponse(aResponse.get_nsresult());
      break;

    case UsageRequestResponse::TUsageResponse:
      HandleResponse(aResponse.get_UsageResponse());
      break;

    default:
      MOZ_CRASH("Unknown response type!");
  }

  return true;
}

/*******************************************************************************
 * QuotaRequestChild
 ******************************************************************************/

QuotaRequestChild::QuotaRequestChild(Request* aRequest)
  : mRequest(aRequest)
{
  AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(quota::QuotaRequestChild);
}

QuotaRequestChild::~QuotaRequestChild()
{
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(quota::QuotaRequestChild);
}

#ifdef DEBUG

void
QuotaRequestChild::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(mRequest);
  mRequest->AssertIsOnOwningThread();
}

#endif // DEBUG

void
QuotaRequestChild::HandleResponse(nsresult aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResponse));
  MOZ_ASSERT(mRequest);

  mRequest->SetError(aResponse);
}

void
QuotaRequestChild::HandleResponse()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  mRequest->SetResult();
}

void
QuotaRequestChild::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();
}

bool
QuotaRequestChild::Recv__delete__(const RequestResponse& aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  switch (aResponse.type()) {
    case RequestResponse::Tnsresult:
      HandleResponse(aResponse.get_nsresult());
      break;

    case RequestResponse::TClearOriginResponse:
    case RequestResponse::TClearAppResponse:
    case RequestResponse::TClearAllResponse:
    case RequestResponse::TResetAllResponse:
      HandleResponse();
      break;

    default:
      MOZ_CRASH("Unknown response type!");
  }

  return true;
}

} // namespace quota
} // namespace dom
} // namespace mozilla

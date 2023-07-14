/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_ORIGINOPERATIONS_H_
#define DOM_QUOTA_ORIGINOPERATIONS_H_

#include <cstdint>

#include "nsTArrayForwardDeclare.h"

class nsISerialEventTarget;
template <class T>
class RefPtr;

namespace mozilla::dom::quota {

class EstimateParams;
class GetFullOriginMetadataParams;
class NormalOriginOperationBase;
class OriginDirectoryLock;
struct OriginMetadata;
class OriginOperationBase;
class QuotaRequestBase;
class QuotaUsageRequestBase;
class RequestParams;
template <typename T>
class ResolvableNormalOriginOp;
class UsageRequestParams;

RefPtr<OriginOperationBase> CreateFinalizeOriginEvictionOp(
    nsISerialEventTarget* aOwningThread,
    nsTArray<RefPtr<OriginDirectoryLock>>&& aLocks);

RefPtr<NormalOriginOperationBase> CreateSaveOriginAccessTimeOp(
    const OriginMetadata& aOriginMetadata, int64_t aTimestamp);

RefPtr<ResolvableNormalOriginOp<bool>> CreateClearPrivateRepositoryOp();

RefPtr<ResolvableNormalOriginOp<bool>> CreateShutdownStorageOp();

RefPtr<QuotaUsageRequestBase> CreateGetUsageOp(
    const UsageRequestParams& aParams);

RefPtr<QuotaUsageRequestBase> CreateGetOriginUsageOp(
    const UsageRequestParams& aParams);

RefPtr<QuotaRequestBase> CreateStorageNameOp();

RefPtr<QuotaRequestBase> CreateStorageInitializedOp();

RefPtr<QuotaRequestBase> CreateTemporaryStorageInitializedOp();

RefPtr<QuotaRequestBase> CreateInitOp();

RefPtr<QuotaRequestBase> CreateInitTemporaryStorageOp();

RefPtr<QuotaRequestBase> CreateInitializePersistentOriginOp(
    const RequestParams& aParams);

RefPtr<QuotaRequestBase> CreateInitializeTemporaryOriginOp(
    const RequestParams& aParams);

RefPtr<QuotaRequestBase> CreateGetFullOriginMetadataOp(
    const GetFullOriginMetadataParams& aParams);

RefPtr<QuotaRequestBase> CreateResetOrClearOp(bool aClear);

RefPtr<QuotaRequestBase> CreateClearOriginOp(const RequestParams& aParams);

RefPtr<QuotaRequestBase> CreateClearDataOp(const RequestParams& aParams);

RefPtr<QuotaRequestBase> CreateResetOriginOp(const RequestParams& aParams);

RefPtr<QuotaRequestBase> CreatePersistedOp(const RequestParams& aParams);

RefPtr<QuotaRequestBase> CreatePersistOp(const RequestParams& aParams);

RefPtr<QuotaRequestBase> CreateEstimateOp(const EstimateParams& aParams);

RefPtr<QuotaRequestBase> CreateListOriginsOp();

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_ORIGINOPERATIONS_H_

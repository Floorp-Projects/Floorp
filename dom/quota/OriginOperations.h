/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_ORIGINOPERATIONS_H_
#define DOM_QUOTA_ORIGINOPERATIONS_H_

#include <cstdint>

#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "nsTArrayForwardDeclare.h"

template <class T>
class RefPtr;

namespace mozilla {

template <class T>
class Maybe;
template <typename T>
class MovingNotNull;
class OriginAttributesPattern;

namespace dom::quota {

class EstimateParams;
class GetFullOriginMetadataParams;
class NormalOriginOperationBase;
class OriginDirectoryLock;
struct OriginMetadata;
class OriginOperationBase;
class QuotaManager;
class QuotaRequestBase;
class QuotaUsageRequestBase;
class RequestParams;
template <typename T>
class ResolvableNormalOriginOp;
class UniversalDirectoryLock;
class UsageRequestParams;

RefPtr<OriginOperationBase> CreateFinalizeOriginEvictionOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    nsTArray<RefPtr<OriginDirectoryLock>>&& aLocks);

RefPtr<NormalOriginOperationBase> CreateSaveOriginAccessTimeOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const OriginMetadata& aOriginMetadata, int64_t aTimestamp);

RefPtr<ResolvableNormalOriginOp<bool>> CreateClearPrivateRepositoryOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager);

RefPtr<ResolvableNormalOriginOp<bool>> CreateShutdownStorageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager);

RefPtr<QuotaUsageRequestBase> CreateGetUsageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const UsageRequestParams& aParams);

RefPtr<QuotaUsageRequestBase> CreateGetOriginUsageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const UsageRequestParams& aParams);

RefPtr<QuotaRequestBase> CreateStorageNameOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager);

RefPtr<ResolvableNormalOriginOp<bool>> CreateStorageInitializedOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager);

RefPtr<ResolvableNormalOriginOp<bool>> CreateTemporaryStorageInitializedOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager);

RefPtr<ResolvableNormalOriginOp<bool>> CreateInitOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    RefPtr<UniversalDirectoryLock> aDirectoryLock);

RefPtr<ResolvableNormalOriginOp<bool>> CreateInitTemporaryStorageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    RefPtr<UniversalDirectoryLock> aDirectoryLock);

RefPtr<QuotaRequestBase> CreateInitializePersistentOriginOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const RequestParams& aParams);

RefPtr<QuotaRequestBase> CreateInitializeTemporaryOriginOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const RequestParams& aParams);

RefPtr<ResolvableNormalOriginOp<bool>> CreateInitializePersistentClientOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
    const Client::Type aClientType);

RefPtr<ResolvableNormalOriginOp<bool>> CreateInitializeTemporaryClientOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const PersistenceType aPersistenceType,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
    const Client::Type aClientType);

RefPtr<QuotaRequestBase> CreateGetFullOriginMetadataOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const GetFullOriginMetadataParams& aParams);

RefPtr<ResolvableNormalOriginOp<bool>> CreateClearStorageOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager);

RefPtr<ResolvableNormalOriginOp<bool>> CreateClearOriginOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const Maybe<PersistenceType>& aPersistenceType,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
    const Maybe<Client::Type>& aClientType);

RefPtr<ResolvableNormalOriginOp<bool>> CreateClearStoragesForOriginPrefixOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const Maybe<PersistenceType>& aPersistenceType,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo);

RefPtr<ResolvableNormalOriginOp<bool>> CreateClearDataOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const OriginAttributesPattern& aPattern);

RefPtr<QuotaRequestBase> CreateResetOriginOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const RequestParams& aParams);

RefPtr<QuotaRequestBase> CreatePersistedOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const RequestParams& aParams);

RefPtr<QuotaRequestBase> CreatePersistOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const RequestParams& aParams);

RefPtr<QuotaRequestBase> CreateEstimateOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const EstimateParams& aParams);

RefPtr<QuotaRequestBase> CreateListOriginsOp(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager);

}  // namespace dom::quota
}  // namespace mozilla

#endif  // DOM_QUOTA_ORIGINOPERATIONS_H_

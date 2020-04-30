/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RefMessageBodyService.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

StaticMutex sRefMessageBodyServiceMutex;

// Raw pointer because the service is kept alive by other objects.
// See the CTOR and the DTOR of this object.
RefMessageBodyService* sService;

// static
already_AddRefed<RefMessageBodyService> RefMessageBodyService::GetOrCreate() {
  StaticMutexAutoLock lock(sRefMessageBodyServiceMutex);

  RefPtr<RefMessageBodyService> service = GetOrCreateInternal(lock);
  return service.forget();
}

// static
RefMessageBodyService* RefMessageBodyService::GetOrCreateInternal(
    const StaticMutexAutoLock& aProofOfLock) {
  if (!sService) {
    sService = new RefMessageBodyService();
  }
  return sService;
}

RefMessageBodyService::RefMessageBodyService() {
  MOZ_DIAGNOSTIC_ASSERT(sService == nullptr);
}

RefMessageBodyService::~RefMessageBodyService() {
  MOZ_DIAGNOSTIC_ASSERT(sService == this);
  sService = nullptr;
}

const nsID RefMessageBodyService::Register(
    already_AddRefed<RefMessageBody> aBody, ErrorResult& aRv) {
  RefPtr<RefMessageBody> body = aBody;
  MOZ_ASSERT(body);

  nsID uuid = {};
  aRv = nsContentUtils::GenerateUUIDInPlace(uuid);
  if (NS_WARN_IF(aRv.Failed())) {
    return nsID();
  }

  StaticMutexAutoLock lock(sRefMessageBodyServiceMutex);
  GetOrCreateInternal(lock)->mMessages.Put(uuid, std::move(body));
  return uuid;
}

already_AddRefed<RefMessageBody> RefMessageBodyService::Steal(const nsID& aID) {
  StaticMutexAutoLock lock(sRefMessageBodyServiceMutex);
  if (!sService) {
    return nullptr;
  }

  RefPtr<RefMessageBody> body;
  sService->mMessages.Remove(aID, getter_AddRefs(body));

  return body.forget();
}

already_AddRefed<RefMessageBody> RefMessageBodyService::GetAndCount(
    const nsID& aID) {
  StaticMutexAutoLock lock(sRefMessageBodyServiceMutex);
  if (!sService) {
    return nullptr;
  }

  RefPtr<RefMessageBody> body = sService->mMessages.Get(aID);
  if (!body) {
    return nullptr;
  }

  ++body->mCount;

  MOZ_ASSERT_IF(body->mMaxCount.isSome(),
                body->mCount <= body->mMaxCount.value());
  if (body->mMaxCount.isSome() && body->mCount >= body->mMaxCount.value()) {
    sService->mMessages.Remove(aID);
  }

  return body.forget();
}

void RefMessageBodyService::SetMaxCount(const nsID& aID, uint32_t aMaxCount) {
  StaticMutexAutoLock lock(sRefMessageBodyServiceMutex);
  if (!sService) {
    return;
  }

  RefPtr<RefMessageBody> body = sService->mMessages.Get(aID);
  if (!body) {
    return;
  }

  MOZ_ASSERT(body->mMaxCount.isNothing());
  body->mMaxCount.emplace(aMaxCount);

  MOZ_ASSERT(body->mCount <= body->mMaxCount.value());
  if (body->mCount >= body->mMaxCount.value()) {
    sService->mMessages.Remove(aID);
  }
}

void RefMessageBodyService::ForgetPort(const nsID& aPortID) {
  StaticMutexAutoLock lock(sRefMessageBodyServiceMutex);
  if (!sService) {
    return;
  }

  for (auto iter = sService->mMessages.ConstIter(); !iter.Done(); iter.Next()) {
    if (iter.UserData()->PortID() == aPortID) {
      iter.Remove();
    }
  }
}

}  // namespace dom
}  // namespace mozilla

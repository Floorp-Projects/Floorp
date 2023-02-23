/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/JSValidatorChild.h"
#include "mozilla/ipc/Endpoint.h"

using namespace mozilla::dom;

mozilla::ipc::IPCResult JSValidatorChild::RecvIsOpaqueResponseAllowed(
    IsOpaqueResponseAllowedResolver&& aResolver) {
  mResolver.emplace(aResolver);

  return IPC_OK();
}

mozilla::ipc::IPCResult JSValidatorChild::RecvOnDataAvailable(Shmem&& aData) {
  if (!mResolver) {
    MOZ_ASSERT(!CanSend());
    return IPC_OK();
  }

  if (!mSourceBytes.Append(Span(aData.get<char>(), aData.Size<char>()),
                           mozilla::fallible)) {
    // To prevent an attacker from flood the validation process,
    // we don't validate here.
    Resolve(ValidatorResult::Failure);
  }
  DeallocShmem(aData);

  return IPC_OK();
}

mozilla::ipc::IPCResult JSValidatorChild::RecvOnStopRequest(
    const nsresult& aReason) {
  if (!mResolver) {
    return IPC_OK();
  }

  if (NS_FAILED(aReason)) {
    Resolve(ValidatorResult::Failure);
  } else {
    Resolve(ShouldAllowJS());
  }

  return IPC_OK();
}

void JSValidatorChild::ActorDestroy(ActorDestroyReason aReason) {
  if (mResolver) {
    Resolve(ValidatorResult::Failure);
  }
};

void JSValidatorChild::Resolve(ValidatorResult aResult) {
  MOZ_ASSERT(mResolver);
  Maybe<Shmem> data = Nothing();
  if (aResult == ValidatorResult::JavaScript && !mSourceBytes.IsEmpty()) {
    Shmem sharedData;
    nsresult rv =
        JSValidatorUtils::CopyCStringToShmem(this, mSourceBytes, sharedData);
    if (NS_SUCCEEDED(rv)) {
      data = Some(std::move(sharedData));
    }
  }

  mResolver.ref()(Tuple<mozilla::Maybe<Shmem>&&, const ValidatorResult&>(
      std::move(data), aResult));
  mResolver.reset();
}

JSValidatorChild::ValidatorResult JSValidatorChild::ShouldAllowJS() const {
  // mSourceBytes could be empty when
  // 1. No OnDataAvailable calls
  // 2. Failed to allocate shmem

  // The empty document parses as JavaScript, so for clarity we have a condition
  // separately for that.
  if (mSourceBytes.IsEmpty()) {
    return ValidatorResult::JavaScript;
  }

  if (StringBeginsWith(NS_ConvertUTF8toUTF16(mSourceBytes), u"{"_ns)) {
    return ValidatorResult::JSON;
  }

  return ValidatorResult::JavaScript;
}

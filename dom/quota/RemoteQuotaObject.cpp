/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteQuotaObject.h"

#include "mozilla/dom/quota/RemoteQuotaObjectChild.h"
#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla::dom::quota {

RemoteQuotaObject::RemoteQuotaObject(RefPtr<RemoteQuotaObjectChild> aActor)
    : QuotaObject(/* aIsRemote */ true), mActor(std::move(aActor)) {
  MOZ_COUNT_CTOR(RemoteQuotaObject);

  mActor->SetRemoteQuotaObject(this);
}

RemoteQuotaObject::~RemoteQuotaObject() {
  MOZ_COUNT_DTOR(RemoteQuotaObject);

  Close();
}

void RemoteQuotaObject::ClearActor() {
  MOZ_ASSERT(mActor);

  mActor = nullptr;
}

void RemoteQuotaObject::Close() {
  if (!mActor) {
    return;
  }

  MOZ_ASSERT(mActor->GetActorEventTarget()->IsOnCurrentThread());

  mActor->Close();
  MOZ_ASSERT(!mActor);
}

const nsAString& RemoteQuotaObject::Path() const { return EmptyString(); }

bool RemoteQuotaObject::MaybeUpdateSize(int64_t aSize, bool aTruncate) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mozilla::ipc::IsOnBackgroundThread());
  MOZ_ASSERT(!GetCurrentThreadWorkerPrivate());

  if (!mActor) {
    return false;
  }

  MOZ_ASSERT(mActor->GetActorEventTarget()->IsOnCurrentThread());

  bool result;
  if (!mActor->SendMaybeUpdateSize(aSize, aTruncate, &result)) {
    return false;
  }

  return result;
}

}  // namespace mozilla::dom::quota

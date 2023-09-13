/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaRequestBase.h"

#include "mozilla/dom/quota/PQuotaUsageRequest.h"
#include "mozilla/dom/quota/QuotaManager.h"

namespace mozilla::dom::quota {

void QuotaRequestBase::SendResults() {
  AssertIsOnOwningThread();

  if (IsActorDestroyed()) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = NS_ERROR_FAILURE;
    }
  } else {
    RequestResponse response;

    if (NS_SUCCEEDED(mResultCode)) {
      GetResponse(response);
    } else {
      response = mResultCode;
    }

    Unused << PQuotaRequestParent::Send__delete__(this, response);
  }
}

void QuotaRequestBase::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  NoteActorDestroyed();
}

}  // namespace mozilla::dom::quota

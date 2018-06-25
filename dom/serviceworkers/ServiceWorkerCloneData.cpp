/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerCloneData.h"

#include "nsProxyRelease.h"

namespace mozilla {
namespace dom {

ServiceWorkerCloneData::~ServiceWorkerCloneData()
{
  RefPtr<ipc::SharedJSAllocatedData> sharedData = TakeSharedData();
  if (sharedData) {
    NS_ProxyRelease(__func__, mEventTarget, sharedData.forget());
  }
}

ServiceWorkerCloneData::ServiceWorkerCloneData()
  : mEventTarget(GetCurrentThreadSerialEventTarget())
{
  MOZ_DIAGNOSTIC_ASSERT(mEventTarget);
}

} // namespace dom
} // namespace mozilla

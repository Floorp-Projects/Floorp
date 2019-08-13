/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchEventOpParent.h"

#include "nsDebug.h"

#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/FetchEventOpProxyParent.h"
#include "mozilla/dom/RemoteWorkerControllerParent.h"
#include "mozilla/dom/RemoteWorkerParent.h"
#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla {

using namespace ipc;

namespace dom {

void FetchEventOpParent::Initialize(
    const ServiceWorkerFetchEventOpArgs& aArgs) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  RemoteWorkerControllerParent* manager =
      static_cast<RemoteWorkerControllerParent*>(Manager());
  MOZ_ASSERT(manager);

  // This will be null when the manager's RemoteWorkerController has shutdown.
  RefPtr<RemoteWorkerParent> proxyManager = manager->GetRemoteWorkerParent();
  if (NS_WARN_IF(!proxyManager)) {
    Unused << Send__delete__(this, NS_ERROR_DOM_ABORT_ERR);

    return;
  }

  FetchEventOpProxyParent::Create(proxyManager.get(), aArgs, this);
}

void FetchEventOpParent::ActorDestroy(ActorDestroyReason) {
  AssertIsOnBackgroundThread();
}

}  // namespace dom
}  // namespace mozilla

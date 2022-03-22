/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BackgroundSessionStorageServiceParent.h"

#include "mozilla/dom/SessionStorageManager.h"
#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla::dom {

using namespace mozilla::ipc;

mozilla::ipc::IPCResult
BackgroundSessionStorageServiceParent::RecvClearStoragesForOrigin(
    const nsACString& aOriginAttrs, const nsACString& aOriginKey) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (!mozilla::dom::RecvClearStoragesForOrigin(aOriginAttrs, aOriginKey)) {
    return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

}  // namespace mozilla::dom

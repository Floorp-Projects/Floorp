/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BroadcastChannelParent.h"
#include "BroadcastChannelService.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/Unused.h"
#include "nsIScriptSecurityManager.h"

namespace mozilla {

using namespace ipc;

namespace dom {

BroadcastChannelParent::BroadcastChannelParent(const nsAString& aOriginChannelKey)
  : mService(BroadcastChannelService::GetOrCreate())
  , mOriginChannelKey(aOriginChannelKey)
{
  AssertIsOnBackgroundThread();
  mService->RegisterActor(this, mOriginChannelKey);
}

BroadcastChannelParent::~BroadcastChannelParent()
{
  AssertIsOnBackgroundThread();
}

mozilla::ipc::IPCResult
BroadcastChannelParent::RecvPostMessage(const ClonedMessageData& aData)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!mService)) {
    return IPC_FAIL_NO_REASON(this);
  }

  mService->PostMessage(this, aData, mOriginChannelKey);
  return IPC_OK();
}

mozilla::ipc::IPCResult
BroadcastChannelParent::RecvClose()
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!mService)) {
    return IPC_FAIL_NO_REASON(this);
  }

  mService->UnregisterActor(this, mOriginChannelKey);
  mService = nullptr;

  Unused << Send__delete__(this);

  return IPC_OK();
}

void
BroadcastChannelParent::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnBackgroundThread();

  if (mService) {
    // This object is about to be released and with it, also mService will be
    // released too.
    mService->UnregisterActor(this, mOriginChannelKey);
  }
}

} // namespace dom
} // namespace mozilla

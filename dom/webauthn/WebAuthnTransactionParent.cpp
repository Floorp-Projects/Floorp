/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthnTransactionParent.h"
#include "mozilla/dom/U2FTokenManager.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla {
namespace dom {

mozilla::ipc::IPCResult
WebAuthnTransactionParent::RecvRequestRegister(const uint64_t& aTransactionId,
                                               const WebAuthnMakeCredentialInfo& aTransactionInfo)
{
  AssertIsOnBackgroundThread();
  U2FTokenManager* mgr = U2FTokenManager::Get();
  mgr->Register(this, aTransactionId, aTransactionInfo);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebAuthnTransactionParent::RecvRequestSign(const uint64_t& aTransactionId,
                                           const WebAuthnGetAssertionInfo& aTransactionInfo)
{
  AssertIsOnBackgroundThread();
  U2FTokenManager* mgr = U2FTokenManager::Get();
  mgr->Sign(this, aTransactionId, aTransactionInfo);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebAuthnTransactionParent::RecvRequestCancel(const uint64_t& aTransactionId)
{
  AssertIsOnBackgroundThread();
  U2FTokenManager* mgr = U2FTokenManager::Get();
  mgr->Cancel(this, aTransactionId);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebAuthnTransactionParent::RecvDestroyMe()
{
  AssertIsOnBackgroundThread();

  // The child was disconnected from the WebAuthnManager instance and will send
  // no further messages. It is kept alive until we delete it explicitly.

  // The child should have cancelled any active transaction. This means
  // we expect no more messages to the child. We'll crash otherwise.

  // The IPC roundtrip is complete. No more messages, hopefully.
  IProtocol* mgr = Manager();
  if (!Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }

  return IPC_OK();
}

void
WebAuthnTransactionParent::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnBackgroundThread();

  // Called either by Send__delete__() in RecvDestroyMe() above, or when
  // the channel disconnects. Ensure the token manager forgets about us.
  U2FTokenManager* mgr = U2FTokenManager::Get();

  // The manager could probably be null on shutdown?
  if (mgr) {
    mgr->MaybeClearTransaction(this);
  }
}

}
}

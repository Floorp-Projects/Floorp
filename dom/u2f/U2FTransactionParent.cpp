/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "U2FTransactionParent.h"
#include "mozilla/dom/U2FTokenManager.h"
#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla {
namespace dom {

mozilla::ipc::IPCResult
U2FTransactionParent::RecvRequestRegister(const uint64_t& aTransactionId,
                                          const WebAuthnMakeCredentialInfo& aTransactionInfo)
{
  AssertIsOnBackgroundThread();
  U2FTokenManager* mgr = U2FTokenManager::Get();
  mgr->Register(this, aTransactionId, aTransactionInfo);
  return IPC_OK();
}

mozilla::ipc::IPCResult
U2FTransactionParent::RecvRequestSign(const uint64_t& aTransactionId,
                                      const WebAuthnGetAssertionInfo& aTransactionInfo)
{
  AssertIsOnBackgroundThread();
  U2FTokenManager* mgr = U2FTokenManager::Get();
  mgr->Sign(this, aTransactionId, aTransactionInfo);
  return IPC_OK();
}

mozilla::ipc::IPCResult
U2FTransactionParent::RecvRequestCancel(const uint64_t& aTransactionId)
{
  AssertIsOnBackgroundThread();
  U2FTokenManager* mgr = U2FTokenManager::Get();
  mgr->Cancel(this, aTransactionId);
  return IPC_OK();
}

void
U2FTransactionParent::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnBackgroundThread();
  U2FTokenManager* mgr = U2FTokenManager::Get();
  mgr->MaybeClearTransaction(this);
}

} // namespace dom
} // namespace mozilla

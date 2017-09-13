/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/U2FTokenManager.h"
#include "U2FTransactionParent.h"

namespace mozilla {
namespace dom {

mozilla::ipc::IPCResult
U2FTransactionParent::RecvRequestRegister(const WebAuthnTransactionInfo& aTransactionInfo)
{
  U2FTokenManager* mgr = U2FTokenManager::Get();
  mgr->Register(this, aTransactionInfo);
  return IPC_OK();
}

mozilla::ipc::IPCResult
U2FTransactionParent::RecvRequestSign(const WebAuthnTransactionInfo& aTransactionInfo)
{
  U2FTokenManager* mgr = U2FTokenManager::Get();
  mgr->Sign(this, aTransactionInfo);
  return IPC_OK();
}

mozilla::ipc::IPCResult
U2FTransactionParent::RecvRequestCancel()
{
  U2FTokenManager* mgr = U2FTokenManager::Get();
  mgr->Cancel(this);
  return IPC_OK();
}

void
U2FTransactionParent::ActorDestroy(ActorDestroyReason aWhy)
{
  U2FTokenManager* mgr = U2FTokenManager::Get();
  mgr->MaybeClearTransaction(this);
}

} // namespace dom
} // namespace mozilla

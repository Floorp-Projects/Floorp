/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthnTransactionParent.h"
#include "mozilla/dom/U2FTokenManager.h"

namespace mozilla {
namespace dom {

mozilla::ipc::IPCResult
WebAuthnTransactionParent::RecvRequestRegister(const WebAuthnTransactionInfo& aTransactionInfo)
{
  U2FTokenManager* mgr = U2FTokenManager::Get();
  // Cast away const here since NSS wants to be able to use non-const functions
  mgr->Register(this, const_cast<WebAuthnTransactionInfo&>(aTransactionInfo));
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebAuthnTransactionParent::RecvRequestSign(const WebAuthnTransactionInfo& aTransactionInfo)
{
  U2FTokenManager* mgr = U2FTokenManager::Get();
  // Cast away const here since NSS wants to be able to use non-const functions
  mgr->Sign(this, const_cast<WebAuthnTransactionInfo&>(aTransactionInfo));
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebAuthnTransactionParent::RecvRequestCancel()
{
  U2FTokenManager* mgr = U2FTokenManager::Get();
  mgr->MaybeClearTransaction(this);
  return IPC_OK();
}

void
WebAuthnTransactionParent::ActorDestroy(ActorDestroyReason aWhy)
{
  U2FTokenManager* mgr = U2FTokenManager::Get();
  mgr->MaybeClearTransaction(this);
}
}
}

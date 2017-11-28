/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "U2FTransactionChild.h"

namespace mozilla {
namespace dom {

mozilla::ipc::IPCResult
U2FTransactionChild::RecvConfirmRegister(const uint64_t& aTransactionId,
                                         nsTArray<uint8_t>&& aRegBuffer)
{
  mU2F->FinishRegister(aTransactionId, aRegBuffer);
  return IPC_OK();
}

mozilla::ipc::IPCResult
U2FTransactionChild::RecvConfirmSign(const uint64_t& aTransactionId,
                                     nsTArray<uint8_t>&& aCredentialId,
                                     nsTArray<uint8_t>&& aBuffer)
{
  mU2F->FinishSign(aTransactionId, aCredentialId, aBuffer);
  return IPC_OK();
}

mozilla::ipc::IPCResult
U2FTransactionChild::RecvAbort(const uint64_t& aTransactionId,
                               const nsresult& aError)
{
  mU2F->RequestAborted(aTransactionId, aError);
  return IPC_OK();
}

void
U2FTransactionChild::ActorDestroy(ActorDestroyReason why)
{
  mU2F->ActorDestroyed();
}

} // namespace dom
} // namespace mozilla

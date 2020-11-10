/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthnTransactionParent.h"
#include "mozilla/dom/U2FTokenManager.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/BackgroundParent.h"

#ifdef OS_WIN
#  include "WinWebAuthnManager.h"
#endif

namespace mozilla {
namespace dom {

mozilla::ipc::IPCResult WebAuthnTransactionParent::RecvRequestRegister(
    const uint64_t& aTransactionId,
    const WebAuthnMakeCredentialInfo& aTransactionInfo) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

#ifdef OS_WIN
  if (WinWebAuthnManager::AreWebAuthNApisAvailable()) {
    WinWebAuthnManager* mgr = WinWebAuthnManager::Get();
    mgr->Register(this, aTransactionId, aTransactionInfo);
  } else {
    U2FTokenManager* mgr = U2FTokenManager::Get();
    mgr->Register(this, aTransactionId, aTransactionInfo);
  }
#else
  U2FTokenManager* mgr = U2FTokenManager::Get();
  mgr->Register(this, aTransactionId, aTransactionInfo);
#endif

  return IPC_OK();
}

mozilla::ipc::IPCResult WebAuthnTransactionParent::RecvRequestSign(
    const uint64_t& aTransactionId,
    const WebAuthnGetAssertionInfo& aTransactionInfo) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

#ifdef OS_WIN
  if (WinWebAuthnManager::AreWebAuthNApisAvailable()) {
    WinWebAuthnManager* mgr = WinWebAuthnManager::Get();
    mgr->Sign(this, aTransactionId, aTransactionInfo);
  } else {
    U2FTokenManager* mgr = U2FTokenManager::Get();
    mgr->Sign(this, aTransactionId, aTransactionInfo);
  }
#else
  U2FTokenManager* mgr = U2FTokenManager::Get();
  mgr->Sign(this, aTransactionId, aTransactionInfo);
#endif

  return IPC_OK();
}

mozilla::ipc::IPCResult WebAuthnTransactionParent::RecvRequestCancel(
    const uint64_t& aTransactionId) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

#ifdef OS_WIN
  if (WinWebAuthnManager::AreWebAuthNApisAvailable()) {
    WinWebAuthnManager* mgr = WinWebAuthnManager::Get();
    mgr->Cancel(this, aTransactionId);
  } else {
    U2FTokenManager* mgr = U2FTokenManager::Get();
    mgr->Cancel(this, aTransactionId);
  }
#else
  U2FTokenManager* mgr = U2FTokenManager::Get();
  mgr->Cancel(this, aTransactionId);
#endif

  return IPC_OK();
}

mozilla::ipc::IPCResult WebAuthnTransactionParent::RecvDestroyMe() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

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

void WebAuthnTransactionParent::ActorDestroy(ActorDestroyReason aWhy) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  // Called either by Send__delete__() in RecvDestroyMe() above, or when
  // the channel disconnects. Ensure the token manager forgets about us.

#ifdef OS_WIN
  if (WinWebAuthnManager::AreWebAuthNApisAvailable()) {
    WinWebAuthnManager* mgr = WinWebAuthnManager::Get();
    if (mgr) {
      mgr->MaybeClearTransaction(this);
    }
  } else {
    U2FTokenManager* mgr = U2FTokenManager::Get();
    if (mgr) {
      mgr->MaybeClearTransaction(this);
    }
  }
#else
  U2FTokenManager* mgr = U2FTokenManager::Get();
  if (mgr) {
    mgr->MaybeClearTransaction(this);
  }
#endif
}

}  // namespace dom
}  // namespace mozilla

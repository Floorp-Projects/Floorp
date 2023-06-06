/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthnTransactionParent.h"
#include "mozilla/dom/WebAuthnController.h"
#include "mozilla/dom/U2FTokenManager.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/StaticPrefs_security.h"

#include "nsThreadUtils.h"

#ifdef XP_WIN
#  include "WinWebAuthnManager.h"
#endif

namespace mozilla::dom {

mozilla::ipc::IPCResult WebAuthnTransactionParent::RecvRequestRegister(
    const uint64_t& aTransactionId,
    const WebAuthnMakeCredentialInfo& aTransactionInfo) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

#ifdef XP_WIN
  if (WinWebAuthnManager::AreWebAuthNApisAvailable()) {
    WinWebAuthnManager* mgr = WinWebAuthnManager::Get();
    mgr->Register(this, aTransactionId, aTransactionInfo);
    return IPC_OK();
  }
#endif

  bool androidFido2 =
      StaticPrefs::security_webauth_webauthn_enable_android_fido2();

  if (!androidFido2) {
    WebAuthnController* ctrl = WebAuthnController::Get();
    ctrl->Register(this, aTransactionId, aTransactionInfo);
  } else {
    U2FTokenManager* mgr = U2FTokenManager::Get();
    mgr->Register(this, aTransactionId, aTransactionInfo);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult WebAuthnTransactionParent::RecvRequestSign(
    const uint64_t& aTransactionId,
    const WebAuthnGetAssertionInfo& aTransactionInfo) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

#ifdef XP_WIN
  if (WinWebAuthnManager::AreWebAuthNApisAvailable()) {
    WinWebAuthnManager* mgr = WinWebAuthnManager::Get();
    mgr->Sign(this, aTransactionId, aTransactionInfo);
    return IPC_OK();
  }
#endif

  bool androidFido2 =
      StaticPrefs::security_webauth_webauthn_enable_android_fido2();

  if (!androidFido2) {
    WebAuthnController* ctrl = WebAuthnController::Get();
    ctrl->Sign(this, aTransactionId, aTransactionInfo);
  } else {
    U2FTokenManager* mgr = U2FTokenManager::Get();
    mgr->Sign(this, aTransactionId, aTransactionInfo);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult WebAuthnTransactionParent::RecvRequestCancel(
    const Tainted<uint64_t>& aTransactionId) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

#ifdef XP_WIN
  if (WinWebAuthnManager::AreWebAuthNApisAvailable()) {
    WinWebAuthnManager* mgr = WinWebAuthnManager::Get();
    mgr->Cancel(this, aTransactionId);
    return IPC_OK();
  }
#endif

  // We don't know whether WebAuthnController or U2FTokenManager was used, so
  // try cancelling both.
  WebAuthnController* ctrl = WebAuthnController::Get();
  if (ctrl) {
    ctrl->Cancel(this, aTransactionId);
  }

  U2FTokenManager* mgr = U2FTokenManager::Get();
  if (mgr) {
    mgr->Cancel(this, aTransactionId);
  }

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

#ifdef XP_WIN
  if (WinWebAuthnManager::AreWebAuthNApisAvailable()) {
    WinWebAuthnManager* mgr = WinWebAuthnManager::Get();
    if (mgr) {
      mgr->MaybeClearTransaction(this);
    }
    return;
  }
#endif

  WebAuthnController* ctrl = WebAuthnController::Get();
  if (ctrl) {
    ctrl->MaybeClearTransaction(this);
  }

  U2FTokenManager* mgr = U2FTokenManager::Get();
  if (mgr) {
    mgr->MaybeClearTransaction(this);
  }
}

}  // namespace mozilla::dom

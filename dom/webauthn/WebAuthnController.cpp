/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "json/json.h"
#include "mozilla/dom/WebAuthnController.h"
#include "mozilla/dom/PWebAuthnTransactionParent.h"
#include "mozilla/dom/WebAuthnUtil.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_security.h"
#include "mozilla/Unused.h"
#include "nsEscape.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIThread.h"
#include "nsServiceManagerUtils.h"
#include "nsTextFormatter.h"
#include "mozilla/Telemetry.h"

#include "AuthrsTransport.h"
#include "CtapArgs.h"
#include "WebAuthnEnumStrings.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/dom/AndroidWebAuthnTokenManager.h"
#endif

namespace mozilla::dom {

/***********************************************************************
 * Statics
 **********************************************************************/

namespace {
static mozilla::LazyLogModule gWebAuthnControllerLog("webauthncontroller");
StaticRefPtr<WebAuthnController> gWebAuthnController;
static nsIThread* gWebAuthnBackgroundThread;
}  // namespace

/***********************************************************************
 * WebAuthnController Implementation
 **********************************************************************/

NS_IMPL_ISUPPORTS(WebAuthnController, nsIWebAuthnController);

WebAuthnController::WebAuthnController() : mTransactionParent(nullptr) {
  MOZ_ASSERT(XRE_IsParentProcess());
  // Create on the main thread to make sure ClearOnShutdown() works.
  MOZ_ASSERT(NS_IsMainThread());
}

// static
void WebAuthnController::Initialize() {
  if (!XRE_IsParentProcess()) {
    return;
  }
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!gWebAuthnController);
  gWebAuthnController = new WebAuthnController();
  ClearOnShutdown(&gWebAuthnController);
}

// static
WebAuthnController* WebAuthnController::Get() {
  MOZ_ASSERT(XRE_IsParentProcess());
  mozilla::ipc::AssertIsOnBackgroundThread();
  return gWebAuthnController;
}

void WebAuthnController::AbortTransaction(
    const nsresult& aError = NS_ERROR_DOM_NOT_ALLOWED_ERR) {
  MOZ_ASSERT(XRE_IsParentProcess());
  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_LOG(gWebAuthnControllerLog, LogLevel::Debug,
          ("WebAuthnController::AbortTransaction"));
  if (mTransactionParent && mTransactionId.isSome()) {
    Unused << mTransactionParent->SendAbort(mTransactionId.ref(), aError);
  }
  ClearTransaction();
}

void WebAuthnController::MaybeClearTransaction(
    PWebAuthnTransactionParent* aParent) {
  MOZ_ASSERT(XRE_IsParentProcess());
  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_LOG(gWebAuthnControllerLog, LogLevel::Debug,
          ("WebAuthnController::MaybeClearTransaction"));
  // Only clear if we've been requested to do so by our current transaction
  // parent.
  if (mTransactionParent == aParent) {
    ClearTransaction();
  }
}

void WebAuthnController::ClearTransaction() {
  MOZ_ASSERT(XRE_IsParentProcess());
  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_LOG(gWebAuthnControllerLog, LogLevel::Debug,
          ("WebAuthnController::ClearTransaction"));
  mTransactionParent = nullptr;
  mPendingClientData.reset();
  mTransactionId.reset();
}

nsCOMPtr<nsIWebAuthnTransport> WebAuthnController::GetTransportImpl() {
  MOZ_ASSERT(XRE_IsParentProcess());
  mozilla::ipc::AssertIsOnBackgroundThread();

  if (mTransportImpl) {
    return mTransportImpl;
  }

  nsCOMPtr<nsIWebAuthnTransport> transport(
      do_GetService("@mozilla.org/webauthn/transport;1"));
  transport->SetController(this);
  return transport;
}

void WebAuthnController::Register(
    PWebAuthnTransactionParent* aTransactionParent,
    const uint64_t& aTransactionId, const WebAuthnMakeCredentialInfo& aInfo) {
  MOZ_ASSERT(XRE_IsParentProcess());
  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_LOG(gWebAuthnControllerLog, LogLevel::Debug,
          ("WebAuthnController::Register"));

  if (!gWebAuthnBackgroundThread) {
    gWebAuthnBackgroundThread = NS_GetCurrentThread();
    MOZ_ASSERT(gWebAuthnBackgroundThread, "This should never be null!");
  }

  // Abort ongoing transaction, if any.
  AbortTransaction(NS_ERROR_DOM_ABORT_ERR);

  mTransportImpl = GetTransportImpl();
  if (!mTransportImpl) {
    AbortTransaction();
    return;
  }

  nsresult rv = mTransportImpl->Reset();
  if (NS_FAILED(rv)) {
    AbortTransaction();
    return;
  }

  MOZ_ASSERT(aTransactionId > 0);
  mTransactionParent = aTransactionParent;
  mTransactionId = Some(aTransactionId);
  mPendingClientData = Some(aInfo.ClientDataJSON());

  RefPtr<CtapRegisterArgs> args(new CtapRegisterArgs(aInfo));
  rv = mTransportImpl->MakeCredential(mTransactionId.ref(),
                                      aInfo.BrowsingContextId(), args);
  if (NS_FAILED(rv)) {
    AbortTransaction();
    return;
  }
}

NS_IMETHODIMP
WebAuthnController::FinishRegister(uint64_t aTransactionId,
                                   nsICtapRegisterResult* aResult) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_LOG(gWebAuthnControllerLog, LogLevel::Debug,
          ("WebAuthnController::FinishRegister"));
  nsCOMPtr<nsIRunnable> r(
      NewRunnableMethod<uint64_t, RefPtr<nsICtapRegisterResult>>(
          "WebAuthnController::RunFinishRegister", this,
          &WebAuthnController::RunFinishRegister, aTransactionId, aResult));

  if (!gWebAuthnBackgroundThread) {
    return NS_ERROR_FAILURE;
  }
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdownThreads)) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }
  return gWebAuthnBackgroundThread->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void WebAuthnController::RunFinishRegister(
    uint64_t aTransactionId, const RefPtr<nsICtapRegisterResult>& aResult) {
  MOZ_ASSERT(XRE_IsParentProcess());
  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_LOG(gWebAuthnControllerLog, LogLevel::Debug,
          ("WebAuthnController::RunFinishRegister"));
  if (mTransactionId.isNothing() || mPendingClientData.isNothing() ||
      aTransactionId != mTransactionId.ref()) {
    // The previous transaction was likely cancelled from the prompt.
    return;
  }

  nsresult status;
  nsresult rv = aResult->GetStatus(&status);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    AbortTransaction();
    return;
  }
  if (NS_FAILED(status)) {
    Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                         u"CTAPRegisterAbort"_ns, 1);
    AbortTransaction(status);
    return;
  }

  nsTArray<uint8_t> attObj;
  rv = aResult->GetAttestationObject(attObj);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    AbortTransaction();
    return;
  }

  nsTArray<uint8_t> credentialId;
  rv = aResult->GetCredentialId(credentialId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    AbortTransaction();
    return;
  }

  nsTArray<nsString> transports;
  rv = aResult->GetTransports(transports);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    AbortTransaction();
    return;
  }

  nsTArray<WebAuthnExtensionResult> extensions;
  bool credPropsRk;
  rv = aResult->GetCredPropsRk(&credPropsRk);
  if (rv != NS_ERROR_NOT_AVAILABLE) {
    if (NS_WARN_IF(NS_FAILED(rv))) {
      AbortTransaction();
      return;
    }
    extensions.AppendElement(WebAuthnExtensionResultCredProps(credPropsRk));
  }

  WebAuthnMakeCredentialResult result(mPendingClientData.extract(), attObj,
                                      credentialId, transports, extensions);

  Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                       u"CTAPRegisterFinish"_ns, 1);
  Unused << mTransactionParent->SendConfirmRegister(aTransactionId, result);
  ClearTransaction();
}

void WebAuthnController::Sign(PWebAuthnTransactionParent* aTransactionParent,
                              const uint64_t& aTransactionId,
                              const WebAuthnGetAssertionInfo& aInfo) {
  MOZ_ASSERT(XRE_IsParentProcess());
  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_LOG(gWebAuthnControllerLog, LogLevel::Debug,
          ("WebAuthnController::Sign"));
  MOZ_ASSERT(aTransactionId > 0);

  if (!gWebAuthnBackgroundThread) {
    gWebAuthnBackgroundThread = NS_GetCurrentThread();
    MOZ_ASSERT(gWebAuthnBackgroundThread, "This should never be null!");
  }

  // Abort ongoing transaction, if any.
  AbortTransaction(NS_ERROR_DOM_ABORT_ERR);

  mTransportImpl = GetTransportImpl();
  if (!mTransportImpl) {
    AbortTransaction();
    return;
  }

  nsresult rv = mTransportImpl->Reset();
  if (NS_FAILED(rv)) {
    AbortTransaction();
    return;
  }

  mTransactionParent = aTransactionParent;
  mTransactionId = Some(aTransactionId);
  mPendingClientData = Some(aInfo.ClientDataJSON());

  RefPtr<CtapSignArgs> args(new CtapSignArgs(aInfo));
  rv = mTransportImpl->GetAssertion(mTransactionId.ref(),
                                    aInfo.BrowsingContextId(), args.get());
  if (NS_FAILED(rv)) {
    AbortTransaction();
    return;
  }
}

NS_IMETHODIMP
WebAuthnController::FinishSign(uint64_t aTransactionId,
                               nsICtapSignResult* aResult) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_LOG(gWebAuthnControllerLog, LogLevel::Debug,
          ("WebAuthnController::FinishSign"));
  nsCOMPtr<nsIRunnable> r(
      NewRunnableMethod<uint64_t, RefPtr<nsICtapSignResult>>(
          "WebAuthnController::RunFinishSign", this,
          &WebAuthnController::RunFinishSign, aTransactionId, aResult));

  if (!gWebAuthnBackgroundThread) {
    return NS_ERROR_FAILURE;
  }
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdownThreads)) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }
  return gWebAuthnBackgroundThread->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void WebAuthnController::RunFinishSign(
    uint64_t aTransactionId, const RefPtr<nsICtapSignResult>& aResult) {
  MOZ_ASSERT(XRE_IsParentProcess());
  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_LOG(gWebAuthnControllerLog, LogLevel::Debug,
          ("WebAuthnController::RunFinishSign"));
  if (mTransactionId.isNothing() || mPendingClientData.isNothing() ||
      aTransactionId != mTransactionId.ref()) {
    return;
  }

  nsresult status;
  nsresult rv = aResult->GetStatus(&status);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    AbortTransaction();
    return;
  }
  if (NS_FAILED(status)) {
    Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                         u"CTAPSignAbort"_ns, 1);
    AbortTransaction(status);
    return;
  }

  nsTArray<uint8_t> credentialId;
  rv = aResult->GetCredentialId(credentialId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    AbortTransaction();
    return;
  }

  nsTArray<uint8_t> signature;
  rv = aResult->GetSignature(signature);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    AbortTransaction();
    return;
  }

  nsTArray<uint8_t> authenticatorData;
  rv = aResult->GetAuthenticatorData(authenticatorData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    AbortTransaction();
    return;
  }

  nsTArray<uint8_t> userHandle;
  Unused << aResult->GetUserHandle(userHandle);  // optional

  nsTArray<WebAuthnExtensionResult> extensions;
  bool usedAppId;
  rv = aResult->GetUsedAppId(&usedAppId);
  if (rv != NS_ERROR_NOT_AVAILABLE) {
    if (NS_FAILED(rv)) {
      AbortTransaction();
      return;
    }
    extensions.AppendElement(WebAuthnExtensionResultAppId(usedAppId));
  }

  WebAuthnGetAssertionResult result(mPendingClientData.extract(), credentialId,
                                    signature, authenticatorData, extensions,
                                    userHandle);

  Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                       u"CTAPSignFinish"_ns, 1);
  Unused << mTransactionParent->SendConfirmSign(aTransactionId, result);
  ClearTransaction();
}

void WebAuthnController::Cancel(PWebAuthnTransactionParent* aTransactionParent,
                                const Tainted<uint64_t>& aTransactionId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_LOG(gWebAuthnControllerLog, LogLevel::Debug,
          ("WebAuthnController::Cancel (IPC)"));
  // The last transaction ID also suffers from the issue described in Bug
  // 1696159. A content process could cancel another content processes
  // transaction by guessing the last transaction ID.
  if (mTransactionParent != aTransactionParent || mTransactionId.isNothing() ||
      !MOZ_IS_VALID(aTransactionId, mTransactionId.ref() == aTransactionId)) {
    return;
  }

  if (mTransportImpl) {
    mTransportImpl->Reset();
  }

  ClearTransaction();
}

NS_IMETHODIMP
WebAuthnController::Cancel(uint64_t aTransactionId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_LOG(gWebAuthnControllerLog, LogLevel::Debug,
          ("WebAuthnController::Cancel (XPCOM)"));

  nsCOMPtr<nsIRunnable> r(NewRunnableMethod<uint64_t>(
      "WebAuthnController::Cancel", this, &WebAuthnController::RunCancel,
      aTransactionId));

  if (!gWebAuthnBackgroundThread) {
    return NS_ERROR_FAILURE;
  }
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdownThreads)) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }
  return gWebAuthnBackgroundThread->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void WebAuthnController::RunCancel(uint64_t aTransactionId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_LOG(gWebAuthnControllerLog, LogLevel::Debug,
          ("WebAuthnController::RunCancel (XPCOM)"));

  if (mTransactionId.isNothing() || mTransactionId.ref() != aTransactionId) {
    return;
  }

  AbortTransaction();
}

}  // namespace mozilla::dom

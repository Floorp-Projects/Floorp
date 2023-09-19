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

// Data for WebAuthn UI prompt notifications.
static const char16_t kPresencePromptNotification[] =
    u"{\"is_ctap2\":true,\"action\":\"presence\",\"tid\":%llu,"
    u"\"origin\":\"%s\",\"browsingContextId\":%llu}";
static const char16_t kRegisterDirectPromptNotification[] =
    u"{\"is_ctap2\":true,\"action\":\"register-direct\",\"tid\":%llu,"
    u"\"origin\":\"%s\",\"browsingContextId\":%llu}";
static const char16_t kCancelPromptNotification[] =
    u"{\"is_ctap2\":true,\"action\":\"cancel\",\"tid\":%llu}";
static const char16_t kSelectSignResultNotification[] =
    u"{\"is_ctap2\":true,\"action\":\"select-sign-result\",\"tid\":%llu,"
    u"\"origin\":\"%s\",\"browsingContextId\":%llu,\"usernames\":[%s]}";

/***********************************************************************
 * U2FManager Implementation
 **********************************************************************/

NS_IMPL_ISUPPORTS(WebAuthnController, nsIWebAuthnController,
                  nsIU2FTokenManager);

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

void WebAuthnController::AbortTransaction(const uint64_t& aTransactionId,
                                          const nsresult& aError,
                                          bool shouldCancelActiveDialog) {
  if (mTransactionParent && mTransaction.isSome() && aTransactionId > 0 &&
      aTransactionId == mTransaction.ref().mTransactionId) {
    Unused << mTransactionParent->SendAbort(aTransactionId, aError);
    ClearTransaction(shouldCancelActiveDialog);
  }
}

void WebAuthnController::AbortOngoingTransaction() {
  if (mTransaction.isSome()) {
    AbortTransaction(mTransaction.ref().mTransactionId, NS_ERROR_DOM_ABORT_ERR,
                     true);
  }
}

void WebAuthnController::MaybeClearTransaction(
    PWebAuthnTransactionParent* aParent) {
  // Only clear if we've been requested to do so by our current transaction
  // parent.
  if (mTransactionParent == aParent) {
    ClearTransaction(true);
  }
}

void WebAuthnController::ClearTransaction(bool cancel_prompt) {
  if (cancel_prompt && mTransaction.isSome() &&
      mTransaction.ref().mTransactionId > 0) {
    // Remove any prompts we might be showing for the current transaction.
    SendPromptNotification(kCancelPromptNotification,
                           mTransaction.ref().mTransactionId);
  }
  mTransactionParent = nullptr;

  // Forget any pending registration.
  mPendingRegisterInfo.reset();
  mPendingSignInfo.reset();
  mPendingSignResults.Clear();
  mTransaction.reset();
}

template <typename... T>
void WebAuthnController::SendPromptNotification(const char16_t* aFormat,
                                                T... aArgs) {
  MOZ_ASSERT(!NS_IsMainThread());
  nsAutoString json;
  nsTextFormatter::ssprintf(json, aFormat, aArgs...);

  nsCOMPtr<nsIRunnable> r(NewRunnableMethod<nsString>(
      "WebAuthnController::RunSendPromptNotification", this,
      &WebAuthnController::RunSendPromptNotification, json));

  MOZ_ALWAYS_SUCCEEDS(GetMainThreadSerialEventTarget()->Dispatch(
      r.forget(), NS_DISPATCH_NORMAL));
}

NS_IMETHODIMP
WebAuthnController::SendPromptNotificationPreformatted(
    uint64_t aTransactionId, const nsACString& aJson) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsIRunnable> r(NewRunnableMethod<nsString>(
      "WebAuthnController::RunSendPromptNotification", this,
      &WebAuthnController::RunSendPromptNotification,
      NS_ConvertUTF8toUTF16(aJson)));
  MOZ_ALWAYS_SUCCEEDS(GetMainThreadSerialEventTarget()->Dispatch(
      r.forget(), NS_DISPATCH_NORMAL));
  return NS_OK;
}

void WebAuthnController::RunSendPromptNotification(const nsString& aJSON) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (NS_WARN_IF(!os)) {
    return;
  }

  nsCOMPtr<nsIWebAuthnController> self = this;
  MOZ_ALWAYS_SUCCEEDS(
      os->NotifyObservers(self, "webauthn-prompt", aJSON.get()));
}

nsCOMPtr<nsIWebAuthnTransport> WebAuthnController::GetTransportImpl() {
  mozilla::ipc::AssertIsOnBackgroundThread();

  if (mTransportImpl) {
    return mTransportImpl;
  }

  nsCOMPtr<nsIWebAuthnTransport> transport(
      do_GetService("@mozilla.org/webauthn/transport;1"));
  transport->SetController(this);
  return transport;
}

void WebAuthnController::Cancel(PWebAuthnTransactionParent* aTransactionParent,
                                const Tainted<uint64_t>& aTransactionId) {
  // The last transaction ID also suffers from the issue described in Bug
  // 1696159. A content process could cancel another content processes
  // transaction by guessing the last transaction ID.
  if (mTransactionParent != aTransactionParent || mTransaction.isNothing() ||
      !MOZ_IS_VALID(aTransactionId,
                    mTransaction.ref().mTransactionId == aTransactionId)) {
    return;
  }

  if (mTransportImpl) {
    mTransportImpl->Cancel();
  }

  ClearTransaction(true);
}

void WebAuthnController::Register(
    PWebAuthnTransactionParent* aTransactionParent,
    const uint64_t& aTransactionId, const WebAuthnMakeCredentialInfo& aInfo) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_LOG(gWebAuthnControllerLog, LogLevel::Debug,
          ("WebAuthnController::Register"));
  MOZ_ASSERT(aTransactionId > 0);

  if (!gWebAuthnBackgroundThread) {
    gWebAuthnBackgroundThread = NS_GetCurrentThread();
    MOZ_ASSERT(gWebAuthnBackgroundThread, "This should never be null!");
  }

  AbortOngoingTransaction();
  mTransactionParent = aTransactionParent;

  /* We could refactor to defer the hashing here */
  nsTArray<uint8_t> rpIdHash, clientDataHash;
  NS_ConvertUTF16toUTF8 rpId(aInfo.RpId());
  nsresult rv = BuildTransactionHashes(rpId, aInfo.ClientDataJSON(), rpIdHash,
                                       clientDataHash);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // We haven't set mTransaction yet, so we can't use AbortTransaction
    Unused << mTransactionParent->SendAbort(aTransactionId,
                                            NS_ERROR_DOM_NOT_ALLOWED_ERR);
    return;
  }

  // Hold on to any state that we need to finish the transaction.
  mTransaction = Some(
      Transaction(aTransactionId, rpIdHash, Nothing(), aInfo.ClientDataJSON()));

  MOZ_ASSERT(mPendingRegisterInfo.isNothing());
  mPendingRegisterInfo = Some(aInfo);

  // Determine whether direct attestation was requested.
  bool noneAttestationRequested = true;

// On Android, let's always reject direct attestations until we have a
// mechanism to solicit user consent, from Bug 1550164
#ifndef MOZ_WIDGET_ANDROID
  // The default attestation type is "none", so set
  // noneAttestationRequested=false only if the RP's preference matches one of
  // the other known types. This needs to be reviewed if values are added to
  // the AttestationConveyancePreference enum.
  const nsString& attestation = aInfo.attestationConveyancePreference();
  static_assert(MOZ_WEBAUTHN_ENUM_STRINGS_VERSION == 2);
  if (attestation.EqualsLiteral(
          MOZ_WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT) ||
      attestation.EqualsLiteral(
          MOZ_WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_INDIRECT) ||
      attestation.EqualsLiteral(
          MOZ_WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_ENTERPRISE)) {
    noneAttestationRequested = false;
  }
#endif  // not MOZ_WIDGET_ANDROID

  // Start a register request immediately if direct attestation
  // wasn't requested or the test pref is set.
  if (noneAttestationRequested ||
      StaticPrefs::
          security_webauth_webauthn_testing_allow_direct_attestation()) {
    DoRegister(aInfo, noneAttestationRequested);
    return;
  }

  // If the RP request direct attestation, ask the user for permission and
  // store the transaction info until the user proceeds or cancels.
  NS_ConvertUTF16toUTF8 origin(aInfo.Origin());
  SendPromptNotification(kRegisterDirectPromptNotification, aTransactionId,
                         origin.get(), aInfo.BrowsingContextId());
}

void WebAuthnController::DoRegister(const WebAuthnMakeCredentialInfo& aInfo,
                                    bool aForceNoneAttestation) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_ASSERT(mTransaction.isSome());
  if (NS_WARN_IF(mTransaction.isNothing())) {
    // Clear prompt?
    return;
  }

  // Show a prompt that lets the user cancel the ongoing transaction.
  NS_ConvertUTF16toUTF8 origin(aInfo.Origin());
  SendPromptNotification(kPresencePromptNotification,
                         mTransaction.ref().mTransactionId, origin.get(),
                         aInfo.BrowsingContextId(), "false");

  RefPtr<CtapRegisterArgs> args(
      new CtapRegisterArgs(aInfo, aForceNoneAttestation));

  mTransportImpl = GetTransportImpl();
  if (!mTransportImpl) {
    AbortTransaction(mTransaction.ref().mTransactionId,
                     NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
    return;
  }
  nsresult rv = mTransportImpl->MakeCredential(
      mTransaction.ref().mTransactionId, aInfo.BrowsingContextId(), args);
  if (NS_FAILED(rv)) {
    AbortTransaction(mTransaction.ref().mTransactionId,
                     NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
    return;
  }
}

NS_IMETHODIMP
WebAuthnController::ResumeRegister(uint64_t aTransactionId,
                                   bool aForceNoneAttestation) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!gWebAuthnBackgroundThread) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIRunnable> r(NewRunnableMethod<uint64_t, bool>(
      "WebAuthnController::RunResumeRegister", this,
      &WebAuthnController::RunResumeRegister, aTransactionId,
      aForceNoneAttestation));

  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdownThreads)) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }
  return gWebAuthnBackgroundThread->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void WebAuthnController::RunResumeRegister(uint64_t aTransactionId,
                                           bool aForceNoneAttestation) {
  mozilla::ipc::AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mPendingRegisterInfo.isNothing())) {
    return;
  }

  if (mTransaction.isNothing() ||
      mTransaction.ref().mTransactionId != aTransactionId) {
    return;
  }

  // Resume registration and cleanup.
  DoRegister(mPendingRegisterInfo.ref(), aForceNoneAttestation);
}

NS_IMETHODIMP
WebAuthnController::FinishRegister(uint64_t aTransactionId,
                                   nsICtapRegisterResult* aResult) {
  MOZ_ASSERT(XRE_IsParentProcess());
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
  mozilla::ipc::AssertIsOnBackgroundThread();
  if (mTransaction.isNothing() ||
      aTransactionId != mTransaction.ref().mTransactionId) {
    // The previous transaction was likely cancelled from the prompt.
    return;
  }

  nsresult status;
  nsresult rv = aResult->GetStatus(&status);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
    return;
  }
  if (NS_FAILED(status)) {
    bool shouldCancelActiveDialog = true;
    if (status == NS_ERROR_DOM_INVALID_STATE_ERR) {
      // PIN-related errors. Let the dialog show to inform the user
      shouldCancelActiveDialog = false;
    } else {
      status = NS_ERROR_DOM_NOT_ALLOWED_ERR;
    }
    Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                         u"CTAPRegisterAbort"_ns, 1);
    AbortTransaction(aTransactionId, status, shouldCancelActiveDialog);
    return;
  }

  nsCString clientDataJson = mPendingRegisterInfo.ref().ClientDataJSON();

  nsTArray<uint8_t> attObj;
  rv = aResult->GetAttestationObject(attObj);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
    return;
  }

  nsTArray<uint8_t> credentialId;
  rv = aResult->GetCredentialId(credentialId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
    return;
  }

  nsTArray<nsString> transports;
  rv = aResult->GetTransports(transports);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
    return;
  }

  nsTArray<WebAuthnExtensionResult> extensions;
  WebAuthnMakeCredentialResult result(clientDataJson, attObj, credentialId,
                                      transports, extensions);

  Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                       u"CTAPRegisterFinish"_ns, 1);
  Unused << mTransactionParent->SendConfirmRegister(aTransactionId, result);
  ClearTransaction(true);
}

void WebAuthnController::Sign(PWebAuthnTransactionParent* aTransactionParent,
                              const uint64_t& aTransactionId,
                              const WebAuthnGetAssertionInfo& aInfo) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_LOG(gWebAuthnControllerLog, LogLevel::Debug, ("WebAuthnSign"));
  MOZ_ASSERT(aTransactionId > 0);

  if (!gWebAuthnBackgroundThread) {
    gWebAuthnBackgroundThread = NS_GetCurrentThread();
    MOZ_ASSERT(gWebAuthnBackgroundThread, "This should never be null!");
  }

  AbortOngoingTransaction();
  mTransactionParent = aTransactionParent;

  /* We could refactor to defer the hashing here */
  nsTArray<uint8_t> rpIdHash, clientDataHash;
  NS_ConvertUTF16toUTF8 rpId(aInfo.RpId());
  nsresult rv = BuildTransactionHashes(rpId, aInfo.ClientDataJSON(), rpIdHash,
                                       clientDataHash);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // We haven't set mTransaction yet, so we can't use AbortTransaction
    Unused << mTransactionParent->SendAbort(aTransactionId,
                                            NS_ERROR_DOM_UNKNOWN_ERR);
    return;
  }

  Maybe<nsTArray<uint8_t>> maybeAppIdHash = Nothing();
  for (const WebAuthnExtension& ext : aInfo.Extensions()) {
    if (ext.type() == WebAuthnExtension::TWebAuthnExtensionAppId) {
      nsTArray<uint8_t> appIdHash;
      rv = HashCString(NS_ConvertUTF16toUTF8(
                           ext.get_WebAuthnExtensionAppId().appIdentifier()),
                       appIdHash);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        Unused << mTransactionParent->SendAbort(aTransactionId,
                                                NS_ERROR_DOM_UNKNOWN_ERR);
        return;
      }
      maybeAppIdHash = Some(std::move(appIdHash));
    }
  }

  // Hold on to any state that we need to finish the transaction.
  mTransaction = Some(Transaction(aTransactionId, rpIdHash, maybeAppIdHash,
                                  aInfo.ClientDataJSON()));

  mPendingSignInfo = Some(aInfo);

  // Show a prompt that lets the user cancel the ongoing transaction.
  NS_ConvertUTF16toUTF8 origin(aInfo.Origin());
  SendPromptNotification(kPresencePromptNotification,
                         mTransaction.ref().mTransactionId, origin.get(),
                         aInfo.BrowsingContextId(), "false");

  RefPtr<CtapSignArgs> args(new CtapSignArgs(aInfo));

  mTransportImpl = GetTransportImpl();
  if (!mTransportImpl) {
    AbortTransaction(mTransaction.ref().mTransactionId,
                     NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
    return;
  }

  rv = mTransportImpl->GetAssertion(mTransaction.ref().mTransactionId,
                                    aInfo.BrowsingContextId(), args.get());
  if (NS_FAILED(rv)) {
    AbortTransaction(mTransaction.ref().mTransactionId,
                     NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
    return;
  }
}

NS_IMETHODIMP
WebAuthnController::FinishSign(
    uint64_t aTransactionId,
    const nsTArray<RefPtr<nsICtapSignResult>>& aResult) {
  MOZ_ASSERT(XRE_IsParentProcess());
  nsTArray<RefPtr<nsICtapSignResult>> ownedResult = aResult.Clone();

  nsCOMPtr<nsIRunnable> r(
      NewRunnableMethod<uint64_t, nsTArray<RefPtr<nsICtapSignResult>>>(
          "WebAuthnController::RunFinishSign", this,
          &WebAuthnController::RunFinishSign, aTransactionId,
          std::move(ownedResult)));

  if (!gWebAuthnBackgroundThread) {
    return NS_ERROR_FAILURE;
  }
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdownThreads)) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }
  return gWebAuthnBackgroundThread->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void WebAuthnController::RunFinishSign(
    uint64_t aTransactionId,
    const nsTArray<RefPtr<nsICtapSignResult>>& aResult) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  if (mTransaction.isNothing() ||
      aTransactionId != mTransaction.ref().mTransactionId) {
    return;
  }

  if (aResult.Length() == 0) {
    Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                         u"CTAPSignAbort"_ns, 1);
    AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
    return;
  }

  if (aResult.Length() == 1) {
    nsresult status;
    nsresult rv = aResult[0]->GetStatus(&status);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
      return;
    }
    if (NS_FAILED(status)) {
      bool shouldCancelActiveDialog = true;
      if (status == NS_ERROR_DOM_INVALID_STATE_ERR) {
        // PIN-related errors, e.g. blocked token. Let the dialog show to inform
        // the user
        shouldCancelActiveDialog = false;
      }
      Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                           u"CTAPSignAbort"_ns, 1);
      AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR,
                       shouldCancelActiveDialog);
      return;
    }
    mPendingSignResults = aResult.Clone();
    RunResumeWithSelectedSignResult(aTransactionId, 0);
    return;
  }

  // If we more than one assertion, all of them should have OK status.
  for (const auto& assertion : aResult) {
    nsresult status;
    nsresult rv = assertion->GetStatus(&status);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
      return;
    }
    if (NS_WARN_IF(NS_FAILED(status))) {
      Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                           u"CTAPSignAbort"_ns, 1);
      AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
      return;
    }
  }

  nsCString usernames;
  StringJoinAppend(
      usernames, ","_ns, aResult,
      [](nsACString& dst, const RefPtr<nsICtapSignResult>& assertion) {
        nsCString username;
        nsresult rv = assertion->GetUserName(username);
        if (NS_FAILED(rv)) {
          username.Assign("<Unknown username>");
        }
        nsCString escaped_username;
        NS_Escape(username, escaped_username, url_XAlphas);
        dst.Append("\""_ns + escaped_username + "\""_ns);
      });

  mPendingSignResults = aResult.Clone();
  NS_ConvertUTF16toUTF8 origin(mPendingSignInfo.ref().Origin());
  SendPromptNotification(kSelectSignResultNotification,
                         mTransaction.ref().mTransactionId, origin.get(),
                         mPendingSignInfo.ref().BrowsingContextId(),
                         usernames.get());
}

NS_IMETHODIMP
WebAuthnController::SignatureSelectionCallback(uint64_t aTransactionId,
                                               uint64_t idx) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIRunnable> r(NewRunnableMethod<uint64_t, uint64_t>(
      "WebAuthnController::RunResumeWithSelectedSignResult", this,
      &WebAuthnController::RunResumeWithSelectedSignResult, aTransactionId,
      idx));

  if (!gWebAuthnBackgroundThread) {
    return NS_ERROR_FAILURE;
  }
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdownThreads)) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }
  return gWebAuthnBackgroundThread->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void WebAuthnController::RunResumeWithSelectedSignResult(
    uint64_t aTransactionId, uint64_t idx) {
  mozilla::ipc::AssertIsOnBackgroundThread();
  if (mTransaction.isNothing() ||
      mTransaction.ref().mTransactionId != aTransactionId) {
    return;
  }

  if (NS_WARN_IF(mPendingSignResults.Length() <= idx)) {
    return;
  }

  RefPtr<nsICtapSignResult>& selected = mPendingSignResults[idx];

  nsTArray<uint8_t> credentialId;
  nsresult rv = selected->GetCredentialId(credentialId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
    return;
  }

  nsTArray<uint8_t> signature;
  rv = selected->GetSignature(signature);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
    return;
  }

  nsTArray<uint8_t> authenticatorData;
  rv = selected->GetAuthenticatorData(authenticatorData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
    return;
  }

  nsTArray<uint8_t> rpIdHash;
  rv = selected->GetRpIdHash(rpIdHash);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
    return;
  }

  nsTArray<uint8_t> userHandle;
  Unused << selected->GetUserHandle(userHandle);  // optional

  nsTArray<WebAuthnExtensionResult> extensions;
  if (mTransaction.ref().mAppIdHash.isSome()) {
    bool usedAppId = (rpIdHash == mTransaction.ref().mAppIdHash.ref());
    extensions.AppendElement(WebAuthnExtensionResultAppId(usedAppId));
  }

  WebAuthnGetAssertionResult result(mTransaction.ref().mClientDataJSON,
                                    credentialId, signature, authenticatorData,
                                    extensions, userHandle);

  Telemetry::ScalarAdd(Telemetry::ScalarID::SECURITY_WEBAUTHN_USED,
                       u"CTAPSignFinish"_ns, 1);
  Unused << mTransactionParent->SendConfirmSign(aTransactionId, result);
  ClearTransaction(true);
}

NS_IMETHODIMP
WebAuthnController::PinCallback(uint64_t aTransactionId,
                                const nsACString& aPin) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIRunnable> r(NewRunnableMethod<uint64_t, nsCString>(
      "WebAuthnController::RunPinCallback", this,
      &WebAuthnController::RunPinCallback, aTransactionId, aPin));

  if (!gWebAuthnBackgroundThread) {
    return NS_ERROR_FAILURE;
  }
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdownThreads)) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }
  return gWebAuthnBackgroundThread->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void WebAuthnController::RunPinCallback(uint64_t aTransactionId,
                                        const nsCString& aPin) {
  mozilla::ipc::AssertIsOnBackgroundThread();

  if (mTransportImpl) {
    mTransportImpl->PinCallback(aTransactionId, aPin);
  }
}

NS_IMETHODIMP
WebAuthnController::Cancel(uint64_t aTransactionId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIRunnable> r(NewRunnableMethod<uint64_t>(
      "WebAuthnController::RunCancel", this, &WebAuthnController::RunCancel,
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
  mozilla::ipc::AssertIsOnBackgroundThread();

  if (mTransaction.isNothing() ||
      mTransaction.ref().mTransactionId != aTransactionId) {
    return;
  }

  // Cancel the request.
  if (mTransportImpl) {
    mTransportImpl->Cancel();
  }

  // Reject the promise.
  AbortTransaction(aTransactionId, NS_ERROR_DOM_NOT_ALLOWED_ERR, true);
}

}  // namespace mozilla::dom

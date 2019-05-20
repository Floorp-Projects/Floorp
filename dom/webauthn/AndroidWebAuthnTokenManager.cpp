/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/jni/GeckoBundleUtils.h"
#include "mozilla/StaticPtr.h"

#include "AndroidWebAuthnTokenManager.h"
#include "GeneratedJNIWrappers.h"
#include "JavaBuiltins.h"

namespace mozilla {
namespace dom {

static nsIThread* gAndroidPBackgroundThread;

StaticRefPtr<AndroidWebAuthnTokenManager> gAndroidWebAuthnManager;

/* static */ AndroidWebAuthnTokenManager*
AndroidWebAuthnTokenManager::GetInstance() {
  if (!gAndroidWebAuthnManager) {
    mozilla::ipc::AssertIsOnBackgroundThread();
    gAndroidWebAuthnManager = new AndroidWebAuthnTokenManager();
  }
  return gAndroidWebAuthnManager;
}

AndroidWebAuthnTokenManager::AndroidWebAuthnTokenManager() {
  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!gAndroidWebAuthnManager);

  gAndroidPBackgroundThread = NS_GetCurrentThread();
  MOZ_ASSERT(gAndroidPBackgroundThread, "This should never be null!");
  gAndroidWebAuthnManager = this;
}

void AndroidWebAuthnTokenManager::AssertIsOnOwningThread() const {
  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_ASSERT(gAndroidPBackgroundThread);
#ifdef DEBUG
  bool current;
  MOZ_ASSERT(
      NS_SUCCEEDED(gAndroidPBackgroundThread->IsOnCurrentThread(&current)));
  MOZ_ASSERT(current);
#endif
}

void AndroidWebAuthnTokenManager::Drop() {
  AssertIsOnOwningThread();

  ClearPromises();
  gAndroidWebAuthnManager = nullptr;
  gAndroidPBackgroundThread = nullptr;
}

RefPtr<U2FRegisterPromise> AndroidWebAuthnTokenManager::Register(
    const WebAuthnMakeCredentialInfo& aInfo, bool aForceNoneAttestation) {
  AssertIsOnOwningThread();

  ClearPromises();

  GetMainThreadEventTarget()->Dispatch(NS_NewRunnableFunction(
      "java::WebAuthnTokenManager::WebAuthnMakeCredential",
      [aInfo, aForceNoneAttestation]() {
        AssertIsOnMainThread();

        // Produce the credential exclusion list
        jni::ObjectArray::LocalRef idList =
            jni::ObjectArray::New(aInfo.ExcludeList().Length());

        nsTArray<uint8_t> transportBuf;
        int ix = 0;

        for (const WebAuthnScopedCredential& cred : aInfo.ExcludeList()) {
          jni::ByteBuffer::LocalRef id = jni::ByteBuffer::New(
              const_cast<void*>(static_cast<const void*>(cred.id().Elements())),
              cred.id().Length());

          idList->SetElement(ix, id);
          transportBuf.AppendElement(cred.transports());

          ix += 1;
        }

        jni::ByteBuffer::LocalRef transportList = jni::ByteBuffer::New(
            const_cast<void*>(
                static_cast<const void*>(transportBuf.Elements())),
            transportBuf.Length());

        const nsTArray<uint8_t>& challBuf = aInfo.Challenge();
        jni::ByteBuffer::LocalRef challenge = jni::ByteBuffer::New(
            const_cast<void*>(static_cast<const void*>(challBuf.Elements())),
            challBuf.Length());

        nsTArray<uint8_t> uidBuf;

        // Get authenticator selection criteria
        GECKOBUNDLE_START(authSelBundle);
        GECKOBUNDLE_START(extensionsBundle);
        GECKOBUNDLE_START(credentialBundle);

        if (aInfo.Extra().isSome()) {
          const auto& extra = aInfo.Extra().ref();
          const auto& rp = extra.Rp();
          const auto& user = extra.User();

          // If we have extra data, then this is WebAuthn, not U2F
          GECKOBUNDLE_PUT(credentialBundle, "isWebAuthn",
                          java::sdk::Integer::ValueOf(1));

          // Get the attestation preference and override if the user asked
          AttestationConveyancePreference attestation =
              extra.attestationConveyancePreference();

          if (aForceNoneAttestation) {
            // Add UI support to trigger this, bug 1550164
            attestation = AttestationConveyancePreference::None;
          }

          if (static_cast<uint32_t>(attestation) <
              static_cast<uint32_t>(
                  AttestationConveyancePreference::EndGuard_)) {
            // Protect the direct array reference below. Note this is still a
            // ContiguousEnumValidator type.
            nsString attestPref;
            const EnumEntry& attPrefEntry =
                AttestationConveyancePreferenceValues::strings
                    [static_cast<uint32_t>(attestation)];
            attestPref.AssignASCII(attPrefEntry.value, attPrefEntry.length);
            GECKOBUNDLE_PUT(authSelBundle, "attestationPreference",
                            jni::StringParam(attestPref));
          }

          const WebAuthnAuthenticatorSelection& sel =
              extra.AuthenticatorSelection();
          if (sel.requireResidentKey()) {
            GECKOBUNDLE_PUT(authSelBundle, "requireResidentKey",
                            java::sdk::Integer::ValueOf(1));
          }

          if (sel.userVerificationRequirement() ==
              UserVerificationRequirement::Required) {
            GECKOBUNDLE_PUT(authSelBundle, "requireUserVerification",
                            java::sdk::Integer::ValueOf(1));
          }

          if (sel.authenticatorAttachment().isSome()) {
            const AuthenticatorAttachment authenticatorAttachment =
                sel.authenticatorAttachment().value();
            if (authenticatorAttachment == AuthenticatorAttachment::Platform) {
              GECKOBUNDLE_PUT(authSelBundle, "requirePlatformAttachment",
                              java::sdk::Integer::ValueOf(1));
            }
          }

          // Get extensions
          for (const WebAuthnExtension& ext : extra.Extensions()) {
            if (ext.type() == WebAuthnExtension::TWebAuthnExtensionAppId) {
              GECKOBUNDLE_PUT(
                  extensionsBundle, "fidoAppId",
                  jni::StringParam(
                      ext.get_WebAuthnExtensionAppId().appIdentifier()));
            }
          }

          uidBuf.Assign(user.Id());

          GECKOBUNDLE_PUT(credentialBundle, "rpName",
                          jni::StringParam(rp.Name()));
          GECKOBUNDLE_PUT(credentialBundle, "rpIcon",
                          jni::StringParam(rp.Icon()));
          GECKOBUNDLE_PUT(credentialBundle, "userName",
                          jni::StringParam(user.Name()));
          GECKOBUNDLE_PUT(credentialBundle, "userIcon",
                          jni::StringParam(user.Icon()));
          GECKOBUNDLE_PUT(credentialBundle, "userDisplayName",
                          jni::StringParam(user.DisplayName()));
        }

        GECKOBUNDLE_PUT(credentialBundle, "rpId",
                        jni::StringParam(aInfo.RpId()));
        GECKOBUNDLE_PUT(credentialBundle, "origin",
                        jni::StringParam(aInfo.Origin()));
        GECKOBUNDLE_PUT(credentialBundle, "timeoutMS",
                        java::sdk::Double::New(aInfo.TimeoutMS()));

        GECKOBUNDLE_FINISH(authSelBundle);
        GECKOBUNDLE_FINISH(extensionsBundle);
        GECKOBUNDLE_FINISH(credentialBundle);

        // For non-WebAuthn cases, uidBuf is empty (and unused)
        jni::ByteBuffer::LocalRef uid = jni::ByteBuffer::New(
            const_cast<void*>(static_cast<const void*>(uidBuf.Elements())),
            uidBuf.Length());

        java::WebAuthnTokenManager::WebAuthnMakeCredential(
            credentialBundle, uid, challenge, idList, transportList,
            authSelBundle, extensionsBundle);
      }));

  return mRegisterPromise.Ensure(__func__);
}

void AndroidWebAuthnTokenManager::HandleRegisterResult(
    const AndroidWebAuthnResult& aResult) {
  // This is likely running on the main thread, so we'll always dispatch to the
  // background for state updates.
  if (aResult.IsError()) {
    nsresult aError = aResult.GetError();

    gAndroidPBackgroundThread->Dispatch(NS_NewRunnableFunction(
        "AndroidWebAuthnTokenManager::RegisterAbort",
        [self = RefPtr<AndroidWebAuthnTokenManager>(this), aError]() {
          self->mRegisterPromise.RejectIfExists(aError, __func__);
        }));
  } else {
    gAndroidPBackgroundThread->Dispatch(NS_NewRunnableFunction(
        "AndroidWebAuthnTokenManager::RegisterComplete",
        [self = RefPtr<AndroidWebAuthnTokenManager>(this), aResult]() {
          CryptoBuffer emptyBuffer;
          WebAuthnMakeCredentialResult result(aResult.mClientDataJSON,
                                              aResult.mAttObj,
                                              aResult.mKeyHandle, emptyBuffer);
          self->mRegisterPromise.Resolve(std::move(result), __func__);
        }));
  }
}

RefPtr<U2FSignPromise> AndroidWebAuthnTokenManager::Sign(
    const WebAuthnGetAssertionInfo& aInfo) {
  AssertIsOnOwningThread();

  ClearPromises();

  GetMainThreadEventTarget()->Dispatch(NS_NewRunnableFunction(
      "java::WebAuthnTokenManager::WebAuthnGetAssertion", [aInfo]() {
        AssertIsOnMainThread();

        jni::ObjectArray::LocalRef idList =
            jni::ObjectArray::New(aInfo.AllowList().Length());

        nsTArray<uint8_t> transportBuf;

        int ix = 0;
        for (const WebAuthnScopedCredential& cred : aInfo.AllowList()) {
          jni::ByteBuffer::LocalRef id = jni::ByteBuffer::New(
              const_cast<void*>(static_cast<const void*>(cred.id().Elements())),
              cred.id().Length());

          idList->SetElement(ix, id);
          transportBuf.AppendElement(cred.transports());

          ix += 1;
        }

        jni::ByteBuffer::LocalRef transportList = jni::ByteBuffer::New(
            const_cast<void*>(
                static_cast<const void*>(transportBuf.Elements())),
            transportBuf.Length());

        const nsTArray<uint8_t>& challBuf = aInfo.Challenge();
        jni::ByteBuffer::LocalRef challenge = jni::ByteBuffer::New(
            const_cast<void*>(static_cast<const void*>(challBuf.Elements())),
            challBuf.Length());

        // Get extensions
        GECKOBUNDLE_START(assertionBundle);
        GECKOBUNDLE_START(extensionsBundle);
        if (aInfo.Extra().isSome()) {
          const auto& extra = aInfo.Extra().ref();

          // If we have extra data, then this is WebAuthn, not U2F
          GECKOBUNDLE_PUT(assertionBundle, "isWebAuthn",
                          java::sdk::Integer::ValueOf(1));

          // User Verification Requirement is not currently used in the
          // Android FIDO API. Adding it should look like
          // AttestationConveyancePreference

          for (const WebAuthnExtension& ext : extra.Extensions()) {
            if (ext.type() == WebAuthnExtension::TWebAuthnExtensionAppId) {
              GECKOBUNDLE_PUT(
                  extensionsBundle, "fidoAppId",
                  jni::StringParam(
                      ext.get_WebAuthnExtensionAppId().appIdentifier()));
            }
          }
        }

        GECKOBUNDLE_PUT(assertionBundle, "rpId",
                        jni::StringParam(aInfo.RpId()));
        GECKOBUNDLE_PUT(assertionBundle, "origin",
                        jni::StringParam(aInfo.Origin()));
        GECKOBUNDLE_PUT(assertionBundle, "timeoutMS",
                        java::sdk::Double::New(aInfo.TimeoutMS()));

        GECKOBUNDLE_FINISH(assertionBundle);
        GECKOBUNDLE_FINISH(extensionsBundle);

        java::WebAuthnTokenManager::WebAuthnGetAssertion(
            challenge, idList, transportList, assertionBundle,
            extensionsBundle);
      }));

  return mSignPromise.Ensure(__func__);
}

void AndroidWebAuthnTokenManager::HandleSignResult(
    const AndroidWebAuthnResult& aResult) {
  // This is likely running on the main thread, so we'll always dispatch to the
  // background for state updates.
  if (aResult.IsError()) {
    nsresult aError = aResult.GetError();

    gAndroidPBackgroundThread->Dispatch(NS_NewRunnableFunction(
        "AndroidWebAuthnTokenManager::SignAbort",
        [self = RefPtr<AndroidWebAuthnTokenManager>(this), aError]() {
          self->mSignPromise.RejectIfExists(aError, __func__);
        }));
  } else {
    gAndroidPBackgroundThread->Dispatch(NS_NewRunnableFunction(
        "AndroidWebAuthnTokenManager::SignComplete",
        [self = RefPtr<AndroidWebAuthnTokenManager>(this), aResult]() {
          CryptoBuffer emptyBuffer;

          nsTArray<WebAuthnExtensionResult> emptyExtensions;
          WebAuthnGetAssertionResult result(
              aResult.mClientDataJSON, aResult.mKeyHandle, aResult.mSignature,
              aResult.mAuthData, emptyExtensions, emptyBuffer,
              aResult.mUserHandle);
          self->mSignPromise.Resolve(std::move(result), __func__);
        }));
  }
}

void AndroidWebAuthnTokenManager::Cancel() {
  AssertIsOnOwningThread();

  ClearPromises();
}

}  // namespace dom
}  // namespace mozilla

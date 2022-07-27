/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OriginTrials.h"
#include "mozilla/Base64.h"
#include "mozilla/Span.h"
#include "nsString.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "xpcpublic.h"
#include "jsapi.h"
#include "js/Wrapper.h"
#include "nsGlobalWindowInner.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkletThread.h"
#include "mozilla/dom/WebCryptoCommon.h"
#include "mozilla/StaticPrefs_dom.h"
#include "ScopedNSSTypes.h"
#include <mutex>

namespace mozilla {

LazyLogModule sOriginTrialsLog("OriginTrials");
#define LOG(...) MOZ_LOG(sOriginTrialsLog, LogLevel::Debug, (__VA_ARGS__))

// prod.pub is the EcdsaP256 public key from the production key managed in
// Google Cloud. See:
//
//   https://github.com/mozilla/origin-trial-token/blob/main/tools/README.md#get-the-public-key
//
// for how to get the public key.
//
// See also:
//
//   https://github.com/mozilla/origin-trial-token/blob/main/tools/README.md#sign-a-token-using-gcloud
//
// for how to sign using this key.
//
// test.pub is the EcdsaP256 public key from this key pair:
//
//  * https://github.com/mozilla/origin-trial-token/blob/64f03749e2e8c58f811f67044cecc7d6955fd51a/tools/test-keys/test-ecdsa.pkcs8
//  * https://github.com/mozilla/origin-trial-token/blob/64f03749e2e8c58f811f67044cecc7d6955fd51a/tools/test-keys/test-ecdsa.pub
//
#include "keys.inc"

constexpr auto kEcAlgorithm =
    NS_LITERAL_STRING_FROM_CSTRING(WEBCRYPTO_NAMED_CURVE_P256);

using RawKeyRef = Span<const unsigned char, sizeof(kProdKey)>;

struct StaticCachedPublicKey {
  constexpr StaticCachedPublicKey() = default;

  SECKEYPublicKey* Get(const RawKeyRef aRawKey);

 private:
  std::once_flag mFlag;
  UniqueSECKEYPublicKey mKey;
};

SECKEYPublicKey* StaticCachedPublicKey::Get(const RawKeyRef aRawKey) {
  std::call_once(mFlag, [&] {
    const SECItem item{siBuffer, const_cast<unsigned char*>(aRawKey.data()),
                       unsigned(aRawKey.Length())};
    MOZ_RELEASE_ASSERT(item.data[0] == EC_POINT_FORM_UNCOMPRESSED);
    mKey = dom::CreateECPublicKey(&item, kEcAlgorithm);
    if (mKey) {
      // It's fine to capture [this] by pointer because we are always static.
      if (NS_IsMainThread()) {
        RunOnShutdown([this] { mKey = nullptr; });
      } else {
        NS_DispatchToMainThread(NS_NewRunnableFunction(
            "ClearStaticCachedPublicKey",
            [this] { RunOnShutdown([this] { mKey = nullptr; }); }));
      }
    }
  });
  return mKey.get();
}

bool VerifySignature(const uint8_t* aSignature, uintptr_t aSignatureLen,
                     const uint8_t* aData, uintptr_t aDataLen,
                     void* aUserData) {
  MOZ_RELEASE_ASSERT(aSignatureLen == 64);
  static StaticCachedPublicKey sTestKey;
  static StaticCachedPublicKey sProdKey;

  LOG("VerifySignature()\n");

  SECKEYPublicKey* pubKey = StaticPrefs::dom_origin_trials_test_key_enabled()
                                ? sTestKey.Get(Span(kTestKey))
                                : sProdKey.Get(Span(kProdKey));
  if (NS_WARN_IF(!pubKey)) {
    LOG("  Failed to create public key?");
    return false;
  }

  if (NS_WARN_IF(aDataLen > UINT_MAX)) {
    LOG("  Way too large data.");
    return false;
  }

  const SECItem signature{siBuffer, const_cast<unsigned char*>(aSignature),
                          unsigned(aSignatureLen)};
  const SECItem data{siBuffer, const_cast<unsigned char*>(aData),
                     unsigned(aDataLen)};

  // SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE
  const SECStatus result = PK11_VerifyWithMechanism(
      pubKey, CKM_ECDSA_SHA256, nullptr, &signature, &data, nullptr);
  if (NS_WARN_IF(result != SECSuccess)) {
    LOG("  Failed to verify data.");
    return false;
  }
  return true;
}

bool MatchesOrigin(const uint8_t* aOrigin, size_t aOriginLen, bool aIsSubdomain,
                   bool aIsThirdParty, bool aIsUsageSubset, void* aUserData) {
  const nsDependentCSubstring origin(reinterpret_cast<const char*>(aOrigin),
                                     aOriginLen);

  LOG("MatchesOrigin(%d, %d, %d, %s)\n", aIsThirdParty, aIsSubdomain,
      aIsUsageSubset, nsCString(origin).get());

  if (aIsThirdParty || aIsUsageSubset) {
    // TODO(emilio): Support third-party tokens and so on.
    return false;
  }

  auto* principal = static_cast<nsIPrincipal*>(aUserData);
  nsCOMPtr<nsIURI> originURI;
  if (NS_WARN_IF(NS_FAILED(NS_NewURI(getter_AddRefs(originURI), origin)))) {
    return false;
  }

  const bool originMatches = [&] {
    if (principal->IsSameOrigin(originURI)) {
      return true;
    }
    if (aIsSubdomain) {
      for (nsCOMPtr<nsIPrincipal> prin = principal->GetNextSubDomainPrincipal();
           prin; prin = prin->GetNextSubDomainPrincipal()) {
        if (prin->IsSameOrigin(originURI)) {
          return true;
        }
      }
    }
    return false;
  }();

  if (NS_WARN_IF(!originMatches)) {
    LOG("Origin doesn't match\n");
    return false;
  }

  return true;
}

void OriginTrials::UpdateFromToken(const nsAString& aBase64EncodedToken,
                                   nsIPrincipal* aPrincipal) {
  if (!StaticPrefs::dom_origin_trials_enabled()) {
    return;
  }

  LOG("OriginTrials::UpdateFromToken()\n");

  nsAutoCString decodedToken;
  nsresult rv = mozilla::Base64Decode(aBase64EncodedToken, decodedToken);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  const Span<const uint8_t> decodedTokenSpan(decodedToken);
  const origin_trials_ffi::OriginTrialValidationParams params{
      VerifySignature,
      MatchesOrigin,
      /* user_data = */ aPrincipal,
  };
  auto result = origin_trials_ffi::origin_trials_parse_and_validate_token(
      decodedTokenSpan.data(), decodedTokenSpan.size(), &params);
  if (NS_WARN_IF(!result.IsOk())) {
    LOG("  result = %d\n", int(result.tag));
    return;  // TODO(emilio): Maybe report to console or what not?
  }
  OriginTrial trial = result.AsOk().trial;
  LOG("  result = Ok(%d)\n", int(trial));
  mEnabledTrials += trial;
}

OriginTrials OriginTrials::FromWindow(const nsGlobalWindowInner* aWindow) {
  if (!aWindow) {
    return {};
  }
  const dom::Document* doc = aWindow->GetExtantDoc();
  if (!doc) {
    return {};
  }
  return doc->Trials();
}

static int32_t PrefState(OriginTrial aTrial) {
  switch (aTrial) {
    case OriginTrial::TestTrial:
      return StaticPrefs::dom_origin_trials_test_trial_state();
    case OriginTrial::OffscreenCanvas:
      return StaticPrefs::dom_origin_trials_offscreen_canvas_state();
    case OriginTrial::CoepCredentialless:
      return StaticPrefs::dom_origin_trials_coep_credentialless_state();
    case OriginTrial::MAX:
      MOZ_ASSERT_UNREACHABLE("Unknown trial!");
      break;
  }
  return 0;
}

bool OriginTrials::IsEnabled(OriginTrial aTrial) const {
  switch (PrefState(aTrial)) {
    case 1:
      return true;
    case 2:
      return false;
    default:
      break;
  }

  return mEnabledTrials.contains(aTrial);
}

bool OriginTrials::IsEnabled(JSContext* aCx, JSObject* aObject,
                             OriginTrial aTrial) {
  if (nsContentUtils::ThreadsafeIsSystemCaller(aCx)) {
    return true;
  }
  LOG("OriginTrials::IsEnabled(%d)\n", int(aTrial));
  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  MOZ_ASSERT(global);
  return global && global->Trials().IsEnabled(aTrial);
}

#undef LOG

}  // namespace mozilla

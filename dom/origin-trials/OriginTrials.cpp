/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OriginTrials.h"
#include "mozilla/Base64.h"
#include "nsString.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsNetUtil.h"

namespace mozilla {

bool VerifySignature(const uint8_t* aSignature, uintptr_t aSignatureLen,
                     const uint8_t* aData, uintptr_t aDataLen,
                     void* aUserData) {
  MOZ_RELEASE_ASSERT(aSignatureLen == 64);
  // TODO(emilio): Implement.
  return false;
}

bool MatchesOrigin(const uint8_t* aOrigin, size_t aOriginLen, bool aIsSubdomain,
                   bool aIsThirdParty, bool aIsUsageSubset, void* aUserData) {
  if (aIsThirdParty || aIsSubdomain || aIsUsageSubset) {
    // TODO(emilio): Support third-party tokens and so on.
    return false;
  }

  auto* principal = static_cast<nsIPrincipal*>(aUserData);
  nsDependentCSubstring origin(reinterpret_cast<const char*>(aOrigin),
                               aOriginLen);
  nsCOMPtr<nsIURI> originURI;
  if (NS_WARN_IF(NS_FAILED(NS_NewURI(getter_AddRefs(originURI), origin)))) {
    return false;
  }

  return principal->IsSameOrigin(originURI);
}

void OriginTrials::UpdateFromToken(const nsAString& aBase64EncodedToken,
                                   nsIPrincipal* aPrincipal) {
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
  if (!result.IsOk()) {
    return;  // TODO(emilio): Maybe report to console or what not?
  }
  OriginTrial trial = result.AsOk().trial;
  mEnabledTrials += trial;
}

}  // namespace mozilla

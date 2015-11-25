/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SRICheck.h"

#include "mozilla/Base64.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsICryptoHash.h"
#include "nsIDocument.h"
#include "nsIProtocolHandler.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamLoader.h"
#include "nsIUnicharStreamLoader.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsWhitespaceTokenizer.h"

static mozilla::LogModule*
GetSriLog()
{
  static mozilla::LazyLogModule gSriPRLog("SRI");
  return gSriPRLog;
}

#define SRILOG(args) MOZ_LOG(GetSriLog(), mozilla::LogLevel::Debug, args)
#define SRIERROR(args) MOZ_LOG(GetSriLog(), mozilla::LogLevel::Error, args)

namespace mozilla {
namespace dom {

/**
 * Returns whether or not the sub-resource about to be loaded is eligible
 * for integrity checks. If it's not, the checks will be skipped and the
 * sub-resource will be loaded.
 */
static nsresult
IsEligible(nsIChannel* aChannel, const CORSMode aCORSMode,
           const nsIDocument* aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);

  if (!aChannel) {
    SRILOG(("SRICheck::IsEligible, null channel"));
    return NS_ERROR_SRI_NOT_ELIGIBLE;
  }

  // Was the sub-resource loaded via CORS?
  if (aCORSMode != CORS_NONE) {
    SRILOG(("SRICheck::IsEligible, CORS mode"));
    return NS_OK;
  }

  nsCOMPtr<nsIURI> finalURI;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(finalURI));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIURI> originalURI;
  rv = aChannel->GetOriginalURI(getter_AddRefs(originalURI));
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoCString requestSpec;
  rv = originalURI->GetSpec(requestSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  if (MOZ_LOG_TEST(GetSriLog(), mozilla::LogLevel::Debug)) {
    nsAutoCString documentSpec, finalSpec;
    aDocument->GetDocumentURI()->GetAsciiSpec(documentSpec);
    if (finalURI) {
      finalURI->GetSpec(finalSpec);
    }
    SRILOG(("SRICheck::IsEligible, documentURI=%s; requestURI=%s; finalURI=%s",
            documentSpec.get(), requestSpec.get(), finalSpec.get()));
  }

  // Is the sub-resource same-origin?
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  if (NS_SUCCEEDED(ssm->CheckSameOriginURI(aDocument->GetDocumentURI(),
                                           finalURI, false))) {
    SRILOG(("SRICheck::IsEligible, same-origin"));
    return NS_OK;
  }
  SRILOG(("SRICheck::IsEligible, NOT same origin"));

  NS_ConvertUTF8toUTF16 requestSpecUTF16(requestSpec);
  const char16_t* params[] = { requestSpecUTF16.get() };
  nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                  NS_LITERAL_CSTRING("Sub-resource Integrity"),
                                  aDocument,
                                  nsContentUtils::eSECURITY_PROPERTIES,
                                  "IneligibleResource",
                                  params, ArrayLength(params));
  return NS_ERROR_SRI_NOT_ELIGIBLE;
}

/**
 * Compute the hash of a sub-resource and compare it with the expected
 * value.
 */
static nsresult
VerifyHash(const SRIMetadata& aMetadata, uint32_t aHashIndex,
           uint32_t aStringLen, const uint8_t* aString,
           const nsIDocument* aDocument)
{
  NS_ENSURE_ARG_POINTER(aString);
  NS_ENSURE_ARG_POINTER(aDocument);

  nsAutoCString base64Hash;
  aMetadata.GetHash(aHashIndex, &base64Hash);
  SRILOG(("SRICheck::VerifyHash, hash[%u]=%s", aHashIndex, base64Hash.get()));

  nsAutoCString binaryHash;
  if (NS_WARN_IF(NS_FAILED(Base64Decode(base64Hash, binaryHash)))) {
    nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                    NS_LITERAL_CSTRING("Sub-resource Integrity"),
                                    aDocument,
                                    nsContentUtils::eSECURITY_PROPERTIES,
                                    "InvalidIntegrityBase64");
    return NS_ERROR_SRI_CORRUPT;
  }

  uint32_t hashLength;
  int8_t hashType;
  aMetadata.GetHashType(&hashType, &hashLength);
  if (binaryHash.Length() != hashLength) {
    nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                    NS_LITERAL_CSTRING("Sub-resource Integrity"),
                                    aDocument,
                                    nsContentUtils::eSECURITY_PROPERTIES,
                                    "InvalidIntegrityLength");
    return NS_ERROR_SRI_CORRUPT;
  }

  nsresult rv;
  nsCOMPtr<nsICryptoHash> cryptoHash =
    do_CreateInstance("@mozilla.org/security/hash;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = cryptoHash->Init(hashType);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = cryptoHash->Update(aString, aStringLen);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString computedHash;
  rv = cryptoHash->Finish(false, computedHash);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!binaryHash.Equals(computedHash)) {
    SRILOG(("SRICheck::VerifyHash, hash[%u] did not match", aHashIndex));
    return NS_ERROR_SRI_CORRUPT;
  }

  SRILOG(("SRICheck::VerifyHash, hash[%u] verified successfully", aHashIndex));
  return NS_OK;
}

/* static */ nsresult
SRICheck::IntegrityMetadata(const nsAString& aMetadataList,
                            const nsIDocument* aDocument,
                            SRIMetadata* outMetadata)
{
  NS_ENSURE_ARG_POINTER(outMetadata);
  NS_ENSURE_ARG_POINTER(aDocument);
  MOZ_ASSERT(outMetadata->IsEmpty()); // caller must pass empty metadata

  if (!Preferences::GetBool("security.sri.enable", false)) {
    SRILOG(("SRICheck::IntegrityMetadata, sri is disabled (pref)"));
    return NS_ERROR_SRI_DISABLED;
  }

  // put a reasonable bound on the length of the metadata
  NS_ConvertUTF16toUTF8 metadataList(aMetadataList);
  if (metadataList.Length() > SRICheck::MAX_METADATA_LENGTH) {
    metadataList.Truncate(SRICheck::MAX_METADATA_LENGTH);
  }
  MOZ_ASSERT(metadataList.Length() <= aMetadataList.Length());

  // the integrity attribute is a list of whitespace-separated hashes
  // and options so we need to look at them one by one and pick the
  // strongest (valid) one
  nsCWhitespaceTokenizer tokenizer(metadataList);
  nsAutoCString token;
  for (uint32_t i=0; tokenizer.hasMoreTokens() &&
         i < SRICheck::MAX_METADATA_TOKENS; ++i) {
    token = tokenizer.nextToken();

    SRIMetadata metadata(token);
    if (metadata.IsMalformed()) {
      NS_ConvertUTF8toUTF16 tokenUTF16(token);
      const char16_t* params[] = { tokenUTF16.get() };
      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                      NS_LITERAL_CSTRING("Sub-resource Integrity"),
                                      aDocument,
                                      nsContentUtils::eSECURITY_PROPERTIES,
                                      "MalformedIntegrityHash",
                                      params, ArrayLength(params));
    } else if (!metadata.IsAlgorithmSupported()) {
      nsAutoCString alg;
      metadata.GetAlgorithm(&alg);
      NS_ConvertUTF8toUTF16 algUTF16(alg);
      const char16_t* params[] = { algUTF16.get() };
      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                      NS_LITERAL_CSTRING("Sub-resource Integrity"),
                                      aDocument,
                                      nsContentUtils::eSECURITY_PROPERTIES,
                                      "UnsupportedHashAlg",
                                      params, ArrayLength(params));
    }

    nsAutoCString alg1, alg2;
    if (MOZ_LOG_TEST(GetSriLog(), mozilla::LogLevel::Debug)) {
      outMetadata->GetAlgorithm(&alg1);
      metadata.GetAlgorithm(&alg2);
    }
    if (*outMetadata == metadata) {
      SRILOG(("SRICheck::IntegrityMetadata, alg '%s' is the same as '%s'",
              alg1.get(), alg2.get()));
      *outMetadata += metadata; // add new hash to strongest metadata
    } else if (*outMetadata < metadata) {
      SRILOG(("SRICheck::IntegrityMetadata, alg '%s' is weaker than '%s'",
              alg1.get(), alg2.get()));
      *outMetadata = metadata; // replace strongest metadata with current
    }
  }

  if (MOZ_LOG_TEST(GetSriLog(), mozilla::LogLevel::Debug)) {
    if (outMetadata->IsValid()) {
      nsAutoCString alg;
      outMetadata->GetAlgorithm(&alg);
      SRILOG(("SRICheck::IntegrityMetadata, using a '%s' hash", alg.get()));
    } else if (outMetadata->IsEmpty()) {
      SRILOG(("SRICheck::IntegrityMetadata, no metadata"));
    } else {
      SRILOG(("SRICheck::IntegrityMetadata, no valid metadata found"));
    }
  }
  return NS_OK;
}

static nsresult
VerifyIntegrityInternal(const SRIMetadata& aMetadata,
                        nsIChannel* aChannel,
                        const CORSMode aCORSMode,
                        uint32_t aStringLen,
                        const uint8_t* aString,
                        const nsIDocument* aDocument)
{
  MOZ_ASSERT(!aMetadata.IsEmpty()); // should be checked by caller

  // IntegrityMetadata() checks this and returns "no metadata" if
  // it's disabled so we should never make it this far
  MOZ_ASSERT(Preferences::GetBool("security.sri.enable", false));

  if (NS_FAILED(IsEligible(aChannel, aCORSMode, aDocument))) {
    return NS_ERROR_SRI_NOT_ELIGIBLE;
  }
  if (!aMetadata.IsValid()) {
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                    NS_LITERAL_CSTRING("Sub-resource Integrity"),
                                    aDocument,
                                    nsContentUtils::eSECURITY_PROPERTIES,
                                    "NoValidMetadata");
    return NS_OK; // ignore invalid metadata for forward-compatibility
  }

  for (uint32_t i = 0; i < aMetadata.HashCount(); i++) {
    if (NS_SUCCEEDED(VerifyHash(aMetadata, i, aStringLen,
                                aString, aDocument))) {
      return NS_OK; // stop at the first valid hash
    }
  }

  nsAutoCString alg;
  aMetadata.GetAlgorithm(&alg);
  NS_ConvertUTF8toUTF16 algUTF16(alg);
  const char16_t* params[] = { algUTF16.get() };
  nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                  NS_LITERAL_CSTRING("Sub-resource Integrity"),
                                  aDocument,
                                  nsContentUtils::eSECURITY_PROPERTIES,
                                  "IntegrityMismatch",
                                  params, ArrayLength(params));
  return NS_ERROR_SRI_CORRUPT;
}

/* static */ nsresult
SRICheck::VerifyIntegrity(const SRIMetadata& aMetadata,
                          nsIUnicharStreamLoader* aLoader,
                          const CORSMode aCORSMode,
                          const nsAString& aString,
                          const nsIDocument* aDocument)
{
  NS_ENSURE_ARG_POINTER(aLoader);

  NS_ConvertUTF16toUTF8 utf8Hash(aString);
  nsCOMPtr<nsIChannel> channel;
  aLoader->GetChannel(getter_AddRefs(channel));

  if (MOZ_LOG_TEST(GetSriLog(), mozilla::LogLevel::Debug)) {
    nsAutoCString requestURL;
    nsCOMPtr<nsIURI> originalURI;
    if (channel &&
        NS_SUCCEEDED(channel->GetOriginalURI(getter_AddRefs(originalURI))) &&
        originalURI) {
      originalURI->GetAsciiSpec(requestURL);
    }
    SRILOG(("SRICheck::VerifyIntegrity (unichar stream), url=%s (length=%u)",
            requestURL.get(), utf8Hash.Length()));
  }

  return VerifyIntegrityInternal(aMetadata, channel, aCORSMode,
                                 utf8Hash.Length(), (uint8_t*)utf8Hash.get(),
                                 aDocument);
}

/* static */ nsresult
SRICheck::VerifyIntegrity(const SRIMetadata& aMetadata,
                          nsIStreamLoader* aLoader,
                          const CORSMode aCORSMode,
                          uint32_t aStringLen,
                          const uint8_t* aString,
                          const nsIDocument* aDocument)
{
  NS_ENSURE_ARG_POINTER(aLoader);

  nsCOMPtr<nsIRequest> request;
  aLoader->GetRequest(getter_AddRefs(request));
  NS_ENSURE_ARG_POINTER(request);
  nsCOMPtr<nsIChannel> channel;
  channel = do_QueryInterface(request);

  if (MOZ_LOG_TEST(GetSriLog(), mozilla::LogLevel::Debug)) {
    nsAutoCString requestURL;
    request->GetName(requestURL);
    SRILOG(("SRICheck::VerifyIntegrity (stream), url=%s (length=%u)",
            requestURL.get(), aStringLen));
  }

  return VerifyIntegrityInternal(aMetadata, channel, aCORSMode,
                                 aStringLen, aString, aDocument);
}

} // namespace dom
} // namespace mozilla

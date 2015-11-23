/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SRIMetadata.h"

#include "hasht.h"
#include "mozilla/dom/URLSearchParams.h"
#include "mozilla/Logging.h"
#include "nsICryptoHash.h"

static mozilla::LogModule*
GetSriMetadataLog()
{
  static mozilla::LazyLogModule gSriMetadataPRLog("SRIMetadata");
  return gSriMetadataPRLog;
}

#define SRIMETADATALOG(args) MOZ_LOG(GetSriMetadataLog(), mozilla::LogLevel::Debug, args)
#define SRIMETADATAERROR(args) MOZ_LOG(GetSriMetadataLog(), mozilla::LogLevel::Error, args)

namespace mozilla {
namespace dom {

SRIMetadata::SRIMetadata(const nsACString& aToken)
  : mAlgorithmType(SRIMetadata::UNKNOWN_ALGORITHM), mEmpty(false)
{
  MOZ_ASSERT(!aToken.IsEmpty()); // callers should check this first

  SRIMETADATALOG(("SRIMetadata::SRIMetadata, aToken='%s'",
                  PromiseFlatCString(aToken).get()));

  int32_t hyphen = aToken.FindChar('-');
  if (hyphen == -1) {
    SRIMETADATAERROR(("SRIMetadata::SRIMetadata, invalid (no hyphen)"));
    return; // invalid metadata
  }

  // split the token into its components
  mAlgorithm = Substring(aToken, 0, hyphen);
  uint32_t hashStart = hyphen + 1;
  if (hashStart >= aToken.Length()) {
    SRIMETADATAERROR(("SRIMetadata::SRIMetadata, invalid (missing digest)"));
    return; // invalid metadata
  }
  int32_t question = aToken.FindChar('?');
  if (question == -1) {
    mHashes.AppendElement(Substring(aToken, hashStart,
                                    aToken.Length() - hashStart));
  } else {
    MOZ_ASSERT(question > 0);
    if (static_cast<uint32_t>(question) <= hashStart) {
      SRIMETADATAERROR(("SRIMetadata::SRIMetadata, invalid (options w/o digest)"));
      return; // invalid metadata
    }
    mHashes.AppendElement(Substring(aToken, hashStart,
                                    question - hashStart));
  }

  if (mAlgorithm.EqualsLiteral("sha256")) {
    mAlgorithmType = nsICryptoHash::SHA256;
  } else if (mAlgorithm.EqualsLiteral("sha384")) {
    mAlgorithmType = nsICryptoHash::SHA384;
  } else if (mAlgorithm.EqualsLiteral("sha512")) {
    mAlgorithmType = nsICryptoHash::SHA512;
  }

  SRIMETADATALOG(("SRIMetadata::SRIMetadata, hash='%s'; alg='%s'",
                  mHashes[0].get(), mAlgorithm.get()));
}

bool
SRIMetadata::operator<(const SRIMetadata& aOther) const
{
  static_assert(nsICryptoHash::SHA256 < nsICryptoHash::SHA384,
                "We rely on the order indicating relative alg strength");
  static_assert(nsICryptoHash::SHA384 < nsICryptoHash::SHA512,
                "We rely on the order indicating relative alg strength");
  MOZ_ASSERT(mAlgorithmType == SRIMetadata::UNKNOWN_ALGORITHM ||
             mAlgorithmType == nsICryptoHash::SHA256 ||
             mAlgorithmType == nsICryptoHash::SHA384 ||
             mAlgorithmType == nsICryptoHash::SHA512);
  MOZ_ASSERT(aOther.mAlgorithmType == SRIMetadata::UNKNOWN_ALGORITHM ||
             aOther.mAlgorithmType == nsICryptoHash::SHA256 ||
             aOther.mAlgorithmType == nsICryptoHash::SHA384 ||
             aOther.mAlgorithmType == nsICryptoHash::SHA512);

  if (mEmpty) {
    SRIMETADATALOG(("SRIMetadata::operator<, first metadata is empty"));
    return true; // anything beats the empty metadata (incl. invalid ones)
  }

  SRIMETADATALOG(("SRIMetadata::operator<, alg1='%d'; alg2='%d'",
                  mAlgorithmType, aOther.mAlgorithmType));
  return (mAlgorithmType < aOther.mAlgorithmType);
}

bool
SRIMetadata::operator>(const SRIMetadata& aOther) const
{
  MOZ_ASSERT(false);
  return false;
}

SRIMetadata&
SRIMetadata::operator+=(const SRIMetadata& aOther)
{
  MOZ_ASSERT(!aOther.IsEmpty() && !IsEmpty());
  MOZ_ASSERT(aOther.IsValid() && IsValid());
  MOZ_ASSERT(mAlgorithmType == aOther.mAlgorithmType);

  // We only pull in the first element of the other metadata
  MOZ_ASSERT(aOther.mHashes.Length() == 1);
  if (mHashes.Length() < SRIMetadata::MAX_ALTERNATE_HASHES) {
    SRIMETADATALOG(("SRIMetadata::operator+=, appending another '%s' hash (new length=%d)",
                    mAlgorithm.get(), mHashes.Length()));
    mHashes.AppendElement(aOther.mHashes[0]);
  }

  MOZ_ASSERT(mHashes.Length() > 1);
  MOZ_ASSERT(mHashes.Length() <= SRIMetadata::MAX_ALTERNATE_HASHES);
  return *this;
}

bool
SRIMetadata::operator==(const SRIMetadata& aOther) const
{
  if (IsEmpty() || !IsValid()) {
    return false;
  }
  return mAlgorithmType == aOther.mAlgorithmType;
}

void
SRIMetadata::GetHash(uint32_t aIndex, nsCString* outHash) const
{
  MOZ_ASSERT(aIndex < SRIMetadata::MAX_ALTERNATE_HASHES);
  if (NS_WARN_IF(aIndex >= mHashes.Length())) {
    *outHash = nullptr;
    return;
  }
  *outHash = mHashes[aIndex];
}

void
SRIMetadata::GetHashType(int8_t* outType, uint32_t* outLength) const
{
  // these constants are defined in security/nss/lib/util/hasht.h and
  // netwerk/base/public/nsICryptoHash.idl
  switch (mAlgorithmType) {
    case nsICryptoHash::SHA256:
      *outLength = SHA256_LENGTH;
      break;
    case nsICryptoHash::SHA384:
      *outLength = SHA384_LENGTH;
      break;
    case nsICryptoHash::SHA512:
      *outLength = SHA512_LENGTH;
      break;
    default:
      *outLength = 0;
  }
  *outType = mAlgorithmType;
}

} // namespace dom
} // namespace mozilla

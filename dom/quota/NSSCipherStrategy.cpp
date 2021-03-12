/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NSSCipherStrategy.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>
#include "mozilla/Assertions.h"
#include "mozilla/ResultExtensions.h"

// NSS includes
#include "blapit.h"
#include "pk11pub.h"
#include "pkcs11t.h"
#include "seccomon.h"
#include "secmodt.h"

namespace mozilla::dom::quota {

static_assert(sizeof(NSSCipherStrategy::KeyType) == 32);
static_assert(NSSCipherStrategy::BlockPrefixLength == 32);
static_assert(NSSCipherStrategy::BasicBlockSize == 16);

Result<NSSCipherStrategy::KeyType, nsresult> NSSCipherStrategy::GenerateKey() {
  const auto slot = UniquePK11SlotInfo{PK11_GetInternalSlot()};
  if (slot == nullptr) {
    return Err(NS_ERROR_FAILURE);
  }
  const auto symKey = UniquePK11SymKey{PK11_KeyGen(
      slot.get(), CKM_CHACHA20_KEY_GEN, nullptr, sizeof(KeyType), nullptr)};
  if (symKey == nullptr) {
    return Err(NS_ERROR_FAILURE);
  }
  if (PK11_ExtractKeyValue(symKey.get()) != SECSuccess) {
    return Err(NS_ERROR_FAILURE);
  }
  // No need to free keyData as it is a buffer managed by symKey.
  SECItem* keyData = PK11_GetKeyData(symKey.get());
  if (keyData == nullptr) {
    return Err(NS_ERROR_FAILURE);
  }
  KeyType key;
  MOZ_RELEASE_ASSERT(keyData->len == key.size());
  std::copy(keyData->data, keyData->data + key.size(), key.data());
  return key;
}

nsresult NSSCipherStrategy::Init(const CipherMode aMode,
                                 const Span<const uint8_t> aKey,
                                 const Span<const uint8_t> aInitialIv) {
  MOZ_ASSERT_IF(CipherMode::Encrypt == aMode, aInitialIv.Length() == 32);

  mMode.init(aMode);

  mIv.AppendElements(aInitialIv);

  const auto slot = UniquePK11SlotInfo{PK11_GetInternalSlot()};
  if (slot == nullptr) {
    return NS_ERROR_FAILURE;
  }

  SECItem keyItem;
  keyItem.data = const_cast<uint8_t*>(aKey.Elements());
  keyItem.len = aKey.Length();
  const auto symKey = UniquePK11SymKey{
      PK11_ImportSymKey(slot.get(), CKM_CHACHA20_POLY1305, PK11_OriginUnwrap,
                        CKA_ENCRYPT, &keyItem, nullptr)};
  if (symKey == nullptr) {
    return NS_ERROR_FAILURE;
  }

  SECItem empty = {siBuffer, nullptr, 0};
  auto pk11Context = UniquePK11Context{PK11_CreateContextBySymKey(
      CKM_CHACHA20_POLY1305,
      CKA_NSS_MESSAGE |
          (CipherMode::Encrypt == aMode ? CKA_ENCRYPT : CKA_DECRYPT),
      symKey.get(), &empty)};
  if (pk11Context == nullptr) {
    return NS_ERROR_FAILURE;
  }

  mPK11Context.init(std::move(pk11Context));
  return NS_OK;
}

nsresult NSSCipherStrategy::Cipher(const Span<uint8_t> aIv,
                                   const Span<const uint8_t> aIn,
                                   const Span<uint8_t> aOut) {
  if (CipherMode::Encrypt == *mMode) {
    MOZ_RELEASE_ASSERT(aIv.Length() == mIv.Length());
    memcpy(aIv.Elements(), mIv.Elements(), aIv.Length());
  }

  // XXX make tag a separate parameter
  constexpr size_t tagLen = 16;
  const auto tag = Span{aIv}.Last(tagLen);
  // tag is const on decrypt, but returned on encrypt

  const auto iv = Span{aIv}.First(12);
  MOZ_ASSERT(tag.Length() + iv.Length() <= aIv.Length());

  int outLen;
  // aIn and aOut may not overlap resp. be the same, so we can't do this
  // in-place.
  const SECStatus rv = PK11_AEADOp(
      mPK11Context->get(), CKG_GENERATE_COUNTER,
      /* TODO use this for the discriminator */ 0, iv.Elements(), iv.Length(),
      nullptr, 0, aOut.Elements(), &outLen, aOut.Length(), tag.Elements(),
      tag.Length(), aIn.Elements(), aIn.Length());

  if (CipherMode::Encrypt == *mMode) {
    memcpy(mIv.Elements(), aIv.Elements(), aIv.Length());
  }

  return MapSECStatus(rv);
}

template <size_t N>
static std::array<uint8_t, N> MakeRandomData() {
  std::array<uint8_t, N> res;

  const auto rv = PK11_GenerateRandom(res.data(), res.size());
  /// XXX Allow return of error code to handle this gracefully.
  MOZ_RELEASE_ASSERT(rv == SECSuccess);

  return res;
}

std::array<uint8_t, NSSCipherStrategy::BlockPrefixLength>
NSSCipherStrategy::MakeBlockPrefix() {
  return MakeRandomData<BlockPrefixLength>();
}

Span<const uint8_t> NSSCipherStrategy::SerializeKey(const KeyType& aKey) {
  return Span(aKey);
}

NSSCipherStrategy::KeyType NSSCipherStrategy::DeserializeKey(
    const Span<const uint8_t>& aSerializedKey) {
  KeyType res;
  MOZ_ASSERT(res.size() == aSerializedKey.size());
  std::copy(aSerializedKey.cbegin(), aSerializedKey.cend(), res.begin());
  return res;
}

}  // namespace mozilla::dom::quota

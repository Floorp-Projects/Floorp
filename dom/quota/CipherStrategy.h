/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_CipherStrategy_h
#define mozilla_dom_quota_CipherStrategy_h

namespace mozilla::dom::quota {

enum class CipherMode { Encrypt, Decrypt };

// An implementation of the CipherStrategy concept must provide the following
// data members:
//
//   static constexpr size_t BlockPrefixLength;
//   static constexpr size_t BasicBlockSize;
//
// It must provide the following member types:
//
//   KeyType
//   BlockPrefixType, which must be an InputRange of type uint8_t and a
//                    SizedRange of size BlockPrefixLength
//
// It must provide the following member functions with compatible signatures:
//
//   static Result<KeyType, nsresult> GenerateKey();
//
//   nsresult Cipher(const CipherMode aMode, const KeyType& aKey,
//                   Span<uint8_t> aIv, Span<const uint8_t> aIn,
//                   Span<uint8_t> aOut);
//
//   BlockPrefixType MakeBlockPrefix();
//
//   Span<const uint8_t> SerializeKey(const KeyType& aKey);
//
//   Maybe<KeyType> DeserializeKey(const Span<const uint8_t>& aSerializedKey);

}  // namespace mozilla::dom::quota

#endif

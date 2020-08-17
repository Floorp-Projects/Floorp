/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_DummyCipherStrategy_h
#define mozilla_dom_quota_DummyCipherStrategy_h

#include "mozilla/dom/quota/CipherStrategy.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"

#include <array>

namespace mozilla::dom::quota {

struct DummyCipherStrategy {
  struct KeyType {};

  static constexpr size_t BlockPrefixLength = 8;
  static constexpr size_t BasicBlockSize = 4;

  static void DummyTransform(Span<const uint8_t> aIn, Span<uint8_t> aOut) {
    std::transform(aIn.cbegin(), aIn.cend(), aOut.begin(),
                   [](const uint8_t byte) { return byte ^ 42; });
  }

  static Result<KeyType, nsresult> GenerateKey() { return KeyType{}; }

  nsresult Init(CipherMode aCipherMode, Span<const uint8_t> aKey,
                Span<const uint8_t> aInitialIv = Span<const uint8_t>{}) {
    return NS_OK;
  }

  nsresult Cipher(Span<uint8_t> aIv, Span<const uint8_t> aIn,
                  Span<uint8_t> aOut) {
    DummyTransform(aIn, aOut);
    return NS_OK;
  }

  static std::array<uint8_t, BlockPrefixLength> MakeBlockPrefix() {
    return {{42, 43, 44, 45}};
  }

  static Span<const uint8_t> SerializeKey(const KeyType&) { return {}; }

  static KeyType DeserializeKey(const Span<const uint8_t>&) { return {}; }
};
}  // namespace mozilla::dom::quota

#endif

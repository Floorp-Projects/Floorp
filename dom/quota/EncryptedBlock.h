/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_EncryptedBlock_h
#define mozilla_dom_quota_EncryptedBlock_h

#include <cstdint>
#include <cstring>
#include <limits>
#include "mozilla/Assertions.h"
#include "mozilla/Span.h"
#include "nsTArray.h"

namespace mozilla::dom::quota {

// An encrypted block has the following format:
// - one basic block containing a uint16_t stating the actual payload length
// - one basic block containing the cipher prefix (tyipically a nonce)
// - encrypted payload up to the remainder of the specified overall size
// We currently assume the basic block size is the same as the cipher prefix
// length.
//
// XXX Actually, we don't need the actual payload length in every block. Only
// the last block may be incomplete. The tricky thing is just that it might be
// incomplete by just one or two bytes.
template <size_t CipherPrefixLength, size_t BasicBlockSize>
class EncryptedBlock {
 public:
  explicit EncryptedBlock(const size_t aOverallSize) {
    MOZ_RELEASE_ASSERT(aOverallSize >
                       CipherPrefixOffset() + CipherPrefixLength);
    MOZ_RELEASE_ASSERT(aOverallSize <= std::numeric_limits<uint16_t>::max());
    // XXX Do we need this to be fallible? Then we need a factory/init function.
    // But maybe that's not necessary as the block size is not user-provided and
    // small.
    mData.SetLength(aOverallSize);

    // Bug 1867394: Making sure to zero-initialize first block as there might
    // be some unused bytes in it which could expose sensitive data.
    // Currently, only sizeof(uint16_t) bytes gets used in the first block.
    std::fill(mData.begin(), mData.begin() + CipherPrefixOffset(), 0);
    SetActualPayloadLength(MaxPayloadLength());
  }

  size_t MaxPayloadLength() const {
    return mData.Length() - CipherPrefixLength - CipherPrefixOffset();
  }

  void SetActualPayloadLength(uint16_t aActualPayloadLength) {
    memcpy(mData.Elements(), &aActualPayloadLength, sizeof(uint16_t));
  }
  size_t ActualPayloadLength() const {
    return *reinterpret_cast<const uint16_t*>(mData.Elements());
  }

  using ConstSpan = Span<const uint8_t>;
  using MutableSpan = Span<uint8_t>;

  ConstSpan CipherPrefix() const {
    return WholeBlock().Subspan(CipherPrefixOffset(), CipherPrefixLength);
  }
  MutableSpan MutableCipherPrefix() {
    return MutableWholeBlock().Subspan(CipherPrefixOffset(),
                                       CipherPrefixLength);
  }

  ConstSpan Payload() const {
    return WholeBlock()
        .SplitAt(CipherPrefixOffset() + CipherPrefixLength)
        .second.First(RoundedUpToBasicBlockSize(ActualPayloadLength()));
  }
  MutableSpan MutablePayload() {
    return MutableWholeBlock()
        .SplitAt(CipherPrefixOffset() + CipherPrefixLength)
        .second.First(RoundedUpToBasicBlockSize(ActualPayloadLength()));
  }

  ConstSpan WholeBlock() const { return mData; }
  MutableSpan MutableWholeBlock() { return mData; }

 private:
  static constexpr size_t CipherPrefixOffset() {
    return RoundedUpToBasicBlockSize(sizeof(uint16_t));
  }

  static constexpr size_t RoundedUpToBasicBlockSize(const size_t aValue) {
    return (aValue + BasicBlockSize - 1) / BasicBlockSize * BasicBlockSize;
  }

  nsTArray<uint8_t> mData;
};

}  // namespace mozilla::dom::quota

#endif

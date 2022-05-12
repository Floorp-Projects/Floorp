/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Script decoding templates for processing byte data as UTF-8 or UTF-16. */

#ifndef mozilla_dom_ScriptDecoding_h
#define mozilla_dom_ScriptDecoding_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/CheckedInt.h"  // mozilla::CheckedInt
#include "mozilla/Encoding.h"    // mozilla::Decoder
#include "mozilla/Span.h"        // mozilla::Span
#include "mozilla/UniquePtr.h"   // mozilla::UniquePtr

#include <stddef.h>     // size_t
#include <stdint.h>     // uint8_t, uint32_t
#include <type_traits>  // std::is_same

namespace mozilla::dom {

template <typename Unit>
struct ScriptDecoding {
  static_assert(std::is_same<Unit, char16_t>::value ||
                    std::is_same<Unit, Utf8Unit>::value,
                "must be either UTF-8 or UTF-16");
};

template <>
struct ScriptDecoding<char16_t> {
  static CheckedInt<size_t> MaxBufferLength(const UniquePtr<Decoder>& aDecoder,
                                            uint32_t aByteLength) {
    return aDecoder->MaxUTF16BufferLength(aByteLength);
  }

  static size_t DecodeInto(const UniquePtr<Decoder>& aDecoder,
                           const Span<const uint8_t>& aSrc,
                           Span<char16_t> aDest, bool aEndOfSource) {
    uint32_t result;
    size_t read;
    size_t written;
    std::tie(result, read, written, std::ignore) =
        aDecoder->DecodeToUTF16(aSrc, aDest, aEndOfSource);
    MOZ_ASSERT(result == kInputEmpty);
    MOZ_ASSERT(read == aSrc.Length());
    MOZ_ASSERT(written <= aDest.Length());

    return written;
  }
};

template <>
struct ScriptDecoding<Utf8Unit> {
  static CheckedInt<size_t> MaxBufferLength(const UniquePtr<Decoder>& aDecoder,
                                            uint32_t aByteLength) {
    return aDecoder->MaxUTF8BufferLength(aByteLength);
  }

  static size_t DecodeInto(const UniquePtr<Decoder>& aDecoder,
                           const Span<const uint8_t>& aSrc,
                           Span<Utf8Unit> aDest, bool aEndOfSource) {
    uint32_t result;
    size_t read;
    size_t written;
    // Until C++ char8_t happens, our decoder APIs deal in |uint8_t| while
    // |Utf8Unit| internally deals with |char|, so there's inevitable impedance
    // mismatch between |aDest| as |Utf8Unit| and |AsWritableBytes(aDest)| as
    // |Span<uint8_t>|.  :-(  The written memory will be interpreted through
    // |char Utf8Unit::mValue| which is *permissible* because any object's
    // memory can be interpreted as |char|.  Unfortunately, until
    // twos-complement is mandated, we have to play fast and loose and *hope*
    // interpreting memory storing |uint8_t| as |char| will pick up the desired
    // wrapped-around value.  ¯\_(ツ)_/¯
    std::tie(result, read, written, std::ignore) =
        aDecoder->DecodeToUTF8(aSrc, AsWritableBytes(aDest), aEndOfSource);
    MOZ_ASSERT(result == kInputEmpty);
    MOZ_ASSERT(read == aSrc.Length());
    MOZ_ASSERT(written <= aDest.Length());

    return written;
  }
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_ScriptDecoding_h

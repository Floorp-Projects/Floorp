/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_ZLIBHELPER_H_
#define DOM_ZLIBHELPER_H_

#include "js/TypeDecls.h"
#include "zlib.h"

#include "mozilla/dom/CompressionStreamBinding.h"

namespace mozilla::dom {

// From the docs in
// https://searchfox.org/mozilla-central/source/modules/zlib/src/zlib.h
inline int8_t ZLibWindowBits(CompressionFormat format) {
  switch (format) {
    case CompressionFormat::Deflate:
      // The windowBits parameter is the base two logarithm of the window size
      // (the size of the history buffer). It should be in the range 8..15 for
      // this version of the library. Larger values of this parameter result
      // in better compression at the expense of memory usage.
      return 15;
    case CompressionFormat::Deflate_raw:
      // windowBits can also be –8..–15 for raw deflate. In this case,
      // -windowBits determines the window size.
      return -15;
    case CompressionFormat::Gzip:
      // windowBits can also be greater than 15 for optional gzip encoding.
      // Add 16 to windowBits to write a simple gzip header and trailer around
      // the compressed data instead of a zlib wrapper.
      return 31;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown compression format");
      return 0;
  }
}

enum ZLibFlush : uint8_t { No = Z_NO_FLUSH, Yes = Z_FINISH };

}  // namespace mozilla::dom

#endif  // DOM_ZLIBHELPER_H_

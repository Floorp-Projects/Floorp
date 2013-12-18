/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** @file
 * An enumeration used by RasterImage and Decoder to specify which 'strategy' to
 * use for image decoding - synchronous or asynchronous.
 */

#ifndef mozilla_imagelib_DecodeStrategy_h_
#define mozilla_imagelib_DecodeStrategy_h_

namespace mozilla {
namespace image {

enum DecodeStrategy {
  // DECODE_SYNC requests a synchronous decode, which will continue decoding
  // frames as long as it has more source data. It returns to the caller only
  // once decoding is complete (or until it needs more source data before
  // continuing). Because DECODE_SYNC can involve allocating new imgFrames, it
  // can only be run on the main thread.
  DECODE_SYNC,

  // DECODE_ASYNC requests an asynchronous decode, which will continue decoding
  // until it either finishes a frame or runs out of source data. Because
  // DECODE_ASYNC does not allocate new imgFrames, it can be safely run off the
  // main thread. (And hence workers in the decode pool always use it.)
  DECODE_ASYNC
};

} // namespace image
} // namespace mozilla

#endif

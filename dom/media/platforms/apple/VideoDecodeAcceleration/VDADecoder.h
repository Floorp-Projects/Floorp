/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Stub header for VideoDecodeAcceleration framework API.
// This is a private Framework on 10.6 see:
// https://developer.apple.com/library/mac/technotes/tn2267/_index.html
// We include our own copy so we can build on MacOS versions
// where it's not available.

#ifndef mozilla_VideoDecodeAcceleration_VDADecoder_h
#define mozilla_VideoDecodeAcceleration_VDADecoder_h

#include <CoreFoundation/CoreFoundation.h>
#include <CoreVideo/CoreVideo.h>

typedef uint32_t VDADecodeFrameFlags;
typedef uint32_t VDADecodeInfoFlags;

enum {
  kVDADecodeInfo_Asynchronous = 1UL << 0,
  kVDADecodeInfo_FrameDropped = 1UL << 1
};

enum {
  kVDADecoderFlush_EmitFrames = 1 << 0
};

typedef struct OpaqueVDADecoder* VDADecoder;

typedef void (*VDADecoderOutputCallback)
  (void* decompressionOutputRefCon,
   CFDictionaryRef frameInfo,
   OSStatus status,
   uint32_t infoFlags,
   CVImageBufferRef imageBuffer);

OSStatus
VDADecoderCreate(
  CFDictionaryRef decoderConfiguration,
  CFDictionaryRef destinationImageBufferAttributes, /* can be NULL */
  VDADecoderOutputCallback* outputCallback,
  void* decoderOutputCallbackRefcon,
  VDADecoder* decoderOut);

OSStatus
VDADecoderDecode(
  VDADecoder decoder,
  uint32_t decodeFlags,
  CFTypeRef compressedBuffer,
  CFDictionaryRef frameInfo); /* can be NULL */

OSStatus
VDADecoderFlush(
  VDADecoder decoder,
  uint32_t flushFlags);

OSStatus
VDADecoderDestroy(VDADecoder decoder);

#endif // mozilla_VideoDecodeAcceleration_VDADecoder_h

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DecoderTraits_h_
#define DecoderTraits_h_

#include "nsCOMPtr.h"

class nsAString;
class nsACString;

namespace mozilla {

class AbstractMediaDecoder;
class ChannelMediaDecoder;
class DecoderDoctorDiagnostics;
class MediaContainerType;
struct MediaDecoderInit;
class MediaDecoderOwner;
class MediaDecoderReader;

enum CanPlayStatus {
  CANPLAY_NO,
  CANPLAY_MAYBE,
  CANPLAY_YES
};

class DecoderTraits {
public:
  // Returns the CanPlayStatus indicating if we can handle this container type.
  static CanPlayStatus CanHandleContainerType(const MediaContainerType& aContainerType,
                                              DecoderDoctorDiagnostics* aDiagnostics);

  // Returns true if we should handle this MIME type when it appears
  // as an <object> or as a toplevel page. If, in practice, our support
  // for the type is more limited than appears in the wild, we should return
  // false here even if CanHandleMediaType would return true.
  static bool ShouldHandleMediaType(const char* aMIMEType,
                                    DecoderDoctorDiagnostics* aDiagnostics);

  // Create a decoder for the given aType. Returns null if we
  // were unable to create the decoder.
  static already_AddRefed<ChannelMediaDecoder> CreateDecoder(
    MediaDecoderInit& aInit,
    DecoderDoctorDiagnostics* aDiagnostics);

  // Create a reader for thew given MIME type aType. Returns null
  // if we were unable to create the reader.
  static MediaDecoderReader* CreateReader(const MediaContainerType& aType,
                                          AbstractMediaDecoder* aDecoder);

  // Returns true if MIME type aType is supported in video documents,
  // or false otherwise. Not all platforms support all MIME types, and
  // vice versa.
  static bool IsSupportedInVideoDocument(const nsACString& aType);

  // Convenience function that returns false if MOZ_FMP4 is not defined,
  // otherwise defers to MP4Decoder::IsSupportedType().
  static bool IsMP4SupportedType(const MediaContainerType& aType,
                                 DecoderDoctorDiagnostics* aDiagnostics);

  // Returns true if aType is MIME type of hls.
  static bool IsHttpLiveStreamingType(const MediaContainerType& aType);
};

} // namespace mozilla

#endif


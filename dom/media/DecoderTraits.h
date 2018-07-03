/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DecoderTraits_h_
#define DecoderTraits_h_

#include "mozilla/UniquePtr.h"
#include "nsStringFwd.h"
#include "nsTArray.h"

namespace mozilla {

class DecoderDoctorDiagnostics;
class MediaContainerType;
struct MediaFormatReaderInit;
class MediaFormatReader;
class TrackInfo;

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

  // Create a reader for thew given MIME type aType. Returns null
  // if we were unable to create the reader.
  static MediaFormatReader* CreateReader(const MediaContainerType& aType,
                                         MediaFormatReaderInit& aInit);

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

  // Returns true if aType is matroska type.
  static bool IsMatroskaType(const MediaContainerType& aType);

  static bool IsSupportedType(const MediaContainerType& aType);

  // Returns an array of all TrackInfo objects described by this type.
  static nsTArray<UniquePtr<TrackInfo>> GetTracksInfo(
    const MediaContainerType& aType);
};

} // namespace mozilla

#endif


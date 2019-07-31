/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebMWriter_h_
#define WebMWriter_h_

#include "ContainerWriter.h"
#include "nsAutoPtr.h"

namespace mozilla {

class EbmlComposer;

// Vorbis meta data structure
class VorbisMetadata : public TrackMetadataBase {
 public:
  nsTArray<uint8_t> mData;
  int32_t mChannels;
  float mSamplingFrequency;
  MetadataKind GetKind() const override { return METADATA_VORBIS; }
};

// VP8 meta data structure
class VP8Metadata : public TrackMetadataBase {
 public:
  int32_t mWidth;
  int32_t mHeight;
  int32_t mDisplayWidth;
  int32_t mDisplayHeight;
  MetadataKind GetKind() const override { return METADATA_VP8; }
};

/**
 * WebM writer helper
 * This class accepts encoder to set audio or video meta data or
 * encoded data to ebml Composer, and get muxing data through GetContainerData.
 * The ctor/dtor run in the MediaRecorder thread, others run in MediaEncoder
 * thread.
 */
class WebMWriter : public ContainerWriter {
 public:
  // Run in MediaRecorder thread
  WebMWriter();
  virtual ~WebMWriter();

  // WriteEncodedTrack inserts raw packets into WebM stream. Does not accept
  // any flags: any specified will be ignored. Writing is finalized via
  // flushing via GetContainerData().
  nsresult WriteEncodedTrack(const nsTArray<RefPtr<EncodedFrame>>& aData,
                             uint32_t aFlags = 0) override;

  // GetContainerData outputs multiplexing data.
  // aFlags indicates the muxer should enter into finished stage and flush out
  // queue data.
  nsresult GetContainerData(nsTArray<nsTArray<uint8_t>>* aOutputBufs,
                            uint32_t aFlags = 0) override;

  // Assign metadata into muxer
  nsresult SetMetadata(
      const nsTArray<RefPtr<TrackMetadataBase>>& aMetadata) override;

 private:
  nsAutoPtr<EbmlComposer> mEbmlComposer;
};

}  // namespace mozilla

#endif

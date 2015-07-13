/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VorbisTrackEncoder_h_
#define VorbisTrackEncoder_h_

#include "TrackEncoder.h"
#include "nsCOMPtr.h"
#include <vorbis/codec.h>

namespace mozilla {

class VorbisTrackEncoder : public AudioTrackEncoder
{
public:
  VorbisTrackEncoder();
  virtual ~VorbisTrackEncoder();

  already_AddRefed<TrackMetadataBase> GetMetadata() final override;

  nsresult GetEncodedTrack(EncodedFrameContainer& aData) final override;

protected:
  /**
   * http://xiph.org/vorbis/doc/libvorbis/vorbis_analysis_buffer.html
   * We use 1024 samples for the write buffer; libvorbis will construct packets
   * with the appropriate duration for the encoding mode internally.
   */
  int GetPacketDuration() final override {
    return 1024;
  }

  nsresult Init(int aChannels, int aSamplingRate) final override;

private:
  // Write Xiph-style lacing to aOutput.
  void WriteLacing(nsTArray<uint8_t> *aOutput, int32_t aLacing);

  // Get the encoded data from vorbis encoder and append into aData.
  void GetEncodedFrames(EncodedFrameContainer& aData);

  // vorbis codec members
  // Struct that stores all the static vorbis bitstream settings.
  vorbis_info mVorbisInfo;
  // Central working state for the PCM->packet encoder.
  vorbis_dsp_state mVorbisDsp;
  // Local working space for PCM->packet encode.
  vorbis_block mVorbisBlock;
};

} // namespace mozilla

#endif

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VP8TrackEncoder_h_
#define VP8TrackEncoder_h_

#include "TrackEncoder.h"
#include "vpx/vpx_codec.h"

namespace mozilla {

typedef struct vpx_codec_ctx vpx_codec_ctx_t;
typedef struct vpx_codec_enc_cfg vpx_codec_enc_cfg_t;
typedef struct vpx_image vpx_image_t;

/**
 * VP8TrackEncoder implements VideoTrackEncoder by using the libvpx library.
 * We implement a realtime and variable frame rate encoder. In order to achieve
 * that, there is a frame-drop encoding policy implemented in GetEncodedTrack.
 */
class VP8TrackEncoder : public VideoTrackEncoder {
  enum EncodeOperation {
    ENCODE_NORMAL_FRAME,  // VP8 track encoder works normally.
    ENCODE_I_FRAME,       // The next frame will be encoded as I-Frame.
    SKIP_FRAME,           // Skip the next frame.
  };

 public:
  VP8TrackEncoder(RefPtr<DriftCompensator> aDriftCompensator,
                  TrackRate aTrackRate, FrameDroppingMode aFrameDroppingMode);
  virtual ~VP8TrackEncoder();

  already_AddRefed<TrackMetadataBase> GetMetadata() final;

  nsresult GetEncodedTrack(EncodedFrameContainer& aData) final;

 protected:
  nsresult Init(int32_t aWidth, int32_t aHeight, int32_t aDisplayWidth,
                int32_t aDisplayHeight) final;

 private:
  // Get the EncodeOperation for next target frame.
  EncodeOperation GetNextEncodeOperation(TimeDuration aTimeElapsed,
                                         StreamTime aProcessedDuration);

  // Get the encoded data from encoder to aData.
  // Return value: false if the vpx_codec_get_cx_data returns null
  //               for EOS detection.
  nsresult GetEncodedPartitions(EncodedFrameContainer& aData);

  // Prepare the input data to the mVPXImageWrapper for encoding.
  nsresult PrepareRawFrame(VideoChunk& aChunk);

  // Re-configures an existing encoder with a new frame size.
  nsresult Reconfigure(int32_t aWidth, int32_t aHeight, int32_t aDisplayWidth,
                       int32_t aDisplayHeight);

  // Destroys the context and image wrapper. Does not de-allocate the structs.
  void Destroy();

  // Helper method to set the values on a VPX configuration.
  nsresult SetConfigurationValues(int32_t aWidth, int32_t aHeight,
                                  int32_t aDisplayWidth, int32_t aDisplayHeight,
                                  vpx_codec_enc_cfg_t& config);

  // Encoded timestamp.
  StreamTime mEncodedTimestamp = 0;

  // Total duration in mTrackRate extracted by GetEncodedPartitions().
  CheckedInt64 mExtractedDuration;

  // Total duration in microseconds extracted by GetEncodedPartitions().
  CheckedInt64 mExtractedDurationUs;

  // Muted frame, we only create it once.
  RefPtr<layers::Image> mMuteFrame;

  // I420 frame, for converting to I420.
  UniquePtr<uint8_t[]> mI420Frame;
  size_t mI420FrameSize = 0;

  /**
   * A duration of non-key frames in milliseconds.
   */
  StreamTime mDurationSinceLastKeyframe = 0;

  /**
   * A local segment queue which takes the raw data out from mRawSegment in the
   * call of GetEncodedTrack().
   */
  VideoSegment mSourceSegment;

  // VP8 relative members.
  // Codec context structure.
  nsAutoPtr<vpx_codec_ctx_t> mVPXContext;
  // Image Descriptor.
  nsAutoPtr<vpx_image_t> mVPXImageWrapper;
};

}  // namespace mozilla

#endif

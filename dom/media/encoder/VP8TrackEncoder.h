/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VP8TrackEncoder_h_
#define VP8TrackEncoder_h_

#include "TrackEncoder.h"

#include "mozilla/RollingMean.h"
#include "TimeUnits.h"
#include "vpx/vpx_codec.h"

namespace mozilla {

typedef struct vpx_codec_ctx vpx_codec_ctx_t;
typedef struct vpx_codec_enc_cfg vpx_codec_enc_cfg_t;
typedef struct vpx_image vpx_image_t;

class VP8Metadata;

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

  nsresult GetEncodedTrack(nsTArray<RefPtr<EncodedFrame>>& aData) final;

  void SetKeyFrameInterval(Maybe<TimeDuration> aKeyFrameInterval) final;

 protected:
  nsresult Init(int32_t aWidth, int32_t aHeight, int32_t aDisplayWidth,
                int32_t aDisplayHeight) final;

 private:
  // Get the EncodeOperation for next target frame.
  EncodeOperation GetNextEncodeOperation(TimeDuration aTimeElapsed,
                                         TimeDuration aProcessedDuration);

  // Get the encoded data from encoder to aData.
  // Return value: NS_ERROR_NOT_AVAILABABLE if the vpx_codec_get_cx_data returns
  //                                        null for EOS detection.
  //               NS_OK if some data was appended to aData.
  //               An error nsresult otherwise.
  nsresult GetEncodedPartitions(nsTArray<RefPtr<EncodedFrame>>& aData);

  // Prepare the input data to the mVPXImageWrapper for encoding.
  nsresult PrepareRawFrame(VideoChunk& aChunk);

  // Re-configures an existing encoder with a new frame size.
  nsresult Reconfigure(int32_t aWidth, int32_t aHeight, int32_t aDisplayWidth,
                       int32_t aDisplayHeight);

  // Destroys the context and image wrapper. Does not de-allocate the structs.
  void Destroy();

  // VP8 Metadata, set on successfuly Init and never modified again.
  RefPtr<VP8Metadata> mMetadata;

  // The width the encoder is currently configured with. The input frames to the
  // underlying encoder must match this width, i.e., the underlying encoder will
  // not do any resampling.
  int mFrameWidth = 0;

  // The height the encoder is currently configured with. The input frames to
  // the underlying encoder must match this height, i.e., the underlying encoder
  // will not do any resampling.
  int mFrameHeight = 0;

  // Encoded timestamp.
  TrackTime mEncodedTimestamp = 0;

  // Total duration in mTrackRate extracted by GetEncodedPartitions().
  CheckedInt64 mExtractedDuration;

  // Total duration extracted by GetEncodedPartitions().
  media::TimeUnit mExtractedDurationUs;

  // Muted frame, we only create it once.
  RefPtr<layers::Image> mMuteFrame;

  // I420 frame, for converting to I420.
  UniquePtr<uint8_t[]> mI420Frame;
  size_t mI420FrameSize = 0;

  /**
   * A duration of non-key frames in mTrackRate.
   */
  TrackTime mDurationSinceLastKeyframe = 0;

  /**
   * The max interval at which a keyframe gets forced (causing video quality
   * degradation). The encoder is configured to encode keyframes more often than
   * this, though it can vary based on frame rate.
   */
  TimeDuration mKeyFrameInterval;

  /**
   * A local segment queue which takes the raw data out from mRawSegment in the
   * call of GetEncodedTrack().
   */
  VideoSegment mSourceSegment;

  /**
   * The mean duration of recent frames.
   */
  RollingMean<TimeDuration, TimeDuration> mMeanFrameDuration{30};

  /**
   * The mean wall-clock time it took to encode recent frames.
   */
  RollingMean<TimeDuration, TimeDuration> mMeanFrameEncodeDuration{30};

  // VP8 relative members.
  // Codec context structure.
  vpx_codec_ctx_t mVPXContext;
  // Image Descriptor.
  vpx_image_t mVPXImageWrapper;
};

}  // namespace mozilla

#endif

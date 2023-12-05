/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VP8TrackEncoder_h_
#define VP8TrackEncoder_h_

#include "TrackEncoder.h"

#include <vpx/vpx_codec.h>

#include "mozilla/RollingMean.h"
#include "TimeUnits.h"

namespace mozilla {

typedef struct vpx_codec_ctx vpx_codec_ctx_t;
typedef struct vpx_codec_enc_cfg vpx_codec_enc_cfg_t;
typedef struct vpx_image vpx_image_t;

class VP8Metadata;

/**
 * VP8TrackEncoder implements VideoTrackEncoder by using the libvpx library.
 * We implement a realtime and variable frame rate encoder. In order to achieve
 * that, there is a frame-drop encoding policy implemented in Encode().
 */
class VP8TrackEncoder : public VideoTrackEncoder {
  enum EncodeOperation {
    ENCODE_NORMAL_FRAME,  // VP8 track encoder works normally.
    ENCODE_I_FRAME,       // The next frame will be encoded as I-Frame.
    SKIP_FRAME,           // Skip the next frame.
  };

 public:
  VP8TrackEncoder(RefPtr<DriftCompensator> aDriftCompensator,
                  TrackRate aTrackRate,
                  MediaQueue<EncodedFrame>& aEncodedDataQueue,
                  FrameDroppingMode aFrameDroppingMode,
                  Maybe<float> aKeyFrameIntervalFactor = Nothing());
  virtual ~VP8TrackEncoder();

  already_AddRefed<TrackMetadataBase> GetMetadata() final;

 protected:
  nsresult Init(int32_t aWidth, int32_t aHeight, int32_t aDisplayWidth,
                int32_t aDisplayHeight, float aEstimatedFrameRate) final;

 private:
  // Initiates the underlying vpx encoder.
  nsresult InitInternal(int32_t aWidth, int32_t aHeight,
                        int32_t aMaxKeyFrameDistance);

  // Get the EncodeOperation for next target frame.
  EncodeOperation GetNextEncodeOperation(TimeDuration aTimeElapsed,
                                         TimeDuration aProcessedDuration);

  // Extracts the encoded data from the underlying encoder and returns it.
  // Return value: An EncodedFrame if a frame was extracted.
  //               nullptr if we reached end-of-stream or nothing was available
  //                       from the underlying encoder.
  //               An error nsresult otherwise.
  Result<RefPtr<EncodedFrame>, nsresult> ExtractEncodedData();

  // Takes the data in aSegment, encodes it, extracts it, and pushes it to
  // mEncodedDataQueue.
  nsresult Encode(VideoSegment* aSegment) final;

  // Prepare the input data to the mVPXImageWrapper for encoding.
  nsresult PrepareRawFrame(VideoChunk& aChunk);

  // Re-configures an existing encoder with a new frame size.
  nsresult Reconfigure(int32_t aWidth, int32_t aHeight,
                       int32_t aMaxKeyFrameDistance);

  // Destroys the context and image wrapper. Does not de-allocate the structs.
  void Destroy();

  // Helper that calculates the desired max keyframe distance (vp8 config's
  // max_kf_dist) based on configured key frame interval and recent framerate.
  // Returns Nothing if not enough input data is available.
  Maybe<int32_t> CalculateMaxKeyFrameDistance(
      Maybe<float> aEstimatedFrameRate = Nothing()) const;

  void SetMaxKeyFrameDistance(int32_t aMaxKeyFrameDistance);

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

  // Total duration in mTrackRate extracted from the underlying encoder.
  CheckedInt64 mExtractedDuration;

  // Total duration extracted from the underlying encoder.
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
  const TimeDuration mKeyFrameInterval;

  /**
   * A factor used to multiply the estimated key-frame-interval based on
   * mKeyFrameInterval (ms) with when configuring kf_max_dist in the encoder.
   * The goal is to set it a bit below 1.0 to avoid falling back to forcing
   * keyframes.
   * NB that for purposes of testing the mKeyFrameInterval fallback this may be
   *    set to values higher than 1.0.
   */
  float mKeyFrameIntervalFactor;

  /**
   * Time when we last updated the key-frame-distance.
   */
  media::TimeUnit mLastKeyFrameDistanceUpdate;

  /**
   * The frame duration value last used to configure kf_max_dist.
   */
  Maybe<int32_t> mMaxKeyFrameDistance;

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

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
 * VP8TrackEncoder implements VideoTrackEncoder by using libvpx library.
 * We implement a realtime and fixed FPS encoder. In order to achieve that,
 * there is a pick target frame and drop frame encoding policy implemented in
 * GetEncodedTrack.
 */
class VP8TrackEncoder : public VideoTrackEncoder
{
  enum EncodeOperation {
    ENCODE_NORMAL_FRAME, // VP8 track encoder works normally.
    ENCODE_I_FRAME, // The next frame will be encoded as I-Frame.
    SKIP_FRAME, // Skip the next frame.
  };
public:
  VP8TrackEncoder();
  virtual ~VP8TrackEncoder();

  already_AddRefed<TrackMetadataBase> GetMetadata() final override;

  nsresult GetEncodedTrack(EncodedFrameContainer& aData) final override;

protected:
  nsresult Init(int32_t aWidth, int32_t aHeight,
                int32_t aDisplayWidth, int32_t aDisplayHeight,
                TrackRate aTrackRate) final override;

private:
  // Calculate the target frame's encoded duration.
  StreamTime CalculateEncodedDuration(StreamTime aDurationCopied);

  // Calculate the mRemainingTicks for next target frame.
  StreamTime CalculateRemainingTicks(StreamTime aDurationCopied,
                                     StreamTime aEncodedDuration);

  // Get the EncodeOperation for next target frame.
  EncodeOperation GetNextEncodeOperation(TimeDuration aTimeElapsed,
                                         StreamTime aProcessedDuration);

  // Get the encoded data from encoder to aData.
  // Return value: false if the vpx_codec_get_cx_data returns null
  //               for EOS detection.
  bool GetEncodedPartitions(EncodedFrameContainer& aData);

  // Prepare the input data to the mVPXImageWrapper for encoding.
  nsresult PrepareRawFrame(VideoChunk &aChunk);

  // Output frame rate.
  uint32_t mEncodedFrameRate;
  // Duration for the output frame, reciprocal to mEncodedFrameRate.
  StreamTime mEncodedFrameDuration;
  // Encoded timestamp.
  StreamTime mEncodedTimestamp;
  // Duration to the next encode frame.
  StreamTime mRemainingTicks;

  // Muted frame, we only create it once.
  RefPtr<layers::Image> mMuteFrame;

  // I420 frame, for converting to I420.
  nsTArray<uint8_t> mI420Frame;

  /**
   * A local segment queue which takes the raw data out from mRawSegment in the
   * call of GetEncodedTrack(). Since we implement the fixed FPS encoding
   * policy, it needs to be global in order to store the leftover segments
   * taken from mRawSegment.
   */
  VideoSegment mSourceSegment;

  // VP8 relative members.
  // Codec context structure.
  nsAutoPtr<vpx_codec_ctx_t> mVPXContext;
  // Image Descriptor.
  nsAutoPtr<vpx_image_t> mVPXImageWrapper;
};

} // namespace mozilla

#endif

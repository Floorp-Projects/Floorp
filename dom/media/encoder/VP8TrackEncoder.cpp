/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VP8TrackEncoder.h"

#include "GeckoProfiler.h"
#include "ImageToI420.h"
#include "LayersLogging.h"
#include "mozilla/gfx/2D.h"
#include "prsystem.h"
#include "VideoSegment.h"
#include "VideoUtils.h"
#include "vpx/vp8cx.h"
#include "vpx/vpx_encoder.h"
#include "WebMWriter.h"
#include "mozilla/media/MediaUtils.h"
#include "mozilla/dom/ImageUtils.h"
#include "mozilla/dom/ImageBitmapBinding.h"

namespace mozilla {

LazyLogModule gVP8TrackEncoderLog("VP8TrackEncoder");
#define VP8LOG(level, msg, ...) \
  MOZ_LOG(gVP8TrackEncoderLog, level, (msg, ##__VA_ARGS__))

#define DEFAULT_BITRATE_BPS 2500000
#define MAX_KEYFRAME_INTERVAL 600

using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::media;
using namespace mozilla::dom;

VP8TrackEncoder::VP8TrackEncoder(RefPtr<DriftCompensator> aDriftCompensator,
                                 TrackRate aTrackRate,
                                 FrameDroppingMode aFrameDroppingMode)
    : VideoTrackEncoder(std::move(aDriftCompensator), aTrackRate,
                        aFrameDroppingMode),
      mVPXContext(new vpx_codec_ctx_t()),
      mVPXImageWrapper(new vpx_image_t()) {
  MOZ_COUNT_CTOR(VP8TrackEncoder);
}

VP8TrackEncoder::~VP8TrackEncoder() {
  Destroy();
  MOZ_COUNT_DTOR(VP8TrackEncoder);
}

void VP8TrackEncoder::Destroy() {
  if (mInitialized) {
    vpx_codec_destroy(mVPXContext);
  }

  if (mVPXImageWrapper) {
    vpx_img_free(mVPXImageWrapper);
  }
  mInitialized = false;
}

nsresult VP8TrackEncoder::Init(int32_t aWidth, int32_t aHeight,
                               int32_t aDisplayWidth, int32_t aDisplayHeight) {
  if (aWidth < 1 || aHeight < 1 || aDisplayWidth < 1 || aDisplayHeight < 1) {
    return NS_ERROR_FAILURE;
  }

  if (mInitialized) {
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  // Encoder configuration structure.
  vpx_codec_enc_cfg_t config;
  nsresult rv = SetConfigurationValues(aWidth, aHeight, aDisplayWidth,
                                       aDisplayHeight, config);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  // Creating a wrapper to the image - setting image data to NULL. Actual
  // pointer will be set in encode. Setting align to 1, as it is meaningless
  // (actual memory is not allocated).
  vpx_img_wrap(mVPXImageWrapper, VPX_IMG_FMT_I420, mFrameWidth, mFrameHeight, 1,
               nullptr);

  vpx_codec_flags_t flags = 0;
  flags |= VPX_CODEC_USE_OUTPUT_PARTITION;
  if (vpx_codec_enc_init(mVPXContext, vpx_codec_vp8_cx(), &config, flags)) {
    return NS_ERROR_FAILURE;
  }

  vpx_codec_control(mVPXContext, VP8E_SET_STATIC_THRESHOLD, 1);
  vpx_codec_control(mVPXContext, VP8E_SET_CPUUSED, -6);
  vpx_codec_control(mVPXContext, VP8E_SET_TOKEN_PARTITIONS,
                    VP8_ONE_TOKENPARTITION);

  SetInitialized();

  return NS_OK;
}

nsresult VP8TrackEncoder::Reconfigure(int32_t aWidth, int32_t aHeight,
                                      int32_t aDisplayWidth,
                                      int32_t aDisplayHeight) {
  if (aWidth <= 0 || aHeight <= 0 || aDisplayWidth <= 0 ||
      aDisplayHeight <= 0) {
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  if (!mInitialized) {
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  // Recreate image wrapper
  vpx_img_free(mVPXImageWrapper);
  vpx_img_wrap(mVPXImageWrapper, VPX_IMG_FMT_I420, aWidth, aHeight, 1, nullptr);
  // Encoder configuration structure.
  vpx_codec_enc_cfg_t config;
  nsresult rv = SetConfigurationValues(aWidth, aHeight, aDisplayWidth,
                                       aDisplayHeight, config);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  // Set new configuration
  if (vpx_codec_enc_config_set(mVPXContext.get(), &config) != VPX_CODEC_OK) {
    VP8LOG(LogLevel::Error, "Failed to set new configuration");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult VP8TrackEncoder::SetConfigurationValues(int32_t aWidth,
                                                 int32_t aHeight,
                                                 int32_t aDisplayWidth,
                                                 int32_t aDisplayHeight,
                                                 vpx_codec_enc_cfg_t& config) {
  mFrameWidth = aWidth;
  mFrameHeight = aHeight;
  mDisplayWidth = aDisplayWidth;
  mDisplayHeight = aDisplayHeight;

  // Encoder configuration structure.
  memset(&config, 0, sizeof(vpx_codec_enc_cfg_t));
  if (vpx_codec_enc_config_default(vpx_codec_vp8_cx(), &config, 0)) {
    VP8LOG(LogLevel::Error, "Failed to get default configuration");
    return NS_ERROR_FAILURE;
  }

  config.g_w = mFrameWidth;
  config.g_h = mFrameHeight;
  // TODO: Maybe we should have various aFrameRate bitrate pair for each
  // devices? or for different platform

  // rc_target_bitrate needs kbit/s
  config.rc_target_bitrate =
      (mVideoBitrate != 0 ? mVideoBitrate : DEFAULT_BITRATE_BPS) / 1000;

  // Setting the time base of the codec
  config.g_timebase.num = 1;
  config.g_timebase.den = mTrackRate;

  config.g_error_resilient = 0;

  config.g_lag_in_frames = 0;  // 0- no frame lagging

  int32_t number_of_cores = PR_GetNumberOfProcessors();
  if (mFrameWidth * mFrameHeight > 1280 * 960 && number_of_cores >= 6) {
    config.g_threads = 3;  // 3 threads for 1080p.
  } else if (mFrameWidth * mFrameHeight > 640 * 480 && number_of_cores >= 3) {
    config.g_threads = 2;  // 2 threads for qHD/HD.
  } else {
    config.g_threads = 1;  // 1 thread for VGA or less
  }

  // rate control settings
  config.rc_dropframe_thresh = 0;
  config.rc_end_usage = VPX_VBR;
  config.g_pass = VPX_RC_ONE_PASS;
  // ffmpeg doesn't currently support streams that use resize.
  // Therefore, for safety, we should turn it off until it does.
  config.rc_resize_allowed = 0;
  config.rc_undershoot_pct = 100;
  config.rc_overshoot_pct = 15;
  config.rc_buf_initial_sz = 500;
  config.rc_buf_optimal_sz = 600;
  config.rc_buf_sz = 1000;

  // we set key frame interval to automatic and later manually
  // force key frame by setting VPX_EFLAG_FORCE_KF when mKeyFrameInterval > 0
  config.kf_mode = VPX_KF_AUTO;
  config.kf_max_dist = MAX_KEYFRAME_INTERVAL;

  return NS_OK;
}

already_AddRefed<TrackMetadataBase> VP8TrackEncoder::GetMetadata() {
  AUTO_PROFILER_LABEL("VP8TrackEncoder::GetMetadata", OTHER);

  MOZ_ASSERT(mInitialized || mCanceled);

  if (mCanceled || mEncodingComplete) {
    return nullptr;
  }

  if (!mInitialized) {
    return nullptr;
  }

  RefPtr<VP8Metadata> meta = new VP8Metadata();
  meta->mWidth = mFrameWidth;
  meta->mHeight = mFrameHeight;
  meta->mDisplayWidth = mDisplayWidth;
  meta->mDisplayHeight = mDisplayHeight;

  VP8LOG(LogLevel::Info,
         "GetMetadata() width=%d, height=%d, "
         "displayWidht=%d, displayHeight=%d",
         meta->mWidth, meta->mHeight, meta->mDisplayWidth,
         meta->mDisplayHeight);

  return meta.forget();
}

nsresult VP8TrackEncoder::GetEncodedPartitions(
    nsTArray<RefPtr<EncodedFrame>>& aData) {
  vpx_codec_iter_t iter = nullptr;
  EncodedFrame::FrameType frameType = EncodedFrame::VP8_P_FRAME;
  nsTArray<uint8_t> frameData;
  const vpx_codec_cx_pkt_t* pkt = nullptr;
  while ((pkt = vpx_codec_get_cx_data(mVPXContext, &iter)) != nullptr) {
    switch (pkt->kind) {
      case VPX_CODEC_CX_FRAME_PKT: {
        // Copy the encoded data from libvpx to frameData
        frameData.AppendElements((uint8_t*)pkt->data.frame.buf,
                                 pkt->data.frame.sz);
        break;
      }
      default: {
        break;
      }
    }
    // End of frame
    if ((pkt->data.frame.flags & VPX_FRAME_IS_FRAGMENT) == 0) {
      if (pkt->data.frame.flags & VPX_FRAME_IS_KEY) {
        frameType = EncodedFrame::VP8_I_FRAME;
      }
      break;
    }
  }

  if (!frameData.IsEmpty()) {
    // Copy the encoded data to aData.
    EncodedFrame* videoData = new EncodedFrame();
    videoData->mFrameType = frameType;

    // Convert the timestamp and duration to Usecs.
    CheckedInt64 timestamp = FramesToUsecs(pkt->data.frame.pts, mTrackRate);
    if (!timestamp.isValid()) {
      NS_ERROR("Microsecond timestamp overflow");
      return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
    }
    videoData->mTime = (uint64_t)timestamp.value();

    mExtractedDuration += pkt->data.frame.duration;
    if (!mExtractedDuration.isValid()) {
      NS_ERROR("Duration overflow");
      return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
    }

    CheckedInt64 totalDuration =
        FramesToUsecs(mExtractedDuration.value(), mTrackRate);
    if (!totalDuration.isValid()) {
      NS_ERROR("Duration overflow");
      return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
    }

    CheckedInt64 duration = totalDuration - mExtractedDurationUs;
    if (!duration.isValid()) {
      NS_ERROR("Duration overflow");
      return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
    }

    mExtractedDurationUs = totalDuration;
    videoData->mDuration = (uint64_t)duration.value();
    videoData->SwapInFrameData(frameData);
    VP8LOG(LogLevel::Verbose,
           "GetEncodedPartitions TimeStamp %" PRIu64 ", Duration %" PRIu64
           ", FrameType %d",
           videoData->mTime, videoData->mDuration, videoData->mFrameType);
    aData.AppendElement(videoData);
  }

  return pkt ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

template <int N>
static int Aligned(int aValue) {
  if (aValue < N) {
    return N;
  }

  // The `- 1` avoids overreaching when `aValue % N == 0`.
  return (((aValue - 1) / N) + 1) * N;
}

nsresult VP8TrackEncoder::PrepareRawFrame(VideoChunk& aChunk) {
  RefPtr<Image> img;
  if (aChunk.mFrame.GetForceBlack() || aChunk.IsNull()) {
    if (!mMuteFrame) {
      mMuteFrame =
          VideoFrame::CreateBlackImage(gfx::IntSize(mFrameWidth, mFrameHeight));
    }
    if (!mMuteFrame) {
      VP8LOG(LogLevel::Warning, "Failed to allocate black image of size %dx%d",
             mFrameWidth, mFrameHeight);
      return NS_OK;
    }
    img = mMuteFrame;
  } else {
    img = aChunk.mFrame.GetImage();
  }

  if (img->GetSize() != IntSize(mFrameWidth, mFrameHeight)) {
    VP8LOG(LogLevel::Info, "Dynamic resolution change (was %dx%d, now %dx%d).",
           mFrameWidth, mFrameHeight, img->GetSize().width,
           img->GetSize().height);

    gfx::IntSize intrinsicSize = aChunk.mFrame.GetIntrinsicSize();
    gfx::IntSize imgSize = aChunk.mFrame.GetImage()->GetSize();
    if (imgSize <= IntSize(mFrameWidth,
                           mFrameHeight) &&  // check buffer size instead
                                             // If the new size is less than or
                                             // equal to old, the existing
                                             // encoder instance can continue.
        NS_SUCCEEDED(Reconfigure(imgSize.width, imgSize.height,
                                 intrinsicSize.width, intrinsicSize.height))) {
      VP8LOG(LogLevel::Info, "Reconfigured VP8 encoder.");
    } else {
      // New frame size is larger; re-create the encoder.
      Destroy();
      nsresult rv = Init(imgSize.width, imgSize.height, intrinsicSize.width,
                         intrinsicSize.height);
      VP8LOG(LogLevel::Info, "Recreated VP8 encoder.");
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Clear image state from last frame
  mVPXImageWrapper->planes[VPX_PLANE_Y] = nullptr;
  mVPXImageWrapper->stride[VPX_PLANE_Y] = 0;
  mVPXImageWrapper->planes[VPX_PLANE_U] = nullptr;
  mVPXImageWrapper->stride[VPX_PLANE_U] = 0;
  mVPXImageWrapper->planes[VPX_PLANE_V] = nullptr;
  mVPXImageWrapper->stride[VPX_PLANE_V] = 0;

  int yStride = Aligned<16>(mFrameWidth);
  int yHeight = mFrameHeight;
  size_t yPlaneSize = yStride * yHeight;

  int uvStride = Aligned<16>((mFrameWidth + 1) / 2);
  int uvHeight = (mFrameHeight + 1) / 2;
  size_t uvPlaneSize = uvStride * uvHeight;

  size_t neededSize = yPlaneSize + uvPlaneSize * 2;

  if (neededSize > mI420FrameSize) {
    mI420Frame.reset(new (fallible) uint8_t[neededSize]);
  }

  if (!mI420Frame) {
    VP8LOG(LogLevel::Warning, "Allocating I420 frame of size %zu failed",
           neededSize);
    return NS_ERROR_FAILURE;
  }
  mI420FrameSize = neededSize;

  uint8_t* yChannel = &mI420Frame[0];
  uint8_t* uChannel = &mI420Frame[yPlaneSize];
  uint8_t* vChannel = &mI420Frame[yPlaneSize + uvPlaneSize];

  nsresult rv = ConvertToI420(img, yChannel, yStride, uChannel, uvStride,
                              vChannel, uvStride);

  if (NS_FAILED(rv)) {
    VP8LOG(LogLevel::Error, "Converting to I420 failed");
    return rv;
  }

  mVPXImageWrapper->planes[VPX_PLANE_Y] = yChannel;
  mVPXImageWrapper->stride[VPX_PLANE_Y] = yStride;
  mVPXImageWrapper->planes[VPX_PLANE_U] = uChannel;
  mVPXImageWrapper->stride[VPX_PLANE_U] = uvStride;
  mVPXImageWrapper->planes[VPX_PLANE_V] = vChannel;
  mVPXImageWrapper->stride[VPX_PLANE_V] = uvStride;

  return NS_OK;
}

// These two define value used in GetNextEncodeOperation to determine the
// EncodeOperation for next target frame.
#define I_FRAME_RATIO (0.5)
#define SKIP_FRAME_RATIO (0.75)

/**
 * Compares the elapsed time from the beginning of GetEncodedTrack and
 * the processed frame duration in mSourceSegment
 * in order to set the nextEncodeOperation for next target frame.
 */
VP8TrackEncoder::EncodeOperation VP8TrackEncoder::GetNextEncodeOperation(
    TimeDuration aTimeElapsed, StreamTime aProcessedDuration) {
  if (mFrameDroppingMode == FrameDroppingMode::DISALLOW) {
    return ENCODE_NORMAL_FRAME;
  }

  int64_t durationInUsec =
      FramesToUsecs(aProcessedDuration, mTrackRate).value();
  if (aTimeElapsed.ToMicroseconds() > (durationInUsec * SKIP_FRAME_RATIO)) {
    // The encoder is too slow.
    // We should skip next frame to consume the mSourceSegment.
    return SKIP_FRAME;
  } else if (aTimeElapsed.ToMicroseconds() > (durationInUsec * I_FRAME_RATIO)) {
    // The encoder is a little slow.
    // We force the encoder to encode an I-frame to accelerate.
    return ENCODE_I_FRAME;
  } else {
    return ENCODE_NORMAL_FRAME;
  }
}

/**
 * Encoding flow in GetEncodedTrack():
 * 1: Check the mInitialized state and the packet duration.
 * 2: Move the data from mRawSegment to mSourceSegment.
 * 3: Encode the video chunks in mSourceSegment in a for-loop.
 * 3.1: The duration is taken straight from the video chunk's duration.
 * 3.2: Setup the video chunk with mVPXImageWrapper by PrepareRawFrame().
 * 3.3: Pass frame to vp8 encoder by vpx_codec_encode().
 * 3.4: Get the encoded frame from encoder by GetEncodedPartitions().
 * 3.5: Set the nextEncodeOperation for the next target frame.
 *      There is a heuristic: If the frame duration we have processed in
 *      mSourceSegment is 100ms, means that we can't spend more than 100ms to
 *      encode it.
 * 4. Remove the encoded chunks in mSourceSegment after for-loop.
 */
nsresult VP8TrackEncoder::GetEncodedTrack(
    nsTArray<RefPtr<EncodedFrame>>& aData) {
  AUTO_PROFILER_LABEL("VP8TrackEncoder::GetEncodedTrack", OTHER);

  MOZ_ASSERT(mInitialized || mCanceled);

  if (mCanceled || mEncodingComplete) {
    return NS_ERROR_FAILURE;
  }

  if (!mInitialized) {
    return NS_ERROR_FAILURE;
  }

  TakeTrackData(mSourceSegment);

  StreamTime totalProcessedDuration = 0;
  TimeStamp timebase = TimeStamp::Now();
  EncodeOperation nextEncodeOperation = ENCODE_NORMAL_FRAME;

  for (VideoSegment::ChunkIterator iter(mSourceSegment); !iter.IsEnded();
       iter.Next()) {
    VideoChunk& chunk = *iter;
    VP8LOG(LogLevel::Verbose,
           "nextEncodeOperation is %d for frame of duration %" PRId64,
           nextEncodeOperation, chunk.GetDuration());

    // Encode frame.
    if (nextEncodeOperation != SKIP_FRAME) {
      nsresult rv = PrepareRawFrame(chunk);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

      // Encode the data with VP8 encoder
      int flags = 0;
      if (nextEncodeOperation == ENCODE_I_FRAME) {
        VP8LOG(LogLevel::Warning,
               "MediaRecorder lagging behind. Encoding keyframe.");
        flags |= VPX_EFLAG_FORCE_KF;
      }

      // Sum duration of non-key frames and force keyframe if exceeded the given
      // keyframe interval
      if (mKeyFrameInterval > 0) {
        if ((mDurationSinceLastKeyframe * 1000 / mTrackRate) >=
            mKeyFrameInterval) {
          mDurationSinceLastKeyframe = 0;
          flags |= VPX_EFLAG_FORCE_KF;
        }
        mDurationSinceLastKeyframe += chunk.GetDuration();
      }

      if (vpx_codec_encode(mVPXContext, mVPXImageWrapper, mEncodedTimestamp,
                           (unsigned long)chunk.GetDuration(), flags,
                           VPX_DL_REALTIME)) {
        VP8LOG(LogLevel::Error, "vpx_codec_encode failed to encode the frame.");
        return NS_ERROR_FAILURE;
      }
      // Get the encoded data from VP8 encoder.
      rv = GetEncodedPartitions(aData);
      if (rv != NS_OK && rv != NS_ERROR_NOT_AVAILABLE) {
        VP8LOG(LogLevel::Error, "GetEncodedPartitions failed.");
        return NS_ERROR_FAILURE;
      }
    } else {
      // SKIP_FRAME
      // Extend the duration of the last encoded data in aData
      // because this frame will be skipped.
      VP8LOG(LogLevel::Warning,
             "MediaRecorder lagging behind. Skipping a frame.");
      RefPtr<EncodedFrame> last = aData.LastElement();
      if (last) {
        mExtractedDuration += chunk.mDuration;
        if (!mExtractedDuration.isValid()) {
          NS_ERROR("skipped duration overflow");
          return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
        }

        CheckedInt64 totalDuration =
            FramesToUsecs(mExtractedDuration.value(), mTrackRate);
        CheckedInt64 skippedDuration = totalDuration - mExtractedDurationUs;
        mExtractedDurationUs = totalDuration;
        if (!skippedDuration.isValid()) {
          NS_ERROR("skipped duration overflow");
          return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
        }
        last->mDuration += static_cast<uint64_t>(skippedDuration.value());
      }
    }

    // Move forward the mEncodedTimestamp.
    mEncodedTimestamp += chunk.GetDuration();
    totalProcessedDuration += chunk.GetDuration();

    // Check what to do next.
    TimeDuration elapsedTime = TimeStamp::Now() - timebase;
    nextEncodeOperation =
        GetNextEncodeOperation(elapsedTime, totalProcessedDuration);
  }

  // Remove the chunks we have processed.
  mSourceSegment.Clear();

  // End of stream, pull the rest frames in encoder.
  if (mEndOfStream) {
    VP8LOG(LogLevel::Debug, "mEndOfStream is true");
    mEncodingComplete = true;
    // Bug 1243611, keep calling vpx_codec_encode and vpx_codec_get_cx_data
    // until vpx_codec_get_cx_data return null.
    while (true) {
      if (vpx_codec_encode(mVPXContext, nullptr, mEncodedTimestamp, 0, 0,
                           VPX_DL_REALTIME)) {
        return NS_ERROR_FAILURE;
      }
      nsresult rv = GetEncodedPartitions(aData);
      if (rv == NS_ERROR_NOT_AVAILABLE) {
        // End-of-stream
        break;
      }
      if (rv != NS_OK) {
        // Error
        return NS_ERROR_FAILURE;
      }
    }
  }

  return NS_OK;
}

}  // namespace mozilla

#undef VP8LOG

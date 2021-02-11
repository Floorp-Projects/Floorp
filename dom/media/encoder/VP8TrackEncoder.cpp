/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VP8TrackEncoder.h"

#include "DriftCompensation.h"
#include "GeckoProfiler.h"
#include "ImageToI420.h"
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
constexpr int DEFAULT_KEYFRAME_INTERVAL_MS = 1000;
constexpr int I420_STRIDE_ALIGN = 16;
constexpr int MAX_KEYFRAME_DISTANCE = 600;

using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::media;
using namespace mozilla::dom;

namespace {

template <int N>
static int Aligned(int aValue) {
  if (aValue < N) {
    return N;
  }

  // The `- 1` avoids overreaching when `aValue % N == 0`.
  return (((aValue - 1) / N) + 1) * N;
}

template <int Alignment>
size_t I420Size(int aWidth, int aHeight) {
  int yStride = Aligned<Alignment>(aWidth);
  int yHeight = aHeight;
  size_t yPlaneSize = yStride * yHeight;

  int uvStride = Aligned<Alignment>((aWidth + 1) / 2);
  int uvHeight = (aHeight + 1) / 2;
  size_t uvPlaneSize = uvStride * uvHeight;

  return yPlaneSize + uvPlaneSize * 2;
}

nsresult CreateEncoderConfig(int32_t aWidth, int32_t aHeight,
                             uint32_t aVideoBitrate, TrackRate aTrackRate,
                             vpx_codec_enc_cfg_t* config) {
  // Encoder configuration structure.
  memset(config, 0, sizeof(vpx_codec_enc_cfg_t));
  if (vpx_codec_enc_config_default(vpx_codec_vp8_cx(), config, 0)) {
    VP8LOG(LogLevel::Error, "Failed to get default configuration");
    return NS_ERROR_FAILURE;
  }

  config->g_w = aWidth;
  config->g_h = aHeight;
  // TODO: Maybe we should have various aFrameRate bitrate pair for each
  // devices? or for different platform

  // rc_target_bitrate needs kbit/s
  config->rc_target_bitrate =
      (aVideoBitrate != 0 ? aVideoBitrate : DEFAULT_BITRATE_BPS) / 1000;

  // Setting the time base of the codec
  config->g_timebase.num = 1;
  config->g_timebase.den = aTrackRate;

  // No error resilience as this is not intended for UDP transports
  config->g_error_resilient = 0;

  // Allow some frame lagging for large timeslices (when low latency is not
  // needed)
  /*std::min(10U, mKeyFrameInterval / 200)*/
  config->g_lag_in_frames = 0;

  int32_t number_of_cores = PR_GetNumberOfProcessors();
  if (aWidth * aHeight > 1920 * 1080 && number_of_cores >= 8) {
    config->g_threads = 4;  // 4 threads for > 1080p.
  } else if (aWidth * aHeight > 1280 * 960 && number_of_cores >= 6) {
    config->g_threads = 3;  // 3 threads for 1080p.
  } else if (aWidth * aHeight > 640 * 480 && number_of_cores >= 3) {
    config->g_threads = 2;  // 2 threads for qHD/HD.
  } else {
    config->g_threads = 1;  // 1 thread for VGA or less
  }

  // rate control settings

  // No frame dropping
  config->rc_dropframe_thresh = 0;
  // Variable bitrate
  config->rc_end_usage = VPX_VBR;
  // Single pass encoding
  config->g_pass = VPX_RC_ONE_PASS;
  // ffmpeg doesn't currently support streams that use resize.
  // Therefore, for safety, we should turn it off until it does.
  config->rc_resize_allowed = 0;
  // Allows 10% under target bitrate to compensate for prior overshoot
  config->rc_undershoot_pct = 200;
  // Allows 1.5% over target bitrate to compensate for prior undershoot
  config->rc_overshoot_pct = 200;
  // Tells the decoding application to buffer 500ms before beginning playback
  config->rc_buf_initial_sz = 500;
  // The decoding application will try to keep 600ms of buffer during playback
  config->rc_buf_optimal_sz = 600;
  // The decoding application may buffer 1000ms worth of encoded data
  config->rc_buf_sz = 1000;

  // We set key frame interval to automatic and try to set kf_max_dist so that
  // the encoder chooses to put keyframes slightly more often than
  // mKeyFrameInterval milliseconds (which will encode with VPX_EFLAG_FORCE_KF
  // when reached).
  config->kf_mode = VPX_KF_AUTO;
  config->kf_max_dist = MAX_KEYFRAME_DISTANCE;

  return NS_OK;
}
}  // namespace

VP8TrackEncoder::VP8TrackEncoder(RefPtr<DriftCompensator> aDriftCompensator,
                                 TrackRate aTrackRate,
                                 FrameDroppingMode aFrameDroppingMode)
    : VideoTrackEncoder(std::move(aDriftCompensator), aTrackRate,
                        aFrameDroppingMode),
      mKeyFrameInterval(
          TimeDuration::FromMilliseconds(DEFAULT_KEYFRAME_INTERVAL_MS)) {
  MOZ_COUNT_CTOR(VP8TrackEncoder);
}

VP8TrackEncoder::~VP8TrackEncoder() {
  Destroy();
  MOZ_COUNT_DTOR(VP8TrackEncoder);
}

void VP8TrackEncoder::Destroy() {
  if (mInitialized) {
    vpx_codec_destroy(&mVPXContext);
  }

  mInitialized = false;
}

void VP8TrackEncoder::SetKeyFrameInterval(
    Maybe<TimeDuration> aKeyFrameInterval) {
  const TimeDuration defaultInterval =
      TimeDuration::FromMilliseconds(DEFAULT_KEYFRAME_INTERVAL_MS);
  mKeyFrameInterval =
      std::min(aKeyFrameInterval.valueOr(defaultInterval), defaultInterval);
  VP8LOG(LogLevel::Debug, "%p, keyframe interval is now %.2fs", this,
         mKeyFrameInterval.ToSeconds());
}

nsresult VP8TrackEncoder::Init(int32_t aWidth, int32_t aHeight,
                               int32_t aDisplayWidth, int32_t aDisplayHeight) {
  if (aDisplayWidth < 1 || aDisplayHeight < 1) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = InitInternal(aWidth, aHeight);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(!mI420Frame);
  MOZ_ASSERT(mI420FrameSize == 0);
  const size_t neededSize = I420Size<I420_STRIDE_ALIGN>(aWidth, aHeight);
  mI420Frame.reset(new (fallible) uint8_t[neededSize]);
  mI420FrameSize = mI420Frame ? neededSize : 0;
  if (!mI420Frame) {
    VP8LOG(LogLevel::Warning, "Allocating I420 frame of size %zu failed",
           neededSize);
    return NS_ERROR_FAILURE;
  }
  vpx_img_wrap(&mVPXImageWrapper, VPX_IMG_FMT_I420, aWidth, aHeight,
               I420_STRIDE_ALIGN, mI420Frame.get());

  if (!mMetadata) {
    mMetadata = MakeAndAddRef<VP8Metadata>();
    mMetadata->mWidth = aWidth;
    mMetadata->mHeight = aHeight;
    mMetadata->mDisplayWidth = aDisplayWidth;
    mMetadata->mDisplayHeight = aDisplayHeight;

    VP8LOG(LogLevel::Info,
           "%p Init() created metadata. width=%d, height=%d, displayWidth=%d, "
           "displayHeight=%d",
           this, mMetadata->mWidth, mMetadata->mHeight,
           mMetadata->mDisplayWidth, mMetadata->mDisplayHeight);
  }

  return NS_OK;
}

nsresult VP8TrackEncoder::InitInternal(int32_t aWidth, int32_t aHeight) {
  if (aWidth < 1 || aHeight < 1) {
    return NS_ERROR_FAILURE;
  }

  if (mInitialized) {
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  VP8LOG(LogLevel::Debug, "%p InitInternal(). width=%d, height=%d", this,
         aWidth, aHeight);

  // Encoder configuration structure.
  vpx_codec_enc_cfg_t config;
  nsresult rv =
      CreateEncoderConfig(aWidth, aHeight, mVideoBitrate, mTrackRate, &config);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  vpx_codec_flags_t flags = 0;
  flags |= VPX_CODEC_USE_OUTPUT_PARTITION;
  if (vpx_codec_enc_init(&mVPXContext, vpx_codec_vp8_cx(), &config, flags)) {
    return NS_ERROR_FAILURE;
  }

  vpx_codec_control(&mVPXContext, VP8E_SET_STATIC_THRESHOLD, 1);
  vpx_codec_control(&mVPXContext, VP8E_SET_CPUUSED, 15);
  vpx_codec_control(&mVPXContext, VP8E_SET_TOKEN_PARTITIONS,
                    VP8_TWO_TOKENPARTITION);

  mFrameWidth = aWidth;
  mFrameHeight = aHeight;

  SetInitialized();

  return NS_OK;
}

nsresult VP8TrackEncoder::Reconfigure(int32_t aWidth, int32_t aHeight) {
  if (aWidth <= 0 || aHeight <= 0) {
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  if (!mInitialized) {
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  bool needsReInit = false;

  if (aWidth != mFrameWidth || aHeight != mFrameHeight) {
    VP8LOG(LogLevel::Info, "Dynamic resolution change (%dx%d -> %dx%d).",
           mFrameWidth, mFrameHeight, aWidth, aHeight);
    const size_t neededSize = I420Size<I420_STRIDE_ALIGN>(aWidth, aHeight);
    if (neededSize > mI420FrameSize) {
      needsReInit = true;
      mI420Frame.reset(new (fallible) uint8_t[neededSize]);
      mI420FrameSize = mI420Frame ? neededSize : 0;
    }
    if (!mI420Frame) {
      VP8LOG(LogLevel::Warning, "Allocating I420 frame of size %zu failed",
             neededSize);
      return NS_ERROR_FAILURE;
    }
    vpx_img_wrap(&mVPXImageWrapper, VPX_IMG_FMT_I420, aWidth, aHeight,
                 I420_STRIDE_ALIGN, mI420Frame.get());
  }

  if (needsReInit) {
    Destroy();
    nsresult rv = InitInternal(aWidth, aHeight);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    mInitialized = true;
    return NS_OK;
  }

  // Encoder configuration structure.
  vpx_codec_enc_cfg_t config;
  nsresult rv =
      CreateEncoderConfig(aWidth, aHeight, mVideoBitrate, mTrackRate, &config);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  // Set new configuration
  if (vpx_codec_enc_config_set(&mVPXContext, &config) != VPX_CODEC_OK) {
    VP8LOG(LogLevel::Error, "Failed to set new configuration");
    return NS_ERROR_FAILURE;
  }

  mFrameWidth = aWidth;
  mFrameHeight = aHeight;

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

  MOZ_ASSERT(mMetadata);
  return do_AddRef(mMetadata);
}

nsresult VP8TrackEncoder::GetEncodedPartitions(
    nsTArray<RefPtr<EncodedFrame>>& aData) {
  vpx_codec_iter_t iter = nullptr;
  EncodedFrame::FrameType frameType = EncodedFrame::VP8_P_FRAME;
  auto frameData = MakeRefPtr<EncodedFrame::FrameData>();
  const vpx_codec_cx_pkt_t* pkt = nullptr;
  while ((pkt = vpx_codec_get_cx_data(&mVPXContext, &iter)) != nullptr) {
    switch (pkt->kind) {
      case VPX_CODEC_CX_FRAME_PKT: {
        // Copy the encoded data from libvpx to frameData
        frameData->AppendElements((uint8_t*)pkt->data.frame.buf,
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

  if (!frameData->IsEmpty()) {
    if (pkt->data.frame.flags & VPX_FRAME_IS_KEY) {
      // Update the since-last-keyframe counter, and account for this frame's
      // time.
      TrackTime frameTime = pkt->data.frame.pts;
      DebugOnly<TrackTime> frameDuration = pkt->data.frame.duration;
      MOZ_ASSERT(frameTime + frameDuration <= mEncodedTimestamp);
      mDurationSinceLastKeyframe =
          std::min(mDurationSinceLastKeyframe, mEncodedTimestamp - frameTime);
    }

    // Convert the timestamp and duration to Usecs.
    media::TimeUnit timestamp =
        FramesToTimeUnit(pkt->data.frame.pts, mTrackRate);
    if (!timestamp.IsValid()) {
      NS_ERROR("Microsecond timestamp overflow");
      return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
    }

    mExtractedDuration += pkt->data.frame.duration;
    if (!mExtractedDuration.isValid()) {
      NS_ERROR("Duration overflow");
      return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
    }

    media::TimeUnit totalDuration =
        FramesToTimeUnit(mExtractedDuration.value(), mTrackRate);
    if (!totalDuration.IsValid()) {
      NS_ERROR("Duration overflow");
      return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
    }

    media::TimeUnit duration = totalDuration - mExtractedDurationUs;
    if (!duration.IsValid()) {
      NS_ERROR("Duration overflow");
      return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
    }

    mExtractedDurationUs = totalDuration;

    VP8LOG(LogLevel::Verbose,
           "GetEncodedPartitions TimeStamp %.2f, Duration %.2f, FrameType %d",
           timestamp.ToSeconds(), duration.ToSeconds(), frameType);

    // Copy the encoded data to aData.
    aData.AppendElement(MakeRefPtr<EncodedFrame>(
        timestamp, duration.ToMicroseconds(), PR_USEC_PER_SEC, frameType,
        std::move(frameData)));
  }

  return pkt ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

nsresult VP8TrackEncoder::PrepareRawFrame(VideoChunk& aChunk) {
  gfx::IntSize intrinsicSize = aChunk.mFrame.GetIntrinsicSize();
  RefPtr<Image> img;
  if (aChunk.mFrame.GetForceBlack() || aChunk.IsNull()) {
    if (!mMuteFrame || mMuteFrame->GetSize() != intrinsicSize) {
      mMuteFrame = VideoFrame::CreateBlackImage(intrinsicSize);
    }
    if (!mMuteFrame) {
      VP8LOG(LogLevel::Warning, "Failed to allocate black image of size %dx%d",
             intrinsicSize.width, intrinsicSize.height);
      return NS_OK;
    }
    img = mMuteFrame;
  } else {
    img = aChunk.mFrame.GetImage();
  }

  gfx::IntSize imgSize = img->GetSize();
  if (imgSize != IntSize(mFrameWidth, mFrameHeight)) {
    nsresult rv = Reconfigure(imgSize.width, imgSize.height);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  MOZ_ASSERT(mFrameWidth == imgSize.width);
  MOZ_ASSERT(mFrameHeight == imgSize.height);

  nsresult rv = ConvertToI420(img, mVPXImageWrapper.planes[VPX_PLANE_Y],
                              mVPXImageWrapper.stride[VPX_PLANE_Y],
                              mVPXImageWrapper.planes[VPX_PLANE_U],
                              mVPXImageWrapper.stride[VPX_PLANE_U],
                              mVPXImageWrapper.planes[VPX_PLANE_V],
                              mVPXImageWrapper.stride[VPX_PLANE_V]);
  if (NS_FAILED(rv)) {
    VP8LOG(LogLevel::Error, "Converting to I420 failed");
    return rv;
  }

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
    TimeDuration aTimeElapsed, TimeDuration aProcessedDuration) {
  if (mFrameDroppingMode == FrameDroppingMode::DISALLOW) {
    return ENCODE_NORMAL_FRAME;
  }

  if (aTimeElapsed.ToSeconds() >
      aProcessedDuration.ToSeconds() * SKIP_FRAME_RATIO) {
    // The encoder is too slow.
    // We should skip next frame to consume the mSourceSegment.
    return SKIP_FRAME;
  }

  if (aTimeElapsed.ToSeconds() >
      aProcessedDuration.ToSeconds() * I_FRAME_RATIO) {
    // The encoder is a little slow.
    // We force the encoder to encode an I-frame to accelerate.
    return ENCODE_I_FRAME;
  }

  return ENCODE_NORMAL_FRAME;
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

  EncodeOperation nextEncodeOperation = ENCODE_NORMAL_FRAME;

  for (VideoSegment::ChunkIterator iter(mSourceSegment); !iter.IsEnded();
       iter.Next()) {
    TimeStamp timebase = TimeStamp::Now();
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

      // Sum duration of non-key frames and force keyframe if exceeded the
      // given keyframe interval
      if (mKeyFrameInterval > TimeDuration::FromSeconds(0)) {
        if (FramesToTimeUnit(mDurationSinceLastKeyframe, mTrackRate)
                .ToTimeDuration() >= mKeyFrameInterval) {
          VP8LOG(LogLevel::Warning,
                 "Reached mKeyFrameInterval without seeing a keyframe. Forcing "
                 "one. time: %.2f, interval: %.2f",
                 FramesToTimeUnit(mDurationSinceLastKeyframe, mTrackRate)
                     .ToSeconds(),
                 mKeyFrameInterval.ToSeconds());
          mDurationSinceLastKeyframe = 0;
          flags |= VPX_EFLAG_FORCE_KF;
        }
        mDurationSinceLastKeyframe += chunk.GetDuration();
      }

      if (vpx_codec_encode(&mVPXContext, &mVPXImageWrapper, mEncodedTimestamp,
                           (unsigned long)chunk.GetDuration(), flags,
                           VPX_DL_REALTIME)) {
        VP8LOG(LogLevel::Error, "vpx_codec_encode failed to encode the frame.");
        return NS_ERROR_FAILURE;
      }

      // Move forward the mEncodedTimestamp.
      mEncodedTimestamp += chunk.GetDuration();

      // Get the encoded data from VP8 encoder.
      rv = GetEncodedPartitions(aData);
      if (rv != NS_OK && rv != NS_ERROR_NOT_AVAILABLE) {
        VP8LOG(LogLevel::Error, "GetEncodedPartitions failed.");
        return NS_ERROR_FAILURE;
      }
    } else {
      // SKIP_FRAME

      // Move forward the mEncodedTimestamp.
      mEncodedTimestamp += chunk.GetDuration();

      // Extend the duration of the last encoded data in aData
      // because this frame will be skipped.
      VP8LOG(LogLevel::Warning,
             "MediaRecorder lagging behind. Skipping a frame.");

      mExtractedDuration += chunk.mDuration;
      if (!mExtractedDuration.isValid()) {
        NS_ERROR("skipped duration overflow");
        return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
      }

      media::TimeUnit totalDuration =
          FramesToTimeUnit(mExtractedDuration.value(), mTrackRate);
      media::TimeUnit skippedDuration = totalDuration - mExtractedDurationUs;
      mExtractedDurationUs = totalDuration;
      if (!skippedDuration.IsValid()) {
        NS_ERROR("skipped duration overflow");
        return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
      }

      {
        auto& last = aData.LastElement();
        MOZ_DIAGNOSTIC_ASSERT(aData.LastElement());
        uint64_t longerDuration =
            last->mDuration + skippedDuration.ToMicroseconds();
        auto longerFrame = MakeRefPtr<EncodedFrame>(
            last->mTime, longerDuration, last->mDurationBase, last->mFrameType,
            last->mFrameData);
        std::swap(last, longerFrame);
        MOZ_ASSERT(last->mDuration == longerDuration);
      }
    }

    mMeanFrameEncodeDuration.insert(TimeStamp::Now() - timebase);
    mMeanFrameDuration.insert(
        FramesToTimeUnit(chunk.GetDuration(), mTrackRate).ToTimeDuration());
    nextEncodeOperation = GetNextEncodeOperation(
        mMeanFrameEncodeDuration.mean(), mMeanFrameDuration.mean());
  }

  // Remove the chunks we have processed.
  mSourceSegment.Clear();

  // End of stream, pull the rest frames in encoder.
  if (mEndOfStream) {
    VP8LOG(LogLevel::Debug, "mEndOfStream is true");
    mEncodingComplete = true;
    if (mI420Frame) {
      mI420Frame = nullptr;
      mI420FrameSize = 0;
    }
    // Bug 1243611, keep calling vpx_codec_encode and vpx_codec_get_cx_data
    // until vpx_codec_get_cx_data return null.
    while (true) {
      if (vpx_codec_encode(&mVPXContext, nullptr, mEncodedTimestamp, 0, 0,
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

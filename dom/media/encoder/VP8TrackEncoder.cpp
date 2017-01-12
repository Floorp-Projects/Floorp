/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VP8TrackEncoder.h"
#include "GeckoProfiler.h"
#include "LayersLogging.h"
#include "libyuv.h"
#include "mozilla/gfx/2D.h"
#include "prsystem.h"
#include "VideoSegment.h"
#include "VideoUtils.h"
#include "vpx/vp8cx.h"
#include "vpx/vpx_encoder.h"
#include "WebMWriter.h"
#include "mozilla/media/MediaUtils.h"

namespace mozilla {

LazyLogModule gVP8TrackEncoderLog("VP8TrackEncoder");
#define VP8LOG(msg, ...) MOZ_LOG(gVP8TrackEncoderLog, mozilla::LogLevel::Debug, \
                                  (msg, ##__VA_ARGS__))
// Debug logging macro with object pointer and class name.

#define DEFAULT_BITRATE_BPS 2500000

using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::media;

static already_AddRefed<SourceSurface>
GetSourceSurface(already_AddRefed<Image> aImg)
{
  RefPtr<Image> img = aImg;
  if (!img) {
    return nullptr;
  }

  if (!img->AsGLImage() || NS_IsMainThread()) {
    RefPtr<SourceSurface> surf = img->GetAsSourceSurface();
    return surf.forget();
  }

  // GLImage::GetAsSourceSurface() only supports main thread
  RefPtr<SourceSurface> surf;
  RefPtr<Runnable> runnable = NewRunnableFrom([img, &surf]() -> nsresult {
    surf = img->GetAsSourceSurface();
    return NS_OK;
  });

  NS_DispatchToMainThread(runnable, NS_DISPATCH_SYNC);
  return surf.forget();
}

VP8TrackEncoder::VP8TrackEncoder(TrackRate aTrackRate)
  : VideoTrackEncoder(aTrackRate)
  , mEncodedTimestamp(0)
  , mVPXContext(new vpx_codec_ctx_t())
  , mVPXImageWrapper(new vpx_image_t())
{
  MOZ_COUNT_CTOR(VP8TrackEncoder);
}

VP8TrackEncoder::~VP8TrackEncoder()
{
  if (mInitialized) {
    vpx_codec_destroy(mVPXContext);
  }

  if (mVPXImageWrapper) {
    vpx_img_free(mVPXImageWrapper);
  }
  MOZ_COUNT_DTOR(VP8TrackEncoder);
}

nsresult
VP8TrackEncoder::Init(int32_t aWidth, int32_t aHeight, int32_t aDisplayWidth,
                      int32_t aDisplayHeight)
{
  if (aWidth < 1 || aHeight < 1 || aDisplayWidth < 1 || aDisplayHeight < 1) {
    return NS_ERROR_FAILURE;
  }

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  mFrameWidth = aWidth;
  mFrameHeight = aHeight;
  mDisplayWidth = aDisplayWidth;
  mDisplayHeight = aDisplayHeight;

  // Encoder configuration structure.
  vpx_codec_enc_cfg_t config;
  memset(&config, 0, sizeof(vpx_codec_enc_cfg_t));
  if (vpx_codec_enc_config_default(vpx_codec_vp8_cx(), &config, 0)) {
    return NS_ERROR_FAILURE;
  }

  // Creating a wrapper to the image - setting image data to NULL. Actual
  // pointer will be set in encode. Setting align to 1, as it is meaningless
  // (actual memory is not allocated).
  vpx_img_wrap(mVPXImageWrapper, VPX_IMG_FMT_I420,
               mFrameWidth, mFrameHeight, 1, nullptr);

  config.g_w = mFrameWidth;
  config.g_h = mFrameHeight;
  // TODO: Maybe we should have various aFrameRate bitrate pair for each devices?
  // or for different platform

  // rc_target_bitrate needs kbit/s
  config.rc_target_bitrate = (mVideoBitrate != 0 ? mVideoBitrate : DEFAULT_BITRATE_BPS)/1000;

  // Setting the time base of the codec
  config.g_timebase.num = 1;
  config.g_timebase.den = mTrackRate;

  config.g_error_resilient = 0;

  config.g_lag_in_frames = 0; // 0- no frame lagging

  int32_t number_of_cores = PR_GetNumberOfProcessors();
  if (mFrameWidth * mFrameHeight > 1280 * 960 && number_of_cores >= 6) {
    config.g_threads = 3; // 3 threads for 1080p.
  } else if (mFrameWidth * mFrameHeight > 640 * 480 && number_of_cores >= 3) {
    config.g_threads = 2; // 2 threads for qHD/HD.
  } else {
    config.g_threads = 1; // 1 thread for VGA or less
  }

  // rate control settings
  config.rc_dropframe_thresh = 0;
  config.rc_end_usage = VPX_CBR;
  config.g_pass = VPX_RC_ONE_PASS;
  // ffmpeg doesn't currently support streams that use resize.
  // Therefore, for safety, we should turn it off until it does.
  config.rc_resize_allowed = 0;
  config.rc_undershoot_pct = 100;
  config.rc_overshoot_pct = 15;
  config.rc_buf_initial_sz = 500;
  config.rc_buf_optimal_sz = 600;
  config.rc_buf_sz = 1000;

  config.kf_mode = VPX_KF_AUTO;
  // Ensure that we can output one I-frame per second.
  config.kf_max_dist = 60;

  vpx_codec_flags_t flags = 0;
  flags |= VPX_CODEC_USE_OUTPUT_PARTITION;
  if (vpx_codec_enc_init(mVPXContext, vpx_codec_vp8_cx(), &config, flags)) {
    return NS_ERROR_FAILURE;
  }

  vpx_codec_control(mVPXContext, VP8E_SET_STATIC_THRESHOLD, 1);
  vpx_codec_control(mVPXContext, VP8E_SET_CPUUSED, -6);
  vpx_codec_control(mVPXContext, VP8E_SET_TOKEN_PARTITIONS,
                    VP8_ONE_TOKENPARTITION);

  mInitialized = true;
  mon.NotifyAll();

  return NS_OK;
}

already_AddRefed<TrackMetadataBase>
VP8TrackEncoder::GetMetadata()
{
  PROFILER_LABEL("VP8TrackEncoder", "GetMetadata",
    js::ProfileEntry::Category::OTHER);
  {
    // Wait if mEncoder is not initialized.
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    while (!mCanceled && !mInitialized) {
      mon.Wait();
    }
  }

  if (mCanceled || mEncodingComplete) {
    return nullptr;
  }

  RefPtr<VP8Metadata> meta = new VP8Metadata();
  meta->mWidth = mFrameWidth;
  meta->mHeight = mFrameHeight;
  meta->mDisplayWidth = mDisplayWidth;
  meta->mDisplayHeight = mDisplayHeight;

  return meta.forget();
}

bool
VP8TrackEncoder::GetEncodedPartitions(EncodedFrameContainer& aData)
{
  vpx_codec_iter_t iter = nullptr;
  EncodedFrame::FrameType frameType = EncodedFrame::VP8_P_FRAME;
  nsTArray<uint8_t> frameData;
  const vpx_codec_cx_pkt_t *pkt = nullptr;
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
    videoData->SetFrameType(frameType);
    // Convert the timestamp and duration to Usecs.
    CheckedInt64 timestamp = FramesToUsecs(pkt->data.frame.pts, mTrackRate);
    if (timestamp.isValid()) {
      videoData->SetTimeStamp((uint64_t)timestamp.value());
    }
    CheckedInt64 duration = FramesToUsecs(pkt->data.frame.duration, mTrackRate);
    if (duration.isValid()) {
      videoData->SetDuration((uint64_t)duration.value());
    }
    videoData->SwapInFrameData(frameData);
    VP8LOG("GetEncodedPartitions TimeStamp %lld Duration %lld\n",
           videoData->GetTimeStamp(), videoData->GetDuration());
    VP8LOG("frameType %d\n", videoData->GetFrameType());
    aData.AppendEncodedFrame(videoData);
  }

  return !!pkt;
}

static bool isYUV420(const PlanarYCbCrImage::Data *aData)
{
  if (aData->mYSize == aData->mCbCrSize * 2) {
    return true;
  }
  return false;
}

static bool isYUV422(const PlanarYCbCrImage::Data *aData)
{
  if ((aData->mYSize.width == aData->mCbCrSize.width * 2) &&
      (aData->mYSize.height == aData->mCbCrSize.height)) {
    return true;
  }
  return false;
}

static bool isYUV444(const PlanarYCbCrImage::Data *aData)
{
  if (aData->mYSize == aData->mCbCrSize) {
    return true;
  }
  return false;
}

nsresult VP8TrackEncoder::PrepareRawFrame(VideoChunk &aChunk)
{
  RefPtr<Image> img;
  if (aChunk.mFrame.GetForceBlack() || aChunk.IsNull()) {
    if (!mMuteFrame) {
      mMuteFrame = VideoFrame::CreateBlackImage(gfx::IntSize(mFrameWidth, mFrameHeight));
      MOZ_ASSERT(mMuteFrame);
    }
    img = mMuteFrame;
  } else {
    img = aChunk.mFrame.GetImage();
  }

  if (img->GetSize() != IntSize(mFrameWidth, mFrameHeight)) {
    VP8LOG("Dynamic resolution changes (was %dx%d, now %dx%d) are unsupported\n",
           mFrameWidth, mFrameHeight, img->GetSize().width, img->GetSize().height);
    return NS_ERROR_FAILURE;
  }

  ImageFormat format = img->GetFormat();
  if (format == ImageFormat::PLANAR_YCBCR) {
    PlanarYCbCrImage* yuv = static_cast<PlanarYCbCrImage *>(img.get());

    MOZ_RELEASE_ASSERT(yuv);
    if (!yuv->IsValid()) {
      NS_WARNING("PlanarYCbCrImage is not valid");
      return NS_ERROR_FAILURE;
    }
    const PlanarYCbCrImage::Data *data = yuv->GetData();

    if (isYUV420(data) && !data->mCbSkip) {
      // 420 planar, no need for conversions
      mVPXImageWrapper->planes[VPX_PLANE_Y] = data->mYChannel;
      mVPXImageWrapper->planes[VPX_PLANE_U] = data->mCbChannel;
      mVPXImageWrapper->planes[VPX_PLANE_V] = data->mCrChannel;
      mVPXImageWrapper->stride[VPX_PLANE_Y] = data->mYStride;
      mVPXImageWrapper->stride[VPX_PLANE_U] = data->mCbCrStride;
      mVPXImageWrapper->stride[VPX_PLANE_V] = data->mCbCrStride;

      return NS_OK;
    }
  }

  // Not 420 planar, have to convert
  uint32_t yPlaneSize = mFrameWidth * mFrameHeight;
  uint32_t halfWidth = (mFrameWidth + 1) / 2;
  uint32_t halfHeight = (mFrameHeight + 1) / 2;
  uint32_t uvPlaneSize = halfWidth * halfHeight;

  if (mI420Frame.IsEmpty()) {
    mI420Frame.SetLength(yPlaneSize + uvPlaneSize * 2);
  }

  uint8_t *y = mI420Frame.Elements();
  uint8_t *cb = mI420Frame.Elements() + yPlaneSize;
  uint8_t *cr = mI420Frame.Elements() + yPlaneSize + uvPlaneSize;

  if (format == ImageFormat::PLANAR_YCBCR) {
    PlanarYCbCrImage* yuv = static_cast<PlanarYCbCrImage *>(img.get());

    MOZ_RELEASE_ASSERT(yuv);
    if (!yuv->IsValid()) {
      NS_WARNING("PlanarYCbCrImage is not valid");
      return NS_ERROR_FAILURE;
    }
    const PlanarYCbCrImage::Data *data = yuv->GetData();

    int rv;
    std::string yuvFormat;
    if (isYUV420(data) && data->mCbSkip) {
      // If mCbSkip is set, we assume it's nv12 or nv21.
      if (data->mCbChannel < data->mCrChannel) { // nv12
        rv = libyuv::NV12ToI420(data->mYChannel, data->mYStride,
                                data->mCbChannel, data->mCbCrStride,
                                y, mFrameWidth,
                                cb, halfWidth,
                                cr, halfWidth,
                                mFrameWidth, mFrameHeight);
        yuvFormat = "NV12";
      } else { // nv21
        rv = libyuv::NV21ToI420(data->mYChannel, data->mYStride,
                                data->mCrChannel, data->mCbCrStride,
                                y, mFrameWidth,
                                cb, halfWidth,
                                cr, halfWidth,
                                mFrameWidth, mFrameHeight);
        yuvFormat = "NV21";
      }
    } else if (isYUV444(data) && !data->mCbSkip) {
      rv = libyuv::I444ToI420(data->mYChannel, data->mYStride,
                              data->mCbChannel, data->mCbCrStride,
                              data->mCrChannel, data->mCbCrStride,
                              y, mFrameWidth,
                              cb, halfWidth,
                              cr, halfWidth,
                              mFrameWidth, mFrameHeight);
      yuvFormat = "I444";
    } else if (isYUV422(data) && !data->mCbSkip) {
      rv = libyuv::I422ToI420(data->mYChannel, data->mYStride,
                              data->mCbChannel, data->mCbCrStride,
                              data->mCrChannel, data->mCbCrStride,
                              y, mFrameWidth,
                              cb, halfWidth,
                              cr, halfWidth,
                              mFrameWidth, mFrameHeight);
      yuvFormat = "I422";
    } else {
      VP8LOG("Unsupported planar format\n");
      NS_ASSERTION(false, "Unsupported planar format");
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    if (rv != 0) {
      VP8LOG("Converting an %s frame to I420 failed\n", yuvFormat.c_str());
      return NS_ERROR_FAILURE;
    }

    VP8LOG("Converted an %s frame to I420\n", yuvFormat.c_str());
  } else {
    // Not YCbCr at all. Try to get access to the raw data and convert.

    RefPtr<SourceSurface> surf = GetSourceSurface(img.forget());
    if (!surf) {
      VP8LOG("Getting surface from %s image failed\n", Stringify(format).c_str());
      return NS_ERROR_FAILURE;
    }

    RefPtr<DataSourceSurface> data = surf->GetDataSurface();
    if (!data) {
      VP8LOG("Getting data surface from %s image with %s (%s) surface failed\n",
             Stringify(format).c_str(), Stringify(surf->GetType()).c_str(),
             Stringify(surf->GetFormat()).c_str());
      return NS_ERROR_FAILURE;
    }

    DataSourceSurface::ScopedMap map(data, DataSourceSurface::READ);
    if (!map.IsMapped()) {
      VP8LOG("Reading DataSourceSurface from %s image with %s (%s) surface failed\n",
             Stringify(format).c_str(), Stringify(surf->GetType()).c_str(),
             Stringify(surf->GetFormat()).c_str());
      return NS_ERROR_FAILURE;
    }

    int rv;
    switch (surf->GetFormat()) {
      case SurfaceFormat::B8G8R8A8:
      case SurfaceFormat::B8G8R8X8:
        rv = libyuv::ARGBToI420(static_cast<uint8*>(map.GetData()),
                                map.GetStride(),
                                y, mFrameWidth,
                                cb, halfWidth,
                                cr, halfWidth,
                                mFrameWidth, mFrameHeight);
        break;
      case SurfaceFormat::R5G6B5_UINT16:
        rv = libyuv::RGB565ToI420(static_cast<uint8*>(map.GetData()),
                                  map.GetStride(),
                                  y, mFrameWidth,
                                  cb, halfWidth,
                                  cr, halfWidth,
                                  mFrameWidth, mFrameHeight);
        break;
      default:
        VP8LOG("Unsupported SourceSurface format %s\n",
               Stringify(surf->GetFormat()).c_str());
        NS_ASSERTION(false, "Unsupported SourceSurface format");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    if (rv != 0) {
      VP8LOG("%s to I420 conversion failed\n",
             Stringify(surf->GetFormat()).c_str());
      return NS_ERROR_FAILURE;
    }

    VP8LOG("Converted a %s frame to I420\n",
           Stringify(surf->GetFormat()).c_str());
  }

  mVPXImageWrapper->planes[VPX_PLANE_Y] = y;
  mVPXImageWrapper->planes[VPX_PLANE_U] = cb;
  mVPXImageWrapper->planes[VPX_PLANE_V] = cr;
  mVPXImageWrapper->stride[VPX_PLANE_Y] = mFrameWidth;
  mVPXImageWrapper->stride[VPX_PLANE_U] = halfWidth;
  mVPXImageWrapper->stride[VPX_PLANE_V] = halfWidth;

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
VP8TrackEncoder::EncodeOperation
VP8TrackEncoder::GetNextEncodeOperation(TimeDuration aTimeElapsed,
                                        StreamTime aProcessedDuration)
{
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
nsresult
VP8TrackEncoder::GetEncodedTrack(EncodedFrameContainer& aData)
{
  PROFILER_LABEL("VP8TrackEncoder", "GetEncodedTrack",
    js::ProfileEntry::Category::OTHER);
  bool EOS;
  {
    // Move all the samples from mRawSegment to mSourceSegment. We only hold
    // the monitor in this block.
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    // Wait if mEncoder is not initialized, or when not enough raw data, but is
    // not the end of stream nor is being canceled.
    while (!mCanceled && (!mInitialized ||
           (mRawSegment.GetDuration() + mSourceSegment.GetDuration() == 0 &&
            !mEndOfStream))) {
      mon.Wait();
    }
    if (mCanceled || mEncodingComplete) {
      return NS_ERROR_FAILURE;
    }
    mSourceSegment.AppendFrom(&mRawSegment);
    EOS = mEndOfStream;
  }

  StreamTime totalProcessedDuration = 0;
  TimeStamp timebase = TimeStamp::Now();
  EncodeOperation nextEncodeOperation = ENCODE_NORMAL_FRAME;

  for (VideoSegment::ChunkIterator iter(mSourceSegment);
       !iter.IsEnded(); iter.Next()) {
    VideoChunk &chunk = *iter;
    VP8LOG("nextEncodeOperation is %d for frame of duration %lld\n",
           nextEncodeOperation, chunk.GetDuration());

    // Encode frame.
    if (nextEncodeOperation != SKIP_FRAME) {
      nsresult rv = PrepareRawFrame(chunk);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

      // Encode the data with VP8 encoder
      int flags = (nextEncodeOperation == ENCODE_NORMAL_FRAME) ?
                  0 : VPX_EFLAG_FORCE_KF;
      if (vpx_codec_encode(mVPXContext, mVPXImageWrapper, mEncodedTimestamp,
                           (unsigned long)chunk.GetDuration(), flags,
                           VPX_DL_REALTIME)) {
        return NS_ERROR_FAILURE;
      }
      // Get the encoded data from VP8 encoder.
      GetEncodedPartitions(aData);
    } else {
      // SKIP_FRAME
      // Extend the duration of the last encoded data in aData
      // because this frame will be skip.
      NS_WARNING("MediaRecorder lagging behind. Skipping a frame.");
      RefPtr<EncodedFrame> last = aData.GetEncodedFrames().LastElement();
      if (last) {
        last->SetDuration(last->GetDuration() + chunk.GetDuration());
      }
    }

    // Move forward the mEncodedTimestamp.
    mEncodedTimestamp += chunk.GetDuration();
    totalProcessedDuration += chunk.GetDuration();

    // Check what to do next.
    TimeDuration elapsedTime = TimeStamp::Now() - timebase;
    nextEncodeOperation = GetNextEncodeOperation(elapsedTime,
                                                 totalProcessedDuration);
  }

  // Remove the chunks we have processed.
  mSourceSegment.Clear();

  // End of stream, pull the rest frames in encoder.
  if (EOS) {
    VP8LOG("mEndOfStream is true\n");
    mEncodingComplete = true;
    // Bug 1243611, keep calling vpx_codec_encode and vpx_codec_get_cx_data
    // until vpx_codec_get_cx_data return null.

    do {
      if (vpx_codec_encode(mVPXContext, nullptr, mEncodedTimestamp,
                           0, 0, VPX_DL_REALTIME)) {
        return NS_ERROR_FAILURE;
      }
    } while(GetEncodedPartitions(aData));
  }

  return NS_OK ;
}

} // namespace mozilla

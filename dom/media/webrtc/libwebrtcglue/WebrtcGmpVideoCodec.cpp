/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcGmpVideoCodec.h"

#include <utility>
#include <vector>

#include "GMPLog.h"
#include "MainThreadUtils.h"
#include "VideoConduit.h"
#include "gmp-video-frame-encoded.h"
#include "gmp-video-frame-i420.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/SyncRunnable.h"
#include "nsServiceManagerUtils.h"
#include "transport/runnable_utils.h"
#include "api/video/video_frame_type.h"
#include "common_video/include/video_frame_buffer.h"
#include "media/base/media_constants.h"
// #include "rtc_base/bind.h"

namespace mozilla {

// QP scaling thresholds.
static const int kLowH264QpThreshold = 24;
static const int kHighH264QpThreshold = 37;

// Encoder.
WebrtcGmpVideoEncoder::WebrtcGmpVideoEncoder(
    const webrtc::SdpVideoFormat& aFormat, std::string aPCHandle)
    : mGMP(nullptr),
      mInitting(false),
      mHost(nullptr),
      mMaxPayloadSize(0),
      mFormatParams(aFormat.parameters),
      mCallbackMutex("WebrtcGmpVideoEncoder encoded callback mutex"),
      mCallback(nullptr),
      mPCHandle(std::move(aPCHandle)),
      mInputImageMap("WebrtcGmpVideoEncoder::mInputImageMap") {
  mCodecParams.mGMPApiVersion = 0;
  mCodecParams.mCodecType = kGMPVideoCodecInvalid;
  mCodecParams.mPLType = 0;
  mCodecParams.mWidth = 0;
  mCodecParams.mHeight = 0;
  mCodecParams.mStartBitrate = 0;
  mCodecParams.mMaxBitrate = 0;
  mCodecParams.mMinBitrate = 0;
  mCodecParams.mMaxFramerate = 0;
  mCodecParams.mFrameDroppingOn = false;
  mCodecParams.mKeyFrameInterval = 0;
  mCodecParams.mQPMax = 0;
  mCodecParams.mNumberOfSimulcastStreams = 0;
  mCodecParams.mMode = kGMPCodecModeInvalid;
  mCodecParams.mLogLevel = GetGMPLibraryLogLevel();
  MOZ_ASSERT(!mPCHandle.empty());
}

WebrtcGmpVideoEncoder::~WebrtcGmpVideoEncoder() {
  // We should not have been destroyed if we never closed our GMP
  MOZ_ASSERT(!mGMP);
}

static int WebrtcFrameTypeToGmpFrameType(webrtc::VideoFrameType aIn,
                                         GMPVideoFrameType* aOut) {
  MOZ_ASSERT(aOut);
  switch (aIn) {
    case webrtc::VideoFrameType::kVideoFrameKey:
      *aOut = kGMPKeyFrame;
      break;
    case webrtc::VideoFrameType::kVideoFrameDelta:
      *aOut = kGMPDeltaFrame;
      break;
    case webrtc::VideoFrameType::kEmptyFrame:
      *aOut = kGMPSkipFrame;
      break;
    default:
      MOZ_CRASH("Unexpected webrtc::FrameType");
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

static int GmpFrameTypeToWebrtcFrameType(GMPVideoFrameType aIn,
                                         webrtc::VideoFrameType* aOut) {
  MOZ_ASSERT(aOut);
  switch (aIn) {
    case kGMPKeyFrame:
      *aOut = webrtc::VideoFrameType::kVideoFrameKey;
      break;
    case kGMPDeltaFrame:
      *aOut = webrtc::VideoFrameType::kVideoFrameDelta;
      break;
    case kGMPSkipFrame:
      *aOut = webrtc::VideoFrameType::kEmptyFrame;
      break;
    default:
      MOZ_CRASH("Unexpected GMPVideoFrameType");
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

static int SizeNumBytes(GMPBufferType aBufferType) {
  switch (aBufferType) {
    case GMP_BufferSingle:
      return 0;
    case GMP_BufferLength8:
      return 1;
    case GMP_BufferLength16:
      return 2;
    case GMP_BufferLength24:
      return 3;
    case GMP_BufferLength32:
      return 4;
    default:
      MOZ_CRASH("Unexpected buffer type");
  }
}

int32_t WebrtcGmpVideoEncoder::InitEncode(
    const webrtc::VideoCodec* aCodecSettings,
    const webrtc::VideoEncoder::Settings& aSettings) {
  if (!mMPS) {
    mMPS = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  }
  MOZ_ASSERT(mMPS);

  if (!mGMPThread) {
    if (NS_WARN_IF(NS_FAILED(mMPS->GetThread(getter_AddRefs(mGMPThread))))) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }

  MOZ_ASSERT(aCodecSettings->numberOfSimulcastStreams == 1,
             "Simulcast not implemented for GMP-H264");

  // Bug XXXXXX: transfer settings from codecSettings to codec.
  GMPVideoCodec codecParams;
  memset(&codecParams, 0, sizeof(codecParams));

  codecParams.mGMPApiVersion = kGMPVersion34;
  codecParams.mLogLevel = GetGMPLibraryLogLevel();
  codecParams.mStartBitrate = aCodecSettings->startBitrate;
  codecParams.mMinBitrate = aCodecSettings->minBitrate;
  codecParams.mMaxBitrate = aCodecSettings->maxBitrate;
  codecParams.mMaxFramerate = aCodecSettings->maxFramerate;

  memset(&mCodecSpecificInfo.codecSpecific, 0,
         sizeof(mCodecSpecificInfo.codecSpecific));
  mCodecSpecificInfo.codecType = webrtc::kVideoCodecH264;
  mCodecSpecificInfo.codecSpecific.H264.packetization_mode =
      mFormatParams.count(cricket::kH264FmtpPacketizationMode) == 1 &&
              mFormatParams.at(cricket::kH264FmtpPacketizationMode) == "1"
          ? webrtc::H264PacketizationMode::NonInterleaved
          : webrtc::H264PacketizationMode::SingleNalUnit;

  uint32_t maxPayloadSize = aSettings.max_payload_size;
  if (mCodecSpecificInfo.codecSpecific.H264.packetization_mode ==
      webrtc::H264PacketizationMode::NonInterleaved) {
    maxPayloadSize = 0;  // No limit, use FUAs
  }

  if (aCodecSettings->mode == webrtc::VideoCodecMode::kScreensharing) {
    codecParams.mMode = kGMPScreensharing;
  } else {
    codecParams.mMode = kGMPRealtimeVideo;
  }

  codecParams.mWidth = aCodecSettings->width;
  codecParams.mHeight = aCodecSettings->height;

  RefPtr<GmpInitDoneRunnable> initDone(new GmpInitDoneRunnable(mPCHandle));
  mGMPThread->Dispatch(
      WrapRunnableNM(WebrtcGmpVideoEncoder::InitEncode_g,
                     RefPtr<WebrtcGmpVideoEncoder>(this), codecParams,
                     aSettings.number_of_cores, maxPayloadSize, initDone),
      NS_DISPATCH_NORMAL);

  // Since init of the GMP encoder is a multi-step async dispatch (including
  // dispatches to main), and since this function is invoked on main, there's
  // no safe way to block until this init is done. If an error occurs, we'll
  // handle it later.
  return WEBRTC_VIDEO_CODEC_OK;
}

/* static */
void WebrtcGmpVideoEncoder::InitEncode_g(
    const RefPtr<WebrtcGmpVideoEncoder>& aThis,
    const GMPVideoCodec& aCodecParams, int32_t aNumberOfCores,
    uint32_t aMaxPayloadSize, const RefPtr<GmpInitDoneRunnable>& aInitDone) {
  nsTArray<nsCString> tags;
  tags.AppendElement("h264"_ns);
  UniquePtr<GetGMPVideoEncoderCallback> callback(
      new InitDoneCallback(aThis, aInitDone, aCodecParams));
  aThis->mInitting = true;
  aThis->mMaxPayloadSize = aMaxPayloadSize;
  nsresult rv = aThis->mMPS->GetGMPVideoEncoder(nullptr, &tags, ""_ns,
                                                std::move(callback));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    GMP_LOG_DEBUG("GMP Encode: GetGMPVideoEncoder failed");
    aThis->Close_g();
    aInitDone->Dispatch(WEBRTC_VIDEO_CODEC_ERROR,
                        "GMP Encode: GetGMPVideoEncoder failed");
  }
}

int32_t WebrtcGmpVideoEncoder::GmpInitDone(GMPVideoEncoderProxy* aGMP,
                                           GMPVideoHost* aHost,
                                           std::string* aErrorOut) {
  if (!mInitting || !aGMP || !aHost) {
    *aErrorOut =
        "GMP Encode: Either init was aborted, "
        "or init failed to supply either a GMP Encoder or GMP host.";
    if (aGMP) {
      // This could destroy us, since aGMP may be the last thing holding a ref
      // Return immediately.
      aGMP->Close();
    }
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  mInitting = false;

  if (mGMP && mGMP != aGMP) {
    Close_g();
  }

  mGMP = aGMP;
  mHost = aHost;
  mCachedPluginId = Some(mGMP->GetPluginId());
  mInitPluginEvent.Notify(*mCachedPluginId);
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcGmpVideoEncoder::GmpInitDone(GMPVideoEncoderProxy* aGMP,
                                           GMPVideoHost* aHost,
                                           const GMPVideoCodec& aCodecParams,
                                           std::string* aErrorOut) {
  int32_t r = GmpInitDone(aGMP, aHost, aErrorOut);
  if (r != WEBRTC_VIDEO_CODEC_OK) {
    // We might have been destroyed if GmpInitDone failed.
    // Return immediately.
    return r;
  }
  mCodecParams = aCodecParams;
  return InitEncoderForSize(aCodecParams.mWidth, aCodecParams.mHeight,
                            aErrorOut);
}

void WebrtcGmpVideoEncoder::Close_g() {
  GMPVideoEncoderProxy* gmp(mGMP);
  mGMP = nullptr;
  mHost = nullptr;
  mInitting = false;

  if (mCachedPluginId) {
    mReleasePluginEvent.Notify(*mCachedPluginId);
  }
  mCachedPluginId = Nothing();

  if (gmp) {
    // Do this last, since this could cause us to be destroyed
    gmp->Close();
  }
}

int32_t WebrtcGmpVideoEncoder::InitEncoderForSize(unsigned short aWidth,
                                                  unsigned short aHeight,
                                                  std::string* aErrorOut) {
  mCodecParams.mWidth = aWidth;
  mCodecParams.mHeight = aHeight;
  // Pass dummy codecSpecific data for now...
  nsTArray<uint8_t> codecSpecific;

  GMPErr err =
      mGMP->InitEncode(mCodecParams, codecSpecific, this, 1, mMaxPayloadSize);
  if (err != GMPNoErr) {
    *aErrorOut = "GMP Encode: InitEncode failed";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcGmpVideoEncoder::Encode(
    const webrtc::VideoFrame& aInputImage,
    const std::vector<webrtc::VideoFrameType>* aFrameTypes) {
  MOZ_ASSERT(aInputImage.width() >= 0 && aInputImage.height() >= 0);
  if (!aFrameTypes) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // It is safe to copy aInputImage here because the frame buffer is held by
  // a refptr.
  mGMPThread->Dispatch(WrapRunnableNM(&WebrtcGmpVideoEncoder::Encode_g,
                                      RefPtr<WebrtcGmpVideoEncoder>(this),
                                      aInputImage, *aFrameTypes),
                       NS_DISPATCH_NORMAL);

  return WEBRTC_VIDEO_CODEC_OK;
}

void WebrtcGmpVideoEncoder::RegetEncoderForResolutionChange(
    uint32_t aWidth, uint32_t aHeight,
    const RefPtr<GmpInitDoneRunnable>& aInitDone) {
  Close_g();

  UniquePtr<GetGMPVideoEncoderCallback> callback(
      new InitDoneForResolutionChangeCallback(this, aInitDone, aWidth,
                                              aHeight));

  // OpenH264 codec (at least) can't handle dynamic input resolution changes
  // re-init the plugin when the resolution changes
  // XXX allow codec to indicate it doesn't need re-init!
  nsTArray<nsCString> tags;
  tags.AppendElement("h264"_ns);
  mInitting = true;
  if (NS_WARN_IF(NS_FAILED(mMPS->GetGMPVideoEncoder(nullptr, &tags, ""_ns,
                                                    std::move(callback))))) {
    aInitDone->Dispatch(WEBRTC_VIDEO_CODEC_ERROR,
                        "GMP Encode: GetGMPVideoEncoder failed");
  }
}

void WebrtcGmpVideoEncoder::Encode_g(
    const RefPtr<WebrtcGmpVideoEncoder>& aEncoder,
    webrtc::VideoFrame aInputImage,
    std::vector<webrtc::VideoFrameType> aFrameTypes) {
  if (!aEncoder->mGMP) {
    // destroyed via Terminate(), failed to init, or just not initted yet
    GMP_LOG_DEBUG("GMP Encode: not initted yet");
    return;
  }
  MOZ_ASSERT(aEncoder->mHost);

  if (static_cast<uint32_t>(aInputImage.width()) !=
          aEncoder->mCodecParams.mWidth ||
      static_cast<uint32_t>(aInputImage.height()) !=
          aEncoder->mCodecParams.mHeight) {
    GMP_LOG_DEBUG("GMP Encode: resolution change from %ux%u to %dx%d",
                  aEncoder->mCodecParams.mWidth, aEncoder->mCodecParams.mHeight,
                  aInputImage.width(), aInputImage.height());

    RefPtr<GmpInitDoneRunnable> initDone(
        new GmpInitDoneRunnable(aEncoder->mPCHandle));
    aEncoder->RegetEncoderForResolutionChange(aInputImage.width(),
                                              aInputImage.height(), initDone);
    if (!aEncoder->mGMP) {
      // We needed to go async to re-get the encoder. Bail.
      return;
    }
  }

  GMPVideoFrame* ftmp = nullptr;
  GMPErr err = aEncoder->mHost->CreateFrame(kGMPI420VideoFrame, &ftmp);
  if (err != GMPNoErr) {
    GMP_LOG_DEBUG("GMP Encode: failed to create frame on host");
    return;
  }
  GMPUniquePtr<GMPVideoi420Frame> frame(static_cast<GMPVideoi420Frame*>(ftmp));
  const webrtc::I420BufferInterface* input_image =
      aInputImage.video_frame_buffer()->GetI420();
  // check for overflow of stride * height
  CheckedInt32 ysize =
      CheckedInt32(input_image->StrideY()) * input_image->height();
  MOZ_RELEASE_ASSERT(ysize.isValid());
  // I will assume that if that doesn't overflow, the others case - YUV
  // 4:2:0 has U/V widths <= Y, even with alignment issues.
  err = frame->CreateFrame(
      ysize.value(), input_image->DataY(),
      input_image->StrideU() * ((input_image->height() + 1) / 2),
      input_image->DataU(),
      input_image->StrideV() * ((input_image->height() + 1) / 2),
      input_image->DataV(), input_image->width(), input_image->height(),
      input_image->StrideY(), input_image->StrideU(), input_image->StrideV());
  if (err != GMPNoErr) {
    GMP_LOG_DEBUG("GMP Encode: failed to create frame");
    return;
  }
  frame->SetTimestamp((aInputImage.timestamp() * 1000ll) /
                      90);  // note: rounds down!
  // frame->SetDuration(1000000ll/30); // XXX base duration on measured current
  // FPS - or don't bother

  // Bug XXXXXX: Set codecSpecific info
  GMPCodecSpecificInfo info;
  memset(&info, 0, sizeof(info));
  info.mCodecType = kGMPVideoCodecH264;
  nsTArray<uint8_t> codecSpecificInfo;
  codecSpecificInfo.AppendElements((uint8_t*)&info,
                                   sizeof(GMPCodecSpecificInfo));

  nsTArray<GMPVideoFrameType> gmp_frame_types;
  for (auto it = aFrameTypes.begin(); it != aFrameTypes.end(); ++it) {
    GMPVideoFrameType ft;

    int32_t ret = WebrtcFrameTypeToGmpFrameType(*it, &ft);
    if (ret != WEBRTC_VIDEO_CODEC_OK) {
      GMP_LOG_DEBUG(
          "GMP Encode: failed to map webrtc frame type to gmp frame type");
      return;
    }

    gmp_frame_types.AppendElement(ft);
  }

  {
    auto inputImageMap = aEncoder->mInputImageMap.Lock();
    DebugOnly<bool> inserted = false;
    std::tie(std::ignore, inserted) = inputImageMap->insert(
        {frame->Timestamp(), {aInputImage.timestamp_us()}});
    MOZ_ASSERT(inserted, "Duplicate timestamp");
  }

  GMP_LOG_DEBUG("GMP Encode: %" PRIu64, (frame->Timestamp()));
  err = aEncoder->mGMP->Encode(std::move(frame), codecSpecificInfo,
                               gmp_frame_types);
  if (err != GMPNoErr) {
    GMP_LOG_DEBUG("GMP Encode: failed to encode frame");
  }
}

int32_t WebrtcGmpVideoEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* aCallback) {
  MutexAutoLock lock(mCallbackMutex);
  mCallback = aCallback;

  return WEBRTC_VIDEO_CODEC_OK;
}

/* static */
void WebrtcGmpVideoEncoder::ReleaseGmp_g(
    const RefPtr<WebrtcGmpVideoEncoder>& aEncoder) {
  aEncoder->Close_g();
}

int32_t WebrtcGmpVideoEncoder::Shutdown() {
  GMP_LOG_DEBUG("GMP Released:");
  RegisterEncodeCompleteCallback(nullptr);
  if (mGMPThread) {
    mGMPThread->Dispatch(WrapRunnableNM(&WebrtcGmpVideoEncoder::ReleaseGmp_g,
                                        RefPtr<WebrtcGmpVideoEncoder>(this)),
                         NS_DISPATCH_NORMAL);
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcGmpVideoEncoder::SetRates(
    const webrtc::VideoEncoder::RateControlParameters& aParameters) {
  MOZ_ASSERT(mGMPThread);
  MOZ_ASSERT(aParameters.bitrate.IsSpatialLayerUsed(0));
  MOZ_ASSERT(!aParameters.bitrate.HasBitrate(0, 1),
             "No simulcast support for H264");
  MOZ_ASSERT(!aParameters.bitrate.IsSpatialLayerUsed(1),
             "No simulcast support for H264");
  mGMPThread->Dispatch(
      WrapRunnableNM(&WebrtcGmpVideoEncoder::SetRates_g,
                     RefPtr<WebrtcGmpVideoEncoder>(this),
                     aParameters.bitrate.GetBitrate(0, 0) / 1000,
                     aParameters.framerate_fps > 0.0
                         ? Some(aParameters.framerate_fps)
                         : Nothing()),
      NS_DISPATCH_NORMAL);

  return WEBRTC_VIDEO_CODEC_OK;
}

WebrtcVideoEncoder::EncoderInfo WebrtcGmpVideoEncoder::GetEncoderInfo() const {
  WebrtcVideoEncoder::EncoderInfo info;
  info.supports_native_handle = false;
  info.implementation_name = "GMPOpenH264";
  info.scaling_settings = WebrtcVideoEncoder::ScalingSettings(
      kLowH264QpThreshold, kHighH264QpThreshold);
  info.is_hardware_accelerated = false;
  info.supports_simulcast = false;
  return info;
}

/* static */
int32_t WebrtcGmpVideoEncoder::SetRates_g(RefPtr<WebrtcGmpVideoEncoder> aThis,
                                          uint32_t aNewBitRateKbps,
                                          Maybe<double> aFrameRate) {
  if (!aThis->mGMP) {
    // destroyed via Terminate()
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  GMPErr err = aThis->mGMP->SetRates(
      aNewBitRateKbps, aFrameRate
                           .map([](double aFr) {
                             // Avoid rounding to 0
                             return std::max(1U, static_cast<uint32_t>(aFr));
                           })
                           .valueOr(aThis->mCodecParams.mMaxFramerate));
  if (err != GMPNoErr) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

// GMPVideoEncoderCallback virtual functions.
void WebrtcGmpVideoEncoder::Terminated() {
  GMP_LOG_DEBUG("GMP Encoder Terminated: %p", (void*)this);

  GMPVideoEncoderProxy* gmp(mGMP);
  mGMP = nullptr;
  mHost = nullptr;
  mInitting = false;

  if (gmp) {
    // Do this last, since this could cause us to be destroyed
    gmp->Close();
  }

  // Could now notify that it's dead
}

void WebrtcGmpVideoEncoder::Encoded(
    GMPVideoEncodedFrame* aEncodedFrame,
    const nsTArray<uint8_t>& aCodecSpecificInfo) {
  webrtc::Timestamp capture_time = webrtc::Timestamp::Micros(0);
  {
    auto inputImageMap = mInputImageMap.Lock();
    auto handle = inputImageMap->extract(aEncodedFrame->TimeStamp());
    MOZ_ASSERT(handle);
    if (handle) {
      capture_time = webrtc::Timestamp::Micros(handle.mapped().timestamp_us);
    }
  }

  MutexAutoLock lock(mCallbackMutex);
  if (!mCallback) {
    return;
  }

  webrtc::VideoFrameType ft;
  GmpFrameTypeToWebrtcFrameType(aEncodedFrame->FrameType(), &ft);
  uint32_t timestamp = (aEncodedFrame->TimeStamp() * 90ll + 999) / 1000;

  GMP_LOG_DEBUG("GMP Encoded: %" PRIu64 ", type %d, len %d",
                aEncodedFrame->TimeStamp(), aEncodedFrame->BufferType(),
                aEncodedFrame->Size());

  if (!aEncodedFrame->Buffer()) {
    GMP_LOG_ERROR("GMP plugin returned null buffer");
    return;
  }

  // Libwebrtc's RtpPacketizerH264 expects a 3- or 4-byte NALU start sequence
  // before the start of the NALU payload. {0,0,1} or {0,0,0,1}. We set this
  // in-place. Any other length of the length field we reject.

  const int sizeNumBytes = SizeNumBytes(aEncodedFrame->BufferType());
  uint32_t unitOffset = 0;
  uint32_t unitSize = 0;
  // Make sure we don't read past the end of the buffer getting the size
  while (unitOffset + sizeNumBytes < aEncodedFrame->Size()) {
    uint8_t* unitBuffer = aEncodedFrame->Buffer() + unitOffset;
    switch (aEncodedFrame->BufferType()) {
      case GMP_BufferLength24: {
#if MOZ_LITTLE_ENDIAN()
        unitSize = (static_cast<uint32_t>(*unitBuffer)) |
                   (static_cast<uint32_t>(*(unitBuffer + 1)) << 8) |
                   (static_cast<uint32_t>(*(unitBuffer + 2)) << 16);
#else
        unitSize = (static_cast<uint32_t>(*unitBuffer) << 16) |
                   (static_cast<uint32_t>(*(unitBuffer + 1)) << 8) |
                   (static_cast<uint32_t>(*(unitBuffer + 2)));
#endif
        const uint8_t startSequence[] = {0, 0, 1};
        if (memcmp(unitBuffer, startSequence, 3) == 0) {
          // This is a bug in OpenH264 where it misses to convert the NALU start
          // sequence to the NALU size per the GMP protocol. We mitigate this by
          // letting it through as this is what libwebrtc already expects and
          // scans for.
          unitSize = aEncodedFrame->Size() - 3;
          break;
        }
        memcpy(unitBuffer, startSequence, 3);
        break;
      }
      case GMP_BufferLength32: {
#if MOZ_LITTLE_ENDIAN()
        unitSize = LittleEndian::readUint32(unitBuffer);
#else
        unitSize = BigEndian::readUint32(unitBuffer);
#endif
        const uint8_t startSequence[] = {0, 0, 0, 1};
        if (memcmp(unitBuffer, startSequence, 4) == 0) {
          // This is a bug in OpenH264 where it misses to convert the NALU start
          // sequence to the NALU size per the GMP protocol. We mitigate this by
          // letting it through as this is what libwebrtc already expects and
          // scans for.
          unitSize = aEncodedFrame->Size() - 4;
          break;
        }
        memcpy(unitBuffer, startSequence, 4);
        break;
      }
      default:
        GMP_LOG_ERROR("GMP plugin returned type we cannot handle (%d)",
                      aEncodedFrame->BufferType());
        return;
    }

    MOZ_ASSERT(unitSize != 0);
    MOZ_ASSERT(unitOffset + sizeNumBytes + unitSize <= aEncodedFrame->Size());
    if (unitSize == 0 ||
        unitOffset + sizeNumBytes + unitSize > aEncodedFrame->Size()) {
      // XXX Should we kill the plugin for returning extra bytes? Probably
      GMP_LOG_ERROR(
          "GMP plugin returned badly formatted encoded data: "
          "unitOffset=%u, sizeNumBytes=%d, unitSize=%u, size=%u",
          unitOffset, sizeNumBytes, unitSize, aEncodedFrame->Size());
      return;
    }

    unitOffset += sizeNumBytes + unitSize;
  }

  if (unitOffset != aEncodedFrame->Size()) {
    // At most 3 bytes can be left over, depending on buffertype
    GMP_LOG_DEBUG("GMP plugin returned %u extra bytes",
                  aEncodedFrame->Size() - unitOffset);
  }

  webrtc::EncodedImage unit;
  unit.SetEncodedData(webrtc::EncodedImageBuffer::Create(
      aEncodedFrame->Buffer(), aEncodedFrame->Size()));
  unit._frameType = ft;
  unit.SetTimestamp(timestamp);
  unit.capture_time_ms_ = capture_time.ms();
  unit._encodedWidth = aEncodedFrame->EncodedWidth();
  unit._encodedHeight = aEncodedFrame->EncodedHeight();

  // Parse QP.
  mH264BitstreamParser.ParseBitstream(unit);
  unit.qp_ = mH264BitstreamParser.GetLastSliceQp().value_or(-1);

  // TODO: Currently the OpenH264 codec does not preserve any codec
  //       specific info passed into it and just returns default values.
  //       If this changes in the future, it would be nice to get rid of
  //       mCodecSpecificInfo.
  mCallback->OnEncodedImage(unit, &mCodecSpecificInfo);
}

// Decoder.
WebrtcGmpVideoDecoder::WebrtcGmpVideoDecoder(std::string aPCHandle,
                                             TrackingId aTrackingId)
    : mGMP(nullptr),
      mInitting(false),
      mHost(nullptr),
      mCallbackMutex("WebrtcGmpVideoDecoder decoded callback mutex"),
      mCallback(nullptr),
      mDecoderStatus(GMPNoErr),
      mPCHandle(std::move(aPCHandle)),
      mTrackingId(std::move(aTrackingId)) {
  MOZ_ASSERT(!mPCHandle.empty());
}

WebrtcGmpVideoDecoder::~WebrtcGmpVideoDecoder() {
  // We should not have been destroyed if we never closed our GMP
  MOZ_ASSERT(!mGMP);
}

bool WebrtcGmpVideoDecoder::Configure(
    const webrtc::VideoDecoder::Settings& settings) {
  if (!mMPS) {
    mMPS = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  }
  MOZ_ASSERT(mMPS);

  if (!mGMPThread) {
    if (NS_WARN_IF(NS_FAILED(mMPS->GetThread(getter_AddRefs(mGMPThread))))) {
      return false;
    }
  }

  RefPtr<GmpInitDoneRunnable> initDone(new GmpInitDoneRunnable(mPCHandle));
  mGMPThread->Dispatch(
      WrapRunnableNM(&WebrtcGmpVideoDecoder::Configure_g,
                     RefPtr<WebrtcGmpVideoDecoder>(this), settings, initDone),
      NS_DISPATCH_NORMAL);

  return true;
}

/* static */
void WebrtcGmpVideoDecoder::Configure_g(
    const RefPtr<WebrtcGmpVideoDecoder>& aThis,
    const webrtc::VideoDecoder::Settings& settings,  // unused
    const RefPtr<GmpInitDoneRunnable>& aInitDone) {
  nsTArray<nsCString> tags;
  tags.AppendElement("h264"_ns);
  UniquePtr<GetGMPVideoDecoderCallback> callback(
      new InitDoneCallback(aThis, aInitDone));
  aThis->mInitting = true;
  nsresult rv = aThis->mMPS->GetGMPVideoDecoder(nullptr, &tags, ""_ns,
                                                std::move(callback));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    GMP_LOG_DEBUG("GMP Decode: GetGMPVideoDecoder failed");
    aThis->Close_g();
    aInitDone->Dispatch(WEBRTC_VIDEO_CODEC_ERROR,
                        "GMP Decode: GetGMPVideoDecoder failed.");
  }
}

int32_t WebrtcGmpVideoDecoder::GmpInitDone(GMPVideoDecoderProxy* aGMP,
                                           GMPVideoHost* aHost,
                                           std::string* aErrorOut) {
  if (!mInitting || !aGMP || !aHost) {
    *aErrorOut =
        "GMP Decode: Either init was aborted, "
        "or init failed to supply either a GMP decoder or GMP host.";
    if (aGMP) {
      // This could destroy us, since aGMP may be the last thing holding a ref
      // Return immediately.
      aGMP->Close();
    }
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  mInitting = false;

  if (mGMP && mGMP != aGMP) {
    Close_g();
  }

  mGMP = aGMP;
  mHost = aHost;
  mCachedPluginId = Some(mGMP->GetPluginId());
  mInitPluginEvent.Notify(*mCachedPluginId);
  // Bug XXXXXX: transfer settings from codecSettings to codec.
  GMPVideoCodec codec;
  memset(&codec, 0, sizeof(codec));
  codec.mGMPApiVersion = kGMPVersion34;
  codec.mLogLevel = GetGMPLibraryLogLevel();

  // XXX this is currently a hack
  // GMPVideoCodecUnion codecSpecific;
  // memset(&codecSpecific, 0, sizeof(codecSpecific));
  nsTArray<uint8_t> codecSpecific;
  nsresult rv = mGMP->InitDecode(codec, codecSpecific, this, 1);
  if (NS_FAILED(rv)) {
    *aErrorOut = "GMP Decode: InitDecode failed";
    mQueuedFrames.Clear();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // now release any frames that got queued waiting for InitDone
  if (!mQueuedFrames.IsEmpty()) {
    // So we're safe to call Decode_g(), which asserts it's empty
    nsTArray<UniquePtr<GMPDecodeData>> temp = std::move(mQueuedFrames);
    for (auto& queued : temp) {
      Decode_g(RefPtr<WebrtcGmpVideoDecoder>(this), std::move(queued));
    }
  }

  // This is an ugly solution to asynchronous decoding errors
  // from Decode_g() not being returned to the synchronous Decode() method.
  // If we don't return an error code at this point, our caller ultimately won't
  // know to request a PLI and the video stream will remain frozen unless an IDR
  // happens to arrive for other reasons. Bug 1492852 tracks implementing a
  // proper solution.
  if (mDecoderStatus != GMPNoErr) {
    GMP_LOG_ERROR("%s: Decoder status is bad (%u)!", __PRETTY_FUNCTION__,
                  static_cast<unsigned>(mDecoderStatus));
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

void WebrtcGmpVideoDecoder::Close_g() {
  GMPVideoDecoderProxy* gmp(mGMP);
  mGMP = nullptr;
  mHost = nullptr;
  mInitting = false;

  if (mCachedPluginId) {
    mReleasePluginEvent.Notify(*mCachedPluginId);
  }
  mCachedPluginId = Nothing();

  if (gmp) {
    // Do this last, since this could cause us to be destroyed
    gmp->Close();
  }
}

int32_t WebrtcGmpVideoDecoder::Decode(const webrtc::EncodedImage& aInputImage,
                                      bool aMissingFrames,
                                      int64_t aRenderTimeMs) {
  MOZ_ASSERT(mGMPThread);
  MOZ_ASSERT(!NS_IsMainThread());
  if (!aInputImage.size()) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  MediaInfoFlag flag = MediaInfoFlag::None;
  flag |= (aInputImage._frameType == webrtc::VideoFrameType::kVideoFrameKey
               ? MediaInfoFlag::KeyFrame
               : MediaInfoFlag::NonKeyFrame);
  flag |= MediaInfoFlag::SoftwareDecoding;
  flag |= MediaInfoFlag::VIDEO_H264;
  mPerformanceRecorder.Start((aInputImage.Timestamp() * 1000ll) / 90,
                             "WebrtcGmpVideoDecoder"_ns, mTrackingId, flag);

  // This is an ugly solution to asynchronous decoding errors
  // from Decode_g() not being returned to the synchronous Decode() method.
  // If we don't return an error code at this point, our caller ultimately won't
  // know to request a PLI and the video stream will remain frozen unless an IDR
  // happens to arrive for other reasons. Bug 1492852 tracks implementing a
  // proper solution.
  auto decodeData =
      MakeUnique<GMPDecodeData>(aInputImage, aMissingFrames, aRenderTimeMs);

  mGMPThread->Dispatch(WrapRunnableNM(&WebrtcGmpVideoDecoder::Decode_g,
                                      RefPtr<WebrtcGmpVideoDecoder>(this),
                                      std::move(decodeData)),
                       NS_DISPATCH_NORMAL);

  if (mDecoderStatus != GMPNoErr) {
    GMP_LOG_ERROR("%s: Decoder status is bad (%u)!", __PRETTY_FUNCTION__,
                  static_cast<unsigned>(mDecoderStatus));
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

/* static */
void WebrtcGmpVideoDecoder::Decode_g(const RefPtr<WebrtcGmpVideoDecoder>& aThis,
                                     UniquePtr<GMPDecodeData>&& aDecodeData) {
  if (!aThis->mGMP) {
    if (aThis->mInitting) {
      // InitDone hasn't been called yet (race)
      aThis->mQueuedFrames.AppendElement(std::move(aDecodeData));
      return;
    }
    // destroyed via Terminate(), failed to init, or just not initted yet
    GMP_LOG_DEBUG("GMP Decode: not initted yet");

    aThis->mDecoderStatus = GMPDecodeErr;
    return;
  }

  MOZ_ASSERT(aThis->mQueuedFrames.IsEmpty());
  MOZ_ASSERT(aThis->mHost);

  GMPVideoFrame* ftmp = nullptr;
  GMPErr err = aThis->mHost->CreateFrame(kGMPEncodedVideoFrame, &ftmp);
  if (err != GMPNoErr) {
    GMP_LOG_ERROR("%s: CreateFrame failed (%u)!", __PRETTY_FUNCTION__,
                  static_cast<unsigned>(err));
    aThis->mDecoderStatus = err;
    return;
  }

  GMPUniquePtr<GMPVideoEncodedFrame> frame(
      static_cast<GMPVideoEncodedFrame*>(ftmp));
  err = frame->CreateEmptyFrame(aDecodeData->mImage.size());
  if (err != GMPNoErr) {
    GMP_LOG_ERROR("%s: CreateEmptyFrame failed (%u)!", __PRETTY_FUNCTION__,
                  static_cast<unsigned>(err));
    aThis->mDecoderStatus = err;
    return;
  }

  // XXX At this point, we only will get mode1 data (a single length and a
  // buffer) Session_info.cc/etc code needs to change to support mode 0.
  *(reinterpret_cast<uint32_t*>(frame->Buffer())) = frame->Size();

  // XXX It'd be wonderful not to have to memcpy the encoded data!
  memcpy(frame->Buffer() + 4, aDecodeData->mImage.data() + 4,
         frame->Size() - 4);

  frame->SetEncodedWidth(aDecodeData->mImage._encodedWidth);
  frame->SetEncodedHeight(aDecodeData->mImage._encodedHeight);
  frame->SetTimeStamp((aDecodeData->mImage.Timestamp() * 1000ll) /
                      90);  // rounds down
  frame->SetCompleteFrame(
      true);  // upstream no longer deals with incomplete frames
  frame->SetBufferType(GMP_BufferLength32);

  GMPVideoFrameType ft;
  int32_t ret =
      WebrtcFrameTypeToGmpFrameType(aDecodeData->mImage._frameType, &ft);
  if (ret != WEBRTC_VIDEO_CODEC_OK) {
    GMP_LOG_ERROR("%s: WebrtcFrameTypeToGmpFrameType failed (%u)!",
                  __PRETTY_FUNCTION__, static_cast<unsigned>(ret));
    aThis->mDecoderStatus = GMPDecodeErr;
    return;
  }

  // Bug XXXXXX: Set codecSpecific info
  GMPCodecSpecificInfo info;
  memset(&info, 0, sizeof(info));
  info.mCodecType = kGMPVideoCodecH264;
  info.mCodecSpecific.mH264.mSimulcastIdx = 0;
  nsTArray<uint8_t> codecSpecificInfo;
  codecSpecificInfo.AppendElements((uint8_t*)&info,
                                   sizeof(GMPCodecSpecificInfo));

  GMP_LOG_DEBUG("GMP Decode: %" PRIu64 ", len %zu%s", frame->TimeStamp(),
                aDecodeData->mImage.size(),
                ft == kGMPKeyFrame ? ", KeyFrame" : "");

  nsresult rv =
      aThis->mGMP->Decode(std::move(frame), aDecodeData->mMissingFrames,
                          codecSpecificInfo, aDecodeData->mRenderTimeMs);
  if (NS_FAILED(rv)) {
    GMP_LOG_ERROR("%s: Decode failed (rv=%u)!", __PRETTY_FUNCTION__,
                  static_cast<unsigned>(rv));
    aThis->mDecoderStatus = GMPDecodeErr;
    return;
  }

  aThis->mDecoderStatus = GMPNoErr;
}

int32_t WebrtcGmpVideoDecoder::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* aCallback) {
  MutexAutoLock lock(mCallbackMutex);
  mCallback = aCallback;

  return WEBRTC_VIDEO_CODEC_OK;
}

/* static */
void WebrtcGmpVideoDecoder::ReleaseGmp_g(
    const RefPtr<WebrtcGmpVideoDecoder>& aDecoder) {
  aDecoder->Close_g();
}

int32_t WebrtcGmpVideoDecoder::ReleaseGmp() {
  GMP_LOG_DEBUG("GMP Released:");
  RegisterDecodeCompleteCallback(nullptr);

  if (mGMPThread) {
    mGMPThread->Dispatch(WrapRunnableNM(&WebrtcGmpVideoDecoder::ReleaseGmp_g,
                                        RefPtr<WebrtcGmpVideoDecoder>(this)),
                         NS_DISPATCH_NORMAL);
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

void WebrtcGmpVideoDecoder::Terminated() {
  GMP_LOG_DEBUG("GMP Decoder Terminated: %p", (void*)this);

  GMPVideoDecoderProxy* gmp(mGMP);
  mGMP = nullptr;
  mHost = nullptr;
  mInitting = false;

  if (gmp) {
    // Do this last, since this could cause us to be destroyed
    gmp->Close();
  }

  // Could now notify that it's dead
}

void WebrtcGmpVideoDecoder::Decoded(GMPVideoi420Frame* aDecodedFrame) {
  // we have two choices here: wrap the frame with a callback that frees
  // the data later (risking running out of shmems), or copy the data out
  // always.  Also, we can only Destroy() the frame on the gmp thread, so
  // copying is simplest if expensive.
  // I420 size including rounding...
  CheckedInt32 length =
      (CheckedInt32(aDecodedFrame->Stride(kGMPYPlane)) *
       aDecodedFrame->Height()) +
      (aDecodedFrame->Stride(kGMPVPlane) + aDecodedFrame->Stride(kGMPUPlane)) *
          ((aDecodedFrame->Height() + 1) / 2);
  int32_t size = length.value();
  MOZ_RELEASE_ASSERT(length.isValid() && size > 0);

  // Don't use MakeUniqueFallible here, because UniquePtr isn't copyable, and
  // the closure below in WrapI420Buffer uses std::function which _is_ copyable.
  // We'll alloc the buffer here, so we preserve the "fallible" nature, and
  // then hand a shared_ptr, which is copyable, to WrapI420Buffer.
  auto falliblebuffer = new (std::nothrow) uint8_t[size];
  if (falliblebuffer) {
    auto buffer = std::shared_ptr<uint8_t>(falliblebuffer);

    // This is 3 separate buffers currently anyways, no use in trying to
    // see if we can use a single memcpy.
    uint8_t* buffer_y = buffer.get();
    memcpy(buffer_y, aDecodedFrame->Buffer(kGMPYPlane),
           aDecodedFrame->Stride(kGMPYPlane) * aDecodedFrame->Height());
    // Should this be aligned, making it non-contiguous?  Assume no, this is
    // already factored into the strides.
    uint8_t* buffer_u =
        buffer_y + aDecodedFrame->Stride(kGMPYPlane) * aDecodedFrame->Height();
    memcpy(buffer_u, aDecodedFrame->Buffer(kGMPUPlane),
           aDecodedFrame->Stride(kGMPUPlane) *
               ((aDecodedFrame->Height() + 1) / 2));
    uint8_t* buffer_v = buffer_u + aDecodedFrame->Stride(kGMPUPlane) *
                                       ((aDecodedFrame->Height() + 1) / 2);
    memcpy(buffer_v, aDecodedFrame->Buffer(kGMPVPlane),
           aDecodedFrame->Stride(kGMPVPlane) *
               ((aDecodedFrame->Height() + 1) / 2));

    MutexAutoLock lock(mCallbackMutex);
    if (mCallback) {
      // Note: the last parameter to WrapI420Buffer is named no_longer_used,
      // but is currently called in the destructor of WrappedYuvBuffer when
      // the buffer is "no_longer_used".
      rtc::scoped_refptr<webrtc::I420BufferInterface> video_frame_buffer =
          webrtc::WrapI420Buffer(
              aDecodedFrame->Width(), aDecodedFrame->Height(), buffer_y,
              aDecodedFrame->Stride(kGMPYPlane), buffer_u,
              aDecodedFrame->Stride(kGMPUPlane), buffer_v,
              aDecodedFrame->Stride(kGMPVPlane), [buffer] {});

      GMP_LOG_DEBUG("GMP Decoded: %" PRIu64, aDecodedFrame->Timestamp());
      auto videoFrame =
          webrtc::VideoFrame::Builder()
              .set_video_frame_buffer(video_frame_buffer)
              .set_timestamp_rtp(
                  // round up
                  (aDecodedFrame->UpdatedTimestamp() * 90ll + 999) / 1000)
              .build();
      mPerformanceRecorder.Record(
          static_cast<int64_t>(aDecodedFrame->Timestamp()),
          [&](DecodeStage& aStage) {
            aStage.SetImageFormat(DecodeStage::YUV420P);
            aStage.SetResolution(aDecodedFrame->Width(),
                                 aDecodedFrame->Height());
            aStage.SetColorDepth(gfx::ColorDepth::COLOR_8);
          });
      mCallback->Decoded(videoFrame);
    }
  }
  aDecodedFrame->Destroy();
}

}  // namespace mozilla

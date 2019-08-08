/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppleVTEncoder.h"

#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFByteOrder.h>
#include <CoreFoundation/CFDictionary.h>

#include "ImageContainer.h"
#include "AnnexB.h"
#include "H264.h"

#include "libyuv.h"

#include "AppleUtils.h"

#define VTENC_LOGE(fmt, ...)                 \
  MOZ_LOG(sPEMLog, mozilla::LogLevel::Error, \
          ("[AppleVTEncoder] %s: " fmt, __func__, ##__VA_ARGS__))
#define VTENC_LOGD(fmt, ...)                 \
  MOZ_LOG(sPEMLog, mozilla::LogLevel::Debug, \
          ("[AppleVTEncoder] %s: " fmt, __func__, ##__VA_ARGS__))

namespace mozilla {

static CFDictionaryRef BuildEncoderSpec() {
  const void* keys[] = {
      kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder};
  const void* values[] = {kCFBooleanTrue};

  static_assert(ArrayLength(keys) == ArrayLength(values),
                "Non matching keys/values array size");
  return CFDictionaryCreate(kCFAllocatorDefault, keys, values,
                            ArrayLength(keys), &kCFTypeDictionaryKeyCallBacks,
                            &kCFTypeDictionaryValueCallBacks);
}

static void FrameCallback(void* aEncoder, void* aFrameParams, OSStatus aStatus,
                          VTEncodeInfoFlags aInfoFlags,
                          CMSampleBufferRef aSampleBuffer) {
  if (aStatus != noErr || !aSampleBuffer) {
    VTENC_LOGE("VideoToolbox encoder returned no data");
    aSampleBuffer = nullptr;
  } else if (aInfoFlags & kVTEncodeInfo_FrameDropped) {
    VTENC_LOGE("  ...frame tagged as dropped...");
  }

  static_cast<AppleVTEncoder*>(aEncoder)->OutputFrame(aSampleBuffer);
}

static bool SetAverageBitrate(VTCompressionSessionRef& aSession,
                              MediaDataEncoder::Rate aBitsPerSec) {
  int64_t bps(aBitsPerSec);
  AutoCFRelease<CFNumberRef> bitrate(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &bps));
  return VTSessionSetProperty(aSession,
                              kVTCompressionPropertyKey_AverageBitRate,
                              bitrate) == noErr;
}

static bool SetRealtimeProperties(VTCompressionSessionRef& aSession) {
  return VTSessionSetProperty(aSession, kVTCompressionPropertyKey_RealTime,
                              kCFBooleanTrue) == noErr &&
         VTSessionSetProperty(aSession,
                              kVTCompressionPropertyKey_AllowFrameReordering,
                              kCFBooleanFalse) == noErr;
}

static bool SetProfileLevel(VTCompressionSessionRef& aSession,
                            AppleVTEncoder::H264Specific::ProfileLevel aValue) {
  CFStringRef profileLevel = nullptr;
  switch (aValue) {
    case AppleVTEncoder::H264Specific::ProfileLevel::BaselineAutoLevel:
      profileLevel = kVTProfileLevel_H264_Baseline_AutoLevel;
      break;
    case AppleVTEncoder::H264Specific::ProfileLevel::MainAutoLevel:
      profileLevel = kVTProfileLevel_H264_Main_AutoLevel;
      break;
  }

  return profileLevel ? VTSessionSetProperty(
                            aSession, kVTCompressionPropertyKey_ProfileLevel,
                            profileLevel) == noErr
                      : false;
}

RefPtr<MediaDataEncoder::InitPromise> AppleVTEncoder::Init() {
  MOZ_ASSERT(!mInited, "Cannot initialize encoder again without shutting down");

  AutoCFRelease<CFDictionaryRef> spec(BuildEncoderSpec());
  AutoCFRelease<CFDictionaryRef> srcBufferAttr(
      BuildSourceImageBufferAttributes());
  if (!srcBufferAttr) {
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR,
                                        __func__);
  }

  OSStatus status = VTCompressionSessionCreate(
      kCFAllocatorDefault, mConfig.mSize.width, mConfig.mSize.height,
      kCMVideoCodecType_H264, spec, srcBufferAttr, kCFAllocatorDefault,
      &FrameCallback, this, &mSession);
  if (status != noErr) {
    VTENC_LOGE("fail to create encoder session");
    // TODO: new error codes for encoder
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_ABORT_ERR, __func__);
  }

  if (!SetAverageBitrate(mSession, mConfig.mBitsPerSec)) {
    VTENC_LOGE("fail to configurate average bitrate");
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_ABORT_ERR, __func__);
  }

  if (mConfig.mUsage == Usage::Realtime && !SetRealtimeProperties(mSession)) {
    VTENC_LOGE("fail to configurate realtime properties");
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_ABORT_ERR, __func__);
  }

  if (mConfig.mCodecSpecific) {
    const H264Specific& specific = mConfig.mCodecSpecific.ref();
    if (!SetProfileLevel(mSession, specific.mProfileLevel)) {
      VTENC_LOGE("fail to configurate profile level:%d",
                 specific.mProfileLevel);
      return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_ABORT_ERR,
                                          __func__);
    }

    int64_t interval = specific.mKeyframeInterval;
    AutoCFRelease<CFNumberRef> cf(
        CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &interval));
    if (VTSessionSetProperty(mSession,
                             kVTCompressionPropertyKey_MaxKeyFrameInterval,
                             cf) != noErr) {
      VTENC_LOGE("fail to configurate keyframe interval");
      return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_ABORT_ERR,
                                          __func__);
    }
  }

  CFBooleanRef isUsingHW = nullptr;
  status = VTSessionCopyProperty(
      mSession, kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder,
      kCFAllocatorDefault, &isUsingHW);
  mIsHardwareAccelerated = status == noErr && isUsingHW == kCFBooleanTrue;

  mError = NS_OK;
  return InitPromise::CreateAndResolve(TrackInfo::TrackType::kVideoTrack,
                                       __func__);
}

static Maybe<OSType> MapPixelFormat(MediaDataEncoder::PixelFormat aFormat) {
  switch (aFormat) {
    case MediaDataEncoder::PixelFormat::RGBA32:
    case MediaDataEncoder::PixelFormat::BGRA32:
      return Some(kCVPixelFormatType_32BGRA);
    case MediaDataEncoder::PixelFormat::RGB24:
      return Some(kCVPixelFormatType_24RGB);
    case MediaDataEncoder::PixelFormat::BGR24:
      return Some(kCVPixelFormatType_24BGR);
    case MediaDataEncoder::PixelFormat::GRAY8:
      return Some(kCVPixelFormatType_OneComponent8);
    case MediaDataEncoder::PixelFormat::YUV444P:
      return Some(kCVPixelFormatType_444YpCbCr8);
    case MediaDataEncoder::PixelFormat::YUV420P:
      return Some(kCVPixelFormatType_420YpCbCr8PlanarFullRange);
    case MediaDataEncoder::PixelFormat::YUV420SP_NV12:
      return Some(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange);
    default:
      return Nothing();
  }
}

CFDictionaryRef AppleVTEncoder::BuildSourceImageBufferAttributes() {
  Maybe<OSType> fmt = MapPixelFormat(mConfig.mSourcePixelFormat);
  if (fmt.isNothing()) {
    VTENC_LOGE("unsupported source pixel format");
    return nullptr;
  }

  // Source image buffer attributes
  const void* keys[] = {kCVPixelBufferOpenGLCompatibilityKey,  // TODO
                        kCVPixelBufferIOSurfacePropertiesKey,  // TODO
                        kCVPixelBufferPixelFormatTypeKey};

  AutoCFRelease<CFDictionaryRef> ioSurfaceProps(CFDictionaryCreate(
      kCFAllocatorDefault, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
  AutoCFRelease<CFNumberRef> pixelFormat(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &fmt));
  const void* values[] = {kCFBooleanTrue, ioSurfaceProps, pixelFormat};

  MOZ_ASSERT(ArrayLength(keys) == ArrayLength(values),
             "Non matching keys/values array size");

  return CFDictionaryCreate(kCFAllocatorDefault, keys, values,
                            ArrayLength(keys), &kCFTypeDictionaryKeyCallBacks,
                            &kCFTypeDictionaryValueCallBacks);
}

static bool IsKeyframe(CMSampleBufferRef aSample) {
  CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(aSample, 0);
  if (attachments == nullptr || CFArrayGetCount(attachments) == 0) {
    return false;
  }

  return !CFDictionaryContainsKey(
      static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachments, 0)),
      kCMSampleAttachmentKey_NotSync);
}

static size_t GetNumParamSets(CMFormatDescriptionRef aDescription) {
  size_t numParamSets = 0;
  OSStatus status = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
      aDescription, 0, nullptr, nullptr, &numParamSets, nullptr);
  if (status != noErr) {
    VTENC_LOGE("Cannot get number of parameter sets from format description");
  }

  return numParamSets;
}

static const uint8_t kNALUStart[4] = {0, 0, 0, 1};

static size_t GetParamSet(CMFormatDescriptionRef aDescription, size_t aIndex,
                          const uint8_t** aDataPtr) {
  size_t length = 0;
  int headerSize = 0;
  if (CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
          aDescription, aIndex, aDataPtr, &length, nullptr, &headerSize) !=
      noErr) {
    VTENC_LOGE("fail to get parameter set from format description");
    return 0;
  }
  MOZ_ASSERT(headerSize == sizeof(kNALUStart), "Only support 4 byte header");

  return length;
}

static bool WriteSPSPPS(MediaRawData* aDst,
                        CMFormatDescriptionRef aDescription) {
  // Get SPS/PPS
  const size_t numParamSets = GetNumParamSets(aDescription);
  UniquePtr<MediaRawDataWriter> writer(aDst->CreateWriter());
  for (size_t i = 0; i < numParamSets; i++) {
    const uint8_t* data = nullptr;
    size_t length = GetParamSet(aDescription, i, &data);
    if (length == 0) {
      return false;
    }
    if (!writer->Append(kNALUStart, sizeof(kNALUStart))) {
      VTENC_LOGE("Cannot write NAL unit start code");
      return false;
    }
    if (!writer->Append(data, length)) {
      VTENC_LOGE("Cannot write parameter set");
      return false;
    }
  }
  return true;
}

static RefPtr<MediaByteBuffer> extractAvcc(
    CMFormatDescriptionRef aDescription) {
  CFPropertyListRef list = CMFormatDescriptionGetExtension(
      aDescription,
      kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms);
  if (!list) {
    VTENC_LOGE("fail to get atoms");
    return nullptr;
  }
  CFDataRef avcC = static_cast<CFDataRef>(
      CFDictionaryGetValue(static_cast<CFDictionaryRef>(list), CFSTR("avcC")));
  if (!avcC) {
    VTENC_LOGE("fail to extract avcC");
    return nullptr;
  }
  CFIndex length = CFDataGetLength(avcC);
  const UInt8* bytes = CFDataGetBytePtr(avcC);
  if (length <= 0 || !bytes) {
    VTENC_LOGE("empty avcC");
    return nullptr;
  }

  RefPtr<MediaByteBuffer> config = new MediaByteBuffer(length);
  config->AppendElements(bytes, length);
  return config;
}

bool AppleVTEncoder::WriteExtraData(MediaRawData* aDst, CMSampleBufferRef aSrc,
                                    const bool aAsAnnexB) {
  if (!IsKeyframe(aSrc)) {
    return true;
  }

  aDst->mKeyframe = true;
  CMFormatDescriptionRef desc = CMSampleBufferGetFormatDescription(aSrc);
  if (!desc) {
    VTENC_LOGE("fail to get format description from sample");
    return false;
  }

  if (aAsAnnexB) {
    return WriteSPSPPS(aDst, desc);
  }

  RefPtr<MediaByteBuffer> avcc = extractAvcc(desc);
  if (!avcc) {
    return false;
  }

  if (!mAvcc || !H264::CompareExtraData(avcc, mAvcc)) {
    mAvcc = avcc;
    aDst->mExtraData = mAvcc;
  }

  return avcc != nullptr;
}

static bool WriteNALUs(MediaRawData* aDst, CMSampleBufferRef aSrc,
                       bool aAsAnnexB = false) {
  size_t srcRemaining = CMSampleBufferGetTotalSampleSize(aSrc);
  CMBlockBufferRef block = CMSampleBufferGetDataBuffer(aSrc);
  if (!block) {
    VTENC_LOGE("Cannot get block buffer frome sample");
    return false;
  }
  UniquePtr<MediaRawDataWriter> writer(aDst->CreateWriter());
  size_t writtenLength = aDst->Size();
  // Ensure capacity.
  if (!writer->SetSize(writtenLength + srcRemaining)) {
    VTENC_LOGE("Cannot allocate buffer");
    return false;
  }
  size_t readLength = 0;
  while (srcRemaining > 0) {
    // Extract the size of next NAL unit
    uint8_t unitSizeBytes[4];
    MOZ_ASSERT(srcRemaining > sizeof(unitSizeBytes));
    if (CMBlockBufferCopyDataBytes(block, readLength, sizeof(unitSizeBytes),
                                   reinterpret_cast<uint32_t*>(
                                       unitSizeBytes)) != kCMBlockBufferNoErr) {
      VTENC_LOGE("Cannot copy unit size bytes");
      return false;
    }
    size_t unitSize =
        CFSwapInt32BigToHost(*reinterpret_cast<uint32_t*>(unitSizeBytes));

    if (aAsAnnexB) {
      // Replace unit size bytes with NALU start code.
      PodCopy(writer->Data() + writtenLength, kNALUStart, sizeof(kNALUStart));
      readLength += sizeof(unitSizeBytes);
      srcRemaining -= sizeof(unitSizeBytes);
      writtenLength += sizeof(kNALUStart);
    } else {
      // Copy unit size bytes + data.
      unitSize += sizeof(unitSizeBytes);
    }
    MOZ_ASSERT(writtenLength + unitSize <= aDst->Size());
    // Copy NAL unit data
    if (CMBlockBufferCopyDataBytes(block, readLength, unitSize,
                                   writer->Data() + writtenLength) !=
        kCMBlockBufferNoErr) {
      VTENC_LOGE("Cannot copy unit data");
      return false;
    }
    readLength += unitSize;
    srcRemaining -= unitSize;
    writtenLength += unitSize;
  }
  MOZ_ASSERT(writtenLength == aDst->Size());
  return true;
}

void AppleVTEncoder::OutputFrame(CMSampleBufferRef aBuffer) {
  RefPtr<MediaRawData> output(new MediaRawData());

  bool asAnnexB = mConfig.mUsage == Usage::Realtime;
  bool succeeded = WriteExtraData(output, aBuffer, asAnnexB) &&
                   WriteNALUs(output, aBuffer, asAnnexB);

  ProcessOutput(succeeded ? std::move(output) : nullptr);
}

void AppleVTEncoder::ProcessOutput(RefPtr<MediaRawData>&& aOutput) {
  if (!mTaskQueue->IsCurrentThreadIn()) {
    nsresult rv = mTaskQueue->Dispatch(NewRunnableMethod<RefPtr<MediaRawData>>(
        "AppleVTEncoder::ProcessOutput", this, &AppleVTEncoder::ProcessOutput,
        std::move(aOutput)));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
    return;
  }
  AssertOnTaskQueue();

  if (aOutput) {
    mEncodedData.AppendElement(std::move(aOutput));
  } else {
    mError = NS_ERROR_DOM_MEDIA_FATAL_ERR;
  }
}

RefPtr<MediaDataEncoder::EncodePromise> AppleVTEncoder::Encode(
    const MediaData* aSample) {
  MOZ_ASSERT(aSample != nullptr);
  RefPtr<const VideoData> sample(aSample->As<const VideoData>());

  return InvokeAsync<RefPtr<const VideoData>>(mTaskQueue, this, __func__,
                                              &AppleVTEncoder::ProcessEncode,
                                              std::move(sample));
}

RefPtr<MediaDataEncoder::EncodePromise> AppleVTEncoder::ProcessEncode(
    RefPtr<const VideoData> aSample) {
  AssertOnTaskQueue();
  MOZ_ASSERT(mSession);

  if (NS_FAILED(mError)) {
    return EncodePromise::CreateAndReject(mError, __func__);
  }

  AutoCVBufferRelease<CVImageBufferRef> buffer(
      CreateCVPixelBuffer(aSample->mImage));
  if (!buffer) {
    return EncodePromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
  }

  CFDictionaryRef frameProps = nullptr;
  if (aSample->mKeyframe) {
    CFTypeRef keys[] = {kVTEncodeFrameOptionKey_ForceKeyFrame};
    CFTypeRef values[] = {kCFBooleanTrue};
    MOZ_ASSERT(ArrayLength(keys) == ArrayLength(values));
    frameProps = CFDictionaryCreate(
        kCFAllocatorDefault, keys, values, ArrayLength(keys),
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  };

  VTEncodeInfoFlags info;
  OSStatus status = VTCompressionSessionEncodeFrame(
      mSession, buffer,
      CMTimeMake(aSample->mTime.ToMicroseconds(), USECS_PER_S),
      CMTimeMake(aSample->mDuration.ToMicroseconds(), USECS_PER_S), frameProps,
      nullptr, &info);
  if (status != noErr) {
    return EncodePromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                          __func__);
  }

  return EncodePromise::CreateAndResolve(std::move(mEncodedData), __func__);
}

static size_t NumberOfPlanes(MediaDataEncoder::PixelFormat aPixelFormat) {
  switch (aPixelFormat) {
    case MediaDataEncoder::PixelFormat::RGBA32:
    case MediaDataEncoder::PixelFormat::BGRA32:
    case MediaDataEncoder::PixelFormat::RGB24:
    case MediaDataEncoder::PixelFormat::BGR24:
    case MediaDataEncoder::PixelFormat::GRAY8:
      return 1;
    case MediaDataEncoder::PixelFormat::YUV444P:
    case MediaDataEncoder::PixelFormat::YUV420P:
      return 3;
    case MediaDataEncoder::PixelFormat::YUV420SP_NV12:
      return 2;
    default:
      VTENC_LOGE("Unsupported input pixel format");
      return 0;
  }
}

using namespace layers;

CVPixelBufferRef AppleVTEncoder::CreateCVPixelBuffer(const Image* aSource) {
  AssertOnTaskQueue();

  // TODO: support types other than YUV
  const PlanarYCbCrImage* image =
      const_cast<Image*>(aSource)->AsPlanarYCbCrImage();
  if (!image || !image->GetData()) {
    return nullptr;
  }

  OSType format = MapPixelFormat(mConfig.mSourcePixelFormat).ref();
  size_t numPlanes = NumberOfPlanes(mConfig.mSourcePixelFormat);
  const PlanarYCbCrImage::Data* yuv = image->GetData();
  void* addresses[3] = {};
  size_t widths[3] = {};
  size_t heights[3] = {};
  size_t strides[3] = {};
  switch (numPlanes) {
    case 3:
      addresses[2] = yuv->mCrChannel;
      widths[2] = yuv->mCbCrSize.width;
      heights[2] = yuv->mCbCrSize.height;
      strides[2] = yuv->mCbCrStride;
      MOZ_FALLTHROUGH;
    case 2:
      addresses[1] = yuv->mCbChannel;
      widths[1] = yuv->mCbCrSize.width;
      heights[1] = yuv->mCbCrSize.height;
      strides[1] = yuv->mCbCrStride;
      MOZ_FALLTHROUGH;
    case 1:
      addresses[0] = yuv->mYChannel;
      widths[0] = yuv->mYSize.width;
      heights[0] = yuv->mYSize.height;
      strides[0] = yuv->mYStride;
      break;
    default:
      return nullptr;
  }

  CVPixelBufferRef buffer = nullptr;
  return CVPixelBufferCreateWithPlanarBytes(
             kCFAllocatorDefault, yuv->mPicSize.width, yuv->mPicSize.height,
             format, nullptr, 0, numPlanes, addresses, widths, heights, strides,
             nullptr, nullptr, nullptr, &buffer) == kCVReturnSuccess
             ? buffer
             : nullptr;
}

RefPtr<MediaDataEncoder::EncodePromise> AppleVTEncoder::Drain() {
  return InvokeAsync(mTaskQueue, this, __func__, &AppleVTEncoder::ProcessDrain);
}

RefPtr<MediaDataEncoder::EncodePromise> AppleVTEncoder::ProcessDrain() {
  AssertOnTaskQueue();
  MOZ_ASSERT(mSession);

  if (mFramesCompleted) {
    MOZ_DIAGNOSTIC_ASSERT(mEncodedData.IsEmpty());
    return EncodePromise::CreateAndResolve(EncodedData(), __func__);
  }

  OSStatus status =
      VTCompressionSessionCompleteFrames(mSession, kCMTimeIndefinite);
  if (status != noErr) {
    return EncodePromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                          __func__);
  }
  mFramesCompleted = true;
  // VTCompressionSessionCompleteFrames() could have queued multiple tasks with
  // the new drained frames. Dispatch a task after them to resolve the promise
  // with those frames.
  RefPtr<AppleVTEncoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self]() {
    EncodedData pendingFrames(std::move(self->mEncodedData));
    self->mEncodedData = EncodedData();
    return EncodePromise::CreateAndResolve(std::move(pendingFrames), __func__);
  });
}

RefPtr<ShutdownPromise> AppleVTEncoder::Shutdown() {
  return InvokeAsync(mTaskQueue, this, __func__,
                     &AppleVTEncoder::ProcessShutdown);
}

RefPtr<ShutdownPromise> AppleVTEncoder::ProcessShutdown() {
  if (mSession) {
    VTCompressionSessionInvalidate(mSession);
    CFRelease(mSession);
    mSession = nullptr;
    mInited = false;
  }
  return ShutdownPromise::CreateAndResolve(true, __func__);
}

RefPtr<GenericPromise> AppleVTEncoder::SetBitrate(
    MediaDataEncoder::Rate aBitsPerSec) {
  RefPtr<AppleVTEncoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self, aBitsPerSec]() {
    MOZ_ASSERT(self->mSession);
    return SetAverageBitrate(self->mSession, aBitsPerSec)
               ? GenericPromise::CreateAndResolve(true, __func__)
               : GenericPromise::CreateAndReject(
                     NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR, __func__);
  });
}

}  // namespace mozilla

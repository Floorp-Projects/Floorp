/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <CoreFoundation/CFString.h>

#include "AppleUtils.h"
#include "AppleVDADecoder.h"
#include "AppleVDALinker.h"
#include "MediaInfo.h"
#include "mp4_demuxer/H264.h"
#include "MP4Decoder.h"
#include "MediaData.h"
#include "MacIOSurfaceImage.h"
#include "mozilla/ArrayUtils.h"
#include "nsAutoPtr.h"
#include "nsCocoaFeatures.h"
#include "nsThreadUtils.h"
#include "prlog.h"
#include "VideoUtils.h"
#include <algorithm>
#include "gfxPlatform.h"

#ifdef PR_LOGGING
PRLogModuleInfo* GetAppleMediaLog();
#define LOG(...) PR_LOG(GetAppleMediaLog(), PR_LOG_DEBUG, (__VA_ARGS__))
//#define LOG_MEDIA_SHA1
#else
#define LOG(...)
#endif

namespace mozilla {

AppleVDADecoder::AppleVDADecoder(const VideoInfo& aConfig,
                               FlushableMediaTaskQueue* aVideoTaskQueue,
                               MediaDataDecoderCallback* aCallback,
                               layers::ImageContainer* aImageContainer)
  : mTaskQueue(aVideoTaskQueue)
  , mCallback(aCallback)
  , mImageContainer(aImageContainer)
  , mPictureWidth(aConfig.mImage.width)
  , mPictureHeight(aConfig.mImage.height)
  , mDisplayWidth(aConfig.mDisplay.width)
  , mDisplayHeight(aConfig.mDisplay.height)
  , mDecoder(nullptr)
  , mIs106(!nsCocoaFeatures::OnLionOrLater())
{
  MOZ_COUNT_CTOR(AppleVDADecoder);
  // TODO: Verify aConfig.mime_type.

  mExtraData = aConfig.mExtraData;
  mMaxRefFrames = 4;
  // Retrieve video dimensions from H264 SPS NAL.
  mp4_demuxer::SPSData spsdata;
  if (mp4_demuxer::H264::DecodeSPSFromExtraData(mExtraData, spsdata)) {
    // max_num_ref_frames determines the size of the sliding window
    // we need to queue that many frames in order to guarantee proper
    // pts frames ordering. Use a minimum of 4 to ensure proper playback of
    // non compliant videos.
    mMaxRefFrames =
      std::min(std::max(mMaxRefFrames, spsdata.max_num_ref_frames + 1), 16u);
  }

  LOG("Creating AppleVDADecoder for %dx%d (%dx%d) h.264 video",
      mPictureWidth,
      mPictureHeight,
      mDisplayWidth,
      mDisplayHeight
     );
}

AppleVDADecoder::~AppleVDADecoder()
{
  MOZ_COUNT_DTOR(AppleVDADecoder);
}

nsresult
AppleVDADecoder::Init()
{
  if (!gfxPlatform::CanUseHardwareVideoDecoding()) {
    // This GPU is blacklisted for hardware decoding.
    return NS_ERROR_FAILURE;
  }

  if (mDecoder) {
    return NS_OK;
  }
  nsresult rv = InitializeSession();
  return rv;
}

nsresult
AppleVDADecoder::Shutdown()
{
  if (mDecoder) {
    LOG("%s: cleaning up decoder %p", __func__, mDecoder);
    VDADecoderDestroy(mDecoder);
    mDecoder = nullptr;
  }
  return NS_OK;
}

nsresult
AppleVDADecoder::Input(MediaRawData* aSample)
{
  LOG("mp4 input sample %p pts %lld duration %lld us%s %d bytes",
      aSample,
      aSample->mTime,
      aSample->mDuration,
      aSample->mKeyframe ? " keyframe" : "",
      aSample->mSize);

  mTaskQueue->Dispatch(
      NS_NewRunnableMethodWithArg<nsRefPtr<MediaRawData>>(
          this,
          &AppleVDADecoder::SubmitFrame,
          nsRefPtr<MediaRawData>(aSample)));
  return NS_OK;
}

nsresult
AppleVDADecoder::Flush()
{
  mTaskQueue->Flush();
  OSStatus rv = VDADecoderFlush(mDecoder, 0 /*dont emit*/);
  if (rv != noErr) {
    LOG("AppleVDADecoder::Flush failed waiting for platform decoder "
        "with error:%d.", rv);
  }
  ClearReorderedFrames();

  return NS_OK;
}

nsresult
AppleVDADecoder::Drain()
{
  mTaskQueue->AwaitIdle();
  OSStatus rv = VDADecoderFlush(mDecoder, kVDADecoderFlush_EmitFrames);
  if (rv != noErr) {
    LOG("AppleVDADecoder::Drain failed waiting for platform decoder "
        "with error:%d.", rv);
  }
  DrainReorderedFrames();
  mCallback->DrainComplete();
  return NS_OK;
}

//
// Implementation details.
//

// Callback passed to the VideoToolbox decoder for returning data.
// This needs to be static because the API takes a C-style pair of
// function and userdata pointers. This validates parameters and
// forwards the decoded image back to an object method.
static void
PlatformCallback(void* decompressionOutputRefCon,
                 CFDictionaryRef frameInfo,
                 OSStatus status,
                 VDADecodeInfoFlags infoFlags,
                 CVImageBufferRef image)
{
  LOG("AppleVDADecoder[%s] status %d flags %d retainCount %ld",
      __func__, status, infoFlags, CFGetRetainCount(frameInfo));

  // Validate our arguments.
  // According to Apple's TN2267
  // The output callback is still called for all flushed frames,
  // but no image buffers will be returned.
  // FIXME: Distinguish between errors and empty flushed frames.
  if (status != noErr || !image) {
    NS_WARNING("AppleVDADecoder decoder returned no data");
    return;
  }
  MOZ_ASSERT(CFGetTypeID(image) == CVPixelBufferGetTypeID(),
             "AppleVDADecoder returned an unexpected image type");

  if (infoFlags & kVDADecodeInfo_FrameDropped)
  {
    NS_WARNING("  ...frame dropped...");
    return;
  }

  AppleVDADecoder* decoder =
    static_cast<AppleVDADecoder*>(decompressionOutputRefCon);

  AutoCFRelease<CFNumberRef> ptsref =
    (CFNumberRef)CFDictionaryGetValue(frameInfo, CFSTR("FRAME_PTS"));
  AutoCFRelease<CFNumberRef> dtsref =
    (CFNumberRef)CFDictionaryGetValue(frameInfo, CFSTR("FRAME_DTS"));
  AutoCFRelease<CFNumberRef> durref =
    (CFNumberRef)CFDictionaryGetValue(frameInfo, CFSTR("FRAME_DURATION"));
  AutoCFRelease<CFNumberRef> boref =
    (CFNumberRef)CFDictionaryGetValue(frameInfo, CFSTR("FRAME_OFFSET"));
  AutoCFRelease<CFNumberRef> kfref =
    (CFNumberRef)CFDictionaryGetValue(frameInfo, CFSTR("FRAME_KEYFRAME"));

  Microseconds dts;
  Microseconds pts;
  Microseconds duration;
  int64_t byte_offset;
  char is_sync_point;

  CFNumberGetValue(ptsref, kCFNumberSInt64Type, &pts);
  CFNumberGetValue(dtsref, kCFNumberSInt64Type, &dts);
  CFNumberGetValue(durref, kCFNumberSInt64Type, &duration);
  CFNumberGetValue(boref, kCFNumberSInt64Type, &byte_offset);
  CFNumberGetValue(kfref, kCFNumberSInt8Type, &is_sync_point);

  nsAutoPtr<AppleVDADecoder::AppleFrameRef> frameRef(
    new AppleVDADecoder::AppleFrameRef(dts,
    pts,
    duration,
    byte_offset,
    is_sync_point == 1));

  // Forward the data back to an object method which can access
  // the correct MP4Reader callback.
  decoder->OutputFrame(image, frameRef);
}

AppleVDADecoder::AppleFrameRef*
AppleVDADecoder::CreateAppleFrameRef(const MediaRawData* aSample)
{
  MOZ_ASSERT(aSample);
  return new AppleFrameRef(*aSample);
}

void
AppleVDADecoder::DrainReorderedFrames()
{
  while (!mReorderQueue.IsEmpty()) {
    mCallback->Output(mReorderQueue.Pop());
  }
}

void
AppleVDADecoder::ClearReorderedFrames()
{
  while (!mReorderQueue.IsEmpty()) {
    mReorderQueue.Pop();
  }
}

// Copy and return a decoded frame.
nsresult
AppleVDADecoder::OutputFrame(CVPixelBufferRef aImage,
                             nsAutoPtr<AppleVDADecoder::AppleFrameRef> aFrameRef)
{
  IOSurfacePtr surface = MacIOSurfaceLib::CVPixelBufferGetIOSurface(aImage);
  MOZ_ASSERT(surface, "Decoder didn't return an IOSurface backed buffer");

  LOG("mp4 output frame %lld dts %lld pts %lld duration %lld us%s",
    aFrameRef->byte_offset,
    aFrameRef->decode_timestamp,
    aFrameRef->composition_timestamp,
    aFrameRef->duration,
    aFrameRef->is_sync_point ? " keyframe" : ""
  );

  nsRefPtr<MacIOSurface> macSurface = new MacIOSurface(surface);
  // Bounds.
  VideoInfo info;
  info.mDisplay = nsIntSize(mDisplayWidth, mDisplayHeight);
  info.mHasVideo = true;
  gfx::IntRect visible = gfx::IntRect(0,
                                      0,
                                      mPictureWidth,
                                      mPictureHeight);

  nsRefPtr<layers::Image> image =
    mImageContainer->CreateImage(ImageFormat::MAC_IOSURFACE);
  layers::MacIOSurfaceImage* videoImage =
    static_cast<layers::MacIOSurfaceImage*>(image.get());
  videoImage->SetSurface(macSurface);

  nsRefPtr<VideoData> data;
  data = VideoData::CreateFromImage(info,
                                    mImageContainer,
                                    aFrameRef->byte_offset,
                                    aFrameRef->composition_timestamp,
                                    aFrameRef->duration, image.forget(),
                                    aFrameRef->is_sync_point,
                                    aFrameRef->decode_timestamp,
                                    visible);

  if (!data) {
    NS_ERROR("Couldn't create VideoData for frame");
    mCallback->Error();
    return NS_ERROR_FAILURE;
  }

  // Frames come out in DTS order but we need to output them
  // in composition order.
  mReorderQueue.Push(data);
  while (mReorderQueue.Length() > mMaxRefFrames) {
    mCallback->Output(mReorderQueue.Pop());
  }
  LOG("%llu decoded frames queued",
      static_cast<unsigned long long>(mReorderQueue.Length()));

  return NS_OK;
}

nsresult
AppleVDADecoder::SubmitFrame(MediaRawData* aSample)
{
  AutoCFRelease<CFDataRef> block =
    CFDataCreate(kCFAllocatorDefault, aSample->mData, aSample->mSize);
  if (!block) {
    NS_ERROR("Couldn't create CFData");
    return NS_ERROR_FAILURE;
  }

  AutoCFRelease<CFNumberRef> pts =
    CFNumberCreate(kCFAllocatorDefault,
                   kCFNumberSInt64Type,
                   &aSample->mTime);
  AutoCFRelease<CFNumberRef> dts =
    CFNumberCreate(kCFAllocatorDefault,
                   kCFNumberSInt64Type,
                   &aSample->mTimecode);
  AutoCFRelease<CFNumberRef> duration =
    CFNumberCreate(kCFAllocatorDefault,
                   kCFNumberSInt64Type,
                   &aSample->mDuration);
  AutoCFRelease<CFNumberRef> byte_offset =
    CFNumberCreate(kCFAllocatorDefault,
                   kCFNumberSInt64Type,
                   &aSample->mOffset);
  char keyframe = aSample->mKeyframe ? 1 : 0;
  AutoCFRelease<CFNumberRef> cfkeyframe =
    CFNumberCreate(kCFAllocatorDefault,
                   kCFNumberSInt8Type,
                   &keyframe);

  const void* keys[] = { CFSTR("FRAME_PTS"),
                         CFSTR("FRAME_DTS"),
                         CFSTR("FRAME_DURATION"),
                         CFSTR("FRAME_OFFSET"),
                         CFSTR("FRAME_KEYFRAME") };
  const void* values[] = { pts,
                           dts,
                           duration,
                           byte_offset,
                           cfkeyframe };
  static_assert(ArrayLength(keys) == ArrayLength(values),
                "Non matching keys/values array size");

  AutoCFRelease<CFDictionaryRef> frameInfo =
    CFDictionaryCreate(kCFAllocatorDefault,
                       keys,
                       values,
                       ArrayLength(keys),
                       &kCFTypeDictionaryKeyCallBacks,
                       &kCFTypeDictionaryValueCallBacks);

  OSStatus rv = VDADecoderDecode(mDecoder,
                                 0,
                                 block,
                                 frameInfo);

  if (rv != noErr) {
    NS_WARNING("AppleVDADecoder: Couldn't pass frame to decoder");
    return NS_ERROR_FAILURE;
  }

  if (mIs106) {
    // TN2267:
    // frameInfo: A CFDictionaryRef containing information to be returned in
    // the output callback for this frame.
    // This dictionary can contain client provided information associated with
    // the frame being decoded, for example presentation time.
    // The CFDictionaryRef will be retained by the framework.
    // In 10.6, it is released one too many. So retain it.
    CFRetain(frameInfo);
  }

  // Ask for more data.
  if (mTaskQueue->IsEmpty()) {
    LOG("AppleVDADecoder task queue empty; requesting more data");
    mCallback->InputExhausted();
  }

  return NS_OK;
}

nsresult
AppleVDADecoder::InitializeSession()
{
  OSStatus rv;

  AutoCFRelease<CFDictionaryRef> decoderConfig =
    CreateDecoderSpecification();

  AutoCFRelease<CFDictionaryRef> outputConfiguration =
    CreateOutputConfiguration();

  rv =
    VDADecoderCreate(decoderConfig,
                     outputConfiguration,
                     (VDADecoderOutputCallback*)PlatformCallback,
                     this,
                     &mDecoder);

  if (rv != noErr) {
    NS_WARNING("AppleVDADecoder: Couldn't create hardware VDA decoder");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

CFDictionaryRef
AppleVDADecoder::CreateDecoderSpecification()
{
  const uint8_t* extradata = mExtraData->Elements();
  int extrasize = mExtraData->Length();

  OSType format = 'avc1';
  AutoCFRelease<CFNumberRef> avc_width  =
    CFNumberCreate(kCFAllocatorDefault,
                   kCFNumberSInt32Type,
                   &mPictureWidth);
  AutoCFRelease<CFNumberRef> avc_height =
    CFNumberCreate(kCFAllocatorDefault,
                   kCFNumberSInt32Type,
                   &mPictureHeight);
  AutoCFRelease<CFNumberRef> avc_format =
    CFNumberCreate(kCFAllocatorDefault,
                   kCFNumberSInt32Type,
                   &format);
  AutoCFRelease<CFDataRef> avc_data =
    CFDataCreate(kCFAllocatorDefault,
                 extradata,
                 extrasize);

  const void* decoderKeys[] = { AppleVDALinker::skPropWidth,
                                AppleVDALinker::skPropHeight,
                                AppleVDALinker::skPropSourceFormat,
                                AppleVDALinker::skPropAVCCData };
  const void* decoderValue[] = { avc_width,
                                 avc_height,
                                 avc_format,
                                 avc_data };
  static_assert(ArrayLength(decoderKeys) == ArrayLength(decoderValue),
                "Non matching keys/values array size");

  return CFDictionaryCreate(kCFAllocatorDefault,
                            decoderKeys,
                            decoderValue,
                            ArrayLength(decoderKeys),
                            &kCFTypeDictionaryKeyCallBacks,
                            &kCFTypeDictionaryValueCallBacks);
}

CFDictionaryRef
AppleVDADecoder::CreateOutputConfiguration()
{
  // Construct IOSurface Properties
  const void* IOSurfaceKeys[] = { MacIOSurfaceLib::kPropIsGlobal };
  const void* IOSurfaceValues[] = { kCFBooleanTrue };
  static_assert(ArrayLength(IOSurfaceKeys) == ArrayLength(IOSurfaceValues),
                "Non matching keys/values array size");

  // Contruct output configuration.
  AutoCFRelease<CFDictionaryRef> IOSurfaceProperties =
    CFDictionaryCreate(kCFAllocatorDefault,
                       IOSurfaceKeys,
                       IOSurfaceValues,
                       ArrayLength(IOSurfaceKeys),
                       &kCFTypeDictionaryKeyCallBacks,
                       &kCFTypeDictionaryValueCallBacks);

  SInt32 PixelFormatTypeValue = kCVPixelFormatType_32BGRA;
  AutoCFRelease<CFNumberRef> PixelFormatTypeNumber =
    CFNumberCreate(kCFAllocatorDefault,
                   kCFNumberSInt32Type,
                   &PixelFormatTypeValue);

  const void* outputKeys[] = { kCVPixelBufferIOSurfacePropertiesKey,
                               kCVPixelBufferPixelFormatTypeKey,
                               kCVPixelBufferOpenGLCompatibilityKey };
  const void* outputValues[] = { IOSurfaceProperties,
                                 PixelFormatTypeNumber,
                                 kCFBooleanTrue };
  static_assert(ArrayLength(outputKeys) == ArrayLength(outputValues),
                "Non matching keys/values array size");

  return CFDictionaryCreate(kCFAllocatorDefault,
                            outputKeys,
                            outputValues,
                            ArrayLength(outputKeys),
                            &kCFTypeDictionaryKeyCallBacks,
                            &kCFTypeDictionaryValueCallBacks);
}

/* static */
already_AddRefed<AppleVDADecoder>
AppleVDADecoder::CreateVDADecoder(
  const VideoInfo& aConfig,
  FlushableMediaTaskQueue* aVideoTaskQueue,
  MediaDataDecoderCallback* aCallback,
  layers::ImageContainer* aImageContainer)
{
  nsRefPtr<AppleVDADecoder> decoder =
    new AppleVDADecoder(aConfig, aVideoTaskQueue, aCallback, aImageContainer);
  if (NS_FAILED(decoder->Init())) {
    return nullptr;
  }
  return decoder.forget();
}

} // namespace mozilla

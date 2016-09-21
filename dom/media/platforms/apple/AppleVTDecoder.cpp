/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <CoreFoundation/CFString.h>

#include "AppleCMLinker.h"
#include "AppleDecoderModule.h"
#include "AppleUtils.h"
#include "AppleVTDecoder.h"
#include "AppleVTLinker.h"
#include "MediaData.h"
#include "mozilla/ArrayUtils.h"
#include "mp4_demuxer/H264.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "mozilla/Logging.h"
#include "VideoUtils.h"
#include "gfxPlatform.h"

#define LOG(...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {

AppleVTDecoder::AppleVTDecoder(const VideoInfo& aConfig,
                               TaskQueue* aTaskQueue,
                               MediaDataDecoderCallback* aCallback,
                               layers::ImageContainer* aImageContainer)
  : mExtraData(aConfig.mExtraData)
  , mCallback(aCallback)
  , mPictureWidth(aConfig.mImage.width)
  , mPictureHeight(aConfig.mImage.height)
  , mDisplayWidth(aConfig.mDisplay.width)
  , mDisplayHeight(aConfig.mDisplay.height)
  , mTaskQueue(aTaskQueue)
  , mMaxRefFrames(mp4_demuxer::H264::ComputeMaxRefFrames(aConfig.mExtraData))
  , mImageContainer(aImageContainer)
  , mIsShutDown(false)
#ifdef MOZ_WIDGET_UIKIT
  , mUseSoftwareImages(true)
#else
  , mUseSoftwareImages(false)
#endif
  , mIsFlushing(false)
  , mMonitor("AppleVideoDecoder")
  , mFormat(nullptr)
  , mSession(nullptr)
  , mIsHardwareAccelerated(false)
{
  MOZ_COUNT_CTOR(AppleVTDecoder);
  // TODO: Verify aConfig.mime_type.
  LOG("Creating AppleVTDecoder for %dx%d h.264 video",
      mDisplayWidth,
      mDisplayHeight
     );
}

AppleVTDecoder::~AppleVTDecoder()
{
  MOZ_COUNT_DTOR(AppleVTDecoder);
}

RefPtr<MediaDataDecoder::InitPromise>
AppleVTDecoder::Init()
{
  nsresult rv = InitializeSession();

  if (NS_SUCCEEDED(rv)) {
    return InitPromise::CreateAndResolve(TrackType::kVideoTrack, __func__);
  }

  return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
}

void
AppleVTDecoder::Input(MediaRawData* aSample)
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());

  LOG("mp4 input sample %p pts %lld duration %lld us%s %d bytes",
      aSample,
      aSample->mTime,
      aSample->mDuration,
      aSample->mKeyframe ? " keyframe" : "",
      aSample->Size());

  mTaskQueue->Dispatch(NewRunnableMethod<RefPtr<MediaRawData>>(
    this, &AppleVTDecoder::ProcessDecode, aSample));
}

void
AppleVTDecoder::Flush()
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  mIsFlushing = true;
  nsCOMPtr<nsIRunnable> runnable =
    NewRunnableMethod(this, &AppleVTDecoder::ProcessFlush);
  SyncRunnable::DispatchToThread(mTaskQueue, runnable);
  mIsFlushing = false;

  mSeekTargetThreshold.reset();
}

void
AppleVTDecoder::Drain()
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  nsCOMPtr<nsIRunnable> runnable =
    NewRunnableMethod(this, &AppleVTDecoder::ProcessDrain);
  mTaskQueue->Dispatch(runnable.forget());
}

void
AppleVTDecoder::Shutdown()
{
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);
  mIsShutDown = true;
  if (mTaskQueue) {
    nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod(this, &AppleVTDecoder::ProcessShutdown);
    mTaskQueue->Dispatch(runnable.forget());
  } else {
    ProcessShutdown();
  }
}

nsresult
AppleVTDecoder::ProcessDecode(MediaRawData* aSample)
{
  AssertOnTaskQueueThread();

  if (mIsFlushing) {
    return NS_OK;
  }

  auto rv = DoDecode(aSample);

  return rv;
}

void
AppleVTDecoder::ProcessShutdown()
{
  if (mSession) {
    LOG("%s: cleaning up session %p", __func__, mSession);
    VTDecompressionSessionInvalidate(mSession);
    CFRelease(mSession);
    mSession = nullptr;
  }
  if (mFormat) {
    LOG("%s: releasing format %p", __func__, mFormat);
    CFRelease(mFormat);
    mFormat = nullptr;
  }
}

void
AppleVTDecoder::ProcessFlush()
{
  AssertOnTaskQueueThread();
  nsresult rv = WaitForAsynchronousFrames();
  if (NS_FAILED(rv)) {
    LOG("AppleVTDecoder::Flush failed waiting for platform decoder "
        "with error:%d.", rv);
  }
  ClearReorderedFrames();
}

void
AppleVTDecoder::ProcessDrain()
{
  AssertOnTaskQueueThread();
  nsresult rv = WaitForAsynchronousFrames();
  if (NS_FAILED(rv)) {
    LOG("AppleVTDecoder::Drain failed waiting for platform decoder "
        "with error:%d.", rv);
  }
  DrainReorderedFrames();
  mCallback->DrainComplete();
}

AppleVTDecoder::AppleFrameRef*
AppleVTDecoder::CreateAppleFrameRef(const MediaRawData* aSample)
{
  MOZ_ASSERT(aSample);
  return new AppleFrameRef(*aSample);
}

void
AppleVTDecoder::DrainReorderedFrames()
{
  MonitorAutoLock mon(mMonitor);
  while (!mReorderQueue.IsEmpty()) {
    mCallback->Output(mReorderQueue.Pop().get());
  }
}

void
AppleVTDecoder::ClearReorderedFrames()
{
  MonitorAutoLock mon(mMonitor);
  while (!mReorderQueue.IsEmpty()) {
    mReorderQueue.Pop();
  }
}

void
AppleVTDecoder::SetSeekThreshold(const media::TimeUnit& aTime)
{
  LOG("SetSeekThreshold %lld", aTime.ToMicroseconds());
  mSeekTargetThreshold = Some(aTime);
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
                 void* sourceFrameRefCon,
                 OSStatus status,
                 VTDecodeInfoFlags flags,
                 CVImageBufferRef image,
                 CMTime presentationTimeStamp,
                 CMTime presentationDuration)
{
  LOG("AppleVideoDecoder %s status %d flags %d", __func__, status, flags);

  AppleVTDecoder* decoder =
    static_cast<AppleVTDecoder*>(decompressionOutputRefCon);
  nsAutoPtr<AppleVTDecoder::AppleFrameRef> frameRef(
    static_cast<AppleVTDecoder::AppleFrameRef*>(sourceFrameRefCon));

  // Validate our arguments.
  if (status != noErr || !image) {
    NS_WARNING("VideoToolbox decoder returned no data");
    image = nullptr;
  } else if (flags & kVTDecodeInfo_FrameDropped) {
    NS_WARNING("  ...frame tagged as dropped...");
  } else {
    MOZ_ASSERT(CFGetTypeID(image) == CVPixelBufferGetTypeID(),
      "VideoToolbox returned an unexpected image type");
  }
  decoder->OutputFrame(image, *frameRef);
}

// Copy and return a decoded frame.
nsresult
AppleVTDecoder::OutputFrame(CVPixelBufferRef aImage,
                            AppleVTDecoder::AppleFrameRef aFrameRef)
{
  if (mIsShutDown || mIsFlushing) {
    // We are in the process of flushing or shutting down; ignore frame.
    return NS_OK;
  }

  LOG("mp4 output frame %lld dts %lld pts %lld duration %lld us%s",
      aFrameRef.byte_offset,
      aFrameRef.decode_timestamp.ToMicroseconds(),
      aFrameRef.composition_timestamp.ToMicroseconds(),
      aFrameRef.duration.ToMicroseconds(),
      aFrameRef.is_sync_point ? " keyframe" : ""
  );

  if (!aImage) {
    // Image was dropped by decoder or none return yet.
    // We need more input to continue.
    mCallback->InputExhausted();
    return NS_OK;
  }

  bool useNullSample = false;
  if (mSeekTargetThreshold.isSome()) {
    if ((aFrameRef.composition_timestamp + aFrameRef.duration) < mSeekTargetThreshold.ref()) {
      useNullSample = true;
    } else {
      mSeekTargetThreshold.reset();
    }
  }

  // Where our resulting image will end up.
  RefPtr<MediaData> data;
  // Bounds.
  VideoInfo info;
  info.mDisplay = nsIntSize(mDisplayWidth, mDisplayHeight);
  gfx::IntRect visible = gfx::IntRect(0,
                                      0,
                                      mPictureWidth,
                                      mPictureHeight);

  if (useNullSample) {
    data = new NullData(aFrameRef.byte_offset,
                        aFrameRef.composition_timestamp.ToMicroseconds(),
                        aFrameRef.duration.ToMicroseconds());
  } else if (mUseSoftwareImages) {
    size_t width = CVPixelBufferGetWidth(aImage);
    size_t height = CVPixelBufferGetHeight(aImage);
    DebugOnly<size_t> planes = CVPixelBufferGetPlaneCount(aImage);
    MOZ_ASSERT(planes == 2, "Likely not NV12 format and it must be.");

    VideoData::YCbCrBuffer buffer;

    // Lock the returned image data.
    CVReturn rv = CVPixelBufferLockBaseAddress(aImage, kCVPixelBufferLock_ReadOnly);
    if (rv != kCVReturnSuccess) {
      NS_ERROR("error locking pixel data");
      mCallback->Error(
        MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                    RESULT_DETAIL("CVPixelBufferLockBaseAddress:%x", rv)));
      return NS_ERROR_DOM_MEDIA_DECODE_ERR;
    }
    // Y plane.
    buffer.mPlanes[0].mData =
      static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(aImage, 0));
    buffer.mPlanes[0].mStride = CVPixelBufferGetBytesPerRowOfPlane(aImage, 0);
    buffer.mPlanes[0].mWidth = width;
    buffer.mPlanes[0].mHeight = height;
    buffer.mPlanes[0].mOffset = 0;
    buffer.mPlanes[0].mSkip = 0;
    // Cb plane.
    buffer.mPlanes[1].mData =
      static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(aImage, 1));
    buffer.mPlanes[1].mStride = CVPixelBufferGetBytesPerRowOfPlane(aImage, 1);
    buffer.mPlanes[1].mWidth = (width+1) / 2;
    buffer.mPlanes[1].mHeight = (height+1) / 2;
    buffer.mPlanes[1].mOffset = 0;
    buffer.mPlanes[1].mSkip = 1;
    // Cr plane.
    buffer.mPlanes[2].mData =
      static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(aImage, 1));
    buffer.mPlanes[2].mStride = CVPixelBufferGetBytesPerRowOfPlane(aImage, 1);
    buffer.mPlanes[2].mWidth = (width+1) / 2;
    buffer.mPlanes[2].mHeight = (height+1) / 2;
    buffer.mPlanes[2].mOffset = 1;
    buffer.mPlanes[2].mSkip = 1;

    // Copy the image data into our own format.
    data =
      VideoData::CreateAndCopyData(info,
                                   mImageContainer,
                                   aFrameRef.byte_offset,
                                   aFrameRef.composition_timestamp.ToMicroseconds(),
                                   aFrameRef.duration.ToMicroseconds(),
                                   buffer,
                                   aFrameRef.is_sync_point,
                                   aFrameRef.decode_timestamp.ToMicroseconds(),
                                   visible);
    // Unlock the returned image data.
    CVPixelBufferUnlockBaseAddress(aImage, kCVPixelBufferLock_ReadOnly);
  } else {
#ifndef MOZ_WIDGET_UIKIT
    IOSurfacePtr surface = MacIOSurfaceLib::CVPixelBufferGetIOSurface(aImage);
    MOZ_ASSERT(surface, "Decoder didn't return an IOSurface backed buffer");

    RefPtr<MacIOSurface> macSurface = new MacIOSurface(surface);

    RefPtr<layers::Image> image = new MacIOSurfaceImage(macSurface);

    data =
      VideoData::CreateFromImage(info,
                                 aFrameRef.byte_offset,
                                 aFrameRef.composition_timestamp.ToMicroseconds(),
                                 aFrameRef.duration.ToMicroseconds(),
                                 image.forget(),
                                 aFrameRef.is_sync_point,
                                 aFrameRef.decode_timestamp.ToMicroseconds(),
                                 visible);
#else
    MOZ_ASSERT_UNREACHABLE("No MacIOSurface on iOS");
#endif
  }

  if (!data) {
    NS_ERROR("Couldn't create VideoData for frame");
    mCallback->Error(MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Frames come out in DTS order but we need to output them
  // in composition order.
  MonitorAutoLock mon(mMonitor);
  mReorderQueue.Push(data);
  if (mReorderQueue.Length() > mMaxRefFrames) {
    mCallback->Output(mReorderQueue.Pop().get());
  }
  mCallback->InputExhausted();
  LOG("%llu decoded frames queued",
      static_cast<unsigned long long>(mReorderQueue.Length()));

  return NS_OK;
}

nsresult
AppleVTDecoder::WaitForAsynchronousFrames()
{
  OSStatus rv = VTDecompressionSessionWaitForAsynchronousFrames(mSession);
  if (rv != noErr) {
    LOG("AppleVTDecoder: Error %d waiting for asynchronous frames", rv);
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// Helper to fill in a timestamp structure.
static CMSampleTimingInfo
TimingInfoFromSample(MediaRawData* aSample)
{
  CMSampleTimingInfo timestamp;

  timestamp.duration = CMTimeMake(aSample->mDuration, USECS_PER_S);
  timestamp.presentationTimeStamp =
    CMTimeMake(aSample->mTime, USECS_PER_S);
  timestamp.decodeTimeStamp =
    CMTimeMake(aSample->mTimecode, USECS_PER_S);

  return timestamp;
}

MediaResult
AppleVTDecoder::DoDecode(MediaRawData* aSample)
{
  AssertOnTaskQueueThread();

  // For some reason this gives me a double-free error with stagefright.
  AutoCFRelease<CMBlockBufferRef> block = nullptr;
  AutoCFRelease<CMSampleBufferRef> sample = nullptr;
  VTDecodeInfoFlags infoFlags;
  OSStatus rv;

  // FIXME: This copies the sample data. I think we can provide
  // a custom block source which reuses the aSample buffer.
  // But note that there may be a problem keeping the samples
  // alive over multiple frames.
  rv = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, // Struct allocator.
                                          const_cast<uint8_t*>(aSample->Data()),
                                          aSample->Size(),
                                          kCFAllocatorNull, // Block allocator.
                                          NULL, // Block source.
                                          0,    // Data offset.
                                          aSample->Size(),
                                          false,
                                          block.receive());
  if (rv != noErr) {
    NS_ERROR("Couldn't create CMBlockBuffer");
    mCallback->Error(
      MediaResult(NS_ERROR_OUT_OF_MEMORY,
                  RESULT_DETAIL("CMBlockBufferCreateWithMemoryBlock:%x", rv)));
    return MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__);
  }
  CMSampleTimingInfo timestamp = TimingInfoFromSample(aSample);
  rv = CMSampleBufferCreate(kCFAllocatorDefault, block, true, 0, 0, mFormat, 1, 1, &timestamp, 0, NULL, sample.receive());
  if (rv != noErr) {
    NS_ERROR("Couldn't create CMSampleBuffer");
    mCallback->Error(MediaResult(NS_ERROR_OUT_OF_MEMORY,
                                 RESULT_DETAIL("CMSampleBufferCreate:%x", rv)));
    return MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__);
  }

  VTDecodeFrameFlags decodeFlags =
    kVTDecodeFrame_EnableAsynchronousDecompression;
  rv = VTDecompressionSessionDecodeFrame(mSession,
                                         sample,
                                         decodeFlags,
                                         CreateAppleFrameRef(aSample),
                                         &infoFlags);
  if (rv != noErr && !(infoFlags & kVTDecodeInfo_FrameDropped)) {
    LOG("AppleVTDecoder: Error %d VTDecompressionSessionDecodeFrame", rv);
    NS_WARNING("Couldn't pass frame to decoder");
    mCallback->Error(
      MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                  RESULT_DETAIL("VTDecompressionSessionDecodeFrame:%x", rv)));
    return NS_ERROR_DOM_MEDIA_DECODE_ERR;
  }

  return NS_OK;
}

nsresult
AppleVTDecoder::InitializeSession()
{
  OSStatus rv;

  AutoCFRelease<CFDictionaryRef> extensions = CreateDecoderExtensions();

  rv = CMVideoFormatDescriptionCreate(kCFAllocatorDefault,
                                      kCMVideoCodecType_H264,
                                      mPictureWidth,
                                      mPictureHeight,
                                      extensions,
                                      &mFormat);
  if (rv != noErr) {
    NS_ERROR("Couldn't create format description!");
    return NS_ERROR_FAILURE;
  }

  // Contruct video decoder selection spec.
  AutoCFRelease<CFDictionaryRef> spec = CreateDecoderSpecification();

  // Contruct output configuration.
  AutoCFRelease<CFDictionaryRef> outputConfiguration =
    CreateOutputConfiguration();

  VTDecompressionOutputCallbackRecord cb = { PlatformCallback, this };
  rv = VTDecompressionSessionCreate(kCFAllocatorDefault,
                                    mFormat,
                                    spec, // Video decoder selection.
                                    outputConfiguration, // Output video format.
                                    &cb,
                                    &mSession);

  if (rv != noErr) {
    NS_ERROR("Couldn't create decompression session!");
    return NS_ERROR_FAILURE;
  }

  if (AppleVTLinker::skPropUsingHWAccel) {
    CFBooleanRef isUsingHW = nullptr;
    rv = VTSessionCopyProperty(mSession,
                               AppleVTLinker::skPropUsingHWAccel,
                               kCFAllocatorDefault,
                               &isUsingHW);
    if (rv != noErr) {
      LOG("AppleVTDecoder: system doesn't support hardware acceleration");
    }
    mIsHardwareAccelerated = rv == noErr && isUsingHW == kCFBooleanTrue;
    LOG("AppleVTDecoder: %s hardware accelerated decoding",
        mIsHardwareAccelerated ? "using" : "not using");
  } else {
    LOG("AppleVTDecoder: couldn't determine hardware acceleration status.");
  }
  return NS_OK;
}

CFDictionaryRef
AppleVTDecoder::CreateDecoderExtensions()
{
  AutoCFRelease<CFDataRef> avc_data =
    CFDataCreate(kCFAllocatorDefault,
                 mExtraData->Elements(),
                 mExtraData->Length());

  const void* atomsKey[] = { CFSTR("avcC") };
  const void* atomsValue[] = { avc_data };
  static_assert(ArrayLength(atomsKey) == ArrayLength(atomsValue),
                "Non matching keys/values array size");

  AutoCFRelease<CFDictionaryRef> atoms =
    CFDictionaryCreate(kCFAllocatorDefault,
                       atomsKey,
                       atomsValue,
                       ArrayLength(atomsKey),
                       &kCFTypeDictionaryKeyCallBacks,
                       &kCFTypeDictionaryValueCallBacks);

  const void* extensionKeys[] =
    { kCVImageBufferChromaLocationBottomFieldKey,
      kCVImageBufferChromaLocationTopFieldKey,
      AppleCMLinker::skPropExtensionAtoms };

  const void* extensionValues[] =
    { kCVImageBufferChromaLocation_Left,
      kCVImageBufferChromaLocation_Left,
      atoms };
  static_assert(ArrayLength(extensionKeys) == ArrayLength(extensionValues),
                "Non matching keys/values array size");

  return CFDictionaryCreate(kCFAllocatorDefault,
                            extensionKeys,
                            extensionValues,
                            ArrayLength(extensionKeys),
                            &kCFTypeDictionaryKeyCallBacks,
                            &kCFTypeDictionaryValueCallBacks);
}

CFDictionaryRef
AppleVTDecoder::CreateDecoderSpecification()
{
  if (!AppleVTLinker::skPropEnableHWAccel) {
    return nullptr;
  }

  const void* specKeys[] = { AppleVTLinker::skPropEnableHWAccel };
  const void* specValues[1];
  if (AppleDecoderModule::sCanUseHardwareVideoDecoder) {
    specValues[0] = kCFBooleanTrue;
  } else {
    // This GPU is blacklisted for hardware decoding.
    specValues[0] = kCFBooleanFalse;
  }
  static_assert(ArrayLength(specKeys) == ArrayLength(specValues),
                "Non matching keys/values array size");

  return CFDictionaryCreate(kCFAllocatorDefault,
                            specKeys,
                            specValues,
                            ArrayLength(specKeys),
                            &kCFTypeDictionaryKeyCallBacks,
                            &kCFTypeDictionaryValueCallBacks);
}

CFDictionaryRef
AppleVTDecoder::CreateOutputConfiguration()
{
  if (mUseSoftwareImages) {
    // Output format type:
    SInt32 PixelFormatTypeValue =
      kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    AutoCFRelease<CFNumberRef> PixelFormatTypeNumber =
      CFNumberCreate(kCFAllocatorDefault,
                     kCFNumberSInt32Type,
                     &PixelFormatTypeValue);
    const void* outputKeys[] = { kCVPixelBufferPixelFormatTypeKey };
    const void* outputValues[] = { PixelFormatTypeNumber };
    static_assert(ArrayLength(outputKeys) == ArrayLength(outputValues),
                  "Non matching keys/values array size");

    return CFDictionaryCreate(kCFAllocatorDefault,
                              outputKeys,
                              outputValues,
                              ArrayLength(outputKeys),
                              &kCFTypeDictionaryKeyCallBacks,
                              &kCFTypeDictionaryValueCallBacks);
  }

#ifndef MOZ_WIDGET_UIKIT
  // Output format type:
  SInt32 PixelFormatTypeValue = kCVPixelFormatType_422YpCbCr8;
  AutoCFRelease<CFNumberRef> PixelFormatTypeNumber =
    CFNumberCreate(kCFAllocatorDefault,
                   kCFNumberSInt32Type,
                   &PixelFormatTypeValue);
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
#else
  MOZ_ASSERT_UNREACHABLE("No MacIOSurface on iOS");
#endif
}


} // namespace mozilla

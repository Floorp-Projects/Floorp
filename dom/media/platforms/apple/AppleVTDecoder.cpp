/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <CoreFoundation/CFString.h>

#include "AppleCMLinker.h"
#include "AppleUtils.h"
#include "AppleVTDecoder.h"
#include "AppleVTLinker.h"
#include "mp4_demuxer/H264.h"
#include "MediaData.h"
#include "mozilla/ArrayUtils.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "mozilla/Logging.h"
#include "VideoUtils.h"
#include "gfxPlatform.h"

PRLogModuleInfo* GetAppleMediaLog();
#define LOG(...) MOZ_LOG(GetAppleMediaLog(), mozilla::LogLevel::Debug, (__VA_ARGS__))
//#define LOG_MEDIA_SHA1

#ifdef LOG_MEDIA_SHA1
#include "mozilla/SHA1.h"
#endif

namespace mozilla {

AppleVTDecoder::AppleVTDecoder(const VideoInfo& aConfig,
                               FlushableTaskQueue* aVideoTaskQueue,
                               MediaDataDecoderCallback* aCallback,
                               layers::ImageContainer* aImageContainer)
  : AppleVDADecoder(aConfig, aVideoTaskQueue, aCallback, aImageContainer)
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

nsRefPtr<MediaDataDecoder::InitPromise>
AppleVTDecoder::Init()
{
  nsresult rv = InitializeSession();

  if (NS_SUCCEEDED(rv)) {
    return InitPromise::CreateAndResolve(TrackType::kVideoTrack, __func__);
  }

  return InitPromise::CreateAndReject(DecoderFailureReason::INIT_ERROR, __func__);
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

nsresult
AppleVTDecoder::Input(MediaRawData* aSample)
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());

  LOG("mp4 input sample %p pts %lld duration %lld us%s %d bytes",
      aSample,
      aSample->mTime,
      aSample->mDuration,
      aSample->mKeyframe ? " keyframe" : "",
      aSample->Size());

#ifdef LOG_MEDIA_SHA1
  SHA1Sum hash;
  hash.update(aSample->data, aSample->size);
  uint8_t digest_buf[SHA1Sum::kHashSize];
  hash.finish(digest_buf);
  nsAutoCString digest;
  for (size_t i = 0; i < sizeof(digest_buf); i++) {
    digest.AppendPrintf("%02x", digest_buf[i]);
  }
  LOG("    sha1 %s", digest.get());
#endif // LOG_MEDIA_SHA1

  mInputIncoming++;

  nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableMethodWithArg<nsRefPtr<MediaRawData>>(
          this, &AppleVTDecoder::SubmitFrame, aSample);
  mTaskQueue->Dispatch(runnable.forget());
  return NS_OK;
}

void
AppleVTDecoder::ProcessFlush()
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  nsresult rv = WaitForAsynchronousFrames();
  if (NS_FAILED(rv)) {
    LOG("AppleVTDecoder::Flush failed waiting for platform decoder "
        "with error:%d.", rv);
  }
  ClearReorderedFrames();
  MonitorAutoLock mon(mMonitor);
  mIsFlushing = false;
  mon.NotifyAll();
}

void
AppleVTDecoder::ProcessDrain()
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  nsresult rv = WaitForAsynchronousFrames();
  if (NS_FAILED(rv)) {
    LOG("AppleVTDecoder::Drain failed waiting for platform decoder "
        "with error:%d.", rv);
  }
  DrainReorderedFrames();
  mCallback->DrainComplete();
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
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArgs<CFRefPtr<CVPixelBufferRef>, AppleVTDecoder::AppleFrameRef>(
      decoder, &AppleVTDecoder::OutputFrame, image, *frameRef);
  decoder->DispatchOutputTask(task.forget());
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

nsresult
AppleVTDecoder::SubmitFrame(MediaRawData* aSample)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  mInputIncoming--;
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
    return NS_ERROR_FAILURE;
  }
  CMSampleTimingInfo timestamp = TimingInfoFromSample(aSample);
  rv = CMSampleBufferCreate(kCFAllocatorDefault, block, true, 0, 0, mFormat, 1, 1, &timestamp, 0, NULL, sample.receive());
  if (rv != noErr) {
    NS_ERROR("Couldn't create CMSampleBuffer");
    return NS_ERROR_FAILURE;
  }

  mQueuedSamples++;

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
    mCallback->Error();
    return NS_ERROR_FAILURE;
  }

  // Ask for more data.
  if (!mInputIncoming && mQueuedSamples <= mMaxRefFrames) {
    LOG("AppleVTDecoder task queue empty; requesting more data");
    mCallback->InputExhausted();
  }

  return NS_OK;
}

nsresult
AppleVTDecoder::InitializeSession()
{
  OSStatus rv;

#ifdef LOG_MEDIA_SHA1
  SHA1Sum avc_hash;
  avc_hash.update(mExtraData->Elements(),mExtraData->Length());
  uint8_t digest_buf[SHA1Sum::kHashSize];
  avc_hash.finish(digest_buf);
  nsAutoCString avc_digest;
  for (size_t i = 0; i < sizeof(digest_buf); i++) {
    avc_digest.AppendPrintf("%02x", digest_buf[i]);
  }
  LOG("AVCDecoderConfig %ld bytes sha1 %s",
      mExtraData->Length(), avc_digest.get());
#endif // LOG_MEDIA_SHA1

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
  if (gfxPlatform::GetPlatform()->CanUseHardwareVideoDecoding()) {
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

} // namespace mozilla

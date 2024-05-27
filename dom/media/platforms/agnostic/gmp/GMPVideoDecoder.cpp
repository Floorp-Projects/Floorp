/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoDecoder.h"
#include "GMPDecoderModule.h"
#include "GMPVideoHost.h"
#include "GMPLog.h"
#include "GMPUtils.h"
#include "MediaData.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/StaticPrefs_media.h"
#include "nsServiceManagerUtils.h"
#include "AnnexB.h"
#include "H264.h"
#include "MP4Decoder.h"
#include "prsystem.h"
#include "VPXDecoder.h"
#include "VideoUtils.h"

namespace mozilla {

GMPVideoDecoderParams::GMPVideoDecoderParams(const CreateDecoderParams& aParams)
    : mConfig(aParams.VideoConfig()),
      mImageContainer(aParams.mImageContainer),
      mCrashHelper(aParams.mCrashHelper),
      mKnowsCompositor(aParams.mKnowsCompositor),
      mTrackingId(aParams.mTrackingId) {}

nsCString GMPVideoDecoder::GetCodecName() const {
  if (MP4Decoder::IsH264(mConfig.mMimeType)) {
    return "h264"_ns;
  } else if (VPXDecoder::IsVP8(mConfig.mMimeType)) {
    return "vp8"_ns;
  } else if (VPXDecoder::IsVP9(mConfig.mMimeType)) {
    return "vp9"_ns;
  }
  return "unknown"_ns;
}

void GMPVideoDecoder::Decoded(GMPVideoi420Frame* aDecodedFrame) {
  GMP_LOG_DEBUG("GMPVideoDecoder::Decoded");

  GMPUniquePtr<GMPVideoi420Frame> decodedFrame(aDecodedFrame);
  MOZ_ASSERT(IsOnGMPThread());

  VideoData::YCbCrBuffer b;
  for (int i = 0; i < kGMPNumOfPlanes; ++i) {
    b.mPlanes[i].mData = decodedFrame->Buffer(GMPPlaneType(i));
    b.mPlanes[i].mStride = decodedFrame->Stride(GMPPlaneType(i));
    if (i == kGMPYPlane) {
      b.mPlanes[i].mWidth = decodedFrame->Width();
      b.mPlanes[i].mHeight = decodedFrame->Height();
    } else {
      b.mPlanes[i].mWidth = (decodedFrame->Width() + 1) / 2;
      b.mPlanes[i].mHeight = (decodedFrame->Height() + 1) / 2;
    }
    b.mPlanes[i].mSkip = 0;
  }

  b.mChromaSubsampling = gfx::ChromaSubsampling::HALF_WIDTH_AND_HEIGHT;
  b.mYUVColorSpace =
      DefaultColorSpace({decodedFrame->Width(), decodedFrame->Height()});

  UniquePtr<SampleMetadata> sampleData;
  if (auto entryHandle = mSamples.Lookup(decodedFrame->Timestamp())) {
    sampleData = std::move(entryHandle.Data());
    entryHandle.Remove();
  } else {
    GMP_LOG_DEBUG(
        "GMPVideoDecoder::Decoded(this=%p) missing sample metadata for "
        "time %" PRIu64,
        this, decodedFrame->Timestamp());
    if (mSamples.IsEmpty()) {
      // If we have no remaining samples in the table, then we have processed
      // all outstanding decode requests.
      ProcessReorderQueue(mDecodePromise, __func__);
    }
    return;
  }

  MOZ_ASSERT(sampleData);

  gfx::IntRect pictureRegion(0, 0, decodedFrame->Width(),
                             decodedFrame->Height());
  Result<already_AddRefed<VideoData>, MediaResult> r =
      VideoData::CreateAndCopyData(
          mConfig, mImageContainer, sampleData->mOffset,
          media::TimeUnit::FromMicroseconds(decodedFrame->UpdatedTimestamp()),
          media::TimeUnit::FromMicroseconds(decodedFrame->Duration()), b,
          sampleData->mKeyframe, media::TimeUnit::FromMicroseconds(-1),
          pictureRegion, mKnowsCompositor);
  if (r.isErr()) {
    mReorderQueue.Clear();
    mUnorderedData.Clear();
    mSamples.Clear();
    mDecodePromise.RejectIfExists(r.unwrapErr(), __func__);
    return;
  }

  RefPtr<VideoData> v = r.unwrap();
  MOZ_ASSERT(v);
  mPerformanceRecorder.Record(
      static_cast<int64_t>(decodedFrame->Timestamp()),
      [&](DecodeStage& aStage) {
        aStage.SetImageFormat(DecodeStage::YUV420P);
        aStage.SetResolution(decodedFrame->Width(), decodedFrame->Height());
        aStage.SetYUVColorSpace(b.mYUVColorSpace);
        aStage.SetColorDepth(b.mColorDepth);
        aStage.SetColorRange(b.mColorRange);
        aStage.SetStartTimeAndEndTime(v->mTime.ToMicroseconds(),
                                      v->GetEndTime().ToMicroseconds());
      });

  if (mReorderFrames) {
    mReorderQueue.Push(std::move(v));
  } else {
    mUnorderedData.AppendElement(std::move(v));
  }

  if (mSamples.IsEmpty()) {
    // If we have no remaining samples in the table, then we have processed
    // all outstanding decode requests.
    ProcessReorderQueue(mDecodePromise, __func__);
  }
}

void GMPVideoDecoder::ReceivedDecodedReferenceFrame(const uint64_t aPictureId) {
  GMP_LOG_DEBUG("GMPVideoDecoder::ReceivedDecodedReferenceFrame");
  MOZ_ASSERT(IsOnGMPThread());
}

void GMPVideoDecoder::ReceivedDecodedFrame(const uint64_t aPictureId) {
  GMP_LOG_DEBUG("GMPVideoDecoder::ReceivedDecodedFrame");
  MOZ_ASSERT(IsOnGMPThread());
}

void GMPVideoDecoder::InputDataExhausted() {
  GMP_LOG_DEBUG("GMPVideoDecoder::InputDataExhausted");
  MOZ_ASSERT(IsOnGMPThread());
  mSamples.Clear();
  ProcessReorderQueue(mDecodePromise, __func__);
}

void GMPVideoDecoder::DrainComplete() {
  GMP_LOG_DEBUG("GMPVideoDecoder::DrainComplete");
  MOZ_ASSERT(IsOnGMPThread());
  mSamples.Clear();

  if (mDrainPromise.IsEmpty()) {
    return;
  }

  DecodedData results;
  if (mReorderFrames) {
    results.SetCapacity(mReorderQueue.Length());
    while (!mReorderQueue.IsEmpty()) {
      results.AppendElement(mReorderQueue.Pop());
    }
  } else {
    results = std::move(mUnorderedData);
  }

  mDrainPromise.Resolve(std::move(results), __func__);
}

void GMPVideoDecoder::ResetComplete() {
  GMP_LOG_DEBUG("GMPVideoDecoder::ResetComplete");
  MOZ_ASSERT(IsOnGMPThread());
  mPerformanceRecorder.Record(std::numeric_limits<int64_t>::max());
  mFlushPromise.ResolveIfExists(true, __func__);
}

void GMPVideoDecoder::Error(GMPErr aErr) {
  GMP_LOG_DEBUG("GMPVideoDecoder::Error");
  MOZ_ASSERT(IsOnGMPThread());
  Teardown(ToMediaResult(aErr, "Error GMP callback"_ns), __func__);
}

void GMPVideoDecoder::Terminated() {
  GMP_LOG_DEBUG("GMPVideoDecoder::Terminated");
  MOZ_ASSERT(IsOnGMPThread());
  Teardown(
      MediaResult(NS_ERROR_DOM_MEDIA_ABORT_ERR, "Terminated GMP callback"_ns),
      __func__);
}

void GMPVideoDecoder::ProcessReorderQueue(
    MozPromiseHolder<DecodePromise>& aPromise, StaticString aMethodName) {
  if (aPromise.IsEmpty()) {
    return;
  }

  if (!mReorderFrames) {
    aPromise.Resolve(std::move(mUnorderedData), aMethodName);
    return;
  }

  DecodedData results;
  size_t availableFrames = mReorderQueue.Length();
  if (availableFrames > mMaxRefFrames) {
    size_t resolvedFrames = availableFrames - mMaxRefFrames;
    results.SetCapacity(resolvedFrames);
    do {
      results.AppendElement(mReorderQueue.Pop());
    } while (--resolvedFrames > 0);
  }

  aPromise.Resolve(std::move(results), aMethodName);
}

GMPVideoDecoder::GMPVideoDecoder(const GMPVideoDecoderParams& aParams)
    : mConfig(aParams.mConfig),
      mGMP(nullptr),
      mHost(nullptr),
      mConvertNALUnitLengths(false),
      mCrashHelper(aParams.mCrashHelper),
      mImageContainer(aParams.mImageContainer),
      mKnowsCompositor(aParams.mKnowsCompositor),
      mTrackingId(aParams.mTrackingId),
      mCanDecodeBatch(StaticPrefs::media_gmp_decoder_decode_batch()),
      mReorderFrames(StaticPrefs::media_gmp_decoder_reorder_frames()) {}

void GMPVideoDecoder::InitTags(nsTArray<nsCString>& aTags) {
  if (MP4Decoder::IsH264(mConfig.mMimeType)) {
    aTags.AppendElement("h264"_ns);
  } else if (VPXDecoder::IsVP8(mConfig.mMimeType)) {
    aTags.AppendElement("vp8"_ns);
  } else if (VPXDecoder::IsVP9(mConfig.mMimeType)) {
    aTags.AppendElement("vp9"_ns);
  }
}

nsCString GMPVideoDecoder::GetNodeId() { return SHARED_GMP_DECODING_NODE_ID; }

GMPUniquePtr<GMPVideoEncodedFrame> GMPVideoDecoder::CreateFrame(
    MediaRawData* aSample) {
  GMPVideoFrame* ftmp = nullptr;
  GMPErr err = mHost->CreateFrame(kGMPEncodedVideoFrame, &ftmp);
  if (GMP_FAILED(err)) {
    return nullptr;
  }

  GMPUniquePtr<GMPVideoEncodedFrame> frame(
      static_cast<GMPVideoEncodedFrame*>(ftmp));
  err = frame->CreateEmptyFrame(aSample->Size());
  if (GMP_FAILED(err)) {
    return nullptr;
  }

  memcpy(frame->Buffer(), aSample->Data(), frame->Size());

  // Convert 4-byte NAL unit lengths to host-endian 4-byte buffer lengths to
  // suit the GMP API.
  if (mConvertNALUnitLengths) {
    const int kNALLengthSize = 4;
    uint8_t* buf = frame->Buffer();
    while (buf < frame->Buffer() + frame->Size() - kNALLengthSize) {
      uint32_t length = BigEndian::readUint32(buf) + kNALLengthSize;
      *reinterpret_cast<uint32_t*>(buf) = length;
      buf += length;
    }
  }

  frame->SetBufferType(GMP_BufferLength32);

  frame->SetEncodedWidth(mConfig.mDisplay.width);
  frame->SetEncodedHeight(mConfig.mDisplay.height);
  frame->SetTimeStamp(aSample->mTime.ToMicroseconds());
  frame->SetCompleteFrame(true);
  frame->SetDuration(aSample->mDuration.ToMicroseconds());
  frame->SetFrameType(aSample->mKeyframe ? kGMPKeyFrame : kGMPDeltaFrame);

  return frame;
}

const VideoInfo& GMPVideoDecoder::GetConfig() const { return mConfig; }

void GMPVideoDecoder::GMPInitDone(GMPVideoDecoderProxy* aGMP,
                                  GMPVideoHost* aHost) {
  MOZ_ASSERT(IsOnGMPThread());

  if (!aGMP) {
    mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
    return;
  }
  MOZ_ASSERT(aHost);

  if (mInitPromise.IsEmpty()) {
    // GMP must have been shutdown while we were waiting for Init operation
    // to complete.
    aGMP->Close();
    return;
  }

  bool isOpenH264 = aGMP->GetPluginType() == GMPPluginType::OpenH264;

  GMPVideoCodec codec;
  memset(&codec, 0, sizeof(codec));

  codec.mGMPApiVersion = kGMPVersion34;
  nsTArray<uint8_t> codecSpecific;
  if (MP4Decoder::IsH264(mConfig.mMimeType)) {
    codec.mCodecType = kGMPVideoCodecH264;
    codecSpecific.AppendElement(0);  // mPacketizationMode.
    codecSpecific.AppendElements(mConfig.mExtraData->Elements(),
                                 mConfig.mExtraData->Length());
    // OpenH264 expects pseudo-AVCC, but others must be passed
    // AnnexB for H264.
    mConvertToAnnexB = !isOpenH264;
    mMaxRefFrames = H264::ComputeMaxRefFrames(mConfig.mExtraData);
  } else if (VPXDecoder::IsVP8(mConfig.mMimeType)) {
    codec.mCodecType = kGMPVideoCodecVP8;
  } else if (VPXDecoder::IsVP9(mConfig.mMimeType)) {
    codec.mCodecType = kGMPVideoCodecVP9;
  } else {
    // Unrecognized mime type
    aGMP->Close();
    mInitPromise.Reject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
    return;
  }
  codec.mWidth = mConfig.mImage.width;
  codec.mHeight = mConfig.mImage.height;
  codec.mUseThreadedDecode = StaticPrefs::media_gmp_decoder_multithreaded();
  codec.mLogLevel = GetGMPLibraryLogLevel();

  nsresult rv =
      aGMP->InitDecode(codec, codecSpecific, this, PR_GetNumberOfProcessors());
  if (NS_FAILED(rv)) {
    aGMP->Close();
    mInitPromise.Reject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
    return;
  }

  mGMP = aGMP;
  mHost = aHost;

  // GMP implementations have interpreted the meaning of GMP_BufferLength32
  // differently.  The OpenH264 GMP expects GMP_BufferLength32 to behave as
  // specified in the GMP API, where each buffer is prefixed by a 32-bit
  // host-endian buffer length that includes the size of the buffer length
  // field.  Other existing GMPs currently expect GMP_BufferLength32 (when
  // combined with kGMPVideoCodecH264) to mean "like AVCC but restricted to
  // 4-byte NAL lengths" (i.e. buffer lengths are specified in big-endian
  // and do not include the length of the buffer length field.
  mConvertNALUnitLengths = isOpenH264;

  mInitPromise.Resolve(TrackInfo::kVideoTrack, __func__);
}

RefPtr<MediaDataDecoder::InitPromise> GMPVideoDecoder::Init() {
  MOZ_ASSERT(IsOnGMPThread());

  mMPS = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  MOZ_ASSERT(mMPS);

  RefPtr<InitPromise> promise(mInitPromise.Ensure(__func__));

  nsTArray<nsCString> tags;
  InitTags(tags);
  UniquePtr<GetGMPVideoDecoderCallback> callback(new GMPInitDoneCallback(this));
  if (NS_FAILED(mMPS->GetGMPVideoDecoder(mCrashHelper, &tags, GetNodeId(),
                                         std::move(callback)))) {
    mInitPromise.Reject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }

  return promise;
}

RefPtr<MediaDataDecoder::DecodePromise> GMPVideoDecoder::Decode(
    MediaRawData* aSample) {
  MOZ_ASSERT(IsOnGMPThread());

  RefPtr<MediaRawData> sample(aSample);
  if (!mGMP) {
    return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("mGMP not initialized")),
        __func__);
  }

  if (mTrackingId) {
    MediaInfoFlag flag = MediaInfoFlag::None;
    flag |= (aSample->mKeyframe ? MediaInfoFlag::KeyFrame
                                : MediaInfoFlag::NonKeyFrame);
    if (mGMP->GetPluginType() == GMPPluginType::OpenH264) {
      flag |= MediaInfoFlag::SoftwareDecoding;
    }
    if (MP4Decoder::IsH264(mConfig.mMimeType)) {
      flag |= MediaInfoFlag::VIDEO_H264;
    } else if (VPXDecoder::IsVP8(mConfig.mMimeType)) {
      flag |= MediaInfoFlag::VIDEO_VP8;
    } else if (VPXDecoder::IsVP9(mConfig.mMimeType)) {
      flag |= MediaInfoFlag::VIDEO_VP9;
    }
    mPerformanceRecorder.Start(aSample->mTime.ToMicroseconds(),
                               "GMPVideoDecoder"_ns, *mTrackingId, flag);
  }

  GMPUniquePtr<GMPVideoEncodedFrame> frame = CreateFrame(sample);
  if (!frame) {
    return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY,
                    RESULT_DETAIL("CreateFrame returned null")),
        __func__);
  }

  uint64_t frameTimestamp = frame->TimeStamp();
  RefPtr<DecodePromise> p = mDecodePromise.Ensure(__func__);
  nsTArray<uint8_t> info;  // No codec specific per-frame info to pass.
  nsresult rv = mGMP->Decode(std::move(frame), false, info, 0);
  if (NS_FAILED(rv)) {
    mDecodePromise.Reject(MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                                      RESULT_DETAIL("mGMP->Decode:%" PRIx32,
                                                    static_cast<uint32_t>(rv))),
                          __func__);
  }

  // If we have multiple outstanding frames, we need to track which offset
  // belongs to which frame. During seek, it is possible to get the same frame
  // requested twice, if the old frame is still outstanding. We will simply drop
  // the extra decoded frame and request more input if the last outstanding.
  mSamples.WithEntryHandle(frameTimestamp, [&](auto entryHandle) {
    auto sampleData = MakeUnique<SampleMetadata>(sample);
    entryHandle.InsertOrUpdate(std::move(sampleData));
  });

  return p;
}

RefPtr<MediaDataDecoder::FlushPromise> GMPVideoDecoder::Flush() {
  MOZ_ASSERT(IsOnGMPThread());

  mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  mDrainPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);

  RefPtr<FlushPromise> p = mFlushPromise.Ensure(__func__);
  if (!mGMP || NS_FAILED(mGMP->Reset())) {
    // Abort the flush.
    mPerformanceRecorder.Record(std::numeric_limits<int64_t>::max());
    mFlushPromise.Resolve(true, __func__);
  }
  return p;
}

RefPtr<MediaDataDecoder::DecodePromise> GMPVideoDecoder::Drain() {
  MOZ_ASSERT(IsOnGMPThread());

  MOZ_ASSERT(mDecodePromise.IsEmpty(), "Must wait for decoding to complete");

  RefPtr<DecodePromise> p = mDrainPromise.Ensure(__func__);
  if (!mGMP || NS_FAILED(mGMP->Drain())) {
    mDrainPromise.Resolve(DecodedData(), __func__);
  }

  return p;
}

void GMPVideoDecoder::Teardown(const MediaResult& aResult,
                               StaticString aCallSite) {
  MOZ_ASSERT(IsOnGMPThread());

  // Ensure we are kept alive at least until we return.
  RefPtr<GMPVideoDecoder> self(this);

  mInitPromise.RejectIfExists(aResult, aCallSite);
  mDecodePromise.RejectIfExists(aResult, aCallSite);

  if (mGMP) {
    // Note this unblocks flush and drain operations waiting for callbacks.
    mGMP->Close();
    mGMP = nullptr;
  } else {
    mFlushPromise.RejectIfExists(aResult, aCallSite);
    mDrainPromise.RejectIfExists(aResult, aCallSite);
  }

  mHost = nullptr;
}

RefPtr<ShutdownPromise> GMPVideoDecoder::Shutdown() {
  MOZ_ASSERT(IsOnGMPThread());
  Teardown(MediaResult(NS_ERROR_DOM_MEDIA_CANCELED, "Shutdown"_ns), __func__);
  return ShutdownPromise::CreateAndResolve(true, __func__);
}

}  // namespace mozilla

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoDecoder.h"
#include "GMPDecoderModule.h"
#include "GMPVideoHost.h"
#include "MediaData.h"
#include "mozilla/EndianUtils.h"
#include "AnnexB.h"
#include "MP4Decoder.h"
#include "prsystem.h"
#include "VPXDecoder.h"

namespace mozilla {

#if defined(DEBUG)
static bool IsOnGMPThread()
{
  nsCOMPtr<mozIGeckoMediaPluginService> mps =
    do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  MOZ_ASSERT(mps);

  nsCOMPtr<nsIThread> gmpThread;
  nsresult rv = mps->GetThread(getter_AddRefs(gmpThread));
  MOZ_ASSERT(NS_SUCCEEDED(rv) && gmpThread);
  return gmpThread->EventTarget()->IsOnCurrentThread();
}
#endif

GMPVideoDecoderParams::GMPVideoDecoderParams(const CreateDecoderParams& aParams)
  : mConfig(aParams.VideoConfig())
  , mTaskQueue(aParams.mTaskQueue)
  , mImageContainer(aParams.mImageContainer)
  , mLayersBackend(aParams.GetLayersBackend())
  , mCrashHelper(aParams.mCrashHelper)
{
}

void
GMPVideoDecoder::Decoded(GMPVideoi420Frame* aDecodedFrame)
{
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
    b.mPlanes[i].mOffset = 0;
    b.mPlanes[i].mSkip = 0;
  }

  gfx::IntRect pictureRegion(
    0, 0, decodedFrame->Width(), decodedFrame->Height());
  RefPtr<VideoData> v = VideoData::CreateAndCopyData(
    mConfig,
    mImageContainer,
    mLastStreamOffset,
    media::TimeUnit::FromMicroseconds(decodedFrame->Timestamp()),
    media::TimeUnit::FromMicroseconds(decodedFrame->Duration()),
    b,
    false,
    media::TimeUnit::FromMicroseconds(-1),
    pictureRegion);
  RefPtr<GMPVideoDecoder> self = this;
  if (v) {
    mDecodedData.AppendElement(std::move(v));
  } else {
    mDecodedData.Clear();
    mDecodePromise.RejectIfExists(
      MediaResult(NS_ERROR_OUT_OF_MEMORY,
                  RESULT_DETAIL("CallBack::CreateAndCopyData")),
      __func__);
  }
}

void
GMPVideoDecoder::ReceivedDecodedReferenceFrame(const uint64_t aPictureId)
{
  MOZ_ASSERT(IsOnGMPThread());
}

void
GMPVideoDecoder::ReceivedDecodedFrame(const uint64_t aPictureId)
{
  MOZ_ASSERT(IsOnGMPThread());
}

void
GMPVideoDecoder::InputDataExhausted()
{
  MOZ_ASSERT(IsOnGMPThread());
  mDecodePromise.ResolveIfExists(mDecodedData, __func__);
  mDecodedData.Clear();
}

void
GMPVideoDecoder::DrainComplete()
{
  MOZ_ASSERT(IsOnGMPThread());
  mDrainPromise.ResolveIfExists(mDecodedData, __func__);
  mDecodedData.Clear();
}

void
GMPVideoDecoder::ResetComplete()
{
  MOZ_ASSERT(IsOnGMPThread());
  mFlushPromise.ResolveIfExists(true, __func__);
}

void
GMPVideoDecoder::Error(GMPErr aErr)
{
  MOZ_ASSERT(IsOnGMPThread());
  auto error = MediaResult(aErr == GMPDecodeErr ? NS_ERROR_DOM_MEDIA_DECODE_ERR
                                                : NS_ERROR_DOM_MEDIA_FATAL_ERR,
                           RESULT_DETAIL("GMPErr:%x", aErr));
  mDecodePromise.RejectIfExists(error, __func__);
  mDrainPromise.RejectIfExists(error, __func__);
  mFlushPromise.RejectIfExists(error, __func__);
}

void
GMPVideoDecoder::Terminated()
{
  MOZ_ASSERT(IsOnGMPThread());
  Error(GMPErr::GMPAbortedErr);
}

GMPVideoDecoder::GMPVideoDecoder(const GMPVideoDecoderParams& aParams)
  : mConfig(aParams.mConfig)
  , mGMP(nullptr)
  , mHost(nullptr)
  , mConvertNALUnitLengths(false)
  , mCrashHelper(aParams.mCrashHelper)
  , mImageContainer(aParams.mImageContainer)
{
}

void
GMPVideoDecoder::InitTags(nsTArray<nsCString>& aTags)
{
  if (MP4Decoder::IsH264(mConfig.mMimeType)) {
    aTags.AppendElement(NS_LITERAL_CSTRING("h264"));
  } else if (VPXDecoder::IsVP8(mConfig.mMimeType)) {
    aTags.AppendElement(NS_LITERAL_CSTRING("vp8"));
  } else if (VPXDecoder::IsVP9(mConfig.mMimeType)) {
    aTags.AppendElement(NS_LITERAL_CSTRING("vp9"));
  }
}

nsCString
GMPVideoDecoder::GetNodeId()
{
  return SHARED_GMP_DECODING_NODE_ID;
}

GMPUniquePtr<GMPVideoEncodedFrame>
GMPVideoDecoder::CreateFrame(MediaRawData* aSample)
{
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
      *reinterpret_cast<uint32_t *>(buf) = length;
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

const VideoInfo&
GMPVideoDecoder::GetConfig() const
{
  return mConfig;
}

void
GMPVideoDecoder::GMPInitDone(GMPVideoDecoderProxy* aGMP, GMPVideoHost* aHost)
{
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

  bool isOpenH264 = aGMP->GetDisplayName().EqualsLiteral("gmpopenh264");

  GMPVideoCodec codec;
  memset(&codec, 0, sizeof(codec));

  codec.mGMPApiVersion = kGMPVersion33;
  nsTArray<uint8_t> codecSpecific;
  if (MP4Decoder::IsH264(mConfig.mMimeType)) {
    codec.mCodecType = kGMPVideoCodecH264;
    codecSpecific.AppendElement(0); // mPacketizationMode.
    codecSpecific.AppendElements(mConfig.mExtraData->Elements(),
                                 mConfig.mExtraData->Length());
    // OpenH264 expects pseudo-AVCC, but others must be passed
    // AnnexB for H264.
    mConvertToAnnexB = !isOpenH264;
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

  nsresult rv = aGMP->InitDecode(codec,
                                 codecSpecific,
                                 this,
                                 PR_GetNumberOfProcessors());
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

RefPtr<MediaDataDecoder::InitPromise>
GMPVideoDecoder::Init()
{
  MOZ_ASSERT(IsOnGMPThread());

  mMPS = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  MOZ_ASSERT(mMPS);

  RefPtr<InitPromise> promise(mInitPromise.Ensure(__func__));

  nsTArray<nsCString> tags;
  InitTags(tags);
  UniquePtr<GetGMPVideoDecoderCallback> callback(new GMPInitDoneCallback(this));
  if (NS_FAILED(mMPS->GetDecryptingGMPVideoDecoder(mCrashHelper,
                                                   &tags,
                                                   GetNodeId(),
                                                   std::move(callback),
                                                   DecryptorId()))) {
    mInitPromise.Reject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }

  return promise;
}

RefPtr<MediaDataDecoder::DecodePromise>
GMPVideoDecoder::Decode(MediaRawData* aSample)
{
  MOZ_ASSERT(IsOnGMPThread());

  RefPtr<MediaRawData> sample(aSample);
  if (!mGMP) {
    return DecodePromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  RESULT_DETAIL("mGMP not initialized")),
      __func__);
  }

  mLastStreamOffset = sample->mOffset;

  GMPUniquePtr<GMPVideoEncodedFrame> frame = CreateFrame(sample);
  if (!frame) {
    return DecodePromise::CreateAndReject(
      MediaResult(NS_ERROR_OUT_OF_MEMORY,
                  RESULT_DETAIL("CreateFrame returned null")),
      __func__);
  }
  RefPtr<DecodePromise> p = mDecodePromise.Ensure(__func__);
  nsTArray<uint8_t> info; // No codec specific per-frame info to pass.
  nsresult rv = mGMP->Decode(std::move(frame), false, info, 0);
  if (NS_FAILED(rv)) {
    mDecodePromise.Reject(MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                                      RESULT_DETAIL("mGMP->Decode:%" PRIx32,
                                                    static_cast<uint32_t>(rv))),
                          __func__);
  }
  return p;
}

RefPtr<MediaDataDecoder::FlushPromise>
GMPVideoDecoder::Flush()
{
  MOZ_ASSERT(IsOnGMPThread());

  mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  mDrainPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);

  RefPtr<FlushPromise> p = mFlushPromise.Ensure(__func__);
  if (!mGMP || NS_FAILED(mGMP->Reset())) {
    // Abort the flush.
    mFlushPromise.Resolve(true, __func__);
  }
  return p;
}

RefPtr<MediaDataDecoder::DecodePromise>
GMPVideoDecoder::Drain()
{
  MOZ_ASSERT(IsOnGMPThread());

  MOZ_ASSERT(mDecodePromise.IsEmpty(), "Must wait for decoding to complete");

  RefPtr<DecodePromise> p = mDrainPromise.Ensure(__func__);
  if (!mGMP || NS_FAILED(mGMP->Drain())) {
    mDrainPromise.Resolve(DecodedData(), __func__);
  }

  return p;
}

RefPtr<ShutdownPromise>
GMPVideoDecoder::Shutdown()
{
  mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  mFlushPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);

  // Note that this *may* be called from the proxy thread also.
  // TODO: If that's the case, then this code is racy.
  if (!mGMP) {
    return ShutdownPromise::CreateAndResolve(true, __func__);
  }
  // Note this unblocks flush and drain operations waiting for callbacks.
  mGMP->Close();
  mGMP = nullptr;
  return ShutdownPromise::CreateAndResolve(true, __func__);
}

} // namespace mozilla

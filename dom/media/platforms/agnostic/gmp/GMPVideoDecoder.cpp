/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoDecoder.h"
#include "GMPVideoHost.h"
#include "mozilla/EndianUtils.h"
#include "prsystem.h"
#include "MediaData.h"
#include "GMPDecoderModule.h"

namespace mozilla {

#if defined(DEBUG)
extern bool IsOnGMPThread();
#endif

void
VideoCallbackAdapter::Decoded(GMPVideoi420Frame* aDecodedFrame)
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

  gfx::IntRect pictureRegion(0, 0, decodedFrame->Width(), decodedFrame->Height());
  RefPtr<VideoData> v = VideoData::Create(mVideoInfo,
                                            mImageContainer,
                                            mLastStreamOffset,
                                            decodedFrame->Timestamp(),
                                            decodedFrame->Duration(),
                                            b,
                                            false,
                                            -1,
                                            pictureRegion);
  if (v) {
    mCallback->Output(v);
  } else {
    mCallback->Error();
  }
}

void
VideoCallbackAdapter::ReceivedDecodedReferenceFrame(const uint64_t aPictureId)
{
  MOZ_ASSERT(IsOnGMPThread());
}

void
VideoCallbackAdapter::ReceivedDecodedFrame(const uint64_t aPictureId)
{
  MOZ_ASSERT(IsOnGMPThread());
}

void
VideoCallbackAdapter::InputDataExhausted()
{
  MOZ_ASSERT(IsOnGMPThread());
  mCallback->InputExhausted();
}

void
VideoCallbackAdapter::DrainComplete()
{
  MOZ_ASSERT(IsOnGMPThread());
  mCallback->DrainComplete();
}

void
VideoCallbackAdapter::ResetComplete()
{
  MOZ_ASSERT(IsOnGMPThread());
  mCallback->FlushComplete();
}

void
VideoCallbackAdapter::Error(GMPErr aErr)
{
  MOZ_ASSERT(IsOnGMPThread());
  mCallback->Error();
}

void
VideoCallbackAdapter::Terminated()
{
  // Note that this *may* be called from the proxy thread also.
  NS_WARNING("H.264 GMP decoder terminated.");
  mCallback->Error();
}

void
GMPVideoDecoder::InitTags(nsTArray<nsCString>& aTags)
{
  aTags.AppendElement(NS_LITERAL_CSTRING("h264"));
  const Maybe<nsCString> gmp(
    GMPDecoderModule::PreferredGMP(NS_LITERAL_CSTRING("video/avc")));
  if (gmp.isSome()) {
    aTags.AppendElement(gmp.value());
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
    mCallback->Error();
    return nullptr;
  }

  GMPUniquePtr<GMPVideoEncodedFrame> frame(static_cast<GMPVideoEncodedFrame*>(ftmp));
  err = frame->CreateEmptyFrame(aSample->Size());
  if (GMP_FAILED(err)) {
    mCallback->Error();
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
  frame->SetTimeStamp(aSample->mTime);
  frame->SetCompleteFrame(true);
  frame->SetDuration(aSample->mDuration);
  frame->SetFrameType(aSample->mKeyframe ? kGMPKeyFrame : kGMPDeltaFrame);

  return frame;
}

void
GMPVideoDecoder::GMPInitDone(GMPVideoDecoderProxy* aGMP, GMPVideoHost* aHost)
{
  MOZ_ASSERT(IsOnGMPThread());

  if (!aGMP) {
    mInitPromise.RejectIfExists(MediaDataDecoder::DecoderFailureReason::INIT_ERROR, __func__);
    return;
  }
  MOZ_ASSERT(aHost);

  if (mInitPromise.IsEmpty()) {
    // GMP must have been shutdown while we were waiting for Init operation
    // to complete.
    aGMP->Close();
    return;
  }

  GMPVideoCodec codec;
  memset(&codec, 0, sizeof(codec));

  codec.mGMPApiVersion = kGMPVersion33;

  codec.mCodecType = kGMPVideoCodecH264;
  codec.mWidth = mConfig.mImage.width;
  codec.mHeight = mConfig.mImage.height;

  nsTArray<uint8_t> codecSpecific;
  codecSpecific.AppendElement(0); // mPacketizationMode.
  codecSpecific.AppendElements(mConfig.mExtraData->Elements(),
                               mConfig.mExtraData->Length());

  nsresult rv = aGMP->InitDecode(codec,
                                 codecSpecific,
                                 mAdapter,
                                 PR_GetNumberOfProcessors());
  if (NS_FAILED(rv)) {
    aGMP->Close();
    mInitPromise.Reject(MediaDataDecoder::DecoderFailureReason::INIT_ERROR, __func__);
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
  mConvertNALUnitLengths = mGMP->GetDisplayName().EqualsLiteral("gmpopenh264");

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
  if (NS_FAILED(mMPS->GetGMPVideoDecoder(&tags, GetNodeId(), Move(callback)))) {
    mInitPromise.Reject(MediaDataDecoder::DecoderFailureReason::INIT_ERROR, __func__);
  }

  return promise;
}

nsresult
GMPVideoDecoder::Input(MediaRawData* aSample)
{
  MOZ_ASSERT(IsOnGMPThread());

  RefPtr<MediaRawData> sample(aSample);
  if (!mGMP) {
    mCallback->Error();
    return NS_ERROR_FAILURE;
  }

  mAdapter->SetLastStreamOffset(sample->mOffset);

  GMPUniquePtr<GMPVideoEncodedFrame> frame = CreateFrame(sample);
  if (!frame) {
    mCallback->Error();
    return NS_ERROR_FAILURE;
  }
  nsTArray<uint8_t> info; // No codec specific per-frame info to pass.
  nsresult rv = mGMP->Decode(Move(frame), false, info, 0);
  if (NS_FAILED(rv)) {
    mCallback->Error();
    return rv;
  }

  return NS_OK;
}

nsresult
GMPVideoDecoder::Flush()
{
  MOZ_ASSERT(IsOnGMPThread());

  if (!mGMP || NS_FAILED(mGMP->Reset())) {
    // Abort the flush.
    mCallback->FlushComplete();
  }

  return NS_OK;
}

nsresult
GMPVideoDecoder::Drain()
{
  MOZ_ASSERT(IsOnGMPThread());

  if (!mGMP || NS_FAILED(mGMP->Drain())) {
    mCallback->DrainComplete();
  }

  return NS_OK;
}

nsresult
GMPVideoDecoder::Shutdown()
{
  mInitPromise.RejectIfExists(MediaDataDecoder::DecoderFailureReason::CANCELED, __func__);
  // Note that this *may* be called from the proxy thread also.
  if (!mGMP) {
    return NS_ERROR_FAILURE;
  }
  // Note this unblocks flush and drain operations waiting for callbacks.
  mGMP->Close();
  mGMP = nullptr;

  return NS_OK;
}

} // namespace mozilla

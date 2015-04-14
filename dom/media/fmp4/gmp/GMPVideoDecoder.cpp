/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoDecoder.h"
#include "GMPVideoHost.h"
#include "mozilla/Endian.h"
#include "prsystem.h"
#include "MediaData.h"

namespace mozilla {

#if defined(DEBUG)
extern bool IsOnGMPThread();
#endif

void
VideoCallbackAdapter::Decoded(GMPVideoi420Frame* aDecodedFrame)
{
  GMPUnique<GMPVideoi420Frame>::Ptr decodedFrame(aDecodedFrame);

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
  nsRefPtr<VideoData> v = VideoData::Create(mVideoInfo,
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
}

nsCString
GMPVideoDecoder::GetNodeId()
{
  return NS_LITERAL_CSTRING("");
}

GMPUnique<GMPVideoEncodedFrame>::Ptr
GMPVideoDecoder::CreateFrame(MediaRawData* aSample)
{
  GMPVideoFrame* ftmp = nullptr;
  GMPErr err = mHost->CreateFrame(kGMPEncodedVideoFrame, &ftmp);
  if (GMP_FAILED(err)) {
    mCallback->Error();
    return nullptr;
  }

  GMPUnique<GMPVideoEncodedFrame>::Ptr frame(static_cast<GMPVideoEncodedFrame*>(ftmp));
  err = frame->CreateEmptyFrame(aSample->mSize);
  if (GMP_FAILED(err)) {
    mCallback->Error();
    return nullptr;
  }

  memcpy(frame->Buffer(), aSample->mData, frame->Size());

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
GMPVideoDecoder::GetGMPAPI(GMPInitDoneRunnable* aInitDone)
{
  MOZ_ASSERT(IsOnGMPThread());

  nsTArray<nsCString> tags;
  InitTags(tags);
  UniquePtr<GetGMPVideoDecoderCallback> callback(
    new GMPInitDoneCallback(this, aInitDone));
  if (NS_FAILED(mMPS->GetGMPVideoDecoder(&tags, GetNodeId(), Move(callback)))) {
    aInitDone->Dispatch();
  }
}

void
GMPVideoDecoder::GMPInitDone(GMPVideoDecoderProxy* aGMP, GMPVideoHost* aHost)
{
  MOZ_ASSERT(aHost && aGMP);

  GMPVideoCodec codec;
  memset(&codec, 0, sizeof(codec));

  codec.mGMPApiVersion = kGMPVersion33;

  codec.mCodecType = kGMPVideoCodecH264;
  codec.mWidth = mConfig.mDisplay.width;
  codec.mHeight = mConfig.mDisplay.height;

  nsTArray<uint8_t> codecSpecific;
  codecSpecific.AppendElement(0); // mPacketizationMode.
  codecSpecific.AppendElements(mConfig.mExtraData->Elements(),
                               mConfig.mExtraData->Length());

  nsresult rv = aGMP->InitDecode(codec,
                                 codecSpecific,
                                 mAdapter,
                                 PR_GetNumberOfProcessors());
  if (NS_SUCCEEDED(rv)) {
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
  }
}

nsresult
GMPVideoDecoder::Init()
{
  MOZ_ASSERT(IsOnGMPThread());

  mMPS = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  MOZ_ASSERT(mMPS);

  nsCOMPtr<nsIThread> gmpThread = NS_GetCurrentThread();

  nsRefPtr<GMPInitDoneRunnable> initDone(new GMPInitDoneRunnable());
  gmpThread->Dispatch(
    NS_NewRunnableMethodWithArg<GMPInitDoneRunnable*>(this,
                                                      &GMPVideoDecoder::GetGMPAPI,
                                                      initDone),
    NS_DISPATCH_NORMAL);

  while (!initDone->IsDone()) {
    NS_ProcessNextEvent(gmpThread, true);
  }

  return mGMP ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
GMPVideoDecoder::Input(MediaRawData* aSample)
{
  MOZ_ASSERT(IsOnGMPThread());

  nsRefPtr<MediaRawData> sample(aSample);
  if (!mGMP) {
    mCallback->Error();
    return NS_ERROR_FAILURE;
  }

  mAdapter->SetLastStreamOffset(sample->mOffset);

  GMPUnique<GMPVideoEncodedFrame>::Ptr frame = CreateFrame(sample);
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
  // Note that this *may* be called from the proxy thread also.
  if (!mGMP) {
    return NS_ERROR_FAILURE;
  }
  mGMP->Close();
  mGMP = nullptr;

  return NS_OK;
}

} // namespace mozilla

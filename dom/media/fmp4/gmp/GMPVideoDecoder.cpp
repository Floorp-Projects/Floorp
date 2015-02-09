/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoDecoder.h"
#include "GMPVideoHost.h"
#include "prsystem.h"

namespace mozilla {

#if defined(DEBUG)
static bool IsOnGMPThread();
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
GMPVideoDecoder::CreateFrame(mp4_demuxer::MP4Sample* aSample)
{
  GMPVideoFrame* ftmp = nullptr;
  GMPErr err = mHost->CreateFrame(kGMPEncodedVideoFrame, &ftmp);
  if (GMP_FAILED(err)) {
    mCallback->Error();
    return nullptr;
  }

  GMPUnique<GMPVideoEncodedFrame>::Ptr frame(static_cast<GMPVideoEncodedFrame*>(ftmp));
  err = frame->CreateEmptyFrame(aSample->size);
  if (GMP_FAILED(err)) {
    mCallback->Error();
    return nullptr;
  }

  memcpy(frame->Buffer(), aSample->data, frame->Size());

  frame->SetEncodedWidth(mConfig.display_width);
  frame->SetEncodedHeight(mConfig.display_height);
  frame->SetTimeStamp(aSample->composition_timestamp);
  frame->SetCompleteFrame(true);
  frame->SetDuration(aSample->duration);
  frame->SetFrameType(aSample->is_sync_point ? kGMPKeyFrame : kGMPDeltaFrame);
  frame->SetBufferType(GMP_BufferLength32);

  return frame;
}

nsresult
GMPVideoDecoder::Init()
{
  MOZ_ASSERT(IsOnGMPThread());

  mMPS = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  MOZ_ASSERT(mMPS);

  nsTArray<nsCString> tags;
  InitTags(tags);
  nsresult rv = mMPS->GetGMPVideoDecoder(&tags, GetNodeId(), &mHost, &mGMP);
  NS_ENSURE_SUCCESS(rv, rv);
  MOZ_ASSERT(mHost && mGMP);

  GMPVideoCodec codec;
  memset(&codec, 0, sizeof(codec));

  codec.mGMPApiVersion = kGMPVersion33;

  codec.mCodecType = kGMPVideoCodecH264;
  codec.mWidth = mConfig.display_width;
  codec.mHeight = mConfig.display_height;

  nsTArray<uint8_t> codecSpecific;
  codecSpecific.AppendElement(0); // mPacketizationMode.
  codecSpecific.AppendElements(mConfig.extra_data->Elements(),
                               mConfig.extra_data->Length());

  rv = mGMP->InitDecode(codec,
                        codecSpecific,
                        mAdapter,
                        PR_GetNumberOfProcessors());
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
GMPVideoDecoder::Input(mp4_demuxer::MP4Sample* aSample)
{
  MOZ_ASSERT(IsOnGMPThread());

  nsAutoPtr<mp4_demuxer::MP4Sample> sample(aSample);
  if (!mGMP) {
    mCallback->Error();
    return NS_ERROR_FAILURE;
  }

  mAdapter->SetLastStreamOffset(sample->byte_offset);

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

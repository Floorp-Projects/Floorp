/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WidevineVideoDecoder.h"

#include "mp4_demuxer/AnnexB.h"
#include "WidevineUtils.h"
#include "WidevineVideoFrame.h"

using namespace cdm;

namespace mozilla {

WidevineVideoDecoder::WidevineVideoDecoder(GMPVideoHost* aVideoHost,
                                           RefPtr<CDMWrapper> aCDMWrapper)
  : mVideoHost(aVideoHost)
  , mCDMWrapper(Move(aCDMWrapper))
  , mExtraData(new MediaByteBuffer())
  , mSentInput(false)
{
  // Expect to start with a CDM wrapper, will release it in DecodingComplete().
  MOZ_ASSERT(mCDMWrapper);
  Log("WidevineVideoDecoder created this=%p", this);

  // Corresponding Release is in DecodingComplete().
  AddRef();
}

WidevineVideoDecoder::~WidevineVideoDecoder()
{
  Log("WidevineVideoDecoder destroyed this=%p", this);
}

static
VideoDecoderConfig::VideoCodecProfile
ToCDMH264Profile(uint8_t aProfile)
{
  switch (aProfile) {
    case 66: return VideoDecoderConfig::kH264ProfileBaseline;
    case 77: return VideoDecoderConfig::kH264ProfileMain;
    case 88: return VideoDecoderConfig::kH264ProfileExtended;
    case 100: return VideoDecoderConfig::kH264ProfileHigh;
    case 110: return VideoDecoderConfig::kH264ProfileHigh10;
    case 122: return VideoDecoderConfig::kH264ProfileHigh422;
    case 144: return VideoDecoderConfig::kH264ProfileHigh444Predictive;
  }
  return VideoDecoderConfig::kUnknownVideoCodecProfile;
}

void
WidevineVideoDecoder::InitDecode(const GMPVideoCodec& aCodecSettings,
                                 const uint8_t* aCodecSpecific,
                                 uint32_t aCodecSpecificLength,
                                 GMPVideoDecoderCallback* aCallback,
                                 int32_t aCoreCount)
{
  mCallback = aCallback;
  VideoDecoderConfig config;
  config.codec = VideoDecoderConfig::kCodecH264; // TODO: others.
  const GMPVideoCodecH264* h264 = (const GMPVideoCodecH264*)(aCodecSpecific);
  config.profile = ToCDMH264Profile(h264->mAVCC.mProfile);
  config.format = kYv12;
  config.coded_size = Size(aCodecSettings.mWidth, aCodecSettings.mHeight);
  mExtraData->AppendElements(aCodecSpecific + 1, aCodecSpecificLength);
  config.extra_data = mExtraData->Elements();
  config.extra_data_size = mExtraData->Length();
  Status rv = CDM()->InitializeVideoDecoder(config);
  if (rv != kSuccess) {
    mCallback->Error(ToGMPErr(rv));
    return;
  }
  Log("WidevineVideoDecoder::InitDecode() rv=%d", rv);
  mAnnexB = mp4_demuxer::AnnexB::ConvertExtraDataToAnnexB(mExtraData);
}

void
WidevineVideoDecoder::Decode(GMPVideoEncodedFrame* aInputFrame,
                             bool aMissingFrames,
                             const uint8_t* aCodecSpecificInfo,
                             uint32_t aCodecSpecificInfoLength,
                             int64_t aRenderTimeMs)
{
  // We may not get the same out of the CDM decoder as we put in, and there
  // may be some latency, i.e. we may need to input (say) 30 frames before
  // we receive output. So we need to store the durations of the frames input,
  // and retrieve them on output.
  mFrameDurations[aInputFrame->TimeStamp()] = aInputFrame->Duration();

  mSentInput = true;
  InputBuffer sample;

  RefPtr<MediaRawData> raw(new MediaRawData(aInputFrame->Buffer(), aInputFrame->Size()));
  raw->mExtraData = mExtraData;
  raw->mKeyframe = (aInputFrame->FrameType() == kGMPKeyFrame);
  // Convert input from AVCC, which GMPAPI passes in, to AnnexB, which
  // Chromium uses internally.
  mp4_demuxer::AnnexB::ConvertSampleToAnnexB(raw);

  const GMPEncryptedBufferMetadata* crypto = aInputFrame->GetDecryptionData();
  nsTArray<SubsampleEntry> subsamples;
  InitInputBuffer(crypto, aInputFrame->TimeStamp(), raw->Data(), raw->Size(), sample, subsamples);

  // For keyframes, ConvertSampleToAnnexB will stick the AnnexB extra data
  // at the start of the input. So we need to account for that as clear data
  // in the subsamples.
  if (raw->mKeyframe && !subsamples.IsEmpty()) {
    subsamples[0].clear_bytes += mAnnexB->Length();
  }

  WidevineVideoFrame frame;
  Status rv = CDM()->DecryptAndDecodeFrame(sample, &frame);
  Log("WidevineVideoDecoder::Decode(timestamp=%lld) rv=%d", sample.timestamp, rv);

  // Destroy frame, so that the shmem is now free to be used to return
  // output to the Gecko process.
  aInputFrame->Destroy();
  aInputFrame = nullptr;

  if (rv == kSuccess) {
    if (!ReturnOutput(frame)) {
      Log("WidevineVideoDecoder::Decode() Failed in ReturnOutput()");
      mCallback->Error(GMPDecodeErr);
      return;
    }
    mCallback->InputDataExhausted();
  } else if (rv == kNeedMoreData) {
    mCallback->InputDataExhausted();
  } else {
    mCallback->Error(ToGMPErr(rv));
  }
}

bool
WidevineVideoDecoder::ReturnOutput(WidevineVideoFrame& aCDMFrame)
{
  GMPVideoFrame* f = nullptr;
  auto err = mVideoHost->CreateFrame(kGMPI420VideoFrame, &f);
  if (GMP_FAILED(err) || !f) {
    Log("Failed to create i420 frame!\n");
    return false;
  }
  auto gmpFrame = static_cast<GMPVideoi420Frame*>(f);
  Size size = aCDMFrame.Size();
  const int32_t yStride = aCDMFrame.Stride(VideoFrame::kYPlane);
  const int32_t uStride = aCDMFrame.Stride(VideoFrame::kUPlane);
  const int32_t vStride = aCDMFrame.Stride(VideoFrame::kVPlane);
  const int32_t halfHeight = size.height / 2;
  err = gmpFrame->CreateEmptyFrame(size.width,
                                   size.height,
                                   yStride,
                                   uStride,
                                   vStride);
  ENSURE_GMP_SUCCESS(err, false);

  err = gmpFrame->SetWidth(size.width);
  ENSURE_GMP_SUCCESS(err, false);

  err = gmpFrame->SetHeight(size.height);
  ENSURE_GMP_SUCCESS(err, false);

  Buffer* buffer = aCDMFrame.FrameBuffer();
  uint8_t* outBuffer = gmpFrame->Buffer(kGMPYPlane);
  ENSURE_TRUE(outBuffer != nullptr, false);
  MOZ_ASSERT(gmpFrame->AllocatedSize(kGMPYPlane) >= yStride*size.height);
  memcpy(outBuffer,
         buffer->Data() + aCDMFrame.PlaneOffset(VideoFrame::kYPlane),
         yStride * size.height);

  outBuffer = gmpFrame->Buffer(kGMPUPlane);
  ENSURE_TRUE(outBuffer != nullptr, false);
  MOZ_ASSERT(gmpFrame->AllocatedSize(kGMPUPlane) >= uStride * halfHeight);
  memcpy(outBuffer,
         buffer->Data() + aCDMFrame.PlaneOffset(VideoFrame::kUPlane),
         uStride * halfHeight);

  outBuffer = gmpFrame->Buffer(kGMPVPlane);
  ENSURE_TRUE(outBuffer != nullptr, false);
  MOZ_ASSERT(gmpFrame->AllocatedSize(kGMPVPlane) >= vStride * halfHeight);
  memcpy(outBuffer,
         buffer->Data() + aCDMFrame.PlaneOffset(VideoFrame::kVPlane),
         vStride * halfHeight);

  gmpFrame->SetTimestamp(aCDMFrame.Timestamp());

  auto d = mFrameDurations.find(aCDMFrame.Timestamp());
  if (d != mFrameDurations.end()) {
    gmpFrame->SetDuration(d->second);
    mFrameDurations.erase(d);
  }

  mCallback->Decoded(gmpFrame);

  return true;
}

void
WidevineVideoDecoder::Reset()
{
  Log("WidevineVideoDecoder::Reset() mSentInput=%d", mSentInput);
  if (mSentInput) {
    CDM()->ResetDecoder(kStreamTypeVideo);
  }
  mFrameDurations.clear();
  mCallback->ResetComplete();
  mSentInput = false;
}

void
WidevineVideoDecoder::Drain()
{
  Log("WidevineVideoDecoder::Drain()");

  Status rv = kSuccess;
  while (rv == kSuccess) {
    WidevineVideoFrame frame;
    InputBuffer sample;
    Status rv = CDM()->DecryptAndDecodeFrame(sample, &frame);
    Log("WidevineVideoDecoder::Drain();  DecryptAndDecodeFrame() rv=%d", rv);
    if (frame.Format() == kUnknownVideoFormat) {
      break;
    }
    if (rv == kSuccess) {
      if (!ReturnOutput(frame)) {
        Log("WidevineVideoDecoder::Decode() Failed in ReturnOutput()");
      }
    }
  }

  CDM()->ResetDecoder(kStreamTypeVideo);
  mCallback->DrainComplete();
}

void
WidevineVideoDecoder::DecodingComplete()
{
  Log("WidevineVideoDecoder::DecodingComplete()");
  if (mCDMWrapper) {
    CDM()->DeinitializeDecoder(kStreamTypeVideo);
    mCDMWrapper = nullptr;
  }
  // Release that corresponds to AddRef() in constructor.
  Release();
}

} // namespace mozilla

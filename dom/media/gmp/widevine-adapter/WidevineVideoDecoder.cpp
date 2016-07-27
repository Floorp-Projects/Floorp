/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WidevineVideoDecoder.h"

#include "mp4_demuxer/AnnexB.h"
#include "WidevineUtils.h"
#include "WidevineVideoFrame.h"
#include "mozilla/Move.h"

using namespace cdm;

namespace mozilla {

WidevineVideoDecoder::WidevineVideoDecoder(GMPVideoHost* aVideoHost,
                                           RefPtr<CDMWrapper> aCDMWrapper)
  : mVideoHost(aVideoHost)
  , mCDMWrapper(Move(aCDMWrapper))
  , mExtraData(new MediaByteBuffer())
  , mSentInput(false)
  , mReturnOutputCallDepth(0)
  , mDrainPending(false)
  , mResetInProgress(false)
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
  // We should not be given new input if a drain has been initiated
  MOZ_ASSERT(!mDrainPending);
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
    // A reset should only be started at most at level mReturnOutputCallDepth 1,
    // and if it's started it should be finished by that call by the time
    // the it returns, so it should always be false by this point.
    MOZ_ASSERT(!mResetInProgress);
    // Only request more data if we don't have pending samples.
    if (mFrameAllocationQueue.empty()) {
      MOZ_ASSERT(mCDMWrapper);
      mCallback->InputDataExhausted();
    }
  } else if (rv == kNeedMoreData) {
    MOZ_ASSERT(mCDMWrapper);
    mCallback->InputDataExhausted();
  } else {
    mCallback->Error(ToGMPErr(rv));
  }
  // Finish a drain if pending and we have no pending ReturnOutput calls on the stack.
  if (mDrainPending && mReturnOutputCallDepth == 0) {
    Drain();
  }
}

// Util class to assist with counting mReturnOutputCallDepth.
class CounterHelper {
public:
  // RAII, increment counter
  explicit CounterHelper(int32_t& counter)
    : mCounter(counter)
  {
    mCounter++;
  }

  // RAII, decrement counter
  ~CounterHelper()
  {
    mCounter--;
  }

private:
  int32_t& mCounter;
};

// Util class to make sure GMP frames are freed. Holds a GMPVideoi420Frame*
// and will destroy it when the helper is destroyed unless the held frame
// if forgotten with ForgetFrame.
class FrameDestroyerHelper {
public:
  explicit FrameDestroyerHelper(GMPVideoi420Frame*& frame)
    : frame(frame)
  {
  }

  // RAII, destroy frame if held.
  ~FrameDestroyerHelper()
  {
    if (frame) {
      frame->Destroy();
    }
    frame = nullptr;
  }

  // Forget the frame without destroying it.
  void ForgetFrame()
  {
    frame = nullptr;
  }

private:
  GMPVideoi420Frame* frame;
};


// Special handing is needed around ReturnOutput as it spins the IPC message
// queue when creating an empty frame and can end up with reentrant calls into
// the class methods.
bool
WidevineVideoDecoder::ReturnOutput(WidevineVideoFrame& aCDMFrame)
{
  MOZ_ASSERT(mReturnOutputCallDepth >= 0);
  CounterHelper counterHelper(mReturnOutputCallDepth);
  mFrameAllocationQueue.push_back(Move(aCDMFrame));
  if (mReturnOutputCallDepth > 1) {
    // In a reentrant call.
    return true;
  }
  while (!mFrameAllocationQueue.empty()) {
    MOZ_ASSERT(mReturnOutputCallDepth == 1);
    // If we're at call level 1 a reset should not have been started. A
    // reset may be received during CreateEmptyFrame below, but we should not
    // be in a reset at this stage -- this would indicate receiving decode
    // messages before completing our reset, which we should not.
    MOZ_ASSERT(!mResetInProgress);
    WidevineVideoFrame currentCDMFrame = Move(mFrameAllocationQueue.front());
    mFrameAllocationQueue.pop_front();
    GMPVideoFrame* f = nullptr;
    auto err = mVideoHost->CreateFrame(kGMPI420VideoFrame, &f);
    if (GMP_FAILED(err) || !f) {
      Log("Failed to create i420 frame!\n");
      return false;
    }
    auto gmpFrame = static_cast<GMPVideoi420Frame*>(f);
    FrameDestroyerHelper frameDestroyerHelper(gmpFrame);
    Size size = currentCDMFrame.Size();
    const int32_t yStride = currentCDMFrame.Stride(VideoFrame::kYPlane);
    const int32_t uStride = currentCDMFrame.Stride(VideoFrame::kUPlane);
    const int32_t vStride = currentCDMFrame.Stride(VideoFrame::kVPlane);
    const int32_t halfHeight = size.height / 2;
    // This call can cause a shmem alloc, during this alloc other calls
    // may be made to this class and placed on the stack. ***WARNING***:
    // other IPC calls can happen during this call, resulting in calls
    // being made to the CDM. After this call state can have changed,
    // and should be reevaluated.
    err = gmpFrame->CreateEmptyFrame(size.width,
                                     size.height,
                                     yStride,
                                     uStride,
                                     vStride);
    // Assert possible reentrant calls or resets haven't altered level unexpectedly.
    MOZ_ASSERT(mReturnOutputCallDepth == 1);
    ENSURE_GMP_SUCCESS(err, false);

    // If a reset started we need to dump the current frame and complete the reset.
    if (mResetInProgress) {
      MOZ_ASSERT(mCDMWrapper);
      MOZ_ASSERT(mFrameAllocationQueue.empty());
      CompleteReset();
      return true;
    }

    err = gmpFrame->SetWidth(size.width);
    ENSURE_GMP_SUCCESS(err, false);

    err = gmpFrame->SetHeight(size.height);
    ENSURE_GMP_SUCCESS(err, false);

    Buffer* buffer = currentCDMFrame.FrameBuffer();
    uint8_t* outBuffer = gmpFrame->Buffer(kGMPYPlane);
    ENSURE_TRUE(outBuffer != nullptr, false);
    MOZ_ASSERT(gmpFrame->AllocatedSize(kGMPYPlane) >= yStride*size.height);
    memcpy(outBuffer,
           buffer->Data() + currentCDMFrame.PlaneOffset(VideoFrame::kYPlane),
           yStride * size.height);

    outBuffer = gmpFrame->Buffer(kGMPUPlane);
    ENSURE_TRUE(outBuffer != nullptr, false);
    MOZ_ASSERT(gmpFrame->AllocatedSize(kGMPUPlane) >= uStride * halfHeight);
    memcpy(outBuffer,
           buffer->Data() + currentCDMFrame.PlaneOffset(VideoFrame::kUPlane),
           uStride * halfHeight);

    outBuffer = gmpFrame->Buffer(kGMPVPlane);
    ENSURE_TRUE(outBuffer != nullptr, false);
    MOZ_ASSERT(gmpFrame->AllocatedSize(kGMPVPlane) >= vStride * halfHeight);
    memcpy(outBuffer,
           buffer->Data() + currentCDMFrame.PlaneOffset(VideoFrame::kVPlane),
           vStride * halfHeight);

    gmpFrame->SetTimestamp(currentCDMFrame.Timestamp());

    auto d = mFrameDurations.find(currentCDMFrame.Timestamp());
    if (d != mFrameDurations.end()) {
      gmpFrame->SetDuration(d->second);
      mFrameDurations.erase(d);
    }

    // Forget frame so it's not deleted, call back taking ownership.
    frameDestroyerHelper.ForgetFrame();
    mCallback->Decoded(gmpFrame);
  }

  return true;
}

void
WidevineVideoDecoder::Reset()
{
  Log("WidevineVideoDecoder::Reset() mSentInput=%d", mSentInput);
  // We shouldn't reset if a drain is pending.
  MOZ_ASSERT(!mDrainPending);
  mResetInProgress = true;
  if (mSentInput) {
    CDM()->ResetDecoder(kStreamTypeVideo);
  }
  // Remove queued frames, but do not reset mReturnOutputCallDepth, let the
  // ReturnOutput calls unwind and decrement the counter as needed.
  mFrameAllocationQueue.clear();
  mFrameDurations.clear();
  // Only if no ReturnOutput calls are in progress can we complete, otherwise
  // ReturnOutput needs to finalize the reset.
  if (mReturnOutputCallDepth == 0) {
    CompleteReset();
  }
}

void
WidevineVideoDecoder::CompleteReset()
{
  mCallback->ResetComplete();
  mSentInput = false;
  mResetInProgress = false;
}

void
WidevineVideoDecoder::Drain()
{
  Log("WidevineVideoDecoder::Drain()");
  if (mReturnOutputCallDepth > 0) {
    Log("Drain call is reentrant, postponing drain");
    mDrainPending = true;
    return;
  }

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
  // Shouldn't be reset while draining.
  MOZ_ASSERT(!mResetInProgress);

  CDM()->ResetDecoder(kStreamTypeVideo);
  mDrainPending = false;
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

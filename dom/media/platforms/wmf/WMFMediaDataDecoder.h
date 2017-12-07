/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(WMFMediaDataDecoder_h_)
#define WMFMediaDataDecoder_h_

#include "MFTDecoder.h"
#include "PlatformDecoderModule.h"
#include "WMF.h"
#include "mozilla/RefPtr.h"
#include "nsAutoPtr.h"

namespace mozilla {

// Encapsulates the initialization of the MFTDecoder appropriate for decoding
// a given stream, and the process of converting the IMFSample produced
// by the MFT into a MediaData object.
class MFTManager
{
public:
  virtual ~MFTManager() { }

  // Submit a compressed sample for decoding.
  // This should forward to the MFTDecoder after performing
  // any required sample formatting.
  virtual HRESULT Input(MediaRawData* aSample) = 0;

  // Produces decoded output, if possible. Blocks until output can be produced,
  // or until no more is able to be produced.
  // Returns S_OK on success, or MF_E_TRANSFORM_NEED_MORE_INPUT if there's not
  // enough data to produce more output. If this returns a failure code other
  // than MF_E_TRANSFORM_NEED_MORE_INPUT, an error will be reported to the
  // MP4Reader.
  virtual HRESULT Output(int64_t aStreamOffset,
                         RefPtr<MediaData>& aOutput) = 0;

  virtual void Flush()
  {
    mDecoder->Flush();
    mSeekTargetThreshold.reset();
  }

  virtual void Drain()
  {
    if (FAILED(mDecoder->SendMFTMessage(MFT_MESSAGE_COMMAND_DRAIN, 0))) {
      NS_WARNING("Failed to send DRAIN command to MFT");
    }
  }

  // Destroys all resources.
  virtual void Shutdown() = 0;

  virtual bool IsHardwareAccelerated(nsACString& aFailureReason) const
  {
    return false;
  }

  virtual TrackInfo::TrackType GetType() = 0;

  virtual nsCString GetDescriptionName() const = 0;

  virtual void SetSeekThreshold(const media::TimeUnit& aTime)
  {
    mSeekTargetThreshold = Some(aTime);
  }

  virtual MediaDataDecoder::ConversionRequired NeedsConversion() const
  {
    return MediaDataDecoder::ConversionRequired::kNeedNone;
  }

protected:
  // IMFTransform wrapper that performs the decoding.
  RefPtr<MFTDecoder> mDecoder;

  Maybe<media::TimeUnit> mSeekTargetThreshold;
};

DDLoggedTypeDeclNameAndBase(WMFMediaDataDecoder, MediaDataDecoder);

// Decodes audio and video using Windows Media Foundation. Samples are decoded
// using the MFTDecoder created by the MFTManager. This class implements
// the higher-level logic that drives mapping the MFT to the async
// MediaDataDecoder interface. The specifics of decoding the exact stream
// type are handled by MFTManager and the MFTDecoder it creates.
class WMFMediaDataDecoder
  : public MediaDataDecoder
  , public DecoderDoctorLifeLogger<WMFMediaDataDecoder>
{
public:
  WMFMediaDataDecoder(MFTManager* aOutputSource, TaskQueue* aTaskQueue);
  ~WMFMediaDataDecoder();

  RefPtr<MediaDataDecoder::InitPromise> Init() override;

  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;

  RefPtr<DecodePromise> Drain() override;

  RefPtr<FlushPromise> Flush() override;

  RefPtr<ShutdownPromise> Shutdown() override;

  bool IsHardwareAccelerated(nsACString& aFailureReason) const override;

  nsCString GetDescriptionName() const override
  {
    return mMFTManager
           ? mMFTManager->GetDescriptionName() : NS_LITERAL_CSTRING("");
  }

  ConversionRequired NeedsConversion() const override
  {
    MOZ_ASSERT(mMFTManager);
    return mMFTManager->NeedsConversion();
  }

  virtual void SetSeekThreshold(const media::TimeUnit& aTime) override;

private:

  RefPtr<DecodePromise> ProcessError(HRESULT aError, const char* aReason);

  // Called on the task queue. Inserts the sample into the decoder, and
  // extracts output if available.
  RefPtr<DecodePromise> ProcessDecode(MediaRawData* aSample);

  // Called on the task queue. Extracts output if available, and delivers
  // it to the reader. Called after ProcessDecode() and ProcessDrain().
  HRESULT ProcessOutput(DecodedData& aResults);

  // Called on the task queue. Orders the MFT to flush.  There is no output to
  // extract.
  RefPtr<FlushPromise> ProcessFlush();

  // Called on the task queue. Orders the MFT to drain, and then extracts
  // all available output.
  RefPtr<DecodePromise> ProcessDrain();

  RefPtr<ShutdownPromise> ProcessShutdown();

  const RefPtr<TaskQueue> mTaskQueue;

  nsAutoPtr<MFTManager> mMFTManager;

  // The last offset into the media resource that was passed into Input().
  // This is used to approximate the decoder's position in the media resource.
  int64_t mLastStreamOffset;

  bool mIsShutDown = false;

  enum class DrainStatus
  {
    DRAINED,
    DRAINABLE,
    DRAINING,
  };
  DrainStatus mDrainStatus = DrainStatus::DRAINED;

  // For telemetry
  bool mHasSuccessfulOutput = false;
  bool mRecordedError = false;
};

} // namespace mozilla

#endif // WMFMediaDataDecoder_h_

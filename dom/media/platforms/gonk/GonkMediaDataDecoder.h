/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GonkMediaDataDecoder_h_)
#define GonkMediaDataDecoder_h_
#include "PlatformDecoderModule.h"
#include <stagefright/foundation/AHandler.h>

namespace android {
struct ALooper;
class MediaBuffer;
class MediaCodecProxy;
} // namespace android

namespace mozilla {
class MediaRawData;

// Manage the data flow from inputting encoded data and outputting decode data.
class GonkDecoderManager : public android::AHandler {
public:
  typedef TrackInfo::TrackType TrackType;
  typedef MediaDataDecoder::InitPromise InitPromise;
  typedef MediaDataDecoder::DecoderFailureReason DecoderFailureReason;

  virtual ~GonkDecoderManager() {}

  virtual RefPtr<InitPromise> Init() = 0;

  // Asynchronously send sample into mDecoder. If out of input buffer, aSample
  // will be queued for later re-send.
  nsresult Input(MediaRawData* aSample);

  // Flush the queued samples and signal decoder to throw all pending input/output away.
  nsresult Flush();

  // Shutdown decoder and rejects the init promise.
  virtual nsresult Shutdown();

  // How many samples are waiting for processing.
  size_t NumQueuedSamples();

  // Set callback for decoder events, such as requesting more input,
  // returning output, or reporting error.
  void SetDecodeCallback(MediaDataDecoderCallback* aCallback)
  {
    mDecodeCallback = aCallback;
  }

protected:
  GonkDecoderManager()
    : mMutex("GonkDecoderManager")
    , mLastTime(INT64_MIN)
    , mFlushMonitor("GonkDecoderManager::Flush")
    , mIsFlushing(false)
    , mDecodeCallback(nullptr)
  {}

  bool InitLoopers(MediaData::Type aType);

  void onMessageReceived(const android::sp<android::AMessage> &aMessage) override;

  // Produces decoded output. It returns NS_OK on success, or NS_ERROR_NOT_AVAILABLE
  // when output is not produced yet.
  // If this returns a failure code other than NS_ERROR_NOT_AVAILABLE, an error
  // will be reported through mDecodeCallback.
  virtual nsresult Output(int64_t aStreamOffset,
                          RefPtr<MediaData>& aOutput) = 0;

  // Send queued samples to OMX. It returns how many samples are still in
  // queue after processing, or negative error code if failed.
  int32_t ProcessQueuedSamples();

  void ProcessInput(bool aEndOfStream);
  virtual void ProcessFlush();
  void ProcessToDo(bool aEndOfStream);

  RefPtr<MediaByteBuffer> mCodecSpecificData;

  nsAutoCString mMimeType;

  // MediaCodedc's wrapper that performs the decoding.
  android::sp<android::MediaCodecProxy> mDecoder;
  // Looper for mDecoder to run on.
  android::sp<android::ALooper> mDecodeLooper;
  // Looper to run decode tasks such as processing input, output, flush, and
  // recycling output buffers.
  android::sp<android::ALooper> mTaskLooper;
  // Message codes for tasks running on mTaskLooper.
  enum {
    // Decoder will send this to indicate internal state change such as input or
    // output buffers availability. Used to run pending input & output tasks.
    kNotifyDecoderActivity = 'nda ',
    // Signal the decoder to flush.
    kNotifyProcessFlush = 'npf ',
    // Used to process queued samples when there is new input.
    kNotifyProcessInput = 'npi ',
#ifdef DEBUG
    kNotifyFindLooperId = 'nfli',
#endif
  };

  MozPromiseHolder<InitPromise> mInitPromise;

  Mutex mMutex; // Protects mQueuedSamples.
  // A queue that stores the samples waiting to be sent to mDecoder.
  // Empty element means EOS and there shouldn't be any sample be queued after it.
  // Samples are queued in caller's thread and dequeued in mTaskLooper.
  nsTArray<RefPtr<MediaRawData>> mQueuedSamples;

  // The last decoded frame presentation time. Only accessed on mTaskLooper.
  int64_t mLastTime;

  Monitor mFlushMonitor; // Waits for flushing to complete.
  bool mIsFlushing; // Protected by mFlushMonitor.

  // Remembers the notification that is currently waiting for the decoder event
  // to avoid requesting more than one notification at the time, which is
  // forbidden by mDecoder.
  android::sp<android::AMessage> mToDo;

  // Stores sample info for output buffer processing later.
  struct WaitOutputInfo {
    WaitOutputInfo(int64_t aOffset, int64_t aTimestamp, bool aEOS)
      : mOffset(aOffset)
      , mTimestamp(aTimestamp)
      , mEOS(aEOS)
    {}
    const int64_t mOffset;
    const int64_t mTimestamp;
    const bool mEOS;
  };

  nsTArray<WaitOutputInfo> mWaitOutput;

  MediaDataDecoderCallback* mDecodeCallback; // Reports decoder output or error.

private:
  void UpdateWaitingList(int64_t aForgetUpTo);

#ifdef DEBUG
  typedef void* LooperId;

  bool OnTaskLooper();
  LooperId mTaskLooperId;
#endif
};

class AutoReleaseMediaBuffer
{
public:
  AutoReleaseMediaBuffer(android::MediaBuffer* aBuffer, android::MediaCodecProxy* aCodec)
    : mBuffer(aBuffer)
    , mCodec(aCodec)
  {}

  ~AutoReleaseMediaBuffer()
  {
    MOZ_ASSERT(mCodec.get());
    if (mBuffer) {
      mCodec->ReleaseMediaBuffer(mBuffer);
    }
  }

  android::MediaBuffer* forget()
  {
    android::MediaBuffer* tmp = mBuffer;
    mBuffer = nullptr;
    return tmp;
  }

private:
  android::MediaBuffer* mBuffer;
  android::sp<android::MediaCodecProxy> mCodec;
};

// Samples are decoded using the GonkDecoder (MediaCodec)
// created by the GonkDecoderManager. This class implements
// the higher-level logic that drives mapping the Gonk to the async
// MediaDataDecoder interface. The specifics of decoding the exact stream
// type are handled by GonkDecoderManager and the GonkDecoder it creates.
class GonkMediaDataDecoder : public MediaDataDecoder {
public:
  GonkMediaDataDecoder(GonkDecoderManager* aDecoderManager,
                       FlushableTaskQueue* aTaskQueue,
                       MediaDataDecoderCallback* aCallback);

  ~GonkMediaDataDecoder();

  RefPtr<InitPromise> Init() override;

  nsresult Input(MediaRawData* aSample) override;

  nsresult Flush() override;

  nsresult Drain() override;

  nsresult Shutdown() override;

private:

  android::sp<GonkDecoderManager> mManager;
};

} // namespace mozilla

#endif // GonkMediaDataDecoder_h_

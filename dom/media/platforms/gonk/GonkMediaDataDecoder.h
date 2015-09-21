/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GonkMediaDataDecoder_h_)
#define GonkMediaDataDecoder_h_
#include "PlatformDecoderModule.h"

namespace android {
class MediaCodecProxy;
} // namespace android

namespace mozilla {
class MediaRawData;

// Manage the data flow from inputting encoded data and outputting decode data.
class GonkDecoderManager {
public:
  typedef TrackInfo::TrackType TrackType;
  typedef MediaDataDecoder::InitPromise InitPromise;
  typedef MediaDataDecoder::DecoderFailureReason DecoderFailureReason;

  virtual ~GonkDecoderManager() {}

  virtual nsRefPtr<InitPromise> Init(MediaDataDecoderCallback* aCallback) = 0;

  // Add samples into OMX decoder or queue them if decoder is out of input buffer.
  virtual nsresult Input(MediaRawData* aSample) = 0;

  // Produces decoded output, it blocks until output can be produced or a timeout
  // is expired or until EOS. Returns NS_OK on success, or NS_ERROR_NOT_AVAILABLE
  // if there's not enough data to produce more output. If this returns a failure
  // code other than NS_ERROR_NOT_AVAILABLE, an error will be reported to the
  // MP4Reader.
  // The overrided class should follow the same behaviour.
  virtual nsresult Output(int64_t aStreamOffset,
                          nsRefPtr<MediaData>& aOutput) = 0;

  // Flush the queued sample.
  virtual nsresult Flush() = 0;

  // Shutdown decoder and rejects the init promise.
  nsresult Shutdown();

  // True if sample is queued.
  virtual bool HasQueuedSample() = 0;

protected:
  nsRefPtr<MediaByteBuffer> mCodecSpecificData;

  nsAutoCString mMimeType;

  // MediaCodedc's wrapper that performs the decoding.
  android::sp<android::MediaCodecProxy> mDecoder;

  MozPromiseHolder<InitPromise> mInitPromise;

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

  nsRefPtr<InitPromise> Init() override;

  nsresult Input(MediaRawData* aSample) override;

  nsresult Flush() override;

  nsresult Drain() override;

  nsresult Shutdown() override;

private:

  // Called on the task queue. Inserts the sample into the decoder, and
  // extracts output if available, if aSample is null, it means there is
  // no data from source, it will notify the decoder EOS and flush all the
  // decoded frames.
  void ProcessDecode(MediaRawData* aSample);

  // Called on the task queue. Extracts output if available, and delivers
  // it to the reader. Called after ProcessDecode() and ProcessDrain().
  void ProcessOutput();

  // Called on the task queue. Orders the Gonk to drain, and then extracts
  // all available output.
  void ProcessDrain();

  RefPtr<FlushableTaskQueue> mTaskQueue;
  MediaDataDecoderCallback* mCallback;

  nsAutoPtr<GonkDecoderManager> mManager;

  // The last offset into the media resource that was passed into Input().
  // This is used to approximate the decoder's position in the media resource.
  int64_t mLastStreamOffset;
  // Set it ture when there is no input data
  bool mSignaledEOS;
  // Set if there is no more output data from decoder
  bool mDrainComplete;
};

} // namespace mozilla

#endif // GonkMediaDataDecoder_h_

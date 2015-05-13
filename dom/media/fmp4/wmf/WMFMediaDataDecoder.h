/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(WMFMediaDataDecoder_h_)
#define WMFMediaDataDecoder_h_


#include "WMF.h"
#include "MP4Reader.h"
#include "MFTDecoder.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

// Encapsulates the initialization of the MFTDecoder appropriate for decoding
// a given stream, and the process of converting the IMFSample produced
// by the MFT into a MediaData object.
class MFTManager {
public:
  virtual ~MFTManager() {}

  // Creates an initializs the MFTDecoder.
  // Returns nullptr on failure.
  virtual TemporaryRef<MFTDecoder> Init() = 0;

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
                         nsRefPtr<MediaData>& aOutput) = 0;

  // Destroys all resources.
  virtual void Shutdown() = 0;

  virtual bool IsHardwareAccelerated() const { return false; }

};

// Decodes audio and video using Windows Media Foundation. Samples are decoded
// using the MFTDecoder created by the MFTManager. This class implements
// the higher-level logic that drives mapping the MFT to the async
// MediaDataDecoder interface. The specifics of decoding the exact stream
// type are handled by MFTManager and the MFTDecoder it creates.
class WMFMediaDataDecoder : public MediaDataDecoder {
public:
  WMFMediaDataDecoder(MFTManager* aOutputSource,
                      FlushableMediaTaskQueue* aAudioTaskQueue,
                      MediaDataDecoderCallback* aCallback);
  ~WMFMediaDataDecoder();

  virtual nsresult Init() override;

  virtual nsresult Input(MediaRawData* aSample);

  virtual nsresult Flush() override;

  virtual nsresult Drain() override;

  virtual nsresult Shutdown() override;

  virtual bool IsHardwareAccelerated() const override;

private:

  // Called on the task queue. Inserts the sample into the decoder, and
  // extracts output if available.
  void ProcessDecode(MediaRawData* aSample);

  // Called on the task queue. Extracts output if available, and delivers
  // it to the reader. Called after ProcessDecode() and ProcessDrain().
  void ProcessOutput();

  // Called on the task queue. Orders the MFT to flush.  There is no output to
  // extract.
  void ProcessFlush();

  // Called on the task queue. Orders the MFT to drain, and then extracts
  // all available output.
  void ProcessDrain();

  void ProcessShutdown();

  RefPtr<FlushableMediaTaskQueue> mTaskQueue;
  MediaDataDecoderCallback* mCallback;

  RefPtr<MFTDecoder> mDecoder;
  nsAutoPtr<MFTManager> mMFTManager;

  // The last offset into the media resource that was passed into Input().
  // This is used to approximate the decoder's position in the media resource.
  int64_t mLastStreamOffset;

  // For access to and waiting on mIsFlushing
  Monitor mMonitor;
  // Set on reader/decode thread calling Flush() to indicate that output is
  // not required and so input samples on mTaskQueue need not be processed.
  // Cleared on mTaskQueue.
  bool mIsFlushing;

  bool mIsShutDown;

  // For telemetry
  bool mHasSuccessfulOutput = false;
  bool mRecordedError = false;
};

} // namespace mozilla

#endif // WMFMediaDataDecoder_h_

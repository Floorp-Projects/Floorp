/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(FuzzingWrapper_h_)
#define FuzzingWrapper_h_

#include "mozilla/Pair.h"
#include "PlatformDecoderModule.h"

#include <deque>

namespace mozilla {

// Fuzzing wrapper for media decoders.
//
// DecoderFuzzingWrapper owns the DecoderCallbackFuzzingWrapper, and inserts
// itself between the reader and the decoder.
// DecoderCallbackFuzzingWrapper inserts itself between a decoder and its
// callback.
// Together they are used to introduce some fuzzing, (e.g. delay output).
//
// Normally:
//          ====================================>
//   reader                                       decoder
//          <------------------------------------
//
// With fuzzing:
//          ======> DecoderFuzzingWrapper ======>
//   reader                  v                    decoder
//          <-- DecoderCallbackFuzzingWrapper <--
//
// Creation order should be:
// 1. Create DecoderCallbackFuzzingWrapper, give the expected callback target.
// 2. Create actual decoder, give DecoderCallbackFuzzingWrapper as callback.
// 3. Create DecoderFuzzingWrapper, give decoder and DecoderCallbackFuzzingWrapper.
// DecoderFuzzingWrapper is what the reader sees as decoder, it owns the
// real decoder and the DecoderCallbackFuzzingWrapper.

class DecoderCallbackFuzzingWrapper : public MediaDataDecoderCallback
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DecoderCallbackFuzzingWrapper)

  explicit DecoderCallbackFuzzingWrapper(MediaDataDecoderCallback* aCallback);

  // Enforce a minimum interval between output frames (i.e., limit frame rate).
  // Of course, if the decoder is even slower, this won't have any effect.
  void SetVideoOutputMinimumInterval(TimeDuration aFrameOutputMinimumInterval);
  // If false (default), if frames are delayed, any InputExhausted is delayed to
  // be later sent after the corresponding delayed frame.
  // If true, InputExhausted are passed through immediately; This could result
  // in lots of frames being decoded and queued for delayed output!
  void SetDontDelayInputExhausted(bool aDontDelayInputExhausted);

private:
  virtual ~DecoderCallbackFuzzingWrapper();

  // MediaDataDecoderCallback implementation.
  void Output(MediaData* aData) override;
  void Error() override;
  void InputExhausted() override;
  void DrainComplete() override;
  void ReleaseMediaResources() override;
  bool OnReaderTaskQueue() override;

  MediaDataDecoderCallback* mCallback;

  // Settings for minimum frame output interval & InputExhausted,
  // should be set during init and then only read on mTaskQueue.
  TimeDuration mFrameOutputMinimumInterval;
  bool mDontDelayInputExhausted;
  // Members for minimum frame output interval & InputExhausted,
  // should only be accessed on mTaskQueue.
  TimeStamp mPreviousOutput;
  // First member is the frame to be delayed.
  // Second member is true if an 'InputExhausted' arrived after that frame; in
  // which case an InputExhausted will be sent after finally outputting the frame.
  typedef Pair<nsRefPtr<MediaData>, bool> MediaDataAndInputExhausted;
  std::deque<MediaDataAndInputExhausted> mDelayedOutput;
  nsRefPtr<MediaTimer> mDelayedOutputTimer;
  // If draining, a 'DrainComplete' will be sent after all delayed frames have
  // been output.
  bool mDraining;
  // All callbacks are redirected through this task queue, both to avoid locking
  // and to have a consistent sequencing of callbacks.
  nsRefPtr<TaskQueue> mTaskQueue;
  void ScheduleOutputDelayedFrame();
  void OutputDelayedFrame();
public: // public for the benefit of DecoderFuzzingWrapper.
  void ClearDelayedOutput();
  void Shutdown();
};

class DecoderFuzzingWrapper : public MediaDataDecoder
{
public:
  DecoderFuzzingWrapper(already_AddRefed<MediaDataDecoder> aDecoder,
                        already_AddRefed<DecoderCallbackFuzzingWrapper> aCallbackWrapper);
  virtual ~DecoderFuzzingWrapper();

private:
  // MediaDataDecoder implementation.
  nsRefPtr<InitPromise> Init() override;
  nsresult Input(MediaRawData* aSample) override;
  nsresult Flush() override;
  nsresult Drain() override;
  nsresult Shutdown() override;
  bool IsHardwareAccelerated(nsACString& aFailureReason) const override;
  nsresult ConfigurationChanged(const TrackInfo& aConfig) override;

  nsRefPtr<MediaDataDecoder> mDecoder;
  nsRefPtr<DecoderCallbackFuzzingWrapper> mCallbackWrapper;
};

} // namespace mozilla

#endif

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(SampleSink_h_)
#define SampleSink_h_

#include "BaseFilter.h"
#include "DirectShowUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ReentrantMonitor.h"

namespace mozilla {

class SampleSink {
public:
  SampleSink();
  virtual ~SampleSink();

  // Sets the audio format of the incoming samples. The upstream filter
  // calls this. This makes a copy.
  void SetAudioFormat(const WAVEFORMATEX* aInFormat);

  // Copies the format of incoming audio samples into into *aOutFormat.
  void GetAudioFormat(WAVEFORMATEX* aOutFormat);

  // Called when a sample is delivered by the DirectShow graph to the sink.
  // The decode thread retrieves the sample by calling WaitForSample().
  // Blocks if there's already a sample waiting to be consumed by the decode
  // thread.
  HRESULT Receive(IMediaSample* aSample);

  // Retrieves a sample from the sample queue, blocking until one becomes
  // available, or until an error occurs. Returns S_FALSE on EOS.
  HRESULT Extract(RefPtr<IMediaSample>& aOutSample);

  // Unblocks any threads waiting in GetSample().
  // Clears mSample, which unblocks upstream stream.
  void Flush();

  // Opens up the sink to receive more samples in PutSample().
  // Clears EOS flag.
  void Reset();

  // Marks that we've reacehd the end of stream.
  void SetEOS();

  // Returns whether we're at end of stream.
  bool AtEOS();

private:
  // All data in this class is syncronized by mMonitor.
  ReentrantMonitor mMonitor;
  RefPtr<IMediaSample> mSample;

  // Format of the audio stream we're receiving.
  WAVEFORMATEX mAudioFormat;

  bool mIsFlushing;
  bool mAtEOS;
};

} // namespace mozilla

#endif // SampleSink_h_

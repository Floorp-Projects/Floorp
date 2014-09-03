/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASOURCEREADER_H_
#define MOZILLA_MEDIASOURCEREADER_H_

#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsString.h"
#include "nsTArray.h"
#include "MediaDecoderReader.h"

namespace mozilla {

class MediaSourceDecoder;
class SourceBufferDecoder;

namespace dom {

class MediaSource;

} // namespace dom

class MediaSourceReader : public MediaDecoderReader
{
public:
  explicit MediaSourceReader(MediaSourceDecoder* aDecoder);

  nsresult Init(MediaDecoderReader* aCloneDonor) MOZ_OVERRIDE
  {
    // Although we technically don't implement anything here, we return NS_OK
    // so that when the state machine initializes and calls this function
    // we don't return an error code back to the media element.
    return NS_OK;
  }

  bool IsWaitingMediaResources() MOZ_OVERRIDE;

  void RequestAudioData() MOZ_OVERRIDE;

  void OnAudioDecoded(AudioData* aSample);

  void OnAudioEOS();

  void RequestVideoData(bool aSkipToNextKeyframe, int64_t aTimeThreshold) MOZ_OVERRIDE;

  void OnVideoDecoded(VideoData* aSample);

  void OnVideoEOS();

  void OnDecodeError();

  bool HasVideo() MOZ_OVERRIDE
  {
    return mInfo.HasVideo();
  }

  bool HasAudio() MOZ_OVERRIDE
  {
    return mInfo.HasAudio();
  }

  bool IsMediaSeekable() { return true; }

  nsresult ReadMetadata(MediaInfo* aInfo, MetadataTags** aTags) MOZ_OVERRIDE;
  nsresult Seek(int64_t aTime, int64_t aStartTime, int64_t aEndTime,
                int64_t aCurrentTime) MOZ_OVERRIDE;
  already_AddRefed<SourceBufferDecoder> CreateSubDecoder(const nsACString& aType);

  void Shutdown();

  virtual void BreakCycles();

  void InitializePendingDecoders();

  bool IsShutdown()
  {
    ReentrantMonitorAutoEnter decoderMon(mDecoder->GetReentrantMonitor());
    return mDecoder->IsShutdown();
  }

  // Return true if any of the active decoders contain data for the given time
  bool DecodersContainTime(double aTime);

  // Mark the reader to indicate that EndOfStream has been called on our MediaSource
  void Ended();

  // Return true if the Ended method has been called
  bool IsEnded();

private:
  enum SwitchType {
    SWITCH_OPTIONAL,
    SWITCH_FORCED
  };

  bool SwitchReaders(SwitchType aType);

  bool SwitchAudioReader(MediaDecoderReader* aTargetReader);
  bool SwitchVideoReader(MediaDecoderReader* aTargetReader);

  // These are read and written on the decode task queue threads.
  int64_t mTimeThreshold;
  bool mDropAudioBeforeThreshold;
  bool mDropVideoBeforeThreshold;

  nsTArray<nsRefPtr<SourceBufferDecoder>> mPendingDecoders;
  nsTArray<nsRefPtr<SourceBufferDecoder>> mDecoders;

  nsRefPtr<MediaDecoderReader> mAudioReader;
  nsRefPtr<MediaDecoderReader> mVideoReader;

  bool mEnded;

  // For a seek to complete we need to send a sample with
  // the mDiscontinuity field set to true once we have the
  // first decoded sample. These flags are set during seeking
  // so we can detect when we have the first decoded sample
  // after a seek.
  bool mAudioIsSeeking;
  bool mVideoIsSeeking;
};

} // namespace mozilla

#endif /* MOZILLA_MEDIASOURCEREADER_H_ */

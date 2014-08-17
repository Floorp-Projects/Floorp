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
#include "nsStringGlue.h"
#include "nsTArray.h"
#include "MediaDecoderReader.h"

namespace mozilla {

class MediaSourceDecoder;
class SubBufferDecoder;

namespace dom {

class MediaSource;

} // namespace dom

class MediaSourceReader : public MediaDecoderReader
{
public:
  MediaSourceReader(MediaSourceDecoder* aDecoder, dom::MediaSource* aSource);

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
  already_AddRefed<SubBufferDecoder> CreateSubDecoder(const nsACString& aType,
                                                      MediaSourceDecoder* aParentDecoder);

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

private:
  enum SwitchType {
    SWITCH_OPTIONAL,
    SWITCH_FORCED
  };

  bool SwitchVideoReaders(SwitchType aType);

  MediaDecoderReader* GetAudioReader();
  MediaDecoderReader* GetVideoReader();

  void SetMediaSourceDuration(double aDuration) ;

  // These are read and written on the decode task queue threads.
  int64_t mTimeThreshold;
  bool mDropVideoBeforeThreshold;

  nsTArray<nsRefPtr<SubBufferDecoder>> mPendingDecoders;
  nsTArray<nsRefPtr<SubBufferDecoder>> mDecoders;

  int32_t mActiveVideoDecoder;
  int32_t mActiveAudioDecoder;
  dom::MediaSource* mMediaSource;
};

} // namespace mozilla

#endif /* MOZILLA_MEDIASOURCEREADER_H_ */

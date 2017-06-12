/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(HLSDemuxer_h_)
#define HLSDemuxer_h_

#include "AutoTaskQueue.h"
#include "GeneratedJNINatives.h"
#include "GeneratedJNIWrappers.h"
#include "MediaCodec.h"
#include "MediaDataDemuxer.h"
#include "MediaDecoder.h"
#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"

#include "VideoUtils.h"

namespace mozilla {

class AbstractThread;
class MediaResult;
class HLSTrackDemuxer;

class HLSDemuxer final : public MediaDataDemuxer
{
  class HLSDemuxerCallbacksSupport;
public:
  explicit HLSDemuxer(MediaResource* aResource);

  RefPtr<InitPromise> Init() override;

  bool HasTrackType(TrackInfo::TrackType aType) const override;

  uint32_t GetNumberTracks(TrackInfo::TrackType aType) const override;

  already_AddRefed<MediaTrackDemuxer>
  GetTrackDemuxer(TrackInfo::TrackType aType, uint32_t aTrackNumber) override;

  bool IsSeekable() const override;

  UniquePtr<EncryptionInfo> GetCrypto() override;

  bool ShouldComputeStartTime() const override { return true; }

  void NotifyDataArrived() override;

  AutoTaskQueue* GetTaskQueue() const { return mTaskQueue; }
  void OnInitialized(bool aHasAudio, bool aHasVideo);

private:
  media::TimeUnit GetNextKeyFrameTime();
  void UpdateVideoInfo(int index);
  void UpdateAudioInfo(int index);
  bool OnTaskQueue() const;
  TrackInfo* GetTrackInfo(TrackInfo::TrackType);
  ~HLSDemuxer();
  RefPtr<MediaResource> mResource;
  friend class HLSTrackDemuxer;

  const RefPtr<AutoTaskQueue> mTaskQueue;
  nsTArray<RefPtr<HLSTrackDemuxer>> mDemuxers;

  MozPromiseHolder<InitPromise> mInitPromise;
  RefPtr<HLSDemuxerCallbacksSupport> mCallbackSupport;

  // Mutex to protect members below across multiple threads.
  mutable Mutex mMutex;
  MediaInfo mInfo;

  java::GeckoHLSDemuxerWrapper::Callbacks::GlobalRef mJavaCallbacks;
  java::GeckoHLSDemuxerWrapper::GlobalRef mHLSDemuxerWrapper;
};

class HLSTrackDemuxer : public MediaTrackDemuxer
{
public:
  HLSTrackDemuxer(HLSDemuxer* aParent,
                  TrackInfo::TrackType aType);
  ~HLSTrackDemuxer();
  UniquePtr<TrackInfo> GetInfo() const override;

  RefPtr<SeekPromise> Seek(const media::TimeUnit& aTime) override;

  RefPtr<SamplesPromise> GetSamples(int32_t aNumSamples = 1) override;

  void Reset() override;

  nsresult GetNextRandomAccessPoint(media::TimeUnit* aTime) override;

  RefPtr<SkipAccessPointPromise> SkipToNextRandomAccessPoint(
    const media::TimeUnit& aTimeThreshold) override;

  media::TimeIntervals GetBuffered() override;

  void BreakCycles() override;

  bool GetSamplesMayBlock() const override
  {
    return false;
  }

private:
  // Update the timestamp of the next keyframe if there's one.
  void UpdateNextKeyFrameTime();

  // Runs on HLSDemuxer's task queue.
  RefPtr<SeekPromise> DoSeek(const media::TimeUnit& aTime);
  RefPtr<SamplesPromise> DoGetSamples(int32_t aNumSamples);
  RefPtr<SkipAccessPointPromise> DoSkipToNextRandomAccessPoint(
    const media::TimeUnit& aTimeThreshold);

  CryptoSample ExtractCryptoSample(size_t aSampleSize,
                                   java::sdk::CryptoInfo::LocalRef aCryptoInfo);
  RefPtr<MediaRawData> ConvertToMediaRawData(java::GeckoHLSSample::LocalRef aSample);

  RefPtr<HLSDemuxer> mParent;
  TrackInfo::TrackType mType;
  Maybe<media::TimeUnit> mNextKeyframeTime;
  int32_t mLastFormatIndex = -1;
  // Queued samples extracted by the demuxer, but not yet returned.
  RefPtr<MediaRawData> mQueuedSample;
};

} // namespace mozilla

#endif

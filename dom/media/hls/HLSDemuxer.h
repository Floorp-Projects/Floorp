/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(HLSDemuxer_h_)
#define HLSDemuxer_h_

#include "GeneratedJNINatives.h"
#include "GeneratedJNIWrappers.h"
#include "MediaCodec.h"
#include "MediaDataDemuxer.h"
#include "MediaDecoder.h"
#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/TaskQueue.h"

#include "VideoUtils.h"

namespace mozilla {

class AbstractThread;
class MediaResult;
class HLSTrackDemuxer;

DDLoggedTypeDeclNameAndBase(HLSDemuxer, MediaDataDemuxer);
DDLoggedTypeNameAndBase(HLSTrackDemuxer, MediaTrackDemuxer);

class HLSDemuxer final
  : public MediaDataDemuxer
  , public DecoderDoctorLifeLogger<HLSDemuxer>
{
  class HLSDemuxerCallbacksSupport;
public:
  explicit HLSDemuxer(int aPlayerId);

  RefPtr<InitPromise> Init() override;

  uint32_t GetNumberTracks(TrackInfo::TrackType aType) const override;

  already_AddRefed<MediaTrackDemuxer>
  GetTrackDemuxer(TrackInfo::TrackType aType, uint32_t aTrackNumber) override;

  bool IsSeekable() const override;

  UniquePtr<EncryptionInfo> GetCrypto() override;

  bool ShouldComputeStartTime() const override { return true; }

  void NotifyDataArrived() override;

  TaskQueue* GetTaskQueue() const { return mTaskQueue; }
  void OnInitialized(bool aHasAudio, bool aHasVideo);
  void OnError(int aErrorCode);

private:
  media::TimeUnit GetNextKeyFrameTime();

  bool OnTaskQueue() const;
  ~HLSDemuxer();
  friend class HLSTrackDemuxer;

  const RefPtr<TaskQueue> mTaskQueue;
  RefPtr<HLSTrackDemuxer> mAudioDemuxer;
  RefPtr<HLSTrackDemuxer> mVideoDemuxer;

  MozPromiseHolder<InitPromise> mInitPromise;
  RefPtr<HLSDemuxerCallbacksSupport> mCallbackSupport;

  java::GeckoHLSDemuxerWrapper::Callbacks::GlobalRef mJavaCallbacks;
  java::GeckoHLSDemuxerWrapper::GlobalRef mHLSDemuxerWrapper;
};

class HLSTrackDemuxer
  : public MediaTrackDemuxer
  , public DecoderDoctorLifeLogger<HLSTrackDemuxer>
{
public:
  HLSTrackDemuxer(HLSDemuxer* aParent,
                  TrackInfo::TrackType aType,
                  UniquePtr<TrackInfo> aTrackInfo);
  ~HLSTrackDemuxer();
  UniquePtr<TrackInfo> GetInfo() const override;

  RefPtr<SeekPromise> Seek(const media::TimeUnit& aTime) override;

  RefPtr<SamplesPromise> GetSamples(int32_t aNumSamples = 1) override;

  void UpdateMediaInfo(int index);

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

  bool IsTrackValid() const
  {
    MutexAutoLock lock(mMutex);
    return mTrackInfo->IsValid();
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

  // Mutex to protect members below across multiple threads.
  mutable Mutex mMutex;
  UniquePtr<TrackInfo> mTrackInfo;
};

} // namespace mozilla

#endif

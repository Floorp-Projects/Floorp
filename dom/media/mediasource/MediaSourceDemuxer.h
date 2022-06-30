/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaSourceDemuxer_h_)
#  define MediaSourceDemuxer_h_

#  include "MediaDataDemuxer.h"
#  include "MediaResource.h"
#  include "MediaSource.h"
#  include "TrackBuffersManager.h"
#  include "mozilla/Atomics.h"
#  include "mozilla/Maybe.h"
#  include "mozilla/Monitor.h"
#  include "mozilla/TaskQueue.h"
#  include "mozilla/dom/MediaDebugInfoBinding.h"

namespace mozilla {

class AbstractThread;
class MediaResult;
class MediaSourceTrackDemuxer;

DDLoggedTypeDeclNameAndBase(MediaSourceDemuxer, MediaDataDemuxer);
DDLoggedTypeNameAndBase(MediaSourceTrackDemuxer, MediaTrackDemuxer);

class MediaSourceDemuxer : public MediaDataDemuxer,
                           public DecoderDoctorLifeLogger<MediaSourceDemuxer> {
 public:
  explicit MediaSourceDemuxer(AbstractThread* aAbstractMainThread);

  RefPtr<InitPromise> Init() override;

  uint32_t GetNumberTracks(TrackInfo::TrackType aType) const override;

  already_AddRefed<MediaTrackDemuxer> GetTrackDemuxer(
      TrackInfo::TrackType aType, uint32_t aTrackNumber) override;

  bool IsSeekable() const override;

  UniquePtr<EncryptionInfo> GetCrypto() override;

  bool ShouldComputeStartTime() const override { return false; }

  /* interface for TrackBuffersManager */
  void AttachSourceBuffer(RefPtr<TrackBuffersManager>& aSourceBuffer);
  void DetachSourceBuffer(RefPtr<TrackBuffersManager>& aSourceBuffer);
  TaskQueue* GetTaskQueue() { return mTaskQueue; }
  void NotifyInitDataArrived();

  // Populates aInfo with info describing the state of the MediaSource internal
  // buffered data. Used for debugging purposes.
  // aInfo should *not* be accessed until the returned promise has been resolved
  // or rejected.
  RefPtr<GenericPromise> GetDebugInfo(
      dom::MediaSourceDemuxerDebugInfo& aInfo) const;

  void AddSizeOfResources(MediaSourceDecoder::ResourceSizes* aSizes);

  // Gap allowed between frames.
  // Due to inaccuracies in determining buffer end
  // frames (Bug 1065207). This value is based on videos seen in the wild.
  static constexpr media::TimeUnit EOS_FUZZ =
      media::TimeUnit::FromMicroseconds(500000);

 private:
  ~MediaSourceDemuxer();
  friend class MediaSourceTrackDemuxer;
  // Scan source buffers and update information.
  bool ScanSourceBuffersForContent();
  RefPtr<TrackBuffersManager> GetManager(TrackInfo::TrackType aType);
  TrackInfo* GetTrackInfo(TrackInfo::TrackType);
  void DoAttachSourceBuffer(RefPtr<TrackBuffersManager>&& aSourceBuffer);
  void DoDetachSourceBuffer(RefPtr<TrackBuffersManager>&& aSourceBuffer);
  bool OnTaskQueue() {
    return !GetTaskQueue() || GetTaskQueue()->IsCurrentThreadIn();
  }

  RefPtr<TaskQueue> mTaskQueue;
  nsTArray<RefPtr<MediaSourceTrackDemuxer>> mDemuxers;

  nsTArray<RefPtr<TrackBuffersManager>> mSourceBuffers;

  MozPromiseHolder<InitPromise> mInitPromise;

  // Monitor to protect members below across multiple threads.
  mutable Monitor mMonitor MOZ_UNANNOTATED;
  RefPtr<TrackBuffersManager> mAudioTrack;
  RefPtr<TrackBuffersManager> mVideoTrack;
  MediaInfo mInfo;
};

class MediaSourceTrackDemuxer
    : public MediaTrackDemuxer,
      public DecoderDoctorLifeLogger<MediaSourceTrackDemuxer> {
 public:
  MediaSourceTrackDemuxer(MediaSourceDemuxer* aParent,
                          TrackInfo::TrackType aType,
                          TrackBuffersManager* aManager);

  UniquePtr<TrackInfo> GetInfo() const override;

  RefPtr<SeekPromise> Seek(const media::TimeUnit& aTime) override;

  RefPtr<SamplesPromise> GetSamples(int32_t aNumSamples = 1) override;

  void Reset() override;

  nsresult GetNextRandomAccessPoint(media::TimeUnit* aTime) override;

  RefPtr<SkipAccessPointPromise> SkipToNextRandomAccessPoint(
      const media::TimeUnit& aTimeThreshold) override;

  media::TimeIntervals GetBuffered() override;

  void BreakCycles() override;

  bool GetSamplesMayBlock() const override { return false; }

  bool HasManager(TrackBuffersManager* aManager) const;
  void DetachManager();

 private:
  bool OnTaskQueue() const { return mTaskQueue->IsCurrentThreadIn(); }

  RefPtr<SeekPromise> DoSeek(const media::TimeUnit& aTime);
  RefPtr<SamplesPromise> DoGetSamples(int32_t aNumSamples);
  RefPtr<SkipAccessPointPromise> DoSkipToNextRandomAccessPoint(
      const media::TimeUnit& aTimeThreadshold);
  already_AddRefed<MediaRawData> GetSample(MediaResult& aError);
  // Return the timestamp of the next keyframe after mLastSampleIndex.
  media::TimeUnit GetNextRandomAccessPoint();

  RefPtr<MediaSourceDemuxer> mParent;
  const RefPtr<TaskQueue> mTaskQueue;

  TrackInfo::TrackType mType;
  // Monitor protecting members below accessed from multiple threads.
  Monitor mMonitor MOZ_UNANNOTATED;
  media::TimeUnit mNextRandomAccessPoint;
  // Would be accessed in MFR's demuxer proxy task queue and TaskQueue, and
  // only be set on the TaskQueue. It can be accessed while on TaskQueue without
  // the need for the lock.
  RefPtr<TrackBuffersManager> mManager;

  // Only accessed on TaskQueue
  Maybe<RefPtr<MediaRawData>> mNextSample;
  // Set to true following a reset. Ensure that the next sample demuxed
  // is available at position 0.
  // Only accessed on TaskQueue
  bool mReset;

  // Amount of pre-roll time when seeking.
  // Set to 80ms if track is Opus.
  const media::TimeUnit mPreRoll;
};

}  // namespace mozilla

#endif

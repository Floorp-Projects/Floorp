/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MP4Demuxer_h_)
#define MP4Demuxer_h_

#include "mozilla/Maybe.h"
#include "mozilla/Monitor.h"
#include "MediaDataDemuxer.h"
#include "MediaResource.h"

namespace mp4_demuxer {
class Index;
class MP4Metadata;
class ResourceStream;
class SampleIterator;
} // namespace mp4_demuxer

namespace mozilla {

class MP4TrackDemuxer;

class MP4Demuxer : public MediaDataDemuxer
{
public:
  explicit MP4Demuxer(MediaResource* aResource);

  virtual nsRefPtr<InitPromise> Init() override;

  virtual already_AddRefed<MediaDataDemuxer> Clone() const override;

  virtual bool HasTrackType(TrackInfo::TrackType aType) const override;

  virtual uint32_t GetNumberTracks(TrackInfo::TrackType aType) const override;

  virtual already_AddRefed<MediaTrackDemuxer> GetTrackDemuxer(TrackInfo::TrackType aType,
                                                              uint32_t aTrackNumber) override;

  virtual bool IsSeekable() const override;

  virtual UniquePtr<EncryptionInfo> GetCrypto() override;

  virtual void NotifyDataArrived(uint32_t aLength, int64_t aOffset) override;

  virtual void NotifyDataRemoved() override;

private:
  friend class MP4TrackDemuxer;
  nsRefPtr<MediaResource> mResource;
  nsRefPtr<mp4_demuxer::ResourceStream> mStream;
  nsRefPtr<MediaByteBuffer> mInitData;
  UniquePtr<mp4_demuxer::MP4Metadata> mMetadata;
  nsTArray<nsRefPtr<MP4TrackDemuxer>> mDemuxers;
};

class MP4TrackDemuxer : public MediaTrackDemuxer
{
public:
  MP4TrackDemuxer(MP4Demuxer* aParent,
                  TrackInfo::TrackType aType,
                  uint32_t aTrackNumber);

  virtual UniquePtr<TrackInfo> GetInfo() const override;

  virtual nsRefPtr<SeekPromise> Seek(media::TimeUnit aTime) override;

  virtual nsRefPtr<SamplesPromise> GetSamples(int32_t aNumSamples = 1) override;

  virtual void Reset() override;

  virtual nsresult GetNextRandomAccessPoint(media::TimeUnit* aTime) override;

  nsRefPtr<SkipAccessPointPromise> SkipToNextRandomAccessPoint(media::TimeUnit aTimeThreshold) override;

  virtual media::TimeIntervals GetBuffered() override;

  virtual int64_t GetEvictionOffset(media::TimeUnit aTime) override;

  virtual void BreakCycles() override;

private:
  friend class MP4Demuxer;
  void NotifyDataArrived();
  void UpdateSamples(nsTArray<nsRefPtr<MediaRawData>>& aSamples);
  void EnsureUpToDateIndex();
  void SetNextKeyFrameTime();
  nsRefPtr<MP4Demuxer> mParent;
  nsRefPtr<mp4_demuxer::Index> mIndex;
  UniquePtr<mp4_demuxer::SampleIterator> mIterator;
  UniquePtr<TrackInfo> mInfo;
  nsRefPtr<mp4_demuxer::ResourceStream> mStream;
  Maybe<media::TimeUnit> mNextKeyframeTime;
  // Queued samples extracted by the demuxer, but not yet returned.
  nsRefPtr<MediaRawData> mQueuedSample;
  bool mNeedReIndex;

  // We do not actually need a monitor, however MoofParser will assert
  // if a monitor isn't held.
  Monitor mMonitor;
};

} // namespace mozilla

#endif

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaDataDemuxer_h)
#define MediaDataDemuxer_h

#include "mozilla/MozPromise.h"
#include "mozilla/UniquePtr.h"

#include "MediaData.h"
#include "MediaInfo.h"
#include "MediaResult.h"
#include "TimeUnits.h"
#include "nsISupportsImpl.h"
#include "mozilla/RefPtr.h"
#include "nsTArray.h"

namespace mozilla {

class MediaTrackDemuxer;
class TrackMetadataHolder;

// Allows reading the media data: to retrieve the metadata and demux samples.
// MediaDataDemuxer isn't designed to be thread safe.
// When used by the MediaFormatDecoder, care is taken to ensure that the demuxer
// will never be called from more than one thread at once.
class MediaDataDemuxer
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDataDemuxer)

  typedef MozPromise<nsresult, MediaResult, /* IsExclusive = */ true> InitPromise;

  // Initializes the demuxer. Other methods cannot be called unless
  // initialization has completed and succeeded.
  // Typically a demuxer will wait to parse the metadata before resolving the
  // promise. The promise must not be resolved until sufficient data is
  // supplied. For example, an incomplete metadata would cause the promise to be
  // rejected should no more data be coming, while the demuxer would wait
  // otherwise.
  virtual RefPtr<InitPromise> Init() = 0;

  // Returns true if a aType track type is available.
  virtual bool HasTrackType(TrackInfo::TrackType aType) const = 0;

  // Returns the number of tracks of aType type available. A value of
  // 0 indicates that no such type is available.
  virtual uint32_t GetNumberTracks(TrackInfo::TrackType aType) const = 0;

  // Returns the MediaTrackDemuxer associated with aTrackNumber aType track.
  // aTrackNumber is not to be confused with the Track ID.
  // aTrackNumber must be constrained between  0 and  GetNumberTracks(aType) - 1
  // The actual Track ID is to be retrieved by calling
  // MediaTrackDemuxer::TrackInfo.
  virtual already_AddRefed<MediaTrackDemuxer> GetTrackDemuxer(TrackInfo::TrackType aType,
                                                              uint32_t aTrackNumber) = 0;

  // Returns true if the underlying resource allows seeking.
  virtual bool IsSeekable() const = 0;

  // Returns true if the underlying resource can only seek within buffered
  // ranges.
  virtual bool IsSeekableOnlyInBufferedRanges() const { return false; }

  // Returns the media's crypto information, or nullptr if media isn't
  // encrypted.
  virtual UniquePtr<EncryptionInfo> GetCrypto()
  {
    return nullptr;
  }

  // Notifies the demuxer that the underlying resource has received more data
  // since the demuxer was initialized.
  // The demuxer can use this mechanism to inform all track demuxers that new
  // data is available and to refresh its buffered range.
  virtual void NotifyDataArrived() { }

  // Notifies the demuxer that the underlying resource has had data removed
  // since the demuxer was initialized.
  // The demuxer can use this mechanism to inform all track demuxers to update
  // its buffered range.
  // This will be called should the demuxer be used with MediaSourceResource.
  virtual void NotifyDataRemoved() { }

  // Indicate to MediaFormatReader if it should compute the start time
  // of the demuxed data. If true (default) the first sample returned will be
  // used as reference time base.
  virtual bool ShouldComputeStartTime() const { return true; }

protected:
  virtual ~MediaDataDemuxer()
  {
  }
};

class MediaTrackDemuxer
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaTrackDemuxer)

  class SamplesHolder {
  public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SamplesHolder)
    nsTArray<RefPtr<MediaRawData>> mSamples;
  private:
    ~SamplesHolder() {}
  };

  class SkipFailureHolder {
  public:
    SkipFailureHolder(const MediaResult& aFailure, uint32_t aSkipped)
      : mFailure(aFailure)
      , mSkipped(aSkipped)
    {}
    MediaResult mFailure;
    uint32_t mSkipped;
  };

  typedef MozPromise<media::TimeUnit, MediaResult, /* IsExclusive = */ true> SeekPromise;
  typedef MozPromise<RefPtr<SamplesHolder>, MediaResult, /* IsExclusive = */ true> SamplesPromise;
  typedef MozPromise<uint32_t, SkipFailureHolder, /* IsExclusive = */ true> SkipAccessPointPromise;

  // Returns the TrackInfo (a.k.a Track Description) for this track.
  // The TrackInfo returned will be:
  // TrackInfo::kVideoTrack -> VideoInfo.
  // TrackInfo::kAudioTrack -> AudioInfo.
  // respectively.
  virtual UniquePtr<TrackInfo> GetInfo() const = 0;

  // Seeks to aTime. Upon success, SeekPromise will be resolved with the
  // actual time seeked to. Typically the random access point time
  virtual RefPtr<SeekPromise> Seek(const media::TimeUnit& aTime) = 0;

  // Returns the next aNumSamples sample(s) available.
  // If only a lesser amount of samples is available, only those will be
  // returned.
  // A aNumSamples value of -1 indicates to return all remaining samples.
  // A video sample is typically made of a single video frame while an audio
  // sample will contains multiple audio frames.
  virtual RefPtr<SamplesPromise> GetSamples(int32_t aNumSamples = 1) = 0;

  // Returns true if a call to GetSamples() may block while waiting on the
  // underlying resource to return the data.
  // This is used by the MediaFormatReader to determine if buffering heuristics
  // should be used.
  virtual bool GetSamplesMayBlock() const
  {
    return true;
  }

  // Cancel all pending actions (Seek, GetSamples) and reset current state
  // All pending promises are to be rejected with CANCEL.
  // The next call to GetSamples would return the first sample available in the
  // track.
  virtual void Reset() = 0;

  // Returns timestamp of next random access point or an error if the demuxer
  // can't report this.
  virtual nsresult GetNextRandomAccessPoint(media::TimeUnit* aTime)
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Returns timestamp of previous random access point or an error if the
  // demuxer can't report this.
  virtual nsresult GetPreviousRandomAccessPoint(media::TimeUnit* aTime)
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Skip frames until the next Random Access Point located after
  // aTimeThreshold.
  // The first frame returned by the next call to GetSamples() will be the
  // first random access point found after aTimeThreshold.
  // Upon success, returns the number of frames skipped.
  virtual RefPtr<SkipAccessPointPromise>
  SkipToNextRandomAccessPoint(const media::TimeUnit& aTimeThreshold) = 0;

  // Gets the resource's offset used for the last Seek() or GetSample().
  // A negative value indicates that this functionality isn't supported.
  virtual int64_t GetResourceOffset() const
  {
    return -1;
  }

  virtual TrackInfo::TrackType GetType() const
  {
    return GetInfo()->GetType();
  }

  virtual media::TimeIntervals GetBuffered() = 0;

  // By default, it is assumed that the entire resource can be evicted once
  // all samples have been demuxed.
  virtual int64_t GetEvictionOffset(const media::TimeUnit& aTime)
  {
    return INT64_MAX;
  }

  // If the MediaTrackDemuxer and MediaDataDemuxer hold cross references.
  // BreakCycles must be overridden.
  virtual void BreakCycles()
  {
  }

protected:
  virtual ~MediaTrackDemuxer() {}
};

} // namespace mozilla

#endif // MediaDataDemuxer_h

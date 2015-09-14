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
#include "TimeUnits.h"
#include "nsISupportsImpl.h"
#include "mozilla/nsRefPtr.h"
#include "nsTArray.h"

namespace mozilla {

class MediaTrackDemuxer;
class TrackMetadataHolder;

enum class DemuxerFailureReason : int8_t
{
  WAITING_FOR_DATA,
  END_OF_STREAM,
  DEMUXER_ERROR,
  CANCELED,
  SHUTDOWN,
};


// Allows reading the media data: to retrieve the metadata and demux samples.
// MediaDataDemuxer isn't designed to be thread safe.
// When used by the MediaFormatDecoder, care is taken to ensure that the demuxer
// will never be called from more than one thread at once.
class MediaDataDemuxer
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDataDemuxer)

  typedef MozPromise<nsresult, DemuxerFailureReason, /* IsExclusive = */ true> InitPromise;

  // Initializes the demuxer. Other methods cannot be called unless
  // initialization has completed and succeeded.
  // Typically a demuxer will wait to parse the metadata before resolving the
  // promise. The promise will be rejected with WAITING_FOR_DATA should
  // insufficient data be available at the time. Init() would have to be called
  // again to retry once more data has been received.
  virtual nsRefPtr<InitPromise> Init() = 0;

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

  // Returns the media's crypto information, or nullptr if media isn't
  // encrypted.
  virtual UniquePtr<EncryptionInfo> GetCrypto()
  {
    return nullptr;
  }

  // Notifies the demuxer that the underlying resource has received more data.
  // The demuxer can use this mechanism to inform all track demuxers that new
  // data is available.
  virtual void NotifyDataArrived(uint32_t aLength, int64_t aOffset) { }

  // Notifies the demuxer that the underlying resource has had data removed.
  // The demuxer can use this mechanism to inform all track demuxers to update
  // its TimeIntervals.
  // This will be called should the demuxer be used with MediaSource.
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
    nsTArray<nsRefPtr<MediaRawData>> mSamples;
  private:
    ~SamplesHolder() {}
  };

  class SkipFailureHolder {
  public:
    SkipFailureHolder(DemuxerFailureReason aFailure, uint32_t aSkipped)
      : mFailure(aFailure)
      , mSkipped(aSkipped)
    {}
    DemuxerFailureReason mFailure;
    uint32_t mSkipped;
  };

  typedef MozPromise<media::TimeUnit, DemuxerFailureReason, /* IsExclusive = */ true> SeekPromise;
  typedef MozPromise<nsRefPtr<SamplesHolder>, DemuxerFailureReason, /* IsExclusive = */ true> SamplesPromise;
  typedef MozPromise<uint32_t, SkipFailureHolder, /* IsExclusive = */ true> SkipAccessPointPromise;

  // Returns the TrackInfo (a.k.a Track Description) for this track.
  // The TrackInfo returned will be:
  // TrackInfo::kVideoTrack -> VideoInfo.
  // TrackInfo::kAudioTrack -> AudioInfo.
  // respectively.
  virtual UniquePtr<TrackInfo> GetInfo() const = 0;

  // Seeks to aTime. Upon success, SeekPromise will be resolved with the
  // actual time seeked to. Typically the random access point time
  virtual nsRefPtr<SeekPromise> Seek(media::TimeUnit aTime) = 0;

  // Returns the next aNumSamples sample(s) available.
  // If only a lesser amount of samples is available, only those will be
  // returned.
  // A aNumSamples value of -1 indicates to return all remaining samples.
  // A video sample is typically made of a single video frame while an audio
  // sample will contains multiple audio frames.
  virtual nsRefPtr<SamplesPromise> GetSamples(int32_t aNumSamples = 1) = 0;

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
  virtual nsRefPtr<SkipAccessPointPromise> SkipToNextRandomAccessPoint(media::TimeUnit aTimeThreshold) = 0;

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

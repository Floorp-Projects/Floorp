/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(OggDemuxer_h_)
#define OggDemuxer_h_

#include "nsTArray.h"
#include "MediaDataDemuxer.h"
#include "OggCodecState.h"
#include "OggCodecStore.h"
#include "MediaMetadataManager.h"

namespace mozilla {

class OggTrackDemuxer;
class OggHeaders;

class OggDemuxer : public MediaDataDemuxer
{
public:
  explicit OggDemuxer(MediaResource* aResource);

  RefPtr<InitPromise> Init() override;

  bool HasTrackType(TrackInfo::TrackType aType) const override;

  uint32_t GetNumberTracks(TrackInfo::TrackType aType) const override;

  already_AddRefed<MediaTrackDemuxer> GetTrackDemuxer(TrackInfo::TrackType aType,
                                                      uint32_t aTrackNumber) override;

  bool IsSeekable() const override;

  UniquePtr<EncryptionInfo> GetCrypto() override;

  // Set the events to notify when chaining is encountered.
  void SetChainingEvents(TimedMetadataEventProducer* aMetadataEvent,
                         MediaEventProducer<void>* aOnSeekableEvent);

private:

  // helpers for friend OggTrackDemuxer
  UniquePtr<TrackInfo> GetTrackInfo(TrackInfo::TrackType aType, size_t aTrackNumber) const;

  struct nsAutoOggSyncState {
    nsAutoOggSyncState() {
      ogg_sync_init(&mState);
    }
    ~nsAutoOggSyncState() {
      ogg_sync_clear(&mState);
    }
    ogg_sync_state mState;
  };
  media::TimeIntervals GetBuffered(TrackInfo::TrackType aType);
  void FindStartTime(int64_t& aOutStartTime);
  void FindStartTime(TrackInfo::TrackType, int64_t& aOutStartTime);

  nsresult SeekInternal(TrackInfo::TrackType aType, const media::TimeUnit& aTarget);

  // Seeks to the keyframe preceding the target time using available
  // keyframe indexes.
  enum IndexedSeekResult
  {
    SEEK_OK,          // Success.
    SEEK_INDEX_FAIL,  // Failure due to no index, or invalid index.
    SEEK_FATAL_ERROR  // Error returned by a stream operation.
  };
  IndexedSeekResult SeekToKeyframeUsingIndex(TrackInfo::TrackType aType, int64_t aTarget);

  // Rolls back a seek-using-index attempt, returning a failure error code.
  IndexedSeekResult RollbackIndexedSeek(TrackInfo::TrackType aType, int64_t aOffset);

  // Represents a section of contiguous media, with a start and end offset,
  // and the timestamps of the start and end of that range, that is cached.
  // Used to denote the extremities of a range in which we can seek quickly
  // (because it's cached).
  class SeekRange
  {
  public:
    SeekRange()
      : mOffsetStart(0)
      , mOffsetEnd(0)
      , mTimeStart(0)
      , mTimeEnd(0)
    {}

    SeekRange(int64_t aOffsetStart,
              int64_t aOffsetEnd,
              int64_t aTimeStart,
              int64_t aTimeEnd)
      : mOffsetStart(aOffsetStart)
      , mOffsetEnd(aOffsetEnd)
      , mTimeStart(aTimeStart)
      , mTimeEnd(aTimeEnd)
    {}

    bool IsNull() const {
      return mOffsetStart == 0 &&
             mOffsetEnd == 0 &&
             mTimeStart == 0 &&
             mTimeEnd == 0;
    }

    int64_t mOffsetStart, mOffsetEnd; // in bytes.
    int64_t mTimeStart, mTimeEnd; // in usecs.
  };

  nsresult GetSeekRanges(TrackInfo::TrackType aType, nsTArray<SeekRange>& aRanges);
  SeekRange SelectSeekRange(TrackInfo::TrackType aType,
                            const nsTArray<SeekRange>& ranges,
                            int64_t aTarget,
                            int64_t aStartTime,
                            int64_t aEndTime,
                            bool aExact);

  // Seeks to aTarget usecs in the buffered range aRange using bisection search,
  // or to the keyframe prior to aTarget if we have video. aAdjustedTarget is
  // an adjusted version of the target used to account for Opus pre-roll, if
  // necessary. aStartTime must be the presentation time at the start of media,
  // and aEndTime the time at end of media. aRanges must be the time/byte ranges
  // buffered in the media cache as per GetSeekRanges().
  nsresult SeekInBufferedRange(TrackInfo::TrackType aType,
                               int64_t aTarget,
                               int64_t aAdjustedTarget,
                               int64_t aStartTime,
                               int64_t aEndTime,
                               const nsTArray<SeekRange>& aRanges,
                               const SeekRange& aRange);

  // Seeks to before aTarget usecs in media using bisection search. If the media
  // has video, this will seek to before the keyframe required to render the
  // media at aTarget. Will use aRanges in order to narrow the bisection
  // search space. aStartTime must be the presentation time at the start of
  // media, and aEndTime the time at end of media. aRanges must be the time/byte
  // ranges buffered in the media cache as per GetSeekRanges().
  nsresult SeekInUnbuffered(TrackInfo::TrackType aType,
                            int64_t aTarget,
                            int64_t aStartTime,
                            int64_t aEndTime,
                            const nsTArray<SeekRange>& aRanges);

  // Performs a seek bisection to move the media stream's read cursor to the
  // last ogg page boundary which has end time before aTarget usecs on both the
  // Theora and Vorbis bitstreams. Limits its search to data inside aRange;
  // i.e. it will only read inside of the aRange's start and end offsets.
  // aFuzz is the number of usecs of leniency we'll allow; we'll terminate the
  // seek when we land in the range (aTime - aFuzz, aTime) usecs.
  nsresult SeekBisection(TrackInfo::TrackType aType,
                         int64_t aTarget,
                         const SeekRange& aRange,
                         uint32_t aFuzz);

  // Chunk size to read when reading Ogg files. Average Ogg page length
  // is about 4300 bytes, so we read the file in chunks larger than that.
  static const int PAGE_STEP = 8192;

  enum PageSyncResult
  {
    PAGE_SYNC_ERROR = 1,
    PAGE_SYNC_END_OF_RANGE= 2,
    PAGE_SYNC_OK = 3
  };
  static PageSyncResult PageSync(MediaResourceIndex* aResource,
                                 ogg_sync_state* aState,
                                 bool aCachedDataOnly,
                                 int64_t aOffset,
                                 int64_t aEndOffset,
                                 ogg_page* aPage,
                                 int& aSkippedBytes);

  // Demux next Ogg packet
  ogg_packet* GetNextPacket(TrackInfo::TrackType aType);

  nsresult Reset(TrackInfo::TrackType aType);

  static const nsString GetKind(const nsCString& aRole);
  static void InitTrack(MessageField* aMsgInfo,
                      TrackInfo* aInfo,
                      bool aEnable);

  // Really private!
  ~OggDemuxer();

  // Read enough of the file to identify track information and header
  // packets necessary for decoding to begin.
  nsresult ReadMetadata();

  // Read a page of data from the Ogg file. Returns true if a page has been
  // read, false if the page read failed or end of file reached.
  bool ReadOggPage(TrackInfo::TrackType aType, ogg_page* aPage);

  // Send a page off to the individual streams it belongs to.
  // Reconstructed packets, if any are ready, will be available
  // on the individual OggCodecStates.
  nsresult DemuxOggPage(TrackInfo::TrackType aType, ogg_page* aPage);

  // Read data and demux until a packet is available on the given stream state
  void DemuxUntilPacketAvailable(TrackInfo::TrackType aType, OggCodecState* aState);

  // Reads and decodes header packets for aState, until either header decode
  // fails, or is complete. Initializes the codec state before returning.
  // Returns true if reading headers and initializtion of the stream
  // succeeds.
  bool ReadHeaders(TrackInfo::TrackType aType, OggCodecState* aState, OggHeaders& aHeaders);

  // Reads the next link in the chain.
  bool ReadOggChain(const media::TimeUnit& aLastEndTime);

  // Set this media as being a chain and notifies the state machine that the
  // media is no longer seekable.
  void SetChained();

  // Fills aTracks with the serial numbers of each active stream, for use by
  // various SkeletonState functions.
  void BuildSerialList(nsTArray<uint32_t>& aTracks);

  // Setup target bitstreams for decoding.
  void SetupTargetTheora(TheoraState* aTheoraState, OggHeaders& aHeaders);
  void SetupTargetVorbis(VorbisState* aVorbisState, OggHeaders& aHeaders);
  void SetupTargetOpus(OpusState* aOpusState, OggHeaders& aHeaders);
  void SetupTargetFlac(FlacState* aFlacState, OggHeaders& aHeaders);
  void SetupTargetSkeleton();
  void SetupMediaTracksInfo(const nsTArray<uint32_t>& aSerials);
  void FillTags(TrackInfo* aInfo, MetadataTags* aTags);

  // Compute an ogg page's checksum
  ogg_uint32_t GetPageChecksum(ogg_page* aPage);

  // Get the end time of aEndOffset. This is the playback position we'd reach
  // after playback finished at aEndOffset.
  int64_t RangeEndTime(TrackInfo::TrackType aType, int64_t aEndOffset);

  // Get the end time of aEndOffset, without reading before aStartOffset.
  // This is the playback position we'd reach after playback finished at
  // aEndOffset. If bool aCachedDataOnly is true, then we'll only read
  // from data which is cached in the media cached, otherwise we'll do
  // regular blocking reads from the media stream. If bool aCachedDataOnly
  // is true, this can safely be called on the main thread, otherwise it
  // must be called on the state machine thread.
  int64_t RangeEndTime(TrackInfo::TrackType aType,
                       int64_t aStartOffset,
                       int64_t aEndOffset,
                       bool aCachedDataOnly);

  // Get the start time of the range beginning at aOffset. This is the start
  // time of the first aType sample we'd be able to play if we
  // started playback at aOffset.
  int64_t RangeStartTime(TrackInfo::TrackType aType, int64_t aOffset);

  MediaInfo mInfo;
  nsTArray<RefPtr<OggTrackDemuxer>> mDemuxers;

  // Map of codec-specific bitstream states.
  OggCodecStore mCodecStore;

  // Decode state of the Theora bitstream we're decoding, if we have video.
  TheoraState* mTheoraState;

  // Decode state of the Vorbis bitstream we're decoding, if we have audio.
  VorbisState* mVorbisState;

  // Decode state of the Opus bitstream we're decoding, if we have one.
  OpusState* mOpusState;

  // Get the bitstream decode state for the given track type
  // Decode state of the Flac bitstream we're decoding, if we have one.
  FlacState* mFlacState;

  OggCodecState* GetTrackCodecState(TrackInfo::TrackType aType) const;
  TrackInfo::TrackType GetCodecStateType(OggCodecState* aState) const;

  // Represents the user pref media.opus.enabled at the time our
  // contructor was called. We can't check it dynamically because
  // we're not on the main thread;
  bool mOpusEnabled;

  // Decode state of the Skeleton bitstream.
  SkeletonState* mSkeletonState;

  // Ogg decoding state.
  struct OggStateContext
  {
    explicit OggStateContext(MediaResource* aResource)
    : mResource(aResource), mNeedKeyframe(true) {}
    nsAutoOggSyncState mOggState;
    MediaResourceIndex mResource;
    Maybe<media::TimeUnit> mStartTime;
    bool mNeedKeyframe;
  };

  OggStateContext& OggState(TrackInfo::TrackType aType);
  ogg_sync_state* OggSyncState(TrackInfo::TrackType aType);
  MediaResourceIndex* Resource(TrackInfo::TrackType aType);
  MediaResourceIndex* CommonResource();
  OggStateContext mAudioOggState;
  OggStateContext mVideoOggState;

  // Vorbis/Opus/Theora data used to compute timestamps. This is written on the
  // decoder thread and read on the main thread. All reading on the main
  // thread must be done after metadataloaded. We can't use the existing
  // data in the codec states due to threading issues. You must check the
  // associated mTheoraState or mVorbisState pointer is non-null before
  // using this codec data.
  uint32_t mVorbisSerial;
  uint32_t mOpusSerial;
  uint32_t mTheoraSerial;
  uint32_t mFlacSerial;

  vorbis_info mVorbisInfo;
  int mOpusPreSkip;
  th_info mTheoraInfo;

  Maybe<int64_t> mStartTime;

  // Booleans to indicate if we have audio and/or video data
  bool HasVideo() const;
  bool HasAudio() const;
  bool HasSkeleton() const
  {
    return mSkeletonState != 0 && mSkeletonState->mActive;
  }
  bool HaveStartTime () const;
  bool HaveStartTime (TrackInfo::TrackType aType);
  int64_t StartTime() const;
  int64_t StartTime(TrackInfo::TrackType aType);

  // The picture region inside Theora frame to be displayed, if we have
  // a Theora video track.
  nsIntRect mPicture;

  // True if we are decoding a chained ogg.
  bool mIsChained;

  // Total audio duration played so far.
  media::TimeUnit mDecodedAudioDuration;

  // Events manager
  TimedMetadataEventProducer* mTimedMetadataEvent;
  MediaEventProducer<void>* mOnSeekableEvent;

  // This will be populated only if a content change occurs, otherwise it
  // will be left as null so the original metadata is used.
  // It is updated once a chained ogg is encountered.
  // As Ogg chaining is only supported for audio, we only need an audio track
  // info.
  RefPtr<SharedTrackInfo> mSharedAudioTrackInfo;

  friend class OggTrackDemuxer;
};

class OggTrackDemuxer : public MediaTrackDemuxer
{
public:
  OggTrackDemuxer(OggDemuxer* aParent,
                  TrackInfo::TrackType aType,
                  uint32_t aTrackNumber);

  UniquePtr<TrackInfo> GetInfo() const override;

  RefPtr<SeekPromise> Seek(const media::TimeUnit& aTime) override;

  RefPtr<SamplesPromise> GetSamples(int32_t aNumSamples = 1) override;

  void Reset() override;

  RefPtr<SkipAccessPointPromise> SkipToNextRandomAccessPoint(const media::TimeUnit& aTimeThreshold) override;

  media::TimeIntervals GetBuffered() override;

  void BreakCycles() override;

private:
  ~OggTrackDemuxer();
  void SetNextKeyFrameTime();
  RefPtr<MediaRawData> NextSample();
  RefPtr<OggDemuxer> mParent;
  TrackInfo::TrackType mType;
  UniquePtr<TrackInfo> mInfo;

  // Queued sample extracted by the demuxer, but not yet returned.
  RefPtr<MediaRawData> mQueuedSample;
};
} // namespace mozilla

#endif

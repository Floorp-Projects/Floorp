 /* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"
#include "MediaDecoderStateMachine.h"
#include "AbstractMediaDecoder.h"
#include "OggDemuxer.h"
#include "OggCodecState.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Atomics.h"
#include "mozilla/PodOperations.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "MediaDataDemuxer.h"
#include "nsAutoRef.h"
#include "XiphExtradata.h"
#include "MediaPrefs.h"

#include <algorithm>

extern mozilla::LazyLogModule gMediaDemuxerLog;
#define OGG_DEBUG(arg, ...) MOZ_LOG(gMediaDemuxerLog, mozilla::LogLevel::Debug, ("OggDemuxer(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

// Un-comment to enable logging of seek bisections.
//#define SEEK_LOGGING
#ifdef SEEK_LOGGING
#define SEEK_LOG(type, msg) MOZ_LOG(gMediaDemuxerLog, type, msg)
#else
#define SEEK_LOG(type, msg)
#endif

namespace mozilla
{

using media::TimeUnit;
using media::TimeInterval;
using media::TimeIntervals;

// The number of microseconds of "fuzz" we use in a bisection search over
// HTTP. When we're seeking with fuzz, we'll stop the search if a bisection
// lands between the seek target and OGG_SEEK_FUZZ_USECS microseconds before the
// seek target.  This is becaue it's usually quicker to just keep downloading
// from an exisiting connection than to do another bisection inside that
// small range, which would open a new HTTP connetion.
static const uint32_t OGG_SEEK_FUZZ_USECS = 500000;

// The number of microseconds of "pre-roll" we use for Opus streams.
// The specification recommends 80 ms.
static const int64_t OGG_SEEK_OPUS_PREROLL = 80 * USECS_PER_MS;

static Atomic<uint32_t> sStreamSourceID(0u);

// Return the corresponding category in aKind based on the following specs.
// (https://www.whatwg.org/specs/web-apps/current-
// work/multipage/embedded-content.html#dom-audiotrack-kind) &
// (http://wiki.xiph.org/SkeletonHeaders)
const nsString
OggDemuxer::GetKind(const nsCString& aRole)
{
  if (aRole.Find("audio/main") != -1 || aRole.Find("video/main") != -1) {
    return NS_LITERAL_STRING("main");
  } else if (aRole.Find("audio/alternate") != -1 ||
             aRole.Find("video/alternate") != -1) {
    return NS_LITERAL_STRING("alternative");
  } else if (aRole.Find("audio/audiodesc") != -1) {
    return NS_LITERAL_STRING("descriptions");
  } else if (aRole.Find("audio/described") != -1) {
    return NS_LITERAL_STRING("main-desc");
  } else if (aRole.Find("audio/dub") != -1) {
    return NS_LITERAL_STRING("translation");
  } else if (aRole.Find("audio/commentary") != -1) {
    return NS_LITERAL_STRING("commentary");
  } else if (aRole.Find("video/sign") != -1) {
    return NS_LITERAL_STRING("sign");
  } else if (aRole.Find("video/captioned") != -1) {
    return NS_LITERAL_STRING("captions");
  } else if (aRole.Find("video/subtitled") != -1) {
    return NS_LITERAL_STRING("subtitles");
  }
  return EmptyString();
}

void
OggDemuxer::InitTrack(MessageField* aMsgInfo,
                      TrackInfo* aInfo,
                      bool aEnable)
{
  MOZ_ASSERT(aMsgInfo);
  MOZ_ASSERT(aInfo);

  nsCString* sName = aMsgInfo->mValuesStore.Get(eName);
  nsCString* sRole = aMsgInfo->mValuesStore.Get(eRole);
  nsCString* sTitle = aMsgInfo->mValuesStore.Get(eTitle);
  nsCString* sLanguage = aMsgInfo->mValuesStore.Get(eLanguage);
  aInfo->Init(sName? NS_ConvertUTF8toUTF16(*sName):EmptyString(),
              sRole? GetKind(*sRole):EmptyString(),
              sTitle? NS_ConvertUTF8toUTF16(*sTitle):EmptyString(),
              sLanguage? NS_ConvertUTF8toUTF16(*sLanguage):EmptyString(),
              aEnable);
}

OggDemuxer::OggDemuxer(MediaResource* aResource)
  : mTheoraState(nullptr)
  , mVorbisState(nullptr)
  , mOpusState(nullptr)
  , mFlacState(nullptr)
  , mOpusEnabled(MediaDecoder::IsOpusEnabled())
  , mSkeletonState(nullptr)
  , mAudioOggState(aResource)
  , mVideoOggState(aResource)
  , mIsChained(false)
  , mTimedMetadataEvent(nullptr)
  , mOnSeekableEvent(nullptr)
{
  MOZ_COUNT_CTOR(OggDemuxer);
}

OggDemuxer::~OggDemuxer()
{
  MOZ_COUNT_DTOR(OggDemuxer);
  Reset(TrackInfo::kAudioTrack);
  Reset(TrackInfo::kVideoTrack);
  if (HasAudio() || HasVideo()) {
    // If we were able to initialize our decoders, report whether we encountered
    // a chained stream or not.
    bool isChained = mIsChained;
    void* ptr = this;
    nsCOMPtr<nsIRunnable> task = NS_NewRunnableFunction(
      "OggDemuxer::~OggDemuxer", [ptr, isChained]() -> void {
        // We can't use OGG_DEBUG here because it implicitly refers to `this`,
        // which we can't capture in this runnable.
        MOZ_LOG(gMediaDemuxerLog,
                mozilla::LogLevel::Debug,
                ("OggDemuxer(%p)::%s: Reporting telemetry "
                 "MEDIA_OGG_LOADED_IS_CHAINED=%d",
                 ptr,
                 __func__,
                 isChained));
        Telemetry::Accumulate(
          Telemetry::HistogramID::MEDIA_OGG_LOADED_IS_CHAINED, isChained);
      });
    SystemGroup::Dispatch("~OggDemuxer::report_telemetry",
                          TaskCategory::Other,
                          task.forget());
  }
}

void
OggDemuxer::SetChainingEvents(TimedMetadataEventProducer* aMetadataEvent,
                              MediaEventProducer<void>* aOnSeekableEvent)
{
  mTimedMetadataEvent = aMetadataEvent;
  mOnSeekableEvent = aOnSeekableEvent;
}


bool
OggDemuxer::HasAudio()
const
{
  return mVorbisState || mOpusState || mFlacState;
}

bool
OggDemuxer::HasVideo()
const
{
  return mTheoraState;
}

bool
OggDemuxer::HaveStartTime()
const
{
  return mStartTime.isSome();
}

int64_t
OggDemuxer::StartTime() const
{
  return mStartTime.refOr(0);
}

bool
OggDemuxer::HaveStartTime(TrackInfo::TrackType aType)
{
  return OggState(aType).mStartTime.isSome();
}

int64_t
OggDemuxer::StartTime(TrackInfo::TrackType aType)
{
  return OggState(aType).mStartTime.refOr(TimeUnit::Zero()).ToMicroseconds();
}

RefPtr<OggDemuxer::InitPromise>
OggDemuxer::Init()
{
  int ret = ogg_sync_init(OggSyncState(TrackInfo::kAudioTrack));
  if (ret != 0) {
    return InitPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
  }
  ret = ogg_sync_init(OggSyncState(TrackInfo::kVideoTrack));
  if (ret != 0) {
    return InitPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
  }
  if (ReadMetadata() != NS_OK) {
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_METADATA_ERR, __func__);
  }

  if (!GetNumberTracks(TrackInfo::kAudioTrack) &&
      !GetNumberTracks(TrackInfo::kVideoTrack)) {
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_METADATA_ERR, __func__);
  }

  return InitPromise::CreateAndResolve(NS_OK, __func__);
}

bool
OggDemuxer::HasTrackType(TrackInfo::TrackType aType) const
{
  return !!GetNumberTracks(aType);
}

OggCodecState*
OggDemuxer::GetTrackCodecState(TrackInfo::TrackType aType) const
{
  switch(aType) {
    case TrackInfo::kAudioTrack:
      if (mVorbisState) {
        return mVorbisState;
      } else if (mOpusState) {
        return mOpusState;
      } else {
        return mFlacState;
      }
    case TrackInfo::kVideoTrack:
      return mTheoraState;
    default:
      return 0;
  }
}

TrackInfo::TrackType
OggDemuxer::GetCodecStateType(OggCodecState* aState) const
{
  switch (aState->GetType()) {
    case OggCodecState::TYPE_THEORA:
      return TrackInfo::kVideoTrack;
    case OggCodecState::TYPE_OPUS:
    case OggCodecState::TYPE_VORBIS:
    case OggCodecState::TYPE_FLAC:
      return TrackInfo::kAudioTrack;
    default:
      return TrackInfo::kUndefinedTrack;
  }
}

uint32_t
OggDemuxer::GetNumberTracks(TrackInfo::TrackType aType) const
{
  switch(aType) {
    case TrackInfo::kAudioTrack:
      return HasAudio() ? 1 : 0;
    case TrackInfo::kVideoTrack:
      return HasVideo() ? 1 : 0;
    default:
      return 0;
  }
}

UniquePtr<TrackInfo>
OggDemuxer::GetTrackInfo(TrackInfo::TrackType aType, size_t aTrackNumber) const
{
  switch(aType) {
    case TrackInfo::kAudioTrack:
      return mInfo.mAudio.Clone();
    case TrackInfo::kVideoTrack:
      return mInfo.mVideo.Clone();
    default:
      return nullptr;
  }
}

already_AddRefed<MediaTrackDemuxer>
OggDemuxer::GetTrackDemuxer(TrackInfo::TrackType aType, uint32_t aTrackNumber)
{
  if (GetNumberTracks(aType) <= aTrackNumber) {
    return nullptr;
  }
  RefPtr<OggTrackDemuxer> e = new OggTrackDemuxer(this, aType, aTrackNumber);
  mDemuxers.AppendElement(e);

  return e.forget();
}

nsresult
OggDemuxer::Reset(TrackInfo::TrackType aType)
{
  // Discard any previously buffered packets/pages.
  ogg_sync_reset(OggSyncState(aType));
  OggCodecState* trackState = GetTrackCodecState(aType);
  if (trackState) {
    return trackState->Reset();
  }
  OggState(aType).mNeedKeyframe = true;
  return NS_OK;
}

bool
OggDemuxer::ReadHeaders(TrackInfo::TrackType aType,
                        OggCodecState* aState)
{
  while (!aState->DoneReadingHeaders()) {
    DemuxUntilPacketAvailable(aType, aState);
    OggPacketPtr packet = aState->PacketOut();
    if (!packet) {
      OGG_DEBUG("Ran out of header packets early; deactivating stream %" PRIu32, aState->mSerial);
      aState->Deactivate();
      return false;
    }

    // Local OggCodecState needs to decode headers in order to process
    // packet granulepos -> time mappings, etc.
    if (!aState->DecodeHeader(Move(packet))) {
      OGG_DEBUG("Failed to decode ogg header packet; deactivating stream %" PRIu32, aState->mSerial);
      aState->Deactivate();
      return false;
    }
  }

  return aState->Init();
}

void
OggDemuxer::BuildSerialList(nsTArray<uint32_t>& aTracks)
{
  // Obtaining seek index information for currently active bitstreams.
  if (HasVideo()) {
    aTracks.AppendElement(mTheoraState->mSerial);
  }
  if (HasAudio()) {
    if (mVorbisState) {
      aTracks.AppendElement(mVorbisState->mSerial);
    } else if (mOpusState) {
      aTracks.AppendElement(mOpusState->mSerial);
    }
  }
}

void
OggDemuxer::SetupTarget(OggCodecState** aSavedState, OggCodecState* aNewState)
{
  if (*aSavedState) {
    (*aSavedState)->Reset();
  }

  if (aNewState->GetInfo()->GetAsAudioInfo()) {
    mInfo.mAudio = *aNewState->GetInfo()->GetAsAudioInfo();
  } else {
    mInfo.mVideo = *aNewState->GetInfo()->GetAsVideoInfo();
  }
  *aSavedState = aNewState;
}

void
OggDemuxer::SetupTargetSkeleton()
{
  // Setup skeleton related information after mVorbisState & mTheroState
  // being set (if they exist).
  if (mSkeletonState) {
    if (!HasAudio() && !HasVideo()) {
      // We have a skeleton track, but no audio or video, may as well disable
      // the skeleton, we can't do anything useful with this media.
      OGG_DEBUG("Deactivating skeleton stream %" PRIu32, mSkeletonState->mSerial);
      mSkeletonState->Deactivate();
    } else if (ReadHeaders(TrackInfo::kAudioTrack, mSkeletonState) &&
               mSkeletonState->HasIndex()) {
      // We don't particularly care about which track we are currently using
      // as both MediaResource points to the same content.
      // Extract the duration info out of the index, so we don't need to seek to
      // the end of resource to get it.
      nsTArray<uint32_t> tracks;
      BuildSerialList(tracks);
      int64_t duration = 0;
      if (NS_SUCCEEDED(mSkeletonState->GetDuration(tracks, duration))) {
        OGG_DEBUG("Got duration from Skeleton index %" PRId64, duration);
        mInfo.mMetadataDuration.emplace(TimeUnit::FromMicroseconds(duration));
      }
    }
  }
}

void
OggDemuxer::SetupMediaTracksInfo(const nsTArray<uint32_t>& aSerials)
{
  // For each serial number
  // 1. Retrieve a codecState from mCodecStore by this serial number.
  // 2. Retrieve a message field from mMsgFieldStore by this serial number.
  // 3. For now, skip if the serial number refers to a non-primary bitstream.
  // 4. Setup track and other audio/video related information per different types.
  for (size_t i = 0; i < aSerials.Length(); i++) {
    uint32_t serial = aSerials[i];
    OggCodecState* codecState = mCodecStore.Get(serial);

    MessageField* msgInfo = nullptr;
    if (mSkeletonState) {
      mSkeletonState->mMsgFieldStore.Get(serial, &msgInfo);
    }

    OggCodecState* primeState = nullptr;
    switch (codecState->GetType()) {
      case OggCodecState::TYPE_THEORA:
        primeState = mTheoraState;
        break;
      case OggCodecState::TYPE_VORBIS:
        primeState = mVorbisState;
        break;
      case OggCodecState::TYPE_OPUS:
        primeState = mOpusState;
        break;
      case OggCodecState::TYPE_FLAC:
        primeState = mFlacState;
        break;
      default:
        break;
    }
    if (primeState && primeState == codecState) {
      bool isAudio = primeState->GetInfo()->GetAsAudioInfo();
      if (msgInfo) {
        InitTrack(msgInfo, isAudio ? static_cast<TrackInfo*>(&mInfo.mAudio)
                                   : &mInfo.mVideo,
                  true);
      }
      FillTags(isAudio ? static_cast<TrackInfo*>(&mInfo.mAudio) : &mInfo.mVideo,
               primeState->GetTags());
    }
  }
}

void
OggDemuxer::FillTags(TrackInfo* aInfo, MetadataTags* aTags)
{
  if (!aTags) {
    return;
  }
  nsAutoPtr<MetadataTags> tags(aTags);
  for (auto iter = aTags->Iter(); !iter.Done(); iter.Next()) {
    aInfo->mTags.AppendElement(MetadataTag(iter.Key(), iter.Data()));
  }
}

nsresult
OggDemuxer::ReadMetadata()
{
  OGG_DEBUG("OggDemuxer::ReadMetadata called!");

  // We read packets until all bitstreams have read all their header packets.
  // We record the offset of the first non-header page so that we know
  // what page to seek to when seeking to the media start.

  // @FIXME we have to read all the header packets on all the streams
  // and THEN we can run SetupTarget*
  // @fixme fixme

  TrackInfo::TrackType tracks[2] =
    { TrackInfo::kAudioTrack, TrackInfo::kVideoTrack };

  nsTArray<OggCodecState*> bitstreams;
  nsTArray<uint32_t> serials;

  for (uint32_t i = 0; i < ArrayLength(tracks); i++) {
    ogg_page page;
    bool readAllBOS = false;
    while (!readAllBOS) {
      if (!ReadOggPage(tracks[i], &page)) {
        // Some kind of error...
        OGG_DEBUG("OggDemuxer::ReadOggPage failed? leaving ReadMetadata...");
        return NS_ERROR_FAILURE;
      }

      int serial = ogg_page_serialno(&page);

      if (!ogg_page_bos(&page)) {
        // We've encountered a non Beginning Of Stream page. No more BOS pages
        // can follow in this Ogg segment, so there will be no other bitstreams
        // in the Ogg (unless it's invalid).
        readAllBOS = true;
      } else if (!mCodecStore.Contains(serial)) {
        // We've not encountered a stream with this serial number before. Create
        // an OggCodecState to demux it, and map that to the OggCodecState
        // in mCodecStates.
        OggCodecState* codecState = OggCodecState::Create(&page);
        mCodecStore.Add(serial, codecState);
        bitstreams.AppendElement(codecState);
        serials.AppendElement(serial);
      }
      if (NS_FAILED(DemuxOggPage(tracks[i], &page))) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  // We've read all BOS pages, so we know the streams contained in the media.
  // 1. Find the first encountered Theora/Vorbis/Opus bitstream, and configure
  //    it as the target A/V bitstream.
  // 2. Deactivate the rest of bitstreams for now, until we have MediaInfo
  //    support multiple track infos.
  for (uint32_t i = 0; i < bitstreams.Length(); ++i) {
    OggCodecState* s = bitstreams[i];
    if (s) {
      if (s->GetType() == OggCodecState::TYPE_THEORA &&
          ReadHeaders(TrackInfo::kVideoTrack, s)) {
        if (!mTheoraState) {
          SetupTarget(&mTheoraState, s);
        } else {
          s->Deactivate();
        }
      } else if (s->GetType() == OggCodecState::TYPE_VORBIS &&
                 ReadHeaders(TrackInfo::kAudioTrack, s)) {
        if (!mVorbisState) {
          SetupTarget(&mVorbisState, s);
        } else {
          s->Deactivate();
        }
      } else if (s->GetType() == OggCodecState::TYPE_OPUS &&
                 ReadHeaders(TrackInfo::kAudioTrack, s)) {
        if (mOpusEnabled) {
          if (!mOpusState) {
            SetupTarget(&mOpusState, s);
          } else {
            s->Deactivate();
          }
        } else {
          NS_WARNING("Opus decoding disabled."
                     " See media.opus.enabled in about:config");
        }
      } else if (MediaPrefs::FlacInOgg() &&
                 s->GetType() == OggCodecState::TYPE_FLAC &&
                 ReadHeaders(TrackInfo::kAudioTrack, s)) {
        if (!mFlacState) {
          SetupTarget(&mFlacState, s);
        } else {
          s->Deactivate();
        }
      } else if (s->GetType() == OggCodecState::TYPE_SKELETON && !mSkeletonState) {
        mSkeletonState = static_cast<SkeletonState*>(s);
      } else {
        // Deactivate any non-primary bitstreams.
        s->Deactivate();
      }
    }
  }

  SetupTargetSkeleton();
  SetupMediaTracksInfo(serials);

  if (HasAudio() || HasVideo()) {
    int64_t startTime = -1;
    FindStartTime(startTime);
    if (startTime >= 0) {
      OGG_DEBUG("Detected stream start time %" PRId64, startTime);
      mStartTime.emplace(startTime);
    }

    if (mInfo.mMetadataDuration.isNothing() &&
        Resource(TrackInfo::kAudioTrack)->GetLength() >= 0) {
      // We didn't get a duration from the index or a Content-Duration header.
      // Seek to the end of file to find the end time.
      int64_t length = Resource(TrackInfo::kAudioTrack)->GetLength();

      NS_ASSERTION(length > 0, "Must have a content length to get end time");

      int64_t endTime = RangeEndTime(TrackInfo::kAudioTrack, length);

      if (endTime != -1) {
        mInfo.mUnadjustedMetadataEndTime.emplace(TimeUnit::FromMicroseconds(endTime));
        mInfo.mMetadataDuration.emplace(TimeUnit::FromMicroseconds(endTime - mStartTime.refOr(0)));
        OGG_DEBUG("Got Ogg duration from seeking to end %" PRId64, endTime);
      }
    }
    if (mInfo.mMetadataDuration.isNothing()) {
      mInfo.mMetadataDuration.emplace(TimeUnit::FromInfinity());
    }
    if (HasAudio()) {
      mInfo.mAudio.mDuration = mInfo.mMetadataDuration.ref();
    }
    if (HasVideo()) {
      mInfo.mVideo.mDuration = mInfo.mMetadataDuration.ref();
    }
  } else {
    OGG_DEBUG("no audio or video tracks");
    return NS_ERROR_FAILURE;
  }

  OGG_DEBUG("success?!");
  return NS_OK;
}

void
OggDemuxer::SetChained() {
  {
    if (mIsChained) {
      return;
    }
    mIsChained = true;
  }
  if (mOnSeekableEvent) {
    mOnSeekableEvent->Notify();
  }
}

bool
OggDemuxer::ReadOggChain(const media::TimeUnit& aLastEndTime)
{
  bool chained = false;
  OpusState* newOpusState = nullptr;
  VorbisState* newVorbisState = nullptr;
  FlacState* newFlacState = nullptr;
  nsAutoPtr<MetadataTags> tags;

  if (HasVideo() || HasSkeleton() || !HasAudio()) {
    return false;
  }

  ogg_page page;
  if (!ReadOggPage(TrackInfo::kAudioTrack, &page) || !ogg_page_bos(&page)) {
    // Chaining is only supported for audio only ogg files.
    return false;
  }

  int serial = ogg_page_serialno(&page);
  if (mCodecStore.Contains(serial)) {
    return false;
  }

  nsAutoPtr<OggCodecState> codecState;
  codecState = OggCodecState::Create(&page);
  if (!codecState) {
    return false;
  }

  if (mVorbisState && (codecState->GetType() == OggCodecState::TYPE_VORBIS)) {
    newVorbisState = static_cast<VorbisState*>(codecState.get());
  } else if (mOpusState && (codecState->GetType() == OggCodecState::TYPE_OPUS)) {
    newOpusState = static_cast<OpusState*>(codecState.get());
  } else if (mFlacState && (codecState->GetType() == OggCodecState::TYPE_FLAC)) {
    newFlacState = static_cast<FlacState*>(codecState.get());
  } else {
    return false;
  }

  OggCodecState* state;

  mCodecStore.Add(serial, codecState.forget());
  state = mCodecStore.Get(serial);

  NS_ENSURE_TRUE(state != nullptr, false);

  if (NS_FAILED(state->PageIn(&page))) {
    return false;
  }

  MessageField* msgInfo = nullptr;
  if (mSkeletonState) {
    mSkeletonState->mMsgFieldStore.Get(serial, &msgInfo);
  }

  if ((newVorbisState &&
       ReadHeaders(TrackInfo::kAudioTrack, newVorbisState)) &&
      (mVorbisState->GetInfo()->GetAsAudioInfo()->mRate ==
       newVorbisState->GetInfo()->GetAsAudioInfo()->mRate) &&
      (mVorbisState->GetInfo()->GetAsAudioInfo()->mChannels ==
       newVorbisState->GetInfo()->GetAsAudioInfo()->mChannels)) {

    SetupTarget(&mVorbisState, newVorbisState);
    LOG(LogLevel::Debug,
        ("New vorbis ogg link, serial=%d\n", mVorbisState->mSerial));

    if (msgInfo) {
      InitTrack(msgInfo, &mInfo.mAudio, true);
    }

    chained = true;
    tags = newVorbisState->GetTags();
  }

  if ((newOpusState &&
       ReadHeaders(TrackInfo::kAudioTrack, newOpusState)) &&
      (mOpusState->GetInfo()->GetAsAudioInfo()->mRate ==
       newOpusState->GetInfo()->GetAsAudioInfo()->mRate) &&
      (mOpusState->GetInfo()->GetAsAudioInfo()->mChannels ==
       newOpusState->GetInfo()->GetAsAudioInfo()->mChannels)) {

    SetupTarget(&mOpusState, newOpusState);

    if (msgInfo) {
      InitTrack(msgInfo, &mInfo.mAudio, true);
    }

    chained = true;
    tags = newOpusState->GetTags();
  }

  if ((newFlacState &&
       ReadHeaders(TrackInfo::kAudioTrack, newFlacState)) &&
      (mFlacState->GetInfo()->GetAsAudioInfo()->mRate ==
       newFlacState->GetInfo()->GetAsAudioInfo()->mRate) &&
      (mFlacState->GetInfo()->GetAsAudioInfo()->mChannels ==
       newFlacState->GetInfo()->GetAsAudioInfo()->mChannels)) {

    SetupTarget(&mFlacState, newFlacState);
    LOG(LogLevel::Debug,
        ("New flac ogg link, serial=%d\n", mFlacState->mSerial));

    if (msgInfo) {
      InitTrack(msgInfo, &mInfo.mAudio, true);
    }

    chained = true;
    tags = newFlacState->GetTags();
  }

  if (chained) {
    SetChained();
    mInfo.mMediaSeekable = false;
    mDecodedAudioDuration += aLastEndTime;
    if (mTimedMetadataEvent) {
      mTimedMetadataEvent->Notify(
        TimedMetadata(mDecodedAudioDuration,
                      Move(tags),
                      nsAutoPtr<MediaInfo>(new MediaInfo(mInfo))));
    }
    // Setup a new TrackInfo so that the MediaFormatReader will flush the
    // current decoder.
    mSharedAudioTrackInfo = new TrackInfoSharedPtr(mInfo.mAudio, ++sStreamSourceID);
    return true;
  }

  return false;
}

OggDemuxer::OggStateContext&
OggDemuxer::OggState(TrackInfo::TrackType aType)
{
  if (aType == TrackInfo::kVideoTrack) {
    return mVideoOggState;
  }
  return mAudioOggState;
}

ogg_sync_state*
OggDemuxer::OggSyncState(TrackInfo::TrackType aType)
{
  return &OggState(aType).mOggState.mState;
}

MediaResourceIndex*
OggDemuxer::Resource(TrackInfo::TrackType aType)
{
  return &OggState(aType).mResource;
}

MediaResourceIndex*
OggDemuxer::CommonResource()
{
  return &mAudioOggState.mResource;
}

bool
OggDemuxer::ReadOggPage(TrackInfo::TrackType aType, ogg_page* aPage)
{
  int ret = 0;
  while((ret = ogg_sync_pageseek(OggSyncState(aType), aPage)) <= 0) {
    if (ret < 0) {
      // Lost page sync, have to skip up to next page.
      continue;
    }
    // Returns a buffer that can be written too
    // with the given size. This buffer is stored
    // in the ogg synchronisation structure.
    char* buffer = ogg_sync_buffer(OggSyncState(aType), 4096);
    NS_ASSERTION(buffer, "ogg_sync_buffer failed");

    // Read from the resource into the buffer
    uint32_t bytesRead = 0;

    nsresult rv = Resource(aType)->Read(buffer, 4096, &bytesRead);
    if (NS_FAILED(rv) || !bytesRead) {
      // End of file or error.
      return false;
    }

    // Update the synchronisation layer with the number
    // of bytes written to the buffer
    ret = ogg_sync_wrote(OggSyncState(aType), bytesRead);
    NS_ENSURE_TRUE(ret == 0, false);
  }

  return true;
}

nsresult
OggDemuxer::DemuxOggPage(TrackInfo::TrackType aType, ogg_page* aPage)
{
  int serial = ogg_page_serialno(aPage);
  OggCodecState* codecState = mCodecStore.Get(serial);
  if (codecState == nullptr) {
    OGG_DEBUG("encountered packet for unrecognized codecState");
    return NS_ERROR_FAILURE;
  }
  if (GetCodecStateType(codecState) != aType &&
      codecState->GetType() != OggCodecState::TYPE_SKELETON) {
    // Not a page we're interested in.
    return NS_OK;
  }
  if (NS_FAILED(codecState->PageIn(aPage))) {
    OGG_DEBUG("codecState->PageIn failed");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

bool
OggDemuxer::IsSeekable() const
{
  if (mIsChained) {
    return false;
  }
  return true;
}

UniquePtr<EncryptionInfo>
OggDemuxer::GetCrypto()
{
  return nullptr;
}

ogg_packet*
OggDemuxer::GetNextPacket(TrackInfo::TrackType aType)
{
  OggCodecState* state = GetTrackCodecState(aType);
  ogg_packet* packet = nullptr;
  OggStateContext& context = OggState(aType);

  while (true) {
    if (packet) {
      Unused << state->PacketOut();
    }
    DemuxUntilPacketAvailable(aType, state);

    packet = state->PacketPeek();
    if (!packet) {
      break;
    }
    if (state->IsHeader(packet)) {
      continue;
    }
    if (context.mNeedKeyframe && !state->IsKeyframe(packet)) {
      continue;
    }
    context.mNeedKeyframe = false;
    break;
  }

  return packet;
}

void
OggDemuxer::DemuxUntilPacketAvailable(TrackInfo::TrackType aType,
                                      OggCodecState* aState)
{
  while (!aState->IsPacketReady()) {
    OGG_DEBUG("no packet yet, reading some more");
    ogg_page page;
    if (!ReadOggPage(aType, &page)) {
      OGG_DEBUG("no more pages to read in resource?");
      return;
    }
    DemuxOggPage(aType, &page);
  }
}

TimeIntervals
OggDemuxer::GetBuffered(TrackInfo::TrackType aType)
{
  if (!HaveStartTime(aType)) {
    return TimeIntervals();
  }
  if (mIsChained) {
    return TimeIntervals::Invalid();
  }
  TimeIntervals buffered;
  // HasAudio and HasVideo are not used here as they take a lock and cause
  // a deadlock. Accessing mInfo doesn't require a lock - it doesn't change
  // after metadata is read.
  if (!mInfo.HasValidMedia()) {
    // No need to search through the file if there are no audio or video tracks
    return buffered;
  }

  AutoPinned<MediaResource> resource(Resource(aType)->GetResource());
  MediaByteRangeSet ranges;
  nsresult res = resource->GetCachedRanges(ranges);
  NS_ENSURE_SUCCESS(res, TimeIntervals::Invalid());

  // Traverse across the buffered byte ranges, determining the time ranges
  // they contain. MediaResource::GetNextCachedData(offset) returns -1 when
  // offset is after the end of the media resource, or there's no more cached
  // data after the offset. This loop will run until we've checked every
  // buffered range in the media, in increasing order of offset.
  nsAutoOggSyncState sync;
  for (uint32_t index = 0; index < ranges.Length(); index++) {
    // Ensure the offsets are after the header pages.
    int64_t startOffset = ranges[index].mStart;
    int64_t endOffset = ranges[index].mEnd;

    // Because the granulepos time is actually the end time of the page,
    // we special-case (startOffset == 0) so that the first
    // buffered range always appears to be buffered from the media start
    // time, rather than from the end-time of the first page.
    int64_t startTime = (startOffset == 0) ? StartTime() : -1;

    // Find the start time of the range. Read pages until we find one with a
    // granulepos which we can convert into a timestamp to use as the time of
    // the start of the buffered range.
    ogg_sync_reset(&sync.mState);
    while (startTime == -1) {
      ogg_page page;
      int32_t discard;
      PageSyncResult pageSyncResult = PageSync(Resource(aType),
                                               &sync.mState,
                                               true,
                                               startOffset,
                                               endOffset,
                                               &page,
                                               discard);
      if (pageSyncResult == PAGE_SYNC_ERROR) {
        return TimeIntervals::Invalid();
      } else if (pageSyncResult == PAGE_SYNC_END_OF_RANGE) {
        // Hit the end of range without reading a page, give up trying to
        // find a start time for this buffered range, skip onto the next one.
        break;
      }

      int64_t granulepos = ogg_page_granulepos(&page);
      if (granulepos == -1) {
        // Page doesn't have an end time, advance to the next page
        // until we find one.
        startOffset += page.header_len + page.body_len;
        continue;
      }

      uint32_t serial = ogg_page_serialno(&page);
      if (aType == TrackInfo::kAudioTrack && mVorbisState &&
          serial == mVorbisState->mSerial) {
        startTime = mVorbisState->Time(granulepos);
        NS_ASSERTION(startTime > 0, "Must have positive start time");
      } else if (aType == TrackInfo::kAudioTrack && mOpusState &&
                 serial == mOpusState->mSerial) {
        startTime = mOpusState->Time(granulepos);
        NS_ASSERTION(startTime > 0, "Must have positive start time");
      } else if (aType == TrackInfo::kAudioTrack && mFlacState &&
                 serial == mFlacState->mSerial) {
        startTime = mFlacState->Time(granulepos);
        NS_ASSERTION(startTime > 0, "Must have positive start time");
      } else if (aType == TrackInfo::kVideoTrack && mTheoraState &&
                 serial == mTheoraState->mSerial) {
        startTime = mTheoraState->Time(granulepos);
        NS_ASSERTION(startTime > 0, "Must have positive start time");
      } else if (mCodecStore.Contains(serial)) {
        // Stream is not the theora or vorbis stream we're playing,
        // but is one that we have header data for.
        startOffset += page.header_len + page.body_len;
        continue;
      } else {
        // Page is for a stream we don't know about (possibly a chained
        // ogg), return OK to abort the finding any further ranges. This
        // prevents us searching through the rest of the media when we
        // may not be able to extract timestamps from it.
        SetChained();
        return buffered;
      }
    }

    if (startTime != -1) {
      // We were able to find a start time for that range, see if we can
      // find an end time.
      int64_t endTime = RangeEndTime(aType, startOffset, endOffset, true);
      if (endTime > startTime) {
        buffered += TimeInterval(
           TimeUnit::FromMicroseconds(startTime - StartTime()),
           TimeUnit::FromMicroseconds(endTime - StartTime()));
      }
    }
  }

  return buffered;
}

void
OggDemuxer::FindStartTime(int64_t& aOutStartTime)
{
  // Extract the start times of the bitstreams in order to calculate
  // the duration.
  int64_t videoStartTime = INT64_MAX;
  int64_t audioStartTime = INT64_MAX;

  if (HasVideo()) {
    FindStartTime(TrackInfo::kVideoTrack, videoStartTime);
    if (videoStartTime != INT64_MAX) {
      OGG_DEBUG("OggDemuxer::FindStartTime() video=%" PRId64, videoStartTime);
      mVideoOggState.mStartTime =
        Some(TimeUnit::FromMicroseconds(videoStartTime));
    }
  }
  if (HasAudio()) {
    FindStartTime(TrackInfo::kAudioTrack, audioStartTime);
    if (audioStartTime != INT64_MAX) {
      OGG_DEBUG("OggDemuxer::FindStartTime() audio=%" PRId64, audioStartTime);
      mAudioOggState.mStartTime =
        Some(TimeUnit::FromMicroseconds(audioStartTime));
    }
  }

  int64_t startTime = std::min(videoStartTime, audioStartTime);
  if (startTime != INT64_MAX) {
    aOutStartTime = startTime;
  }
}

void
OggDemuxer::FindStartTime(TrackInfo::TrackType aType, int64_t& aOutStartTime)
{
  int64_t startTime = INT64_MAX;

  OggCodecState* state = GetTrackCodecState(aType);
  ogg_packet* pkt = GetNextPacket(aType);
  if (pkt) {
    startTime = state->PacketStartTime(pkt);
  }

  if (startTime != INT64_MAX) {
    aOutStartTime = startTime;
  }
}

nsresult
OggDemuxer::SeekInternal(TrackInfo::TrackType aType, const TimeUnit& aTarget)
{
  int64_t target = aTarget.ToMicroseconds();
  OGG_DEBUG("About to seek to %" PRId64, target);
  nsresult res;
  int64_t adjustedTarget = target;
  int64_t startTime = StartTime(aType);
  int64_t endTime = mInfo.mMetadataDuration->ToMicroseconds();
  if (aType == TrackInfo::kAudioTrack && mOpusState){
    adjustedTarget = std::max(startTime, target - OGG_SEEK_OPUS_PREROLL);
  }

  if (!HaveStartTime(aType) || adjustedTarget == startTime) {
    // We've seeked to the media start or we can't seek.
    // Just seek to the offset of the first content page.
    res = Resource(aType)->Seek(nsISeekableStream::NS_SEEK_SET, 0);
    NS_ENSURE_SUCCESS(res,res);

    res = Reset(aType);
    NS_ENSURE_SUCCESS(res,res);
  } else {
    // TODO: This may seek back unnecessarily far in the video, but we don't
    // have a way of asking Skeleton to seek to a different target for each
    // stream yet. Using adjustedTarget here is at least correct, if slow.
    IndexedSeekResult sres = SeekToKeyframeUsingIndex(aType, adjustedTarget);
    NS_ENSURE_TRUE(sres != SEEK_FATAL_ERROR, NS_ERROR_FAILURE);
    if (sres == SEEK_INDEX_FAIL) {
      // No index or other non-fatal index-related failure. Try to seek
      // using a bisection search. Determine the already downloaded data
      // in the media cache, so we can try to seek in the cached data first.
      AutoTArray<SeekRange, 16> ranges;
      res = GetSeekRanges(aType, ranges);
      NS_ENSURE_SUCCESS(res,res);

      // Figure out if the seek target lies in a buffered range.
      SeekRange r = SelectSeekRange(aType, ranges, target, startTime, endTime, true);

      if (!r.IsNull()) {
        // We know the buffered range in which the seek target lies, do a
        // bisection search in that buffered range.
        res = SeekInBufferedRange(aType, target, adjustedTarget, startTime, endTime, ranges, r);
        NS_ENSURE_SUCCESS(res,res);
      } else {
        // The target doesn't lie in a buffered range. Perform a bisection
        // search over the whole media, using the known buffered ranges to
        // reduce the search space.
        res = SeekInUnbuffered(aType, target, startTime, endTime, ranges);
        NS_ENSURE_SUCCESS(res,res);
      }
    }
  }

  // Demux forwards until we find the first keyframe prior the target.
  // there may be non-keyframes in the page before the keyframe.
  // Additionally, we may have seeked to the first page referenced by the
  // page index which may be quite far off the target.
  // When doing fastSeek we display the first frame after the seek, so
  // we need to advance the decode to the keyframe otherwise we'll get
  // visual artifacts in the first frame output after the seek.
  OggCodecState* state = GetTrackCodecState(aType);
  OggPacketQueue tempPackets;
  bool foundKeyframe = false;
  while (true) {
    DemuxUntilPacketAvailable(aType, state);
    ogg_packet* packet = state->PacketPeek();
    if (packet == nullptr) {
      OGG_DEBUG("End of stream reached before keyframe found in indexed seek");
      break;
    }
    int64_t startTstamp = state->PacketStartTime(packet);
    if (foundKeyframe && startTstamp > adjustedTarget) {
      break;
    }
    if (state->IsKeyframe(packet)) {
      OGG_DEBUG("keyframe found after seeking at %" PRId64, startTstamp);
      tempPackets.Erase();
      foundKeyframe = true;
    }
    if (foundKeyframe && startTstamp == adjustedTarget) {
      break;
    }
    if (foundKeyframe) {
      tempPackets.Append(state->PacketOut());
    } else {
      // Discard video packets before the first keyframe.
      Unused << state->PacketOut();
    }
  }
  // Re-add all packet into the codec state in order.
  state->PushFront(Move(tempPackets));

  return NS_OK;
}

OggDemuxer::IndexedSeekResult
OggDemuxer::RollbackIndexedSeek(TrackInfo::TrackType aType, int64_t aOffset)
{
  if (mSkeletonState) {
    mSkeletonState->Deactivate();
  }
  nsresult res = Resource(aType)->Seek(nsISeekableStream::NS_SEEK_SET, aOffset);
  NS_ENSURE_SUCCESS(res, SEEK_FATAL_ERROR);
  return SEEK_INDEX_FAIL;
}

OggDemuxer::IndexedSeekResult
OggDemuxer::SeekToKeyframeUsingIndex(TrackInfo::TrackType aType, int64_t aTarget)
{
  if (!HasSkeleton() || !mSkeletonState->HasIndex()) {
    return SEEK_INDEX_FAIL;
  }
  // We have an index from the Skeleton track, try to use it to seek.
  AutoTArray<uint32_t, 2> tracks;
  BuildSerialList(tracks);
  SkeletonState::nsSeekTarget keyframe;
  if (NS_FAILED(mSkeletonState->IndexedSeekTarget(aTarget,
                                                  tracks,
                                                  keyframe))) {
    // Could not locate a keypoint for the target in the index.
    return SEEK_INDEX_FAIL;
  }

  // Remember original resource read cursor position so we can rollback on failure.
  int64_t tell = Resource(aType)->Tell();

  // Seek to the keypoint returned by the index.
  if (keyframe.mKeyPoint.mOffset > Resource(aType)->GetLength() ||
      keyframe.mKeyPoint.mOffset < 0) {
    // Index must be invalid.
    return RollbackIndexedSeek(aType, tell);
  }
  LOG(LogLevel::Debug, ("Seeking using index to keyframe at offset %" PRId64 "\n",
                     keyframe.mKeyPoint.mOffset));
  nsresult res = Resource(aType)->Seek(nsISeekableStream::NS_SEEK_SET,
                                       keyframe.mKeyPoint.mOffset);
  NS_ENSURE_SUCCESS(res, SEEK_FATAL_ERROR);

  // We've moved the read set, so reset decode.
  res = Reset(aType);
  NS_ENSURE_SUCCESS(res, SEEK_FATAL_ERROR);

  // Check that the page the index thinks is exactly here is actually exactly
  // here. If not, the index is invalid.
  ogg_page page;
  int skippedBytes = 0;
  PageSyncResult syncres = PageSync(Resource(aType),
                                    OggSyncState(aType),
                                    false,
                                    keyframe.mKeyPoint.mOffset,
                                    Resource(aType)->GetLength(),
                                    &page,
                                    skippedBytes);
  NS_ENSURE_TRUE(syncres != PAGE_SYNC_ERROR, SEEK_FATAL_ERROR);
  if (syncres != PAGE_SYNC_OK || skippedBytes != 0) {
    LOG(LogLevel::Debug, ("Indexed-seek failure: Ogg Skeleton Index is invalid "
                       "or sync error after seek"));
    return RollbackIndexedSeek(aType, tell);
  }
  uint32_t serial = ogg_page_serialno(&page);
  if (serial != keyframe.mSerial) {
    // Serialno of page at offset isn't what the index told us to expect.
    // Assume the index is invalid.
    return RollbackIndexedSeek(aType, tell);
  }
  OggCodecState* codecState = mCodecStore.Get(serial);
  if (codecState && codecState->mActive &&
      ogg_stream_pagein(&codecState->mState, &page) != 0) {
    // Couldn't insert page into the ogg resource, or somehow the resource
    // is no longer active.
    return RollbackIndexedSeek(aType, tell);
  }
  return SEEK_OK;
}

// Reads a page from the media resource.
OggDemuxer::PageSyncResult
OggDemuxer::PageSync(MediaResourceIndex* aResource,
                     ogg_sync_state* aState,
                     bool aCachedDataOnly,
                     int64_t aOffset,
                     int64_t aEndOffset,
                     ogg_page* aPage,
                     int& aSkippedBytes)
{
  aSkippedBytes = 0;
  // Sync to the next page.
  int ret = 0;
  uint32_t bytesRead = 0;
  int64_t readHead = aOffset;
  while (ret <= 0) {
    ret = ogg_sync_pageseek(aState, aPage);
    if (ret == 0) {
      char* buffer = ogg_sync_buffer(aState, PAGE_STEP);
      NS_ASSERTION(buffer, "Must have a buffer");

      // Read from the file into the buffer
      int64_t bytesToRead = std::min(static_cast<int64_t>(PAGE_STEP),
                                   aEndOffset - readHead);
      NS_ASSERTION(bytesToRead <= UINT32_MAX, "bytesToRead range check");
      if (bytesToRead <= 0) {
        return PAGE_SYNC_END_OF_RANGE;
      }
      nsresult rv = NS_OK;
      if (aCachedDataOnly) {
        rv = aResource->GetResource()->ReadFromCache(buffer, readHead,
                                                     static_cast<uint32_t>(bytesToRead));
        NS_ENSURE_SUCCESS(rv,PAGE_SYNC_ERROR);
        bytesRead = static_cast<uint32_t>(bytesToRead);
      } else {
        rv = aResource->Seek(nsISeekableStream::NS_SEEK_SET, readHead);
        NS_ENSURE_SUCCESS(rv,PAGE_SYNC_ERROR);
        rv = aResource->Read(buffer,
                             static_cast<uint32_t>(bytesToRead),
                             &bytesRead);
        NS_ENSURE_SUCCESS(rv,PAGE_SYNC_ERROR);
      }
      if (bytesRead == 0 && NS_SUCCEEDED(rv)) {
        // End of file.
        return PAGE_SYNC_END_OF_RANGE;
      }
      readHead += bytesRead;

      // Update the synchronisation layer with the number
      // of bytes written to the buffer
      ret = ogg_sync_wrote(aState, bytesRead);
      NS_ENSURE_TRUE(ret == 0, PAGE_SYNC_ERROR);
      continue;
    }

    if (ret < 0) {
      NS_ASSERTION(aSkippedBytes >= 0, "Offset >= 0");
      aSkippedBytes += -ret;
      NS_ASSERTION(aSkippedBytes >= 0, "Offset >= 0");
      continue;
    }
  }

  return PAGE_SYNC_OK;
}

//OggTrackDemuxer
OggTrackDemuxer::OggTrackDemuxer(OggDemuxer* aParent,
                                 TrackInfo::TrackType aType,
                                 uint32_t aTrackNumber)
  : mParent(aParent)
  , mType(aType)
{
  mInfo = mParent->GetTrackInfo(aType, aTrackNumber);
  MOZ_ASSERT(mInfo);
}

OggTrackDemuxer::~OggTrackDemuxer()
{
}

UniquePtr<TrackInfo>
OggTrackDemuxer::GetInfo() const
{
  return mInfo->Clone();
}

RefPtr<OggTrackDemuxer::SeekPromise>
OggTrackDemuxer::Seek(const TimeUnit& aTime)
{
  // Seeks to aTime. Upon success, SeekPromise will be resolved with the
  // actual time seeked to. Typically the random access point time
  mQueuedSample = nullptr;
  TimeUnit seekTime = aTime;
  if (mParent->SeekInternal(mType, aTime) == NS_OK) {
    RefPtr<MediaRawData> sample(NextSample());

    // Check what time we actually seeked to.
    if (sample != nullptr) {
      seekTime = sample->mTime;
      OGG_DEBUG("%p seeked to time %" PRId64, this, seekTime.ToMicroseconds());
    }
    mQueuedSample = sample;

    return SeekPromise::CreateAndResolve(seekTime, __func__);
  } else {
    return SeekPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_DEMUXER_ERR, __func__);
  }
}

RefPtr<MediaRawData>
OggTrackDemuxer::NextSample()
{
  if (mQueuedSample) {
    RefPtr<MediaRawData> nextSample = mQueuedSample;
    mQueuedSample = nullptr;
    if (mType == TrackInfo::kAudioTrack) {
      nextSample->mTrackInfo = mParent->mSharedAudioTrackInfo;
    }
    return nextSample;
  }
  ogg_packet* packet = mParent->GetNextPacket(mType);
  if (!packet) {
    return nullptr;
  }
  // Check the eos state in case we need to look for chained streams.
  bool eos = packet->e_o_s;
  OggCodecState* state = mParent->GetTrackCodecState(mType);
  RefPtr<MediaRawData> data = state->PacketOutAsMediaRawData();
  if (!data) {
    return nullptr;
  }
  if (mType == TrackInfo::kAudioTrack) {
    data->mTrackInfo = mParent->mSharedAudioTrackInfo;
  }
  if (eos) {
    // We've encountered an end of bitstream packet; check for a chained
    // bitstream following this one.
    // This will also update mSharedAudioTrackInfo.
    mParent->ReadOggChain(data->GetEndTime());
  }
  return data;
}

RefPtr<OggTrackDemuxer::SamplesPromise>
OggTrackDemuxer::GetSamples(int32_t aNumSamples)
{
  RefPtr<SamplesHolder> samples = new SamplesHolder;
  if (!aNumSamples) {
    return SamplesPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_DEMUXER_ERR, __func__);
  }

  while (aNumSamples) {
    RefPtr<MediaRawData> sample(NextSample());
    if (!sample) {
      break;
    }
    samples->mSamples.AppendElement(sample);
    aNumSamples--;
  }

  if (samples->mSamples.IsEmpty()) {
    return SamplesPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_END_OF_STREAM, __func__);
  } else {
    return SamplesPromise::CreateAndResolve(samples, __func__);
  }
}

void
OggTrackDemuxer::Reset()
{
  mParent->Reset(mType);
  mQueuedSample = nullptr;
}

RefPtr<OggTrackDemuxer::SkipAccessPointPromise>
OggTrackDemuxer::SkipToNextRandomAccessPoint(const TimeUnit& aTimeThreshold)
{
  uint32_t parsed = 0;
  bool found = false;
  RefPtr<MediaRawData> sample;

  OGG_DEBUG("TimeThreshold: %f", aTimeThreshold.ToSeconds());
  while (!found && (sample = NextSample())) {
    parsed++;
    if (sample->mKeyframe && sample->mTime >= aTimeThreshold) {
      found = true;
      mQueuedSample = sample;
    }
  }
  if (found) {
    OGG_DEBUG("next sample: %f (parsed: %d)",
               sample->mTime.ToSeconds(), parsed);
    return SkipAccessPointPromise::CreateAndResolve(parsed, __func__);
  } else {
    SkipFailureHolder failure(NS_ERROR_DOM_MEDIA_END_OF_STREAM, parsed);
    return SkipAccessPointPromise::CreateAndReject(Move(failure), __func__);
  }
}

TimeIntervals
OggTrackDemuxer::GetBuffered()
{
  return mParent->GetBuffered(mType);
}

void
OggTrackDemuxer::BreakCycles()
{
  mParent = nullptr;
}


// Returns an ogg page's checksum.
ogg_uint32_t
OggDemuxer::GetPageChecksum(ogg_page* page)
{
  if (page == 0 || page->header == 0 || page->header_len < 25) {
    return 0;
  }
  const unsigned char* p = page->header + 22;
  uint32_t c = p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);
  return c;
}

int64_t
OggDemuxer::RangeStartTime(TrackInfo::TrackType aType, int64_t aOffset)
{
  int64_t position = Resource(aType)->Tell();
  nsresult res = Resource(aType)->Seek(nsISeekableStream::NS_SEEK_SET, aOffset);
  NS_ENSURE_SUCCESS(res, 0);
  int64_t startTime = 0;
  FindStartTime(aType, startTime);
  res = Resource(aType)->Seek(nsISeekableStream::NS_SEEK_SET, position);
  NS_ENSURE_SUCCESS(res, -1);
  return startTime;
}

struct nsDemuxerAutoOggSyncState
{
  nsDemuxerAutoOggSyncState()
  {
    ogg_sync_init(&mState);
  }
  ~nsDemuxerAutoOggSyncState()
  {
    ogg_sync_clear(&mState);
  }
  ogg_sync_state mState;
};

int64_t
OggDemuxer::RangeEndTime(TrackInfo::TrackType aType, int64_t aEndOffset)
{
  int64_t position = Resource(aType)->Tell();
  int64_t endTime = RangeEndTime(aType, 0, aEndOffset, false);
  nsresult res = Resource(aType)->Seek(nsISeekableStream::NS_SEEK_SET, position);
  NS_ENSURE_SUCCESS(res, -1);
  return endTime;
}

int64_t
OggDemuxer::RangeEndTime(TrackInfo::TrackType aType,
                         int64_t aStartOffset,
                         int64_t aEndOffset,
                         bool aCachedDataOnly)
{
  nsDemuxerAutoOggSyncState sync;

  // We need to find the last page which ends before aEndOffset that
  // has a granulepos that we can convert to a timestamp. We do this by
  // backing off from aEndOffset until we encounter a page on which we can
  // interpret the granulepos. If while backing off we encounter a page which
  // we've previously encountered before, we'll either backoff again if we
  // haven't found an end time yet, or return the last end time found.
  const int step = 5000;
  const int maxOggPageSize = 65306;
  int64_t readStartOffset = aEndOffset;
  int64_t readLimitOffset = aEndOffset;
  int64_t readHead = aEndOffset;
  int64_t endTime = -1;
  uint32_t checksumAfterSeek = 0;
  uint32_t prevChecksumAfterSeek = 0;
  bool mustBackOff = false;
  while (true) {
    ogg_page page;
    int ret = ogg_sync_pageseek(&sync.mState, &page);
    if (ret == 0) {
      // We need more data if we've not encountered a page we've seen before,
      // or we've read to the end of file.
      if (mustBackOff || readHead == aEndOffset || readHead == aStartOffset) {
        if (endTime != -1 || readStartOffset == 0) {
          // We have encountered a page before, or we're at the end of file.
          break;
        }
        mustBackOff = false;
        prevChecksumAfterSeek = checksumAfterSeek;
        checksumAfterSeek = 0;
        ogg_sync_reset(&sync.mState);
        readStartOffset = std::max(static_cast<int64_t>(0), readStartOffset - step);
        // There's no point reading more than the maximum size of
        // an Ogg page into data we've previously scanned. Any data
        // between readLimitOffset and aEndOffset must be garbage
        // and we can ignore it thereafter.
        readLimitOffset = std::min(readLimitOffset,
                                   readStartOffset + maxOggPageSize);
        readHead = std::max(aStartOffset, readStartOffset);
      }

      int64_t limit = std::min(static_cast<int64_t>(UINT32_MAX),
                               aEndOffset - readHead);
      limit = std::max(static_cast<int64_t>(0), limit);
      limit = std::min(limit, static_cast<int64_t>(step));
      uint32_t bytesToRead = static_cast<uint32_t>(limit);
      uint32_t bytesRead = 0;
      char* buffer = ogg_sync_buffer(&sync.mState, bytesToRead);
      NS_ASSERTION(buffer, "Must have buffer");
      nsresult res;
      if (aCachedDataOnly) {
        res = Resource(aType)->GetResource()->ReadFromCache(buffer, readHead, bytesToRead);
        NS_ENSURE_SUCCESS(res, -1);
        bytesRead = bytesToRead;
      } else {
        NS_ASSERTION(readHead < aEndOffset,
                     "resource pos must be before range end");
        res = Resource(aType)->Seek(nsISeekableStream::NS_SEEK_SET, readHead);
        NS_ENSURE_SUCCESS(res, -1);
        res = Resource(aType)->Read(buffer, bytesToRead, &bytesRead);
        NS_ENSURE_SUCCESS(res, -1);
      }
      readHead += bytesRead;
      if (readHead > readLimitOffset) {
        mustBackOff = true;
      }

      // Update the synchronisation layer with the number
      // of bytes written to the buffer
      ret = ogg_sync_wrote(&sync.mState, bytesRead);
      if (ret != 0) {
        endTime = -1;
        break;
      }
      continue;
    }

    if (ret < 0 || ogg_page_granulepos(&page) < 0) {
      continue;
    }

    uint32_t checksum = GetPageChecksum(&page);
    if (checksumAfterSeek == 0) {
      // This is the first page we've decoded after a backoff/seek. Remember
      // the page checksum. If we backoff further and encounter this page
      // again, we'll know that we won't find a page with an end time after
      // this one, so we'll know to back off again.
      checksumAfterSeek = checksum;
    }
    if (checksum == prevChecksumAfterSeek) {
      // This page has the same checksum as the first page we encountered
      // after the last backoff/seek. Since we've already scanned after this
      // page and failed to find an end time, we may as well backoff again and
      // try to find an end time from an earlier page.
      mustBackOff = true;
      continue;
    }

    int64_t granulepos = ogg_page_granulepos(&page);
    int serial = ogg_page_serialno(&page);

    OggCodecState* codecState = nullptr;
    codecState = mCodecStore.Get(serial);
    if (!codecState) {
      // This page is from a bitstream which we haven't encountered yet.
      // It's probably from a new "link" in a "chained" ogg. Don't
      // bother even trying to find a duration...
      SetChained();
      endTime = -1;
      break;
    }

    int64_t t = codecState->Time(granulepos);
    if (t != -1) {
      endTime = t;
    }
  }

  return endTime;
}

nsresult
OggDemuxer::GetSeekRanges(TrackInfo::TrackType aType,
                          nsTArray<SeekRange>& aRanges)
{
  AutoPinned<MediaResource> resource(Resource(aType)->GetResource());
  MediaByteRangeSet cached;
  nsresult res = resource->GetCachedRanges(cached);
  NS_ENSURE_SUCCESS(res, res);

  for (uint32_t index = 0; index < cached.Length(); index++) {
    auto& range = cached[index];
    int64_t startTime = -1;
    int64_t endTime = -1;
    if (NS_FAILED(Reset(aType))) {
      return NS_ERROR_FAILURE;
    }
    int64_t startOffset = range.mStart;
    int64_t endOffset = range.mEnd;
    startTime = RangeStartTime(aType, startOffset);
    if (startTime != -1 &&
        ((endTime = RangeEndTime(aType, endOffset)) != -1)) {
      NS_WARNING_ASSERTION(startTime < endTime,
                           "Start time must be before end time");
      aRanges.AppendElement(SeekRange(startOffset,
                                      endOffset,
                                      startTime,
                                      endTime));
     }
  }
  if (NS_FAILED(Reset(aType))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

OggDemuxer::SeekRange
OggDemuxer::SelectSeekRange(TrackInfo::TrackType aType,
                            const nsTArray<SeekRange>& ranges,
                            int64_t aTarget,
                            int64_t aStartTime,
                            int64_t aEndTime,
                            bool aExact)
{
  int64_t so = 0;
  int64_t eo = Resource(aType)->GetLength();
  int64_t st = aStartTime;
  int64_t et = aEndTime;
  for (uint32_t i = 0; i < ranges.Length(); i++) {
    const SeekRange& r = ranges[i];
    if (r.mTimeStart < aTarget) {
      so = r.mOffsetStart;
      st = r.mTimeStart;
    }
    if (r.mTimeEnd >= aTarget && r.mTimeEnd < et) {
      eo = r.mOffsetEnd;
      et = r.mTimeEnd;
    }

    if (r.mTimeStart < aTarget && aTarget <= r.mTimeEnd) {
      // Target lies exactly in this range.
      return ranges[i];
    }
  }
  if (aExact || eo == -1) {
    return SeekRange();
  }
  return SeekRange(so, eo, st, et);
}


nsresult
OggDemuxer::SeekInBufferedRange(TrackInfo::TrackType aType,
                                int64_t aTarget,
                                int64_t aAdjustedTarget,
                                int64_t aStartTime,
                                int64_t aEndTime,
                                const nsTArray<SeekRange>& aRanges,
                                const SeekRange& aRange)
{
  OGG_DEBUG("Seeking in buffered data to %" PRId64 " using bisection search", aTarget);
  if (aType == TrackInfo::kVideoTrack || aAdjustedTarget >= aTarget) {
    // We know the exact byte range in which the target must lie. It must
    // be buffered in the media cache. Seek there.
    nsresult res = SeekBisection(aType, aTarget, aRange, 0);
    if (NS_FAILED(res) || aType != TrackInfo::kVideoTrack) {
      return res;
    }

    // We have an active Theora bitstream. Peek the next Theora frame, and
    // extract its keyframe's time.
    DemuxUntilPacketAvailable(aType, mTheoraState);
    ogg_packet* packet = mTheoraState->PacketPeek();
    if (packet && !mTheoraState->IsKeyframe(packet)) {
      // First post-seek frame isn't a keyframe, seek back to previous keyframe,
      // otherwise we'll get visual artifacts.
      NS_ASSERTION(packet->granulepos != -1, "Must have a granulepos");
      int shift = mTheoraState->KeyFrameGranuleJobs();
      int64_t keyframeGranulepos = (packet->granulepos >> shift) << shift;
      int64_t keyframeTime = mTheoraState->StartTime(keyframeGranulepos);
      SEEK_LOG(LogLevel::Debug,
               ("Keyframe for %lld is at %lld, seeking back to it", frameTime,
                keyframeTime));
      aAdjustedTarget = std::min(aAdjustedTarget, keyframeTime);
    }
  }

  nsresult res = NS_OK;
  if (aAdjustedTarget < aTarget) {
    SeekRange k = SelectSeekRange(aType,
                                  aRanges,
                                  aAdjustedTarget,
                                  aStartTime,
                                  aEndTime,
                                  false);
    res = SeekBisection(aType, aAdjustedTarget, k, OGG_SEEK_FUZZ_USECS);
  }
  return res;
}

nsresult
OggDemuxer::SeekInUnbuffered(TrackInfo::TrackType aType,
                             int64_t aTarget,
                             int64_t aStartTime,
                             int64_t aEndTime,
                             const nsTArray<SeekRange>& aRanges)
{
  OGG_DEBUG("Seeking in unbuffered data to %" PRId64 " using bisection search", aTarget);

  // If we've got an active Theora bitstream, determine the maximum possible
  // time in usecs which a keyframe could be before a given interframe. We
  // subtract this from our seek target, seek to the new target, and then
  // will decode forward to the original seek target. We should encounter a
  // keyframe in that interval. This prevents us from needing to run two
  // bisections; one for the seek target frame, and another to find its
  // keyframe. It's usually faster to just download this extra data, rather
  // tham perform two bisections to find the seek target's keyframe. We
  // don't do this offsetting when seeking in a buffered range,
  // as the extra decoding causes a noticeable speed hit when all the data
  // is buffered (compared to just doing a bisection to exactly find the
  // keyframe).
  int64_t keyframeOffsetMs = 0;
  if (aType == TrackInfo::kVideoTrack && mTheoraState) {
    keyframeOffsetMs = mTheoraState->MaxKeyframeOffset();
  }
  // Add in the Opus pre-roll if necessary, as well.
  if (aType == TrackInfo::kAudioTrack && mOpusState) {
    keyframeOffsetMs = std::max(keyframeOffsetMs, OGG_SEEK_OPUS_PREROLL);
  }
  int64_t seekTarget = std::max(aStartTime, aTarget - keyframeOffsetMs);
  // Minimize the bisection search space using the known timestamps from the
  // buffered ranges.
  SeekRange k =
    SelectSeekRange(aType, aRanges, seekTarget, aStartTime, aEndTime, false);
  return SeekBisection(aType, seekTarget, k, OGG_SEEK_FUZZ_USECS);
}

nsresult
OggDemuxer::SeekBisection(TrackInfo::TrackType aType,
                          int64_t aTarget,
                          const SeekRange& aRange,
                          uint32_t aFuzz)
{
  nsresult res;

  if (aTarget <= aRange.mTimeStart) {
    if (NS_FAILED(Reset(aType))) {
      return NS_ERROR_FAILURE;
    }
    res = Resource(aType)->Seek(nsISeekableStream::NS_SEEK_SET, 0);
    NS_ENSURE_SUCCESS(res,res);
    return NS_OK;
  }

  // Bisection search, find start offset of last page with end time less than
  // the seek target.
  ogg_int64_t startOffset = aRange.mOffsetStart;
  ogg_int64_t startTime = aRange.mTimeStart;
  ogg_int64_t startLength = 0; // Length of the page at startOffset.
  ogg_int64_t endOffset = aRange.mOffsetEnd;
  ogg_int64_t endTime = aRange.mTimeEnd;

  ogg_int64_t seekTarget = aTarget;
  int64_t seekLowerBound = std::max(static_cast<int64_t>(0), aTarget - aFuzz);
  int hops = 0;
  DebugOnly<ogg_int64_t> previousGuess = -1;
  int backsteps = 0;
  const int maxBackStep = 10;
  NS_ASSERTION(static_cast<uint64_t>(PAGE_STEP) * pow(2.0, maxBackStep) < INT32_MAX,
               "Backstep calculation must not overflow");

  // Seek via bisection search. Loop until we find the offset where the page
  // before the offset is before the seek target, and the page after the offset
  // is after the seek target.
  while (true) {
    ogg_int64_t duration = 0;
    double target = 0;
    ogg_int64_t interval = 0;
    ogg_int64_t guess = 0;
    ogg_page page;
    int skippedBytes = 0;
    ogg_int64_t pageOffset = 0;
    ogg_int64_t pageLength = 0;
    ogg_int64_t granuleTime = -1;
    bool mustBackoff = false;

    // Guess where we should bisect to, based on the bit rate and the time
    // remaining in the interval. Loop until we can determine the time at
    // the guess offset.
    while (true) {

      // Discard any previously buffered packets/pages.
      if (NS_FAILED(Reset(aType))) {
        return NS_ERROR_FAILURE;
      }

      interval = endOffset - startOffset - startLength;
      if (interval == 0) {
        // Our interval is empty, we've found the optimal seek point, as the
        // page at the start offset is before the seek target, and the page
        // at the end offset is after the seek target.
        SEEK_LOG(LogLevel::Debug, ("Interval narrowed, terminating bisection."));
        break;
      }

      // Guess bisection point.
      duration = endTime - startTime;
      target = (double)(seekTarget - startTime) / (double)duration;
      guess = startOffset + startLength +
              static_cast<ogg_int64_t>((double)interval * target);
      guess = std::min(guess, endOffset - PAGE_STEP);
      if (mustBackoff) {
        // We previously failed to determine the time at the guess offset,
        // probably because we ran out of data to decode. This usually happens
        // when we guess very close to the end offset. So reduce the guess
        // offset using an exponential backoff until we determine the time.
        SEEK_LOG(LogLevel::Debug, ("Backing off %d bytes, backsteps=%d",
          static_cast<int32_t>(PAGE_STEP * pow(2.0, backsteps)), backsteps));
        guess -= PAGE_STEP * static_cast<ogg_int64_t>(pow(2.0, backsteps));

        if (guess <= startOffset) {
          // We've tried to backoff to before the start offset of our seek
          // range. This means we couldn't find a seek termination position
          // near the end of the seek range, so just set the seek termination
          // condition, and break out of the bisection loop. We'll begin
          // decoding from the start of the seek range.
          interval = 0;
          break;
        }

        backsteps = std::min(backsteps + 1, maxBackStep);
        // We reset mustBackoff. If we still need to backoff further, it will
        // be set to true again.
        mustBackoff = false;
      } else {
        backsteps = 0;
      }
      guess = std::max(guess, startOffset + startLength);

      SEEK_LOG(LogLevel::Debug, ("Seek loop start[o=%lld..%lld t=%lld] "
                              "end[o=%lld t=%lld] "
                              "interval=%lld target=%lf guess=%lld",
                              startOffset, (startOffset+startLength), startTime,
                              endOffset, endTime, interval, target, guess));

      NS_ASSERTION(guess >= startOffset + startLength, "Guess must be after range start");
      NS_ASSERTION(guess < endOffset, "Guess must be before range end");
      NS_ASSERTION(guess != previousGuess, "Guess should be different to previous");
      previousGuess = guess;

      hops++;

      // Locate the next page after our seek guess, and then figure out the
      // granule time of the audio and video bitstreams there. We can then
      // make a bisection decision based on our location in the media.
      PageSyncResult pageSyncResult = PageSync(Resource(aType),
                                               OggSyncState(aType),
                                               false,
                                               guess,
                                               endOffset,
                                               &page,
                                               skippedBytes);
      NS_ENSURE_TRUE(pageSyncResult != PAGE_SYNC_ERROR, NS_ERROR_FAILURE);

      if (pageSyncResult == PAGE_SYNC_END_OF_RANGE) {
        // Our guess was too close to the end, we've ended up reading the end
        // page. Backoff exponentially from the end point, in case the last
        // page/frame/sample is huge.
        mustBackoff = true;
        SEEK_LOG(LogLevel::Debug, ("Hit the end of range, backing off"));
        continue;
      }

      // We've located a page of length |ret| at |guess + skippedBytes|.
      // Remember where the page is located.
      pageOffset = guess + skippedBytes;
      pageLength = page.header_len + page.body_len;

      // Read pages until we can determine the granule time of the audio and
      // video bitstream.
      ogg_int64_t audioTime = -1;
      ogg_int64_t videoTime = -1;
      do {
        // Add the page to its codec state, determine its granule time.
        uint32_t serial = ogg_page_serialno(&page);
        OggCodecState* codecState = mCodecStore.Get(serial);
        if (codecState && GetCodecStateType(codecState) == aType) {
          if (codecState->mActive) {
            int ret = ogg_stream_pagein(&codecState->mState, &page);
            NS_ENSURE_TRUE(ret == 0, NS_ERROR_FAILURE);
          }

          ogg_int64_t granulepos = ogg_page_granulepos(&page);

          if (aType == TrackInfo::kAudioTrack &&
              granulepos > 0 && audioTime == -1) {
            if (mVorbisState && serial == mVorbisState->mSerial) {
              audioTime = mVorbisState->Time(granulepos);
            } else if (mOpusState && serial == mOpusState->mSerial) {
              audioTime = mOpusState->Time(granulepos);
            } else if (mFlacState && serial == mFlacState->mSerial) {
              audioTime = mFlacState->Time(granulepos);
            }
          }

          if (aType == TrackInfo::kVideoTrack &&
              granulepos > 0 && serial == mTheoraState->mSerial &&
              videoTime == -1) {
            videoTime = mTheoraState->Time(granulepos);
          }

          if (pageOffset + pageLength >= endOffset) {
            // Hit end of readable data.
            break;
          }
        }
        if (!ReadOggPage(aType, &page)) {
          break;
        }

      } while ((aType == TrackInfo::kAudioTrack && audioTime == -1) ||
               (aType == TrackInfo::kVideoTrack && videoTime == -1));


      if ((aType == TrackInfo::kAudioTrack && audioTime == -1) ||
          (aType == TrackInfo::kVideoTrack && videoTime == -1)) {
        // We don't have timestamps for all active tracks...
        if (pageOffset == startOffset + startLength &&
            pageOffset + pageLength >= endOffset) {
          // We read the entire interval without finding timestamps for all
          // active tracks. We know the interval start offset is before the seek
          // target, and the interval end is after the seek target, and we can't
          // terminate inside the interval, so we terminate the seek at the
          // start of the interval.
          interval = 0;
          break;
        }

        // We should backoff; cause the guess to back off from the end, so
        // that we've got more room to capture.
        mustBackoff = true;
        continue;
      }

      // We've found appropriate time stamps here. Proceed to bisect
      // the search space.
      granuleTime = aType == TrackInfo::kAudioTrack ? audioTime : videoTime;
      NS_ASSERTION(granuleTime > 0, "Must get a granuletime");
      break;
    } // End of "until we determine time at guess offset" loop.

    if (interval == 0) {
      // Seek termination condition; we've found the page boundary of the
      // last page before the target, and the first page after the target.
      SEEK_LOG(LogLevel::Debug, ("Terminating seek at offset=%lld", startOffset));
      NS_ASSERTION(startTime < aTarget, "Start time must always be less than target");
      res = Resource(aType)->Seek(nsISeekableStream::NS_SEEK_SET, startOffset);
      NS_ENSURE_SUCCESS(res,res);
      if (NS_FAILED(Reset(aType))) {
        return NS_ERROR_FAILURE;
      }
      break;
    }

    SEEK_LOG(LogLevel::Debug, ("Time at offset %lld is %lld", guess, granuleTime));
    if (granuleTime < seekTarget && granuleTime > seekLowerBound) {
      // We're within the fuzzy region in which we want to terminate the search.
      res = Resource(aType)->Seek(nsISeekableStream::NS_SEEK_SET, pageOffset);
      NS_ENSURE_SUCCESS(res,res);
      if (NS_FAILED(Reset(aType))) {
        return NS_ERROR_FAILURE;
      }
      SEEK_LOG(LogLevel::Debug, ("Terminating seek at offset=%lld", pageOffset));
      break;
    }

    if (granuleTime >= seekTarget) {
      // We've landed after the seek target.
      NS_ASSERTION(pageOffset < endOffset, "offset_end must decrease");
      endOffset = pageOffset;
      endTime = granuleTime;
    } else if (granuleTime < seekTarget) {
      // Landed before seek target.
      NS_ASSERTION(pageOffset >= startOffset + startLength,
        "Bisection point should be at or after end of first page in interval");
      startOffset = pageOffset;
      startLength = pageLength;
      startTime = granuleTime;
    }
    NS_ASSERTION(startTime <= seekTarget, "Must be before seek target");
    NS_ASSERTION(endTime >= seekTarget, "End must be after seek target");
  }

  SEEK_LOG(LogLevel::Debug, ("Seek complete in %d bisections.", hops));

  return NS_OK;
}

#undef OGG_DEBUG
#undef SEEK_DEBUG
} // namespace mozilla

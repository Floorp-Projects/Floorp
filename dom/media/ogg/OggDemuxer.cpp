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
#include "mozilla/PodOperations.h"
#include "mozilla/Preferences.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/TimeStamp.h"
#include "MediaDataDemuxer.h"
#include "nsAutoRef.h"
#include "XiphExtradata.h"

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

namespace mozilla {

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

class OggHeaders {
public:
  OggHeaders() {}
  ~OggHeaders()
  {
    for (size_t i = 0; i < mHeaders.Length(); i++) {
      delete[] mHeaders[i];
    }
  }

  void AppendPacket(const ogg_packet* aPacket)
  {
    size_t packetSize = aPacket->bytes;
    unsigned char* packetData = new unsigned char[packetSize];
    memcpy(packetData, aPacket->packet, packetSize);
    mHeaders.AppendElement(packetData);
    mHeaderLens.AppendElement(packetSize);
  }

  nsTArray<const unsigned char*> mHeaders;
  nsTArray<size_t> mHeaderLens;
};

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
  , mOpusEnabled(MediaDecoder::IsOpusEnabled())
  , mSkeletonState(nullptr)
  , mVorbisSerial(0)
  , mOpusSerial(0)
  , mTheoraSerial(0)
  , mOpusPreSkip(0)
  , mIsChained(false)
  , mDecodedAudioFrames(0)
  , mResource(aResource)
{
  MOZ_COUNT_CTOR(OggDemuxer);
  PodZero(&mTheoraInfo);
}

OggDemuxer::~OggDemuxer()
{
  Reset();
  Cleanup();
  MOZ_COUNT_DTOR(OggDemuxer);
  if (HasAudio() || HasVideo()) {
    // If we were able to initialize our decoders, report whether we encountered
    // a chained stream or not.
    bool isChained = mIsChained;
    nsCOMPtr<nsIRunnable> task = NS_NewRunnableFunction([=]() -> void {
      OGG_DEBUG("Reporting telemetry MEDIA_OGG_LOADED_IS_CHAINED=%d", isChained);
      Telemetry::Accumulate(Telemetry::ID::MEDIA_OGG_LOADED_IS_CHAINED, isChained);
    });
    AbstractThread::MainThread()->Dispatch(task.forget());
  }
}

bool
OggDemuxer::HasAudio()
const
{
  return mVorbisState || mOpusState;
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
  MOZ_ASSERT(HaveStartTime());
  return mStartTime.ref();
}

RefPtr<OggDemuxer::InitPromise>
OggDemuxer::Init()
{
  int ret = ogg_sync_init(&mOggState);
  if (ret != 0) {
    return InitPromise::CreateAndReject(DemuxerFailureReason::DEMUXER_ERROR, __func__);
  }
  /*
  if (InitBufferedState() != NS_OK) {
    return InitPromise::CreateAndReject(DemuxerFailureReason::WAITING_FOR_DATA, __func__);
  }
  */
  if (ReadMetadata() != NS_OK) {
    return InitPromise::CreateAndReject(DemuxerFailureReason::DEMUXER_ERROR, __func__);
  }

  if (!GetNumberTracks(TrackInfo::kAudioTrack) &&
      !GetNumberTracks(TrackInfo::kVideoTrack)) {
    return InitPromise::CreateAndReject(DemuxerFailureReason::DEMUXER_ERROR, __func__);
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
      } else {
        return mOpusState;
      }
    case TrackInfo::kVideoTrack:
      return mTheoraState;
    default:
      return 0;
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
OggDemuxer::GetTrackInfo(TrackInfo::TrackType aType,
                         size_t aTrackNumber) const
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
OggDemuxer::Reset()
{
  nsresult res = NS_OK;

  // Discard any previously buffered packets/pages.
  ogg_sync_reset(&mOggState);
  if (mVorbisState && NS_FAILED(mVorbisState->Reset())) {
    res = NS_ERROR_FAILURE;
  }
  if (mOpusState && NS_FAILED(mOpusState->Reset())) { // false?
    res = NS_ERROR_FAILURE;
  }
  if (mTheoraState && NS_FAILED(mTheoraState->Reset())) {
    res = NS_ERROR_FAILURE;
  }

  return res;
}

nsresult
OggDemuxer::ResetTrackState(TrackInfo::TrackType aType)
{
  OggCodecState* trackState = GetTrackCodecState(aType);
  if (trackState) {
    return trackState->Reset();
  }
  return NS_OK;
}

void
OggDemuxer::Cleanup()
{
  ogg_sync_clear(&mOggState);
}

bool
OggDemuxer::ReadHeaders(OggCodecState* aState, OggHeaders& aHeaders)
{
  while (!aState->DoneReadingHeaders()) {
    DemuxUntilPacketAvailable(aState);
    ogg_packet* packet = aState->PacketOut();
    if (!packet) {
      OGG_DEBUG("Ran out of header packets early; deactivating stream %ld", aState->mSerial);
      aState->Deactivate();
      return false;
    }

    // Save a copy of the header packet for the decoder to use later;
    // OggCodecState::DecodeHeader will free it when processing locally.
    aHeaders.AppendPacket(packet);

    // Local OggCodecState needs to decode headers in order to process
    // packet granulepos -> time mappings, etc.
    if (!aState->DecodeHeader(packet)) {
      OGG_DEBUG("Failed to decode ogg header packet; deactivating stream %ld", aState->mSerial);
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
OggDemuxer::SetupTargetTheora(TheoraState* aTheoraState, OggHeaders& aHeaders)
{
  if (mTheoraState) {
    mTheoraState->Reset();
  }

  nsIntRect picture = nsIntRect(aTheoraState->mInfo.pic_x,
                                aTheoraState->mInfo.pic_y,
                                aTheoraState->mInfo.pic_width,
                                aTheoraState->mInfo.pic_height);

  nsIntSize displaySize = nsIntSize(aTheoraState->mInfo.pic_width,
                                    aTheoraState->mInfo.pic_height);

  // Apply the aspect ratio to produce the intrinsic display size we report
  // to the element.
  ScaleDisplayByAspectRatio(displaySize, aTheoraState->mPixelAspectRatio);

  nsIntSize frameSize(aTheoraState->mInfo.frame_width,
                      aTheoraState->mInfo.frame_height);
  if (IsValidVideoRegion(frameSize, picture, displaySize)) {
    // Video track's frame sizes will not overflow. Activate the video track.
    mInfo.mVideo.mMimeType = "video/ogg; codecs=theora";
    mInfo.mVideo.mDisplay = displaySize;
    mInfo.mVideo.mImage = frameSize;
    mInfo.mVideo.SetImageRect(picture);

    // Copy Theora info data for time computations on other threads.
    memcpy(&mTheoraInfo, &aTheoraState->mInfo, sizeof(mTheoraInfo));

    // Save header packets for the decoder
    if (!XiphHeadersToExtradata(mInfo.mVideo.mCodecSpecificConfig,
                                aHeaders.mHeaders, aHeaders.mHeaderLens)) {
      return;
    }

    mTheoraState = aTheoraState;
    mTheoraSerial = aTheoraState->mSerial;
  }
}

void
OggDemuxer::SetupTargetVorbis(VorbisState* aVorbisState, OggHeaders& aHeaders)
{
  if (mVorbisState) {
    mVorbisState->Reset();
  }

  // Copy Vorbis info data for time computations on other threads.
  memcpy(&mVorbisInfo, &aVorbisState->mInfo, sizeof(mVorbisInfo));
  mVorbisInfo.codec_setup = nullptr;

  mInfo.mAudio.mMimeType = "audio/ogg; codecs=vorbis";
  mInfo.mAudio.mRate = aVorbisState->mInfo.rate;
  mInfo.mAudio.mChannels = aVorbisState->mInfo.channels;

  // Save header packets for the decoder
  if (!XiphHeadersToExtradata(mInfo.mAudio.mCodecSpecificConfig,
                              aHeaders.mHeaders, aHeaders.mHeaderLens)) {
    return;
  }

  mVorbisState = aVorbisState;
  mVorbisSerial = aVorbisState->mSerial;
}

void
OggDemuxer::SetupTargetOpus(OpusState* aOpusState, OggHeaders& aHeaders)
{
  if (mOpusState) {
    mOpusState->Reset();
  }

  mInfo.mAudio.mMimeType = "audio/ogg; codecs=opus";
  mInfo.mAudio.mRate = aOpusState->mRate;
  mInfo.mAudio.mChannels = aOpusState->mChannels;

  // Save preskip & the first header packet for the Opus decoder
  uint64_t preSkip = aOpusState->Time(0, aOpusState->mPreSkip);
  uint8_t c[sizeof(preSkip)];
  BigEndian::writeUint64(&c[0], preSkip);
  mInfo.mAudio.mCodecSpecificConfig->AppendElements(&c[0], sizeof(preSkip));
  mInfo.mAudio.mCodecSpecificConfig->AppendElements(aHeaders.mHeaders[0],
                                                    aHeaders.mHeaderLens[0]);

  mOpusState = aOpusState;
  mOpusSerial = aOpusState->mSerial;
  mOpusPreSkip = aOpusState->mPreSkip;
}

void
OggDemuxer::SetupTargetSkeleton()
{
  // Setup skeleton related information after mVorbisState & mTheroState
  // being set (if they exist).
  if (mSkeletonState) {
    OggHeaders headers;
    if (!HasAudio() && !HasVideo()) {
      // We have a skeleton track, but no audio or video, may as well disable
      // the skeleton, we can't do anything useful with this media.
      OGG_DEBUG("Deactivating skeleton stream %ld", mSkeletonState->mSerial);
      mSkeletonState->Deactivate();
    } else if (ReadHeaders(mSkeletonState, headers) && mSkeletonState->HasIndex()) {
      // Extract the duration info out of the index, so we don't need to seek to
      // the end of resource to get it.
      nsTArray<uint32_t> tracks;
      BuildSerialList(tracks);
      int64_t duration = 0;
      if (NS_SUCCEEDED(mSkeletonState->GetDuration(tracks, duration))) {
        OGG_DEBUG("Got duration from Skeleton index %lld", duration);
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
    if (mSkeletonState && mSkeletonState->mMsgFieldStore.Contains(serial)) {
      mSkeletonState->mMsgFieldStore.Get(serial, &msgInfo);
    }

    if (codecState->GetType() == OggCodecState::TYPE_THEORA) {
      TheoraState* theoraState = static_cast<TheoraState*>(codecState);
      if (!(mTheoraState && mTheoraState->mSerial == theoraState->mSerial)) {
        continue;
      }

      if (msgInfo) {
        InitTrack(msgInfo, &mInfo.mVideo, mTheoraState == theoraState);
      }

      nsIntRect picture = nsIntRect(theoraState->mInfo.pic_x,
                                    theoraState->mInfo.pic_y,
                                    theoraState->mInfo.pic_width,
                                    theoraState->mInfo.pic_height);
      nsIntSize displaySize = nsIntSize(theoraState->mInfo.pic_width,
                                        theoraState->mInfo.pic_height);
      nsIntSize frameSize(theoraState->mInfo.frame_width,
                          theoraState->mInfo.frame_height);
      ScaleDisplayByAspectRatio(displaySize, theoraState->mPixelAspectRatio);
      if (IsValidVideoRegion(frameSize, picture, displaySize)) {
        mInfo.mVideo.mDisplay = displaySize;
      }
    } else if (codecState->GetType() == OggCodecState::TYPE_VORBIS) {
      VorbisState* vorbisState = static_cast<VorbisState*>(codecState);
      if (!(mVorbisState && mVorbisState->mSerial == vorbisState->mSerial)) {
        continue;
      }

      if (msgInfo) {
        InitTrack(msgInfo,
                  &mInfo.mAudio,
                  mVorbisState == vorbisState);
      }

      mInfo.mAudio.mRate = vorbisState->mInfo.rate;
      mInfo.mAudio.mChannels = vorbisState->mInfo.channels;
      FillTags(&mInfo.mAudio, vorbisState->GetTags());
    } else if (codecState->GetType() == OggCodecState::TYPE_OPUS) {
      OpusState* opusState = static_cast<OpusState*>(codecState);
      if (!(mOpusState && mOpusState->mSerial == opusState->mSerial)) {
        continue;
      }

      if (msgInfo) {
        InitTrack(msgInfo,
                  &mInfo.mAudio,
                  mOpusState == opusState);
      }

      mInfo.mAudio.mRate = opusState->mRate;
      mInfo.mAudio.mChannels = opusState->mChannels;
      FillTags(&mInfo.mAudio, opusState->GetTags());
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

  ogg_page page;
  nsTArray<OggCodecState*> bitstreams;
  nsTArray<uint32_t> serials;
  bool readAllBOS = false;
  while (!readAllBOS) {
    if (!ReadOggPage(&page)) {
      // Some kind of error...
      OGG_DEBUG("OggDemuxer::ReadOggPage failed? leaving ReadMetadata...");
      break;
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
    if (NS_FAILED(DemuxOggPage(&page))) {
      return NS_ERROR_FAILURE;
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
      OggHeaders headers;
      if (s->GetType() == OggCodecState::TYPE_THEORA && ReadHeaders(s, headers)) {
        if (!mTheoraState) {
          TheoraState* theoraState = static_cast<TheoraState*>(s);
          SetupTargetTheora(theoraState, headers);
        } else {
          s->Deactivate();
        }
      } else if (s->GetType() == OggCodecState::TYPE_VORBIS && ReadHeaders(s, headers)) {
        if (!mVorbisState) {
          VorbisState* vorbisState = static_cast<VorbisState*>(s);
          SetupTargetVorbis(vorbisState, headers);
        } else {
          s->Deactivate();
        }
      } else if (s->GetType() == OggCodecState::TYPE_OPUS && ReadHeaders(s, headers)) {
        if (mOpusEnabled) {
          if (!mOpusState) {
            OpusState* opusState = static_cast<OpusState*>(s);
            SetupTargetOpus(opusState, headers);
          } else {
            s->Deactivate();
          }
        } else {
          NS_WARNING("Opus decoding disabled."
                     " See media.opus.enabled in about:config");
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
    NS_ASSERTION(startTime >= 0, "Must have a non-negative start time");
    OGG_DEBUG("Detected stream start time %lld", startTime);
    if (startTime >= 0) {
      mStartTime.emplace(startTime);
    }

    if (mInfo.mMetadataDuration.isNothing() &&
        mResource.GetLength() >= 0 && IsSeekable())
    {
      // We didn't get a duration from the index or a Content-Duration header.
      // Seek to the end of file to find the end time.
      int64_t length = mResource.GetLength();

      NS_ASSERTION(length > 0, "Must have a content length to get end time");

      int64_t endTime = RangeEndTime(length);

      if (endTime != -1) {
        mInfo.mUnadjustedMetadataEndTime.emplace(TimeUnit::FromMicroseconds(endTime));
        mInfo.mMetadataDuration.emplace(TimeUnit::FromMicroseconds(endTime - mStartTime.refOr(0)));
        OGG_DEBUG("Got Ogg duration from seeking to end %lld", endTime);
      }
    }
    if (mInfo.mMetadataDuration.isNothing()) {
      mInfo.mMetadataDuration.emplace(TimeUnit::FromInfinity());
    }
    if (HasAudio()) {
      mInfo.mAudio.mDuration = mInfo.mMetadataDuration->ToMicroseconds();
    }
    if (HasVideo()) {
      mInfo.mVideo.mDuration = mInfo.mMetadataDuration->ToMicroseconds();
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
  // @FIXME how can MediaDataDemuxer / MediaTrackDemuxer notify this has changed?
  //mOnMediaNotSeekable.Notify();
}

bool
OggDemuxer::ReadOggChain()
{
  bool chained = false;
  OpusState* newOpusState = nullptr;
  VorbisState* newVorbisState = nullptr;
  nsAutoPtr<MetadataTags> tags;

  if (HasVideo() || HasSkeleton() || !HasAudio()) {
    return false;
  }

  ogg_page page;
  if (!ReadOggPage(&page) || !ogg_page_bos(&page)) {
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
  }
  else {
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
  if (mSkeletonState && mSkeletonState->mMsgFieldStore.Contains(serial)) {
    mSkeletonState->mMsgFieldStore.Get(serial, &msgInfo);
  }

  OggHeaders vorbisHeaders;
  if ((newVorbisState && ReadHeaders(newVorbisState, vorbisHeaders)) &&
      (mVorbisState->mInfo.rate == newVorbisState->mInfo.rate) &&
      (mVorbisState->mInfo.channels == newVorbisState->mInfo.channels)) {

    SetupTargetVorbis(newVorbisState, vorbisHeaders);
    LOG(LogLevel::Debug, ("New vorbis ogg link, serial=%d\n", mVorbisSerial));

    if (msgInfo) {
      InitTrack(msgInfo, &mInfo.mAudio, true);
    }
    mInfo.mAudio.mMimeType = NS_LITERAL_CSTRING("audio/ogg; codec=vorbis");
    mInfo.mAudio.mRate = newVorbisState->mInfo.rate;
    mInfo.mAudio.mChannels = newVorbisState->mInfo.channels;

    chained = true;
    tags = newVorbisState->GetTags();
  }

  OggHeaders opusHeaders;
  if ((newOpusState && ReadHeaders(newOpusState, opusHeaders)) &&
      (mOpusState->mRate == newOpusState->mRate) &&
      (mOpusState->mChannels == newOpusState->mChannels)) {

    SetupTargetOpus(newOpusState, opusHeaders);

    if (msgInfo) {
      InitTrack(msgInfo, &mInfo.mAudio, true);
    }
    mInfo.mAudio.mMimeType = NS_LITERAL_CSTRING("audio/ogg; codec=opus");
    mInfo.mAudio.mRate = newOpusState->mRate;
    mInfo.mAudio.mChannels = newOpusState->mChannels;

    chained = true;
    tags = newOpusState->GetTags();
  }

  if (chained) {
    SetChained();
    {
      // @FIXME notify this!
      /*
      auto t = mDecodedAudioFrames * USECS_PER_S / mInfo.mAudio.mRate;
      mTimedMetadataEvent.Notify(
        TimedMetadata(TimeUnit::FromMicroseconds(t),
                      Move(tags),
                      nsAutoPtr<MediaInfo>(new MediaInfo(mInfo))));
      */
    }
    return true;
  }

  return false;
}

bool
OggDemuxer::ReadOggPage(ogg_page* aPage)
{
  int ret = 0;
  while((ret = ogg_sync_pageseek(&mOggState, aPage)) <= 0) {
    if (ret < 0) {
      // Lost page sync, have to skip up to next page.
      continue;
    }
    // Returns a buffer that can be written too
    // with the given size. This buffer is stored
    // in the ogg synchronisation structure.
    char* buffer = ogg_sync_buffer(&mOggState, 4096);
    NS_ASSERTION(buffer, "ogg_sync_buffer failed");

    // Read from the resource into the buffer
    uint32_t bytesRead = 0;

    nsresult rv = mResource.Read(buffer, 4096, &bytesRead);
    if (NS_FAILED(rv) || !bytesRead) {
      // End of file or error.
      return false;
    }

    // Update the synchronisation layer with the number
    // of bytes written to the buffer
    ret = ogg_sync_wrote(&mOggState, bytesRead);
    NS_ENSURE_TRUE(ret == 0, false);
  }

  return true;
}

nsresult
OggDemuxer::DemuxOggPage(ogg_page* aPage)
{
  int serial = ogg_page_serialno(aPage);
  OggCodecState* codecState = mCodecStore.Get(serial);
  if (codecState == nullptr) {
    OGG_DEBUG("encountered packet for unrecognized codecState");
    return NS_ERROR_FAILURE;
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

  do {
    if (packet) {
      OggCodecState::ReleasePacket(state->PacketOut());
    }
    DemuxUntilPacketAvailable(state);

    packet = state->PacketPeek();
  } while (packet && state->IsHeader(packet));

  return packet;
}

void
OggDemuxer::DemuxUntilPacketAvailable(OggCodecState* aState)
{
  while (!aState->IsPacketReady()) {
    OGG_DEBUG("no packet yet, reading some more");
    ogg_page page;
    if (!ReadOggPage(&page)) {
      OGG_DEBUG("no more pages to read in resource?");
      return;
    }
    DemuxOggPage(&page);
  }
}

TimeIntervals
OggDemuxer::GetBuffered()
{
  if (!HaveStartTime()) {
    return TimeIntervals();
  }
  {
    if (mIsChained) {
      return TimeIntervals::Invalid();
    }
  }
  TimeIntervals buffered;
  // HasAudio and HasVideo are not used here as they take a lock and cause
  // a deadlock. Accessing mInfo doesn't require a lock - it doesn't change
  // after metadata is read.
  if (!mInfo.HasValidMedia()) {
    // No need to search through the file if there are no audio or video tracks
    return buffered;
  }

  AutoPinned<MediaResource> resource(mResource.GetResource());
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
      PageSyncResult pageSyncResult = PageSync(&mResource,
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
      if (mVorbisState && serial == mVorbisSerial) {
        startTime = VorbisState::Time(&mVorbisInfo, granulepos);
        NS_ASSERTION(startTime > 0, "Must have positive start time");
      }
      else if (mOpusState && serial == mOpusSerial) {
        startTime = OpusState::Time(mOpusPreSkip, granulepos);
        NS_ASSERTION(startTime > 0, "Must have positive start time");
      }
      else if (mTheoraState && serial == mTheoraSerial) {
        startTime = TheoraState::Time(&mTheoraInfo, granulepos);
        NS_ASSERTION(startTime > 0, "Must have positive start time");
      }
      else if (mCodecStore.Contains(serial)) {
        // Stream is not the theora or vorbis stream we're playing,
        // but is one that we have header data for.
        startOffset += page.header_len + page.body_len;
        continue;
      }
      else {
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
      int64_t endTime = RangeEndTime(startOffset, endOffset, true);
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
    ogg_packet* pkt = GetNextPacket(TrackInfo::kVideoTrack);
    if (pkt) {
      videoStartTime = mTheoraState->PacketStartTime(pkt);
      OGG_DEBUG("OggDemuxer::FindStartTime() video=%lld", videoStartTime);
    }
  }
  if (HasAudio()) {
    OggCodecState* state = GetTrackCodecState(TrackInfo::kAudioTrack);
    ogg_packet* pkt = GetNextPacket(TrackInfo::kAudioTrack);
    if (pkt) {
      audioStartTime = state->PacketStartTime(pkt);
      OGG_DEBUG("OggReader::FindStartTime() audio=%lld", audioStartTime);
    }
  }

  int64_t startTime = std::min(videoStartTime, audioStartTime);
  if (startTime != INT64_MAX) {
    aOutStartTime = startTime;
  }
}

nsresult
OggDemuxer::SeekInternal(const TimeUnit& aTarget)
{
  int64_t target = aTarget.ToMicroseconds();
  OGG_DEBUG("About to seek to %lld", target);
  nsresult res;
  int64_t adjustedTarget = target;
  int64_t startTime = StartTime();
  int64_t endTime = mInfo.mMetadataDuration->ToMicroseconds();
  if (HasAudio() && mOpusState){
    adjustedTarget = std::max(startTime, target - OGG_SEEK_OPUS_PREROLL);
  }

  if (adjustedTarget == startTime) {
    // We've seeked to the media start. Just seek to the offset of the first
    // content page.
    res = mResource.Seek(nsISeekableStream::NS_SEEK_SET, 0);
    NS_ENSURE_SUCCESS(res,res);

    res = Reset();
    NS_ENSURE_SUCCESS(res,res);
  } else {
    // TODO: This may seek back unnecessarily far in the video, but we don't
    // have a way of asking Skeleton to seek to a different target for each
    // stream yet. Using adjustedTarget here is at least correct, if slow.
    IndexedSeekResult sres = SeekToKeyframeUsingIndex(adjustedTarget);
    NS_ENSURE_TRUE(sres != SEEK_FATAL_ERROR, NS_ERROR_FAILURE);
    if (sres == SEEK_INDEX_FAIL) {
      // No index or other non-fatal index-related failure. Try to seek
      // using a bisection search. Determine the already downloaded data
      // in the media cache, so we can try to seek in the cached data first.
      AutoTArray<SeekRange, 16> ranges;
      res = GetSeekRanges(ranges);
      NS_ENSURE_SUCCESS(res,res);

      // Figure out if the seek target lies in a buffered range.
      SeekRange r = SelectSeekRange(ranges, target, startTime, endTime, true);

      if (!r.IsNull()) {
        // We know the buffered range in which the seek target lies, do a
        // bisection search in that buffered range.
        res = SeekInBufferedRange(target, adjustedTarget, startTime, endTime, ranges, r);
        NS_ENSURE_SUCCESS(res,res);
      } else {
        // The target doesn't lie in a buffered range. Perform a bisection
        // search over the whole media, using the known buffered ranges to
        // reduce the search space.
        res = SeekInUnbuffered(target, startTime, endTime, ranges);
        NS_ENSURE_SUCCESS(res,res);
      }
    }
  }

  if (HasVideo()) {
    // Demux forwards until we find the next keyframe. This is required,
    // as although the seek should finish on a page containing a keyframe,
    // there may be non-keyframes in the page before the keyframe.
    // When doing fastSeek we display the first frame after the seek, so
    // we need to advance the decode to the keyframe otherwise we'll get
    // visual artifacts in the first frame output after the seek.
    while (true) {
      DemuxUntilPacketAvailable(mTheoraState);
      ogg_packet* packet = mTheoraState->PacketPeek();
      if (packet == nullptr) {
        OGG_DEBUG("End of Theora stream reached before keyframe found in indexed seek");
        break;
      }
      if (mTheoraState->IsKeyframe(packet)) {
        OGG_DEBUG("Theora keyframe found after seek");
        break;
      }
      // Discard video packets before the first keyframe.
      ogg_packet* releaseMe = mTheoraState->PacketOut();
      OggCodecState::ReleasePacket(releaseMe);
    }
  }
  return NS_OK;
}

OggDemuxer::IndexedSeekResult
OggDemuxer::RollbackIndexedSeek(int64_t aOffset)
{
  if (mSkeletonState) {
    mSkeletonState->Deactivate();
  }
  nsresult res = mResource.Seek(nsISeekableStream::NS_SEEK_SET, aOffset);
  NS_ENSURE_SUCCESS(res, SEEK_FATAL_ERROR);
  return SEEK_INDEX_FAIL;
}

OggDemuxer::IndexedSeekResult
OggDemuxer::SeekToKeyframeUsingIndex(int64_t aTarget)
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
                                                  keyframe)))
  {
    // Could not locate a keypoint for the target in the index.
    return SEEK_INDEX_FAIL;
  }

  // Remember original resource read cursor position so we can rollback on failure.
  int64_t tell = mResource.Tell();

  // Seek to the keypoint returned by the index.
  if (keyframe.mKeyPoint.mOffset > mResource.GetLength() ||
      keyframe.mKeyPoint.mOffset < 0)
  {
    // Index must be invalid.
    return RollbackIndexedSeek(tell);
  }
  LOG(LogLevel::Debug, ("Seeking using index to keyframe at offset %lld\n",
                     keyframe.mKeyPoint.mOffset));
  nsresult res = mResource.Seek(nsISeekableStream::NS_SEEK_SET,
                                keyframe.mKeyPoint.mOffset);
  NS_ENSURE_SUCCESS(res, SEEK_FATAL_ERROR);

  // We've moved the read set, so reset decode.
  res = Reset();
  NS_ENSURE_SUCCESS(res, SEEK_FATAL_ERROR);

  // Check that the page the index thinks is exactly here is actually exactly
  // here. If not, the index is invalid.
  ogg_page page;
  int skippedBytes = 0;
  PageSyncResult syncres = PageSync(&mResource,
                                    &mOggState,
                                    false,
                                    keyframe.mKeyPoint.mOffset,
                                    mResource.GetLength(),
                                    &page,
                                    skippedBytes);
  NS_ENSURE_TRUE(syncres != PAGE_SYNC_ERROR, SEEK_FATAL_ERROR);
  if (syncres != PAGE_SYNC_OK || skippedBytes != 0) {
    LOG(LogLevel::Debug, ("Indexed-seek failure: Ogg Skeleton Index is invalid "
                       "or sync error after seek"));
    return RollbackIndexedSeek(tell);
  }
  uint32_t serial = ogg_page_serialno(&page);
  if (serial != keyframe.mSerial) {
    // Serialno of page at offset isn't what the index told us to expect.
    // Assume the index is invalid.
    return RollbackIndexedSeek(tell);
  }
  OggCodecState* codecState = mCodecStore.Get(serial);
  if (codecState && codecState->mActive &&
      ogg_stream_pagein(&codecState->mState, &page) != 0)
  {
    // Couldn't insert page into the ogg resource, or somehow the resource
    // is no longer active.
    return RollbackIndexedSeek(tell);
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
OggTrackDemuxer::Seek(TimeUnit aTime)
{
  // Seeks to aTime. Upon success, SeekPromise will be resolved with the
  // actual time seeked to. Typically the random access point time
  mQueuedSample = nullptr;
  TimeUnit seekTime = aTime;
  if (mParent->SeekInternal(aTime) == NS_OK) {
    RefPtr<MediaRawData> sample(NextSample());

    // Check what time we actually seeked to.
    if (sample != nullptr) {
      seekTime = TimeUnit::FromMicroseconds(sample->mTime);
      OGG_DEBUG("%p seeked to time %lld", this, seekTime.ToMicroseconds());
    }
    mQueuedSample = sample;

    return SeekPromise::CreateAndResolve(seekTime, __func__);
  } else {
    return SeekPromise::CreateAndReject(DemuxerFailureReason::DEMUXER_ERROR, __func__);
  }
}

RefPtr<MediaRawData>
OggTrackDemuxer::NextSample()
{
  if (mQueuedSample) {
    RefPtr<MediaRawData> nextSample = mQueuedSample;
    mQueuedSample = nullptr;
    return nextSample;
  }
  ogg_packet* packet = mParent->GetNextPacket(mType);
  if (!packet) {
    return nullptr;
  }
  // Check the eos state in case we need to look for chained streams.
  bool eos = packet->e_o_s;
  OggCodecState* state = mParent->GetTrackCodecState(mType);
  RefPtr<MediaRawData> data = state->PacketOutAsMediaRawData();;
  if (eos) {
    // We've encountered an end of bitstream packet; check for a chained
    // bitstream following this one.
    mParent->ReadOggChain();
  }
  return data;
}

RefPtr<OggTrackDemuxer::SamplesPromise>
OggTrackDemuxer::GetSamples(int32_t aNumSamples)
{
  RefPtr<SamplesHolder> samples = new SamplesHolder;
  if (!aNumSamples) {
    return SamplesPromise::CreateAndReject(DemuxerFailureReason::DEMUXER_ERROR, __func__);
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
    return SamplesPromise::CreateAndReject(DemuxerFailureReason::END_OF_STREAM, __func__);
  } else {
    return SamplesPromise::CreateAndResolve(samples, __func__);
  }
}

void
OggTrackDemuxer::Reset()
{
  mParent->ResetTrackState(mType);
  mQueuedSample = nullptr;
  TimeIntervals buffered = GetBuffered();
  if (buffered.Length()) {
    OGG_DEBUG("Seek to start point: %f", buffered.Start(0).ToSeconds());
    mParent->SeekInternal(buffered.Start(0));
  }
}

RefPtr<OggTrackDemuxer::SkipAccessPointPromise>
OggTrackDemuxer::SkipToNextRandomAccessPoint(TimeUnit aTimeThreshold)
{
  uint32_t parsed = 0;
  bool found = false;
  RefPtr<MediaRawData> sample;

  OGG_DEBUG("TimeThreshold: %f", aTimeThreshold.ToSeconds());
  while (!found && (sample = NextSample())) {
    parsed++;
    if (sample->mKeyframe && sample->mTime >= aTimeThreshold.ToMicroseconds()) {
      found = true;
      mQueuedSample = sample;
    }
  }
  if (found) {
    OGG_DEBUG("next sample: %f (parsed: %d)",
               TimeUnit::FromMicroseconds(sample->mTime).ToSeconds(),
               parsed);
    return SkipAccessPointPromise::CreateAndResolve(parsed, __func__);
  } else {
    SkipFailureHolder failure(DemuxerFailureReason::END_OF_STREAM, parsed);
    return SkipAccessPointPromise::CreateAndReject(Move(failure), __func__);
  }
}

TimeIntervals
OggTrackDemuxer::GetBuffered()
{
  return mParent->GetBuffered();
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
  uint32_t c =  p[0] +
               (p[1] << 8) +
               (p[2] << 16) +
               (p[3] << 24);
  return c;
}

int64_t
OggDemuxer::RangeStartTime(int64_t aOffset)
{
  int64_t position = mResource.Tell();
  nsresult res = mResource.Seek(nsISeekableStream::NS_SEEK_SET, aOffset);
  NS_ENSURE_SUCCESS(res, 0);
  int64_t startTime = 0;
  FindStartTime(startTime); // @fixme
  res = mResource.Seek(nsISeekableStream::NS_SEEK_SET, position);
  NS_ENSURE_SUCCESS(res, -1);
  return startTime;
}

struct nsDemuxerAutoOggSyncState {
  nsDemuxerAutoOggSyncState() {
    ogg_sync_init(&mState);
  }
  ~nsDemuxerAutoOggSyncState() {
    ogg_sync_clear(&mState);
  }
  ogg_sync_state mState;
};

int64_t
OggDemuxer::RangeEndTime(int64_t aEndOffset)
{
  int64_t position = mResource.Tell();
  int64_t endTime = RangeEndTime(0, aEndOffset, false);
  nsresult res = mResource.Seek(nsISeekableStream::NS_SEEK_SET, position);
  NS_ENSURE_SUCCESS(res, -1);
  return endTime;
}

int64_t
OggDemuxer::RangeEndTime(int64_t aStartOffset,
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
        res = mResource.GetResource()->ReadFromCache(buffer, readHead, bytesToRead);
        NS_ENSURE_SUCCESS(res, -1);
        bytesRead = bytesToRead;
      } else {
        NS_ASSERTION(readHead < aEndOffset,
                     "resource pos must be before range end");
        res = mResource.Seek(nsISeekableStream::NS_SEEK_SET, readHead);
        NS_ENSURE_SUCCESS(res, -1);
        res = mResource.Read(buffer, bytesToRead, &bytesRead);
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
OggDemuxer::GetSeekRanges(nsTArray<SeekRange>& aRanges)
{
  AutoPinned<MediaResource> resource(mResource.GetResource());
  MediaByteRangeSet cached;
  nsresult res = resource->GetCachedRanges(cached);
  NS_ENSURE_SUCCESS(res, res);

  for (uint32_t index = 0; index < cached.Length(); index++) {
    auto& range = cached[index];
    int64_t startTime = -1;
    int64_t endTime = -1;
    if (NS_FAILED(Reset())) {
      return NS_ERROR_FAILURE;
    }
    int64_t startOffset = range.mStart;
    int64_t endOffset = range.mEnd;
    startTime = RangeStartTime(startOffset);
    if (startTime != -1 &&
        ((endTime = RangeEndTime(endOffset)) != -1))
    {
      NS_WARN_IF_FALSE(startTime < endTime,
                       "Start time must be before end time");
      aRanges.AppendElement(SeekRange(startOffset,
                                      endOffset,
                                      startTime,
                                      endTime));
     }
  }
  if (NS_FAILED(Reset())) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

OggDemuxer::SeekRange
OggDemuxer::SelectSeekRange(const nsTArray<SeekRange>& ranges,
                            int64_t aTarget,
                            int64_t aStartTime,
                            int64_t aEndTime,
                            bool aExact)
{
  int64_t so = 0;
  int64_t eo = mResource.GetLength();
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
OggDemuxer::SeekInBufferedRange(int64_t aTarget,
                                int64_t aAdjustedTarget,
                                int64_t aStartTime,
                                int64_t aEndTime,
                                const nsTArray<SeekRange>& aRanges,
                                const SeekRange& aRange)
{
  OGG_DEBUG("Seeking in buffered data to %lld using bisection search", aTarget);
  if (HasVideo() || aAdjustedTarget >= aTarget) {
    // We know the exact byte range in which the target must lie. It must
    // be buffered in the media cache. Seek there.
    nsresult res = SeekBisection(aTarget, aRange, 0);
    if (NS_FAILED(res) || !HasVideo()) {
      return res;
    }

    // We have an active Theora bitstream. Peek the next Theora frame, and
    // extract its keyframe's time.
    DemuxUntilPacketAvailable(mTheoraState);
    ogg_packet* packet = mTheoraState->PacketPeek();
    if (packet && !mTheoraState->IsKeyframe(packet)) {
      // First post-seek frame isn't a keyframe, seek back to previous keyframe,
      // otherwise we'll get visual artifacts.
      NS_ASSERTION(packet->granulepos != -1, "Must have a granulepos");
      int shift = mTheoraState->mInfo.keyframe_granule_shift;
      int64_t keyframeGranulepos = (packet->granulepos >> shift) << shift;
      int64_t keyframeTime = mTheoraState->StartTime(keyframeGranulepos);
      SEEK_LOG(LogLevel::Debug, ("Keyframe for %lld is at %lld, seeking back to it",
                              frameTime, keyframeTime));
      aAdjustedTarget = std::min(aAdjustedTarget, keyframeTime);
    }
  }

  nsresult res = NS_OK;
  if (aAdjustedTarget < aTarget) {
    SeekRange k = SelectSeekRange(aRanges,
                                  aAdjustedTarget,
                                  aStartTime,
                                  aEndTime,
                                  false);
    res = SeekBisection(aAdjustedTarget, k, OGG_SEEK_FUZZ_USECS);
  }
  return res;
}

nsresult
OggDemuxer::SeekInUnbuffered(int64_t aTarget,
                             int64_t aStartTime,
                             int64_t aEndTime,
                             const nsTArray<SeekRange>& aRanges)
{
  OGG_DEBUG("Seeking in unbuffered data to %lld using bisection search", aTarget);

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
  if (HasVideo() && mTheoraState) {
    keyframeOffsetMs = mTheoraState->MaxKeyframeOffset();
  }
  // Add in the Opus pre-roll if necessary, as well.
  if (HasAudio() && mOpusState) {
    keyframeOffsetMs = std::max(keyframeOffsetMs, OGG_SEEK_OPUS_PREROLL);
  }
  int64_t seekTarget = std::max(aStartTime, aTarget - keyframeOffsetMs);
  // Minimize the bisection search space using the known timestamps from the
  // buffered ranges.
  SeekRange k = SelectSeekRange(aRanges, seekTarget, aStartTime, aEndTime, false);
  return SeekBisection(seekTarget, k, OGG_SEEK_FUZZ_USECS);
}

nsresult
OggDemuxer::SeekBisection(int64_t aTarget,
                          const SeekRange& aRange,
                          uint32_t aFuzz)
{
  nsresult res;

  if (aTarget == aRange.mTimeStart) {
    if (NS_FAILED(Reset())) {
      return NS_ERROR_FAILURE;
    }
    res = mResource.Seek(nsISeekableStream::NS_SEEK_SET, 0);
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
      if (NS_FAILED(Reset())) {
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
      PageSyncResult pageSyncResult = PageSync(&mResource,
                                               &mOggState,
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
        if (codecState && codecState->mActive) {
          int ret = ogg_stream_pagein(&codecState->mState, &page);
          NS_ENSURE_TRUE(ret == 0, NS_ERROR_FAILURE);
        }

        ogg_int64_t granulepos = ogg_page_granulepos(&page);

        if (HasAudio() && granulepos > 0 && audioTime == -1) {
          if (mVorbisState && serial == mVorbisState->mSerial) {
            audioTime = mVorbisState->Time(granulepos);
          } else if (mOpusState && serial == mOpusState->mSerial) {
            audioTime = mOpusState->Time(granulepos);
          }
        }

        if (HasVideo() &&
            granulepos > 0 &&
            serial == mTheoraState->mSerial &&
            videoTime == -1) {
          videoTime = mTheoraState->Time(granulepos);
        }

        if (pageOffset + pageLength >= endOffset) {
          // Hit end of readable data.
          break;
        }

        if (!ReadOggPage(&page)) {
          break;
        }

      } while ((HasAudio() && audioTime == -1) ||
               (HasVideo() && videoTime == -1));


      if ((HasAudio() && audioTime == -1) ||
          (HasVideo() && videoTime == -1))
      {
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
      granuleTime = std::max(audioTime, videoTime);
      NS_ASSERTION(granuleTime > 0, "Must get a granuletime");
      break;
    } // End of "until we determine time at guess offset" loop.

    if (interval == 0) {
      // Seek termination condition; we've found the page boundary of the
      // last page before the target, and the first page after the target.
      SEEK_LOG(LogLevel::Debug, ("Terminating seek at offset=%lld", startOffset));
      NS_ASSERTION(startTime < aTarget, "Start time must always be less than target");
      res = mResource.Seek(nsISeekableStream::NS_SEEK_SET, startOffset);
      NS_ENSURE_SUCCESS(res,res);
      if (NS_FAILED(Reset())) {
        return NS_ERROR_FAILURE;
      }
      break;
    }

    SEEK_LOG(LogLevel::Debug, ("Time at offset %lld is %lld", guess, granuleTime));
    if (granuleTime < seekTarget && granuleTime > seekLowerBound) {
      // We're within the fuzzy region in which we want to terminate the search.
      res = mResource.Seek(nsISeekableStream::NS_SEEK_SET, pageOffset);
      NS_ENSURE_SUCCESS(res,res);
      if (NS_FAILED(Reset())) {
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

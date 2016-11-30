/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"
#include "MediaDecoderStateMachine.h"
#include "AbstractMediaDecoder.h"
#include "MediaResource.h"
#include "OpusDecoder.h"
#include "WebMDemuxer.h"
#include "WebMBufferedParser.h"
#include "gfx2DGlue.h"
#include "mozilla/Atomics.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/SharedThreadPool.h"
#include "MediaDataDemuxer.h"
#include "nsAutoPtr.h"
#include "nsAutoRef.h"
#include "NesteggPacketHolder.h"
#include "XiphExtradata.h"
#include "prprf.h"           // leaving it for PR_vsnprintf()
#include "mozilla/Sprintf.h"

#include <algorithm>
#include <stdint.h>

#define VPX_DONT_DEFINE_STDINT_TYPES
#include "vpx/vp8dx.h"
#include "vpx/vpx_decoder.h"

#define WEBM_DEBUG(arg, ...) MOZ_LOG(gMediaDemuxerLog, mozilla::LogLevel::Debug, ("WebMDemuxer(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
extern mozilla::LazyLogModule gMediaDemuxerLog;

namespace mozilla {

using namespace gfx;

LazyLogModule gNesteggLog("Nestegg");

// How far ahead will we look when searching future keyframe. In microseconds.
// This value is based on what appears to be a reasonable value as most webm
// files encountered appear to have keyframes located < 4s.
#define MAX_LOOK_AHEAD 10000000

static Atomic<uint32_t> sStreamSourceID(0u);

// Functions for reading and seeking using WebMDemuxer required for
// nestegg_io. The 'user data' passed to these functions is the
// demuxer.
static int webmdemux_read(void* aBuffer, size_t aLength, void* aUserData)
{
  MOZ_ASSERT(aUserData);
  MOZ_ASSERT(aLength < UINT32_MAX);
  WebMDemuxer::NestEggContext* context =
    reinterpret_cast<WebMDemuxer::NestEggContext*>(aUserData);
  uint32_t count = aLength;
  if (context->IsMediaSource()) {
    int64_t length = context->GetEndDataOffset();
    int64_t position = context->GetResource()->Tell();
    MOZ_ASSERT(position <= context->GetResource()->GetLength());
    MOZ_ASSERT(position <= length);
    if (length >= 0 && count + position > length) {
      count = length - position;
    }
    MOZ_ASSERT(count <= aLength);
  }
  uint32_t bytes = 0;
  nsresult rv =
    context->GetResource()->Read(static_cast<char*>(aBuffer), count, &bytes);
  bool eof = bytes < aLength;
  return NS_FAILED(rv) ? -1 : eof ? 0 : 1;
}

static int webmdemux_seek(int64_t aOffset, int aWhence, void* aUserData)
{
  MOZ_ASSERT(aUserData);
  WebMDemuxer::NestEggContext* context = reinterpret_cast<WebMDemuxer::NestEggContext*>(aUserData);
  nsresult rv = context->GetResource()->Seek(aWhence, aOffset);
  return NS_SUCCEEDED(rv) ? 0 : -1;
}

static int64_t webmdemux_tell(void* aUserData)
{
  MOZ_ASSERT(aUserData);
  WebMDemuxer::NestEggContext* context = reinterpret_cast<WebMDemuxer::NestEggContext*>(aUserData);
  return context->GetResource()->Tell();
}

static void webmdemux_log(nestegg* aContext,
                          unsigned int aSeverity,
                          char const* aFormat, ...)
{
  if (!MOZ_LOG_TEST(gNesteggLog, LogLevel::Debug)) {
    return;
  }

  va_list args;
  char msg[256];
  const char* sevStr;

  switch(aSeverity) {
    case NESTEGG_LOG_DEBUG:
      sevStr = "DBG";
      break;
    case NESTEGG_LOG_INFO:
      sevStr = "INF";
      break;
    case NESTEGG_LOG_WARNING:
      sevStr = "WRN";
      break;
    case NESTEGG_LOG_ERROR:
      sevStr = "ERR";
      break;
    case NESTEGG_LOG_CRITICAL:
      sevStr = "CRT";
      break;
    default:
      sevStr = "UNK";
      break;
  }

  va_start(args, aFormat);

  SprintfLiteral(msg, "%p [Nestegg-%s] ", aContext, sevStr);
  PR_vsnprintf(msg+strlen(msg), sizeof(msg)-strlen(msg), aFormat, args);
  MOZ_LOG(gNesteggLog, LogLevel::Debug, (msg));

  va_end(args);
}

WebMDemuxer::NestEggContext::~NestEggContext()
{
  if (mContext) {
    nestegg_destroy(mContext);
  }
}

int
WebMDemuxer::NestEggContext::Init()
{
  nestegg_io io;
  io.read = webmdemux_read;
  io.seek = webmdemux_seek;
  io.tell = webmdemux_tell;
  io.userdata = this;

  // While reading the metadata, we do not really care about which nestegg
  // context is being used so long that they are both initialised.
  // For reading the metadata however, we will use mVideoContext.
  return nestegg_init(&mContext, io, &webmdemux_log,
                      mParent->IsMediaSource() ? mResource.GetLength() : -1);
}

WebMDemuxer::WebMDemuxer(MediaResource* aResource)
  : WebMDemuxer(aResource, false)
{
}

WebMDemuxer::WebMDemuxer(MediaResource* aResource, bool aIsMediaSource)
  : mVideoContext(this, aResource)
  , mAudioContext(this, aResource)
  , mBufferedState(nullptr)
  , mInitData(nullptr)
  , mVideoTrack(0)
  , mAudioTrack(0)
  , mSeekPreroll(0)
  , mAudioCodec(-1)
  , mVideoCodec(-1)
  , mHasVideo(false)
  , mHasAudio(false)
  , mNeedReIndex(true)
  , mLastWebMBlockOffset(-1)
  , mIsMediaSource(aIsMediaSource)
{
}

WebMDemuxer::~WebMDemuxer()
{
  Reset(TrackInfo::kVideoTrack);
  Reset(TrackInfo::kAudioTrack);
}

RefPtr<WebMDemuxer::InitPromise>
WebMDemuxer::Init()
{
  InitBufferedState();

  if (NS_FAILED(ReadMetadata())) {
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_METADATA_ERR, __func__);
  }

  if (!GetNumberTracks(TrackInfo::kAudioTrack) &&
      !GetNumberTracks(TrackInfo::kVideoTrack)) {
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_METADATA_ERR, __func__);
  }

  return InitPromise::CreateAndResolve(NS_OK, __func__);
}

void
WebMDemuxer::InitBufferedState()
{
  MOZ_ASSERT(!mBufferedState);
  mBufferedState = new WebMBufferedState;
}

bool
WebMDemuxer::HasTrackType(TrackInfo::TrackType aType) const
{
  return !!GetNumberTracks(aType);
}

uint32_t
WebMDemuxer::GetNumberTracks(TrackInfo::TrackType aType) const
{
  switch(aType) {
    case TrackInfo::kAudioTrack:
      return mHasAudio ? 1 : 0;
    case TrackInfo::kVideoTrack:
      return mHasVideo ? 1 : 0;
    default:
      return 0;
  }
}

UniquePtr<TrackInfo>
WebMDemuxer::GetTrackInfo(TrackInfo::TrackType aType,
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
WebMDemuxer::GetTrackDemuxer(TrackInfo::TrackType aType, uint32_t aTrackNumber)
{
  if (GetNumberTracks(aType) <= aTrackNumber) {
    return nullptr;
  }
  RefPtr<WebMTrackDemuxer> e =
    new WebMTrackDemuxer(this, aType, aTrackNumber);
  mDemuxers.AppendElement(e);

  return e.forget();
}

nsresult
WebMDemuxer::Reset(TrackInfo::TrackType aType)
{
  if (aType == TrackInfo::kVideoTrack) {
    mVideoPackets.Reset();
  } else {
    mAudioPackets.Reset();
  }
  return NS_OK;
}

nsresult
WebMDemuxer::ReadMetadata()
{
  int r = mVideoContext.Init();
  if (r == -1) {
    return NS_ERROR_FAILURE;
  }
  if (mAudioContext.Init() == -1) {
    return NS_ERROR_FAILURE;
  }

  // For reading the metadata we can only use the video resource/context.
  MediaResourceIndex& resource = Resource(TrackInfo::kVideoTrack);
  nestegg* context = Context(TrackInfo::kVideoTrack);

  {
    // Check how much data nestegg read and force feed it to BufferedState.
    RefPtr<MediaByteBuffer> buffer = resource.MediaReadAt(0, resource.Tell());
    if (!buffer) {
      return NS_ERROR_FAILURE;
    }
    mBufferedState->NotifyDataArrived(buffer->Elements(), buffer->Length(), 0);
    if (mBufferedState->GetInitEndOffset() < 0) {
      return NS_ERROR_FAILURE;
    }
    MOZ_ASSERT(mBufferedState->GetInitEndOffset() <= resource.Tell());
  }
  mInitData = resource.MediaReadAt(0, mBufferedState->GetInitEndOffset());
  if (!mInitData ||
      mInitData->Length() != size_t(mBufferedState->GetInitEndOffset())) {
    return NS_ERROR_FAILURE;
  }

  unsigned int ntracks = 0;
  r = nestegg_track_count(context, &ntracks);
  if (r == -1) {
    return NS_ERROR_FAILURE;
  }

  for (unsigned int track = 0; track < ntracks; ++track) {
    int id = nestegg_track_codec_id(context, track);
    if (id == -1) {
      return NS_ERROR_FAILURE;
    }
    int type = nestegg_track_type(context, track);
    if (type == NESTEGG_TRACK_VIDEO && !mHasVideo) {
      nestegg_video_params params;
      r = nestegg_track_video_params(context, track, &params);
      if (r == -1) {
        return NS_ERROR_FAILURE;
      }
      mVideoCodec = nestegg_track_codec_id(context, track);
      switch(mVideoCodec) {
        case NESTEGG_CODEC_VP8:
          mInfo.mVideo.mMimeType = "video/webm; codecs=vp8";
          break;
        case NESTEGG_CODEC_VP9:
          mInfo.mVideo.mMimeType = "video/webm; codecs=vp9";
          break;
        default:
          NS_WARNING("Unknown WebM video codec");
          return NS_ERROR_FAILURE;
      }
      // Picture region, taking into account cropping, before scaling
      // to the display size.
      unsigned int cropH = params.crop_right + params.crop_left;
      unsigned int cropV = params.crop_bottom + params.crop_top;
      nsIntRect pictureRect(params.crop_left,
                            params.crop_top,
                            params.width - cropH,
                            params.height - cropV);

      // If the cropping data appears invalid then use the frame data
      if (pictureRect.width <= 0 ||
          pictureRect.height <= 0 ||
          pictureRect.x < 0 ||
          pictureRect.y < 0) {
        pictureRect.x = 0;
        pictureRect.y = 0;
        pictureRect.width = params.width;
        pictureRect.height = params.height;
      }

      // Validate the container-reported frame and pictureRect sizes. This
      // ensures that our video frame creation code doesn't overflow.
      nsIntSize displaySize(params.display_width, params.display_height);
      nsIntSize frameSize(params.width, params.height);
      if (!IsValidVideoRegion(frameSize, pictureRect, displaySize)) {
        // Video track's frame sizes will overflow. Ignore the video track.
        continue;
      }

      mVideoTrack = track;
      mHasVideo = true;

      mInfo.mVideo.mDisplay = displaySize;
      mInfo.mVideo.mImage = frameSize;
      mInfo.mVideo.SetImageRect(pictureRect);
      mInfo.mVideo.SetAlpha(params.alpha_mode);

      switch (params.stereo_mode) {
        case NESTEGG_VIDEO_MONO:
          mInfo.mVideo.mStereoMode = StereoMode::MONO;
          break;
        case NESTEGG_VIDEO_STEREO_LEFT_RIGHT:
          mInfo.mVideo.mStereoMode = StereoMode::LEFT_RIGHT;
          break;
        case NESTEGG_VIDEO_STEREO_BOTTOM_TOP:
          mInfo.mVideo.mStereoMode = StereoMode::BOTTOM_TOP;
          break;
        case NESTEGG_VIDEO_STEREO_TOP_BOTTOM:
          mInfo.mVideo.mStereoMode = StereoMode::TOP_BOTTOM;
          break;
        case NESTEGG_VIDEO_STEREO_RIGHT_LEFT:
          mInfo.mVideo.mStereoMode = StereoMode::RIGHT_LEFT;
          break;
      }
      uint64_t duration = 0;
      r = nestegg_duration(context, &duration);
      if (!r) {
        mInfo.mVideo.mDuration = media::TimeUnit::FromNanoseconds(duration).ToMicroseconds();
      }
      mInfo.mVideo.mCrypto = GetTrackCrypto(TrackInfo::kVideoTrack, track);
      if (mInfo.mVideo.mCrypto.mValid) {
        mCrypto.AddInitData(NS_LITERAL_STRING("webm"), mInfo.mVideo.mCrypto.mKeyId);
      }
    } else if (type == NESTEGG_TRACK_AUDIO && !mHasAudio) {
      nestegg_audio_params params;
      r = nestegg_track_audio_params(context, track, &params);
      if (r == -1) {
        return NS_ERROR_FAILURE;
      }

      mAudioTrack = track;
      mHasAudio = true;
      mAudioCodec = nestegg_track_codec_id(context, track);
      if (mAudioCodec == NESTEGG_CODEC_VORBIS) {
        mInfo.mAudio.mMimeType = "audio/webm; codecs=vorbis";
      } else if (mAudioCodec == NESTEGG_CODEC_OPUS) {
        mInfo.mAudio.mMimeType = "audio/webm; codecs=opus";
        OpusDataDecoder::AppendCodecDelay(mInfo.mAudio.mCodecSpecificConfig,
            media::TimeUnit::FromNanoseconds(params.codec_delay).ToMicroseconds());
      }
      mSeekPreroll = params.seek_preroll;
      mInfo.mAudio.mRate = params.rate;
      mInfo.mAudio.mChannels = params.channels;

      unsigned int nheaders = 0;
      r = nestegg_track_codec_data_count(context, track, &nheaders);
      if (r == -1) {
        return NS_ERROR_FAILURE;
      }

      AutoTArray<const unsigned char*,4> headers;
      AutoTArray<size_t,4> headerLens;
      for (uint32_t header = 0; header < nheaders; ++header) {
        unsigned char* data = 0;
        size_t length = 0;
        r = nestegg_track_codec_data(context, track, header, &data, &length);
        if (r == -1) {
          return NS_ERROR_FAILURE;
        }
        headers.AppendElement(data);
        headerLens.AppendElement(length);
      }

      // Vorbis has 3 headers, convert to Xiph extradata format to send them to
      // the demuxer.
      // TODO: This is already the format WebM stores them in. Would be nice
      // to avoid having libnestegg split them only for us to pack them again,
      // but libnestegg does not give us an API to access this data directly.
      if (nheaders > 1) {
        if (!XiphHeadersToExtradata(mInfo.mAudio.mCodecSpecificConfig,
                                    headers, headerLens)) {
          return NS_ERROR_FAILURE;
        }
      }
      else {
        mInfo.mAudio.mCodecSpecificConfig->AppendElements(headers[0],
                                                          headerLens[0]);
      }
      uint64_t duration = 0;
      r = nestegg_duration(context, &duration);
      if (!r) {
        mInfo.mAudio.mDuration = media::TimeUnit::FromNanoseconds(duration).ToMicroseconds();
      }
      mInfo.mAudio.mCrypto = GetTrackCrypto(TrackInfo::kAudioTrack, track);
      if (mInfo.mAudio.mCrypto.mValid) {
        mCrypto.AddInitData(NS_LITERAL_STRING("webm"), mInfo.mAudio.mCrypto.mKeyId);
      }
    }
  }
  return NS_OK;
}

bool
WebMDemuxer::IsSeekable() const
{
  return Context(TrackInfo::kVideoTrack) &&
         nestegg_has_cues(Context(TrackInfo::kVideoTrack));
}

bool
WebMDemuxer::IsSeekableOnlyInBufferedRanges() const
{
  return Context(TrackInfo::kVideoTrack) &&
         !nestegg_has_cues(Context(TrackInfo::kVideoTrack));
}

void
WebMDemuxer::EnsureUpToDateIndex()
{
  if (!mNeedReIndex || !mInitData) {
    return;
  }
  AutoPinned<MediaResource> resource(
    Resource(TrackInfo::kVideoTrack).GetResource());
  MediaByteRangeSet byteRanges;
  nsresult rv = resource->GetCachedRanges(byteRanges);
  if (NS_FAILED(rv) || !byteRanges.Length()) {
    return;
  }
  mBufferedState->UpdateIndex(byteRanges, resource);

  mNeedReIndex = false;

  if (!mIsMediaSource) {
    return;
  }
  mLastWebMBlockOffset = mBufferedState->GetLastBlockOffset();
  MOZ_ASSERT(mLastWebMBlockOffset <= resource->GetLength());
}

void
WebMDemuxer::NotifyDataArrived()
{
  WEBM_DEBUG("");
  mNeedReIndex = true;
}

void
WebMDemuxer::NotifyDataRemoved()
{
  mBufferedState->Reset();
  if (mInitData) {
    mBufferedState->NotifyDataArrived(mInitData->Elements(), mInitData->Length(), 0);
  }
  mNeedReIndex = true;
}

UniquePtr<EncryptionInfo>
WebMDemuxer::GetCrypto()
{
  return mCrypto.IsEncrypted() ? MakeUnique<EncryptionInfo>(mCrypto) : nullptr;
}

CryptoTrack
WebMDemuxer::GetTrackCrypto(TrackInfo::TrackType aType, size_t aTrackNumber) {
  const int WEBM_IV_SIZE = 16;
  const unsigned char * contentEncKeyId;
  size_t contentEncKeyIdLength;
  CryptoTrack crypto;
  nestegg* context = Context(aType);

  int r = nestegg_track_content_enc_key_id(context, aTrackNumber, &contentEncKeyId, &contentEncKeyIdLength);

  if (r == -1) {
    WEBM_DEBUG("nestegg_track_content_enc_key_id failed r=%d", r);
    return crypto;
  }

  uint32_t i;
  nsTArray<uint8_t> initData;
  for (i = 0; i < contentEncKeyIdLength; i++) {
    initData.AppendElement(contentEncKeyId[i]);
  }

  if (!initData.IsEmpty()) {
    crypto.mValid = true;
    // crypto.mMode is not used for WebMs
    crypto.mIVSize = WEBM_IV_SIZE;
    crypto.mKeyId = Move(initData);
  }

  return crypto;
}

bool
WebMDemuxer::GetNextPacket(TrackInfo::TrackType aType, MediaRawDataQueue *aSamples)
{
  if (mIsMediaSource) {
    // To ensure mLastWebMBlockOffset is properly up to date.
    EnsureUpToDateIndex();
  }

  RefPtr<NesteggPacketHolder> holder(NextPacket(aType));

  if (!holder) {
    return false;
  }

  int r = 0;
  unsigned int count = 0;
  r = nestegg_packet_count(holder->Packet(), &count);
  if (r == -1) {
    return false;
  }
  int64_t tstamp = holder->Timestamp();
  int64_t duration = holder->Duration();

  // The end time of this frame is the start time of the next frame. Fetch
  // the timestamp of the next packet for this track.  If we've reached the
  // end of the resource, use the file's duration as the end time of this
  // video frame.
  int64_t next_tstamp = INT64_MIN;
  if (aType == TrackInfo::kAudioTrack) {
    RefPtr<NesteggPacketHolder> next_holder(NextPacket(aType));
    if (next_holder) {
      next_tstamp = next_holder->Timestamp();
      PushAudioPacket(next_holder);
    } else if (duration >= 0) {
      next_tstamp = tstamp + duration;
    } else if (!mIsMediaSource ||
               (mIsMediaSource && mLastAudioFrameTime.isSome())) {
      next_tstamp = tstamp;
      next_tstamp += tstamp - mLastAudioFrameTime.refOr(0);
    } else {
      PushAudioPacket(holder);
    }
    mLastAudioFrameTime = Some(tstamp);
  } else if (aType == TrackInfo::kVideoTrack) {
    RefPtr<NesteggPacketHolder> next_holder(NextPacket(aType));
    if (next_holder) {
      next_tstamp = next_holder->Timestamp();
      PushVideoPacket(next_holder);
    } else if (duration >= 0) {
      next_tstamp = tstamp + duration;
    } else if (!mIsMediaSource ||
               (mIsMediaSource && mLastVideoFrameTime.isSome())) {
      next_tstamp = tstamp;
      next_tstamp += tstamp - mLastVideoFrameTime.refOr(0);
    } else {
      PushVideoPacket(holder);
    }
    mLastVideoFrameTime = Some(tstamp);
  }

  if (mIsMediaSource && next_tstamp == INT64_MIN) {
    return false;
  }

  int64_t discardPadding = 0;
  if (aType == TrackInfo::kAudioTrack) {
    (void) nestegg_packet_discard_padding(holder->Packet(), &discardPadding);
  }

  int packetEncryption = nestegg_packet_encryption(holder->Packet());

  for (uint32_t i = 0; i < count; ++i) {
    unsigned char* data;
    size_t length;
    r = nestegg_packet_data(holder->Packet(), i, &data, &length);
    if (r == -1) {
      WEBM_DEBUG("nestegg_packet_data failed r=%d", r);
      return false;
    }
    unsigned char* alphaData;
    size_t alphaLength = 0;
    // Check packets for alpha information if file has declared alpha frames
    // may be present.
    if (mInfo.mVideo.HasAlpha()) {
      r = nestegg_packet_additional_data(holder->Packet(),
                                         1,
                                         &alphaData,
                                         &alphaLength);
      if (r == -1) {
        WEBM_DEBUG(
          "nestegg_packet_additional_data failed to retrieve alpha data r=%d",
          r);
      }
    }
    bool isKeyframe = false;
    if (aType == TrackInfo::kAudioTrack) {
      isKeyframe = true;
    } else if (aType == TrackInfo::kVideoTrack) {
      if (packetEncryption == NESTEGG_PACKET_HAS_SIGNAL_BYTE_ENCRYPTED) {
        // Packet is encrypted, can't peek, use packet info
        isKeyframe = nestegg_packet_has_keyframe(holder->Packet()) == NESTEGG_PACKET_HAS_KEYFRAME_TRUE;
      } else {
        vpx_codec_stream_info_t si;
        PodZero(&si);
        si.sz = sizeof(si);
        switch (mVideoCodec) {
        case NESTEGG_CODEC_VP8:
          vpx_codec_peek_stream_info(vpx_codec_vp8_dx(), data, length, &si);
          break;
        case NESTEGG_CODEC_VP9:
          vpx_codec_peek_stream_info(vpx_codec_vp9_dx(), data, length, &si);
          break;
        }
        isKeyframe = si.is_kf;
        if (isKeyframe) {
          // We only look for resolution changes on keyframes for both VP8 and
          // VP9. Other resolution changes are invalid.
          if (mLastSeenFrameWidth.isSome() && mLastSeenFrameHeight.isSome() &&
            (si.w != mLastSeenFrameWidth.value() ||
              si.h != mLastSeenFrameHeight.value())) {
            mInfo.mVideo.mDisplay = nsIntSize(si.w, si.h);
            mSharedVideoTrackInfo = new SharedTrackInfo(mInfo.mVideo, ++sStreamSourceID);
          }
          mLastSeenFrameWidth = Some(si.w);
          mLastSeenFrameHeight = Some(si.h);
        }
      }
    }

    WEBM_DEBUG("push sample tstamp: %ld next_tstamp: %ld length: %ld kf: %d",
               tstamp, next_tstamp, length, isKeyframe);
    RefPtr<MediaRawData> sample;
    if (mInfo.mVideo.HasAlpha() && alphaLength != 0) {
      sample = new MediaRawData(data, length, alphaData, alphaLength);
    } else {
      sample = new MediaRawData(data, length);
    }
    sample->mTimecode = tstamp;
    sample->mTime = tstamp;
    sample->mDuration = next_tstamp - tstamp;
    sample->mOffset = holder->Offset();
    sample->mKeyframe = isKeyframe;
    if (discardPadding && i == count - 1) {
      CheckedInt64 discardFrames;
      if (discardPadding < 0) {
        // This is an invalid value as discard padding should never be negative.
        // Set to maximum value so that the decoder will reject it as it's
        // greater than the number of frames available.
        discardFrames = INT32_MAX;
        WEBM_DEBUG("Invalid negative discard padding");
      } else {
        discardFrames = TimeUnitToFrames(
          media::TimeUnit::FromNanoseconds(discardPadding), mInfo.mAudio.mRate);
      }
      if (discardFrames.isValid()) {
        sample->mDiscardPadding = discardFrames.value();
      }
    }

    if (packetEncryption == NESTEGG_PACKET_HAS_SIGNAL_BYTE_UNENCRYPTED ||
        packetEncryption == NESTEGG_PACKET_HAS_SIGNAL_BYTE_ENCRYPTED) {
      nsAutoPtr<MediaRawDataWriter> writer(sample->CreateWriter());
      unsigned char const* iv;
      size_t ivLength;
      nestegg_packet_iv(holder->Packet(), &iv, &ivLength);
      writer->mCrypto.mValid = true;
      writer->mCrypto.mIVSize = ivLength;
      if (ivLength == 0) {
        // Frame is not encrypted
        writer->mCrypto.mPlainSizes.AppendElement(length);
        writer->mCrypto.mEncryptedSizes.AppendElement(0);
      } else {
        // Frame is encrypted
        writer->mCrypto.mIV.AppendElements(iv, 8);
        // Iv from a sample is 64 bits, must be padded with 64 bits more 0s
        // in compliance with spec
        for (uint32_t i = 0; i < 8; i++) {
          writer->mCrypto.mIV.AppendElement(0);
        }
        writer->mCrypto.mPlainSizes.AppendElement(0);
        writer->mCrypto.mEncryptedSizes.AppendElement(length);
      }
    }
    if (aType == TrackInfo::kVideoTrack) {
      sample->mTrackInfo = mSharedVideoTrackInfo;
    }
    aSamples->Push(sample);
  }
  return true;
}

RefPtr<NesteggPacketHolder>
WebMDemuxer::NextPacket(TrackInfo::TrackType aType)
{
  bool isVideo = aType == TrackInfo::kVideoTrack;

  // Flag to indicate that we do need to playback these types of
  // packets.
  bool hasType = isVideo ? mHasVideo : mHasAudio;

  if (!hasType) {
    return nullptr;
  }

  // The packet queue for the type that we are interested in.
  WebMPacketQueue &packets = isVideo ? mVideoPackets : mAudioPackets;

  if (packets.GetSize() > 0) {
    return packets.PopFront();
  }

  // Track we are interested in
  uint32_t ourTrack = isVideo ? mVideoTrack : mAudioTrack;

  do {
    RefPtr<NesteggPacketHolder> holder = DemuxPacket(aType);
    if (!holder) {
      return nullptr;
    }

    if (ourTrack == holder->Track()) {
      return holder;
    }
  } while (true);
}

RefPtr<NesteggPacketHolder>
WebMDemuxer::DemuxPacket(TrackInfo::TrackType aType)
{
  nestegg_packet* packet;
  int r = nestegg_read_packet(Context(aType), &packet);
  if (r == 0) {
    nestegg_read_reset(Context(aType));
    return nullptr;
  } else if (r < 0) {
    return nullptr;
  }

  unsigned int track = 0;
  r = nestegg_packet_track(packet, &track);
  if (r == -1) {
    return nullptr;
  }

  int64_t offset = Resource(aType).Tell();
  RefPtr<NesteggPacketHolder> holder = new NesteggPacketHolder();
  if (!holder->Init(packet, offset, track, false)) {
    return nullptr;
  }

  return holder;
}

void
WebMDemuxer::PushAudioPacket(NesteggPacketHolder* aItem)
{
  mAudioPackets.PushFront(aItem);
}

void
WebMDemuxer::PushVideoPacket(NesteggPacketHolder* aItem)
{
  mVideoPackets.PushFront(aItem);
}

nsresult
WebMDemuxer::SeekInternal(TrackInfo::TrackType aType,
                          const media::TimeUnit& aTarget)
{
  EnsureUpToDateIndex();
  uint32_t trackToSeek = mHasVideo ? mVideoTrack : mAudioTrack;
  uint64_t target = aTarget.ToNanoseconds();

  if (NS_FAILED(Reset(aType))) {
    return NS_ERROR_FAILURE;
  }

  if (mSeekPreroll) {
    uint64_t startTime = 0;
    if (!mBufferedState->GetStartTime(&startTime)) {
      startTime = 0;
    }
    WEBM_DEBUG("Seek Target: %f",
               media::TimeUnit::FromNanoseconds(target).ToSeconds());
    if (target < mSeekPreroll || target - mSeekPreroll < startTime) {
      target = startTime;
    } else {
      target -= mSeekPreroll;
    }
    WEBM_DEBUG("SeekPreroll: %f StartTime: %f Adjusted Target: %f",
               media::TimeUnit::FromNanoseconds(mSeekPreroll).ToSeconds(),
               media::TimeUnit::FromNanoseconds(startTime).ToSeconds(),
               media::TimeUnit::FromNanoseconds(target).ToSeconds());
  }
  int r = nestegg_track_seek(Context(aType), trackToSeek, target);
  if (r == -1) {
    WEBM_DEBUG("track_seek for track %u to %f failed, r=%d", trackToSeek,
               media::TimeUnit::FromNanoseconds(target).ToSeconds(), r);
    // Try seeking directly based on cluster information in memory.
    int64_t offset = 0;
    bool rv = mBufferedState->GetOffsetForTime(target, &offset);
    if (!rv) {
      WEBM_DEBUG("mBufferedState->GetOffsetForTime failed too");
      return NS_ERROR_FAILURE;
    }

    r = nestegg_offset_seek(Context(aType), offset);
    if (r == -1) {
      WEBM_DEBUG("and nestegg_offset_seek to %" PRIu64 " failed", offset);
      return NS_ERROR_FAILURE;
    }
    WEBM_DEBUG("got offset from buffered state: %" PRIu64 "", offset);
  }

  if (aType == TrackInfo::kAudioTrack) {
    mLastAudioFrameTime.reset();
  } else {
    mLastVideoFrameTime.reset();
  }

  return NS_OK;
}

media::TimeIntervals
WebMDemuxer::GetBuffered()
{
  EnsureUpToDateIndex();
  AutoPinned<MediaResource> resource(
    Resource(TrackInfo::kVideoTrack).GetResource());

  media::TimeIntervals buffered;

  MediaByteRangeSet ranges;
  nsresult rv = resource->GetCachedRanges(ranges);
  if (NS_FAILED(rv)) {
    return media::TimeIntervals();
  }
  uint64_t duration = 0;
  uint64_t startOffset = 0;
  if (!nestegg_duration(Context(TrackInfo::kVideoTrack), &duration)) {
    if(mBufferedState->GetStartTime(&startOffset)) {
      duration += startOffset;
    }
    WEBM_DEBUG("Duration: %f StartTime: %f",
               media::TimeUnit::FromNanoseconds(duration).ToSeconds(),
               media::TimeUnit::FromNanoseconds(startOffset).ToSeconds());
  }
  for (uint32_t index = 0; index < ranges.Length(); index++) {
    uint64_t start, end;
    bool rv = mBufferedState->CalculateBufferedForRange(ranges[index].mStart,
                                                        ranges[index].mEnd,
                                                        &start, &end);
    if (rv) {
      NS_ASSERTION(startOffset <= start,
          "startOffset negative or larger than start time");

      if (duration && end > duration) {
        WEBM_DEBUG("limit range to duration, end: %f duration: %f",
                   media::TimeUnit::FromNanoseconds(end).ToSeconds(),
                   media::TimeUnit::FromNanoseconds(duration).ToSeconds());
        end = duration;
      }
      media::TimeUnit startTime = media::TimeUnit::FromNanoseconds(start);
      media::TimeUnit endTime = media::TimeUnit::FromNanoseconds(end);
      WEBM_DEBUG("add range %f-%f", startTime.ToSeconds(), endTime.ToSeconds());
      buffered += media::TimeInterval(startTime, endTime);
    }
  }
  return buffered;
}

bool WebMDemuxer::GetOffsetForTime(uint64_t aTime, int64_t* aOffset)
{
  EnsureUpToDateIndex();
  return mBufferedState && mBufferedState->GetOffsetForTime(aTime, aOffset);
}


//WebMTrackDemuxer
WebMTrackDemuxer::WebMTrackDemuxer(WebMDemuxer* aParent,
                                   TrackInfo::TrackType aType,
                                   uint32_t aTrackNumber)
  : mParent(aParent)
  , mType(aType)
  , mNeedKeyframe(true)
{
  mInfo = mParent->GetTrackInfo(aType, aTrackNumber);
  MOZ_ASSERT(mInfo);
}

WebMTrackDemuxer::~WebMTrackDemuxer()
{
  mSamples.Reset();
}

UniquePtr<TrackInfo>
WebMTrackDemuxer::GetInfo() const
{
  return mInfo->Clone();
}

RefPtr<WebMTrackDemuxer::SeekPromise>
WebMTrackDemuxer::Seek(const media::TimeUnit& aTime)
{
  // Seeks to aTime. Upon success, SeekPromise will be resolved with the
  // actual time seeked to. Typically the random access point time

  media::TimeUnit seekTime = aTime;
  mSamples.Reset();
  mParent->SeekInternal(mType, aTime);
  mParent->GetNextPacket(mType, &mSamples);
  mNeedKeyframe = true;

  // Check what time we actually seeked to.
  if (mSamples.GetSize() > 0) {
    const RefPtr<MediaRawData>& sample = mSamples.First();
    seekTime = media::TimeUnit::FromMicroseconds(sample->mTime);
  }
  SetNextKeyFrameTime();

  return SeekPromise::CreateAndResolve(seekTime, __func__);
}

RefPtr<MediaRawData>
WebMTrackDemuxer::NextSample()
{
  while (mSamples.GetSize() < 1 && mParent->GetNextPacket(mType, &mSamples)) {
  }
  if (mSamples.GetSize()) {
    return mSamples.PopFront();
  }
  return nullptr;
}

RefPtr<WebMTrackDemuxer::SamplesPromise>
WebMTrackDemuxer::GetSamples(int32_t aNumSamples)
{
  RefPtr<SamplesHolder> samples = new SamplesHolder;
  MOZ_ASSERT(aNumSamples);

  while (aNumSamples) {
    RefPtr<MediaRawData> sample(NextSample());
    if (!sample) {
      break;
    }
    if (mNeedKeyframe && !sample->mKeyframe) {
      continue;
    }
    mNeedKeyframe = false;
    samples->mSamples.AppendElement(sample);
    aNumSamples--;
  }

  if (samples->mSamples.IsEmpty()) {
    return SamplesPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_END_OF_STREAM, __func__);
  } else {
    UpdateSamples(samples->mSamples);
    return SamplesPromise::CreateAndResolve(samples, __func__);
  }
}

void
WebMTrackDemuxer::SetNextKeyFrameTime()
{
  if (mType != TrackInfo::kVideoTrack || mParent->IsMediaSource()) {
    return;
  }

  int64_t frameTime = -1;

  mNextKeyframeTime.reset();

  MediaRawDataQueue skipSamplesQueue;
  bool foundKeyframe = false;
  while (!foundKeyframe && mSamples.GetSize()) {
    RefPtr<MediaRawData> sample = mSamples.PopFront();
    if (sample->mKeyframe) {
      frameTime = sample->mTime;
      foundKeyframe = true;
    }
    skipSamplesQueue.Push(sample.forget());
  }
  Maybe<int64_t> startTime;
  if (skipSamplesQueue.GetSize()) {
    const RefPtr<MediaRawData>& sample = skipSamplesQueue.First();
    startTime.emplace(sample->mTimecode);
  }
  // Demux and buffer frames until we find a keyframe.
  RefPtr<MediaRawData> sample;
  while (!foundKeyframe && (sample = NextSample())) {
    if (sample->mKeyframe) {
      frameTime = sample->mTime;
      foundKeyframe = true;
    }
    int64_t sampleTimecode = sample->mTimecode;
    skipSamplesQueue.Push(sample.forget());
    if (!startTime) {
      startTime.emplace(sampleTimecode);
    } else if (!foundKeyframe &&
               sampleTimecode > startTime.ref() + MAX_LOOK_AHEAD) {
      WEBM_DEBUG("Couldn't find keyframe in a reasonable time, aborting");
      break;
    }
  }
  // We may have demuxed more than intended, so ensure that all frames are kept
  // in the right order.
  mSamples.PushFront(Move(skipSamplesQueue));

  if (frameTime != -1) {
    mNextKeyframeTime.emplace(media::TimeUnit::FromMicroseconds(frameTime));
    WEBM_DEBUG("Next Keyframe %f (%u queued %.02fs)",
               mNextKeyframeTime.value().ToSeconds(),
               uint32_t(mSamples.GetSize()),
               media::TimeUnit::FromMicroseconds(mSamples.Last()->mTimecode - mSamples.First()->mTimecode).ToSeconds());
  } else {
    WEBM_DEBUG("Couldn't determine next keyframe time  (%u queued)",
               uint32_t(mSamples.GetSize()));
  }
}

void
WebMTrackDemuxer::Reset()
{
  mSamples.Reset();
  media::TimeIntervals buffered = GetBuffered();
  mNeedKeyframe = true;
  if (buffered.Length()) {
    WEBM_DEBUG("Seek to start point: %f", buffered.Start(0).ToSeconds());
    mParent->SeekInternal(mType, buffered.Start(0));
    SetNextKeyFrameTime();
  } else {
    mNextKeyframeTime.reset();
  }
}

void
WebMTrackDemuxer::UpdateSamples(nsTArray<RefPtr<MediaRawData>>& aSamples)
{
  for (const auto& sample : aSamples) {
    if (sample->mCrypto.mValid) {
      nsAutoPtr<MediaRawDataWriter> writer(sample->CreateWriter());
      writer->mCrypto.mMode = mInfo->mCrypto.mMode;
      writer->mCrypto.mIVSize = mInfo->mCrypto.mIVSize;
      writer->mCrypto.mKeyId.AppendElements(mInfo->mCrypto.mKeyId);
    }
  }
  if (mNextKeyframeTime.isNothing() ||
      aSamples.LastElement()->mTime >= mNextKeyframeTime.value().ToMicroseconds()) {
    SetNextKeyFrameTime();
  }
}

nsresult
WebMTrackDemuxer::GetNextRandomAccessPoint(media::TimeUnit* aTime)
{
  if (mNextKeyframeTime.isNothing()) {
    // There's no next key frame.
    *aTime =
      media::TimeUnit::FromMicroseconds(std::numeric_limits<int64_t>::max());
  } else {
    *aTime = mNextKeyframeTime.ref();
  }
  return NS_OK;
}

RefPtr<WebMTrackDemuxer::SkipAccessPointPromise>
WebMTrackDemuxer::SkipToNextRandomAccessPoint(const media::TimeUnit& aTimeThreshold)
{
  uint32_t parsed = 0;
  bool found = false;
  RefPtr<MediaRawData> sample;
  int64_t sampleTime;

  WEBM_DEBUG("TimeThreshold: %f", aTimeThreshold.ToSeconds());
  while (!found && (sample = NextSample())) {
    parsed++;
    sampleTime = sample->mTime;
    if (sample->mKeyframe && sampleTime >= aTimeThreshold.ToMicroseconds()) {
      found = true;
      mSamples.Reset();
      mSamples.PushFront(sample.forget());
    }
  }
  SetNextKeyFrameTime();
  if (found) {
    WEBM_DEBUG("next sample: %f (parsed: %d)",
               media::TimeUnit::FromMicroseconds(sampleTime).ToSeconds(),
               parsed);
    return SkipAccessPointPromise::CreateAndResolve(parsed, __func__);
  } else {
    SkipFailureHolder failure(NS_ERROR_DOM_MEDIA_END_OF_STREAM, parsed);
    return SkipAccessPointPromise::CreateAndReject(Move(failure), __func__);
  }
}

media::TimeIntervals
WebMTrackDemuxer::GetBuffered()
{
  return mParent->GetBuffered();
}

void
WebMTrackDemuxer::BreakCycles()
{
  mParent = nullptr;
}

int64_t
WebMTrackDemuxer::GetEvictionOffset(const media::TimeUnit& aTime)
{
  int64_t offset;
  if (!mParent->GetOffsetForTime(aTime.ToNanoseconds(), &offset)) {
    return 0;
  }

  return offset;
}

#undef WEBM_DEBUG
} // namespace mozilla

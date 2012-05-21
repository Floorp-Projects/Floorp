/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsError.h"
#include "nsBuiltinDecoderStateMachine.h"
#include "nsBuiltinDecoder.h"
#include "MediaResource.h"
#include "nsWebMReader.h"
#include "nsWebMBufferedParser.h"
#include "VideoUtils.h"
#include "nsTimeRanges.h"
#include "mozilla/Preferences.h"

#define VPX_DONT_DEFINE_STDINT_TYPES
#include "vpx/vp8dx.h"
#include "vpx/vpx_decoder.h"

using namespace mozilla;
using namespace mozilla::layers;

// Un-comment to enable logging of seek bisections.
//#define SEEK_LOGGING

#ifdef PR_LOGGING
extern PRLogModuleInfo* gBuiltinDecoderLog;
#define LOG(type, msg) PR_LOG(gBuiltinDecoderLog, type, msg)
#ifdef SEEK_LOGGING
#define SEEK_LOG(type, msg) PR_LOG(gBuiltinDecoderLog, type, msg)
#else
#define SEEK_LOG(type, msg)
#endif
#else
#define LOG(type, msg)
#define SEEK_LOG(type, msg)
#endif

static const unsigned NS_PER_USEC = 1000;
static const double NS_PER_S = 1e9;

// If a seek request is within SEEK_DECODE_MARGIN microseconds of the
// current time, decode ahead from the current frame rather than performing
// a full seek.
static const int SEEK_DECODE_MARGIN = 250000;

template <>
class nsAutoRefTraits<NesteggPacketHolder> : public nsPointerRefTraits<NesteggPacketHolder>
{
public:
  static void Release(NesteggPacketHolder* aHolder) { delete aHolder; }
};

// Functions for reading and seeking using MediaResource required for
// nestegg_io. The 'user data' passed to these functions is the
// decoder from which the media resource is obtained.
static int webm_read(void *aBuffer, size_t aLength, void *aUserData)
{
  NS_ASSERTION(aUserData, "aUserData must point to a valid nsBuiltinDecoder");
  nsBuiltinDecoder* decoder = reinterpret_cast<nsBuiltinDecoder*>(aUserData);
  MediaResource* resource = decoder->GetResource();
  NS_ASSERTION(resource, "Decoder has no media resource");

  nsresult rv = NS_OK;
  bool eof = false;

  char *p = static_cast<char *>(aBuffer);
  while (NS_SUCCEEDED(rv) && aLength > 0) {
    PRUint32 bytes = 0;
    rv = resource->Read(p, aLength, &bytes);
    if (bytes == 0) {
      eof = true;
      break;
    }
    decoder->NotifyBytesConsumed(bytes);
    aLength -= bytes;
    p += bytes;
  }

  return NS_FAILED(rv) ? -1 : eof ? 0 : 1;
}

static int webm_seek(int64_t aOffset, int aWhence, void *aUserData)
{
  NS_ASSERTION(aUserData, "aUserData must point to a valid nsBuiltinDecoder");
  nsBuiltinDecoder* decoder = reinterpret_cast<nsBuiltinDecoder*>(aUserData);
  MediaResource* resource = decoder->GetResource();
  NS_ASSERTION(resource, "Decoder has no media resource");
  nsresult rv = resource->Seek(aWhence, aOffset);
  return NS_SUCCEEDED(rv) ? 0 : -1;
}

static int64_t webm_tell(void *aUserData)
{
  NS_ASSERTION(aUserData, "aUserData must point to a valid nsBuiltinDecoder");
  nsBuiltinDecoder* decoder = reinterpret_cast<nsBuiltinDecoder*>(aUserData);
  MediaResource* resource = decoder->GetResource();
  NS_ASSERTION(resource, "Decoder has no media resource");
  return resource->Tell();
}

nsWebMReader::nsWebMReader(nsBuiltinDecoder* aDecoder)
  : nsBuiltinDecoderReader(aDecoder),
  mContext(nsnull),
  mPacketCount(0),
  mChannels(0),
  mVideoTrack(0),
  mAudioTrack(0),
  mAudioStartUsec(-1),
  mAudioFrames(0),
  mForceStereoMode(0),
  mHasVideo(false),
  mHasAudio(false),
  mStereoModeForced(false)
{
  MOZ_COUNT_CTOR(nsWebMReader);

  mStereoModeForced =
    NS_SUCCEEDED(Preferences::GetInt(
          "media.webm.force_stereo_mode",
          &mForceStereoMode));
}

nsWebMReader::~nsWebMReader()
{
  Cleanup();

  mVideoPackets.Reset();
  mAudioPackets.Reset();

  vpx_codec_destroy(&mVP8);

  vorbis_block_clear(&mVorbisBlock);
  vorbis_dsp_clear(&mVorbisDsp);
  vorbis_info_clear(&mVorbisInfo);
  vorbis_comment_clear(&mVorbisComment);

  MOZ_COUNT_DTOR(nsWebMReader);
}

nsresult nsWebMReader::Init(nsBuiltinDecoderReader* aCloneDonor)
{
  if (vpx_codec_dec_init(&mVP8, vpx_codec_vp8_dx(), NULL, 0)) {
    return NS_ERROR_FAILURE;
  }

  vorbis_info_init(&mVorbisInfo);
  vorbis_comment_init(&mVorbisComment);
  memset(&mVorbisDsp, 0, sizeof(vorbis_dsp_state));
  memset(&mVorbisBlock, 0, sizeof(vorbis_block));

  if (aCloneDonor) {
    mBufferedState = static_cast<nsWebMReader*>(aCloneDonor)->mBufferedState;
  } else {
    mBufferedState = new nsWebMBufferedState;
  }

  return NS_OK;
}

nsresult nsWebMReader::ResetDecode()
{
  mAudioFrames = 0;
  mAudioStartUsec = -1;
  nsresult res = NS_OK;
  if (NS_FAILED(nsBuiltinDecoderReader::ResetDecode())) {
    res = NS_ERROR_FAILURE;
  }

  // Ignore failed results from vorbis_synthesis_restart. They
  // aren't fatal and it fails when ResetDecode is called at a
  // time when no vorbis data has been read.
  vorbis_synthesis_restart(&mVorbisDsp);

  mVideoPackets.Reset();
  mAudioPackets.Reset();

  return res;
}

void nsWebMReader::Cleanup()
{
  if (mContext) {
    nestegg_destroy(mContext);
    mContext = nsnull;
  }
}

nsresult nsWebMReader::ReadMetadata(nsVideoInfo* aInfo)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  nestegg_io io;
  io.read = webm_read;
  io.seek = webm_seek;
  io.tell = webm_tell;
  io.userdata = static_cast<nsBuiltinDecoder*>(mDecoder);
  int r = nestegg_init(&mContext, io, NULL);
  if (r == -1) {
    return NS_ERROR_FAILURE;
  }

  uint64_t duration = 0;
  r = nestegg_duration(mContext, &duration);
  if (r == 0) {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mDecoder->GetStateMachine()->SetDuration(duration / NS_PER_USEC);
  }

  unsigned int ntracks = 0;
  r = nestegg_track_count(mContext, &ntracks);
  if (r == -1) {
    Cleanup();
    return NS_ERROR_FAILURE;
  }

  mInfo.mHasAudio = false;
  mInfo.mHasVideo = false;
  for (PRUint32 track = 0; track < ntracks; ++track) {
    int id = nestegg_track_codec_id(mContext, track);
    if (id == -1) {
      Cleanup();
      return NS_ERROR_FAILURE;
    }
    int type = nestegg_track_type(mContext, track);
    if (!mHasVideo && type == NESTEGG_TRACK_VIDEO) {
      nestegg_video_params params;
      r = nestegg_track_video_params(mContext, track, &params);
      if (r == -1) {
        Cleanup();
        return NS_ERROR_FAILURE;
      }

      // Picture region, taking into account cropping, before scaling
      // to the display size.
      nsIntRect pictureRect(params.crop_left,
                            params.crop_top,
                            params.width - (params.crop_right + params.crop_left),
                            params.height - (params.crop_bottom + params.crop_top));

      // If the cropping data appears invalid then use the frame data
      if (pictureRect.width <= 0 ||
          pictureRect.height <= 0 ||
          pictureRect.x < 0 ||
          pictureRect.y < 0)
      {
        pictureRect.x = 0;
        pictureRect.y = 0;
        pictureRect.width = params.width;
        pictureRect.height = params.height;
      }

      // Validate the container-reported frame and pictureRect sizes. This ensures
      // that our video frame creation code doesn't overflow.
      nsIntSize displaySize(params.display_width, params.display_height);
      nsIntSize frameSize(params.width, params.height);
      if (!nsVideoInfo::ValidateVideoRegion(frameSize, pictureRect, displaySize)) {
        // Video track's frame sizes will overflow. Ignore the video track.
        continue;
      }

      mVideoTrack = track;
      mHasVideo = true;
      mInfo.mHasVideo = true;

      mInfo.mDisplay = displaySize;
      mPicture = pictureRect;
      mInitialFrame = frameSize;

      switch (params.stereo_mode) {
      case NESTEGG_VIDEO_MONO:
        mInfo.mStereoMode = STEREO_MODE_MONO;
        break;
      case NESTEGG_VIDEO_STEREO_LEFT_RIGHT:
        mInfo.mStereoMode = STEREO_MODE_LEFT_RIGHT;
        break;
      case NESTEGG_VIDEO_STEREO_BOTTOM_TOP:
        mInfo.mStereoMode = STEREO_MODE_BOTTOM_TOP;
        break;
      case NESTEGG_VIDEO_STEREO_TOP_BOTTOM:
        mInfo.mStereoMode = STEREO_MODE_TOP_BOTTOM;
        break;
      case NESTEGG_VIDEO_STEREO_RIGHT_LEFT:
        mInfo.mStereoMode = STEREO_MODE_RIGHT_LEFT;
        break;
      }

      // Switch only when stereo mode is explicitly set.
      if (mStereoModeForced) {
        switch (mForceStereoMode) {
        case 1:
          mInfo.mStereoMode = STEREO_MODE_LEFT_RIGHT;
          break;
        case 2:
          mInfo.mStereoMode = STEREO_MODE_RIGHT_LEFT;
          break;
        case 3:
          mInfo.mStereoMode = STEREO_MODE_TOP_BOTTOM;
          break;
        case 4:
          mInfo.mStereoMode = STEREO_MODE_BOTTOM_TOP;
          break;
        default:
          mInfo.mStereoMode = STEREO_MODE_MONO;
        }
      }
    }
    else if (!mHasAudio && type == NESTEGG_TRACK_AUDIO) {
      nestegg_audio_params params;
      r = nestegg_track_audio_params(mContext, track, &params);
      if (r == -1) {
        Cleanup();
        return NS_ERROR_FAILURE;
      }

      mAudioTrack = track;
      mHasAudio = true;
      mInfo.mHasAudio = true;

      // Get the Vorbis header data
      unsigned int nheaders = 0;
      r = nestegg_track_codec_data_count(mContext, track, &nheaders);
      if (r == -1 || nheaders != 3) {
        Cleanup();
        return NS_ERROR_FAILURE;
      }

      for (PRUint32 header = 0; header < nheaders; ++header) {
        unsigned char* data = 0;
        size_t length = 0;

        r = nestegg_track_codec_data(mContext, track, header, &data, &length);
        if (r == -1) {
          Cleanup();
          return NS_ERROR_FAILURE;
        }

        ogg_packet opacket = InitOggPacket(data, length, header == 0, false, 0);

        r = vorbis_synthesis_headerin(&mVorbisInfo,
                                      &mVorbisComment,
                                      &opacket);
        if (r != 0) {
          Cleanup();
          return NS_ERROR_FAILURE;
        }
      }

      r = vorbis_synthesis_init(&mVorbisDsp, &mVorbisInfo);
      if (r != 0) {
        Cleanup();
        return NS_ERROR_FAILURE;
      }

      r = vorbis_block_init(&mVorbisDsp, &mVorbisBlock);
      if (r != 0) {
        Cleanup();
        return NS_ERROR_FAILURE;
      }

      mInfo.mAudioRate = mVorbisDsp.vi->rate;
      mInfo.mAudioChannels = mVorbisDsp.vi->channels;
      mChannels = mInfo.mAudioChannels;
    }
  }

  *aInfo = mInfo;

  return NS_OK;
}

ogg_packet nsWebMReader::InitOggPacket(unsigned char* aData,
                                       size_t aLength,
                                       bool aBOS,
                                       bool aEOS,
                                       PRInt64 aGranulepos)
{
  ogg_packet packet;
  packet.packet = aData;
  packet.bytes = aLength;
  packet.b_o_s = aBOS;
  packet.e_o_s = aEOS;
  packet.granulepos = aGranulepos;
  packet.packetno = mPacketCount++;
  return packet;
}
 
bool nsWebMReader::DecodeAudioPacket(nestegg_packet* aPacket, PRInt64 aOffset)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  int r = 0;
  unsigned int count = 0;
  r = nestegg_packet_count(aPacket, &count);
  if (r == -1) {
    return false;
  }

  uint64_t tstamp = 0;
  r = nestegg_packet_tstamp(aPacket, &tstamp);
  if (r == -1) {
    return false;
  }

  const PRUint32 rate = mVorbisDsp.vi->rate;
  PRUint64 tstamp_usecs = tstamp / NS_PER_USEC;
  if (mAudioStartUsec == -1) {
    // This is the first audio chunk. Assume the start time of our decode
    // is the start of this chunk.
    mAudioStartUsec = tstamp_usecs;
  }
  // If there's a gap between the start of this audio chunk and the end of
  // the previous audio chunk, we need to increment the packet count so that
  // the vorbis decode doesn't use data from before the gap to help decode
  // from after the gap.
  CheckedInt64 tstamp_frames = UsecsToFrames(tstamp_usecs, rate);
  CheckedInt64 decoded_frames = UsecsToFrames(mAudioStartUsec, rate);
  if (!tstamp_frames.isValid() || !decoded_frames.isValid()) {
    NS_WARNING("Int overflow converting WebM times to frames");
    return false;
  }
  decoded_frames += mAudioFrames;
  if (!decoded_frames.isValid()) {
    NS_WARNING("Int overflow adding decoded_frames");
    return false;
  }
  if (tstamp_frames.value() > decoded_frames.value()) {
#ifdef DEBUG
    CheckedInt64 usecs = FramesToUsecs(tstamp_frames.value() - decoded_frames.value(), rate);
    LOG(PR_LOG_DEBUG, ("WebMReader detected gap of %lld, %lld frames, in audio stream\n",
      usecs.isValid() ? usecs.value() : -1,
      tstamp_frames.value() - decoded_frames.value()));
#endif
    mPacketCount++;
    mAudioStartUsec = tstamp_usecs;
    mAudioFrames = 0;
  }

  PRInt32 total_frames = 0;
  for (PRUint32 i = 0; i < count; ++i) {
    unsigned char* data;
    size_t length;
    r = nestegg_packet_data(aPacket, i, &data, &length);
    if (r == -1) {
      return false;
    }

    ogg_packet opacket = InitOggPacket(data, length, false, false, -1);

    if (vorbis_synthesis(&mVorbisBlock, &opacket) != 0) {
      return false;
    }

    if (vorbis_synthesis_blockin(&mVorbisDsp,
                                 &mVorbisBlock) != 0) {
      return false;
    }

    VorbisPCMValue** pcm = 0;
    PRInt32 frames = 0;
    while ((frames = vorbis_synthesis_pcmout(&mVorbisDsp, &pcm)) > 0) {
      nsAutoArrayPtr<AudioDataValue> buffer(new AudioDataValue[frames * mChannels]);
      for (PRUint32 j = 0; j < mChannels; ++j) {
        VorbisPCMValue* channel = pcm[j];
        for (PRUint32 i = 0; i < PRUint32(frames); ++i) {
          buffer[i*mChannels + j] = MOZ_CONVERT_VORBIS_SAMPLE(channel[i]);
        }
      }

      CheckedInt64 duration = FramesToUsecs(frames, rate);
      if (!duration.isValid()) {
        NS_WARNING("Int overflow converting WebM audio duration");
        return false;
      }
      CheckedInt64 total_duration = FramesToUsecs(total_frames, rate);
      if (!total_duration.isValid()) {
        NS_WARNING("Int overflow converting WebM audio total_duration");
        return false;
      }
      
      CheckedInt64 time = total_duration + tstamp_usecs;
      if (!time.isValid()) {
        NS_WARNING("Int overflow adding total_duration and tstamp_usecs");
        nestegg_free_packet(aPacket);
        return false;
      };

      total_frames += frames;
      mAudioQueue.Push(new AudioData(aOffset,
                                     time.value(),
                                     duration.value(),
                                     frames,
                                     buffer.forget(),
                                     mChannels));
      mAudioFrames += frames;
      if (vorbis_synthesis_read(&mVorbisDsp, frames) != 0) {
        return false;
      }
    }
  }

  return true;
}

nsReturnRef<NesteggPacketHolder> nsWebMReader::NextPacket(TrackType aTrackType)
{
  // The packet queue that packets will be pushed on if they
  // are not the type we are interested in.
  PacketQueue& otherPackets = 
    aTrackType == VIDEO ? mAudioPackets : mVideoPackets;

  // The packet queue for the type that we are interested in.
  PacketQueue &packets =
    aTrackType == VIDEO ? mVideoPackets : mAudioPackets;

  // Flag to indicate that we do need to playback these types of
  // packets.
  bool hasType = aTrackType == VIDEO ? mHasVideo : mHasAudio;

  // Flag to indicate that we do need to playback the other type
  // of track.
  bool hasOtherType = aTrackType == VIDEO ? mHasAudio : mHasVideo;

  // Track we are interested in
  PRUint32 ourTrack = aTrackType == VIDEO ? mVideoTrack : mAudioTrack;

  // Value of other track
  PRUint32 otherTrack = aTrackType == VIDEO ? mAudioTrack : mVideoTrack;

  nsAutoRef<NesteggPacketHolder> holder;

  if (packets.GetSize() > 0) {
    holder.own(packets.PopFront());
  } else {
    // Keep reading packets until we find a packet
    // for the track we want.
    do {
      nestegg_packet* packet;
      int r = nestegg_read_packet(mContext, &packet);
      if (r <= 0) {
        return nsReturnRef<NesteggPacketHolder>();
      }
      PRInt64 offset = mDecoder->GetResource()->Tell();
      holder.own(new NesteggPacketHolder(packet, offset));

      unsigned int track = 0;
      r = nestegg_packet_track(packet, &track);
      if (r == -1) {
        return nsReturnRef<NesteggPacketHolder>();
      }

      if (hasOtherType && otherTrack == track) {
        // Save the packet for when we want these packets
        otherPackets.Push(holder.disown());
        continue;
      }

      // The packet is for the track we want to play
      if (hasType && ourTrack == track) {
        break;
      }
    } while (true);
  }

  return holder.out();
}

bool nsWebMReader::DecodeAudioData()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  nsAutoRef<NesteggPacketHolder> holder(NextPacket(AUDIO));
  if (!holder) {
    mAudioQueue.Finish();
    return false;
  }

  return DecodeAudioPacket(holder->mPacket, holder->mOffset);
}

bool nsWebMReader::DecodeVideoFrame(bool &aKeyframeSkip,
                                      PRInt64 aTimeThreshold)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  // Record number of frames decoded and parsed. Automatically update the
  // stats counters using the AutoNotifyDecoded stack-based class.
  PRUint32 parsed = 0, decoded = 0;
  nsMediaDecoder::AutoNotifyDecoded autoNotify(mDecoder, parsed, decoded);

  nsAutoRef<NesteggPacketHolder> holder(NextPacket(VIDEO));
  if (!holder) {
    mVideoQueue.Finish();
    return false;
  }

  nestegg_packet* packet = holder->mPacket;
  unsigned int track = 0;
  int r = nestegg_packet_track(packet, &track);
  if (r == -1) {
    return false;
  }

  unsigned int count = 0;
  r = nestegg_packet_count(packet, &count);
  if (r == -1) {
    return false;
  }

  uint64_t tstamp = 0;
  r = nestegg_packet_tstamp(packet, &tstamp);
  if (r == -1) {
    return false;
  }

  // The end time of this frame is the start time of the next frame.  Fetch
  // the timestamp of the next packet for this track.  If we've reached the
  // end of the resource, use the file's duration as the end time of this
  // video frame.
  uint64_t next_tstamp = 0;
  {
    nsAutoRef<NesteggPacketHolder> next_holder(NextPacket(VIDEO));
    if (next_holder) {
      r = nestegg_packet_tstamp(next_holder->mPacket, &next_tstamp);
      if (r == -1) {
        return false;
      }
      mVideoPackets.PushFront(next_holder.disown());
    } else {
      ReentrantMonitorAutoEnter decoderMon(mDecoder->GetReentrantMonitor());
      nsBuiltinDecoderStateMachine* s =
        static_cast<nsBuiltinDecoderStateMachine*>(mDecoder->GetStateMachine());
      PRInt64 endTime = s->GetEndMediaTime();
      if (endTime == -1) {
        return false;
      }
      next_tstamp = endTime * NS_PER_USEC;
    }
  }

  PRInt64 tstamp_usecs = tstamp / NS_PER_USEC;
  for (PRUint32 i = 0; i < count; ++i) {
    unsigned char* data;
    size_t length;
    r = nestegg_packet_data(packet, i, &data, &length);
    if (r == -1) {
      return false;
    }

    vpx_codec_stream_info_t si;
    memset(&si, 0, sizeof(si));
    si.sz = sizeof(si);
    vpx_codec_peek_stream_info(vpx_codec_vp8_dx(), data, length, &si);
    if (aKeyframeSkip && (!si.is_kf || tstamp_usecs < aTimeThreshold)) {
      // Skipping to next keyframe...
      parsed++; // Assume 1 frame per chunk.
      continue;
    }

    if (aKeyframeSkip && si.is_kf) {
      aKeyframeSkip = false;
    }

    if (vpx_codec_decode(&mVP8, data, length, NULL, 0)) {
      return false;
    }

    // If the timestamp of the video frame is less than
    // the time threshold required then it is not added
    // to the video queue and won't be displayed.
    if (tstamp_usecs < aTimeThreshold) {
      parsed++; // Assume 1 frame per chunk.
      continue;
    }

    vpx_codec_iter_t  iter = NULL;
    vpx_image_t      *img;

    while ((img = vpx_codec_get_frame(&mVP8, &iter))) {
      NS_ASSERTION(img->fmt == IMG_FMT_I420, "WebM image format is not I420");

      // Chroma shifts are rounded down as per the decoding examples in the VP8 SDK
      VideoData::YCbCrBuffer b;
      b.mPlanes[0].mData = img->planes[0];
      b.mPlanes[0].mStride = img->stride[0];
      b.mPlanes[0].mHeight = img->d_h;
      b.mPlanes[0].mWidth = img->d_w;

      b.mPlanes[1].mData = img->planes[1];
      b.mPlanes[1].mStride = img->stride[1];
      b.mPlanes[1].mHeight = img->d_h >> img->y_chroma_shift;
      b.mPlanes[1].mWidth = img->d_w >> img->x_chroma_shift;
 
      b.mPlanes[2].mData = img->planes[2];
      b.mPlanes[2].mStride = img->stride[2];
      b.mPlanes[2].mHeight = img->d_h >> img->y_chroma_shift;
      b.mPlanes[2].mWidth = img->d_w >> img->x_chroma_shift;
  
      nsIntRect picture = mPicture;
      if (img->d_w != static_cast<PRUint32>(mInitialFrame.width) ||
          img->d_h != static_cast<PRUint32>(mInitialFrame.height)) {
        // Frame size is different from what the container reports. This is legal
        // in WebM, and we will preserve the ratio of the crop rectangle as it
        // was reported relative to the picture size reported by the container.
        picture.x = (mPicture.x * img->d_w) / mInitialFrame.width;
        picture.y = (mPicture.y * img->d_h) / mInitialFrame.height;
        picture.width = (img->d_w * mPicture.width) / mInitialFrame.width;
        picture.height = (img->d_h * mPicture.height) / mInitialFrame.height;
      }

      VideoData *v = VideoData::Create(mInfo,
                                       mDecoder->GetImageContainer(),
                                       holder->mOffset,
                                       tstamp_usecs,
                                       next_tstamp / NS_PER_USEC,
                                       b,
                                       si.is_kf,
                                       -1,
                                       picture);
      if (!v) {
        return false;
      }
      parsed++;
      decoded++;
      NS_ASSERTION(decoded <= parsed,
        "Expect only 1 frame per chunk per packet in WebM...");
      mVideoQueue.Push(v);
    }
  }

  return true;
}

nsresult nsWebMReader::Seek(PRInt64 aTarget, PRInt64 aStartTime, PRInt64 aEndTime,
                            PRInt64 aCurrentTime)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  LOG(PR_LOG_DEBUG, ("%p About to seek to %fs", mDecoder, aTarget/1000000.0));
  if (NS_FAILED(ResetDecode())) {
    return NS_ERROR_FAILURE;
  }
  PRUint32 trackToSeek = mHasVideo ? mVideoTrack : mAudioTrack;
  int r = nestegg_track_seek(mContext, trackToSeek, aTarget * NS_PER_USEC);
  if (r != 0) {
    return NS_ERROR_FAILURE;
  }
  return DecodeToTarget(aTarget);
}

nsresult nsWebMReader::GetBuffered(nsTimeRanges* aBuffered, PRInt64 aStartTime)
{
  MediaResource* resource = mDecoder->GetResource();

  uint64_t timecodeScale;
  if (!mContext || nestegg_tstamp_scale(mContext, &timecodeScale) == -1) {
    return NS_OK;
  }

  // Special case completely cached files.  This also handles local files.
  bool isFullyCached = resource->IsDataCachedToEndOfResource(0);
  if (isFullyCached) {
    uint64_t duration = 0;
    if (nestegg_duration(mContext, &duration) == 0) {
      aBuffered->Add(0, duration / NS_PER_S);
    }
  }

  PRUint32 bufferedLength = 0;
  aBuffered->GetLength(&bufferedLength);

  // Either we the file is not fully cached, or we couldn't find a duration in
  // the WebM bitstream.
  if (!isFullyCached || !bufferedLength) {
    MediaResource* resource = mDecoder->GetResource();
    nsTArray<MediaByteRange> ranges;
    nsresult res = resource->GetCachedRanges(ranges);
    NS_ENSURE_SUCCESS(res, res);

    for (PRUint32 index = 0; index < ranges.Length(); index++) {
      PRUint64 start, end;
      bool rv = mBufferedState->CalculateBufferedForRange(ranges[index].mStart,
                                                          ranges[index].mEnd,
                                                          &start, &end);
      if (rv) {
        double startTime = start * timecodeScale / NS_PER_S - aStartTime;
        double endTime = end * timecodeScale / NS_PER_S - aStartTime;

        // If this range extends to the end of the file, the true end time
        // is the file's duration.
        if (resource->IsDataCachedToEndOfResource(ranges[index].mStart)) {
          uint64_t duration = 0;
          if (nestegg_duration(mContext, &duration) == 0) {
            endTime = duration / NS_PER_S;
          }
        }

        aBuffered->Add(startTime, endTime);
      }
    }
  }

  return NS_OK;
}

void nsWebMReader::NotifyDataArrived(const char* aBuffer, PRUint32 aLength, PRInt64 aOffset)
{
  mBufferedState->NotifyDataArrived(aBuffer, aLength, aOffset);
}

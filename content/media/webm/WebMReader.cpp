/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsError.h"
#include "MediaDecoderStateMachine.h"
#include "AbstractMediaDecoder.h"
#include "MediaResource.h"
#include "WebMReader.h"
#include "WebMBufferedParser.h"
#include "VideoUtils.h"
#include "mozilla/dom/TimeRanges.h"
#include "VorbisUtils.h"

#define VPX_DONT_DEFINE_STDINT_TYPES
#include "vpx/vp8dx.h"
#include "vpx/vpx_decoder.h"

using mozilla::NesteggPacketHolder;

template <>
class nsAutoRefTraits<NesteggPacketHolder> : public nsPointerRefTraits<NesteggPacketHolder>
{
public:
  static void Release(NesteggPacketHolder* aHolder) { delete aHolder; }
};

namespace mozilla {

using namespace layers;

// Un-comment to enable logging of seek bisections.
//#define SEEK_LOGGING

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaDecoderLog;
PRLogModuleInfo* gNesteggLog;
#define LOG(type, msg) PR_LOG(gMediaDecoderLog, type, msg)
#ifdef SEEK_LOGGING
#define SEEK_LOG(type, msg) PR_LOG(gMediaDecoderLog, type, msg)
#else
#define SEEK_LOG(type, msg)
#endif
#else
#define LOG(type, msg)
#define SEEK_LOG(type, msg)
#endif

static const unsigned NS_PER_USEC = 1000;
static const double NS_PER_S = 1e9;
#ifdef MOZ_DASH
static const double USEC_PER_S = 1e6;
#endif

// Functions for reading and seeking using MediaResource required for
// nestegg_io. The 'user data' passed to these functions is the
// decoder from which the media resource is obtained.
static int webm_read(void *aBuffer, size_t aLength, void *aUserData)
{
  NS_ASSERTION(aUserData, "aUserData must point to a valid AbstractMediaDecoder");
  AbstractMediaDecoder* decoder = reinterpret_cast<AbstractMediaDecoder*>(aUserData);
  MediaResource* resource = decoder->GetResource();
  NS_ASSERTION(resource, "Decoder has no media resource");

  nsresult rv = NS_OK;
  bool eof = false;

  char *p = static_cast<char *>(aBuffer);
  while (NS_SUCCEEDED(rv) && aLength > 0) {
    uint32_t bytes = 0;
    rv = resource->Read(p, aLength, &bytes);
    if (bytes == 0) {
      eof = true;
      break;
    }
    aLength -= bytes;
    p += bytes;
  }

  return NS_FAILED(rv) ? -1 : eof ? 0 : 1;
}

static int webm_seek(int64_t aOffset, int aWhence, void *aUserData)
{
  NS_ASSERTION(aUserData, "aUserData must point to a valid AbstractMediaDecoder");
  AbstractMediaDecoder* decoder = reinterpret_cast<AbstractMediaDecoder*>(aUserData);
  MediaResource* resource = decoder->GetResource();
  NS_ASSERTION(resource, "Decoder has no media resource");
  nsresult rv = resource->Seek(aWhence, aOffset);
  return NS_SUCCEEDED(rv) ? 0 : -1;
}

static int64_t webm_tell(void *aUserData)
{
  NS_ASSERTION(aUserData, "aUserData must point to a valid AbstractMediaDecoder");
  AbstractMediaDecoder* decoder = reinterpret_cast<AbstractMediaDecoder*>(aUserData);
  MediaResource* resource = decoder->GetResource();
  NS_ASSERTION(resource, "Decoder has no media resource");
  return resource->Tell();
}

static void webm_log(nestegg * context,
                     unsigned int severity,
                     char const * format, ...)
{
#ifdef PR_LOGGING
  va_list args;
  char msg[256];
  const char * sevStr;

  switch(severity) {
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

  va_start(args, format);

  PR_snprintf(msg, sizeof(msg), "%p [Nestegg-%s] ", context, sevStr);
  PR_vsnprintf(msg+strlen(msg), sizeof(msg)-strlen(msg), format, args);
  PR_LOG(gNesteggLog, PR_LOG_DEBUG, (msg));

  va_end(args);
#endif
}

WebMReader::WebMReader(AbstractMediaDecoder* aDecoder)
#ifdef MOZ_DASH
  : DASHRepReader(aDecoder),
#else
  : MediaDecoderReader(aDecoder),
#endif
  mContext(nullptr),
  mPacketCount(0),
  mChannels(0),
  mVideoTrack(0),
  mAudioTrack(0),
  mAudioStartUsec(-1),
  mAudioFrames(0),
  mHasVideo(false),
  mHasAudio(false)
#ifdef MOZ_DASH
  , mMainReader(nullptr),
  mSwitchingCluster(-1),
  mNextReader(nullptr),
  mSeekToCluster(-1),
  mCurrentOffset(-1),
  mNextCluster(1),
  mPushVideoPacketToNextReader(false),
  mReachedSwitchAccessPoint(false)
#endif
{
  MOZ_COUNT_CTOR(WebMReader);
#ifdef PR_LOGGING
  if (!gNesteggLog) {
    gNesteggLog = PR_NewLogModule("Nestegg");
  }
#endif
  // Zero these member vars to avoid crashes in VP8 destroy and Vorbis clear
  // functions when destructor is called before |Init|.
  memset(&mVP8, 0, sizeof(vpx_codec_ctx_t));
  memset(&mVorbisBlock, 0, sizeof(vorbis_block));
  memset(&mVorbisDsp, 0, sizeof(vorbis_dsp_state));
  memset(&mVorbisInfo, 0, sizeof(vorbis_info));
  memset(&mVorbisComment, 0, sizeof(vorbis_comment));
}

WebMReader::~WebMReader()
{
  Cleanup();

  mVideoPackets.Reset();
  mAudioPackets.Reset();

  vpx_codec_destroy(&mVP8);

  vorbis_block_clear(&mVorbisBlock);
  vorbis_dsp_clear(&mVorbisDsp);
  vorbis_info_clear(&mVorbisInfo);
  vorbis_comment_clear(&mVorbisComment);

  MOZ_COUNT_DTOR(WebMReader);
}

nsresult WebMReader::Init(MediaDecoderReader* aCloneDonor)
{
  if (vpx_codec_dec_init(&mVP8, vpx_codec_vp8_dx(), NULL, 0)) {
    return NS_ERROR_FAILURE;
  }

  vorbis_info_init(&mVorbisInfo);
  vorbis_comment_init(&mVorbisComment);
  memset(&mVorbisDsp, 0, sizeof(vorbis_dsp_state));
  memset(&mVorbisBlock, 0, sizeof(vorbis_block));

  if (aCloneDonor) {
    mBufferedState = static_cast<WebMReader*>(aCloneDonor)->mBufferedState;
  } else {
    mBufferedState = new WebMBufferedState;
  }

  return NS_OK;
}

nsresult WebMReader::ResetDecode()
{
  mAudioFrames = 0;
  mAudioStartUsec = -1;
  nsresult res = NS_OK;
  if (NS_FAILED(MediaDecoderReader::ResetDecode())) {
    res = NS_ERROR_FAILURE;
  }

  // Ignore failed results from vorbis_synthesis_restart. They
  // aren't fatal and it fails when ResetDecode is called at a
  // time when no vorbis data has been read.
  vorbis_synthesis_restart(&mVorbisDsp);

  mVideoPackets.Reset();
  mAudioPackets.Reset();

#ifdef MOZ_DASH
  LOG(PR_LOG_DEBUG, ("Resetting DASH seek vars"));
  mSwitchingCluster = -1;
  mNextReader = nullptr;
  mSeekToCluster = -1;
  mCurrentOffset = -1;
  mPushVideoPacketToNextReader = false;
  mReachedSwitchAccessPoint = false;
#endif

  return res;
}

void WebMReader::Cleanup()
{
  if (mContext) {
    nestegg_destroy(mContext);
    mContext = nullptr;
  }
}

nsresult WebMReader::ReadMetadata(MediaInfo* aInfo,
                                  MetadataTags** aTags)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

#ifdef MOZ_DASH
  LOG(PR_LOG_DEBUG, ("Reader [%p] for Decoder [%p]: Reading WebM Metadata: "
                     "init bytes [%d - %d] cues bytes [%d - %d]",
                     this, mDecoder,
                     mInitByteRange.mStart, mInitByteRange.mEnd,
                     mCuesByteRange.mStart, mCuesByteRange.mEnd));
#endif
  nestegg_io io;
  io.read = webm_read;
  io.seek = webm_seek;
  io.tell = webm_tell;
  io.userdata = mDecoder;
#ifdef MOZ_DASH
  int64_t maxOffset = mInitByteRange.IsNull() ? -1 : mInitByteRange.mEnd;
#else
  int64_t maxOffset = -1;
#endif
  int r = nestegg_init(&mContext, io, &webm_log, maxOffset);
  if (r == -1) {
    return NS_ERROR_FAILURE;
  }

  uint64_t duration = 0;
  r = nestegg_duration(mContext, &duration);
  if (r == 0) {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mDecoder->SetMediaDuration(duration / NS_PER_USEC);
  }

  unsigned int ntracks = 0;
  r = nestegg_track_count(mContext, &ntracks);
  if (r == -1) {
    Cleanup();
    return NS_ERROR_FAILURE;
  }

  for (uint32_t track = 0; track < ntracks; ++track) {
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
      if (!VideoInfo::ValidateVideoRegion(frameSize, pictureRect, displaySize)) {
        // Video track's frame sizes will overflow. Ignore the video track.
        continue;
      }

      mVideoTrack = track;
      mHasVideo = true;
      mInfo.mVideo.mHasVideo = true;

      mInfo.mVideo.mDisplay = displaySize;
      mPicture = pictureRect;
      mInitialFrame = frameSize;

      switch (params.stereo_mode) {
      case NESTEGG_VIDEO_MONO:
        mInfo.mVideo.mStereoMode = STEREO_MODE_MONO;
        break;
      case NESTEGG_VIDEO_STEREO_LEFT_RIGHT:
        mInfo.mVideo.mStereoMode = STEREO_MODE_LEFT_RIGHT;
        break;
      case NESTEGG_VIDEO_STEREO_BOTTOM_TOP:
        mInfo.mVideo.mStereoMode = STEREO_MODE_BOTTOM_TOP;
        break;
      case NESTEGG_VIDEO_STEREO_TOP_BOTTOM:
        mInfo.mVideo.mStereoMode = STEREO_MODE_TOP_BOTTOM;
        break;
      case NESTEGG_VIDEO_STEREO_RIGHT_LEFT:
        mInfo.mVideo.mStereoMode = STEREO_MODE_RIGHT_LEFT;
        break;
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
      mInfo.mAudio.mHasAudio = true;

      // Get the Vorbis header data
      unsigned int nheaders = 0;
      r = nestegg_track_codec_data_count(mContext, track, &nheaders);
      if (r == -1 || nheaders != 3) {
        Cleanup();
        return NS_ERROR_FAILURE;
      }

      for (uint32_t header = 0; header < nheaders; ++header) {
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

      mInfo.mAudio.mRate = mVorbisDsp.vi->rate;
      mInfo.mAudio.mChannels = mVorbisDsp.vi->channels;
      mChannels = mInfo.mAudio.mChannels;
    }
  }

#ifdef MOZ_DASH
  // Byte range for cues has been specified; load them.
  if (!mCuesByteRange.IsNull()) {
    maxOffset = mCuesByteRange.mEnd;

    // Iterate through cluster ranges until nestegg returns the last one
    NS_ENSURE_TRUE(mClusterByteRanges.IsEmpty(),
                   NS_ERROR_ALREADY_INITIALIZED);
    int clusterNum = 0;
    bool done = false;
    uint64_t timestamp;
    do {
      mClusterByteRanges.AppendElement();
      r = nestegg_get_cue_point(mContext, clusterNum, maxOffset,
                                &(mClusterByteRanges[clusterNum].mStart),
                                &(mClusterByteRanges[clusterNum].mEnd),
                                &timestamp);
      if (r != 0) {
        Cleanup();
        return NS_ERROR_FAILURE;
      }
      LOG(PR_LOG_DEBUG, ("Reader [%p] for Decoder [%p]: Cluster [%d]: "
                         "start [%lld] end [%lld], timestamp [%.2llfs]",
                         this, mDecoder, clusterNum,
                         mClusterByteRanges[clusterNum].mStart,
                         mClusterByteRanges[clusterNum].mEnd,
                         timestamp/NS_PER_S));
      mClusterByteRanges[clusterNum].mStartTime = timestamp/NS_PER_USEC;
      // Last cluster will have '-1' as end value
      if (mClusterByteRanges[clusterNum].mEnd == -1) {
        mClusterByteRanges[clusterNum].mEnd = (mCuesByteRange.mStart-1);
        done = true;
      } else {
        clusterNum++;
      }
    } while (!done);
  }
#endif

  // We can't seek in buffered regions if we have no cues.
  mDecoder->SetMediaSeekable(nestegg_has_cues(mContext) == 1);

  *aInfo = mInfo;

  *aTags = nullptr;

#ifdef MOZ_DASH
  mDecoder->OnReadMetadataCompleted();
#endif

  return NS_OK;
}

ogg_packet WebMReader::InitOggPacket(unsigned char* aData,
                                       size_t aLength,
                                       bool aBOS,
                                       bool aEOS,
                                       int64_t aGranulepos)
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
 
bool WebMReader::DecodeAudioPacket(nestegg_packet* aPacket, int64_t aOffset)
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

  const uint32_t rate = mVorbisDsp.vi->rate;
  uint64_t tstamp_usecs = tstamp / NS_PER_USEC;
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

  int32_t total_frames = 0;
  for (uint32_t i = 0; i < count; ++i) {
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
    int32_t frames = 0;
    while ((frames = vorbis_synthesis_pcmout(&mVorbisDsp, &pcm)) > 0) {
      nsAutoArrayPtr<AudioDataValue> buffer(new AudioDataValue[frames * mChannels]);
      for (uint32_t j = 0; j < mChannels; ++j) {
        VorbisPCMValue* channel = pcm[j];
        for (uint32_t i = 0; i < uint32_t(frames); ++i) {
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
      AudioQueue().Push(new AudioData(aOffset,
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

nsReturnRef<NesteggPacketHolder> WebMReader::NextPacket(TrackType aTrackType)
#ifdef MOZ_DASH
{
  nsAutoRef<NesteggPacketHolder> holder;
  // It is possible that following a seek, a requested switching offset could
  // be reached before |DASHReader| calls |RequestSwitchAtSubsegment|. In this
  // case |mNextReader| will be null, so check its value and at every possible
  // switch access point, i.e. cluster boundary, ask |mMainReader| to
  // |GetReaderForSubsegment|.
  if (mMainReader && !mNextReader && aTrackType == VIDEO) {
    WebMReader* nextReader = nullptr;
    LOG(PR_LOG_DEBUG,
      ("WebMReader[%p] for decoder [%p] NextPacket mNextReader not set: "
       "mCurrentOffset[%lld] nextCluster [%d] comparing with offset[%lld]",
       this, mDecoder, mCurrentOffset, mNextCluster,
       mClusterByteRanges[mNextCluster].mStart));

    if (mNextCluster < mClusterByteRanges.Length() &&
        mCurrentOffset == mClusterByteRanges[mNextCluster].mStart) {
      DASHRepReader* nextDASHRepReader =
        mMainReader->GetReaderForSubsegment(mNextCluster);
      nextReader = static_cast<WebMReader*>(nextDASHRepReader);
      LOG(PR_LOG_DEBUG,
          ("WebMReader[%p] for decoder [%p] reached SAP at cluster [%d]: next "
         "reader is [%p]", this, mDecoder, mNextCluster, nextReader));
      if (nextReader && nextReader != this) {
        {
          ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
          // Ensure this reader is set to switch for the next packet.
          RequestSwitchAtSubsegment(mNextCluster, nextReader);
          NS_ASSERTION(mNextReader == nextReader, "mNextReader should be set");
          // Ensure the next reader seeks to |mNextCluster|. |PrepareToDecode|
          // must be called to ensure the reader's variables are set correctly.
          nextReader->RequestSeekToSubsegment(mNextCluster);
          nextReader->PrepareToDecode();
        }
      }
      // Keep mNextCluster up-to-date with the |mCurrentOffset|.
      if (mNextCluster+1 < mClusterByteRanges.Length()) {
        // At least one more cluster to go.
        mNextCluster++;
      } else {
        // Reached last cluster; prepare for being in cluster 0 again.
        mNextCluster = 1;
      }
      LOG(PR_LOG_DEBUG,
          ("WebMReader [%p] for decoder [%p] updating mNextCluster to [%d] "
           "at offset [%lld]", this, mDecoder, mNextCluster, mCurrentOffset));
    }
  }

  // Get packet from next reader if we're at a switching point; most likely we
  // did not download the next packet for this reader's stream, so we have to
  // get it from the next one. Note: Switch to next reader only for video;
  // audio switching is not supported in the DASH-WebM On Demand profile.
  if (aTrackType == VIDEO &&
      (uint32_t)mSwitchingCluster < mClusterByteRanges.Length() &&
      mCurrentOffset == mClusterByteRanges[mSwitchingCluster].mStart) {

    if (mVideoPackets.GetSize() > 0) {
      holder = NextPacketInternal(VIDEO);
      LOG(PR_LOG_DEBUG,
          ("WebMReader[%p] got packet from mVideoPackets @[%lld]",
           this, holder->mOffset));
    } else {
      mReachedSwitchAccessPoint = true;
      NS_ASSERTION(mNextReader,
                   "Stream switch has been requested but mNextReader is null");
      holder = mNextReader->NextPacket(aTrackType);
      mPushVideoPacketToNextReader = true;
      // Reset for possible future switches.
      mSwitchingCluster = -1;
      LOG(PR_LOG_DEBUG,
          ("WebMReader[%p] got packet from mNextReader[%p] @[%lld]",
           this, mNextReader.get(), (holder ? holder->mOffset : 0)));
    }
  } else {
    holder = NextPacketInternal(aTrackType);
    if (holder) {
      mCurrentOffset = holder->mOffset;
    }
  }
  return holder.out();
}

nsReturnRef<NesteggPacketHolder>
WebMReader::NextPacketInternal(TrackType aTrackType)
#endif
{
  // The packet queue that packets will be pushed on if they
  // are not the type we are interested in.
  WebMPacketQueue& otherPackets =
    aTrackType == VIDEO ? mAudioPackets : mVideoPackets;

  // The packet queue for the type that we are interested in.
  WebMPacketQueue &packets =
    aTrackType == VIDEO ? mVideoPackets : mAudioPackets;

  // Flag to indicate that we do need to playback these types of
  // packets.
  bool hasType = aTrackType == VIDEO ? mHasVideo : mHasAudio;

  // Flag to indicate that we do need to playback the other type
  // of track.
  bool hasOtherType = aTrackType == VIDEO ? mHasAudio : mHasVideo;

  // Track we are interested in
  uint32_t ourTrack = aTrackType == VIDEO ? mVideoTrack : mAudioTrack;

  // Value of other track
  uint32_t otherTrack = aTrackType == VIDEO ? mAudioTrack : mVideoTrack;

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
      int64_t offset = mDecoder->GetResource()->Tell();
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

bool WebMReader::DecodeAudioData()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  nsAutoRef<NesteggPacketHolder> holder(NextPacket(AUDIO));
  if (!holder) {
    return false;
  }

  return DecodeAudioPacket(holder->mPacket, holder->mOffset);
}

bool WebMReader::DecodeVideoFrame(bool &aKeyframeSkip,
                                      int64_t aTimeThreshold)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  // Record number of frames decoded and parsed. Automatically update the
  // stats counters using the AutoNotifyDecoded stack-based class.
  uint32_t parsed = 0, decoded = 0;
  AbstractMediaDecoder::AutoNotifyDecoded autoNotify(mDecoder, parsed, decoded);

  nsAutoRef<NesteggPacketHolder> holder(NextPacket(VIDEO));
  if (!holder) {
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
      PushVideoPacket(next_holder.disown());
    } else {
      ReentrantMonitorAutoEnter decoderMon(mDecoder->GetReentrantMonitor());
      int64_t endTime = mDecoder->GetEndMediaTime();
      if (endTime == -1) {
        return false;
      }
      next_tstamp = endTime * NS_PER_USEC;
    }
  }

  int64_t tstamp_usecs = tstamp / NS_PER_USEC;
  for (uint32_t i = 0; i < count; ++i) {
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
      b.mPlanes[0].mOffset = b.mPlanes[0].mSkip = 0;

      b.mPlanes[1].mData = img->planes[1];
      b.mPlanes[1].mStride = img->stride[1];
      b.mPlanes[1].mHeight = (img->d_h + 1) >> img->y_chroma_shift;
      b.mPlanes[1].mWidth = (img->d_w + 1) >> img->x_chroma_shift;
      b.mPlanes[1].mOffset = b.mPlanes[1].mSkip = 0;
 
      b.mPlanes[2].mData = img->planes[2];
      b.mPlanes[2].mStride = img->stride[2];
      b.mPlanes[2].mHeight = (img->d_h + 1) >> img->y_chroma_shift;
      b.mPlanes[2].mWidth = (img->d_w + 1) >> img->x_chroma_shift;
      b.mPlanes[2].mOffset = b.mPlanes[2].mSkip = 0;
  
      nsIntRect picture = mPicture;
      if (img->d_w != static_cast<uint32_t>(mInitialFrame.width) ||
          img->d_h != static_cast<uint32_t>(mInitialFrame.height)) {
        // Frame size is different from what the container reports. This is legal
        // in WebM, and we will preserve the ratio of the crop rectangle as it
        // was reported relative to the picture size reported by the container.
        picture.x = (mPicture.x * img->d_w) / mInitialFrame.width;
        picture.y = (mPicture.y * img->d_h) / mInitialFrame.height;
        picture.width = (img->d_w * mPicture.width) / mInitialFrame.width;
        picture.height = (img->d_h * mPicture.height) / mInitialFrame.height;
      }

      VideoData *v = VideoData::Create(mInfo.mVideo,
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
      VideoQueue().Push(v);
    }
  }

  return true;
}

void
WebMReader::PushVideoPacket(NesteggPacketHolder* aItem)
{
#ifdef MOZ_DASH
  if (mPushVideoPacketToNextReader) {
    NS_ASSERTION(mNextReader,
                 "Stream switch has been requested but mNextReader is null");
    mNextReader->mVideoPackets.PushFront(aItem);
    mPushVideoPacketToNextReader = false;
  } else {
#endif
    mVideoPackets.PushFront(aItem);
#ifdef MOZ_DASH
  }
#endif
}

nsresult WebMReader::Seek(int64_t aTarget, int64_t aStartTime, int64_t aEndTime,
                            int64_t aCurrentTime)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  LOG(PR_LOG_DEBUG, ("Reader [%p] for Decoder [%p]: About to seek to %fs",
                     this, mDecoder, aTarget/1000000.0));
  if (NS_FAILED(ResetDecode())) {
    return NS_ERROR_FAILURE;
  }
  uint32_t trackToSeek = mHasVideo ? mVideoTrack : mAudioTrack;
  int r = nestegg_track_seek(mContext, trackToSeek, aTarget * NS_PER_USEC);
  if (r != 0) {
    return NS_ERROR_FAILURE;
  }
#ifdef MOZ_DASH
  // Find next cluster index;
  MediaResource* resource = mDecoder->GetResource();
  int64_t newOffset = resource->Tell();
  for (uint32_t i = 1; i < mClusterByteRanges.Length(); i++) {
    if (newOffset < mClusterByteRanges[i].mStart) {
      mNextCluster = i;
      LOG(PR_LOG_DEBUG,
          ("WebMReader [%p] for decoder [%p] updating mNextCluster to [%d] "
           "after seek to offset [%lld]",
           this, mDecoder, mNextCluster, resource->Tell()));
      break;
    }
  }
#endif
  return DecodeToTarget(aTarget);
}

#ifdef MOZ_DASH
bool WebMReader::IsDataCachedAtEndOfSubsegments()
{
  MediaResource* resource = mDecoder->GetResource();
  NS_ENSURE_TRUE(resource, false);
  if (resource->IsDataCachedToEndOfResource(0)) {
     return true;
  }

  if (mClusterByteRanges.IsEmpty()) {
    return false;
  }

  nsTArray<MediaByteRange> ranges;
  nsresult rv = resource->GetCachedRanges(ranges);
  NS_ENSURE_SUCCESS(rv, false);
  if (ranges.IsEmpty()) {
    return false;
  }

  // Return true if data at the end of the final subsegment is cached.
  uint32_t finalSubsegmentIndex = mClusterByteRanges.Length()-1;
  uint64_t finalSubEndOffset = mClusterByteRanges[finalSubsegmentIndex].mEnd;
  uint32_t finalRangeIndex = ranges.Length()-1;
  uint64_t finalRangeStartOffset = ranges[finalRangeIndex].mStart;
  uint64_t finalRangeEndOffset = ranges[finalRangeIndex].mEnd;

  return (finalRangeStartOffset < finalSubEndOffset &&
          finalSubEndOffset <= finalRangeEndOffset);
}
#endif

nsresult WebMReader::GetBuffered(dom::TimeRanges* aBuffered, int64_t aStartTime)
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

  uint32_t bufferedLength = 0;
  aBuffered->GetLength(&bufferedLength);

  // Either we the file is not fully cached, or we couldn't find a duration in
  // the WebM bitstream.
  if (!isFullyCached || !bufferedLength) {
    MediaResource* resource = mDecoder->GetResource();
    nsTArray<MediaByteRange> ranges;
    nsresult res = resource->GetCachedRanges(ranges);
    NS_ENSURE_SUCCESS(res, res);

    for (uint32_t index = 0; index < ranges.Length(); index++) {
      uint64_t start, end;
      bool rv = mBufferedState->CalculateBufferedForRange(ranges[index].mStart,
                                                          ranges[index].mEnd,
                                                          &start, &end);
      if (rv) {
        double startTime = start * timecodeScale / NS_PER_S - aStartTime;
        double endTime = end * timecodeScale / NS_PER_S - aStartTime;
#ifdef MOZ_DASH
        // If this range extends to the end of a cluster, the true end time is
        // the cluster's end timestamp. Since WebM frames do not have an end
        // timestamp, a fully cached cluster must be reported with the correct
        // end time of its final frame. Otherwise, buffered ranges could be
        // reported with missing frames at cluster boundaries, specifically
        // boundaries where stream switching has occurred.
        if (!mClusterByteRanges.IsEmpty()) {
          for (uint32_t clusterIndex = 0;
               clusterIndex < (mClusterByteRanges.Length()-1);
               clusterIndex++) {
            if (ranges[index].mEnd >= mClusterByteRanges[clusterIndex].mEnd) {
              double clusterEndTime =
                  mClusterByteRanges[clusterIndex+1].mStartTime / USEC_PER_S;
              if (endTime < clusterEndTime) {
                LOG(PR_LOG_DEBUG, ("End of cluster: endTime becoming %0.3fs",
                                   clusterEndTime));
                endTime = clusterEndTime;
              }
            }
          }
        }
#endif
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

void WebMReader::NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset)
{
  mBufferedState->NotifyDataArrived(aBuffer, aLength, aOffset);
}

#ifdef MOZ_DASH
int64_t
WebMReader::GetSubsegmentForSeekTime(int64_t aSeekToTime)
{
  NS_ENSURE_TRUE(0 <= aSeekToTime, -1);
  // Check the first n-1 subsegments. End time is the start time of the next
  // subsegment.
  for (uint32_t i = 1; i < (mClusterByteRanges.Length()); i++) {
    if (aSeekToTime < mClusterByteRanges[i].mStartTime) {
      return i-1;
    }
  }
  // Check the last subsegment. End time is the end time of the file.
  NS_ASSERTION(mDecoder, "Decoder should not be null!");
  if (aSeekToTime <= mDecoder->GetMediaDuration()) {
    return mClusterByteRanges.Length()-1;
  }

  return (-1);
}
nsresult
WebMReader::GetSubsegmentByteRanges(nsTArray<MediaByteRange>& aByteRanges)
{
  NS_ENSURE_TRUE(mContext, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aByteRanges.IsEmpty(), NS_ERROR_ALREADY_INITIALIZED);
  NS_ENSURE_FALSE(mClusterByteRanges.IsEmpty(), NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_FALSE(mCuesByteRange.IsNull(), NS_ERROR_NOT_INITIALIZED);

  for (uint32_t i = 0; i < mClusterByteRanges.Length(); i++) {
    aByteRanges.AppendElement();
    aByteRanges[i] = mClusterByteRanges[i];
  }

  return NS_OK;
}

void
WebMReader::RequestSwitchAtSubsegment(int32_t aSubsegmentIdx,
                                      MediaDecoderReader* aNextReader)
{
  NS_ASSERTION(NS_IsMainThread() || mDecoder->OnDecodeThread(),
               "Should be on main thread or decode thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  // Only allow one switch at a time; ignore if one is already requested.
  if (mSwitchingCluster != -1) {
    return;
  }
  NS_ENSURE_TRUE_VOID((uint32_t)aSubsegmentIdx < mClusterByteRanges.Length());
  mSwitchingCluster = aSubsegmentIdx;
  NS_ENSURE_TRUE_VOID(aNextReader);
  NS_ENSURE_TRUE_VOID(aNextReader != this);
  mNextReader = static_cast<WebMReader*>(aNextReader);
}

void
WebMReader::RequestSeekToSubsegment(uint32_t aIdx)
{
  NS_ASSERTION(NS_IsMainThread() || mDecoder->OnDecodeThread(),
               "Should be on main thread or decode thread.");
  NS_ASSERTION(mDecoder, "decoder should not be null!");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  // Don't seek if we're about to switch to another reader.
  if (mSwitchingCluster != -1) {
    return;
  }
  // Only allow seeking if a request was not already made.
  if (mSeekToCluster != -1) {
    return;
  }
  NS_ENSURE_TRUE_VOID(aIdx < mClusterByteRanges.Length());
  mSeekToCluster = aIdx;

  // XXX Hack to get the resource to seek to the correct offset if the decode
  // thread is in shutdown, e.g. if the video is not autoplay.
  if (mDecoder->IsShutdown()) {
    ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
    mDecoder->GetResource()->Seek(PR_SEEK_SET,
                                  mClusterByteRanges[mSeekToCluster].mStart);
  }
}

void
WebMReader::PrepareToDecode()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  if (mSeekToCluster != -1) {
    ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
    SeekToCluster(mSeekToCluster);
  }
}

void
WebMReader::SeekToCluster(uint32_t aIdx)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  NS_ENSURE_TRUE_VOID(aIdx < mClusterByteRanges.Length());
  LOG(PR_LOG_DEBUG, ("Reader [%p] for Decoder [%p]: seeking to "
                     "subsegment [%lld] at offset [%lld]",
                     this, mDecoder, aIdx, mClusterByteRanges[aIdx].mStart));
  int r = nestegg_offset_seek(mContext, mClusterByteRanges[aIdx].mStart);
  NS_ENSURE_TRUE_VOID(r == 0);
  if (aIdx + 1 < mClusterByteRanges.Length()) {
    mNextCluster = aIdx + 1;
  } else {
    mNextCluster = 1;
  }
  mSeekToCluster = -1;
}

bool
WebMReader::HasReachedSubsegment(uint32_t aSubsegmentIndex)
{
  NS_ASSERTION(mDecoder, "Decoder is null.");
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  NS_ENSURE_TRUE(aSubsegmentIndex < mClusterByteRanges.Length(), false);

  NS_ASSERTION(mDecoder->GetResource(), "Decoder has no media resource.");
  if (mReachedSwitchAccessPoint) {
    LOG(PR_LOG_DEBUG,
        ("Reader [%p] for Decoder [%p]: reached switching offset [%lld] = "
         "mClusterByteRanges[%d].mStart[%lld]",
         this, mDecoder, mCurrentOffset, aSubsegmentIndex,
         mClusterByteRanges[aSubsegmentIndex].mStart));
    mReachedSwitchAccessPoint = false;
    return true;
  }
  return false;
}
#endif /* MOZ_DASH */

} // namespace mozilla

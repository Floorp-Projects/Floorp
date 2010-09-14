/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
 *  Chris Pearce <chris@pearce.org.nz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsError.h"
#include "nsBuiltinDecoderStateMachine.h"
#include "nsBuiltinDecoder.h"
#include "nsOggReader.h"
#include "VideoUtils.h"
#include "theora/theoradec.h"
#include "nsTimeRanges.h"

using namespace mozilla;

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

// If we don't have a Theora video stream, then during seeking, if a seek
// target is less than SEEK_DECODE_MARGIN ahead of the current playback
// position, we'll just decode forwards rather than performing a bisection
// search. If we have Theora video we use the maximum keyframe interval as
// this value, rather than SEEK_DECODE_MARGIN. This makes small seeks faster.
#define SEEK_DECODE_MARGIN 2000

// The number of milliseconds of "fuzz" we use in a bisection search over
// HTTP. When we're seeking with fuzz, we'll stop the search if a bisection
// lands between the seek target and SEEK_FUZZ_MS milliseconds before the
// seek target.  This is becaue it's usually quicker to just keep downloading
// from an exisiting connection than to do another bisection inside that
// small range, which would open a new HTTP connetion.
#define SEEK_FUZZ_MS 500

enum PageSyncResult {
  PAGE_SYNC_ERROR = 1,
  PAGE_SYNC_END_OF_RANGE= 2,
  PAGE_SYNC_OK = 3
};

// Reads a page from the media stream.
static PageSyncResult
PageSync(nsMediaStream* aStream,
         ogg_sync_state* aState,
         PRBool aCachedDataOnly,
         PRInt64 aOffset,
         PRInt64 aEndOffset,
         ogg_page* aPage,
         int& aSkippedBytes);

// Chunk size to read when reading Ogg files. Average Ogg page length
// is about 4300 bytes, so we read the file in chunks larger than that.
static const int PAGE_STEP = 8192;

nsOggReader::nsOggReader(nsBuiltinDecoder* aDecoder)
  : nsBuiltinDecoderReader(aDecoder),
    mTheoraState(nsnull),
    mVorbisState(nsnull),
    mSkeletonState(nsnull),
    mPageOffset(0),
    mTheoraGranulepos(-1),
    mVorbisGranulepos(-1)
{
  MOZ_COUNT_CTOR(nsOggReader);
}

nsOggReader::~nsOggReader()
{
  ogg_sync_clear(&mOggState);
  MOZ_COUNT_DTOR(nsOggReader);
}

nsresult nsOggReader::Init() {
  PRBool init = mCodecStates.Init();
  NS_ASSERTION(init, "Failed to initialize mCodecStates");
  if (!init) {
    return NS_ERROR_FAILURE;
  }
  int ret = ogg_sync_init(&mOggState);
  NS_ENSURE_TRUE(ret == 0, NS_ERROR_FAILURE);
  return NS_OK;
}

nsresult nsOggReader::ResetDecode()
{
  nsresult res = NS_OK;

  // Clear the Theora/Vorbis granulepos capture status, so that the next
  // decode calls recaptures the granulepos.
  mTheoraGranulepos = -1;
  mVorbisGranulepos = -1;

  if (NS_FAILED(nsBuiltinDecoderReader::ResetDecode())) {
    res = NS_ERROR_FAILURE;
  }

  {
    MonitorAutoEnter mon(mMonitor);

    // Discard any previously buffered packets/pages.
    ogg_sync_reset(&mOggState);
    if (mVorbisState && NS_FAILED(mVorbisState->Reset())) {
      res = NS_ERROR_FAILURE;
    }
    if (mTheoraState && NS_FAILED(mTheoraState->Reset())) {
      res = NS_ERROR_FAILURE;
    }
  }

  return res;
}

// Returns PR_TRUE when all bitstreams in aBitstreams array have finished
// reading their headers.
static PRBool DoneReadingHeaders(nsTArray<nsOggCodecState*>& aBitstreams) {
  for (PRUint32 i = 0; i < aBitstreams .Length(); i++) {
    if (!aBitstreams [i]->DoneReadingHeaders()) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

nsresult nsOggReader::ReadMetadata()
{
  NS_ASSERTION(mDecoder->OnStateMachineThread(), "Should be on play state machine thread.");
  MonitorAutoEnter mon(mMonitor);

  // We read packets until all bitstreams have read all their header packets.
  // We record the offset of the first non-header page so that we know
  // what page to seek to when seeking to the media start.

  ogg_page page;
  PRInt64 pageOffset;
  nsAutoTArray<nsOggCodecState*,4> bitstreams;
  PRBool readAllBOS = PR_FALSE;
  mDataOffset = 0;
  while (PR_TRUE) {
    if (readAllBOS && DoneReadingHeaders(bitstreams)) {
      if (mDataOffset == 0) {
        // We've previously found the start of the first non-header packet.
        mDataOffset = mPageOffset;
      }
      break;
    }
    pageOffset = ReadOggPage(&page);
    if (pageOffset == -1) {
      // Some kind of error...
      break;
    }

    int ret = 0;
    int serial = ogg_page_serialno(&page);
    nsOggCodecState* codecState = 0;

    if (ogg_page_bos(&page)) {
      NS_ASSERTION(!readAllBOS, "We shouldn't encounter another BOS page");
      codecState = nsOggCodecState::Create(&page);
      PRBool r = mCodecStates.Put(serial, codecState);
      NS_ASSERTION(r, "Failed to insert into mCodecStates");
      bitstreams.AppendElement(codecState);
      if (codecState &&
          codecState->GetType() == nsOggCodecState::TYPE_VORBIS &&
          !mVorbisState)
      {
        // First Vorbis bitstream, we'll play this one. Subsequent Vorbis
        // bitstreams will be ignored.
        mVorbisState = static_cast<nsVorbisState*>(codecState);
      }
      if (codecState &&
          codecState->GetType() == nsOggCodecState::TYPE_THEORA &&
          !mTheoraState)
      {
        // First Theora bitstream, we'll play this one. Subsequent Theora
        // bitstreams will be ignored.
        mTheoraState = static_cast<nsTheoraState*>(codecState);
      }
      if (codecState &&
          codecState->GetType() == nsOggCodecState::TYPE_SKELETON &&
          !mSkeletonState)
      {
        mSkeletonState = static_cast<nsSkeletonState*>(codecState);
      }
    } else {
      // We've encountered the a non Beginning Of Stream page. No more
      // BOS pages can follow in this Ogg segment, so there will be no other
      // bitstreams in the Ogg (unless it's invalid).
      readAllBOS = PR_TRUE;
    }

    mCodecStates.Get(serial, &codecState);
    NS_ENSURE_TRUE(codecState, NS_ERROR_FAILURE);

    // Add a complete page to the bitstream
    ret = ogg_stream_pagein(&codecState->mState, &page);
    NS_ENSURE_TRUE(ret == 0, NS_ERROR_FAILURE);

    // Process all available header packets in the stream.
    ogg_packet packet;
    if (codecState->DoneReadingHeaders() && mDataOffset == 0)
    {
      // Stream has read all header packets, but now there's more data in
      // (presumably) a non-header page, we must have finished header packets.
      // This can happen in incorrectly chopped streams.
      mDataOffset = pageOffset;
      continue;
    }
    while (!codecState->DoneReadingHeaders() &&
           (ret = ogg_stream_packetout(&codecState->mState, &packet)) != 0)
    {
      if (ret == -1) {
        // Sync lost, we've probably encountered the continuation of a packet
        // in a chopped video.
        continue;
      }
      // A packet is available. If it is not a header packet we'll break.
      // If it is a header packet, process it as normal.
      codecState->DecodeHeader(&packet);
    }
    if (ogg_stream_packetpeek(&codecState->mState, &packet) != 0 &&
        mDataOffset == 0)
    {
      // We're finished reading headers for this bitstream, but there's still
      // packets in the bitstream to read. The bitstream is probably poorly
      // muxed, and includes the last header packet on a page with non-header
      // packets. We need to ensure that this is the media start page offset.
      mDataOffset = pageOffset;
    }
  }
  // Deactivate any non-primary bitstreams.
  for (PRUint32 i = 0; i < bitstreams.Length(); i++) {
    nsOggCodecState* s = bitstreams[i];
    if (s != mVorbisState && s != mTheoraState && s != mSkeletonState) {
      s->Deactivate();
    }
  }

  // Initialize the first Theora and Vorbis bitstreams. According to the
  // Theora spec these can be considered the 'primary' bitstreams for playback.
  // Extract the metadata needed from these streams.
  // Set a default callback period for if we have no video data
  if (mTheoraState && mTheoraState->Init()) {
    gfxIntSize sz(mTheoraState->mInfo.pic_width,
                  mTheoraState->mInfo.pic_height);
    mDecoder->SetVideoData(sz, mTheoraState->mPixelAspectRatio, nsnull);
  }
  if (mVorbisState) {
    mVorbisState->Init();
  }

  if (!HasAudio() && !HasVideo() && mSkeletonState) {
    // We have a skeleton track, but no audio or video, may as well disable
    // the skeleton, we can't do anything useful with this media.
    mSkeletonState->Deactivate();
  }

  mInfo.mHasAudio = HasAudio();
  mInfo.mHasVideo = HasVideo();
  if (HasAudio()) {
    mInfo.mAudioRate = mVorbisState->mInfo.rate;
    mInfo.mAudioChannels = mVorbisState->mInfo.channels;
  }
  if (HasVideo()) {
    mInfo.mPixelAspectRatio = mTheoraState->mPixelAspectRatio;
    mInfo.mPicture.width = mTheoraState->mInfo.pic_width;
    mInfo.mPicture.height = mTheoraState->mInfo.pic_height;
    mInfo.mPicture.x = mTheoraState->mInfo.pic_x;
    mInfo.mPicture.y = mTheoraState->mInfo.pic_y;
    mInfo.mFrame.width = mTheoraState->mInfo.frame_width;
    mInfo.mFrame.height = mTheoraState->mInfo.frame_height;
  }
  mInfo.mDataOffset = mDataOffset;

  if (mSkeletonState && mSkeletonState->HasIndex()) {
    // Extract the duration info out of the index, so we don't need to seek to
    // the end of stream to get it.
    nsAutoTArray<PRUint32, 2> tracks;
    if (HasVideo()) {
      tracks.AppendElement(mTheoraState->mSerial);
    }
    if (HasAudio()) {
      tracks.AppendElement(mVorbisState->mSerial);
    }
    PRInt64 duration = 0;
    if (NS_SUCCEEDED(mSkeletonState->GetDuration(tracks, duration))) {
      MonitorAutoExit exitReaderMon(mMonitor);
      MonitorAutoEnter decoderMon(mDecoder->GetMonitor());
      mDecoder->GetStateMachine()->SetDuration(duration);
      LOG(PR_LOG_DEBUG, ("Got duration from Skeleton index %lld", duration));
    }
  }

  LOG(PR_LOG_DEBUG, ("Done loading headers, data offset %lld", mDataOffset));

  return NS_OK;
}

nsresult nsOggReader::DecodeVorbis(nsTArray<nsAutoPtr<SoundData> >& aChunks,
                                   ogg_packet* aPacket)
{
  // Successfully read a packet.
  if (vorbis_synthesis(&mVorbisState->mBlock, aPacket) != 0) {
    return NS_ERROR_FAILURE;
  }
  if (vorbis_synthesis_blockin(&mVorbisState->mDsp,
                               &mVorbisState->mBlock) != 0)
  {
    return NS_ERROR_FAILURE;
  }

  float** pcm = 0;
  PRInt32 samples = 0;
  PRUint32 channels = mVorbisState->mInfo.channels;
  while ((samples = vorbis_synthesis_pcmout(&mVorbisState->mDsp, &pcm)) > 0) {
    float* buffer = new float[samples * channels];
    float* p = buffer;
    for (PRUint32 i = 0; i < PRUint32(samples); ++i) {
      for (PRUint32 j = 0; j < channels; ++j) {
        *p++ = pcm[j][i];
      }
    }

    PRInt64 duration = mVorbisState->Time((PRInt64)samples);
    PRInt64 startTime = (mVorbisGranulepos != -1) ?
      mVorbisState->Time(mVorbisGranulepos) : -1;
    SoundData* s = new SoundData(mPageOffset,
                                 startTime,
                                 duration,
                                 samples,
                                 buffer,
                                 channels);
    if (mVorbisGranulepos != -1) {
      mVorbisGranulepos += samples;
    }
    if (!aChunks.AppendElement(s)) {
      delete s;
    }
    if (vorbis_synthesis_read(&mVorbisState->mDsp, samples) != 0) {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

PRBool nsOggReader::DecodeAudioData()
{
  MonitorAutoEnter mon(mMonitor);
  NS_ASSERTION(mDecoder->OnStateMachineThread() || mDecoder->OnDecodeThread(),
               "Should be on playback or decode thread.");
  NS_ASSERTION(mVorbisState!=0, "Need Vorbis state to decode audio");
  ogg_packet packet;
  packet.granulepos = -1;

  PRBool endOfStream = PR_FALSE;

  nsAutoTArray<nsAutoPtr<SoundData>, 64> chunks;
  if (mVorbisGranulepos == -1) {
    // Not captured Vorbis granulepos, read up until we get a granulepos, and
    // back propagate the granulepos.

    // We buffer the packets' pcm samples until we reach a packet with a granulepos.
    // This will be the last packet in a page. Then using that granulepos to 
    // calculate the packet's end time, we calculate all the packets' start times by
    // subtracting their durations.

    // Ensure we've got Vorbis packets; read one more Vorbis page if necessary.
    while (packet.granulepos <= 0 && !endOfStream) {
      if (!ReadOggPacket(mVorbisState, &packet)) {
        endOfStream = PR_TRUE;
        break;
      }
      if (packet.e_o_s != 0) {
        // This packet marks the logical end of the Vorbis bitstream. It may
        // still contain sound samples, so we must still decode it.
        endOfStream = PR_TRUE;
      }

      if (NS_FAILED(DecodeVorbis(chunks, &packet))) {
        NS_WARNING("Failed to decode Vorbis packet");
      }
    }

    if (packet.granulepos > 0) {
      // Successfully read up to a non -1 granulepos.
      // Calculate the timestamps of the sound samples.
      PRInt64 granulepos = packet.granulepos; // Represents end time of last sample.
      mVorbisGranulepos = packet.granulepos;
      for (int i = chunks.Length() - 1; i >= 0; --i) {
        SoundData* s = chunks[i];
        PRInt64 startGranule = granulepos - s->mSamples;
        s->mTime = mVorbisState->Time(startGranule);
        granulepos = startGranule;
      }
    }
  } else {
    // We have already captured the granulepos. The next packet's granulepos
    // is its number of samples, plus the previous granulepos.
    if (!ReadOggPacket(mVorbisState, &packet)) {
      endOfStream = PR_TRUE;
    } else {
      // Successfully read a packet from the file. Decode it.
      endOfStream = packet.e_o_s != 0;

      // Try to decode any packet we've read.
      if (NS_FAILED(DecodeVorbis(chunks, &packet))) {
        NS_WARNING("Failed to decode Vorbis packet");
      }

      if (packet.granulepos != -1 && packet.granulepos != mVorbisGranulepos) {
        // If the packet's granulepos doesn't match our running sample total,
        // it's likely the bitstream has been damaged somehow, or perhaps
        // oggz-chopped. Just assume the packet's granulepos is correct...
        mVorbisGranulepos = packet.granulepos;
      }
    }
  }

  // We've successfully decoded some sound chunks. Push them onto the audio
  // queue.
  for (PRUint32 i = 0; i < chunks.Length(); ++i) {
    mAudioQueue.Push(chunks[i].forget());
  }

  if (endOfStream) {
    // We've encountered an end of bitstream packet, or we've hit the end of
    // file while trying to decode, so inform the audio queue that there'll
    // be no more samples.
    mAudioQueue.Finish();
    return PR_FALSE;
  }

  return PR_TRUE;
}

// Returns 1 if the Theora info struct is decoding a media of Theora
// verion (maj,min,sub) or later, otherwise returns 0.
static int
TheoraVersion(th_info* info,
              unsigned char maj,
              unsigned char min,
              unsigned char sub)
{
  ogg_uint32_t ver = (maj << 16) + (min << 8) + sub;
  ogg_uint32_t th_ver = (info->version_major << 16) +
                        (info->version_minor << 8) +
                        info->version_subminor;
  return (th_ver >= ver) ? 1 : 0;
}

#ifdef DEBUG
// Ensures that all the VideoData in aFrames array are stored in increasing
// order by timestamp. Used in assertions in debug builds.
static PRBool
AllFrameTimesIncrease(nsTArray<nsAutoPtr<VideoData> >& aFrames)
{
  PRInt64 prevTime = -1;
  PRInt64 prevGranulepos = -1;
  for (PRUint32 i = 0; i < aFrames.Length(); i++) {
    VideoData* f = aFrames[i];
    if (f->mTime < prevTime) {
      return PR_FALSE;
    }
    prevTime = f->mTime;
    prevGranulepos = f->mTimecode;
  }

  return PR_TRUE;
}
#endif

nsresult nsOggReader::DecodeTheora(nsTArray<nsAutoPtr<VideoData> >& aFrames,
                                   ogg_packet* aPacket)
{
  int ret = th_decode_packetin(mTheoraState->mCtx, aPacket, 0);
  if (ret != 0 && ret != TH_DUPFRAME) {
    return NS_ERROR_FAILURE;
  }
  PRInt64 time = (aPacket->granulepos != -1)
    ? mTheoraState->StartTime(aPacket->granulepos) : -1;
  PRInt64 endTime = time != -1 ? time + mTheoraState->mFrameDuration : -1;
  if (ret == TH_DUPFRAME) {
    VideoData* v = VideoData::CreateDuplicate(mPageOffset,
                                              time,
                                              endTime,
                                              aPacket->granulepos);
    if (!aFrames.AppendElement(v)) {
      delete v;
    }
  } else if (ret == 0) {
    th_ycbcr_buffer buffer;
    ret = th_decode_ycbcr_out(mTheoraState->mCtx, buffer);
    NS_ASSERTION(ret == 0, "th_decode_ycbcr_out failed");
    PRBool isKeyframe = th_packet_iskeyframe(aPacket) == 1;
    VideoData::YCbCrBuffer b;
    for (PRUint32 i=0; i < 3; ++i) {
      b.mPlanes[i].mData = buffer[i].data;
      b.mPlanes[i].mHeight = buffer[i].height;
      b.mPlanes[i].mWidth = buffer[i].width;
      b.mPlanes[i].mStride = buffer[i].stride;
    }
    VideoData *v = VideoData::Create(mInfo,
                                     mDecoder->GetImageContainer(),
                                     mPageOffset,
                                     time,
                                     endTime,
                                     b,
                                     isKeyframe,
                                     aPacket->granulepos);
    if (!v) {
      // There may be other reasons for this error, but for
      // simplicity just assume the worst case: out of memory.
      NS_WARNING("Failed to allocate memory for video frame");
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (!aFrames.AppendElement(v)) {
      delete v;
    }
  }
  return NS_OK;
}

PRBool nsOggReader::DecodeVideoFrame(PRBool &aKeyframeSkip,
                                     PRInt64 aTimeThreshold)
{
  MonitorAutoEnter mon(mMonitor);
  NS_ASSERTION(mDecoder->OnStateMachineThread() || mDecoder->OnDecodeThread(),
               "Should be on state machine or AV thread.");
  // We chose to keep track of the Theora granulepos ourselves, rather than
  // rely on th_decode_packetin() to do it for us. This is because
  // th_decode_packetin() simply works by incrementing a counter every time
  // it's called, so if we drop frames and don't call it, subsequent granulepos
  // will be wrong. Whenever we read a packet which has a granulepos, we use
  // its granulepos, otherwise we increment the previous packet's granulepos.

  nsAutoTArray<nsAutoPtr<VideoData>, 8> frames;
  ogg_packet packet;
  PRBool endOfStream = PR_FALSE;
  if (mTheoraGranulepos == -1) {
    // We've not read a Theora packet with a granulepos, so we don't know what
    // timestamp to assign to Theora frames we decode. This will only happen
    // the first time we read, or after a seek. We must read and buffer up to
    // the first Theora packet with a granulepos, and back-propagate its 
    // granulepos to calculate the buffered frames' granulepos.
    do {
      if (!ReadOggPacket(mTheoraState, &packet)) {
        // Failed to read another page, must be the end of file. We can't have
        // already encountered an end of bitstream packet, else we wouldn't be
        // here, so this bitstream must be missing its end of stream packet, or
        // is otherwise corrupt (oggz-chop can output files like this). Inform
        // the queue that there will be no more frames.
        mVideoQueue.Finish();
        return PR_FALSE;
      }

      if (packet.granulepos > 0) {
        // We've found a packet with a granulepos, we can now determine the
        // buffered packet's timestamps, as well as the timestamps for any
        // packets we read subsequently.
        mTheoraGranulepos = packet.granulepos;
      }
    
      if (DecodeTheora(frames, &packet) == NS_ERROR_OUT_OF_MEMORY) {
        NS_WARNING("Theora decode memory allocation failure!");
        return PR_FALSE;
      }

    } while (packet.granulepos <= 0 && !endOfStream);

    if (packet.granulepos > 0) {
      // We have captured a granulepos. Backpropagate the granulepos
      // to determine buffered packets' timestamps.
      PRInt64 succGranulepos = packet.granulepos;
      int version_3_2_1 = TheoraVersion(&mTheoraState->mInfo,3,2,1);
      int shift = mTheoraState->mInfo.keyframe_granule_shift;
      for (int i = frames.Length() - 2; i >= 0; --i) {
        PRInt64 granulepos = succGranulepos;
        if (frames[i]->mKeyframe) {
          // This frame is a keyframe. It's granulepos is the previous granule
          // number minus 1, shifted by granuleshift.
          ogg_int64_t frame_index = th_granule_frame(mTheoraState->mCtx,
                                                     granulepos);
          granulepos = (frame_index + version_3_2_1 - 1) << shift;
          // Theora 3.2.1+ granulepos store frame number [1..N], so granulepos
          // should be > 0.
          // Theora 3.2.0 granulepos store the frame index [0..(N-1)], so
          // granulepos should be >= 0. 
          NS_ASSERTION((version_3_2_1 && granulepos > 0) ||
                       granulepos >= 0, "Should have positive granulepos");
        } else {
          // Packet is not a keyframe. It's granulepos depends on its successor
          // packet...
          if (frames[i+1]->mKeyframe) {
            // The successor frame is a keyframe, so we can't just subtract 1
            // from the "keyframe offset" part of its granulepos, as it
            // doesn't have one! So fake it, take the keyframe offset as the
            // max possible keyframe offset. This means the granulepos (probably)
            // overshoots and claims that it depends on a frame before its actual
            // keyframe but at least its granule number will be correct, so the
            // times we calculate from this granulepos will also be correct.
            ogg_int64_t frameno = th_granule_frame(mTheoraState->mCtx,
                                                   granulepos);
            ogg_int64_t max_offset = NS_MIN((frameno - 1),
                                         (ogg_int64_t)(1 << shift) - 1);
            ogg_int64_t granule = frameno +
                                  TheoraVersion(&mTheoraState->mInfo,3,2,1) -
                                  1 - max_offset;
            NS_ASSERTION(granule > 0, "Must have positive granulepos");
            granulepos = (granule << shift) + max_offset;
          } else {
            // Neither previous nor this frame are keyframes, so we can just
            // decrement the previous granulepos to calculate this frames
            // granulepos.
            --granulepos;
          }
        }
        // Check that the frame's granule number (it's frame number) is
        // one less than the successor frame.
        NS_ASSERTION(th_granule_frame(mTheoraState->mCtx, succGranulepos) ==
                     th_granule_frame(mTheoraState->mCtx, granulepos) + 1,
                     "Granulepos calculation is incorrect!");
        frames[i]->mTime = mTheoraState->StartTime(granulepos);
        frames[i]->mEndTime = frames[i]->mTime + mTheoraState->mFrameDuration;
        NS_ASSERTION(frames[i]->mEndTime >= frames[i]->mTime, "Frame must start before it ends.");
        frames[i]->mTimecode = granulepos;
        succGranulepos = granulepos;
        NS_ASSERTION(frames[i]->mTime < frames[i+1]->mTime, "Times should increase");      
      }
      NS_ASSERTION(AllFrameTimesIncrease(frames), "All frames must have granulepos");
    }
  } else {
    
    NS_ASSERTION(mTheoraGranulepos > 0, "We must Theora granulepos!");
    
    if (!ReadOggPacket(mTheoraState, &packet)) {
      // Failed to read from file, so EOF or other premature failure.
      // Inform the queue that there will be no more frames.
      mVideoQueue.Finish();
      return PR_FALSE;
    }

    endOfStream = packet.e_o_s != 0;

    // Maintain the Theora granulepos. We must do this even if we drop frames,
    // otherwise our clock will be wrong after we've skipped frames.
    if (packet.granulepos != -1) {
      // Incoming packet has a granulepos, use that as it's granulepos.
      mTheoraGranulepos = packet.granulepos;
    } else {
      // Increment the previous Theora granulepos.
      PRInt64 granulepos = 0;
      int shift = mTheoraState->mInfo.keyframe_granule_shift;
      // Theora 3.2.1+ bitstreams granulepos store frame number; [1..N]
      // Theora 3.2.0 bitstreams store the frame index; [0..(N-1)]
      if (!th_packet_iskeyframe(&packet)) {
        granulepos = mTheoraGranulepos + 1;
      } else {
        ogg_int64_t frameindex = th_granule_frame(mTheoraState->mCtx,
                                                  mTheoraGranulepos);
        ogg_int64_t granule = frameindex +
                              TheoraVersion(&mTheoraState->mInfo,3,2,1) + 1;
        NS_ASSERTION(granule > 0, "Must have positive granulepos");
        granulepos = granule << shift;
      }

      NS_ASSERTION(th_granule_frame(mTheoraState->mCtx, mTheoraGranulepos) + 1 == 
                   th_granule_frame(mTheoraState->mCtx, granulepos),
                   "Frame number must increment by 1");
      packet.granulepos = mTheoraGranulepos = granulepos;
    }

    PRInt64 time = mTheoraState->StartTime(mTheoraGranulepos);
    NS_ASSERTION(packet.granulepos != -1, "Must know packet granulepos");
    if (!aKeyframeSkip ||
        (th_packet_iskeyframe(&packet) == 1 && time >= aTimeThreshold))
    {
      if (DecodeTheora(frames, &packet) == NS_ERROR_OUT_OF_MEMORY) {
        NS_WARNING("Theora decode memory allocation failure");
        return PR_FALSE;
      }
    }
  }

  // Push decoded data into the video frame queue.
  for (PRUint32 i = 0; i < frames.Length(); i++) {
    nsAutoPtr<VideoData> data(frames[i].forget());
    if (aKeyframeSkip && data->mKeyframe) {
      aKeyframeSkip = PR_FALSE;
    }

    if (!aKeyframeSkip) {
      mVideoQueue.Push(data.forget());
    }
  }

  if (endOfStream) {
    // We've encountered an end of bitstream packet. Inform the queue that
    // there will be no more frames.
    mVideoQueue.Finish();
  }

  return !endOfStream;
}

PRInt64 nsOggReader::ReadOggPage(ogg_page* aPage)
{
  NS_ASSERTION(mDecoder->OnStateMachineThread() || mDecoder->OnDecodeThread(),
               "Should be on play state machine or decode thread.");
  mMonitor.AssertCurrentThreadIn();

  int ret = 0;
  while((ret = ogg_sync_pageseek(&mOggState, aPage)) <= 0) {
    if (ret < 0) {
      // Lost page sync, have to skip up to next page.
      mPageOffset += -ret;
      continue;
    }
    // Returns a buffer that can be written too
    // with the given size. This buffer is stored
    // in the ogg synchronisation structure.
    char* buffer = ogg_sync_buffer(&mOggState, 4096);
    NS_ASSERTION(buffer, "ogg_sync_buffer failed");

    // Read from the stream into the buffer
    PRUint32 bytesRead = 0;

    nsresult rv = mDecoder->GetCurrentStream()->Read(buffer, 4096, &bytesRead);
    if (NS_FAILED(rv) || (bytesRead == 0 && ret == 0)) {
      // End of file.
      return -1;
    }

    mDecoder->NotifyBytesConsumed(bytesRead);
    // Update the synchronisation layer with the number
    // of bytes written to the buffer
    ret = ogg_sync_wrote(&mOggState, bytesRead);
    NS_ENSURE_TRUE(ret == 0, -1);    
  }
  PRInt64 offset = mPageOffset;
  mPageOffset += aPage->header_len + aPage->body_len;
  
  return offset;
}

PRBool nsOggReader::ReadOggPacket(nsOggCodecState* aCodecState,
                                  ogg_packet* aPacket)
{
  NS_ASSERTION(mDecoder->OnStateMachineThread() || mDecoder->OnDecodeThread(),
               "Should be on play state machine or decode thread.");
  mMonitor.AssertCurrentThreadIn();

  if (!aCodecState || !aCodecState->mActive) {
    return PR_FALSE;
  }

  int ret = 0;
  while ((ret = ogg_stream_packetout(&aCodecState->mState, aPacket)) != 1) {
    ogg_page page;

    if (aCodecState->PageInFromBuffer()) {
      // The codec state has inserted a previously buffered page into its
      // ogg_stream_state, no need to read a page from the channel.
      continue;
    }

    // The codec state does not have any buffered pages, so try to read another
    // page from the channel.
    if (ReadOggPage(&page) == -1) {
      return PR_FALSE;
    }

    PRUint32 serial = ogg_page_serialno(&page);
    nsOggCodecState* codecState = nsnull;
    mCodecStates.Get(serial, &codecState);

    if (serial == aCodecState->mSerial) {
      // This page is from our target bitstream, insert it into the
      // codec state's ogg_stream_state so we can read a packet.
      ret = ogg_stream_pagein(&codecState->mState, &page);
      NS_ENSURE_TRUE(ret == 0, PR_FALSE);
    } else if (codecState && codecState->mActive) {
      // Page is for another active bitstream, add the page to its codec
      // state's buffer for later consumption when that stream next tries
      // to read a packet.
      codecState->AddToBuffer(&page);
    }
  }

  return PR_TRUE;
}

// Returns an ogg page's checksum.
static ogg_uint32_t
GetChecksum(ogg_page* page)
{
  if (page == 0 || page->header == 0 || page->header_len < 25) {
    return 0;
  }
  const unsigned char* p = page->header + 22;
  PRUint32 c =  p[0] +
               (p[1] << 8) + 
               (p[2] << 16) +
               (p[3] << 24);
  return c;
}

VideoData* nsOggReader::FindStartTime(PRInt64 aOffset,
                                      PRInt64& aOutStartTime)
{
  NS_ASSERTION(mDecoder->OnStateMachineThread(),
               "Should be on state machine thread.");
  nsMediaStream* stream = mDecoder->GetCurrentStream();
  NS_ENSURE_TRUE(stream != nsnull, nsnull);
  nsresult res = stream->Seek(nsISeekableStream::NS_SEEK_SET, aOffset);
  NS_ENSURE_SUCCESS(res, nsnull);
  return nsBuiltinDecoderReader::FindStartTime(aOffset, aOutStartTime);
}

PRInt64 nsOggReader::FindEndTime(PRInt64 aEndOffset)
{
  MonitorAutoEnter mon(mMonitor);
  NS_ASSERTION(mDecoder->OnStateMachineThread(),
               "Should be on state machine thread.");
  PRInt64 endTime = FindEndTime(aEndOffset, PR_FALSE, &mOggState);
  // Reset read head to start of media data.
  NS_ASSERTION(mDataOffset > 0,
               "Should have offset of first non-header page");
  nsMediaStream* stream = mDecoder->GetCurrentStream();
  NS_ENSURE_TRUE(stream != nsnull, -1);
  nsresult res = stream->Seek(nsISeekableStream::NS_SEEK_SET, mDataOffset);
  NS_ENSURE_SUCCESS(res, -1);
  return endTime;
}

PRInt64 nsOggReader::FindEndTime(PRInt64 aEndOffset,
                                 PRBool aCachedDataOnly,
                                 ogg_sync_state* aState)
{
  nsMediaStream* stream = mDecoder->GetCurrentStream();
  ogg_sync_reset(aState);

  // We need to find the last page which ends before aEndOffset that
  // has a granulepos that we can convert to a timestamp. We do this by
  // backing off from aEndOffset until we encounter a page on which we can
  // interpret the granulepos. If while backing off we encounter a page which
  // we've previously encountered before, we'll either backoff again if we
  // haven't found an end time yet, or return the last end time found.
  const int step = 5000;
  PRInt64 readStartOffset = aEndOffset;
  PRInt64 readHead = aEndOffset;
  PRInt64 endTime = -1;
  PRUint32 checksumAfterSeek = 0;
  PRUint32 prevChecksumAfterSeek = 0;
  PRBool mustBackOff = PR_FALSE;
  while (PR_TRUE) {
    ogg_page page;    
    int ret = ogg_sync_pageseek(aState, &page);
    if (ret == 0) {
      // We need more data if we've not encountered a page we've seen before,
      // or we've read to the end of file.
      if (mustBackOff || readHead == aEndOffset) {
        if (endTime != -1 || readStartOffset == 0) {
          // We have encountered a page before, or we're at the end of file.
          break;
        }
        mustBackOff = PR_FALSE;
        prevChecksumAfterSeek = checksumAfterSeek;
        checksumAfterSeek = 0;
        ogg_sync_reset(aState);
        readStartOffset = NS_MAX(static_cast<PRInt64>(0), readStartOffset - step);
        readHead = readStartOffset;
      }

      PRInt64 limit = NS_MIN(static_cast<PRInt64>(PR_UINT32_MAX),
                             aEndOffset - readHead);
      limit = NS_MAX(static_cast<PRInt64>(0), limit);
      limit = NS_MIN(limit, static_cast<PRInt64>(step));
      PRUint32 bytesToRead = static_cast<PRUint32>(limit);
      PRUint32 bytesRead = 0;
      char* buffer = ogg_sync_buffer(aState, bytesToRead);
      NS_ASSERTION(buffer, "Must have buffer");
      nsresult res;
      if (aCachedDataOnly) {
        res = stream->ReadFromCache(buffer, readHead, bytesToRead);
        NS_ENSURE_SUCCESS(res,res);
        bytesRead = bytesToRead;
      } else {
        NS_ASSERTION(readHead < aEndOffset,
                     "Stream pos must be before range end");
        res = stream->Seek(nsISeekableStream::NS_SEEK_SET, readHead);
        NS_ENSURE_SUCCESS(res,res);
        res = stream->Read(buffer, bytesToRead, &bytesRead);
        NS_ENSURE_SUCCESS(res,res);
      }
      readHead += bytesRead;

      // Update the synchronisation layer with the number
      // of bytes written to the buffer
      ret = ogg_sync_wrote(aState, bytesRead);
      if (ret != 0) {
        endTime = -1;
        break;
      }

      continue;
    }

    if (ret < 0 || ogg_page_granulepos(&page) < 0) {
      continue;
    }

    PRUint32 checksum = GetChecksum(&page);
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
      mustBackOff = PR_TRUE;
      continue;
    }

    PRInt64 granulepos = ogg_page_granulepos(&page);
    int serial = ogg_page_serialno(&page);

    nsOggCodecState* codecState = nsnull;
    mCodecStates.Get(serial, &codecState);

    if (!codecState) {
      // This page is from a bitstream which we haven't encountered yet.
      // It's probably from a new "link" in a "chained" ogg. Don't
      // bother even trying to find a duration...
      endTime = -1;
      break;
    }

    PRInt64 t = codecState->Time(granulepos);
    if (t != -1) {
      endTime = t;
    }
  }

  ogg_sync_reset(aState);

  return endTime;
}

nsOggReader::IndexedSeekResult nsOggReader::RollbackIndexedSeek(PRInt64 aOffset)
{
  mSkeletonState->Deactivate();
  nsMediaStream* stream = mDecoder->GetCurrentStream();
  NS_ENSURE_TRUE(stream != nsnull, SEEK_FATAL_ERROR);
  nsresult res = stream->Seek(nsISeekableStream::NS_SEEK_SET, aOffset);
  NS_ENSURE_SUCCESS(res, SEEK_FATAL_ERROR);
  return SEEK_INDEX_FAIL;
}
 
nsOggReader::IndexedSeekResult nsOggReader::SeekToKeyframeUsingIndex(PRInt64 aTarget)
{
  nsMediaStream* stream = mDecoder->GetCurrentStream();
  NS_ENSURE_TRUE(stream != nsnull, SEEK_FATAL_ERROR);
  if (!HasSkeleton() || !mSkeletonState->HasIndex()) {
    return SEEK_INDEX_FAIL;
  }
  // We have an index from the Skeleton track, try to use it to seek.
  nsAutoTArray<PRUint32, 2> tracks;
  if (HasVideo()) {
    tracks.AppendElement(mTheoraState->mSerial);
  }
  if (HasAudio()) {
    tracks.AppendElement(mVorbisState->mSerial);
  }
  nsSkeletonState::nsSeekTarget keyframe;
  if (NS_FAILED(mSkeletonState->IndexedSeekTarget(aTarget,
                                                  tracks,
                                                  keyframe)))
  {
    // Could not locate a keypoint for the target in the index.
    return SEEK_INDEX_FAIL;
  }

  // Remember original stream read cursor position so we can rollback on failure.
  PRInt64 tell = stream->Tell();

  // Seek to the keypoint returned by the index.
  if (keyframe.mKeyPoint.mOffset > stream->GetLength() ||
      keyframe.mKeyPoint.mOffset < 0)
  {
    // Index must be invalid.
    return RollbackIndexedSeek(tell);
  }
  LOG(PR_LOG_DEBUG, ("Seeking using index to keyframe at offset %lld\n",
                     keyframe.mKeyPoint.mOffset));
  nsresult res = stream->Seek(nsISeekableStream::NS_SEEK_SET,
                              keyframe.mKeyPoint.mOffset);
  NS_ENSURE_SUCCESS(res, SEEK_FATAL_ERROR);
  mPageOffset = keyframe.mKeyPoint.mOffset;

  // We've moved the read set, so reset decode.
  res = ResetDecode();
  NS_ENSURE_SUCCESS(res, SEEK_FATAL_ERROR);

  // Check that the page the index thinks is exactly here is actually exactly
  // here. If not, the index is invalid.
  ogg_page page;
  int skippedBytes = 0;
  PageSyncResult syncres = PageSync(stream,
                                    &mOggState,
                                    PR_FALSE,
                                    mPageOffset,
                                    stream->GetLength(),
                                    &page,
                                    skippedBytes);
  NS_ENSURE_TRUE(syncres != PAGE_SYNC_ERROR, SEEK_FATAL_ERROR);
  if (syncres != PAGE_SYNC_OK || skippedBytes != 0) {
    LOG(PR_LOG_DEBUG, ("Indexed-seek failure: Ogg Skeleton Index is invalid "
                       "or sync error after seek"));
    return RollbackIndexedSeek(tell);
  }
  PRUint32 serial = ogg_page_serialno(&page);
  if (serial != keyframe.mSerial) {
    // Serialno of page at offset isn't what the index told us to expect.
    // Assume the index is invalid.
    return RollbackIndexedSeek(tell);
  }
  nsOggCodecState* codecState = nsnull;
  mCodecStates.Get(serial, &codecState);
  if (codecState &&
      codecState->mActive &&
      ogg_stream_pagein(&codecState->mState, &page) != 0)
  {
    // Couldn't insert page into the ogg stream, or somehow the stream
    // is no longer active.
    return RollbackIndexedSeek(tell);
  }      
  mPageOffset = keyframe.mKeyPoint.mOffset + page.header_len + page.body_len;
  return SEEK_OK;
}

nsresult nsOggReader::SeekInBufferedRange(PRInt64 aTarget,
                                          PRInt64 aStartTime,
                                          PRInt64 aEndTime,
                                          const nsTArray<ByteRange>& aRanges,
                                          const ByteRange& aRange)
{
  LOG(PR_LOG_DEBUG, ("%p Seeking in buffered data to %lldms using bisection search", mDecoder, aTarget));

  // We know the exact byte range in which the target must lie. It must
  // be buffered in the media cache. Seek there.
  nsresult res = SeekBisection(aTarget, aRange, 0);
  if (NS_FAILED(res) || !HasVideo()) {
    return res;
  }

  // We have an active Theora bitstream. Decode the next Theora frame, and
  // extract its keyframe's time.
  PRBool eof;
  do {
    PRBool skip = PR_FALSE;
    eof = !DecodeVideoFrame(skip, 0);
    {
      MonitorAutoExit exitReaderMon(mMonitor);
      MonitorAutoEnter decoderMon(mDecoder->GetMonitor());
      if (mDecoder->GetDecodeState() == nsBuiltinDecoderStateMachine::DECODER_STATE_SHUTDOWN) {
        return NS_ERROR_FAILURE;
      }
    }
  } while (!eof &&
           mVideoQueue.GetSize() == 0);

  VideoData* video = mVideoQueue.PeekFront();
  if (video && !video->mKeyframe) {
    // First decoded frame isn't a keyframe, seek back to previous keyframe,
    // otherwise we'll get visual artifacts.
    NS_ASSERTION(video->mTimecode != -1, "Must have a granulepos");
    int shift = mTheoraState->mInfo.keyframe_granule_shift;
    PRInt64 keyframeGranulepos = (video->mTimecode >> shift) << shift;
    PRInt64 keyframeTime = mTheoraState->StartTime(keyframeGranulepos);
    SEEK_LOG(PR_LOG_DEBUG, ("Keyframe for %lld is at %lld, seeking back to it",
                            video->mTime, keyframeTime));
    ByteRange k = GetSeekRange(aRanges,
                               keyframeTime,
                               aStartTime,
                               aEndTime,
                               PR_FALSE);
    res = SeekBisection(keyframeTime, k, SEEK_FUZZ_MS);
    NS_ASSERTION(mTheoraGranulepos == -1, "SeekBisection must reset Theora decode");
    NS_ASSERTION(mVorbisGranulepos == -1, "SeekBisection must reset Vorbis decode");
  }
  return res;
}

PRBool nsOggReader::CanDecodeToTarget(PRInt64 aTarget,
                                      PRInt64 aCurrentTime)
{
  // We can decode to the target if the target is no further than the
  // maximum keyframe offset ahead of the current playback position, if
  // we have video, or SEEK_DECODE_MARGIN if we don't have video.
  PRInt64 margin = HasVideo() ? mTheoraState->MaxKeyframeOffset() : SEEK_DECODE_MARGIN;
  return aTarget >= aCurrentTime &&
         aTarget - aCurrentTime < margin;
}

nsresult nsOggReader::SeekInUnbuffered(PRInt64 aTarget,
                                       PRInt64 aStartTime,
                                       PRInt64 aEndTime,
                                       const nsTArray<ByteRange>& aRanges)
{
  LOG(PR_LOG_DEBUG, ("%p Seeking in unbuffered data to %lldms using bisection search", mDecoder, aTarget));
  
  // If we've got an active Theora bitstream, determine the maximum possible
  // time in ms which a keyframe could be before a given interframe. We
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
  PRInt64 keyframeOffsetMs = 0;
  if (HasVideo() && mTheoraState) {
    keyframeOffsetMs = mTheoraState->MaxKeyframeOffset();
  }
  PRInt64 seekTarget = NS_MAX(aStartTime, aTarget - keyframeOffsetMs);
  // Minimize the bisection search space using the known timestamps from the
  // buffered ranges.
  ByteRange k = GetSeekRange(aRanges, seekTarget, aStartTime, aEndTime, PR_FALSE);
  nsresult res = SeekBisection(seekTarget, k, SEEK_FUZZ_MS);
  NS_ASSERTION(mTheoraGranulepos == -1, "SeekBisection must reset Theora decode");
  NS_ASSERTION(mVorbisGranulepos == -1, "SeekBisection must reset Vorbis decode");
  return res;
}

nsresult nsOggReader::Seek(PRInt64 aTarget,
                           PRInt64 aStartTime,
                           PRInt64 aEndTime,
                           PRInt64 aCurrentTime)
{
  MonitorAutoEnter mon(mMonitor);
  NS_ASSERTION(mDecoder->OnStateMachineThread(),
               "Should be on state machine thread.");
  LOG(PR_LOG_DEBUG, ("%p About to seek to %lldms", mDecoder, aTarget));
  nsresult res;
  nsMediaStream* stream = mDecoder->GetCurrentStream();
  NS_ENSURE_TRUE(stream != nsnull, NS_ERROR_FAILURE);

  if (aTarget == aStartTime) {
    // We've seeked to the media start. Just seek to the offset of the first
    // content page.
    res = stream->Seek(nsISeekableStream::NS_SEEK_SET, mDataOffset);
    NS_ENSURE_SUCCESS(res,res);

    mPageOffset = mDataOffset;
    res = ResetDecode();
    NS_ENSURE_SUCCESS(res,res);

    NS_ASSERTION(aStartTime != -1, "mStartTime should be known");
    {
      MonitorAutoExit exitReaderMon(mMonitor);
      MonitorAutoEnter decoderMon(mDecoder->GetMonitor());
      mDecoder->UpdatePlaybackPosition(aStartTime);
    }
  } else if (CanDecodeToTarget(aTarget, aCurrentTime)) {
    LOG(PR_LOG_DEBUG, ("%p Seek target (%lld) is close to current time (%lld), "
        "will just decode to it", mDecoder, aCurrentTime, aTarget));
  } else {
    IndexedSeekResult sres = SeekToKeyframeUsingIndex(aTarget);
    NS_ENSURE_TRUE(sres != SEEK_FATAL_ERROR, NS_ERROR_FAILURE);
    if (sres == SEEK_INDEX_FAIL) {
      // No index or other non-fatal index-related failure. Try to seek
      // using a bisection search. Determine the already downloaded data
      // in the media cache, so we can try to seek in the cached data first.
      nsAutoTArray<ByteRange, 16> ranges;
      res = GetBufferedBytes(ranges);
      NS_ENSURE_SUCCESS(res,res);

      // Figure out if the seek target lies in a buffered range.
      ByteRange r = GetSeekRange(ranges, aTarget, aStartTime, aEndTime, PR_TRUE);

      if (!r.IsNull()) {
        // We know the buffered range in which the seek target lies, do a
        // bisection search in that buffered range.
        res = SeekInBufferedRange(aTarget, aStartTime, aEndTime, ranges, r);
        NS_ENSURE_SUCCESS(res,res);
      } else {
        // The target doesn't lie in a buffered range. Perform a bisection
        // search over the whole media, using the known buffered ranges to
        // reduce the search space.
        res = SeekInUnbuffered(aTarget, aStartTime, aEndTime, ranges);
        NS_ENSURE_SUCCESS(res,res);
      }
    }
  }

  // The decode position must now be either close to the seek target, or
  // we've seeked to before the keyframe before the seek target. Decode
  // forward to the seek target frame.
  return DecodeToTarget(aTarget);
}

// Reads a page from the media stream.
static PageSyncResult
PageSync(nsMediaStream* aStream,
         ogg_sync_state* aState,
         PRBool aCachedDataOnly,
         PRInt64 aOffset,
         PRInt64 aEndOffset,
         ogg_page* aPage,
         int& aSkippedBytes)
{
  aSkippedBytes = 0;
  // Sync to the next page.
  int ret = 0;
  PRUint32 bytesRead = 0;
  PRInt64 readHead = aOffset;
  while (ret <= 0) {
    ret = ogg_sync_pageseek(aState, aPage);
    if (ret == 0) {
      char* buffer = ogg_sync_buffer(aState, PAGE_STEP);
      NS_ASSERTION(buffer, "Must have a buffer");

      // Read from the file into the buffer
      PRInt64 bytesToRead = NS_MIN(static_cast<PRInt64>(PAGE_STEP),
                                   aEndOffset - readHead);
      if (bytesToRead <= 0) {
        return PAGE_SYNC_END_OF_RANGE;
      }
      nsresult rv = NS_OK;
      if (aCachedDataOnly) {
        rv = aStream->ReadFromCache(buffer, readHead, bytesToRead);
        NS_ENSURE_SUCCESS(rv,PAGE_SYNC_ERROR);
        bytesRead = bytesToRead;
      } else {
        rv = aStream->Seek(nsISeekableStream::NS_SEEK_SET, readHead);
        NS_ENSURE_SUCCESS(rv,PAGE_SYNC_ERROR);
        rv = aStream->Read(buffer,
                           static_cast<PRUint32>(bytesToRead),
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

nsresult nsOggReader::SeekBisection(PRInt64 aTarget,
                                    const ByteRange& aRange,
                                    PRUint32 aFuzz)
{
  NS_ASSERTION(mDecoder->OnStateMachineThread(),
               "Should be on state machine thread.");
  nsresult res;
  nsMediaStream* stream = mDecoder->GetCurrentStream();

  if (aTarget == aRange.mTimeStart) {
    if (NS_FAILED(ResetDecode())) {
      return NS_ERROR_FAILURE;
    }
    res = stream->Seek(nsISeekableStream::NS_SEEK_SET, mDataOffset);
    NS_ENSURE_SUCCESS(res,res);
    mPageOffset = mDataOffset;
    return NS_OK;
  }

  // Bisection search, find start offset of last page with end time less than
  // the seek target.
  ogg_int64_t startOffset = aRange.mOffsetStart;
  ogg_int64_t startTime = aRange.mTimeStart;
  ogg_int64_t startLength = 0;
  ogg_int64_t endOffset = aRange.mOffsetEnd;
  ogg_int64_t endTime = aRange.mTimeEnd;

  ogg_int64_t seekTarget = aTarget;
  PRInt64 seekLowerBound = NS_MAX(static_cast<PRInt64>(0), aTarget - aFuzz);
  int hops = 0;
  ogg_int64_t previousGuess = -1;
  int backsteps = 1;
  const int maxBackStep = 10;
  NS_ASSERTION(static_cast<PRUint64>(PAGE_STEP) * pow(2.0, maxBackStep) < PR_INT32_MAX,
               "Backstep calculation must not overflow");
  while (PR_TRUE) {
    ogg_int64_t duration = 0;
    double target = 0;
    ogg_int64_t interval = 0;
    ogg_int64_t guess = 0;
    ogg_page page;
    int skippedBytes = 0;
    ogg_int64_t pageOffset = 0;
    ogg_int64_t pageLength = 0;
    int backoff = 0;
    ogg_int64_t granuleTime = -1;
    PRInt64 oldPageOffset = 0;

    // Guess where we should bisect to, based on the bit rate and the time
    // remaining in the interval.
    while (PR_TRUE) {
  
      // Discard any previously buffered packets/pages.
      if (NS_FAILED(ResetDecode())) {
        return NS_ERROR_FAILURE;
      }

      // Guess bisection point.
      duration = endTime - startTime;
      target = (double)(seekTarget - startTime) / (double)duration;
      interval = endOffset - startOffset - startLength;
      guess = startOffset + startLength +
              (ogg_int64_t)((double)interval * target) - backoff;
      guess = NS_MIN(guess, endOffset - PAGE_STEP);
      guess = NS_MAX(guess, startOffset + startLength);

      if (interval == 0 || guess == previousGuess) {
        interval = 0;
        // Our interval is empty, we've found the optimal seek point, as the
        // start page is before the seek target, and the end page after the
        // seek target.
        break;
      }

      NS_ASSERTION(guess >= startOffset + startLength, "Guess must be after range start");
      NS_ASSERTION(guess < endOffset, "Guess must be before range end");
      NS_ASSERTION(guess != previousGuess, "Guess should be differnt to previous");
      previousGuess = guess;

      SEEK_LOG(PR_LOG_DEBUG, ("Seek loop offset_start=%lld start_end=%lld "
                              "offset_guess=%lld offset_end=%lld interval=%lld "
                              "target=%lf time_start=%lld time_end=%lld",
                              startOffset, (startOffset+startLength), guess,
                              endOffset, interval, target, startTime, endTime));
      hops++;
    
      // Locate the next page after our seek guess, and then figure out the
      // granule time of the audio and video bitstreams there. We can then
      // make a bisection decision based on our location in the media.
      PageSyncResult res = PageSync(stream,
                                    &mOggState,
                                    PR_FALSE,
                                    guess,
                                    endOffset,
                                    &page,
                                    skippedBytes);
      NS_ENSURE_TRUE(res != PAGE_SYNC_ERROR, NS_ERROR_FAILURE);

      // We've located a page of length |ret| at |guess + skippedBytes|.
      // Remember where the page is located.
      pageOffset = guess + skippedBytes;
      pageLength = page.header_len + page.body_len;
      mPageOffset = pageOffset + pageLength;

      if (mPageOffset == endOffset || res == PAGE_SYNC_END_OF_RANGE) {
        // Our guess was too close to the end, we've ended up reading the end
        // page. Backoff exponentially from the end point, in case the last
        // page/frame/sample is huge.
        backsteps = NS_MIN(backsteps + 1, maxBackStep);
        backoff = PAGE_STEP * pow(2.0, backsteps);
        continue;
      }

      NS_ASSERTION(mPageOffset < endOffset, "Page read cursor should be inside range");

      // Read pages until we can determine the granule time of the audio and 
      // video bitstream.
      ogg_int64_t audioTime = -1;
      ogg_int64_t videoTime = -1;
      int ret;
      oldPageOffset = mPageOffset;
      while ((mVorbisState && audioTime == -1) ||
             (mTheoraState && videoTime == -1)) {
      
        // Add the page to its codec state, determine its granule time.
        PRUint32 serial = ogg_page_serialno(&page);
        nsOggCodecState* codecState = nsnull;
        mCodecStates.Get(serial, &codecState);
        if (codecState && codecState->mActive) {
          ret = ogg_stream_pagein(&codecState->mState, &page);
          NS_ENSURE_TRUE(ret == 0, NS_ERROR_FAILURE);
        }      

        ogg_int64_t granulepos = ogg_page_granulepos(&page);

        if (HasAudio() &&
            granulepos != -1 &&
            serial == mVorbisState->mSerial &&
            audioTime == -1) {
          audioTime = mVorbisState->Time(granulepos);
        }
        
        if (HasVideo() &&
            granulepos != -1 &&
            serial == mTheoraState->mSerial &&
            videoTime == -1) {
          videoTime = mTheoraState->StartTime(granulepos);
        }

        mPageOffset += page.header_len + page.body_len;
        if (ReadOggPage(&page) == -1) {
          break;
        }
      }

      if ((HasAudio() && audioTime == -1) ||
          (HasVideo() && videoTime == -1)) 
      {
        backsteps = NS_MIN(backsteps + 1, maxBackStep);
        backoff = PAGE_STEP * pow(2.0, backsteps);
        continue;
      }

      // We've found appropriate time stamps here. Proceed to bisect
      // the search space.
      granuleTime = NS_MAX(audioTime, videoTime);
      NS_ASSERTION(granuleTime > 0, "Must get a granuletime");
      break;
    }

    if (interval == 0) {
      // Seek termination condition; we've found the page boundary of the
      // last page before the target, and the first page after the target.
      SEEK_LOG(PR_LOG_DEBUG, ("Seek loop (interval == 0) break"));
      NS_ASSERTION(startTime < aTarget, "Start time must always be less than target");
      res = stream->Seek(nsISeekableStream::NS_SEEK_SET, startOffset);
      NS_ENSURE_SUCCESS(res,res);
      mPageOffset = startOffset;
      if (NS_FAILED(ResetDecode())) {
        return NS_ERROR_FAILURE;
      }
      break;
    }

    SEEK_LOG(PR_LOG_DEBUG, ("Time at offset %lld is %lldms", guess, granuleTime));
    if (granuleTime < seekTarget && granuleTime > seekLowerBound) {
      // We're within the fuzzy region in which we want to terminate the search.
      res = stream->Seek(nsISeekableStream::NS_SEEK_SET, oldPageOffset);
      NS_ENSURE_SUCCESS(res,res);
      mPageOffset = oldPageOffset;
      if (NS_FAILED(ResetDecode())) {
        return NS_ERROR_FAILURE;
      }
      break;
    }

    if (granuleTime >= seekTarget) {
      // We've landed after the seek target.
      NS_ASSERTION(pageOffset < endOffset, "offset_end must decrease");
      endOffset = pageOffset;
      endTime = granuleTime;
    } else if (granuleTime < seekTarget) {
      // Landed before seek target.
      NS_ASSERTION(pageOffset > startOffset, "offset_start must increase");
      startOffset = pageOffset;
      startLength = pageLength;
      startTime = granuleTime;
    }
    NS_ASSERTION(startTime < seekTarget, "Must be before seek target");
    NS_ASSERTION(endTime >= seekTarget, "End must be after seek target");
  }

  SEEK_LOG(PR_LOG_DEBUG, ("Seek complete in %d bisections.", hops));

  return NS_OK;
}

nsresult nsOggReader::GetBuffered(nsTimeRanges* aBuffered, PRInt64 aStartTime)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  // HasAudio and HasVideo are not used here as they take a lock and cause
  // a deadlock. Accessing mInfo doesn't require a lock - it doesn't change
  // after metadata is read and GetBuffered isn't called before metadata is
  // read.
  if (!mInfo.mHasVideo && !mInfo.mHasAudio) {
    // No need to search through the file if there are no audio or video tracks
    return NS_OK;
  }

  nsMediaStream* stream = mDecoder->GetCurrentStream();

  // Traverse across the buffered byte ranges, determining the time ranges
  // they contain. nsMediaStream::GetNextCachedData(offset) returns -1 when
  // offset is after the end of the media stream, or there's no more cached
  // data after the offset. This loop will run until we've checked every
  // buffered range in the media, in increasing order of offset.
  ogg_sync_state state;
  ogg_sync_init(&state);
  PRInt64 startOffset = stream->GetNextCachedData(mDataOffset);
  while (startOffset >= 0) {
    PRInt64 endOffset = stream->GetCachedDataEnd(startOffset);
    NS_ASSERTION(startOffset < endOffset, "Buffered range must end after its start");
    // Bytes [startOffset..endOffset] are cached.

    // Find the start time of the range.
    PRInt64 startTime = -1;
    if (startOffset == mDataOffset) {
      // Because the granulepos time is actually the end time of the page,
      // we special-case (startOffset == mDataOffset) so that the first
      // buffered range always appears to be buffered from [t=0...] rather
      // than from the end-time of the first page.
      startTime = aStartTime;
    }
    // Read pages until we find one with a granulepos which we can convert
    // into a timestamp to use as the time of the start of the buffered range.
    ogg_sync_reset(&state);
    while (startTime == -1) {
      ogg_page page;
      PRInt32 discard;
      PageSyncResult res = PageSync(stream,
                                    &state,
                                    PR_TRUE,
                                    startOffset,
                                    endOffset,
                                    &page,
                                    discard);
      if (res == PAGE_SYNC_ERROR) {
        // If we don't clear the sync state before exit we'll leak.
        ogg_sync_clear(&state);
        return NS_ERROR_FAILURE;
      } else if (res == PAGE_SYNC_END_OF_RANGE) {
        // Hit the end of range without reading a page, give up trying to
        // find a start time for this buffered range, skip onto the next one.
        break;
      }

      PRInt64 granulepos = ogg_page_granulepos(&page);
      if (granulepos == -1) {
        // Page doesn't have an end time, advance to the next page
        // until we find one.
        startOffset += page.header_len + page.body_len;
        continue;
      }

      PRUint32 serial = ogg_page_serialno(&page);
      nsOggCodecState* codecState = nsnull;
      mCodecStates.Get(serial, &codecState);
      if (codecState && codecState->mActive) {
        startTime = codecState->Time(granulepos) - aStartTime;
        NS_ASSERTION(startTime > 0, "Must have positive start time");
      }
      else if(codecState) {
        // Page is for an inactive stream, skip it.
        startOffset += page.header_len + page.body_len;
        continue;
      }
      else {
        // Page is for a stream we don't know about (possibly a chained
        // ogg), return an error.
        return PAGE_SYNC_ERROR;
      }
    }

    if (startTime != -1) {
      // We were able to find a start time for that range, see if we can
      // find an end time.
      PRInt64 endTime = FindEndTime(endOffset, PR_TRUE, &state);
      if (endTime != -1) {
        endTime -= aStartTime;
        aBuffered->Add(static_cast<float>(startTime) / 1000.0f,
                       static_cast<float>(endTime) / 1000.0f);
      }
    }
    startOffset = stream->GetNextCachedData(endOffset);
    NS_ASSERTION(startOffset == -1 || startOffset > endOffset,
      "Must have advanced to start of next range, or hit end of stream");
  }

  // If we don't clear the sync state before exit we'll leak.
  ogg_sync_clear(&state);

  return NS_OK;
}

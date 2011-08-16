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
#include "mozilla/TimeStamp.h"

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
#define SEEK_DECODE_MARGIN 2000000

// The number of microseconds of "fuzz" we use in a bisection search over
// HTTP. When we're seeking with fuzz, we'll stop the search if a bisection
// lands between the seek target and SEEK_FUZZ_USECS microseconds before the
// seek target.  This is becaue it's usually quicker to just keep downloading
// from an exisiting connection than to do another bisection inside that
// small range, which would open a new HTTP connetion.
#define SEEK_FUZZ_USECS 500000

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

class nsAutoReleasePacket {
public:
  nsAutoReleasePacket(ogg_packet* aPacket) : mPacket(aPacket) { }
  ~nsAutoReleasePacket() {
    nsOggCodecState::ReleasePacket(mPacket);
  }
private:
  ogg_packet* mPacket;
};

nsOggReader::nsOggReader(nsBuiltinDecoder* aDecoder)
  : nsBuiltinDecoderReader(aDecoder),
    mTheoraState(nsnull),
    mVorbisState(nsnull),
    mSkeletonState(nsnull),
    mVorbisSerial(0),
    mTheoraSerial(0),
    mPageOffset(0)
{
  MOZ_COUNT_CTOR(nsOggReader);
  memset(&mTheoraInfo, 0, sizeof(mTheoraInfo));
}

nsOggReader::~nsOggReader()
{
  ogg_sync_clear(&mOggState);
  MOZ_COUNT_DTOR(nsOggReader);
}

nsresult nsOggReader::Init(nsBuiltinDecoderReader* aCloneDonor) {
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
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  nsresult res = NS_OK;

  if (NS_FAILED(nsBuiltinDecoderReader::ResetDecode())) {
    res = NS_ERROR_FAILURE;
  }

  // Discard any previously buffered packets/pages.
  ogg_sync_reset(&mOggState);
  if (mVorbisState && NS_FAILED(mVorbisState->Reset())) {
    res = NS_ERROR_FAILURE;
  }
  if (mTheoraState && NS_FAILED(mTheoraState->Reset())) {
    res = NS_ERROR_FAILURE;
  }

  return res;
}

PRBool nsOggReader::ReadHeaders(nsOggCodecState* aState)
{
  while (!aState->DoneReadingHeaders()) {
    ogg_packet* packet = NextOggPacket(aState);
    nsAutoReleasePacket autoRelease(packet);
    if (!packet || !aState->IsHeader(packet)) {
      aState->Deactivate();
    } else {
      aState->DecodeHeader(packet);
    }
  }
  return aState->Init();
}

nsresult nsOggReader::ReadMetadata(nsVideoInfo* aInfo)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  // We read packets until all bitstreams have read all their header packets.
  // We record the offset of the first non-header page so that we know
  // what page to seek to when seeking to the media start.

  ogg_page page;
  nsAutoTArray<nsOggCodecState*,4> bitstreams;
  PRBool readAllBOS = PR_FALSE;
  while (!readAllBOS) {
    PRInt64 pageOffset = ReadOggPage(&page);
    if (pageOffset == -1) {
      // Some kind of error...
      break;
    }

    int serial = ogg_page_serialno(&page);
    nsOggCodecState* codecState = 0;

    if (!ogg_page_bos(&page)) {
      // We've encountered a non Beginning Of Stream page. No more BOS pages
      // can follow in this Ogg segment, so there will be no other bitstreams
      // in the Ogg (unless it's invalid).
      readAllBOS = PR_TRUE;
    } else if (!mCodecStates.Get(serial, nsnull)) {
      // We've not encountered a stream with this serial number before. Create
      // an nsOggCodecState to demux it, and map that to the nsOggCodecState
      // in mCodecStates.
      codecState = nsOggCodecState::Create(&page);
      DebugOnly<PRBool> r = mCodecStates.Put(serial, codecState);
      NS_ASSERTION(r, "Failed to insert into mCodecStates");
      bitstreams.AppendElement(codecState);
      mKnownStreams.AppendElement(serial);
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
    }

    mCodecStates.Get(serial, &codecState);
    NS_ENSURE_TRUE(codecState, NS_ERROR_FAILURE);

    if (NS_FAILED(codecState->PageIn(&page))) {
      return NS_ERROR_FAILURE;
    }
  }

  // We've read all BOS pages, so we know the streams contained in the media.
  // Now process all available header packets in the active Theora, Vorbis and
  // Skeleton streams.

  // Deactivate any non-primary bitstreams.
  for (PRUint32 i = 0; i < bitstreams.Length(); i++) {
    nsOggCodecState* s = bitstreams[i];
    if (s != mVorbisState && s != mTheoraState && s != mSkeletonState) {
      s->Deactivate();
    }
  }

  if (mTheoraState && ReadHeaders(mTheoraState)) {
    nsIntRect picture = nsIntRect(mTheoraState->mInfo.pic_x,
                                  mTheoraState->mInfo.pic_y,
                                  mTheoraState->mInfo.pic_width,
                                  mTheoraState->mInfo.pic_height);

    nsIntSize displaySize = nsIntSize(mTheoraState->mInfo.pic_width,
                                      mTheoraState->mInfo.pic_height);

    // Apply the aspect ratio to produce the intrinsic display size we report
    // to the element.
    ScaleDisplayByAspectRatio(displaySize, mTheoraState->mPixelAspectRatio);

    nsIntSize frameSize(mTheoraState->mInfo.frame_width,
                        mTheoraState->mInfo.frame_height);
    if (nsVideoInfo::ValidateVideoRegion(frameSize, picture, displaySize)) {
      // Video track's frame sizes will not overflow. Activate the video track.
      mInfo.mHasVideo = PR_TRUE;
      mInfo.mDisplay = displaySize;
      mPicture = picture;

      mDecoder->SetVideoData(gfxIntSize(displaySize.width, displaySize.height),
                             nsnull,
                             TimeStamp::Now());

      // Copy Theora info data for time computations on other threads.
      memcpy(&mTheoraInfo, &mTheoraState->mInfo, sizeof(mTheoraInfo));
      mTheoraSerial = mTheoraState->mSerial;
    }
  }

  if (mVorbisState && ReadHeaders(mVorbisState)) {
    mInfo.mHasAudio = PR_TRUE;
    mInfo.mAudioRate = mVorbisState->mInfo.rate;
    mInfo.mAudioChannels = mVorbisState->mInfo.channels;
    // Copy Vorbis info data for time computations on other threads.
    memcpy(&mVorbisInfo, &mVorbisState->mInfo, sizeof(mVorbisInfo));
    mVorbisInfo.codec_setup = NULL;
    mVorbisSerial = mVorbisState->mSerial;
  } else {
    memset(&mVorbisInfo, 0, sizeof(mVorbisInfo));
  }

  if (mSkeletonState) {
    if (!HasAudio() && !HasVideo()) {
      // We have a skeleton track, but no audio or video, may as well disable
      // the skeleton, we can't do anything useful with this media.
      mSkeletonState->Deactivate();
    } else if (ReadHeaders(mSkeletonState) && mSkeletonState->HasIndex()) {
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
        ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
        mDecoder->GetStateMachine()->SetDuration(duration);
        LOG(PR_LOG_DEBUG, ("Got duration from Skeleton index %lld", duration));
      }
    }
  }

  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

    nsMediaStream* stream = mDecoder->GetCurrentStream();
    if (mDecoder->GetStateMachine()->GetDuration() == -1 &&
        mDecoder->GetStateMachine()->GetState() != nsDecoderStateMachine::DECODER_STATE_SHUTDOWN &&
        stream->GetLength() >= 0 &&
        mDecoder->GetStateMachine()->IsSeekable())
    {
      // We didn't get a duration from the index or a Content-Duration header.
      // Seek to the end of file to find the end time.
      PRInt64 length = stream->GetLength();
      NS_ASSERTION(length > 0, "Must have a content length to get end time");

      PRInt64 endTime = 0;
      {
        ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
        endTime = RangeEndTime(length);
      }
      if (endTime != -1) {
        mDecoder->GetStateMachine()->SetEndTime(endTime);
        LOG(PR_LOG_DEBUG, ("Got Ogg duration from seeking to end %lld", endTime));
      }
    }
  }
  *aInfo = mInfo;

  return NS_OK;
}

nsresult nsOggReader::DecodeVorbis(ogg_packet* aPacket) {
  NS_ASSERTION(aPacket->granulepos != -1, "Must know vorbis granulepos!");

  if (vorbis_synthesis(&mVorbisState->mBlock, aPacket) != 0) {
    return NS_ERROR_FAILURE;
  }
  if (vorbis_synthesis_blockin(&mVorbisState->mDsp,
                               &mVorbisState->mBlock) != 0)
  {
    return NS_ERROR_FAILURE;
  }

  VorbisPCMValue** pcm = 0;
  PRInt32 samples = 0;
  PRUint32 channels = mVorbisState->mInfo.channels;
  ogg_int64_t endSample = aPacket->granulepos;
  while ((samples = vorbis_synthesis_pcmout(&mVorbisState->mDsp, &pcm)) > 0) {
    mVorbisState->ValidateVorbisPacketSamples(aPacket, samples);
    nsAutoArrayPtr<AudioDataValue> buffer(new AudioDataValue[samples * channels]);
    for (PRUint32 j = 0; j < channels; ++j) {
      VorbisPCMValue* channel = pcm[j];
      for (PRUint32 i = 0; i < PRUint32(samples); ++i) {
        buffer[i*channels + j] = MOZ_CONVERT_VORBIS_SAMPLE(channel[i]);
      }
    }

    PRInt64 duration = mVorbisState->Time((PRInt64)samples);
    PRInt64 startTime = mVorbisState->Time(endSample - samples);
    mAudioQueue.Push(new AudioData(mPageOffset,
                                   startTime,
                                   duration,
                                   samples,
                                   buffer.forget(),
                                   channels));
    endSample -= samples;
    if (vorbis_synthesis_read(&mVorbisState->mDsp, samples) != 0) {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

PRBool nsOggReader::DecodeAudioData()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  NS_ASSERTION(mVorbisState!=0, "Need Vorbis state to decode audio");

  // Read the next data packet. Skip any non-data packets we encounter.
  ogg_packet* packet = 0;
  do {
    if (packet) {
      nsOggCodecState::ReleasePacket(packet);
    }
    packet = NextOggPacket(mVorbisState);
  } while (packet && mVorbisState->IsHeader(packet));
  if (!packet) {
    mAudioQueue.Finish();
    return PR_FALSE;
  }

  NS_ASSERTION(packet && packet->granulepos != -1,
    "Must have packet with known granulepos");
  nsAutoReleasePacket autoRelease(packet);
  DecodeVorbis(packet);
  if (packet->e_o_s) {
    // We've encountered an end of bitstream packet, or we've hit the end of
    // file while trying to decode, so inform the audio queue that there'll
    // be no more samples.
    mAudioQueue.Finish();
    return PR_FALSE;
  }

  return PR_TRUE;
}

nsresult nsOggReader::DecodeTheora(ogg_packet* aPacket, PRInt64 aTimeThreshold)
{
  NS_ASSERTION(aPacket->granulepos >= TheoraVersion(&mTheoraState->mInfo,3,2,1),
    "Packets must have valid granulepos and packetno");

  int ret = th_decode_packetin(mTheoraState->mCtx, aPacket, 0);
  if (ret != 0 && ret != TH_DUPFRAME) {
    return NS_ERROR_FAILURE;
  }
  PRInt64 time = mTheoraState->StartTime(aPacket->granulepos);

  // Don't use the frame if it's outside the bounds of the presentation
  // start time in the skeleton track. Note we still must submit the frame
  // to the decoder (via th_decode_packetin), as the frames which are
  // presentable may depend on this frame's data.
  if (mSkeletonState && !mSkeletonState->IsPresentable(time)) {
    return NS_OK;
  }

  PRInt64 endTime = mTheoraState->Time(aPacket->granulepos);
  if (endTime < aTimeThreshold) {
    // The end time of this frame is already before the current playback
    // position. It will never be displayed, don't bother enqueing it.
    return NS_OK;
  }

  if (ret == TH_DUPFRAME) {
    VideoData* v = VideoData::CreateDuplicate(mPageOffset,
                                              time,
                                              endTime,
                                              aPacket->granulepos);
    mVideoQueue.Push(v);
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
                                     aPacket->granulepos,
                                     mPicture);
    if (!v) {
      // There may be other reasons for this error, but for
      // simplicity just assume the worst case: out of memory.
      NS_WARNING("Failed to allocate memory for video frame");
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mVideoQueue.Push(v);
  }
  return NS_OK;
}

PRBool nsOggReader::DecodeVideoFrame(PRBool &aKeyframeSkip,
                                     PRInt64 aTimeThreshold)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  // Record number of frames decoded and parsed. Automatically update the
  // stats counters using the AutoNotifyDecoded stack-based class.
  PRUint32 parsed = 0, decoded = 0;
  nsMediaDecoder::AutoNotifyDecoded autoNotify(mDecoder, parsed, decoded);

  // Read the next data packet. Skip any non-data packets we encounter.
  ogg_packet* packet = 0;
  do {
    if (packet) {
      nsOggCodecState::ReleasePacket(packet);
    }
    packet = NextOggPacket(mTheoraState);
  } while (packet && mTheoraState->IsHeader(packet));
  if (!packet) {
    mVideoQueue.Finish();
    return PR_FALSE;
  }
  nsAutoReleasePacket autoRelease(packet);

  parsed++;
  NS_ASSERTION(packet && packet->granulepos != -1,
                "Must know first packet's granulepos");
  PRBool eos = packet->e_o_s;
  PRInt64 frameEndTime = mTheoraState->Time(packet->granulepos);
  if (!aKeyframeSkip ||
     (th_packet_iskeyframe(packet) && frameEndTime >= aTimeThreshold))
  {
    aKeyframeSkip = PR_FALSE;
    nsresult res = DecodeTheora(packet, aTimeThreshold);
    decoded++;
    if (NS_FAILED(res)) {
      return PR_FALSE;
    }
  }

  if (eos) {
    // We've encountered an end of bitstream packet. Inform the queue that
    // there will be no more frames.
    mVideoQueue.Finish();
    return PR_FALSE;
  }

  return PR_TRUE;
}

PRInt64 nsOggReader::ReadOggPage(ogg_page* aPage)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

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

ogg_packet* nsOggReader::NextOggPacket(nsOggCodecState* aCodecState)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  if (!aCodecState || !aCodecState->mActive) {
    return nsnull;
  }

  ogg_packet* packet;
  while ((packet = aCodecState->PacketOut()) == nsnull) {
    // The codec state does not have any buffered pages, so try to read another
    // page from the channel.
    ogg_page page;
    if (ReadOggPage(&page) == -1) {
      return nsnull;
    }

    PRUint32 serial = ogg_page_serialno(&page);
    nsOggCodecState* codecState = nsnull;
    mCodecStates.Get(serial, &codecState);
    if (codecState && NS_FAILED(codecState->PageIn(&page))) {
      return nsnull;
    }
  }

  return packet;
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

PRInt64 nsOggReader::RangeStartTime(PRInt64 aOffset)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  nsMediaStream* stream = mDecoder->GetCurrentStream();
  NS_ENSURE_TRUE(stream != nsnull, nsnull);
  nsresult res = stream->Seek(nsISeekableStream::NS_SEEK_SET, aOffset);
  NS_ENSURE_SUCCESS(res, nsnull);
  PRInt64 startTime = 0;
  nsBuiltinDecoderReader::FindStartTime(startTime);
  return startTime;
}

struct nsAutoOggSyncState {
  nsAutoOggSyncState() {
    ogg_sync_init(&mState);
  }
  ~nsAutoOggSyncState() {
    ogg_sync_clear(&mState);
  }
  ogg_sync_state mState;
};

PRInt64 nsOggReader::RangeEndTime(PRInt64 aEndOffset)
{
  NS_ASSERTION(mDecoder->OnStateMachineThread() || mDecoder->OnDecodeThread(),
               "Should be on state machine or decode thread.");

  nsMediaStream* stream = mDecoder->GetCurrentStream();
  NS_ENSURE_TRUE(stream != nsnull, -1);
  PRInt64 position = stream->Tell();
  PRInt64 endTime = RangeEndTime(0, aEndOffset, PR_FALSE);
  nsresult res = stream->Seek(nsISeekableStream::NS_SEEK_SET, position);
  NS_ENSURE_SUCCESS(res, -1);
  return endTime;
}

PRInt64 nsOggReader::RangeEndTime(PRInt64 aStartOffset,
                                  PRInt64 aEndOffset,
                                  PRBool aCachedDataOnly)
{
  nsMediaStream* stream = mDecoder->GetCurrentStream();
  nsAutoOggSyncState sync;

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
    int ret = ogg_sync_pageseek(&sync.mState, &page);
    if (ret == 0) {
      // We need more data if we've not encountered a page we've seen before,
      // or we've read to the end of file.
      if (mustBackOff || readHead == aEndOffset || readHead == aStartOffset) {
        if (endTime != -1 || readStartOffset == 0) {
          // We have encountered a page before, or we're at the end of file.
          break;
        }
        mustBackOff = PR_FALSE;
        prevChecksumAfterSeek = checksumAfterSeek;
        checksumAfterSeek = 0;
        ogg_sync_reset(&sync.mState);
        readStartOffset = NS_MAX(static_cast<PRInt64>(0), readStartOffset - step);
        readHead = NS_MAX(aStartOffset, readStartOffset);
      }

      PRInt64 limit = NS_MIN(static_cast<PRInt64>(PR_UINT32_MAX),
                             aEndOffset - readHead);
      limit = NS_MAX(static_cast<PRInt64>(0), limit);
      limit = NS_MIN(limit, static_cast<PRInt64>(step));
      PRUint32 bytesToRead = static_cast<PRUint32>(limit);
      PRUint32 bytesRead = 0;
      char* buffer = ogg_sync_buffer(&sync.mState, bytesToRead);
      NS_ASSERTION(buffer, "Must have buffer");
      nsresult res;
      if (aCachedDataOnly) {
        res = stream->ReadFromCache(buffer, readHead, bytesToRead);
        NS_ENSURE_SUCCESS(res, -1);
        bytesRead = bytesToRead;
      } else {
        NS_ASSERTION(readHead < aEndOffset,
                     "Stream pos must be before range end");
        res = stream->Seek(nsISeekableStream::NS_SEEK_SET, readHead);
        NS_ENSURE_SUCCESS(res, -1);
        res = stream->Read(buffer, bytesToRead, &bytesRead);
        NS_ENSURE_SUCCESS(res, -1);
      }
      readHead += bytesRead;

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

  return endTime;
}

nsresult nsOggReader::GetSeekRanges(nsTArray<SeekRange>& aRanges)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  nsTArray<nsByteRange> cached;
  nsresult res = mDecoder->GetCurrentStream()->GetCachedRanges(cached);
  NS_ENSURE_SUCCESS(res, res);

  for (PRUint32 index = 0; index < cached.Length(); index++) {
    nsByteRange& range = cached[index];
    PRInt64 startTime = -1;
    PRInt64 endTime = -1;
    if (NS_FAILED(ResetDecode())) {
      return NS_ERROR_FAILURE;
    }
    PRInt64 startOffset = range.mStart;
    PRInt64 endOffset = range.mEnd;
    startTime = RangeStartTime(startOffset);
    if (startTime != -1 &&
        ((endTime = RangeEndTime(endOffset)) != -1))
    {
      NS_ASSERTION(startTime < endTime,
                   "Start time must be before end time");
      aRanges.AppendElement(SeekRange(startOffset,
                                      endOffset,
                                      startTime,
                                      endTime));
     }
  }
  if (NS_FAILED(ResetDecode())) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsOggReader::SeekRange
nsOggReader::SelectSeekRange(const nsTArray<SeekRange>& ranges,
                             PRInt64 aTarget,
                             PRInt64 aStartTime,
                             PRInt64 aEndTime,
                             PRBool aExact)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  PRInt64 so = 0;
  PRInt64 eo = mDecoder->GetCurrentStream()->GetLength();
  PRInt64 st = aStartTime;
  PRInt64 et = aEndTime;
  for (PRUint32 i = 0; i < ranges.Length(); i++) {
    const SeekRange &r = ranges[i];
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
                                          const nsTArray<SeekRange>& aRanges,
                                          const SeekRange& aRange)
{
  LOG(PR_LOG_DEBUG, ("%p Seeking in buffered data to %lld using bisection search", mDecoder, aTarget));

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
      ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
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
    SeekRange k = SelectSeekRange(aRanges,
                                  keyframeTime,
                                  aStartTime,
                                  aEndTime,
                                  PR_FALSE);
    res = SeekBisection(keyframeTime, k, SEEK_FUZZ_USECS);
  }
  return res;
}

nsresult nsOggReader::SeekInUnbuffered(PRInt64 aTarget,
                                       PRInt64 aStartTime,
                                       PRInt64 aEndTime,
                                       const nsTArray<SeekRange>& aRanges)
{
  LOG(PR_LOG_DEBUG, ("%p Seeking in unbuffered data to %lld using bisection search", mDecoder, aTarget));
  
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
  PRInt64 keyframeOffsetMs = 0;
  if (HasVideo() && mTheoraState) {
    keyframeOffsetMs = mTheoraState->MaxKeyframeOffset();
  }
  PRInt64 seekTarget = NS_MAX(aStartTime, aTarget - keyframeOffsetMs);
  // Minimize the bisection search space using the known timestamps from the
  // buffered ranges.
  SeekRange k = SelectSeekRange(aRanges, seekTarget, aStartTime, aEndTime, PR_FALSE);
  return SeekBisection(seekTarget, k, SEEK_FUZZ_USECS);
}

nsresult nsOggReader::Seek(PRInt64 aTarget,
                           PRInt64 aStartTime,
                           PRInt64 aEndTime,
                           PRInt64 aCurrentTime)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  LOG(PR_LOG_DEBUG, ("%p About to seek to %lld", mDecoder, aTarget));
  nsresult res;
  nsMediaStream* stream = mDecoder->GetCurrentStream();
  NS_ENSURE_TRUE(stream != nsnull, NS_ERROR_FAILURE);

  if (aTarget == aStartTime) {
    // We've seeked to the media start. Just seek to the offset of the first
    // content page.
    res = stream->Seek(nsISeekableStream::NS_SEEK_SET, 0);
    NS_ENSURE_SUCCESS(res,res);

    mPageOffset = 0;
    res = ResetDecode();
    NS_ENSURE_SUCCESS(res,res);

    NS_ASSERTION(aStartTime != -1, "mStartTime should be known");
    {
      ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
      mDecoder->UpdatePlaybackPosition(aStartTime);
    }
  } else {
    IndexedSeekResult sres = SeekToKeyframeUsingIndex(aTarget);
    NS_ENSURE_TRUE(sres != SEEK_FATAL_ERROR, NS_ERROR_FAILURE);
    if (sres == SEEK_INDEX_FAIL) {
      // No index or other non-fatal index-related failure. Try to seek
      // using a bisection search. Determine the already downloaded data
      // in the media cache, so we can try to seek in the cached data first.
      nsAutoTArray<SeekRange, 16> ranges;
      res = GetSeekRanges(ranges);
      NS_ENSURE_SUCCESS(res,res);

      // Figure out if the seek target lies in a buffered range.
      SeekRange r = SelectSeekRange(ranges, aTarget, aStartTime, aEndTime, PR_TRUE);

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
      NS_ASSERTION(bytesToRead <= PR_UINT32_MAX, "bytesToRead range check");
      if (bytesToRead <= 0) {
        return PAGE_SYNC_END_OF_RANGE;
      }
      nsresult rv = NS_OK;
      if (aCachedDataOnly) {
        rv = aStream->ReadFromCache(buffer, readHead,
                                    static_cast<PRUint32>(bytesToRead));
        NS_ENSURE_SUCCESS(rv,PAGE_SYNC_ERROR);
        bytesRead = static_cast<PRUint32>(bytesToRead);
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
                                    const SeekRange& aRange,
                                    PRUint32 aFuzz)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  nsresult res;
  nsMediaStream* stream = mDecoder->GetCurrentStream();

  if (aTarget == aRange.mTimeStart) {
    if (NS_FAILED(ResetDecode())) {
      return NS_ERROR_FAILURE;
    }
    res = stream->Seek(nsISeekableStream::NS_SEEK_SET, 0);
    NS_ENSURE_SUCCESS(res,res);
    mPageOffset = 0;
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
  PRInt64 seekLowerBound = NS_MAX(static_cast<PRInt64>(0), aTarget - aFuzz);
  int hops = 0;
  ogg_int64_t previousGuess = -1;
  int backsteps = 0;
  const int maxBackStep = 10;
  NS_ASSERTION(static_cast<PRUint64>(PAGE_STEP) * pow(2.0, maxBackStep) < PR_INT32_MAX,
               "Backstep calculation must not overflow");

  // Seek via bisection search. Loop until we find the offset where the page
  // before the offset is before the seek target, and the page after the offset
  // is after the seek target.
  while (PR_TRUE) {
    ogg_int64_t duration = 0;
    double target = 0;
    ogg_int64_t interval = 0;
    ogg_int64_t guess = 0;
    ogg_page page;
    int skippedBytes = 0;
    ogg_int64_t pageOffset = 0;
    ogg_int64_t pageLength = 0;
    ogg_int64_t granuleTime = -1;
    PRBool mustBackoff = PR_FALSE;

    // Guess where we should bisect to, based on the bit rate and the time
    // remaining in the interval. Loop until we can determine the time at
    // the guess offset.
    while (PR_TRUE) {
  
      // Discard any previously buffered packets/pages.
      if (NS_FAILED(ResetDecode())) {
        return NS_ERROR_FAILURE;
      }

      interval = endOffset - startOffset - startLength;
      if (interval == 0) {
        // Our interval is empty, we've found the optimal seek point, as the
        // page at the start offset is before the seek target, and the page
        // at the end offset is after the seek target.
        SEEK_LOG(PR_LOG_DEBUG, ("Interval narrowed, terminating bisection."));
        break;
      }

      // Guess bisection point.
      duration = endTime - startTime;
      target = (double)(seekTarget - startTime) / (double)duration;
      guess = startOffset + startLength +
              static_cast<ogg_int64_t>((double)interval * target);
      guess = NS_MIN(guess, endOffset - PAGE_STEP);
      if (mustBackoff) {
        // We previously failed to determine the time at the guess offset,
        // probably because we ran out of data to decode. This usually happens
        // when we guess very close to the end offset. So reduce the guess
        // offset using an exponential backoff until we determine the time.
        SEEK_LOG(PR_LOG_DEBUG, ("Backing off %d bytes, backsteps=%d",
          static_cast<PRInt32>(PAGE_STEP * pow(2.0, backsteps)), backsteps));
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

        backsteps = NS_MIN(backsteps + 1, maxBackStep);
        // We reset mustBackoff. If we still need to backoff further, it will
        // be set to PR_TRUE again.
        mustBackoff = PR_FALSE;
      } else {
        backsteps = 0;
      }
      guess = NS_MAX(guess, startOffset + startLength);

      SEEK_LOG(PR_LOG_DEBUG, ("Seek loop start[o=%lld..%lld t=%lld] "
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

      if (res == PAGE_SYNC_END_OF_RANGE) {
        // Our guess was too close to the end, we've ended up reading the end
        // page. Backoff exponentially from the end point, in case the last
        // page/frame/sample is huge.
        mustBackoff = PR_TRUE;
        SEEK_LOG(PR_LOG_DEBUG, ("Hit the end of range, backing off"));
        continue;
      }

      // Read pages until we can determine the granule time of the audio and 
      // video bitstream.
      ogg_int64_t audioTime = -1;
      ogg_int64_t videoTime = -1;
      do {
        // Add the page to its codec state, determine its granule time.
        PRUint32 serial = ogg_page_serialno(&page);
        nsOggCodecState* codecState = nsnull;
        mCodecStates.Get(serial, &codecState);
        if (codecState && codecState->mActive) {
          int ret = ogg_stream_pagein(&codecState->mState, &page);
          NS_ENSURE_TRUE(ret == 0, NS_ERROR_FAILURE);
        }

        ogg_int64_t granulepos = ogg_page_granulepos(&page);

        if (HasAudio() &&
            granulepos > 0 &&
            serial == mVorbisState->mSerial &&
            audioTime == -1) {
          audioTime = mVorbisState->Time(granulepos);
        }
        
        if (HasVideo() &&
            granulepos > 0 &&
            serial == mTheoraState->mSerial &&
            videoTime == -1) {
          videoTime = mTheoraState->StartTime(granulepos);
        }

        if (mPageOffset == endOffset) {
          // Hit end of readable data.
          break;
        }

        if (ReadOggPage(&page) == -1) {
          break;
        }
        
      } while ((mVorbisState && audioTime == -1) ||
               (mTheoraState && videoTime == -1));

      NS_ASSERTION(mPageOffset <= endOffset, "Page read cursor should be inside range");

      if ((HasAudio() && audioTime == -1) ||
          (HasVideo() && videoTime == -1)) 
      {
        // We don't have timestamps for all active tracks...
        if (pageOffset == startOffset + startLength && mPageOffset == endOffset) {
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
        mustBackoff = PR_TRUE;
        continue;
      }

      // We've found appropriate time stamps here. Proceed to bisect
      // the search space.
      granuleTime = NS_MAX(audioTime, videoTime);
      NS_ASSERTION(granuleTime > 0, "Must get a granuletime");
      break;
    } // End of "until we determine time at guess offset" loop.

    if (interval == 0) {
      // Seek termination condition; we've found the page boundary of the
      // last page before the target, and the first page after the target.
      SEEK_LOG(PR_LOG_DEBUG, ("Terminating seek at offset=%lld", startOffset));
      NS_ASSERTION(startTime < aTarget, "Start time must always be less than target");
      res = stream->Seek(nsISeekableStream::NS_SEEK_SET, startOffset);
      NS_ENSURE_SUCCESS(res,res);
      mPageOffset = startOffset;
      if (NS_FAILED(ResetDecode())) {
        return NS_ERROR_FAILURE;
      }
      break;
    }

    SEEK_LOG(PR_LOG_DEBUG, ("Time at offset %lld is %lld", guess, granuleTime));
    if (granuleTime < seekTarget && granuleTime > seekLowerBound) {
      // We're within the fuzzy region in which we want to terminate the search.
      res = stream->Seek(nsISeekableStream::NS_SEEK_SET, pageOffset);
      NS_ENSURE_SUCCESS(res,res);
      mPageOffset = pageOffset;
      if (NS_FAILED(ResetDecode())) {
        return NS_ERROR_FAILURE;
      }
      SEEK_LOG(PR_LOG_DEBUG, ("Terminating seek at offset=%lld", mPageOffset));
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
    NS_ASSERTION(startTime < seekTarget, "Must be before seek target");
    NS_ASSERTION(endTime >= seekTarget, "End must be after seek target");
  }

  SEEK_LOG(PR_LOG_DEBUG, ("Seek complete in %d bisections.", hops));

  return NS_OK;
}

nsresult nsOggReader::GetBuffered(nsTimeRanges* aBuffered, PRInt64 aStartTime)
{
  // HasAudio and HasVideo are not used here as they take a lock and cause
  // a deadlock. Accessing mInfo doesn't require a lock - it doesn't change
  // after metadata is read and GetBuffered isn't called before metadata is
  // read.
  if (!mInfo.mHasVideo && !mInfo.mHasAudio) {
    // No need to search through the file if there are no audio or video tracks
    return NS_OK;
  }

  nsMediaStream* stream = mDecoder->GetCurrentStream();
  nsTArray<nsByteRange> ranges;
  nsresult res = stream->GetCachedRanges(ranges);
  NS_ENSURE_SUCCESS(res, res);

  // Traverse across the buffered byte ranges, determining the time ranges
  // they contain. nsMediaStream::GetNextCachedData(offset) returns -1 when
  // offset is after the end of the media stream, or there's no more cached
  // data after the offset. This loop will run until we've checked every
  // buffered range in the media, in increasing order of offset.
  nsAutoOggSyncState sync;
  for (PRUint32 index = 0; index < ranges.Length(); index++) {
    // Ensure the offsets are after the header pages.
    PRInt64 startOffset = ranges[index].mStart;
    PRInt64 endOffset = ranges[index].mEnd;

    // Because the granulepos time is actually the end time of the page,
    // we special-case (startOffset == 0) so that the first
    // buffered range always appears to be buffered from the media start
    // time, rather than from the end-time of the first page.
    PRInt64 startTime = (startOffset == 0) ? aStartTime : -1;

    // Find the start time of the range. Read pages until we find one with a
    // granulepos which we can convert into a timestamp to use as the time of
    // the start of the buffered range.
    ogg_sync_reset(&sync.mState);
    while (startTime == -1) {
      ogg_page page;
      PRInt32 discard;
      PageSyncResult res = PageSync(stream,
                                    &sync.mState,
                                    PR_TRUE,
                                    startOffset,
                                    endOffset,
                                    &page,
                                    discard);
      if (res == PAGE_SYNC_ERROR) {
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
      if (mVorbisState && serial == mVorbisSerial) {
        startTime = nsVorbisState::Time(&mVorbisInfo, granulepos);
        NS_ASSERTION(startTime > 0, "Must have positive start time");
      }
      else if (mTheoraState && serial == mTheoraSerial) {
        startTime = nsTheoraState::Time(&mTheoraInfo, granulepos);
        NS_ASSERTION(startTime > 0, "Must have positive start time");
      }
      else if (IsKnownStream(serial)) {
        // Stream is not the theora or vorbis stream we're playing,
        // but is one that we have header data for.
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
      PRInt64 endTime = RangeEndTime(startOffset, endOffset, PR_TRUE);
      if (endTime != -1) {
        aBuffered->Add((startTime - aStartTime) / static_cast<double>(USECS_PER_S),
                       (endTime - aStartTime) / static_cast<double>(USECS_PER_S));
      }
    }
  }

  return NS_OK;
}

PRBool nsOggReader::IsKnownStream(PRUint32 aSerial)
{
  for (PRUint32 i = 0; i < mKnownStreams.Length(); i++) {
    PRUint32 serial = mKnownStreams[i];
    if (serial == aSerial) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

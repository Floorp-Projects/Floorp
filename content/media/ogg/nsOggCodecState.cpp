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
 * Portions created by the Initial Developer are Copyright (C) 2010
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
#include "nsDebug.h"
#include "nsOggCodecState.h"
#include "nsOggDecoder.h"
#include <string.h>
#include "nsTraceRefcnt.h"
#include "VideoUtils.h"
#include "nsBuiltinDecoderReader.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gBuiltinDecoderLog;
#define LOG(type, msg) PR_LOG(gBuiltinDecoderLog, type, msg)
#else
#define LOG(type, msg)
#endif

nsOggCodecState*
nsOggCodecState::Create(ogg_page* aPage)
{
  NS_ASSERTION(ogg_page_bos(aPage), "Only call on BOS page!");
  nsAutoPtr<nsOggCodecState> codecState;
  if (aPage->body_len > 6 && memcmp(aPage->body+1, "theora", 6) == 0) {
    codecState = new nsTheoraState(aPage);
  } else if (aPage->body_len > 6 && memcmp(aPage->body+1, "vorbis", 6) == 0) {
    codecState = new nsVorbisState(aPage);
  } else if (aPage->body_len > 8 && memcmp(aPage->body, "fishead\0", 8) == 0) {
    codecState = new nsSkeletonState(aPage);
  } else {
    codecState = new nsOggCodecState(aPage, PR_FALSE);
  }
  return codecState->nsOggCodecState::Init() ? codecState.forget() : nsnull;
}

nsOggCodecState::nsOggCodecState(ogg_page* aBosPage, PRBool aActive) :
  mPacketCount(0),
  mSerial(ogg_page_serialno(aBosPage)),
  mActive(aActive),
  mDoneReadingHeaders(!aActive)
{
  MOZ_COUNT_CTOR(nsOggCodecState);
  memset(&mState, 0, sizeof(ogg_stream_state));
}

nsOggCodecState::~nsOggCodecState() {
  MOZ_COUNT_DTOR(nsOggCodecState);
  Reset();
#ifdef DEBUG
  int ret =
#endif
  ogg_stream_clear(&mState);
  NS_ASSERTION(ret == 0, "ogg_stream_clear failed");
}

nsresult nsOggCodecState::Reset() {
  if (ogg_stream_reset(&mState) != 0) {
    return NS_ERROR_FAILURE;
  }
  mPackets.Erase();
  ClearUnstamped();
  return NS_OK;
}

void nsOggCodecState::ClearUnstamped()
{
  for (PRUint32 i = 0; i < mUnstamped.Length(); ++i) {
    nsOggCodecState::ReleasePacket(mUnstamped[i]);
  }
  mUnstamped.Clear();
}

PRBool nsOggCodecState::Init() {
  int ret = ogg_stream_init(&mState, mSerial);
  return ret == 0;
}

void nsVorbisState::RecordVorbisPacketSamples(ogg_packet* aPacket,
                                              long aSamples)
{
#ifdef VALIDATE_VORBIS_SAMPLE_CALCULATION
  mVorbisPacketSamples[aPacket] = aSamples;
#endif
}

void nsVorbisState::ValidateVorbisPacketSamples(ogg_packet* aPacket,
                                                long aSamples)
{
#ifdef VALIDATE_VORBIS_SAMPLE_CALCULATION
  NS_ASSERTION(mVorbisPacketSamples[aPacket] == aSamples,
    "Decoded samples for Vorbis packet don't match expected!");
  mVorbisPacketSamples.erase(aPacket);
#endif
}

void nsVorbisState::AssertHasRecordedPacketSamples(ogg_packet* aPacket)
{
#ifdef VALIDATE_VORBIS_SAMPLE_CALCULATION
  NS_ASSERTION(mVorbisPacketSamples.count(aPacket) == 1,
    "Must have recorded packet samples");
#endif
}

static ogg_packet* Clone(ogg_packet* aPacket) {
  ogg_packet* p = new ogg_packet();
  memcpy(p, aPacket, sizeof(ogg_packet));
  p->packet = new unsigned char[p->bytes];
  memcpy(p->packet, aPacket->packet, p->bytes);
  return p;
}

void nsOggCodecState::ReleasePacket(ogg_packet* aPacket) {
  if (aPacket)
    delete [] aPacket->packet;
  delete aPacket;
}

void nsPacketQueue::Append(ogg_packet* aPacket) {
  nsDeque::Push(aPacket);
}

ogg_packet* nsOggCodecState::PacketOut() {
  if (mPackets.IsEmpty()) {
    return nsnull;
  }
  return mPackets.PopFront();
}

nsresult nsOggCodecState::PageIn(ogg_page* aPage) {
  if (!mActive)
    return NS_OK;
  NS_ASSERTION(ogg_page_serialno(aPage) == mSerial, "Page must be for this stream!");
  if (ogg_stream_pagein(&mState, aPage) == -1)
    return NS_ERROR_FAILURE;
  int r;
  do {
    ogg_packet packet;
    r = ogg_stream_packetout(&mState, &packet);
    if (r == 1) {
      mPackets.Append(Clone(&packet));
    }
  } while (r != 0);
  if (ogg_stream_check(&mState)) {
    NS_WARNING("Unrecoverable error in ogg_stream_packetout");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult nsOggCodecState::PacketOutUntilGranulepos(PRBool& aFoundGranulepos) {
  int r;
  aFoundGranulepos = PR_FALSE;
  // Extract packets from the sync state until either no more packets
  // come out, or we get a data packet with non -1 granulepos.
  do {
    ogg_packet packet;
    r = ogg_stream_packetout(&mState, &packet);
    if (r == 1) {
      ogg_packet* clone = Clone(&packet);
      if (IsHeader(&packet)) {
        // Header packets go straight into the packet queue.
        mPackets.Append(clone);
      } else {
        // We buffer data packets until we encounter a granulepos. We'll
        // then use the granulepos to figure out the granulepos of the
        // preceeding packets.
        mUnstamped.AppendElement(clone);
        aFoundGranulepos = packet.granulepos > 0;
      }
    }
  } while (r != 0 && !aFoundGranulepos);
  if (ogg_stream_check(&mState)) {
    NS_WARNING("Unrecoverable error in ogg_stream_packetout");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsTheoraState::nsTheoraState(ogg_page* aBosPage) :
  nsOggCodecState(aBosPage, PR_TRUE),
  mSetup(0),
  mCtx(0),
  mPixelAspectRatio(0)
{
  MOZ_COUNT_CTOR(nsTheoraState);
  th_info_init(&mInfo);
  th_comment_init(&mComment);
}

nsTheoraState::~nsTheoraState() {
  MOZ_COUNT_DTOR(nsTheoraState);
  th_setup_free(mSetup);
  th_decode_free(mCtx);
  th_comment_clear(&mComment);
  th_info_clear(&mInfo);
}

PRBool nsTheoraState::Init() {
  if (!mActive)
    return PR_FALSE;

  PRInt64 n = mInfo.aspect_numerator;
  PRInt64 d = mInfo.aspect_denominator;

  mPixelAspectRatio = (n == 0 || d == 0) ?
    1.0f : static_cast<float>(n) / static_cast<float>(d);

  // Ensure the frame and picture regions aren't larger than our prescribed
  // maximum, or zero sized.
  nsIntSize frame(mInfo.frame_width, mInfo.frame_height);
  nsIntRect picture(mInfo.pic_x, mInfo.pic_y, mInfo.pic_width, mInfo.pic_height);
  if (!nsVideoInfo::ValidateVideoRegion(frame, picture, frame)) {
    return mActive = PR_FALSE;
  }

  mCtx = th_decode_alloc(&mInfo, mSetup);
  if (mCtx == NULL) {
    return mActive = PR_FALSE;
  }

  return PR_TRUE;
}

PRBool
nsTheoraState::DecodeHeader(ogg_packet* aPacket)
{
  mPacketCount++;
  int ret = th_decode_headerin(&mInfo,
                               &mComment,
                               &mSetup,
                               aPacket);
 
  // We must determine when we've read the last header packet.
  // th_decode_headerin() does not tell us when it's read the last header, so
  // we must keep track of the headers externally.
  //
  // There are 3 header packets, the Identification, Comment, and Setup
  // headers, which must be in that order. If they're out of order, the file
  // is invalid. If we've successfully read a header, and it's the setup
  // header, then we're done reading headers. The first byte of each packet
  // determines it's type as follows:
  //    0x80 -> Identification header
  //    0x81 -> Comment header
  //    0x82 -> Setup header
  // See http://www.theora.org/doc/Theora.pdf Chapter 6, "Bitstream Headers",
  // for more details of the Ogg/Theora containment scheme.
  PRBool isSetupHeader = aPacket->bytes > 0 && aPacket->packet[0] == 0x82;
  if (ret < 0 || mPacketCount > 3) {
    // We've received an error, or the first three packets weren't valid
    // header packets, assume bad input, and don't activate the bitstream.
    mDoneReadingHeaders = PR_TRUE;
  } else if (ret > 0 && isSetupHeader && mPacketCount == 3) {
    // Successfully read the three header packets.
    mDoneReadingHeaders = PR_TRUE;
    mActive = PR_TRUE;
  }
  return mDoneReadingHeaders;
}

PRInt64
nsTheoraState::Time(PRInt64 granulepos) {
  if (!mActive) {
    return -1;
  }
  return nsTheoraState::Time(&mInfo, granulepos);
}

PRBool
nsTheoraState::IsHeader(ogg_packet* aPacket) {
  return th_packet_isheader(aPacket);
}

# define TH_VERSION_CHECK(_info,_maj,_min,_sub) \
 ((_info)->version_major>(_maj)||(_info)->version_major==(_maj)&& \
 ((_info)->version_minor>(_min)||(_info)->version_minor==(_min)&& \
 (_info)->version_subminor>=(_sub)))

PRInt64 nsTheoraState::Time(th_info* aInfo, PRInt64 aGranulepos)
{
  if (aGranulepos < 0 || aInfo->fps_numerator == 0) {
    return -1;
  }
  PRInt64 t = 0;
  // Implementation of th_granule_frame inlined here to operate
  // on the th_info structure instead of the theora_state.
  int shift = aInfo->keyframe_granule_shift; 
  ogg_int64_t iframe = aGranulepos >> shift;
  ogg_int64_t pframe = aGranulepos - (iframe << shift);
  PRInt64 frameno = iframe + pframe - TH_VERSION_CHECK(aInfo, 3, 2, 1);
  if (!AddOverflow(frameno, 1, t))
    return -1;
  if (!MulOverflow(t, USECS_PER_S, t))
    return -1;
  if (!MulOverflow(t, aInfo->fps_denominator, t))
    return -1;
  return t / aInfo->fps_numerator;
}

PRInt64 nsTheoraState::StartTime(PRInt64 granulepos) {
  if (granulepos < 0 || !mActive || mInfo.fps_numerator == 0) {
    return -1;
  }
  PRInt64 t = 0;
  PRInt64 frameno = th_granule_frame(mCtx, granulepos);
  if (!MulOverflow(frameno, USECS_PER_S, t))
    return -1;
  if (!MulOverflow(t, mInfo.fps_denominator, t))
    return -1;
  return t / mInfo.fps_numerator;
}

PRInt64
nsTheoraState::MaxKeyframeOffset()
{
  // Determine the maximum time in microseconds by which a key frame could
  // offset for the theora bitstream. Theora granulepos encode time as:
  // ((key_frame_number << granule_shift) + frame_offset).
  // Therefore the maximum possible time by which any frame could be offset
  // from a keyframe is the duration of (1 << granule_shift) - 1) frames.
  PRInt64 frameDuration;
  
  // Max number of frames keyframe could possibly be offset.
  PRInt64 keyframeDiff = (1 << mInfo.keyframe_granule_shift) - 1;

  // Length of frame in usecs.
  PRInt64 d = 0; // d will be 0 if multiplication overflows.
  MulOverflow(USECS_PER_S, mInfo.fps_denominator, d);
  frameDuration = d / mInfo.fps_numerator;

  // Total time in usecs keyframe can be offset from any given frame.
  return frameDuration * keyframeDiff;
}

nsresult
nsTheoraState::PageIn(ogg_page* aPage)
{
  if (!mActive)
    return NS_OK;
  NS_ASSERTION(static_cast<PRUint32>(ogg_page_serialno(aPage)) == mSerial,
               "Page must be for this stream!");
  if (ogg_stream_pagein(&mState, aPage) == -1)
    return NS_ERROR_FAILURE;
  PRBool foundGp;
  nsresult res = PacketOutUntilGranulepos(foundGp);
  if (NS_FAILED(res))
    return res;
  if (foundGp && mDoneReadingHeaders) {
    // We've found a packet with a granulepos, and we've loaded our metadata
    // and initialized our decoder. Determine granulepos of buffered packets.
    ReconstructTheoraGranulepos();
    for (PRUint32 i = 0; i < mUnstamped.Length(); ++i) {
      ogg_packet* packet = mUnstamped[i];
#ifdef DEBUG
      NS_ASSERTION(!IsHeader(packet), "Don't try to recover header packet gp");
      NS_ASSERTION(packet->granulepos != -1, "Packet must have gp by now");
#endif
      mPackets.Append(packet);
    }
    mUnstamped.Clear();
  }
  return NS_OK;
}

// Returns 1 if the Theora info struct is decoding a media of Theora
// version (maj,min,sub) or later, otherwise returns 0.
int
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

void nsTheoraState::ReconstructTheoraGranulepos()
{
  if (mUnstamped.Length() == 0) {
    return;
  }
  ogg_int64_t lastGranulepos = mUnstamped[mUnstamped.Length() - 1]->granulepos;
  NS_ASSERTION(lastGranulepos != -1, "Must know last granulepos");

  // Reconstruct the granulepos (and thus timestamps) of the decoded
  // frames. Granulepos are stored as ((keyframe<<shift)+offset). We
  // know the granulepos of the last frame in the list, so we can infer
  // the granulepos of the intermediate frames using their frame numbers.
  ogg_int64_t shift = mInfo.keyframe_granule_shift;
  ogg_int64_t version_3_2_1 = TheoraVersion(&mInfo,3,2,1);
  ogg_int64_t lastFrame = th_granule_frame(mCtx,
                                           lastGranulepos) + version_3_2_1;
  ogg_int64_t firstFrame = lastFrame - mUnstamped.Length() + 1;

  // Until we encounter a keyframe, we'll assume that the "keyframe"
  // segment of the granulepos is the first frame, or if that causes
  // the "offset" segment to overflow, we assume the required
  // keyframe is maximumally offset. Until we encounter a keyframe
  // the granulepos will probably be wrong, but we can't decode the
  // frame anyway (since we don't have its keyframe) so it doesn't really
  // matter.
  ogg_int64_t keyframe = lastGranulepos >> shift;

  // The lastFrame, firstFrame, keyframe variables, as well as the frame
  // variable in the loop below, store the frame number for Theora
  // version >= 3.2.1 streams, and store the frame index for Theora
  // version < 3.2.1 streams.
  for (PRUint32 i = 0; i < mUnstamped.Length() - 1; ++i) {
    ogg_int64_t frame = firstFrame + i;
    ogg_int64_t granulepos;
    ogg_packet* packet = mUnstamped[i];
    PRBool isKeyframe = th_packet_iskeyframe(packet) == 1;

    if (isKeyframe) {
      granulepos = frame << shift;
      keyframe = frame;
    } else if (frame >= keyframe &&
                frame - keyframe < ((ogg_int64_t)1 << shift))
    {
      // (frame - keyframe) won't overflow the "offset" segment of the
      // granulepos, so it's safe to calculate the granulepos.
      granulepos = (keyframe << shift) + (frame - keyframe);
    } else {
      // (frame - keyframeno) will overflow the "offset" segment of the
      // granulepos, so we take "keyframe" to be the max possible offset
      // frame instead.
      ogg_int64_t k = NS_MAX(frame - (((ogg_int64_t)1 << shift) - 1), version_3_2_1);
      granulepos = (k << shift) + (frame - k);
    }
    // Theora 3.2.1+ granulepos store frame number [1..N], so granulepos
    // should be > 0.
    // Theora 3.2.0 granulepos store the frame index [0..(N-1)], so
    // granulepos should be >= 0. 
    NS_ASSERTION(granulepos >= version_3_2_1,
                  "Invalid granulepos for Theora version");

    // Check that the frame's granule number is one more than the
    // previous frame's.
    NS_ASSERTION(i == 0 ||
                 th_granule_frame(mCtx, granulepos) ==
                 th_granule_frame(mCtx, mUnstamped[i-1]->granulepos) + 1,
                 "Granulepos calculation is incorrect!");

    packet->granulepos = granulepos;
  }

  // Check that the second to last frame's granule number is one less than
  // the last frame's (the known granule number). If not our granulepos
  // recovery missed a beat.
  NS_ASSERTION(mUnstamped.Length() < 2 ||
    th_granule_frame(mCtx, mUnstamped[mUnstamped.Length()-2]->granulepos) + 1 ==
    th_granule_frame(mCtx, lastGranulepos),
    "Granulepos recovery should catch up with packet->granulepos!");
}

nsresult nsVorbisState::Reset()
{
  nsresult res = NS_OK;
  if (mActive && vorbis_synthesis_restart(&mDsp) != 0) {
    res = NS_ERROR_FAILURE;
  }
  if (NS_FAILED(nsOggCodecState::Reset())) {
    return NS_ERROR_FAILURE;
  }

  mGranulepos = 0;
  mPrevVorbisBlockSize = 0;

  return res;
}

nsVorbisState::nsVorbisState(ogg_page* aBosPage) :
  nsOggCodecState(aBosPage, PR_TRUE),
  mPrevVorbisBlockSize(0),
  mGranulepos(0)
{
  MOZ_COUNT_CTOR(nsVorbisState);
  vorbis_info_init(&mInfo);
  vorbis_comment_init(&mComment);
  memset(&mDsp, 0, sizeof(vorbis_dsp_state));
  memset(&mBlock, 0, sizeof(vorbis_block));
}

nsVorbisState::~nsVorbisState() {
  MOZ_COUNT_DTOR(nsVorbisState);
  Reset();
  vorbis_block_clear(&mBlock);
  vorbis_dsp_clear(&mDsp);
  vorbis_info_clear(&mInfo);
  vorbis_comment_clear(&mComment);
}

PRBool nsVorbisState::DecodeHeader(ogg_packet* aPacket) {
  mPacketCount++;
  int ret = vorbis_synthesis_headerin(&mInfo,
                                      &mComment,
                                      aPacket);
  // We must determine when we've read the last header packet.
  // vorbis_synthesis_headerin() does not tell us when it's read the last
  // header, so we must keep track of the headers externally.
  //
  // There are 3 header packets, the Identification, Comment, and Setup
  // headers, which must be in that order. If they're out of order, the file
  // is invalid. If we've successfully read a header, and it's the setup
  // header, then we're done reading headers. The first byte of each packet
  // determines it's type as follows:
  //    0x1 -> Identification header
  //    0x3 -> Comment header
  //    0x5 -> Setup header
  // For more details of the Vorbis/Ogg containment scheme, see the Vorbis I
  // Specification, Chapter 4, Codec Setup and Packet Decode:
  // http://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-580004

  PRBool isSetupHeader = aPacket->bytes > 0 && aPacket->packet[0] == 0x5;

  if (ret < 0 || mPacketCount > 3) {
    // We've received an error, or the first three packets weren't valid
    // header packets, assume bad input, and deactivate the bitstream.
    mDoneReadingHeaders = PR_TRUE;
    mActive = PR_FALSE;
  } else if (ret == 0 && isSetupHeader && mPacketCount == 3) {
    // Successfully read the three header packets.
    // The bitstream remains active.
    mDoneReadingHeaders = PR_TRUE;
  }
  return mDoneReadingHeaders;
}

PRBool nsVorbisState::Init()
{
  if (!mActive)
    return PR_FALSE;

  int ret = vorbis_synthesis_init(&mDsp, &mInfo);
  if (ret != 0) {
    NS_WARNING("vorbis_synthesis_init() failed initializing vorbis bitstream");
    return mActive = PR_FALSE;
  }
  ret = vorbis_block_init(&mDsp, &mBlock);
  if (ret != 0) {
    NS_WARNING("vorbis_block_init() failed initializing vorbis bitstream");
    if (mActive) {
      vorbis_dsp_clear(&mDsp);
    }
    return mActive = PR_FALSE;
  }
  return PR_TRUE;
}

PRInt64 nsVorbisState::Time(PRInt64 granulepos)
{
  if (!mActive) {
    return -1;
  }

  return nsVorbisState::Time(&mInfo, granulepos);
}

PRInt64 nsVorbisState::Time(vorbis_info* aInfo, PRInt64 aGranulepos)
{
  if (aGranulepos == -1 || aInfo->rate == 0) {
    return -1;
  }
  PRInt64 t = 0;
  MulOverflow(USECS_PER_S, aGranulepos, t);
  return t / aInfo->rate;
}

PRBool
nsVorbisState::IsHeader(ogg_packet* aPacket)
{
  // The first byte in each Vorbis header packet is either 0x01, 0x03, or 0x05,
  // i.e. the first bit is odd. Audio data packets have their first bit as 0x0.
  // Any packet with its first bit set cannot be a data packet, it's a
  // (possibly invalid) header packet.
  // See: http://xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-610004.2.1
  return aPacket->bytes > 0 ? (aPacket->packet[0] & 0x1) : PR_FALSE;
}

nsresult
nsVorbisState::PageIn(ogg_page* aPage)
{
  if (!mActive)
    return NS_OK;
  NS_ASSERTION(static_cast<PRUint32>(ogg_page_serialno(aPage)) == mSerial,
               "Page must be for this stream!");
  if (ogg_stream_pagein(&mState, aPage) == -1)
    return NS_ERROR_FAILURE;
  PRBool foundGp;
  nsresult res = PacketOutUntilGranulepos(foundGp);
  if (NS_FAILED(res))
    return res;
  if (foundGp && mDoneReadingHeaders) {
    // We've found a packet with a granulepos, and we've loaded our metadata
    // and initialized our decoder. Determine granulepos of buffered packets.
    ReconstructVorbisGranulepos();
    for (PRUint32 i = 0; i < mUnstamped.Length(); ++i) {
      ogg_packet* packet = mUnstamped[i];
      AssertHasRecordedPacketSamples(packet);
      NS_ASSERTION(!IsHeader(packet), "Don't try to recover header packet gp");
      NS_ASSERTION(packet->granulepos != -1, "Packet must have gp by now");
      mPackets.Append(packet);
    }
    mUnstamped.Clear();
  }
  return NS_OK;
}

nsresult nsVorbisState::ReconstructVorbisGranulepos()
{
  // The number of samples in a Vorbis packet is:
  // window_blocksize(previous_packet)/4+window_blocksize(current_packet)/4
  // See: http://xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-230001.3.2
  // So we maintain mPrevVorbisBlockSize, the block size of the last packet
  // encountered. We also maintain mGranulepos, which is the granulepos of
  // the last encountered packet. This enables us to give granulepos to
  // packets when the last packet in mUnstamped doesn't have a granulepos
  // (for example if the stream was truncated).
  //
  // We validate our prediction of the number of samples decoded when
  // VALIDATE_VORBIS_SAMPLE_CALCULATION is defined by recording the predicted
  // number of samples, and verifing we extract that many when decoding
  // each packet.

  NS_ASSERTION(mUnstamped.Length() > 0, "Length must be > 0");
  ogg_packet* last = mUnstamped[mUnstamped.Length()-1];
  NS_ASSERTION(last->e_o_s || last->granulepos >= 0,
    "Must know last granulepos!");
  if (mUnstamped.Length() == 1) {
    ogg_packet* packet = mUnstamped[0];
    long blockSize = vorbis_packet_blocksize(&mInfo, packet);
    if (blockSize < 0) {
      // On failure vorbis_packet_blocksize returns < 0. If we've got
      // a bad packet, we just assume that decode will have to skip this
      // packet, i.e. assume 0 samples are decodable from this packet.
      blockSize = 0;
      mPrevVorbisBlockSize = 0;
    }
    long samples = mPrevVorbisBlockSize / 4 + blockSize / 4;
    mPrevVorbisBlockSize = blockSize;
    if (packet->granulepos == -1) {
      packet->granulepos = mGranulepos + samples;
    }

    // Account for a partial last frame
    if (packet->e_o_s && packet->granulepos >= mGranulepos) {
       samples = packet->granulepos - mGranulepos;
    }
 
    mGranulepos = packet->granulepos;
    RecordVorbisPacketSamples(packet, samples);
    return NS_OK;
  }

  PRBool unknownGranulepos = last->granulepos == -1;
  int totalSamples = 0;
  for (PRInt32 i = mUnstamped.Length() - 1; i > 0; i--) {
    ogg_packet* packet = mUnstamped[i];
    ogg_packet* prev = mUnstamped[i-1];
    ogg_int64_t granulepos = packet->granulepos;
    NS_ASSERTION(granulepos != -1, "Must know granulepos!");
    long prevBlockSize = vorbis_packet_blocksize(&mInfo, prev);
    long blockSize = vorbis_packet_blocksize(&mInfo, packet);

    if (blockSize < 0 || prevBlockSize < 0) {
      // On failure vorbis_packet_blocksize returns < 0. If we've got
      // a bad packet, we just assume that decode will have to skip this
      // packet, i.e. assume 0 samples are decodable from this packet.
      blockSize = 0;
      prevBlockSize = 0;
    }

    long samples = prevBlockSize / 4 + blockSize / 4;
    totalSamples += samples;
    prev->granulepos = granulepos - samples;
    RecordVorbisPacketSamples(packet, samples);
  }

  if (unknownGranulepos) {
    for (PRUint32 i = 0; i < mUnstamped.Length(); i++) {
      ogg_packet* packet = mUnstamped[i];
      packet->granulepos += mGranulepos + totalSamples + 1;
    }
  }

  ogg_packet* first = mUnstamped[0];
  long blockSize = vorbis_packet_blocksize(&mInfo, first);
  if (blockSize < 0) {
    mPrevVorbisBlockSize = 0;
    blockSize = 0;
  }

  long samples = (mPrevVorbisBlockSize == 0) ? 0 :
                  mPrevVorbisBlockSize / 4 + blockSize / 4;
  PRInt64 start = first->granulepos - samples;
  RecordVorbisPacketSamples(first, samples);

  if (last->e_o_s && start < mGranulepos) {
    // We've calculated that there are more samples in this page than its
    // granulepos claims, and it's the last page in the stream. This is legal,
    // and we will need to prune the trailing samples when we come to decode it.
    // We must correct the timestamps so that they follow the last Vorbis page's
    // samples.
    PRInt64 pruned = mGranulepos - start;
    for (PRUint32 i = 0; i < mUnstamped.Length() - 1; i++) {
      mUnstamped[i]->granulepos += pruned;
    }
#ifdef VALIDATE_VORBIS_SAMPLE_CALCULATION
    mVorbisPacketSamples[last] -= pruned;
#endif
  }

  mPrevVorbisBlockSize = vorbis_packet_blocksize(&mInfo, last);
  mPrevVorbisBlockSize = NS_MAX(static_cast<long>(0), mPrevVorbisBlockSize);
  mGranulepos = last->granulepos;

  return NS_OK;
}


nsSkeletonState::nsSkeletonState(ogg_page* aBosPage)
  : nsOggCodecState(aBosPage, PR_TRUE),
    mVersion(0),
    mPresentationTime(0),
    mLength(0)
{
  MOZ_COUNT_CTOR(nsSkeletonState);
}
 
nsSkeletonState::~nsSkeletonState()
{
  MOZ_COUNT_DTOR(nsSkeletonState);
}

// Support for Ogg Skeleton 4.0, as per specification at:
// http://wiki.xiph.org/Ogg_Skeleton_4

// Minimum length in bytes of a Skeleton header packet.
#define SKELETON_MIN_HEADER_LEN 28
#define SKELETON_4_0_MIN_HEADER_LEN 80

// Minimum length in bytes of a Skeleton 4.0 index packet.
#define SKELETON_4_0_MIN_INDEX_LEN 42

// Minimum possible size of a compressed index keypoint.
#define MIN_KEY_POINT_SIZE 2

// Byte offset of the major and minor version numbers in the
// Ogg Skeleton 4.0 header packet.
#define SKELETON_VERSION_MAJOR_OFFSET 8
#define SKELETON_VERSION_MINOR_OFFSET 10

// Byte-offsets of the presentation time numerator and denominator
#define SKELETON_PRESENTATION_TIME_NUMERATOR_OFFSET 12
#define SKELETON_PRESENTATION_TIME_DENOMINATOR_OFFSET 20

// Byte-offsets of the length of file field in the Skeleton 4.0 header packet.
#define SKELETON_FILE_LENGTH_OFFSET 64

// Byte-offsets of the fields in the Skeleton index packet.
#define INDEX_SERIALNO_OFFSET 6
#define INDEX_NUM_KEYPOINTS_OFFSET 10
#define INDEX_TIME_DENOM_OFFSET 18
#define INDEX_FIRST_NUMER_OFFSET 26
#define INDEX_LAST_NUMER_OFFSET 34
#define INDEX_KEYPOINT_OFFSET 42

static PRBool IsSkeletonBOS(ogg_packet* aPacket)
{
  return aPacket->bytes >= SKELETON_MIN_HEADER_LEN && 
         memcmp(reinterpret_cast<char*>(aPacket->packet), "fishead", 8) == 0;
}

static PRBool IsSkeletonIndex(ogg_packet* aPacket)
{
  return aPacket->bytes >= SKELETON_4_0_MIN_INDEX_LEN &&
         memcmp(reinterpret_cast<char*>(aPacket->packet), "index", 5) == 0;
}

// Reads a little-endian encoded unsigned 32bit integer at p.
static PRUint32 LEUint32(const unsigned char* p)
{
  return p[0] +
        (p[1] << 8) + 
        (p[2] << 16) +
        (p[3] << 24);
}

// Reads a little-endian encoded 64bit integer at p.
static PRInt64 LEInt64(const unsigned char* p)
{
  PRUint32 lo = LEUint32(p);
  PRUint32 hi = LEUint32(p + 4);
  return static_cast<PRInt64>(lo) | (static_cast<PRInt64>(hi) << 32);
}

// Reads a little-endian encoded unsigned 16bit integer at p.
static PRUint16 LEUint16(const unsigned char* p)
{
  return p[0] + (p[1] << 8);  
}

// Reads a variable length encoded integer at p. Will not read
// past aLimit. Returns pointer to character after end of integer.
static const unsigned char* ReadVariableLengthInt(const unsigned char* p,
                                                  const unsigned char* aLimit,
                                                  PRInt64& n)
{
  int shift = 0;
  PRInt64 byte = 0;
  n = 0;
  while (p < aLimit &&
         (byte & 0x80) != 0x80 &&
         shift < 57)
  {
    byte = static_cast<PRInt64>(*p);
    n |= ((byte & 0x7f) << shift);
    shift += 7;
    p++;
  }
  return p;
}

PRBool nsSkeletonState::DecodeIndex(ogg_packet* aPacket)
{
  NS_ASSERTION(aPacket->bytes >= SKELETON_4_0_MIN_INDEX_LEN,
               "Index must be at least minimum size");
  if (!mActive) {
    return PR_FALSE;
  }

  PRUint32 serialno = LEUint32(aPacket->packet + INDEX_SERIALNO_OFFSET);
  PRInt64 numKeyPoints = LEInt64(aPacket->packet + INDEX_NUM_KEYPOINTS_OFFSET);

  PRInt64 n = 0;
  PRInt64 endTime = 0, startTime = 0;
  const unsigned char* p = aPacket->packet;

  PRInt64 timeDenom = LEInt64(aPacket->packet + INDEX_TIME_DENOM_OFFSET);
  if (timeDenom == 0) {
    LOG(PR_LOG_DEBUG, ("Ogg Skeleton Index packet for stream %u has 0 "
                       "timestamp denominator.", serialno));
    return (mActive = PR_FALSE);
  }

  // Extract the start time.
  n = LEInt64(p + INDEX_FIRST_NUMER_OFFSET);
  PRInt64 t;
  if (!MulOverflow(n, USECS_PER_S, t)) {
    return (mActive = PR_FALSE);
  } else {
    startTime = t / timeDenom;
  }

  // Extract the end time.
  n = LEInt64(p + INDEX_LAST_NUMER_OFFSET);
  if (!MulOverflow(n, USECS_PER_S, t)) {
    return (mActive = PR_FALSE);
  } else {
    endTime = t / timeDenom;
  }

  // Check the numKeyPoints value read, ensure we're not going to run out of
  // memory while trying to decode the index packet.
  PRInt64 minPacketSize;
  if (!MulOverflow(numKeyPoints, MIN_KEY_POINT_SIZE, minPacketSize) ||
      !AddOverflow(INDEX_KEYPOINT_OFFSET, minPacketSize, minPacketSize))
  {
    return (mActive = PR_FALSE);
  }
  
  PRInt64 sizeofIndex = aPacket->bytes - INDEX_KEYPOINT_OFFSET;
  PRInt64 maxNumKeyPoints = sizeofIndex / MIN_KEY_POINT_SIZE;
  if (aPacket->bytes < minPacketSize ||
      numKeyPoints > maxNumKeyPoints || 
      numKeyPoints < 0)
  {
    // Packet size is less than the theoretical minimum size, or the packet is
    // claiming to store more keypoints than it's capable of storing. This means
    // that the numKeyPoints field is too large or small for the packet to
    // possibly contain as many packets as it claims to, so the numKeyPoints
    // field is possibly malicious. Don't try decoding this index, we may run
    // out of memory.
    LOG(PR_LOG_DEBUG, ("Possibly malicious number of key points reported "
                       "(%lld) in index packet for stream %u.",
                       numKeyPoints,
                       serialno));
    return (mActive = PR_FALSE);
  }

  nsAutoPtr<nsKeyFrameIndex> keyPoints(new nsKeyFrameIndex(startTime, endTime));
  
  p = aPacket->packet + INDEX_KEYPOINT_OFFSET;
  const unsigned char* limit = aPacket->packet + aPacket->bytes;
  PRInt64 numKeyPointsRead = 0;
  PRInt64 offset = 0;
  PRInt64 time = 0;
  while (p < limit &&
         numKeyPointsRead < numKeyPoints)
  {
    PRInt64 delta = 0;
    p = ReadVariableLengthInt(p, limit, delta);
    if (p == limit ||
        !AddOverflow(offset, delta, offset) ||
        offset > mLength ||
        offset < 0)
    {
      return (mActive = PR_FALSE);
    }
    p = ReadVariableLengthInt(p, limit, delta);
    if (!AddOverflow(time, delta, time) ||
        time > endTime ||
        time < startTime)
    {
      return (mActive = PR_FALSE);
    }
    PRInt64 timeUsecs = 0;
    if (!MulOverflow(time, USECS_PER_S, timeUsecs))
      return mActive = PR_FALSE;
    timeUsecs /= timeDenom;
    keyPoints->Add(offset, timeUsecs);
    numKeyPointsRead++;
  }

  PRInt32 keyPointsRead = keyPoints->Length();
  if (keyPointsRead > 0) {
    mIndex.Put(serialno, keyPoints.forget());
  }

  LOG(PR_LOG_DEBUG, ("Loaded %d keypoints for Skeleton on stream %u",
                     keyPointsRead, serialno));
  return PR_TRUE;
}

nsresult nsSkeletonState::IndexedSeekTargetForTrack(PRUint32 aSerialno,
                                                    PRInt64 aTarget,
                                                    nsKeyPoint& aResult)
{
  nsKeyFrameIndex* index = nsnull;
  mIndex.Get(aSerialno, &index);

  if (!index ||
      index->Length() == 0 ||
      aTarget < index->mStartTime ||
      aTarget > index->mEndTime)
  {
    return NS_ERROR_FAILURE;
  }

  // Binary search to find the last key point with time less than target.
  int start = 0;
  int end = index->Length() - 1;
  while (end > start) {
    int mid = start + ((end - start + 1) >> 1);
    if (index->Get(mid).mTime == aTarget) {
       start = mid;
       break;
    } else if (index->Get(mid).mTime < aTarget) {
      start = mid;
    } else {
      end = mid - 1;
    }
  }

  aResult = index->Get(start);
  NS_ASSERTION(aResult.mTime <= aTarget, "Result should have time <= target");
  return NS_OK;
}

nsresult nsSkeletonState::IndexedSeekTarget(PRInt64 aTarget,
                                            nsTArray<PRUint32>& aTracks,
                                            nsSeekTarget& aResult)
{
  if (!mActive || mVersion < SKELETON_VERSION(4,0)) {
    return NS_ERROR_FAILURE;
  }
  // Loop over all requested tracks' indexes, and get the keypoint for that
  // seek target. Record the keypoint with the lowest offset, this will be
  // our seek result. User must seek to the one with lowest offset to ensure we
  // pass "keyframes" on all tracks when we decode forwards to the seek target.
  nsSeekTarget r;
  for (PRUint32 i=0; i<aTracks.Length(); i++) {
    nsKeyPoint k;
    if (NS_SUCCEEDED(IndexedSeekTargetForTrack(aTracks[i], aTarget, k)) &&
        k.mOffset < r.mKeyPoint.mOffset)
    {
      r.mKeyPoint = k;
      r.mSerial = aTracks[i];
    }
  }
  if (r.IsNull()) {
    return NS_ERROR_FAILURE;
  }
  LOG(PR_LOG_DEBUG, ("Indexed seek target for time %lld is offset %lld",
                     aTarget, r.mKeyPoint.mOffset));
  aResult = r;
  return NS_OK;
}

nsresult nsSkeletonState::GetDuration(const nsTArray<PRUint32>& aTracks,
                                      PRInt64& aDuration)
{
  if (!mActive ||
      mVersion < SKELETON_VERSION(4,0) ||
      !HasIndex() ||
      aTracks.Length() == 0)
  {
    return NS_ERROR_FAILURE;
  }
  PRInt64 endTime = PR_INT64_MIN;
  PRInt64 startTime = PR_INT64_MAX;
  for (PRUint32 i=0; i<aTracks.Length(); i++) {
    nsKeyFrameIndex* index = nsnull;
    mIndex.Get(aTracks[i], &index);
    if (!index) {
      // Can't get the timestamps for one of the required tracks, fail.
      return NS_ERROR_FAILURE;
    }
    if (index->mEndTime > endTime) {
      endTime = index->mEndTime;
    }
    if (index->mStartTime < startTime) {
      startTime = index->mStartTime;
    }
  }
  NS_ASSERTION(endTime > startTime, "Duration must be positive");
  return AddOverflow(endTime, -startTime, aDuration) ? NS_OK : NS_ERROR_FAILURE;
}

PRBool nsSkeletonState::DecodeHeader(ogg_packet* aPacket)
{
  if (IsSkeletonBOS(aPacket)) {
    PRUint16 verMajor = LEUint16(aPacket->packet + SKELETON_VERSION_MAJOR_OFFSET);
    PRUint16 verMinor = LEUint16(aPacket->packet + SKELETON_VERSION_MINOR_OFFSET);

    // Read the presentation time. We read this before the version check as the
    // presentation time exists in all versions.
    PRInt64 n = LEInt64(aPacket->packet + SKELETON_PRESENTATION_TIME_NUMERATOR_OFFSET);
    PRInt64 d = LEInt64(aPacket->packet + SKELETON_PRESENTATION_TIME_DENOMINATOR_OFFSET);
    mPresentationTime = d == 0 ? 0 : (static_cast<float>(n) / static_cast<float>(d)) * USECS_PER_S;

    mVersion = SKELETON_VERSION(verMajor, verMinor);
    if (mVersion < SKELETON_VERSION(4,0) ||
        mVersion >= SKELETON_VERSION(5,0) ||
        aPacket->bytes < SKELETON_4_0_MIN_HEADER_LEN)
    {
      // We can only care to parse Skeleton version 4.0+.
      mActive = PR_FALSE;
      return mDoneReadingHeaders = PR_TRUE;
    }

    // Extract the segment length.
    mLength = LEInt64(aPacket->packet + SKELETON_FILE_LENGTH_OFFSET);

    LOG(PR_LOG_DEBUG, ("Skeleton segment length: %lld", mLength));

    // Initialize the serianlno-to-index map.
    PRBool init = mIndex.Init();
    if (!init) {
      NS_WARNING("Failed to initialize Ogg skeleton serialno-to-index map");
      mActive = PR_FALSE;
      return mDoneReadingHeaders = PR_TRUE;
    }
    mActive = PR_TRUE;
  } else if (IsSkeletonIndex(aPacket) && mVersion >= SKELETON_VERSION(4,0)) {
    if (!DecodeIndex(aPacket)) {
      // Failed to parse index, or invalid/hostile index. DecodeIndex() will
      // have deactivated the track.
      return mDoneReadingHeaders = PR_TRUE;
    }

  } else if (aPacket->e_o_s) {
    mDoneReadingHeaders = PR_TRUE;
  }
  return mDoneReadingHeaders;
}

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
  nsAutoPtr<nsOggCodecState> codecState;
  if (aPage->body_len > 6 && memcmp(aPage->body+1, "theora", 6) == 0) {
    codecState = new nsTheoraState(aPage);
  } else if (aPage->body_len > 6 && memcmp(aPage->body+1, "vorbis", 6) == 0) {
    codecState = new nsVorbisState(aPage);
  } else if (aPage->body_len > 8 && memcmp(aPage->body, "fishead\0", 8) == 0) {
    codecState = new nsSkeletonState(aPage);
  } else {
    codecState = new nsOggCodecState(aPage);
  }
  return codecState->nsOggCodecState::Init() ? codecState.forget() : nsnull;
}

nsOggCodecState::nsOggCodecState(ogg_page* aBosPage) :
  mPacketCount(0),
  mSerial(ogg_page_serialno(aBosPage)),
  mActive(PR_FALSE),
  mDoneReadingHeaders(PR_FALSE)
{
  MOZ_COUNT_CTOR(nsOggCodecState);
  memset(&mState, 0, sizeof(ogg_stream_state));
}

nsOggCodecState::~nsOggCodecState() {
  MOZ_COUNT_DTOR(nsOggCodecState);
  int ret = ogg_stream_clear(&mState);
  NS_ASSERTION(ret == 0, "ogg_stream_clear failed");
}

nsresult nsOggCodecState::Reset() {
  if (ogg_stream_reset(&mState) != 0) {
    return NS_ERROR_FAILURE;
  }
  mBuffer.Erase();
  return NS_OK;
}

PRBool nsOggCodecState::Init() {
  int ret = ogg_stream_init(&mState, mSerial);
  return ret == 0;
}

void nsPageQueue::Append(ogg_page* aPage) {
  ogg_page* p = new ogg_page();
  p->header_len = aPage->header_len;
  p->body_len = aPage->body_len;
  p->header = new unsigned char[p->header_len + p->body_len];
  p->body = p->header + p->header_len;
  memcpy(p->header, aPage->header, p->header_len);
  memcpy(p->body, aPage->body, p->body_len);
  nsDeque::Push(p);
}

PRBool nsOggCodecState::PageInFromBuffer() {
  if (mBuffer.IsEmpty())
    return PR_FALSE;
  ogg_page *p = mBuffer.PeekFront();
  int ret = ogg_stream_pagein(&mState, p);
  NS_ENSURE_TRUE(ret == 0, PR_FALSE);
  mBuffer.PopFront();
  delete p->header;
  delete p;
  return PR_TRUE;
}

nsTheoraState::nsTheoraState(ogg_page* aBosPage) :
  nsOggCodecState(aBosPage),
  mSetup(0),
  mCtx(0),
  mFrameDuration(0),
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

  PRInt64 n = mInfo.fps_numerator;
  PRInt64 d = mInfo.fps_denominator;

  PRInt64 f;
  if (!MulOverflow(1000, d, f)) {
    return mActive = PR_FALSE;
  }
  f /= n;
  if (f > PR_UINT32_MAX) {
    return mActive = PR_FALSE;
  }
  mFrameDuration = static_cast<PRUint32>(f);

  n = mInfo.aspect_numerator;

  d = mInfo.aspect_denominator;
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
  if (!MulOverflow(t, 1000, t))
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
  if (!MulOverflow(frameno, 1000, t))
    return -1;
  if (!MulOverflow(t, mInfo.fps_denominator, t))
    return -1;
  return t / mInfo.fps_numerator;
}

PRInt64
nsTheoraState::MaxKeyframeOffset()
{
  // Determine the maximum time in milliseconds by which a key frame could
  // offset for the theora bitstream. Theora granulepos encode time as:
  // ((key_frame_number << granule_shift) + frame_offset).
  // Therefore the maximum possible time by which any frame could be offset
  // from a keyframe is the duration of (1 << granule_shift) - 1) frames.
  PRInt64 frameDuration;
  PRInt64 keyframeDiff;

  PRInt64 shift = mInfo.keyframe_granule_shift;

  // Max number of frames keyframe could possibly be offset.
  keyframeDiff = (1 << shift) - 1;

  // Length of frame in ms.
  PRInt64 d = 0; // d will be 0 if multiplication overflows.
  MulOverflow(1000, mInfo.fps_denominator, d);
  frameDuration = d / mInfo.fps_numerator;

  // Total time in ms keyframe can be offset from any given frame.
  return frameDuration * keyframeDiff;
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
  return res;
}

nsVorbisState::nsVorbisState(ogg_page* aBosPage) :
  nsOggCodecState(aBosPage)
{
  MOZ_COUNT_CTOR(nsVorbisState);
  vorbis_info_init(&mInfo);
  vorbis_comment_init(&mComment);
  memset(&mDsp, 0, sizeof(vorbis_dsp_state));
  memset(&mBlock, 0, sizeof(vorbis_block));
}

nsVorbisState::~nsVorbisState() {
  MOZ_COUNT_DTOR(nsVorbisState);
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
    // header packets, assume bad input, and don't activate the bitstream.
    mDoneReadingHeaders = PR_TRUE;
  } else if (ret == 0 && isSetupHeader && mPacketCount == 3) {
    // Successfully read the three header packets, activate the bitstream.
    mDoneReadingHeaders = PR_TRUE;
    mActive = PR_TRUE;
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
  MulOverflow(1000, aGranulepos, t);
  return t / aInfo->rate;
}

nsSkeletonState::nsSkeletonState(ogg_page* aBosPage)
  : nsOggCodecState(aBosPage),
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
  if (!MulOverflow(n, 1000, t)) {
    return (mActive = PR_FALSE);
  } else {
    startTime = t / timeDenom;
  }

  // Extract the end time.
  n = LEInt64(p + INDEX_LAST_NUMER_OFFSET);
  if (!MulOverflow(n, 1000, t)) {
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
    PRInt64 timeMs = 0;
    if (!MulOverflow(time, 1000, timeMs))
      return mActive = PR_FALSE;
    timeMs /= timeDenom;
    keyPoints->Add(offset, timeMs);
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
    mPresentationTime = d == 0 ? 0 : (static_cast<float>(n) / static_cast<float>(d)) * 1000;

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

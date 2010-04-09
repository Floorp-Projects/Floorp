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
#include "nsOggHacks.h"

/*
   The maximum height and width of the video. Used for
   sanitizing the memory allocation of the RGB buffer.
   The maximum resolution we anticipate encountering in the
   wild is 2160p - 3840x2160 pixels.
*/
#define MAX_VIDEO_WIDTH  4000
#define MAX_VIDEO_HEIGHT 3000

// Adds two 64bit numbers, retuns PR_TRUE if addition succeeded, or PR_FALSE
// if addition would result in an overflow.
static PRBool AddOverflow(PRInt64 a, PRInt64 b, PRInt64& aResult);

// 64 bit integer multiplication with overflow checking. Returns PR_TRUE
// if the multiplication was successful, or PR_FALSE if the operation resulted
// in an integer overflow.
static PRBool MulOverflow(PRInt64 a, PRInt64 b, PRInt64& aResult);

// Defined in nsOggReader.cpp.
extern PRBool MulOverflow32(PRUint32 a, PRUint32 b, PRUint32& aResult);


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
  mFrameRate(0),
  mAspectRatio(0)
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
  mCtx = th_decode_alloc(&mInfo, mSetup);
  if (mCtx == NULL) {
    return mActive = PR_FALSE;
  }

  PRInt64 n = mInfo.fps_numerator;
  PRInt64 d = mInfo.fps_denominator;

  mFrameRate = (n == 0 || d == 0) ?
    0.0f : static_cast<float>(n) / static_cast<float>(d);

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
  mAspectRatio = (n == 0 || d == 0) ?
    1.0f : static_cast<float>(n) / static_cast<float>(d);

  // Ensure the frame isn't larger than our prescribed maximum.
  PRUint32 pixels;
  if (!MulOverflow32(mInfo.pic_width, mInfo.pic_height, pixels) ||
      pixels > MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT ||
      pixels == 0)
  {
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
  if (granulepos < 0 || !mActive || mInfo.fps_numerator == 0) {
    return -1;
  }
  PRInt64 t = 0;
  PRInt64 frameno = th_granule_frame(mCtx, granulepos);
  if (!AddOverflow(frameno, 1, t))
    return -1;
  if (!MulOverflow(t, 1000, t))
    return -1;
  if (!MulOverflow(t, mInfo.fps_denominator, t))
    return -1;
  return t / mInfo.fps_numerator;
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

PRInt64 nsVorbisState::Time(PRInt64 granulepos) {
  if (granulepos == -1 || !mActive || mDsp.vi->rate == 0) {
    return -1;
  }
  PRInt64 t = 0;
  MulOverflow(1000, granulepos, t);
  return t / mDsp.vi->rate;
}

nsSkeletonState::nsSkeletonState(ogg_page* aBosPage)
  : nsOggCodecState(aBosPage)
{
  MOZ_COUNT_CTOR(nsSkeletonState);
}

nsSkeletonState::~nsSkeletonState()
{
  MOZ_COUNT_DTOR(nsSkeletonState);
}

PRBool nsSkeletonState::DecodeHeader(ogg_packet* aPacket)
{
  if (aPacket->e_o_s) {
    mActive = PR_TRUE;
    mDoneReadingHeaders = PR_TRUE;
  }
  return mDoneReadingHeaders;
}

// Adds two 64bit numbers, retuns PR_TRUE if addition succeeded, or PR_FALSE
// if addition would result in an overflow.
static PRBool AddOverflow(PRInt64 a, PRInt64 b, PRInt64& aResult) {
  if (b < 1) {
    if (PR_INT64_MIN - b <= a) {
      aResult = a + b;
      return PR_TRUE;
    }
  } else if (PR_INT64_MAX - b >= a) {
    aResult = a + b;
    return PR_TRUE;
  }
  return PR_FALSE;
}

// 64 bit integer multiplication with overflow checking. Returns PR_TRUE
// if the multiplication was successful, or PR_FALSE if the operation resulted
// in an integer overflow.
static PRBool MulOverflow(PRInt64 a, PRInt64 b, PRInt64& aResult) {
  // We break a multiplication a * b into of sign_a * sign_b * abs(a) * abs(b)
  //
  // This is equivalent to:
  //
  // (sign_a * sign_b) * ((a_hi * 2^32) + a_lo) * ((b_hi * 2^32) + b_lo)
  //
  // Which is equivalent to:
  //
  // (sign_a * sign_b) *
  // ((a_hi * b_hi << 64) +
  //  (a_hi * b_lo << 32) + (a_lo * b_hi << 32) +
  //   a_lo * b_lo)
  //
  // So to check if a*b overflows, we must check each sub part of the above
  // sum.
  //
  // Note: -1 * PR_INT64_MIN == PR_INT64_MIN ; we can't negate PR_INT64_MIN!
  // Note: Shift of negative numbers is undefined.
  //
  // Figure out the sign after multiplication. Then we can just work with
  // unsigned numbers.
  PRInt64 sign = (!(a < 0) == !(b < 0)) ? 1 : -1;

  PRInt64 abs_a = (a < 0) ? -a : a;
  PRInt64 abs_b = (b < 0) ? -b : b;

  if (abs_a < 0) {
    NS_ASSERTION(a == PR_INT64_MIN, "How else can this happen?");
    if (b == 0 || b == 1) {
      aResult = a * b;
      return PR_TRUE;
    } else {
      return PR_FALSE;
    }
  }

  if (abs_b < 0) {
    NS_ASSERTION(b == PR_INT64_MIN, "How else can this happen?");
    if (a == 0 || a == 1) {
      aResult = a * b;
      return PR_TRUE;
    } else {
      return PR_FALSE;
    }
  }

  NS_ASSERTION(abs_a >= 0 && abs_b >= 0, "abs values must be non-negative");

  PRInt64 a_hi = abs_a >> 32;
  PRInt64 a_lo = abs_a & 0xFFFFFFFF;
  PRInt64 b_hi = abs_b >> 32;
  PRInt64 b_lo = abs_b & 0xFFFFFFFF;

  NS_ASSERTION((a_hi<<32) + a_lo == abs_a, "Partition must be correct");
  NS_ASSERTION((b_hi<<32) + b_lo == abs_b, "Partition must be correct");

  // In the sub-equation (a_hi * b_hi << 64), if a_hi or b_hi
  // are non-zero, this will overflow as it's shifted by 64.
  // Abort if this overflows.
  if (a_hi != 0 && b_hi != 0) {
    return PR_FALSE;
  }

  // We can now assume that either a_hi or b_hi is 0.
  NS_ASSERTION(a_hi == 0 || b_hi == 0, "One of these must be 0");

  // Next we calculate:
  // (a_hi * b_lo << 32) + (a_lo * b_hi << 32)
  // We can factor this as:
  // (a_hi * b_lo + a_lo * b_hi) << 32
  PRInt64 q = a_hi * b_lo + a_lo * b_hi;
  if (q > PR_INT32_MAX) {
    // q will overflow when we shift by 32; abort.
    return PR_FALSE;
  }
  q <<= 32;

  // Both a_lo and b_lo are less than INT32_MAX, so can't overflow.
  PRUint64 lo = a_lo * b_lo;
  if (lo > PR_INT64_MAX) {
    return PR_FALSE;
  }

  // Add the final result. We must check for overflow during addition.
  if (!AddOverflow(q, static_cast<PRInt64>(lo), aResult)) {
    return PR_FALSE;
  }

  aResult *= sign;
  NS_ASSERTION(a * b == aResult, "We didn't overflow, but result is wrong!");
  return PR_TRUE;
}

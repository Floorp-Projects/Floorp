/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* Copyright (c) 2011, The WebRTC project authors. All rights reserved.
 * Copyright (c) 2014, Mozilla
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 ** Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 ** Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in
 *  the documentation and/or other materials provided with the
 *  distribution.
 *
 ** Neither the name of Google nor the names of its contributors may
 *  be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GMP_VIDEO_CODEC_h_
#define GMP_VIDEO_CODEC_h_

#include <stdint.h>
#include <stddef.h>

enum { kGMPPayloadNameSize = 32};
enum { kGMPMaxSimulcastStreams = 4};

enum GMPVideoCodecComplexity
{
  kGMPComplexityNormal = 0,
  kGMPComplexityHigh = 1,
  kGMPComplexityHigher = 2,
  kGMPComplexityMax = 3,
  kGMPComplexityInvalid // Should always be last
};

enum GMPVP8ResilienceMode {
  kResilienceOff,    // The stream produced by the encoder requires a
                     // recovery frame (typically a key frame) to be
                     // decodable after a packet loss.
  kResilientStream,  // A stream produced by the encoder is resilient to
                     // packet losses, but packets within a frame subsequent
                     // to a loss can't be decoded.
  kResilientFrames,  // Same as kResilientStream but with added resilience
                     // within a frame.
  kResilienceInvalid // Should always be last.
};

// VP8 specific
struct GMPVideoCodecVP8
{
  bool mPictureLossIndicationOn;
  bool mFeedbackModeOn;
  GMPVideoCodecComplexity mComplexity;
  GMPVP8ResilienceMode mResilience;
  uint32_t mNumberOfTemporalLayers;
  bool mDenoisingOn;
  bool mErrorConcealmentOn;
  bool mAutomaticResizeOn;
};

// H264 specific

// Needs to match a binary spec for this structure.
// Note: the mSPS at the end of this structure is variable length.
struct GMPVideoCodecH264AVCC
{
  uint8_t        mVersion; // == 0x01
  uint8_t        mProfile; // these 3 are profile_level_id
  uint8_t        mConstraints;
  uint8_t        mLevel;
  uint8_t        mLengthSizeMinusOne; // lower 2 bits (== GMPBufferType-1). Top 6 reserved (1's)

  // SPS/PPS will not generally be present for interactive use unless SDP
  // parameter-sets are used.
  uint8_t        mNumSPS; // lower 5 bits; top 5 reserved (1's)

  /*** uint8_t   mSPS[];  (Not defined due to compiler warnings and warnings-as-errors ...) **/
  // Following mNumSPS is a variable number of bytes, which is the SPS and PPS.
  // Each SPS == 16 bit size, ("N"), then "N" bytes,
  // then uint8_t mNumPPS, then each PPS == 16 bit size ("N"), then "N" bytes.
};

// Codec specific data for H.264 decoding/encoding.
// Cast the "aCodecSpecific" parameter of GMPVideoDecoder::InitDecode() and
// GMPVideoEncoder::InitEncode() to this structure.
struct GMPVideoCodecH264
{
  uint8_t        mPacketizationMode; // 0 or 1
  struct GMPVideoCodecH264AVCC mAVCC; // holds a variable-sized struct GMPVideoCodecH264AVCC mAVCC;
};

enum GMPVideoCodecType
{
  kGMPVideoCodecVP8,

  // Encoded frames are in AVCC format; NAL length field of 4 bytes, followed
  // by frame data. May be multiple NALUs per sample. Codec specific extra data
  // is the AVCC extra data (in AVCC format).
  kGMPVideoCodecH264,
  kGMPVideoCodecInvalid // Should always be last.
};

// Simulcast is when the same stream is encoded multiple times with different
// settings such as resolution.
struct GMPSimulcastStream
{
  uint32_t mWidth;
  uint32_t mHeight;
  uint32_t mNumberOfTemporalLayers;
  uint32_t mMaxBitrate; // kilobits/sec.
  uint32_t mTargetBitrate; // kilobits/sec.
  uint32_t mMinBitrate; // kilobits/sec.
  uint32_t mQPMax; // minimum quality
};

enum GMPVideoCodecMode {
  kGMPRealtimeVideo,
  kGMPScreensharing,
  kGMPStreamingVideo,
  kGMPCodecModeInvalid // Should always be last.
};

enum GMPApiVersion {
  kGMPVersion32 = 1, // leveraging that V32 had mCodecType first, and only supported H264
  kGMPVersion33 = 33,
};

struct GMPVideoCodec
{
  uint32_t mGMPApiVersion;

  GMPVideoCodecType mCodecType;
  char mPLName[kGMPPayloadNameSize]; // Must be NULL-terminated!
  uint32_t mPLType;

  uint32_t mWidth;
  uint32_t mHeight;

  uint32_t mStartBitrate; // kilobits/sec.
  uint32_t mMaxBitrate; // kilobits/sec.
  uint32_t mMinBitrate; // kilobits/sec.
  uint32_t mMaxFramerate;

  bool mFrameDroppingOn;
  int32_t mKeyFrameInterval;

  uint32_t mQPMax;
  uint32_t mNumberOfSimulcastStreams;
  GMPSimulcastStream mSimulcastStream[kGMPMaxSimulcastStreams];

  GMPVideoCodecMode mMode;
};

// Either single encoded unit, or multiple units separated by 8/16/24/32
// bit lengths, all with the same timestamp.  Note there is no final 0-length
// entry; one should check the overall end-of-buffer against where the next
// length would be.
enum GMPBufferType {
  GMP_BufferSingle = 0,
  GMP_BufferLength8,
  GMP_BufferLength16,
  GMP_BufferLength24,
  GMP_BufferLength32,
  GMP_BufferInvalid,
};

struct GMPCodecSpecificInfoGeneric {
  uint8_t mSimulcastIdx;
};

struct GMPCodecSpecificInfoH264 {
  uint8_t mSimulcastIdx;
};

// Note: if any pointers are added to this struct, it must be fitted
// with a copy-constructor. See below.
struct GMPCodecSpecificInfoVP8
{
  bool mHasReceivedSLI;
  uint8_t mPictureIdSLI;
  bool mHasReceivedRPSI;
  uint64_t mPictureIdRPSI;
  int16_t mPictureId; // negative value to skip pictureId
  bool mNonReference;
  uint8_t mSimulcastIdx;
  uint8_t mTemporalIdx;
  bool mLayerSync;
  int32_t mTL0PicIdx; // negative value to skip tl0PicIdx
  int8_t mKeyIdx; // negative value to skip keyIdx
};

union GMPCodecSpecificInfoUnion
{
  GMPCodecSpecificInfoGeneric mGeneric;
  GMPCodecSpecificInfoVP8 mVP8;
  GMPCodecSpecificInfoH264 mH264;
};

// Note: if any pointers are added to this struct or its sub-structs, it
// must be fitted with a copy-constructor. This is because it is copied
// in the copy-constructor of VCMEncodedFrame.
struct GMPCodecSpecificInfo
{
  GMPVideoCodecType mCodecType;
  GMPBufferType mBufferType;
  GMPCodecSpecificInfoUnion mCodecSpecific;
};

#endif // GMP_VIDEO_CODEC_h_

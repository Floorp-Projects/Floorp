/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>

#include "mozilla/EndianUtils.h"
#include <stdint.h>
#include <algorithm>
#include <opus/opus.h>

#include "OggCodecState.h"
#include "OpusDecoder.h"
#include "OpusParser.h"
#include "VideoUtils.h"
#include "XiphExtradata.h"
#include "nsDebug.h"
#include "opus/opus_multistream.h"

namespace mozilla {

extern LazyLogModule gMediaDecoderLog;
#define LOG(type, msg) MOZ_LOG(gMediaDecoderLog, type, msg)

using media::TimeUnit;

/** Decoder base class for Ogg-encapsulated streams. */
OggCodecState* OggCodecState::Create(ogg_page* aPage) {
  NS_ASSERTION(ogg_page_bos(aPage), "Only call on BOS page!");
  nsAutoPtr<OggCodecState> codecState;
  if (aPage->body_len > 6 && memcmp(aPage->body + 1, "theora", 6) == 0) {
    codecState = new TheoraState(aPage);
  } else if (aPage->body_len > 6 && memcmp(aPage->body + 1, "vorbis", 6) == 0) {
    codecState = new VorbisState(aPage);
  } else if (aPage->body_len > 8 && memcmp(aPage->body, "OpusHead", 8) == 0) {
    codecState = new OpusState(aPage);
  } else if (aPage->body_len > 8 && memcmp(aPage->body, "fishead\0", 8) == 0) {
    codecState = new SkeletonState(aPage);
  } else if (aPage->body_len > 5 && memcmp(aPage->body, "\177FLAC", 5) == 0) {
    codecState = new FlacState(aPage);
  } else {
    codecState = new OggCodecState(aPage, false);
  }
  return codecState->OggCodecState::InternalInit() ? codecState.forget()
                                                   : nullptr;
}

OggCodecState::OggCodecState(ogg_page* aBosPage, bool aActive)
    : mPacketCount(0),
      mSerial(ogg_page_serialno(aBosPage)),
      mActive(aActive),
      mDoneReadingHeaders(!aActive) {
  MOZ_COUNT_CTOR(OggCodecState);
  memset(&mState, 0, sizeof(ogg_stream_state));
}

OggCodecState::~OggCodecState() {
  MOZ_COUNT_DTOR(OggCodecState);
  Reset();
#ifdef DEBUG
  int ret =
#endif
      ogg_stream_clear(&mState);
  NS_ASSERTION(ret == 0, "ogg_stream_clear failed");
}

nsresult OggCodecState::Reset() {
  if (ogg_stream_reset(&mState) != 0) {
    return NS_ERROR_FAILURE;
  }
  mPackets.Erase();
  ClearUnstamped();
  return NS_OK;
}

void OggCodecState::ClearUnstamped() { mUnstamped.Clear(); }

bool OggCodecState::InternalInit() {
  int ret = ogg_stream_init(&mState, mSerial);
  return ret == 0;
}

bool OggCodecState::IsValidVorbisTagName(nsCString& aName) {
  // Tag names must consist of ASCII 0x20 through 0x7D,
  // excluding 0x3D '=' which is the separator.
  uint32_t length = aName.Length();
  const char* data = aName.Data();
  for (uint32_t i = 0; i < length; i++) {
    if (data[i] < 0x20 || data[i] > 0x7D || data[i] == '=') {
      return false;
    }
  }
  return true;
}

bool OggCodecState::AddVorbisComment(UniquePtr<MetadataTags>& aTags,
                                     const char* aComment, uint32_t aLength) {
  const char* div = (const char*)memchr(aComment, '=', aLength);
  if (!div) {
    LOG(LogLevel::Debug, ("Skipping comment: no separator"));
    return false;
  }
  nsCString key = nsCString(aComment, div - aComment);
  if (!IsValidVorbisTagName(key)) {
    LOG(LogLevel::Debug, ("Skipping comment: invalid tag name"));
    return false;
  }
  uint32_t valueLength = aLength - (div - aComment);
  nsCString value = nsCString(div + 1, valueLength);
  if (!IsUTF8(value)) {
    LOG(LogLevel::Debug, ("Skipping comment: invalid UTF-8 in value"));
    return false;
  }
  aTags->Put(key, value);
  return true;
}

bool OggCodecState::SetCodecSpecificConfig(MediaByteBuffer* aBuffer,
                                           OggPacketQueue& aHeaders) {
  nsTArray<const unsigned char*> headers;
  nsTArray<size_t> headerLens;
  for (size_t i = 0; i < aHeaders.Length(); i++) {
    headers.AppendElement(aHeaders[i]->packet);
    headerLens.AppendElement(aHeaders[i]->bytes);
  }
  // Save header packets for the decoder
  if (!XiphHeadersToExtradata(aBuffer, headers, headerLens)) {
    return false;
  }
  aHeaders.Erase();
  return true;
}

void VorbisState::RecordVorbisPacketSamples(ogg_packet* aPacket,
                                            long aSamples) {
#ifdef VALIDATE_VORBIS_SAMPLE_CALCULATION
  mVorbisPacketSamples[aPacket] = aSamples;
#endif
}

void VorbisState::ValidateVorbisPacketSamples(ogg_packet* aPacket,
                                              long aSamples) {
#ifdef VALIDATE_VORBIS_SAMPLE_CALCULATION
  NS_ASSERTION(mVorbisPacketSamples[aPacket] == aSamples,
               "Decoded samples for Vorbis packet don't match expected!");
  mVorbisPacketSamples.erase(aPacket);
#endif
}

void VorbisState::AssertHasRecordedPacketSamples(ogg_packet* aPacket) {
#ifdef VALIDATE_VORBIS_SAMPLE_CALCULATION
  NS_ASSERTION(mVorbisPacketSamples.count(aPacket) == 1,
               "Must have recorded packet samples");
#endif
}

static OggPacketPtr Clone(ogg_packet* aPacket) {
  ogg_packet* p = new ogg_packet();
  memcpy(p, aPacket, sizeof(ogg_packet));
  p->packet = new unsigned char[p->bytes];
  memcpy(p->packet, aPacket->packet, p->bytes);
  return OggPacketPtr(p);
}

void OggPacketQueue::Append(OggPacketPtr aPacket) {
  nsDeque::Push(aPacket.release());
}

bool OggCodecState::IsPacketReady() { return !mPackets.IsEmpty(); }

OggPacketPtr OggCodecState::PacketOut() {
  if (mPackets.IsEmpty()) {
    return nullptr;
  }
  return mPackets.PopFront();
}

ogg_packet* OggCodecState::PacketPeek() {
  if (mPackets.IsEmpty()) {
    return nullptr;
  }
  return mPackets.PeekFront();
}

void OggCodecState::PushFront(OggPacketQueue&& aOther) {
  while (!aOther.IsEmpty()) {
    mPackets.PushFront(aOther.Pop());
  }
}

already_AddRefed<MediaRawData> OggCodecState::PacketOutAsMediaRawData() {
  OggPacketPtr packet = PacketOut();
  if (!packet) {
    return nullptr;
  }

  NS_ASSERTION(
      !IsHeader(packet.get()),
      "PacketOutAsMediaRawData can only be called on non-header packets");
  RefPtr<MediaRawData> sample = new MediaRawData(packet->packet, packet->bytes);
  if (packet->bytes && !sample->Data()) {
    // OOM.
    return nullptr;
  }

  int64_t end_tstamp = Time(packet->granulepos);
  NS_ASSERTION(end_tstamp >= 0, "timestamp invalid");

  int64_t duration = PacketDuration(packet.get());
  NS_ASSERTION(duration >= 0, "duration invalid");

  sample->mTimecode = TimeUnit::FromMicroseconds(packet->granulepos);
  sample->mTime = TimeUnit::FromMicroseconds(end_tstamp - duration);
  sample->mDuration = TimeUnit::FromMicroseconds(duration);
  sample->mKeyframe = IsKeyframe(packet.get());
  sample->mEOS = packet->e_o_s;

  return sample.forget();
}

nsresult OggCodecState::PageIn(ogg_page* aPage) {
  if (!mActive) {
    return NS_OK;
  }
  NS_ASSERTION(static_cast<uint32_t>(ogg_page_serialno(aPage)) == mSerial,
               "Page must be for this stream!");
  if (ogg_stream_pagein(&mState, aPage) == -1) {
    return NS_ERROR_FAILURE;
  }
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

nsresult OggCodecState::PacketOutUntilGranulepos(bool& aFoundGranulepos) {
  int r;
  aFoundGranulepos = false;
  // Extract packets from the sync state until either no more packets
  // come out, or we get a data packet with non -1 granulepos.
  do {
    ogg_packet packet;
    r = ogg_stream_packetout(&mState, &packet);
    if (r == 1) {
      OggPacketPtr clone = Clone(&packet);
      if (IsHeader(&packet)) {
        // Header packets go straight into the packet queue.
        mPackets.Append(std::move(clone));
      } else {
        // We buffer data packets until we encounter a granulepos. We'll
        // then use the granulepos to figure out the granulepos of the
        // preceeding packets.
        mUnstamped.AppendElement(std::move(clone));
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

TheoraState::TheoraState(ogg_page* aBosPage)
    : OggCodecState(aBosPage, true), mSetup(0), mCtx(0) {
  MOZ_COUNT_CTOR(TheoraState);
  th_info_init(&mTheoraInfo);
  th_comment_init(&mComment);
}

TheoraState::~TheoraState() {
  MOZ_COUNT_DTOR(TheoraState);
  th_setup_free(mSetup);
  th_decode_free(mCtx);
  th_comment_clear(&mComment);
  th_info_clear(&mTheoraInfo);
  Reset();
}

bool TheoraState::Init() {
  if (!mActive) {
    return false;
  }

  int64_t n = mTheoraInfo.aspect_numerator;
  int64_t d = mTheoraInfo.aspect_denominator;

  float aspectRatio =
      (n == 0 || d == 0) ? 1.0f : static_cast<float>(n) / static_cast<float>(d);

  // Ensure the frame and picture regions aren't larger than our prescribed
  // maximum, or zero sized.
  gfx::IntSize frame(mTheoraInfo.frame_width, mTheoraInfo.frame_height);
  gfx::IntRect picture(mTheoraInfo.pic_x, mTheoraInfo.pic_y,
                       mTheoraInfo.pic_width, mTheoraInfo.pic_height);
  gfx::IntSize display(mTheoraInfo.pic_width, mTheoraInfo.pic_height);
  ScaleDisplayByAspectRatio(display, aspectRatio);
  if (!IsValidVideoRegion(frame, picture, display)) {
    return mActive = false;
  }

  mCtx = th_decode_alloc(&mTheoraInfo, mSetup);
  if (!mCtx) {
    return mActive = false;
  }

  // Video track's frame sizes will not overflow. Activate the video track.
  mInfo.mMimeType = NS_LITERAL_CSTRING("video/theora");
  mInfo.mDisplay = display;
  mInfo.mImage = frame;
  mInfo.SetImageRect(picture);

  return mActive = SetCodecSpecificConfig(mInfo.mCodecSpecificConfig, mHeaders);
}

nsresult TheoraState::Reset() {
  mHeaders.Erase();
  return OggCodecState::Reset();
}

bool TheoraState::DecodeHeader(OggPacketPtr aPacket) {
  ogg_packet* packet = aPacket.get();  // Will be owned by mHeaders.
  mHeaders.Append(std::move(aPacket));
  mPacketCount++;
  int ret = th_decode_headerin(&mTheoraInfo, &mComment, &mSetup, packet);

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
  bool isSetupHeader = packet->bytes > 0 && packet->packet[0] == 0x82;
  if (ret < 0 || mPacketCount > 3) {
    // We've received an error, or the first three packets weren't valid
    // header packets. Assume bad input.
    // Our caller will deactivate the bitstream.
    return false;
  } else if (ret > 0 && isSetupHeader && mPacketCount == 3) {
    // Successfully read the three header packets.
    mDoneReadingHeaders = true;
  }
  return true;
}

int64_t TheoraState::Time(int64_t granulepos) {
  if (!mActive) {
    return -1;
  }
  return TheoraState::Time(&mTheoraInfo, granulepos);
}

bool TheoraState::IsHeader(ogg_packet* aPacket) {
  return th_packet_isheader(aPacket);
}

#define TH_VERSION_CHECK(_info, _maj, _min, _sub)                            \
  (((_info)->version_major > (_maj) || (_info)->version_major == (_maj)) &&  \
   (((_info)->version_minor > (_min) || (_info)->version_minor == (_min)) && \
    (_info)->version_subminor >= (_sub)))

int64_t TheoraState::Time(th_info* aInfo, int64_t aGranulepos) {
  if (aGranulepos < 0 || aInfo->fps_numerator == 0) {
    return -1;
  }
  // Implementation of th_granule_frame inlined here to operate
  // on the th_info structure instead of the theora_state.
  int shift = aInfo->keyframe_granule_shift;
  ogg_int64_t iframe = aGranulepos >> shift;
  ogg_int64_t pframe = aGranulepos - (iframe << shift);
  int64_t frameno = iframe + pframe - TH_VERSION_CHECK(aInfo, 3, 2, 1);
  CheckedInt64 t =
      ((CheckedInt64(frameno) + 1) * USECS_PER_S) * aInfo->fps_denominator;
  if (!t.isValid()) {
    return -1;
  }
  t /= aInfo->fps_numerator;
  return t.isValid() ? t.value() : -1;
}

int64_t TheoraState::StartTime(int64_t granulepos) {
  if (granulepos < 0 || !mActive || mTheoraInfo.fps_numerator == 0) {
    return -1;
  }
  CheckedInt64 t =
      (CheckedInt64(th_granule_frame(mCtx, granulepos)) * USECS_PER_S) *
      mTheoraInfo.fps_denominator;
  if (!t.isValid()) {
    return -1;
  }
  return t.value() / mTheoraInfo.fps_numerator;
}

int64_t TheoraState::PacketDuration(ogg_packet* aPacket) {
  if (!mActive || mTheoraInfo.fps_numerator == 0) {
    return -1;
  }
  CheckedInt64 t = SaferMultDiv(mTheoraInfo.fps_denominator, USECS_PER_S,
                                mTheoraInfo.fps_numerator);
  return t.isValid() ? t.value() : -1;
}

int64_t TheoraState::MaxKeyframeOffset() {
  // Determine the maximum time in microseconds by which a key frame could
  // offset for the theora bitstream. Theora granulepos encode time as:
  // ((key_frame_number << granule_shift) + frame_offset).
  // Therefore the maximum possible time by which any frame could be offset
  // from a keyframe is the duration of (1 << granule_shift) - 1) frames.
  int64_t frameDuration;

  // Max number of frames keyframe could possibly be offset.
  int64_t keyframeDiff = (1 << mTheoraInfo.keyframe_granule_shift) - 1;

  // Length of frame in usecs.
  frameDuration =
      (mTheoraInfo.fps_denominator * USECS_PER_S) / mTheoraInfo.fps_numerator;

  // Total time in usecs keyframe can be offset from any given frame.
  return frameDuration * keyframeDiff;
}

bool TheoraState::IsKeyframe(ogg_packet* pkt) {
  // first bit of packet is 1 for header, 0 for data
  // second bit of packet is 1 for inter frame, 0 for intra frame
  return (pkt->bytes >= 1 && (pkt->packet[0] & 0x40) == 0x00);
}

nsresult TheoraState::PageIn(ogg_page* aPage) {
  if (!mActive) return NS_OK;
  NS_ASSERTION(static_cast<uint32_t>(ogg_page_serialno(aPage)) == mSerial,
               "Page must be for this stream!");
  if (ogg_stream_pagein(&mState, aPage) == -1) return NS_ERROR_FAILURE;
  bool foundGp;
  nsresult res = PacketOutUntilGranulepos(foundGp);
  if (NS_FAILED(res)) return res;
  if (foundGp && mDoneReadingHeaders) {
    // We've found a packet with a granulepos, and we've loaded our metadata
    // and initialized our decoder. Determine granulepos of buffered packets.
    ReconstructTheoraGranulepos();
    for (uint32_t i = 0; i < mUnstamped.Length(); ++i) {
      OggPacketPtr packet = std::move(mUnstamped[i]);
#ifdef DEBUG
      NS_ASSERTION(!IsHeader(packet.get()),
                   "Don't try to recover header packet gp");
      NS_ASSERTION(packet->granulepos != -1, "Packet must have gp by now");
#endif
      mPackets.Append(std::move(packet));
    }
    mUnstamped.Clear();
  }
  return NS_OK;
}

// Returns 1 if the Theora info struct is decoding a media of Theora
// version (maj,min,sub) or later, otherwise returns 0.
int TheoraVersion(th_info* info, unsigned char maj, unsigned char min,
                  unsigned char sub) {
  ogg_uint32_t ver = (maj << 16) + (min << 8) + sub;
  ogg_uint32_t th_ver = (info->version_major << 16) +
                        (info->version_minor << 8) + info->version_subminor;
  return (th_ver >= ver) ? 1 : 0;
}

void TheoraState::ReconstructTheoraGranulepos() {
  if (mUnstamped.Length() == 0) {
    return;
  }
  ogg_int64_t lastGranulepos = mUnstamped[mUnstamped.Length() - 1]->granulepos;
  NS_ASSERTION(lastGranulepos != -1, "Must know last granulepos");

  // Reconstruct the granulepos (and thus timestamps) of the decoded
  // frames. Granulepos are stored as ((keyframe<<shift)+offset). We
  // know the granulepos of the last frame in the list, so we can infer
  // the granulepos of the intermediate frames using their frame numbers.
  ogg_int64_t shift = mTheoraInfo.keyframe_granule_shift;
  ogg_int64_t version_3_2_1 = TheoraVersion(&mTheoraInfo, 3, 2, 1);
  ogg_int64_t lastFrame =
      th_granule_frame(mCtx, lastGranulepos) + version_3_2_1;
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
  for (uint32_t i = 0; i < mUnstamped.Length() - 1; ++i) {
    ogg_int64_t frame = firstFrame + i;
    ogg_int64_t granulepos;
    auto& packet = mUnstamped[i];
    bool isKeyframe = th_packet_iskeyframe(packet.get()) == 1;

    if (isKeyframe) {
      granulepos = frame << shift;
      keyframe = frame;
    } else if (frame >= keyframe &&
               frame - keyframe < ((ogg_int64_t)1 << shift)) {
      // (frame - keyframe) won't overflow the "offset" segment of the
      // granulepos, so it's safe to calculate the granulepos.
      granulepos = (keyframe << shift) + (frame - keyframe);
    } else {
      // (frame - keyframeno) will overflow the "offset" segment of the
      // granulepos, so we take "keyframe" to be the max possible offset
      // frame instead.
      ogg_int64_t k =
          std::max(frame - (((ogg_int64_t)1 << shift) - 1), version_3_2_1);
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
    NS_ASSERTION(
        i == 0 || th_granule_frame(mCtx, granulepos) ==
                      th_granule_frame(mCtx, mUnstamped[i - 1]->granulepos) + 1,
        "Granulepos calculation is incorrect!");

    packet->granulepos = granulepos;
  }

  // Check that the second to last frame's granule number is one less than
  // the last frame's (the known granule number). If not our granulepos
  // recovery missed a beat.
  NS_ASSERTION(mUnstamped.Length() < 2 ||
                   (th_granule_frame(
                        mCtx, mUnstamped[mUnstamped.Length() - 2]->granulepos) +
                    1) == th_granule_frame(mCtx, lastGranulepos),
               "Granulepos recovery should catch up with packet->granulepos!");
}

nsresult VorbisState::Reset() {
  nsresult res = NS_OK;
  if (mActive && vorbis_synthesis_restart(&mDsp) != 0) {
    res = NS_ERROR_FAILURE;
  }
  mHeaders.Erase();
  if (NS_FAILED(OggCodecState::Reset())) {
    return NS_ERROR_FAILURE;
  }

  mGranulepos = 0;
  mPrevVorbisBlockSize = 0;

  return res;
}

VorbisState::VorbisState(ogg_page* aBosPage)
    : OggCodecState(aBosPage, true), mPrevVorbisBlockSize(0), mGranulepos(0) {
  MOZ_COUNT_CTOR(VorbisState);
  vorbis_info_init(&mVorbisInfo);
  vorbis_comment_init(&mComment);
  memset(&mDsp, 0, sizeof(vorbis_dsp_state));
  memset(&mBlock, 0, sizeof(vorbis_block));
}

VorbisState::~VorbisState() {
  MOZ_COUNT_DTOR(VorbisState);
  Reset();
  vorbis_block_clear(&mBlock);
  vorbis_dsp_clear(&mDsp);
  vorbis_info_clear(&mVorbisInfo);
  vorbis_comment_clear(&mComment);
}

bool VorbisState::DecodeHeader(OggPacketPtr aPacket) {
  ogg_packet* packet = aPacket.get();  // Will be owned by mHeaders.
  mHeaders.Append(std::move(aPacket));
  mPacketCount++;
  int ret = vorbis_synthesis_headerin(&mVorbisInfo, &mComment, packet);
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

  bool isSetupHeader = packet->bytes > 0 && packet->packet[0] == 0x5;

  if (ret < 0 || mPacketCount > 3) {
    // We've received an error, or the first three packets weren't valid
    // header packets. Assume bad input. Our caller will deactivate the
    // bitstream.
    return false;
  } else if (!ret && isSetupHeader && mPacketCount == 3) {
    // Successfully read the three header packets.
    // The bitstream remains active.
    mDoneReadingHeaders = true;
  }

  return true;
}

bool VorbisState::Init() {
  if (!mActive) {
    return false;
  }

  int ret = vorbis_synthesis_init(&mDsp, &mVorbisInfo);
  if (ret != 0) {
    NS_WARNING("vorbis_synthesis_init() failed initializing vorbis bitstream");
    return mActive = false;
  }
  ret = vorbis_block_init(&mDsp, &mBlock);
  if (ret != 0) {
    NS_WARNING("vorbis_block_init() failed initializing vorbis bitstream");
    if (mActive) {
      vorbis_dsp_clear(&mDsp);
    }
    return mActive = false;
  }

  nsTArray<const unsigned char*> headers;
  nsTArray<size_t> headerLens;
  for (size_t i = 0; i < mHeaders.Length(); i++) {
    headers.AppendElement(mHeaders[i]->packet);
    headerLens.AppendElement(mHeaders[i]->bytes);
  }
  // Save header packets for the decoder
  if (!XiphHeadersToExtradata(mInfo.mCodecSpecificConfig, headers,
                              headerLens)) {
    return mActive = false;
  }
  mHeaders.Erase();
  mInfo.mMimeType = NS_LITERAL_CSTRING("audio/vorbis");
  mInfo.mRate = mVorbisInfo.rate;
  mInfo.mChannels = mVorbisInfo.channels;
  mInfo.mBitDepth = 16;

  return true;
}

int64_t VorbisState::Time(int64_t granulepos) {
  if (!mActive) {
    return -1;
  }

  return VorbisState::Time(&mVorbisInfo, granulepos);
}

int64_t VorbisState::Time(vorbis_info* aInfo, int64_t aGranulepos) {
  if (aGranulepos == -1 || aInfo->rate == 0) {
    return -1;
  }
  CheckedInt64 t = SaferMultDiv(aGranulepos, USECS_PER_S, aInfo->rate);
  return t.isValid() ? t.value() : 0;
}

int64_t VorbisState::PacketDuration(ogg_packet* aPacket) {
  if (!mActive) {
    return -1;
  }
  if (aPacket->granulepos == -1) {
    return -1;
  }
  // @FIXME store these in a more stable place
  if (mVorbisPacketSamples.count(aPacket) == 0) {
    // We haven't seen this packet, don't know its size?
    return -1;
  }

  long samples = mVorbisPacketSamples[aPacket];
  return Time(samples);
}

bool VorbisState::IsHeader(ogg_packet* aPacket) {
  // The first byte in each Vorbis header packet is either 0x01, 0x03, or 0x05,
  // i.e. the first bit is odd. Audio data packets have their first bit as 0x0.
  // Any packet with its first bit set cannot be a data packet, it's a
  // (possibly invalid) header packet.
  // See: http://xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-610004.2.1
  return aPacket->bytes > 0 ? (aPacket->packet[0] & 0x1) : false;
}

UniquePtr<MetadataTags> VorbisState::GetTags() {
  NS_ASSERTION(mComment.user_comments, "no vorbis comment strings!");
  NS_ASSERTION(mComment.comment_lengths, "no vorbis comment lengths!");
  auto tags = MakeUnique<MetadataTags>();
  for (int i = 0; i < mComment.comments; i++) {
    AddVorbisComment(tags, mComment.user_comments[i],
                     mComment.comment_lengths[i]);
  }
  return tags;
}

nsresult VorbisState::PageIn(ogg_page* aPage) {
  if (!mActive) {
    return NS_OK;
  }
  NS_ASSERTION(static_cast<uint32_t>(ogg_page_serialno(aPage)) == mSerial,
               "Page must be for this stream!");
  if (ogg_stream_pagein(&mState, aPage) == -1) return NS_ERROR_FAILURE;
  bool foundGp;
  nsresult res = PacketOutUntilGranulepos(foundGp);
  if (NS_FAILED(res)) {
    return res;
  }
  if (foundGp && mDoneReadingHeaders) {
    // We've found a packet with a granulepos, and we've loaded our metadata
    // and initialized our decoder. Determine granulepos of buffered packets.
    ReconstructVorbisGranulepos();
    for (uint32_t i = 0; i < mUnstamped.Length(); ++i) {
      OggPacketPtr packet = std::move(mUnstamped[i]);
      AssertHasRecordedPacketSamples(packet.get());
      NS_ASSERTION(!IsHeader(packet.get()),
                   "Don't try to recover header packet gp");
      NS_ASSERTION(packet->granulepos != -1, "Packet must have gp by now");
      mPackets.Append(std::move(packet));
    }
    mUnstamped.Clear();
  }
  return NS_OK;
}

nsresult VorbisState::ReconstructVorbisGranulepos() {
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
  auto& last = mUnstamped.LastElement();
  NS_ASSERTION(last->e_o_s || last->granulepos >= 0,
               "Must know last granulepos!");
  if (mUnstamped.Length() == 1) {
    auto& packet = mUnstamped[0];
    long blockSize = vorbis_packet_blocksize(&mVorbisInfo, packet.get());
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
    RecordVorbisPacketSamples(packet.get(), samples);
    return NS_OK;
  }

  bool unknownGranulepos = last->granulepos == -1;
  int totalSamples = 0;
  for (int32_t i = mUnstamped.Length() - 1; i > 0; i--) {
    auto& packet = mUnstamped[i];
    auto& prev = mUnstamped[i - 1];
    ogg_int64_t granulepos = packet->granulepos;
    NS_ASSERTION(granulepos != -1, "Must know granulepos!");
    long prevBlockSize = vorbis_packet_blocksize(&mVorbisInfo, prev.get());
    long blockSize = vorbis_packet_blocksize(&mVorbisInfo, packet.get());

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
    RecordVorbisPacketSamples(packet.get(), samples);
  }

  if (unknownGranulepos) {
    for (uint32_t i = 0; i < mUnstamped.Length(); i++) {
      mUnstamped[i]->granulepos += mGranulepos + totalSamples + 1;
    }
  }

  auto& first = mUnstamped[0];
  long blockSize = vorbis_packet_blocksize(&mVorbisInfo, first.get());
  if (blockSize < 0) {
    mPrevVorbisBlockSize = 0;
    blockSize = 0;
  }

  long samples = (mPrevVorbisBlockSize == 0)
                     ? 0
                     : mPrevVorbisBlockSize / 4 + blockSize / 4;
  int64_t start = first->granulepos - samples;
  RecordVorbisPacketSamples(first.get(), samples);

  if (last->e_o_s && start < mGranulepos) {
    // We've calculated that there are more samples in this page than its
    // granulepos claims, and it's the last page in the stream. This is legal,
    // and we will need to prune the trailing samples when we come to decode it.
    // We must correct the timestamps so that they follow the last Vorbis page's
    // samples.
    int64_t pruned = mGranulepos - start;
    for (uint32_t i = 0; i < mUnstamped.Length() - 1; i++) {
      mUnstamped[i]->granulepos += pruned;
    }
#ifdef VALIDATE_VORBIS_SAMPLE_CALCULATION
    mVorbisPacketSamples[last.get()] -= pruned;
#endif
  }

  mPrevVorbisBlockSize = vorbis_packet_blocksize(&mVorbisInfo, last.get());
  mPrevVorbisBlockSize = std::max(static_cast<long>(0), mPrevVorbisBlockSize);
  mGranulepos = last->granulepos;

  return NS_OK;
}

OpusState::OpusState(ogg_page* aBosPage)
    : OggCodecState(aBosPage, true),
      mParser(nullptr),
      mDecoder(nullptr),
      mPrevPacketGranulepos(0),
      mPrevPageGranulepos(0) {
  MOZ_COUNT_CTOR(OpusState);
}

OpusState::~OpusState() {
  MOZ_COUNT_DTOR(OpusState);
  Reset();

  if (mDecoder) {
    opus_multistream_decoder_destroy(mDecoder);
    mDecoder = nullptr;
  }
}

nsresult OpusState::Reset() { return Reset(false); }

nsresult OpusState::Reset(bool aStart) {
  nsresult res = NS_OK;

  if (mActive && mDecoder) {
    // Reset the decoder.
    opus_multistream_decoder_ctl(mDecoder, OPUS_RESET_STATE);
    // This lets us distinguish the first page being the last page vs. just
    // not having processed the previous page when we encounter the last page.
    mPrevPageGranulepos = aStart ? 0 : -1;
    mPrevPacketGranulepos = aStart ? 0 : -1;
  }

  // Clear queued data.
  if (NS_FAILED(OggCodecState::Reset())) {
    return NS_ERROR_FAILURE;
  }

  LOG(LogLevel::Debug, ("Opus decoder reset"));

  return res;
}

bool OpusState::Init(void) {
  if (!mActive) {
    return false;
  }

  int error;

  NS_ASSERTION(mDecoder == nullptr, "leaking OpusDecoder");

  mDecoder = opus_multistream_decoder_create(
      mParser->mRate, mParser->mChannels, mParser->mStreams,
      mParser->mCoupledStreams, mParser->mMappingTable, &error);

  mInfo.mMimeType = NS_LITERAL_CSTRING("audio/opus");
  mInfo.mRate = mParser->mRate;
  mInfo.mChannels = mParser->mChannels;
  mInfo.mBitDepth = 16;
  // Save preskip & the first header packet for the Opus decoder
  OpusDataDecoder::AppendCodecDelay(mInfo.mCodecSpecificConfig,
                                    Time(0, mParser->mPreSkip));
  if (!mHeaders.PeekFront()) {
    return false;
  }
  mInfo.mCodecSpecificConfig->AppendElements(mHeaders.PeekFront()->packet,
                                             mHeaders.PeekFront()->bytes);
  mHeaders.Erase();
  LOG(LogLevel::Debug, ("Opus decoder init"));

  return error == OPUS_OK;
}

bool OpusState::DecodeHeader(OggPacketPtr aPacket) {
  switch (mPacketCount++) {
    // Parse the id header.
    case 0:
      mParser = new OpusParser;
      if (!mParser->DecodeHeader(aPacket->packet, aPacket->bytes)) {
        return false;
      }
      mHeaders.Append(std::move(aPacket));
      break;

    // Parse the metadata header.
    case 1:
      if (!mParser->DecodeTags(aPacket->packet, aPacket->bytes)) {
        return false;
      }
      break;

    // We made it to the first data packet (which includes reconstructing
    // timestamps for it in PageIn). Success!
    default:
      mDoneReadingHeaders = true;
      // Put it back on the queue so we can decode it.
      mPackets.PushFront(std::move(aPacket));
      break;
  }
  return true;
}

/* Construct and return a tags hashmap from our internal array */
UniquePtr<MetadataTags> OpusState::GetTags() {
  auto tags = MakeUnique<MetadataTags>();
  for (uint32_t i = 0; i < mParser->mTags.Length(); i++) {
    AddVorbisComment(tags, mParser->mTags[i].Data(),
                     mParser->mTags[i].Length());
  }

  return tags;
}

/* Return the timestamp (in microseconds) equivalent to a granulepos. */
int64_t OpusState::Time(int64_t aGranulepos) {
  if (!mActive) {
    return -1;
  }

  return Time(mParser->mPreSkip, aGranulepos);
}

int64_t OpusState::Time(int aPreSkip, int64_t aGranulepos) {
  if (aGranulepos < 0) {
    return -1;
  }

  // Ogg Opus always runs at a granule rate of 48 kHz.
  CheckedInt64 t = SaferMultDiv(aGranulepos - aPreSkip, USECS_PER_S, 48000);
  return t.isValid() ? t.value() : -1;
}

bool OpusState::IsHeader(ogg_packet* aPacket) {
  return aPacket->bytes >= 16 && (!memcmp(aPacket->packet, "OpusHead", 8) ||
                                  !memcmp(aPacket->packet, "OpusTags", 8));
}

nsresult OpusState::PageIn(ogg_page* aPage) {
  if (!mActive) {
    return NS_OK;
  }
  NS_ASSERTION(static_cast<uint32_t>(ogg_page_serialno(aPage)) == mSerial,
               "Page must be for this stream!");
  if (ogg_stream_pagein(&mState, aPage) == -1) return NS_ERROR_FAILURE;

  bool haveGranulepos;
  nsresult rv = PacketOutUntilGranulepos(haveGranulepos);
  if (NS_FAILED(rv) || !haveGranulepos || mPacketCount < 2) {
    return rv;
  }
  if (!ReconstructOpusGranulepos()) {
    return NS_ERROR_FAILURE;
  }
  for (uint32_t i = 0; i < mUnstamped.Length(); i++) {
    OggPacketPtr packet = std::move(mUnstamped[i]);
    NS_ASSERTION(!IsHeader(packet.get()), "Don't try to play a header packet");
    NS_ASSERTION(packet->granulepos != -1, "Packet should have a granulepos");
    mPackets.Append(std::move(packet));
  }
  mUnstamped.Clear();
  return NS_OK;
}

// Helper method to return the change in granule position due to an Opus packet
// (as distinct from the number of samples in the packet, which depends on the
// decoder rate). It should work with a multistream Opus file, and continue to
// work should we ever allow the decoder to decode at a rate other than 48 kHz.
// It even works before we've created the actual Opus decoder.
static int GetOpusDeltaGP(ogg_packet* packet) {
  int nframes;
  nframes = opus_packet_get_nb_frames(packet->packet, packet->bytes);
  if (nframes > 0) {
    return nframes * opus_packet_get_samples_per_frame(packet->packet, 48000);
  }
  NS_WARNING("Invalid Opus packet.");
  return nframes;
}

int64_t OpusState::PacketDuration(ogg_packet* aPacket) {
  CheckedInt64 t = SaferMultDiv(GetOpusDeltaGP(aPacket), USECS_PER_S, 48000);
  return t.isValid() ? t.value() : -1;
}

bool OpusState::ReconstructOpusGranulepos(void) {
  NS_ASSERTION(mUnstamped.Length() > 0, "Must have unstamped packets");
  NS_ASSERTION(mUnstamped.LastElement()->e_o_s ||
                   mUnstamped.LastElement()->granulepos > 0,
               "Must know last granulepos!");
  int64_t gp;
  // If this is the last page, and we've seen at least one previous page (or
  // this is the first page)...
  if (mUnstamped.LastElement()->e_o_s) {
    auto& last = mUnstamped.LastElement();
    if (mPrevPageGranulepos != -1) {
      // If this file only has one page and the final granule position is
      // smaller than the pre-skip amount, we MUST reject the stream.
      if (!mDoneReadingHeaders && last->granulepos < mParser->mPreSkip)
        return false;
      int64_t last_gp = last->granulepos;
      gp = mPrevPageGranulepos;
      // Loop through the packets forwards, adding the current packet's
      // duration to the previous granulepos to get the value for the
      // current packet.
      for (uint32_t i = 0; i < mUnstamped.Length() - 1; ++i) {
        auto& packet = mUnstamped[i];
        int offset = GetOpusDeltaGP(packet.get());
        // Check for error (negative offset) and overflow.
        if (offset >= 0 && gp <= INT64_MAX - offset) {
          gp += offset;
          if (gp >= last_gp) {
            NS_WARNING("Opus end trimming removed more than a full packet.");
            // We were asked to remove a full packet's worth of data or more.
            // Encoders SHOULD NOT produce streams like this, but we'll handle
            // it for them anyway.
            gp = last_gp;
            mUnstamped.RemoveElementsAt(i + 1, mUnstamped.Length() - (i + 1));
            packet->e_o_s = 1;
          }
        }
        packet->granulepos = gp;
      }
      mPrevPageGranulepos = last_gp;
      return true;
    } else {
      NS_WARNING("No previous granule position to use for Opus end trimming.");
      // If we don't have a previous granule position, fall through.
      // We simply won't trim any samples from the end.
      // TODO: Are we guaranteed to have seen a previous page if there is one?
    }
  }

  auto& last = mUnstamped.LastElement();
  gp = last->granulepos;
  // Loop through the packets backwards, subtracting the next
  // packet's duration from its granulepos to get the value
  // for the current packet.
  for (uint32_t i = mUnstamped.Length() - 1; i > 0; i--) {
    int offset = GetOpusDeltaGP(mUnstamped[i].get());
    // Check for error (negative offset) and overflow.
    if (offset >= 0) {
      if (offset <= gp) {
        gp -= offset;
      } else {
        // If the granule position of the first data page is smaller than the
        // number of decodable audio samples on that page, then we MUST reject
        // the stream.
        if (!mDoneReadingHeaders) return false;
        // It's too late to reject the stream.
        // If we get here, this almost certainly means the file has screwed-up
        // timestamps somewhere after the first page.
        NS_WARNING("Clamping negative Opus granulepos to zero.");
        gp = 0;
      }
    }
    mUnstamped[i - 1]->granulepos = gp;
  }

  // Check to make sure the first granule position is at least as large as the
  // total number of samples decodable from the first page with completed
  // packets. This requires looking at the duration of the first packet, too.
  // We MUST reject such streams.
  if (!mDoneReadingHeaders && GetOpusDeltaGP(mUnstamped[0].get()) > gp) {
    return false;
  }
  mPrevPageGranulepos = last->granulepos;
  return true;
}

already_AddRefed<MediaRawData> OpusState::PacketOutAsMediaRawData() {
  ogg_packet* packet = PacketPeek();
  if (!packet) {
    return nullptr;
  }

  uint32_t frames = 0;
  const int64_t endFrame = packet->granulepos;

  if (packet->e_o_s) {
    frames = GetOpusDeltaGP(packet);
  }

  RefPtr<MediaRawData> data = OggCodecState::PacketOutAsMediaRawData();
  if (!data) {
    return nullptr;
  }

  if (data->mEOS && mPrevPacketGranulepos != -1) {
    // If this is the last packet, perform end trimming.
    int64_t startFrame = mPrevPacketGranulepos;
    frames -= std::max<int64_t>(
        0, std::min(endFrame - startFrame, static_cast<int64_t>(frames)));
    data->mDiscardPadding = frames;
  }

  // Save this packet's granule position in case we need to perform end
  // trimming on the next packet.
  mPrevPacketGranulepos = endFrame;

  return data.forget();
}

FlacState::FlacState(ogg_page* aBosPage) : OggCodecState(aBosPage, true) {}

bool FlacState::DecodeHeader(OggPacketPtr aPacket) {
  if (mParser.DecodeHeaderBlock(aPacket->packet, aPacket->bytes).isErr()) {
    return false;
  }
  if (mParser.HasFullMetadata()) {
    mDoneReadingHeaders = true;
  }
  return true;
}

int64_t FlacState::Time(int64_t granulepos) {
  if (!mParser.mInfo.IsValid()) {
    return -1;
  }
  CheckedInt64 t = SaferMultDiv(granulepos, USECS_PER_S, mParser.mInfo.mRate);
  if (!t.isValid()) {
    return -1;
  }
  return t.value();
}

int64_t FlacState::PacketDuration(ogg_packet* aPacket) {
  return mParser.BlockDuration(aPacket->packet, aPacket->bytes);
}

bool FlacState::IsHeader(ogg_packet* aPacket) {
  auto res = mParser.IsHeaderBlock(aPacket->packet, aPacket->bytes);
  return res.isOk() ? res.unwrap() : false;
}

nsresult FlacState::PageIn(ogg_page* aPage) {
  if (!mActive) {
    return NS_OK;
  }
  NS_ASSERTION(static_cast<uint32_t>(ogg_page_serialno(aPage)) == mSerial,
               "Page must be for this stream!");
  if (ogg_stream_pagein(&mState, aPage) == -1) return NS_ERROR_FAILURE;
  bool foundGp;
  nsresult res = PacketOutUntilGranulepos(foundGp);
  if (NS_FAILED(res)) {
    return res;
  }
  if (foundGp && mDoneReadingHeaders) {
    // We've found a packet with a granulepos, and we've loaded our metadata
    // and initialized our decoder. Determine granulepos of buffered packets.
    ReconstructFlacGranulepos();
    for (uint32_t i = 0; i < mUnstamped.Length(); ++i) {
      OggPacketPtr packet = std::move(mUnstamped[i]);
      NS_ASSERTION(!IsHeader(packet.get()),
                   "Don't try to recover header packet gp");
      NS_ASSERTION(packet->granulepos != -1, "Packet must have gp by now");
      mPackets.Append(std::move(packet));
    }
    mUnstamped.Clear();
  }
  return NS_OK;
}

// Return a hash table with tag metadata.
UniquePtr<MetadataTags> FlacState::GetTags() { return mParser.GetTags(); }

const TrackInfo* FlacState::GetInfo() const { return &mParser.mInfo; }

bool FlacState::ReconstructFlacGranulepos(void) {
  NS_ASSERTION(mUnstamped.Length() > 0, "Must have unstamped packets");
  auto& last = mUnstamped.LastElement();
  NS_ASSERTION(last->e_o_s || last->granulepos > 0,
               "Must know last granulepos!");
  int64_t gp;

  gp = last->granulepos;
  // Loop through the packets backwards, subtracting the next
  // packet's duration from its granulepos to get the value
  // for the current packet.
  for (uint32_t i = mUnstamped.Length() - 1; i > 0; i--) {
    int offset =
        mParser.BlockDuration(mUnstamped[i]->packet, mUnstamped[i]->bytes);
    // Check for error (negative offset) and overflow.
    if (offset >= 0) {
      if (offset <= gp) {
        gp -= offset;
      } else {
        // If the granule position of the first data page is smaller than the
        // number of decodable audio samples on that page, then we MUST reject
        // the stream.
        if (!mDoneReadingHeaders) {
          return false;
        }
        // It's too late to reject the stream.
        // If we get here, this almost certainly means the file has screwed-up
        // timestamps somewhere after the first page.
        NS_WARNING("Clamping negative granulepos to zero.");
        gp = 0;
      }
    }
    mUnstamped[i - 1]->granulepos = gp;
  }

  return true;
}

SkeletonState::SkeletonState(ogg_page* aBosPage)
    : OggCodecState(aBosPage, true),
      mVersion(0),
      mPresentationTime(0),
      mLength(0) {
  MOZ_COUNT_CTOR(SkeletonState);
}

SkeletonState::~SkeletonState() { MOZ_COUNT_DTOR(SkeletonState); }

// Support for Ogg Skeleton 4.0, as per specification at:
// http://wiki.xiph.org/Ogg_Skeleton_4

// Minimum length in bytes of a Skeleton header packet.
static const long SKELETON_MIN_HEADER_LEN = 28;
static const long SKELETON_4_0_MIN_HEADER_LEN = 80;

// Minimum length in bytes of a Skeleton 4.0 index packet.
static const long SKELETON_4_0_MIN_INDEX_LEN = 42;

// Minimum length in bytes of a Skeleton 3.0/4.0 Fisbone packet.
static const long SKELETON_MIN_FISBONE_LEN = 52;

// Minimum possible size of a compressed index keypoint.
static const size_t MIN_KEY_POINT_SIZE = 2;

// Byte offset of the major and minor version numbers in the
// Ogg Skeleton 4.0 header packet.
static const size_t SKELETON_VERSION_MAJOR_OFFSET = 8;
static const size_t SKELETON_VERSION_MINOR_OFFSET = 10;

// Byte-offsets of the presentation time numerator and denominator
static const size_t SKELETON_PRESENTATION_TIME_NUMERATOR_OFFSET = 12;
static const size_t SKELETON_PRESENTATION_TIME_DENOMINATOR_OFFSET = 20;

// Byte-offsets of the length of file field in the Skeleton 4.0 header packet.
static const size_t SKELETON_FILE_LENGTH_OFFSET = 64;

// Byte-offsets of the fields in the Skeleton index packet.
static const size_t INDEX_SERIALNO_OFFSET = 6;
static const size_t INDEX_NUM_KEYPOINTS_OFFSET = 10;
static const size_t INDEX_TIME_DENOM_OFFSET = 18;
static const size_t INDEX_FIRST_NUMER_OFFSET = 26;
static const size_t INDEX_LAST_NUMER_OFFSET = 34;
static const size_t INDEX_KEYPOINT_OFFSET = 42;

// Byte-offsets of the fields in the Skeleton Fisbone packet.
static const size_t FISBONE_MSG_FIELDS_OFFSET = 8;
static const size_t FISBONE_SERIALNO_OFFSET = 12;

static bool IsSkeletonBOS(ogg_packet* aPacket) {
  static_assert(SKELETON_MIN_HEADER_LEN >= 8,
                "Minimum length of skeleton BOS header incorrect");
  return aPacket->bytes >= SKELETON_MIN_HEADER_LEN &&
         memcmp(reinterpret_cast<char*>(aPacket->packet), "fishead", 8) == 0;
}

static bool IsSkeletonIndex(ogg_packet* aPacket) {
  static_assert(SKELETON_4_0_MIN_INDEX_LEN >= 5,
                "Minimum length of skeleton index header incorrect");
  return aPacket->bytes >= SKELETON_4_0_MIN_INDEX_LEN &&
         memcmp(reinterpret_cast<char*>(aPacket->packet), "index", 5) == 0;
}

static bool IsSkeletonFisbone(ogg_packet* aPacket) {
  static_assert(SKELETON_MIN_FISBONE_LEN >= 8,
                "Minimum length of skeleton fisbone header incorrect");
  return aPacket->bytes >= SKELETON_MIN_FISBONE_LEN &&
         memcmp(reinterpret_cast<char*>(aPacket->packet), "fisbone", 8) == 0;
}

// Reads a variable length encoded integer at p. Will not read
// past aLimit. Returns pointer to character after end of integer.
static const unsigned char* ReadVariableLengthInt(const unsigned char* p,
                                                  const unsigned char* aLimit,
                                                  int64_t& n) {
  int shift = 0;
  int64_t byte = 0;
  n = 0;
  while (p < aLimit && (byte & 0x80) != 0x80 && shift < 57) {
    byte = static_cast<int64_t>(*p);
    n |= ((byte & 0x7f) << shift);
    shift += 7;
    p++;
  }
  return p;
}

bool SkeletonState::DecodeIndex(ogg_packet* aPacket) {
  NS_ASSERTION(aPacket->bytes >= SKELETON_4_0_MIN_INDEX_LEN,
               "Index must be at least minimum size");
  if (!mActive) {
    return false;
  }

  uint32_t serialno =
      LittleEndian::readUint32(aPacket->packet + INDEX_SERIALNO_OFFSET);
  int64_t numKeyPoints =
      LittleEndian::readInt64(aPacket->packet + INDEX_NUM_KEYPOINTS_OFFSET);

  int64_t endTime = 0, startTime = 0;
  const unsigned char* p = aPacket->packet;

  int64_t timeDenom =
      LittleEndian::readInt64(aPacket->packet + INDEX_TIME_DENOM_OFFSET);
  if (timeDenom == 0) {
    LOG(LogLevel::Debug, ("Ogg Skeleton Index packet for stream %u has 0 "
                          "timestamp denominator.",
                          serialno));
    return (mActive = false);
  }

  // Extract the start time.
  int64_t timeRawInt = LittleEndian::readInt64(p + INDEX_FIRST_NUMER_OFFSET);
  CheckedInt64 t = SaferMultDiv(timeRawInt, USECS_PER_S, timeDenom);
  if (!t.isValid()) {
    return (mActive = false);
  } else {
    startTime = t.value();
  }

  // Extract the end time.
  timeRawInt = LittleEndian::readInt64(p + INDEX_LAST_NUMER_OFFSET);
  t = SaferMultDiv(timeRawInt, USECS_PER_S, timeDenom);
  if (!t.isValid()) {
    return (mActive = false);
  } else {
    endTime = t.value();
  }

  // Check the numKeyPoints value read, ensure we're not going to run out of
  // memory while trying to decode the index packet.
  CheckedInt64 minPacketSize =
      (CheckedInt64(numKeyPoints) * MIN_KEY_POINT_SIZE) + INDEX_KEYPOINT_OFFSET;
  if (!minPacketSize.isValid()) {
    return (mActive = false);
  }

  int64_t sizeofIndex = aPacket->bytes - INDEX_KEYPOINT_OFFSET;
  int64_t maxNumKeyPoints = sizeofIndex / MIN_KEY_POINT_SIZE;
  if (aPacket->bytes < minPacketSize.value() ||
      numKeyPoints > maxNumKeyPoints || numKeyPoints < 0) {
    // Packet size is less than the theoretical minimum size, or the packet is
    // claiming to store more keypoints than it's capable of storing. This means
    // that the numKeyPoints field is too large or small for the packet to
    // possibly contain as many packets as it claims to, so the numKeyPoints
    // field is possibly malicious. Don't try decoding this index, we may run
    // out of memory.
    LOG(LogLevel::Debug, ("Possibly malicious number of key points reported "
                          "(%" PRId64 ") in index packet for stream %u.",
                          numKeyPoints, serialno));
    return (mActive = false);
  }

  nsAutoPtr<nsKeyFrameIndex> keyPoints(new nsKeyFrameIndex(startTime, endTime));

  p = aPacket->packet + INDEX_KEYPOINT_OFFSET;
  const unsigned char* limit = aPacket->packet + aPacket->bytes;
  int64_t numKeyPointsRead = 0;
  CheckedInt64 offset = 0;
  CheckedInt64 time = 0;
  while (p < limit && numKeyPointsRead < numKeyPoints) {
    int64_t delta = 0;
    p = ReadVariableLengthInt(p, limit, delta);
    offset += delta;
    if (p == limit || !offset.isValid() || offset.value() > mLength ||
        offset.value() < 0) {
      return (mActive = false);
    }
    p = ReadVariableLengthInt(p, limit, delta);
    time += delta;
    if (!time.isValid() || time.value() > endTime || time.value() < startTime) {
      return (mActive = false);
    }
    CheckedInt64 timeUsecs = SaferMultDiv(time.value(), USECS_PER_S, timeDenom);
    if (!timeUsecs.isValid()) {
      return (mActive = false);
    }
    keyPoints->Add(offset.value(), timeUsecs.value());
    numKeyPointsRead++;
  }

  int32_t keyPointsRead = keyPoints->Length();
  if (keyPointsRead > 0) {
    mIndex.Put(serialno, keyPoints.forget());
  }

  LOG(LogLevel::Debug, ("Loaded %d keypoints for Skeleton on stream %u",
                        keyPointsRead, serialno));
  return true;
}

nsresult SkeletonState::IndexedSeekTargetForTrack(uint32_t aSerialno,
                                                  int64_t aTarget,
                                                  nsKeyPoint& aResult) {
  nsKeyFrameIndex* index = nullptr;
  mIndex.Get(aSerialno, &index);

  if (!index || index->Length() == 0 || aTarget < index->mStartTime ||
      aTarget > index->mEndTime) {
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

nsresult SkeletonState::IndexedSeekTarget(int64_t aTarget,
                                          nsTArray<uint32_t>& aTracks,
                                          nsSeekTarget& aResult) {
  if (!mActive || mVersion < SKELETON_VERSION(4, 0)) {
    return NS_ERROR_FAILURE;
  }
  // Loop over all requested tracks' indexes, and get the keypoint for that
  // seek target. Record the keypoint with the lowest offset, this will be
  // our seek result. User must seek to the one with lowest offset to ensure we
  // pass "keyframes" on all tracks when we decode forwards to the seek target.
  nsSeekTarget r;
  for (uint32_t i = 0; i < aTracks.Length(); i++) {
    nsKeyPoint k;
    if (NS_SUCCEEDED(IndexedSeekTargetForTrack(aTracks[i], aTarget, k)) &&
        k.mOffset < r.mKeyPoint.mOffset) {
      r.mKeyPoint = k;
      r.mSerial = aTracks[i];
    }
  }
  if (r.IsNull()) {
    return NS_ERROR_FAILURE;
  }
  LOG(LogLevel::Debug,
      ("Indexed seek target for time %" PRId64 " is offset %" PRId64, aTarget,
       r.mKeyPoint.mOffset));
  aResult = r;
  return NS_OK;
}

nsresult SkeletonState::GetDuration(const nsTArray<uint32_t>& aTracks,
                                    int64_t& aDuration) {
  if (!mActive || mVersion < SKELETON_VERSION(4, 0) || !HasIndex() ||
      aTracks.Length() == 0) {
    return NS_ERROR_FAILURE;
  }
  int64_t endTime = INT64_MIN;
  int64_t startTime = INT64_MAX;
  for (uint32_t i = 0; i < aTracks.Length(); i++) {
    nsKeyFrameIndex* index = nullptr;
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
  CheckedInt64 duration = CheckedInt64(endTime) - startTime;
  aDuration = duration.isValid() ? duration.value() : 0;
  return duration.isValid() ? NS_OK : NS_ERROR_FAILURE;
}

bool SkeletonState::DecodeFisbone(ogg_packet* aPacket) {
  if (aPacket->bytes < static_cast<long>(FISBONE_MSG_FIELDS_OFFSET + 4)) {
    return false;
  }
  uint32_t offsetMsgField =
      LittleEndian::readUint32(aPacket->packet + FISBONE_MSG_FIELDS_OFFSET);

  if (aPacket->bytes < static_cast<long>(FISBONE_SERIALNO_OFFSET + 4)) {
    return false;
  }
  uint32_t serialno =
      LittleEndian::readUint32(aPacket->packet + FISBONE_SERIALNO_OFFSET);

  CheckedUint32 checked_fields_pos =
      CheckedUint32(FISBONE_MSG_FIELDS_OFFSET) + offsetMsgField;
  if (!checked_fields_pos.isValid() ||
      aPacket->bytes < static_cast<int64_t>(checked_fields_pos.value())) {
    return false;
  }
  int64_t msgLength = aPacket->bytes - checked_fields_pos.value();
  char* msgProbe = (char*)aPacket->packet + checked_fields_pos.value();
  char* msgHead = msgProbe;
  nsAutoPtr<MessageField> field(new MessageField());

  const static FieldPatternType kFieldTypeMaps[] = {
      {"Content-Type:", eContentType},
      {"Role:", eRole},
      {"Name:", eName},
      {"Language:", eLanguage},
      {"Title:", eTitle},
      {"Display-hint:", eDisplayHint},
      {"Altitude:", eAltitude},
      {"TrackOrder:", eTrackOrder},
      {"Track dependencies:", eTrackDependencies}};

  bool isContentTypeParsed = false;
  while (msgLength > 1) {
    if (*msgProbe == '\r' && *(msgProbe + 1) == '\n') {
      nsAutoCString strMsg(msgHead, msgProbe - msgHead);
      for (size_t i = 0; i < ArrayLength(kFieldTypeMaps); i++) {
        if (strMsg.Find(kFieldTypeMaps[i].mPatternToRecognize) != -1) {
          // The content of message header fields follows [RFC2822], and the
          // mandatory message field must be encoded in US-ASCII, others
          // must be be encoded in UTF-8. "Content-Type" must come first
          // for all of message header fields.
          // See
          // http://svn.annodex.net/standards/draft-pfeiffer-oggskeleton-current.txt.
          if (i != 0 && !isContentTypeParsed) {
            return false;
          }

          if ((i == 0 && IsASCII(strMsg)) || (i != 0 && IsUTF8(strMsg))) {
            EMsgHeaderType eHeaderType = kFieldTypeMaps[i].mMsgHeaderType;
            field->mValuesStore.LookupForAdd(eHeaderType)
                .OrInsert([i, msgHead, msgProbe]() {
                  uint32_t nameLen =
                      strlen(kFieldTypeMaps[i].mPatternToRecognize);
                  return new nsCString(msgHead + nameLen,
                                       msgProbe - msgHead - nameLen);
                });
            isContentTypeParsed = i == 0 ? true : isContentTypeParsed;
          }
          break;
        }
      }
      msgProbe += 2;
      msgLength -= 2;
      msgHead = msgProbe;
      continue;
    }
    msgLength--;
    msgProbe++;
  }

  auto entry = mMsgFieldStore.LookupForAdd(serialno);
  if (entry) {
    // mMsgFieldStore has an entry for serialno already.
    return false;
  }
  entry.OrInsert([&field]() { return field.forget(); });
  return true;
}

bool SkeletonState::DecodeHeader(OggPacketPtr aPacket) {
  if (IsSkeletonBOS(aPacket.get())) {
    uint16_t verMajor = LittleEndian::readUint16(aPacket->packet +
                                                 SKELETON_VERSION_MAJOR_OFFSET);
    uint16_t verMinor = LittleEndian::readUint16(aPacket->packet +
                                                 SKELETON_VERSION_MINOR_OFFSET);

    // Read the presentation time. We read this before the version check as the
    // presentation time exists in all versions.
    int64_t n = LittleEndian::readInt64(
        aPacket->packet + SKELETON_PRESENTATION_TIME_NUMERATOR_OFFSET);
    int64_t d = LittleEndian::readInt64(
        aPacket->packet + SKELETON_PRESENTATION_TIME_DENOMINATOR_OFFSET);
    mPresentationTime =
        d == 0 ? 0
               : (static_cast<float>(n) / static_cast<float>(d)) * USECS_PER_S;

    mVersion = SKELETON_VERSION(verMajor, verMinor);
    // We can only care to parse Skeleton version 4.0+.
    if (mVersion < SKELETON_VERSION(4, 0) ||
        mVersion >= SKELETON_VERSION(5, 0) ||
        aPacket->bytes < SKELETON_4_0_MIN_HEADER_LEN) {
      return false;
    }

    // Extract the segment length.
    mLength =
        LittleEndian::readInt64(aPacket->packet + SKELETON_FILE_LENGTH_OFFSET);

    LOG(LogLevel::Debug, ("Skeleton segment length: %" PRId64, mLength));

    // Initialize the serialno-to-index map.
    return true;
  } else if (IsSkeletonIndex(aPacket.get()) &&
             mVersion >= SKELETON_VERSION(4, 0)) {
    return DecodeIndex(aPacket.get());
  } else if (IsSkeletonFisbone(aPacket.get())) {
    return DecodeFisbone(aPacket.get());
  } else if (aPacket->e_o_s) {
    mDoneReadingHeaders = true;
    return true;
  }
  return true;
}

#undef LOG

}  // namespace mozilla

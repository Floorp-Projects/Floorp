/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "SoftwareWebMVideoDecoder.h"
#include "AbstractMediaDecoder.h"
#include "gfx2DGlue.h"
#include "MediaDecoderStateMachine.h"
#include "MediaResource.h"
#include "mozilla/dom/TimeRanges.h"
#include "nsError.h"
#include "OggReader.h"
#include "VorbisUtils.h"
#include "WebMBufferedParser.h"

#include <algorithm>

#define VPX_DONT_DEFINE_STDINT_TYPES
#include "vpx/vp8dx.h"
#include "vpx/vpx_decoder.h"

namespace mozilla {

using namespace gfx;
using namespace layers;

SoftwareWebMVideoDecoder::SoftwareWebMVideoDecoder(WebMReader* aReader)
  : WebMVideoDecoder(),
    mReader(aReader)
{
  MOZ_COUNT_CTOR(SoftwareWebMVideoDecoder);
  memset(&mVPX, 0, sizeof(vpx_codec_ctx_t));
}

SoftwareWebMVideoDecoder::~SoftwareWebMVideoDecoder()
{
  MOZ_COUNT_DTOR(SoftwareWebMVideoDecoder);
}

void
SoftwareWebMVideoDecoder::Shutdown()
{
  vpx_codec_destroy(&mVPX);
  mReader = nullptr;
}

/* static */
WebMVideoDecoder*
SoftwareWebMVideoDecoder::Create(WebMReader* aReader)
{
  return new SoftwareWebMVideoDecoder(aReader);
}

nsresult
SoftwareWebMVideoDecoder::Init(unsigned int aWidth, unsigned int aHeight)
{
  vpx_codec_iface_t* dx = nullptr;
  switch(mReader->GetVideoCodec()) {
    case NESTEGG_CODEC_VP8:
      dx = vpx_codec_vp8_dx();
      break;
    case NESTEGG_CODEC_VP9:
      dx = vpx_codec_vp9_dx();
      break;
  }
  if (!dx || vpx_codec_dec_init(&mVPX, dx, nullptr, 0)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

bool
SoftwareWebMVideoDecoder::DecodeVideoFrame(bool &aKeyframeSkip,
                                           int64_t aTimeThreshold)
{
  MOZ_ASSERT(mReader->OnTaskQueue());

  // Record number of frames decoded and parsed. Automatically update the
  // stats counters using the AutoNotifyDecoded stack-based class.
  AbstractMediaDecoder::AutoNotifyDecoded a(mReader->GetDecoder());

  nsAutoRef<NesteggPacketHolder> holder(mReader->NextPacket(WebMReader::VIDEO));
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
  nsAutoRef<NesteggPacketHolder> next_holder(mReader->NextPacket(WebMReader::VIDEO));
  if (next_holder) {
    r = nestegg_packet_tstamp(next_holder->mPacket, &next_tstamp);
    if (r == -1) {
      return false;
    }
    mReader->PushVideoPacket(next_holder.disown());
  } else {
    next_tstamp = tstamp;
    next_tstamp += tstamp - mReader->GetLastVideoFrameTime();
  }
  mReader->SetLastVideoFrameTime(tstamp);

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
    if (mReader->GetVideoCodec() == NESTEGG_CODEC_VP8) {
      vpx_codec_peek_stream_info(vpx_codec_vp8_dx(), data, length, &si);
    } else if (mReader->GetVideoCodec() == NESTEGG_CODEC_VP9) {
      vpx_codec_peek_stream_info(vpx_codec_vp9_dx(), data, length, &si);
    }
    if (aKeyframeSkip && (!si.is_kf || tstamp_usecs < aTimeThreshold)) {
      // Skipping to next keyframe...
      a.mParsed++; // Assume 1 frame per chunk.
      a.mDropped++;
      continue;
    }

    if (aKeyframeSkip && si.is_kf) {
      aKeyframeSkip = false;
    }

    if (vpx_codec_decode(&mVPX, data, length, nullptr, 0)) {
      return false;
    }

    // If the timestamp of the video frame is less than
    // the time threshold required then it is not added
    // to the video queue and won't be displayed.
    if (tstamp_usecs < aTimeThreshold) {
      a.mParsed++; // Assume 1 frame per chunk.
      a.mDropped++;
      continue;
    }

    vpx_codec_iter_t  iter = nullptr;
    vpx_image_t      *img;

    while ((img = vpx_codec_get_frame(&mVPX, &iter))) {
      NS_ASSERTION(img->fmt == VPX_IMG_FMT_I420, "WebM image format not I420");

      // Chroma shifts are rounded down as per the decoding examples in the SDK
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

      nsIntRect pictureRect = mReader->GetPicture();
      IntRect picture = pictureRect;
      nsIntSize initFrame = mReader->GetInitialFrame();
      if (img->d_w != static_cast<uint32_t>(initFrame.width) ||
          img->d_h != static_cast<uint32_t>(initFrame.height)) {
        // Frame size is different from what the container reports. This is
        // legal in WebM, and we will preserve the ratio of the crop rectangle
        // as it was reported relative to the picture size reported by the
        // container.
        picture.x = (pictureRect.x * img->d_w) / initFrame.width;
        picture.y = (pictureRect.y * img->d_h) / initFrame.height;
        picture.width = (img->d_w * pictureRect.width) / initFrame.width;
        picture.height = (img->d_h * pictureRect.height) / initFrame.height;
      }

      VideoInfo videoInfo = mReader->GetMediaInfo().mVideo;
      nsRefPtr<VideoData> v = VideoData::Create(videoInfo,
                                                mReader->GetDecoder()->GetImageContainer(),
                                                holder->mOffset,
                                                tstamp_usecs,
                                                (next_tstamp / NS_PER_USEC) - tstamp_usecs,
                                                b,
                                                si.is_kf,
                                                -1,
                                                picture);
      if (!v) {
        return false;
      }
      a.mParsed++;
      a.mDecoded++;
      NS_ASSERTION(a.mDecoded <= a.mParsed,
        "Expect only 1 frame per chunk per packet in WebM...");
      mReader->VideoQueue().Push(v);
    }
  }

  return true;
}

} // namespace mozilla

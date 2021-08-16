/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DAV1DDecoder.h"

#include "gfxUtils.h"
#include "ImageContainer.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/gfx/gfxVars.h"
#include "nsThreadUtils.h"
#include "VideoUtils.h"

#undef LOG
#define LOG(arg, ...)                                                  \
  DDMOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, "::%s: " arg, __func__, \
            ##__VA_ARGS__)

namespace mozilla {

DAV1DDecoder::DAV1DDecoder(const CreateDecoderParams& aParams)
    : mInfo(aParams.VideoConfig()),
      mTaskQueue(
          new TaskQueue(GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER),
                        "Dav1dDecoder")),
      mImageContainer(aParams.mImageContainer),
      mImageAllocator(aParams.mKnowsCompositor) {}

RefPtr<MediaDataDecoder::InitPromise> DAV1DDecoder::Init() {
  Dav1dSettings settings;
  dav1d_default_settings(&settings);
  size_t decoder_threads = 2;
  if (mInfo.mDisplay.width >= 2048) {
    decoder_threads = 8;
  } else if (mInfo.mDisplay.width >= 1024) {
    decoder_threads = 4;
  }
  settings.n_frame_threads =
      static_cast<int>(std::min(decoder_threads, GetNumberOfProcessors()));
  // There is not much improvement with more than 2 tile threads at least with
  // the content being currently served. The ideal number of tile thread would
  // much the tile count of the content. Maybe dav1d can help to do that in the
  // future.
  settings.n_tile_threads = 2;

  int res = dav1d_open(&mContext, &settings);
  if (res < 0) {
    return DAV1DDecoder::InitPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Couldn't get dAV1d decoder interface.")),
        __func__);
  }
  return DAV1DDecoder::InitPromise::CreateAndResolve(TrackInfo::kVideoTrack,
                                                     __func__);
}

RefPtr<MediaDataDecoder::DecodePromise> DAV1DDecoder::Decode(
    MediaRawData* aSample) {
  return InvokeAsync<MediaRawData*>(mTaskQueue, this, __func__,
                                    &DAV1DDecoder::InvokeDecode, aSample);
}

void ReleaseDataBuffer_s(const uint8_t* buf, void* user_data) {
  MOZ_ASSERT(user_data);
  MOZ_ASSERT(buf);
  DAV1DDecoder* d = static_cast<DAV1DDecoder*>(user_data);
  d->ReleaseDataBuffer(buf);
}

void DAV1DDecoder::ReleaseDataBuffer(const uint8_t* buf) {
  // The release callback may be called on a different thread defined by the
  // third party dav1d execution. In that case post a task into TaskQueue to
  // ensure that mDecodingBuffers is only ever accessed on the TaskQueue.
  RefPtr<DAV1DDecoder> self = this;
  auto releaseBuffer = [self, buf] {
    MOZ_ASSERT(self->mTaskQueue->IsCurrentThreadIn());
    DebugOnly<bool> found = self->mDecodingBuffers.Remove(buf);
    MOZ_ASSERT(found);
  };

  if (mTaskQueue->IsCurrentThreadIn()) {
    releaseBuffer();
  } else {
    nsresult rv = mTaskQueue->Dispatch(NS_NewRunnableFunction(
        "DAV1DDecoder::ReleaseDataBuffer", std::move(releaseBuffer)));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }
}

RefPtr<MediaDataDecoder::DecodePromise> DAV1DDecoder::InvokeDecode(
    MediaRawData* aSample) {
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  MOZ_ASSERT(aSample);

  // Add the buffer to the hashtable in order to increase
  // the ref counter and keep it alive. When dav1d does not
  // need it any more will call it's release callback. Remove
  // the buffer, in there, to reduce the ref counter and eventually
  // free it. We need a hashtable and not an array because the
  // release callback are not coming in the same order that the
  // buffers have been added in the decoder (threading ordering
  // inside decoder)
  mDecodingBuffers.InsertOrUpdate(aSample->Data(), RefPtr{aSample});
  Dav1dData data;
  int res = dav1d_data_wrap(&data, aSample->Data(), aSample->Size(),
                            ReleaseDataBuffer_s, this);
  data.m.timestamp = aSample->mTimecode.ToMicroseconds();
  data.m.duration = aSample->mDuration.ToMicroseconds();
  data.m.offset = aSample->mOffset;

  if (res < 0) {
    LOG("Create decoder data error.");
    return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__), __func__);
  }
  DecodedData results;
  do {
    res = dav1d_send_data(mContext, &data);
    if (res < 0 && res != -EAGAIN) {
      LOG("Decode error: %d", res);
      return DecodePromise::CreateAndReject(
          MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__), __func__);
    }
    // Alway consume the whole buffer on success.
    // At this point only -EAGAIN error is expected.
    MOZ_ASSERT((res == 0 && !data.sz) ||
               (res == -EAGAIN && data.sz == aSample->Size()));

    MediaResult rs(NS_OK);
    res = GetPicture(results, rs);
    if (res < 0) {
      if (res == -EAGAIN) {
        // No frames ready to return. This is not an
        // error, in some circumstances, we need to
        // feed it with a certain amount of frames
        // before we get a picture.
        continue;
      }
      return DecodePromise::CreateAndReject(rs, __func__);
    }
  } while (data.sz > 0);

  return DecodePromise::CreateAndResolve(std::move(results), __func__);
}

int DAV1DDecoder::GetPicture(DecodedData& aData, MediaResult& aResult) {
  class Dav1dPictureWrapper {
   public:
    Dav1dPicture* operator&() { return &p; }
    const Dav1dPicture& operator*() const { return p; }
    ~Dav1dPictureWrapper() { dav1d_picture_unref(&p); }

   private:
    Dav1dPicture p = Dav1dPicture();
  };
  Dav1dPictureWrapper picture;

  int res = dav1d_get_picture(mContext, &picture);
  if (res < 0) {
    LOG("Decode error: %d", res);
    aResult = MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
    return res;
  }

  if ((*picture).p.layout == DAV1D_PIXEL_LAYOUT_I400) {
    return 0;
  }

#ifdef ANDROID
  if (!gfxVars::UseWebRender() && (*picture).p.bpc != 8) {
    aResult = MediaResult(NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR, __func__);
    return -1;
  }
#endif

  RefPtr<VideoData> v = ConstructImage(*picture);
  if (!v) {
    LOG("Image allocation error: %ux%u"
        " display %ux%u picture %ux%u",
        (*picture).p.w, (*picture).p.h, mInfo.mDisplay.width,
        mInfo.mDisplay.height, mInfo.mImage.width, mInfo.mImage.height);
    aResult = MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__);
    return -1;
  }
  aData.AppendElement(std::move(v));
  return 0;
}

// When returning Nothing(), the caller chooses the appropriate default
/* static */ Maybe<gfx::YUVColorSpace> DAV1DDecoder::GetColorSpace(
    const Dav1dPicture& aPicture, LazyLogModule& aLogger) {
  if (!aPicture.seq_hdr || !aPicture.seq_hdr->color_description_present) {
    return Nothing();
  }

  return gfxUtils::CicpToColorSpace(
      static_cast<gfx::CICP::MatrixCoefficients>(aPicture.seq_hdr->mtrx),
      static_cast<gfx::CICP::ColourPrimaries>(aPicture.seq_hdr->pri), aLogger);
}

already_AddRefed<VideoData> DAV1DDecoder::ConstructImage(
    const Dav1dPicture& aPicture) {
  VideoData::YCbCrBuffer b;
  if (aPicture.p.bpc == 10) {
    b.mColorDepth = gfx::ColorDepth::COLOR_10;
  } else if (aPicture.p.bpc == 12) {
    b.mColorDepth = gfx::ColorDepth::COLOR_12;
  } else {
    b.mColorDepth = gfx::ColorDepth::COLOR_8;
  }

  b.mYUVColorSpace =
      DAV1DDecoder::GetColorSpace(aPicture, sPDMLog)
          .valueOr(DefaultColorSpace({aPicture.p.w, aPicture.p.h}));
  b.mColorRange = aPicture.seq_hdr->color_range ? gfx::ColorRange::FULL
                                                : gfx::ColorRange::LIMITED;

  b.mPlanes[0].mData = static_cast<uint8_t*>(aPicture.data[0]);
  b.mPlanes[0].mStride = aPicture.stride[0];
  b.mPlanes[0].mHeight = aPicture.p.h;
  b.mPlanes[0].mWidth = aPicture.p.w;
  b.mPlanes[0].mSkip = 0;

  b.mPlanes[1].mData = static_cast<uint8_t*>(aPicture.data[1]);
  b.mPlanes[1].mStride = aPicture.stride[1];
  b.mPlanes[1].mSkip = 0;

  b.mPlanes[2].mData = static_cast<uint8_t*>(aPicture.data[2]);
  b.mPlanes[2].mStride = aPicture.stride[1];
  b.mPlanes[2].mSkip = 0;

  // https://code.videolan.org/videolan/dav1d/blob/master/tools/output/yuv.c#L67
  const int ss_ver = aPicture.p.layout == DAV1D_PIXEL_LAYOUT_I420;
  const int ss_hor = aPicture.p.layout != DAV1D_PIXEL_LAYOUT_I444;

  b.mPlanes[1].mHeight = (aPicture.p.h + ss_ver) >> ss_ver;
  b.mPlanes[1].mWidth = (aPicture.p.w + ss_hor) >> ss_hor;

  b.mPlanes[2].mHeight = (aPicture.p.h + ss_ver) >> ss_ver;
  b.mPlanes[2].mWidth = (aPicture.p.w + ss_hor) >> ss_hor;

  // Timestamp, duration and offset used here are wrong.
  // We need to take those values from the decoder. Latest
  // dav1d version allows for that.
  media::TimeUnit timecode =
      media::TimeUnit::FromMicroseconds(aPicture.m.timestamp);
  media::TimeUnit duration =
      media::TimeUnit::FromMicroseconds(aPicture.m.duration);
  int64_t offset = aPicture.m.offset;
  bool keyframe = aPicture.frame_hdr->frame_type == DAV1D_FRAME_TYPE_KEY;

  return VideoData::CreateAndCopyData(
      mInfo, mImageContainer, offset, timecode, duration, b, keyframe, timecode,
      mInfo.ScaledImageRect(aPicture.p.w, aPicture.p.h), mImageAllocator);
}

RefPtr<MediaDataDecoder::DecodePromise> DAV1DDecoder::Drain() {
  RefPtr<DAV1DDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self, this] {
    int res = 0;
    DecodedData results;
    do {
      MediaResult rs(NS_OK);
      res = GetPicture(results, rs);
      if (res < 0 && res != -EAGAIN) {
        return DecodePromise::CreateAndReject(rs, __func__);
      }
    } while (res != -EAGAIN);
    return DecodePromise::CreateAndResolve(std::move(results), __func__);
  });
}

RefPtr<MediaDataDecoder::FlushPromise> DAV1DDecoder::Flush() {
  RefPtr<DAV1DDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self]() {
    dav1d_flush(self->mContext);
    return FlushPromise::CreateAndResolve(true, __func__);
  });
}

RefPtr<ShutdownPromise> DAV1DDecoder::Shutdown() {
  RefPtr<DAV1DDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self]() {
    dav1d_close(&self->mContext);
    return self->mTaskQueue->BeginShutdown();
  });
}

}  // namespace mozilla
#undef LOG

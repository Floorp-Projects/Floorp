/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaCodecProxy.h"
#include <OMX_IVCommon.h>
#include <gui/Surface.h>
#include <ICrypto.h>
#include "GonkVideoDecoderManager.h"
#include "MediaDecoderReader.h"
#include "ImageContainer.h"
#include "VideoUtils.h"
#include "nsThreadUtils.h"
#include "Layers.h"
#include "prlog.h"
#include "stagefright/MediaBuffer.h"
#include "stagefright/MetaData.h"
#include "stagefright/MediaErrors.h"
#include <stagefright/foundation/ADebug.h>
#include <stagefright/foundation/AMessage.h>
#include <stagefright/foundation/AString.h>
#include <stagefright/foundation/ALooper.h>
#include "mp4_demuxer/AnnexB.h"

#define READ_OUTPUT_BUFFER_TIMEOUT_US  3000

#define LOG_TAG "GonkVideoDecoderManager"
#include <android/log.h>
#define ALOG(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

#ifdef PR_LOGGING
PRLogModuleInfo* GetDemuxerLog();
#define LOG(...) PR_LOG(GetDemuxerLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define LOG(...)
#endif
using namespace mozilla::layers;
using namespace android;
typedef android::MediaCodecProxy MediaCodecProxy;

namespace mozilla {
enum {
  kNotifyCodecReserved = 'core',
  kNotifyCodecCanceled = 'coca',
};

GonkVideoDecoderManager::GonkVideoDecoderManager(
                                  mozilla::layers::ImageContainer* aImageContainer,
		                  const mp4_demuxer::VideoDecoderConfig& aConfig)
  : mImageContainer(aImageContainer)
  , mConfig(aConfig)
  , mReaderCallback(nullptr)
  , mColorConverterBufferSize(0)
{
  NS_ASSERTION(!NS_IsMainThread(), "Should not be on main thread.");
  MOZ_ASSERT(mImageContainer);
  MOZ_COUNT_CTOR(GonkVideoDecoderManager);
  mVideoWidth  = aConfig.display_width;
  mVideoHeight = aConfig.display_height;
  mDisplayWidth = aConfig.display_width;
  mDisplayHeight = aConfig.display_width;
  mInfo.mVideo.mHasVideo = true;
  nsIntSize displaySize(mDisplayWidth, mDisplayHeight);
  mInfo.mVideo.mDisplay = displaySize;

  nsIntRect pictureRect(0, 0, mVideoWidth, mVideoHeight);
  nsIntSize frameSize(mVideoWidth, mVideoHeight);
  mPicture = pictureRect;
  mInitialFrame = frameSize;
  mHandler = new MessageHandler(this);
  mVideoListener = new VideoResourceListener(this);

}

GonkVideoDecoderManager::~GonkVideoDecoderManager()
{
  MOZ_COUNT_DTOR(GonkVideoDecoderManager);
}

android::sp<MediaCodecProxy>
GonkVideoDecoderManager::Init(MediaDataDecoderCallback* aCallback)
{
  nsIntSize displaySize(mDisplayWidth, mDisplayHeight);
  nsIntRect pictureRect(0, 0, mVideoWidth, mVideoHeight);
  // Validate the container-reported frame and pictureRect sizes. This ensures
  // that our video frame creation code doesn't overflow.
  nsIntSize frameSize(mVideoWidth, mVideoHeight);
  if (!IsValidVideoRegion(frameSize, pictureRect, displaySize)) {
    ALOG("It is not a valid region");
    return nullptr;
  }

  mReaderCallback = aCallback;

  if (mLooper.get() != nullptr) {
    return nullptr;
  }
  // Create ALooper
  mLooper = new ALooper;
  mLooper->setName("GonkVideoDecoderManager");
  // Register AMessage handler to ALooper.
  mLooper->registerHandler(mHandler);
  // Start ALooper thread.
  if (mLooper->start() != OK) {
    return nullptr;
  }
  mDecoder = MediaCodecProxy::CreateByType(mLooper, "video/avc", false, true, mVideoListener);
  return mDecoder;
}

nsresult
GonkVideoDecoderManager::CreateVideoData(int64_t aStreamOffset, VideoData **v)
{
  *v = nullptr;
  int64_t timeUs;
  int32_t keyFrame;

  if (!(mVideoBuffer != nullptr && mVideoBuffer->data() != nullptr)) {
    ALOG("Video Buffer is not valid!");
    return NS_ERROR_UNEXPECTED;
  }

  if (!mVideoBuffer->meta_data()->findInt64(kKeyTime, &timeUs)) {
    ALOG("Decoder did not return frame time");
    return NS_ERROR_UNEXPECTED;
  }

  if (mVideoBuffer->range_length() == 0) {
    // Some decoders may return spurious empty buffers that we just want to ignore
    // quoted from Android's AwesomePlayer.cpp
    ReleaseVideoBuffer();
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!mVideoBuffer->meta_data()->findInt32(kKeyIsSyncFrame, &keyFrame)) {
    keyFrame = 0;
  }

  gfx::IntRect picture = ToIntRect(mPicture);
  if (mFrameInfo.mWidth != mInitialFrame.width ||
      mFrameInfo.mHeight != mInitialFrame.height) {

    // Frame size is different from what the container reports. This is legal,
    // and we will preserve the ratio of the crop rectangle as it
    // was reported relative to the picture size reported by the container.
    picture.x = (mPicture.x * mFrameInfo.mWidth) / mInitialFrame.width;
    picture.y = (mPicture.y * mFrameInfo.mHeight) / mInitialFrame.height;
    picture.width = (mFrameInfo.mWidth * mPicture.width) / mInitialFrame.width;
    picture.height = (mFrameInfo.mHeight * mPicture.height) / mInitialFrame.height;
  }

  uint8_t *yuv420p_buffer = (uint8_t *)mVideoBuffer->data();
  int32_t stride = mFrameInfo.mStride;
  int32_t slice_height = mFrameInfo.mSliceHeight;

  // Converts to OMX_COLOR_FormatYUV420Planar
  if (mFrameInfo.mColorFormat != OMX_COLOR_FormatYUV420Planar) {
    ARect crop;
    crop.top = 0;
    crop.bottom = mFrameInfo.mHeight;
    crop.left = 0;
    crop.right = mFrameInfo.mWidth;
    yuv420p_buffer = GetColorConverterBuffer(mFrameInfo.mWidth, mFrameInfo.mHeight);
    if (mColorConverter.convertDecoderOutputToI420(mVideoBuffer->data(),
        mFrameInfo.mWidth, mFrameInfo.mHeight, crop, yuv420p_buffer) != OK) {
        ReleaseVideoBuffer();
        ALOG("Color conversion failed!");
        return NS_ERROR_UNEXPECTED;
    }
      stride = mFrameInfo.mWidth;
      slice_height = mFrameInfo.mHeight;
  }

  size_t yuv420p_y_size = stride * slice_height;
  size_t yuv420p_u_size = ((stride + 1) / 2) * ((slice_height + 1) / 2);
  uint8_t *yuv420p_y = yuv420p_buffer;
  uint8_t *yuv420p_u = yuv420p_y + yuv420p_y_size;
  uint8_t *yuv420p_v = yuv420p_u + yuv420p_u_size;

  // This is the approximate byte position in the stream.
  int64_t pos = aStreamOffset;

  VideoData::YCbCrBuffer b;
  b.mPlanes[0].mData = yuv420p_y;
  b.mPlanes[0].mWidth = mFrameInfo.mWidth;
  b.mPlanes[0].mHeight = mFrameInfo.mHeight;
  b.mPlanes[0].mStride = stride;
  b.mPlanes[0].mOffset = 0;
  b.mPlanes[0].mSkip = 0;

  b.mPlanes[1].mData = yuv420p_u;
  b.mPlanes[1].mWidth = (mFrameInfo.mWidth + 1) / 2;
  b.mPlanes[1].mHeight = (mFrameInfo.mHeight + 1) / 2;
  b.mPlanes[1].mStride = (stride + 1) / 2;
  b.mPlanes[1].mOffset = 0;
  b.mPlanes[1].mSkip = 0;

  b.mPlanes[2].mData = yuv420p_v;
  b.mPlanes[2].mWidth =(mFrameInfo.mWidth + 1) / 2;
  b.mPlanes[2].mHeight = (mFrameInfo.mHeight + 1) / 2;
  b.mPlanes[2].mStride = (stride + 1) / 2;
  b.mPlanes[2].mOffset = 0;
  b.mPlanes[2].mSkip = 0;

  *v = VideoData::Create(
      mInfo.mVideo,
      mImageContainer,
      pos,
      timeUs,
      1, // We don't know the duration.
      b,
      keyFrame,
      -1,
      picture);
  ReleaseVideoBuffer();
  return NS_OK;
}

bool
GonkVideoDecoderManager::SetVideoFormat()
{
  // read video metadata from MediaCodec
  sp<AMessage> codecFormat;
  if (mDecoder->getOutputFormat(&codecFormat) == OK) {
    AString mime;
    int32_t width = 0;
    int32_t height = 0;
    int32_t stride = 0;
    int32_t slice_height = 0;
    int32_t color_format = 0;
    int32_t crop_left = 0;
    int32_t crop_top = 0;
    int32_t crop_right = 0;
    int32_t crop_bottom = 0;
    if (!codecFormat->findString("mime", &mime) ||
        !codecFormat->findInt32("width", &width) ||
        !codecFormat->findInt32("height", &height) ||
        !codecFormat->findInt32("stride", &stride) ||
        !codecFormat->findInt32("slice-height", &slice_height) ||
        !codecFormat->findInt32("color-format", &color_format) ||
        !codecFormat->findRect("crop", &crop_left, &crop_top, &crop_right, &crop_bottom)) {
      ALOG("Failed to find values");
      return false;
    }
    mFrameInfo.mWidth = width;
    mFrameInfo.mHeight = height;
    mFrameInfo.mStride = stride;
    mFrameInfo.mSliceHeight = slice_height;
    mFrameInfo.mColorFormat = color_format;

    nsIntSize displaySize(width, height);
    if (!IsValidVideoRegion(mInitialFrame, mPicture, displaySize)) {
      ALOG("It is not a valid region");
      return false;
    }
  }
  return true;
}

// Blocks until decoded sample is produced by the deoder.
nsresult
GonkVideoDecoderManager::Output(int64_t aStreamOffset,
                                nsAutoPtr<MediaData>& aOutData)
{
  aOutData = nullptr;
  status_t err;
  if (mDecoder == nullptr) {
    ALOG("Decoder is not inited");
    return NS_ERROR_UNEXPECTED;
  }
  err = mDecoder->Output(&mVideoBuffer, READ_OUTPUT_BUFFER_TIMEOUT_US);

  switch (err) {
    case OK:
    {
      VideoData* data = nullptr;
      nsresult rv = CreateVideoData(aStreamOffset, &data);
      if (rv == NS_ERROR_NOT_AVAILABLE) {
	// Decoder outputs a empty video buffer, try again
        return NS_ERROR_NOT_AVAILABLE;
      } else if (rv != NS_OK || data == nullptr) {
        ALOG("Failed to create VideoData");
        return NS_ERROR_UNEXPECTED;
      }
      aOutData = data;
      return NS_OK;
    }
    case android::INFO_FORMAT_CHANGED:
    case android::INFO_OUTPUT_BUFFERS_CHANGED:
    {
      // If the format changed, update our cached info.
      ALOG("Decoder format changed");
      if (!SetVideoFormat()) {
        return NS_ERROR_UNEXPECTED;
      }
      else
        return Output(aStreamOffset, aOutData);
    }
    case -EAGAIN:
    {
      return NS_ERROR_NOT_AVAILABLE;
    }
    case android::ERROR_END_OF_STREAM:
    {
      ALOG("Got the EOS frame!");
      VideoData* data = nullptr;
      nsresult rv = CreateVideoData(aStreamOffset, &data);
      if (rv == NS_ERROR_NOT_AVAILABLE) {
	// For EOS, no need to do any thing.
        return NS_ERROR_ABORT;
      }
      if (rv != NS_OK || data == nullptr) {
        ALOG("Failed to create video data");
        return NS_ERROR_UNEXPECTED;
      }
      aOutData = data;
      return NS_ERROR_ABORT;
    }
    case -ETIMEDOUT:
    {
      ALOG("Timeout. can try again next time");
      return NS_ERROR_UNEXPECTED;
    }
    default:
    {
      ALOG("Decoder failed, err=%d", err);
      return NS_ERROR_UNEXPECTED;
    }
  }

  return NS_OK;
}

void GonkVideoDecoderManager::ReleaseVideoBuffer() {
  if (mVideoBuffer) {
    sp<MetaData> metaData = mVideoBuffer->meta_data();
    int32_t index;
    metaData->findInt32(android::MediaCodecProxy::kKeyBufferIndex, &index);
    mVideoBuffer->release();
    mVideoBuffer = nullptr;
    mDecoder->releaseOutputBuffer(index);
  }
}

nsresult
GonkVideoDecoderManager::Input(mp4_demuxer::MP4Sample* aSample)
{
  if (mDecoder == nullptr) {
    ALOG("Decoder is not inited");
    return NS_ERROR_UNEXPECTED;
  }
  status_t rv;
  if (aSample != nullptr) {
    // We must prepare samples in AVC Annex B.
    mp4_demuxer::AnnexB::ConvertSample(aSample, mConfig.annex_b);
    // Forward sample data to the decoder.

    const uint8_t* data = reinterpret_cast<const uint8_t*>(aSample->data);
    uint32_t length = aSample->size;
    rv = mDecoder->Input(data, length, aSample->composition_timestamp, 0);
  }
  else {
    // Inputted data is null, so it is going to notify decoder EOS
    rv = mDecoder->Input(nullptr, 0, 0ll, 0);
  }
  return (rv == OK) ? NS_OK : NS_ERROR_FAILURE;
}

void
GonkVideoDecoderManager::codecReserved()
{
  sp<AMessage> format = new AMessage;
  // Fixed values
  format->setString("mime", "video/avc");
  format->setInt32("width", mVideoWidth);
  format->setInt32("height", mVideoHeight);

  mDecoder->configure(format, nullptr, nullptr, 0);
  mDecoder->Prepare();
  SetVideoFormat();

  if (mHandler != nullptr) {
    // post kNotifyCodecReserved to Looper thread.
    sp<AMessage> notify = new AMessage(kNotifyCodecReserved, mHandler->id());
    notify->post();
  }
}

void
GonkVideoDecoderManager::codecCanceled()
{
  mDecoder = nullptr;
  if (mHandler != nullptr) {
    // post kNotifyCodecCanceled to Looper thread.
    sp<AMessage> notify = new AMessage(kNotifyCodecCanceled, mHandler->id());
    notify->post();
  }

}

// Called on GonkVideoDecoderManager::mLooper thread.
void
GonkVideoDecoderManager::onMessageReceived(const sp<AMessage> &aMessage)
{
  switch (aMessage->what()) {
    case kNotifyCodecReserved:
    {
      // Our decode may have acquired the hardware resource that it needs
      // to start. Notify the state machine to resume loading metadata.
      mReaderCallback->NotifyResourcesStatusChanged();
      break;
    }

    case kNotifyCodecCanceled:
    {
      mReaderCallback->ReleaseMediaResources();
      break;
    }

    default:
      TRESPASS();
      break;
  }
}

GonkVideoDecoderManager::MessageHandler::MessageHandler(GonkVideoDecoderManager *aManager)
  : mManager(aManager)
{
}

GonkVideoDecoderManager::MessageHandler::~MessageHandler()
{
  mManager = nullptr;
}

void
GonkVideoDecoderManager::MessageHandler::onMessageReceived(const android::sp<android::AMessage> &aMessage)
{
  if (mManager != nullptr) {
    mManager->onMessageReceived(aMessage);
  }
}

GonkVideoDecoderManager::VideoResourceListener::VideoResourceListener(GonkVideoDecoderManager *aManager)
  : mManager(aManager)
{
}

GonkVideoDecoderManager::VideoResourceListener::~VideoResourceListener()
{
  mManager = nullptr;
}

void
GonkVideoDecoderManager::VideoResourceListener::codecReserved()
{
  if (mManager != nullptr) {
    mManager->codecReserved();
  }
}

void
GonkVideoDecoderManager::VideoResourceListener::codecCanceled()
{
  if (mManager != nullptr) {
    mManager->codecCanceled();
  }
}

uint8_t *
GonkVideoDecoderManager::GetColorConverterBuffer(int32_t aWidth, int32_t aHeight)
{
  // Allocate a temporary YUV420Planer buffer.
  size_t yuv420p_y_size = aWidth * aHeight;
  size_t yuv420p_u_size = ((aWidth + 1) / 2) * ((aHeight + 1) / 2);
  size_t yuv420p_v_size = yuv420p_u_size;
  size_t yuv420p_size = yuv420p_y_size + yuv420p_u_size + yuv420p_v_size;
  if (mColorConverterBufferSize != yuv420p_size) {
    mColorConverterBuffer = nullptr; // release the previous buffer first
    mColorConverterBuffer = new uint8_t[yuv420p_size];
    mColorConverterBufferSize = yuv420p_size;
  }
  return mColorConverterBuffer.get();
}

} // namespace mozilla

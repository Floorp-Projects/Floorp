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
#include "GonkNativeWindow.h"
#include "GonkNativeWindowClient.h"
#include "mozilla/layers/GrallocTextureClient.h"
#include "mozilla/layers/TextureClient.h"

#define READ_OUTPUT_BUFFER_TIMEOUT_US  3000

#include <android/log.h>
#define GVDM_LOG(...) __android_log_print(ANDROID_LOG_DEBUG, "GonkVideoDecoderManager", __VA_ARGS__)

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

GonkVideoDecoderManager::GonkVideoDecoderManager(
  MediaTaskQueue* aTaskQueue,
  mozilla::layers::ImageContainer* aImageContainer,
  const VideoInfo& aConfig)
  : GonkDecoderManager(aTaskQueue)
  , mImageContainer(aImageContainer)
  , mReaderCallback(nullptr)
  , mColorConverterBufferSize(0)
  , mNativeWindow(nullptr)
  , mPendingVideoBuffersLock("GonkVideoDecoderManager::mPendingVideoBuffersLock")
{
  MOZ_COUNT_CTOR(GonkVideoDecoderManager);
  mMimeType = aConfig.mMimeType;
  mVideoWidth  = aConfig.mDisplay.width;
  mVideoHeight = aConfig.mDisplay.height;
  mDisplayWidth = aConfig.mDisplay.width;
  mDisplayHeight = aConfig.mDisplay.height;
  mInfo.mVideo = aConfig;

  mCodecSpecificData = aConfig.mCodecSpecificConfig;
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
    GVDM_LOG("It is not a valid region");
    return nullptr;
  }

  mReaderCallback = aCallback;

  if (mLooper.get() != nullptr) {
    return nullptr;
  }
  // Create ALooper
  mLooper = new ALooper;
  mManagerLooper = new ALooper;
  mManagerLooper->setName("GonkVideoDecoderManager");
  // Register AMessage handler to ALooper.
  mManagerLooper->registerHandler(mHandler);
  // Start ALooper thread.
  if (mLooper->start() != OK || mManagerLooper->start() != OK ) {
    return nullptr;
  }
  mDecoder = MediaCodecProxy::CreateByType(mLooper, mMimeType.get(), false, mVideoListener);
  mDecoder->AskMediaCodecAndWait();
  uint32_t capability = MediaCodecProxy::kEmptyCapability;
  if (mDecoder->getCapability(&capability) == OK && (capability &
      MediaCodecProxy::kCanExposeGraphicBuffer)) {
    mNativeWindow = new GonkNativeWindow();
  }

  mReaderCallback->NotifyResourcesStatusChanged();

  return mDecoder;
}

void
GonkVideoDecoderManager::QueueFrameTimeIn(int64_t aPTS, int64_t aDuration)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

  FrameTimeInfo timeInfo = {aPTS, aDuration};
  mFrameTimeInfo.AppendElement(timeInfo);
}

nsresult
GonkVideoDecoderManager::QueueFrameTimeOut(int64_t aPTS, int64_t& aDuration)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

  // Set default to 1 here.
  // During seeking, frames could still in MediaCodec and the mFrameTimeInfo could
  // be cleared before these frames are out from MediaCodec. This is ok because
  // these frames are old frame before seeking.
  aDuration = 1;
  for (uint32_t i = 0; i < mFrameTimeInfo.Length(); i++) {
    const FrameTimeInfo& entry = mFrameTimeInfo.ElementAt(i);
    if (i == 0) {
      if (entry.pts > aPTS) {
        // Codec sent a frame with rollbacked PTS time. It could
        // be codec's problem.
        ReleaseVideoBuffer();
        return NS_ERROR_NOT_AVAILABLE;
      }
    }

    // Ideally, the first entry in mFrameTimeInfo should be the one we are looking
    // for. However, MediaCodec could dropped frame and the first entry doesn't
    // match current decoded frame's PTS.
    if (entry.pts == aPTS) {
      aDuration = entry.duration;
      if (i > 0) {
        LOG("Frame could be dropped by MediaCodec, %d dropped frames.", i);
      }
      mFrameTimeInfo.RemoveElementsAt(0, i+1);
      break;
    }
  }
  return NS_OK;
}

nsresult
GonkVideoDecoderManager::CreateVideoData(int64_t aStreamOffset, VideoData **v)
{
  *v = nullptr;
  nsRefPtr<VideoData> data;
  int64_t timeUs;
  int32_t keyFrame;

  if (mVideoBuffer == nullptr) {
    GVDM_LOG("Video Buffer is not valid!");
    return NS_ERROR_UNEXPECTED;
  }

  if (!mVideoBuffer->meta_data()->findInt64(kKeyTime, &timeUs)) {
    GVDM_LOG("Decoder did not return frame time");
    return NS_ERROR_UNEXPECTED;
  }

  int64_t duration;
  nsresult rv = QueueFrameTimeOut(timeUs, duration);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mVideoBuffer->range_length() == 0) {
    // Some decoders may return spurious empty buffers that we just want to ignore
    // quoted from Android's AwesomePlayer.cpp
    ReleaseVideoBuffer();
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!mVideoBuffer->meta_data()->findInt32(kKeyIsSyncFrame, &keyFrame)) {
    keyFrame = 0;
  }

  gfx::IntRect picture = mPicture;
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

  RefPtr<mozilla::layers::TextureClient> textureClient;

  if ((mVideoBuffer->graphicBuffer().get())) {
    textureClient = mNativeWindow->getTextureClientFromBuffer(mVideoBuffer->graphicBuffer().get());
  }

  if (textureClient) {
    GrallocTextureClientOGL* grallocClient = static_cast<GrallocTextureClientOGL*>(textureClient.get());
    grallocClient->SetMediaBuffer(mVideoBuffer);
    textureClient->SetRecycleCallback(GonkVideoDecoderManager::RecycleCallback, this);

    data = VideoData::Create(mInfo.mVideo,
                             mImageContainer,
                             aStreamOffset,
                             timeUs,
                             duration,
                             textureClient,
                             keyFrame,
                             -1,
                             picture);
  } else {
    if (!mVideoBuffer->data()) {
      GVDM_LOG("No data in Video Buffer!");
      return NS_ERROR_UNEXPECTED;
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
          GVDM_LOG("Color conversion failed!");
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

    data = VideoData::Create(
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
  }

  data.forget(v);
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
      GVDM_LOG("Failed to find values");
      return false;
    }
    mFrameInfo.mWidth = width;
    mFrameInfo.mHeight = height;
    mFrameInfo.mStride = stride;
    mFrameInfo.mSliceHeight = slice_height;
    mFrameInfo.mColorFormat = color_format;

    nsIntSize displaySize(width, height);
    if (!IsValidVideoRegion(mInitialFrame, mPicture, displaySize)) {
      GVDM_LOG("It is not a valid region");
      return false;
    }
    return true;
  }
  GVDM_LOG("Fail to get output format");
  return false;
}

// Blocks until decoded sample is produced by the deoder.
nsresult
GonkVideoDecoderManager::Output(int64_t aStreamOffset,
                                nsRefPtr<MediaData>& aOutData)
{
  aOutData = nullptr;
  status_t err;
  if (mDecoder == nullptr) {
    GVDM_LOG("Decoder is not inited");
    return NS_ERROR_UNEXPECTED;
  }
  err = mDecoder->Output(&mVideoBuffer, READ_OUTPUT_BUFFER_TIMEOUT_US);

  switch (err) {
    case OK:
    {
      nsRefPtr<VideoData> data;
      nsresult rv = CreateVideoData(aStreamOffset, getter_AddRefs(data));
      if (rv == NS_ERROR_NOT_AVAILABLE) {
        // Decoder outputs a empty video buffer, try again
        return NS_ERROR_NOT_AVAILABLE;
      } else if (rv != NS_OK || data == nullptr) {
        GVDM_LOG("Failed to create VideoData");
        return NS_ERROR_UNEXPECTED;
      }
      aOutData = data;
      return NS_OK;
    }
    case android::INFO_FORMAT_CHANGED:
    {
      // If the format changed, update our cached info.
      GVDM_LOG("Decoder format changed");
      if (!SetVideoFormat()) {
        return NS_ERROR_UNEXPECTED;
      }
      return Output(aStreamOffset, aOutData);
    }
    case android::INFO_OUTPUT_BUFFERS_CHANGED:
    {
      if (mDecoder->UpdateOutputBuffers()) {
        return Output(aStreamOffset, aOutData);
      }
      GVDM_LOG("Fails to update output buffers!");
      return NS_ERROR_FAILURE;
    }
    case -EAGAIN:
    {
//      GVDM_LOG("Need to try again!");
      return NS_ERROR_NOT_AVAILABLE;
    }
    case android::ERROR_END_OF_STREAM:
    {
      GVDM_LOG("Got the EOS frame!");
      nsRefPtr<VideoData> data;
      nsresult rv = CreateVideoData(aStreamOffset, getter_AddRefs(data));
      if (rv == NS_ERROR_NOT_AVAILABLE) {
        // For EOS, no need to do any thing.
        return NS_ERROR_ABORT;
      }
      if (rv != NS_OK || data == nullptr) {
        GVDM_LOG("Failed to create video data");
        return NS_ERROR_UNEXPECTED;
      }
      aOutData = data;
      return NS_ERROR_ABORT;
    }
    case -ETIMEDOUT:
    {
      GVDM_LOG("Timeout. can try again next time");
      return NS_ERROR_UNEXPECTED;
    }
    default:
    {
      GVDM_LOG("Decoder failed, err=%d", err);
      return NS_ERROR_UNEXPECTED;
    }
  }

  return NS_OK;
}

void GonkVideoDecoderManager::ReleaseVideoBuffer() {
  if (mVideoBuffer) {
    mDecoder->ReleaseMediaBuffer(mVideoBuffer);
    mVideoBuffer = nullptr;
  }
}

status_t
GonkVideoDecoderManager::SendSampleToOMX(MediaRawData* aSample)
{
  // An empty MediaRawData is going to notify EOS to decoder. It doesn't need
  // to keep PTS and duration.
  if (aSample->mData && aSample->mDuration && aSample->mTime) {
    QueueFrameTimeIn(aSample->mTime, aSample->mDuration);
  }

  return mDecoder->Input(reinterpret_cast<const uint8_t*>(aSample->mData),
                         aSample->mSize,
                         aSample->mTime,
                         0);
}

void
GonkVideoDecoderManager::ClearQueueFrameTime()
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  mFrameTimeInfo.Clear();
}

nsresult
GonkVideoDecoderManager::Flush()
{
  GonkDecoderManager::Flush();

  class ClearFrameTimeRunnable : public nsRunnable
  {
  public:
    explicit ClearFrameTimeRunnable(GonkVideoDecoderManager* aManager)
      : mManager(aManager) {}

    NS_IMETHOD Run()
    {
      mManager->ClearQueueFrameTime();
      return NS_OK;
    }

    GonkVideoDecoderManager* mManager;
  };

  mTaskQueue->SyncDispatch(new ClearFrameTimeRunnable(this));

  status_t err = mDecoder->flush();
  if (err != OK) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

void
GonkVideoDecoderManager::codecReserved()
{
  GVDM_LOG("codecReserved");
  sp<AMessage> format = new AMessage;
  sp<Surface> surface;
  status_t rv = OK;
  // Fixed values
  GVDM_LOG("Configure video mime type: %s, widht:%d, height:%d", mMimeType.get(), mVideoWidth, mVideoHeight);
  format->setString("mime", mMimeType.get());
  format->setInt32("width", mVideoWidth);
  format->setInt32("height", mVideoHeight);
  if (mNativeWindow != nullptr) {
    surface = new Surface(mNativeWindow->getBufferQueue());
  }
  mDecoder->configure(format, surface, nullptr, 0);
  mDecoder->Prepare();

  if (mMimeType.EqualsLiteral("video/mp4v-es")) {
    rv = mDecoder->Input(mCodecSpecificData->Elements(),
                         mCodecSpecificData->Length(), 0,
                         android::MediaCodec::BUFFER_FLAG_CODECCONFIG);
  }

  if (rv != OK) {
    GVDM_LOG("Failed to configure codec!!!!");
    mReaderCallback->Error();
  }
}

void
GonkVideoDecoderManager::codecCanceled()
{
  mDecoder = nullptr;
}

// Called on GonkVideoDecoderManager::mManagerLooper thread.
void
GonkVideoDecoderManager::onMessageReceived(const sp<AMessage> &aMessage)
{
  switch (aMessage->what()) {
    case kNotifyPostReleaseBuffer:
    {
      ReleaseAllPendingVideoBuffers();
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

/* static */
void
GonkVideoDecoderManager::RecycleCallback(TextureClient* aClient, void* aClosure)
{
  GonkVideoDecoderManager* videoManager = static_cast<GonkVideoDecoderManager*>(aClosure);
  GrallocTextureClientOGL* client = static_cast<GrallocTextureClientOGL*>(aClient);
  aClient->ClearRecycleCallback();
  videoManager->PostReleaseVideoBuffer(client->GetMediaBuffer());
}

void GonkVideoDecoderManager::PostReleaseVideoBuffer(
                                android::MediaBuffer *aBuffer)
{
  {
    MutexAutoLock autoLock(mPendingVideoBuffersLock);
    if (aBuffer) {
      mPendingVideoBuffers.append(aBuffer);
    }
  }
  sp<AMessage> notify =
            new AMessage(kNotifyPostReleaseBuffer, mHandler->id());
  notify->post();

}

void GonkVideoDecoderManager::ReleaseAllPendingVideoBuffers()
{
  Vector<android::MediaBuffer*> releasingVideoBuffers;
  {
    MutexAutoLock autoLock(mPendingVideoBuffersLock);
    int size = mPendingVideoBuffers.length();
    for (int i = 0; i < size; i++) {
      releasingVideoBuffers.append(mPendingVideoBuffers[i]);
    }
    mPendingVideoBuffers.clear();
  }
  // Free all pending video buffers without holding mPendingVideoBuffersLock.
  int size = releasingVideoBuffers.length();
  for (int i = 0; i < size; i++) {
    android::MediaBuffer *buffer;
    buffer = releasingVideoBuffers[i];
    mDecoder->ReleaseMediaBuffer(buffer);
    buffer = nullptr;
  }
  releasingVideoBuffers.clear();
}

void GonkVideoDecoderManager::ReleaseMediaResources() {
  GVDM_LOG("ReleseMediaResources");
  if (mDecoder == nullptr) {
    return;
  }
  ReleaseAllPendingVideoBuffers();
  mDecoder->ReleaseMediaResources();
}
} // namespace mozilla

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GonkVideoDecoderManager_h_)
#define GonkVideoDecoderManager_h_

#include "nsRect.h"
#include "GonkMediaDataDecoder.h"
#include "mozilla/RefPtr.h"
#include "I420ColorConverterHelper.h"
#include "MediaCodecProxy.h"
#include "GonkNativeWindow.h"
#include "mozilla/layers/FenceUtils.h"
#include "mozilla/UniquePtr.h"
#include <ui/Fence.h>

using namespace android;

namespace android {
class MediaBuffer;
struct MOZ_EXPORT AString;
class GonkNativeWindow;
} // namespace android

namespace mozilla {

namespace layers {
class TextureClient;
class TextureClientRecycleAllocator;
} // namespace mozilla::layers

class GonkVideoDecoderManager : public GonkDecoderManager {
typedef android::MediaCodecProxy MediaCodecProxy;
typedef mozilla::layers::TextureClient TextureClient;

public:
  GonkVideoDecoderManager(mozilla::layers::ImageContainer* aImageContainer,
                          const VideoInfo& aConfig);

  virtual ~GonkVideoDecoderManager();

  RefPtr<InitPromise> Init() override;

  nsresult Output(int64_t aStreamOffset,
                          RefPtr<MediaData>& aOutput) override;

  nsresult Shutdown() override;

  const char* GetDescriptionName() const override
  {
    return "gonk video decoder";
  }

  static void RecycleCallback(TextureClient* aClient, void* aClosure);

protected:
  // Bug 1199809: workaround to avoid sending the graphic buffer by making a
  // copy of output buffer after calling flush(). Bug 1203859 was created to
  // reimplementing Gonk PDM on top of OpenMax IL directly. Its buffer
  // management will work better with Gecko and solve problems like this.
  void ProcessFlush() override
  {
    mNeedsCopyBuffer = true;
    GonkDecoderManager::ProcessFlush();
  }

private:
  struct FrameInfo
  {
    int32_t mWidth = 0;
    int32_t mHeight = 0;
    int32_t mStride = 0;
    int32_t mSliceHeight = 0;
    int32_t mColorFormat = 0;
    int32_t mCropLeft = 0;
    int32_t mCropTop = 0;
    int32_t mCropRight = 0;
    int32_t mCropBottom = 0;
  };

  void onMessageReceived(const android::sp<android::AMessage> &aMessage) override;

  bool SetVideoFormat();

  nsresult CreateVideoData(MediaBuffer* aBuffer, int64_t aStreamOffset, VideoData** aOutData);
  already_AddRefed<VideoData> CreateVideoDataFromGraphicBuffer(android::MediaBuffer* aSource,
                                                               gfx::IntRect& aPicture);
  already_AddRefed<VideoData> CreateVideoDataFromDataBuffer(android::MediaBuffer* aSource,
                                                            gfx::IntRect& aPicture);

  uint8_t* GetColorConverterBuffer(int32_t aWidth, int32_t aHeight);

  // For codec resource management
  void codecReserved();
  void codecCanceled();

  void ReleaseAllPendingVideoBuffers();
  void PostReleaseVideoBuffer(android::MediaBuffer *aBuffer,
                              layers::FenceHandle mReleaseFence);

  VideoInfo mConfig;

  RefPtr<layers::ImageContainer> mImageContainer;
  RefPtr<layers::TextureClientRecycleAllocator> mCopyAllocator;

  MozPromiseRequestHolder<android::MediaCodecProxy::CodecPromise> mVideoCodecRequest;
  FrameInfo mFrameInfo;

  // color converter
  android::I420ColorConverterHelper mColorConverter;
  UniquePtr<uint8_t[]> mColorConverterBuffer;
  size_t mColorConverterBufferSize;

  android::sp<android::GonkNativeWindow> mNativeWindow;
#if ANDROID_VERSION >= 21
  android::sp<android::IGraphicBufferProducer> mGraphicBufferProducer;
#endif

  enum {
    kNotifyPostReleaseBuffer = 'nprb',
  };

  struct ReleaseItem {
    ReleaseItem(android::MediaBuffer* aBuffer, layers::FenceHandle& aFence)
    : mBuffer(aBuffer)
    , mReleaseFence(aFence) {}
    android::MediaBuffer* mBuffer;
    layers::FenceHandle mReleaseFence;
  };
  nsTArray<ReleaseItem> mPendingReleaseItems;

  // The lock protects mPendingReleaseItems.
  Mutex mPendingReleaseItemsLock;

  // This TaskQueue should be the same one in mDecodeCallback->OnReaderTaskQueue().
  // It is for codec resource mangement, decoding task should not dispatch to it.
  RefPtr<TaskQueue> mReaderTaskQueue;

  // Bug 1199809: do we need to make a copy of output buffer? Used only when
  // the decoder outputs graphic buffers.
  bool mNeedsCopyBuffer;
};

} // namespace mozilla

#endif // GonkVideoDecoderManager_h_

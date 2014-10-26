/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GonkVideoDecoderManager_h_)
#define GonkVideoDecoderManager_h_

#include "MP4Reader.h"
#include "nsRect.h"
#include "GonkMediaDataDecoder.h"
#include "mozilla/RefPtr.h"
#include "I420ColorConverterHelper.h"
#include "MediaCodecProxy.h"
#include <stagefright/foundation/AHandler.h>

using namespace android;

namespace android {
struct MOZ_EXPORT ALooper;
class MOZ_EXPORT MediaBuffer;
struct MOZ_EXPORT AString;
} // namespace android

namespace mozilla {

class GonkVideoDecoderManager : public GonkDecoderManager {
typedef android::MediaCodecProxy MediaCodecProxy;

public:
  GonkVideoDecoderManager(mozilla::layers::ImageContainer* aImageContainer,
		          const mp4_demuxer::VideoDecoderConfig& aConfig);

  ~GonkVideoDecoderManager();

  virtual android::sp<MediaCodecProxy> Init(MediaDataDecoderCallback* aCallback) MOZ_OVERRIDE;

  virtual nsresult Input(mp4_demuxer::MP4Sample* aSample) MOZ_OVERRIDE;

  virtual nsresult Output(int64_t aStreamOffset,
                          nsAutoPtr<MediaData>& aOutput) MOZ_OVERRIDE;

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
  class MessageHandler : public android::AHandler
  {
  public:
    MessageHandler(GonkVideoDecoderManager *aManager);
    ~MessageHandler();

    virtual void onMessageReceived(const android::sp<android::AMessage> &aMessage);

  private:
    // Forbidden
    MessageHandler() MOZ_DELETE;
    MessageHandler(const MessageHandler &rhs) MOZ_DELETE;
    const MessageHandler &operator=(const MessageHandler &rhs) MOZ_DELETE;

    GonkVideoDecoderManager *mManager;
  };
  friend class MessageHandler;

  class VideoResourceListener : public android::MediaCodecProxy::CodecResourceListener
  {
  public:
    VideoResourceListener(GonkVideoDecoderManager *aManager);
    ~VideoResourceListener();

    virtual void codecReserved() MOZ_OVERRIDE;
    virtual void codecCanceled() MOZ_OVERRIDE;

  private:
    // Forbidden
    VideoResourceListener() MOZ_DELETE;
    VideoResourceListener(const VideoResourceListener &rhs) MOZ_DELETE;
    const VideoResourceListener &operator=(const VideoResourceListener &rhs) MOZ_DELETE;

    GonkVideoDecoderManager *mManager;
  };
  friend class VideoResourceListener;

  bool SetVideoFormat();

  nsresult CreateVideoData(int64_t aStreamOffset, VideoData** aOutData);
  void ReleaseVideoBuffer();
  uint8_t* GetColorConverterBuffer(int32_t aWidth, int32_t aHeight);

  // For codec resource management
  void codecReserved();
  void codecCanceled();
  void onMessageReceived(const sp<AMessage> &aMessage);

  const mp4_demuxer::VideoDecoderConfig& mConfig;
  uint32_t mVideoWidth;
  uint32_t mVideoHeight;
  uint32_t mDisplayWidth;
  uint32_t mDisplayHeight;
  nsIntRect mPicture;
  nsIntSize mInitialFrame;

  android::sp<MediaCodecProxy> mDecoder;
  nsRefPtr<layers::ImageContainer> mImageContainer;
  MediaDataDecoderCallback* mCallback;

  android::MediaBuffer* mVideoBuffer;

  MediaDataDecoderCallback*  mReaderCallback;
  MediaInfo mInfo;
  android::sp<VideoResourceListener> mVideoListener;
  android::sp<MessageHandler> mHandler;
  android::sp<ALooper> mLooper;
  FrameInfo mFrameInfo;

  // color converter
  android::I420ColorConverterHelper mColorConverter;
  nsAutoArrayPtr<uint8_t> mColorConverterBuffer;
  size_t mColorConverterBufferSize;
};

} // namespace mozilla

#endif // GonkVideoDecoderManager_h_

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcMediaDataDecoderCodec.h"

#include "ImageContainer.h"
#include "Layers.h"
#include "MediaDataDecoderProxy.h"
#include "PDMFactory.h"
#include "VideoUtils.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/media/MediaUtils.h"

namespace mozilla {

WebrtcMediaDataDecoder::WebrtcMediaDataDecoder(nsACString& aCodecMimeType)
    : mThreadPool(GetMediaThreadPool(MediaThreadType::SUPERVISOR)),
      mTaskQueue(TaskQueue::Create(do_AddRef(mThreadPool),
                                   "WebrtcMediaDataDecoder::mTaskQueue")),
      mImageContainer(MakeAndAddRef<layers::ImageContainer>(
          layers::ImageContainer::ASYNCHRONOUS)),
      mFactory(new PDMFactory()),
      mTrackType(TrackInfo::kUndefinedTrack),
      mCodecType(aCodecMimeType) {}

WebrtcMediaDataDecoder::~WebrtcMediaDataDecoder() {}

bool WebrtcMediaDataDecoder::Configure(
    const webrtc::VideoDecoder::Settings& settings) {
  nsCString codec;
  mTrackType = TrackInfo::kVideoTrack;
  mInfo = VideoInfo(settings.max_render_resolution().Width(),
                    settings.max_render_resolution().Height());
  mInfo.mMimeType = mCodecType;

  return WEBRTC_VIDEO_CODEC_OK == CreateDecoder();
}

int32_t WebrtcMediaDataDecoder::Decode(const webrtc::EncodedImage& aInputImage,
                                       bool aMissingFrames,
                                       int64_t aRenderTimeMs) {
  if (!mCallback || !mDecoder) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  if (!aInputImage.data() || !aInputImage.size()) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  // Always start with a complete key frame.
  if (mNeedKeyframe) {
    if (aInputImage._frameType != webrtc::VideoFrameType::kVideoFrameKey)
      return WEBRTC_VIDEO_CODEC_ERROR;
    // We have a key frame - is it complete?
    mNeedKeyframe = false;
  }

  auto disabledHardwareAcceleration =
      MakeScopeExit([&] { mDisabledHardwareAcceleration = true; });

  RefPtr<MediaRawData> compressedFrame =
      new MediaRawData(aInputImage.data(), aInputImage.size());
  if (!compressedFrame->Data()) {
    return WEBRTC_VIDEO_CODEC_MEMORY;
  }

  compressedFrame->mTime =
      media::TimeUnit::FromMicroseconds(aInputImage.Timestamp());
  compressedFrame->mTimecode =
      media::TimeUnit::FromMicroseconds(aRenderTimeMs * 1000);
  compressedFrame->mKeyframe =
      aInputImage._frameType == webrtc::VideoFrameType::kVideoFrameKey;
  {
    media::Await(
        do_AddRef(mThreadPool), mDecoder->Decode(compressedFrame),
        [&](const MediaDataDecoder::DecodedData& aResults) {
          mResults = aResults.Clone();
          mError = NS_OK;
        },
        [&](const MediaResult& aError) { mError = aError; });

    for (auto& frame : mResults) {
      MOZ_ASSERT(frame->mType == MediaData::Type::VIDEO_DATA);
      RefPtr<VideoData> video = frame->As<VideoData>();
      MOZ_ASSERT(video);
      if (!video->mImage) {
        // Nothing to display.
        continue;
      }
      rtc::scoped_refptr<ImageBuffer> image(
          new rtc::RefCountedObject<ImageBuffer>(std::move(video->mImage)));

      auto videoFrame = webrtc::VideoFrame::Builder()
                            .set_video_frame_buffer(image)
                            .set_timestamp_rtp(aInputImage.Timestamp())
                            .set_rotation(aInputImage.rotation_)
                            .build();
      mCallback->Decoded(videoFrame);
    }
    mResults.Clear();
  }

  if (NS_FAILED(mError) && mError != NS_ERROR_DOM_MEDIA_CANCELED) {
    CreateDecoder();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  if (NS_FAILED(mError)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  disabledHardwareAcceleration.release();
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcMediaDataDecoder::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* aCallback) {
  mCallback = aCallback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcMediaDataDecoder::Release() {
  if (mDecoder) {
    RefPtr<MediaDataDecoder> decoder = std::move(mDecoder);
    decoder->Flush()->Then(mTaskQueue, __func__,
                           [decoder]() { decoder->Shutdown(); });
  }

  mNeedKeyframe = true;
  mError = NS_OK;

  return WEBRTC_VIDEO_CODEC_OK;
}

bool WebrtcMediaDataDecoder::OnTaskQueue() const {
  return mTaskQueue->IsOnCurrentThread();
}

int32_t WebrtcMediaDataDecoder::CreateDecoder() {
  RefPtr<layers::KnowsCompositor> knowsCompositor =
      layers::ImageBridgeChild::GetSingleton();

  if (mDecoder) {
    Release();
  }

  RefPtr<TaskQueue> tq =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER),
                        "webrtc decode TaskQueue");
  RefPtr<MediaDataDecoder> decoder;

  media::Await(do_AddRef(mThreadPool), InvokeAsync(tq, __func__, [&] {
                 RefPtr<GenericPromise> p =
                     mFactory
                         ->CreateDecoder(
                             {mInfo,
                              CreateDecoderParams::OptionSet(
                                  CreateDecoderParams::Option::LowLatency,
                                  CreateDecoderParams::Option::FullH264Parsing,
                                  CreateDecoderParams::Option::
                                      ErrorIfNoInitializationData,
                                  mDisabledHardwareAcceleration
                                      ? CreateDecoderParams::Option::
                                            HardwareDecoderNotAllowed
                                      : CreateDecoderParams::Option::Default),
                              mTrackType, mImageContainer, knowsCompositor})
                         ->Then(
                             tq, __func__,
                             [&](RefPtr<MediaDataDecoder>&& aDecoder) {
                               decoder = std::move(aDecoder);
                               return GenericPromise::CreateAndResolve(
                                   true, __func__);
                             },
                             [](const MediaResult& aResult) {
                               return GenericPromise::CreateAndReject(
                                   NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
                             });
                 return p;
               }));

  if (!decoder) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // We need to wrap our decoder in a MediaDataDecoderProxy so that it always
  // run on an nsISerialEventTarget (which the webrtc code doesn't do)
  mDecoder = new MediaDataDecoderProxy(decoder.forget(), tq.forget());

  media::Await(
      do_AddRef(mThreadPool), mDecoder->Init(),
      [&](TrackInfo::TrackType) { mError = NS_OK; },
      [&](const MediaResult& aError) { mError = aError; });

  return NS_SUCCEEDED(mError) ? WEBRTC_VIDEO_CODEC_OK
                              : WEBRTC_VIDEO_CODEC_ERROR;
}

}  // namespace mozilla

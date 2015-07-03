/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AndroidMediaReader.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/gfx/Point.h"
#include "MediaResource.h"
#include "VideoUtils.h"
#include "AndroidMediaDecoder.h"
#include "AndroidMediaPluginHost.h"
#include "MediaDecoderStateMachine.h"
#include "ImageContainer.h"
#include "AbstractMediaDecoder.h"
#include "gfx2DGlue.h"

namespace mozilla {

using namespace mozilla::gfx;
using namespace mozilla::media;

typedef mozilla::layers::Image Image;
typedef mozilla::layers::PlanarYCbCrImage PlanarYCbCrImage;

AndroidMediaReader::AndroidMediaReader(AbstractMediaDecoder *aDecoder,
                                       const nsACString& aContentType) :
  MediaDecoderReader(aDecoder),
  mType(aContentType),
  mPlugin(nullptr),
  mHasAudio(false),
  mHasVideo(false),
  mVideoSeekTimeUs(-1),
  mAudioSeekTimeUs(-1)
{
}

nsresult AndroidMediaReader::Init(MediaDecoderReader* aCloneDonor)
{
  return NS_OK;
}

nsresult AndroidMediaReader::ReadMetadata(MediaInfo* aInfo,
                                          MetadataTags** aTags)
{
  MOZ_ASSERT(OnTaskQueue());

  if (!mPlugin) {
    mPlugin = GetAndroidMediaPluginHost()->CreateDecoder(mDecoder->GetResource(), mType);
    if (!mPlugin) {
      return NS_ERROR_FAILURE;
    }
  }

  // Set the total duration (the max of the audio and video track).
  int64_t durationUs;
  mPlugin->GetDuration(mPlugin, &durationUs);
  if (durationUs) {
    mInfo.mMetadataDuration.emplace(TimeUnit::FromMicroseconds(durationUs));
  }

  if (mPlugin->HasVideo(mPlugin)) {
    int32_t width, height;
    mPlugin->GetVideoParameters(mPlugin, &width, &height);
    nsIntRect pictureRect(0, 0, width, height);

    // Validate the container-reported frame and pictureRect sizes. This ensures
    // that our video frame creation code doesn't overflow.
    nsIntSize displaySize(width, height);
    nsIntSize frameSize(width, height);
    if (!IsValidVideoRegion(frameSize, pictureRect, displaySize)) {
      return NS_ERROR_FAILURE;
    }

    // Video track's frame sizes will not overflow. Activate the video track.
    mHasVideo = true;
    mInfo.mVideo.mDisplay = displaySize;
    mPicture = pictureRect;
    mInitialFrame = frameSize;
    VideoFrameContainer* container = mDecoder->GetVideoFrameContainer();
    if (container) {
      container->SetCurrentFrame(gfxIntSize(displaySize.width, displaySize.height),
                                 nullptr,
                                 mozilla::TimeStamp::Now());
    }
  }

  if (mPlugin->HasAudio(mPlugin)) {
    int32_t numChannels, sampleRate;
    mPlugin->GetAudioParameters(mPlugin, &numChannels, &sampleRate);
    mHasAudio = true;
    mInfo.mAudio.mChannels = numChannels;
    mInfo.mAudio.mRate = sampleRate;
  }

 *aInfo = mInfo;
 *aTags = nullptr;
  return NS_OK;
}

nsRefPtr<ShutdownPromise>
AndroidMediaReader::Shutdown()
{
  ResetDecode();
  if (mPlugin) {
    GetAndroidMediaPluginHost()->DestroyDecoder(mPlugin);
    mPlugin = nullptr;
  }

  return MediaDecoderReader::Shutdown();
}

// Resets all state related to decoding, emptying all buffers etc.
nsresult AndroidMediaReader::ResetDecode()
{
  if (mLastVideoFrame) {
    mLastVideoFrame = nullptr;
  }
  mSeekRequest.DisconnectIfExists();
  mSeekPromise.RejectIfExists(NS_OK, __func__);
  return MediaDecoderReader::ResetDecode();
}

bool AndroidMediaReader::DecodeVideoFrame(bool &aKeyframeSkip,
                                          int64_t aTimeThreshold)
{
  // Record number of frames decoded and parsed. Automatically update the
  // stats counters using the AutoNotifyDecoded stack-based class.
  AbstractMediaDecoder::AutoNotifyDecoded a(mDecoder);

  // Throw away the currently buffered frame if we are seeking.
  if (mLastVideoFrame && mVideoSeekTimeUs != -1) {
    mLastVideoFrame = nullptr;
  }

  ImageBufferCallback bufferCallback(mDecoder->GetImageContainer());
  nsRefPtr<Image> currentImage;

  // Read next frame
  while (true) {
    MPAPI::VideoFrame frame;
    if (!mPlugin->ReadVideo(mPlugin, &frame, mVideoSeekTimeUs, &bufferCallback)) {
      // We reached the end of the video stream. If we have a buffered
      // video frame, push it the video queue using the total duration
      // of the video as the end time.
      if (mLastVideoFrame) {
        int64_t durationUs;
        mPlugin->GetDuration(mPlugin, &durationUs);
        durationUs = std::max<int64_t>(durationUs - mLastVideoFrame->mTime, 0);
        nsRefPtr<VideoData> data = VideoData::ShallowCopyUpdateDuration(mLastVideoFrame,
                                                                        durationUs);
        mVideoQueue.Push(data);
        mLastVideoFrame = nullptr;
      }
      return false;
    }
    mVideoSeekTimeUs = -1;

    if (aKeyframeSkip) {
      // Disable keyframe skipping for now as
      // stagefright doesn't seem to be telling us
      // when a frame is a keyframe.
#if 0
      if (!frame.mKeyFrame) {
        ++a.mParsed;
        ++a.mDropped;
        continue;
      }
#endif
      aKeyframeSkip = false;
    }

    if (frame.mSize == 0)
      return true;

    currentImage = bufferCallback.GetImage();
    int64_t pos = mDecoder->GetResource()->Tell();
    IntRect picture = mPicture;

    nsRefPtr<VideoData> v;
    if (currentImage) {
      gfx::IntSize frameSize = currentImage->GetSize();
      if (frameSize.width != mInitialFrame.width ||
          frameSize.height != mInitialFrame.height) {
        // Frame size is different from what the container reports. This is legal,
        // and we will preserve the ratio of the crop rectangle as it
        // was reported relative to the picture size reported by the container.
        picture.x = (mPicture.x * frameSize.width) / mInitialFrame.width;
        picture.y = (mPicture.y * frameSize.height) / mInitialFrame.height;
        picture.width = (frameSize.width * mPicture.width) / mInitialFrame.width;
        picture.height = (frameSize.height * mPicture.height) / mInitialFrame.height;
      }

      v = VideoData::CreateFromImage(mInfo.mVideo,
                                     mDecoder->GetImageContainer(),
                                     pos,
                                     frame.mTimeUs,
                                     1, // We don't know the duration yet.
                                     currentImage,
                                     frame.mKeyFrame,
                                     -1,
                                     picture);
    } else {
      // Assume YUV
      VideoData::YCbCrBuffer b;
      b.mPlanes[0].mData = static_cast<uint8_t *>(frame.Y.mData);
      b.mPlanes[0].mStride = frame.Y.mStride;
      b.mPlanes[0].mHeight = frame.Y.mHeight;
      b.mPlanes[0].mWidth = frame.Y.mWidth;
      b.mPlanes[0].mOffset = frame.Y.mOffset;
      b.mPlanes[0].mSkip = frame.Y.mSkip;

      b.mPlanes[1].mData = static_cast<uint8_t *>(frame.Cb.mData);
      b.mPlanes[1].mStride = frame.Cb.mStride;
      b.mPlanes[1].mHeight = frame.Cb.mHeight;
      b.mPlanes[1].mWidth = frame.Cb.mWidth;
      b.mPlanes[1].mOffset = frame.Cb.mOffset;
      b.mPlanes[1].mSkip = frame.Cb.mSkip;

      b.mPlanes[2].mData = static_cast<uint8_t *>(frame.Cr.mData);
      b.mPlanes[2].mStride = frame.Cr.mStride;
      b.mPlanes[2].mHeight = frame.Cr.mHeight;
      b.mPlanes[2].mWidth = frame.Cr.mWidth;
      b.mPlanes[2].mOffset = frame.Cr.mOffset;
      b.mPlanes[2].mSkip = frame.Cr.mSkip;

      if (frame.Y.mWidth != mInitialFrame.width ||
          frame.Y.mHeight != mInitialFrame.height) {

        // Frame size is different from what the container reports. This is legal,
        // and we will preserve the ratio of the crop rectangle as it
        // was reported relative to the picture size reported by the container.
        picture.x = (mPicture.x * frame.Y.mWidth) / mInitialFrame.width;
        picture.y = (mPicture.y * frame.Y.mHeight) / mInitialFrame.height;
        picture.width = (frame.Y.mWidth * mPicture.width) / mInitialFrame.width;
        picture.height = (frame.Y.mHeight * mPicture.height) / mInitialFrame.height;
      }

      // This is the approximate byte position in the stream.
      v = VideoData::Create(mInfo.mVideo,
                            mDecoder->GetImageContainer(),
                            pos,
                            frame.mTimeUs,
                            1, // We don't know the duration yet.
                            b,
                            frame.mKeyFrame,
                            -1,
                            picture);
    }

    if (!v) {
      return false;
    }
    a.mParsed++;
    a.mDecoded++;
    NS_ASSERTION(a.mDecoded <= a.mParsed, "Expect to decode fewer frames than parsed in AndroidMedia...");

    // Since MPAPI doesn't give us the end time of frames, we keep one frame
    // buffered in AndroidMediaReader and push it into the queue as soon
    // we read the following frame so we can use that frame's start time as
    // the end time of the buffered frame.
    if (!mLastVideoFrame) {
      mLastVideoFrame = v;
      continue;
    }

    // Calculate the duration as the timestamp of the current frame minus the
    // timestamp of the previous frame. We can then return the previously
    // decoded frame, and it will have a valid timestamp.
    int64_t duration = v->mTime - mLastVideoFrame->mTime;
    mLastVideoFrame = VideoData::ShallowCopyUpdateDuration(mLastVideoFrame, duration);

    // We have the start time of the next frame, so we can push the previous
    // frame into the queue, except if the end time is below the threshold,
    // in which case it wouldn't be displayed anyway.
    if (mLastVideoFrame->GetEndTime() < aTimeThreshold) {
      mLastVideoFrame = nullptr;
      continue;
    }

    // Buffer the current frame we just decoded.
    mVideoQueue.Push(mLastVideoFrame);
    mLastVideoFrame = v;

    break;
  }

  return true;
}

bool AndroidMediaReader::DecodeAudioData()
{
  MOZ_ASSERT(OnTaskQueue());

  // This is the approximate byte position in the stream.
  int64_t pos = mDecoder->GetResource()->Tell();

  // Read next frame
  MPAPI::AudioFrame source;
  if (!mPlugin->ReadAudio(mPlugin, &source, mAudioSeekTimeUs)) {
    return false;
  }
  mAudioSeekTimeUs = -1;

  // Ignore empty buffers which stagefright media read will sporadically return
  if (source.mSize == 0)
    return true;

  uint32_t frames = source.mSize / (source.mAudioChannels *
                                    sizeof(AudioDataValue));

  typedef AudioCompactor::NativeCopy MPCopy;
  return mAudioCompactor.Push(pos,
                              source.mTimeUs,
                              source.mAudioSampleRate,
                              frames,
                              source.mAudioChannels,
                              MPCopy(static_cast<uint8_t *>(source.mData),
                                     source.mSize,
                                     source.mAudioChannels));
}

nsRefPtr<MediaDecoderReader::SeekPromise>
AndroidMediaReader::Seek(int64_t aTarget, int64_t aEndTime)
{
  MOZ_ASSERT(OnTaskQueue());

  nsRefPtr<SeekPromise> p = mSeekPromise.Ensure(__func__);
  if (mHasAudio && mHasVideo) {
    // The decoder seeks/demuxes audio and video streams separately. So if
    // we seek both audio and video to aTarget, the audio stream can typically
    // seek closer to the seek target, since typically every audio block is
    // a sync point, whereas for video there are only keyframes once every few
    // seconds. So if we have both audio and video, we must seek the video
    // stream to the preceeding keyframe first, get the stream time, and then
    // seek the audio stream to match the video stream's time. Otherwise, the
    // audio and video streams won't be in sync after the seek.
    mVideoSeekTimeUs = aTarget;

    nsRefPtr<AndroidMediaReader> self = this;
    mSeekRequest.Begin(DecodeToFirstVideoData()->Then(TaskQueue(), __func__, [self] (VideoData* v) {
      self->mSeekRequest.Complete();
      self->mAudioSeekTimeUs = v->mTime;
      self->mSeekPromise.Resolve(self->mAudioSeekTimeUs, __func__);
    }, [self, aTarget] () {
      self->mSeekRequest.Complete();
      self->mAudioSeekTimeUs = aTarget;
      self->mSeekPromise.Resolve(aTarget, __func__);
    }));
  } else {
    mAudioSeekTimeUs = mVideoSeekTimeUs = aTarget;
    mSeekPromise.Resolve(aTarget, __func__);
  }

  return p;
}

AndroidMediaReader::ImageBufferCallback::ImageBufferCallback(mozilla::layers::ImageContainer *aImageContainer) :
  mImageContainer(aImageContainer)
{
}

void *
AndroidMediaReader::ImageBufferCallback::operator()(size_t aWidth, size_t aHeight,
                                                    MPAPI::ColorFormat aColorFormat)
{
  if (!mImageContainer) {
    NS_WARNING("No image container to construct an image");
    return nullptr;
  }

  nsRefPtr<Image> image;
  switch(aColorFormat) {
    case MPAPI::RGB565:
      image = mozilla::layers::CreateSharedRGBImage(mImageContainer,
                                                    nsIntSize(aWidth, aHeight),
                                                    gfxImageFormat::RGB16_565);
      if (!image) {
        NS_WARNING("Could not create rgb image");
        return nullptr;
      }

      mImage = image;
      return image->GetBuffer();
    case MPAPI::I420:
      return CreateI420Image(aWidth, aHeight);
    default:
      NS_NOTREACHED("Color format not supported");
      return nullptr;
  }
}

uint8_t *
AndroidMediaReader::ImageBufferCallback::CreateI420Image(size_t aWidth,
                                                         size_t aHeight)
{
  mImage = mImageContainer->CreateImage(ImageFormat::PLANAR_YCBCR);
  PlanarYCbCrImage *yuvImage = static_cast<PlanarYCbCrImage *>(mImage.get());

  if (!yuvImage) {
    NS_WARNING("Could not create I420 image");
    return nullptr;
  }

  size_t frameSize = aWidth * aHeight;

  // Allocate enough for one full resolution Y plane
  // and two quarter resolution Cb/Cr planes.
  uint8_t *buffer = yuvImage->AllocateAndGetNewBuffer(frameSize * 3 / 2);

  mozilla::layers::PlanarYCbCrData frameDesc;

  frameDesc.mYChannel = buffer;
  frameDesc.mCbChannel = buffer + frameSize;
  frameDesc.mCrChannel = buffer + frameSize * 5 / 4;

  frameDesc.mYSize = IntSize(aWidth, aHeight);
  frameDesc.mCbCrSize = IntSize(aWidth / 2, aHeight / 2);

  frameDesc.mYStride = aWidth;
  frameDesc.mCbCrStride = aWidth / 2;

  frameDesc.mYSkip = 0;
  frameDesc.mCbSkip = 0;
  frameDesc.mCrSkip = 0;

  frameDesc.mPicX = 0;
  frameDesc.mPicY = 0;
  frameDesc.mPicSize = IntSize(aWidth, aHeight);

  yuvImage->SetDataNoCopy(frameDesc);

  return buffer;
}

already_AddRefed<Image>
AndroidMediaReader::ImageBufferCallback::GetImage()
{
  return mImage.forget();
}

} // namespace mozilla

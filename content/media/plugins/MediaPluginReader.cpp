/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaPluginReader.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/TimeRanges.h"
#include "MediaResource.h"
#include "VideoUtils.h"
#include "MediaPluginDecoder.h"
#include "MediaPluginHost.h"
#include "MediaDecoderStateMachine.h"
#include "ImageContainer.h"
#include "AbstractMediaDecoder.h"

namespace mozilla {

typedef mozilla::layers::Image Image;

MediaPluginReader::MediaPluginReader(AbstractMediaDecoder *aDecoder,
                                     const nsACString& aContentType) :
  MediaDecoderReader(aDecoder),
  mType(aContentType),
  mPlugin(NULL),
  mHasAudio(false),
  mHasVideo(false),
  mVideoSeekTimeUs(-1),
  mAudioSeekTimeUs(-1),
  mLastVideoFrame(NULL)
{
}

MediaPluginReader::~MediaPluginReader()
{
  ResetDecode();
}

nsresult MediaPluginReader::Init(MediaDecoderReader* aCloneDonor)
{
  return NS_OK;
}

nsresult MediaPluginReader::ReadMetadata(VideoInfo* aInfo,
                                           MetadataTags** aTags)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  if (!mPlugin) {
    mPlugin = GetMediaPluginHost()->CreateDecoder(mDecoder->GetResource(), mType);
    if (!mPlugin) {
      return NS_ERROR_FAILURE;
    }
  }

  // Set the total duration (the max of the audio and video track).
  int64_t durationUs;
  mPlugin->GetDuration(mPlugin, &durationUs);
  if (durationUs) {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mDecoder->SetMediaDuration(durationUs);
  }

  if (mPlugin->HasVideo(mPlugin)) {
    int32_t width, height;
    mPlugin->GetVideoParameters(mPlugin, &width, &height);
    nsIntRect pictureRect(0, 0, width, height);

    // Validate the container-reported frame and pictureRect sizes. This ensures
    // that our video frame creation code doesn't overflow.
    nsIntSize displaySize(width, height);
    nsIntSize frameSize(width, height);
    if (!VideoInfo::ValidateVideoRegion(frameSize, pictureRect, displaySize)) {
      return NS_ERROR_FAILURE;
    }

    // Video track's frame sizes will not overflow. Activate the video track.
    mHasVideo = mInfo.mHasVideo = true;
    mInfo.mDisplay = displaySize;
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
    mHasAudio = mInfo.mHasAudio = true;
    mInfo.mAudioChannels = numChannels;
    mInfo.mAudioRate = sampleRate;
  }

 *aInfo = mInfo;
 *aTags = nullptr;
  return NS_OK;
}

// Resets all state related to decoding, emptying all buffers etc.
nsresult MediaPluginReader::ResetDecode()
{
  if (mLastVideoFrame) {
    delete mLastVideoFrame;
    mLastVideoFrame = NULL;
  }
  if (mPlugin) {
    GetMediaPluginHost()->DestroyDecoder(mPlugin);
    mPlugin = NULL;
  }

  return NS_OK;
}

bool MediaPluginReader::DecodeVideoFrame(bool &aKeyframeSkip,
                                           int64_t aTimeThreshold)
{
  // Record number of frames decoded and parsed. Automatically update the
  // stats counters using the AutoNotifyDecoded stack-based class.
  uint32_t parsed = 0, decoded = 0;
  AbstractMediaDecoder::AutoNotifyDecoded autoNotify(mDecoder, parsed, decoded);

  // Throw away the currently buffered frame if we are seeking.
  if (mLastVideoFrame && mVideoSeekTimeUs != -1) {
    delete mLastVideoFrame;
    mLastVideoFrame = NULL;
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
        mLastVideoFrame->mEndTime = (durationUs > mLastVideoFrame->mTime)
                                  ? durationUs
                                  : mLastVideoFrame->mTime;
        mVideoQueue.Push(mLastVideoFrame);
        mLastVideoFrame = NULL;
      }
      mVideoQueue.Finish();
      return false;
    }
    mVideoSeekTimeUs = -1;

    if (aKeyframeSkip) {
      // Disable keyframe skipping for now as
      // stagefright doesn't seem to be telling us
      // when a frame is a keyframe.
#if 0
      if (!frame.mKeyFrame) {
        ++parsed;
        continue;
      }
#endif
      aKeyframeSkip = false;
    }

    if (frame.mSize == 0)
      return true;

    currentImage = bufferCallback.GetImage();
    int64_t pos = mDecoder->GetResource()->Tell();
    nsIntRect picture = mPicture;
 
    VideoData *v;
    if (currentImage) {
      gfxIntSize frameSize = currentImage->GetSize();
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

      v = VideoData::CreateFromImage(mInfo,
                                     mDecoder->GetImageContainer(),
                                     pos,
                                     frame.mTimeUs,
                                     frame.mTimeUs+1, // We don't know the end time.
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
      v = VideoData::Create(mInfo,
                            mDecoder->GetImageContainer(),
                            pos,
                            frame.mTimeUs,
                            frame.mTimeUs+1, // We don't know the end time.
                            b,
                            frame.mKeyFrame,
                            -1,
                            picture);
    }
 
    if (!v) {
      return false;
    }
    parsed++;
    decoded++;
    NS_ASSERTION(decoded <= parsed, "Expect to decode fewer frames than parsed in MediaPlugin...");

    // Since MPAPI doesn't give us the end time of frames, we keep one frame
    // buffered in MediaPluginReader and push it into the queue as soon
    // we read the following frame so we can use that frame's start time as
    // the end time of the buffered frame.
    if (!mLastVideoFrame) {
      mLastVideoFrame = v;
      continue;
    }

    mLastVideoFrame->mEndTime = v->mTime;

    // We have the start time of the next frame, so we can push the previous
    // frame into the queue, except if the end time is below the threshold,
    // in which case it wouldn't be displayed anyway.
    if (mLastVideoFrame->mEndTime < aTimeThreshold) {
      delete mLastVideoFrame;
      mLastVideoFrame = NULL;
      continue;
    }

    mVideoQueue.Push(mLastVideoFrame);

    // Buffer the current frame we just decoded.
    mLastVideoFrame = v;

    break;
  }

  return true;
}

bool MediaPluginReader::DecodeAudioData()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  // This is the approximate byte position in the stream.
  int64_t pos = mDecoder->GetResource()->Tell();

  // Read next frame
  MPAPI::AudioFrame frame;
  if (!mPlugin->ReadAudio(mPlugin, &frame, mAudioSeekTimeUs)) {
    mAudioQueue.Finish();
    return false;
  }
  mAudioSeekTimeUs = -1;

  // Ignore empty buffers which stagefright media read will sporadically return
  if (frame.mSize == 0)
    return true;

  nsAutoArrayPtr<AudioDataValue> buffer(new AudioDataValue[frame.mSize/2] );
  memcpy(buffer.get(), frame.mData, frame.mSize);

  uint32_t frames = frame.mSize / (2 * frame.mAudioChannels);
  CheckedInt64 duration = FramesToUsecs(frames, frame.mAudioSampleRate);
  if (!duration.isValid()) {
    return false;
  }

  mAudioQueue.Push(new AudioData(pos,
                                 frame.mTimeUs,
                                 duration.value(),
                                 frames,
                                 buffer.forget(),
                                 frame.mAudioChannels));
  return true;
}

nsresult MediaPluginReader::Seek(int64_t aTarget, int64_t aStartTime, int64_t aEndTime, int64_t aCurrentTime)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  mVideoQueue.Erase();
  mAudioQueue.Erase();

  mAudioSeekTimeUs = mVideoSeekTimeUs = aTarget;

  return DecodeToTarget(aTarget);
}

nsresult MediaPluginReader::GetBuffered(mozilla::dom::TimeRanges* aBuffered, int64_t aStartTime)
{
  if (!mPlugin)
    return NS_OK;

  MediaResource* stream = mDecoder->GetResource();

  int64_t durationUs = 0;
  mPlugin->GetDuration(mPlugin, &durationUs);

  GetEstimatedBufferedTimeRanges(stream, durationUs, aBuffered);
  
  return NS_OK;
}

MediaPluginReader::ImageBufferCallback::ImageBufferCallback(mozilla::layers::ImageContainer *aImageContainer) :
  mImageContainer(aImageContainer)
{
}

void *
MediaPluginReader::ImageBufferCallback::operator()(size_t aWidth, size_t aHeight,
                                                     MPAPI::ColorFormat aColorFormat)
{
  if (!mImageContainer) {
    NS_WARNING("No image container to construct an image");
    return nullptr;
  }

  nsRefPtr<Image> rgbImage;
  switch(aColorFormat) {
    case MPAPI::RGB565:
      rgbImage = mozilla::layers::CreateSharedRGBImage(mImageContainer,
                                                       nsIntSize(aWidth, aHeight),
                                                       gfxASurface::ImageFormatRGB16_565);
      if (!rgbImage) {
        NS_WARNING("Could not create rgb image");
        return nullptr;
      }

      mImage = rgbImage;
      return rgbImage->AsSharedImage()->GetBuffer();
    case MPAPI::YCbCr:
    default:
      NS_NOTREACHED("Color format not supported");
      return nullptr;
  }
}

already_AddRefed<Image>
MediaPluginReader::ImageBufferCallback::GetImage()
{
  return mImage.forget();
}

} // namespace mozilla

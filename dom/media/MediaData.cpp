/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaData.h"
#include "MediaInfo.h"
#ifdef MOZ_OMX_DECODER
#include "GrallocImages.h"
#include "mozilla/layers/TextureClient.h"
#endif
#include "VideoUtils.h"
#include "ImageContainer.h"

#ifdef MOZ_WIDGET_GONK
#include <cutils/properties.h>
#endif

namespace mozilla {

using namespace mozilla::gfx;
using layers::ImageContainer;
using layers::PlanarYCbCrImage;
using layers::PlanarYCbCrData;

void
AudioData::EnsureAudioBuffer()
{
  if (mAudioBuffer)
    return;
  mAudioBuffer = SharedBuffer::Create(mFrames*mChannels*sizeof(AudioDataValue));

  AudioDataValue* data = static_cast<AudioDataValue*>(mAudioBuffer->Data());
  for (uint32_t i = 0; i < mFrames; ++i) {
    for (uint32_t j = 0; j < mChannels; ++j) {
      data[j*mFrames + i] = mAudioData[i*mChannels + j];
    }
  }
}

size_t
AudioData::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t size = aMallocSizeOf(this) + aMallocSizeOf(mAudioData);
  if (mAudioBuffer) {
    size += mAudioBuffer->SizeOfIncludingThis(aMallocSizeOf);
  }
  return size;
}

/* static */
already_AddRefed<AudioData>
AudioData::TransferAndUpdateTimestampAndDuration(AudioData* aOther,
                                                  int64_t aTimestamp,
                                                  int64_t aDuration)
{
  NS_ENSURE_TRUE(aOther, nullptr);
  nsRefPtr<AudioData> v = new AudioData(aOther->mOffset,
                                        aTimestamp,
                                        aDuration,
                                        aOther->mFrames,
                                        aOther->mAudioData,
                                        aOther->mChannels,
                                        aOther->mRate);
  v->mDiscontinuity = aOther->mDiscontinuity;
  // Remove aOther's AudioData as it can't be shared across two targets.
  aOther->mAudioData.forget();

  return v.forget();
}

static bool
ValidatePlane(const VideoData::YCbCrBuffer::Plane& aPlane)
{
  return aPlane.mWidth <= PlanarYCbCrImage::MAX_DIMENSION &&
         aPlane.mHeight <= PlanarYCbCrImage::MAX_DIMENSION &&
         aPlane.mWidth * aPlane.mHeight < MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT &&
         aPlane.mStride > 0;
}

#ifdef MOZ_WIDGET_GONK
static bool
IsYV12Format(const VideoData::YCbCrBuffer::Plane& aYPlane,
             const VideoData::YCbCrBuffer::Plane& aCbPlane,
             const VideoData::YCbCrBuffer::Plane& aCrPlane)
{
  return
    aYPlane.mWidth % 2 == 0 &&
    aYPlane.mHeight % 2 == 0 &&
    aYPlane.mWidth / 2 == aCbPlane.mWidth &&
    aYPlane.mHeight / 2 == aCbPlane.mHeight &&
    aCbPlane.mWidth == aCrPlane.mWidth &&
    aCbPlane.mHeight == aCrPlane.mHeight;
}

static bool
IsInEmulator()
{
  char propQemu[PROPERTY_VALUE_MAX];
  property_get("ro.kernel.qemu", propQemu, "");
  return !strncmp(propQemu, "1", 1);
}

#endif

VideoData::VideoData(int64_t aOffset, int64_t aTime, int64_t aDuration, int64_t aTimecode)
  : MediaData(VIDEO_DATA, aOffset, aTime, aDuration),
    mTimecode(aTimecode),
    mDuplicate(true),
    mKeyframe(false)
{
  NS_ASSERTION(mDuration >= 0, "Frame must have non-negative duration.");
}

VideoData::VideoData(int64_t aOffset,
                     int64_t aTime,
                     int64_t aDuration,
                     bool aKeyframe,
                     int64_t aTimecode,
                     IntSize aDisplay)
  : MediaData(VIDEO_DATA, aOffset, aTime, aDuration),
    mDisplay(aDisplay),
    mTimecode(aTimecode),
    mDuplicate(false),
    mKeyframe(aKeyframe)
{
  NS_ASSERTION(mDuration >= 0, "Frame must have non-negative duration.");
}

VideoData::~VideoData()
{
}

size_t
VideoData::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t size = aMallocSizeOf(this);

  // Currently only PLANAR_YCBCR has a well defined function for determining
  // it's size, so reporting is limited to that type.
  if (mImage && mImage->GetFormat() == ImageFormat::PLANAR_YCBCR) {
    const mozilla::layers::PlanarYCbCrImage* img =
        static_cast<const mozilla::layers::PlanarYCbCrImage*>(mImage.get());
    size += img->SizeOfIncludingThis(aMallocSizeOf);
  }

  return size;
}

/* static */
already_AddRefed<VideoData>
VideoData::ShallowCopyUpdateDuration(VideoData* aOther,
                                     int64_t aDuration)
{
  nsRefPtr<VideoData> v = new VideoData(aOther->mOffset,
                                        aOther->mTime,
                                        aDuration,
                                        aOther->mKeyframe,
                                        aOther->mTimecode,
                                        aOther->mDisplay);
  v->mDiscontinuity = aOther->mDiscontinuity;
  v->mImage = aOther->mImage;
  return v.forget();
}

/* static */
already_AddRefed<VideoData>
VideoData::ShallowCopyUpdateTimestamp(VideoData* aOther,
                                      int64_t aTimestamp)
{
  NS_ENSURE_TRUE(aOther, nullptr);
  nsRefPtr<VideoData> v = new VideoData(aOther->mOffset,
                                        aTimestamp,
                                        aOther->GetEndTime() - aTimestamp,
                                        aOther->mKeyframe,
                                        aOther->mTimecode,
                                        aOther->mDisplay);
  v->mDiscontinuity = aOther->mDiscontinuity;
  v->mImage = aOther->mImage;
  return v.forget();
}

/* static */
already_AddRefed<VideoData>
VideoData::ShallowCopyUpdateTimestampAndDuration(VideoData* aOther,
                                                 int64_t aTimestamp,
                                                 int64_t aDuration)
{
  NS_ENSURE_TRUE(aOther, nullptr);
  nsRefPtr<VideoData> v = new VideoData(aOther->mOffset,
                                        aTimestamp,
                                        aDuration,
                                        aOther->mKeyframe,
                                        aOther->mTimecode,
                                        aOther->mDisplay);
  v->mDiscontinuity = aOther->mDiscontinuity;
  v->mImage = aOther->mImage;
  return v.forget();
}

/* static */
void VideoData::SetVideoDataToImage(PlanarYCbCrImage* aVideoImage,
                                    VideoInfo& aInfo,
                                    const YCbCrBuffer &aBuffer,
                                    const IntRect& aPicture,
                                    bool aCopyData)
{
  if (!aVideoImage) {
    return;
  }
  const YCbCrBuffer::Plane &Y = aBuffer.mPlanes[0];
  const YCbCrBuffer::Plane &Cb = aBuffer.mPlanes[1];
  const YCbCrBuffer::Plane &Cr = aBuffer.mPlanes[2];

  PlanarYCbCrData data;
  data.mYChannel = Y.mData + Y.mOffset;
  data.mYSize = IntSize(Y.mWidth, Y.mHeight);
  data.mYStride = Y.mStride;
  data.mYSkip = Y.mSkip;
  data.mCbChannel = Cb.mData + Cb.mOffset;
  data.mCrChannel = Cr.mData + Cr.mOffset;
  data.mCbCrSize = IntSize(Cb.mWidth, Cb.mHeight);
  data.mCbCrStride = Cb.mStride;
  data.mCbSkip = Cb.mSkip;
  data.mCrSkip = Cr.mSkip;
  data.mPicX = aPicture.x;
  data.mPicY = aPicture.y;
  data.mPicSize = aPicture.Size();
  data.mStereoMode = aInfo.mStereoMode;

  aVideoImage->SetDelayedConversion(true);
  if (aCopyData) {
    aVideoImage->SetData(data);
  } else {
    aVideoImage->SetDataNoCopy(data);
  }
}

/* static */
already_AddRefed<VideoData>
VideoData::Create(VideoInfo& aInfo,
                  ImageContainer* aContainer,
                  Image* aImage,
                  int64_t aOffset,
                  int64_t aTime,
                  int64_t aDuration,
                  const YCbCrBuffer& aBuffer,
                  bool aKeyframe,
                  int64_t aTimecode,
                  const IntRect& aPicture)
{
  if (!aImage && !aContainer) {
    // Create a dummy VideoData with no image. This gives us something to
    // send to media streams if necessary.
    nsRefPtr<VideoData> v(new VideoData(aOffset,
                                        aTime,
                                        aDuration,
                                        aKeyframe,
                                        aTimecode,
                                        aInfo.mDisplay.ToIntSize()));
    return v.forget();
  }

  // The following situation should never happen unless there is a bug
  // in the decoder
  if (aBuffer.mPlanes[1].mWidth != aBuffer.mPlanes[2].mWidth ||
      aBuffer.mPlanes[1].mHeight != aBuffer.mPlanes[2].mHeight) {
    NS_ERROR("C planes with different sizes");
    return nullptr;
  }

  // The following situations could be triggered by invalid input
  if (aPicture.width <= 0 || aPicture.height <= 0) {
    // In debug mode, makes the error more noticeable
    MOZ_ASSERT(false, "Empty picture rect");
    return nullptr;
  }
  if (!ValidatePlane(aBuffer.mPlanes[0]) || !ValidatePlane(aBuffer.mPlanes[1]) ||
      !ValidatePlane(aBuffer.mPlanes[2])) {
    NS_WARNING("Invalid plane size");
    return nullptr;
  }

  // Ensure the picture size specified in the headers can be extracted out of
  // the frame we've been supplied without indexing out of bounds.
  CheckedUint32 xLimit = aPicture.x + CheckedUint32(aPicture.width);
  CheckedUint32 yLimit = aPicture.y + CheckedUint32(aPicture.height);
  if (!xLimit.isValid() || xLimit.value() > aBuffer.mPlanes[0].mStride ||
      !yLimit.isValid() || yLimit.value() > aBuffer.mPlanes[0].mHeight)
  {
    // The specified picture dimensions can't be contained inside the video
    // frame, we'll stomp memory if we try to copy it. Fail.
    NS_WARNING("Overflowing picture rect");
    return nullptr;
  }

  nsRefPtr<VideoData> v(new VideoData(aOffset,
                                      aTime,
                                      aDuration,
                                      aKeyframe,
                                      aTimecode,
                                      aInfo.mDisplay.ToIntSize()));
#ifdef MOZ_WIDGET_GONK
  const YCbCrBuffer::Plane &Y = aBuffer.mPlanes[0];
  const YCbCrBuffer::Plane &Cb = aBuffer.mPlanes[1];
  const YCbCrBuffer::Plane &Cr = aBuffer.mPlanes[2];
#endif

  if (!aImage) {
    // Currently our decoder only knows how to output to ImageFormat::PLANAR_YCBCR
    // format.
#ifdef MOZ_WIDGET_GONK
    if (IsYV12Format(Y, Cb, Cr) && !IsInEmulator()) {
      v->mImage = aContainer->CreateImage(ImageFormat::GRALLOC_PLANAR_YCBCR);
    }
#endif
    if (!v->mImage) {
      v->mImage = aContainer->CreateImage(ImageFormat::PLANAR_YCBCR);
    }
  } else {
    v->mImage = aImage;
  }

  if (!v->mImage) {
    return nullptr;
  }
  NS_ASSERTION(v->mImage->GetFormat() == ImageFormat::PLANAR_YCBCR ||
               v->mImage->GetFormat() == ImageFormat::GRALLOC_PLANAR_YCBCR,
               "Wrong format?");
  PlanarYCbCrImage* videoImage = static_cast<PlanarYCbCrImage*>(v->mImage.get());

  if (!aImage) {
    VideoData::SetVideoDataToImage(videoImage, aInfo, aBuffer, aPicture,
                                   true /* aCopyData */);
  } else {
    VideoData::SetVideoDataToImage(videoImage, aInfo, aBuffer, aPicture,
                                   false /* aCopyData */);
  }

#ifdef MOZ_WIDGET_GONK
  if (!videoImage->IsValid() && !aImage && IsYV12Format(Y, Cb, Cr)) {
    // Failed to allocate gralloc. Try fallback.
    v->mImage = aContainer->CreateImage(ImageFormat::PLANAR_YCBCR);
    if (!v->mImage) {
      return nullptr;
    }
    videoImage = static_cast<PlanarYCbCrImage*>(v->mImage.get());
    VideoData::SetVideoDataToImage(videoImage, aInfo, aBuffer, aPicture,
                                   true /* aCopyData */);
  }
#endif
  return v.forget();
}

/* static */
already_AddRefed<VideoData>
VideoData::Create(VideoInfo& aInfo,
                  ImageContainer* aContainer,
                  int64_t aOffset,
                  int64_t aTime,
                  int64_t aDuration,
                  const YCbCrBuffer& aBuffer,
                  bool aKeyframe,
                  int64_t aTimecode,
                  const IntRect& aPicture)
{
  return Create(aInfo, aContainer, nullptr, aOffset, aTime, aDuration, aBuffer,
                aKeyframe, aTimecode, aPicture);
}

/* static */
already_AddRefed<VideoData>
VideoData::Create(VideoInfo& aInfo,
                  Image* aImage,
                  int64_t aOffset,
                  int64_t aTime,
                  int64_t aDuration,
                  const YCbCrBuffer& aBuffer,
                  bool aKeyframe,
                  int64_t aTimecode,
                  const IntRect& aPicture)
{
  return Create(aInfo, nullptr, aImage, aOffset, aTime, aDuration, aBuffer,
                aKeyframe, aTimecode, aPicture);
}

/* static */
already_AddRefed<VideoData>
VideoData::CreateFromImage(VideoInfo& aInfo,
                           ImageContainer* aContainer,
                           int64_t aOffset,
                           int64_t aTime,
                           int64_t aDuration,
                           const nsRefPtr<Image>& aImage,
                           bool aKeyframe,
                           int64_t aTimecode,
                           const IntRect& aPicture)
{
  nsRefPtr<VideoData> v(new VideoData(aOffset,
                                      aTime,
                                      aDuration,
                                      aKeyframe,
                                      aTimecode,
                                      aInfo.mDisplay.ToIntSize()));
  v->mImage = aImage;
  return v.forget();
}

#ifdef MOZ_OMX_DECODER
/* static */
already_AddRefed<VideoData>
VideoData::Create(VideoInfo& aInfo,
                  ImageContainer* aContainer,
                  int64_t aOffset,
                  int64_t aTime,
                  int64_t aDuration,
                  mozilla::layers::TextureClient* aBuffer,
                  bool aKeyframe,
                  int64_t aTimecode,
                  const IntRect& aPicture)
{
  if (!aContainer) {
    // Create a dummy VideoData with no image. This gives us something to
    // send to media streams if necessary.
    nsRefPtr<VideoData> v(new VideoData(aOffset,
                                        aTime,
                                        aDuration,
                                        aKeyframe,
                                        aTimecode,
                                        aInfo.mDisplay.ToIntSize()));
    return v.forget();
  }

  // The following situations could be triggered by invalid input
  if (aPicture.width <= 0 || aPicture.height <= 0) {
    NS_WARNING("Empty picture rect");
    return nullptr;
  }

  // Ensure the picture size specified in the headers can be extracted out of
  // the frame we've been supplied without indexing out of bounds.
  CheckedUint32 xLimit = aPicture.x + CheckedUint32(aPicture.width);
  CheckedUint32 yLimit = aPicture.y + CheckedUint32(aPicture.height);
  if (!xLimit.isValid() || !yLimit.isValid())
  {
    // The specified picture dimensions can't be contained inside the video
    // frame, we'll stomp memory if we try to copy it. Fail.
    NS_WARNING("Overflowing picture rect");
    return nullptr;
  }

  nsRefPtr<VideoData> v(new VideoData(aOffset,
                                      aTime,
                                      aDuration,
                                      aKeyframe,
                                      aTimecode,
                                      aInfo.mDisplay.ToIntSize()));

  v->mImage = aContainer->CreateImage(ImageFormat::GRALLOC_PLANAR_YCBCR);
  if (!v->mImage) {
    return nullptr;
  }
  NS_ASSERTION(v->mImage->GetFormat() == ImageFormat::GRALLOC_PLANAR_YCBCR,
               "Wrong format?");
  typedef mozilla::layers::GrallocImage GrallocImage;
  GrallocImage* videoImage = static_cast<GrallocImage*>(v->mImage.get());
  GrallocImage::GrallocData data;

  data.mPicSize = aPicture.Size();
  data.mGraphicBuffer = aBuffer;

  videoImage->SetData(data);

  return v.forget();
}
#endif  // MOZ_OMX_DECODER

} // namespace mozilla

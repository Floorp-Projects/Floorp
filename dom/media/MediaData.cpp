/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaData.h"

#include "ImageContainer.h"
#include "MediaInfo.h"
#include "PerformanceRecorder.h"
#include "VideoUtils.h"
#include "YCbCrUtils.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/KnowsCompositor.h"
#include "mozilla/layers/SharedRGBImage.h"

#include <stdint.h>

#ifdef XP_WIN
#  include "mozilla/gfx/DeviceManagerDx.h"
#  include "mozilla/layers/D3D11ShareHandleImage.h"
#  include "mozilla/layers/D3D11YCbCrImage.h"
#elif XP_MACOSX
#  include "MacIOSurfaceImage.h"
#  include "mozilla/gfx/gfxVars.h"
#endif

namespace mozilla {

using namespace mozilla::gfx;
using layers::PlanarYCbCrData;
using layers::PlanarYCbCrImage;
using media::TimeUnit;

const char* AudioData::sTypeName = "audio";
const char* VideoData::sTypeName = "video";

AudioData::AudioData(int64_t aOffset, const media::TimeUnit& aTime,
                     AlignedAudioBuffer&& aData, uint32_t aChannels,
                     uint32_t aRate, uint32_t aChannelMap)
    // Passing TimeUnit::Zero() here because we can't pass the result of an
    // arithmetic operation to the CheckedInt ctor. We set the duration in the
    // ctor body below.
    : MediaData(sType, aOffset, aTime, TimeUnit::Zero()),
      mChannels(aChannels),
      mChannelMap(aChannelMap),
      mRate(aRate),
      mOriginalTime(aTime),
      mAudioData(std::move(aData)),
      mFrames(mAudioData.Length() / aChannels) {
  MOZ_RELEASE_ASSERT(aChannels != 0,
                     "Can't create an AudioData with 0 channels.");
  MOZ_RELEASE_ASSERT(aRate != 0,
                     "Can't create an AudioData with a sample-rate of 0.");
  mDuration = TimeUnit(mFrames, aRate);
}

Span<AudioDataValue> AudioData::Data() const {
  return Span{GetAdjustedData(), mFrames * mChannels};
}

void AudioData::SetOriginalStartTime(const media::TimeUnit& aStartTime) {
  MOZ_ASSERT(mTime == mOriginalTime,
             "Do not call this if data has been trimmed!");
  mTime = aStartTime;
  mOriginalTime = aStartTime;
}

bool AudioData::AdjustForStartTime(const media::TimeUnit& aStartTime) {
  mOriginalTime -= aStartTime;
  mTime -= aStartTime;
  if (mTrimWindow) {
    *mTrimWindow -= aStartTime;
  }
  if (mTime.IsNegative()) {
    NS_WARNING("Negative audio start time after time-adjustment!");
  }
  return mTime.IsValid() && mOriginalTime.IsValid();
}

bool AudioData::SetTrimWindow(const media::TimeInterval& aTrim) {
  MOZ_DIAGNOSTIC_ASSERT(aTrim.mStart.IsValid() && aTrim.mEnd.IsValid(),
                        "An overflow occurred on the provided TimeInterval");
  if (!mAudioData) {
    // MoveableData got called. Can no longer work on it.
    return false;
  }
  if (aTrim.mStart < mOriginalTime || aTrim.mEnd > GetEndTime()) {
    return false;
  }

  auto trimBefore = aTrim.mStart - mOriginalTime;
  auto trimAfter = aTrim.mEnd - mOriginalTime;
  if (!trimBefore.IsValid() || !trimAfter.IsValid()) {
    // Overflow.
    return false;
  }
  if (!mTrimWindow && trimBefore.IsZero() && trimAfter == mDuration) {
    // Nothing to change, abort early to prevent rounding errors.
    return true;
  }

  size_t frameOffset = trimBefore.ToTicksAtRate(mRate);
  mTrimWindow = Some(aTrim);
  mDataOffset = frameOffset * mChannels;
  MOZ_DIAGNOSTIC_ASSERT(mDataOffset <= mAudioData.Length(),
                        "Data offset outside original buffer");
  mFrames = (trimAfter - trimBefore).ToTicksAtRate(mRate);
  MOZ_DIAGNOSTIC_ASSERT(mFrames <= mAudioData.Length() / mChannels,
                        "More frames than found in container");
  mTime = mOriginalTime + trimBefore;
  mDuration = TimeUnit(mFrames, mRate);

  return true;
}

AudioDataValue* AudioData::GetAdjustedData() const {
  if (!mAudioData) {
    return nullptr;
  }
  return mAudioData.Data() + mDataOffset;
}

void AudioData::EnsureAudioBuffer() {
  if (mAudioBuffer || !mAudioData) {
    return;
  }
  const AudioDataValue* srcData = GetAdjustedData();
  CheckedInt<size_t> bufferSize(sizeof(AudioDataValue));
  bufferSize *= mFrames;
  bufferSize *= mChannels;
  mAudioBuffer = SharedBuffer::Create(bufferSize);

  AudioDataValue* destData = static_cast<AudioDataValue*>(mAudioBuffer->Data());
  for (uint32_t i = 0; i < mFrames; ++i) {
    for (uint32_t j = 0; j < mChannels; ++j) {
      destData[j * mFrames + i] = srcData[i * mChannels + j];
    }
  }
}

size_t AudioData::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t size =
      aMallocSizeOf(this) + mAudioData.SizeOfExcludingThis(aMallocSizeOf);
  if (mAudioBuffer) {
    size += mAudioBuffer->SizeOfIncludingThis(aMallocSizeOf);
  }
  return size;
}

AlignedAudioBuffer AudioData::MoveableData() {
  // Trim buffer according to trimming mask.
  mAudioData.PopFront(mDataOffset);
  mAudioData.SetLength(mFrames * mChannels);
  mDataOffset = 0;
  mFrames = 0;
  mTrimWindow.reset();
  return std::move(mAudioData);
}

static bool ValidatePlane(const VideoData::YCbCrBuffer::Plane& aPlane) {
  return aPlane.mWidth <= PlanarYCbCrImage::MAX_DIMENSION &&
         aPlane.mHeight <= PlanarYCbCrImage::MAX_DIMENSION &&
         aPlane.mWidth * aPlane.mHeight < MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT &&
         aPlane.mStride > 0 && aPlane.mWidth <= aPlane.mStride;
}

static bool ValidateBufferAndPicture(const VideoData::YCbCrBuffer& aBuffer,
                                     const IntRect& aPicture) {
  // The following situation should never happen unless there is a bug
  // in the decoder
  if (aBuffer.mPlanes[1].mWidth != aBuffer.mPlanes[2].mWidth ||
      aBuffer.mPlanes[1].mHeight != aBuffer.mPlanes[2].mHeight) {
    NS_ERROR("C planes with different sizes");
    return false;
  }

  // The following situations could be triggered by invalid input
  if (aPicture.width <= 0 || aPicture.height <= 0) {
    NS_WARNING("Empty picture rect");
    return false;
  }
  if (!ValidatePlane(aBuffer.mPlanes[0]) ||
      !ValidatePlane(aBuffer.mPlanes[1]) ||
      !ValidatePlane(aBuffer.mPlanes[2])) {
    NS_WARNING("Invalid plane size");
    return false;
  }

  // Ensure the picture size specified in the headers can be extracted out of
  // the frame we've been supplied without indexing out of bounds.
  CheckedUint32 xLimit = aPicture.x + CheckedUint32(aPicture.width);
  CheckedUint32 yLimit = aPicture.y + CheckedUint32(aPicture.height);
  if (!xLimit.isValid() || xLimit.value() > aBuffer.mPlanes[0].mStride ||
      !yLimit.isValid() || yLimit.value() > aBuffer.mPlanes[0].mHeight) {
    // The specified picture dimensions can't be contained inside the video
    // frame, we'll stomp memory if we try to copy it. Fail.
    NS_WARNING("Overflowing picture rect");
    return false;
  }
  return true;
}

VideoData::VideoData(int64_t aOffset, const TimeUnit& aTime,
                     const TimeUnit& aDuration, bool aKeyframe,
                     const TimeUnit& aTimecode, IntSize aDisplay,
                     layers::ImageContainer::FrameID aFrameID)
    : MediaData(Type::VIDEO_DATA, aOffset, aTime, aDuration),
      mDisplay(aDisplay),
      mFrameID(aFrameID),
      mSentToCompositor(false),
      mNextKeyFrameTime(TimeUnit::Invalid()) {
  MOZ_ASSERT(!mDuration.IsNegative(), "Frame must have non-negative duration.");
  mKeyframe = aKeyframe;
  mTimecode = aTimecode;
}

VideoData::~VideoData() = default;

size_t VideoData::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
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

ColorDepth VideoData::GetColorDepth() const {
  if (!mImage) {
    return ColorDepth::COLOR_8;
  }

  return mImage->GetColorDepth();
}

void VideoData::UpdateDuration(const TimeUnit& aDuration) {
  MOZ_ASSERT(!aDuration.IsNegative());
  mDuration = aDuration;
}

void VideoData::UpdateTimestamp(const TimeUnit& aTimestamp) {
  MOZ_ASSERT(!aTimestamp.IsNegative());

  auto updatedDuration = GetEndTime() - aTimestamp;
  MOZ_ASSERT(!updatedDuration.IsNegative());

  mTime = aTimestamp;
  mDuration = updatedDuration;
}

bool VideoData::AdjustForStartTime(const media::TimeUnit& aStartTime) {
  mTime -= aStartTime;
  if (mTime.IsNegative()) {
    NS_WARNING("Negative video start time after time-adjustment!");
  }
  return mTime.IsValid();
}

PlanarYCbCrData ConstructPlanarYCbCrData(const VideoInfo& aInfo,
                                         const VideoData::YCbCrBuffer& aBuffer,
                                         const IntRect& aPicture) {
  const VideoData::YCbCrBuffer::Plane& Y = aBuffer.mPlanes[0];
  const VideoData::YCbCrBuffer::Plane& Cb = aBuffer.mPlanes[1];
  const VideoData::YCbCrBuffer::Plane& Cr = aBuffer.mPlanes[2];

  PlanarYCbCrData data;
  data.mYChannel = Y.mData;
  data.mYStride = AssertedCast<int32_t>(Y.mStride);
  data.mYSkip = AssertedCast<int32_t>(Y.mSkip);
  data.mCbChannel = Cb.mData;
  data.mCrChannel = Cr.mData;
  data.mCbCrStride = AssertedCast<int32_t>(Cb.mStride);
  data.mCbSkip = AssertedCast<int32_t>(Cb.mSkip);
  data.mCrSkip = AssertedCast<int32_t>(Cr.mSkip);
  data.mPictureRect = aPicture;
  data.mStereoMode = aInfo.mStereoMode;
  data.mYUVColorSpace = aBuffer.mYUVColorSpace;
  data.mColorPrimaries = aBuffer.mColorPrimaries;
  data.mColorDepth = aBuffer.mColorDepth;
  if (aInfo.mTransferFunction) {
    data.mTransferFunction = *aInfo.mTransferFunction;
  }
  data.mColorRange = aBuffer.mColorRange;
  data.mChromaSubsampling = aBuffer.mChromaSubsampling;
  return data;
}

/* static */
bool VideoData::SetVideoDataToImage(PlanarYCbCrImage* aVideoImage,
                                    const VideoInfo& aInfo,
                                    const YCbCrBuffer& aBuffer,
                                    const IntRect& aPicture, bool aCopyData) {
  if (!aVideoImage) {
    return false;
  }

  PlanarYCbCrData data = ConstructPlanarYCbCrData(aInfo, aBuffer, aPicture);

  if (aCopyData) {
    return aVideoImage->CopyData(data);
  }
  return aVideoImage->AdoptData(data);
}

/* static */
already_AddRefed<VideoData> VideoData::CreateAndCopyData(
    const VideoInfo& aInfo, ImageContainer* aContainer, int64_t aOffset,
    const TimeUnit& aTime, const TimeUnit& aDuration,
    const YCbCrBuffer& aBuffer, bool aKeyframe, const TimeUnit& aTimecode,
    const IntRect& aPicture, layers::KnowsCompositor* aAllocator) {
  if (!aContainer) {
    // Create a dummy VideoData with no image. This gives us something to
    // send to media streams if necessary.
    RefPtr<VideoData> v(new VideoData(aOffset, aTime, aDuration, aKeyframe,
                                      aTimecode, aInfo.mDisplay, 0));
    return v.forget();
  }

  if (!ValidateBufferAndPicture(aBuffer, aPicture)) {
    return nullptr;
  }

  PerformanceRecorder<PlaybackStage> perfRecorder(MediaStage::CopyDecodedVideo,
                                                  aInfo.mImage.height);
  RefPtr<VideoData> v(new VideoData(aOffset, aTime, aDuration, aKeyframe,
                                    aTimecode, aInfo.mDisplay, 0));

  // Currently our decoder only knows how to output to ImageFormat::PLANAR_YCBCR
  // format.
#if XP_MACOSX
  if (aAllocator && aAllocator->GetWebRenderCompositorType() !=
                        layers::WebRenderCompositor::SOFTWARE) {
    RefPtr<layers::MacIOSurfaceImage> ioImage =
        new layers::MacIOSurfaceImage(nullptr);
    PlanarYCbCrData data = ConstructPlanarYCbCrData(aInfo, aBuffer, aPicture);
    if (ioImage->SetData(aContainer, data)) {
      v->mImage = ioImage;
      perfRecorder.Record();
      return v.forget();
    }
  }
#endif
  if (!v->mImage) {
    v->mImage = aContainer->CreatePlanarYCbCrImage();
  }

  if (!v->mImage) {
    return nullptr;
  }
  NS_ASSERTION(v->mImage->GetFormat() == ImageFormat::PLANAR_YCBCR,
               "Wrong format?");
  PlanarYCbCrImage* videoImage = v->mImage->AsPlanarYCbCrImage();
  MOZ_ASSERT(videoImage);

  if (!VideoData::SetVideoDataToImage(videoImage, aInfo, aBuffer, aPicture,
                                      true /* aCopyData */)) {
    return nullptr;
  }

  perfRecorder.Record();
  return v.forget();
}

/* static */
already_AddRefed<VideoData> VideoData::CreateAndCopyData(
    const VideoInfo& aInfo, ImageContainer* aContainer, int64_t aOffset,
    const TimeUnit& aTime, const TimeUnit& aDuration,
    const YCbCrBuffer& aBuffer, const YCbCrBuffer::Plane& aAlphaPlane,
    bool aKeyframe, const TimeUnit& aTimecode, const IntRect& aPicture) {
  if (!aContainer) {
    // Create a dummy VideoData with no image. This gives us something to
    // send to media streams if necessary.
    RefPtr<VideoData> v(new VideoData(aOffset, aTime, aDuration, aKeyframe,
                                      aTimecode, aInfo.mDisplay, 0));
    return v.forget();
  }

  if (!ValidateBufferAndPicture(aBuffer, aPicture)) {
    return nullptr;
  }

  RefPtr<VideoData> v(new VideoData(aOffset, aTime, aDuration, aKeyframe,
                                    aTimecode, aInfo.mDisplay, 0));

  // Convert from YUVA to BGRA format on the software side.
  RefPtr<layers::SharedRGBImage> videoImage =
      aContainer->CreateSharedRGBImage();
  v->mImage = videoImage;

  if (!v->mImage) {
    return nullptr;
  }
  if (!videoImage->Allocate(
          IntSize(aBuffer.mPlanes[0].mWidth, aBuffer.mPlanes[0].mHeight),
          SurfaceFormat::B8G8R8A8)) {
    return nullptr;
  }

  RefPtr<layers::TextureClient> texture =
      videoImage->GetTextureClient(/* aKnowsCompositor */ nullptr);
  if (!texture) {
    NS_WARNING("Failed to allocate TextureClient");
    return nullptr;
  }

  layers::TextureClientAutoLock autoLock(texture,
                                         layers::OpenMode::OPEN_WRITE_ONLY);
  if (!autoLock.Succeeded()) {
    NS_WARNING("Failed to lock TextureClient");
    return nullptr;
  }

  layers::MappedTextureData buffer;
  if (!texture->BorrowMappedData(buffer)) {
    NS_WARNING("Failed to borrow mapped data");
    return nullptr;
  }

  // The naming convention for libyuv and associated utils is word-order.
  // The naming convention in the gfx stack is byte-order.
  ConvertI420AlphaToARGB(aBuffer.mPlanes[0].mData, aBuffer.mPlanes[1].mData,
                         aBuffer.mPlanes[2].mData, aAlphaPlane.mData,
                         AssertedCast<int>(aBuffer.mPlanes[0].mStride),
                         AssertedCast<int>(aBuffer.mPlanes[1].mStride),
                         buffer.data, buffer.stride, buffer.size.width,
                         buffer.size.height);

  return v.forget();
}

/* static */
already_AddRefed<VideoData> VideoData::CreateFromImage(
    const IntSize& aDisplay, int64_t aOffset, const TimeUnit& aTime,
    const TimeUnit& aDuration, const RefPtr<Image>& aImage, bool aKeyframe,
    const TimeUnit& aTimecode) {
  RefPtr<VideoData> v(new VideoData(aOffset, aTime, aDuration, aKeyframe,
                                    aTimecode, aDisplay, 0));
  v->mImage = aImage;
  return v.forget();
}

MediaRawData::MediaRawData()
    : MediaData(Type::RAW_DATA), mCrypto(mCryptoInternal) {}

MediaRawData::MediaRawData(const uint8_t* aData, size_t aSize)
    : MediaData(Type::RAW_DATA),
      mCrypto(mCryptoInternal),
      mBuffer(aData, aSize) {}

MediaRawData::MediaRawData(const uint8_t* aData, size_t aSize,
                           const uint8_t* aAlphaData, size_t aAlphaSize)
    : MediaData(Type::RAW_DATA),
      mCrypto(mCryptoInternal),
      mBuffer(aData, aSize),
      mAlphaBuffer(aAlphaData, aAlphaSize) {}

MediaRawData::MediaRawData(AlignedByteBuffer&& aData)
    : MediaData(Type::RAW_DATA),
      mCrypto(mCryptoInternal),
      mBuffer(std::move(aData)) {}

MediaRawData::MediaRawData(AlignedByteBuffer&& aData,
                           AlignedByteBuffer&& aAlphaData)
    : MediaData(Type::RAW_DATA),
      mCrypto(mCryptoInternal),
      mBuffer(std::move(aData)),
      mAlphaBuffer(std::move(aAlphaData)) {}

already_AddRefed<MediaRawData> MediaRawData::Clone() const {
  int32_t sampleHeight = 0;
  if (mTrackInfo && mTrackInfo->GetAsVideoInfo()) {
    sampleHeight = mTrackInfo->GetAsVideoInfo()->mImage.height;
  }
  PerformanceRecorder<PlaybackStage> perfRecorder(MediaStage::CopyDemuxedData,
                                                  sampleHeight);
  RefPtr<MediaRawData> s = new MediaRawData;
  s->mTimecode = mTimecode;
  s->mTime = mTime;
  s->mDuration = mDuration;
  s->mOffset = mOffset;
  s->mKeyframe = mKeyframe;
  s->mExtraData = mExtraData;
  s->mCryptoInternal = mCryptoInternal;
  s->mTrackInfo = mTrackInfo;
  s->mEOS = mEOS;
  s->mOriginalPresentationWindow = mOriginalPresentationWindow;
  if (!s->mBuffer.Append(mBuffer.Data(), mBuffer.Length())) {
    return nullptr;
  }
  if (!s->mAlphaBuffer.Append(mAlphaBuffer.Data(), mAlphaBuffer.Length())) {
    return nullptr;
  }
  perfRecorder.Record();
  return s.forget();
}

MediaRawData::~MediaRawData() = default;

size_t MediaRawData::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t size = aMallocSizeOf(this);
  size += mBuffer.SizeOfExcludingThis(aMallocSizeOf);
  return size;
}

UniquePtr<MediaRawDataWriter> MediaRawData::CreateWriter() {
  UniquePtr<MediaRawDataWriter> p(new MediaRawDataWriter(this));
  return p;
}

MediaRawDataWriter::MediaRawDataWriter(MediaRawData* aMediaRawData)
    : mCrypto(aMediaRawData->mCryptoInternal), mTarget(aMediaRawData) {}

bool MediaRawDataWriter::SetSize(size_t aSize) {
  return mTarget->mBuffer.SetLength(aSize);
}

bool MediaRawDataWriter::Prepend(const uint8_t* aData, size_t aSize) {
  return mTarget->mBuffer.Prepend(aData, aSize);
}

bool MediaRawDataWriter::Append(const uint8_t* aData, size_t aSize) {
  return mTarget->mBuffer.Append(aData, aSize);
}

bool MediaRawDataWriter::Replace(const uint8_t* aData, size_t aSize) {
  return mTarget->mBuffer.Replace(aData, aSize);
}

void MediaRawDataWriter::Clear() { mTarget->mBuffer.Clear(); }

uint8_t* MediaRawDataWriter::Data() { return mTarget->mBuffer.Data(); }

size_t MediaRawDataWriter::Size() { return mTarget->Size(); }

void MediaRawDataWriter::PopFront(size_t aSize) {
  mTarget->mBuffer.PopFront(aSize);
}

const char* CryptoSchemeToString(const CryptoScheme& aScheme) {
  switch (aScheme) {
    case CryptoScheme::None:
      return "None";
    case CryptoScheme::Cenc:
      return "Cenc";
    case CryptoScheme::Cbcs:
      return "Cbcs";
    default:
      MOZ_ASSERT_UNREACHABLE();
      return "";
  }
}

}  // namespace mozilla

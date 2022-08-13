/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaEngineVideoStream.h"

#include "mozilla/layers/DcompSurfaceImage.h"
#include "MFMediaEngineUtils.h"
#include "MP4Decoder.h"

namespace mozilla {

#define LOGV(msg, ...)                          \
  MOZ_LOG(gMFMediaEngineLog, LogLevel::Verbose, \
          ("MFMediaStream=%p (%s), " msg, this, \
           this->GetDescriptionName().get(), ##__VA_ARGS__))

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

/* static */
MFMediaEngineVideoStream* MFMediaEngineVideoStream::Create(
    uint64_t aStreamId, const TrackInfo& aInfo, MFMediaSource* aParentSource) {
  MFMediaEngineVideoStream* stream;
  MOZ_ASSERT(aInfo.IsVideo());
  if (FAILED(MakeAndInitialize<MFMediaEngineVideoStream>(
          &stream, aStreamId, aInfo, aParentSource))) {
    return nullptr;
  }
  stream->SetDCompSurfaceHandle(INVALID_HANDLE_VALUE);
  return stream;
}

void MFMediaEngineVideoStream::SetDCompSurfaceHandle(
    HANDLE aDCompSurfaceHandle) {
  MutexAutoLock lock(mMutex);
  if (mDCompSurfaceHandle == aDCompSurfaceHandle) {
    return;
  }
  mDCompSurfaceHandle = aDCompSurfaceHandle;
  mNeedRecreateImage = true;
  LOGV("Set DCompSurfaceHandle, handle=%p", mDCompSurfaceHandle);
}

HRESULT MFMediaEngineVideoStream::CreateMediaType(const TrackInfo& aInfo,
                                                  IMFMediaType** aMediaType) {
  mInfo = *aInfo.GetAsVideoInfo();
  GUID subType = VideoMimeTypeToMediaFoundationSubtype(mInfo.mMimeType);
  NS_ENSURE_TRUE(subType != GUID_NULL, MF_E_TOPO_CODEC_NOT_FOUND);

  // https://docs.microsoft.com/en-us/windows/win32/medfound/media-type-attributes
  ComPtr<IMFMediaType> mediaType;
  RETURN_IF_FAILED(wmf::MFCreateMediaType(&mediaType));
  RETURN_IF_FAILED(mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
  RETURN_IF_FAILED(mediaType->SetGUID(MF_MT_SUBTYPE, subType));

  const auto& image = mInfo.mImage;
  UINT32 imageWidth = image.Width();
  UINT32 imageHeight = image.Height();
  RETURN_IF_FAILED(MFSetAttributeSize(mediaType.Get(), MF_MT_FRAME_SIZE,
                                      imageWidth, imageHeight));

  UINT32 displayWidth = mInfo.mDisplay.Width();
  UINT32 displayHeight = mInfo.mDisplay.Height();
  // PAR = DAR / SAR = (DW / DH) / (SW / SH) = (DW * SH) / (DH * SW)
  RETURN_IF_FAILED(MFSetAttributeRatio(
      mediaType.Get(), MF_MT_PIXEL_ASPECT_RATIO, displayWidth * imageHeight,
      displayHeight * imageWidth));

  // https://docs.microsoft.com/en-us/windows/win32/api/mfobjects/ns-mfobjects-mfoffset
  // The value of the MFOffset number is value + (fract / 65536.0f).
  static const auto ToMFOffset = [](float aValue) {
    MFOffset offset;
    offset.value = static_cast<short>(aValue);
    offset.fract = static_cast<WORD>(65536 * (aValue - offset.value));
    return offset;
  };
  MFVideoArea area;
  area.OffsetX = ToMFOffset(mInfo.ImageRect().x);
  area.OffsetY = ToMFOffset(mInfo.ImageRect().y);
  area.Area = {(LONG)imageWidth, (LONG)imageHeight};
  RETURN_IF_FAILED(mediaType->SetBlob(MF_MT_GEOMETRIC_APERTURE, (UINT8*)&area,
                                      sizeof(area)));

  // https://docs.microsoft.com/en-us/windows/win32/api/mfapi/ne-mfapi-mfvideorotationformat
  static const auto ToMFVideoRotationFormat =
      [](VideoInfo::Rotation aRotation) {
        using Rotation = VideoInfo::Rotation;
        switch (aRotation) {
          case Rotation::kDegree_0:
            return MFVideoRotationFormat_0;
          case Rotation::kDegree_90:
            return MFVideoRotationFormat_90;
          case Rotation::kDegree_180:
            return MFVideoRotationFormat_180;
          default:
            MOZ_ASSERT(aRotation == Rotation::kDegree_270);
            return MFVideoRotationFormat_270;
        }
      };
  const auto rotation = ToMFVideoRotationFormat(mInfo.mRotation);
  RETURN_IF_FAILED(mediaType->SetUINT32(MF_MT_VIDEO_ROTATION, rotation));

  static const auto ToMFVideoTransFunc =
      [](const Maybe<gfx::YUVColorSpace>& aColorSpace) {
        using YUVColorSpace = gfx::YUVColorSpace;
        if (!aColorSpace) {
          return MFVideoTransFunc_Unknown;
        }
        // https://docs.microsoft.com/en-us/windows/win32/api/mfobjects/ne-mfobjects-mfvideotransferfunction
        switch (*aColorSpace) {
          case YUVColorSpace::BT601:
          case YUVColorSpace::BT709:
            return MFVideoTransFunc_709;
          case YUVColorSpace::BT2020:
            return MFVideoTransFunc_2020;
          case YUVColorSpace::Identity:
            return MFVideoTransFunc_sRGB;
          default:
            return MFVideoTransFunc_Unknown;
        }
      };
  const auto transFunc = ToMFVideoTransFunc(mInfo.mColorSpace);
  RETURN_IF_FAILED(mediaType->SetUINT32(MF_MT_TRANSFER_FUNCTION, transFunc));

  static const auto ToMFVideoPrimaries =
      [](const Maybe<gfx::YUVColorSpace>& aColorSpace) {
        using YUVColorSpace = gfx::YUVColorSpace;
        if (!aColorSpace) {
          return MFVideoPrimaries_Unknown;
        }
        // https://docs.microsoft.com/en-us/windows/win32/api/mfobjects/ne-mfobjects-mfvideoprimaries
        switch (*aColorSpace) {
          case YUVColorSpace::BT601:
            return MFVideoPrimaries_Unknown;
          case YUVColorSpace::BT709:
            return MFVideoPrimaries_BT709;
          case YUVColorSpace::BT2020:
            return MFVideoPrimaries_BT2020;
          case YUVColorSpace::Identity:
            return MFVideoPrimaries_BT709;
          default:
            return MFVideoPrimaries_Unknown;
        }
      };
  const auto videoPrimaries = ToMFVideoPrimaries(mInfo.mColorSpace);
  RETURN_IF_FAILED(mediaType->SetUINT32(MF_MT_VIDEO_PRIMARIES, videoPrimaries));

  LOGV(
      "Created video type, subtype=%s, image=[%ux%u], display=[%ux%u], "
      "rotation=%s, tranFuns=%s, primaries=%s",
      GUIDToStr(subType), imageWidth, imageHeight, displayWidth, displayHeight,
      MFVideoRotationFormatToStr(rotation),
      MFVideoTransferFunctionToStr(transFunc),
      MFVideoPrimariesToStr(videoPrimaries));
  *aMediaType = mediaType.Detach();
  return S_OK;
}

bool MFMediaEngineVideoStream::HasEnoughRawData() const {
  // If more than this much raw video is queued, we'll hold off request more
  // video.
  static const int64_t VIDEO_VIDEO_USECS = 500000;
  return mRawDataQueue.Duration() >= VIDEO_VIDEO_USECS;
}

layers::Image* MFMediaEngineVideoStream::GetDcompSurfaceImage() {
  MutexAutoLock lock(mMutex);
  if (!mDCompSurfaceHandle || mDCompSurfaceHandle == INVALID_HANDLE_VALUE) {
    LOGV("Can't create image without a valid dcomp surface handle");
    return nullptr;
  }

  if (!mKnowsCompositor) {
    LOGV("Can't create image without the knows compositor");
    return nullptr;
  }

  if (!mDcompSurfaceImage || mNeedRecreateImage) {
    // DirectComposition only supports RGBA. We use DXGI_FORMAT_B8G8R8A8_UNORM
    // as a default because we can't know what format the dcomp surface is.
    // https://docs.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-idcompositionsurfacefactory-createsurface
    mDcompSurfaceImage = new layers::DcompSurfaceImage(
        mDCompSurfaceHandle, mInfo.mDisplay, gfx::SurfaceFormat::B8G8R8A8,
        mKnowsCompositor);
    mNeedRecreateImage = false;
    LOGV("Created dcomp surface image, handle=%p, size=[%u,%u]",
         mDCompSurfaceHandle, mInfo.mDisplay.Width(), mInfo.mDisplay.Height());
  }
  return mDcompSurfaceImage;
}

already_AddRefed<MediaData> MFMediaEngineVideoStream::OutputData(
    MediaRawData* aSample) {
  RefPtr<layers::Image> image = GetDcompSurfaceImage();
  if (!image) {
    return nullptr;
  }
  return VideoData::CreateFromImage(mInfo.mDisplay, aSample->mOffset,
                                    aSample->mTime, aSample->mDuration, image,
                                    aSample->mKeyframe, aSample->mTimecode);
}

MediaDataDecoder::ConversionRequired MFMediaEngineVideoStream::NeedsConversion()
    const {
  return MP4Decoder::IsH264(mInfo.mMimeType)
             ? MediaDataDecoder::ConversionRequired::kNeedAnnexB
             : MediaDataDecoder::ConversionRequired::kNeedNone;
}

#undef LOGV

}  // namespace mozilla

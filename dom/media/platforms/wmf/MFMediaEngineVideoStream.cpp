/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaEngineVideoStream.h"

#include "mozilla/layers/DcompSurfaceImage.h"
#include "MFMediaEngineUtils.h"

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
  stream->mStreamType =
      GetStreamTypeFromMimeType(aInfo.GetAsVideoInfo()->mMimeType);
  MOZ_ASSERT(StreamTypeIsVideo(stream->mStreamType));
  stream->mHasReceivedInitialCreateDecoderConfig = false;
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
  auto& videoInfo = *aInfo.GetAsVideoInfo();

  GUID subType = VideoMimeTypeToMediaFoundationSubtype(videoInfo.mMimeType);
  NS_ENSURE_TRUE(subType != GUID_NULL, MF_E_TOPO_CODEC_NOT_FOUND);

  // https://docs.microsoft.com/en-us/windows/win32/medfound/media-type-attributes
  ComPtr<IMFMediaType> mediaType;
  RETURN_IF_FAILED(wmf::MFCreateMediaType(&mediaType));
  RETURN_IF_FAILED(mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
  RETURN_IF_FAILED(mediaType->SetGUID(MF_MT_SUBTYPE, subType));

  const auto& image = videoInfo.mImage;
  UINT32 imageWidth = image.Width();
  UINT32 imageHeight = image.Height();
  RETURN_IF_FAILED(MFSetAttributeSize(mediaType.Get(), MF_MT_FRAME_SIZE,
                                      imageWidth, imageHeight));

  UINT32 displayWidth = videoInfo.mDisplay.Width();
  UINT32 displayHeight = videoInfo.mDisplay.Height();
  {
    MutexAutoLock lock(mMutex);
    mDisplay = videoInfo.mDisplay;
  }
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
  area.OffsetX = ToMFOffset(videoInfo.ImageRect().x);
  area.OffsetY = ToMFOffset(videoInfo.ImageRect().y);
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
  const auto rotation = ToMFVideoRotationFormat(videoInfo.mRotation);
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
  const auto transFunc = ToMFVideoTransFunc(videoInfo.mColorSpace);
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
  const auto videoPrimaries = ToMFVideoPrimaries(videoInfo.mColorSpace);
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

already_AddRefed<MediaData> MFMediaEngineVideoStream::OutputData(
    MediaRawData* aSample) {
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
        mDCompSurfaceHandle, mDisplay, gfx::SurfaceFormat::B8G8R8A8,
        mKnowsCompositor);
    mNeedRecreateImage = false;
    LOGV("Created dcomp surface image, handle=%p, size=[%u,%u]",
         mDCompSurfaceHandle, mDisplay.Width(), mDisplay.Height());
  }

  return VideoData::CreateFromImage(mDisplay, aSample->mOffset, aSample->mTime,
                                    aSample->mDuration, mDcompSurfaceImage,
                                    aSample->mKeyframe, aSample->mTimecode);
}

MediaDataDecoder::ConversionRequired MFMediaEngineVideoStream::NeedsConversion()
    const {
  return mStreamType == WMFStreamType::H264
             ? MediaDataDecoder::ConversionRequired::kNeedAnnexB
             : MediaDataDecoder::ConversionRequired::kNeedNone;
}

void MFMediaEngineVideoStream::SetConfig(const TrackInfo& aConfig) {
  MOZ_ASSERT(aConfig.IsVideo());
  ComPtr<MFMediaEngineStream> self = this;
  Unused << mTaskQueue->Dispatch(
      NS_NewRunnableFunction("MFMediaEngineStream::SetConfig",
                             [self, info = *aConfig.GetAsVideoInfo(), this]() {
                               if (mHasReceivedInitialCreateDecoderConfig) {
                                 // Here indicating a new config for video,
                                 // which is triggered by the media change
                                 // monitor, so we need to update the config.
                                 UpdateConfig(info);
                               }
                               mHasReceivedInitialCreateDecoderConfig = true;
                             }));
}

void MFMediaEngineVideoStream::UpdateConfig(const VideoInfo& aInfo) {
  AssertOnTaskQueue();
  // Disable explicit format change event for H264 to allow switching to the
  // new stream without a full re-create, which will be much faster. This is
  // also due to the fact that the MFT decoder can handle some format changes
  // without a format change event. For format changes that the MFT decoder
  // cannot support (e.g. codec change), the playback will fail later with
  // MF_E_INVALIDMEDIATYPE (0xC00D36B4).
  if (mStreamType == WMFStreamType::H264) {
    return;
  }

  LOGV("Video config changed, will update stream descriptor");
  ComPtr<IMFMediaType> mediaType;
  RETURN_VOID_IF_FAILED(CreateMediaType(aInfo, mediaType.GetAddressOf()));
  RETURN_VOID_IF_FAILED(GenerateStreamDescriptor(mediaType));
  RETURN_VOID_IF_FAILED(mMediaEventQueue->QueueEventParamUnk(
      MEStreamFormatChanged, GUID_NULL, S_OK, mediaType.Get()));
}

#undef LOGV

}  // namespace mozilla

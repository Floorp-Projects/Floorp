/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFVideoDecoder.h"
#include "MediaDecoderReader.h"
#include "WMFUtils.h"
#include "ImageContainer.h"
#include "VideoUtils.h"
#include "DXVA2Manager.h"
#include "nsThreadUtils.h"
#include "Layers.h"
#include "mozilla/layers/LayersTypes.h"
#include "prlog.h"

#ifdef PR_LOGGING
PRLogModuleInfo* GetDemuxerLog();
#define LOG(...) PR_LOG(GetDemuxerLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define LOG(...)
#endif

using mozilla::layers::Image;
using mozilla::layers::LayerManager;
using mozilla::layers::LayersBackend;

namespace mozilla {

WMFVideoDecoder::WMFVideoDecoder(bool aDXVAEnabled)
  : mVideoStride(0),
    mVideoWidth(0),
    mVideoHeight(0),
    mLastStreamOffset(0),
    mDXVAEnabled(aDXVAEnabled),
    mUseHwAccel(false)
{
  NS_ASSERTION(!NS_IsMainThread(), "Must be on main thread.");
  MOZ_COUNT_CTOR(WMFVideoDecoder);
}

WMFVideoDecoder::~WMFVideoDecoder()
{
  MOZ_COUNT_DTOR(WMFVideoDecoder);
}

class CreateDXVAManagerEvent : public nsRunnable {
public:
  NS_IMETHOD Run() {
    NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");
    mDXVA2Manager = DXVA2Manager::Create();
    return NS_OK;
  }
  nsAutoPtr<DXVA2Manager> mDXVA2Manager;
};

bool
WMFVideoDecoder::InitializeDXVA(mozilla::layers::LayersBackend aLayersBackend)
{
  // If we use DXVA but aren't running with a D3D layer manager then the
  // readback of decoded video frames from GPU to CPU memory grinds painting
  // to a halt, and makes playback performance *worse*.
  if (!mDXVAEnabled ||
      (aLayersBackend != LayersBackend::LAYERS_D3D9 &&
       aLayersBackend != LayersBackend::LAYERS_D3D10)) {
    return false;
  }

  // The DXVA manager must be created on the main thread.
  nsRefPtr<CreateDXVAManagerEvent> event(new CreateDXVAManagerEvent());
  NS_DispatchToMainThread(event, NS_DISPATCH_SYNC);
  mDXVA2Manager = event->mDXVA2Manager;

  return mDXVA2Manager != nullptr;
}

nsresult
WMFVideoDecoder::Init(mozilla::layers::LayersBackend aLayersBackend,
                      mozilla::layers::ImageContainer* aImageContainer)
{
  NS_ENSURE_ARG_POINTER(aImageContainer);

  bool useDxva= InitializeDXVA(aLayersBackend);

  mDecoder = new MFTDecoder();

  HRESULT hr = mDecoder->Create(CLSID_CMSH264DecoderMFT);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  if (useDxva) {
    RefPtr<IMFAttributes> attr(mDecoder->GetAttributes());

    UINT32 aware = 0;
    if (attr) {
      attr->GetUINT32(MF_SA_D3D_AWARE, &aware);
    }
    if (aware) {
      // TODO: Test if I need this anywhere... Maybe on Vista?
      //hr = attr->SetUINT32(CODECAPI_AVDecVideoAcceleration_H264, TRUE);
      //NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
      MOZ_ASSERT(mDXVA2Manager);
      ULONG_PTR manager = ULONG_PTR(mDXVA2Manager->GetDXVADeviceManager());
      hr = mDecoder->SendMFTMessage(MFT_MESSAGE_SET_D3D_MANAGER, manager);
      if (SUCCEEDED(hr)) {
        mUseHwAccel = true;
      }
    }
  }

  // Setup the input/output media types.
  RefPtr<IMFMediaType> type;
  hr = wmf::MFCreateMediaType(byRef(type));
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  hr = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  hr = type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  hr = type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_MixedInterlaceOrProgressive);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  GUID outputType = mUseHwAccel ? MFVideoFormat_NV12 : MFVideoFormat_YV12;
  hr = mDecoder->SetMediaTypes(type, outputType);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  mImageContainer = aImageContainer;

  LOG("Video Decoder initialized, Using DXVA: %s", (mUseHwAccel ? "Yes" : "No"));

  return NS_OK;
}

HRESULT
WMFVideoDecoder::ConfigureVideoFrameGeometry()
{
  RefPtr<IMFMediaType> mediaType;
  HRESULT hr = mDecoder->GetOutputMediaType(mediaType);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Verify that the video subtype is what we expect it to be.
  // When using hardware acceleration/DXVA2 the video format should
  // be NV12, which is DXVA2's preferred format. For software decoding
  // we use YV12, as that's easier for us to stick into our rendering
  // pipeline than NV12. NV12 has interleaved UV samples, whereas YV12
  // is a planar format.
  GUID videoFormat;
  hr = mediaType->GetGUID(MF_MT_SUBTYPE, &videoFormat);
  NS_ENSURE_TRUE(videoFormat == MFVideoFormat_NV12 || !mUseHwAccel, E_FAIL);
  NS_ENSURE_TRUE(videoFormat == MFVideoFormat_YV12 || mUseHwAccel, E_FAIL);

  nsIntRect pictureRegion;
  hr = GetPictureRegion(mediaType, pictureRegion);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  UINT32 width = 0, height = 0;
  hr = MFGetAttributeSize(mediaType, MF_MT_FRAME_SIZE, &width, &height);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  uint32_t aspectNum = 0, aspectDenom = 0;
  hr = MFGetAttributeRatio(mediaType,
                           MF_MT_PIXEL_ASPECT_RATIO,
                           &aspectNum,
                           &aspectDenom);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Calculate and validate the picture region and frame dimensions after
  // scaling by the pixel aspect ratio.
  nsIntSize frameSize = nsIntSize(width, height);
  nsIntSize displaySize = nsIntSize(pictureRegion.width, pictureRegion.height);
  ScaleDisplayByAspectRatio(displaySize, float(aspectNum) / float(aspectDenom));
  if (!VideoInfo::ValidateVideoRegion(frameSize, pictureRegion, displaySize)) {
    // Video track's frame sizes will overflow. Ignore the video track.
    return E_FAIL;
  }

  // Success! Save state.
  mVideoInfo.mDisplay = displaySize;
  mVideoInfo.mHasVideo = true;
  GetDefaultStride(mediaType, &mVideoStride);
  mVideoWidth = width;
  mVideoHeight = height;
  mPictureRegion = pictureRegion;

  LOG("WMFReader frame geometry frame=(%u,%u) stride=%u picture=(%d, %d, %d, %d) display=(%d,%d) PAR=%d:%d",
      width, height,
      mVideoStride,
      mPictureRegion.x, mPictureRegion.y, mPictureRegion.width, mPictureRegion.height,
      displaySize.width, displaySize.height,
      aspectNum, aspectDenom);

  return S_OK;
}

nsresult
WMFVideoDecoder::Shutdown()
{
  return NS_OK;
}

// Inserts data into the decoder's pipeline.
DecoderStatus
WMFVideoDecoder::Input(const uint8_t* aData,
                       uint32_t aLength,
                       Microseconds aDTS,
                       Microseconds aPTS,
                       int64_t aOffsetInStream)
{
  mLastStreamOffset = aOffsetInStream;
  HRESULT hr = mDecoder->Input(aData, aLength, aPTS);
  if (hr == MF_E_NOTACCEPTING) {
    return DECODE_STATUS_NOT_ACCEPTING;
  }
  NS_ENSURE_TRUE(SUCCEEDED(hr), DECODE_STATUS_ERROR);

  return DECODE_STATUS_OK;
}

HRESULT
WMFVideoDecoder::CreateBasicVideoFrame(IMFSample* aSample,
                                       VideoData** aOutVideoData)
{
  NS_ENSURE_TRUE(aSample, E_POINTER);
  NS_ENSURE_TRUE(aOutVideoData, E_POINTER);

  *aOutVideoData = nullptr;

  HRESULT hr;
  RefPtr<IMFMediaBuffer> buffer;

  // Must convert to contiguous buffer to use IMD2DBuffer interface.
  hr = aSample->ConvertToContiguousBuffer(byRef(buffer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Try and use the IMF2DBuffer interface if available, otherwise fallback
  // to the IMFMediaBuffer interface. Apparently IMF2DBuffer is more efficient,
  // but only some systems (Windows 8?) support it.
  BYTE* data = nullptr;
  LONG stride = 0;
  RefPtr<IMF2DBuffer> twoDBuffer;
  hr = buffer->QueryInterface(static_cast<IMF2DBuffer**>(byRef(twoDBuffer)));
  if (SUCCEEDED(hr)) {
    hr = twoDBuffer->Lock2D(&data, &stride);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  } else {
    hr = buffer->Lock(&data, NULL, NULL);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
    stride = mVideoStride;
  }

  // YV12, planar format: [YYYY....][VVVV....][UUUU....]
  // i.e., Y, then V, then U.
  VideoData::YCbCrBuffer b;

  // Y (Y') plane
  b.mPlanes[0].mData = data;
  b.mPlanes[0].mStride = stride;
  b.mPlanes[0].mHeight = mVideoHeight;
  b.mPlanes[0].mWidth = mVideoWidth;
  b.mPlanes[0].mOffset = 0;
  b.mPlanes[0].mSkip = 0;

  // The V and U planes are stored 16-row-aligned, so we need to add padding
  // to the row heights to ensure the Y'CbCr planes are referenced properly.
  uint32_t padding = 0;
  if (mVideoHeight % 16 != 0) {
    padding = 16 - (mVideoHeight % 16);
  }
  uint32_t y_size = stride * (mVideoHeight + padding);
  uint32_t v_size = stride * (mVideoHeight + padding) / 4;
  uint32_t halfStride = (stride + 1) / 2;
  uint32_t halfHeight = (mVideoHeight + 1) / 2;
  uint32_t halfWidth = (mVideoWidth + 1) / 2;

  // U plane (Cb)
  b.mPlanes[1].mData = data + y_size + v_size;
  b.mPlanes[1].mStride = halfStride;
  b.mPlanes[1].mHeight = halfHeight;
  b.mPlanes[1].mWidth = halfWidth;
  b.mPlanes[1].mOffset = 0;
  b.mPlanes[1].mSkip = 0;

  // V plane (Cr)
  b.mPlanes[2].mData = data + y_size;
  b.mPlanes[2].mStride = halfStride;
  b.mPlanes[2].mHeight = halfHeight;
  b.mPlanes[2].mWidth = halfWidth;
  b.mPlanes[2].mOffset = 0;
  b.mPlanes[2].mSkip = 0;

  Microseconds pts = GetSampleTime(aSample);
  Microseconds duration = GetSampleDuration(aSample);
  VideoData *v = VideoData::Create(mVideoInfo,
                                   mImageContainer,
                                   mLastStreamOffset,
                                   pts,
                                   duration,
                                   b,
                                   false,
                                   -1,
                                   mPictureRegion);
  if (twoDBuffer) {
    twoDBuffer->Unlock2D();
  } else {
    buffer->Unlock();
  }

  *aOutVideoData = v;

  return S_OK;
}

HRESULT
WMFVideoDecoder::CreateD3DVideoFrame(IMFSample* aSample,
                                     VideoData** aOutVideoData)
{
  NS_ENSURE_TRUE(aSample, E_POINTER);
  NS_ENSURE_TRUE(aOutVideoData, E_POINTER);
  NS_ENSURE_TRUE(mDXVA2Manager, E_ABORT);
  NS_ENSURE_TRUE(mUseHwAccel, E_ABORT);

  *aOutVideoData = nullptr;
  HRESULT hr;

  nsRefPtr<Image> image;
  hr = mDXVA2Manager->CopyToImage(aSample,
                                  mPictureRegion,
                                  mImageContainer,
                                  getter_AddRefs(image));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  NS_ENSURE_TRUE(image, E_FAIL);

  Microseconds pts = GetSampleTime(aSample);
  Microseconds duration = GetSampleDuration(aSample);
  VideoData *v = VideoData::CreateFromImage(mVideoInfo,
                                            mImageContainer,
                                            mLastStreamOffset,
                                            pts,
                                            duration,
                                            image.forget(),
                                            false,
                                            -1,
                                            mPictureRegion);

  NS_ENSURE_TRUE(v, E_FAIL);
  *aOutVideoData = v;

  return S_OK;
}

// Blocks until decoded sample is produced by the deoder.
DecoderStatus
WMFVideoDecoder::Output(nsAutoPtr<MediaData>& aOutData)
{
  RefPtr<IMFSample> sample;
  HRESULT hr;

  // Loop until we decode a sample, or an unexpected error that we can't
  // handle occurs.
  while (true) {
    hr = mDecoder->Output(&sample);
    if (SUCCEEDED(hr)) {
      NS_ENSURE_TRUE(sample, DECODE_STATUS_ERROR);
      break;
    }
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      return DECODE_STATUS_NEED_MORE_INPUT;
    }
    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
      // Video stream output type change. Probably a geometric apperature
      // change. Reconfigure the video geometry, so that we output the
      // correct size frames.
      MOZ_ASSERT(!sample);
      hr = ConfigureVideoFrameGeometry();
      NS_ENSURE_TRUE(SUCCEEDED(hr), DECODE_STATUS_ERROR);
      // Loop back and try decoding again...
      continue;
    }
    // Else unexpected error, assert, and bail.
    NS_WARNING("WMFVideoDecoder::Output() unexpected error");
    return DECODE_STATUS_ERROR;
  }

  VideoData* frame = nullptr;
  if (mUseHwAccel) {
    hr = CreateD3DVideoFrame(sample, &frame);
  } else {
    hr = CreateBasicVideoFrame(sample, &frame);
  }
  // Frame should be non null only when we succeeded.
  MOZ_ASSERT((frame != nullptr) == SUCCEEDED(hr));
  NS_ENSURE_TRUE(SUCCEEDED(hr) && frame, DECODE_STATUS_ERROR);

  aOutData = frame;

  return DECODE_STATUS_OK;
}

DecoderStatus
WMFVideoDecoder::Flush()
{
  NS_ENSURE_TRUE(mDecoder, DECODE_STATUS_ERROR);
  HRESULT hr = mDecoder->Flush();
  NS_ENSURE_TRUE(SUCCEEDED(hr), DECODE_STATUS_ERROR);
  return DECODE_STATUS_OK;
}

} // namespace mozilla

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DXVA2Manager.h"
#include <d3d11.h>
#include "nsThreadUtils.h"
#include "ImageContainer.h"
#include "gfxWindowsPlatform.h"
#include "D3D9SurfaceImage.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/layers/D3D11ShareHandleImage.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/Telemetry.h"
#include "MediaTelemetryConstants.h"
#include "mfapi.h"
#include "MediaPrefs.h"
#include "MFTDecoder.h"
#include "DriverCrashGuard.h"
#include "nsPrintfCString.h"
#include "gfxCrashReporterUtils.h"

const CLSID CLSID_VideoProcessorMFT =
{
  0x88753b26,
  0x5b24,
  0x49bd,
  { 0xb2, 0xe7, 0xc, 0x44, 0x5c, 0x78, 0xc9, 0x82 }
};

const GUID MF_XVP_PLAYBACK_MODE =
{
  0x3c5d293f,
  0xad67,
  0x4e29,
  { 0xaf, 0x12, 0xcf, 0x3e, 0x23, 0x8a, 0xcc, 0xe9 }
};

DEFINE_GUID(MF_LOW_LATENCY,
  0x9c27891a, 0xed7a, 0x40e1, 0x88, 0xe8, 0xb2, 0x27, 0x27, 0xa0, 0x24, 0xee);

// R600, R700, Evergreen and Cayman AMD cards. These support DXVA via UVD3 or earlier, and don't
// handle 1080p60 well.
static const DWORD sAMDPreUVD4[] = {
  0x9400, 0x9401, 0x9402, 0x9403, 0x9405, 0x940a, 0x940b, 0x940f, 0x94c0, 0x94c1, 0x94c3, 0x94c4, 0x94c5,
  0x94c6, 0x94c7, 0x94c8, 0x94c9, 0x94cb, 0x94cc, 0x94cd, 0x9580, 0x9581, 0x9583, 0x9586, 0x9587, 0x9588,
  0x9589, 0x958a, 0x958b, 0x958c, 0x958d, 0x958e, 0x958f, 0x9500, 0x9501, 0x9504, 0x9505, 0x9506, 0x9507,
  0x9508, 0x9509, 0x950f, 0x9511, 0x9515, 0x9517, 0x9519, 0x95c0, 0x95c2, 0x95c4, 0x95c5, 0x95c6, 0x95c7,
  0x95c9, 0x95cc, 0x95cd, 0x95ce, 0x95cf, 0x9590, 0x9591, 0x9593, 0x9595, 0x9596, 0x9597, 0x9598, 0x9599,
  0x959b, 0x9610, 0x9611, 0x9612, 0x9613, 0x9614, 0x9615, 0x9616, 0x9710, 0x9711, 0x9712, 0x9713, 0x9714,
  0x9715, 0x9440, 0x9441, 0x9442, 0x9443, 0x9444, 0x9446, 0x944a, 0x944b, 0x944c, 0x944e, 0x9450, 0x9452,
  0x9456, 0x945a, 0x945b, 0x945e, 0x9460, 0x9462, 0x946a, 0x946b, 0x947a, 0x947b, 0x9480, 0x9487, 0x9488,
  0x9489, 0x948a, 0x948f, 0x9490, 0x9491, 0x9495, 0x9498, 0x949c, 0x949e, 0x949f, 0x9540, 0x9541, 0x9542,
  0x954e, 0x954f, 0x9552, 0x9553, 0x9555, 0x9557, 0x955f, 0x94a0, 0x94a1, 0x94a3, 0x94b1, 0x94b3, 0x94b4,
  0x94b5, 0x94b9, 0x68e0, 0x68e1, 0x68e4, 0x68e5, 0x68e8, 0x68e9, 0x68f1, 0x68f2, 0x68f8, 0x68f9, 0x68fa,
  0x68fe, 0x68c0, 0x68c1, 0x68c7, 0x68c8, 0x68c9, 0x68d8, 0x68d9, 0x68da, 0x68de, 0x68a0, 0x68a1, 0x68a8,
  0x68a9, 0x68b0, 0x68b8, 0x68b9, 0x68ba, 0x68be, 0x68bf, 0x6880, 0x6888, 0x6889, 0x688a, 0x688c, 0x688d,
  0x6898, 0x6899, 0x689b, 0x689e, 0x689c, 0x689d, 0x9802, 0x9803, 0x9804, 0x9805, 0x9806, 0x9807, 0x9808,
  0x9809, 0x980a, 0x9640, 0x9641, 0x9647, 0x9648, 0x964a, 0x964b, 0x964c, 0x964e, 0x964f, 0x9642, 0x9643,
  0x9644, 0x9645, 0x9649, 0x6720, 0x6721, 0x6722, 0x6723, 0x6724, 0x6725, 0x6726, 0x6727, 0x6728, 0x6729,
  0x6738, 0x6739, 0x673e, 0x6740, 0x6741, 0x6742, 0x6743, 0x6744, 0x6745, 0x6746, 0x6747, 0x6748, 0x6749,
  0x674a, 0x6750, 0x6751, 0x6758, 0x6759, 0x675b, 0x675d, 0x675f, 0x6840, 0x6841, 0x6842, 0x6843, 0x6849,
  0x6850, 0x6858, 0x6859, 0x6760, 0x6761, 0x6762, 0x6763, 0x6764, 0x6765, 0x6766, 0x6767, 0x6768, 0x6770,
  0x6771, 0x6772, 0x6778, 0x6779, 0x677b, 0x6700, 0x6701, 0x6702, 0x6703, 0x6704, 0x6705, 0x6706, 0x6707,
  0x6708, 0x6709, 0x6718, 0x6719, 0x671c, 0x671d, 0x671f, 0x9900, 0x9901, 0x9903, 0x9904, 0x9905, 0x9906,
  0x9907, 0x9908, 0x9909, 0x990a, 0x990b, 0x990c, 0x990d, 0x990e, 0x990f, 0x9910, 0x9913, 0x9917, 0x9918,
  0x9919, 0x9990, 0x9991, 0x9992, 0x9993, 0x9994, 0x9995, 0x9996, 0x9997, 0x9998, 0x9999, 0x999a, 0x999b,
  0x999c, 0x999d, 0x99a0, 0x99a2, 0x99a4
};

// The size we use for our synchronization surface.
// 16x16 is the size recommended by Microsoft (in the D3D9ExDXGISharedSurf sample) that works
// best to avoid driver bugs.
static const uint32_t kSyncSurfaceSize = 16;

namespace mozilla {

using layers::Image;
using layers::ImageContainer;
using layers::D3D9SurfaceImage;
using layers::D3D9RecycleAllocator;
using layers::D3D11ShareHandleImage;
using layers::D3D11RecycleAllocator;

class D3D9DXVA2Manager : public DXVA2Manager
{
public:
  D3D9DXVA2Manager();
  virtual ~D3D9DXVA2Manager();

  HRESULT Init(nsACString& aFailureReason);

  IUnknown* GetDXVADeviceManager() override;

  // Copies a region (aRegion) of the video frame stored in aVideoSample
  // into an image which is returned by aOutImage.
  HRESULT CopyToImage(IMFSample* aVideoSample,
                      const nsIntRect& aRegion,
                      Image** aOutImage) override;

  bool SupportsConfig(IMFMediaType* aType, float aFramerate) override;

private:
  RefPtr<IDirect3D9Ex> mD3D9;
  RefPtr<IDirect3DDevice9Ex> mDevice;
  RefPtr<IDirect3DDeviceManager9> mDeviceManager;
  RefPtr<D3D9RecycleAllocator> mTextureClientAllocator;
  RefPtr<IDirectXVideoDecoderService> mDecoderService;
  RefPtr<IDirect3DSurface9> mSyncSurface;
  GUID mDecoderGUID;
  UINT32 mResetToken;
  bool mFirstFrame;
  bool mIsAMDPreUVD4;
};

void GetDXVA2ExtendedFormatFromMFMediaType(IMFMediaType *pType,
                                           DXVA2_ExtendedFormat *pFormat)
{
  // Get the interlace mode.
  MFVideoInterlaceMode interlace =
    (MFVideoInterlaceMode)MFGetAttributeUINT32(pType, MF_MT_INTERLACE_MODE, MFVideoInterlace_Unknown);

  if (interlace == MFVideoInterlace_MixedInterlaceOrProgressive) {
    pFormat->SampleFormat = DXVA2_SampleFieldInterleavedEvenFirst;
  } else {
    pFormat->SampleFormat = (UINT)interlace;
  }

  pFormat->VideoChromaSubsampling =
    MFGetAttributeUINT32(pType, MF_MT_VIDEO_CHROMA_SITING, MFVideoChromaSubsampling_Unknown);
  pFormat->NominalRange =
    MFGetAttributeUINT32(pType, MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_Unknown);
  pFormat->VideoTransferMatrix =
    MFGetAttributeUINT32(pType, MF_MT_YUV_MATRIX, MFVideoTransferMatrix_Unknown);
  pFormat->VideoLighting =
    MFGetAttributeUINT32(pType, MF_MT_VIDEO_LIGHTING, MFVideoLighting_Unknown);
  pFormat->VideoPrimaries =
    MFGetAttributeUINT32(pType, MF_MT_VIDEO_PRIMARIES, MFVideoPrimaries_Unknown);
  pFormat->VideoTransferFunction =
    MFGetAttributeUINT32(pType, MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_Unknown);
}

HRESULT ConvertMFTypeToDXVAType(IMFMediaType *pType, DXVA2_VideoDesc *pDesc)
{
  ZeroMemory(pDesc, sizeof(*pDesc));

  // The D3D format is the first DWORD of the subtype GUID.
  GUID subtype = GUID_NULL;
  HRESULT hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  pDesc->Format = (D3DFORMAT)subtype.Data1;

  UINT32 width = 0;
  UINT32 height = 0;
  hr = MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  pDesc->SampleWidth = width;
  pDesc->SampleHeight = height;

  UINT32 fpsNumerator = 0;
  UINT32 fpsDenominator = 0;
  hr = MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, &fpsNumerator, &fpsDenominator);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  pDesc->InputSampleFreq.Numerator = fpsNumerator;
  pDesc->InputSampleFreq.Denominator = fpsDenominator;

  GetDXVA2ExtendedFormatFromMFMediaType(pType, &pDesc->SampleFormat);
  pDesc->OutputFrameFreq = pDesc->InputSampleFreq;
  if ((pDesc->SampleFormat.SampleFormat == DXVA2_SampleFieldInterleavedEvenFirst) ||
      (pDesc->SampleFormat.SampleFormat == DXVA2_SampleFieldInterleavedOddFirst)) {
    pDesc->OutputFrameFreq.Numerator *= 2;
  }

  return S_OK;
}

static const GUID DXVA2_ModeH264_E = {
  0x1b81be68, 0xa0c7, 0x11d3, { 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5 }
};

static const GUID DXVA2_Intel_ModeH264_E = {
  0x604F8E68, 0x4951, 0x4c54, { 0x88, 0xFE, 0xAB, 0xD2, 0x5C, 0x15, 0xB3, 0xD6 }
};

// This tests if a DXVA video decoder can be created for the given media type/resolution.
// It uses the same decoder device (DXVA2_ModeH264_E - DXVA2_ModeH264_VLD_NoFGT) as the H264
// decoder MFT provided by windows (CLSID_CMSH264DecoderMFT) uses, so we can use it to determine
// if the MFT will use software fallback or not.
bool
D3D9DXVA2Manager::SupportsConfig(IMFMediaType* aType, float aFramerate)
{
  MOZ_ASSERT(NS_IsMainThread());
  gfx::D3D9VideoCrashGuard crashGuard;
  if (crashGuard.Crashed()) {
    NS_WARNING("DXVA2D3D9 crash detected");
    return false;
  }

  DXVA2_VideoDesc desc;
  HRESULT hr = ConvertMFTypeToDXVAType(aType, &desc);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  // AMD cards with UVD3 or earlier perform poorly trying to decode 1080p60 in hardware,
  // so use software instead. Pick 45 as an arbitrary upper bound for the framerate we can
  // handle.
  if (mIsAMDPreUVD4 &&
      (desc.SampleWidth >= 1920 || desc.SampleHeight >= 1088) &&
      aFramerate > 45) {
    return false;
  }

  UINT configCount;
  DXVA2_ConfigPictureDecode* configs = nullptr;
  hr = mDecoderService->GetDecoderConfigurations(mDecoderGUID, &desc, nullptr, &configCount, &configs);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  RefPtr<IDirect3DSurface9> surface;
  hr = mDecoderService->CreateSurface(desc.SampleWidth, desc.SampleHeight, 0, (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2'),
                                      D3DPOOL_DEFAULT, 0, DXVA2_VideoDecoderRenderTarget,
                                      surface.StartAssignment(), NULL);
  if (!SUCCEEDED(hr)) {
    CoTaskMemFree(configs);
    return false;
  }

  for (UINT i = 0; i < configCount; i++) {
    RefPtr<IDirectXVideoDecoder> decoder;
    IDirect3DSurface9* surfaces = surface;
    hr = mDecoderService->CreateVideoDecoder(mDecoderGUID, &desc, &configs[i], &surfaces, 1, decoder.StartAssignment());
    if (SUCCEEDED(hr) && decoder) {
      CoTaskMemFree(configs);
      return true;
    }
  }
  CoTaskMemFree(configs);
  return false;
}

D3D9DXVA2Manager::D3D9DXVA2Manager()
  : mResetToken(0)
  , mFirstFrame(true)
  , mIsAMDPreUVD4(false)
{
  MOZ_COUNT_CTOR(D3D9DXVA2Manager);
  MOZ_ASSERT(NS_IsMainThread());
}

D3D9DXVA2Manager::~D3D9DXVA2Manager()
{
  MOZ_COUNT_DTOR(D3D9DXVA2Manager);
  MOZ_ASSERT(NS_IsMainThread());
}

IUnknown*
D3D9DXVA2Manager::GetDXVADeviceManager()
{
  MutexAutoLock lock(mLock);
  return mDeviceManager;
}

HRESULT
D3D9DXVA2Manager::Init(nsACString& aFailureReason)
{
  MOZ_ASSERT(NS_IsMainThread());

  ScopedGfxFeatureReporter reporter("DXVA2D3D9");

  gfx::D3D9VideoCrashGuard crashGuard;
  if (crashGuard.Crashed()) {
    NS_WARNING("DXVA2D3D9 crash detected");
    aFailureReason.AssignLiteral("DXVA2D3D9 crashes detected in the past");
    return E_FAIL;
  }

  // Create D3D9Ex.
  HMODULE d3d9lib = LoadLibraryW(L"d3d9.dll");
  NS_ENSURE_TRUE(d3d9lib, E_FAIL);
  decltype(Direct3DCreate9Ex)* d3d9Create =
    (decltype(Direct3DCreate9Ex)*) GetProcAddress(d3d9lib, "Direct3DCreate9Ex");
  if (!d3d9Create) {
    NS_WARNING("Couldn't find Direct3DCreate9Ex symbol in d3d9.dll");
    aFailureReason.AssignLiteral("Couldn't find Direct3DCreate9Ex symbol in d3d9.dll");
    return E_FAIL;
  }
  RefPtr<IDirect3D9Ex> d3d9Ex;
  HRESULT hr = d3d9Create(D3D_SDK_VERSION, getter_AddRefs(d3d9Ex));
  if (!d3d9Ex) {
    NS_WARNING("Direct3DCreate9 failed");
    aFailureReason.AssignLiteral("Direct3DCreate9 failed");
    return E_FAIL;
  }

  // Ensure we can do the YCbCr->RGB conversion in StretchRect.
  // Fail if we can't.
  hr = d3d9Ex->CheckDeviceFormatConversion(D3DADAPTER_DEFAULT,
                                           D3DDEVTYPE_HAL,
                                           (D3DFORMAT)MAKEFOURCC('N','V','1','2'),
                                           D3DFMT_X8R8G8B8);
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString("CheckDeviceFormatConversion failed with error %X", hr);
    return hr;
  }

  // Create D3D9DeviceEx. We pass null HWNDs here even though the documentation
  // suggests that one of them should not be. At this point in time Chromium
  // does the same thing for video acceleration.
  D3DPRESENT_PARAMETERS params = {0};
  params.BackBufferWidth = 1;
  params.BackBufferHeight = 1;
  params.BackBufferFormat = D3DFMT_A8R8G8B8;
  params.BackBufferCount = 1;
  params.SwapEffect = D3DSWAPEFFECT_DISCARD;
  params.hDeviceWindow = nullptr;
  params.Windowed = TRUE;
  params.Flags = D3DPRESENTFLAG_VIDEO;

  RefPtr<IDirect3DDevice9Ex> device;
  hr = d3d9Ex->CreateDeviceEx(D3DADAPTER_DEFAULT,
                              D3DDEVTYPE_HAL,
                              nullptr,
                              D3DCREATE_FPU_PRESERVE |
                              D3DCREATE_MULTITHREADED |
                              D3DCREATE_MIXED_VERTEXPROCESSING,
                              &params,
                              nullptr,
                              getter_AddRefs(device));
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString("CreateDeviceEx failed with error %X", hr);
    return hr;
  }

  // Ensure we can create queries to synchronize operations between devices.
  // Without this, when we make a copy of the frame in order to share it with
  // another device, we can't be sure that the copy has finished before the
  // other device starts using it.
  RefPtr<IDirect3DQuery9> query;

  hr = device->CreateQuery(D3DQUERYTYPE_EVENT, getter_AddRefs(query));
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString("CreateQuery failed with error %X", hr);
    return hr;
  }

  // Create and initialize IDirect3DDeviceManager9.
  UINT resetToken = 0;
  RefPtr<IDirect3DDeviceManager9> deviceManager;

  hr = wmf::DXVA2CreateDirect3DDeviceManager9(&resetToken,
                                              getter_AddRefs(deviceManager));
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString("DXVA2CreateDirect3DDeviceManager9 failed with error %X", hr);
    return hr;
  }
  hr = deviceManager->ResetDevice(device, resetToken);
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString("IDirect3DDeviceManager9::ResetDevice failed with error %X", hr);
    return hr;
  }

  HANDLE deviceHandle;
  RefPtr<IDirectXVideoDecoderService> decoderService;
  hr = deviceManager->OpenDeviceHandle(&deviceHandle);
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString("IDirect3DDeviceManager9::OpenDeviceHandle failed with error %X", hr);
    return hr;
  }

  hr = deviceManager->GetVideoService(deviceHandle, IID_PPV_ARGS(decoderService.StartAssignment()));
  deviceManager->CloseDeviceHandle(deviceHandle);
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString("IDirectXVideoDecoderServer::GetVideoService failed with error %X", hr);
    return hr;
  }

  UINT deviceCount;
  GUID* decoderDevices = nullptr;
  hr = decoderService->GetDecoderDeviceGuids(&deviceCount, &decoderDevices);
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString("IDirectXVideoDecoderServer::GetDecoderDeviceGuids failed with error %X", hr);
    return hr;
  }

  bool found = false;
  for (UINT i = 0; i < deviceCount; i++) {
    if (decoderDevices[i] == DXVA2_ModeH264_E ||
        decoderDevices[i] == DXVA2_Intel_ModeH264_E) {
      mDecoderGUID = decoderDevices[i];
      found = true;
      break;
    }
  }
  CoTaskMemFree(decoderDevices);

  if (!found) {
    aFailureReason.AssignLiteral("Failed to find an appropriate decoder GUID");
    return E_FAIL;
  }

  D3DADAPTER_IDENTIFIER9 adapter;
  hr = d3d9Ex->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &adapter);
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString("IDirect3D9Ex::GetAdapterIdentifier failed with error %X", hr);
    return hr;
  }

  if (adapter.VendorId == 0x1022 && !MediaPrefs::PDMWMFSkipBlacklist()) {
    for (size_t i = 0; i < MOZ_ARRAY_LENGTH(sAMDPreUVD4); i++) {
      if (adapter.DeviceId == sAMDPreUVD4[i]) {
        mIsAMDPreUVD4 = true;
        break;
      }
    }
  }

  RefPtr<IDirect3DSurface9> syncSurf;
  hr = device->CreateRenderTarget(kSyncSurfaceSize, kSyncSurfaceSize,
                                  D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE,
                                  0, TRUE, getter_AddRefs(syncSurf), NULL);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  mDecoderService = decoderService;

  mResetToken = resetToken;
  mD3D9 = d3d9Ex;
  mDevice = device;
  mDeviceManager = deviceManager;
  mSyncSurface = syncSurf;

  mTextureClientAllocator = new D3D9RecycleAllocator(layers::ImageBridgeChild::GetSingleton().get(),
                                                     mDevice);
  mTextureClientAllocator->SetMaxPoolSize(5);

  Telemetry::Accumulate(Telemetry::MEDIA_DECODER_BACKEND_USED,
                        uint32_t(media::MediaDecoderBackend::WMFDXVA2D3D9));

  reporter.SetSuccessful();

  return S_OK;
}

HRESULT
D3D9DXVA2Manager::CopyToImage(IMFSample* aSample,
                              const nsIntRect& aRegion,
                              Image** aOutImage)
{
  RefPtr<IMFMediaBuffer> buffer;
  HRESULT hr = aSample->GetBufferByIndex(0, getter_AddRefs(buffer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IDirect3DSurface9> surface;
  hr = wmf::MFGetService(buffer,
                         MR_BUFFER_SERVICE,
                         IID_IDirect3DSurface9,
                         getter_AddRefs(surface));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<D3D9SurfaceImage> image = new D3D9SurfaceImage();
  hr = image->AllocateAndCopy(mTextureClientAllocator, surface, aRegion);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IDirect3DSurface9> sourceSurf = image->GetD3D9Surface();

  // Copy a small rect into our sync surface, and then map it
  // to block until decoding/color conversion completes.
  RECT copyRect = { 0, 0, kSyncSurfaceSize, kSyncSurfaceSize };
  hr = mDevice->StretchRect(sourceSurf, &copyRect, mSyncSurface, &copyRect, D3DTEXF_NONE);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  D3DLOCKED_RECT lockedRect;
  hr = mSyncSurface->LockRect(&lockedRect, NULL, D3DLOCK_READONLY);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = mSyncSurface->UnlockRect();
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  image.forget(aOutImage);
  return S_OK;
}

// Count of the number of DXVAManager's we've created. This is also the
// number of videos we're decoding with DXVA. Use on main thread only.
static uint32_t sDXVAVideosCount = 0;

/* static */
DXVA2Manager*
DXVA2Manager::CreateD3D9DXVA(nsACString& aFailureReason)
{
  MOZ_ASSERT(NS_IsMainThread());
  HRESULT hr;

  // DXVA processing takes up a lot of GPU resources, so limit the number of
  // videos we use DXVA with at any one time.
  const uint32_t dxvaLimit = MediaPrefs::PDMWMFMaxDXVAVideos();
  if (sDXVAVideosCount == dxvaLimit) {
    aFailureReason.AssignLiteral("Too many DXVA videos playing");
    return nullptr;
  }

  nsAutoPtr<D3D9DXVA2Manager> d3d9Manager(new D3D9DXVA2Manager());
  hr = d3d9Manager->Init(aFailureReason);
  if (SUCCEEDED(hr)) {
    return d3d9Manager.forget();
  }

  // No hardware accelerated video decoding. :(
  return nullptr;
}

class D3D11DXVA2Manager : public DXVA2Manager
{
public:
  D3D11DXVA2Manager();
  virtual ~D3D11DXVA2Manager();

  HRESULT Init(nsACString& aFailureReason);

  IUnknown* GetDXVADeviceManager() override;

  // Copies a region (aRegion) of the video frame stored in aVideoSample
  // into an image which is returned by aOutImage.
  HRESULT CopyToImage(IMFSample* aVideoSample,
                      const nsIntRect& aRegion,
                      Image** aOutImage) override;

  HRESULT ConfigureForSize(uint32_t aWidth, uint32_t aHeight) override;

  bool IsD3D11() override { return true; }

  bool SupportsConfig(IMFMediaType* aType, float aFramerate) override;

private:
  HRESULT CreateFormatConverter();

  HRESULT CreateOutputSample(RefPtr<IMFSample>& aSample,
                             ID3D11Texture2D* aTexture);

  RefPtr<ID3D11Device> mDevice;
  RefPtr<ID3D11DeviceContext> mContext;
  RefPtr<IMFDXGIDeviceManager> mDXGIDeviceManager;
  RefPtr<MFTDecoder> mTransform;
  RefPtr<D3D11RecycleAllocator> mTextureClientAllocator;
  RefPtr<ID3D11Texture2D> mSyncSurface;
  GUID mDecoderGUID;
  uint32_t mWidth;
  uint32_t mHeight;
  UINT mDeviceManagerToken;
  bool mIsAMDPreUVD4;
};

bool
D3D11DXVA2Manager::SupportsConfig(IMFMediaType* aType, float aFramerate)
{
  MOZ_ASSERT(NS_IsMainThread());
  gfx::D3D11VideoCrashGuard crashGuard;
  if (crashGuard.Crashed()) {
    NS_WARNING("DXVA2D3D9 crash detected");
    return false;
  }

  RefPtr<ID3D11VideoDevice> videoDevice;
  HRESULT hr = mDevice->QueryInterface(static_cast<ID3D11VideoDevice**>(getter_AddRefs(videoDevice)));
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  D3D11_VIDEO_DECODER_DESC desc;
  desc.Guid = mDecoderGUID;

  UINT32 width = 0;
  UINT32 height = 0;
  hr = MFGetAttributeSize(aType, MF_MT_FRAME_SIZE, &width, &height);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  desc.SampleWidth = width;
  desc.SampleHeight = height;

  desc.OutputFormat = DXGI_FORMAT_NV12;

  // AMD cards with UVD3 or earlier perform poorly trying to decode 1080p60 in hardware,
  // so use software instead. Pick 45 as an arbitrary upper bound for the framerate we can
  // handle.
  if (mIsAMDPreUVD4 &&
    (desc.SampleWidth >= 1920 || desc.SampleHeight >= 1088) &&
    aFramerate > 45) {
    return false;
  }

  UINT configCount = 0;
  hr = videoDevice->GetVideoDecoderConfigCount(&desc, &configCount);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  for (UINT i = 0; i < configCount; i++) {
    D3D11_VIDEO_DECODER_CONFIG config;
    hr = videoDevice->GetVideoDecoderConfig(&desc, i, &config);
    if (SUCCEEDED(hr)) {
      RefPtr<ID3D11VideoDecoder> decoder;
      hr = videoDevice->CreateVideoDecoder(&desc, &config, decoder.StartAssignment());
      if (SUCCEEDED(hr) && decoder) {
        return true;
      }
    }
  }
  return false;
}

D3D11DXVA2Manager::D3D11DXVA2Manager()
  : mWidth(0)
  , mHeight(0)
  , mDeviceManagerToken(0)
  , mIsAMDPreUVD4(false)
{
}

D3D11DXVA2Manager::~D3D11DXVA2Manager()
{
}

IUnknown*
D3D11DXVA2Manager::GetDXVADeviceManager()
{
  MutexAutoLock lock(mLock);
  return mDXGIDeviceManager;
}

HRESULT
D3D11DXVA2Manager::Init(nsACString& aFailureReason)
{
  HRESULT hr;

  ScopedGfxFeatureReporter reporter("DXVA2D3D11");

  gfx::D3D11VideoCrashGuard crashGuard;
  if (crashGuard.Crashed()) {
    NS_WARNING("DXVA2D3D11 crash detected");
    aFailureReason.AssignLiteral("DXVA2D3D11 crashes detected in the past");
    return E_FAIL;
  }

  mDevice = gfx::DeviceManagerDx::Get()->CreateDecoderDevice();
  if (!mDevice) {
    aFailureReason.AssignLiteral("Failed to create D3D11 device for decoder");
    return E_FAIL;
  }

  mDevice->GetImmediateContext(getter_AddRefs(mContext));
  if (!mContext) {
    aFailureReason.AssignLiteral("Failed to get immediate context for d3d11 device");
    return E_FAIL;
  }

  hr = wmf::MFCreateDXGIDeviceManager(&mDeviceManagerToken, getter_AddRefs(mDXGIDeviceManager));
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString("MFCreateDXGIDeviceManager failed with code %X", hr);
    return hr;
  }

  hr = mDXGIDeviceManager->ResetDevice(mDevice, mDeviceManagerToken);
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString("IMFDXGIDeviceManager::ResetDevice failed with code %X", hr);
    return hr;
  }

  mTransform = new MFTDecoder();
  hr = mTransform->Create(CLSID_VideoProcessorMFT);
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString("MFTDecoder::Create(CLSID_VideoProcessorMFT) failed with code %X", hr);
    return hr;
  }

  hr = mTransform->SendMFTMessage(MFT_MESSAGE_SET_D3D_MANAGER, ULONG_PTR(mDXGIDeviceManager.get()));
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString("MFTDecoder::SendMFTMessage(MFT_MESSAGE_SET_D3D_MANAGER) failed with code %X", hr);
    return hr;
  }

  RefPtr<ID3D11VideoDevice> videoDevice;
  hr = mDevice->QueryInterface(static_cast<ID3D11VideoDevice**>(getter_AddRefs(videoDevice)));
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString("QI to ID3D11VideoDevice failed with code %X", hr);
    return hr;
  }

  bool found = false;
  UINT profileCount = videoDevice->GetVideoDecoderProfileCount();
  for (UINT i = 0; i < profileCount; i++) {
    GUID id;
    hr = videoDevice->GetVideoDecoderProfile(i, &id);
    if (SUCCEEDED(hr) && (id == DXVA2_ModeH264_E || id == DXVA2_Intel_ModeH264_E)) {
      mDecoderGUID = id;
      found = true;
      break;
    }
  }
  if (!found) {
    aFailureReason.AssignLiteral("Failed to find an appropriate decoder GUID");
    return E_FAIL;
  }

  BOOL nv12Support = false;
  hr = videoDevice->CheckVideoDecoderFormat(&mDecoderGUID, DXGI_FORMAT_NV12, &nv12Support);
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString("CheckVideoDecoderFormat failed with code %X", hr);
    return hr;
  }
  if (!nv12Support) {
    aFailureReason.AssignLiteral("Decoder doesn't support NV12 surfaces");
    return E_FAIL;
  }

  RefPtr<IDXGIDevice> dxgiDevice;
  hr = mDevice->QueryInterface(static_cast<IDXGIDevice**>(getter_AddRefs(dxgiDevice)));
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString("QI to IDXGIDevice failed with code %X", hr);
    return hr;
  }

  RefPtr<IDXGIAdapter> adapter;
  hr = dxgiDevice->GetAdapter(adapter.StartAssignment());
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString("IDXGIDevice::GetAdapter failed with code %X", hr);
    return hr;
  }

  DXGI_ADAPTER_DESC adapterDesc;
  hr = adapter->GetDesc(&adapterDesc);
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString("IDXGIAdapter::GetDesc failed with code %X", hr);
    return hr;
  }

  if (adapterDesc.VendorId == 0x1022 && !MediaPrefs::PDMWMFSkipBlacklist()) {
    for (size_t i = 0; i < MOZ_ARRAY_LENGTH(sAMDPreUVD4); i++) {
      if (adapterDesc.DeviceId == sAMDPreUVD4[i]) {
        mIsAMDPreUVD4 = true;
        break;
      }
    }
  }

  D3D11_TEXTURE2D_DESC desc;
  desc.Width = kSyncSurfaceSize;
  desc.Height = kSyncSurfaceSize;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.BindFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  desc.MiscFlags = 0;

  hr = mDevice->CreateTexture2D(&desc, NULL, getter_AddRefs(mSyncSurface));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  mTextureClientAllocator = new D3D11RecycleAllocator(layers::ImageBridgeChild::GetSingleton().get(),
                                                      mDevice);
  mTextureClientAllocator->SetMaxPoolSize(5);

  Telemetry::Accumulate(Telemetry::MEDIA_DECODER_BACKEND_USED,
                        uint32_t(media::MediaDecoderBackend::WMFDXVA2D3D11));

  reporter.SetSuccessful();

  return S_OK;
}

HRESULT
D3D11DXVA2Manager::CreateOutputSample(RefPtr<IMFSample>& aSample, ID3D11Texture2D* aTexture)
{
  RefPtr<IMFSample> sample;
  HRESULT hr = wmf::MFCreateSample(getter_AddRefs(sample));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IMFMediaBuffer> buffer;
  hr = wmf::MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), aTexture, 0, FALSE, getter_AddRefs(buffer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  sample->AddBuffer(buffer);

  aSample = sample;
  return S_OK;
}

HRESULT
D3D11DXVA2Manager::CopyToImage(IMFSample* aVideoSample,
                               const nsIntRect& aRegion,
                               Image** aOutImage)
{
  NS_ENSURE_TRUE(aVideoSample, E_POINTER);
  NS_ENSURE_TRUE(aOutImage, E_POINTER);

  // Our video frame is stored in a non-sharable ID3D11Texture2D. We need
  // to create a copy of that frame as a sharable resource, save its share
  // handle, and put that handle into the rendering pipeline.

  RefPtr<D3D11ShareHandleImage> image =
    new D3D11ShareHandleImage(gfx::IntSize(mWidth, mHeight), aRegion);
  bool ok = image->AllocateTexture(mTextureClientAllocator);
  NS_ENSURE_TRUE(ok, E_FAIL);

  HRESULT hr = mTransform->Input(aVideoSample);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IMFSample> sample;
  RefPtr<ID3D11Texture2D> texture = image->GetTexture();
  hr = CreateOutputSample(sample, texture);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = mTransform->Output(&sample);

  RefPtr<ID3D11DeviceContext> ctx;
  mDevice->GetImmediateContext(getter_AddRefs(ctx));

  // Copy a small rect into our sync surface, and then map it
  // to block until decoding/color conversion completes.
  D3D11_BOX rect = { 0, 0, 0, kSyncSurfaceSize, kSyncSurfaceSize, 1 };
  ctx->CopySubresourceRegion(mSyncSurface, 0, 0, 0, 0, texture, 0, &rect);

  D3D11_MAPPED_SUBRESOURCE mapped;
  hr = ctx->Map(mSyncSurface, 0, D3D11_MAP_READ, 0, &mapped);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  ctx->Unmap(mSyncSurface, 0);

  image.forget(aOutImage);

  return S_OK;
}

HRESULT ConfigureOutput(IMFMediaType* aOutput, void* aData)
{
  HRESULT hr = aOutput->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = aOutput->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  gfx::IntSize* size = reinterpret_cast<gfx::IntSize*>(aData);
  hr = MFSetAttributeSize(aOutput, MF_MT_FRAME_SIZE, size->width, size->height);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  return S_OK;
}

HRESULT
D3D11DXVA2Manager::ConfigureForSize(uint32_t aWidth, uint32_t aHeight)
{
  mWidth = aWidth;
  mHeight = aHeight;

  RefPtr<IMFMediaType> inputType;
  HRESULT hr = wmf::MFCreateMediaType(getter_AddRefs(inputType));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = inputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = inputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = inputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = inputType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IMFAttributes> attr = mTransform->GetAttributes();

  hr = attr->SetUINT32(MF_XVP_PLAYBACK_MODE, TRUE);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = attr->SetUINT32(MF_LOW_LATENCY, FALSE);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = MFSetAttributeSize(inputType, MF_MT_FRAME_SIZE, aWidth, aHeight);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IMFMediaType> outputType;
  hr = wmf::MFCreateMediaType(getter_AddRefs(outputType));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  gfx::IntSize size(mWidth, mHeight);
  hr = mTransform->SetMediaTypes(inputType, outputType, ConfigureOutput, &size);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  return S_OK;
}

/* static */
DXVA2Manager*
DXVA2Manager::CreateD3D11DXVA(nsACString& aFailureReason)
{
  // DXVA processing takes up a lot of GPU resources, so limit the number of
  // videos we use DXVA with at any one time.
  const uint32_t dxvaLimit = MediaPrefs::PDMWMFMaxDXVAVideos();
  if (sDXVAVideosCount == dxvaLimit) {
    aFailureReason.AssignLiteral("Too many DXVA videos playing");
    return nullptr;
  }

  nsAutoPtr<D3D11DXVA2Manager> manager(new D3D11DXVA2Manager());
  HRESULT hr = manager->Init(aFailureReason);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  return manager.forget();
}

DXVA2Manager::DXVA2Manager()
  : mLock("DXVA2Manager")
{
  MOZ_ASSERT(NS_IsMainThread());
  ++sDXVAVideosCount;
}

DXVA2Manager::~DXVA2Manager()
{
  MOZ_ASSERT(NS_IsMainThread());
  --sDXVAVideosCount;
}

} // namespace mozilla

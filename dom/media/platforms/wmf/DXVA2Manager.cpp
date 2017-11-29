/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <d3d11.h>
#include "DXVA2Manager.h"
#include "D3D9SurfaceImage.h"
#include "DriverCrashGuard.h"
#include "GfxDriverInfo.h"
#include "ImageContainer.h"
#include "MFTDecoder.h"
#include "MediaTelemetryConstants.h"
#include "gfxCrashReporterUtils.h"
#include "gfxPrefs.h"
#include "gfxWindowsPlatform.h"
#include "mfapi.h"
#include "mozilla/Telemetry.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/layers/D3D11ShareHandleImage.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/TextureForwarder.h"
#include "mozilla/layers/TextureD3D11.h"
#include "nsPrintfCString.h"
#include "nsThreadUtils.h"
#include "VideoUtils.h"
#include "mozilla/mscom/EnsureMTA.h"

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

// List of NVidia Telsa GPU known to have broken NV12 rendering.
static const DWORD sNVIDIABrokenNV12[] = {
  0x0191, 0x0193, 0x0194, 0x0197, 0x019d, 0x019e, // G80
  0x0400, 0x0401, 0x0402, 0x0403, 0x0404, 0x0405, 0x0406, 0x0407, 0x0408, 0x0409, // G84
  0x040a, 0x040b, 0x040c, 0x040d, 0x040e, 0x040f,
  0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429, // G86
  0x042a, 0x042b, 0x042c, 0x042d, 0x042e, 0x042f,
  0x0410, 0x0600, 0x0601, 0x0602, 0x0603, 0x0604, 0x0605, 0x0606, 0x0607, 0x0608, // G92
  0x0609, 0x060a, 0x060b, 0x060c, 0x060f, 0x0610, 0x0611, 0x0612, 0x0613, 0x0614,
  0x0615, 0x0617, 0x0618, 0x0619, 0x061a, 0x061b, 0x061c, 0x061d, 0x061e, 0x061f, // G94
  0x0621, 0x0622, 0x0623, 0x0625, 0x0626, 0x0627, 0x0628, 0x062a, 0x062b, 0x062c,
  0x062d, 0x062e, 0x0631, 0x0635, 0x0637, 0x0638, 0x063a,
  0x0640, 0x0641, 0x0643, 0x0644, 0x0645, 0x0646, 0x0647, 0x0648, 0x0649, 0x064a, // G96
  0x064b, 0x064c, 0x0651, 0x0652, 0x0653, 0x0654, 0x0655, 0x0656, 0x0658, 0x0659,
  0x065a, 0x065b, 0x065c, 0x065f,
  0x06e0, 0x06e1, 0x06e2, 0x06e3, 0x06e4, 0x06e6, 0x06e7, 0x06e8, 0x06e9, 0x06ea, // G98
  0x06eb, 0x06ec, 0x06ef, 0x06f1, 0x06f8, 0x06f9, 0x06fa, 0x06fb, 0x06fd, 0x06ff,
  0x05e0, 0x05e1, 0x05e2, 0x05e3, 0x05e6, 0x05e7, 0x05e9, 0x05ea, 0x05eb, 0x05ed, // G200
  0x05ee, 0x05ef,
  0x0840, 0x0844, 0x0845, 0x0846, 0x0847, 0x0848, 0x0849, 0x084a, 0x084b, 0x084c, // MCP77
  0x084d, 0x084f,
  0x0860, 0x0861, 0x0862, 0x0863, 0x0864, 0x0865, 0x0866, 0x0867, 0x0868, 0x0869, // MCP79
  0x086a, 0x086c, 0x086d, 0x086e, 0x086f, 0x0870, 0x0871, 0x0872, 0x0873, 0x0874,
  0x0876, 0x087a, 0x087d, 0x087e, 0x087f,
  0x0ca0, 0x0ca2, 0x0ca3, 0x0ca2, 0x0ca4, 0x0ca5, 0x0ca7, 0x0ca9, 0x0cac, 0x0caf, // GT215
  0x0cb0, 0x0cb1, 0x0cbc,
  0x0a20, 0x0a22, 0x0a23, 0x0a26, 0x0a27, 0x0a28, 0x0a29, 0x0a2a, 0x0a2b, 0x0a2c, // GT216
  0x0a2d, 0x0a32, 0x0a34, 0x0a35, 0x0a38, 0x0a3c,
  0x0a60, 0x0a62, 0x0a63, 0x0a64, 0x0a65, 0x0a66, 0x0a67, 0x0a68, 0x0a69, 0x0a6a, // GT218
  0x0a6c, 0x0a6e, 0x0a6f, 0x0a70, 0x0a71, 0x0a72, 0x0a73, 0x0a74, 0x0a75, 0x0a76,
  0x0a78, 0x0a7a, 0x0a7c, 0x10c0, 0x10c3, 0x10c5, 0x10d8
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
using namespace layers;
using namespace gfx;

class D3D9DXVA2Manager : public DXVA2Manager
{
public:
  D3D9DXVA2Manager();
  virtual ~D3D9DXVA2Manager();

  HRESULT Init(layers::KnowsCompositor* aKnowsCompositor,
               nsACString& aFailureReason);

  IUnknown* GetDXVADeviceManager() override;

  // Copies a region (aRegion) of the video frame stored in aVideoSample
  // into an image which is returned by aOutImage.
  HRESULT CopyToImage(IMFSample* aVideoSample,
                      const gfx::IntRect& aRegion,
                      Image** aOutImage) override;

  bool SupportsConfig(IMFMediaType* aType, float aFramerate) override;

private:
  bool CanCreateDecoder(const DXVA2_VideoDesc& aDesc,
                        const float aFramerate) const;

  already_AddRefed<IDirectXVideoDecoder>
  CreateDecoder(const DXVA2_VideoDesc& aDesc) const;

  RefPtr<IDirect3D9Ex> mD3D9;
  RefPtr<IDirect3DDevice9Ex> mDevice;
  RefPtr<IDirect3DDeviceManager9> mDeviceManager;
  RefPtr<D3D9RecycleAllocator> mTextureClientAllocator;
  RefPtr<IDirectXVideoDecoderService> mDecoderService;
  RefPtr<IDirect3DSurface9> mSyncSurface;
  RefPtr<IDirectXVideoDecoder> mDecoder;
  GUID mDecoderGUID;
  UINT32 mResetToken = 0;
  bool mFirstFrame = true;
};

void GetDXVA2ExtendedFormatFromMFMediaType(IMFMediaType *pType,
                                           DXVA2_ExtendedFormat *pFormat)
{
  // Get the interlace mode.
  MFVideoInterlaceMode interlace = MFVideoInterlaceMode(MFGetAttributeUINT32(
    pType, MF_MT_INTERLACE_MODE, MFVideoInterlace_Unknown));

  if (interlace == MFVideoInterlace_MixedInterlaceOrProgressive) {
    pFormat->SampleFormat = DXVA2_SampleFieldInterleavedEvenFirst;
  } else {
    pFormat->SampleFormat = UINT(interlace);
  }

  pFormat->VideoChromaSubsampling = MFGetAttributeUINT32(
    pType, MF_MT_VIDEO_CHROMA_SITING, MFVideoChromaSubsampling_Unknown);
  pFormat->NominalRange = MFGetAttributeUINT32(
    pType, MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_Unknown);
  pFormat->VideoTransferMatrix = MFGetAttributeUINT32(
    pType, MF_MT_YUV_MATRIX, MFVideoTransferMatrix_Unknown);
  pFormat->VideoLighting =
    MFGetAttributeUINT32(pType, MF_MT_VIDEO_LIGHTING, MFVideoLighting_Unknown);
  pFormat->VideoPrimaries =
    MFGetAttributeUINT32(pType, MF_MT_VIDEO_PRIMARIES, MFVideoPrimaries_Unknown);
  pFormat->VideoTransferFunction = MFGetAttributeUINT32(
    pType, MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_Unknown);
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
  NS_ENSURE_TRUE(width <= MAX_VIDEO_WIDTH, E_FAIL);
  NS_ENSURE_TRUE(height <= MAX_VIDEO_HEIGHT, E_FAIL);
  pDesc->SampleWidth = width;
  pDesc->SampleHeight = height;

  UINT32 fpsNumerator = 0;
  UINT32 fpsDenominator = 0;
  hr = MFGetAttributeRatio(
    pType, MF_MT_FRAME_RATE, &fpsNumerator, &fpsDenominator);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  pDesc->InputSampleFreq.Numerator = fpsNumerator;
  pDesc->InputSampleFreq.Denominator = fpsDenominator;

  GetDXVA2ExtendedFormatFromMFMediaType(pType, &pDesc->SampleFormat);
  pDesc->OutputFrameFreq = pDesc->InputSampleFreq;
  if ((pDesc->SampleFormat.SampleFormat ==
       DXVA2_SampleFieldInterleavedEvenFirst) ||
      (pDesc->SampleFormat.SampleFormat ==
       DXVA2_SampleFieldInterleavedOddFirst)) {
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
  DXVA2_VideoDesc desc;
  HRESULT hr = ConvertMFTypeToDXVAType(aType, &desc);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);
  return CanCreateDecoder(desc, aFramerate);
}

D3D9DXVA2Manager::D3D9DXVA2Manager()
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
D3D9DXVA2Manager::Init(layers::KnowsCompositor* aKnowsCompositor,
                       nsACString& aFailureReason)
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
    (decltype(Direct3DCreate9Ex)*)GetProcAddress(d3d9lib, "Direct3DCreate9Ex");
  if (!d3d9Create) {
    NS_WARNING("Couldn't find Direct3DCreate9Ex symbol in d3d9.dll");
    aFailureReason.AssignLiteral(
      "Couldn't find Direct3DCreate9Ex symbol in d3d9.dll");
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
  hr = d3d9Ex->CheckDeviceFormatConversion(
    D3DADAPTER_DEFAULT,
    D3DDEVTYPE_HAL,
    (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2'),
    D3DFMT_X8R8G8B8);
  if (!SUCCEEDED(hr)) {
    aFailureReason =
      nsPrintfCString("CheckDeviceFormatConversion failed with error %X", hr);
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
                              D3DCREATE_FPU_PRESERVE
                              | D3DCREATE_MULTITHREADED
                              | D3DCREATE_MIXED_VERTEXPROCESSING,
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
    aFailureReason = nsPrintfCString(
      "DXVA2CreateDirect3DDeviceManager9 failed with error %X", hr);
    return hr;
  }
  hr = deviceManager->ResetDevice(device, resetToken);
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString(
      "IDirect3DDeviceManager9::ResetDevice failed with error %X", hr);
    return hr;
  }

  HANDLE deviceHandle;
  RefPtr<IDirectXVideoDecoderService> decoderService;
  hr = deviceManager->OpenDeviceHandle(&deviceHandle);
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString(
      "IDirect3DDeviceManager9::OpenDeviceHandle failed with error %X", hr);
    return hr;
  }

  hr = deviceManager->GetVideoService(
    deviceHandle, IID_PPV_ARGS(decoderService.StartAssignment()));
  deviceManager->CloseDeviceHandle(deviceHandle);
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString(
      "IDirectXVideoDecoderServer::GetVideoService failed with error %X", hr);
    return hr;
  }

  UINT deviceCount;
  GUID* decoderDevices = nullptr;
  hr = decoderService->GetDecoderDeviceGuids(&deviceCount, &decoderDevices);
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString(
      "IDirectXVideoDecoderServer::GetDecoderDeviceGuids failed with error %X",
      hr);
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
    aFailureReason = nsPrintfCString(
      "IDirect3D9Ex::GetAdapterIdentifier failed with error %X", hr);
    return hr;
  }

  if ((adapter.VendorId == 0x1022  || adapter.VendorId == 0x1002) &&
      !gfxPrefs::PDMWMFSkipBlacklist()) {
    for (const auto& model : sAMDPreUVD4) {
      if (adapter.DeviceId == model) {
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

  if (layers::ImageBridgeChild::GetSingleton()) {
    // There's no proper KnowsCompositor for ImageBridge currently (and it
    // implements the interface), so just use that if it's available.
    mTextureClientAllocator = new D3D9RecycleAllocator(
      layers::ImageBridgeChild::GetSingleton().get(), mDevice);
  } else {
    mTextureClientAllocator =
      new D3D9RecycleAllocator(aKnowsCompositor, mDevice);
  }
  mTextureClientAllocator->SetMaxPoolSize(5);

  Telemetry::Accumulate(Telemetry::MEDIA_DECODER_BACKEND_USED,
                        uint32_t(media::MediaDecoderBackend::WMFDXVA2D3D9));

  reporter.SetSuccessful();

  return S_OK;
}

HRESULT
D3D9DXVA2Manager::CopyToImage(IMFSample* aSample,
                              const gfx::IntRect& aRegion,
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
  hr = mDevice->StretchRect(
    sourceSurf, &copyRect, mSyncSurface, &copyRect, D3DTEXF_NONE);
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
DXVA2Manager::CreateD3D9DXVA(layers::KnowsCompositor* aKnowsCompositor,
                             nsACString& aFailureReason)
{
  MOZ_ASSERT(NS_IsMainThread());
  HRESULT hr;

  // DXVA processing takes up a lot of GPU resources, so limit the number of
  // videos we use DXVA with at any one time.
  uint32_t dxvaLimit = gfxPrefs::PDMWMFMaxDXVAVideos();

  if (sDXVAVideosCount == dxvaLimit) {
    aFailureReason.AssignLiteral("Too many DXVA videos playing");
    return nullptr;
  }

  nsAutoPtr<D3D9DXVA2Manager> d3d9Manager(new D3D9DXVA2Manager());
  hr = d3d9Manager->Init(aKnowsCompositor, aFailureReason);
  if (SUCCEEDED(hr)) {
    return d3d9Manager.forget();
  }

  // No hardware accelerated video decoding. :(
  return nullptr;
}

bool
D3D9DXVA2Manager::CanCreateDecoder(const DXVA2_VideoDesc& aDesc,
                                   const float aFramerate) const
{
  MOZ_ASSERT(NS_IsMainThread());
  if (IsUnsupportedResolution(
        aDesc.SampleWidth, aDesc.SampleHeight, aFramerate)) {
    return false;
  }
  RefPtr<IDirectXVideoDecoder> decoder = CreateDecoder(aDesc);
  return decoder.get() != nullptr;
}

already_AddRefed<IDirectXVideoDecoder>
D3D9DXVA2Manager::CreateDecoder(const DXVA2_VideoDesc& aDesc) const
{
  MOZ_ASSERT(NS_IsMainThread());
  gfx::D3D9VideoCrashGuard crashGuard;
  if (crashGuard.Crashed()) {
    NS_WARNING("DXVA2D3D9 crash detected");
    return nullptr;
  }

  UINT configCount;
  DXVA2_ConfigPictureDecode* configs = nullptr;
  HRESULT hr = mDecoderService->GetDecoderConfigurations(
    mDecoderGUID, &aDesc, nullptr, &configCount, &configs);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  RefPtr<IDirect3DSurface9> surface;
  hr = mDecoderService->CreateSurface(aDesc.SampleWidth,
                                      aDesc.SampleHeight,
                                      0,
                                      (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2'),
                                      D3DPOOL_DEFAULT,
                                      0,
                                      DXVA2_VideoDecoderRenderTarget,
                                      surface.StartAssignment(),
                                      NULL);
  if (!SUCCEEDED(hr)) {
    CoTaskMemFree(configs);
    return nullptr;
  }

  for (UINT i = 0; i < configCount; i++) {
    RefPtr<IDirectXVideoDecoder> decoder;
    IDirect3DSurface9* surfaces = surface;
    hr = mDecoderService->CreateVideoDecoder(mDecoderGUID,
                                             &aDesc,
                                             &configs[i],
                                             &surfaces,
                                             1,
                                             decoder.StartAssignment());
    CoTaskMemFree(configs);
    return decoder.forget();
  }

  CoTaskMemFree(configs);
  return nullptr;
}

class D3D11DXVA2Manager : public DXVA2Manager
{
public:
  virtual ~D3D11DXVA2Manager();

  HRESULT Init(layers::KnowsCompositor* aKnowsCompositor,
               nsACString& aFailureReason,
               ID3D11Device* aDevice);
  HRESULT InitInternal(layers::KnowsCompositor* aKnowsCompositor,
                       nsACString& aFailureReason,
                       ID3D11Device* aDevice);

  IUnknown* GetDXVADeviceManager() override;

  // Copies a region (aRegion) of the video frame stored in aVideoSample
  // into an image which is returned by aOutImage.
  HRESULT CopyToImage(IMFSample* aVideoSample,
                      const gfx::IntRect& aRegion,
                      Image** aOutImage) override;

  virtual HRESULT CopyToBGRATexture(ID3D11Texture2D *aInTexture,
                                    ID3D11Texture2D** aOutTexture);

  HRESULT ConfigureForSize(uint32_t aWidth, uint32_t aHeight) override;

  bool IsD3D11() override { return true; }

  bool SupportsConfig(IMFMediaType* aType, float aFramerate) override;

private:
  HRESULT CreateFormatConverter();

  HRESULT CreateOutputSample(RefPtr<IMFSample>& aSample,
                             ID3D11Texture2D* aTexture);

  bool CanCreateDecoder(const D3D11_VIDEO_DECODER_DESC& aDesc,
                        const float aFramerate) const;

  already_AddRefed<ID3D11VideoDecoder>
  CreateDecoder(const D3D11_VIDEO_DECODER_DESC& aDesc) const;

  RefPtr<ID3D11Device> mDevice;
  RefPtr<ID3D11DeviceContext> mContext;
  RefPtr<IMFDXGIDeviceManager> mDXGIDeviceManager;
  RefPtr<MFTDecoder> mTransform;
  RefPtr<D3D11RecycleAllocator> mTextureClientAllocator;
  RefPtr<ID3D11VideoDecoder> mDecoder;
  RefPtr<layers::SyncObjectClient> mSyncObject;
  GUID mDecoderGUID;
  uint32_t mWidth = 0;
  uint32_t mHeight = 0;
  UINT mDeviceManagerToken = 0;
  bool mConfiuredForSize = false;
};

bool
D3D11DXVA2Manager::SupportsConfig(IMFMediaType* aType, float aFramerate)
{
  MOZ_ASSERT(NS_IsMainThread());
  D3D11_VIDEO_DECODER_DESC desc;
  desc.Guid = mDecoderGUID;
  desc.OutputFormat = DXGI_FORMAT_NV12;
  HRESULT hr = MFGetAttributeSize(aType, MF_MT_FRAME_SIZE, &desc.SampleWidth, &desc.SampleHeight);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);
  NS_ENSURE_TRUE(desc.SampleWidth <= MAX_VIDEO_WIDTH, false);
  NS_ENSURE_TRUE(desc.SampleHeight <= MAX_VIDEO_HEIGHT, false);

  return CanCreateDecoder(desc, aFramerate);
}

D3D11DXVA2Manager::~D3D11DXVA2Manager() { }

IUnknown*
D3D11DXVA2Manager::GetDXVADeviceManager()
{
  MutexAutoLock lock(mLock);
  return mDXGIDeviceManager;
}
HRESULT
D3D11DXVA2Manager::Init(layers::KnowsCompositor* aKnowsCompositor,
                        nsACString& aFailureReason,
                        ID3D11Device* aDevice)
{
  if (!NS_IsMainThread()) {
    // DXVA Managers used for full video have to be initialized on the main
    // thread. Managers initialized off the main thread have to pass a device
    // and can only be used for color conversion.
    MOZ_ASSERT(aDevice);
    return InitInternal(aKnowsCompositor, aFailureReason, aDevice);
  }

  HRESULT hr;
  ScopedGfxFeatureReporter reporter("DXVA2D3D11");

  gfx::D3D11VideoCrashGuard crashGuard;
  if (crashGuard.Crashed()) {
    NS_WARNING("DXVA2D3D11 crash detected");
    aFailureReason.AssignLiteral("DXVA2D3D11 crashes detected in the past");
    return E_FAIL;
  }

  hr = InitInternal(aKnowsCompositor, aFailureReason, aDevice);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  if (layers::ImageBridgeChild::GetSingleton() || !aKnowsCompositor) {
    // There's no proper KnowsCompositor for ImageBridge currently (and it
    // implements the interface), so just use that if it's available.
    mTextureClientAllocator = new D3D11RecycleAllocator(
      layers::ImageBridgeChild::GetSingleton().get(), mDevice);

    if (ImageBridgeChild::GetSingleton() && gfxPrefs::PDMWMFUseSyncTexture() &&
        mDevice != DeviceManagerDx::Get()->GetCompositorDevice()) {
      // We use a syncobject to avoid the cost of the mutex lock when compositing,
      // and because it allows color conversion ocurring directly from this texture
      // DXVA does not seem to accept IDXGIKeyedMutex textures as input.
      mSyncObject =
        layers::SyncObjectClient::CreateSyncObjectClient(
            layers::ImageBridgeChild::GetSingleton()->
              GetTextureFactoryIdentifier().mSyncHandle,
            mDevice);
    }
  } else {
    mTextureClientAllocator =
      new D3D11RecycleAllocator(aKnowsCompositor, mDevice);
    if (gfxPrefs::PDMWMFUseSyncTexture()) {
      // We use a syncobject to avoid the cost of the mutex lock when compositing,
      // and because it allows color conversion ocurring directly from this texture
      // DXVA does not seem to accept IDXGIKeyedMutex textures as input.
      mSyncObject =
        layers::SyncObjectClient::CreateSyncObjectClient(
            aKnowsCompositor->GetTextureFactoryIdentifier().mSyncHandle,
            mDevice);
    }
  }
  mTextureClientAllocator->SetMaxPoolSize(5);

  Telemetry::Accumulate(Telemetry::MEDIA_DECODER_BACKEND_USED,
                        uint32_t(media::MediaDecoderBackend::WMFDXVA2D3D11));

  reporter.SetSuccessful();

  return S_OK;
}

HRESULT
D3D11DXVA2Manager::InitInternal(layers::KnowsCompositor* aKnowsCompositor,
                                nsACString& aFailureReason,
                                ID3D11Device* aDevice)
{
  HRESULT hr;

  mDevice = aDevice;

  if (!mDevice) {
    mDevice = gfx::DeviceManagerDx::Get()->CreateDecoderDevice();
    if (!mDevice) {
      aFailureReason.AssignLiteral("Failed to create D3D11 device for decoder");
      return E_FAIL;
    }
  }

  RefPtr<ID3D10Multithread> mt;
  hr = mDevice->QueryInterface((ID3D10Multithread**)getter_AddRefs(mt));
  NS_ENSURE_TRUE(SUCCEEDED(hr) && mt, hr);
  mt->SetMultithreadProtected(TRUE);

  mDevice->GetImmediateContext(getter_AddRefs(mContext));

  hr = wmf::MFCreateDXGIDeviceManager(&mDeviceManagerToken,
                                      getter_AddRefs(mDXGIDeviceManager));
  if (!SUCCEEDED(hr)) {
    aFailureReason =
      nsPrintfCString("MFCreateDXGIDeviceManager failed with code %X", hr);
    return hr;
  }

  hr = mDXGIDeviceManager->ResetDevice(mDevice, mDeviceManagerToken);
  if (!SUCCEEDED(hr)) {
    aFailureReason = nsPrintfCString(
      "IMFDXGIDeviceManager::ResetDevice failed with code %X", hr);
    return hr;
  }

  // The IMFTransform interface used by MFTDecoder is documented to require to
  // run on an MTA thread.
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ee892371(v=vs.85).aspx#components
  // The main thread (where this function is called) is STA, not MTA.
  RefPtr<MFTDecoder> mft;
  mozilla::mscom::EnsureMTA([&]() -> void {
    mft = new MFTDecoder();
    hr = mft->Create(CLSID_VideoProcessorMFT);

    if (!SUCCEEDED(hr)) {
      aFailureReason = nsPrintfCString(
        "MFTDecoder::Create(CLSID_VideoProcessorMFT) failed with code %X", hr);
      return;
    }

    hr = mft->SendMFTMessage(MFT_MESSAGE_SET_D3D_MANAGER,
                             ULONG_PTR(mDXGIDeviceManager.get()));
    if (!SUCCEEDED(hr)) {
      aFailureReason = nsPrintfCString("MFTDecoder::SendMFTMessage(MFT_MESSAGE_"
                                       "SET_D3D_MANAGER) failed with code %X",
                                       hr);
      return;
    }
  });

  if (!SUCCEEDED(hr)) {
    return hr;
  }
  mTransform = mft;

  RefPtr<ID3D11VideoDevice> videoDevice;
  hr = mDevice->QueryInterface(
    static_cast<ID3D11VideoDevice**>(getter_AddRefs(videoDevice)));
  if (!SUCCEEDED(hr)) {
    aFailureReason =
      nsPrintfCString("QI to ID3D11VideoDevice failed with code %X", hr);
    return hr;
  }

  bool found = false;
  UINT profileCount = videoDevice->GetVideoDecoderProfileCount();
  for (UINT i = 0; i < profileCount; i++) {
    GUID id;
    hr = videoDevice->GetVideoDecoderProfile(i, &id);
    if (SUCCEEDED(hr) &&
        (id == DXVA2_ModeH264_E || id == DXVA2_Intel_ModeH264_E)) {
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
  hr = videoDevice->CheckVideoDecoderFormat(
    &mDecoderGUID, DXGI_FORMAT_NV12, &nv12Support);
  if (!SUCCEEDED(hr)) {
    aFailureReason =
      nsPrintfCString("CheckVideoDecoderFormat failed with code %X", hr);
    return hr;
  }
  if (!nv12Support) {
    aFailureReason.AssignLiteral("Decoder doesn't support NV12 surfaces");
    return E_FAIL;
  }

  RefPtr<IDXGIDevice> dxgiDevice;
  hr = mDevice->QueryInterface(
    static_cast<IDXGIDevice**>(getter_AddRefs(dxgiDevice)));
  if (!SUCCEEDED(hr)) {
    aFailureReason =
      nsPrintfCString("QI to IDXGIDevice failed with code %X", hr);
    return hr;
  }

  RefPtr<IDXGIAdapter> adapter;
  hr = dxgiDevice->GetAdapter(adapter.StartAssignment());
  if (!SUCCEEDED(hr)) {
    aFailureReason =
      nsPrintfCString("IDXGIDevice::GetAdapter failed with code %X", hr);
    return hr;
  }

  DXGI_ADAPTER_DESC adapterDesc;
  hr = adapter->GetDesc(&adapterDesc);
  if (!SUCCEEDED(hr)) {
    aFailureReason =
      nsPrintfCString("IDXGIAdapter::GetDesc failed with code %X", hr);
    return hr;
  }

  if ((adapterDesc.VendorId == 0x1022 || adapterDesc.VendorId == 0x1002) &&
      !gfxPrefs::PDMWMFSkipBlacklist()) {
    for (const auto& model : sAMDPreUVD4) {
      if (adapterDesc.DeviceId == model) {
        mIsAMDPreUVD4 = true;
        break;
      }
    }
  }

  return S_OK;
}

HRESULT
D3D11DXVA2Manager::CreateOutputSample(RefPtr<IMFSample>& aSample,
                                      ID3D11Texture2D* aTexture)
{
  RefPtr<IMFSample> sample;
  HRESULT hr = wmf::MFCreateSample(getter_AddRefs(sample));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IMFMediaBuffer> buffer;
  hr = wmf::MFCreateDXGISurfaceBuffer(
    __uuidof(ID3D11Texture2D), aTexture, 0, FALSE, getter_AddRefs(buffer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = sample->AddBuffer(buffer);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  aSample = sample;
  return S_OK;
}

HRESULT
D3D11DXVA2Manager::CopyToImage(IMFSample* aVideoSample,
                               const gfx::IntRect& aRegion,
                               Image** aOutImage)
{
  NS_ENSURE_TRUE(aVideoSample, E_POINTER);
  NS_ENSURE_TRUE(aOutImage, E_POINTER);
  MOZ_ASSERT(mTextureClientAllocator);

  RefPtr<D3D11ShareHandleImage> image =
    new D3D11ShareHandleImage(gfx::IntSize(mWidth, mHeight), aRegion);
  bool ok = image->AllocateTexture(mTextureClientAllocator, mDevice);
  NS_ENSURE_TRUE(ok, E_FAIL);

  RefPtr<TextureClient> client = image->GetTextureClient(ImageBridgeChild::GetSingleton().get());
  NS_ENSURE_TRUE(client, E_FAIL);

  RefPtr<IDXGIKeyedMutex> mutex;
  HRESULT hr = S_OK;
  RefPtr<ID3D11Texture2D> texture = image->GetTexture();

  texture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mutex));

  {
    AutoTextureLock(mutex, hr, 2000);
    if (mutex && (FAILED(hr) || hr == WAIT_TIMEOUT || hr == WAIT_ABANDONED)) {
      return hr;
    }

    if (!mutex && mDevice != DeviceManagerDx::Get()->GetCompositorDevice()) {
      NS_ENSURE_TRUE(mSyncObject, E_FAIL);
    }

    if (client && client->GetFormat() == SurfaceFormat::NV12) {
      // Our video frame is stored in a non-sharable ID3D11Texture2D. We need
      // to create a copy of that frame as a sharable resource, save its share
      // handle, and put that handle into the rendering pipeline.

      RefPtr<IMFMediaBuffer> buffer;
      hr = aVideoSample->GetBufferByIndex(0, getter_AddRefs(buffer));
      NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

      RefPtr<IMFDXGIBuffer> dxgiBuf;
      hr = buffer->QueryInterface((IMFDXGIBuffer**)getter_AddRefs(dxgiBuf));
      NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

      RefPtr<ID3D11Texture2D> tex;
      hr = dxgiBuf->GetResource(__uuidof(ID3D11Texture2D), getter_AddRefs(tex));
      NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

      UINT index;
      dxgiBuf->GetSubresourceIndex(&index);
      mContext->CopySubresourceRegion(texture, 0, 0, 0, 0, tex, index, nullptr);
    } else {
      // Our video sample is in NV12 format but our output texture is in BGRA.
      // Use MFT to do color conversion.
      hr = E_FAIL;
      mozilla::mscom::EnsureMTA(
        [&]() -> void { hr = mTransform->Input(aVideoSample); });
      NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

      RefPtr<IMFSample> sample;
      hr = CreateOutputSample(sample, texture);
      NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

      hr = E_FAIL;
      mozilla::mscom::EnsureMTA(
        [&]() -> void { hr = mTransform->Output(&sample); });
      NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
    }
  }

  if (!mutex && mDevice != DeviceManagerDx::Get()->GetCompositorDevice() && mSyncObject) {
    // It appears some race-condition may allow us to arrive here even when mSyncObject
    // is null. It's better to avoid that crash.
    client->SyncWithObject(mSyncObject);
    if (!mSyncObject->Synchronize(true)) {
      return DXGI_ERROR_DEVICE_RESET;
    }
  }

  image.forget(aOutImage);

  return S_OK;
}

HRESULT
D3D11DXVA2Manager::CopyToBGRATexture(ID3D11Texture2D *aInTexture,
                                     ID3D11Texture2D** aOutTexture)
{
  NS_ENSURE_TRUE(aInTexture, E_POINTER);
  NS_ENSURE_TRUE(aOutTexture, E_POINTER);

  HRESULT hr;
  RefPtr<ID3D11Texture2D> texture, inTexture;

  inTexture = aInTexture;

  CD3D11_TEXTURE2D_DESC desc;
  aInTexture->GetDesc(&desc);
  hr = ConfigureForSize(desc.Width, desc.Height);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IDXGIKeyedMutex> mutex;
  inTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mutex));
  // The rest of this function will not work if inTexture implements
  // IDXGIKeyedMutex! In that case case we would have to copy to a
  // non-mutex using texture.

  if (mutex) {
    RefPtr<ID3D11Texture2D> newTexture;

    desc.MiscFlags = 0;
    hr = mDevice->CreateTexture2D(&desc, nullptr, getter_AddRefs(newTexture));
    NS_ENSURE_TRUE(SUCCEEDED(hr) && newTexture, E_FAIL);

    hr = mutex->AcquireSync(0, 2000);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

    mContext->CopyResource(newTexture, inTexture);

    mutex->ReleaseSync(0);
    inTexture = newTexture;
  }

  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

  hr = mDevice->CreateTexture2D(&desc, nullptr, getter_AddRefs(texture));
  NS_ENSURE_TRUE(SUCCEEDED(hr) && texture, E_FAIL);

  RefPtr<IMFSample> inputSample;
  wmf::MFCreateSample(getter_AddRefs(inputSample));

  // If these aren't set the decoder fails.
  inputSample->SetSampleTime(10);
  inputSample->SetSampleDuration(10000);

  RefPtr<IMFMediaBuffer> inputBuffer;
  hr = wmf::MFCreateDXGISurfaceBuffer(
    __uuidof(ID3D11Texture2D), inTexture, 0, FALSE, getter_AddRefs(inputBuffer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  inputSample->AddBuffer(inputBuffer);

  hr = E_FAIL;
  mozilla::mscom::EnsureMTA(
    [&]() -> void { hr = mTransform->Input(inputSample); });
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IMFSample> outputSample;
  hr = CreateOutputSample(outputSample, texture);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = E_FAIL;
  mozilla::mscom::EnsureMTA(
    [&]() -> void { hr = mTransform->Output(&outputSample); });
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  texture.forget(aOutTexture);

  return S_OK;
}

HRESULT ConfigureOutput(IMFMediaType* aOutput, void* aData)
{
  HRESULT hr =
    aOutput->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
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
  if (mConfiuredForSize && aWidth == mWidth && aHeight == mHeight) {
    // If the size hasn't changed, don't reconfigure.
    return S_OK;
  }

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

  RefPtr<IMFAttributes> attr;
  mozilla::mscom::EnsureMTA(
    [&]() -> void { attr = mTransform->GetAttributes(); });
  NS_ENSURE_TRUE(attr != nullptr, E_FAIL);

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
  hr = E_FAIL;
  mozilla::mscom::EnsureMTA([&]() -> void {
    hr =
      mTransform->SetMediaTypes(inputType, outputType, ConfigureOutput, &size);
  });
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  mConfiuredForSize = true;

  return S_OK;
}

bool
D3D11DXVA2Manager::CanCreateDecoder(const D3D11_VIDEO_DECODER_DESC& aDesc,
                                    const float aFramerate) const
{
  MOZ_ASSERT(NS_IsMainThread());
  if (IsUnsupportedResolution(
        aDesc.SampleWidth, aDesc.SampleHeight, aFramerate)) {
    return false;
  }
  RefPtr<ID3D11VideoDecoder> decoder = CreateDecoder(aDesc);
  return decoder.get() != nullptr;
}

already_AddRefed<ID3D11VideoDecoder>
D3D11DXVA2Manager::CreateDecoder(const D3D11_VIDEO_DECODER_DESC& aDesc) const
{
  MOZ_ASSERT(NS_IsMainThread());
  gfx::D3D11VideoCrashGuard crashGuard;
  if (crashGuard.Crashed()) {
    NS_WARNING("DXVA2D3D9 crash detected");
    return nullptr;
  }

  RefPtr<ID3D11VideoDevice> videoDevice;
  HRESULT hr = mDevice->QueryInterface(
    static_cast<ID3D11VideoDevice**>(getter_AddRefs(videoDevice)));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  UINT configCount = 0;
  hr = videoDevice->GetVideoDecoderConfigCount(&aDesc, &configCount);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  for (UINT i = 0; i < configCount; i++) {
    D3D11_VIDEO_DECODER_CONFIG config;
    hr = videoDevice->GetVideoDecoderConfig(&aDesc, i, &config);
    if (SUCCEEDED(hr)) {
      RefPtr<ID3D11VideoDecoder> decoder;
      hr = videoDevice->CreateVideoDecoder(
        &aDesc, &config, decoder.StartAssignment());
      return decoder.forget();
    }
  }
  return nullptr;
}

/* static */
DXVA2Manager*
DXVA2Manager::CreateD3D11DXVA(layers::KnowsCompositor* aKnowsCompositor,
                              nsACString& aFailureReason,
                              ID3D11Device* aDevice)
{
  // DXVA processing takes up a lot of GPU resources, so limit the number of
  // videos we use DXVA with at any one time.
  uint32_t dxvaLimit = gfxPrefs::PDMWMFMaxDXVAVideos();

  if (sDXVAVideosCount == dxvaLimit) {
    aFailureReason.AssignLiteral("Too many DXVA videos playing");
    return nullptr;
  }

  nsAutoPtr<D3D11DXVA2Manager> manager(new D3D11DXVA2Manager());
  HRESULT hr = manager->Init(aKnowsCompositor, aFailureReason, aDevice);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  return manager.forget();
}

DXVA2Manager::DXVA2Manager()
  : mLock("DXVA2Manager")
{
  if (NS_IsMainThread()) {
    ++sDXVAVideosCount;
  }
}

DXVA2Manager::~DXVA2Manager()
{
  if (NS_IsMainThread()) {
    --sDXVAVideosCount;
  }
}

bool
DXVA2Manager::IsUnsupportedResolution(const uint32_t& aWidth,
                                      const uint32_t& aHeight,
                                      const float& aFramerate) const
{
  // AMD cards with UVD3 or earlier perform poorly trying to decode 1080p60 in
  // hardware, so use software instead. Pick 45 as an arbitrary upper bound for
  // the framerate we can handle.
  return !gfxPrefs::PDMWMFAMDHighResEnabled() && mIsAMDPreUVD4 &&
         (aWidth >= 1920 || aHeight >= 1088) &&
         aFramerate > 45;
}

/* static */ bool
DXVA2Manager::IsNV12Supported(uint32_t aVendorID,
                              uint32_t aDeviceID,
                              const nsAString& aDriverVersionString)
{
  if (aVendorID == 0x1022 || aVendorID == 0x1002) {
    // AMD
    // Block old cards regardless of driver version.
    for (const auto& model : sAMDPreUVD4) {
      if (aDeviceID == model) {
        return false;
      }
    }
    // AMD driver earlier than 21.19.411.0 have bugs in their handling of NV12
    // surfaces.
    uint64_t driverVersion;
    if (!widget::ParseDriverVersion(aDriverVersionString, &driverVersion) ||
        driverVersion < widget::V(21, 19, 411, 0)) {
      return false;
    }
  } else if (aVendorID == 0x10DE) {
    // NVidia
    for (const auto& model : sNVIDIABrokenNV12) {
      if (aDeviceID == model) {
        return false;
      }
    }
  }
  return true;
}

} // namespace mozilla

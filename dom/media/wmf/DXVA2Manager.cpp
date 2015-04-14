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
#include "mozilla/layers/D3D11ShareHandleImage.h"
#include "mozilla/Preferences.h"
#include "mfapi.h"
#include "MFTDecoder.h"

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

namespace mozilla {

using layers::Image;
using layers::ImageContainer;
using layers::D3D9SurfaceImage;
using layers::D3D11ShareHandleImage;

class D3D9DXVA2Manager : public DXVA2Manager
{
public:
  D3D9DXVA2Manager();
  virtual ~D3D9DXVA2Manager();

  HRESULT Init();

  IUnknown* GetDXVADeviceManager() override;

  // Copies a region (aRegion) of the video frame stored in aVideoSample
  // into an image which is returned by aOutImage.
  HRESULT CopyToImage(IMFSample* aVideoSample,
                      const nsIntRect& aRegion,
                      ImageContainer* aContainer,
                      Image** aOutImage) override;

private:
  nsRefPtr<IDirect3D9Ex> mD3D9;
  nsRefPtr<IDirect3DDevice9Ex> mDevice;
  nsRefPtr<IDirect3DDeviceManager9> mDeviceManager;
  UINT32 mResetToken;
};

D3D9DXVA2Manager::D3D9DXVA2Manager()
  : mResetToken(0)
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
D3D9DXVA2Manager::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Create D3D9Ex.
  HMODULE d3d9lib = LoadLibraryW(L"d3d9.dll");
  NS_ENSURE_TRUE(d3d9lib, E_FAIL);
  decltype(Direct3DCreate9Ex)* d3d9Create =
    (decltype(Direct3DCreate9Ex)*) GetProcAddress(d3d9lib, "Direct3DCreate9Ex");
  nsRefPtr<IDirect3D9Ex> d3d9Ex;
  HRESULT hr = d3d9Create(D3D_SDK_VERSION, getter_AddRefs(d3d9Ex));
  if (!d3d9Ex) {
    NS_WARNING("Direct3DCreate9 failed");
    return E_FAIL;
  }

  // Ensure we can do the YCbCr->RGB conversion in StretchRect.
  // Fail if we can't.
  hr = d3d9Ex->CheckDeviceFormatConversion(D3DADAPTER_DEFAULT,
                                           D3DDEVTYPE_HAL,
                                           (D3DFORMAT)MAKEFOURCC('N','V','1','2'),
                                           D3DFMT_X8R8G8B8);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Create D3D9DeviceEx.
  D3DPRESENT_PARAMETERS params = {0};
  params.BackBufferWidth = 1;
  params.BackBufferHeight = 1;
  params.BackBufferFormat = D3DFMT_UNKNOWN;
  params.BackBufferCount = 1;
  params.SwapEffect = D3DSWAPEFFECT_DISCARD;
  params.hDeviceWindow = ::GetShellWindow();
  params.Windowed = TRUE;
  params.Flags = D3DPRESENTFLAG_VIDEO;

  nsRefPtr<IDirect3DDevice9Ex> device;
  hr = d3d9Ex->CreateDeviceEx(D3DADAPTER_DEFAULT,
                              D3DDEVTYPE_HAL,
                              ::GetShellWindow(),
                              D3DCREATE_FPU_PRESERVE |
                              D3DCREATE_MULTITHREADED |
                              D3DCREATE_MIXED_VERTEXPROCESSING,
                              &params,
                              nullptr,
                              getter_AddRefs(device));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Ensure we can create queries to synchronize operations between devices.
  // Without this, when we make a copy of the frame in order to share it with
  // another device, we can't be sure that the copy has finished before the
  // other device starts using it.
  nsRefPtr<IDirect3DQuery9> query;

  hr = device->CreateQuery(D3DQUERYTYPE_EVENT, getter_AddRefs(query));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Create and initialize IDirect3DDeviceManager9.
  UINT resetToken = 0;
  nsRefPtr<IDirect3DDeviceManager9> deviceManager;

  hr = wmf::DXVA2CreateDirect3DDeviceManager9(&resetToken,
                                              getter_AddRefs(deviceManager));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  hr = deviceManager->ResetDevice(device, resetToken);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  mResetToken = resetToken;
  mD3D9 = d3d9Ex;
  mDevice = device;
  mDeviceManager = deviceManager;

  return S_OK;
}

HRESULT
D3D9DXVA2Manager::CopyToImage(IMFSample* aSample,
                              const nsIntRect& aRegion,
                              ImageContainer* aImageContainer,
                              Image** aOutImage)
{
  nsRefPtr<IMFMediaBuffer> buffer;
  HRESULT hr = aSample->GetBufferByIndex(0, getter_AddRefs(buffer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  nsRefPtr<IDirect3DSurface9> surface;
  hr = wmf::MFGetService(buffer,
                         MR_BUFFER_SERVICE,
                         IID_IDirect3DSurface9,
                         getter_AddRefs(surface));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  nsRefPtr<Image> image = aImageContainer->CreateImage(ImageFormat::D3D9_RGB32_TEXTURE);
  NS_ENSURE_TRUE(image, E_FAIL);
  NS_ASSERTION(image->GetFormat() == ImageFormat::D3D9_RGB32_TEXTURE,
               "Wrong format?");

  D3D9SurfaceImage* videoImage = static_cast<D3D9SurfaceImage*>(image.get());
  hr = videoImage->SetData(D3D9SurfaceImage::Data(surface, aRegion));

  image.forget(aOutImage);

  return S_OK;
}

// Count of the number of DXVAManager's we've created. This is also the
// number of videos we're decoding with DXVA. Use on main thread only.
static uint32_t sDXVAVideosCount = 0;

/* static */
DXVA2Manager*
DXVA2Manager::CreateD3D9DXVA()
{
  MOZ_ASSERT(NS_IsMainThread());
  HRESULT hr;

  // DXVA processing takes up a lot of GPU resources, so limit the number of
  // videos we use DXVA with at any one time.
  const uint32_t dxvaLimit =
    Preferences::GetInt("media.windows-media-foundation.max-dxva-videos", 8);
  if (sDXVAVideosCount == dxvaLimit) {
    return nullptr;
  }

  nsAutoPtr<D3D9DXVA2Manager> d3d9Manager(new D3D9DXVA2Manager());
  hr = d3d9Manager->Init();
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

  HRESULT Init();

  IUnknown* GetDXVADeviceManager() override;

  // Copies a region (aRegion) of the video frame stored in aVideoSample
  // into an image which is returned by aOutImage.
  HRESULT CopyToImage(IMFSample* aVideoSample,
                      const nsIntRect& aRegion,
                      ImageContainer* aContainer,
                      Image** aOutImage) override;

  virtual HRESULT ConfigureForSize(uint32_t aWidth, uint32_t aHeight) override;

private:
  HRESULT CreateFormatConverter();

  HRESULT CreateOutputSample(RefPtr<IMFSample>& aSample,
                             RefPtr<ID3D11Texture2D>& aTexture);

  RefPtr<ID3D11Device> mDevice;
  RefPtr<ID3D11DeviceContext> mContext;
  RefPtr<IMFDXGIDeviceManager> mDXGIDeviceManager;
  RefPtr<MFTDecoder> mTransform;
  uint32_t mWidth;
  uint32_t mHeight;
  UINT mDeviceManagerToken;
};

D3D11DXVA2Manager::D3D11DXVA2Manager()
  : mWidth(0)
  , mHeight(0)
  , mDeviceManagerToken(0)
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
D3D11DXVA2Manager::Init()
{
  HRESULT hr;

  mDevice = gfxWindowsPlatform::GetPlatform()->CreateD3D11DecoderDevice();
  NS_ENSURE_TRUE(mDevice, E_FAIL);

  mDevice->GetImmediateContext(byRef(mContext));
  NS_ENSURE_TRUE(mContext, E_FAIL);

  hr = wmf::MFCreateDXGIDeviceManager(&mDeviceManagerToken, byRef(mDXGIDeviceManager));
  NS_ENSURE_TRUE(SUCCEEDED(hr),hr);

  hr = mDXGIDeviceManager->ResetDevice(mDevice, mDeviceManagerToken);
  NS_ENSURE_TRUE(SUCCEEDED(hr),hr);

  mTransform = new MFTDecoder();
  hr = mTransform->Create(CLSID_VideoProcessorMFT);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = mTransform->SendMFTMessage(MFT_MESSAGE_SET_D3D_MANAGER, ULONG_PTR(mDXGIDeviceManager.get()));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  return S_OK;
}

HRESULT
D3D11DXVA2Manager::CreateOutputSample(RefPtr<IMFSample>& aSample, RefPtr<ID3D11Texture2D>& aTexture)
{
  RefPtr<IMFSample> sample;
  HRESULT hr = wmf::MFCreateSample(byRef(sample));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  D3D11_TEXTURE2D_DESC desc;
  desc.Width = mWidth;
  desc.Height = mHeight;
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  RefPtr<ID3D11Texture2D> texture;
  hr = mDevice->CreateTexture2D(&desc, nullptr, byRef(texture));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IMFMediaBuffer> buffer;
  hr = wmf::MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), texture, 0, FALSE, byRef(buffer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  sample->AddBuffer(buffer);

  aSample = sample;
  aTexture = texture;
  return S_OK;
}

HRESULT
D3D11DXVA2Manager::CopyToImage(IMFSample* aVideoSample,
                               const nsIntRect& aRegion,
                               ImageContainer* aContainer,
                               Image** aOutImage)
{
  NS_ENSURE_TRUE(aVideoSample, E_POINTER);
  NS_ENSURE_TRUE(aContainer, E_POINTER);
  NS_ENSURE_TRUE(aOutImage, E_POINTER);

  // Our video frame is stored in a non-sharable ID3D11Texture2D. We need
  // to create a copy of that frame as a sharable resource, save its share
  // handle, and put that handle into the rendering pipeline.

  HRESULT hr = mTransform->Input(aVideoSample);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IMFSample> sample;
  RefPtr<ID3D11Texture2D> texture;
  hr = CreateOutputSample(sample, texture);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IDXGIKeyedMutex> keyedMutex;
  hr = texture->QueryInterface(static_cast<IDXGIKeyedMutex**>(byRef(keyedMutex)));
  NS_ENSURE_TRUE(SUCCEEDED(hr) && keyedMutex, hr);

  hr = keyedMutex->AcquireSync(0, INFINITE);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = mTransform->Output(&sample);

  keyedMutex->ReleaseSync(0);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  ImageFormat format = ImageFormat::D3D11_SHARE_HANDLE_TEXTURE;
  nsRefPtr<Image> image(aContainer->CreateImage(format));
  NS_ENSURE_TRUE(image, E_FAIL);
  NS_ASSERTION(image->GetFormat() == ImageFormat::D3D11_SHARE_HANDLE_TEXTURE,
               "Wrong format?");

  D3D11ShareHandleImage* videoImage = static_cast<D3D11ShareHandleImage*>(image.get());
  hr = videoImage->SetData(D3D11ShareHandleImage::Data(texture, mDevice, mContext, aRegion));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

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
  HRESULT hr = wmf::MFCreateMediaType(byRef(inputType));
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
  hr = wmf::MFCreateMediaType(byRef(outputType));
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
DXVA2Manager::CreateD3D11DXVA()
{
  // DXVA processing takes up a lot of GPU resources, so limit the number of
  // videos we use DXVA with at any one time.
  const uint32_t dxvaLimit =
    Preferences::GetInt("media.windows-media-foundation.max-dxva-videos", 8);
  if (sDXVAVideosCount == dxvaLimit) {
    return nullptr;
  }

  nsAutoPtr<D3D11DXVA2Manager> manager(new D3D11DXVA2Manager());
  HRESULT hr = manager->Init();
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

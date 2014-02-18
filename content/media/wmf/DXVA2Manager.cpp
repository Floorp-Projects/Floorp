/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DXVA2Manager.h"
#include "nsThreadUtils.h"
#include "ImageContainer.h"
#include "D3D9SurfaceImage.h"
#include "mozilla/Preferences.h"

namespace mozilla {

using layers::Image;
using layers::ImageContainer;
using layers::D3D9SurfaceImage;

class D3D9DXVA2Manager : public DXVA2Manager
{
public:
  D3D9DXVA2Manager();
  virtual ~D3D9DXVA2Manager();

  HRESULT Init();

  IUnknown* GetDXVADeviceManager() MOZ_OVERRIDE;

  // Copies a region (aRegion) of the video frame stored in aVideoSample
  // into an image which is returned by aOutImage.
  HRESULT CopyToImage(IMFSample* aVideoSample,
                      const nsIntRect& aRegion,
                      ImageContainer* aContainer,
                      Image** aOutImage) MOZ_OVERRIDE;

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
DXVA2Manager::Create()
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

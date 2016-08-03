/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IMFYCbCrImage.h"
#include "mozilla/layers/TextureD3D11.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/gfx/DeviceManagerD3D11.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/TextureClient.h"
#include "d3d9.h"

namespace mozilla {
namespace layers {

IMFYCbCrImage::IMFYCbCrImage(IMFMediaBuffer* aBuffer, IMF2DBuffer* a2DBuffer)
  : RecyclingPlanarYCbCrImage(nullptr)
  , mBuffer(aBuffer)
  , m2DBuffer(a2DBuffer)
{}

IMFYCbCrImage::~IMFYCbCrImage()
{
  if (m2DBuffer) {
    m2DBuffer->Unlock2D();
  }
  else {
    mBuffer->Unlock();
  }
}

struct AutoLockTexture
{
  AutoLockTexture(ID3D11Texture2D* aTexture)
  {
    aTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mMutex));
    HRESULT hr = mMutex->AcquireSync(0, 10000);
    if (hr == WAIT_TIMEOUT) {
      MOZ_CRASH("GFX: IMFYCbCrImage timeout");
    }

    if (FAILED(hr)) {
      NS_WARNING("Failed to lock the texture");
    }
  }

  ~AutoLockTexture()
  {
    HRESULT hr = mMutex->ReleaseSync(0);
    if (FAILED(hr)) {
      NS_WARNING("Failed to unlock the texture");
    }
  }

  RefPtr<IDXGIKeyedMutex> mMutex;
};

static already_AddRefed<IDirect3DTexture9>
InitTextures(IDirect3DDevice9* aDevice,
             const IntSize &aSize,
            _D3DFORMAT aFormat,
            RefPtr<IDirect3DSurface9>& aSurface,
            HANDLE& aHandle,
            D3DLOCKED_RECT& aLockedRect)
{
  if (!aDevice) {
    return nullptr;
  }

  RefPtr<IDirect3DTexture9> result;
  if (FAILED(aDevice->CreateTexture(aSize.width, aSize.height,
                                    1, 0, aFormat, D3DPOOL_DEFAULT,
                                    getter_AddRefs(result), &aHandle))) {
    return nullptr;
  }
  if (!result) {
    return nullptr;
  }

  RefPtr<IDirect3DTexture9> tmpTexture;
  if (FAILED(aDevice->CreateTexture(aSize.width, aSize.height,
                                    1, 0, aFormat, D3DPOOL_SYSTEMMEM,
                                    getter_AddRefs(tmpTexture), nullptr))) {
    return nullptr;
  }
  if (!tmpTexture) {
    return nullptr;
  }

  tmpTexture->GetSurfaceLevel(0, getter_AddRefs(aSurface));
  if (FAILED(aSurface->LockRect(&aLockedRect, nullptr, 0)) ||
      !aLockedRect.pBits) {
    NS_WARNING("Could not lock surface");
    return nullptr;
  }

  return result.forget();
}

static bool
FinishTextures(IDirect3DDevice9* aDevice,
               IDirect3DTexture9* aTexture,
               IDirect3DSurface9* aSurface)
{
  if (!aDevice) {
    return false;
  }

  HRESULT hr = aSurface->UnlockRect();
  if (FAILED(hr)) {
    return false;
  }

  RefPtr<IDirect3DSurface9> dstSurface;
  hr = aTexture->GetSurfaceLevel(0, getter_AddRefs(dstSurface));
  if (FAILED(hr)) {
    return false;
  }

  hr = aDevice->UpdateSurface(aSurface, nullptr, dstSurface, nullptr);
  if (FAILED(hr)) {
    return false;
  }
  return true;
}

static bool UploadData(IDirect3DDevice9* aDevice,
                       RefPtr<IDirect3DTexture9>& aTexture,
                       HANDLE& aHandle,
                       uint8_t* aSrc,
                       const gfx::IntSize& aSrcSize,
                       int32_t aSrcStride)
{
  RefPtr<IDirect3DSurface9> surf;
  D3DLOCKED_RECT rect;
  aTexture = InitTextures(aDevice, aSrcSize, D3DFMT_A8, surf, aHandle, rect);
  if (!aTexture) {
    return false;
  }

  if (aSrcStride == rect.Pitch) {
    memcpy(rect.pBits, aSrc, rect.Pitch * aSrcSize.height);
  } else {
    for (int i = 0; i < aSrcSize.height; i++) {
      memcpy((uint8_t*)rect.pBits + i * rect.Pitch,
             aSrc + i * aSrcStride,
             aSrcSize.width);
    }
  }

  return FinishTextures(aDevice, aTexture, surf);
}

TextureClient*
IMFYCbCrImage::GetD3D9TextureClient(CompositableClient* aClient)
{
  IDirect3DDevice9* device = gfxWindowsPlatform::GetPlatform()->GetD3D9Device();
  if (!device) {
    return nullptr;
  }

  RefPtr<IDirect3DTexture9> textureY;
  HANDLE shareHandleY = 0;
  if (!UploadData(device, textureY, shareHandleY,
                  mData.mYChannel, mData.mYSize, mData.mYStride)) {
    return nullptr;
  }

  RefPtr<IDirect3DTexture9> textureCb;
  HANDLE shareHandleCb = 0;
  if (!UploadData(device, textureCb, shareHandleCb,
                  mData.mCbChannel, mData.mCbCrSize, mData.mCbCrStride)) {
    return nullptr;
  }

  RefPtr<IDirect3DTexture9> textureCr;
  HANDLE shareHandleCr = 0;
  if (!UploadData(device, textureCr, shareHandleCr,
                  mData.mCrChannel, mData.mCbCrSize, mData.mCbCrStride)) {
    return nullptr;
  }

  RefPtr<IDirect3DQuery9> query;
  HRESULT hr = device->CreateQuery(D3DQUERYTYPE_EVENT, getter_AddRefs(query));
  hr = query->Issue(D3DISSUE_END);

  int iterations = 0;
  bool valid = false;
  while (iterations < 10) {
    HRESULT hr = query->GetData(nullptr, 0, D3DGETDATA_FLUSH);
    if (hr == S_FALSE) {
      Sleep(1);
      iterations++;
      continue;
    }
    if (hr == S_OK) {
      valid = true;
    }
    break;
  }

  if (!valid) {
    return nullptr;
  }

  mTextureClient = TextureClient::CreateWithData(
    DXGIYCbCrTextureData::Create(aClient->GetForwarder(),
                                 TextureFlags::DEFAULT,
                                 textureY, textureCb, textureCr,
                                 shareHandleY, shareHandleCb, shareHandleCr,
                                 GetSize(), mData.mYSize, mData.mCbCrSize),
    TextureFlags::DEFAULT,
    aClient->GetForwarder()
  );

  return mTextureClient;
}

TextureClient*
IMFYCbCrImage::GetTextureClient(CompositableClient* aClient)
{
  if (mTextureClient) {
    return mTextureClient;
  }

  RefPtr<ID3D11Device> device =
    gfx::DeviceManagerD3D11::Get()->GetContentDevice();

  LayersBackend backend = aClient->GetForwarder()->GetCompositorBackendType();
  if (!device || backend != LayersBackend::LAYERS_D3D11) {
    if (backend == LayersBackend::LAYERS_D3D9 ||
        backend == LayersBackend::LAYERS_D3D11) {
      return GetD3D9TextureClient(aClient);
    }
    return nullptr;
  }

  if (mData.mYStride < 0 || mData.mCbCrStride < 0) {
    // D3D11 only supports unsigned stride values.
    return nullptr;
  }

  CD3D11_TEXTURE2D_DESC newDesc(DXGI_FORMAT_R8_UNORM,
                                mData.mYSize.width, mData.mYSize.height, 1, 1);

  newDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  RefPtr<ID3D11Texture2D> textureY;
  D3D11_SUBRESOURCE_DATA yData = { mData.mYChannel, (UINT)mData.mYStride, 0 };
  HRESULT hr = device->CreateTexture2D(&newDesc, &yData, getter_AddRefs(textureY));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  newDesc.Width = mData.mCbCrSize.width;
  newDesc.Height = mData.mCbCrSize.height;

  RefPtr<ID3D11Texture2D> textureCb;
  D3D11_SUBRESOURCE_DATA cbData = { mData.mCbChannel, (UINT)mData.mCbCrStride, 0 };
  hr = device->CreateTexture2D(&newDesc, &cbData, getter_AddRefs(textureCb));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  RefPtr<ID3D11Texture2D> textureCr;
  D3D11_SUBRESOURCE_DATA crData = { mData.mCrChannel, (UINT)mData.mCbCrStride, 0 };
  hr = device->CreateTexture2D(&newDesc, &crData, getter_AddRefs(textureCr));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  // Even though the textures we created are meant to be protected by a keyed mutex,
  // it appears that D3D doesn't include the initial memory upload within this
  // synchronization. Add an empty lock/unlock pair since that appears to
  // be sufficient to make sure we synchronize.
  {
    AutoLockTexture lockCr(textureCr);
  }

  mTextureClient = TextureClient::CreateWithData(
    DXGIYCbCrTextureData::Create(aClient->GetForwarder(),
                                 TextureFlags::DEFAULT,
                                 textureY, textureCb, textureCr,
                                 GetSize(), mData.mYSize, mData.mCbCrSize),
    TextureFlags::DEFAULT,
    aClient->GetForwarder()
  );

  return mTextureClient;
}

} // namespace layers
} // namespace mozilla

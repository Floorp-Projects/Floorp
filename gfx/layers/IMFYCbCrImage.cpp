/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IMFYCbCrImage.h"
#include "mozilla/layers/TextureD3D11.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/TextureClient.h"
#include "d3d9.h"

namespace mozilla {
namespace layers {

IMFYCbCrImage::IMFYCbCrImage(IMFMediaBuffer* aBuffer, IMF2DBuffer* a2DBuffer)
  : PlanarYCbCrImage(nullptr)
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
    aTexture->QueryInterface((IDXGIKeyedMutex**)byRef(mMutex));
    HRESULT hr = mMutex->AcquireSync(0, 10000);
    if (hr == WAIT_TIMEOUT) {
      MOZ_CRASH();
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
                                    byRef(result), &aHandle))) {
    return nullptr;
  }
  if (!result) {
    return nullptr;
  }

  RefPtr<IDirect3DTexture9> tmpTexture;
  if (FAILED(aDevice->CreateTexture(aSize.width, aSize.height,
                                    1, 0, aFormat, D3DPOOL_SYSTEMMEM,
                                    byRef(tmpTexture), nullptr))) {
    return nullptr;
  }
  if (!tmpTexture) {
    return nullptr;
  }

  tmpTexture->GetSurfaceLevel(0, byRef(aSurface));
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

  nsRefPtr<IDirect3DSurface9> dstSurface;
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
  HRESULT hr = device->CreateQuery(D3DQUERYTYPE_EVENT, byRef(query));
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

  mTextureClient = DXGIYCbCrTextureClient::Create(aClient->GetForwarder(),
                                                  TextureFlags::DEFAULT,
                                                  textureY,
                                                  textureCb,
                                                  textureCr,
                                                  shareHandleY,
                                                  shareHandleCb,
                                                  shareHandleCr,
                                                  GetSize(),
                                                  mData.mYSize,
                                                  mData.mCbCrSize);

  return mTextureClient;
}

TextureClient*
IMFYCbCrImage::GetTextureClient(CompositableClient* aClient)
{
  LayersBackend backend = aClient->GetForwarder()->GetCompositorBackendType();
  ID3D11Device* device = gfxWindowsPlatform::GetPlatform()->GetD3D11ImageBridgeDevice();

  if (!device || backend != LayersBackend::LAYERS_D3D11) {
    if (backend == LayersBackend::LAYERS_D3D9 ||
        backend == LayersBackend::LAYERS_D3D11) {
      return GetD3D9TextureClient(aClient);
    }
    return nullptr;
  }

  if (mTextureClient) {
    return mTextureClient;
  }

  RefPtr<ID3D11DeviceContext> ctx;
  device->GetImmediateContext(byRef(ctx));

  CD3D11_TEXTURE2D_DESC newDesc(DXGI_FORMAT_R8_UNORM,
                                mData.mYSize.width, mData.mYSize.height, 1, 1);

  newDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  RefPtr<ID3D11Texture2D> textureY;
  HRESULT hr = device->CreateTexture2D(&newDesc, nullptr, byRef(textureY));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  newDesc.Width = mData.mCbCrSize.width;
  newDesc.Height = mData.mCbCrSize.height;

  RefPtr<ID3D11Texture2D> textureCb;
  hr = device->CreateTexture2D(&newDesc, nullptr, byRef(textureCb));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  RefPtr<ID3D11Texture2D> textureCr;
  hr = device->CreateTexture2D(&newDesc, nullptr, byRef(textureCr));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  {
    AutoLockTexture lockY(textureY);
    AutoLockTexture lockCb(textureCb);
    AutoLockTexture lockCr(textureCr);

    ctx->UpdateSubresource(textureY, 0, nullptr, mData.mYChannel,
                           mData.mYStride, mData.mYStride * mData.mYSize.height);
    ctx->UpdateSubresource(textureCb, 0, nullptr, mData.mCbChannel,
                           mData.mCbCrStride, mData.mCbCrStride * mData.mCbCrSize.height);
    ctx->UpdateSubresource(textureCr, 0, nullptr, mData.mCrChannel,
                           mData.mCbCrStride, mData.mCbCrStride * mData.mCbCrSize.height);
  }

  mTextureClient = DXGIYCbCrTextureClient::Create(aClient->GetForwarder(),
                                                  TextureFlags::DEFAULT,
                                                  textureY,
                                                  textureCb,
                                                  textureCr,
                                                  GetSize(),
                                                  mData.mYSize,
                                                  mData.mCbCrSize);

  return mTextureClient;
}

} // namespace layers
} // namespace mozilla

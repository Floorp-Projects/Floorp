/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IMFYCbCrImage.h"
#include "mozilla/layers/TextureD3D11.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/gfx/DeviceManagerDx.h"
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

DXGIYCbCrTextureData*
IMFYCbCrImage::GetD3D11TextureData(Data aData, gfx::IntSize aSize)
{
  HRESULT hr;
  RefPtr<ID3D10Multithread> mt;

  RefPtr<ID3D11Device> device = gfx::DeviceManagerDx::Get()->GetContentDevice();

  if (!device) {
    device = gfx::DeviceManagerDx::Get()->GetCompositorDevice();
  }

  hr = device->QueryInterface((ID3D10Multithread**)getter_AddRefs(mt));

  if (FAILED(hr)) {
    return nullptr;
  }

  if (!mt->GetMultithreadProtected()) {
    return nullptr;
  }

  if (!gfx::DeviceManagerDx::Get()->CanInitializeKeyedMutexTextures()) {
    return nullptr;
  }

  if (aData.mYStride < 0 || aData.mCbCrStride < 0) {
    // D3D11 only supports unsigned stride values.
    return nullptr;
  }

  CD3D11_TEXTURE2D_DESC newDesc(DXGI_FORMAT_R8_UNORM,
                                aData.mYSize.width, aData.mYSize.height, 1, 1);

  if (device == gfx::DeviceManagerDx::Get()->GetCompositorDevice()) {
    newDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
  } else {
    newDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
  }

  RefPtr<ID3D11Texture2D> textureY;
  hr = device->CreateTexture2D(&newDesc, nullptr, getter_AddRefs(textureY));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  newDesc.Width = aData.mCbCrSize.width;
  newDesc.Height = aData.mCbCrSize.height;

  RefPtr<ID3D11Texture2D> textureCb;
  hr = device->CreateTexture2D(&newDesc, nullptr, getter_AddRefs(textureCb));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);


  RefPtr<ID3D11Texture2D> textureCr;
  hr = device->CreateTexture2D(&newDesc, nullptr, getter_AddRefs(textureCr));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  // The documentation here seems to suggest using the immediate mode context
  // on more than one thread is not allowed:
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ff476891(v=vs.85).aspx
  // The Debug Layer seems to imply it is though. When the ID3D10Multithread
  // layer is on. The Enter/Leave of the critical section shouldn't even be
  // required but were added for extra security.

  {
    AutoLockD3D11Texture lockY(textureY);
    AutoLockD3D11Texture lockCr(textureCr);
    AutoLockD3D11Texture lockCb(textureCb);
    D3D11MTAutoEnter mtAutoEnter(mt.forget());

    RefPtr<ID3D11DeviceContext> ctx;
    device->GetImmediateContext((ID3D11DeviceContext**)getter_AddRefs(ctx));

    D3D11_BOX box;
    box.front = box.top = box.left = 0;
    box.back = 1;
    box.right = aData.mYSize.width;
    box.bottom = aData.mYSize.height;
    ctx->UpdateSubresource(textureY, 0, &box, aData.mYChannel, aData.mYStride, 0);

    box.right = aData.mCbCrSize.width;
    box.bottom = aData.mCbCrSize.height;
    ctx->UpdateSubresource(textureCb, 0, &box, aData.mCbChannel, aData.mCbCrStride, 0);
    ctx->UpdateSubresource(textureCr, 0, &box, aData.mCrChannel, aData.mCbCrStride, 0);
  }

  return DXGIYCbCrTextureData::Create(textureY, textureCb, textureCr,
                                      aSize, aData.mYSize, aData.mCbCrSize);
}

TextureClient*
IMFYCbCrImage::GetD3D11TextureClient(KnowsCompositor* aForwarder)
{
  DXGIYCbCrTextureData* textureData = GetD3D11TextureData(mData, GetSize());

  if (textureData == nullptr) {
    return nullptr;
  }

  mTextureClient = TextureClient::CreateWithData(
    textureData, TextureFlags::DEFAULT,
    aForwarder->GetTextureForwarder()
  );

  return mTextureClient;
}

TextureClient*
IMFYCbCrImage::GetTextureClient(KnowsCompositor* aForwarder)
{
  if (mTextureClient) {
    return mTextureClient;
  }

  RefPtr<ID3D11Device> device =
    gfx::DeviceManagerDx::Get()->GetContentDevice();
  if (!device) {
    device =
      gfx::DeviceManagerDx::Get()->GetCompositorDevice();
  }

  LayersBackend backend = aForwarder->GetCompositorBackendType();
  if (!device || backend != LayersBackend::LAYERS_D3D11) {
    return nullptr;
  }
  return GetD3D11TextureClient(aForwarder);
}

} // namespace layers
} // namespace mozilla

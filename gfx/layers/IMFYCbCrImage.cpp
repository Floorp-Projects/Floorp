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

TextureClient*
IMFYCbCrImage::GetTextureClient(CompositableClient* aClient)
{
  ID3D11Device* device = gfxWindowsPlatform::GetPlatform()->GetD3D11MediaDevice();
  if (!device ||
    aClient->GetForwarder()->GetCompositorBackendType() != LayersBackend::LAYERS_D3D11) {
    return nullptr;
  }

  if (mTextureClient) {
    return mTextureClient;
  }

  RefPtr<ID3D11DeviceContext> ctx;
  device->GetImmediateContext(byRef(ctx));

  CD3D11_TEXTURE2D_DESC newDesc(DXGI_FORMAT_A8_UNORM,
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

  RefPtr<DXGIYCbCrTextureClientD3D11> texClient =
    new DXGIYCbCrTextureClientD3D11(aClient->GetForwarder(), TextureFlags::DEFAULT);
  texClient->InitWith(textureY, textureCb, textureCr, GetSize());
  mTextureClient = texClient;

  return mTextureClient;
}


} /* layers */
} /* mozilla */

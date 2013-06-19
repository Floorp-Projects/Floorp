/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceSurfaceD2DTarget.h"
#include "Logging.h"
#include "DrawTargetD2D.h"
#include "Tools.h"

#include <algorithm>

namespace mozilla {
namespace gfx {

SourceSurfaceD2DTarget::SourceSurfaceD2DTarget(DrawTargetD2D* aDrawTarget,
                                               ID3D10Texture2D* aTexture,
                                               SurfaceFormat aFormat)
  : mDrawTarget(aDrawTarget)
  , mTexture(aTexture)
  , mFormat(aFormat)
  , mOwnsCopy(false)
{
}

SourceSurfaceD2DTarget::~SourceSurfaceD2DTarget()
{
  // We don't need to do anything special here to notify our mDrawTarget. It must
  // already have cleared its mSnapshot field, otherwise this object would
  // be kept alive.
  if (mOwnsCopy) {
    IntSize size = GetSize();

    DrawTargetD2D::mVRAMUsageSS -= size.width * size.height * BytesPerPixel(mFormat);
  }
}

IntSize
SourceSurfaceD2DTarget::GetSize() const
{
  D3D10_TEXTURE2D_DESC desc;
  mTexture->GetDesc(&desc);

  return IntSize(desc.Width, desc.Height);
}

SurfaceFormat
SourceSurfaceD2DTarget::GetFormat() const
{
  return mFormat;
}

TemporaryRef<DataSourceSurface>
SourceSurfaceD2DTarget::GetDataSurface()
{
  RefPtr<DataSourceSurfaceD2DTarget> dataSurf =
    new DataSourceSurfaceD2DTarget();

  D3D10_TEXTURE2D_DESC desc;
  mTexture->GetDesc(&desc);

  desc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
  desc.Usage = D3D10_USAGE_STAGING;
  desc.BindFlags = 0;
  desc.MiscFlags = 0;

  HRESULT hr = Factory::GetDirect3D10Device()->CreateTexture2D(&desc, nullptr, byRef(dataSurf->mTexture));

  if (FAILED(hr)) {
    gfxDebug() << "Failed to create staging texture for SourceSurface. Code: " << hr;
    return nullptr;
  }
  Factory::GetDirect3D10Device()->CopyResource(dataSurf->mTexture, mTexture);

  return dataSurf;
}

ID3D10ShaderResourceView*
SourceSurfaceD2DTarget::GetSRView()
{
  if (mSRView) {
    return mSRView;
  }

  HRESULT hr = Factory::GetDirect3D10Device()->CreateShaderResourceView(mTexture, nullptr, byRef(mSRView));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create ShaderResourceView. Code: " << hr;
  }

  return mSRView;
}

void
SourceSurfaceD2DTarget::DrawTargetWillChange()
{
  RefPtr<ID3D10Texture2D> oldTexture = mTexture;

  D3D10_TEXTURE2D_DESC desc;
  mTexture->GetDesc(&desc);

  // Our original texture might implement the keyed mutex flag. We shouldn't
  // need that here. We actually specifically don't want it since we don't lock
  // our texture for usage!
  desc.MiscFlags = 0;

  // Get a copy of the surface data so the content at snapshot time was saved.
  Factory::GetDirect3D10Device()->CreateTexture2D(&desc, nullptr, byRef(mTexture));
  Factory::GetDirect3D10Device()->CopyResource(mTexture, oldTexture);

  mBitmap = nullptr;

  DrawTargetD2D::mVRAMUsageSS += desc.Width * desc.Height * BytesPerPixel(mFormat);
  mOwnsCopy = true;

  // We now no longer depend on the source surface content remaining the same.
  MarkIndependent();
}

ID2D1Bitmap*
SourceSurfaceD2DTarget::GetBitmap(ID2D1RenderTarget *aRT)
{
  if (mBitmap) {
    return mBitmap;
  }

  HRESULT hr;
  D3D10_TEXTURE2D_DESC desc;
  mTexture->GetDesc(&desc);

  IntSize size(desc.Width, desc.Height);
  
  RefPtr<IDXGISurface> surf;
  hr = mTexture->QueryInterface((IDXGISurface**)byRef(surf));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to query interface texture to DXGISurface. Code: " << hr;
    return nullptr;
  }

  D2D1_BITMAP_PROPERTIES props =
    D2D1::BitmapProperties(D2D1::PixelFormat(DXGIFormat(mFormat), AlphaMode(mFormat)));
  hr = aRT->CreateSharedBitmap(IID_IDXGISurface, surf, &props, byRef(mBitmap));

  if (FAILED(hr)) {
    // This seems to happen for FORMAT_A8 sometimes...
    aRT->CreateBitmap(D2D1::SizeU(desc.Width, desc.Height),
                      D2D1::BitmapProperties(D2D1::PixelFormat(DXGIFormat(mFormat),
                                             AlphaMode(mFormat))),
                      byRef(mBitmap));

    RefPtr<ID2D1RenderTarget> rt;

    if (mDrawTarget) {
      rt = mDrawTarget->mRT;
    }

    if (!rt) {
      // Okay, we already separated from our drawtarget. And we're an A8
      // surface the only way we can get to a bitmap is by creating a
      // a rendertarget and from there copying to a bitmap! Terrible!
      RefPtr<IDXGISurface> surface;

      hr = mTexture->QueryInterface((IDXGISurface**)byRef(surface));

      if (FAILED(hr)) {
        gfxWarning() << "Failed to QI texture to surface.";
        return nullptr;
      }

      D2D1_RENDER_TARGET_PROPERTIES props =
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGIFormat(mFormat), AlphaMode(mFormat)));
      hr = DrawTargetD2D::factory()->CreateDxgiSurfaceRenderTarget(surface, props, byRef(rt));

      if (FAILED(hr)) {
        gfxWarning() << "Failed to create D2D render target for texture.";
        return nullptr;
      }
    }

    mBitmap->CopyFromRenderTarget(nullptr, rt, nullptr);
    return mBitmap;
  }

  return mBitmap;
}

void
SourceSurfaceD2DTarget::MarkIndependent()
{
  if (mDrawTarget) {
    MOZ_ASSERT(mDrawTarget->mSnapshot == this);
    mDrawTarget->mSnapshot = nullptr;
    mDrawTarget = nullptr;
  }
}

DataSourceSurfaceD2DTarget::DataSourceSurfaceD2DTarget()
  : mFormat(FORMAT_B8G8R8A8)
  , mMapped(false)
{
}

DataSourceSurfaceD2DTarget::~DataSourceSurfaceD2DTarget()
{
  if (mMapped) {
    mTexture->Unmap(0);
  }
}

IntSize
DataSourceSurfaceD2DTarget::GetSize() const
{
  D3D10_TEXTURE2D_DESC desc;
  mTexture->GetDesc(&desc);

  return IntSize(desc.Width, desc.Height);
}

SurfaceFormat
DataSourceSurfaceD2DTarget::GetFormat() const
{
  return mFormat;
}

uint8_t*
DataSourceSurfaceD2DTarget::GetData()
{
  EnsureMapped();

  return (unsigned char*)mMap.pData;
}

int32_t
DataSourceSurfaceD2DTarget::Stride()
{
  EnsureMapped();
  return mMap.RowPitch;
}

void
DataSourceSurfaceD2DTarget::EnsureMapped()
{
  if (!mMapped) {
    HRESULT hr = mTexture->Map(0, D3D10_MAP_READ, 0, &mMap);
    if (FAILED(hr)) {
      gfxWarning() << "Failed to map texture to memory. Code: " << hr;
      return;
    }
    mMapped = true;
  }
}

}
}

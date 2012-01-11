/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "SourceSurfaceD2DTarget.h"
#include "Logging.h"
#include "DrawTargetD2D.h"

#include <algorithm>

namespace mozilla {
namespace gfx {

SourceSurfaceD2DTarget::SourceSurfaceD2DTarget(DrawTargetD2D* aDrawTarget,
                                               ID3D10Texture2D* aTexture,
                                               SurfaceFormat aFormat)
  : mDrawTarget(aDrawTarget)
  , mTexture(aTexture)
  , mFormat(aFormat)
{
}

SourceSurfaceD2DTarget::~SourceSurfaceD2DTarget()
{
  // We don't need to do anything special here to notify our mDrawTarget. It must
  // already have cleared its mSnapshot field, otherwise this object would
  // be kept alive.
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

  HRESULT hr = Factory::GetDirect3D10Device()->CreateTexture2D(&desc, NULL, byRef(dataSurf->mTexture));

  if (FAILED(hr)) {
    gfxDebug() << "Failed to create staging texture for SourceSurface. Code: " << hr;
    return NULL;
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

  HRESULT hr = Factory::GetDirect3D10Device()->CreateShaderResourceView(mTexture, NULL, byRef(mSRView));

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

  // Get a copy of the surface data so the content at snapshot time was saved.
  Factory::GetDirect3D10Device()->CreateTexture2D(&desc, NULL, byRef(mTexture));
  Factory::GetDirect3D10Device()->CopyResource(mTexture, oldTexture);

  mBitmap = NULL;

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
    return NULL;
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

    if (mDrawTarget) {
      mBitmap->CopyFromRenderTarget(NULL, mDrawTarget->mRT, NULL);
      return mBitmap;
    }
    gfxWarning() << "Failed to create shared bitmap for DrawTarget snapshot. Code: " << hr;
    return NULL;
  }

  return mBitmap;
}

void
SourceSurfaceD2DTarget::MarkIndependent()
{
  if (mDrawTarget) {
    MOZ_ASSERT(mDrawTarget->mSnapshot == this);
    mDrawTarget->mSnapshot = NULL;
    mDrawTarget = NULL;
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

unsigned char*
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

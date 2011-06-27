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

#include "SourceSurfaceD2D.h"
#include "Logging.h"

namespace mozilla {
namespace gfx {

SourceSurfaceD2D::SourceSurfaceD2D()
{
}

SourceSurfaceD2D::~SourceSurfaceD2D()
{
}

IntSize
SourceSurfaceD2D::GetSize() const
{
  return mSize;
}

SurfaceFormat
SourceSurfaceD2D::GetFormat() const
{
  return mFormat;
}

TemporaryRef<DataSourceSurface>
SourceSurfaceD2D::GetDataSurface()
{
  return NULL;
}

bool
SourceSurfaceD2D::InitFromData(unsigned char *aData,
                               const IntSize &aSize,
                               int32_t aStride,
                               SurfaceFormat aFormat,
                               ID2D1RenderTarget *aRT)
{
  HRESULT hr;

  mFormat = aFormat;
  mSize = aSize;

  if ((uint32_t)aSize.width > aRT->GetMaximumBitmapSize() ||
      (uint32_t)aSize.height > aRT->GetMaximumBitmapSize()) {
    int newStride = BytesPerPixel(aFormat) * aSize.width;
    mRawData.resize(aSize.height * newStride);
    for (int y = 0; y < aSize.height; y++) {
      memcpy(&mRawData.front() + y * newStride, aData + y * aStride, newStride);
    }
    gfxDebug() << "Bitmap does not fit in texture, saving raw data.";
    return true;
  }

  D2D1_BITMAP_PROPERTIES props =
    D2D1::BitmapProperties(D2D1::PixelFormat(DXGIFormat(aFormat), AlphaMode(aFormat)));
  hr = aRT->CreateBitmap(D2DIntSize(aSize), aData, aStride, props, byRef(mBitmap));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create D2D Bitmap for data. Code: " << hr;
    return false;
  }

  return true;
}

bool
SourceSurfaceD2D::InitFromTexture(ID3D10Texture2D *aTexture,
                                  SurfaceFormat aFormat,
                                  ID2D1RenderTarget *aRT)
{
  HRESULT hr;

  RefPtr<IDXGISurface> surf;

  hr = aTexture->QueryInterface((IDXGISurface**)&surf);

  if (FAILED(hr)) {
    gfxWarning() << "Failed to QI texture to surface. Code: " << hr;
    return false;
  }

  D3D10_TEXTURE2D_DESC desc;
  aTexture->GetDesc(&desc);

  mSize = IntSize(desc.Width, desc.Height);
  mFormat = aFormat;

  D2D1_BITMAP_PROPERTIES props =
    D2D1::BitmapProperties(D2D1::PixelFormat(DXGIFormat(aFormat), AlphaMode(aFormat)));
  hr = aRT->CreateSharedBitmap(IID_IDXGISurface, surf, &props, byRef(mBitmap));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create SharedBitmap. Code: " << hr;
    return false;
  }

  return true;
}

}
}

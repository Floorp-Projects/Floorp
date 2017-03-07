/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderTextureHost.h"

#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/ImageDataSerializer.h"

namespace mozilla {

using namespace gfx;
using namespace layers;

namespace wr {

RenderTextureHost::RenderTextureHost(uint8_t* aBuffer, const BufferDescriptor& aDescriptor)
  : mBuffer(aBuffer)
  , mDescriptor(aDescriptor)
  , mLocked(false)
{
  MOZ_COUNT_CTOR(RenderTextureHost);

  switch (mDescriptor.type()) {
    case BufferDescriptor::TYCbCrDescriptor: {
      const YCbCrDescriptor& ycbcr = mDescriptor.get_YCbCrDescriptor();
      mSize = ycbcr.ySize();
      mFormat = gfx::SurfaceFormat::YUV;
      break;
    }
    case BufferDescriptor::TRGBDescriptor: {
      const RGBDescriptor& rgb = mDescriptor.get_RGBDescriptor();
      mSize = rgb.size();
      mFormat = rgb.format();
      break;
    }
    default:
      gfxCriticalError() << "Bad buffer host descriptor " << (int)mDescriptor.type();
      MOZ_CRASH("GFX: Bad descriptor");
  }
}

RenderTextureHost::~RenderTextureHost()
{
  MOZ_COUNT_DTOR(RenderTextureHost);
}

already_AddRefed<gfx::DataSourceSurface>
RenderTextureHost::GetAsSurface()
{
  RefPtr<gfx::DataSourceSurface> result;
  if (mFormat == gfx::SurfaceFormat::YUV) {
    result = ImageDataSerializer::DataSourceSurfaceFromYCbCrDescriptor(
      GetBuffer(), mDescriptor.get_YCbCrDescriptor());
    if (NS_WARN_IF(!result)) {
      return nullptr;
    }
  } else {
    result =
      gfx::Factory::CreateWrappingDataSourceSurface(GetBuffer(),
        ImageDataSerializer::GetRGBStride(mDescriptor.get_RGBDescriptor()),
        mSize, mFormat);
  }
  return result.forget();
}

bool
RenderTextureHost::Lock()
{
  MOZ_ASSERT(!mLocked);

  // XXX temporal workaround for YUV handling
  if (!mSurface) {
    mSurface = GetAsSurface();
    if (!mSurface) {
      return false;
    }
  }

  if (NS_WARN_IF(!mSurface->Map(DataSourceSurface::MapType::READ_WRITE, &mMap))) {
    mSurface = nullptr;
    return false;
  }

  mLocked = true;
  return true;
}

void
RenderTextureHost::Unlock()
{
  MOZ_ASSERT(mLocked);
  mLocked = false;
  if (mSurface) {
    mSurface->Unmap();
  }
  mSurface = nullptr;
}

uint8_t*
RenderTextureHost::GetDataForRender() const
{
  MOZ_ASSERT(mLocked);
  MOZ_ASSERT(mSurface);
  return mMap.mData;
}

size_t
RenderTextureHost::GetBufferSizeForRender() const
{
  MOZ_ASSERT(mLocked);
  MOZ_ASSERT(mSurface);
  return mMap.mStride * mSurface->GetSize().height;
}

} // namespace wr
} // namespace mozilla

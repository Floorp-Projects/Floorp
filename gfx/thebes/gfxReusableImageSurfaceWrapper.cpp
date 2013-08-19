/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxReusableImageSurfaceWrapper.h"
#include "gfxImageSurface.h"

gfxReusableImageSurfaceWrapper::gfxReusableImageSurfaceWrapper(gfxImageSurface* aSurface)
  : mSurface(aSurface)
{
  MOZ_COUNT_CTOR(gfxReusableImageSurfaceWrapper);
}

gfxReusableImageSurfaceWrapper::~gfxReusableImageSurfaceWrapper()
{
  MOZ_COUNT_DTOR(gfxReusableImageSurfaceWrapper);
}

void
gfxReusableImageSurfaceWrapper::ReadLock()
{
  NS_CheckThreadSafe(_mOwningThread.GetThread(), "Only the owner thread can call ReadLock");
  AddRef();
}

void
gfxReusableImageSurfaceWrapper::ReadUnlock()
{
  Release();
}

gfxReusableSurfaceWrapper*
gfxReusableImageSurfaceWrapper::GetWritable(gfxImageSurface** aSurface)
{
  NS_CheckThreadSafe(_mOwningThread.GetThread(), "Only the owner thread can call GetWritable");

  if (mRefCnt == 1) {
    *aSurface = mSurface;
    return this;
  }

  // Something else is reading the surface, copy it
  gfxImageSurface* copySurface = new gfxImageSurface(mSurface->GetSize(), mSurface->Format(), false);
  copySurface->CopyFrom(mSurface);
  *aSurface = copySurface;

  return new gfxReusableImageSurfaceWrapper(copySurface);
}

const unsigned char*
gfxReusableImageSurfaceWrapper::GetReadOnlyData() const
{
  return mSurface->Data();
}

gfxASurface::gfxImageFormat
gfxReusableImageSurfaceWrapper::Format()
{
  return mSurface->Format();
}


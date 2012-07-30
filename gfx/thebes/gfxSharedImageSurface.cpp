// vim:set ts=4 sts=4 sw=4 et cin:
/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "gfxSharedImageSurface.h"
#include "cairo.h"

#define MOZ_ALIGN_WORD(x) (((x) + 3) & ~3)

using namespace mozilla::ipc;

static const cairo_user_data_key_t SHM_KEY = {0};

struct SharedImageInfo {
    PRInt32 width;
    PRInt32 height;
    PRInt32 format;
};

static SharedImageInfo*
GetShmInfoPtr(const Shmem& aShmem)
{
    return reinterpret_cast<SharedImageInfo*>
        (aShmem.get<char>() + aShmem.Size<char>() - sizeof(SharedImageInfo));
}

gfxSharedImageSurface::~gfxSharedImageSurface()
{
    MOZ_COUNT_DTOR(gfxSharedImageSurface);
}

/*static*/ bool
gfxSharedImageSurface::IsSharedImage(gfxASurface* aSurface)
{
    return (aSurface
            && aSurface->GetType() == gfxASurface::SurfaceTypeImage
            && aSurface->GetData(&SHM_KEY));
}

gfxSharedImageSurface::gfxSharedImageSurface(const gfxIntSize& aSize,
                                             gfxImageFormat aFormat,
                                             const Shmem& aShmem)
{
    MOZ_COUNT_CTOR(gfxSharedImageSurface);

    mSize = aSize;
    mFormat = aFormat;
    mStride = ComputeStride(aSize, aFormat);
    mShmem = aShmem;
    mData = aShmem.get<unsigned char>();
    cairo_surface_t *surface =
        cairo_image_surface_create_for_data(mData,
                                            (cairo_format_t)mFormat,
                                            mSize.width,
                                            mSize.height,
                                            mStride);
    if (surface) {
        cairo_surface_set_user_data(surface, &SHM_KEY, this, NULL);
    }
    Init(surface);
}

void
gfxSharedImageSurface::WriteShmemInfo()
{
    SharedImageInfo* shmInfo = GetShmInfoPtr(mShmem);
    shmInfo->width = mSize.width;
    shmInfo->height = mSize.height;
    shmInfo->format = mFormat;
}

/*static*/ size_t
gfxSharedImageSurface::GetAlignedSize(const gfxIntSize& aSize, long aStride)
{
   return MOZ_ALIGN_WORD(sizeof(SharedImageInfo) + aSize.height * aStride);
}

/*static*/ already_AddRefed<gfxSharedImageSurface>
gfxSharedImageSurface::Open(const Shmem& aShmem)
{
    SharedImageInfo* shmInfo = GetShmInfoPtr(aShmem);
    gfxIntSize size(shmInfo->width, shmInfo->height);
    if (!CheckSurfaceSize(size))
        return nullptr;

    nsRefPtr<gfxSharedImageSurface> s =
        new gfxSharedImageSurface(size,
                                  (gfxImageFormat)shmInfo->format,
                                  aShmem);
    // We didn't create this Shmem and so don't free it on errors
    return (s->CairoStatus() != 0) ? nullptr : s.forget();
}

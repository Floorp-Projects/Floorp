// vim:set ts=4 sts=4 sw=4 et cin:
/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_SHARED_IMAGESURFACE_H
#define GFX_SHARED_IMAGESURFACE_H

#include "mozilla/ipc/Shmem.h"
#include "mozilla/ipc/SharedMemory.h"

#include "gfxASurface.h"
#include "gfxImageSurface.h"

class THEBES_API gfxSharedImageSurface : public gfxImageSurface {
    typedef mozilla::ipc::SharedMemory SharedMemory;
    typedef mozilla::ipc::Shmem Shmem;

public:
    virtual ~gfxSharedImageSurface();

    /**
     * Return a new gfxSharedImageSurface around a shmem segment newly
     * allocated by this function.  |aAllocator| is the object used to
     * allocate the new shmem segment.  Null is returned if creating
     * the surface failed.
     *
     * NB: the *caller* is responsible for freeing the Shmem allocated
     * by this function.
     */
    template<class ShmemAllocator>
    static already_AddRefed<gfxSharedImageSurface>
    Create(ShmemAllocator* aAllocator,
           const gfxIntSize& aSize,
           gfxImageFormat aFormat,
           SharedMemory::SharedMemoryType aShmType = SharedMemory::TYPE_BASIC)
    {
        return Create<ShmemAllocator, false>(aAllocator, aSize, aFormat, aShmType);
    }

    /**
     * Return a new gfxSharedImageSurface that wraps a shmem segment
     * already created by the Create() above.  Bad things will happen
     * if an attempt is made to wrap any other shmem segment.  Null is
     * returned if creating the surface failed.
     */
    static already_AddRefed<gfxSharedImageSurface>
    Open(const Shmem& aShmem);

    template<class ShmemAllocator>
    static already_AddRefed<gfxSharedImageSurface>
    CreateUnsafe(ShmemAllocator* aAllocator,
                 const gfxIntSize& aSize,
                 gfxImageFormat aFormat,
                 SharedMemory::SharedMemoryType aShmType = SharedMemory::TYPE_BASIC)
    {
        return Create<ShmemAllocator, true>(aAllocator, aSize, aFormat, aShmType);
    }

    Shmem& GetShmem() { return mShmem; }

    static bool IsSharedImage(gfxASurface *aSurface);

private:
    gfxSharedImageSurface(const gfxIntSize&, gfxImageFormat, const Shmem&);

    void WriteShmemInfo();

    static size_t GetAlignedSize(const gfxIntSize&, long aStride);

    template<class ShmemAllocator, bool Unsafe>
    static already_AddRefed<gfxSharedImageSurface>
    Create(ShmemAllocator* aAllocator,
           const gfxIntSize& aSize,
           gfxImageFormat aFormat,
           SharedMemory::SharedMemoryType aShmType)
    {
        if (!CheckSurfaceSize(aSize))
            return nsnull;

        Shmem shmem;
        long stride = ComputeStride(aSize, aFormat);
        size_t size = GetAlignedSize(aSize, stride);
        if (!Unsafe) {
            if (!aAllocator->AllocShmem(size, aShmType, &shmem))
                return nsnull;
        } else {
            if (!aAllocator->AllocUnsafeShmem(size, aShmType, &shmem))
                return nsnull;
        }

        nsRefPtr<gfxSharedImageSurface> s =
            new gfxSharedImageSurface(aSize, aFormat, shmem);
        if (s->CairoStatus() != 0) {
            aAllocator->DeallocShmem(shmem);
            return nsnull;
        }
        s->WriteShmemInfo();
        return s.forget();
    }

    Shmem mShmem;

    // Calling these is very bad, disallow it
    gfxSharedImageSurface(const gfxSharedImageSurface&);
    gfxSharedImageSurface& operator=(const gfxSharedImageSurface&);
};

#endif /* GFX_SHARED_IMAGESURFACE_H */

// vim:set ts=4 sts=4 sw=4 et cin:
/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_SHARED_MEMORYSURFACE_H
#define GFX_SHARED_MEMORYSURFACE_H

#include "mozilla/ipc/Shmem.h"
#include "mozilla/ipc/SharedMemory.h"

#include "gfxASurface.h"
#include "gfxImageSurface.h"
#include "cairo.h"
#include "pratom.h"

struct SharedImageInfo {
    int32_t width;
    int32_t height;
    int32_t format;
    int32_t readCount;
};

inline SharedImageInfo*
GetShmInfoPtr(const mozilla::ipc::Shmem& aShmem)
{
    return reinterpret_cast<SharedImageInfo*>
        (aShmem.get<char>() + aShmem.Size<char>() - sizeof(SharedImageInfo));
}

extern const cairo_user_data_key_t SHM_KEY;

template <typename Base, typename Sub>
class gfxBaseSharedMemorySurface : public Base {
    typedef mozilla::ipc::SharedMemory SharedMemory;
    typedef mozilla::ipc::Shmem Shmem;
    friend class gfxReusableSurfaceWrapper;

public:
    virtual ~gfxBaseSharedMemorySurface()
    {
        MOZ_COUNT_DTOR(gfxBaseSharedMemorySurface);
    }

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
    static already_AddRefed<Sub>
    Create(ShmemAllocator* aAllocator,
           const gfxIntSize& aSize,
           gfxASurface::gfxImageFormat aFormat,
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
    static already_AddRefed<Sub>
    Open(const Shmem& aShmem)
    {
        SharedImageInfo* shmInfo = GetShmInfoPtr(aShmem);
        gfxIntSize size(shmInfo->width, shmInfo->height);
        if (!gfxASurface::CheckSurfaceSize(size))
            return nullptr;
       
        gfxASurface::gfxImageFormat format = (gfxASurface::gfxImageFormat)shmInfo->format;
        long stride = gfxImageSurface::ComputeStride(size, format);

        nsRefPtr<Sub> s =
            new Sub(size,
                    stride,
                    format,
                    aShmem);
        // We didn't create this Shmem and so don't free it on errors
        return (s->CairoStatus() != 0) ? nullptr : s.forget();
    }

    template<class ShmemAllocator>
    static already_AddRefed<Sub>
    CreateUnsafe(ShmemAllocator* aAllocator,
                 const gfxIntSize& aSize,
                 gfxASurface::gfxImageFormat aFormat,
                 SharedMemory::SharedMemoryType aShmType = SharedMemory::TYPE_BASIC)
    {
        return Create<ShmemAllocator, true>(aAllocator, aSize, aFormat, aShmType);
    }

    Shmem& GetShmem() { return mShmem; }

    static bool IsSharedImage(gfxASurface *aSurface)
    {
        return (aSurface
                && aSurface->GetType() == gfxASurface::SurfaceTypeImage
                && aSurface->GetData(&SHM_KEY));
    }

protected:
    gfxBaseSharedMemorySurface(const gfxIntSize& aSize, long aStride, 
                               gfxASurface::gfxImageFormat aFormat, 
                               const Shmem& aShmem)
      : Base(aShmem.get<unsigned char>(), aSize, aStride, aFormat)
    {
        MOZ_COUNT_CTOR(gfxBaseSharedMemorySurface);

        mShmem = aShmem;
        this->SetData(&SHM_KEY, this, nullptr);
    }

private:
    void WriteShmemInfo()
    {
        SharedImageInfo* shmInfo = GetShmInfoPtr(mShmem);
        shmInfo->width = this->mSize.width;
        shmInfo->height = this->mSize.height;
        shmInfo->format = this->mFormat;
        shmInfo->readCount = 0;
    }

    int32_t
    ReadLock()
    {
        SharedImageInfo* shmInfo = GetShmInfoPtr(mShmem);
        return PR_ATOMIC_INCREMENT(&shmInfo->readCount);
    }

    int32_t
    ReadUnlock()
    {
        SharedImageInfo* shmInfo = GetShmInfoPtr(mShmem);
        return PR_ATOMIC_DECREMENT(&shmInfo->readCount);
    }

    int32_t
    GetReadCount()
    {
        SharedImageInfo* shmInfo = GetShmInfoPtr(mShmem);
        return shmInfo->readCount;
    }

    static size_t GetAlignedSize(const gfxIntSize& aSize, long aStride)
    {
        #define MOZ_ALIGN_WORD(x) (((x) + 3) & ~3)
        return MOZ_ALIGN_WORD(sizeof(SharedImageInfo) + aSize.height * aStride);
    }

    template<class ShmemAllocator, bool Unsafe>
    static already_AddRefed<Sub>
    Create(ShmemAllocator* aAllocator,
           const gfxIntSize& aSize,
           gfxASurface::gfxImageFormat aFormat,
           SharedMemory::SharedMemoryType aShmType)
    {
        if (!gfxASurface::CheckSurfaceSize(aSize))
            return nullptr;

        Shmem shmem;
        long stride = gfxImageSurface::ComputeStride(aSize, aFormat);
        size_t size = GetAlignedSize(aSize, stride);
        if (!Unsafe) {
            if (!aAllocator->AllocShmem(size, aShmType, &shmem))
                return nullptr;
        } else {
            if (!aAllocator->AllocUnsafeShmem(size, aShmType, &shmem))
                return nullptr;
        }

        nsRefPtr<Sub> s =
            new Sub(aSize, stride, aFormat, shmem);
        if (s->CairoStatus() != 0) {
            aAllocator->DeallocShmem(shmem);
            return nullptr;
        }
        s->WriteShmemInfo();
        return s.forget();
    }

    Shmem mShmem;

    // Calling these is very bad, disallow it
    gfxBaseSharedMemorySurface(const gfxBaseSharedMemorySurface&);
    gfxBaseSharedMemorySurface& operator=(const gfxBaseSharedMemorySurface&);
};

#endif /* GFX_SHARED_MEMORYSURFACE_H */

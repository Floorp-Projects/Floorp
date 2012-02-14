// vim:set ts=4 sts=4 sw=4 et cin:
/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Nokia Corporation code.
 *
 * The Initial Developer of the Original Code is Nokia Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Oleg Romashin <romaxa@gmail.com>
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

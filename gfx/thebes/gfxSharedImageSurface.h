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
    /**
     * Init must be called after ctor
     */
    gfxSharedImageSurface();

    /**
     * Create shared image from external Shmem
     * Shmem must be initialized by this class
     */
    gfxSharedImageSurface(const Shmem &aShmem);

    ~gfxSharedImageSurface();

    /**
     * Initialize shared image surface
     * @param aAllocator The pointer to protocol class which has AllocShmem method
     * @param aSize The size of the buffer
     * @param aFormat Format of the data
     * @see gfxImageFormat
     */
    template<class ShmemAllocator>
    bool Init(ShmemAllocator *aAllocator,
              const gfxIntSize& aSize,
              gfxImageFormat aFormat,
              SharedMemory::SharedMemoryType aShmType = SharedMemory::TYPE_BASIC)
    {
        return Init<ShmemAllocator, false>(aAllocator, aSize, aFormat, aShmType);
    }

    template<class ShmemAllocator>
    bool InitUnsafe(ShmemAllocator *aAllocator,
                    const gfxIntSize& aSize,
                    gfxImageFormat aFormat,
                    SharedMemory::SharedMemoryType aShmType = SharedMemory::TYPE_BASIC)
    {
        return Init<ShmemAllocator, true>(aAllocator, aSize, aFormat, aShmType);
    }

    /* Gives Shmem data, which can be passed to IPDL interfaces */
    Shmem& GetShmem() { return mShmem; }

    // This can be used for recognizing normal gfxImageSurface as SharedImage
    static PRBool IsSharedImage(gfxASurface *aSurface);

private:
    template<class ShmemAllocator, bool Unsafe>
    bool Init(ShmemAllocator *aAllocator,
              const gfxIntSize& aSize,
              gfxImageFormat aFormat,
              SharedMemory::SharedMemoryType aShmType)
    {
        mSize = aSize;
        mFormat = aFormat;
        mStride = ComputeStride();
        if (!Unsafe) {
            if (!aAllocator->AllocShmem(GetAlignedSize(),
                                        aShmType, &mShmem))
                return false;
        } else {
            if (!aAllocator->AllocUnsafeShmem(GetAlignedSize(),
                                              aShmType, &mShmem))
                return false;
        }

        return InitSurface(PR_TRUE);
    }

    size_t GetAlignedSize();
    bool InitSurface(PRBool aUpdateShmemInfo);

    Shmem mShmem;
};

#endif /* GFX_SHARED_IMAGESURFACE_H */

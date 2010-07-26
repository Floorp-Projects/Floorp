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

#include "base/basictypes.h"
#include "gfxSharedImageSurface.h"
#include "cairo.h"

#define MOZ_ALIGN_WORD(x) (((x) + 3) & ~3)

using mozilla::ipc::SharedMemory;

static cairo_user_data_key_t SHM_KEY;

typedef struct _SharedImageInfo
{
    PRInt32 width;
    PRInt32 height;
    PRInt32 format;
} SharedImageInfo;

static SharedImageInfo*
GetShmInfoPtr(const mozilla::ipc::Shmem &aShmem)
{
    return reinterpret_cast<SharedImageInfo*>
        (aShmem.get<char>() + aShmem.Size<char>() - sizeof(SharedImageInfo));
}

size_t
gfxSharedImageSurface::GetAlignedSize()
{
   return MOZ_ALIGN_WORD(sizeof(SharedImageInfo) + mSize.height * mStride);
}

bool
gfxSharedImageSurface::InitSurface(PRBool aUpdateShmemInfo)
{
    if (!CheckSurfaceSize(mSize))
        return false;

    cairo_surface_t *surface =
        cairo_image_surface_create_for_data(mShmem.get<unsigned char>(),
                                            (cairo_format_t)mFormat,
                                            mSize.width,
                                            mSize.height,
                                            mStride);

    if (!surface)
        return false;

    cairo_surface_set_user_data(surface,
                                &SHM_KEY,
                                this, NULL);

    if (aUpdateShmemInfo) {
        SharedImageInfo *shmInfo = GetShmInfoPtr(mShmem);
        shmInfo->width = mSize.width;
        shmInfo->height = mSize.height;
        shmInfo->format = mFormat;
    }

    InitFromSurface(surface);
    return true;
}

gfxSharedImageSurface::~gfxSharedImageSurface()
{
}

gfxSharedImageSurface::gfxSharedImageSurface()
{
}

gfxSharedImageSurface::gfxSharedImageSurface(const mozilla::ipc::Shmem &aShmem)
{
    mShmem = aShmem;
    SharedImageInfo *shmInfo = GetShmInfoPtr(aShmem);
    mSize.width = shmInfo->width;
    mSize.height = shmInfo->height;
    mFormat = (gfxImageFormat)shmInfo->format;
    mStride = ComputeStride();

    if (!InitSurface(PR_FALSE))
        NS_RUNTIMEABORT("Shared memory is bad");
}

PRBool
gfxSharedImageSurface::IsSharedImage(gfxASurface *aSurface)
{
    return (aSurface
            && aSurface->GetType() == gfxASurface::SurfaceTypeImage
            && aSurface->GetData(&SHM_KEY));
}

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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
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

#ifndef GFX_IMAGESURFACE_H
#define GFX_IMAGESURFACE_H

#include "gfxASurface.h"
#include "gfxPoint.h"

// ARGB -- raw buffer.. wont be changed.. good for storing data.

/**
 * A raw image buffer. The format can be set in the constructor. Its main
 * purpose is for storing read-only images and using it as a source surface,
 * but it can also be drawn to.
 */
class THEBES_API gfxImageSurface : public gfxASurface {
public:
    /**
     * Construct an image surface around an existing buffer of image data.
     * @param aData A buffer containing the image data
     * @param aSize The size of the buffer
     * @param aStride The stride of the buffer
     * @param format Format of the data
     *
     * @see gfxImageFormat
     */
    gfxImageSurface(unsigned char *aData, const gfxIntSize& aSize,
                    long aStride, gfxImageFormat aFormat);

    /**
     * Construct an image surface.
     * @param aSize The size of the buffer
     * @param format Format of the data
     *
     * @see gfxImageFormat
     */
    gfxImageSurface(const gfxIntSize& size, gfxImageFormat format);
    gfxImageSurface(cairo_surface_t *csurf);

    virtual ~gfxImageSurface();

    // ImageSurface methods
    gfxImageFormat Format() const { return mFormat; }

    const gfxIntSize& GetSize() const { return mSize; }
    PRInt32 Width() const { return mSize.width; }
    PRInt32 Height() const { return mSize.height; }

    /**
     * Distance in bytes between the start of a line and the start of the
     * next line.
     */
    PRInt32 Stride() const { return mStride; }
    /**
     * Returns a pointer for the image data. Users of this function can
     * write to it, but must not attempt to free the buffer.
     */
    unsigned char* Data() const { return mData; } // delete this data under us and die.
    /**
     * Returns the total size of the image data.
     */
    PRInt32 GetDataSize() const { return mStride*mSize.height; }

    /* Fast copy from another image surface; returns TRUE if successful, FALSE otherwise */
    PRBool CopyFrom (gfxImageSurface *other);

private:
    long ComputeStride() const;

    gfxIntSize mSize;
    PRBool mOwnsData;
    unsigned char *mData;
    gfxImageFormat mFormat;
    long mStride;
};

#endif /* GFX_IMAGESURFACE_H */

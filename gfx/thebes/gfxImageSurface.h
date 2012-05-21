/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_IMAGESURFACE_H
#define GFX_IMAGESURFACE_H

#include "gfxASurface.h"
#include "gfxPoint.h"

// ARGB -- raw buffer.. wont be changed.. good for storing data.

class gfxSubimageSurface;

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
    gfxImageSurface(const gfxIntSize& size, gfxImageFormat format, bool aClear = true);
    gfxImageSurface(cairo_surface_t *csurf);

    virtual ~gfxImageSurface();

    // ImageSurface methods
    gfxImageFormat Format() const { return mFormat; }

    virtual const gfxIntSize GetSize() const { return mSize; }
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
    bool CopyFrom (gfxImageSurface *other);

    /* return new Subimage with pointing to original image starting from aRect.pos
     * and size of aRect.size. New subimage keeping current image reference
     */
    already_AddRefed<gfxSubimageSurface> GetSubimage(const gfxRect& aRect);

    virtual already_AddRefed<gfxImageSurface> GetAsImageSurface();

    /** See gfxASurface.h. */
    NS_OVERRIDE
    virtual void MovePixels(const nsIntRect& aSourceRect,
                            const nsIntPoint& aDestTopLeft);

protected:
    gfxImageSurface();
    void InitWithData(unsigned char *aData, const gfxIntSize& aSize,
                      long aStride, gfxImageFormat aFormat);
    void InitFromSurface(cairo_surface_t *csurf);
    long ComputeStride() const { return ComputeStride(mSize, mFormat); }

    static long ComputeStride(const gfxIntSize&, gfxImageFormat);

    void MakeInvalid();

    gfxIntSize mSize;
    bool mOwnsData;
    unsigned char *mData;
    gfxImageFormat mFormat;
    long mStride;
};

class THEBES_API gfxSubimageSurface : public gfxImageSurface {
protected:
    friend class gfxImageSurface;
    gfxSubimageSurface(gfxImageSurface* aParent,
                       unsigned char* aData,
                       const gfxIntSize& aSize);
private:
    nsRefPtr<gfxImageSurface> mParent;
};

#endif /* GFX_IMAGESURFACE_H */

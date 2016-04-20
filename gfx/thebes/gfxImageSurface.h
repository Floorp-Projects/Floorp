/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_IMAGESURFACE_H
#define GFX_IMAGESURFACE_H

#include "mozilla/MemoryReporting.h"
#include "mozilla/RefPtr.h"
#include "gfxASurface.h"
#include "nsSize.h"

// ARGB -- raw buffer.. wont be changed.. good for storing data.

class gfxSubimageSurface;

namespace mozilla {
namespace gfx {
class DataSourceSurface;
class SourceSurface;
} // namespace gfx
} // namespace mozilla

/**
 * A raw image buffer. The format can be set in the constructor. Its main
 * purpose is for storing read-only images and using it as a source surface,
 * but it can also be drawn to.
 */
class gfxImageSurface : public gfxASurface {
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
    gfxImageSurface(unsigned char *aData, const mozilla::gfx::IntSize& aSize,
                    long aStride, gfxImageFormat aFormat);

    /**
     * Construct an image surface.
     * @param aSize The size of the buffer
     * @param format Format of the data
     *
     * @see gfxImageFormat
     */
    gfxImageSurface(const mozilla::gfx::IntSize& size, gfxImageFormat format, bool aClear = true);

    /**
     * Construct an image surface, with a specified stride and allowing the
     * allocation of more memory than required for the storage of the surface
     * itself.  When aStride and aMinimalAllocation are <=0, this constructor
     * is the equivalent of the preceeding one.
     *
     * @param format Format of the data
     * @param aSize The size of the buffer
     * @param aStride The stride of the buffer - if <=0, use ComputeStride()
     * @param aMinimalAllocation Allocate at least this many bytes.  If smaller
     *        than width * stride, or width*stride <=0, this value is ignored.
     * @param aClear 
     *
     * @see gfxImageFormat
     */
    gfxImageSurface(const mozilla::gfx::IntSize& aSize, gfxImageFormat aFormat,
                    long aStride, int32_t aMinimalAllocation, bool aClear);

    explicit gfxImageSurface(cairo_surface_t *csurf);

    virtual ~gfxImageSurface();

    // ImageSurface methods
    gfxImageFormat Format() const { return mFormat; }

    virtual const mozilla::gfx::IntSize GetSize() const override { return mSize; }
    int32_t Width() const {
        if (mSize.width < 0) {
            return 0;
        }
        return mSize.width;
    }
    int32_t Height() const {
        if (mSize.height < 0) {
            return 0;
        }
        return mSize.height;
    }

    /**
     * Distance in bytes between the start of a line and the start of the
     * next line.
     */
    int32_t Stride() const { return mStride; }
    /**
     * Returns a pointer for the image data. Users of this function can
     * write to it, but must not attempt to free the buffer.
     */
    unsigned char* Data() const { return mData; } // delete this data under us and die.
    /**
     * Returns the total size of the image data.
     */
    int32_t GetDataSize() const {
        if (mStride < 0 || mSize.height < 0) {
            return 0;
        }
        return mStride*mSize.height;
    }

    /* Fast copy from another image surface; returns TRUE if successful, FALSE otherwise */
    bool CopyFrom (gfxImageSurface *other);

    /**
     * Fast copy from a source surface; returns TRUE if successful, FALSE otherwise
     * Assumes that the format of this surface is compatable with aSurface
     */
    bool CopyFrom (mozilla::gfx::SourceSurface *aSurface);

    /**
     * Fast copy to a source surface; returns TRUE if successful, FALSE otherwise
     * Assumes that the format of this surface is compatible with aSurface
     */
    bool CopyTo (mozilla::gfx::SourceSurface *aSurface);

    /**
     * Copy to a Moz2D DataSourceSurface.
     * Marked as virtual so that browsercomps can access this method.
     */
    virtual already_AddRefed<mozilla::gfx::DataSourceSurface> CopyToB8G8R8A8DataSourceSurface();

    /* return new Subimage with pointing to original image starting from aRect.pos
     * and size of aRect.size. New subimage keeping current image reference
     */
    already_AddRefed<gfxSubimageSurface> GetSubimage(const gfxRect& aRect);

    virtual already_AddRefed<gfxImageSurface> GetAsImageSurface() override;

    /** See gfxASurface.h. */
    static long ComputeStride(const mozilla::gfx::IntSize&, gfxImageFormat);

    virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
        override;
    virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
        override;
    virtual bool SizeOfIsMeasured() const override;

protected:
    gfxImageSurface();
    void InitWithData(unsigned char *aData, const mozilla::gfx::IntSize& aSize,
                      long aStride, gfxImageFormat aFormat);
    /**
     * See the parameters to the matching constructor.  This should only
     * be called once, in the constructor, which has already set mSize
     * and mFormat.
     */
    void AllocateAndInit(long aStride, int32_t aMinimalAllocation, bool aClear);
    void InitFromSurface(cairo_surface_t *csurf);

    long ComputeStride() const { 
        if (mSize.height < 0 || mSize.width < 0) {
            return 0;
        }
        return ComputeStride(mSize, mFormat);
    }

    void MakeInvalid();

    mozilla::gfx::IntSize mSize;
    bool mOwnsData;
    unsigned char *mData;
    gfxImageFormat mFormat;
    long mStride;
};

class gfxSubimageSurface : public gfxImageSurface {
protected:
    friend class gfxImageSurface;
    gfxSubimageSurface(gfxImageSurface* aParent,
                       unsigned char* aData,
                       const mozilla::gfx::IntSize& aSize,
                       gfxImageFormat aFormat);
private:
    RefPtr<gfxImageSurface> mParent;
};

#endif /* GFX_IMAGESURFACE_H */

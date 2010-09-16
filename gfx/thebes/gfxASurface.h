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

#ifndef GFX_ASURFACE_H
#define GFX_ASURFACE_H

#include "gfxTypes.h"
#include "gfxRect.h"
#include "nsAutoPtr.h"

typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo_user_data_key cairo_user_data_key_t;

typedef void (*thebes_destroy_func_t) (void *data);

class gfxImageSurface;

/**
 * A surface is something you can draw on. Instantiate a subclass of this
 * abstract class, and use gfxContext to draw on this surface.
 */
class THEBES_API gfxASurface {
public:
    nsrefcnt AddRef(void);
    nsrefcnt Release(void);

public:
    /**
     * The format for an image surface. For all formats with alpha data, 0
     * means transparent, 1 or 255 means fully opaque.
     */
    typedef enum {
        ImageFormatARGB32, ///< ARGB data in native endianness, using premultiplied alpha
        ImageFormatRGB24,  ///< xRGB data in native endianness
        ImageFormatA8,     ///< Only an alpha channel
        ImageFormatA1,     ///< Packed transparency information (one byte refers to 8 pixels)
        ImageFormatRGB16_565,  ///< RGB_565 data in native endianness
        ImageFormatUnknown
    } gfxImageFormat;

    typedef enum {
        SurfaceTypeImage,
        SurfaceTypePDF,
        SurfaceTypePS,
        SurfaceTypeXlib,
        SurfaceTypeXcb,
        SurfaceTypeGlitz,
        SurfaceTypeQuartz,
        SurfaceTypeWin32,
        SurfaceTypeBeOS,
        SurfaceTypeDirectFB,
        SurfaceTypeSVG,
        SurfaceTypeOS2,
        SurfaceTypeWin32Printing,
        SurfaceTypeQuartzImage,
        SurfaceTypeScript,
        SurfaceTypeQPainter,
        SurfaceTypeRecording,
        SurfaceTypeVG,
        SurfaceTypeGL,
        SurfaceTypeDRM,
        SurfaceTypeTee,
        SurfaceTypeXML,
        SurfaceTypeSkia,
        SurfaceTypeDDraw,
        SurfaceTypeD2D,
        SurfaceTypeMax
    } gfxSurfaceType;

    typedef enum {
        CONTENT_COLOR       = 0x1000,
        CONTENT_ALPHA       = 0x2000,
        CONTENT_COLOR_ALPHA = 0x3000
    } gfxContentType;

    /** Wrap the given cairo surface and return a gfxASurface for it.
     * This adds a reference to csurf (owned by the returned gfxASurface).
     */
    static already_AddRefed<gfxASurface> Wrap(cairo_surface_t *csurf);

    /*** this DOES NOT addref the surface */
    cairo_surface_t *CairoSurface() {
        NS_ASSERTION(mSurface != nsnull, "gfxASurface::CairoSurface called with mSurface == nsnull!");
        return mSurface;
    }

    gfxSurfaceType GetType() const;

    gfxContentType GetContentType() const;

    void SetDeviceOffset(const gfxPoint& offset);
    gfxPoint GetDeviceOffset() const;

    void Flush() const;
    void MarkDirty();
    void MarkDirty(const gfxRect& r);

    /* Printing backend functions */
    virtual nsresult BeginPrinting(const nsAString& aTitle, const nsAString& aPrintToFileName);
    virtual nsresult EndPrinting();
    virtual nsresult AbortPrinting();
    virtual nsresult BeginPage();
    virtual nsresult EndPage();

    void SetData(const cairo_user_data_key_t *key,
                 void *user_data,
                 thebes_destroy_func_t destroy);
    void *GetData(const cairo_user_data_key_t *key);

    virtual void Finish();

    /**
     * Create an offscreen surface that can be efficiently copied into
     * this surface (at least if tiling is not involved).
     * Returns null on error.
     */
    virtual already_AddRefed<gfxASurface> CreateSimilarSurface(gfxContentType aType,
                                                               const gfxIntSize& aSize);
    /**
     * Return trues if offscreen surfaces created from this surface
     * would behave differently depending on the gfxContentType. Returns
     * false if they don't (i.e. the surface returned by
     * CreateOffscreenSurface is always as if you passed
     * CONTENT_COLOR_ALPHA). Knowing this can be useful to avoid
     * recreating a surface just because it changed from opaque to
     * transparent.
     */
    virtual PRBool AreSimilarSurfacesSensitiveToContentType()
    {
        return PR_TRUE;
    }

    enum TextQuality {
        /**
         * TEXT_QUALITY_OK means that text is always rendered to a
         * transparent surface just as well as it would be rendered to an
         * opaque surface. This would normally only be true if
         * subpixel antialiasing is disabled or if the platform's
         * transparent surfaces support component alpha.
         */
        TEXT_QUALITY_OK,
        /**
         * TEXT_QUALITY_OK_OVER_OPAQUE_PIXELS means that text is rendered
         * to a transparent surface just as well as it would be rendered to an
         * opaque surface, but only if all the pixels the text is drawn
         * over already have opaque alpha values.
         */
        TEXT_QUALITY_OK_OVER_OPAQUE_PIXELS,
        /**
         * TEXT_QUALITY_BAD means that text is rendered
         * to a transparent surface worse than it would be rendered to an
         * opaque surface, even if all the pixels the text is drawn
         * over already have opaque alpha values.
         */
        TEXT_QUALITY_BAD
    };
    /**
     * Determine how well text would be rendered in transparent surfaces that
     * are similar to this surface.
     */
    virtual TextQuality GetTextQualityInTransparentSurfaces()
    {
        return TEXT_QUALITY_BAD;
    }

    int CairoStatus();

    /* Make sure that the given dimensions don't overflow a 32-bit signed int
     * using 4 bytes per pixel; optionally, make sure that either dimension
     * doesn't exceed the given limit.
     */
    static PRBool CheckSurfaceSize(const gfxIntSize& sz, PRInt32 limit = 0);

    /* Return the default set of context flags for this surface; these are
     * hints to the context about any special rendering considerations.  See
     * gfxContext::SetFlag for documentation.
     */
    virtual PRInt32 GetDefaultContextFlags() const { return 0; }

    static gfxContentType ContentFromFormat(gfxImageFormat format);
    static gfxImageFormat FormatFromContent(gfxContentType format);

    /**
     * Record number of bytes for given surface type.  Use positive bytes
     * for allocations and negative bytes for deallocations.
     */
    static void RecordMemoryUsedForSurfaceType(gfxASurface::gfxSurfaceType aType,
                                               PRInt32 aBytes);

    /**
     * Same as above, but use current surface type as returned by GetType().
     * The bytes will be accumulated until RecordMemoryFreed is called,
     * in which case the value that was recorded for this surface will
     * be freed.
     */
    void RecordMemoryUsed(PRInt32 aBytes);
    void RecordMemoryFreed();

    PRInt32 KnownMemoryUsed() { return mBytesRecorded; }

    static PRInt32 BytePerPixelFromFormat(gfxImageFormat format);

    virtual const gfxIntSize GetSize() const { return gfxIntSize(-1, -1); }

protected:
    gfxASurface() : mSurface(nsnull), mFloatingRefs(0), mBytesRecorded(0), mSurfaceValid(PR_FALSE)
    {
        MOZ_COUNT_CTOR(gfxASurface);
    }

    static gfxASurface* GetSurfaceWrapper(cairo_surface_t *csurf);
    static void SetSurfaceWrapper(cairo_surface_t *csurf, gfxASurface *asurf);

    void Init(cairo_surface_t *surface, PRBool existingSurface = PR_FALSE);

    virtual ~gfxASurface()
    {
        RecordMemoryFreed();

        MOZ_COUNT_DTOR(gfxASurface);
    }

    cairo_surface_t *mSurface;

private:
    static void SurfaceDestroyFunc(void *data);

    PRInt32 mFloatingRefs;
    PRInt32 mBytesRecorded;

protected:
    PRPackedBool mSurfaceValid;
};

/**
 * An Unknown surface; used to wrap unknown cairo_surface_t returns from cairo
 */
class THEBES_API gfxUnknownSurface : public gfxASurface {
public:
    gfxUnknownSurface(cairo_surface_t *surf) {
        Init(surf, PR_TRUE);
    }

    virtual ~gfxUnknownSurface() { }
};

#endif /* GFX_ASURFACE_H */

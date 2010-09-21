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
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include "nsIMemoryReporter.h"
#include "nsMemory.h"

#include "gfxASurface.h"

#include "gfxImageSurface.h"

#include "cairo.h"

#ifdef CAIRO_HAS_WIN32_SURFACE
#include "gfxWindowsSurface.h"
#endif
#ifdef CAIRO_HAS_D2D_SURFACE
#include "gfxD2DSurface.h"
#endif

#ifdef MOZ_X11
#include "gfxXlibSurface.h"
#endif

#ifdef CAIRO_HAS_QUARTZ_SURFACE
#include "gfxQuartzSurface.h"
#include "gfxQuartzImageSurface.h"
#endif

#ifdef MOZ_DFB
#include "gfxDirectFBSurface.h"
#endif

#ifdef CAIRO_HAS_QT_SURFACE
#include "gfxQPainterSurface.h"
#endif

#ifdef CAIRO_HAS_DDRAW_SURFACE
#include "gfxDDrawSurface.h"
#endif

#include <stdio.h>
#include <limits.h>

static cairo_user_data_key_t gfxasurface_pointer_key;

// Surfaces use refcounting that's tied to the cairo surface refcnt, to avoid
// refcount mismatch issues.
nsrefcnt
gfxASurface::AddRef(void)
{
    if (mSurfaceValid) {
        if (mFloatingRefs) {
            // eat a floating ref
            mFloatingRefs--;
        } else {
            cairo_surface_reference(mSurface);
        }

        return (nsrefcnt) cairo_surface_get_reference_count(mSurface);
    } else {
        // the surface isn't valid, but we still need to refcount
        // the gfxASurface
        return ++mFloatingRefs;
    }
}

nsrefcnt
gfxASurface::Release(void)
{
    if (mSurfaceValid) {
        NS_ASSERTION(mFloatingRefs == 0, "gfxASurface::Release with floating refs still hanging around!");

        // Note that there is a destructor set on user data for mSurface,
        // which will delete this gfxASurface wrapper when the surface's refcount goes
        // out of scope.
        nsrefcnt refcnt = (nsrefcnt) cairo_surface_get_reference_count(mSurface);
        cairo_surface_destroy(mSurface);

        // |this| may not be valid any more, don't use it!

        return --refcnt;
    } else {
        if (--mFloatingRefs == 0) {
            delete this;
            return 0;
        }

        return mFloatingRefs;
    }
}

void
gfxASurface::SurfaceDestroyFunc(void *data) {
    gfxASurface *surf = (gfxASurface*) data;
    // fprintf (stderr, "Deleting wrapper for %p (wrapper: %p)\n", surf->mSurface, data);
    delete surf;
}

gfxASurface*
gfxASurface::GetSurfaceWrapper(cairo_surface_t *csurf)
{
    return (gfxASurface*) cairo_surface_get_user_data(csurf, &gfxasurface_pointer_key);
}

void
gfxASurface::SetSurfaceWrapper(cairo_surface_t *csurf, gfxASurface *asurf)
{
    cairo_surface_set_user_data(csurf, &gfxasurface_pointer_key, asurf, SurfaceDestroyFunc);
}

already_AddRefed<gfxASurface>
gfxASurface::Wrap (cairo_surface_t *csurf)
{
    gfxASurface *result;

    /* Do we already have a wrapper for this surface? */
    result = GetSurfaceWrapper(csurf);
    if (result) {
        // fprintf(stderr, "Existing wrapper for %p -> %p\n", csurf, result);
        NS_ADDREF(result);
        return result;
    }

    /* No wrapper; figure out the surface type and create it */
    cairo_surface_type_t stype = cairo_surface_get_type(csurf);

    if (stype == CAIRO_SURFACE_TYPE_IMAGE) {
        result = new gfxImageSurface(csurf);
    }
#ifdef CAIRO_HAS_WIN32_SURFACE
    else if (stype == CAIRO_SURFACE_TYPE_WIN32 ||
             stype == CAIRO_SURFACE_TYPE_WIN32_PRINTING) {
        result = new gfxWindowsSurface(csurf);
    }
#endif
#ifdef CAIRO_HAS_D2D_SURFACE
    else if (stype == CAIRO_SURFACE_TYPE_D2D) {
        result = new gfxD2DSurface(csurf);
    }
#endif
#ifdef MOZ_X11
    else if (stype == CAIRO_SURFACE_TYPE_XLIB) {
        result = new gfxXlibSurface(csurf);
    }
#endif
#ifdef CAIRO_HAS_QUARTZ_SURFACE
    else if (stype == CAIRO_SURFACE_TYPE_QUARTZ) {
        result = new gfxQuartzSurface(csurf);
    }
    else if (stype == CAIRO_SURFACE_TYPE_QUARTZ_IMAGE) {
        result = new gfxQuartzImageSurface(csurf);
    }
#endif
#ifdef MOZ_DFB
    else if (stype == CAIRO_SURFACE_TYPE_DIRECTFB) {
        result = new gfxDirectFBSurface(csurf);
    }
#endif
#ifdef CAIRO_HAS_QT_SURFACE
    else if (stype == CAIRO_SURFACE_TYPE_QT) {
        result = new gfxQPainterSurface(csurf);
    }
#endif
#ifdef CAIRO_HAS_DDRAW_SURFACE
    else if (stype == CAIRO_SURFACE_TYPE_DDRAW) {
        result = new gfxDDrawSurface(csurf);
    }
#endif
    else {
        result = new gfxUnknownSurface(csurf);
    }

    // fprintf(stderr, "New wrapper for %p -> %p\n", csurf, result);

    NS_ADDREF(result);
    return result;
}

void
gfxASurface::Init(cairo_surface_t* surface, PRBool existingSurface)
{
    if (cairo_surface_status(surface)) {
        // the surface has an error on it
        mSurfaceValid = PR_FALSE;
        cairo_surface_destroy(surface);
        return;
    }

    SetSurfaceWrapper(surface, this);

    mSurface = surface;
    mSurfaceValid = PR_TRUE;

    if (existingSurface) {
        mFloatingRefs = 0;
    } else {
        mFloatingRefs = 1;
    }
}

gfxASurface::gfxSurfaceType
gfxASurface::GetType() const
{
    if (!mSurfaceValid)
        return (gfxSurfaceType)-1;

    return (gfxSurfaceType)cairo_surface_get_type(mSurface);
}

gfxASurface::gfxContentType
gfxASurface::GetContentType() const
{
    if (!mSurfaceValid)
        return (gfxContentType)-1;

    return (gfxContentType)cairo_surface_get_content(mSurface);
}

void
gfxASurface::SetDeviceOffset(const gfxPoint& offset)
{
    cairo_surface_set_device_offset(mSurface,
                                    offset.x, offset.y);
}

gfxPoint
gfxASurface::GetDeviceOffset() const
{
    gfxPoint pt;
    cairo_surface_get_device_offset(mSurface, &pt.x, &pt.y);
    return pt;
}

void
gfxASurface::Flush() const
{
    cairo_surface_flush(mSurface);
}

void
gfxASurface::MarkDirty()
{
    cairo_surface_mark_dirty(mSurface);
}

void
gfxASurface::MarkDirty(const gfxRect& r)
{
    cairo_surface_mark_dirty_rectangle(mSurface,
                                       (int) r.pos.x, (int) r.pos.y,
                                       (int) r.size.width, (int) r.size.height);
}

void
gfxASurface::SetData(const cairo_user_data_key_t *key,
                     void *user_data,
                     thebes_destroy_func_t destroy)
{
    cairo_surface_set_user_data(mSurface, key, user_data, destroy);
}

void *
gfxASurface::GetData(const cairo_user_data_key_t *key)
{
    return cairo_surface_get_user_data(mSurface, key);
}

void
gfxASurface::Finish()
{
    cairo_surface_finish(mSurface);
}

already_AddRefed<gfxASurface>
gfxASurface::CreateSimilarSurface(gfxContentType aContent,
                                  const gfxIntSize& aSize)
{
    if (!mSurface || !mSurfaceValid) {
      return nsnull;
    }
    
    cairo_surface_t *surface =
        cairo_surface_create_similar(mSurface, cairo_content_t(aContent),
                                     aSize.width, aSize.height);
    if (cairo_surface_status(surface)) {
        cairo_surface_destroy(surface);
        return nsnull;
    }

    nsRefPtr<gfxASurface> result = Wrap(surface);
    cairo_surface_destroy(surface);
    return result.forget();
}

int
gfxASurface::CairoStatus()
{
    if (!mSurfaceValid)
        return -1;

    return cairo_surface_status(mSurface);
}

/* static */
PRBool
gfxASurface::CheckSurfaceSize(const gfxIntSize& sz, PRInt32 limit)
{
    if (sz.width < 0 || sz.height < 0) {
        NS_WARNING("Surface width or height < 0!");
        return PR_FALSE;
    }

#if defined(XP_MACOSX)
    // CoreGraphics is limited to images < 32K in *height*, so clamp all surfaces on the Mac to that height
    if (sz.height > SHRT_MAX) {
        NS_WARNING("Surface size too large (would overflow)!");
        return PR_FALSE;
    }
#endif

    // check to make sure we don't overflow a 32-bit
    PRInt32 tmp = sz.width * sz.height;
    if (tmp && tmp / sz.height != sz.width) {
        NS_WARNING("Surface size too large (would overflow)!");
        return PR_FALSE;
    }

    // always assume 4-byte stride
    tmp = tmp * 4;
    if (tmp && tmp / 4 != sz.width * sz.height) {
        NS_WARNING("Surface size too large (would overflow)!");
        return PR_FALSE;
    }

    // reject images with sides bigger than limit
    if (limit &&
        (sz.width > limit || sz.height > limit))
        return PR_FALSE;

    return PR_TRUE;
}

nsresult
gfxASurface::BeginPrinting(const nsAString& aTitle, const nsAString& aPrintToFileName)
{
    return NS_OK;
}

nsresult
gfxASurface::EndPrinting()
{
    return NS_OK;
}

nsresult
gfxASurface::AbortPrinting()
{
    return NS_OK;
}

nsresult
gfxASurface::BeginPage()
{
    return NS_OK;
}

nsresult
gfxASurface::EndPage()
{
    return NS_OK;
}

gfxASurface::gfxContentType
gfxASurface::ContentFromFormat(gfxImageFormat format)
{
    switch (format) {
        case ImageFormatARGB32:
            return CONTENT_COLOR_ALPHA;
        case ImageFormatRGB24:
        case ImageFormatRGB16_565:
            return CONTENT_COLOR;
        case ImageFormatA8:
        case ImageFormatA1:
            return CONTENT_ALPHA;

        case ImageFormatUnknown:
        default:
            return CONTENT_COLOR;
    }
}

gfxASurface::gfxImageFormat
gfxASurface::FormatFromContent(gfxASurface::gfxContentType type)
{
    switch (type) {
        case CONTENT_COLOR_ALPHA:
            return ImageFormatARGB32;
        case CONTENT_ALPHA:
            return ImageFormatA8;
        case CONTENT_COLOR:
        default:
            return ImageFormatRGB24;
    }
}

PRInt32
gfxASurface::BytePerPixelFromFormat(gfxImageFormat format)
{
    switch (format) {
        case ImageFormatARGB32:
        case ImageFormatRGB24:
            return 4;
        case ImageFormatRGB16_565:
            return 2;
        case ImageFormatA8:
            return 1;
        default:
            NS_WARNING("Unknown byte per pixel value for Image format");
    }
    return 0;
}

/** Memory reporting **/

static const char *sSurfaceNamesForSurfaceType[] = {
    "gfx/surface/image",
    "gfx/surface/pdf",
    "gfx/surface/ps",
    "gfx/surface/xlib",
    "gfx/surface/xcb",
    "gfx/surface/glitz",
    "gfx/surface/quartz",
    "gfx/surface/win32",
    "gfx/surface/beos",
    "gfx/surface/directfb",
    "gfx/surface/svg",
    "gfx/surface/os2",
    "gfx/surface/win32printing",
    "gfx/surface/quartzimage",
    "gfx/surface/script",
    "gfx/surface/qpainter",
    "gfx/surface/recording",
    "gfx/surface/vg",
    "gfx/surface/gl",
    "gfx/surface/drm",
    "gfx/surface/tee",
    "gfx/surface/xml",
    "gfx/surface/skia",
    "gfx/surface/ddraw",
    "gfx/surface/d2d"
};

PR_STATIC_ASSERT(NS_ARRAY_LENGTH(sSurfaceNamesForSurfaceType) == gfxASurface::SurfaceTypeMax);
#ifdef CAIRO_HAS_D2D_SURFACE
PR_STATIC_ASSERT(CAIRO_SURFACE_TYPE_D2D == gfxASurface::SurfaceTypeD2D);
#endif
PR_STATIC_ASSERT(CAIRO_SURFACE_TYPE_SKIA == gfxASurface::SurfaceTypeSkia);

static const char *
SurfaceMemoryReporterPathForType(gfxASurface::gfxSurfaceType aType)
{
    if (aType < 0 ||
        aType >= gfxASurface::SurfaceTypeMax)
        return "gfx/surface/unknown";

    return sSurfaceNamesForSurfaceType[aType];
}

/* Surface size memory reporting */
static nsIMemoryReporter *gSurfaceMemoryReporters[gfxASurface::SurfaceTypeMax] = { 0 };
static PRInt64 gSurfaceMemoryUsed[gfxASurface::SurfaceTypeMax] = { 0 };

class SurfaceMemoryReporter :
    public nsIMemoryReporter
{
public:
    SurfaceMemoryReporter(gfxASurface::gfxSurfaceType aType)
        : mType(aType)
    { }

    NS_DECL_ISUPPORTS

    NS_IMETHOD GetPath(char **memoryPath) {
        *memoryPath = strdup(SurfaceMemoryReporterPathForType(mType));
        return NS_OK;
    }

    NS_IMETHOD GetDescription(char **desc) {
        *desc = strdup("Memory used by gfx surface of given type.");
        return NS_OK;
    }

    NS_IMETHOD GetMemoryUsed(PRInt64 *memoryUsed) {
        *memoryUsed = gSurfaceMemoryUsed[mType];
        return NS_OK;
    }

    gfxASurface::gfxSurfaceType mType;
};

NS_IMPL_ISUPPORTS1(SurfaceMemoryReporter, nsIMemoryReporter)

void
gfxASurface::RecordMemoryUsedForSurfaceType(gfxASurface::gfxSurfaceType aType,
                                            PRInt32 aBytes)
{
    if (aType < 0 || aType >= SurfaceTypeMax) {
        NS_WARNING("Invalid type to RecordMemoryUsedForSurfaceType!");
        return;
    }

    if (gSurfaceMemoryReporters[aType] == 0) {
        gSurfaceMemoryReporters[aType] = new SurfaceMemoryReporter(aType);
        NS_RegisterMemoryReporter(gSurfaceMemoryReporters[aType]);
    }

    gSurfaceMemoryUsed[aType] += aBytes;
}

void
gfxASurface::RecordMemoryUsed(PRInt32 aBytes)
{
    RecordMemoryUsedForSurfaceType(GetType(), aBytes);
    mBytesRecorded += aBytes;
}

void
gfxASurface::RecordMemoryFreed()
{
    if (mBytesRecorded) {
        RecordMemoryUsedForSurfaceType(GetType(), -mBytesRecorded);
        mBytesRecorded = 0;
    }
}

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIMemoryReporter.h"
#include "nsMemory.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Base64.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "nsISupportsImpl.h"
#include "mozilla/gfx/2D.h"
#include "gfx2DGlue.h"

#include "gfxASurface.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxPlatform.h"
#include "gfxRect.h"

#include "cairo.h"
#include <algorithm>

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

#if defined(CAIRO_HAS_QT_SURFACE) && defined(MOZ_WIDGET_QT)
#include "gfxQPainterSurface.h"
#endif

#include <stdio.h>
#include <limits.h>

#include "imgIEncoder.h"
#include "nsComponentManagerUtils.h"
#include "nsISupportsUtils.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsIClipboardHelper.h"

using namespace mozilla;
using namespace mozilla::gfx;

static cairo_user_data_key_t gfxasurface_pointer_key;

gfxASurface::gfxASurface()
 : mSurface(nullptr), mFloatingRefs(0), mBytesRecorded(0),
   mSurfaceValid(false), mAllowUseAsSource(true)
{
    MOZ_COUNT_CTOR(gfxASurface);
}

gfxASurface::~gfxASurface()
{
    RecordMemoryFreed();

    MOZ_COUNT_DTOR(gfxASurface);
}

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

nsrefcnt
gfxASurface::AddRefExternal(void)
{
  return AddRef();
}

nsrefcnt
gfxASurface::ReleaseExternal(void)
{
  return Release();
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
    if (!csurf)
        return nullptr;
    return (gfxASurface*) cairo_surface_get_user_data(csurf, &gfxasurface_pointer_key);
}

void
gfxASurface::SetSurfaceWrapper(cairo_surface_t *csurf, gfxASurface *asurf)
{
    if (!csurf)
        return;
    cairo_surface_set_user_data(csurf, &gfxasurface_pointer_key, asurf, SurfaceDestroyFunc);
}

already_AddRefed<gfxASurface>
gfxASurface::Wrap (cairo_surface_t *csurf, const gfxIntSize& aSize)
{
    nsRefPtr<gfxASurface> result;

    /* Do we already have a wrapper for this surface? */
    result = GetSurfaceWrapper(csurf);
    if (result) {
        // fprintf(stderr, "Existing wrapper for %p -> %p\n", csurf, result);
        return result.forget();
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
        result = new gfxQuartzSurface(csurf, aSize);
    }
    else if (stype == CAIRO_SURFACE_TYPE_QUARTZ_IMAGE) {
        result = new gfxQuartzImageSurface(csurf);
    }
#endif
#if defined(CAIRO_HAS_QT_SURFACE) && defined(MOZ_WIDGET_QT)
    else if (stype == CAIRO_SURFACE_TYPE_QT) {
        result = new gfxQPainterSurface(csurf);
    }
#endif
    else {
        result = new gfxUnknownSurface(csurf, aSize);
    }

    // fprintf(stderr, "New wrapper for %p -> %p\n", csurf, result);

    return result.forget();
}

void
gfxASurface::Init(cairo_surface_t* surface, bool existingSurface)
{
    SetSurfaceWrapper(surface, this);

    mSurface = surface;
    mSurfaceValid = surface && !cairo_surface_status(surface);

    if (existingSurface || !mSurfaceValid) {
        mFloatingRefs = 0;
    } else {
        mFloatingRefs = 1;
#ifdef MOZ_TREE_CAIRO
        if (cairo_surface_get_content(surface) != CAIRO_CONTENT_COLOR) {
            cairo_surface_set_subpixel_antialiasing(surface, CAIRO_SUBPIXEL_ANTIALIASING_DISABLED);
        }
#endif
    }
}

gfxSurfaceType
gfxASurface::GetType() const
{
    if (!mSurfaceValid)
        return (gfxSurfaceType)-1;

    return (gfxSurfaceType)cairo_surface_get_type(mSurface);
}

gfxContentType
gfxASurface::GetContentType() const
{
    if (!mSurfaceValid)
        return (gfxContentType)-1;

    return (gfxContentType)cairo_surface_get_content(mSurface);
}

void
gfxASurface::SetDeviceOffset(const gfxPoint& offset)
{
    if (!mSurfaceValid)
        return;
    cairo_surface_set_device_offset(mSurface,
                                    offset.x, offset.y);
}

gfxPoint
gfxASurface::GetDeviceOffset() const
{
    if (!mSurfaceValid)
        return gfxPoint(0.0, 0.0);
    gfxPoint pt;
    cairo_surface_get_device_offset(mSurface, &pt.x, &pt.y);
    return pt;
}

void
gfxASurface::Flush() const
{
    if (!mSurfaceValid)
        return;
    cairo_surface_flush(mSurface);
    gfxPlatform::ClearSourceSurfaceForSurface(const_cast<gfxASurface*>(this));
}

void
gfxASurface::MarkDirty()
{
    if (!mSurfaceValid)
        return;
    cairo_surface_mark_dirty(mSurface);
    gfxPlatform::ClearSourceSurfaceForSurface(this);
}

void
gfxASurface::MarkDirty(const gfxRect& r)
{
    if (!mSurfaceValid)
        return;
    cairo_surface_mark_dirty_rectangle(mSurface,
                                       (int) r.X(), (int) r.Y(),
                                       (int) r.Width(), (int) r.Height());
    gfxPlatform::ClearSourceSurfaceForSurface(this);
}

void
gfxASurface::SetData(const cairo_user_data_key_t *key,
                     void *user_data,
                     thebes_destroy_func_t destroy)
{
    if (!mSurfaceValid)
        return;
    cairo_surface_set_user_data(mSurface, key, user_data, destroy);
}

void *
gfxASurface::GetData(const cairo_user_data_key_t *key)
{
    if (!mSurfaceValid)
        return nullptr;
    return cairo_surface_get_user_data(mSurface, key);
}

void
gfxASurface::Finish()
{
    // null surfaces are allowed here
    cairo_surface_finish(mSurface);
}

already_AddRefed<gfxASurface>
gfxASurface::CreateSimilarSurface(gfxContentType aContent,
                                  const nsIntSize& aSize)
{
    if (!mSurface || !mSurfaceValid) {
      return nullptr;
    }
    
    cairo_surface_t *surface =
        cairo_surface_create_similar(mSurface, cairo_content_t(int(aContent)),
                                     aSize.width, aSize.height);
    if (cairo_surface_status(surface)) {
        cairo_surface_destroy(surface);
        return nullptr;
    }

    nsRefPtr<gfxASurface> result = Wrap(surface, aSize);
    cairo_surface_destroy(surface);
    return result.forget();
}

already_AddRefed<gfxImageSurface>
gfxASurface::GetAsReadableARGB32ImageSurface()
{
    nsRefPtr<gfxImageSurface> imgSurface = GetAsImageSurface();
    if (!imgSurface || imgSurface->Format() != gfxImageFormat::ARGB32) {
      imgSurface = CopyToARGB32ImageSurface();
    }
    return imgSurface.forget();
}

already_AddRefed<gfxImageSurface>
gfxASurface::CopyToARGB32ImageSurface()
{
    if (!mSurface || !mSurfaceValid) {
      return nullptr;
    }

    const nsIntSize size = GetSize();
    nsRefPtr<gfxImageSurface> imgSurface =
        new gfxImageSurface(size, gfxImageFormat::ARGB32);

    RefPtr<DrawTarget> dt = gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(imgSurface, IntSize(size.width, size.height));
    RefPtr<SourceSurface> source = gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(dt, this);

    dt->CopySurface(source, IntRect(0, 0, size.width, size.height), IntPoint());

    return imgSurface.forget();
}

int
gfxASurface::CairoStatus()
{
    if (!mSurfaceValid)
        return -1;

    return cairo_surface_status(mSurface);
}

/* static */
bool
gfxASurface::CheckSurfaceSize(const nsIntSize& sz, int32_t limit)
{
    if (sz.width < 0 || sz.height < 0) {
        NS_WARNING("Surface width or height < 0!");
        return false;
    }

    // reject images with sides bigger than limit
    if (limit && (sz.width > limit || sz.height > limit)) {
        NS_WARNING("Surface size too large (exceeds caller's limit)!");
        return false;
    }

#if defined(XP_MACOSX)
    // CoreGraphics is limited to images < 32K in *height*,
    // so clamp all surfaces on the Mac to that height
    if (sz.height > SHRT_MAX) {
        NS_WARNING("Surface size too large (exceeds CoreGraphics limit)!");
        return false;
    }
#endif

    // make sure the surface area doesn't overflow a int32_t
    CheckedInt<int32_t> tmp = sz.width;
    tmp *= sz.height;
    if (!tmp.isValid()) {
        NS_WARNING("Surface size too large (would overflow)!");
        return false;
    }

    // assuming 4-byte stride, make sure the allocation size
    // doesn't overflow a int32_t either
    tmp *= 4;
    if (!tmp.isValid()) {
        NS_WARNING("Allocation too large (would overflow)!");
        return false;
    }

    return true;
}

/* static */
int32_t
gfxASurface::FormatStrideForWidth(gfxImageFormat format, int32_t width)
{
    return cairo_format_stride_for_width((cairo_format_t)(int)format, (int)width);
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

gfxContentType
gfxASurface::ContentFromFormat(gfxImageFormat format)
{
    switch (format) {
        case gfxImageFormat::ARGB32:
            return gfxContentType::COLOR_ALPHA;
        case gfxImageFormat::RGB24:
        case gfxImageFormat::RGB16_565:
            return gfxContentType::COLOR;
        case gfxImageFormat::A8:
        case gfxImageFormat::A1:
            return gfxContentType::ALPHA;

        case gfxImageFormat::Unknown:
        default:
            return gfxContentType::COLOR;
    }
}

void
gfxASurface::SetSubpixelAntialiasingEnabled(bool aEnabled)
{
#ifdef MOZ_TREE_CAIRO
    if (!mSurfaceValid)
        return;
    cairo_surface_set_subpixel_antialiasing(mSurface,
        aEnabled ? CAIRO_SUBPIXEL_ANTIALIASING_ENABLED : CAIRO_SUBPIXEL_ANTIALIASING_DISABLED);
#endif
}

bool
gfxASurface::GetSubpixelAntialiasingEnabled()
{
    if (!mSurfaceValid)
      return false;
#ifdef MOZ_TREE_CAIRO
    return cairo_surface_get_subpixel_antialiasing(mSurface) == CAIRO_SUBPIXEL_ANTIALIASING_ENABLED;
#else
    return true;
#endif
}

gfxMemoryLocation
gfxASurface::GetMemoryLocation() const
{
    return gfxMemoryLocation::IN_PROCESS_HEAP;
}

int32_t
gfxASurface::BytePerPixelFromFormat(gfxImageFormat format)
{
    switch (format) {
        case gfxImageFormat::ARGB32:
        case gfxImageFormat::RGB24:
            return 4;
        case gfxImageFormat::RGB16_565:
            return 2;
        case gfxImageFormat::A8:
            return 1;
        default:
            NS_WARNING("Unknown byte per pixel value for Image format");
    }
    return 0;
}

/** Memory reporting **/

static const char *sDefaultSurfaceDescription =
    "Memory used by gfx surface of the given type.";

struct SurfaceMemoryReporterAttrs {
  const char *path;
  const char *description;
};

static const SurfaceMemoryReporterAttrs sSurfaceMemoryReporterAttrs[] = {
    {"gfx-surface-image", nullptr},
    {"gfx-surface-pdf", nullptr},
    {"gfx-surface-ps", nullptr},
    {"gfx-surface-xlib",
     "Memory used by xlib surfaces to store pixmaps. This memory lives in "
     "the X server's process rather than in this application, so the bytes "
     "accounted for here aren't counted in vsize, resident, explicit, or any of "
     "the other measurements on this page."},
    {"gfx-surface-xcb", nullptr},
    {"gfx-surface-glitz???", nullptr},       // should never be used
    {"gfx-surface-quartz", nullptr},
    {"gfx-surface-win32", nullptr},
    {"gfx-surface-beos", nullptr},
    {"gfx-surface-directfb???", nullptr},    // should never be used
    {"gfx-surface-svg", nullptr},
    {"gfx-surface-os2", nullptr},
    {"gfx-surface-win32printing", nullptr},
    {"gfx-surface-quartzimage", nullptr},
    {"gfx-surface-script", nullptr},
    {"gfx-surface-qpainter", nullptr},
    {"gfx-surface-recording", nullptr},
    {"gfx-surface-vg", nullptr},
    {"gfx-surface-gl", nullptr},
    {"gfx-surface-drm", nullptr},
    {"gfx-surface-tee", nullptr},
    {"gfx-surface-xml", nullptr},
    {"gfx-surface-skia", nullptr},
    {"gfx-surface-subsurface", nullptr},
    {"gfx-surface-d2d", nullptr},
};

PR_STATIC_ASSERT(MOZ_ARRAY_LENGTH(sSurfaceMemoryReporterAttrs) ==
                 size_t(gfxSurfaceType::Max));
#ifdef CAIRO_HAS_D2D_SURFACE
PR_STATIC_ASSERT(uint32_t(CAIRO_SURFACE_TYPE_D2D) ==
                 uint32_t(gfxSurfaceType::D2D));
#endif
PR_STATIC_ASSERT(uint32_t(CAIRO_SURFACE_TYPE_SKIA) ==
                 uint32_t(gfxSurfaceType::Skia));

/* Surface size memory reporting */

static int64_t gSurfaceMemoryUsed[size_t(gfxSurfaceType::Max)] = { 0 };

class SurfaceMemoryReporter MOZ_FINAL : public nsIMemoryReporter
{
    ~SurfaceMemoryReporter() {}

public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD CollectReports(nsIMemoryReporterCallback *aCb,
                              nsISupports *aClosure, bool aAnonymize)
    {
        const size_t len = ArrayLength(sSurfaceMemoryReporterAttrs);
        for (size_t i = 0; i < len; i++) {
            int64_t amount = gSurfaceMemoryUsed[i];

            if (amount != 0) {
                const char *path = sSurfaceMemoryReporterAttrs[i].path;
                const char *desc = sSurfaceMemoryReporterAttrs[i].description;
                if (!desc) {
                    desc = sDefaultSurfaceDescription;
                }

                nsresult rv = aCb->Callback(EmptyCString(), nsCString(path),
                                            KIND_OTHER, UNITS_BYTES,
                                            gSurfaceMemoryUsed[i],
                                            nsCString(desc), aClosure);
                NS_ENSURE_SUCCESS(rv, rv);
            }
        }

        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(SurfaceMemoryReporter, nsIMemoryReporter)

void
gfxASurface::RecordMemoryUsedForSurfaceType(gfxSurfaceType aType,
                                            int32_t aBytes)
{
    if (int(aType) < 0 || aType >= gfxSurfaceType::Max) {
        NS_WARNING("Invalid type to RecordMemoryUsedForSurfaceType!");
        return;
    }

    static bool registered = false;
    if (!registered) {
        RegisterStrongMemoryReporter(new SurfaceMemoryReporter());
        registered = true;
    }

    gSurfaceMemoryUsed[size_t(aType)] += aBytes;
}

void
gfxASurface::RecordMemoryUsed(int32_t aBytes)
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

size_t
gfxASurface::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
    // We don't measure mSurface because cairo doesn't allow it.
    return 0;
}

size_t
gfxASurface::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

/* static */ uint8_t
gfxASurface::BytesPerPixel(gfxImageFormat aImageFormat)
{
  switch (aImageFormat) {
    case gfxImageFormat::ARGB32:
      return 4;
    case gfxImageFormat::RGB24:
      return 4;
    case gfxImageFormat::RGB16_565:
      return 2;
    case gfxImageFormat::A8:
      return 1;
    case gfxImageFormat::A1:
      return 1; // Close enough
    case gfxImageFormat::Unknown:
    default:
      NS_NOTREACHED("Not really sure what you want me to say here");
      return 0;
  }
}

void
gfxASurface::SetOpaqueRect(const gfxRect& aRect)
{
    if (aRect.IsEmpty()) {
        mOpaqueRect = nullptr;
    } else if (!!mOpaqueRect) {
        *mOpaqueRect = aRect;
    } else {
        mOpaqueRect = MakeUnique<gfxRect>(aRect);
    }
}

/* static */const gfxRect&
gfxASurface::GetEmptyOpaqueRect()
{
  static const gfxRect empty(0, 0, 0, 0);
  return empty;
}

const nsIntSize
gfxASurface::GetSize() const
{
  return nsIntSize(-1, -1);
}

already_AddRefed<gfxImageSurface>
gfxASurface::GetAsImageSurface()
{
  return nullptr;
}

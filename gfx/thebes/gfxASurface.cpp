/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIMemoryReporter.h"
#include "nsMemory.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Attributes.h"

#include "gfxASurface.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"

#include "nsRect.h"

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

#if defined(CAIRO_HAS_QT_SURFACE) && defined(MOZ_WIDGET_QT)
#include "gfxQPainterSurface.h"
#endif

#include <stdio.h>
#include <limits.h>

#include "imgIEncoder.h"
#include "nsComponentManagerUtils.h"
#include "prmem.h"
#include "nsISupportsUtils.h"
#include "plbase64.h"
#include "nsCOMPtr.h"
#include "nsIConsoleService.h"
#include "nsServiceManagerUtils.h"
#include "nsStringGlue.h"
#include "nsIClipboardHelper.h"

using mozilla::CheckedInt;

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
    if (!csurf)
        return NULL;
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
#if defined(CAIRO_HAS_QT_SURFACE) && defined(MOZ_WIDGET_QT)
    else if (stype == CAIRO_SURFACE_TYPE_QT) {
        result = new gfxQPainterSurface(csurf);
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
}

void
gfxASurface::MarkDirty()
{
    if (!mSurfaceValid)
        return;
    cairo_surface_mark_dirty(mSurface);
}

void
gfxASurface::MarkDirty(const gfxRect& r)
{
    if (!mSurfaceValid)
        return;
    cairo_surface_mark_dirty_rectangle(mSurface,
                                       (int) r.X(), (int) r.Y(),
                                       (int) r.Width(), (int) r.Height());
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
        return NULL;
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
bool
gfxASurface::CheckSurfaceSize(const gfxIntSize& sz, PRInt32 limit)
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

    // make sure the surface area doesn't overflow a PRInt32
    CheckedInt<PRInt32> tmp = sz.width;
    tmp *= sz.height;
    if (!tmp.isValid()) {
        NS_WARNING("Surface size too large (would overflow)!");
        return false;
    }

    // assuming 4-byte stride, make sure the allocation size
    // doesn't overflow a PRInt32 either
    tmp *= 4;
    if (!tmp.isValid()) {
        NS_WARNING("Allocation too large (would overflow)!");
        return false;
    }

    return true;
}

/* static */
PRInt32
gfxASurface::FormatStrideForWidth(gfxImageFormat format, PRInt32 width)
{
    return cairo_format_stride_for_width((cairo_format_t)format, (int)width);
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

gfxASurface::MemoryLocation
gfxASurface::GetMemoryLocation() const
{
    return MEMORY_IN_PROCESS_HEAP;
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

void
gfxASurface::FastMovePixels(const nsIntRect& aSourceRect,
                            const nsIntPoint& aDestTopLeft)
{
    // Used when the backend can internally handle self copies.
    nsIntRect dest(aDestTopLeft, aSourceRect.Size());
    
    nsRefPtr<gfxContext> ctx = new gfxContext(this);
    ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
    nsIntPoint srcOrigin = dest.TopLeft() - aSourceRect.TopLeft();
    ctx->SetSource(this, gfxPoint(srcOrigin.x, srcOrigin.y));
    ctx->Rectangle(gfxRect(dest.x, dest.y, dest.width, dest.height));
    ctx->Fill();
}

void
gfxASurface::MovePixels(const nsIntRect& aSourceRect,
                        const nsIntPoint& aDestTopLeft)
{
    // Assume the backend can't handle self copying well and allocate
    // a temporary surface instead.
    nsRefPtr<gfxASurface> tmp = 
      CreateSimilarSurface(GetContentType(), 
                           gfxIntSize(aSourceRect.width, aSourceRect.height));
    // CreateSimilarSurface can return nsnull if the current surface is
    // in an error state. This isn't good, but its better to carry
    // on with the error surface instead of crashing.
    NS_ASSERTION(tmp, "Must have temporary surface to move pixels!");
    if (!tmp) {
        return;
    }
    nsRefPtr<gfxContext> ctx = new gfxContext(tmp);
    ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
    ctx->SetSource(this, gfxPoint(-aSourceRect.x, -aSourceRect.y));
    ctx->Paint();

    ctx = new gfxContext(this);
    ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
    ctx->SetSource(tmp, gfxPoint(aDestTopLeft.x, aDestTopLeft.y));
    ctx->Rectangle(gfxRect(aDestTopLeft.x, 
                           aDestTopLeft.y, 
                           aSourceRect.width, 
                           aSourceRect.height));
    ctx->Fill();
}

/** Memory reporting **/

static const char *sDefaultSurfaceDescription =
    "Memory used by gfx surface of the given type.";

struct SurfaceMemoryReporterAttrs {
  const char *path;
  const char *description;
};

static const SurfaceMemoryReporterAttrs sSurfaceMemoryReporterAttrs[] = {
    {"gfx-surface-image", nsnull},
    {"gfx-surface-pdf", nsnull},
    {"gfx-surface-ps", nsnull},
    {"gfx-surface-xlib",
     "Memory used by xlib surfaces to store pixmaps. This memory lives in "
     "the X server's process rather than in this application, so the bytes "
     "accounted for here aren't counted in vsize, resident, explicit, or any of "
     "the other measurements on this page."},
    {"gfx-surface-xcb", nsnull},
    {"gfx-surface-glitz???", nsnull},       // should never be used
    {"gfx-surface-quartz", nsnull},
    {"gfx-surface-win32", nsnull},
    {"gfx-surface-beos", nsnull},
    {"gfx-surface-directfb???", nsnull},    // should never be used
    {"gfx-surface-svg", nsnull},
    {"gfx-surface-os2", nsnull},
    {"gfx-surface-win32printing", nsnull},
    {"gfx-surface-quartzimage", nsnull},
    {"gfx-surface-script", nsnull},
    {"gfx-surface-qpainter", nsnull},
    {"gfx-surface-recording", nsnull},
    {"gfx-surface-vg", nsnull},
    {"gfx-surface-gl", nsnull},
    {"gfx-surface-drm", nsnull},
    {"gfx-surface-tee", nsnull},
    {"gfx-surface-xml", nsnull},
    {"gfx-surface-skia", nsnull},
    {"gfx-surface-subsurface", nsnull},
    {"gfx-surface-d2d", nsnull},
};

PR_STATIC_ASSERT(NS_ARRAY_LENGTH(sSurfaceMemoryReporterAttrs) ==
                 gfxASurface::SurfaceTypeMax);
#ifdef CAIRO_HAS_D2D_SURFACE
PR_STATIC_ASSERT(PRUint32(CAIRO_SURFACE_TYPE_D2D) ==
                 PRUint32(gfxASurface::SurfaceTypeD2D));
#endif
PR_STATIC_ASSERT(PRUint32(CAIRO_SURFACE_TYPE_SKIA) ==
                 PRUint32(gfxASurface::SurfaceTypeSkia));

/* Surface size memory reporting */

static PRInt64 gSurfaceMemoryUsed[gfxASurface::SurfaceTypeMax] = { 0 };

class SurfaceMemoryReporter MOZ_FINAL :
    public nsIMemoryMultiReporter
{
public:
    SurfaceMemoryReporter()
    { }

    NS_DECL_ISUPPORTS

    NS_IMETHOD GetName(nsACString &name)
    {
        name.AssignLiteral("gfx-surface");
        return NS_OK;
    }

    NS_IMETHOD CollectReports(nsIMemoryMultiReporterCallback *aCb,
                              nsISupports *aClosure)
    {
        size_t len = NS_ARRAY_LENGTH(sSurfaceMemoryReporterAttrs);
        for (size_t i = 0; i < len; i++) {
            PRInt64 amount = gSurfaceMemoryUsed[i];

            if (amount != 0) {
                const char *path = sSurfaceMemoryReporterAttrs[i].path;
                const char *desc = sSurfaceMemoryReporterAttrs[i].description;
                if (!desc) {
                    desc = sDefaultSurfaceDescription;
                }

                nsresult rv = aCb->Callback(EmptyCString(), nsCString(path),
                                            nsIMemoryReporter::KIND_OTHER,
                                            nsIMemoryReporter::UNITS_BYTES, 
                                            gSurfaceMemoryUsed[i],
                                            nsCString(desc), aClosure);
                NS_ENSURE_SUCCESS(rv, rv);
            }
        }

        return NS_OK;
    }

    NS_IMETHOD GetExplicitNonHeap(PRInt64 *n)
    {
        *n = 0; // this reporter makes neither "explicit" non NONHEAP reports
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS1(SurfaceMemoryReporter, nsIMemoryMultiReporter)

void
gfxASurface::RecordMemoryUsedForSurfaceType(gfxASurface::gfxSurfaceType aType,
                                            PRInt32 aBytes)
{
    if (aType < 0 || aType >= SurfaceTypeMax) {
        NS_WARNING("Invalid type to RecordMemoryUsedForSurfaceType!");
        return;
    }

    static bool registered = false;
    if (!registered) {
        NS_RegisterMemoryMultiReporter(new SurfaceMemoryReporter());
        registered = true;
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

#ifdef MOZ_DUMP_IMAGES
void
gfxASurface::WriteAsPNG(const char* aFile)
{
    FILE *file = fopen(aFile, "wb");
    if (file) {
      WriteAsPNG_internal(file, true);
      fclose(file);
    } else {
      NS_WARNING("Failed to create file!\n");
    }
}

void
gfxASurface::DumpAsDataURL(FILE* aOutput)
{
  WriteAsPNG_internal(aOutput, false);
}

void
gfxASurface::PrintAsDataURL()
{
  WriteAsPNG_internal(stdout, false);
  fprintf(stdout, "\n");
}

void
gfxASurface::CopyAsDataURL()
{
  WriteAsPNG_internal(nsnull, false);
}

/**
 * Write to a PNG file. If aBinary is true, then it is written
 * as binary, otherwise as a data URL. If no file is specified then
 * data is copied to the clipboard (must not be binary!).
 */
void
gfxASurface::WriteAsPNG_internal(FILE* aFile, bool aBinary)
{
  nsRefPtr<gfxImageSurface> imgsurf = GetAsImageSurface();
  gfxIntSize size;

  if (!imgsurf) {
    size = GetSize();
    if (size.width == -1 && size.height == -1) {
      printf("Could not determine surface size\n");
      return;
    }

    imgsurf =
      new gfxImageSurface(gfxIntSize(size.width, size.height),
                          gfxASurface::ImageFormatARGB32);

    if (!imgsurf || imgsurf->CairoStatus()) {
      printf("Could not allocate image surface\n");
      return;
    }

    nsRefPtr<gfxContext> ctx = new gfxContext(imgsurf);
    if (!ctx || ctx->HasError()) {
      printf("Could not allocate image context\n");
      return;
    }

    ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
    ctx->SetSource(this, gfxPoint(0, 0));
    ctx->Paint();
  }
  size = imgsurf->GetSize();

  nsCOMPtr<imgIEncoder> encoder =
    do_CreateInstance("@mozilla.org/image/encoder;2?type=image/png");
  if (!encoder) {
    PRInt32 w = NS_MIN(size.width, 8);
    PRInt32 h = NS_MIN(size.height, 8);
    printf("Could not create encoder. Printing %dx%d pixels.\n", w, h);
    for (PRInt32 y = 0; y < h; ++y) {
      for (PRInt32 x = 0; x < w; ++x) {
        printf("%x ", reinterpret_cast<PRUint32*>(imgsurf->Data())[y*imgsurf->Stride()+ x]);
      }
    }
    return;
  }

  nsresult rv = encoder->InitFromData(imgsurf->Data(),
                                      size.width * size.height * 4,
                                      size.width,
                                      size.height,
                                      imgsurf->Stride(),
                                      imgIEncoder::INPUT_FORMAT_HOSTARGB,
                                      NS_LITERAL_STRING(""));
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIInputStream> imgStream;
  CallQueryInterface(encoder.get(), getter_AddRefs(imgStream));
  if (!imgStream)
    return;

  PRUint32 bufSize;
  rv = imgStream->Available(&bufSize);
  if (NS_FAILED(rv))
    return;

  // ...leave a little extra room so we can call read again and make sure we
  // got everything. 16 bytes for better padding (maybe)
  bufSize += 16;
  PRUint32 imgSize = 0;
  char* imgData = (char*)PR_Malloc(bufSize);
  if (!imgData)
    return;
  PRUint32 numReadThisTime = 0;
  while ((rv = imgStream->Read(&imgData[imgSize],
                               bufSize - imgSize,
                               &numReadThisTime)) == NS_OK && numReadThisTime > 0)
  {
    imgSize += numReadThisTime;
    if (imgSize == bufSize) {
      // need a bigger buffer, just double
      bufSize *= 2;
      char* newImgData = (char*)PR_Realloc(imgData, bufSize);
      if (!newImgData) {
        PR_Free(imgData);
        return;
      }
      imgData = newImgData;
    }
  }

  if (aBinary) {
    if (aFile) {
      fwrite(imgData, 1, imgSize, aFile);
    } else {
      NS_WARNING("Can't write binary image data without a file!");
    }
    return;
  }

  // base 64, result will be NULL terminated
  char* encodedImg = PL_Base64Encode(imgData, imgSize, nsnull);
  PR_Free(imgData);
  if (!encodedImg) // not sure why this would fail
    return;

  nsCString string("data:image/png;base64,");
  string.Append(encodedImg);

  if (aFile) {
#ifdef ANDROID
     if (aFile == stdout || aFile == stderr) {
       // ADB logcat cuts off long strings so we will break it down
       const char* cStr = string.BeginReading();
       size_t len = strlen(cStr);
       while (true) {
         printf_stderr("IMG: %.140s\n", cStr);
         if (len <= 140)
           break;
         len -= 140;
         cStr += 140;
       }
     }
#endif
    fprintf(aFile, "%s", string.BeginReading());
  } else {
    nsCOMPtr<nsIClipboardHelper> clipboard(do_GetService("@mozilla.org/widget/clipboardhelper;1", &rv));
    if (clipboard) {
      clipboard->CopyString(NS_ConvertASCIItoUTF16(string), nsnull);
    }
  }

  PR_Free(encodedImg);

  return;
}
#endif


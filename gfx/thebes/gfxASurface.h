/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_ASURFACE_H
#define GFX_ASURFACE_H

#include "gfxTypes.h"
#include "gfxRect.h"
#include "nsAutoPtr.h"
#include "nsAutoRef.h"
#include "nsThreadUtils.h"

typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo_user_data_key cairo_user_data_key_t;

typedef void (*thebes_destroy_func_t) (void *data);

class gfxImageSurface;
struct nsIntPoint;
struct nsIntRect;

/**
 * A surface is something you can draw on. Instantiate a subclass of this
 * abstract class, and use gfxContext to draw on this surface.
 */
class THEBES_API gfxASurface {
public:
#ifdef MOZILLA_INTERNAL_API
    nsrefcnt AddRef(void);
    nsrefcnt Release(void);

    // These functions exist so that browsercomps can refcount a gfxASurface
    virtual nsresult AddRefExternal(void)
    {
      return AddRef();
    }
    virtual nsresult ReleaseExternal(void)
    {
      return Release();
    }
#else
    virtual nsresult AddRef(void);
    virtual nsresult Release(void);
#endif

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
        SurfaceTypeGlitz,           // unused, but needed for cairo parity
        SurfaceTypeQuartz,
        SurfaceTypeWin32,
        SurfaceTypeBeOS,
        SurfaceTypeDirectFB,        // unused, but needed for cairo parity
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
        SurfaceTypeSubsurface,
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

    virtual bool GetRotateForLandscape() { return false; }

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
     * Returns an image surface for this surface, or nsnull if not supported.
     * This will not copy image data, just wraps an image surface around
     * pixel data already available in memory.
     */
    virtual already_AddRefed<gfxImageSurface> GetAsImageSurface()
    {
      return nsnull;
    }

    int CairoStatus();

    /* Make sure that the given dimensions don't overflow a 32-bit signed int
     * using 4 bytes per pixel; optionally, make sure that either dimension
     * doesn't exceed the given limit.
     */
    static bool CheckSurfaceSize(const gfxIntSize& sz, PRInt32 limit = 0);

    /* Provide a stride value that will respect all alignment requirements of
     * the accelerated image-rendering code.
     */
    static PRInt32 FormatStrideForWidth(gfxImageFormat format, PRInt32 width);

    /* Return the default set of context flags for this surface; these are
     * hints to the context about any special rendering considerations.  See
     * gfxContext::SetFlag for documentation.
     */
    virtual PRInt32 GetDefaultContextFlags() const { return 0; }

    static gfxContentType ContentFromFormat(gfxImageFormat format);

    void SetSubpixelAntialiasingEnabled(bool aEnabled);
    bool GetSubpixelAntialiasingEnabled();

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

    virtual PRInt32 KnownMemoryUsed() { return mBytesRecorded; }

    /**
     * The memory used by this surface (as reported by KnownMemoryUsed()) can
     * either live in this process's heap, in this process but outside the
     * heap, or in another process altogether.
     */
    enum MemoryLocation {
      MEMORY_IN_PROCESS_HEAP,
      MEMORY_IN_PROCESS_NONHEAP,
      MEMORY_OUT_OF_PROCESS
    };

    /**
     * Where does this surface's memory live?  By default, we say it's in this
     * process's heap.
     */
    virtual MemoryLocation GetMemoryLocation() const;

    static PRInt32 BytePerPixelFromFormat(gfxImageFormat format);

    virtual const gfxIntSize GetSize() const { return gfxIntSize(-1, -1); }

#ifdef MOZ_DUMP_PAINTING
    /**
     * Debug functions to encode the current image as a PNG and export it.
     */

    /**
     * Writes a binary PNG file.
     */
    void WriteAsPNG(const char* aFile);

    /**
     * Write as a PNG encoded Data URL to a file.
     */
    void DumpAsDataURL(FILE* aOutput = stdout);

    /**
     * Write as a PNG encoded Data URL to stdout.
     */
    void PrintAsDataURL();

    /**
     * Copy a PNG encoded Data URL to the clipboard.
     */
    void CopyAsDataURL();
    
    void WriteAsPNG_internal(FILE* aFile, bool aBinary);
#endif

    void SetOpaqueRect(const gfxRect& aRect) {
        if (aRect.IsEmpty()) {
            mOpaqueRect = nsnull;
        } else if (mOpaqueRect) {
            *mOpaqueRect = aRect;
        } else {
            mOpaqueRect = new gfxRect(aRect);
        }
    }
    const gfxRect& GetOpaqueRect() {
        if (mOpaqueRect)
            return *mOpaqueRect;
        static const gfxRect empty(0, 0, 0, 0);
        return empty;
    }

    /**
     * Move the pixels in |aSourceRect| to |aDestTopLeft|.  Like with
     * memmove(), |aSourceRect| and the rectangle defined by
     * |aDestTopLeft| are allowed to overlap, and the effect is
     * equivalent to copying |aSourceRect| to a scratch surface and
     * then back to |aDestTopLeft|.
     *
     * |aSourceRect| and the destination rectangle defined by
     * |aDestTopLeft| are clipped to this surface's bounds.
     */
    virtual void MovePixels(const nsIntRect& aSourceRect,
                            const nsIntPoint& aDestTopLeft);

    /**
     * Mark the surface as being allowed/not allowed to be used as a source.
     */
    void SetAllowUseAsSource(bool aAllow) { mAllowUseAsSource = aAllow; }
    bool GetAllowUseAsSource() { return mAllowUseAsSource; }

protected:
    gfxASurface() : mSurface(nsnull), mFloatingRefs(0), mBytesRecorded(0),
                    mSurfaceValid(false), mAllowUseAsSource(true)
    {
        MOZ_COUNT_CTOR(gfxASurface);
    }

    static gfxASurface* GetSurfaceWrapper(cairo_surface_t *csurf);
    static void SetSurfaceWrapper(cairo_surface_t *csurf, gfxASurface *asurf);

    /**
     * An implementation of MovePixels that assumes the backend can
     * internally handle this operation and doesn't allocate any
     * temporary surfaces.
     */
    void FastMovePixels(const nsIntRect& aSourceRect,
                        const nsIntPoint& aDestTopLeft);

    // NB: Init() *must* be called from within subclass's
    // constructors.  It's unsafe to call it after the ctor finishes;
    // leaks and use-after-frees are possible.
    void Init(cairo_surface_t *surface, bool existingSurface = false);

    virtual ~gfxASurface()
    {
        RecordMemoryFreed();

        MOZ_COUNT_DTOR(gfxASurface);
    }

    cairo_surface_t *mSurface;
    nsAutoPtr<gfxRect> mOpaqueRect;

private:
    static void SurfaceDestroyFunc(void *data);

    PRInt32 mFloatingRefs;
    PRInt32 mBytesRecorded;

protected:
    bool mSurfaceValid;
    bool mAllowUseAsSource;
};

/**
 * An Unknown surface; used to wrap unknown cairo_surface_t returns from cairo
 */
class THEBES_API gfxUnknownSurface : public gfxASurface {
public:
    gfxUnknownSurface(cairo_surface_t *surf) {
        Init(surf, true);
    }

    virtual ~gfxUnknownSurface() { }
};

#ifndef XPCOM_GLUE_AVOID_NSPR
/**
 * We need to be able to hold a reference to a gfxASurface from Image
 * subclasses. This is potentially a problem since Images can be addrefed
 * or released off the main thread. We can ensure that we never AddRef
 * a gfxASurface off the main thread, but we might want to Release due
 * to an Image being destroyed off the main thread.
 * 
 * We use nsCountedRef<nsMainThreadSurfaceRef> to reference the
 * gfxASurface. When AddRefing, we assert that we're on the main thread.
 * When Releasing, if we're not on the main thread, we post an event to
 * the main thread to do the actual release.
 */
class nsMainThreadSurfaceRef;

template <>
class nsAutoRefTraits<nsMainThreadSurfaceRef> {
public:
  typedef gfxASurface* RawRef;

  /**
   * The XPCOM event that will do the actual release on the main thread.
   */
  class SurfaceReleaser : public nsRunnable {
  public:
    SurfaceReleaser(RawRef aRef) : mRef(aRef) {}
    NS_IMETHOD Run() {
      mRef->Release();
      return NS_OK;
    }
    RawRef mRef;
  };

  static RawRef Void() { return nsnull; }
  static void Release(RawRef aRawRef)
  {
    if (NS_IsMainThread()) {
      aRawRef->Release();
      return;
    }
    nsCOMPtr<nsIRunnable> runnable = new SurfaceReleaser(aRawRef);
    NS_DispatchToMainThread(runnable);
  }
  static void AddRef(RawRef aRawRef)
  {
    NS_ASSERTION(NS_IsMainThread(),
                 "Can only add a reference on the main thread");
    aRawRef->AddRef();
  }
};

#endif
#endif /* GFX_ASURFACE_H */

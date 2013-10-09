/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PATTERN_H
#define GFX_PATTERN_H

#include "gfxTypes.h"

#include "gfxMatrix.h"
#include "mozilla/Alignment.h"
#include "mozilla/gfx/2D.h"
#include "GraphicsFilter.h"
#include "nsISupportsImpl.h"
#include "nsAutoPtr.h"

class gfxContext;
class gfxASurface;
struct gfxRGBA;
typedef struct _cairo_pattern cairo_pattern_t;


class gfxPattern {
    NS_INLINE_DECL_REFCOUNTING(gfxPattern)

public:
    gfxPattern(cairo_pattern_t *aPattern);
    gfxPattern(const gfxRGBA& aColor);
    gfxPattern(gfxASurface *surface); // from another surface
    // linear
    gfxPattern(gfxFloat x0, gfxFloat y0, gfxFloat x1, gfxFloat y1); // linear
    gfxPattern(gfxFloat cx0, gfxFloat cy0, gfxFloat radius0,
               gfxFloat cx1, gfxFloat cy1, gfxFloat radius1); // radial
    gfxPattern(mozilla::gfx::SourceSurface *aSurface,
               const mozilla::gfx::Matrix &aTransform); // Azure
    virtual ~gfxPattern();

    cairo_pattern_t *CairoPattern();
    void AddColorStop(gfxFloat offset, const gfxRGBA& c);
    void SetColorStops(mozilla::RefPtr<mozilla::gfx::GradientStops> aStops);

    void SetMatrix(const gfxMatrix& matrix);
    gfxMatrix GetMatrix() const;
    gfxMatrix GetInverseMatrix() const;

    /* Get an Azure Pattern for the current Cairo pattern. aPattern transform
     * specifies the transform that was set on the DrawTarget when the pattern
     * was set. When this is nullptr it is assumed the transform is identical
     * to the current transform.
     */
    mozilla::gfx::Pattern *GetPattern(mozilla::gfx::DrawTarget *aTarget,
                                      mozilla::gfx::Matrix *aPatternTransform = nullptr);
    bool IsOpaque();

    enum GraphicsExtend {
        EXTEND_NONE,
        EXTEND_REPEAT,
        EXTEND_REFLECT,
        EXTEND_PAD,

        // Our own private flag for setting either NONE or PAD,
        // depending on what the platform does for NONE.  This is only
        // relevant for surface patterns; for all other patterns, it
        // behaves identical to PAD.  On MacOS X, this becomes "NONE",
        // because Quartz does the thing that we want at image edges;
        // similarily on the win32 printing surface, since
        // everything's done with GDI there.  On other platforms, it
        // usually becomes PAD.
        EXTEND_PAD_EDGE = 1000
    };

    // none, repeat, reflect
    void SetExtend(GraphicsExtend extend);
    GraphicsExtend Extend() const;

    enum GraphicsPatternType {
        PATTERN_SOLID,
        PATTERN_SURFACE,
        PATTERN_LINEAR,
        PATTERN_RADIAL
    };

    GraphicsPatternType GetType() const;

    int CairoStatus();

    void SetFilter(GraphicsFilter filter);
    GraphicsFilter Filter() const;

    /* returns TRUE if it succeeded */
    bool GetSolidColor(gfxRGBA& aColor);

    already_AddRefed<gfxASurface> GetSurface();

    bool IsAzure() { return !mPattern; }

    mozilla::TemporaryRef<mozilla::gfx::SourceSurface> GetAzureSurface() { return mSourceSurface; }

protected:
    cairo_pattern_t *mPattern;

    /**
     * aPatternTransform is the cairo pattern transform --- from user space at
     * the time the pattern was set, to pattern space.
     * aCurrentTransform is the DrawTarget's CTM --- from user space to device
     * space.
     * aOriginalTransform, if non-null, is the DrawTarget's TM when
     * aPatternTransform was set --- user space to device space. If null, then
     * the DrawTarget's CTM is the same as the TM when aPatternTransfrom was set.
     * This function sets aPatternTransform to the Azure pattern transform ---
     * from pattern space to current DrawTarget user space.
     */
    void AdjustTransformForPattern(mozilla::gfx::Matrix &aPatternTransform,
                                   const mozilla::gfx::Matrix &aCurrentTransform,
                                   const mozilla::gfx::Matrix *aOriginalTransform);

    union {
      mozilla::AlignedStorage2<mozilla::gfx::ColorPattern> mColorPattern;
      mozilla::AlignedStorage2<mozilla::gfx::LinearGradientPattern> mLinearGradientPattern;
      mozilla::AlignedStorage2<mozilla::gfx::RadialGradientPattern> mRadialGradientPattern;
      mozilla::AlignedStorage2<mozilla::gfx::SurfacePattern> mSurfacePattern;
    };

    mozilla::gfx::Pattern *mGfxPattern;

    mozilla::RefPtr<mozilla::gfx::SourceSurface> mSourceSurface;
    mozilla::gfx::Matrix mTransform;
    mozilla::RefPtr<mozilla::gfx::GradientStops> mStops;
    GraphicsExtend mExtend;
    mozilla::gfx::Filter mFilter;
};

#endif /* GFX_PATTERN_H */

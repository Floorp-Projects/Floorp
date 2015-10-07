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
#include "mozilla/gfx/PatternHelpers.h"
#include "nsISupportsImpl.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"

typedef struct _cairo_pattern cairo_pattern_t;


class gfxPattern final{
    NS_INLINE_DECL_REFCOUNTING(gfxPattern)

public:
    explicit gfxPattern(const mozilla::gfx::Color& aColor);
    // linear
    gfxPattern(gfxFloat x0, gfxFloat y0, gfxFloat x1, gfxFloat y1); // linear
    gfxPattern(gfxFloat cx0, gfxFloat cy0, gfxFloat radius0,
               gfxFloat cx1, gfxFloat cy1, gfxFloat radius1); // radial
    gfxPattern(mozilla::gfx::SourceSurface *aSurface,
               const mozilla::gfx::Matrix &aPatternToUserSpace);

    void AddColorStop(gfxFloat offset, const mozilla::gfx::Color& c);
    void SetColorStops(mozilla::gfx::GradientStops* aStops);

    // This should only be called on a cairo pattern that we want to use with
    // Azure. We will read back the color stops from cairo and try to look
    // them up in the cache.
    void CacheColorStops(const mozilla::gfx::DrawTarget *aDT);

    void SetMatrix(const gfxMatrix& matrix);
    gfxMatrix GetMatrix() const;
    gfxMatrix GetInverseMatrix() const;

    /* Get an Azure Pattern for the current Cairo pattern. aPattern transform
     * specifies the transform that was set on the DrawTarget when the pattern
     * was set. When this is nullptr it is assumed the transform is identical
     * to the current transform.
     */
    mozilla::gfx::Pattern *GetPattern(const mozilla::gfx::DrawTarget *aTarget,
                                      mozilla::gfx::Matrix *aOriginalUserToDevice = nullptr);
    bool IsOpaque();

    // clamp, repeat, reflect
    void SetExtend(mozilla::gfx::ExtendMode aExtend);

    int CairoStatus();

    void SetFilter(mozilla::gfx::Filter filter);
    mozilla::gfx::Filter Filter() const;

    /* returns TRUE if it succeeded */
    bool GetSolidColor(mozilla::gfx::Color& aColorOut);

private:
    // Private destructor, to discourage deletion outside of Release():
    ~gfxPattern() {}

    mozilla::gfx::GeneralPattern mGfxPattern;
    mozilla::RefPtr<mozilla::gfx::SourceSurface> mSourceSurface;
    mozilla::gfx::Matrix mPatternToUserSpace;
    mozilla::RefPtr<mozilla::gfx::GradientStops> mStops;
    nsTArray<mozilla::gfx::GradientStop> mStopsList;
    mozilla::gfx::ExtendMode mExtend;
};

#endif /* GFX_PATTERN_H */

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSRENDERINGCONTEXT__H__
#define NSRENDERINGCONTEXT__H__

#include <stdint.h>                     // for uint32_t
#include <sys/types.h>                  // for int32_t
#include "gfxContext.h"                 // for gfxContext
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "mozilla/gfx/2D.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsBoundingMetrics.h"          // for nsBoundingMetrics
#include "nsColor.h"                    // for nscolor
#include "nsCoord.h"                    // for nscoord, NSToIntRound
#include "nsFontMetrics.h"              // for nsFontMetrics
#include "nsISupports.h"                // for NS_INLINE_DECL_REFCOUNTING, etc
#include "nsString.h"               // for nsString
#include "nscore.h"                     // for char16_t

class nsIntRegion;
struct nsPoint;
struct nsRect;

class nsRenderingContext MOZ_FINAL
{
    typedef mozilla::gfx::DrawTarget DrawTarget;

public:
    nsRenderingContext() {}

    NS_INLINE_DECL_REFCOUNTING(nsRenderingContext)

    void Init(gfxContext* aThebesContext);
    void Init(DrawTarget* aDrawTarget);

    // These accessors will never return null.
    gfxContext *ThebesContext() { return mThebes; }
    DrawTarget *GetDrawTarget() { return mThebes->GetDrawTarget(); }

    // Text

    void SetFont(nsFontMetrics *aFontMetrics);
    nsFontMetrics *FontMetrics() { return mFontMetrics; } // may be null

    void SetTextRunRTL(bool aIsRTL);

private:
    // Private destructor, to discourage deletion outside of Release():
    ~nsRenderingContext()
    {
    }

    nsRefPtr<gfxContext> mThebes;
    nsRefPtr<nsFontMetrics> mFontMetrics;
};

#endif  // NSRENDERINGCONTEXT__H__

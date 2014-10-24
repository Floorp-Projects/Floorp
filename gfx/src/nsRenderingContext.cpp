/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRenderingContext.h"

void
nsRenderingContext::Init(gfxContext *aThebesContext)
{
    mThebes = aThebesContext;
    mThebes->SetLineWidth(1.0);
}

void
nsRenderingContext::Init(DrawTarget *aDrawTarget)
{
    Init(new gfxContext(aDrawTarget));
}

void
nsRenderingContext::SetTextRunRTL(bool aIsRTL)
{
    mFontMetrics->SetTextRunRTL(aIsRTL);
}

void
nsRenderingContext::SetFont(nsFontMetrics *aFontMetrics)
{
    mFontMetrics = aFontMetrics;
}

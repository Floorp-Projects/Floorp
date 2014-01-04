/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_DWRITESHAPER_H
#define GFX_DWRITESHAPER_H

#include "gfxDWriteFonts.h"

/**
 * \brief Class representing a DWrite font shaper.
 */
class gfxDWriteShaper : public gfxFontShaper
{
public:
    gfxDWriteShaper(gfxDWriteFont *aFont)
        : gfxFontShaper(aFont)
    {
        MOZ_COUNT_CTOR(gfxDWriteShaper);
    }

    virtual ~gfxDWriteShaper()
    {
        MOZ_COUNT_DTOR(gfxDWriteShaper);
    }

    virtual bool ShapeText(gfxContext      *aContext,
                           const char16_t *aText,
                           uint32_t         aOffset,
                           uint32_t         aLength,
                           int32_t          aScript,
                           gfxShapedText   *aShapedText);
};

#endif /* GFX_DWRITESHAPER_H */

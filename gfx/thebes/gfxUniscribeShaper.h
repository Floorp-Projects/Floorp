/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_UNISCRIBESHAPER_H
#define GFX_UNISCRIBESHAPER_H

#include "prtypes.h"
#include "gfxTypes.h"
#include "gfxGDIFont.h"

#include <usp10.h>
#include <cairo-win32.h>


class gfxUniscribeShaper : public gfxFontShaper
{
public:
    gfxUniscribeShaper(gfxGDIFont *aFont)
        : gfxFontShaper(aFont)
        , mScriptCache(NULL)
    {
        MOZ_COUNT_CTOR(gfxUniscribeShaper);
    }

    virtual ~gfxUniscribeShaper()
    {
        MOZ_COUNT_DTOR(gfxUniscribeShaper);
    }

    virtual bool ShapeText(gfxContext      *aContext,
                           const PRUnichar *aText,
                           uint32_t         aOffset,
                           uint32_t         aLength,
                           int32_t          aScript,
                           gfxShapedText   *aShapedText);

    SCRIPT_CACHE *ScriptCache() { return &mScriptCache; }

    gfxGDIFont *GetFont() { return static_cast<gfxGDIFont*>(mFont); }

private:
    SCRIPT_CACHE mScriptCache;

    enum {
        kUnicodeVS17 = gfxFontUtils::kUnicodeVS17,
        kUnicodeVS256 = gfxFontUtils::kUnicodeVS256
    };
};

#endif /* GFX_UNISCRIBESHAPER_H */

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_GDISHAPER_H
#define GFX_GDISHAPER_H

#include "gfxGDIFont.h"

class gfxGDIShaper : public gfxFontShaper
{
public:
    gfxGDIShaper(gfxGDIFont *aFont)
        : gfxFontShaper(aFont)
    {
        MOZ_COUNT_CTOR(gfxGDIShaper);
    }

    virtual ~gfxGDIShaper()
    {
        MOZ_COUNT_DTOR(gfxGDIShaper);
    }

    virtual bool ShapeWord(gfxContext *aContext,
                           gfxShapedWord *aShapedWord,
                           const PRUnichar *aString);
};

#endif /* GFX_GDISHAPER_H */

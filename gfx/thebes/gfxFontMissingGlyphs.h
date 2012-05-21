/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONTMISSINGGLYPHS_H
#define GFX_FONTMISSINGGLYPHS_H

#include "prtypes.h"
#include "gfxTypes.h"
#include "gfxContext.h"
#include "gfxRect.h"

/**
 * This class should not be instantiated. It's just a container
 * for some helper functions.
 */
class THEBES_API gfxFontMissingGlyphs {
public:
    /**
     * Draw hexboxes for a missing glyph.
     * @param aContext the context to draw to
     * @param aRect the glyph-box for the glyph that is missing
     * @param aChar the UTF16 codepoint for the character
     */
    static void DrawMissingGlyph(gfxContext *aContext, const gfxRect& aRect,
                                 PRUint32 aChar);
    /**
     * @return the desired minimum width for a glyph-box that will allow
     * the hexboxes to be drawn reasonably.
     */
    static gfxFloat GetDesiredMinWidth(PRUint32 aChar);
};

#endif

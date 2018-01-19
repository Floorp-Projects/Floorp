/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONTMISSINGGLYPHS_H
#define GFX_FONTMISSINGGLYPHS_H

#include "mozilla/Attributes.h"
#include "mozilla/gfx/Rect.h"

namespace mozilla {
namespace gfx {
class DrawTarget;
class Pattern;
} // namespace gfx
} // namespace mozilla

/**
 * This class should not be instantiated. It's just a container
 * for some helper functions.
 */
class gfxFontMissingGlyphs final
{
    typedef mozilla::gfx::DrawTarget DrawTarget;
    typedef mozilla::gfx::Float Float;
    typedef mozilla::gfx::Pattern Pattern;
    typedef mozilla::gfx::Rect Rect;

    gfxFontMissingGlyphs() = delete; // prevent instantiation

public:
    /**
     * Draw hexboxes for a missing glyph.
     * @param aChar the UTF16 codepoint for the character
     * @param aRect the glyph-box for the glyph that is missing
     * @param aDrawTarget the DrawTarget to draw to
     * @param aPattern the pattern currently being used to paint
     * @param aAppUnitsPerDevPixel the appUnits to devPixel ratio we're using,
     *                             (so we can scale glyphs to a sensible size)
     * @param aMat optional local-space orientation matrix
     */
    static void DrawMissingGlyph(uint32_t aChar,
                                 const Rect& aRect,
                                 DrawTarget& aDrawTarget,
                                 const Pattern& aPattern,
                                 uint32_t aAppUnitsPerDevPixel,
                                 const Matrix* aMat = nullptr);
    /**
     * @return the desired minimum width for a glyph-box that will allow
     * the hexboxes to be drawn reasonably.
     */
    static Float GetDesiredMinWidth(uint32_t aChar,
                                    uint32_t aAppUnitsPerDevUnit);
};

#endif

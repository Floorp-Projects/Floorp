/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxFontMissingGlyphs.h"
#include "nsDeviceContext.h"
#include "gfxContext.h"
#include "gfxColor.h"

#define CHAR_BITS(b00, b01, b02, b10, b11, b12, b20, b21, b22, b30, b31, b32, b40, b41, b42) \
  ((b00 << 0) | (b01 << 1) | (b02 << 2) | (b10 << 3) | (b11 << 4) | (b12 << 5) | \
   (b20 << 6) | (b21 << 7) | (b22 << 8) | (b30 << 9) | (b31 << 10) | (b32 << 11) | \
   (b40 << 12) | (b41 << 13) | (b42 << 14))

static const uint16_t glyphMicroFont[16] = {
  CHAR_BITS(0, 1, 0,
            1, 0, 1,
            1, 0, 1,
            1, 0, 1,
            0, 1, 0),
  CHAR_BITS(0, 1, 0,
            0, 1, 0,
            0, 1, 0,
            0, 1, 0,
            0, 1, 0),
  CHAR_BITS(1, 1, 1,
            0, 0, 1,
            1, 1, 1,
            1, 0, 0,
            1, 1, 1),
  CHAR_BITS(1, 1, 1,
            0, 0, 1,
            1, 1, 1,
            0, 0, 1,
            1, 1, 1),
  CHAR_BITS(1, 0, 1,
            1, 0, 1,
            1, 1, 1,
            0, 0, 1,
            0, 0, 1),
  CHAR_BITS(1, 1, 1,
            1, 0, 0,
            1, 1, 1,
            0, 0, 1,
            1, 1, 1),
  CHAR_BITS(1, 1, 1,
            1, 0, 0,
            1, 1, 1,
            1, 0, 1,
            1, 1, 1),
  CHAR_BITS(1, 1, 1,
            0, 0, 1,
            0, 0, 1,
            0, 0, 1,
            0, 0, 1),
  CHAR_BITS(0, 1, 0,
            1, 0, 1,
            0, 1, 0,
            1, 0, 1,
            0, 1, 0),
  CHAR_BITS(1, 1, 1,
            1, 0, 1,
            1, 1, 1,
            0, 0, 1,
            0, 0, 1),
  CHAR_BITS(1, 1, 1,
            1, 0, 1,
            1, 1, 1,
            1, 0, 1,
            1, 0, 1),
  CHAR_BITS(1, 1, 0,
            1, 0, 1,
            1, 1, 0,
            1, 0, 1,
            1, 1, 0),
  CHAR_BITS(0, 1, 1,
            1, 0, 0,
            1, 0, 0,
            1, 0, 0,
            0, 1, 1),
  CHAR_BITS(1, 1, 0,
            1, 0, 1,
            1, 0, 1,
            1, 0, 1,
            1, 1, 0),
  CHAR_BITS(1, 1, 1,
            1, 0, 0,
            1, 1, 1,
            1, 0, 0,
            1, 1, 1),
  CHAR_BITS(1, 1, 1,
            1, 0, 0,
            1, 1, 1,
            1, 0, 0,
            1, 0, 0)
};

/* Parameters that control the rendering of hexboxes. They look like this:

        BMP codepoints           non-BMP codepoints
      (U+0000 - U+FFFF)         (U+10000 - U+10FFFF)

         +---------+              +-------------+ 
         |         |              |             |
         | HHH HHH |              | HHH HHH HHH |
         | HHH HHH |              | HHH HHH HHH |
         | HHH HHH |              | HHH HHH HHH |
         | HHH HHH |              | HHH HHH HHH |
         | HHH HHH |              | HHH HHH HHH |
         |         |              |             |
         | HHH HHH |              | HHH HHH HHH |
         | HHH HHH |              | HHH HHH HHH |
         | HHH HHH |              | HHH HHH HHH |
         | HHH HHH |              | HHH HHH HHH |
         | HHH HHH |              | HHH HHH HHH |
         |         |              |             |
         +---------+              +-------------+
*/

/** Width of a minifont glyph (see above) */
static const int MINIFONT_WIDTH = 3;
/** Height of a minifont glyph (see above) */
static const int MINIFONT_HEIGHT = 5;
/**
 * Gap between minifont glyphs (both horizontal and vertical) and also
 * the minimum desired gap between the box border and the glyphs
 */
static const int HEX_CHAR_GAP = 1;
/**
 * The amount of space between the vertical edge of the glyphbox and the
 * box border. We make this nonzero so that when multiple missing glyphs
 * occur consecutively there's a gap between their rendered boxes.
 */
static const int BOX_HORIZONTAL_INSET = 1;
/** The width of the border */
static const int BOX_BORDER_WIDTH = 1;
/**
 * The scaling factor for the border opacity; this is multiplied by the current
 * opacity being used to draw the text.
 */
static const gfxFloat BOX_BORDER_OPACITY = 0.5;
/**
 * Draw a single hex character using the current color. A nice way to do this
 * would be to fill in an A8 image surface and then use it as a mask
 * to paint the current color. Tragically this doesn't currently work with the
 * Quartz cairo backend which doesn't generally support masking with surfaces.
 * So for now we just paint a bunch of rectangles...
 */
#ifndef MOZ_GFX_OPTIMIZE_MOBILE
static void
DrawHexChar(gfxContext *aContext, const gfxPoint& aPt, uint32_t aDigit)
{
    aContext->NewPath();
    uint32_t glyphBits = glyphMicroFont[aDigit];
    int x, y;
    for (y = 0; y < MINIFONT_HEIGHT; ++y) {
        for (x = 0; x < MINIFONT_WIDTH; ++x) {
            if (glyphBits & 1) {
                aContext->Rectangle(gfxRect(x, y, 1, 1) + aPt, true);
            }
            glyphBits >>= 1;
        }
    }
    aContext->Fill();
}
#endif // MOZ_GFX_OPTIMIZE_MOBILE

void
gfxFontMissingGlyphs::DrawMissingGlyph(gfxContext    *aContext,
                                       const gfxRect& aRect,
                                       uint32_t       aChar,
                                       uint32_t       aAppUnitsPerDevPixel)
{
    aContext->Save();

    gfxRGBA currentColor;
    if (!aContext->GetDeviceColor(currentColor)) {
        // We're currently drawing with some kind of pattern... Just draw
        // the missing-glyph data in black.
        currentColor = gfxRGBA(0,0,0,1);
    }

    // Stroke a rectangle so that the stroke's left edge is inset one pixel
    // from the left edge of the glyph box and the stroke's right edge
    // is inset one pixel from the right edge of the glyph box.
    gfxFloat halfBorderWidth = BOX_BORDER_WIDTH / 2.0;
    gfxFloat borderLeft = aRect.X() + BOX_HORIZONTAL_INSET + halfBorderWidth;
    gfxFloat borderRight = aRect.XMost() - BOX_HORIZONTAL_INSET - halfBorderWidth;
    gfxRect borderStrokeRect(borderLeft, aRect.Y() + halfBorderWidth,
                             borderRight - borderLeft,
                             aRect.Height() - 2.0 * halfBorderWidth);
    if (!borderStrokeRect.IsEmpty()) {
        aContext->SetLineWidth(BOX_BORDER_WIDTH);
        aContext->SetDash(gfxContext::gfxLineSolid);
        aContext->SetLineCap(gfxContext::LINE_CAP_SQUARE);
        aContext->SetLineJoin(gfxContext::LINE_JOIN_MITER);
        gfxRGBA color = currentColor;
        color.a *= BOX_BORDER_OPACITY;
        aContext->SetDeviceColor(color);
        aContext->NewPath();
        aContext->Rectangle(borderStrokeRect);

#ifdef MOZ_GFX_OPTIMIZE_MOBILE
        aContext->Fill();
#else
        aContext->Stroke();
#endif
    }

#ifndef MOZ_GFX_OPTIMIZE_MOBILE
    gfxPoint center(aRect.X() + aRect.Width() / 2,
                    aRect.Y() + aRect.Height() / 2);
    gfxFloat halfGap = HEX_CHAR_GAP / 2.0;
    gfxFloat top = -(MINIFONT_HEIGHT + halfGap);
    aContext->SetDeviceColor(currentColor);
    aContext->Translate(center);
    // We always want integer scaling, otherwise the "bitmap" glyphs will look
    // even uglier than usual when zoomed
    int32_t scale =
        std::max<int32_t>(1, nsDeviceContext::AppUnitsPerCSSPixel() /
                             aAppUnitsPerDevPixel);
    aContext->Scale(gfxFloat(scale), gfxFloat(scale));
    if (aChar < 0x10000) {
        if (aRect.Width() >= 2 * (MINIFONT_WIDTH + HEX_CHAR_GAP) &&
            aRect.Height() >= 2 * MINIFONT_HEIGHT + HEX_CHAR_GAP) {
            // Draw 4 digits for BMP
            gfxFloat left = -(MINIFONT_WIDTH + halfGap);
            DrawHexChar(aContext,
                        gfxPoint(left, top), (aChar >> 12) & 0xF);
            DrawHexChar(aContext,
                        gfxPoint(halfGap, top), (aChar >> 8) & 0xF);
            DrawHexChar(aContext,
                        gfxPoint(left, halfGap), (aChar >> 4) & 0xF);
            DrawHexChar(aContext,
                        gfxPoint(halfGap, halfGap), aChar & 0xF);
        }
    } else {
        if (aRect.Width() >= 3 * (MINIFONT_WIDTH + HEX_CHAR_GAP) &&
            aRect.Height() >= 2 * MINIFONT_HEIGHT + HEX_CHAR_GAP) {
            // Draw 6 digits for non-BMP
            gfxFloat first = -(MINIFONT_WIDTH * 1.5 + HEX_CHAR_GAP);
            gfxFloat second = -(MINIFONT_WIDTH / 2.0);
            gfxFloat third = (MINIFONT_WIDTH / 2.0 + HEX_CHAR_GAP);
            DrawHexChar(aContext,
                        gfxPoint(first, top), (aChar >> 20) & 0xF);
            DrawHexChar(aContext,
                        gfxPoint(second, top), (aChar >> 16) & 0xF);
            DrawHexChar(aContext,
                        gfxPoint(third, top), (aChar >> 12) & 0xF);
            DrawHexChar(aContext,
                        gfxPoint(first, halfGap), (aChar >> 8) & 0xF);
            DrawHexChar(aContext,
                        gfxPoint(second, halfGap), (aChar >> 4) & 0xF);
            DrawHexChar(aContext,
                        gfxPoint(third, halfGap), aChar & 0xF);
        }
    }
#endif

    aContext->Restore();
}

gfxFloat
gfxFontMissingGlyphs::GetDesiredMinWidth(uint32_t aChar,
                                         uint32_t aAppUnitsPerDevPixel)
{
/**
 * The minimum desired width for a missing-glyph glyph box. I've laid it out
 * like this so you can see what goes where.
 */
    gfxFloat width = BOX_HORIZONTAL_INSET + BOX_BORDER_WIDTH + HEX_CHAR_GAP +
        MINIFONT_WIDTH + HEX_CHAR_GAP + MINIFONT_WIDTH +
         ((aChar < 0x10000) ? 0 : HEX_CHAR_GAP + MINIFONT_WIDTH) +
        HEX_CHAR_GAP + BOX_BORDER_WIDTH + BOX_HORIZONTAL_INSET;
    // Note that this will give us floating-point division, so the width will
    // -not- be snapped to integer multiples of its basic pixel value
    width *= gfxFloat(nsDeviceContext::AppUnitsPerCSSPixel()) / aAppUnitsPerDevPixel;
    return width;
}

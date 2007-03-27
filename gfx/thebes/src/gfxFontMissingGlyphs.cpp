/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <robert@ocallahan.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "gfxFontMissingGlyphs.h"

#define CHAR_BITS(b00, b01, b02, b10, b11, b12, b20, b21, b22, b30, b31, b32, b40, b41, b42) \
  ((b00 << 0) | (b01 << 1) | (b02 << 2) | (b10 << 3) | (b11 << 4) | (b12 << 5) | \
   (b20 << 6) | (b21 << 7) | (b22 << 8) | (b30 << 9) | (b31 << 10) | (b32 << 11) | \
   (b40 << 12) | (b41 << 13) | (b42 << 14))

static const PRUint16 glyphMicroFont[16] = {
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
  CHAR_BITS(1, 1, 1,
            1, 0, 1,
            1, 1, 1,
            1, 0, 1,
            1, 1, 1),
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
   
       +---------+
       |         |
       | HHH HHH |
       | HHH HHH |
       | HHH HHH |
       | HHH HHH |
       | HHH HHH |
       |         |
       | HHH HHH |
       | HHH HHH |
       | HHH HHH |
       | HHH HHH |
       | HHH HHH |
       |         |
       +---------+
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
 * The minimum desired width for a missing-glyph glyph box. I've laid it out
 * like this so you can see what goes where.
 */
static const int MIN_DESIRED_WIDTH =
  BOX_HORIZONTAL_INSET + BOX_BORDER_WIDTH + HEX_CHAR_GAP +
  MINIFONT_WIDTH + HEX_CHAR_GAP + MINIFONT_WIDTH +
  HEX_CHAR_GAP + BOX_BORDER_WIDTH + BOX_HORIZONTAL_INSET;

/**
 * Draw a single hex character using the current color. A nice way to do this
 * would be to fill in an A8 image surface and then use it as a mask
 * to paint the current color. Tragically this doesn't currently work with the
 * Quartz cairo backend which doesn't generally support masking with surfaces.
 * So for now we just paint a bunch of rectangles...
 */
static void
DrawHexChar(gfxContext *aContext, const gfxPoint& aPt, PRUint32 aDigit)
{
    aContext->NewPath();
    PRUint32 glyphBits = glyphMicroFont[aDigit];
    int x, y;
    for (y = 0; y < MINIFONT_HEIGHT; ++y) {
        for (x = 0; x < MINIFONT_WIDTH; ++x) {
            if (glyphBits & 1) {
                aContext->Rectangle(gfxRect(x, y, 1, 1) + aPt, PR_TRUE);
            }
            glyphBits >>= 1;
        }
    }
    aContext->Fill();
}

void
gfxFontMissingGlyphs::DrawMissingGlyph(gfxContext *aContext, const gfxRect& aRect,
                                       PRUnichar aChar)
{
    aContext->Save();

    gfxRGBA currentColor;
    if (!aContext->GetColor(currentColor)) {
        // We're currently drawing with some kind of pattern... Just draw
        // the missing-glyph data in black.
        currentColor = gfxRGBA(0,0,0,1);
    }

    // Stroke a rectangle so that the stroke's left edge is inset one pixel
    // from the left edge of the glyph box and the stroke's right edge
    // is inset one pixel from the right edge of the glyph box.
    gfxFloat halfBorderWidth = BOX_BORDER_WIDTH/2.0;
    gfxFloat borderLeft = aRect.X() + BOX_HORIZONTAL_INSET + halfBorderWidth;
    gfxFloat borderRight = aRect.XMost() - BOX_HORIZONTAL_INSET - halfBorderWidth;
    gfxRect borderStrokeRect(borderLeft, aRect.Y() + halfBorderWidth,
                             borderRight - borderLeft, aRect.Height() - 2*halfBorderWidth);
    if (!borderStrokeRect.IsEmpty()) {
        aContext->SetLineWidth(BOX_BORDER_WIDTH);
        aContext->SetDash(gfxContext::gfxLineSolid);
        aContext->SetLineCap(gfxContext::LINE_CAP_SQUARE);
        aContext->SetLineJoin(gfxContext::LINE_JOIN_MITER);
        gfxRGBA color = currentColor;
        color.a *= BOX_BORDER_OPACITY;
        aContext->SetColor(color);
        aContext->NewPath();
        aContext->Rectangle(borderStrokeRect);
        aContext->Stroke();
    }

    if (aRect.Width() >= 2*MINIFONT_WIDTH + HEX_CHAR_GAP &&
        aRect.Height() >= 2*MINIFONT_HEIGHT + HEX_CHAR_GAP) {
        aContext->SetColor(currentColor);
        gfxPoint center(aRect.X() + aRect.Width()/2,
                        aRect.Y() + aRect.Height()/2);
        gfxFloat halfGap = HEX_CHAR_GAP/2.0;
        gfxFloat left = -(MINIFONT_WIDTH + halfGap);
        gfxFloat top = -(MINIFONT_HEIGHT + halfGap);
        DrawHexChar(aContext,
                    center + gfxPoint(left, top), (aChar >> 12) & 0xF);
        DrawHexChar(aContext,
                    center + gfxPoint(halfGap, top), (aChar >> 8) & 0xF);
        DrawHexChar(aContext,
                    center + gfxPoint(left, halfGap), (aChar >> 4) & 0xF);
        DrawHexChar(aContext,
                    center + gfxPoint(halfGap, halfGap), aChar & 0xF);
    }

    aContext->Restore();
}

gfxFloat
gfxFontMissingGlyphs::GetDesiredMinWidth()
{
    return MIN_DESIRED_WIDTH;
}

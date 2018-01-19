/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxFontMissingGlyphs.h"

#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/RefPtr.h"
#include "nsDeviceContext.h"
#include "nsLayoutUtils.h"

using namespace mozilla;
using namespace mozilla::gfx;

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
static const Float BOX_BORDER_OPACITY = 0.5;
/**
 * Draw a single hex character using the current color. A nice way to do this
 * would be to fill in an A8 image surface and then use it as a mask
 * to paint the current color. Tragically this doesn't currently work with the
 * Quartz cairo backend which doesn't generally support masking with surfaces.
 * So for now we just paint a bunch of rectangles...
 */
#ifndef MOZ_GFX_OPTIMIZE_MOBILE
static void
DrawHexChar(uint32_t aDigit, const Point& aPt, DrawTarget& aDrawTarget,
            const Pattern &aPattern, const Matrix* aMat)
{
    uint32_t glyphBits = glyphMicroFont[aDigit];

    if (aMat) {
        // If using an orientation matrix instead of a DT transform, step
        // with the matrix basis vectors, filling individual rectangles of
        // the size indicated by the matrix.
        Point stepX(aMat->_11, aMat->_12);
        Point stepY(aMat->_21, aMat->_22);
        Point corner = stepX + stepY;
        // Get the rectangle at the origin that will be stepped into place.
        Rect startRect(std::min(corner.x, 0.0f), std::min(corner.y, 0.0f),
                       fabs(corner.x), fabs(corner.y));
        startRect.MoveBy(aMat->TransformPoint(aPt));
        for (int y = 0; y < MINIFONT_HEIGHT; ++y) {
            Rect curRect = startRect;
            for (int x = 0; x < MINIFONT_WIDTH; ++x) {
                if (glyphBits & 1) {
                    aDrawTarget.FillRect(curRect, aPattern);
                }
                glyphBits >>= 1;
                curRect.MoveBy(stepX);
            }
            startRect.MoveBy(stepY);
        }
        return;
    }

    // To avoid the potential for seams showing between rects when we're under
    // a transform we concat all the rects into a PathBuilder and fill the
    // resulting Path (rather than using DrawTarget::FillRect).
    RefPtr<PathBuilder> builder = aDrawTarget.CreatePathBuilder();
    for (int y = 0; y < MINIFONT_HEIGHT; ++y) {
        for (int x = 0; x < MINIFONT_WIDTH; ++x) {
            if (glyphBits & 1) {
                Rect r(aPt.x + x, aPt.y + y, 1, 1);
                MaybeSnapToDevicePixels(r, aDrawTarget, true);
                builder->MoveTo(r.TopLeft());
                builder->LineTo(r.TopRight());
                builder->LineTo(r.BottomRight());
                builder->LineTo(r.BottomLeft());
                builder->Close();
            }
            glyphBits >>= 1;
        }
    }
    RefPtr<Path> path = builder->Finish();
    aDrawTarget.Fill(path, aPattern);
}
#endif // MOZ_GFX_OPTIMIZE_MOBILE

void
gfxFontMissingGlyphs::DrawMissingGlyph(uint32_t aChar,
                                       const Rect& aRect,
                                       DrawTarget& aDrawTarget,
                                       const Pattern& aPattern,
                                       uint32_t aAppUnitsPerDevPixel,
                                       const Matrix* aMat)
{
    Rect rect(aRect);
    // If there is an orientation transform, reorient the bounding rect.
    if (aMat) {
        rect.MoveBy(-aRect.BottomLeft());
        rect = aMat->TransformBounds(rect);
        rect.MoveBy(aRect.BottomLeft());
    }

    // If we're currently drawing with some kind of pattern, we just draw the
    // missing-glyph data in black.
    ColorPattern color = aPattern.GetType() == PatternType::COLOR ?
        static_cast<const ColorPattern&>(aPattern) :
        ColorPattern(ToDeviceColor(Color(0.f, 0.f, 0.f, 1.f)));

    // Stroke a rectangle so that the stroke's left edge is inset one pixel
    // from the left edge of the glyph box and the stroke's right edge
    // is inset one pixel from the right edge of the glyph box.
    Float halfBorderWidth = BOX_BORDER_WIDTH / 2.0;
    Float borderLeft = rect.X() + BOX_HORIZONTAL_INSET + halfBorderWidth;
    Float borderRight = rect.XMost() - BOX_HORIZONTAL_INSET - halfBorderWidth;
    Rect borderStrokeRect(borderLeft, rect.Y() + halfBorderWidth,
                          borderRight - borderLeft,
                          rect.Height() - 2.0 * halfBorderWidth);
    if (!borderStrokeRect.IsEmpty()) {
        ColorPattern adjustedColor = color;
        color.mColor.a *= BOX_BORDER_OPACITY;
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
        aDrawTarget.FillRect(borderStrokeRect, adjustedColor);
#else
        StrokeOptions strokeOptions(BOX_BORDER_WIDTH);
        aDrawTarget.StrokeRect(borderStrokeRect, adjustedColor, strokeOptions);
#endif
    }

#ifndef MOZ_GFX_OPTIMIZE_MOBILE
    Point center = rect.Center();
    Float halfGap = HEX_CHAR_GAP / 2.f;
    Float top = -(MINIFONT_HEIGHT + halfGap);
    // We always want integer scaling, otherwise the "bitmap" glyphs will look
    // even uglier than usual when zoomed
    int32_t devPixelsPerCSSPx =
        std::max<int32_t>(1, nsDeviceContext::AppUnitsPerCSSPixel() /
                             aAppUnitsPerDevPixel);

    Matrix tempMat;
    if (aMat) {
        // If there is an orientation transform, since draw target transforms may
        // not be supported, scale and translate it so that it can be directly used
        // for rendering the mini font without changing the draw target transform.
        tempMat = Matrix(*aMat).PostScale(devPixelsPerCSSPx, devPixelsPerCSSPx)
                               .PostTranslate(center);
        aMat = &tempMat;
    } else {
        // Otherwise, scale and translate the draw target transform assuming it
        // supports that.
        tempMat = aDrawTarget.GetTransform();
        aDrawTarget.SetTransform(
            Matrix(tempMat).PreTranslate(center)
                           .PreScale(devPixelsPerCSSPx, devPixelsPerCSSPx));
    }

    if (aChar < 0x10000) {
        if (rect.Width() >= 2 * (MINIFONT_WIDTH + HEX_CHAR_GAP) &&
            rect.Height() >= 2 * MINIFONT_HEIGHT + HEX_CHAR_GAP) {
            // Draw 4 digits for BMP
            Float left = -(MINIFONT_WIDTH + halfGap);
            DrawHexChar((aChar >> 12) & 0xF,
                        Point(left, top), aDrawTarget, color, aMat);
            DrawHexChar((aChar >> 8) & 0xF,
                        Point(halfGap, top), aDrawTarget, color, aMat);
            DrawHexChar((aChar >> 4) & 0xF,
                        Point(left, halfGap), aDrawTarget, color, aMat);
            DrawHexChar(aChar & 0xF,
                        Point(halfGap, halfGap), aDrawTarget, color, aMat);
        }
    } else {
        if (rect.Width() >= 3 * (MINIFONT_WIDTH + HEX_CHAR_GAP) &&
            rect.Height() >= 2 * MINIFONT_HEIGHT + HEX_CHAR_GAP) {
            // Draw 6 digits for non-BMP
            Float first = -(MINIFONT_WIDTH * 1.5 + HEX_CHAR_GAP);
            Float second = -(MINIFONT_WIDTH / 2.0);
            Float third = (MINIFONT_WIDTH / 2.0 + HEX_CHAR_GAP);
            DrawHexChar((aChar >> 20) & 0xF,
                        Point(first, top), aDrawTarget, color, aMat);
            DrawHexChar((aChar >> 16) & 0xF,
                        Point(second, top), aDrawTarget, color, aMat);
            DrawHexChar((aChar >> 12) & 0xF,
                        Point(third, top), aDrawTarget, color, aMat);
            DrawHexChar((aChar >> 8) & 0xF,
                        Point(first, halfGap), aDrawTarget, color, aMat);
            DrawHexChar((aChar >> 4) & 0xF,
                        Point(second, halfGap), aDrawTarget, color, aMat);
            DrawHexChar(aChar & 0xF,
                        Point(third, halfGap), aDrawTarget, color, aMat);
        }
    }

    if (!aMat) {
        // The draw target transform was changed, so it must be restored to
        // the original value.
        aDrawTarget.SetTransform(tempMat);
    }
#endif
}

Float
gfxFontMissingGlyphs::GetDesiredMinWidth(uint32_t aChar,
                                         uint32_t aAppUnitsPerDevPixel)
{
/**
 * The minimum desired width for a missing-glyph glyph box. I've laid it out
 * like this so you can see what goes where.
 */
    Float width = BOX_HORIZONTAL_INSET + BOX_BORDER_WIDTH + HEX_CHAR_GAP +
        MINIFONT_WIDTH + HEX_CHAR_GAP + MINIFONT_WIDTH +
         ((aChar < 0x10000) ? 0 : HEX_CHAR_GAP + MINIFONT_WIDTH) +
        HEX_CHAR_GAP + BOX_BORDER_WIDTH + BOX_HORIZONTAL_INSET;
    // Note that this will give us floating-point division, so the width will
    // -not- be snapped to integer multiples of its basic pixel value
    width *= Float(nsDeviceContext::AppUnitsPerCSSPixel()) / aAppUnitsPerDevPixel;
    return width;
}

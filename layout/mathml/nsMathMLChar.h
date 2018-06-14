/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLChar_h___
#define nsMathMLChar_h___

#include "nsColor.h"
#include "nsMathMLOperators.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsString.h"
#include "nsBoundingMetrics.h"
#include "gfxTextRun.h"

class gfxContext;
class nsGlyphTable;
class nsIFrame;
class nsDisplayListBuilder;
class nsDisplayListSet;
class nsPresContext;
struct nsBoundingMetrics;
struct nsFont;

namespace mozilla {
class ComputedStyle;
}

// Hints for Stretch() to indicate criteria for stretching
enum {
  // Don't stretch
  NS_STRETCH_NONE     = 0x00,
  // Variable size stretches
  NS_STRETCH_VARIABLE_MASK = 0x0F,
  NS_STRETCH_NORMAL   = 0x01, // try to stretch to requested size
  NS_STRETCH_NEARER   = 0x02, // stretch very close to requested size
  NS_STRETCH_SMALLER  = 0x04, // don't stretch more than requested size
  NS_STRETCH_LARGER   = 0x08, // don't stretch less than requested size
  // A largeop in displaystyle
  NS_STRETCH_LARGEOP  = 0x10,
  NS_STRETCH_INTEGRAL  = 0x20,

  // Intended for internal use:
  // Find the widest metrics that might be returned from a vertical stretch
  NS_STRETCH_MAXWIDTH = 0x40
};

// A single glyph in our internal representation is either
// 1) a 'code@font' pair from the mathfontFONTFAMILY.properties table. The
// 'code' is interpreted as a Unicode point. The 'font' is a numeric
// identifier given to the font to which the glyph belongs, which is 0 for the
// FONTFAMILY and > 0 for 'external' fonts.
// 2) a glyph index from the Open Type MATH table. In that case, all the glyphs
// come from the font containing that table and 'font' is just set to -1.
struct nsGlyphCode {
  union {
    char16_t code[2];
    uint32_t glyphID;
  };
  int8_t   font;

  bool IsGlyphID() const { return font == -1; }

  int32_t Length() const {
    return (IsGlyphID() || code[1] == char16_t('\0') ? 1 : 2);
  }
  bool Exists() const
  {
    return IsGlyphID() ? glyphID != 0 : code[0] != 0;
  }
  bool operator==(const nsGlyphCode& other) const
  {
    return (other.font == font &&
            ((IsGlyphID() && other.glyphID == glyphID) ||
             (!IsGlyphID() && other.code[0] == code[0] &&
              other.code[1] == code[1])));
  }
  bool operator!=(const nsGlyphCode& other) const
  {
    return ! operator==(other);
  }
};

// Class used to handle stretchy symbols (accent, delimiter and boundary
// symbols).
class nsMathMLChar
{
public:
  typedef gfxTextRun::Range Range;
  typedef mozilla::gfx::DrawTarget DrawTarget;

  // constructor and destructor
  nsMathMLChar()
    : mDirection(NS_STRETCH_DIRECTION_DEFAULT)
  {
    MOZ_COUNT_CTOR(nsMathMLChar);
    mComputedStyle = nullptr;
    mUnscaledAscent = 0;
    mScaleX = mScaleY = 1.0;
    mDraw = DRAW_NORMAL;
    mMirrored = false;
  }

  // not a virtual destructor: this class is not intended to be subclassed
  ~nsMathMLChar();

  void Display(nsDisplayListBuilder*   aBuilder,
               nsIFrame*               aForFrame,
               const nsDisplayListSet& aLists,
               uint32_t                aIndex,
               const nsRect*           aSelectedRect = nullptr);

  void PaintForeground(nsIFrame* aForFrame,
                       gfxContext& aRenderingContext,
                       nsPoint aPt,
                       bool aIsSelected);

  // This is the method called to ask the char to stretch itself.
  // @param aContainerSize - IN - suggested size for the stretched char
  // @param aDesiredStretchSize - OUT - the size that the char wants
  nsresult
  Stretch(nsIFrame*                aForFrame,
          DrawTarget*              aDrawTarget,
          float                    aFontSizeInflation,
          nsStretchDirection       aStretchDirection,
          const nsBoundingMetrics& aContainerSize,
          nsBoundingMetrics&       aDesiredStretchSize,
          uint32_t                 aStretchHint,
          bool                     aRTL);

  void
  SetData(nsString& aData);

  void
  GetData(nsString& aData) {
    aData = mData;
  }

  int32_t
  Length() {
    return mData.Length();
  }

  nsStretchDirection
  GetStretchDirection() {
    return mDirection;
  }

  // Sometimes we only want to pass the data to another routine,
  // this function helps to avoid copying
  const char16_t*
  get() {
    return mData.get();
  }

  void
  GetRect(nsRect& aRect) {
    aRect = mRect;
  }

  void
  SetRect(const nsRect& aRect) {
    mRect = aRect;
  }

  // Get the maximum width that the character might have after a vertical
  // Stretch().
  //
  // @param aStretchHint can be the value that will be passed to Stretch().
  // It is used to determine whether the operator is stretchy or a largeop.
  nscoord
  GetMaxWidth(nsIFrame* aForFrame,
              DrawTarget* aDrawTarget,
              float aFontSizeInflation,
              uint32_t aStretchHint = NS_STRETCH_NORMAL);

  // Metrics that _exactly_ enclose the char. The char *must* have *already*
  // being stretched before you can call the GetBoundingMetrics() method.
  // IMPORTANT: since chars have their own ComputedStyles, and may be rendered
  // with glyphs that are not in the parent font, just calling the default
  // aRenderingContext.GetBoundingMetrics(aChar) can give incorrect results.
  void
  GetBoundingMetrics(nsBoundingMetrics& aBoundingMetrics) {
    aBoundingMetrics = mBoundingMetrics;
  }

  void
  SetBoundingMetrics(nsBoundingMetrics& aBoundingMetrics) {
    mBoundingMetrics = aBoundingMetrics;
  }

  // Hooks to access the extra leaf ComputedStyles given to the MathMLChars.
  // They provide an interface to make them accessible to the Style System via
  // the Get/Set AdditionalComputedStyle() APIs. Owners of MathMLChars
  // should honor these APIs.
  mozilla::ComputedStyle* GetComputedStyle() const;

  void SetComputedStyle(mozilla::ComputedStyle* aComputedStyle);

protected:
  friend class nsGlyphTable;
  friend class nsPropertiesTable;
  friend class nsOpenTypeTable;
  nsString           mData;

private:
  nsRect             mRect;
  nsStretchDirection mDirection;
  nsBoundingMetrics  mBoundingMetrics;
  RefPtr<mozilla::ComputedStyle> mComputedStyle;
  // mGlyphs/mBmData are arrays describing the glyphs used to draw the operator.
  // See the drawing methods below.
  RefPtr<gfxTextRun> mGlyphs[4];
  nsBoundingMetrics     mBmData[4];
  // mUnscaledAscent is the actual ascent of the char.
  nscoord            mUnscaledAscent;
  // mScaleX, mScaleY are the factors by which we scale the char.
  float              mScaleX, mScaleY;

  // mDraw indicates how we draw the stretchy operator:
  // - DRAW_NORMAL: we render the mData string normally.
  // - DRAW_VARIANT: we draw a larger size variant given by mGlyphs[0].
  // - DRAW_PARTS: we assemble several parts given by mGlyphs[0], ... mGlyphs[4]
  // XXXfredw: the MATH table can have any numbers of parts and extenders.
  enum DrawingMethod {
    DRAW_NORMAL, DRAW_VARIANT, DRAW_PARTS
  };
  DrawingMethod mDraw;

  // mMirrored indicates whether the character is mirrored.
  bool               mMirrored;

  class StretchEnumContext;
  friend class StretchEnumContext;

  // helper methods
  bool
  SetFontFamily(nsPresContext*          aPresContext,
                const nsGlyphTable*     aGlyphTable,
                const nsGlyphCode&      aGlyphCode,
                const mozilla::FontFamilyList& aDefaultFamily,
                nsFont&                 aFont,
                RefPtr<gfxFontGroup>* aFontGroup);

  nsresult
  StretchInternal(nsIFrame*                aForFrame,
                  DrawTarget*              aDrawTarget,
                  float                    aFontSizeInflation,
                  nsStretchDirection&      aStretchDirection,
                  const nsBoundingMetrics& aContainerSize,
                  nsBoundingMetrics&       aDesiredStretchSize,
                  uint32_t                 aStretchHint,
                  float           aMaxSize = NS_MATHML_OPERATOR_SIZE_INFINITY,
                  bool            aMaxSizeIsAbsolute = false);

  nsresult
  PaintVertically(nsPresContext* aPresContext,
                  gfxContext*    aThebesContext,
                  nsRect&        aRect,
                  nscolor        aColor);

  nsresult
  PaintHorizontally(nsPresContext* aPresContext,
                    gfxContext*    aThebesContext,
                    nsRect&        aRect,
                    nscolor        aColor);

  void
  ApplyTransforms(gfxContext* aThebesContext, int32_t aAppUnitsPerGfxUnit,
                  nsRect &r);
};

#endif /* nsMathMLChar_h___ */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMathMLmencloseFrame.h"

#include "gfx2DGlue.h"
#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/PathHelpers.h"
#include "nsPresContext.h"
#include "nsWhitespaceTokenizer.h"

#include "nsDisplayList.h"
#include "gfxContext.h"
#include "nsMathMLChar.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::gfx;

//
// <menclose> -- enclose content with a stretching symbol such
// as a long division sign. - implementation

// longdiv:
// Unicode 5.1 assigns U+27CC to LONG DIVISION, but a right parenthesis
// renders better with current font support.
static const char16_t kLongDivChar = ')';

// radical: 'SQUARE ROOT'
static const char16_t kRadicalChar = 0x221A;

// updiagonalstrike
static const uint8_t kArrowHeadSize = 10;

// phasorangle
static const uint8_t kPhasorangleWidth = 8;

nsIFrame*
NS_NewMathMLmencloseFrame(nsIPresShell* aPresShell, ComputedStyle* aStyle)
{
  return new (aPresShell) nsMathMLmencloseFrame(aStyle);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmencloseFrame)

nsMathMLmencloseFrame::nsMathMLmencloseFrame(ComputedStyle* aStyle, ClassID aID) :
  nsMathMLContainerFrame(aStyle, aID),
  mRuleThickness(0), mRadicalRuleThickness(0),
  mLongDivCharIndex(-1), mRadicalCharIndex(-1), mContentWidth(0)
{
}

nsMathMLmencloseFrame::~nsMathMLmencloseFrame()
{
}

nsresult nsMathMLmencloseFrame::AllocateMathMLChar(nsMencloseNotation mask)
{
  // Is the char already allocated?
  if ((mask == NOTATION_LONGDIV && mLongDivCharIndex >= 0) ||
      (mask == NOTATION_RADICAL && mRadicalCharIndex >= 0))
    return NS_OK;

  // No need to track the ComputedStyle given to our MathML chars.
  // The Style System will use Get/SetAdditionalComputedStyle() to keep it
  // up-to-date if dynamic changes arise.
  uint32_t i = mMathMLChar.Length();
  nsAutoString Char;

  if (!mMathMLChar.AppendElement())
    return NS_ERROR_OUT_OF_MEMORY;

  if (mask == NOTATION_LONGDIV) {
    Char.Assign(kLongDivChar);
    mLongDivCharIndex = i;
  } else if (mask == NOTATION_RADICAL) {
    Char.Assign(kRadicalChar);
    mRadicalCharIndex = i;
  }

  nsPresContext *presContext = PresContext();
  mMathMLChar[i].SetData(Char);
  ResolveMathMLCharStyle(presContext, mContent, mComputedStyle, &mMathMLChar[i]);

  return NS_OK;
}

/*
 * Add a notation to draw, if the argument is the name of a known notation.
 * @param aNotation string name of a notation
 */
nsresult nsMathMLmencloseFrame::AddNotation(const nsAString& aNotation)
{
  nsresult rv;

  if (aNotation.EqualsLiteral("longdiv")) {
    rv = AllocateMathMLChar(NOTATION_LONGDIV);
    NS_ENSURE_SUCCESS(rv, rv);
    mNotationsToDraw += NOTATION_LONGDIV;
  } else if (aNotation.EqualsLiteral("actuarial")) {
    mNotationsToDraw += NOTATION_RIGHT;
    mNotationsToDraw += NOTATION_TOP;
  } else if (aNotation.EqualsLiteral("radical")) {
    rv = AllocateMathMLChar(NOTATION_RADICAL);
    NS_ENSURE_SUCCESS(rv, rv);
    mNotationsToDraw += NOTATION_RADICAL;
  } else if (aNotation.EqualsLiteral("box")) {
    mNotationsToDraw += NOTATION_LEFT;
    mNotationsToDraw += NOTATION_RIGHT;
    mNotationsToDraw += NOTATION_TOP;
    mNotationsToDraw += NOTATION_BOTTOM;
  } else if (aNotation.EqualsLiteral("roundedbox")) {
    mNotationsToDraw += NOTATION_ROUNDEDBOX;
  } else if (aNotation.EqualsLiteral("circle")) {
    mNotationsToDraw += NOTATION_CIRCLE;
  } else if (aNotation.EqualsLiteral("left")) {
    mNotationsToDraw += NOTATION_LEFT;
  } else if (aNotation.EqualsLiteral("right")) {
    mNotationsToDraw += NOTATION_RIGHT;
  } else if (aNotation.EqualsLiteral("top")) {
    mNotationsToDraw += NOTATION_TOP;
  } else if (aNotation.EqualsLiteral("bottom")) {
    mNotationsToDraw += NOTATION_BOTTOM;
  } else if (aNotation.EqualsLiteral("updiagonalstrike")) {
    mNotationsToDraw += NOTATION_UPDIAGONALSTRIKE;
  } else if (aNotation.EqualsLiteral("updiagonalarrow")) {
    mNotationsToDraw += NOTATION_UPDIAGONALARROW;
  } else if (aNotation.EqualsLiteral("downdiagonalstrike")) {
    mNotationsToDraw += NOTATION_DOWNDIAGONALSTRIKE;
  } else if (aNotation.EqualsLiteral("verticalstrike")) {
    mNotationsToDraw += NOTATION_VERTICALSTRIKE;
  } else if (aNotation.EqualsLiteral("horizontalstrike")) {
    mNotationsToDraw += NOTATION_HORIZONTALSTRIKE;
  } else if (aNotation.EqualsLiteral("madruwb")) {
    mNotationsToDraw += NOTATION_RIGHT;
    mNotationsToDraw += NOTATION_BOTTOM;
  } else if (aNotation.EqualsLiteral("phasorangle")) {
    mNotationsToDraw += NOTATION_BOTTOM;
    mNotationsToDraw += NOTATION_PHASORANGLE;
  }

  return NS_OK;
}

/*
 * Initialize the list of notations to draw
 */
void nsMathMLmencloseFrame::InitNotations()
{
  MarkNeedsDisplayItemRebuild();
  mNotationsToDraw.clear();
  mLongDivCharIndex = mRadicalCharIndex = -1;
  mMathMLChar.Clear();

  nsAutoString value;

  if (mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::notation_, value)) {
    // parse the notation attribute
    nsWhitespaceTokenizer tokenizer(value);

    while (tokenizer.hasMoreTokens())
      AddNotation(tokenizer.nextToken());

    if (IsToDraw(NOTATION_UPDIAGONALARROW)) {
      // For <menclose notation="updiagonalstrike updiagonalarrow">, if
      // the two notations are drawn then the strike line may cause the point of
      // the arrow to be too wide. Hence we will only draw the updiagonalarrow
      // and the arrow shaft may be thought to be the updiagonalstrike.
      mNotationsToDraw -= NOTATION_UPDIAGONALSTRIKE;
    }
  } else {
    // default: longdiv
    if (NS_FAILED(AllocateMathMLChar(NOTATION_LONGDIV)))
      return;
    mNotationsToDraw += NOTATION_LONGDIV;
  }
}

NS_IMETHODIMP
nsMathMLmencloseFrame::InheritAutomaticData(nsIFrame* aParent)
{
  // let the base class get the default from our parent
  nsMathMLContainerFrame::InheritAutomaticData(aParent);

  mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY;

  InitNotations();

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmencloseFrame::TransmitAutomaticData()
{
  if (IsToDraw(NOTATION_RADICAL)) {
    // The TeXBook (Ch 17. p.141) says that \sqrt is cramped
    UpdatePresentationDataFromChildAt(0, -1,
                                      NS_MATHML_COMPRESSED,
                                      NS_MATHML_COMPRESSED);
  }

  return NS_OK;
}

void
nsMathMLmencloseFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                        const nsDisplayListSet& aLists)
{
  /////////////
  // paint the menclosed content
  nsMathMLContainerFrame::BuildDisplayList(aBuilder, aLists);

  if (NS_MATHML_HAS_ERROR(mPresentationData.flags))
    return;

  nsRect mencloseRect = nsIFrame::GetRect();
  mencloseRect.x = mencloseRect.y = 0;

  if (IsToDraw(NOTATION_RADICAL)) {
    mMathMLChar[mRadicalCharIndex].Display(aBuilder, this, aLists, 0);

    nsRect rect;
    mMathMLChar[mRadicalCharIndex].GetRect(rect);
    rect.MoveBy(StyleVisibility()->mDirection ? -mContentWidth : rect.width, 0);
    rect.SizeTo(mContentWidth, mRadicalRuleThickness);
    DisplayBar(aBuilder, this, rect, aLists, NOTATION_RADICAL);
  }

  if (IsToDraw(NOTATION_PHASORANGLE)) {
    DisplayNotation(aBuilder, this, mencloseRect, aLists,
                mRuleThickness, NOTATION_PHASORANGLE);
  }

  if (IsToDraw(NOTATION_LONGDIV)) {
    mMathMLChar[mLongDivCharIndex].Display(aBuilder, this, aLists, 1);

    nsRect rect;
    mMathMLChar[mLongDivCharIndex].GetRect(rect);
    rect.SizeTo(rect.width + mContentWidth, mRuleThickness);
    DisplayBar(aBuilder, this, rect, aLists, NOTATION_LONGDIV);
  }

  if (IsToDraw(NOTATION_TOP)) {
    nsRect rect(0, 0, mencloseRect.width, mRuleThickness);
    DisplayBar(aBuilder, this, rect, aLists, NOTATION_TOP);
  }

  if (IsToDraw(NOTATION_BOTTOM)) {
    nsRect rect(0, mencloseRect.height - mRuleThickness,
                mencloseRect.width, mRuleThickness);
    DisplayBar(aBuilder, this, rect, aLists, NOTATION_BOTTOM);
  }

  if (IsToDraw(NOTATION_LEFT)) {
    nsRect rect(0, 0, mRuleThickness, mencloseRect.height);
    DisplayBar(aBuilder, this, rect, aLists, NOTATION_LEFT);
  }

  if (IsToDraw(NOTATION_RIGHT)) {
    nsRect rect(mencloseRect.width - mRuleThickness, 0,
                mRuleThickness, mencloseRect.height);
    DisplayBar(aBuilder, this, rect, aLists, NOTATION_RIGHT);
  }

  if (IsToDraw(NOTATION_ROUNDEDBOX)) {
    DisplayNotation(aBuilder, this, mencloseRect, aLists,
                    mRuleThickness, NOTATION_ROUNDEDBOX);
  }

  if (IsToDraw(NOTATION_CIRCLE)) {
    DisplayNotation(aBuilder, this, mencloseRect, aLists,
                    mRuleThickness, NOTATION_CIRCLE);
  }

  if (IsToDraw(NOTATION_UPDIAGONALSTRIKE)) {
    DisplayNotation(aBuilder, this, mencloseRect, aLists,
                    mRuleThickness, NOTATION_UPDIAGONALSTRIKE);
  }

  if (IsToDraw(NOTATION_UPDIAGONALARROW)) {
    DisplayNotation(aBuilder, this, mencloseRect, aLists,
                    mRuleThickness, NOTATION_UPDIAGONALARROW);
  }

  if (IsToDraw(NOTATION_DOWNDIAGONALSTRIKE)) {
    DisplayNotation(aBuilder, this, mencloseRect, aLists,
                    mRuleThickness, NOTATION_DOWNDIAGONALSTRIKE);
  }

  if (IsToDraw(NOTATION_HORIZONTALSTRIKE)) {
    nsRect rect(0, mencloseRect.height / 2 - mRuleThickness / 2,
                mencloseRect.width, mRuleThickness);
    DisplayBar(aBuilder, this, rect, aLists, NOTATION_HORIZONTALSTRIKE);
  }

  if (IsToDraw(NOTATION_VERTICALSTRIKE)) {
    nsRect rect(mencloseRect.width / 2 - mRuleThickness / 2, 0,
                mRuleThickness, mencloseRect.height);
    DisplayBar(aBuilder, this, rect, aLists, NOTATION_VERTICALSTRIKE);
  }
}

/* virtual */ nsresult
nsMathMLmencloseFrame::MeasureForWidth(DrawTarget* aDrawTarget,
                                       ReflowOutput& aDesiredSize)
{
  return PlaceInternal(aDrawTarget, false, aDesiredSize, true);
}

/* virtual */ nsresult
nsMathMLmencloseFrame::Place(DrawTarget*          aDrawTarget,
                             bool                 aPlaceOrigin,
                             ReflowOutput& aDesiredSize)
{
  return PlaceInternal(aDrawTarget, aPlaceOrigin, aDesiredSize, false);
}

/* virtual */ nsresult
nsMathMLmencloseFrame::PlaceInternal(DrawTarget*          aDrawTarget,
                                     bool                 aPlaceOrigin,
                                     ReflowOutput& aDesiredSize,
                                     bool                 aWidthOnly)
{
  ///////////////
  // Measure the size of our content using the base class to format like an
  // inferred mrow.
  ReflowOutput baseSize(aDesiredSize.GetWritingMode());
  nsresult rv =
    nsMathMLContainerFrame::Place(aDrawTarget, false, baseSize);

  if (NS_MATHML_HAS_ERROR(mPresentationData.flags) || NS_FAILED(rv)) {
      DidReflowChildren(PrincipalChildList().FirstChild());
      return rv;
    }

  nsBoundingMetrics bmBase = baseSize.mBoundingMetrics;
  nscoord dx_left = 0, dx_right = 0;
  nsBoundingMetrics bmLongdivChar, bmRadicalChar;
  nscoord radicalAscent = 0, radicalDescent = 0;
  nscoord longdivAscent = 0, longdivDescent = 0;
  nscoord psi = 0;
  nscoord leading = 0;

  ///////////////
  // Thickness of bars and font metrics
  nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);

  float fontSizeInflation = nsLayoutUtils::FontSizeInflationFor(this);
  RefPtr<nsFontMetrics> fm =
    nsLayoutUtils::GetFontMetricsForFrame(this, fontSizeInflation);
  GetRuleThickness(aDrawTarget, fm, mRuleThickness);
  if (mRuleThickness < onePixel) {
    mRuleThickness = onePixel;
  }

  char16_t one = '1';
  nsBoundingMetrics bmOne =
    nsLayoutUtils::AppUnitBoundsOfString(&one, 1, *fm, aDrawTarget);

  ///////////////
  // General rules: the menclose element takes the size of the enclosed content.
  // We add a padding when needed.

  // determine padding & psi
  nscoord padding = 3 * mRuleThickness;
  nscoord delta = padding % onePixel;
  if (delta)
    padding += onePixel - delta; // round up

  if (IsToDraw(NOTATION_LONGDIV) || IsToDraw(NOTATION_RADICAL)) {
    GetRadicalParameters(fm, StyleFont()->mMathDisplay ==
                         NS_MATHML_DISPLAYSTYLE_BLOCK,
                         mRadicalRuleThickness, leading, psi);

    // make sure that the rule appears on on screen
    if (mRadicalRuleThickness < onePixel) {
      mRadicalRuleThickness = onePixel;
    }

    // adjust clearance psi to get an exact number of pixels -- this
    // gives a nicer & uniform look on stacked radicals (bug 130282)
    delta = psi % onePixel;
    if (delta) {
      psi += onePixel - delta; // round up
    }
  }

  // Set horizontal parameters
  if (IsToDraw(NOTATION_ROUNDEDBOX) ||
      IsToDraw(NOTATION_TOP) ||
      IsToDraw(NOTATION_LEFT) ||
      IsToDraw(NOTATION_BOTTOM) ||
      IsToDraw(NOTATION_CIRCLE))
    dx_left = padding;

  if (IsToDraw(NOTATION_ROUNDEDBOX) ||
      IsToDraw(NOTATION_TOP) ||
      IsToDraw(NOTATION_RIGHT) ||
      IsToDraw(NOTATION_BOTTOM) ||
      IsToDraw(NOTATION_CIRCLE))
    dx_right = padding;

  // Set vertical parameters
  if (IsToDraw(NOTATION_RIGHT) ||
      IsToDraw(NOTATION_LEFT) ||
      IsToDraw(NOTATION_UPDIAGONALSTRIKE) ||
      IsToDraw(NOTATION_UPDIAGONALARROW) ||
      IsToDraw(NOTATION_DOWNDIAGONALSTRIKE) ||
      IsToDraw(NOTATION_VERTICALSTRIKE) ||
      IsToDraw(NOTATION_CIRCLE) ||
      IsToDraw(NOTATION_ROUNDEDBOX) ||
      IsToDraw(NOTATION_RADICAL) ||
      IsToDraw(NOTATION_LONGDIV) ||
      IsToDraw(NOTATION_PHASORANGLE)) {
      // set a minimal value for the base height
      bmBase.ascent = std::max(bmOne.ascent, bmBase.ascent);
      bmBase.descent = std::max(0, bmBase.descent);
  }

  mBoundingMetrics.ascent = bmBase.ascent;
  mBoundingMetrics.descent = bmBase.descent;

  if (IsToDraw(NOTATION_ROUNDEDBOX) ||
      IsToDraw(NOTATION_TOP) ||
      IsToDraw(NOTATION_LEFT) ||
      IsToDraw(NOTATION_RIGHT) ||
      IsToDraw(NOTATION_CIRCLE))
    mBoundingMetrics.ascent += padding;

  if (IsToDraw(NOTATION_ROUNDEDBOX) ||
      IsToDraw(NOTATION_LEFT) ||
      IsToDraw(NOTATION_RIGHT) ||
      IsToDraw(NOTATION_BOTTOM) ||
      IsToDraw(NOTATION_CIRCLE))
    mBoundingMetrics.descent += padding;

   ///////////////
   // phasorangle notation
  if (IsToDraw(NOTATION_PHASORANGLE)) {
    nscoord phasorangleWidth = kPhasorangleWidth * mRuleThickness;
    // Update horizontal parameters
    dx_left = std::max(dx_left, phasorangleWidth);
  }

  ///////////////
  // updiagonal arrow notation. We need enough space at the top right corner to
  // draw the arrow head.
  if (IsToDraw(NOTATION_UPDIAGONALARROW)) {
    // This is an estimate, see nsDisplayNotation::Paint for the exact head size
    nscoord arrowHeadSize = kArrowHeadSize * mRuleThickness;

    // We want that the arrow shaft strikes the menclose content and that the
    // arrow head does not overlap with that content. Hence we add some space
    // on the right. We don't add space on the top but only ensure that the
    // ascent is large enough.
    dx_right = std::max(dx_right, arrowHeadSize);
    mBoundingMetrics.ascent = std::max(mBoundingMetrics.ascent, arrowHeadSize);
  }

  ///////////////
  // circle notation: we don't want the ellipse to overlap the enclosed
  // content. Hence, we need to increase the size of the bounding box by a
  // factor of at least sqrt(2).
  if (IsToDraw(NOTATION_CIRCLE)) {
    double ratio = (sqrt(2.0) - 1.0) / 2.0;
    nscoord padding2;

    // Update horizontal parameters
    padding2 = ratio * bmBase.width;

    dx_left = std::max(dx_left, padding2);
    dx_right = std::max(dx_right, padding2);

    // Update vertical parameters
    padding2 = ratio * (bmBase.ascent + bmBase.descent);

    mBoundingMetrics.ascent = std::max(mBoundingMetrics.ascent,
                                     bmBase.ascent + padding2);
    mBoundingMetrics.descent = std::max(mBoundingMetrics.descent,
                                      bmBase.descent + padding2);
  }

  ///////////////
  // longdiv notation:
  if (IsToDraw(NOTATION_LONGDIV)) {
    if (aWidthOnly) {
        nscoord longdiv_width = mMathMLChar[mLongDivCharIndex].
          GetMaxWidth(this, aDrawTarget, fontSizeInflation);

        // Update horizontal parameters
        dx_left = std::max(dx_left, longdiv_width);
    } else {
      // Stretch the parenthesis to the appropriate height if it is not
      // big enough.
      nsBoundingMetrics contSize = bmBase;
      contSize.ascent = mRuleThickness;
      contSize.descent = bmBase.ascent + bmBase.descent + psi;

      // height(longdiv) should be >= height(base) + psi + mRuleThickness
      mMathMLChar[mLongDivCharIndex].Stretch(this, aDrawTarget,
                                             fontSizeInflation,
                                             NS_STRETCH_DIRECTION_VERTICAL,
                                             contSize, bmLongdivChar,
                                             NS_STRETCH_LARGER, false);
      mMathMLChar[mLongDivCharIndex].GetBoundingMetrics(bmLongdivChar);

      // Update horizontal parameters
      dx_left = std::max(dx_left, bmLongdivChar.width);

      // Update vertical parameters
      longdivAscent = bmBase.ascent + psi + mRuleThickness;
      longdivDescent = std::max(bmBase.descent,
                              (bmLongdivChar.ascent + bmLongdivChar.descent -
                               longdivAscent));

      mBoundingMetrics.ascent = std::max(mBoundingMetrics.ascent,
                                       longdivAscent);
      mBoundingMetrics.descent = std::max(mBoundingMetrics.descent,
                                        longdivDescent);
    }
  }

  ///////////////
  // radical notation:
  if (IsToDraw(NOTATION_RADICAL)) {
    nscoord *dx_leading = StyleVisibility()->mDirection ? &dx_right : &dx_left;

    if (aWidthOnly) {
      nscoord radical_width = mMathMLChar[mRadicalCharIndex].
        GetMaxWidth(this, aDrawTarget, fontSizeInflation);

      // Update horizontal parameters
      *dx_leading = std::max(*dx_leading, radical_width);
    } else {
      // Stretch the radical symbol to the appropriate height if it is not
      // big enough.
      nsBoundingMetrics contSize = bmBase;
      contSize.ascent = mRadicalRuleThickness;
      contSize.descent = bmBase.ascent + bmBase.descent + psi;

      // height(radical) should be >= height(base) + psi + mRadicalRuleThickness
      mMathMLChar[mRadicalCharIndex].Stretch(this, aDrawTarget,
                                             fontSizeInflation,
                                             NS_STRETCH_DIRECTION_VERTICAL,
                                             contSize, bmRadicalChar,
                                             NS_STRETCH_LARGER,
                                             StyleVisibility()->mDirection);
      mMathMLChar[mRadicalCharIndex].GetBoundingMetrics(bmRadicalChar);

      // Update horizontal parameters
      *dx_leading = std::max(*dx_leading, bmRadicalChar.width);

      // Update vertical parameters
      radicalAscent = bmBase.ascent + psi + mRadicalRuleThickness;
      radicalDescent = std::max(bmBase.descent,
                              (bmRadicalChar.ascent + bmRadicalChar.descent -
                               radicalAscent));

      mBoundingMetrics.ascent = std::max(mBoundingMetrics.ascent,
                                       radicalAscent);
      mBoundingMetrics.descent = std::max(mBoundingMetrics.descent,
                                        radicalDescent);
    }
  }

  ///////////////
  //
  if (IsToDraw(NOTATION_CIRCLE) ||
      IsToDraw(NOTATION_ROUNDEDBOX) ||
      (IsToDraw(NOTATION_LEFT) && IsToDraw(NOTATION_RIGHT))) {
    // center the menclose around the content (horizontally)
    dx_left = dx_right = std::max(dx_left, dx_right);
  }

  ///////////////
  // The maximum size is now computed: set the remaining parameters
  mBoundingMetrics.width = dx_left + bmBase.width + dx_right;

  mBoundingMetrics.leftBearing = std::min(0, dx_left + bmBase.leftBearing);
  mBoundingMetrics.rightBearing =
    std::max(mBoundingMetrics.width, dx_left + bmBase.rightBearing);

  aDesiredSize.Width() = mBoundingMetrics.width;

  aDesiredSize.SetBlockStartAscent(std::max(mBoundingMetrics.ascent,
                                            baseSize.BlockStartAscent()));
  aDesiredSize.Height() = aDesiredSize.BlockStartAscent() +
    std::max(mBoundingMetrics.descent,
             baseSize.Height() - baseSize.BlockStartAscent());

  if (IsToDraw(NOTATION_LONGDIV) || IsToDraw(NOTATION_RADICAL)) {
    nscoord desiredSizeAscent = aDesiredSize.BlockStartAscent();
    nscoord desiredSizeDescent = aDesiredSize.Height() -
                                 aDesiredSize.BlockStartAscent();

    if (IsToDraw(NOTATION_LONGDIV)) {
      desiredSizeAscent = std::max(desiredSizeAscent,
                                 longdivAscent + leading);
      desiredSizeDescent = std::max(desiredSizeDescent,
                                  longdivDescent + mRuleThickness);
    }

    if (IsToDraw(NOTATION_RADICAL)) {
      desiredSizeAscent = std::max(desiredSizeAscent,
                                 radicalAscent + leading);
      desiredSizeDescent = std::max(desiredSizeDescent,
                                    radicalDescent + mRadicalRuleThickness);
    }

    aDesiredSize.SetBlockStartAscent(desiredSizeAscent);
    aDesiredSize.Height() = desiredSizeAscent + desiredSizeDescent;
  }

  if (IsToDraw(NOTATION_CIRCLE) ||
      IsToDraw(NOTATION_ROUNDEDBOX) ||
      (IsToDraw(NOTATION_TOP) && IsToDraw(NOTATION_BOTTOM))) {
    // center the menclose around the content (vertically)
    nscoord dy = std::max(aDesiredSize.BlockStartAscent() - bmBase.ascent,
                          aDesiredSize.Height() -
                          aDesiredSize.BlockStartAscent() - bmBase.descent);

    aDesiredSize.SetBlockStartAscent(bmBase.ascent + dy);
    aDesiredSize.Height() = aDesiredSize.BlockStartAscent() + bmBase.descent + dy;
  }

  // Update mBoundingMetrics ascent/descent
  if (IsToDraw(NOTATION_TOP) ||
      IsToDraw(NOTATION_RIGHT) ||
      IsToDraw(NOTATION_LEFT) ||
      IsToDraw(NOTATION_UPDIAGONALSTRIKE) ||
      IsToDraw(NOTATION_UPDIAGONALARROW) ||
      IsToDraw(NOTATION_DOWNDIAGONALSTRIKE) ||
      IsToDraw(NOTATION_VERTICALSTRIKE) ||
      IsToDraw(NOTATION_CIRCLE) ||
      IsToDraw(NOTATION_ROUNDEDBOX))
    mBoundingMetrics.ascent = aDesiredSize.BlockStartAscent();

  if (IsToDraw(NOTATION_BOTTOM) ||
      IsToDraw(NOTATION_RIGHT) ||
      IsToDraw(NOTATION_LEFT) ||
      IsToDraw(NOTATION_UPDIAGONALSTRIKE) ||
      IsToDraw(NOTATION_UPDIAGONALARROW) ||
      IsToDraw(NOTATION_DOWNDIAGONALSTRIKE) ||
      IsToDraw(NOTATION_VERTICALSTRIKE) ||
      IsToDraw(NOTATION_CIRCLE) ||
      IsToDraw(NOTATION_ROUNDEDBOX))
    mBoundingMetrics.descent = aDesiredSize.Height() - aDesiredSize.BlockStartAscent();

  // phasorangle notation:
  // move up from the bottom by the angled line height
  if (IsToDraw(NOTATION_PHASORANGLE))
    mBoundingMetrics.ascent = std::max(mBoundingMetrics.ascent, 2 * kPhasorangleWidth * mRuleThickness - mBoundingMetrics.descent);

  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.BlockStartAscent();

  if (aPlaceOrigin) {
    //////////////////
    // Set position and size of MathMLChars
    if (IsToDraw(NOTATION_LONGDIV))
      mMathMLChar[mLongDivCharIndex].SetRect(nsRect(dx_left -
                                                    bmLongdivChar.width,
                                                    aDesiredSize.BlockStartAscent() -
                                                    longdivAscent,
                                                    bmLongdivChar.width,
                                                    bmLongdivChar.ascent +
                                                    bmLongdivChar.descent));

    if (IsToDraw(NOTATION_RADICAL)) {
      nscoord dx = (StyleVisibility()->mDirection ?
                    dx_left + bmBase.width : dx_left - bmRadicalChar.width);

      mMathMLChar[mRadicalCharIndex].SetRect(nsRect(dx,
                                                    aDesiredSize.BlockStartAscent() -
                                                    radicalAscent,
                                                    bmRadicalChar.width,
                                                    bmRadicalChar.ascent +
                                                    bmRadicalChar.descent));
    }

    mContentWidth = bmBase.width;

    //////////////////
    // Finish reflowing child frames
    PositionRowChildFrames(dx_left, aDesiredSize.BlockStartAscent());
  }

  return NS_OK;
}

nscoord
nsMathMLmencloseFrame::FixInterFrameSpacing(ReflowOutput& aDesiredSize)
{
  nscoord gap = nsMathMLContainerFrame::FixInterFrameSpacing(aDesiredSize);
  if (!gap)
    return 0;

  // Move the MathML characters
  nsRect rect;
  for (uint32_t i = 0; i < mMathMLChar.Length(); i++) {
    mMathMLChar[i].GetRect(rect);
    rect.MoveBy(gap, 0);
    mMathMLChar[i].SetRect(rect);
  }

  return gap;
}

nsresult
nsMathMLmencloseFrame::AttributeChanged(int32_t         aNameSpaceID,
                                        nsAtom*        aAttribute,
                                        int32_t         aModType)
{
  if (aAttribute == nsGkAtoms::notation_) {
    InitNotations();
  }

  return nsMathMLContainerFrame::
    AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

//////////////////
// the Style System will use these to pass the proper ComputedStyle to our
// MathMLChar
ComputedStyle*
nsMathMLmencloseFrame::GetAdditionalComputedStyle(int32_t aIndex) const
{
  int32_t len = mMathMLChar.Length();
  if (aIndex >= 0 && aIndex < len)
    return mMathMLChar[aIndex].GetComputedStyle();
  else
    return nullptr;
}

void
nsMathMLmencloseFrame::SetAdditionalComputedStyle(int32_t          aIndex,
                                                 ComputedStyle*  aComputedStyle)
{
  int32_t len = mMathMLChar.Length();
  if (aIndex >= 0 && aIndex < len)
    mMathMLChar[aIndex].SetComputedStyle(aComputedStyle);
}

class nsDisplayNotation : public nsDisplayItem
{
public:
  nsDisplayNotation(nsDisplayListBuilder* aBuilder,
                    nsIFrame* aFrame, const nsRect& aRect,
                    nscoord aThickness, nsMencloseNotation aType)
    : nsDisplayItem(aBuilder, aFrame), mRect(aRect),
      mThickness(aThickness), mType(aType) {
    MOZ_COUNT_CTOR(nsDisplayNotation);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayNotation() {
    MOZ_COUNT_DTOR(nsDisplayNotation);
  }
#endif

  virtual uint32_t GetPerFrameKey() const override {
    return (mType << TYPE_BITS) | nsDisplayItem::GetPerFrameKey();
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     gfxContext* aCtx) override;
  NS_DISPLAY_DECL_NAME("MathMLMencloseNotation", TYPE_MATHML_MENCLOSE_NOTATION)

private:
  nsRect             mRect;
  nscoord            mThickness;
  nsMencloseNotation mType;
};

void nsDisplayNotation::Paint(nsDisplayListBuilder* aBuilder,
                              gfxContext* aCtx)
{
  DrawTarget& aDrawTarget = *aCtx->GetDrawTarget();
  nsPresContext* presContext = mFrame->PresContext();

  Float strokeWidth = presContext->AppUnitsToGfxUnits(mThickness);

  Rect rect = NSRectToRect(mRect + ToReferenceFrame(),
                           presContext->AppUnitsPerDevPixel());
  rect.Deflate(strokeWidth / 2.f);

  ColorPattern color(ToDeviceColor(
    mFrame->GetVisitedDependentColor(&nsStyleText::mWebkitTextFillColor)));

  StrokeOptions strokeOptions(strokeWidth);

  switch(mType)
  {
    case NOTATION_CIRCLE: {
      RefPtr<Path> ellipse =
        MakePathForEllipse(aDrawTarget, rect.Center(), rect.Size());
      aDrawTarget.Stroke(ellipse, color, strokeOptions);
      return;
    }
    case NOTATION_ROUNDEDBOX: {
      Float radius = 3 * strokeWidth;
      RectCornerRadii radii(radius, radius);
      RefPtr<Path> roundedRect =
        MakePathForRoundedRect(aDrawTarget, rect, radii, true);
      aDrawTarget.Stroke(roundedRect, color, strokeOptions);
      return;
    }
    case NOTATION_UPDIAGONALSTRIKE: {
      aDrawTarget.StrokeLine(rect.BottomLeft(), rect.TopRight(),
                             color, strokeOptions);
      return;
    }
    case NOTATION_DOWNDIAGONALSTRIKE: {
      aDrawTarget.StrokeLine(rect.TopLeft(), rect.BottomRight(),
                             color, strokeOptions);
      return;
    }
    case NOTATION_UPDIAGONALARROW: {
      // Compute some parameters to draw the updiagonalarrow. The values below
      // are taken from MathJax's HTML-CSS output.
      Float W = rect.Width(); gfxFloat H = rect.Height();
      Float l = sqrt(W*W + H*H);
      Float f = Float(kArrowHeadSize) * strokeWidth / l;
      Float w = W * f; gfxFloat h = H * f;

      // Draw the arrow shaft
      aDrawTarget.StrokeLine(rect.BottomLeft(),
                             rect.TopRight() + Point(-.7*w, .7*h),
                             color, strokeOptions);

      // Draw the arrow head
      RefPtr<PathBuilder> builder = aDrawTarget.CreatePathBuilder();
      builder->MoveTo(rect.TopRight());
      builder->LineTo(rect.TopRight() + Point(-w -.4*h, std::max(-strokeWidth / 2.0, h - .4*w)));
      builder->LineTo(rect.TopRight() + Point(-.7*w, .7*h));
      builder->LineTo(rect.TopRight() + Point(std::min(strokeWidth / 2.0, -w + .4*h), h + .4*w));
      builder->Close();
      RefPtr<Path> path = builder->Finish();
      aDrawTarget.Fill(path, color);
      return;
    }
    case NOTATION_PHASORANGLE: {
      // Compute some parameters to draw the angled line,
      // that uses a slope of 2 (angle = tan^-1(2)).
      // H = w * tan(angle) = w * 2
      Float w = Float(kPhasorangleWidth) * strokeWidth;
      Float H = 2 * w;

      // Draw the angled line
      aDrawTarget.StrokeLine(rect.BottomLeft(),
                             rect.BottomLeft() + Point(w, -H),
                             color, strokeOptions);
      return;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("This notation can not be drawn using "
                             "nsDisplayNotation");
  }
}

void
nsMathMLmencloseFrame::DisplayNotation(nsDisplayListBuilder* aBuilder,
                                       nsIFrame* aFrame, const nsRect& aRect,
                                       const nsDisplayListSet& aLists,
                                       nscoord aThickness,
                                       nsMencloseNotation aType)
{
  if (!aFrame->StyleVisibility()->IsVisible() || aRect.IsEmpty() ||
      aThickness <= 0)
    return;

  aLists.Content()->AppendToTop(
    MakeDisplayItem<nsDisplayNotation>(aBuilder, aFrame, aRect, aThickness, aType));
}

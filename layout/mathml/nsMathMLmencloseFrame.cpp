/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is
 * The University Of Queensland.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *   David J. Fiddes <D.J.Fiddes@hw.ac.uk>
 *   Vilya Harvey <vilya@nag.co.uk>
 *   Shyjan Mahamud <mahamud@cs.cmu.edu>
 *   Karl Tomlinson <karlt+@karlt.net>, Mozilla Corporation
 *   Frederic Wang <fred.wang@free.fr> - extension of <msqrt/> to <menclose/>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsRenderingContext.h"
#include "nsWhitespaceTokenizer.h"

#include "nsMathMLmencloseFrame.h"
#include "nsDisplayList.h"
#include "gfxContext.h"

//
// <menclose> -- enclose content with a stretching symbol such
// as a long division sign. - implementation

// longdiv:
// Unicode 5.1 assigns U+27CC to LONG DIVISION, but a right parenthesis
// renders better with current font support.
static const PRUnichar kLongDivChar = ')';

// radical: 'SQUARE ROOT'
static const PRUnichar kRadicalChar = 0x221A;

nsIFrame*
NS_NewMathMLmencloseFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmencloseFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmencloseFrame)

nsMathMLmencloseFrame::nsMathMLmencloseFrame(nsStyleContext* aContext) :
  nsMathMLContainerFrame(aContext), mNotationsToDraw(0),
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

  // No need to track the style context given to our MathML chars.
  // The Style System will use Get/SetAdditionalStyleContext() to keep it
  // up-to-date if dynamic changes arise.
  PRUint32 i = mMathMLChar.Length();
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
  mMathMLChar[i].SetData(presContext, Char);
  ResolveMathMLCharStyle(presContext, mContent, mStyleContext,
                         &mMathMLChar[i],
                         PR_TRUE);

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
    mNotationsToDraw |= NOTATION_LONGDIV;
  } else if (aNotation.EqualsLiteral("actuarial")) {
    mNotationsToDraw |= (NOTATION_RIGHT | NOTATION_TOP);
  } else if (aNotation.EqualsLiteral("radical")) {
    rv = AllocateMathMLChar(NOTATION_RADICAL);
    NS_ENSURE_SUCCESS(rv, rv);
    mNotationsToDraw |= NOTATION_RADICAL;
  } else if (aNotation.EqualsLiteral("box")) {
    mNotationsToDraw |= (NOTATION_LEFT | NOTATION_RIGHT |
                         NOTATION_TOP | NOTATION_BOTTOM);
  } else if (aNotation.EqualsLiteral("roundedbox")) {
    mNotationsToDraw |= NOTATION_ROUNDEDBOX;
  } else if (aNotation.EqualsLiteral("circle")) {
    mNotationsToDraw |= NOTATION_CIRCLE;
  } else if (aNotation.EqualsLiteral("left")) {
    mNotationsToDraw |= NOTATION_LEFT;
  } else if (aNotation.EqualsLiteral("right")) {
    mNotationsToDraw |= NOTATION_RIGHT;
  } else if (aNotation.EqualsLiteral("top")) {
    mNotationsToDraw |= NOTATION_TOP;
  } else if (aNotation.EqualsLiteral("bottom")) {
    mNotationsToDraw |= NOTATION_BOTTOM;
  } else if (aNotation.EqualsLiteral("updiagonalstrike")) {
    mNotationsToDraw |= NOTATION_UPDIAGONALSTRIKE;
  } else if (aNotation.EqualsLiteral("downdiagonalstrike")) {
    mNotationsToDraw |= NOTATION_DOWNDIAGONALSTRIKE;
  } else if (aNotation.EqualsLiteral("verticalstrike")) {
    mNotationsToDraw |= NOTATION_VERTICALSTRIKE;
  } else if (aNotation.EqualsLiteral("horizontalstrike")) {
    mNotationsToDraw |= NOTATION_HORIZONTALSTRIKE;
  } else if (aNotation.EqualsLiteral("madruwb")) {
    mNotationsToDraw |= (NOTATION_RIGHT | NOTATION_BOTTOM);
  }

  return NS_OK;
}

/*
 * Initialize the list of notations to draw
 */
void nsMathMLmencloseFrame::InitNotations()
{
  mNotationsToDraw = 0;
  mLongDivCharIndex = mRadicalCharIndex = -1;
  mMathMLChar.Clear();

  nsAutoString value;

  if (GetAttribute(mContent, mPresentationData.mstyle, nsGkAtoms::notation_,
                   value)) {
    // parse the notation attribute
    nsWhitespaceTokenizer tokenizer(value);

    while (tokenizer.hasMoreTokens())
      AddNotation(tokenizer.nextToken());
  } else {
    // default: longdiv
    if (NS_FAILED(AllocateMathMLChar(NOTATION_LONGDIV)))
      return;
    mNotationsToDraw = NOTATION_LONGDIV;
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

NS_IMETHODIMP
nsMathMLmencloseFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                        const nsRect&           aDirtyRect,
                                        const nsDisplayListSet& aLists)
{
  /////////////
  // paint the menclosed content
  nsresult rv = nsMathMLContainerFrame::BuildDisplayList(aBuilder, aDirtyRect,
                                                         aLists);

  NS_ENSURE_SUCCESS(rv, rv);

  if (NS_MATHML_HAS_ERROR(mPresentationData.flags))
    return rv;

  nsRect mencloseRect = nsIFrame::GetRect();
  mencloseRect.x = mencloseRect.y = 0;

  if (IsToDraw(NOTATION_RADICAL)) {
    rv = mMathMLChar[mRadicalCharIndex].Display(aBuilder, this, aLists);
    NS_ENSURE_SUCCESS(rv, rv);

    nsRect rect;
    mMathMLChar[mRadicalCharIndex].GetRect(rect);
    rect.MoveBy(rect.width, 0);
    rect.SizeTo(mContentWidth, mRuleThickness);
    rv = DisplayBar(aBuilder, this, rect, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (IsToDraw(NOTATION_LONGDIV)) {
    rv = mMathMLChar[mLongDivCharIndex].Display(aBuilder, this, aLists);
    NS_ENSURE_SUCCESS(rv, rv);

    nsRect rect;
    mMathMLChar[mLongDivCharIndex].GetRect(rect);
    rect.SizeTo(rect.width + mContentWidth, mRuleThickness);
    rv = DisplayBar(aBuilder, this, rect, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (IsToDraw(NOTATION_TOP)) {
    nsRect rect(0, 0, mencloseRect.width, mRuleThickness);
    rv = DisplayBar(aBuilder, this, rect, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (IsToDraw(NOTATION_BOTTOM)) {
    nsRect rect(0, mencloseRect.height - mRuleThickness,
                mencloseRect.width, mRuleThickness);
    rv = DisplayBar(aBuilder, this, rect, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (IsToDraw(NOTATION_LEFT)) {
    nsRect rect(0, 0, mRuleThickness, mencloseRect.height);
    rv = DisplayBar(aBuilder, this, rect, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (IsToDraw(NOTATION_RIGHT)) {
    nsRect rect(mencloseRect.width - mRuleThickness, 0,
                mRuleThickness, mencloseRect.height);
    rv = DisplayBar(aBuilder, this, rect, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (IsToDraw(NOTATION_ROUNDEDBOX)) {
    rv = DisplayNotation(aBuilder, this, mencloseRect, aLists,
                         mRuleThickness, NOTATION_ROUNDEDBOX);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (IsToDraw(NOTATION_CIRCLE)) {
    rv = DisplayNotation(aBuilder, this, mencloseRect, aLists,
                         mRuleThickness, NOTATION_CIRCLE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (IsToDraw(NOTATION_UPDIAGONALSTRIKE)) {
    rv = DisplayNotation(aBuilder, this, mencloseRect, aLists,
                         mRuleThickness, NOTATION_UPDIAGONALSTRIKE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (IsToDraw(NOTATION_DOWNDIAGONALSTRIKE)) {
    rv = DisplayNotation(aBuilder, this, mencloseRect, aLists,
                         mRuleThickness, NOTATION_DOWNDIAGONALSTRIKE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (IsToDraw(NOTATION_HORIZONTALSTRIKE)) {
    nsRect rect(0, mencloseRect.height / 2 - mRuleThickness / 2,
                mencloseRect.width, mRuleThickness);
    rv = DisplayBar(aBuilder, this, rect, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (IsToDraw(NOTATION_VERTICALSTRIKE)) {
    nsRect rect(mencloseRect.width / 2 - mRuleThickness / 2, 0,
                mRuleThickness, mencloseRect.height);
    rv = DisplayBar(aBuilder, this, rect, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return rv;
}

/* virtual */ nsresult
nsMathMLmencloseFrame::MeasureForWidth(nsRenderingContext& aRenderingContext,
                                       nsHTMLReflowMetrics& aDesiredSize)
{
  return PlaceInternal(aRenderingContext, PR_FALSE, aDesiredSize, PR_TRUE);
}

/* virtual */ nsresult
nsMathMLmencloseFrame::Place(nsRenderingContext& aRenderingContext,
                             PRBool               aPlaceOrigin,
                             nsHTMLReflowMetrics& aDesiredSize)
{
  return PlaceInternal(aRenderingContext, aPlaceOrigin, aDesiredSize, PR_FALSE);
}

/* virtual */ nsresult
nsMathMLmencloseFrame::PlaceInternal(nsRenderingContext& aRenderingContext,
                                     PRBool               aPlaceOrigin,
                                     nsHTMLReflowMetrics& aDesiredSize,
                                     PRBool               aWidthOnly)
{
  ///////////////
  // Measure the size of our content using the base class to format like an
  // inferred mrow.
  nsHTMLReflowMetrics baseSize;
  nsresult rv =
    nsMathMLContainerFrame::Place(aRenderingContext, PR_FALSE, baseSize);

  if (NS_MATHML_HAS_ERROR(mPresentationData.flags) || NS_FAILED(rv)) {
      DidReflowChildren(GetFirstPrincipalChild());
      return rv;
    }

  nsBoundingMetrics bmBase = baseSize.mBoundingMetrics;
  nscoord dx_left = 0, dx_right = 0;
  nsBoundingMetrics bmLongdivChar, bmRadicalChar;
  nscoord radicalAscent = 0, radicalDescent = 0;
  nscoord longdivAscent = 0, longdivDescent = 0;
  nscoord psi = 0;

  ///////////////
  // Thickness of bars and font metrics
  nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);

  nscoord mEmHeight;
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm));
  aRenderingContext.SetFont(fm);
  GetRuleThickness(aRenderingContext, fm, mRuleThickness);
  GetEmHeight(fm, mEmHeight);

  PRUnichar one = '1';
  nsBoundingMetrics bmOne = aRenderingContext.GetBoundingMetrics(&one, 1);

  ///////////////
  // General rules: the menclose element takes the size of the enclosed content.
  // We add a padding when needed.

  // determine padding & psi
  nscoord padding = 3 * mRuleThickness;
  nscoord delta = padding % onePixel;
  if (delta)
    padding += onePixel - delta; // round up

  if (IsToDraw(NOTATION_LONGDIV) || IsToDraw(NOTATION_RADICAL)) {
      nscoord phi;
      // Rule 11, App. G, TeXbook
      // psi = clearance between rule and content
      if (NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags))
        phi = fm->XHeight();
      else
        phi = mRuleThickness;
      psi = mRuleThickness + phi / 4;

      delta = psi % onePixel;
      if (delta)
        psi += onePixel - delta; // round up
    }

  if (mRuleThickness < onePixel)
    mRuleThickness = onePixel;
 
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
      IsToDraw(NOTATION_DOWNDIAGONALSTRIKE) ||
      IsToDraw(NOTATION_VERTICALSTRIKE) ||
      IsToDraw(NOTATION_CIRCLE) ||
      IsToDraw(NOTATION_ROUNDEDBOX) ||
      IsToDraw(NOTATION_RADICAL) ||
      IsToDraw(NOTATION_LONGDIV)) {
      // set a minimal value for the base height
      bmBase.ascent = NS_MAX(bmOne.ascent, bmBase.ascent);
      bmBase.descent = NS_MAX(0, bmBase.descent);
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
  // circle notation: we don't want the ellipse to overlap the enclosed
  // content. Hence, we need to increase the size of the bounding box by a
  // factor of at least sqrt(2).
  if (IsToDraw(NOTATION_CIRCLE)) {
    double ratio = (sqrt(2.0) - 1.0) / 2.0;
    nscoord padding2;

    // Update horizontal parameters
    padding2 = ratio * bmBase.width;

    dx_left = NS_MAX(dx_left, padding2);
    dx_right = NS_MAX(dx_right, padding2);

    // Update vertical parameters
    padding2 = ratio * (bmBase.ascent + bmBase.descent);

    mBoundingMetrics.ascent = NS_MAX(mBoundingMetrics.ascent,
                                     bmBase.ascent + padding2);
    mBoundingMetrics.descent = NS_MAX(mBoundingMetrics.descent,
                                      bmBase.descent + padding2);
  }

  ///////////////
  // longdiv notation:
  if (IsToDraw(NOTATION_LONGDIV)) {
    if (aWidthOnly) {
        nscoord longdiv_width = mMathMLChar[mLongDivCharIndex].
          GetMaxWidth(PresContext(), aRenderingContext);

        // Update horizontal parameters
        dx_left = NS_MAX(dx_left, longdiv_width);
    } else {
      // Stretch the parenthesis to the appropriate height if it is not
      // big enough.
      nsBoundingMetrics contSize = bmBase;
      contSize.ascent = mRuleThickness;
      contSize.descent = bmBase.ascent + bmBase.descent + psi;

      // height(longdiv) should be >= height(base) + psi + mRuleThickness
      mMathMLChar[mLongDivCharIndex].Stretch(PresContext(), aRenderingContext,
                                             NS_STRETCH_DIRECTION_VERTICAL,
                                             contSize, bmLongdivChar,
                                             NS_STRETCH_LARGER);
      mMathMLChar[mLongDivCharIndex].GetBoundingMetrics(bmLongdivChar);

      // Update horizontal parameters
      dx_left = NS_MAX(dx_left, bmLongdivChar.width);

      // Update vertical parameters
      longdivAscent = bmBase.ascent + psi + mRuleThickness;
      longdivDescent = NS_MAX(bmBase.descent,
                              (bmLongdivChar.ascent + bmLongdivChar.descent -
                               longdivAscent));

      mBoundingMetrics.ascent = NS_MAX(mBoundingMetrics.ascent,
                                       longdivAscent);
      mBoundingMetrics.descent = NS_MAX(mBoundingMetrics.descent,
                                        longdivDescent);
    }
  }

  ///////////////
  // radical notation:
  if (IsToDraw(NOTATION_RADICAL)) {
    if (aWidthOnly) {
      nscoord radical_width = mMathMLChar[mRadicalCharIndex].
        GetMaxWidth(PresContext(), aRenderingContext);
      
      // Update horizontal parameters
      dx_left = NS_MAX(dx_left, radical_width);
    } else {
      // Stretch the radical symbol to the appropriate height if it is not
      // big enough.
      nsBoundingMetrics contSize = bmBase;
      contSize.ascent = mRuleThickness;
      contSize.descent = bmBase.ascent + bmBase.descent + psi;

      // height(radical) should be >= height(base) + psi + mRuleThickness
      mMathMLChar[mRadicalCharIndex].Stretch(PresContext(), aRenderingContext,
                                             NS_STRETCH_DIRECTION_VERTICAL,
                                             contSize, bmRadicalChar,
                                             NS_STRETCH_LARGER);
      mMathMLChar[mRadicalCharIndex].GetBoundingMetrics(bmRadicalChar);

      // Update horizontal parameters
      dx_left = NS_MAX(dx_left, bmRadicalChar.width);

      // Update vertical parameters
      radicalAscent = bmBase.ascent + psi + mRuleThickness;
      radicalDescent = NS_MAX(bmBase.descent,
                              (bmRadicalChar.ascent + bmRadicalChar.descent -
                               radicalAscent));

      mBoundingMetrics.ascent = NS_MAX(mBoundingMetrics.ascent,
                                       radicalAscent);
      mBoundingMetrics.descent = NS_MAX(mBoundingMetrics.descent,
                                        radicalDescent);
    }
  }

  ///////////////
  //
  if (IsToDraw(NOTATION_CIRCLE) ||
      IsToDraw(NOTATION_ROUNDEDBOX) ||
      (IsToDraw(NOTATION_LEFT) && IsToDraw(NOTATION_RIGHT))) {
    // center the menclose around the content (horizontally)
    dx_left = dx_right = NS_MAX(dx_left, dx_right);
  }

  ///////////////
  // The maximum size is now computed: set the remaining parameters
  mBoundingMetrics.width = dx_left + bmBase.width + dx_right;

  mBoundingMetrics.leftBearing = NS_MIN(0, dx_left + bmBase.leftBearing);
  mBoundingMetrics.rightBearing =
    NS_MAX(mBoundingMetrics.width, dx_left + bmBase.rightBearing);
  
  aDesiredSize.width = mBoundingMetrics.width;

  aDesiredSize.ascent = NS_MAX(mBoundingMetrics.ascent, baseSize.ascent);
  aDesiredSize.height = aDesiredSize.ascent +
    NS_MAX(mBoundingMetrics.descent, baseSize.height - baseSize.ascent);

  if (IsToDraw(NOTATION_LONGDIV) || IsToDraw(NOTATION_RADICAL)) {
    // get the leading to be left at the top of the resulting frame
    // this seems more reliable than using fm->GetLeading() on suspicious
    // fonts
    nscoord leading = nscoord(0.2f * mEmHeight);
    nscoord desiredSizeAscent = aDesiredSize.ascent;
    nscoord desiredSizeDescent = aDesiredSize.height - aDesiredSize.ascent;
    
    if (IsToDraw(NOTATION_LONGDIV)) {
      desiredSizeAscent = NS_MAX(desiredSizeAscent,
                                 longdivAscent + leading);
      desiredSizeDescent = NS_MAX(desiredSizeDescent,
                                  longdivDescent + mRuleThickness);
    }
    
    if (IsToDraw(NOTATION_RADICAL)) {
      desiredSizeAscent = NS_MAX(desiredSizeAscent,
                                 radicalAscent + leading);
      desiredSizeDescent = NS_MAX(desiredSizeDescent,
                                  radicalDescent + mRuleThickness);
    }

    aDesiredSize.ascent = desiredSizeAscent;
    aDesiredSize.height = desiredSizeAscent + desiredSizeDescent;
  }
    
  if (IsToDraw(NOTATION_CIRCLE) ||
      IsToDraw(NOTATION_ROUNDEDBOX) ||
      (IsToDraw(NOTATION_TOP) && IsToDraw(NOTATION_BOTTOM))) {
    // center the menclose around the content (vertically)
    nscoord dy = NS_MAX(aDesiredSize.ascent - bmBase.ascent,
                        aDesiredSize.height - aDesiredSize.ascent -
                        bmBase.descent);

    aDesiredSize.ascent = bmBase.ascent + dy;
    aDesiredSize.height = aDesiredSize.ascent + bmBase.descent + dy;
  }

  // Update mBoundingMetrics ascent/descent
  if (IsToDraw(NOTATION_TOP) ||
      IsToDraw(NOTATION_RIGHT) ||
      IsToDraw(NOTATION_LEFT) ||
      IsToDraw(NOTATION_UPDIAGONALSTRIKE) ||
      IsToDraw(NOTATION_DOWNDIAGONALSTRIKE) ||
      IsToDraw(NOTATION_VERTICALSTRIKE) ||
      IsToDraw(NOTATION_CIRCLE) ||
      IsToDraw(NOTATION_ROUNDEDBOX))
    mBoundingMetrics.ascent = aDesiredSize.ascent;
  
  if (IsToDraw(NOTATION_BOTTOM) ||
      IsToDraw(NOTATION_RIGHT) ||
      IsToDraw(NOTATION_LEFT) ||
      IsToDraw(NOTATION_UPDIAGONALSTRIKE) ||
      IsToDraw(NOTATION_DOWNDIAGONALSTRIKE) ||
      IsToDraw(NOTATION_VERTICALSTRIKE) ||
      IsToDraw(NOTATION_CIRCLE) ||
      IsToDraw(NOTATION_ROUNDEDBOX))
    mBoundingMetrics.descent = aDesiredSize.height - aDesiredSize.ascent;

  aDesiredSize.mBoundingMetrics = mBoundingMetrics;
  
  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  if (aPlaceOrigin) {
    //////////////////
    // Set position and size of MathMLChars
    if (IsToDraw(NOTATION_LONGDIV))
      mMathMLChar[mLongDivCharIndex].SetRect(nsRect(dx_left -
                                                    bmLongdivChar.width,
                                                    aDesiredSize.ascent -
                                                    longdivAscent,
                                                    bmLongdivChar.width,
                                                    bmLongdivChar.ascent +
                                                    bmLongdivChar.descent));

    if (IsToDraw(NOTATION_RADICAL))
      mMathMLChar[mRadicalCharIndex].SetRect(nsRect(dx_left -
                                                    bmRadicalChar.width,
                                                    aDesiredSize.ascent -
                                                    radicalAscent,
                                                    bmRadicalChar.width,
                                                    bmRadicalChar.ascent +
                                                    bmRadicalChar.descent));

    mContentWidth = bmBase.width;

    //////////////////
    // Finish reflowing child frames
    PositionRowChildFrames(dx_left, aDesiredSize.ascent);
  }

  return NS_OK;
}

nscoord
nsMathMLmencloseFrame::FixInterFrameSpacing(nsHTMLReflowMetrics& aDesiredSize)
{
  nscoord gap = nsMathMLContainerFrame::FixInterFrameSpacing(aDesiredSize);
  if (!gap)
    return 0;

  // Move the MathML characters
  nsRect rect;
  for (PRUint32 i = 0; i < mMathMLChar.Length(); i++) {
    mMathMLChar[i].GetRect(rect);
    rect.MoveBy(gap, 0);
    mMathMLChar[i].SetRect(rect);
  }

  return gap;
}

NS_IMETHODIMP
nsMathMLmencloseFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                        nsIAtom*        aAttribute,
                                        PRInt32         aModType)
{
  if (aAttribute == nsGkAtoms::notation_) {
    InitNotations();
  }

  return nsMathMLContainerFrame::
    AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

//////////////////
// the Style System will use these to pass the proper style context to our
// MathMLChar
nsStyleContext*
nsMathMLmencloseFrame::GetAdditionalStyleContext(PRInt32 aIndex) const
{
  PRInt32 len = mMathMLChar.Length();
  if (aIndex >= 0 && aIndex < len)
    return mMathMLChar[aIndex].GetStyleContext();
  else
    return nsnull;
}

void
nsMathMLmencloseFrame::SetAdditionalStyleContext(PRInt32          aIndex, 
                                                 nsStyleContext*  aStyleContext)
{
  PRInt32 len = mMathMLChar.Length();
  if (aIndex >= 0 && aIndex < len)
    mMathMLChar[aIndex].SetStyleContext(aStyleContext);
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

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx);
  NS_DISPLAY_DECL_NAME("MathMLMencloseNotation", TYPE_MATHML_MENCLOSE_NOTATION)

private:
  nsRect             mRect;
  nscoord            mThickness;
  nsMencloseNotation mType;
};

void nsDisplayNotation::Paint(nsDisplayListBuilder* aBuilder,
                              nsRenderingContext* aCtx)
{
  // get the gfxRect
  nsPresContext* presContext = mFrame->PresContext();
  gfxRect rect = presContext->AppUnitsToGfxUnits(mRect + ToReferenceFrame());

  // paint the frame with the current text color
  aCtx->SetColor(mFrame->GetStyleColor()->mColor);

  // change line width to mThickness
  gfxContext *gfxCtx = aCtx->ThebesContext();
  gfxFloat currentLineWidth = gfxCtx->CurrentLineWidth();
  gfxFloat e = presContext->AppUnitsToGfxUnits(mThickness);
  gfxCtx->SetLineWidth(e);

  rect.Deflate(e / 2.0);

  gfxCtx->NewPath();

  switch(mType)
    {
    case NOTATION_CIRCLE:
      gfxCtx->Ellipse(rect.Center(), rect.Size());
      break;

    case NOTATION_ROUNDEDBOX:
      gfxCtx->RoundedRectangle(rect, gfxCornerSizes(3 * e), PR_TRUE);
      break;

    case NOTATION_UPDIAGONALSTRIKE:
      gfxCtx->Line(rect.BottomLeft(), rect.TopRight());
      break;

    case NOTATION_DOWNDIAGONALSTRIKE:
      gfxCtx->Line(rect.TopLeft(), rect.BottomRight());
      break;

    default:
      NS_NOTREACHED("This notation can not be drawn using nsDisplayNotation");
      break;
    }

  gfxCtx->Stroke();

  // restore previous line width
  gfxCtx->SetLineWidth(currentLineWidth);
}

nsresult
nsMathMLmencloseFrame::DisplayNotation(nsDisplayListBuilder* aBuilder,
                                       nsIFrame* aFrame, const nsRect& aRect,
                                       const nsDisplayListSet& aLists,
                                       nscoord aThickness,
                                       nsMencloseNotation aType)
{
  if (!aFrame->GetStyleVisibility()->IsVisible() || aRect.IsEmpty() ||
      aThickness <= 0)
    return NS_OK;

  return aLists.Content()->AppendNewToTop(new (aBuilder)
      nsDisplayNotation(aBuilder, aFrame, aRect, aThickness, aType));
}

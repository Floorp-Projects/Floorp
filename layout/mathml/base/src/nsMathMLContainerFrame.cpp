/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is The University Of
 * Queensland.  Portions created by The University Of Queensland are
 * Copyright (C) 1999 The University Of Queensland.  All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *   David J. Fiddes <D.J.Fiddes@hw.ac.uk>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsFrame.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsStyleUtil.h"

#include "nsIDOMText.h"
#include "nsITextContent.h"

#include "nsMathMLAtoms.h"
#include "nsMathMLParts.h"
#include "nsMathMLChar.h"
#include "nsMathMLContainerFrame.h"

//
// nsMathMLContainerFrame implementation
//

// nsISupports
// =============================================================================

NS_IMPL_ADDREF_INHERITED(nsMathMLContainerFrame, nsMathMLFrame)
NS_IMPL_RELEASE_INHERITED(nsMathMLContainerFrame, nsMathMLFrame)
NS_IMPL_QUERY_INTERFACE_INHERITED1(nsMathMLContainerFrame, nsHTMLContainerFrame, nsMathMLFrame)

// =============================================================================

// helper to get an attribute from the content or the surrounding <mstyle> hierarchy
nsresult
nsMathMLContainerFrame::GetAttribute(nsIContent* aContent,
                                     nsIFrame*   aMathMLmstyleFrame,
                                     nsIAtom*    aAttributeAtom,
                                     nsString&   aValue)
{
  nsresult rv = NS_CONTENT_ATTR_NOT_THERE;

  // see if we can get the attribute from the content
  if (aContent) {
    rv = aContent->GetAttr(kNameSpaceID_None, aAttributeAtom, aValue);
  }

  if (NS_CONTENT_ATTR_NOT_THERE == rv) {
    // see if we can get the attribute from the mstyle frame
    if (aMathMLmstyleFrame) {
      nsCOMPtr<nsIContent> mstyleContent;
      aMathMLmstyleFrame->GetContent(getter_AddRefs(mstyleContent));

      nsIFrame* mstyleParent;
      aMathMLmstyleFrame->GetParent(&mstyleParent);

      nsPresentationData mstyleParentData;
      mstyleParentData.mstyle = nsnull;

      if (mstyleParent) {
        nsIMathMLFrame* mathMLFrame;
        rv = mstyleParent->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
        if (NS_SUCCEEDED(rv) && mathMLFrame) {
          mathMLFrame->GetPresentationData(mstyleParentData);
        }
      }

      // recurse all the way up into the <mstyle> hierarchy
      rv = GetAttribute(mstyleContent, mstyleParentData.mstyle, aAttributeAtom, aValue);
    }
  }
  return rv;
}

void
nsMathMLContainerFrame::GetRuleThickness(nsIRenderingContext& aRenderingContext,
                                         nsIFontMetrics*      aFontMetrics,
                                         nscoord&             aRuleThickness)
{
  // get the bounding metrics of the overbar char, the rendering context
  // is assumed to have been set with the font of the current style context
  nscoord xHeight;
  aFontMetrics->GetXHeight(xHeight);
  PRUnichar overBar = 0x00AF;
  nsBoundingMetrics bm;
  nsresult rv = aRenderingContext.GetBoundingMetrics(&overBar, PRUint32(1), bm);
  if (NS_SUCCEEDED(rv)) {
    aRuleThickness = bm.ascent + bm.descent;
  }
  if (NS_FAILED(rv) || aRuleThickness <= 0 || aRuleThickness >= xHeight) {
    // fall-back to the other version
    GetRuleThickness(aFontMetrics, aRuleThickness);
  }

#if 0
  nscoord oldRuleThickness;
  GetRuleThickness(aFontMetrics, oldRuleThickness);

  PRUnichar sqrt = 0xE063; // a sqrt glyph from TeX's CMEX font
  rv = aRenderingContext.GetBoundingMetrics(&sqrt, PRUint32(1), bm);
  nscoord sqrtrule = bm.ascent; // according to TeX, the ascent should be the rule

  printf("xheight:%4d rule:%4d oldrule:%4d  sqrtrule:%4d\n",
          xHeight, aRuleThickness, oldRuleThickness, sqrtrule);
#endif
}

void
nsMathMLContainerFrame::GetAxisHeight(nsIRenderingContext& aRenderingContext,
                                     nsIFontMetrics*       aFontMetrics,
                                     nscoord&              aAxisHeight)
{
  // get the bounding metrics of the minus sign, the rendering context
  // is assumed to have been set with the font of the current style context
  nscoord xHeight;
  aFontMetrics->GetXHeight(xHeight);
  PRUnichar minus = '-';
  nsBoundingMetrics bm;
  nsresult rv = aRenderingContext.GetBoundingMetrics(&minus, PRUint32(1), bm);
  if (NS_SUCCEEDED(rv)) {
    aAxisHeight = bm.ascent - (bm.ascent + bm.descent)/2;
  }
  if (NS_FAILED(rv) || aAxisHeight <= 0 || aAxisHeight >= xHeight) {
    // fall-back to the other version
    GetAxisHeight(aFontMetrics, aAxisHeight);
  }

#if 0
  nscoord oldAxis;
  GetAxisHeight(aFontMetrics, oldAxis);

  PRUnichar plus = '+';
  rv = aRenderingContext.GetBoundingMetrics(&plus, PRUint32(1), bm);
  nscoord plusAxis = bm.ascent - (bm.ascent + bm.descent)/2;;

  printf("xheight:%4d Axis:%4d oldAxis:%4d  plusAxis:%4d\n",
          xHeight, aAxisHeight, oldAxis, plusAxis);
#endif
}

// ================
// Utilities for parsing and retrieving numeric values
// All returned values are in twips.

/*
The REC says:
  An explicit plus sign ('+') is not allowed as part of a numeric value
  except when it is specifically listed in the syntax (as a quoted '+'  or "+"),

  Units allowed
  ID  Description
  em  ems (font-relative unit traditionally used for horizontal lengths)
  ex  exs (font-relative unit traditionally used for vertical lengths)
  px  pixels, or pixel size of a "typical computer display"
  in  inches (1 inch = 2.54 centimeters)
  cm  centimeters
  mm  millimeters
  pt  points (1 point = 1/72 inch)
  pc  picas (1 pica = 12 points)
  %   percentage of default value

Implementation here:
  The numeric value is valid only if it is of the form nnn.nnn [h/v-unit]
*/

// Adapted from nsCSSScanner.cpp & CSSParser.cpp
PRBool
nsMathMLContainerFrame::ParseNumericValue(nsString&   aString,
                                          nsCSSValue& aCSSValue)
{
  aCSSValue.Reset();
  aString.CompressWhitespace(); //  aString is not a const in this code...

  PRInt32 stringLength = aString.Length();

  if (!stringLength) return PR_FALSE;

  nsAutoString number(aString);
  number.SetLength(0);

  nsAutoString unit(aString);
  unit.SetLength(0);

  // Gather up characters that make up the number
  PRBool gotDot = PR_FALSE;
  PRUnichar c;
  for (PRInt32 i = 0; i < stringLength; i++) {
    c = aString[i];
    if (gotDot && c == '.')
      return PR_FALSE;  // two dots encountered
    else if (c == '.')
      gotDot = PR_TRUE;
    else if (!nsCRT::IsAsciiDigit(c)) {
      aString.Right(unit, stringLength - i);
      unit.CompressWhitespace(); // some authors leave blanks before the unit
      break;
    }
    number.Append(c);
  }

#if 0
char s1[50], s2[50], s3[50];
aString.ToCString(s1, 50);
number.ToCString(s2, 50);
unit.ToCString(s3, 50);
printf("String:%s,  Number:%s,  Unit:%s\n", s1, s2, s3);
#endif

  // Convert number to floating point
  PRInt32 errorCode;
  float floatValue = number.ToFloat(&errorCode);
  if (NS_FAILED(errorCode)) return PR_FALSE;

  nsCSSUnit cssUnit;
  if (0 == unit.Length()) {
    cssUnit = eCSSUnit_Number; // no explicit unit, this is a number that will act as a multiplier
  }
  else if (unit.EqualsWithConversion("%")) {
    floatValue = floatValue / 100.0f;
    aCSSValue.SetPercentValue(floatValue);
    return PR_TRUE;
  }
  else if (unit.EqualsWithConversion("em")) cssUnit = eCSSUnit_EM;
  else if (unit.EqualsWithConversion("ex")) cssUnit = eCSSUnit_XHeight;
  else if (unit.EqualsWithConversion("px")) cssUnit = eCSSUnit_Pixel;
  else if (unit.EqualsWithConversion("in")) cssUnit = eCSSUnit_Inch;
  else if (unit.EqualsWithConversion("cm")) cssUnit = eCSSUnit_Centimeter;
  else if (unit.EqualsWithConversion("mm")) cssUnit = eCSSUnit_Millimeter;
  else if (unit.EqualsWithConversion("pt")) cssUnit = eCSSUnit_Point;
  else if (unit.EqualsWithConversion("pc")) cssUnit = eCSSUnit_Pica;
  else // unexpected unit
    return PR_FALSE;

  aCSSValue.SetFloatValue(floatValue, cssUnit);
  return PR_TRUE;
}

// Adapted from nsCSSStyleRule.cpp
nscoord
nsMathMLContainerFrame::CalcLength(nsIPresContext*   aPresContext,
                                   nsIStyleContext*  aStyleContext,
                                   const nsCSSValue& aCSSValue)
{
  NS_ASSERTION(aCSSValue.IsLengthUnit(), "not a length unit");

  if (aCSSValue.IsFixedLengthUnit()) {
    return aCSSValue.GetLengthTwips();
  }

  nsCSSUnit unit = aCSSValue.GetUnit();

  if (eCSSUnit_Pixel == unit) {
    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    return NSFloatPixelsToTwips(aCSSValue.GetFloatValue(), p2t);
  }
  else if (eCSSUnit_EM == unit) {
    const nsStyleFont *font = NS_STATIC_CAST(const nsStyleFont*,
      aStyleContext->GetStyleData(eStyleStruct_Font));
    return NSToCoordRound(aCSSValue.GetFloatValue() * (float)font->mFont.size);
  }
  else if (eCSSUnit_XHeight == unit) {
    nscoord xHeight;
    const nsStyleFont *font = NS_STATIC_CAST(const nsStyleFont*,
      aStyleContext->GetStyleData(eStyleStruct_Font));
    nsCOMPtr<nsIFontMetrics> fm;
    aPresContext->GetMetricsFor(font->mFont, getter_AddRefs(fm));
    fm->GetXHeight(xHeight);
    return NSToCoordRound(aCSSValue.GetFloatValue() * (float)xHeight);
  }

  return 0;
}

PRBool
nsMathMLContainerFrame::ParseNamedSpaceValue(nsIFrame*   aMathMLmstyleFrame,
                                             nsString&   aString,
                                             nsCSSValue& aCSSValue)
{
  aCSSValue.Reset();
  aString.CompressWhitespace(); //  aString is not a const in this code...
  if (!aString.Length()) return PR_FALSE;

  // See if it is one of the 'namedspace' (ranging 1/18em...7/18em)
  PRInt32 i = 0;
  nsIAtom* namedspaceAtom;
  if (aString.EqualsWithConversion("veryverythinmathspace")) {
    i = 1;
    namedspaceAtom = nsMathMLAtoms::veryverythinmathspace_;
  }
  else if (aString.EqualsWithConversion("verythinmathspace")) {
    i = 2;
    namedspaceAtom = nsMathMLAtoms::verythinmathspace_;
  }
  else if (aString.EqualsWithConversion("thinmathspace")) {
    i = 3;
    namedspaceAtom = nsMathMLAtoms::thinmathspace_;
  }
  else if (aString.EqualsWithConversion("mediummathspace")) {
    i = 4;
    namedspaceAtom = nsMathMLAtoms::mediummathspace_;
  }
  else if (aString.EqualsWithConversion("thickmathspace")) {
    i = 5;
    namedspaceAtom = nsMathMLAtoms::thickmathspace_;
  }
  else if (aString.EqualsWithConversion("verythickmathspace")) {
    i = 6;
    namedspaceAtom = nsMathMLAtoms::verythickmathspace_;
  }
  else if (aString.EqualsWithConversion("veryverythickmathspace")) {
    i = 7;
    namedspaceAtom = nsMathMLAtoms::veryverythickmathspace_;
  }

  if (0 != i) {
    if (aMathMLmstyleFrame) {
      // see if there is a <mstyle> that has overriden the default value
      // GetAttribute() will recurse all the way up into the <mstyle> hierarchy
      nsAutoString value;
      if (NS_CONTENT_ATTR_HAS_VALUE ==
          GetAttribute(nsnull, aMathMLmstyleFrame, namedspaceAtom, value)) {
        if (ParseNumericValue(value, aCSSValue) &&
            aCSSValue.IsLengthUnit()) {
          return PR_TRUE;
        }
      }
    }

    // fall back to the default value
    aCSSValue.SetFloatValue(float(i)/float(18), eCSSUnit_EM);
    return PR_TRUE;
  }

  return PR_FALSE;
}

// -------------------------
// error handlers
// by default show the Unicode REPLACEMENT CHARACTER U+FFFD
// when a frame with bad markup can not be rendered
nsresult
nsMathMLContainerFrame::ReflowError(nsIPresContext*      aPresContext,
                                    nsIRenderingContext& aRenderingContext,
                                    nsHTMLReflowMetrics& aDesiredSize)
{
  nsresult rv;

  // clear all other flags and record that there is an error with this frame
  mEmbellishData.flags = 0;
  mPresentationData.flags = NS_MATHML_ERROR;

  ///////////////
  // Set font
  const nsStyleFont *font = NS_STATIC_CAST(const nsStyleFont*,
    mStyleContext->GetStyleData(eStyleStruct_Font));
  aRenderingContext.SetFont(font->mFont);

  // bounding metrics
  nsAutoString errorMsg(PRUnichar(0xFFFD));
  rv = aRenderingContext.GetBoundingMetrics(errorMsg.get(),
                                            PRUint32(errorMsg.Length()),
                                            mBoundingMetrics);
  if (NS_FAILED(rv)) {
    NS_WARNING("GetBoundingMetrics failed");
    aDesiredSize.width = aDesiredSize.height = 0;
    aDesiredSize.ascent = aDesiredSize.descent = 0;
    return NS_OK;
  }

  // reflow metrics
  nsCOMPtr<nsIFontMetrics> fm;
  aRenderingContext.GetFontMetrics(*getter_AddRefs(fm));
  fm->GetMaxAscent(aDesiredSize.ascent);
  fm->GetMaxDescent(aDesiredSize.descent);
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.width = mBoundingMetrics.width;

  if (aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
  // Also return our bounding metrics
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  return NS_OK;
}

nsresult
nsMathMLContainerFrame::PaintError(nsIPresContext*      aPresContext,
                                   nsIRenderingContext& aRenderingContext,
                                   const nsRect&        aDirtyRect,
                                   nsFramePaintLayer    aWhichLayer)
{
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer)
  {
    NS_ASSERTION(NS_MATHML_HAS_ERROR(mPresentationData.flags),
                 "There is nothing wrong with this frame!");
    // Set color and font ...
    const nsStyleFont *font = NS_STATIC_CAST(const nsStyleFont*,
      mStyleContext->GetStyleData(eStyleStruct_Font));
    const nsStyleColor *color = NS_STATIC_CAST(const nsStyleColor*,
      mStyleContext->GetStyleData(eStyleStruct_Color));
    aRenderingContext.SetColor(color->mColor);
    aRenderingContext.SetFont(font->mFont);

    nscoord ascent;
    nsCOMPtr<nsIFontMetrics> fm;
    aRenderingContext.GetFontMetrics(*getter_AddRefs(fm));
    fm->GetMaxAscent(ascent);

    nsAutoString errorMsg(PRUnichar(0xFFFD));
    aRenderingContext.DrawString(errorMsg.get(),
                                 PRUint32(errorMsg.Length()),
                                 mRect.x, mRect.y + ascent);
  }
  return NS_OK;
}


/* /////////////
 * nsIMathMLFrame - support methods for precise positioning
 * =============================================================================
 */

// helper method to facilitate getting the reflow and bounding metrics
void
nsMathMLContainerFrame::GetReflowAndBoundingMetricsFor(nsIFrame*            aFrame,
                                                       nsHTMLReflowMetrics& aReflowMetrics,
                                                       nsBoundingMetrics&   aBoundingMetrics)
{
  NS_PRECONDITION(aFrame, "null arg");

  // IMPORTANT: This function is only meant to be called in Place() methods
  // where it is assumed that the frame's rect is still acting as place holder
  // for the frame's ascent and descent information

  nsRect rect;
  aFrame->GetRect(rect);
  aReflowMetrics.descent = rect.x;
  aReflowMetrics.ascent  = rect.y;
  aReflowMetrics.width   = rect.width;
  aReflowMetrics.height  = rect.height;

  aBoundingMetrics.Clear();
  nsIMathMLFrame* mathMLFrame;
  nsresult rv = aFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
  if (NS_SUCCEEDED(rv) && mathMLFrame) {
    mathMLFrame->GetBoundingMetrics(aBoundingMetrics);
  }
  else { // aFrame is not a MathML frame, just return the reflow metrics
    aBoundingMetrics.descent = aReflowMetrics.descent;
    aBoundingMetrics.ascent  = aReflowMetrics.ascent;
    aBoundingMetrics.width   = aReflowMetrics.width;
    aBoundingMetrics.rightBearing = aReflowMetrics.width;
  }
}

/* /////////////
 * nsIMathMLFrame - support methods for stretchy elements
 * =============================================================================
 */

NS_IMETHODIMP
nsMathMLContainerFrame::Stretch(nsIPresContext*      aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                nsStretchDirection   aStretchDirection,
                                nsBoundingMetrics&   aContainerSize,
                                nsHTMLReflowMetrics& aDesiredStretchSize)
{
  nsresult rv = NS_OK;
  if (NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags)) {

    if (NS_MATHML_STRETCH_WAS_DONE(mEmbellishData.flags)) {
      NS_WARNING("it is wrong to fire stretch more than once on a frame");
      return NS_OK;
    }
    mEmbellishData.flags |= NS_MATHML_STRETCH_DONE;

    if (NS_MATHML_HAS_ERROR(mPresentationData.flags)) {
      NS_WARNING("it is wrong to fire stretch on a erroneous frame");
      return NS_OK;
    }

    // Pass the stretch to the first non-empty child ...

    nsIFrame* childFrame = mEmbellishData.firstChild;
    NS_ASSERTION(childFrame, "Something is wrong somewhere");

    if (childFrame) {
      nsIMathMLFrame* mathMLFrame;
      rv = childFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
      NS_ASSERTION(NS_SUCCEEDED(rv) && mathMLFrame, "Something is wrong somewhere");
      if (NS_SUCCEEDED(rv) && mathMLFrame) {

        // And the trick is that the child's rect.x is still holding the descent,
        // and rect.y is still holding the ascent ...
        nsHTMLReflowMetrics childSize(aDesiredStretchSize);
        GetReflowAndBoundingMetricsFor(childFrame, childSize, childSize.mBoundingMetrics);

        nsBoundingMetrics containerSize = aContainerSize;

        if (aStretchDirection != NS_STRETCH_DIRECTION_DEFAULT &&
            aStretchDirection != mEmbellishData.direction) {
          // change the direction and confine the stretch to us
          // XXX tune this
          containerSize = mBoundingMetrics;
        }

        // do the stretching...
        mathMLFrame->Stretch(aPresContext, aRenderingContext,
                             mEmbellishData.direction, containerSize, childSize);

        // store the updated metrics
        childFrame->SetRect(aPresContext,
                            nsRect(childSize.descent, childSize.ascent,
                                   childSize.width, childSize.height));

        // We now have one child that may have changed, re-position all our children
        Place(aPresContext, aRenderingContext, PR_TRUE, aDesiredStretchSize);

        // If our parent is not embellished, it means we are the outermost embellished
        // container and so we put the spacing, otherwise we don't include the spacing,
        // the outermost embellished container will take care of it.

        if (!IsEmbellishOperator(mParent)) {

          const nsStyleFont *font = NS_STATIC_CAST(const nsStyleFont*,
            mStyleContext->GetStyleData(eStyleStruct_Font));
          nscoord em = NSToCoordRound(float(font->mFont.size));

          nsEmbellishData coreData;
          mEmbellishData.core->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
          mathMLFrame->GetEmbellishData(coreData);

          // cache these values
          mEmbellishData.leftSpace = coreData.leftSpace;
          mEmbellishData.rightSpace = coreData.rightSpace;

          aDesiredStretchSize.width +=
            NSToCoordRound((coreData.leftSpace + coreData.rightSpace) * em);

// XXX is this what to do ?
          aDesiredStretchSize.mBoundingMetrics.width +=
            NSToCoordRound((coreData.leftSpace + coreData.rightSpace) * em);

          nscoord dx = nscoord( coreData.leftSpace * em );
          if (!dx) return NS_OK;

          aDesiredStretchSize.mBoundingMetrics.leftBearing += dx;
          aDesiredStretchSize.mBoundingMetrics.rightBearing += dx;

          nsPoint origin;
          childFrame = mFrames.FirstChild();
          while (childFrame) {
            childFrame->GetOrigin(origin);
            childFrame->MoveTo(aPresContext, origin.x + dx, origin.y);
            childFrame->GetNextSibling(&childFrame);
          }
        }
      }
    }
  }
  return NS_OK;
}

nsresult
nsMathMLContainerFrame::FinalizeReflow(nsIPresContext*      aPresContext,
                                       nsIRenderingContext& aRenderingContext,
                                       nsHTMLReflowMetrics& aDesiredSize)
{
  // During reflow, we use rect.x and rect.y as placeholders for the child's ascent
  // and descent in expectation of a stretch command. Hence we need to ensure that
  // a stretch command will actually be fired later on, after exiting from our
  // reflow. If the stretch is not fired, the rect.x, and rect.y will remain
  // with inappropriate data causing children to be improperly positioned.
  // This helper method checks to see if our parent will fire a stretch command
  // targeted at us. If not, we go ahead and fire an involutive stretch on
  // ourselves. This will clear all the rect.x and rect.y, and return our
  // desired size.


  // First, complete the post-reflow hook.
  // We use the information in our children rectangles to position them.
  // If placeOrigin==false, then Place() will not touch rect.x, and rect.y.
  // They will still be holding the ascent and descent for each child.
  PRBool placeOrigin = !NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags);
  Place(aPresContext, aRenderingContext, placeOrigin, aDesiredSize);

  if (!placeOrigin) {
    // This means the rect.x and rect.y of our children were not set!!
    // Don't go without checking to see if our parent will later fire a Stretch() command
    // targeted at us. The Stretch() will cause the rect.x and rect.y to clear...
    PRBool parentWillFireStretch = PR_FALSE;
    nsEmbellishData parentData;
    nsIMathMLFrame* mathMLFrame;
    nsresult rv = mParent->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
    if (NS_SUCCEEDED(rv) && mathMLFrame) {
      mathMLFrame->GetEmbellishData(parentData);
      if (NS_MATHML_WILL_STRETCH_ALL_CHILDREN_VERTICALLY(parentData.flags) ||
          NS_MATHML_WILL_STRETCH_ALL_CHILDREN_HORIZONTALLY(parentData.flags) ||
          (NS_MATHML_IS_EMBELLISH_OPERATOR(parentData.flags)
            && parentData.firstChild == this))
      {
        parentWillFireStretch = PR_TRUE;
      }
    }
    if (!parentWillFireStretch) {
      // There is nobody who will fire the stretch for us, we do it ourselves!

       // BEGIN of GETTING THE STRETCH SIZE
       // What is the size that we should use to stretch our stretchy children ????

// 1) With this code, vertical stretching works. But horizontal stretching
// does not work when the firstChild happens to be the core embellished mo...
//      nsRect rect;
//      nsIFrame* childFrame = mEmbellishData.firstChild;
//      NS_ASSERTION(childFrame, "Something is wrong somewhere");
//      childFrame->GetRect(rect);
//      nsStretchMetrics curSize(rect.x, rect.y, rect.width, rect.height);


// 2) With this code, horizontal stretching works. But vertical stretching
// is done in some cases where frames could have simply been kept as is.
//      nsStretchMetrics curSize(aDesiredSize);
      nsBoundingMetrics curSize = mBoundingMetrics;


// 3) With this code, we should get appropriate size when it is done !!
//      GetDesiredSize(aDirection, aPresContext, aRenderingContext, curSize);

      // XXX It is not clear if a direction should be imposed.
      // With the default direction, the MathMLChar will attempt to stretch
      // in its preferred direction.

      Stretch(aPresContext, aRenderingContext, NS_STRETCH_DIRECTION_DEFAULT,
              curSize, aDesiredSize);
    }
  }
  if (aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
  // Also return our bounding metrics
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  // see if we should fix the spacing
  FixInterFrameSpacing(aPresContext, aDesiredSize);

  return NS_OK;
}

// This is the method used to set the frame as an embellished container.
// It checks if the first (non-empty) child is embellished. Hence, calls
// must be bottom-up. The method must only be called from within frames who are
// entitled to be potential embellished operators as per the MathML REC.
NS_IMETHODIMP
nsMathMLContainerFrame::EmbellishOperator()
{
  nsIFrame* firstChild = mFrames.FirstChild();
  if (firstChild && IsEmbellishOperator(firstChild)) {
    // Cache the first child
    mEmbellishData.flags |= NS_MATHML_EMBELLISH_OPERATOR;
    mEmbellishData.firstChild = firstChild;
    // Cache also the inner-most embellished frame at the core of the hierarchy
    nsIMathMLFrame* mathMLFrame;
    firstChild->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
    nsEmbellishData embellishData;
    mathMLFrame->GetEmbellishData(embellishData);
    mEmbellishData.core = embellishData.core;
    mEmbellishData.direction = embellishData.direction;
  }
  else {
    mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_OPERATOR;
    mEmbellishData.firstChild = nsnull;
    mEmbellishData.core = nsnull;
    mEmbellishData.direction = NS_STRETCH_DIRECTION_UNSUPPORTED;
  }
  return NS_OK;
}

PRBool
nsMathMLContainerFrame::IsEmbellishOperator(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null arg");
  if (!aFrame) return PR_FALSE;
  nsIMathMLFrame* mathMLFrame;
  nsresult rv = aFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
  if (NS_FAILED(rv) || !mathMLFrame) return PR_FALSE;
  nsEmbellishData embellishData;
  mathMLFrame->GetEmbellishData(embellishData);
  return NS_MATHML_IS_EMBELLISH_OPERATOR(embellishData.flags);
}

/* /////////////
 * nsIMathMLFrame - support methods for scripting elements (nested frames
 * within msub, msup, msubsup, munder, mover, munderover, mmultiscripts,
 * mfrac, mroot, mtable).
 * =============================================================================
 */

NS_IMETHODIMP
nsMathMLContainerFrame::UpdatePresentationDataFromChildAt(nsIPresContext* aPresContext,
                                                          PRInt32         aFirstIndex,
                                                          PRInt32         aLastIndex,
                                                          PRInt32         aScriptLevelIncrement,
                                                          PRUint32        aFlagsValues,
                                                          PRUint32        aFlagsToUpdate)
{
  PRInt32 index = 0;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if ((index >= aFirstIndex) &&
        ((aLastIndex <= 0) || ((aLastIndex > 0) && (index <= aLastIndex)))) {
      nsIMathMLFrame* mathMLFrame;
      nsresult rv = childFrame->QueryInterface(
        NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
      if (NS_SUCCEEDED(rv) && mathMLFrame) {
        // update
        mathMLFrame->UpdatePresentationData(
          aScriptLevelIncrement, aFlagsValues, aFlagsToUpdate);
        // propagate down the subtrees
        mathMLFrame->UpdatePresentationDataFromChildAt(aPresContext, 0, -1,
          aScriptLevelIncrement, aFlagsValues, aFlagsToUpdate);
      }
    }
    index++;
    childFrame->GetNextSibling(&childFrame);
  }
  return NS_OK;
}

// Helper to give a style context suitable for doing the stretching of
// a MathMLChar. Frame classes that use this should ensure that the 
// extra leaf style contexts given to the MathMLChars are acessible to
// the Style System via the Get/Set AdditionalStyleContext() APIs.
PRBool
nsMathMLContainerFrame::ResolveMathMLCharStyle(nsIPresContext*  aPresContext,
                                               nsIContent*      aContent,
                                               nsIStyleContext* aParentStyleContext,
                                               nsMathMLChar*    aMathMLChar)
{
  nsAutoString data;
  aMathMLChar->GetData(data);
  PRBool isStretchy = nsMathMLOperators::IsMutableOperator(data);
  nsIAtom* fontAtom = (isStretchy) ?
    nsMathMLAtoms::fontstyle_stretchy :
    nsMathMLAtoms::fontstyle_anonymous;
  nsCOMPtr<nsIStyleContext> newStyleContext;
  nsresult rv = aPresContext->ResolvePseudoStyleContextFor(aContent, fontAtom, 
                                             aParentStyleContext, PR_FALSE,
                                             getter_AddRefs(newStyleContext));
  if (NS_SUCCEEDED(rv) && newStyleContext)
    aMathMLChar->SetStyleContext(newStyleContext);

  return isStretchy;
}

NS_IMETHODIMP
nsMathMLContainerFrame::ReResolveScriptStyle(nsIPresContext*  aPresContext,
                                             nsIStyleContext* aParentContext,
                                             PRInt32          aParentScriptLevel)
{
  PRInt32 gap = mPresentationData.scriptLevel - aParentScriptLevel;
  if (gap) {
  	// By default scriptminsize=8pt and scriptsizemultiplier=0.71
    nscoord scriptminsize = NSIntPointsToTwips(NS_MATHML_SCRIPTMINSIZE);
    float scriptsizemultiplier = NS_MATHML_SCRIPTSIZEMULTIPLIER;
#if 0
    // XXX Bug 44201
    // user-supplied scriptminsize and scriptsizemultiplier that are
    // restricted to particular elements are not supported because our
    // css rules are fixed in mathml.css and are applicable to all elements.

    // see if there is a scriptminsize attribute on a <mstyle> that wraps us
    if (NS_CONTENT_ATTR_HAS_VALUE ==
        GetAttribute(nsnull, mPresentationData.mstyle,
                     nsMathMLAtoms::scriptminsize_, fontsize)) {
      nsCSSValue cssValue;
      if (ParseNumericValue(fontsize, cssValue)) {
        nsCSSUnit unit = cssValue.GetUnit();
        if (eCSSUnit_Number == unit)
          scriptminsize = nscoord(float(scriptminsize) * cssValue.GetFloatValue());
        else if (eCSSUnit_Percent == unit)
          scriptminsize = nscoord(float(scriptminsize) * cssValue.GetPercentValue());
        else if (eCSSUnit_Null != unit)
          scriptminsize = CalcLength(aPresContext, mStyleContext, cssValue);
      }
    }
#endif

    // get the incremental factor
    nsAutoString fontsize;
    if (0 > gap) { // the size is going to be increased
      if (gap < NS_MATHML_CSS_NEGATIVE_SCRIPTLEVEL_LIMIT)
        gap = NS_MATHML_CSS_NEGATIVE_SCRIPTLEVEL_LIMIT;
      gap = -gap;
      scriptsizemultiplier = 1.0f / scriptsizemultiplier;
      fontsize.AssignWithConversion("-");
    }
    else { // the size is going to be decreased
      if (gap > NS_MATHML_CSS_POSITIVE_SCRIPTLEVEL_LIMIT)
        gap = NS_MATHML_CSS_POSITIVE_SCRIPTLEVEL_LIMIT;
      fontsize.AssignWithConversion("+");
    }
    fontsize.AppendInt(gap, 10);
    // we want to make sure that the size will stay readable
    const nsStyleFont *font = NS_STATIC_CAST(const nsStyleFont*,
      aParentContext->GetStyleData(eStyleStruct_Font));
    nscoord newFontSize = font->mFont.size;
    while (0 < gap--) {
      newFontSize = (nscoord)((float)(newFontSize) * scriptsizemultiplier);
    }
    if (newFontSize <= scriptminsize) {
      fontsize.AssignWithConversion("scriptminsize");
    }

    // set the -moz-math-font-size attribute without notifying that we want a reflow
    mContent->SetAttr(kNameSpaceID_None, nsMathMLAtoms::fontsize,
                      fontsize, PR_FALSE);
    // then, re-resolve the style contexts in our subtree
    nsCOMPtr<nsIStyleContext> newStyleContext;
    aPresContext->ResolveStyleContextFor(mContent, aParentContext,
                                         PR_FALSE, getter_AddRefs(newStyleContext));
    if (newStyleContext && newStyleContext.get() != mStyleContext) {
      SetStyleContext(aPresContext, newStyleContext);
      nsIFrame* childFrame = mFrames.FirstChild();
      while (childFrame) {
        aPresContext->ReParentStyleContext(childFrame, newStyleContext);
        childFrame->GetNextSibling(&childFrame);
      }
    }
  }

  // let children with different scriptsizes handle that themselves 
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    nsIMathMLFrame* mathMLFrame;
    nsresult res = childFrame->QueryInterface(
      NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
    if (NS_SUCCEEDED(res) && mathMLFrame) {
      mathMLFrame->ReResolveScriptStyle(aPresContext, mStyleContext,
                                        mPresentationData.scriptLevel);
    }
    childFrame->GetNextSibling(&childFrame);
  }
  return NS_OK;
}


/* //////////////////
 * Frame construction
 * =============================================================================
 */

NS_IMETHODIMP
nsMathMLContainerFrame::Paint(nsIPresContext*      aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              const nsRect&        aDirtyRect,
                              nsFramePaintLayer    aWhichLayer,
                              PRUint32             aFlags)
{
  nsresult rv = NS_OK;

  // report an error if something wrong was found in this frame
  if (NS_MATHML_HAS_ERROR(mPresentationData.flags)) {
    return PaintError(aPresContext, aRenderingContext,
                      aDirtyRect, aWhichLayer);
  }

  rv = nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext,
                                   aDirtyRect, aWhichLayer);

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
  // for visual debug
  // ----------------
  // if you want to see your bounding box, make sure to properly fill
  // your mBoundingMetrics and mReference point, and set
  // mPresentationData.flags |= NS_MATHML_SHOW_BOUNDING_METRICS
  // in the Init() of your sub-class

  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer &&
      NS_MATHML_PAINT_BOUNDING_METRICS(mPresentationData.flags))
  {
    aRenderingContext.SetColor(NS_RGB(0,0,255));

    nscoord x = mReference.x + mBoundingMetrics.leftBearing;
    nscoord y = mReference.y - mBoundingMetrics.ascent;
    nscoord w = mBoundingMetrics.rightBearing - mBoundingMetrics.leftBearing;
    nscoord h = mBoundingMetrics.ascent + mBoundingMetrics.descent;

    aRenderingContext.DrawRect(x,y,w,h);
  }
#endif
  return rv;
}

static void
CompressWhitespace(nsIContent* aContent)
{
  nsCOMPtr<nsIAtom> tag;
  aContent->GetTag(*getter_AddRefs(tag));
  if (tag.get() == nsMathMLAtoms::mo_ ||
      tag.get() == nsMathMLAtoms::mi_ ||
      tag.get() == nsMathMLAtoms::mn_ ||
      tag.get() == nsMathMLAtoms::ms_ ||
      tag.get() == nsMathMLAtoms::mtext_) {
    PRInt32 numKids;
    aContent->ChildCount(numKids);
    for (PRInt32 kid = 0; kid < numKids; kid++) {
      nsCOMPtr<nsIContent> kidContent;
      aContent->ChildAt(kid, *getter_AddRefs(kidContent));
      if (kidContent.get()) {       
        nsCOMPtr<nsIDOMText> kidText(do_QueryInterface(kidContent));
        if (kidText.get()) {
          nsCOMPtr<nsITextContent> tc(do_QueryInterface(kidContent));
          if (tc) {
            nsAutoString text;
            tc->CopyText(text);
            text.CompressWhitespace();
            tc->SetText(text, PR_FALSE); // not meant to be used if notify is needed
          }
        }
      }
    }
  }
}

NS_IMETHODIMP
nsMathMLContainerFrame::Init(nsIPresContext*  aPresContext,
                             nsIContent*      aContent,
                             nsIFrame*        aParent,
                             nsIStyleContext* aContext,
                             nsIFrame*        aPrevInFlow)
{
  // leading and trailing whitespace doesn't count -- bug 15402
  // brute force removal for people who do <mi> a </mi> instead of <mi>a</mi>
  // XXX the best fix is to skip these in nsTextFrame
  CompressWhitespace(aContent);

  // let the base class do its Init()
  nsresult rv;
  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // now, if our parent implements the nsIMathMLFrame interface, we inherit
  // its scriptlevel and displaystyle. If the parent later wishes to increment
  // with other values, it will do so in its SetInitialChildList() method.

  nsIMathMLFrame* mathMLFrame;
  nsresult res = aParent->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
  if (NS_SUCCEEDED(res) && mathMLFrame) {
    nsPresentationData parentData;
    mathMLFrame->GetPresentationData(parentData);
    mPresentationData.mstyle = parentData.mstyle;
    mPresentationData.scriptLevel = parentData.scriptLevel;
    if (NS_MATHML_IS_DISPLAYSTYLE(parentData.flags))
      mPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
    else
      mPresentationData.flags &= ~NS_MATHML_DISPLAYSTYLE;
  }
  else {
    // see if our parent has 'display: block'
    // XXX should we restrict this to the top level <math> parent ?
    const nsStyleDisplay* display;
    aParent->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
    if (display->mDisplay == NS_STYLE_DISPLAY_BLOCK) {
      mPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsMathMLContainerFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                            nsIAtom*        aListName,
                                            nsIFrame*       aChildList)
{
  // First, let the base class do its job
  nsresult rv;
  rv = nsHTMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  // Next, since we are an inline frame, and since we are a container, we have to
  // be very careful with the way we treat our children. Things look okay when
  // all of our children are only MathML frames. But there are problems if one of
  // our children happens to be an nsInlineFrame, e.g., from generated content such
  // as :before { content: open-quote } or :after { content: close-quote }
  // The code asserts during reflow (in nsLineLayout::BeginSpan)
  // Also there are problems when our children are hybrid, e.g., from html markups.
  // In short, the nsInlineFrame class expects a number of *invariants* that are not
  // met when we mix things.

  // So what we do here is to wrap children that happen to be nsInlineFrames in
  // anonymous block frames.
  // XXX Question: Do we have to handle Insert/Remove/Append on behalf of
  //     these anonymous blocks?
  //     Note: By construction, our anonymous blocks have only one child.

  nsIFrame* next = mFrames.FirstChild();
  while (next) {
    nsIFrame* child = next;
    next->GetNextSibling(&next);
    nsInlineFrame* inlineFrame = nsnull;
    nsresult res = child->QueryInterface(nsInlineFrame::kInlineFrameCID, (void**)&inlineFrame);
    if (NS_SUCCEEDED(res) && inlineFrame) {
      // create a new anonymous block frame to wrap this child...
      nsCOMPtr<nsIPresShell> shell;
      aPresContext->GetShell(getter_AddRefs(shell));
      nsIFrame* anonymous;
      rv = NS_NewBlockFrame(shell, &anonymous);
      if (NS_FAILED(rv))
        return rv;
      nsCOMPtr<nsIStyleContext> newStyleContext;
      aPresContext->ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::mozAnonymousBlock,
                                                 mStyleContext, PR_FALSE,
                                                 getter_AddRefs(newStyleContext));
      rv = anonymous->Init(aPresContext, mContent, this, newStyleContext, nsnull);
      if (NS_FAILED(rv)) {
        anonymous->Destroy(aPresContext);
        return rv;
      }
      mFrames.ReplaceFrame(this, child, anonymous);
      child->SetParent(anonymous);
      child->SetNextSibling(nsnull);
      aPresContext->ReParentStyleContext(child, newStyleContext);
      anonymous->SetInitialChildList(aPresContext, nsnull, child);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsMathMLContainerFrame::AttributeChanged(nsIPresContext* aPresContext,
                                         nsIContent*     aChild,
                                         PRInt32         aNameSpaceID,
                                         nsIAtom*        aAttribute,
                                         PRInt32         aModType, 
                                         PRInt32         aHint)
{
  nsresult rv = nsHTMLContainerFrame::AttributeChanged(aPresContext, aChild,
                                                       aNameSpaceID, aAttribute, aModType, aHint);
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIPresShell> shell;
  nsCOMPtr<nsIReflowCommand> reflowCmd;
  aPresContext->GetShell(getter_AddRefs(shell));
  rv = NS_NewHTMLReflowCommand(getter_AddRefs(reflowCmd), this,
                               nsIReflowCommand::ContentChanged,
                               nsnull, aAttribute);
  if (NS_SUCCEEDED(rv) && shell) shell->AppendReflowCommand(reflowCmd);
  return rv;
}

// helper function to reflow token elements
// note that mBoundingMetrics is computed here
nsresult
nsMathMLContainerFrame::ReflowTokenFor(nsIFrame*                aFrame,
                                       nsIPresContext*          aPresContext,
                                       nsHTMLReflowMetrics&     aDesiredSize,
                                       const nsHTMLReflowState& aReflowState,
                                       nsReflowStatus&          aStatus)
{
  NS_PRECONDITION(aFrame, "null arg");
  nsresult rv = NS_OK;

  // See if this is an incremental reflow
  if (aReflowState.reason == eReflowReason_Incremental) {
    nsIFrame* targetFrame;
    aReflowState.reflowCommand->GetTarget(targetFrame);
#ifdef MATHML_NOISY_INCREMENTAL_REFLOW
printf("nsMathMLContainerFrame::ReflowTokenFor:IncrementalReflow received by: ");
nsFrame::ListTag(stdout, aFrame);
printf("for target: ");
nsFrame::ListTag(stdout, targetFrame);
printf("\n");
#endif
    if (aFrame == targetFrame) {
    }
    else {
      // Remove the next frame from the reflow path
      nsIFrame* nextFrame;
      aReflowState.reflowCommand->GetNext(nextFrame);
    }
  }

  // initializations needed for empty markup like <mtag></mtag>
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = aDesiredSize.descent = 0;
  aDesiredSize.mBoundingMetrics.Clear();

  // ask our children to compute their bounding metrics
  nsHTMLReflowMetrics childDesiredSize(aDesiredSize.maxElementSize,
                      aDesiredSize.mFlags | NS_REFLOW_CALC_BOUNDING_METRICS);
  nsSize availSize(aReflowState.mComputedWidth, aReflowState.mComputedHeight);
  PRInt32 count = 0;
  nsIFrame* childFrame;
  aFrame->FirstChild(aPresContext, nsnull, &childFrame);
  while (childFrame) {
    nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                       childFrame, availSize);
    rv = NS_STATIC_CAST(nsMathMLContainerFrame*,
                        aFrame)->ReflowChild(childFrame,
                                             aPresContext, childDesiredSize,
                                             childReflowState, aStatus);
    //NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
    if (NS_FAILED(rv)) return rv;

    // origins are used as placeholders to store the child's ascent and descent.
    childFrame->SetRect(aPresContext,
                        nsRect(childDesiredSize.descent, childDesiredSize.ascent,
                               childDesiredSize.width, childDesiredSize.height));
    // compute and cache the bounding metrics
    if (0 == count)
      aDesiredSize.mBoundingMetrics  = childDesiredSize.mBoundingMetrics;
    else
      aDesiredSize.mBoundingMetrics += childDesiredSize.mBoundingMetrics;

    count++;
    childFrame->GetNextSibling(&childFrame);
  }

  // cache the frame's mBoundingMetrics
  NS_STATIC_CAST(nsMathMLContainerFrame*,
                 aFrame)->SetBoundingMetrics(aDesiredSize.mBoundingMetrics);

  // place and size children
  NS_STATIC_CAST(nsMathMLContainerFrame*,
                 aFrame)->FinalizeReflow(aPresContext, *aReflowState.rendContext,
                                         aDesiredSize);
  return NS_OK;
}

// helper function to place token elements
// mBoundingMetrics is computed at the ReflowToken pass, it is
// not computed here because our children may be text frames that
// do not implement the GetBoundingMetrics() interface.
nsresult
nsMathMLContainerFrame::PlaceTokenFor(nsIFrame*            aFrame,
                                      nsIPresContext*      aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      PRBool               aPlaceOrigin,
                                      nsHTMLReflowMetrics& aDesiredSize)
{
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = aDesiredSize.descent = 0;

  nsRect rect;
  nsIFrame* childFrame;
  aFrame->FirstChild(aPresContext, nsnull, &childFrame);
  while (childFrame) {
    childFrame->GetRect(rect);
    aDesiredSize.width += rect.width;
    if (aDesiredSize.descent < rect.x) aDesiredSize.descent = rect.x;
    if (aDesiredSize.ascent < rect.y) aDesiredSize.ascent = rect.y;
    childFrame->GetNextSibling(&childFrame);
  }
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  NS_STATIC_CAST(nsMathMLContainerFrame*,
                 aFrame)->GetBoundingMetrics(aDesiredSize.mBoundingMetrics);

  if (aPlaceOrigin) {
    nscoord dy, dx = 0;
    aFrame->FirstChild(aPresContext, nsnull, &childFrame);
    while (childFrame) {
      childFrame->GetRect(rect);
      nsHTMLReflowMetrics childSize(nsnull);
      childSize.width = rect.width;
      childSize.height = rect.height;

      // place and size the child
      dy = aDesiredSize.ascent - rect.y;
      NS_STATIC_CAST(nsMathMLContainerFrame*,
                     aFrame)->FinishReflowChild(childFrame, aPresContext,
                                                childSize, dx, dy, 0);
      dx += rect.width;
      childFrame->GetNextSibling(&childFrame);
    }
  }

  NS_STATIC_CAST(nsMathMLContainerFrame*,
                 aFrame)->SetReference(nsPoint(0, aDesiredSize.ascent));

  return NS_OK;
}

// We are an inline frame, so we handle dirty request like nsInlineFrame
NS_IMETHODIMP
nsMathMLContainerFrame::ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild)
{
  // The inline container frame does not handle the reflow
  // request.  It passes it up to its parent container.

  // If you don't already have dirty children,
  if (!(mState & NS_FRAME_HAS_DIRTY_CHILDREN)) {
    if (mParent) {
      // Record that you are dirty and have dirty children
      mState |= NS_FRAME_IS_DIRTY;
      mState |= NS_FRAME_HAS_DIRTY_CHILDREN;

      // Pass the reflow request up to the parent
      mParent->ReflowDirtyChild(aPresShell, (nsIFrame*) this);
    }
    else {
      NS_ASSERTION(0, "No parent to pass the reflow request up to.");
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::Reflow(nsIPresContext*          aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus)
{
  nsresult rv;
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = aDesiredSize.descent = 0;
  aDesiredSize.mBoundingMetrics.Clear();

  // See if this is an incremental reflow
  if (aReflowState.reason == eReflowReason_Incremental) {
    nsIFrame* targetFrame;
    aReflowState.reflowCommand->GetTarget(targetFrame);
#ifdef MATHML_NOISY_INCREMENTAL_REFLOW
printf("nsMathMLContainerFrame::Reflow:IncrementalReflow received by: ");
nsFrame::ListTag(stdout, this);
printf("for target: ");
nsFrame::ListTag(stdout, targetFrame);
printf("\n");
#endif
    if (this == targetFrame) {
      // XXX We are the target of the incremental reflow.
      // Rather than reflowing everything, see if we can speedup things
      // by just doing the minimal work needed to update ourselves
    }
    else {
      // Remove the next frame from the reflow path
      nsIFrame* nextFrame;
      aReflowState.reflowCommand->GetNext(nextFrame);
    }
  }

  /////////////
  // Reflow children
  // Asking each child to cache its bounding metrics

  nsReflowStatus childStatus;
  nsSize availSize(aReflowState.mComputedWidth, aReflowState.mComputedHeight);
  nsHTMLReflowMetrics childDesiredSize(aDesiredSize.maxElementSize,
                      aDesiredSize.mFlags | NS_REFLOW_CALC_BOUNDING_METRICS);
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                       childFrame, availSize);
    rv = ReflowChild(childFrame, aPresContext, childDesiredSize,
                     childReflowState, childStatus);
    //NS_ASSERTION(NS_FRAME_IS_COMPLETE(childStatus), "bad status");
    if (NS_FAILED(rv)) return rv;

    // At this stage, the origin points of the children have no use, so we will use the
    // origins as placeholders to store the child's ascent and descent. Later on,
    // we should set the origins so as to overwrite what we are storing there now.
    childFrame->SetRect(aPresContext,
                        nsRect(childDesiredSize.descent, childDesiredSize.ascent,
                               childDesiredSize.width, childDesiredSize.height));
    childFrame->GetNextSibling(&childFrame);
  }

  /////////////
  // If we are a container which is entitled to stretch its children, then we
  // ask our stretchy children to stretch themselves

  if (NS_MATHML_WILL_STRETCH_ALL_CHILDREN_VERTICALLY(mEmbellishData.flags) ||
      NS_MATHML_WILL_STRETCH_ALL_CHILDREN_HORIZONTALLY(mEmbellishData.flags)) {

    // get our tentative bounding metrics using Place()
    Place(aPresContext, *aReflowState.rendContext, PR_FALSE, aDesiredSize);

    // What size should we use to stretch our stretchy children
    // XXX tune this
    nsBoundingMetrics containerSize = mBoundingMetrics;

    // get the strech direction
    nsStretchDirection stretchDir =
      NS_MATHML_WILL_STRETCH_ALL_CHILDREN_VERTICALLY(mEmbellishData.flags) ?
        NS_STRETCH_DIRECTION_VERTICAL : NS_STRETCH_DIRECTION_HORIZONTAL;

    // fire the stretch on each child
    childFrame = mFrames.FirstChild();
    while (childFrame) {
      if (mEmbellishData.firstChild == childFrame) {
        // skip this child... because:
        // If we are here it means we are an embellished container and
        // for now, we don't touch our embellished child frame.
        // Its stretch will be handled separatedly when we receive
        // the stretch command fired by our parent frame.
      }
      else {
        nsIMathMLFrame* mathMLFrame;
        rv = childFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
        if (NS_SUCCEEDED(rv) && mathMLFrame) {
          // retrieve the metrics that was stored at the previous pass
          GetReflowAndBoundingMetricsFor(childFrame, 
            childDesiredSize, childDesiredSize.mBoundingMetrics);

          mathMLFrame->Stretch(aPresContext, *aReflowState.rendContext,
                               stretchDir, containerSize, childDesiredSize);
          // store the updated metrics
          childFrame->SetRect(aPresContext,
                              nsRect(childDesiredSize.descent, childDesiredSize.ascent,
                                     childDesiredSize.width, childDesiredSize.height));
        }
      }
      childFrame->GetNextSibling(&childFrame);
    }
  }

  /////////////
  // Place children now by re-adjusting the origins to align the baselines
  FinalizeReflow(aPresContext, *aReflowState.rendContext, aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

// For MathML, the 'type' will be used to determine the spacing between frames
// Subclasses can override this method to return a 'type' that will give
// them a particular spacing
NS_IMETHODIMP
nsMathMLContainerFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  // see if this is an embellished operator (mapped to 'Op' in TeX)
  if (NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags)) {
    *aType = nsMathMLAtoms::operatorMathMLFrame;
  }
  else {
    nsCOMPtr<nsIAtom> tag;
    mContent->GetTag(*getter_AddRefs(tag));
    // see if this a token element (mapped to 'Ord'in TeX)
    if (tag.get() == nsMathMLAtoms::mi_ ||
        tag.get() == nsMathMLAtoms::mn_ ||
        tag.get() == nsMathMLAtoms::ms_ ||
        tag.get() == nsMathMLAtoms::mtext_) {
      *aType = nsMathMLAtoms::ordinaryMathMLFrame;
    }
    else {
      // everything else is a schematta element (mapped to 'Inner' in TeX)
      *aType = nsMathMLAtoms::schemataMathMLFrame;
    }
  }
  NS_ADDREF(*aType);
  return NS_OK;
}

enum nsMathMLFrameTypeEnum {
  eMathMLFrameType_UNKNOWN = -1,
  eMathMLFrameType_Ordinary,
  eMathMLFrameType_Operator,
  eMathMLFrameType_Punctuation,
  eMathMLFrameType_Inner
};

// see spacing table in Chapter 18, TeXBook (p.170)
static PRInt32 interFrameSpacingTable[4][4] =
{
  // in units of muspace.
  // upper half of the byte is set if the
  // spacing is not to be used for scriptlevel > 0
  /*          Ord   Op    Punc  Inner */
  /*Ord  */  {0x01, 0x00, 0x00, 0x01},
  /*Op   */  {0x00, 0x00, 0x00, 0x00},
  /*Punc */  {0x11, 0x00, 0x11, 0x11},
  /*Inner*/  {0x01, 0x00, 0x11, 0x01}
};

// XXX more tuning of the result as in TeX, maybe depending on fence:true, etc
static nscoord
GetInterFrameSpacing(PRInt32  aScriptLevel,
                     nsIAtom* aFirstFrameType,
                     nsIAtom* aSecondFrameType)
{
  nsMathMLFrameTypeEnum firstType = eMathMLFrameType_UNKNOWN;
  nsMathMLFrameTypeEnum secondType = eMathMLFrameType_UNKNOWN;

  // do the mapping for the first frame
  if (aFirstFrameType == nsMathMLAtoms::ordinaryMathMLFrame)
    firstType = eMathMLFrameType_Ordinary;
  else if (aFirstFrameType == nsMathMLAtoms::operatorMathMLFrame)
    firstType = eMathMLFrameType_Operator;
  else if (aFirstFrameType == nsMathMLAtoms::schemataMathMLFrame)
    firstType = eMathMLFrameType_Inner;

  // do the mapping for the second frame
  if (aSecondFrameType == nsMathMLAtoms::ordinaryMathMLFrame)
    secondType = eMathMLFrameType_Ordinary;
  else if (aSecondFrameType == nsMathMLAtoms::operatorMathMLFrame)
    secondType = eMathMLFrameType_Operator;
  else if (aSecondFrameType == nsMathMLAtoms::schemataMathMLFrame)
    secondType = eMathMLFrameType_Inner;

  // return 0 if there is a frame that we know nothing about
  if (firstType == eMathMLFrameType_UNKNOWN ||
      secondType == eMathMLFrameType_UNKNOWN) {
    return 0;
  }

  PRInt32 space = interFrameSpacingTable[firstType][secondType];
  if (aScriptLevel > 0 && (space & 0xF0)) {
    // spacing is disabled
    return 0;
  }
  else {
    return (space & 0x0F);
  }
}

NS_IMETHODIMP
nsMathMLContainerFrame::Place(nsIPresContext*      aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              PRBool               aPlaceOrigin,
                              nsHTMLReflowMetrics& aDesiredSize)
{
  // these are needed in case this frame is empty (i.e., we don't enter the loop)
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = aDesiredSize.descent = 0;
  mBoundingMetrics.Clear();

  // cache away thinspace
  const nsStyleFont *font = NS_STATIC_CAST(const nsStyleFont*,
    mStyleContext->GetStyleData(eStyleStruct_Font));
  nscoord thinSpace = NSToCoordRound(float(font->mFont.size)*float(3) / float(18));

  PRInt32 count = 0;
  nsHTMLReflowMetrics childSize (nsnull);
  nsBoundingMetrics bmChild;
  nscoord leftCorrection = 0, italicCorrection = 0;
  nsCOMPtr<nsIAtom> prevFrameType;

  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    nsCOMPtr<nsIAtom> childFrameType;
    childFrame->GetFrameType(getter_AddRefs(childFrameType));
    GetReflowAndBoundingMetricsFor(childFrame, childSize, bmChild);
    GetItalicCorrection(bmChild, leftCorrection, italicCorrection);
    if (0 == count) {
      aDesiredSize.ascent = childSize.ascent;
      aDesiredSize.descent = childSize.descent;
      mBoundingMetrics = bmChild;
      // update to include the left correction
      mBoundingMetrics.leftBearing += leftCorrection;
    }
    else {
      if (aDesiredSize.descent < childSize.descent)
        aDesiredSize.descent = childSize.descent;
      if (aDesiredSize.ascent < childSize.ascent)
        aDesiredSize.ascent = childSize.ascent;
      // add inter frame spacing
      nscoord space = GetInterFrameSpacing(mPresentationData.scriptLevel,
        prevFrameType, childFrameType);
      mBoundingMetrics.width += space * thinSpace;
      // add the child size
      mBoundingMetrics += bmChild;
    }
    count++;
    prevFrameType = childFrameType;
    // add left correction -- this fixes the problem of the italic 'f'
    // e.g., <mo>q</mo> <mi>f</mi> <mo>I</mo> 
    mBoundingMetrics.width += leftCorrection;
    mBoundingMetrics.rightBearing += leftCorrection;
    // add the italic correction at the end (including the last child).
    // this gives a nice gap between math and non-math frames, and still
    // gives the same math inter-spacing in case this frame connects to
    // another math frame
    mBoundingMetrics.width += italicCorrection;

    childFrame->GetNextSibling(&childFrame);
  }
  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  //////////////////
  // Place Children

  if (aPlaceOrigin) {
    count = 0;
    nscoord dx = 0, dy = 0;
    italicCorrection = 0;
    childFrame = mFrames.FirstChild();
    while (childFrame) {
      nsCOMPtr<nsIAtom> childFrameType;
      childFrame->GetFrameType(getter_AddRefs(childFrameType));
      GetReflowAndBoundingMetricsFor(childFrame, childSize, bmChild);
      dy = aDesiredSize.ascent - childSize.ascent;
      if (0 < count) {
        // add inter frame spacing
        nscoord space = GetInterFrameSpacing(mPresentationData.scriptLevel,
          prevFrameType, childFrameType);
        dx += space * thinSpace;
      }
      count++;
      prevFrameType = childFrameType;
      GetItalicCorrection(bmChild, leftCorrection, italicCorrection);
      // add left correction
      dx += leftCorrection;
      FinishReflowChild(childFrame, aPresContext, childSize, dx, dy, 0);
      // add child size + italic correction
      dx += bmChild.width + italicCorrection;
      childFrame->GetNextSibling(&childFrame);
    }
  }

  return NS_OK;
}

// helper to fix the inter-spacing when <math> is the only parent
// e.g., it fixes <math> <mi>f</mi> <mo>q</mo> <mi>f</mi> <mo>I</mo> </math>
nsresult
nsMathMLContainerFrame::FixInterFrameSpacing(nsIPresContext*      aPresContext,
                                             nsHTMLReflowMetrics& aDesiredSize)
{
  nsCOMPtr<nsIAtom> parentTag;
  nsCOMPtr<nsIContent> parentContent;
  mParent->GetContent(getter_AddRefs(parentContent));
  parentContent->GetTag(*getter_AddRefs(parentTag));
  if (parentTag.get() == nsMathMLAtoms::math) {
    nscoord gap = 0;
    nscoord leftCorrection, italicCorrection;
    if (mNextSibling) {
      nsIMathMLFrame* mathMLFrame;
      nsresult res = mNextSibling->QueryInterface(
        NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
      if (NS_SUCCEEDED(res) && mathMLFrame) {
         // get thinspace
         nsCOMPtr<nsIStyleContext> parentContext;
         mParent->GetStyleContext(getter_AddRefs(parentContext));
         const nsStyleFont *font = NS_STATIC_CAST(const nsStyleFont*,
           parentContext->GetStyleData(eStyleStruct_Font));
         nscoord thinSpace = NSToCoordRound(float(font->mFont.size)*float(3) / float(18));
         // add inter frame spacing to our width
         nsCOMPtr<nsIAtom> frameType;
         GetFrameType(getter_AddRefs(frameType));
         nsCOMPtr<nsIAtom> nextFrameType;
         mNextSibling->GetFrameType(getter_AddRefs(nextFrameType));
         nscoord space = GetInterFrameSpacing(mPresentationData.scriptLevel,
           frameType, nextFrameType);
         gap += space * thinSpace;
         // look ahead and also add the left italic correction of the next frame
         nsBoundingMetrics bmNext;
         mathMLFrame->GetBoundingMetrics(bmNext);
         GetItalicCorrection(bmNext, leftCorrection, italicCorrection);
         gap += leftCorrection;
      }
    }
    // add our own italic correction
    GetItalicCorrection(mBoundingMetrics, leftCorrection, italicCorrection);
    gap += italicCorrection;
    // see if we should shift our own children to account for the left correction
    // only shift if we are the first child of <math> - the look-ahead fixes the rest
    if (leftCorrection) {
      nsIFrame* childFrame;
      mParent->FirstChild(aPresContext, nsnull, &childFrame);
      if (this == childFrame) {
        gap += leftCorrection;
        childFrame = mFrames.FirstChild();
        while (childFrame) {
          nsPoint origin;
          childFrame->GetOrigin(origin);
          childFrame->MoveTo(aPresContext, origin.x + leftCorrection, origin.y);
          childFrame->GetNextSibling(&childFrame);
        }
        mBoundingMetrics.leftBearing += leftCorrection;
        mBoundingMetrics.rightBearing += leftCorrection;
        mBoundingMetrics.width += leftCorrection;
      }
    }
    aDesiredSize.width += gap;
  }
  return NS_OK;
}



//==========================
nsresult
NS_NewMathMLmathBlockFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmathBlockFrame* it = new (aPresShell) nsMathMLmathBlockFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsresult
NS_NewMathMLmathInlineFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmathInlineFrame* it = new (aPresShell) nsMathMLmathInlineFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

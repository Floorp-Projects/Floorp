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

#include "nsITextContent.h"

#include "nsMathMLAtoms.h"
#include "nsMathMLParts.h"
#include "nsMathMLChar.h"
#include "nsMathMLContainerFrame.h"

//
// nsMathMLContainerFrame implementation
//

// TODO: Proper management of ignorable whitespace 
//       and handling of non-math related frames.
// * Should math markups only enclose other math markups?
//   Why bother... we can leave them in, as hinted in the newsgroup.
//
// * Space doesn't count. Handling space is more complicated than it seems. 
//   We have to clean inside and outside:
//       <mi> a </mi> <mo> + </mo> <mi> b </mi>
//           ^ ^     ^    ^ ^     ^    ^ ^
//   *Outside* currently handled using IsOnlyWhitespace().
//   For whitespace only frames, their rect is set zero.
//
//   *Inside* not currently handled... so the user must do <mi>a</mi>
//   What to do?!
//   - Add a nsTextFrame::CompressWhitespace() *if* it is MathML content?!
//   - via CSS property? Support for "none/compress"? (white-space: normal | pre | nowrap)


// nsISupports
// =============================================================================

NS_INTERFACE_MAP_BEGIN(nsMathMLContainerFrame)
  NS_INTERFACE_MAP_ENTRY(nsIMathMLFrame)
#ifdef NS_DEBUG
  NS_INTERFACE_MAP_ENTRY(nsIFrameDebug)
#endif
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMathMLFrame)
NS_INTERFACE_MAP_END_INHERITING(nsHTMLContainerFrame)

NS_IMETHODIMP_(nsrefcnt) 
nsMathMLContainerFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsMathMLContainerFrame::Release(void)
{
  return NS_OK;
}

/* ///////////////
 * MathML specific - Whitespace management ...
 * WHITESPACE: don't forget that whitespace doesn't count in MathML!
 * empty frames shouldn't be created in MathML, oh well that's another story.
 * =============================================================================
 */

PRBool
nsMathMLContainerFrame::IsOnlyWhitespace(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null arg");

  // quick return if we have encountered this frame before
  nsFrameState state;
  aFrame->GetFrameState(&state);
  if (NS_FRAME_IS_UNFLOWABLE & state)
    return PR_TRUE;

  // by empty frame we mean a leaf frame whose text content is empty...
  PRBool rv = PR_FALSE;
  nsCOMPtr<nsIContent> aContent;
  aFrame->GetContent(getter_AddRefs(aContent));
  if (!aContent) return PR_TRUE;
  PRInt32 numKids;
  aContent->ChildCount(numKids);
  if (0 == numKids) {
    nsCOMPtr<nsITextContent> tc(do_QueryInterface(aContent));
    if (tc.get()) tc->IsOnlyWhitespace(&rv);
  }
  if (rv) {
    // mark the frame as unflowable
    aFrame->SetFrameState(NS_FRAME_IS_UNFLOWABLE | state);
  }
  return rv;
}

// helper to get an attribute from the content or the surrounding <mstyle> hierarchy
nsresult
nsMathMLContainerFrame::GetAttribute(nsIContent* aContent,
                                     nsIFrame*   aMathMLmstyleFrame,          
                                     nsIAtom*    aAttributeAtom,
                                     nsString&   aValue)
{
  nsresult rv = NS_CONTENT_ATTR_NOT_THERE;

  // see if we can get the attribute from the content
  if (aContent)
  {
    rv = aContent->GetAttribute(kNameSpaceID_None, aAttributeAtom, aValue);
  }

  if (NS_CONTENT_ATTR_NOT_THERE == rv)
  {
    // see if we can get the attribute from the mstyle frame
    if (aMathMLmstyleFrame)
    {
      nsCOMPtr<nsIContent> mstyleContent;
      aMathMLmstyleFrame->GetContent(getter_AddRefs(mstyleContent)); 

      nsIFrame* mstyleParent;
      aMathMLmstyleFrame->GetParent(&mstyleParent); 

      nsPresentationData mstyleParentData;
      mstyleParentData.mstyle = nsnull;
 
      if (mstyleParent)
      {
        nsIMathMLFrame* aMathMLFrame = nsnull;
        rv = mstyleParent->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&aMathMLFrame);
        if (NS_SUCCEEDED(rv) && nsnull != aMathMLFrame)
        {
          aMathMLFrame->GetPresentationData(mstyleParentData);
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
    nsStyleFont font;
    aStyleContext->GetStyle(eStyleStruct_Font, font);
    return NSToCoordRound(aCSSValue.GetFloatValue() * (float)font.mFont.size);
  }
  else if (eCSSUnit_XHeight == unit) {
    nscoord xHeight;
    nsStyleFont font;
    aStyleContext->GetStyle(eStyleStruct_Font, font);
    nsCOMPtr<nsIFontMetrics> fm;
    aPresContext->GetMetricsFor(font.mFont, getter_AddRefs(fm));
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
  if (aString.EqualsWithConversion("veryverythinmathspace"))
  {
    i = 1;
    namedspaceAtom = nsMathMLAtoms::veryverythinmathspace_;
  }
  else if (aString.EqualsWithConversion("verythinmathspace"))
  {
    i = 2; 
    namedspaceAtom = nsMathMLAtoms::verythinmathspace_;
  }
  else if (aString.EqualsWithConversion("thinmathspace"))
  {
    i = 3;
    namedspaceAtom = nsMathMLAtoms::thinmathspace_;
  }
  else if (aString.EqualsWithConversion("mediummathspace"))
  {
    i = 4;
    namedspaceAtom = nsMathMLAtoms::mediummathspace_;
  }
  else if (aString.EqualsWithConversion("thickmathspace"))
  {
    i = 5;
    namedspaceAtom = nsMathMLAtoms::thickmathspace_;
  }
  else if (aString.EqualsWithConversion("verythickmathspace"))
  {
    i = 6;
    namedspaceAtom = nsMathMLAtoms::verythickmathspace_;
  }
  else if (aString.EqualsWithConversion("veryverythickmathspace"))
  {
    i = 7;
    namedspaceAtom = nsMathMLAtoms::veryverythickmathspace_;
  }

  if (0 != i) 
  {
    if (aMathMLmstyleFrame) 
    {
      // see if there is a <mstyle> that has overriden the default value
      // GetAttribute() will recurse all the way up into the <mstyle> hierarchy
      nsAutoString value;
      if (NS_CONTENT_ATTR_HAS_VALUE ==
          GetAttribute(nsnull, aMathMLmstyleFrame, namedspaceAtom, value))
      {
        if (ParseNumericValue(value, aCSSValue) &&
            aCSSValue.IsLengthUnit())
        {
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
  nsStyleFont font;
  mStyleContext->GetStyle(eStyleStruct_Font, font);
  aRenderingContext.SetFont(font.mFont);

  // bounding metrics
  nsAutoString errorMsg(PRUnichar(0xFFFD));
  rv = aRenderingContext.GetBoundingMetrics(errorMsg.GetUnicode(), 
                                            PRUint32(errorMsg.Length()),
                                            mBoundingMetrics);
  if (NS_FAILED(rv)) {
    printf("GetBoundingMetrics failed\n");
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

  if (nsnull != aDesiredSize.maxElementSize) {
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
    nsStyleFont font;
    nsStyleColor color;
    mStyleContext->GetStyle(eStyleStruct_Font, font);
    mStyleContext->GetStyle(eStyleStruct_Color, color);
    aRenderingContext.SetColor(color.mColor);
    aRenderingContext.SetFont(font.mFont);

    nsAutoString errorMsg(PRUnichar(0xFFFD));
    aRenderingContext.DrawString(errorMsg.GetUnicode(), 
                                 PRUint32(errorMsg.Length()), 
                                 mRect.x, mRect.y);
  }
  return NS_OK;
}

// -------------------------

void
nsMathMLContainerFrame::ReflowEmptyChild(nsIPresContext* aPresContext,
                                         nsIFrame*       aFrame)
{
//  nsHTMLReflowMetrics emptySize(nsnull);
//  nsHTMLReflowState emptyReflowState(aPresContext, aReflowState, aFrame, nsSize(0,0));
//  nsresult rv = ReflowChild(aFrame, aPresContext, emptySize, emptyReflowState, aStatus);
 
  // 0-size the frame
  aFrame->SetRect(aPresContext, nsRect(0,0,0,0));

  // 0-size the view, if any
  nsIView* view = nsnull;
  aFrame->GetView(aPresContext, &view);
  if (view) {
    nsCOMPtr<nsIViewManager> vm;
    view->GetViewManager(*getter_AddRefs(vm));
    vm->ResizeView(view, 0,0);
  }
}

/* /////////////
 * nsIMathMLFrame - support methods for precise positioning 
 * =============================================================================
 */

NS_IMETHODIMP
nsMathMLContainerFrame::GetBoundingMetrics(nsBoundingMetrics& aBoundingMetrics)
{
  aBoundingMetrics = mBoundingMetrics;
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::SetBoundingMetrics(const nsBoundingMetrics& aBoundingMetrics)
{
  mBoundingMetrics = aBoundingMetrics;
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::GetReference(nsPoint& aReference)
{
  aReference = mReference;
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::SetReference(const nsPoint& aReference)
{
  mReference = aReference;
  return NS_OK;
}

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
  
  nsRect aRect;
  aFrame->GetRect(aRect);
  aReflowMetrics.descent = aRect.x; 
  aReflowMetrics.ascent  = aRect.y;
  aReflowMetrics.width   = aRect.width; 
  aReflowMetrics.height  = aRect.height;
	
  aBoundingMetrics.Clear();
  nsIMathMLFrame* aMathMLFrame = nsnull;
  nsresult rv = aFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&aMathMLFrame);
  if (NS_SUCCEEDED(rv) && aMathMLFrame) {
    aMathMLFrame->GetBoundingMetrics(aBoundingMetrics);
#if 0
    nsFrame::ListTag(stdout, aFrame);
    printf(" subItalicCorrection:%d supItalicCorrection:%d\n",
    aBoundingMetrics.subItalicCorrection, aBoundingMetrics.supItalicCorrection);
#endif
  }
  else { // aFrame is not a MathML frame, just return the reflow metrics
    aBoundingMetrics.descent = aReflowMetrics.descent;
    aBoundingMetrics.ascent  = aReflowMetrics.ascent;
    aBoundingMetrics.width   = aReflowMetrics.width;
#if 0
    printf("GetBoundingMetrics() failed for: "); /* getchar(); */
    nsFrame::ListTag(stdout, aFrame);
    printf("\n");
#endif
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
      printf("WARNING *** it is wrong to fire stretch more than once on a frame...\n");
//      NS_ASSERTION(PR_FALSE,"Stretch() was fired more than once on a frame!");
      return NS_OK;
    }
    mEmbellishData.flags |= NS_MATHML_STRETCH_DONE;

    if (NS_MATHML_HAS_ERROR(mPresentationData.flags)) {
      printf("WARNING *** it is wrong to fire stretch on a erroneous frame...\n");
      return NS_OK;
    }

    // Pass the stretch to the first non-empty child ...

    nsIFrame* childFrame = mEmbellishData.firstChild;
    NS_ASSERTION(childFrame, "Something is wrong somewhere");

    if (childFrame) {
      nsIMathMLFrame* aMathMLFrame = nsnull;
      rv = childFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&aMathMLFrame);
      NS_ASSERTION(NS_SUCCEEDED(rv) && aMathMLFrame, "Something is wrong somewhere");
      if (NS_SUCCEEDED(rv) && aMathMLFrame) {

        // And the trick is that the child's rect.x is still holding the descent, 
        // and rect.y is still holding the ascent ...
        nsRect rect;
        childFrame->GetRect(rect);

        nsHTMLReflowMetrics childSize(aDesiredStretchSize);
        aMathMLFrame->GetBoundingMetrics(childSize.mBoundingMetrics);
        childSize.descent = rect.x;
        childSize.ascent = rect.y;
        childSize.height = rect.height;
        childSize.width = rect.width;
        
        nsBoundingMetrics containerSize = aContainerSize;

        if (aStretchDirection != NS_STRETCH_DIRECTION_DEFAULT && 
            aStretchDirection != mEmbellishData.direction) {
          // change the direction and confine the stretch to us
          // XXX tune this
          containerSize = mBoundingMetrics;
        }

        // do the stretching...
        aMathMLFrame->Stretch(aPresContext, aRenderingContext,
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

          nsStyleFont font;
          mStyleContext->GetStyle(eStyleStruct_Font, font);
          nscoord em = NSToCoordRound(float(font.mFont.size));

          nsEmbellishData coreData;
          mEmbellishData.core->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&aMathMLFrame);
          aMathMLFrame->GetEmbellishData(coreData);

          // cache these values
          mEmbellishData.leftSpace = coreData.leftSpace;
          mEmbellishData.rightSpace = coreData.rightSpace;

          aDesiredStretchSize.width +=
            NSToCoordRound((coreData.leftSpace + coreData.rightSpace) * em);

// XXX is this what to do ?
          aDesiredStretchSize.mBoundingMetrics.width +=
            NSToCoordRound((coreData.leftSpace + coreData.rightSpace) * em);

          nscoord dx = nscoord( coreData.leftSpace * em );        
          if (0 == dx) return NS_OK;
    
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
    nsIMathMLFrame* aMathMLFrame = nsnull;
    nsresult rv = mParent->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&aMathMLFrame);
    if (NS_SUCCEEDED(rv) && aMathMLFrame) {
      aMathMLFrame->GetEmbellishData(parentData);
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
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
  // Also return our bounding metrics
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;
  return NS_OK;
}

// This is the method used to set the frame as an embellished container.
// It checks if the first (non-empty) child is embellished. Hence, calls 
// must be bottom-up. The method must only be called from within frames who are 
// entitled to be potential embellished operators as per the MathML REC. 
NS_IMETHODIMP
nsMathMLContainerFrame::EmbellishOperator()
{
  // Get the first non-empty child
  nsIFrame* firstChild = mFrames.FirstChild();
  while (firstChild) {
    if (!IsOnlyWhitespace(firstChild)) break;
    firstChild->GetNextSibling(&firstChild);
  }
  if (firstChild && IsEmbellishOperator(firstChild)) {
    // Cache the first child
    mEmbellishData.flags |= NS_MATHML_EMBELLISH_OPERATOR;
    mEmbellishData.firstChild = firstChild;
    // Cache also the inner-most embellished frame at the core of the hierarchy
    nsIMathMLFrame* aMathMLFrame = nsnull;
    firstChild->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&aMathMLFrame);
    nsEmbellishData embellishData;
    aMathMLFrame->GetEmbellishData(embellishData);
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

NS_IMETHODIMP
nsMathMLContainerFrame::GetEmbellishData(nsEmbellishData& aEmbellishData)
{
  aEmbellishData = mEmbellishData;
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::SetEmbellishData(const nsEmbellishData& aEmbellishData)
{
  mEmbellishData = aEmbellishData;
  return NS_OK;
}

PRBool
nsMathMLContainerFrame::IsEmbellishOperator(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null arg");
  if (!aFrame) return PR_FALSE;
  nsIMathMLFrame* aMathMLFrame = nsnull;
  nsresult rv = aFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&aMathMLFrame);
  if (NS_FAILED(rv) || !aMathMLFrame) return PR_FALSE;
  nsEmbellishData aEmbellishData;
  aMathMLFrame->GetEmbellishData(aEmbellishData);
  return NS_MATHML_IS_EMBELLISH_OPERATOR(aEmbellishData.flags);
}

/* /////////////
 * nsIMathMLFrame - support methods for scripting elements (nested frames
 * within msub, msup, msubsup, munder, mover, munderover, mmultiscripts, 
 * mfrac, mroot, mtable).
 * =============================================================================
 */

NS_IMETHODIMP
nsMathMLContainerFrame::GetPresentationData(nsPresentationData& aPresentationData)
{
  aPresentationData = mPresentationData;
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::SetPresentationData(const nsPresentationData& aPresentationData)
{
  mPresentationData = aPresentationData;
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::UpdatePresentationData(PRInt32 aScriptLevelIncrement, 
                                               PRBool  aDisplayStyle,
                                               PRBool  aCompressed)
{
  mPresentationData.scriptLevel += aScriptLevelIncrement;
  if (aDisplayStyle)
    mPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
  else
    mPresentationData.flags &= ~NS_MATHML_DISPLAYSTYLE;
  if (aCompressed) {
    // 'compressed' means 'prime' style in App. G, TeXbook
    // (the flag retains its value once it is set)
    mPresentationData.flags |= NS_MATHML_COMPRESSED;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::UpdatePresentationDataFromChildAt(PRInt32 aIndex, 
                                                          PRInt32 aScriptLevelIncrement,
                                                          PRBool  aDisplayStyle,
                                                          PRBool  aCompressed)
{
  nsIFrame* childFrame = mFrames.FirstChild();
  while (nsnull != childFrame) {
    if (!IsOnlyWhitespace(childFrame)) {
      if (0 >= aIndex--) {
        nsIMathMLFrame* aMathMLFrame = nsnull;
        nsresult rv = childFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&aMathMLFrame);
        if (NS_SUCCEEDED(rv) && nsnull != aMathMLFrame) {
          // update
      	  aMathMLFrame->UpdatePresentationData(aScriptLevelIncrement, aDisplayStyle, aCompressed);
          // propagate down the subtrees
          aMathMLFrame->UpdatePresentationDataFromChildAt(0, aScriptLevelIncrement, aDisplayStyle, aCompressed);
      	}
      }
    }
    childFrame->GetNextSibling(&childFrame);
  }
  return NS_OK;
}

PRInt32
nsMathMLContainerFrame::FindSmallestFontSizeFor(nsIPresContext* aPresContext, 
                                                nsIFrame*       aFrame)
{
  nsStyleFont aFont;
  nsCOMPtr<nsIStyleContext> aStyleContext;
  aFrame->GetStyleContext(getter_AddRefs(aStyleContext));
  aStyleContext->GetStyle(eStyleStruct_Font, aFont);
  PRInt32 fontSize = aFont.mFont.size;

  PRInt32 childSize;
  nsIFrame* childFrame;
  aFrame->FirstChild(aPresContext, nsnull, &childFrame);
  while (nsnull != childFrame) {
    if (!IsOnlyWhitespace(childFrame)) {
      childSize = FindSmallestFontSizeFor(aPresContext, childFrame);
      if (fontSize > childSize) fontSize = childSize;
    }
    childFrame->GetNextSibling(&childFrame);
  }
  return fontSize;
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

// helper method to alter the style context
// This method is used for switching the font to a subscript/superscript font in
// mfrac, msub, msup, msubsup, munder, mover, munderover, mmultiscripts 

// XXX change the code to ensure that the size decreases in a top-down manner. 
// the coide should always insert top-scriptstyle frames, and inner frames that 
// cause the size to decrease pass the smallest limit should be removed
nsresult
nsMathMLContainerFrame::InsertScriptLevelStyleContext(nsIPresContext* aPresContext)
{
  nsresult rv = NS_OK;
  nsIFrame* nextFrame = mFrames.FirstChild();
  while (nsnull != nextFrame && NS_SUCCEEDED(rv)) { 	
    nsIFrame* childFrame = nextFrame;
    rv = nextFrame->GetNextSibling(&nextFrame);
    if (!IsOnlyWhitespace(childFrame) && NS_SUCCEEDED(rv)) {

      // see if the child frame implements the nsIMathMLFrame interface
      nsIMathMLFrame* aMathMLFrame = nsnull;
      rv = childFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&aMathMLFrame);
      if (nsnull != aMathMLFrame && NS_SUCCEEDED(rv)) {

        // get the scriptlevel of the child
        nsPresentationData childData;
        aMathMLFrame->GetPresentationData(childData);

        // Iteration to set a style context for the script level font.
        // Wow, here is what is happening: the style system requires that any style context
        // *must* be uniquely associated to a frame. So we insert as many frames as needed
        // to scale-down (or scale-up) the fontsize.

        PRInt32 gap = childData.scriptLevel - mPresentationData.scriptLevel;
        if (0 != gap) {
          // We are about to change the font-size... We first see if we
          // are in the scope of a <mstyle> that tells us what to do.
          // This is one of the most obscure part to implement in the spec...
          /*
          The REC says:

          Whenever the scriptlevel is changed, either automatically or by being
          explicitly incremented, decremented, or set, the current font size is
          multiplied by the value of scriptsizemultiplier to the power of the
          change in scriptlevel. For example, if scriptlevel is increased by 2,
          the font size is multiplied by scriptsizemultiplier twice in succession;
          if scriptlevel is explicitly set to 2 when it had been 3, the font size
          is divided by scriptsizemultiplier. 

          The default value of scriptsizemultiplier is less than one (in fact, it
          is approximately the square root of 1/2), resulting in a smaller font size
          with increasing scriptlevel. To prevent scripts from becoming unreadably
          small, the font size is never allowed to go below the value of
          scriptminsize as a result of a change to scriptlevel, though it can be
          set to a lower value using the fontsize attribute  on <mstyle> or on
          token elements. If a change to scriptlevel would cause the font size to
          become lower than scriptminsize using the above formula, the font size
          is instead set equal to scriptminsize within the subexpression for which
          scriptlevel was changed. 

          In the syntax for scriptminsize, v-unit represents a unit of vertical
          length. The most common unit for specifying font sizes in typesetting
          is pt (points). 
          */

          // default scriptsizemultiplier = 0.71 
          // default scriptminsize = 8pt 

          // here we only consider scriptminsize, and use the default
          // smaller-font-size algorithm of the style system
          PRInt32 scriptminsize = NSIntPointsToTwips(8);

          // see if there is a scriptminsize attribute on a <mstyle> that wraps us
          nsAutoString value;
          if (NS_CONTENT_ATTR_HAS_VALUE == 
              GetAttribute(nsnull, mPresentationData.mstyle, 
                           nsMathMLAtoms::scriptminsize_, value))
          {
            nsCSSValue cssValue;
            if (ParseNumericValue(value, cssValue)) {
              nsCSSUnit unit = cssValue.GetUnit();
              if (eCSSUnit_Number == unit)
                scriptminsize = nscoord(float(scriptminsize) * cssValue.GetFloatValue());
              else if (eCSSUnit_Percent == unit)
                scriptminsize = nscoord(float(scriptminsize) * cssValue.GetPercentValue());
              else if (eCSSUnit_Null != unit)
                scriptminsize = CalcLength(aPresContext, mStyleContext, cssValue);
            }
#ifdef NS_DEBUG
            else {
              char str[50];
              value.ToCString(str, 50);
              printf("Invalid attribute scriptminsize=%s\n", str);
            }
#endif
          }

          // get Nav's magic font scaler
          PRInt32 scaler;
          aPresContext->GetFontScaler(&scaler);
          float scaleFactor = nsStyleUtil::GetScalingFactor(scaler);
          const nsFont& defaultFont = aPresContext->GetDefaultFontDeprecated();

          nsCOMPtr<nsIContent> childContent;
          childFrame->GetContent(getter_AddRefs(childContent));

          nsCOMPtr<nsIPresShell> shell;
          aPresContext->GetShell(getter_AddRefs(shell));

          nsIFrame* firstFrame = nsnull;
          nsIFrame* lastFrame = this;
          nsIStyleContext* lastStyleContext = mStyleContext;
          nsCOMPtr<nsIStyleContext> newStyleContext;

          // XXX seems not to decrease when the initail font-size is large (100pt)
          nsIAtom* fontAtom = (0 < gap) ?
            nsMathMLAtoms::fontsize_smaller :
            nsMathMLAtoms::fontsize_larger;

          PRBool isSmaller = PR_TRUE;
          if (0 > gap) { isSmaller = PR_FALSE; gap = -gap; } // absolute value

          PRInt32 smallestFontIndex, smallestFontSize = 0;
          if (isSmaller) {
            // find the smallest font-size in this subtree
            smallestFontSize = FindSmallestFontSizeFor(aPresContext, childFrame);
          }

          while (0 < gap--) {

            if (isSmaller) {
              // look ahead for the next smallest font size that will be in the subtree
              smallestFontIndex = nsStyleUtil::FindNextSmallerFontSize(smallestFontSize, (PRInt32)defaultFont.size, scaleFactor, aPresContext);
              smallestFontSize = nsStyleUtil::CalcFontPointSize(smallestFontIndex, (PRInt32)defaultFont.size, scaleFactor, aPresContext);
//((nsFrame*)childFrame)->ListTag(stdout);
//printf(" About to move to fontsize:%dpt(%dtwips)\n", 
//NSTwipsToFloorIntPoints(smallestFontSize), smallestFontSize);
              if (smallestFontSize < scriptminsize) {
                // don't bother doing any work
//printf("..... stopping ......\n");
// XXX there should be a mechanism so that we never try this subtree again
                break;
              }
            }

            aPresContext->ResolvePseudoStyleContextFor(childContent, fontAtom, lastStyleContext,
                                                       PR_FALSE, getter_AddRefs(newStyleContext));          
            if (newStyleContext && newStyleContext.get() != lastStyleContext) {
              // create a new wrapper frame and append it as sole child of the last created frame
              nsIFrame* newFrame = nsnull;
              NS_NewMathMLWrapperFrame(shell, &newFrame);
              NS_ASSERTION(newFrame, "Failed to create new frame");
              if (!newFrame) break;
              newFrame->Init(aPresContext, childContent, lastFrame, newStyleContext, nsnull);

              if (nsnull == firstFrame) {
                firstFrame = newFrame; 
              }
              if (this != lastFrame) {
                lastFrame->SetInitialChildList(aPresContext, nsnull, newFrame);
              }
              lastStyleContext = newStyleContext;
              lastFrame = newFrame;    
            }
            else {
              break;
            }
          }
          if (nsnull != firstFrame) { // at least one new frame was created
            mFrames.ReplaceFrame(this, childFrame, firstFrame);
            childFrame->SetParent(lastFrame);
            childFrame->SetNextSibling(nsnull);
            aPresContext->ReParentStyleContext(childFrame, lastStyleContext);
            lastFrame->SetInitialChildList(aPresContext, nsnull, childFrame);

            // if the child was an embellished operator,
            // make the whole list embellished as well
            nsEmbellishData embellishData;
            aMathMLFrame->GetEmbellishData(embellishData);
            if (0 != embellishData.flags && nsnull != embellishData.firstChild) {
              do { // walk the hierarchy in a bottom-up manner
                rv = lastFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&aMathMLFrame);
                NS_ASSERTION(NS_SUCCEEDED(rv) && aMathMLFrame, "Mystery!");
                if (NS_FAILED(rv) || !aMathMLFrame) break;
                embellishData.firstChild = childFrame;
                aMathMLFrame->SetEmbellishData(embellishData);
                childFrame = lastFrame;
                lastFrame->GetParent(&lastFrame);
              } while (lastFrame != this);
            }
          }
        }
      }
    }
  }
  return rv;
}


/* //////////////////
 * Frame construction
 * =============================================================================
 */

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
NS_IMETHODIMP
nsMathMLContainerFrame::Paint(nsIPresContext*      aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              const nsRect&        aDirtyRect,
                              nsFramePaintLayer    aWhichLayer)
{
  nsresult rv = NS_OK;

  // report an error if something wrong was found in this frame
  if (NS_MATHML_HAS_ERROR(mPresentationData.flags)) {
    return PaintError(aPresContext, aRenderingContext,
                      aDirtyRect, aWhichLayer);
  }

  rv = nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext,
                                   aDirtyRect, aWhichLayer);

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
  return rv;
}
#endif

NS_IMETHODIMP
nsMathMLContainerFrame::Init(nsIPresContext*  aPresContext,
                             nsIContent*      aContent,
                             nsIFrame*        aParent,
                             nsIStyleContext* aContext,
                             nsIFrame*        aPrevInFlow)
{
  nsresult rv;
  // first, let the base class do its Init()
  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
 
  mBoundingMetrics.Clear();
  mReference.x = mReference.y = 0;

  // now, if our parent implements the nsIMathMLFrame interface, we inherit
  // its scriptlevel and displaystyle. If the parent later wishes to increment
  // with other values, it will do so in its SetInitialChildList() method.
 
  mPresentationData.flags = 0;
  mPresentationData.scriptLevel = 0;
  mPresentationData.mstyle = nsnull;

  mEmbellishData.flags = 0;
  mEmbellishData.firstChild = nsnull;

  nsIMathMLFrame* aMathMLFrame = nsnull;
  nsresult res = aParent->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&aMathMLFrame);
  if (NS_SUCCEEDED(res) && nsnull != aMathMLFrame) {
    nsPresentationData parentData;
    aMathMLFrame->GetPresentationData(parentData);

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
    if (!IsOnlyWhitespace(child)) {
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
  }

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
    if (IsOnlyWhitespace(childFrame)) {
      ReflowEmptyChild(aPresContext, childFrame);      
    }
    else {
      nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                         childFrame, availSize);
      rv = NS_STATIC_CAST(nsMathMLContainerFrame*, 
                          aFrame)->ReflowChild(childFrame, 
                                               aPresContext, childDesiredSize,
                                               childReflowState, aStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
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
    }
    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
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
    if (!IsOnlyWhitespace(childFrame)) {
      childFrame->GetRect(rect);
      aDesiredSize.width += rect.width;
      if (aDesiredSize.descent < rect.x) aDesiredSize.descent = rect.x;
      if (aDesiredSize.ascent < rect.y) aDesiredSize.ascent = rect.y;
    }
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
    //////////////
    // WHITESPACE: don't forget that whitespace doesn't count in MathML!
    if (IsOnlyWhitespace(childFrame)) {
      ReflowEmptyChild(aPresContext, childFrame);      
    }
    else {
      nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                         childFrame, availSize);
      rv = ReflowChild(childFrame, aPresContext, childDesiredSize,
                       childReflowState, childStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(childStatus), "bad status");
      if (NS_FAILED(rv)) return rv;

      // At this stage, the origin points of the children have no use, so we will use the
      // origins as placeholders to store the child's ascent and descent. Later on,
      // we should set the origins so as to overwrite what we are storing there now.
      childFrame->SetRect(aPresContext,
                          nsRect(childDesiredSize.descent, childDesiredSize.ascent,
                                 childDesiredSize.width, childDesiredSize.height));
    }
    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
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
      else if (!IsOnlyWhitespace(childFrame)) {
        nsIMathMLFrame* aMathMLFrame = nsnull;
        rv = childFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&aMathMLFrame);
        if (NS_SUCCEEDED(rv) && aMathMLFrame) {
          // retrieve the metrics that was stored at the previous pass
          nsRect rect;
          childFrame->GetRect(rect);
          aMathMLFrame->GetBoundingMetrics(childDesiredSize.mBoundingMetrics);
          childDesiredSize.descent = rect.x;
          childDesiredSize.ascent = rect.y;
          childDesiredSize.height = rect.height;
          childDesiredSize.width = rect.width;

          aMathMLFrame->Stretch(aPresContext, *aReflowState.rendContext, 
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

NS_IMETHODIMP
nsMathMLContainerFrame::Place(nsIPresContext*      aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              PRBool               aPlaceOrigin,
                              nsHTMLReflowMetrics& aDesiredSize)
{
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = aDesiredSize.descent = 0;
  mBoundingMetrics.Clear();
 
  PRInt32 count = 0; 
  nsBoundingMetrics bm;
  nsHTMLReflowMetrics childSize(nsnull);
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (!IsOnlyWhitespace(childFrame)) {
      GetReflowAndBoundingMetricsFor(childFrame, childSize, bm);

      aDesiredSize.width += childSize.width;
      if (aDesiredSize.descent < childSize.descent)
        aDesiredSize.descent = childSize.descent;
      if (aDesiredSize.ascent < childSize.ascent)
        aDesiredSize.ascent = childSize.ascent;

      // Compute and cache our bounding metrics
      if (0 == count)   
        mBoundingMetrics  = bm;
      else
        mBoundingMetrics += bm;

      count++;
    }
    childFrame->GetNextSibling(&childFrame);
  }
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  if (aPlaceOrigin) {
    nsRect rect;
    nscoord dy, dx = 0;
    childFrame = mFrames.FirstChild();
    while (childFrame) {
      childFrame->GetRect(rect);
      childSize.width = rect.width;
      childSize.height = rect.height;

      // Place and size the child
      dy = aDesiredSize.ascent - rect.y;
      FinishReflowChild(childFrame, aPresContext, childSize, dx, dy, 0);

      dx += rect.width;
      childFrame->GetNextSibling(&childFrame);
    }
  }

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;
  return NS_OK;
}


//==========================
// *BEWARE* of the wrapper frame!
// This is the frame that is inserted to alter the style context of
// scripting elements. What this means is that looking for your parent
// or your siblings with (possible several) wrapper frames around you
// can make you wonder what is going on. For example, the direct parent
// of the subscript within <msub> is not <msub>, but instead the wrapper
// frame that was insterted to alter the style context of the subscript!
// You will seldom need to find out who is exactly your parent. You should
// first rethink your code to see if you can avoid finding who is your parent. 
// Be careful, there are wrapper frames all over the place, and probably 
// one or many are wrapping you if you are in a position where the
// scriptlevel is non zero.

nsresult
NS_NewMathMLWrapperFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLWrapperFrame* it = new (aPresShell) nsMathMLWrapperFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;  
  return NS_OK;
}

nsMathMLWrapperFrame::nsMathMLWrapperFrame()
{
}

nsMathMLWrapperFrame::~nsMathMLWrapperFrame()
{
}

NS_IMETHODIMP
nsMathMLWrapperFrame::Reflow(nsIPresContext*          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;
  aStatus = NS_FRAME_COMPLETE;
  aDesiredSize.width = aDesiredSize.height = aDesiredSize.ascent = aDesiredSize.descent = 0;

  // See if this is an incremental reflow
  if (aReflowState.reason == eReflowReason_Incremental) {
    nsIFrame* targetFrame;
    aReflowState.reflowCommand->GetTarget(targetFrame);
#ifdef MATHML_NOISY_INCREMENTAL_REFLOW
printf("nsMathMLWrapperFrame::Reflow:IncrementalReflow received by: ");
nsFrame::ListTag(stdout, this);
printf("for target: ");
nsFrame::ListTag(stdout, targetFrame);
printf("\n");
#endif
    if (this == targetFrame) {
    }
    else {
      // Remove the next frame from the reflow path
      nsIFrame* nextFrame;
      aReflowState.reflowCommand->GetNext(nextFrame);
    }
  }

  nsIFrame* childFrame = mFrames.FirstChild();
  if (childFrame) {
    nsReflowStatus childStatus;
    nsHTMLReflowMetrics childDesiredSize(aDesiredSize.maxElementSize,
                                         aDesiredSize.mFlags | NS_REFLOW_CALC_BOUNDING_METRICS);
    nsSize availSize(aReflowState.mComputedWidth, aReflowState.mComputedHeight);
    nsHTMLReflowState childReflowState(aPresContext, aReflowState, childFrame, availSize);
    rv = ReflowChild(childFrame, aPresContext, childDesiredSize, childReflowState, childStatus);
    childFrame->SetRect(aPresContext, 
                        nsRect(childDesiredSize.descent,childDesiredSize.ascent,
                               childDesiredSize.width,childDesiredSize.height));
    aDesiredSize = childDesiredSize;
    aStatus = childStatus;

    mBoundingMetrics = childDesiredSize.mBoundingMetrics;

    mReference.x = 0;
    mReference.y = aDesiredSize.ascent;

    FinalizeReflow(aPresContext, *aReflowState.rendContext, aDesiredSize);
  }
  return rv;
}

NS_IMETHODIMP
nsMathMLWrapperFrame::Place(nsIPresContext*      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            PRBool               aPlaceOrigin,
                            nsHTMLReflowMetrics& aDesiredSize)
{
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = aDesiredSize.descent = 0;
  aDesiredSize.mBoundingMetrics.Clear();

  nsIFrame* childFrame = mFrames.FirstChild();
  if (childFrame) {

    // get a possibly updated bounding metrics if the child is
    // a stretchy MathML frame that has just been stretched, otherwise
    // keep the bounding metrics that was computed in Reflow()
    nsIMathMLFrame* aMathMLFrame = nsnull;
    nsresult rv = childFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&aMathMLFrame);
    if (NS_SUCCEEDED(rv) && aMathMLFrame) {
      aMathMLFrame->GetBoundingMetrics(mBoundingMetrics);
      aMathMLFrame->GetReference(mReference); // XXX doesn't set the correct value (due to lspace) fix me!
    }

    nsRect rect;
    childFrame->GetRect(rect);
    aDesiredSize.descent = rect.x;
    aDesiredSize.ascent = rect.y;
    aDesiredSize.width = rect.width;
    aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
    aDesiredSize.mBoundingMetrics = mBoundingMetrics;

    if (aPlaceOrigin) {
      nsHTMLReflowMetrics childSize(nsnull);
      childSize.width = rect.width;
      childSize.height = rect.height;
      FinishReflowChild(childFrame, aPresContext, childSize, 0, 0, 0);
    }
  }
  return NS_OK;
}

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
NS_IMETHODIMP
nsMathMLWrapperFrame::Paint(nsIPresContext*      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect&        aDirtyRect,
                            nsFramePaintLayer    aWhichLayer)
{
  nsresult rv = NS_OK;

  rv = nsHTMLContainerFrame::Paint(aPresContext,
                                   aRenderingContext,
                                   aDirtyRect,
                                   aWhichLayer);

  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer)
  {
    aRenderingContext.SetColor(NS_RGB(255,0,0));

    nscoord x = mReference.x + mBoundingMetrics.leftBearing;
    nscoord y = mReference.y - mBoundingMetrics.ascent;
    nscoord w = mBoundingMetrics.rightBearing - mBoundingMetrics.leftBearing;
    nscoord h = mBoundingMetrics.ascent + mBoundingMetrics.descent;

    aRenderingContext.DrawRect(x,y,w,h);
  }
  return rv;
}
#endif

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
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsStyleUtil.h"

#include "nsMathMLmfencedFrame.h"

//
// <mfenced> -- surround content with a pair of fences
//

nsresult
NS_NewMathMLmfencedFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmfencedFrame* it = new (aPresShell) nsMathMLmfencedFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;  
  return NS_OK;
}

nsMathMLmfencedFrame::nsMathMLmfencedFrame()
{
}

nsMathMLmfencedFrame::~nsMathMLmfencedFrame()
{
  RemoveFencesAndSeparators();
}

NS_IMETHODIMP
nsMathMLmfencedFrame::Init(nsIPresContext*  aPresContext,
                           nsIContent*      aContent,
                           nsIFrame*        aParent,
                           nsIStyleContext* aContext,
                           nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsMathMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  mEmbellishData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY;

  mOpenChar = nsnull;
  mCloseChar = nsnull;
  mSeparatorsChar = nsnull;
  mSeparatorsCount = 0;

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
  mPresentationData.flags |= NS_MATHML_SHOW_BOUNDING_METRICS;
#endif
  return rv;
}


NS_IMETHODIMP
nsMathMLmfencedFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                          nsIAtom*        aListName,
                                          nsIFrame*       aChildList)
{
  nsresult rv;

  // First, let the base class do its work
  rv = nsMathMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  if (NS_FAILED(rv)) return rv;

  rv = CreateFencesAndSeparators(aPresContext);  
  return rv;
}

void
nsMathMLmfencedFrame::RemoveFencesAndSeparators()
{
  if (mOpenChar) delete mOpenChar;
  if (mCloseChar) delete mCloseChar;
  if (mSeparatorsChar) delete[] mSeparatorsChar;
  
  mOpenChar = nsnull;
  mCloseChar = nsnull;
  mSeparatorsChar = nsnull;
  mSeparatorsCount = 0;
}

nsresult
nsMathMLmfencedFrame::CreateFencesAndSeparators(nsIPresContext* aPresContext)
{
  nsresult rv;

  nsAutoString value, data;

  //////////////  
  // see if the opening fence is there ...
  rv = GetAttribute(mContent, mPresentationData.mstyle,
                    nsMathMLAtoms::open_, value);
  if (NS_CONTENT_ATTR_HAS_VALUE == rv) {
    value.Trim(" ");
    data = value;
  }
  else if (NS_CONTENT_ATTR_NOT_THERE == rv)
    data = PRUnichar('('); // default as per the MathML REC
  else
    data = nsAutoString();

  if (0 < data.Length()) {
    mOpenChar = new nsMathMLChar;
    if (!mOpenChar) return NS_ERROR_OUT_OF_MEMORY;
    mOpenChar->SetData(aPresContext, data);
    ResolveMathMLCharStyle(aPresContext, mContent, mStyleContext, mOpenChar);
  }

  //////////////
  // see if the closing fence is there ...
  rv = GetAttribute(mContent, mPresentationData.mstyle,
                    nsMathMLAtoms::close_, value);
  if (NS_CONTENT_ATTR_HAS_VALUE == rv) {
    value.Trim(" ");
    data = value;
  }
  else if (NS_CONTENT_ATTR_NOT_THERE == rv)
    data = PRUnichar(')'); // default as per the MathML REC
  else
    data = nsAutoString();

  if (0 < data.Length()) {
    mCloseChar = new nsMathMLChar;
    if (!mCloseChar) return NS_ERROR_OUT_OF_MEMORY;
    mCloseChar->SetData(aPresContext, data);
    ResolveMathMLCharStyle(aPresContext, mContent, mStyleContext, mCloseChar);
  }

  //////////////
  // see if separators are there ...
  rv = GetAttribute(mContent, mPresentationData.mstyle, 
                    nsMathMLAtoms::separators_, value);
  if (NS_CONTENT_ATTR_HAS_VALUE == rv) {
    value.Trim(" ");
    data = value;
  }
  else if (NS_CONTENT_ATTR_NOT_THERE == rv)
    data = PRUnichar(','); // default as per the MathML REC
  else
    data = nsAutoString();

  mSeparatorsCount = data.Length();
  if (0 < mSeparatorsCount) {
    PRInt32 sepCount = -1;
    nsIFrame* childFrame = mFrames.FirstChild();
    while (childFrame) {
      sepCount++;
      childFrame->GetNextSibling(&childFrame);
    }
    if (0 < sepCount) {
      mSeparatorsChar = new nsMathMLChar[sepCount];
      if (!mSeparatorsChar) return NS_ERROR_OUT_OF_MEMORY;
      nsAutoString sepChar;
      for (PRInt32 i = 0; i < sepCount; i++) {
        if (i < mSeparatorsCount)
          sepChar = data[i];
        else
          sepChar = data[mSeparatorsCount-1];
        mSeparatorsChar[i].SetData(aPresContext, sepChar);
        ResolveMathMLCharStyle(aPresContext, mContent, mStyleContext, &mSeparatorsChar[i]);
      }
    }
    mSeparatorsCount = sepCount;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmfencedFrame::Paint(nsIPresContext*      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect&        aDirtyRect,
                            nsFramePaintLayer    aWhichLayer,
                            PRUint32             aFlags)
{
  nsresult rv = NS_OK;

  /////////////
  // paint the content
  rv = nsMathMLContainerFrame::Paint(aPresContext, aRenderingContext, 
                                     aDirtyRect, aWhichLayer);
  ////////////
  // paint fences and separators
  if (NS_SUCCEEDED(rv)) {
    if (mOpenChar) mOpenChar->Paint(aPresContext, aRenderingContext,
                                    aDirtyRect, aWhichLayer, this);
    if (mCloseChar) mCloseChar->Paint(aPresContext, aRenderingContext,
                                      aDirtyRect, aWhichLayer, this);
    for (PRInt32 i = 0; i < mSeparatorsCount; i++) {
      mSeparatorsChar[i].Paint(aPresContext, aRenderingContext,
                               aDirtyRect, aWhichLayer, this);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsMathMLmfencedFrame::Reflow(nsIPresContext*          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  nsresult rv;
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = aDesiredSize.descent = 0;

  /////////////
  // Reflow children
  // Asking each child to cache its bounding metrics

  // Note that we don't use the base method nsMathMLContainerFrame::Reflow()
  // because we want to stretch our fences, separators and stretchy frames using
  // the *same* initial aDesiredSize.mBoundingMetrics. If we were to use the base
  // method here, our stretchy frames will be stretched and placed, and we may
  // end up stretching our fences/separators with a different aDesiredSize.

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

  // get our bounding metrics using a tentative Place()
  Place(aPresContext, *aReflowState.rendContext, PR_FALSE, aDesiredSize);

  // What size should we use to stretch our stretchy children
  // XXX tune this
  nsBoundingMetrics containerSize = mBoundingMetrics;

  /////////////
  // Ask stretchy children to stretch themselves
  
  nsStretchDirection stretchDir = NS_STRETCH_DIRECTION_VERTICAL;
  nsRect rect;
 
  childFrame = mFrames.FirstChild();
  while (childFrame) {
    nsIMathMLFrame* mathMLFrame;
    rv = childFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
    if (NS_SUCCEEDED(rv) && mathMLFrame) {
      // retrieve the metrics that was stored at the previous pass
      childFrame->GetRect(rect);
      mathMLFrame->GetBoundingMetrics(childDesiredSize.mBoundingMetrics);
      childDesiredSize.descent = rect.x;
      childDesiredSize.ascent = rect.y;
      childDesiredSize.height = rect.height;
      childDesiredSize.width = rect.width;

      mathMLFrame->Stretch(aPresContext, *aReflowState.rendContext, 
                           stretchDir, containerSize, childDesiredSize);
      // store the updated metrics
      childFrame->SetRect(aPresContext,
                          nsRect(childDesiredSize.descent, childDesiredSize.ascent,
                                 childDesiredSize.width, childDesiredSize.height));

      if (aDesiredSize.descent < childDesiredSize.descent)
        aDesiredSize.descent = childDesiredSize.descent;
      if (aDesiredSize.ascent < childDesiredSize.ascent)
        aDesiredSize.ascent = childDesiredSize.ascent;
    }
    childFrame->GetNextSibling(&childFrame);
  }

  //////////////////////////////////////////
  // Prepare the opening fence, separators, and closing fence, and
  // adjust the origin of children.

  PRInt32 i;
  const nsStyleFont *font = NS_STATIC_CAST(const nsStyleFont*,
    mStyleContext->GetStyleData(eStyleStruct_Font));
  nsCOMPtr<nsIFontMetrics> fm;
  aReflowState.rendContext->SetFont(font->mFont);
  aReflowState.rendContext->GetFontMetrics(*getter_AddRefs(fm));
  nscoord axisHeight, em;
  GetAxisHeight(*aReflowState.rendContext, fm, axisHeight);
  em = NSToCoordRound(float(font->mFont.size));
 
  nscoord fontAscent, fontDescent;
  fm->GetMaxAscent(fontAscent);
  fm->GetMaxDescent(fontDescent);

  // we need to center around the axis
  nscoord delta = PR_MAX(containerSize.ascent - axisHeight, 
                         containerSize.descent + axisHeight);
  containerSize.ascent = delta + axisHeight;
  containerSize.descent = delta - axisHeight;

  /////////////////
  // opening fence ...
  ReflowChar(aPresContext, *aReflowState.rendContext, mOpenChar,
             NS_MATHML_OPERATOR_FORM_PREFIX, mPresentationData.scriptLevel, 
             fontAscent, fontDescent, axisHeight, em,
             containerSize, aDesiredSize);
  /////////////////
  // separators ...
  for (i = 0; i < mSeparatorsCount; i++) {
    ReflowChar(aPresContext, *aReflowState.rendContext, &mSeparatorsChar[i],
               NS_MATHML_OPERATOR_FORM_INFIX, mPresentationData.scriptLevel,
               fontAscent, fontDescent, axisHeight, em,
               containerSize, aDesiredSize);
  }
  /////////////////
  // closing fence ...
  ReflowChar(aPresContext, *aReflowState.rendContext, mCloseChar,
             NS_MATHML_OPERATOR_FORM_POSTFIX, mPresentationData.scriptLevel,
             fontAscent, fontDescent, axisHeight, em,
             containerSize, aDesiredSize);

  //////////////////
  // Adjust the origins of each child.
  // and update our bounding metrics

  i = 0;
  nscoord dx = 0;
  nsBoundingMetrics bm;
  PRBool firstTime = PR_TRUE;
  mBoundingMetrics.Clear();
  if (mOpenChar) {
    PlaceChar(mOpenChar, fontAscent, aDesiredSize.ascent, bm, dx);
    mBoundingMetrics = bm;
    firstTime = PR_FALSE;
  }

  childFrame = mFrames.FirstChild();
  while (childFrame) {
    nsHTMLReflowMetrics childSize(nsnull);
    GetReflowAndBoundingMetricsFor(childFrame, childSize, bm);
    if (firstTime) {
      firstTime = PR_FALSE;
      mBoundingMetrics  = bm;
    }
    else  
      mBoundingMetrics += bm;

    FinishReflowChild(childFrame, aPresContext, childSize, 
                      dx, aDesiredSize.ascent - childSize.ascent, 0);
    dx += childSize.width;

    if (i < mSeparatorsCount) {
      PlaceChar(&mSeparatorsChar[i], fontAscent, aDesiredSize.ascent, bm, dx);
      mBoundingMetrics += bm;
    }
    i++;

    childFrame->GetNextSibling(&childFrame);
  }

  if (mCloseChar) {
    PlaceChar(mCloseChar, fontAscent, aDesiredSize.ascent, bm, dx);
    if (firstTime)
      mBoundingMetrics  = bm;
    else  
      mBoundingMetrics += bm;
  }

  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  if (aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

// helper functions to perform the common task of formatting our chars
nsresult
nsMathMLmfencedFrame::ReflowChar(nsIPresContext*      aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 nsMathMLChar*        aMathMLChar,
                                 nsOperatorFlags      aForm,
                                 PRInt32              aScriptLevel,
                                 nscoord              fontAscent,
                                 nscoord              fontDescent,
                                 nscoord              axisHeight,
                                 nscoord              em,
                                 nsBoundingMetrics&   aContainerSize,
                                 nsHTMLReflowMetrics& aDesiredSize)
{
  if (aMathMLChar && 0 < aMathMLChar->Length()) {
    nsOperatorFlags flags = 0;
    float leftSpace = 0.0f;
    float rightSpace = 0.0f;
 
    nsAutoString data;
    aMathMLChar->GetData(data);
    aMathMLChar->SetData(aPresContext, data); // XXX hack to reset, bug 45010
    PRBool found = nsMathMLOperators::LookupOperator(data, aForm,              
                                           &flags, &leftSpace, &rightSpace);

    // If we don't want extra space when we are a script
    if (found && aScriptLevel > 0) {
      leftSpace /= 2.0f;
      rightSpace /= 2.0f;
    }

    // stretch the char to the appropriate height if it is not big enough.
    nsBoundingMetrics charSize;
    nsresult res = aMathMLChar->Stretch(aPresContext, aRenderingContext,
                                        NS_STRETCH_DIRECTION_VERTICAL,
                                        aContainerSize, charSize);

    if (NS_STRETCH_DIRECTION_UNSUPPORTED != aMathMLChar->GetStretchDirection())
    {
      // has changed... so center the char around the axis
      nscoord height = charSize.ascent + charSize.descent;
      charSize.ascent = height/2 + axisHeight;
      charSize.descent = height - charSize.ascent;
    }
    else {
      charSize.ascent = fontAscent;
      charSize.descent = fontDescent;
      if (NS_FAILED(res) && charSize.width == 0) {
        // gracefully handle cases where stretching the char failed (i.e., GetBoundingMetrics failed)
        // we need to get the width
        aRenderingContext.GetWidth(data, charSize.width);
        // Note: the bounding metrics of the MathMLChar is not updated, but PlaceChar()
        // will do the right thing by leaving the necessary room to paint the char.
      }
    }

    if (aDesiredSize.ascent < charSize.ascent) 
      aDesiredSize.ascent = charSize.ascent;
    if (aDesiredSize.descent < charSize.descent) 
      aDesiredSize.descent = charSize.descent;

    // account the spacing
    charSize.width += NSToCoordRound((leftSpace + rightSpace) * em);

    // x-origin is used to store lspace ...
    // y-origin is used to stored the ascent ... 
    aMathMLChar->SetRect(nsRect(NSToCoordRound(leftSpace * em), 
                                charSize.ascent, charSize.width,
                                charSize.ascent + charSize.descent));
  }
  return NS_OK;
}

void
nsMathMLmfencedFrame::PlaceChar(nsMathMLChar*      aMathMLChar,
                                nscoord            aFontAscent,
                                nscoord            aDesiredAscent,
                                nsBoundingMetrics& bm,
                                nscoord&           dx)
{
  aMathMLChar->GetBoundingMetrics(bm);

  // the char's x-origin was used to store lspace ...
  // the char's y-origin was used to store the ascent ... 
  nsRect rect;
  aMathMLChar->GetRect(rect);
 
  nscoord dy = aDesiredAscent - rect.y;
  if (aMathMLChar->GetStretchDirection() == NS_STRETCH_DIRECTION_UNSUPPORTED)
  {
    // normal char, nsMathMLChar::Paint() will substract this later
    dy += (aFontAscent - bm.ascent);
  }
  else
  {
    // the stretchy char will be centered around the axis
    // so we adjust the returned bounding metrics accordingly
    bm.descent = (bm.ascent + bm.descent) - rect.y;
    bm.ascent = rect.y;
  }

  aMathMLChar->SetRect(nsRect(dx + rect.x, dy,
                              bm.width, rect.height));

  bm.leftBearing += rect.x;
  bm.rightBearing += rect.x;
  
  // return rect.width since it includes lspace and rspace
  bm.width = rect.width;

  dx += rect.width;
}

// ----------------------
// the Style System will use these to pass the proper style context to our MathMLChar
NS_IMETHODIMP
nsMathMLmfencedFrame::GetAdditionalStyleContext(PRInt32           aIndex, 
                                                nsIStyleContext** aStyleContext) const
{
  NS_PRECONDITION(aStyleContext, "null OUT ptr");
  PRInt32 openIndex = -1;
  PRInt32 closeIndex = -1;
  PRInt32 lastIndex = mSeparatorsCount-1;

  if (mOpenChar) { 
    lastIndex++; 
    openIndex = lastIndex; 
  }
  if (mCloseChar) { 
    lastIndex++;
    closeIndex = lastIndex;
  }
  if (aIndex < 0 || aIndex > lastIndex) {
    return NS_ERROR_INVALID_ARG;
  }

  *aStyleContext = nsnull;
  if (aIndex < mSeparatorsCount) {
    mSeparatorsChar[aIndex].GetStyleContext(aStyleContext);
  }
  else if (aIndex == openIndex) {
    mOpenChar->GetStyleContext(aStyleContext);
  }
  else if (aIndex == closeIndex) {
    mCloseChar->GetStyleContext(aStyleContext);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmfencedFrame::SetAdditionalStyleContext(PRInt32          aIndex, 
                                                nsIStyleContext* aStyleContext)
{
  PRInt32 openIndex = -1;
  PRInt32 closeIndex = -1;
  PRInt32 lastIndex = mSeparatorsCount-1;

  if (mOpenChar) {
    lastIndex++;
    openIndex = lastIndex;
  }
  if (mCloseChar) {
    lastIndex++;
    closeIndex = lastIndex;
  }
  if (aIndex < 0 || aIndex > lastIndex) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aIndex < mSeparatorsCount) {
    mSeparatorsChar[aIndex].SetStyleContext(aStyleContext);
  }
  else if (aIndex == openIndex) {
    mOpenChar->SetStyleContext(aStyleContext);
  }
  else if (aIndex == closeIndex) {
    mCloseChar->SetStyleContext(aStyleContext);
  }
  return NS_OK;
}

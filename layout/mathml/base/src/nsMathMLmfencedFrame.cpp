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
  return rv;

  mOpenChar = nsnull;
  mCloseChar = nsnull;
  mSeparatorsChar = nsnull;
  mSeparatorsCount = 0;
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

  rv = ReCreateFencesAndSeparators();
  
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
nsMathMLmfencedFrame::ReCreateFencesAndSeparators()
{
  nsresult rv;

  RemoveFencesAndSeparators();

  nsAutoString value, data;

  //////////////  
  // see if the opening fence is there ...
  data = '('; // default as per the MathML REC
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                   nsMathMLAtoms::open_, value)) {
    value.Trim(" ");
    data = value;
  }
  if (0 < data.Length()) {
    mOpenChar = new nsMathMLChar;
    if (!mOpenChar) return NS_ERROR_OUT_OF_MEMORY;
    mOpenChar->SetData(data);
  }
  //////////////
  // see if the closing fence is there ...
  data = ')'; // default as per the MathML REC
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                   nsMathMLAtoms::close_, value)) {
    value.Trim(" ");
    data = value;
  }
  if (0 < data.Length()) {
    mCloseChar = new nsMathMLChar;
    if (!mCloseChar) return NS_ERROR_OUT_OF_MEMORY;
    mCloseChar->SetData(data);
  }
  //////////////
  // see if separators are there ...
  data = ','; // default as per the MathML REC
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                   nsMathMLAtoms::separators_, value)) {
    value.Trim(" ");
    data = value;
  }
  mSeparatorsCount = data.Length();
  if (0 < mSeparatorsCount) {
    PRInt32 sepCount = -1;
    nsIFrame* childFrame = mFrames.FirstChild();
    while (childFrame) {
      if (!IsOnlyWhitespace(childFrame)) {
        sepCount++;
      }
      rv = childFrame->GetNextSibling(&childFrame);
      NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
    }
    if (0 < sepCount) {
      mSeparatorsChar = new nsMathMLChar[sepCount];
      if (!mSeparatorsChar) return NS_ERROR_OUT_OF_MEMORY;
      nsAutoString sepChar;
      for (PRInt32 i = 0; i < sepCount; i++) {
        sepChar = (i < mSeparatorsCount) ? data[i] : data[mSeparatorsCount-1]; 
        mSeparatorsChar[i].SetData(sepChar);
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
                            nsFramePaintLayer    aWhichLayer)
{
  nsresult rv = NS_OK;

  /////////////
  // paint the content
  rv = nsMathMLContainerFrame::Paint(aPresContext, aRenderingContext, 
                                     aDirtyRect, aWhichLayer);
  ////////////
  // paint fences and separators
  if (NS_SUCCEEDED(rv) && NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    if (mOpenChar) mOpenChar->Paint(aPresContext, aRenderingContext, mStyleContext);
    if (mCloseChar) mCloseChar->Paint(aPresContext, aRenderingContext, mStyleContext);
    for (PRInt32 i = 0; i < mSeparatorsCount; i++) {
      mSeparatorsChar[i].Paint(aPresContext, aRenderingContext, mStyleContext);
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
  nsresult rv = NS_OK;
  nsReflowStatus childStatus;
  nsHTMLReflowMetrics childDesiredSize(aDesiredSize.maxElementSize);
  nsSize availSize(aReflowState.mComputedWidth, aReflowState.mComputedHeight);

  //////////////////
  // Reflow Children

  nsRect rect;
  aDesiredSize.width = aDesiredSize.height = aDesiredSize.ascent = aDesiredSize.descent = 0;
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
      rv = ReflowChild(childFrame, aPresContext,
                       childDesiredSize, childReflowState, childStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(childStatus), "bad status");
      if (NS_FAILED(rv)) {
        return rv;
      }

      // At this stage, the origin points of the children have no use, so we
      // will use the origins as placeholders to store the child's ascent and
      // descent. Before return, we should set the origins so as to overwrite
      // what we are storing there now.
      childFrame->SetRect(aPresContext,
                          nsRect(childDesiredSize.descent, childDesiredSize.ascent,
                                 childDesiredSize.width, childDesiredSize.height));

      aDesiredSize.width += childDesiredSize.width;
      if (aDesiredSize.ascent < childDesiredSize.ascent) {
        aDesiredSize.ascent = childDesiredSize.ascent;
      }
      if (aDesiredSize.descent < childDesiredSize.descent) {
        aDesiredSize.descent = childDesiredSize.descent;
      }
    }
    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
  }
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;

  /////////////
  // Ask stretchy children to stretch themselves
  
  nsStretchDirection stretchDir = NS_STRETCH_DIRECTION_VERTICAL;
  nsStretchMetrics parentSize(aDesiredSize);
  aDesiredSize.width = aDesiredSize.height = aDesiredSize.ascent = aDesiredSize.descent = 0;
    
  childFrame = mFrames.FirstChild();
  while (childFrame) {
    // retrieve the metrics that was stored at the previous pass
    childFrame->GetRect(rect);
    nsStretchMetrics childSize(rect.x, rect.y, rect.width, rect.height);
    //////////
    // Stretch ...
    // Only directed at frames that implement the nsIMathMLFrame interface
    nsIMathMLFrame* aMathMLFrame;
    rv = childFrame->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&aMathMLFrame);
    if (NS_SUCCEEDED(rv) && aMathMLFrame) {
      nsIRenderingContext& renderingContext = *aReflowState.rendContext;
      aMathMLFrame->Stretch(aPresContext, renderingContext, 
                            stretchDir, parentSize, childSize);
      // store the updated metrics
      childFrame->SetRect(aPresContext,
                          nsRect(childSize.descent, childSize.ascent,
                                 childSize.width, childSize.height));
    }
    aDesiredSize.width += childSize.width;
    if (aDesiredSize.ascent < childSize.ascent) {
      aDesiredSize.ascent = childSize.ascent;
    }
    if (aDesiredSize.descent < childSize.descent) {
      aDesiredSize.descent = childSize.descent;
    }
    childFrame->GetNextSibling(&childFrame);
  }
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;

  //////////////////////////////////////////
  // Prepare the opening fence, separators, and closing fence, and
  // adjust the x-origin of children.

  nsIRenderingContext& renderingContext = *aReflowState.rendContext;
  nsStyleFont font;
  mStyleContext->GetStyle(eStyleStruct_Font, font);
  renderingContext.SetFont(font.mFont);

  // grab some metrics that will help to adjust the placements
  nsCOMPtr<nsIFontMetrics> fm;
  nscoord fontAscent, fontDescent, em; 
  renderingContext.GetFontMetrics(*getter_AddRefs(fm));
  fm->GetMaxAscent(fontAscent);
  fm->GetMaxDescent(fontDescent);
  em = NSToCoordRound(float(font.mFont.size));
  parentSize = nsStretchMetrics(aDesiredSize);

  nscoord dx = 0; // running x-origin of children ...

  /////////////////
  // opening fence ...
  ReflowChar(aPresContext, renderingContext, mStyleContext, mOpenChar, NS_MATHML_OPERATOR_FORM_PREFIX,
             mScriptLevel, fontAscent, fontDescent, em, parentSize, aDesiredSize, dx);
  /////////////////
  // separators ...
  PRInt32 i = 0;
  childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (!IsOnlyWhitespace(childFrame)) {
      childFrame->GetRect(rect);
      childFrame->MoveTo(aPresContext, dx, rect.y);
      dx += rect.width;
      if (i < mSeparatorsCount) {
        ReflowChar(aPresContext, renderingContext, mStyleContext, &mSeparatorsChar[i], NS_MATHML_OPERATOR_FORM_INFIX,
                   mScriptLevel, fontAscent, fontDescent, em, parentSize, aDesiredSize, dx);
        i++;
      }
    }
    childFrame->GetNextSibling(&childFrame);
  }
  /////////////////
  // closing fence ...
  ReflowChar(aPresContext, renderingContext, mStyleContext, mCloseChar, NS_MATHML_OPERATOR_FORM_POSTFIX,
             mScriptLevel, fontAscent, fontDescent, em, parentSize, aDesiredSize, dx);

  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;

  //////////////////
  // Adjust the y-origin of each child.
  
  childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (!IsOnlyWhitespace(childFrame)) {
      childFrame->GetRect(rect);

      // childFrame->MoveTo(aPresContext, rect.x, aDesiredSize.ascent - rect.y);
      nsHTMLReflowMetrics childSize(nsnull);
      childSize.width = rect.width;
      childSize.height = rect.height;
      FinishReflowChild(childFrame, aPresContext, childSize, rect.x, aDesiredSize.ascent - rect.y, 0);
    }
    childFrame->GetNextSibling(&childFrame);
  }
  if (mOpenChar) {
    mOpenChar->GetRect(rect);
    mOpenChar->SetRect(nsRect(rect.x, aDesiredSize.ascent - rect.y, rect.width, rect.height));
  }
  if (mCloseChar) {
    mCloseChar->GetRect(rect);
    mCloseChar->SetRect(nsRect(rect.x, aDesiredSize.ascent - rect.y, rect.width, rect.height));
  }
  for (i = 0; i < mSeparatorsCount; i++) {
    mSeparatorsChar[i].GetRect(rect);
    mSeparatorsChar[i].SetRect(nsRect(rect.x, aDesiredSize.ascent - rect.y, rect.width, rect.height));
  }

  if (aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

// helper function to perform the common task of formatting our chars
nsresult
nsMathMLmfencedFrame::ReflowChar(nsIPresContext*      aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 nsIStyleContext*     aStyleContext,
                                 nsMathMLChar*        aMathMLChar,
                                 nsOperatorFlags      aForm,
                                 PRInt32              aScriptLevel,
                                 nscoord              fontAscent,
                                 nscoord              fontDescent,
                                 nscoord              em,
                                 nsStretchMetrics&    aContainerSize,
                                 nsHTMLReflowMetrics& aDesiredSize,
                                 nscoord&             aX)
{
  if (aMathMLChar && 0 < aMathMLChar->Length()) {
    nsStretchMetrics aCharSize(fontDescent, fontAscent, 0, fontDescent + fontAscent);
    aRenderingContext.GetWidth(aMathMLChar->GetUnicode(), 
                               PRUint32(aMathMLChar->Length()), 
                               aCharSize.width);
    nsOperatorFlags aFlags;
    float aLeftSpace = 0.0f;
    float aRightSpace = 0.0f;
    nsAutoString aData;
    aMathMLChar->GetData(aData);
    PRBool found = nsMathMLOperators::LookupOperator(aData, aForm,              
                                                     &aFlags, &aLeftSpace, &aRightSpace);
    if (found) {
      // If we don't want extra space when we are a script
      if (aScriptLevel > 0) {
        aLeftSpace /= 2.0f;
        aRightSpace /= 2.0f;
      }
      if (NS_MATHML_OPERATOR_IS_STRETCHY(aFlags)) {
        // Stretch the operator to the appropriate height if it is not big enough.
        aMathMLChar->Stretch(aPresContext, aRenderingContext, aStyleContext,
                             NS_STRETCH_DIRECTION_VERTICAL,
                             aContainerSize, aCharSize);
      }
    }
    else { // call an involutive Stretch() to properly initialize and prepare the char
      aMathMLChar->Stretch(aPresContext, aRenderingContext, aStyleContext,
                           NS_STRETCH_DIRECTION_VERTICAL,
                           aCharSize, aCharSize);
    }
    if (aDesiredSize.ascent < aCharSize.ascent) 
      aDesiredSize.ascent = aCharSize.ascent;
    if (aDesiredSize.descent < aCharSize.descent) 
      aDesiredSize.descent = aCharSize.descent;

    // y-origin is used to store the ascent ... 
    aMathMLChar->SetRect(nsRect(aX + nscoord( aLeftSpace * em ), 
                                aCharSize.ascent, aCharSize.width, aCharSize.height));
    // account the spacing
    aCharSize.width += nscoord( (aLeftSpace + aRightSpace) * em );

    aDesiredSize.width += aCharSize.width;
    aX += aCharSize.width;
  }

  return NS_OK;
}

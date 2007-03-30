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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsUnitConversion.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"

#include "nsMathMLmfencedFrame.h"

//
// <mfenced> -- surround content with a pair of fences
//

nsIFrame*
NS_NewMathMLmfencedFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmfencedFrame(aContext);
}

nsMathMLmfencedFrame::~nsMathMLmfencedFrame()
{
  RemoveFencesAndSeparators();
}

NS_IMETHODIMP
nsMathMLmfencedFrame::InheritAutomaticData(nsIFrame* aParent)
{
  // let the base class get the default from our parent
  nsMathMLContainerFrame::InheritAutomaticData(aParent);

  mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY;

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmfencedFrame::SetInitialChildList(nsIAtom*        aListName,
                                          nsIFrame*       aChildList)
{
  // First, let the base class do its work
  nsresult rv = nsMathMLContainerFrame::SetInitialChildList(aListName, aChildList);
  if (NS_FAILED(rv)) return rv;

  // No need to tract the style contexts given to our MathML chars. 
  // The Style System will use Get/SetAdditionalStyleContext() to keep them
  // up-to-date if dynamic changes arise.
  return CreateFencesAndSeparators(PresContext());
}

NS_IMETHODIMP
nsMathMLmfencedFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                       nsIAtom*        aAttribute,
                                       PRInt32         aModType)
{
  RemoveFencesAndSeparators();
  CreateFencesAndSeparators(PresContext());

  return nsMathMLContainerFrame::
         AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

nsresult
nsMathMLmfencedFrame::ChildListChanged(PRInt32 aModType)
{
  RemoveFencesAndSeparators();
  CreateFencesAndSeparators(PresContext());

  return nsMathMLContainerFrame::ChildListChanged(aModType);
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
nsMathMLmfencedFrame::CreateFencesAndSeparators(nsPresContext* aPresContext)
{
  nsAutoString value;
  PRBool isMutable = PR_FALSE;

  //////////////  
  // see if the opening fence is there ...
  if (!GetAttribute(mContent, mPresentationData.mstyle, nsGkAtoms::open,
                    value)) {
    value = PRUnichar('('); // default as per the MathML REC
  }
  else {
    value.Trim(" ");
  }

  if (!value.IsEmpty()) {
    mOpenChar = new nsMathMLChar;
    if (!mOpenChar) return NS_ERROR_OUT_OF_MEMORY;
    mOpenChar->SetData(aPresContext, value);
    isMutable = nsMathMLOperators::IsMutableOperator(value);
    ResolveMathMLCharStyle(aPresContext, mContent, mStyleContext, mOpenChar, isMutable);
  }

  //////////////
  // see if the closing fence is there ...
  if(!GetAttribute(mContent, mPresentationData.mstyle,
                    nsGkAtoms::close, value)) {
    value = PRUnichar(')'); // default as per the MathML REC
  }
  else {
    value.Trim(" ");
  }

  if (!value.IsEmpty()) {
    mCloseChar = new nsMathMLChar;
    if (!mCloseChar) return NS_ERROR_OUT_OF_MEMORY;
    mCloseChar->SetData(aPresContext, value);
    isMutable = nsMathMLOperators::IsMutableOperator(value);
    ResolveMathMLCharStyle(aPresContext, mContent, mStyleContext, mCloseChar, isMutable);
  }

  //////////////
  // see if separators are there ...
  if(!GetAttribute(mContent, mPresentationData.mstyle, 
                   nsGkAtoms::separators_, value)) {
    value = PRUnichar(','); // default as per the MathML REC
  }
  else {
    value.Trim(" ");
  }

  mSeparatorsCount = value.Length();
  if (0 < mSeparatorsCount) {
    PRInt32 sepCount = mFrames.GetLength() - 1;
    if (0 < sepCount) {
      mSeparatorsChar = new nsMathMLChar[sepCount];
      if (!mSeparatorsChar) return NS_ERROR_OUT_OF_MEMORY;
      nsAutoString sepChar;
      for (PRInt32 i = 0; i < sepCount; i++) {
        if (i < mSeparatorsCount) {
          sepChar = value[i];
          isMutable = nsMathMLOperators::IsMutableOperator(sepChar);
        }
        else {
          sepChar = value[mSeparatorsCount-1];
          // keep the value of isMutable that was set earlier
        }
        mSeparatorsChar[i].SetData(aPresContext, sepChar);
        ResolveMathMLCharStyle(aPresContext, mContent, mStyleContext, &mSeparatorsChar[i], isMutable);
      }
    }
    mSeparatorsCount = sepCount;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmfencedFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                       const nsRect&           aDirtyRect,
                                       const nsDisplayListSet& aLists)
{
  /////////////
  // display the content
  nsresult rv = nsMathMLContainerFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
  NS_ENSURE_SUCCESS(rv, rv);
  
  ////////////
  // display fences and separators
  if (mOpenChar) {
    rv = mOpenChar->Display(aBuilder, this, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  if (mCloseChar) {
    rv = mCloseChar->Display(aBuilder, this, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  for (PRInt32 i = 0; i < mSeparatorsCount; i++) {
    rv = mSeparatorsChar[i].Display(aBuilder, this, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmfencedFrame::Reflow(nsPresContext*          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  return doReflow(aPresContext, aReflowState, aDesiredSize, aStatus, this,
                  mOpenChar, mCloseChar, mSeparatorsChar, mSeparatorsCount);
}

// exported routine that both mfenced and mfrac share.
// mfrac uses this when its bevelled attribute is set.
/*static*/ nsresult
nsMathMLmfencedFrame::doReflow(nsPresContext*          aPresContext,
                               const nsHTMLReflowState& aReflowState,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               nsReflowStatus&          aStatus,
                               nsIFrame*                aForFrame,
                               nsMathMLChar*            aOpenChar,
                               nsMathMLChar*            aCloseChar,
                               nsMathMLChar*            aSeparatorsChar,
                               PRInt32                  aSeparatorsCount)

{
  nsresult rv;
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;
  aDesiredSize.mBoundingMetrics.Clear();

  nsMathMLContainerFrame* mathMLFrame =
    NS_STATIC_CAST(nsMathMLContainerFrame*, aForFrame);

  PRInt32 i;
  nsCOMPtr<nsIFontMetrics> fm;
  aReflowState.rendContext->SetFont(aForFrame->GetStyleFont()->mFont, nsnull);
  aReflowState.rendContext->GetFontMetrics(*getter_AddRefs(fm));
  nscoord axisHeight, em;
  GetAxisHeight(*aReflowState.rendContext, fm, axisHeight);
  GetEmHeight(fm, em);
  // leading to be left at the top and the bottom of stretched chars
  nscoord leading = NSToCoordRound(0.2f * em); 

  /////////////
  // Reflow children
  // Asking each child to cache its bounding metrics

  // Note that we don't use the base method nsMathMLContainerFrame::Reflow()
  // because we want to stretch our fences, separators and stretchy frames using
  // the *same* initial aDesiredSize.mBoundingMetrics. If we were to use the base
  // method here, our stretchy frames will be stretched and placed, and we may
  // end up stretching our fences/separators with a different aDesiredSize.
  // XXX The above decision was revisited in bug 121748 and this code can be
  // refactored to use nsMathMLContainerFrame::Reflow() at some stage.

  PRInt32 count = 0;
  nsReflowStatus childStatus;
  nsSize availSize(aReflowState.ComputedWidth(), aReflowState.mComputedHeight);
  nsHTMLReflowMetrics childDesiredSize(
                      aDesiredSize.mFlags | NS_REFLOW_CALC_BOUNDING_METRICS);
  nsIFrame* firstChild = aForFrame->GetFirstChild(nsnull);
  nsIFrame* childFrame = firstChild;
  nscoord ascent = 0, descent = 0;
  if (firstChild || aOpenChar || aCloseChar || aSeparatorsCount > 0) {
    // We use the ASCII metrics to get our minimum height. This way, if we have
    // borders or a background, they will fit better with other elements on the line
    fm->GetMaxAscent(ascent);
    fm->GetMaxDescent(descent);
  }
  while (childFrame) {
    nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                       childFrame, availSize);
    rv = mathMLFrame->ReflowChild(childFrame, aPresContext, childDesiredSize,
                                  childReflowState, childStatus);
    //NS_ASSERTION(NS_FRAME_IS_COMPLETE(childStatus), "bad status");
    if (NS_FAILED(rv)) {
      // Call DidReflow() for the child frames we successfully did reflow.
      mathMLFrame->DidReflowChildren(firstChild, childFrame);
      return rv;
    }

    // At this stage, the origin points of the children have no use, so we will use the
    // origins as placeholders to store the child's ascent and descent. Later on,
    // we should set the origins so as to overwrite what we are storing there now.
    childFrame->SetRect(nsRect(0, childDesiredSize.ascent,
                               childDesiredSize.width, childDesiredSize.height));

    // compute the bounding metrics right now for mfrac
    nscoord childDescent = childDesiredSize.height - childDesiredSize.ascent;
    if (descent < childDescent)
      descent = childDescent;
    if (ascent < childDesiredSize.ascent)
      ascent = childDesiredSize.ascent;
    if (0 == count++)
      aDesiredSize.mBoundingMetrics  = childDesiredSize.mBoundingMetrics;
    else
      aDesiredSize.mBoundingMetrics += childDesiredSize.mBoundingMetrics;

    childFrame = childFrame->GetNextSibling();
  }

  /////////////
  // Ask stretchy children to stretch themselves

  nsBoundingMetrics containerSize;
  nsStretchDirection stretchDir = NS_STRETCH_DIRECTION_VERTICAL;

  nsPresentationData presentationData;
  mathMLFrame->GetPresentationData(presentationData);
  if (!NS_MATHML_WILL_STRETCH_ALL_CHILDREN_VERTICALLY(presentationData.flags)) {
    // case when the call is made for mfrac, we only need to stretch the '/' separator
    containerSize = aDesiredSize.mBoundingMetrics; // computed earlier
  }
  else {
    // case when the call is made for mfenced
    mathMLFrame->GetPreferredStretchSize(*aReflowState.rendContext,
                                         0, /* i.e., without embellishments */
                                         stretchDir, containerSize);
    childFrame = firstChild;
    while (childFrame) {
      nsIMathMLFrame* mathmlChild;
      childFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathmlChild);
      if (mathmlChild) {
        // retrieve the metrics that was stored at the previous pass
        GetReflowAndBoundingMetricsFor(childFrame, childDesiredSize, childDesiredSize.mBoundingMetrics);

        mathmlChild->Stretch(*aReflowState.rendContext, 
                             stretchDir, containerSize, childDesiredSize);
        // store the updated metrics
        childFrame->SetRect(nsRect(0, childDesiredSize.ascent,
                                   childDesiredSize.width, childDesiredSize.height));

        nscoord childDescent = childDesiredSize.height - childDesiredSize.ascent;
        if (descent < childDescent)
          descent = childDescent;
        if (ascent < childDesiredSize.ascent)
          ascent = childDesiredSize.ascent;
      }
      childFrame = childFrame->GetNextSibling();
    }
    // bug 121748: for surrounding fences & separators, use a size that covers everything
    mathMLFrame->GetPreferredStretchSize(*aReflowState.rendContext,
                                         STRETCH_CONSIDER_EMBELLISHMENTS,
                                         stretchDir, containerSize);
  }

  //////////////////////////////////////////
  // Prepare the opening fence, separators, and closing fence, and
  // adjust the origin of children.

  // we need to center around the axis
  if (firstChild) { // do nothing for an empty <mfenced></mfenced>
    nscoord delta = PR_MAX(containerSize.ascent - axisHeight, 
                           containerSize.descent + axisHeight);
    containerSize.ascent = delta + axisHeight;
    containerSize.descent = delta - axisHeight;
  }

  /////////////////
  // opening fence ...
  ReflowChar(aPresContext, *aReflowState.rendContext, aOpenChar,
             NS_MATHML_OPERATOR_FORM_PREFIX, presentationData.scriptLevel, 
             axisHeight, leading, em, containerSize, ascent, descent);
  /////////////////
  // separators ...
  for (i = 0; i < aSeparatorsCount; i++) {
    ReflowChar(aPresContext, *aReflowState.rendContext, &aSeparatorsChar[i],
               NS_MATHML_OPERATOR_FORM_INFIX, presentationData.scriptLevel,
               axisHeight, leading, em, containerSize, ascent, descent);
  }
  /////////////////
  // closing fence ...
  ReflowChar(aPresContext, *aReflowState.rendContext, aCloseChar,
             NS_MATHML_OPERATOR_FORM_POSTFIX, presentationData.scriptLevel,
             axisHeight, leading, em, containerSize, ascent, descent);

  //////////////////
  // Adjust the origins of each child.
  // and update our bounding metrics

  i = 0;
  nscoord dx = 0;
  nsBoundingMetrics bm;
  PRBool firstTime = PR_TRUE;
  if (aOpenChar) {
    PlaceChar(aOpenChar, ascent, bm, dx);
    aDesiredSize.mBoundingMetrics = bm;
    firstTime = PR_FALSE;
  }

  childFrame = firstChild;
  while (childFrame) {
    nsHTMLReflowMetrics childSize;
    GetReflowAndBoundingMetricsFor(childFrame, childSize, bm);
    if (firstTime) {
      firstTime = PR_FALSE;
      aDesiredSize.mBoundingMetrics  = bm;
    }
    else  
      aDesiredSize.mBoundingMetrics += bm;

    mathMLFrame->FinishReflowChild(childFrame, aPresContext, nsnull, childSize, 
                                   dx, ascent - childSize.ascent, 0);
    dx += childSize.width;

    if (i < aSeparatorsCount) {
      PlaceChar(&aSeparatorsChar[i], ascent, bm, dx);
      aDesiredSize.mBoundingMetrics += bm;
    }
    i++;

    childFrame = childFrame->GetNextSibling();
  }

  if (aCloseChar) {
    PlaceChar(aCloseChar, ascent, bm, dx);
    if (firstTime)
      aDesiredSize.mBoundingMetrics  = bm;
    else  
      aDesiredSize.mBoundingMetrics += bm;
  }

  aDesiredSize.width = aDesiredSize.mBoundingMetrics.width;
  aDesiredSize.height = ascent + descent;
  aDesiredSize.ascent = ascent;

  mathMLFrame->SetBoundingMetrics(aDesiredSize.mBoundingMetrics);
  mathMLFrame->SetReference(nsPoint(0, aDesiredSize.ascent));

  // see if we should fix the spacing
  mathMLFrame->FixInterFrameSpacing(aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

// helper functions to perform the common task of formatting our chars
/*static*/ nsresult
nsMathMLmfencedFrame::ReflowChar(nsPresContext*      aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 nsMathMLChar*        aMathMLChar,
                                 nsOperatorFlags      aForm,
                                 PRInt32              aScriptLevel,
                                 nscoord              axisHeight,
                                 nscoord              leading,
                                 nscoord              em,
                                 nsBoundingMetrics&   aContainerSize,
                                 nscoord&             aAscent,
                                 nscoord&             aDescent)
{
  if (aMathMLChar && 0 < aMathMLChar->Length()) {
    nsOperatorFlags flags = 0;
    float leftSpace = 0.0f;
    float rightSpace = 0.0f;

    nsAutoString data;
    aMathMLChar->GetData(data);
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

    if (NS_STRETCH_DIRECTION_UNSUPPORTED != aMathMLChar->GetStretchDirection()) {
      // has changed... so center the char around the axis
      nscoord height = charSize.ascent + charSize.descent;
      charSize.ascent = height/2 + axisHeight;
      charSize.descent = height - charSize.ascent;
    }
    else {
      // either it hasn't changed or stretching the char failed (i.e.,
      // GetBoundingMetrics failed)
      leading = 0;
      if (NS_FAILED(res)) {
        nsTextDimensions dimensions;
        aRenderingContext.GetTextDimensions(data.get(), data.Length(), dimensions);
        charSize.ascent = dimensions.ascent;
        charSize.descent = dimensions.descent;
        charSize.width = dimensions.width;
        // Set this as the bounding metrics of the MathMLChar to leave
        // the necessary room to paint the char.
        aMathMLChar->SetBoundingMetrics(charSize);
      }
    }

    if (aAscent < charSize.ascent + leading) 
      aAscent = charSize.ascent + leading;
    if (aDescent < charSize.descent + leading) 
      aDescent = charSize.descent + leading;

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

/*static*/ void
nsMathMLmfencedFrame::PlaceChar(nsMathMLChar*      aMathMLChar,
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
  if (aMathMLChar->GetStretchDirection() != NS_STRETCH_DIRECTION_UNSUPPORTED) {
    // the stretchy char will be centered around the axis
    // so we adjust the returned bounding metrics accordingly
    bm.descent = (bm.ascent + bm.descent) - rect.y;
    bm.ascent = rect.y;
  }

  aMathMLChar->SetRect(nsRect(dx + rect.x, dy, bm.width, rect.height));

  bm.leftBearing += rect.x;
  bm.rightBearing += rect.x;

  // return rect.width since it includes lspace and rspace
  bm.width = rect.width;
  dx += rect.width;
}

nscoord
nsMathMLmfencedFrame::FixInterFrameSpacing(nsHTMLReflowMetrics& aDesiredSize)
{
  nscoord gap = nsMathMLContainerFrame::FixInterFrameSpacing(aDesiredSize);
  if (!gap) return 0;

  nsRect rect;
  if (mOpenChar) {
    mOpenChar->GetRect(rect);
    rect.MoveBy(gap, 0);
    mOpenChar->SetRect(rect);
  }
  if (mCloseChar) {
    mCloseChar->GetRect(rect);
    rect.MoveBy(gap, 0);
    mCloseChar->SetRect(rect);
  }
  for (PRInt32 i = 0; i < mSeparatorsCount; i++) {
    mSeparatorsChar[i].GetRect(rect);
    rect.MoveBy(gap, 0);
    mSeparatorsChar[i].SetRect(rect);
  }
  return gap;
}

// ----------------------
// the Style System will use these to pass the proper style context to our MathMLChar
nsStyleContext*
nsMathMLmfencedFrame::GetAdditionalStyleContext(PRInt32 aIndex) const
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
    return nsnull;
  }

  if (aIndex < mSeparatorsCount) {
    return mSeparatorsChar[aIndex].GetStyleContext();
  }
  else if (aIndex == openIndex) {
    return mOpenChar->GetStyleContext();
  }
  else if (aIndex == closeIndex) {
    return mCloseChar->GetStyleContext();
  }
  return nsnull;
}

void
nsMathMLmfencedFrame::SetAdditionalStyleContext(PRInt32          aIndex, 
                                                nsStyleContext*  aStyleContext)
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
    return;
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
}

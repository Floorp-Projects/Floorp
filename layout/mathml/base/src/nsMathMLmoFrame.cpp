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

#include "nsIDOMText.h"

#include "nsMathMLmoFrame.h"

//
// <mo> -- operator, fence, or separator - implementation
//

// TODO:
// * Handle embellished operators
//   See the section "Exception for embellished operators"
//   in http://www.w3.org/TR/REC-MathML/chap3_2.html
//
// * For a markup <mo>content</mo>, if "content" is not found in
//   the operator dictionary, the REC sets default attributes :
//   fence = false
//   separator = false
//   lspace = .27777em
//   rspace = .27777em
//   stretchy = false
//   symmetric = true
//   maxsize = infinity
//   minsize = 1
//   largeop = false
//   movablelimits = false
//   accent = false
//
//   We only have to handle *lspace* and *rspace*, perhaps via a CSS padding rule
//   in mathml.css, and override the rule if the content is found?
//

// * The spacing is wrong in certain situations, e.g.// <mrow> <mo>&ForAll;</mo> <mo>+</mo></mrow>
//
//   The REC tells more about lspace and rspace attributes:
//
//   The values for lspace and rspace given here range from 0 to 6/18 em in
//   units of 1/18 em. For the invisible operators whose content is
//   "&InvisibleTimes;" or "&ApplyFunction;", it is suggested that MathML
//   renderers choose spacing in a context-sensitive way (which is an exception to
//   the static values given in the following table). For <mo>&ApplyFunction;</mo>,
//   the total spacing (lspace + rspace) in expressions such as "sin x"
//   (where the right operand doesn't start with a fence) should be greater
//   than 0; for <mo>&InvisibleTimes;</mo>, the total spacing should be greater
//   than 0 when both operands (or the nearest tokens on either side, if on
//   the baseline) are identifiers displayed in a non-slanted font (i.e., under
//   the suggested rules, when both operands are multi-character identifiers).

nsresult
NS_NewMathMLmoFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmoFrame* it = new (aPresShell) nsMathMLmoFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmoFrame::nsMathMLmoFrame()
{
}

nsMathMLmoFrame::~nsMathMLmoFrame()
{
}

NS_IMETHODIMP
nsMathMLmoFrame::Paint(nsIPresContext*      aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       const nsRect&        aDirtyRect,
                       nsFramePaintLayer    aWhichLayer)
{
  nsresult rv = NS_OK;

  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer &&
      NS_MATHML_OPERATOR_IS_MUTABLE(mFlags)) {
    rv = mMathMLChar.Paint(aPresContext,
                           aRenderingContext,
                           mStyleContext);
  }
  else { // let the base class worry about the painting
    rv = nsMathMLContainerFrame::Paint(aPresContext,
                                       aRenderingContext,
                                       aDirtyRect,
                                       aWhichLayer);
  }
  return rv;
}

NS_IMETHODIMP
nsMathMLmoFrame::Init(nsIPresContext*  aPresContext,
                      nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIStyleContext* aContext,
                      nsIFrame*        aPrevInFlow)
{
  // Let the base class do its Init()
  nsresult rv = nsMathMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // Init our local attributes
  mFlags = 0;
  mLeftSpace = 0.0f; // .27777f;
  mRightSpace = 0.0f; // .27777f;

  return rv;
}

NS_IMETHODIMP
nsMathMLmoFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                     nsIAtom*        aListName,
                                     nsIFrame*       aChildList)
{
  nsresult rv;
  rv = nsMathMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  // get the text that we enclose. // XXX aData.CompressWhitespace() ?
  nsAutoString aData;
  // PRInt32 aLength = 0;
  // kids can be comment-nodes, attribute-nodes, text-nodes...
  // we use the DOM to ensure that we only look at text-nodes...
  PRInt32 numKids;
  mContent->ChildCount(numKids);
  for (PRInt32 kid=0; kid<numKids; kid++) {
    nsCOMPtr<nsIContent> kidContent;
    mContent->ChildAt(kid, *getter_AddRefs(kidContent));
    if (kidContent.get()) {
      nsCOMPtr<nsIDOMText> kidText(do_QueryInterface(kidContent));
      if (kidText.get()) {
        // PRUint32 kidLength;
        // kidText->GetLength(&kidLength);
        // aLength += kidLength;
        nsAutoString kidData;
        kidText->GetData(kidData);
        aData += kidData;
      }
    }
  }
  // cache the operator
  mMathMLChar.SetData(aData);

  // for consistency, set the first non-empty child as the embellished child
  nsIFrame* firstChild = mFrames.FirstChild();
  while (firstChild) {
    if (!IsOnlyWhitespace(firstChild)) {
      mEmbellish.flags = NS_MATHML_EMBELLISH_OPERATOR;
      mEmbellish.firstChild = firstChild;
      mEmbellish.core = this;
      mEmbellish.direction = mMathMLChar.GetStretchDirection();
      break;
    }
    firstChild->GetNextSibling(&firstChild);
  }
  return rv;
}

void
nsMathMLmoFrame::InitData()
{
  nsresult rv = NS_OK;
  mFlags = 0;

  // find our form
  nsAutoString value;
  nsOperatorFlags aForm = NS_MATHML_OPERATOR_FORM_INFIX;
  nsIMathMLFrame* aMathMLFrame = nsnull;
  PRBool hasEmbellishAncestor = PR_FALSE;
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None,
                   nsMathMLAtoms::form_, value)) {
    if (value == "prefix")
      aForm = NS_MATHML_OPERATOR_FORM_PREFIX;
    else if (value == "postfix")
      aForm = NS_MATHML_OPERATOR_FORM_POSTFIX;

    // flag if we have an embellished ancestor
    hasEmbellishAncestor = IsEmbellishOperator(mParent);
  }
  else {
    // Get our outermost embellished container and its parent
    nsIFrame* aParent = this;
    nsIFrame* aEmbellish = this;
    do {
      aEmbellish = aParent;
      aParent->GetParent(&aParent);
    } while (IsEmbellishOperator(aParent));
    // flag if we have an embellished ancestor
    if (aEmbellish != this) {
      hasEmbellishAncestor = PR_TRUE;
    }

    // Find the position of our outermost embellished container w.r.t
    // its sibblings (frames are singly-linked together).
    //////////////
    // WHITESPACE: don't forget that whitespace doesn't count in MathML!
    // Here is the situation: we may have empty frames between us:
    // [space*] [prev] [space*] [aEmbellish] [space*] [next]
    // We want to skip them...
    // The problem looks like a regexp, we ask a little flag to help us.
    PRInt32 state = 0;
    nsIFrame* prev = nsnull;
    nsIFrame* next = nsnull;
    nsIFrame* aFrame;

    aParent->FirstChild(nsnull, &aFrame);
    while (aFrame) {
      if (aFrame == aEmbellish) { // we start looking for next
        state++;
      }
      else if (!IsOnlyWhitespace(aFrame)) {
        if (0 == state) { // we are still looking for prev
          prev = aFrame;
        }
        else if (1 == state) { // we got next
          next = aFrame;
          break; // we can exit the while loop
        }
      }
      aFrame->GetNextSibling(&aFrame);
    }

    // set our form flag depending on the position
    if (!prev && next)
      aForm = NS_MATHML_OPERATOR_FORM_PREFIX;
    else if (prev && !next)
      aForm = NS_MATHML_OPERATOR_FORM_POSTFIX;
  }

  // Lookup the operator dictionary
  nsAutoString aData;
  mMathMLChar.GetData(aData);
  PRBool found = nsMathMLOperators::LookupOperator(aData, aForm,
                                                   &mFlags, &mLeftSpace, &mRightSpace);

  // All operators are symmetric. But this symmetric flag is *not* stored in the 
  // operator dictionary and operaptors are treated as non-symmetric...
  // Uncomment the folllowing line to change this behavior. 
  // mFlags |= NS_MATHML_OPERATOR_SYMMETRIC;

  // If the operator exists in the dictionary and is stretchy, it is mutable
  if (found && NS_MATHML_OPERATOR_IS_STRETCHY(mFlags)) {
    mFlags |= NS_MATHML_OPERATOR_MUTABLE;
  }

  // Set the flag if the operator has an embellished ancestor
  if (hasEmbellishAncestor) {
    mFlags |= NS_MATHML_OPERATOR_EMBELLISH_ANCESTOR;
  }

  // If we don't want too much extra space when we are a script
  if (!hasEmbellishAncestor && 0 < mScriptLevel) {
    mLeftSpace /= 2.0f;
    mRightSpace /= 2.0f;
  }

// XXX Factor all this in nsMathMLAttributes.cpp

  // Now see if there are user-defined attributes that override the dictionary.
  // XXX If an attribute can be forced to be true when it is false in the
  // dictionary, then the following code has to change...

  // For each attribute disabled by the user, turn off its bit flag.
  // movablelimits|separator|largeop|accent|fence|stretchy|form

  nsAutoString kfalse("false"), ktrue("true");
  if (NS_MATHML_OPERATOR_IS_STRETCHY(mFlags)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None,
                     nsMathMLAtoms::stretchy_, value) && value == kfalse)
      mFlags &= ~NS_MATHML_OPERATOR_STRETCHY;
  }
  if (NS_MATHML_OPERATOR_IS_FENCE(mFlags)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None,
                     nsMathMLAtoms::fence_, value) && value == kfalse)
      mFlags &= ~NS_MATHML_OPERATOR_FENCE;
  }
  if (NS_MATHML_OPERATOR_IS_ACCENT(mFlags)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None,
                     nsMathMLAtoms::accent_, value) && value == kfalse)
      mFlags &= ~NS_MATHML_OPERATOR_ACCENT;
  }
  if (NS_MATHML_OPERATOR_IS_LARGEOP(mFlags)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None,
                     nsMathMLAtoms::largeop_, value) && value == kfalse)
      mFlags &= ~NS_MATHML_OPERATOR_LARGEOP;
  }
  if (NS_MATHML_OPERATOR_IS_SEPARATOR(mFlags)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None,
                     nsMathMLAtoms::separator_, value) && value == kfalse)
      mFlags &= ~NS_MATHML_OPERATOR_SEPARATOR;
  }
  if (NS_MATHML_OPERATOR_IS_MOVABLELIMITS(mFlags)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None,
                     nsMathMLAtoms::movablelimits_, value) && value == kfalse)
      mFlags &= ~NS_MATHML_OPERATOR_MOVABLELIMITS;
  }

  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None,
                   nsMathMLAtoms::symmetric_, value)) { 
   if (value == ktrue) mFlags |= NS_MATHML_OPERATOR_SYMMETRIC;                     
   if (value == kfalse) mFlags &= ~NS_MATHML_OPERATOR_SYMMETRIC;
  }

  // TODO: add also lspace and rspace, minsize, maxsize, later ...

  // If the stretchy attribute has been disabled, the operator is not mutable
  if (!found || !NS_MATHML_OPERATOR_IS_STRETCHY(mFlags)) {
    mFlags &= ~NS_MATHML_OPERATOR_MUTABLE;
  }

}

// NOTE: aDesiredStretchSize is an IN/OUT parameter
//       On input  - it contains our current size
//       On output - the same size or the new size that we want
NS_IMETHODIMP
nsMathMLmoFrame::Stretch(nsIPresContext*      aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         nsStretchDirection   aStretchDirection,
                         nsStretchMetrics&    aContainerSize,
                         nsStretchMetrics&    aDesiredStretchSize)
{
  if (NS_MATHML_STRETCH_WAS_DONE(mEmbellish.flags)) {
    printf("WARNING *** it is wrong to fire stretch more than once on a frame...\n");
    return NS_OK;
  }
  mEmbellish.flags |= NS_MATHML_STRETCH_DONE;

//  if (0 == mFlags) { // first time...
    InitData();
//  }


  /////////
  // See if it is okay to stretch

  if (NS_MATHML_OPERATOR_IS_MUTABLE(mFlags)) {
    nsStretchMetrics container(aContainerSize);
    // little adjustments if the operator is symmetric
    // XXX Also use maxsize and minsize to find what size we should
    //     really suggest to our MathMLChar.
    if (NS_MATHML_OPERATOR_IS_SYMMETRIC(mFlags) && 
         ((aStretchDirection == NS_STRETCH_DIRECTION_VERTICAL) ||
          (aStretchDirection == NS_STRETCH_DIRECTION_DEFAULT && 
           mMathMLChar.GetStretchDirection() == NS_STRETCH_DIRECTION_VERTICAL))) 
    {
      container.ascent = PR_MAX(container.ascent, container.descent);
      container.descent = container.ascent;
      container.height = container.ascent + container.descent; 
    }

    // let the MathMLChar stretch itself...
    nsStretchMetrics old(aDesiredStretchSize);
    mMathMLChar.Stretch(aPresContext, aRenderingContext,
                        mStyleContext, aStretchDirection,
                        container, aDesiredStretchSize);
    if (old == aDesiredStretchSize) { // hasn't changed !
      mFlags &= ~NS_MATHML_OPERATOR_MUTABLE;
    }
  }

  /////////
  // Place our children and adjust our children's offsets to leave the spacing.

  if (NS_MATHML_OPERATOR_IS_MUTABLE(mFlags)) {
    // The rendering will be handled by our MathML char
    mMathMLChar.SetRect(nsRect(0, 0, aDesiredStretchSize.width,
                               aDesiredStretchSize.height));
  }
  else {
    nsHTMLReflowMetrics aReflowMetrics(nsnull);
    Place(aPresContext, aRenderingContext, PR_TRUE, aReflowMetrics);

    // Prepare the metrics to be returned
    aDesiredStretchSize.width = aReflowMetrics.width;
    aDesiredStretchSize.height = aReflowMetrics.height;
    aDesiredStretchSize.ascent = aReflowMetrics.ascent;
    aDesiredStretchSize.descent = aReflowMetrics.descent;
  }
  aDesiredStretchSize.leftSpace = mLeftSpace;
  aDesiredStretchSize.rightSpace = mRightSpace;

  // Before we leave... there is a last item in the check-list:
  // If our parent is not embellished, it means we are the outermost embellished
  // container and so we put the spacing, otherwise we don't include the spacing,
  // the outermost embellished container will take care of it.

  if (!NS_MATHML_OPERATOR_HAS_EMBELLISH_ANCESTOR(mFlags)) {
    // Get the value of 'em'
    nsStyleFont font;
    mStyleContext->GetStyle(eStyleStruct_Font, font);
    nscoord em = NSToCoordRound(float(font.mFont.size));

    // Account the spacing
    aDesiredStretchSize.width += nscoord( (aDesiredStretchSize.leftSpace + aDesiredStretchSize.rightSpace) * em );
    nscoord dx = nscoord( mLeftSpace * em );
    if (0 == dx) return NS_OK;

    // adjust the offsets
    nsRect rect;
    if (NS_MATHML_OPERATOR_IS_MUTABLE(mFlags)) {
      mMathMLChar.GetRect(rect);
      mMathMLChar.SetRect(nsRect(rect.x + dx, rect.y, rect.width, rect.height));
    }
    else {
      nsIFrame* childFrame = mFrames.FirstChild();
      while (childFrame) {
        childFrame->GetRect(rect);
        childFrame->MoveTo(aPresContext, rect.x + dx, rect.y);
        childFrame->GetNextSibling(&childFrame);
      }
    }
  }
  return NS_OK;
}


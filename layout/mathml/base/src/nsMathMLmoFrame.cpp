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
NS_NewMathMLmoFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmoFrame* it = new nsMathMLmoFrame;
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
nsMathMLmoFrame::Paint(nsIPresContext&      aPresContext,
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
nsMathMLmoFrame::Init(nsIPresContext&  aPresContext,
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

void
nsMathMLmoFrame::InitData()
{

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

  // find our form 
  nsAutoString value;
  nsOperatorFlags aForm = NS_MATHML_OPERATOR_FORM_INFIX;

  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                   nsMathMLAtoms::form, value)) {
    if (value == "prefix") 
      aForm = NS_MATHML_OPERATOR_FORM_PREFIX;
    else if (value == "postfix") 
      aForm = NS_MATHML_OPERATOR_FORM_POSTFIX;
  }
  else { // look at our position from our parent (frames are singly-linked together).                

    //////////////
    // WHITESPACE: don't forget that whitespace doesn't count in MathML!
    // Here is the situation: we may have empty frames between us:
    // [space*] [prev] [space*] [this] [space*] [next]
    // We want to skip them... 
    // The problem looks like a regexp, we ask a little flag to help us.
   
    PRInt32 state = 0;
    nsIFrame* prev = nsnull;
    nsIFrame* next = nsnull;
    nsIFrame* aFrame;
   
    mParent->FirstChild(nsnull, &aFrame);
    while (nsnull != aFrame) {
      if (aFrame == this) { // we start looking for next
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
   
    // set our form flag depending on our position
    if (nsnull == prev && nsnull != next)
      aForm = NS_MATHML_OPERATOR_FORM_PREFIX;
    else if (nsnull != prev && nsnull == next)
      aForm = NS_MATHML_OPERATOR_FORM_POSTFIX;
  }

  // cache our form
  mFlags |= aForm;

  // Now that we know our text an our form, we can lookup the operator dictionary
  PRBool found;
  found = nsMathMLOperators::LookupOperator(aData, aForm,              
                                            &mFlags, &mLeftSpace, &mRightSpace);

  // If the operator exists in the dictionary and is stretchy, it is mutable
  if (found && NS_MATHML_OPERATOR_IS_STRETCHY(mFlags)) {
    mFlags |= NS_MATHML_OPERATOR_MUTABLE;
  }

  // If we don't want extra space when we are a script
  if (mScriptLevel > 0) {
    mLeftSpace /= 2.0f;
    mRightSpace /= 2.0f;
  }

// XXX Factor all this in nsMathMLAttributes.cpp

  // Now see if there are user-defined attributes that override the dictionary. 
  // XXX If an attribute can be forced to be true when it is false in the 
  // dictionary, then the following code has to change...
  
  // For each attribute disabled by the user, turn off its bit flag.
  // movablelimits|separator|largeop|accent|fence|stretchy|form

  nsAutoString kfalse("false");
  if (NS_MATHML_OPERATOR_IS_STRETCHY(mFlags)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                     nsMathMLAtoms::stretchy, value) && value == kfalse)
      mFlags &= ~NS_MATHML_OPERATOR_STRETCHY;
  }
  if (NS_MATHML_OPERATOR_IS_FENCE(mFlags)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                     nsMathMLAtoms::fence, value) && value == kfalse)
      mFlags &= ~NS_MATHML_OPERATOR_FENCE;
  }
  if (NS_MATHML_OPERATOR_IS_ACCENT(mFlags)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                     nsMathMLAtoms::accent, value) && value == kfalse)
      mFlags &= ~NS_MATHML_OPERATOR_ACCENT;
  }
  if (NS_MATHML_OPERATOR_IS_LARGEOP(mFlags)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                     nsMathMLAtoms::largeop, value) && value == kfalse)
      mFlags &= ~NS_MATHML_OPERATOR_LARGEOP;
  }
  if (NS_MATHML_OPERATOR_IS_SEPARATOR(mFlags)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                     nsMathMLAtoms::separator, value) && value == kfalse)
      mFlags &= ~NS_MATHML_OPERATOR_SEPARATOR;
  }
  if (NS_MATHML_OPERATOR_IS_MOVABLELIMITS(mFlags)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                     nsMathMLAtoms::movablelimits, value) && value == kfalse)
      mFlags &= ~NS_MATHML_OPERATOR_MOVABLELIMITS;
  }

  // TODO: add also lspace and rspace, minsize, maxsize later ...

  // If the stretchy attribute has been disabled, the operator is not mutable
  if (!found || !NS_MATHML_OPERATOR_IS_STRETCHY(mFlags)) {
    mFlags &= ~NS_MATHML_OPERATOR_MUTABLE;
    return;
  }

  // cache the operator
  mMathMLChar.SetData(aData);

  return;
}

// NOTE: aDesiredStretchSize is an IN/OUT parameter
//       On input  - it contains our current size
//       On output - the same size or the new size that we want
NS_IMETHODIMP
nsMathMLmoFrame::Stretch(nsIPresContext&      aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         nsStretchDirection   aStretchDirection,
                         nsCharMetrics&       aContainerSize,
                         nsCharMetrics&       aDesiredStretchSize)
{
  if (0 == mFlags) { // first time...
    InitData();
  }

  // get the value of 'em';
  const nsStyleFont* aFont =
    (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
  nscoord em = NSToCoordRound(float(aFont->mFont.size));

  nscoord dx = nscoord( mLeftSpace * em );
  nscoord dy = 0;

  // see if it is okay to stretch
  if (NS_MATHML_OPERATOR_IS_MUTABLE(mFlags)) {
    nsCharMetrics old(aDesiredStretchSize);
    // let the MathMLChar stretch itself...
    mMathMLChar.Stretch(aPresContext, aRenderingContext,
                        mStyleContext, aStretchDirection,
                        aContainerSize, aDesiredStretchSize);

    if (old == aDesiredStretchSize) { // hasn't changed !
      mFlags &= ~NS_MATHML_OPERATOR_MUTABLE;
    }
    else {
      nscoord gap;
      if (aDesiredStretchSize == NS_STRETCH_DIRECTION_VERTICAL) {
        gap = aContainerSize.ascent - aDesiredStretchSize.ascent;
        if (0 < gap) dy += gap;
      } else if (aDesiredStretchSize == NS_STRETCH_DIRECTION_HORIZONTAL) {
        gap = (aContainerSize.width - aDesiredStretchSize.width)/2;
        if (0 < gap) dx += gap;
      }
      mMathMLChar.SetRect(nsRect(dx, dy, aDesiredStretchSize.width, 
                                 aDesiredStretchSize.height));
    }
  }

  // adjust our children's offsets to leave the spacing
  if (0 < dx || 0 < dy) {
    nsRect rect;
    nsIFrame* childFrame = mFrames.FirstChild();
    while (nsnull != childFrame) {
      if (!IsOnlyWhitespace(childFrame)) {
        childFrame->GetRect(rect);
        childFrame->MoveTo(&aPresContext, rect.x + dx, rect.y + dy);
        dx += rect.width; 
      }
      childFrame->GetNextSibling(&childFrame);
    }
  }

  // account the spacing
  aDesiredStretchSize.width += nscoord( (mLeftSpace + mRightSpace) * em );

  return NS_OK;
}

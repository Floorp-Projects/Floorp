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

#include "nsMathMLAtoms.h"
#include "nsMathMLParts.h"
#include "nsMathMLChar.h"
#include "nsMathMLOperators.h"
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
nsMathMLmoFrame::Init(nsIPresContext&  aPresContext,
                      nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIStyleContext* aContext,
                      nsIFrame*        aPrevInFlow)
{
  nsresult rv = NS_OK;
  rv = nsMathMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // candidly assume we are an operator until proven false -- see Stretch()
  mOperator = PR_TRUE;

  // record that the MathChar doesn't know anything yet
  mMathMLChar.SetLength(-1);  

//  mMathMLChar.Init(this, -1, 0, 0, 0.0f, 0.0f, 0);

  return rv;
}

NS_IMETHODIMP
nsMathMLmoFrame::Paint(nsIPresContext&      aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       const nsRect&        aDirtyRect,
                       nsFramePaintLayer    aWhichLayer)
{
  nsresult rv = NS_OK;
  
  if (NS_FRAME_PAINT_LAYER_FOREGROUND != aWhichLayer)
    return rv;
  
  if (mOperator && 0 < mMathMLChar.GetLength()) {
//printf("Doing the painting with mMathMLChar\n");
    rv = mMathMLChar.Paint(aPresContext, 
                           aRenderingContext,
                           mStyleContext,     
                           nsRect(0,0,mRect.width,mRect.height));
  }	
  else { // let the base class worry about the painting
//printf("Doing the painting with nsMathMLContainerFrame\n");  	
    rv = nsMathMLContainerFrame::Paint(aPresContext, 
                                       aRenderingContext,
                                       aDirtyRect, 
                                       aWhichLayer);
  }
  return rv;
}


#if 0
void DEBUG_PrintString(const nsString aString) {
#if 0
   PRUnichar* pc = (PRUnichar*)(const PRUnichar*)aString.GetUnicode();
   while (*pc) {
      if (*pc < 0x00FF)
         printf("%c", char(*pc++));
      else
         printf("[%04x]", *pc++);
   }
#endif

  PRInt32 i;
  for (i = 0; i<aString.Length(); i++) {
    PRUnichar ch = aString.CharAt(i);
    if (nsString::IsAlpha(ch) || nsString::IsDigit(ch) || ch == ' ')
      printf("%c", char(ch));
    else
      printf("[0x%04X]", ch);
  }
}
#endif

// NOTE: aDesiredStretchSize is an IN/OUT parameter
//       On input  - it contains our current size
//       On output - the same size or the new size that we want
NS_IMETHODIMP
nsMathMLmoFrame::Stretch(nsIPresContext&    aPresContext,
                         nsStretchDirection aStretchDirection,
                         nsCharMetrics&     aContainerSize,
                         nsCharMetrics&     aDesiredStretchSize)
{
  nsresult rv = NS_OK;

  if (!mOperator)   // don't wander here if we are not an operator 
    return NS_OK;   // XXX Need to set default values for lspace and rspace

  // find the text that we enclose if it is the first time we come here
  PRInt32 aLength = mMathMLChar.GetLength();
  if (0 > aLength) { // this means it is the very first time we have been here.
                     // we will not be going over all this again when the 
                     // window is repainted (resize, scroll).

    // if we exit prematurely from now on, we are surely not an operator
    mOperator = PR_FALSE;
          
    // get the text that we enclose. XXX TrimWhitespace?
    
    nsAutoString aData;
//    PRInt32 aLength = 0;
    
    // kids can be comment-nodes, attribute-nodes, text-nodes...
    // we use to DOM to ensure that we only look at text-nodes...
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

    // Look at our position from our parent (frames are singly-linked together).                
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
      rv = aFrame->GetNextSibling(&aFrame);
      NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next sibbling");
    }

    // set our form flag depending on our position
    nsOperatorFlags aForm = NS_MATHML_OPERATOR_FORM_INFIX;
    if (nsnull == prev && nsnull != next)
      aForm = NS_MATHML_OPERATOR_FORM_PREFIX;
    else if (nsnull != prev && nsnull == next)
      aForm = NS_MATHML_OPERATOR_FORM_POSTFIX;

    // Now that we know the text, we can lookup the operator dictionary to see if
    // it was right to assume that this is an operator
    // XXX aData.CompressWhitespace() ?
    nsOperatorFlags aFlags;
    float aLeftSpace, aRightSpace;                 
    mOperator = nsMathMLOperators::LookupOperator(aData, aForm,              
                                   &aFlags, &aLeftSpace, &aRightSpace);

    // If it doesn't exist in the the dictionary, stop now
    if (!mOperator) 
      return NS_OK;

    // XXX If we don't want extra space when we are a script
//    if (mScriptLevel > 0) {
//      aLeftSpace = aRightSpace = 0;
//    }

// XXX Factor all this in nsMathMLAttributes.cpp

    // Now see if there are user-defined attributes that override the dictionary. 
    // XXX If an attribute can be forced to be true when it is false in the 
    // dictionary, then the following code has to change...
   
    // For each attribute disabled by the user, turn off its bit flag.
    // movablelimits|separator|largeop|accent|fence|stretchy|form

    nsAutoString kfalse("false");
    nsAutoString value;
    if (NS_MATHML_OPERATOR_IS_STRETCHY(aFlags)) {
      if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                       nsMathMLAtoms::stretchy, value) && value == kfalse)
        aFlags &= ~NS_MATHML_OPERATOR_STRETCHY;
    }
    if (NS_MATHML_OPERATOR_IS_FENCE(aFlags)) {
      if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                       nsMathMLAtoms::fence, value) && value == kfalse)
        aFlags &= ~NS_MATHML_OPERATOR_FENCE;
    }
    if (NS_MATHML_OPERATOR_IS_ACCENT(aFlags)) {
      if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                       nsMathMLAtoms::accent, value) && value == kfalse)
        aFlags &= ~NS_MATHML_OPERATOR_ACCENT;
    }
    if (NS_MATHML_OPERATOR_IS_LARGEOP(aFlags)) {
      if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                       nsMathMLAtoms::largeop, value) && value == kfalse)
        aFlags &= ~NS_MATHML_OPERATOR_LARGEOP;
    }
    if (NS_MATHML_OPERATOR_IS_SEPARATOR(aFlags)) {
      if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                       nsMathMLAtoms::separator, value) && value == kfalse)
        aFlags &= ~NS_MATHML_OPERATOR_SEPARATOR;
    }
    if (NS_MATHML_OPERATOR_IS_MOVABLELIMITS(aFlags)) {
      if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                       nsMathMLAtoms::movablelimits, value) && value == kfalse)
        aFlags &= ~NS_MATHML_OPERATOR_MOVABLELIMITS;
    }

    // TODO: add also lspace and rspace, minsize, maxsize later ...

    // cache all what we have learnt about this operator
    mMathMLChar.Init(this, aData.Length(), aData, aFlags, 
                     aLeftSpace, aRightSpace, aStretchDirection);
  }

  // let the MathMLChar stretch itself...
  mMathMLChar.Stretch(aPresContext, mStyleContext, 
                      aContainerSize, aDesiredStretchSize);
  return rv;
}



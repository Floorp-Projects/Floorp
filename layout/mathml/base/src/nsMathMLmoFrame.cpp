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
 *   Shyjan Mahamud <mahamud@cs.cmu.edu>
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

#include "nsIDOMText.h"

#include "nsMathMLmoFrame.h"
#include "nslog.h"

NS_IMPL_LOG(nsMathMLmoFrameLog)
#define PRINTF NS_LOG_PRINTF(nsMathMLmoFrameLog)
#define FLUSH  NS_LOG_FLUSH(nsMathMLmoFrameLog)

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


// additional style context to be used by our MathMLChar.
#define NS_MATHML_CHAR_STYLE_CONTEXT_INDEX   0

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

  if (NS_MATHML_OPERATOR_GET_FORM(mFlags)) {
    rv = mMathMLChar.Paint(aPresContext, aRenderingContext,
                           aDirtyRect, aWhichLayer, this);
#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
    // for visual debug
    if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer &&
        NS_MATHML_PAINT_BOUNDING_METRICS(mPresentationData.flags))
    {
      aRenderingContext.SetColor(NS_RGB(0,255,255));
      nscoord x = mReference.x + mBoundingMetrics.leftBearing;
      nscoord y = mReference.y - mBoundingMetrics.ascent;
      nscoord w = mBoundingMetrics.rightBearing - mBoundingMetrics.leftBearing;
      nscoord h = mBoundingMetrics.ascent + mBoundingMetrics.descent;
      aRenderingContext.DrawRect(x,y,w,h);
    }
#endif
  }
  else { // let the base class worry about the painting
    rv = nsMathMLContainerFrame::Paint(aPresContext, aRenderingContext,
                                       aDirtyRect, aWhichLayer);
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
  mMinSize = float(NS_UNCONSTRAINEDSIZE);
  mMaxSize = float(NS_UNCONSTRAINEDSIZE);

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
  mMathMLChar.SetData(aPresContext, aData);
  PRBool isMutable = ResolveMathMLCharStyle(aPresContext, mContent, mStyleContext, &mMathMLChar);
  if (isMutable) {
    mFlags |= NS_MATHML_OPERATOR_MUTABLE;
  }

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
  mPresentationData.flags |= NS_MATHML_SHOW_BOUNDING_METRICS;
#endif
  return rv;
}

NS_IMETHODIMP
nsMathMLmoFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                     nsIAtom*        aListName,
                                     nsIFrame*       aChildList)
{
  nsresult rv;
  rv = nsMathMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  // fill our mEmbellishData member variable
  nsIFrame* firstChild = mFrames.FirstChild();
  while (firstChild) {
    if (!IsOnlyWhitespace(firstChild)) {
      mEmbellishData.flags |= NS_MATHML_EMBELLISH_OPERATOR;
      mEmbellishData.core = this;
      mEmbellishData.direction = mMathMLChar.GetStretchDirection();

      // for consistency, set the first non-empty child as the embellished child
      mEmbellishData.firstChild = firstChild;

      // there are two extra things that we need to record so that if our
      // parent is <mover>, <munder>, or <munderover>, they will treat us properly:
      // 1) do we have accent="true"
      // 2) do we have movablelimits="true"

      // they need the extra information to decide how to treat their scripts/limits
      // (note: <mover>, <munder>, or <munderover> need not necessarily be our
      // direct parent -- case of embellished operators)

      mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENT; // default is false
      mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_MOVABLELIMITS; // default is false

      nsAutoString value;
      PRBool accentAttribute = PR_FALSE;
      PRBool movablelimitsAttribute = PR_FALSE;

      // see if the accent attribute is there
      if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                       nsMathMLAtoms::accent_, value))
      {
        accentAttribute = PR_TRUE;
        if (value.EqualsWithConversion("true"))
        {
          mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENT;
        }
      }

      // see if the movablelimits attribute is there
      if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                       nsMathMLAtoms::movablelimits_, value))
      {
        movablelimitsAttribute = PR_TRUE;
        if (value.EqualsWithConversion("true"))
        {
          mEmbellishData.flags |= NS_MATHML_EMBELLISH_MOVABLELIMITS;
        }
      }

      if (!accentAttribute || !movablelimitsAttribute) {
        // If we reach here, it means one or both attributes are missing.
        // Unfortunately, we have to lookup the dictionary to see who
        // we are, i.e., two lookups, counting also the one in Stretch()!
        // The lookup in Stretch() assumes that the surrounding frame tree
        // is already fully constructed, which is not true at this stage.

        // all accent="true" in the dictionary have form="postfix"
        // XXX should we check if the form attribute is there?
        nsAutoString aData;
        mMathMLChar.GetData(aData);
        nsOperatorFlags aForm = NS_MATHML_OPERATOR_FORM_POSTFIX;
        nsOperatorFlags aFlags = 0;
        float aLeftSpace, aRightSpace;
        PRBool found = nsMathMLOperators::LookupOperator(aData, aForm,
                                               &aFlags, &aLeftSpace, &aRightSpace);

        if (found && !accentAttribute && NS_MATHML_OPERATOR_IS_ACCENT(aFlags))
        {
          mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENT;
        }

        // all movablemits="true" in the dictionary have form="prefix",
        // but this doesn't matter here, as the lookup has returned whatever
        // is in the dictionary
        if (found && !movablelimitsAttribute && NS_MATHML_OPERATOR_IS_MOVABLELIMITS(aFlags))
        {
          mEmbellishData.flags |= NS_MATHML_EMBELLISH_MOVABLELIMITS;
        }
      }
      break;
    }
    firstChild->GetNextSibling(&firstChild);
  }
  return rv;
}

void
nsMathMLmoFrame::InitData(nsIPresContext* aPresContext)
{
  nsresult rv = NS_OK;

  // Remember the mutable bit from Init().
  // Some chars are listed under different forms in the dictionary,
  // and there could be a form under which the char is mutable.
  // If the char is the core of an embellished container, we will keep
  // it mutable irrespective of the form of the embellished container
  mFlags &= NS_MATHML_OPERATOR_MUTABLE;

  // find our form
  nsAutoString value;
  nsOperatorFlags aForm = NS_MATHML_OPERATOR_FORM_INFIX;

  nsIMathMLFrame* aMathMLFrame = nsnull;
  nsEmbellishData embellishData;

  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::form_, value)) {
    if (value.EqualsWithConversion("prefix"))
      aForm = NS_MATHML_OPERATOR_FORM_PREFIX;
    else if (value.EqualsWithConversion("postfix"))
      aForm = NS_MATHML_OPERATOR_FORM_POSTFIX;

    // see if we have an embellished ancestor, and check that we are really
    // the 'core' of the ancestor, not just a sibling of the core
    rv = mParent->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&aMathMLFrame);
    if (NS_SUCCEEDED(rv) && aMathMLFrame) {
      aMathMLFrame->GetEmbellishData(embellishData);
      if (embellishData.core == this) {
        // flag if we have an embellished ancestor
        mFlags |= NS_MATHML_OPERATOR_EMBELLISH_ANCESTOR;
      }
    }
  }
  else {
    // Get our outermost embellished container and its parent
    nsIFrame* embellishAncestor;
    nsIFrame* aParent = this;
    do {
      embellishAncestor = aParent;
      embellishAncestor->GetParent(&aParent);
      rv = aParent->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&aMathMLFrame);
      if (NS_SUCCEEDED(rv) && aMathMLFrame) {
        aMathMLFrame->GetEmbellishData(embellishData);
      }
      else {
      	embellishData.core = nsnull;
      }
    } while (embellishData.core == this);
    // flag if we have an embellished ancestor
    if (embellishAncestor != this)
    {
      mFlags |= NS_MATHML_OPERATOR_EMBELLISH_ANCESTOR;
    }

    // Find the position of our outermost embellished container w.r.t
    // its siblings (frames are singly-linked together).
    //////////////
    // WHITESPACE: don't forget that whitespace doesn't count in MathML!
    // Here is the situation: we may have empty frames between us:
    // [space*] [prev] [space*] [embellishAncestor] [space*] [next]
    // We want to skip them...
    // The problem looks like a regexp, we ask a little flag to help us.
    PRInt32 state = 0;
    nsIFrame* prev = nsnull;
    nsIFrame* next = nsnull;
    nsIFrame* aFrame;

    aParent->FirstChild(aPresContext, nsnull, &aFrame);
    while (aFrame) {
      if (aFrame == embellishAncestor) { // we start looking for next
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
  mMathMLChar.SetData(aPresContext, aData); // XXX hack to reset the enum, bug 45010
  PRBool found = nsMathMLOperators::LookupOperator(aData, aForm,
                                                   &mFlags, &mLeftSpace, &mRightSpace);
  // All operators are symmetric. But this symmetric flag is *not* stored in
  // the Operator Dictionary ... Add it now
  mFlags |= NS_MATHML_OPERATOR_SYMMETRIC;

#if 0
  // If the operator exists in the dictionary and is stretchy or largeop, 
  // then it is mutable
  if (found && 
      (NS_MATHML_OPERATOR_IS_STRETCHY(mFlags) || 
       NS_MATHML_OPERATOR_IS_LARGEOP(mFlags)))
  {
    mFlags |= NS_MATHML_OPERATOR_MUTABLE;
  }

  // Set the flag if the operator has an embellished ancestor
  if (hasEmbellishAncestor) {
    mFlags |= NS_MATHML_OPERATOR_EMBELLISH_ANCESTOR;
  }
#endif

  // If we don't want too much extra space when we are a script
  if ((0 < mPresentationData.scriptLevel) &&
      !NS_MATHML_OPERATOR_HAS_EMBELLISH_ANCESTOR(mFlags)) {
    mLeftSpace /= 2.0f;
    mRightSpace /= 2.0f;
  }

  // Now see if there are user-defined attributes that override the dictionary.
  // XXX If an attribute can be forced to be true when it is false in the
  // dictionary, then the following code has to change...

  // For each attribute disabled by the user, turn off its bit flag.
  // movablelimits|separator|largeop|accent|fence|stretchy|form

  nsAutoString kfalse, ktrue;
  kfalse.AssignWithConversion("false");
  ktrue.AssignWithConversion("true");
  if (NS_MATHML_OPERATOR_IS_STRETCHY(mFlags)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                     nsMathMLAtoms::stretchy_, value) && value == kfalse)
      mFlags &= ~NS_MATHML_OPERATOR_STRETCHY;
  }
  if (NS_MATHML_OPERATOR_IS_FENCE(mFlags)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                     nsMathMLAtoms::fence_, value) && value == kfalse)
      mFlags &= ~NS_MATHML_OPERATOR_FENCE;
  }
  if (NS_MATHML_OPERATOR_IS_ACCENT(mFlags)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                     nsMathMLAtoms::accent_, value) && value == kfalse)
      mFlags &= ~NS_MATHML_OPERATOR_ACCENT;
  }
  if (NS_MATHML_OPERATOR_IS_LARGEOP(mFlags)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                     nsMathMLAtoms::largeop_, value) && value == kfalse)
      mFlags &= ~NS_MATHML_OPERATOR_LARGEOP;
  }
  if (NS_MATHML_OPERATOR_IS_SEPARATOR(mFlags)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                     nsMathMLAtoms::separator_, value) && value == kfalse)
      mFlags &= ~NS_MATHML_OPERATOR_SEPARATOR;
  }
  if (NS_MATHML_OPERATOR_IS_MOVABLELIMITS(mFlags)) {
    if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                     nsMathMLAtoms::movablelimits_, value) && value == kfalse)
      mFlags &= ~NS_MATHML_OPERATOR_MOVABLELIMITS;
  }

  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::symmetric_, value)) {
   if (value == kfalse) mFlags &= ~NS_MATHML_OPERATOR_SYMMETRIC;
   else if (value == ktrue) mFlags |= NS_MATHML_OPERATOR_SYMMETRIC;
  }

  // If we are an accent without explicit lspace="." or rspace=".",
  // ignore our default left/right space

  // Get the value of 'em'
  nsStyleFont font;
  mStyleContext->GetStyle(eStyleStruct_Font, font);
  float em = float(font.mFont.size);

  // lspace = number h-unit | namedspace
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::lspace_, value)) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) ||
        ParseNamedSpaceValue(mPresentationData.mstyle, value, cssValue))
    {
      if ((eCSSUnit_Number == cssValue.GetUnit()) &&
          (0.0f == cssValue.GetFloatValue()))
        mLeftSpace = 0.0f;
      else if (cssValue.IsLengthUnit())
        mLeftSpace = float(CalcLength(aPresContext, mStyleContext, cssValue)) / em;
    }
  }
  else if (NS_MATHML_EMBELLISH_IS_ACCENT(mEmbellishData.flags)) {
    mLeftSpace = 0.0f;
  }

  // rspace = number h-unit | namedspace
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::rspace_, value)) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) ||
        ParseNamedSpaceValue(mPresentationData.mstyle, value, cssValue))
    {
      if ((eCSSUnit_Number == cssValue.GetUnit()) &&
          (0.0f == cssValue.GetFloatValue()))
        mRightSpace = 0.0f;
      else if (cssValue.IsLengthUnit())
        mRightSpace = float(CalcLength(aPresContext, mStyleContext, cssValue)) / em;
    }
  }
  else if (NS_MATHML_EMBELLISH_IS_ACCENT(mEmbellishData.flags)) {
    mRightSpace = 0.0f;
  }

  // minsize = number [ v-unit | h-unit ] | namedspace
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::minsize_, value)) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) ||
        ParseNamedSpaceValue(mPresentationData.mstyle, value, cssValue))
    {
      nsCSSUnit unit = cssValue.GetUnit();
      if (eCSSUnit_Number == unit)
        mMinSize = cssValue.GetFloatValue();
      else if (eCSSUnit_Percent == unit)
        mMinSize = cssValue.GetPercentValue();
      else if (eCSSUnit_Null != unit) {
        mMinSize = float(CalcLength(aPresContext, mStyleContext, cssValue));
        mFlags |= NS_MATHML_OPERATOR_MINSIZE_EXPLICIT;
      }

      if ((eCSSUnit_Number == unit) || (eCSSUnit_Percent == unit)) {
        // see if the multiplicative inheritance should be from <mstyle>
        if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(nsnull, mPresentationData.mstyle,
                         nsMathMLAtoms::minsize_, value)) {
          if (ParseNumericValue(value, cssValue)) {
            if (cssValue.IsLengthUnit()) {
              mMinSize *= float(CalcLength(aPresContext, mStyleContext, cssValue));
              mFlags |= NS_MATHML_OPERATOR_MINSIZE_EXPLICIT;
            }
          }
        }
      }
    }
  }

  // maxsize = number [ v-unit | h-unit ] | namedspace | infinity
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::maxsize_, value)) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) ||
        ParseNamedSpaceValue(mPresentationData.mstyle, value, cssValue))
    {
      nsCSSUnit unit = cssValue.GetUnit();
      if (eCSSUnit_Number == unit)
        mMaxSize = cssValue.GetFloatValue();
      else if (eCSSUnit_Percent == unit)
        mMaxSize = cssValue.GetPercentValue();
      else if (eCSSUnit_Null != unit) {
        mMaxSize = float(CalcLength(aPresContext, mStyleContext, cssValue));
        mFlags |= NS_MATHML_OPERATOR_MAXSIZE_EXPLICIT;
      }

      if ((eCSSUnit_Number == unit) || (eCSSUnit_Percent == unit)) {
        // see if the multiplicative inheritance should be from <mstyle>
        if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(nsnull, mPresentationData.mstyle,
                         nsMathMLAtoms::maxsize_, value)) {
          if (ParseNumericValue(value, cssValue)) {
            if (cssValue.IsLengthUnit()) {
              mMaxSize *= float(CalcLength(aPresContext, mStyleContext, cssValue));
              mFlags |= NS_MATHML_OPERATOR_MAXSIZE_EXPLICIT;
            }
          }
        }
      }
    }
  }

  // If the stretchy or largeop attributes have been disabled,
  // the operator is not mutable
  if (!found ||
      (!NS_MATHML_OPERATOR_IS_STRETCHY(mFlags) &&
       !NS_MATHML_OPERATOR_IS_LARGEOP(mFlags)))
  {
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
                         nsBoundingMetrics&   aContainerSize,
                         nsHTMLReflowMetrics& aDesiredStretchSize)
{
  if (NS_MATHML_STRETCH_WAS_DONE(mEmbellishData.flags)) {
      PRINTF("WARNING *** it is wrong to fire stretch more than once on a frame...\n");
    return NS_OK;
  }
  mEmbellishData.flags |= NS_MATHML_STRETCH_DONE;

  InitData(aPresContext);

  // get the axis height;
  nsStyleFont font;
  mStyleContext->GetStyle(eStyleStruct_Font, font);
  nsCOMPtr<nsIFontMetrics> fm;
  aRenderingContext.SetFont(font.mFont);
  aRenderingContext.GetFontMetrics(*getter_AddRefs(fm));
  nscoord fontAscent, fontDescent, axisHeight, height;
  GetAxisHeight(aRenderingContext, fm, axisHeight);
  fm->GetMaxAscent(fontAscent);
  fm->GetMaxDescent(fontDescent);

  // Operators that exist in the dictionary are handled by the MathMLChar

  if (NS_MATHML_OPERATOR_GET_FORM(mFlags)) {
    nsBoundingMetrics charSize;
    nsBoundingMetrics initialSize = aDesiredStretchSize.mBoundingMetrics;
    nsBoundingMetrics container = initialSize;

    PRBool isVertical = PR_FALSE;
    PRInt32 stretchHint = NS_STRETCH_NORMAL;

    // see if it is okay to stretch
    if (NS_MATHML_OPERATOR_IS_MUTABLE(mFlags)) {

      container = aContainerSize;

      // some adjustments if the operator is symmetric and vertical

      if (((aStretchDirection == NS_STRETCH_DIRECTION_VERTICAL) ||
           (aStretchDirection == NS_STRETCH_DIRECTION_DEFAULT))  &&
          (mMathMLChar.GetStretchDirection() == NS_STRETCH_DIRECTION_VERTICAL))
      {
        isVertical = PR_TRUE;
      }

      if (isVertical && NS_MATHML_OPERATOR_IS_SYMMETRIC(mFlags))
      {
        // we need to center about the axis
        nscoord delta = PR_MAX(container.ascent - axisHeight,
                               container.descent + axisHeight);
        container.ascent = delta + axisHeight;
        container.descent = delta - axisHeight;

        // get ready in case we encounter user-desired min-max size
        delta = PR_MAX(initialSize.ascent - axisHeight,
                       initialSize.descent + axisHeight);
        initialSize.ascent = delta + axisHeight;
        initialSize.descent = delta - axisHeight;
      }

      // check for user-desired min-max size

      if (mMaxSize != float(NS_UNCONSTRAINEDSIZE) && mMaxSize > 0.0f) {
        // if we are here, there is a user defined maxsize ...
        if (NS_MATHML_OPERATOR_MAXSIZE_IS_EXPLICIT(mFlags)) {
          // there is an explicit value like maxsize="20pt"
          // try to maintain the aspect ratio of the char
          float aspect = mMaxSize / float(initialSize.ascent + initialSize.descent);
          container.ascent =
            PR_MIN(container.ascent, nscoord(initialSize.ascent * aspect));
          container.descent =
            PR_MIN(container.descent, nscoord(initialSize.descent * aspect)); 
          // below we use a type cast instead of a conversion to avoid a VC++ bug
          // see http://support.microsoft.com/support/kb/articles/Q115/7/05.ASP
          container.width =
            PR_MIN(container.width, (nscoord)mMaxSize);
        }
        else { // multiplicative value
          container.ascent =
            PR_MIN(container.ascent, nscoord(initialSize.ascent * mMaxSize));
          container.descent =
            PR_MIN(container.descent, nscoord(initialSize.descent * mMaxSize));
          container.width =
            PR_MIN(container.width, nscoord(initialSize.width * mMaxSize));
        }

        if (isVertical && !NS_MATHML_OPERATOR_IS_SYMMETRIC(mFlags))
        {
          // re-adjust to align the char with the bottom of the initial container
          height = container.ascent + container.descent;
          container.descent = aContainerSize.descent;	
          container.ascent = height - container.descent;
        }
      }

      if (mMinSize != float(NS_UNCONSTRAINEDSIZE) && mMinSize > 0.0f) {
        // if we are here, there is a user defined minsize ...
        if (NS_MATHML_OPERATOR_MINSIZE_IS_EXPLICIT(mFlags)) {
          // there is an explicit value like minsize="20pt"
          // try to maintain the aspect ratio of the char
          float aspect = mMinSize / float(initialSize.ascent + initialSize.descent);
          container.ascent =
            PR_MAX(container.ascent, nscoord(initialSize.ascent * aspect));
          container.descent =
            PR_MAX(container.descent, nscoord(initialSize.descent * aspect));
          container.width =
            PR_MAX(container.width, (nscoord)mMinSize);
        }
        else { // multiplicative value
          container.ascent =
            PR_MAX(container.ascent, nscoord(initialSize.ascent * mMinSize));
          container.descent =
            PR_MAX(container.descent, nscoord(initialSize.descent * mMinSize));
          container.width =
            PR_MAX(container.width, nscoord(initialSize.width * mMinSize));
        }

        if (isVertical && !NS_MATHML_OPERATOR_IS_SYMMETRIC(mFlags))
        {
          // re-adjust to align the char with the bottom of the initial container
          height = container.ascent + container.descent;
          container.descent = aContainerSize.descent;	
          container.ascent = height - container.descent;
        }
      }
    }

    // let the MathMLChar stretch itself...
    charSize.Clear(); // this will tell stretch that we don't know the default size
    nsresult res = mMathMLChar.Stretch(aPresContext, aRenderingContext,
                                       aStretchDirection, container, charSize, stretchHint);
    if (NS_FAILED(res)) {
    	// gracefully handle cases where stretching the char failed (i.e., GetBoundingMetrics failed)
      // clear our 'form' to behave as if the operator wasn't in the dictionary
      mFlags &= ~0x3;
    }
    else {
      // update our bounding metrics... it becomes that of our MathML char
      mMathMLChar.GetBoundingMetrics(mBoundingMetrics);
      
      if (isVertical)
      {
        // the desired size returned by mMathMLChar maybe different
        // from the size of the container.
        // the mMathMLChar.mRect.y calculation is subtle, watch out!!!
      
        height = mBoundingMetrics.ascent + mBoundingMetrics.descent;
        if (NS_MATHML_OPERATOR_IS_SYMMETRIC(mFlags)) {
          // For symmetric and vertical operators,
          // we want to center about the axis of the container
          mBoundingMetrics.descent = height/2 - axisHeight;
        }
        else {
          // Otherwise, align the char with the bottom of the container
          mBoundingMetrics.descent = container.descent;
        }
        mBoundingMetrics.ascent = height - mBoundingMetrics.descent;
      }
      
      // Prepare the metrics to return
      aDesiredStretchSize.ascent = PR_MAX(fontAscent, mBoundingMetrics.ascent); /*  + delta1*/
      aDesiredStretchSize.descent = PR_MAX(fontDescent, mBoundingMetrics.descent); /* + delta2*/
      aDesiredStretchSize.width = mBoundingMetrics.width;
      aDesiredStretchSize.height = aDesiredStretchSize.ascent + aDesiredStretchSize.descent;
      aDesiredStretchSize.mBoundingMetrics = mBoundingMetrics;
      
      nscoord dy = aDesiredStretchSize.ascent - mBoundingMetrics.ascent;
      if (mMathMLChar.GetEnum() == eMathMLChar_DONT_STRETCH)
      {
        // reset
        dy = aDesiredStretchSize.ascent - charSize.ascent;
        aDesiredStretchSize.mBoundingMetrics = mBoundingMetrics = charSize;
      }
      
      mMathMLChar.SetRect(
         nsRect(0, dy, charSize.width,
                charSize.ascent + charSize.descent));
      
      mReference.x = 0;
      mReference.y = aDesiredStretchSize.ascent;
    }
  }


  if (!NS_MATHML_OPERATOR_GET_FORM(mFlags)) {
    // Place our children using the default method
    Place(aPresContext, aRenderingContext, PR_TRUE, aDesiredStretchSize);
  }

  // Before we leave... there is a last item in the check-list:
  // If our parent is not embellished, it means we are the outermost embellished
  // container and so we put the spacing, otherwise we don't include the spacing,
  // the outermost embellished container will take care of it.

  if (!NS_MATHML_OPERATOR_HAS_EMBELLISH_ANCESTOR(mFlags)) {

    // Get the value of 'em'
    nscoord em = NSToCoordRound(float(font.mFont.size));

    // Account the spacing
    aDesiredStretchSize.width += nscoord((mLeftSpace + mRightSpace) * em);
    mBoundingMetrics.width = aDesiredStretchSize.width;
    nscoord dx = nscoord(mLeftSpace * em);

    if (0 == dx) return NS_OK;

    // adjust the offsets
    mBoundingMetrics.leftBearing += dx;
    mBoundingMetrics.rightBearing += dx;

    nsRect rect;
    if (NS_MATHML_OPERATOR_GET_FORM(mFlags)) {
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

NS_IMETHODIMP
nsMathMLmoFrame::Reflow(nsIPresContext*          aPresContext,
                        nsHTMLReflowMetrics&     aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&          aStatus)
{
  return ReflowTokenFor(this, aPresContext, aDesiredSize,
                        aReflowState, aStatus);
}

NS_IMETHODIMP
nsMathMLmoFrame::Place(nsIPresContext*      aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       PRBool               aPlaceOrigin,
                       nsHTMLReflowMetrics& aDesiredSize)
{
  return PlaceTokenFor(this, aPresContext, aRenderingContext,
                       aPlaceOrigin, aDesiredSize);
}

// ----------------------
// the Style System will use these to pass the proper style context to our MathMLChar
NS_IMETHODIMP
nsMathMLmoFrame::GetAdditionalStyleContext(PRInt32           aIndex,
                                           nsIStyleContext** aStyleContext) const
{
  NS_PRECONDITION(aStyleContext, "null OUT ptr");
  if (aIndex < 0) {
    return NS_ERROR_INVALID_ARG;
  }
  *aStyleContext = nsnull;
  switch (aIndex) {
  case NS_MATHML_CHAR_STYLE_CONTEXT_INDEX:
    mMathMLChar.GetStyleContext(aStyleContext);
    break;
  default:
    return NS_ERROR_INVALID_ARG;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmoFrame::SetAdditionalStyleContext(PRInt32          aIndex,
                                           nsIStyleContext* aStyleContext)
{
  if (aIndex < 0) {
    return NS_ERROR_INVALID_ARG;
  }
  switch (aIndex) {
  case NS_MATHML_CHAR_STYLE_CONTEXT_INDEX:
    mMathMLChar.SetStyleContext(aStyleContext);
    break;
  }
  return NS_OK;
}

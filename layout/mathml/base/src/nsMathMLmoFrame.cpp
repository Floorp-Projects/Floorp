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
                       nsFramePaintLayer    aWhichLayer,
                       PRUint32             aFlags)
{
  nsresult rv = NS_OK;
  if (NS_MATHML_OPERATOR_GET_FORM(mFlags) ||
      NS_MATHML_OPERATOR_IS_CENTERED(mFlags)) {
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

  // get the text that we enclose
  nsAutoString data;
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
        nsAutoString kidData;
        kidText->GetData(kidData);
        data += kidData;
      }
    }
  }
  // cache the operator
  mMathMLChar.SetData(aPresContext, data);
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
  if (firstChild) {
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
      if (value.Equals(NS_LITERAL_STRING("true")))
      {
        mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENT;
      }
    }
    
    // see if the movablelimits attribute is there
    if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                     nsMathMLAtoms::movablelimits_, value))
    {
      movablelimitsAttribute = PR_TRUE;
      if (value.Equals(NS_LITERAL_STRING("true")))
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
      nsAutoString data;
      mMathMLChar.GetData(data);
      nsOperatorFlags form = NS_MATHML_OPERATOR_FORM_POSTFIX;
      nsOperatorFlags flags = 0;
      float leftSpace, rightSpace;
      PRBool found = nsMathMLOperators::LookupOperator(data, form,
                                             &flags, &leftSpace, &rightSpace);
    
      if (found && !accentAttribute && NS_MATHML_OPERATOR_IS_ACCENT(flags))
      {
        mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENT;
      }
    
      // all movablemits="true" in the dictionary have form="prefix",
      // but this doesn't matter here, as the lookup has returned whatever
      // is in the dictionary
      if (found && !movablelimitsAttribute && NS_MATHML_OPERATOR_IS_MOVABLELIMITS(flags))
      {
        mEmbellishData.flags |= NS_MATHML_EMBELLISH_MOVABLELIMITS;
      }
    }
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
  nsOperatorFlags form = NS_MATHML_OPERATOR_FORM_INFIX;

  nsIMathMLFrame* mathMLFrame = nsnull;
  nsEmbellishData embellishData;

  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::form_, value)) {
    if (value.Equals(NS_LITERAL_STRING("prefix")))
      form = NS_MATHML_OPERATOR_FORM_PREFIX;
    else if (value.Equals(NS_LITERAL_STRING("postfix")))
      form = NS_MATHML_OPERATOR_FORM_POSTFIX;

    // see if we have an embellished ancestor, and check that we are really
    // the 'core' of the ancestor, not just a sibling of the core
    rv = mParent->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
    if (NS_SUCCEEDED(rv) && mathMLFrame) {
      mathMLFrame->GetEmbellishData(embellishData);
      if (embellishData.core == this) {
        // flag if we have an embellished ancestor
        mFlags |= NS_MATHML_OPERATOR_EMBELLISH_ANCESTOR;
      }
    }
  }
  else {
    // Get our outermost embellished container and its parent
    nsIFrame* embellishAncestor;
    nsIFrame* parent = this;
    do {
      embellishAncestor = parent;
      embellishAncestor->GetParent(&parent);
      rv = parent->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
      if (NS_SUCCEEDED(rv) && mathMLFrame) {
        mathMLFrame->GetEmbellishData(embellishData);
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
    nsIFrame* prev = nsnull;
    nsIFrame* next = nsnull;
    nsIFrame* frame;

    embellishAncestor->GetNextSibling(&next);
    parent->FirstChild(aPresContext, nsnull, &frame);
    while (frame) {
      if (frame == embellishAncestor) break;
      prev = frame;
      frame->GetNextSibling(&frame);
    }

    // set our form flag depending on the position
    if (!prev && next)
      form = NS_MATHML_OPERATOR_FORM_PREFIX;
    else if (prev && !next)
      form = NS_MATHML_OPERATOR_FORM_POSTFIX;
  }

  // Lookup the operator dictionary
  nsAutoString data;
  mMathMLChar.GetData(data);
  mMathMLChar.SetData(aPresContext, data); // XXX hack to reset, bug 45010
  PRBool found = nsMathMLOperators::LookupOperator(data, form,
                                                   &mFlags, &mLeftSpace, &mRightSpace);

  // If we don't want too much extra space when we are a script
  if ((0 < mPresentationData.scriptLevel) &&
      !NS_MATHML_OPERATOR_HAS_EMBELLISH_ANCESTOR(mFlags)) {
    mLeftSpace /= 2.0f;
    mRightSpace /= 2.0f;
  }

  // Now see if there are user-defined attributes that override the dictionary.
  // XXX If an attribute can be forced to be true when it is false in the
  // dictionary, then the following code has to change...

  // For each attribute overriden by the user, turn off its bit flag.
  // symmetric|movablelimits|separator|largeop|accent|fence|stretchy|form
  nsAutoString kfalse, ktrue;
  kfalse.Assign(NS_LITERAL_STRING("false"));
  ktrue.Assign(NS_LITERAL_STRING("true"));
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
  const nsStyleFont *font = NS_STATIC_CAST(const nsStyleFont*,
    mStyleContext->GetStyleData(eStyleStruct_Font));
  float em = float(font->mFont.size);

  // lspace = number h-unit | namedspace
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::lspace_, value)) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) ||
        ParseNamedSpaceValue(mPresentationData.mstyle, value, cssValue))
    {
      if ((eCSSUnit_Number == cssValue.GetUnit()) && !cssValue.GetFloatValue())
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
      if ((eCSSUnit_Number == cssValue.GetUnit()) && !cssValue.GetFloatValue())
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

  // See if this is an operator that should be centered to cater for 
  // fonts that are not math-aware
  if (1 == data.Length()) {
    PRUnichar ch = data[0];
    if ((ch == '+') || (ch == '=') || (ch == '*') ||
        (ch == 0x00D7)) { // &times;
      mFlags |= NS_MATHML_OPERATOR_CENTERED;
    }
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
    NS_WARNING("it is wrong to fire stretch more than once on a frame");
    return NS_OK;
  }
  mEmbellishData.flags |= NS_MATHML_STRETCH_DONE;

  InitData(aPresContext);

  // get the axis height;
  const nsStyleFont *font = NS_STATIC_CAST(const nsStyleFont*,
    mStyleContext->GetStyleData(eStyleStruct_Font));
  nsCOMPtr<nsIFontMetrics> fm;
  aRenderingContext.SetFont(font->mFont);
  aRenderingContext.GetFontMetrics(*getter_AddRefs(fm));
  nscoord leading, axisHeight, height;
  GetAxisHeight(aRenderingContext, fm, axisHeight);

  // Operators that exist in the dictionary, or those that are to be centered
  // to cater for fonts that are not math-aware, are handled by the MathMLChar

  if (NS_MATHML_OPERATOR_GET_FORM(mFlags) ||
      NS_MATHML_OPERATOR_IS_CENTERED(mFlags)) {
    nsBoundingMetrics charSize;
    nsBoundingMetrics initialSize = aDesiredStretchSize.mBoundingMetrics;
    nsBoundingMetrics container = initialSize;

    PRBool isVertical = PR_FALSE;
    PRUint32 stretchHint = NS_STRETCH_NORMAL;

    // see if it is okay to stretch
    if (NS_MATHML_OPERATOR_IS_MUTABLE(mFlags)) {

      container = aContainerSize;

      if (((aStretchDirection == NS_STRETCH_DIRECTION_VERTICAL) ||
           (aStretchDirection == NS_STRETCH_DIRECTION_DEFAULT))  &&
          (mMathMLChar.GetStretchDirection() == NS_STRETCH_DIRECTION_VERTICAL))
      {
        isVertical = PR_TRUE;
      }

      // see if we are in display mode, and set largeop or largeopOnly
      // . largeopOnly is taken if largeop=true and stretchy=false
      // . largeop is taken if largeop=true and stretchy=true
      if (NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags) &&
          NS_MATHML_OPERATOR_IS_LARGEOP(mFlags)) {
        stretchHint = NS_STRETCH_LARGEOP; // (largeopOnly, not mask!)
        if (NS_MATHML_OPERATOR_IS_STRETCHY(mFlags)) {
          stretchHint |= NS_STRETCH_NEARER | NS_STRETCH_LARGER;
        }
      }
      else if (isVertical) {
        // TeX hint. Can impact some sloppy markups missing <mrow></mrow>
        stretchHint = NS_STRETCH_NEARER;
      }

      // some adjustments if the operator is symmetric and vertical

      if (isVertical && NS_MATHML_OPERATOR_IS_SYMMETRIC(mFlags)) {
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

        if (isVertical && !NS_MATHML_OPERATOR_IS_SYMMETRIC(mFlags)) {
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

        if (isVertical && !NS_MATHML_OPERATOR_IS_SYMMETRIC(mFlags)) {
          // re-adjust to align the char with the bottom of the initial container
          height = container.ascent + container.descent;
          container.descent = aContainerSize.descent;
          container.ascent = height - container.descent;
        }
      }
    }

    // let the MathMLChar stretch itself...
    nsresult res = mMathMLChar.Stretch(aPresContext, aRenderingContext,
                                       aStretchDirection, container, charSize, stretchHint);
    if (NS_FAILED(res)) {
      // gracefully handle cases where stretching the char failed (i.e., GetBoundingMetrics failed)
      // clear our 'form' to behave as if the operator wasn't in the dictionary
      mFlags &= ~NS_MATHML_OPERATOR_FORM;
    }
    else {
      // update our bounding metrics... it becomes that of our MathML char
      mMathMLChar.GetBoundingMetrics(mBoundingMetrics);

      // if the returned direction is 'unsupported', the char didn't actually change. 
      // So we do the centering only if necessary
      if ((mMathMLChar.GetStretchDirection() != NS_STRETCH_DIRECTION_UNSUPPORTED)
          || NS_MATHML_OPERATOR_IS_CENTERED(mFlags)) {

        if (isVertical || NS_MATHML_OPERATOR_IS_CENTERED(mFlags)) {
          // the desired size returned by mMathMLChar maybe different
          // from the size of the container.
          // the mMathMLChar.mRect.y calculation is subtle, watch out!!!

          height = mBoundingMetrics.ascent + mBoundingMetrics.descent;
          if (NS_MATHML_OPERATOR_IS_SYMMETRIC(mFlags) ||
              NS_MATHML_OPERATOR_IS_CENTERED(mFlags)) {
            // For symmetric and vertical operators, or for operators that are always
            // centered ('+', '*', etc) we want to center about the axis of the container
            mBoundingMetrics.descent = height/2 - axisHeight;
          }
          else {
            // Otherwise, align the char with the bottom of the container
            mBoundingMetrics.descent = container.descent;
          }
          mBoundingMetrics.ascent = height - mBoundingMetrics.descent;
        }

        // get the leading to be left at the top and the bottom of the stretched char
        // this seems more reliable than using fm->GetLeading() on suspicious fonts               
        float em = float(font->mFont.size);
        leading = nscoord(0.2f * em); 

        aDesiredStretchSize.ascent = mBoundingMetrics.ascent + leading;
        aDesiredStretchSize.descent = mBoundingMetrics.descent + leading;
      }
      aDesiredStretchSize.height = aDesiredStretchSize.ascent + aDesiredStretchSize.descent;
      aDesiredStretchSize.width = mBoundingMetrics.width;
      aDesiredStretchSize.mBoundingMetrics = mBoundingMetrics;

      nscoord dy = aDesiredStretchSize.ascent - mBoundingMetrics.ascent;
      mMathMLChar.SetRect(
         nsRect(0, dy, charSize.width,
                charSize.ascent + charSize.descent));

      mReference.x = 0;
      mReference.y = aDesiredStretchSize.ascent;
    }
  }

  if (!NS_MATHML_OPERATOR_GET_FORM(mFlags) &&
      !NS_MATHML_OPERATOR_IS_CENTERED(mFlags)) {
    // Place our children using the default method
    Place(aPresContext, aRenderingContext, PR_TRUE, aDesiredStretchSize);
  }

  // Before we leave... there is a last item in the check-list:
  // If our parent is not embellished, it means we are the outermost embellished
  // container and so we put the spacing, otherwise we don't include the spacing,
  // the outermost embellished container will take care of it.

  if (!NS_MATHML_OPERATOR_HAS_EMBELLISH_ANCESTOR(mFlags)) {

    // Get the value of 'em'
    nscoord em = NSToCoordRound(float(font->mFont.size));

    // Account the spacing
    aDesiredStretchSize.width += nscoord((mLeftSpace + mRightSpace) * em);
    mBoundingMetrics.width = aDesiredStretchSize.width;
    nscoord dx = nscoord(mLeftSpace * em);

    if (!dx) return NS_OK;

    // adjust the offsets
    mBoundingMetrics.leftBearing += dx;
    mBoundingMetrics.rightBearing += dx;
    aDesiredStretchSize.mBoundingMetrics = mBoundingMetrics;

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

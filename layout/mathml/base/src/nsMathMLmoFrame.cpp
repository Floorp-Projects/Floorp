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
        NS_MATHML_PAINT_BOUNDING_METRICS(mPresentationData.flags)) {
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

// get the text that we enclose and setup our nsMathMLChar
void
nsMathMLmoFrame::ProcessTextData(nsIPresContext* aPresContext)
{
  mFlags = 0;
  if (!mFrames.FirstChild())
    return;

  // kids can be comment-nodes, attribute-nodes, text-nodes...
  // we use the DOM to ensure that we only look at text-nodes...
  nsAutoString data;
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

  // cache the special bits: mutable, accent, movablelimits, centered.
  // we need to do this in anticipation of other requirements, and these
  // bits don't change. Do not reset these bits unless the text gets changed.

  // lookup all the forms under which the operator is listed in the dictionary,
  // and record whether the operator has accent="true" or movablelimits="true"
  nsOperatorFlags flags[4];
  float lspace[4], rspace[4];
  nsMathMLOperators::LookupOperators(data, flags, lspace, rspace);
  nsOperatorFlags allFlags =
    flags[NS_MATHML_OPERATOR_FORM_INFIX] |
    flags[NS_MATHML_OPERATOR_FORM_POSTFIX] |
    flags[NS_MATHML_OPERATOR_FORM_PREFIX];

  mFlags |= allFlags & NS_MATHML_OPERATOR_ACCENT;
  mFlags |= allFlags & NS_MATHML_OPERATOR_MOVABLELIMITS;

  PRBool isMutable =
    NS_MATHML_OPERATOR_IS_STRETCHY(allFlags) ||
    NS_MATHML_OPERATOR_IS_LARGEOP(allFlags);
  if (isMutable)
    mFlags |= NS_MATHML_OPERATOR_MUTABLE;

  // see if this is an operator that should be centered to cater for 
  // fonts that are not math-aware
  if (1 == data.Length()) {
    PRUnichar ch = data[0];
    if ((ch == '+') || (ch == '=') || (ch == '*') ||
        (ch == 0x00D7)) { // &times;
      mFlags |= NS_MATHML_OPERATOR_CENTERED;
    }
  }

  // cache the operator
  mMathMLChar.SetData(aPresContext, data);
  ResolveMathMLCharStyle(aPresContext, mContent, mStyleContext, &mMathMLChar, isMutable);
}

// get our 'form' and lookup in the Operator Dictionary to fetch 
// our default data that may come from there, the look our attributes.
// The function has to be called during reflow because certain value
// before, we will re-use unchanged things that we computed earlier
void
nsMathMLmoFrame::ProcessOperatorData(nsIPresContext* aPresContext)
{
  // if we have been here before, we will just use our cached form
  nsOperatorFlags form = NS_MATHML_OPERATOR_GET_FORM(mFlags);
  nsAutoString value;

  // special bits are always kept in mFlags.
  // remember the mutable bit from ProcessTextData().
  // Some chars are listed under different forms in the dictionary,
  // and there could be a form under which the char is mutable.
  // If the char is the core of an embellished container, we will keep
  // it mutable irrespective of the form of the embellished container.
  // Also remember the other special bits that we want to carry forward.
  mFlags &= NS_MATHML_OPERATOR_MUTABLE |
            NS_MATHML_OPERATOR_ACCENT | 
            NS_MATHML_OPERATOR_MOVABLELIMITS |
            NS_MATHML_OPERATOR_CENTERED;

  if (!mEmbellishData.coreFrame) {
    // i.e., we haven't been here before, the default form is infix
    form = NS_MATHML_OPERATOR_FORM_INFIX;

    // reset everything so that we don't keep outdated values around
    // in case of dynamic changes
    mEmbellishData.flags = 0;
    mEmbellishData.nextFrame = nsnull; 
    mEmbellishData.coreFrame = nsnull;
    mEmbellishData.direction = NS_STRETCH_DIRECTION_UNSUPPORTED;
    mEmbellishData.leftSpace = 0;
    mEmbellishData.rightSpace = 0;

    if (!mFrames.FirstChild())
      return;

    mEmbellishData.flags |= NS_MATHML_EMBELLISH_OPERATOR;
    mEmbellishData.nextFrame = nsnull; 
    mEmbellishData.coreFrame = this;
    mEmbellishData.direction = mMathMLChar.GetStretchDirection();

    // there are two particular things that we also need to record so that if our
    // parent is <mover>, <munder>, or <munderover>, they will treat us properly:
    // 1) do we have accent="true"
    // 2) do we have movablelimits="true"

    // they need the extra information to decide how to treat their scripts/limits
    // (note: <mover>, <munder>, or <munderover> need not necessarily be our
    // direct parent -- case of embellished operators)

    // default values from the Operator Dictionary were obtained in ProcessTextData()
    // and these special bits are always kept in mFlags
    if (NS_MATHML_OPERATOR_IS_ACCENT(mFlags))
      mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENT;
    if (NS_MATHML_OPERATOR_IS_MOVABLELIMITS(mFlags))
      mEmbellishData.flags |= NS_MATHML_EMBELLISH_MOVABLELIMITS;

    // see if the accent attribute is there
    if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                     nsMathMLAtoms::accent_, value)) {
      if (value.Equals(NS_LITERAL_STRING("true")))
        mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENT;
      else if (value.Equals(NS_LITERAL_STRING("false")))
        mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENT;
    }
    // see if the movablelimits attribute is there
    if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                     nsMathMLAtoms::movablelimits_, value)) {
      if (value.Equals(NS_LITERAL_STRING("true")))
        mEmbellishData.flags |= NS_MATHML_EMBELLISH_MOVABLELIMITS;
      else if (value.Equals(NS_LITERAL_STRING("false")))
        mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_MOVABLELIMITS;
    }

    // get our outermost embellished container and its parent. 
    // (we ensure that we are the core, not just a sibling of the core)
    nsIFrame* embellishAncestor = this;
    nsEmbellishData embellishData;
    nsIFrame* parentAncestor = this;
    do {
      embellishAncestor = parentAncestor;
      embellishAncestor->GetParent(&parentAncestor);
      GetEmbellishDataFrom(parentAncestor, embellishData);
    } while (embellishData.coreFrame == this);

    // flag if we have an embellished ancestor
    if (embellishAncestor != this)
      mFlags |= NS_MATHML_OPERATOR_EMBELLISH_ANCESTOR;
    else
      mFlags &= ~NS_MATHML_OPERATOR_EMBELLISH_ANCESTOR;

    // find the position of our outermost embellished container w.r.t
    // its siblings (frames are singly-linked together).
    nsIFrame* firstChild;
    parentAncestor->FirstChild(aPresContext, nsnull, &firstChild);
    nsFrameList frameList(firstChild);

    nsIFrame* nextSibling;
    embellishAncestor->GetNextSibling(&nextSibling);
    nsIFrame* prevSibling = frameList.GetPrevSiblingFor(embellishAncestor);

    // flag to distinguish from a real infix
    if (!prevSibling && !nextSibling)
      mFlags |= NS_MATHML_OPERATOR_EMBELLISH_ISOLATED;
    else
      mFlags &= ~NS_MATHML_OPERATOR_EMBELLISH_ISOLATED;

    // find our form
    if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                     nsMathMLAtoms::form_, value)) {
      if (value.Equals(NS_LITERAL_STRING("prefix")))
        form = NS_MATHML_OPERATOR_FORM_PREFIX;
      else if (value.Equals(NS_LITERAL_STRING("postfix")))
        form = NS_MATHML_OPERATOR_FORM_POSTFIX;
    }
    else {
      // set our form flag depending on the position
      if (!prevSibling && nextSibling)
        form = NS_MATHML_OPERATOR_FORM_PREFIX;
      else if (prevSibling && !nextSibling)
        form = NS_MATHML_OPERATOR_FORM_POSTFIX;
    }
  }

  // lookup the operator dictionary
  // the form can be null to mean that the stretching of our mMathMLChar
  // failed earlier, in which case we won't bother re-stretching again
  if (form) {
    float lspace = 0.0f;
    float rspace = 0.0f;
    nsAutoString data;
    mMathMLChar.GetData(data);
    mMathMLChar.SetData(aPresContext, data); // XXX hack to reset, bug 45010
    PRBool found = nsMathMLOperators::LookupOperator(data, form, &mFlags, &lspace, &rspace);
    if (found) {
      // cache the default values of lspace & rspace that we get from the dictionary.
      // since these values are relative to the 'em' unit, convert to twips now
      nscoord em;
      const nsStyleFont* font;
      GetStyleData(eStyleStruct_Font, (const nsStyleStruct *&)font);
      nsCOMPtr<nsIFontMetrics> fm;
      aPresContext->GetMetricsFor(font->mFont, getter_AddRefs(fm));
      GetEmHeight(fm, em);

      mEmbellishData.leftSpace = NSToCoordRound(lspace * em);
      mEmbellishData.rightSpace = NSToCoordRound(rspace * em);

      // tuning if we don't want too much extra space when we are a script.
      // (with its fonts, TeX sets lspace=0 & rspace=0 as soon as scriptlevel>0.
      // Our fonts can be anything, so...)
      if (mPresentationData.scriptLevel > 0) {
        if (NS_MATHML_OPERATOR_EMBELLISH_IS_ISOLATED(mFlags)) {
          // could be an isolated accent or script, e.g., x^{+}, just zero out
          mEmbellishData.leftSpace = 0;
          mEmbellishData.rightSpace  = 0;
        }
        else if (!NS_MATHML_OPERATOR_HAS_EMBELLISH_ANCESTOR(mFlags)) {
          mEmbellishData.leftSpace /= 2;
          mEmbellishData.rightSpace  /= 2;
        }
      }
    }
  }

  // If we are an accent without explicit lspace="." or rspace=".",
  // we will ignore our default left/right space

  // lspace = number h-unit | namedspace
  nscoord leftSpace = mEmbellishData.leftSpace;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::lspace_, value)) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) ||
        ParseNamedSpaceValue(mPresentationData.mstyle, value, cssValue))
    {
      if ((eCSSUnit_Number == cssValue.GetUnit()) && !cssValue.GetFloatValue())
        leftSpace = 0;
      else if (cssValue.IsLengthUnit())
        leftSpace = CalcLength(aPresContext, mStyleContext, cssValue);
    }
  }
  else if (NS_MATHML_EMBELLISH_IS_ACCENT(mEmbellishData.flags)) {
    leftSpace = 0;
  }

  // rspace = number h-unit | namedspace
  nscoord rightSpace = mEmbellishData.rightSpace;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::rspace_, value)) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) ||
        ParseNamedSpaceValue(mPresentationData.mstyle, value, cssValue))
    {
      if ((eCSSUnit_Number == cssValue.GetUnit()) && !cssValue.GetFloatValue())
        rightSpace = 0;
      else if (cssValue.IsLengthUnit())
        rightSpace = CalcLength(aPresContext, mStyleContext, cssValue);
    }
  }
  else if (NS_MATHML_EMBELLISH_IS_ACCENT(mEmbellishData.flags)) {
    rightSpace = 0;
  }

  // the values that we get from our attributes override the dictionary
  mEmbellishData.leftSpace = leftSpace;
  mEmbellishData.rightSpace = rightSpace;

  // Nows see if there are user-defined attributes that override the dictionary.
  // XXX If an attribute can be forced to be true when it is false in the
  // dictionary, then the following code has to change...

  // For each attribute overriden by the user, turn off its bit flag.
  // symmetric|movablelimits|separator|largeop|accent|fence|stretchy|form
  // special: accent and movablelimits are handled in ProcessEmbellishData()
  // don't process them here

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
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::symmetric_, value)) {
   if (value == kfalse)
     mFlags &= ~NS_MATHML_OPERATOR_SYMMETRIC;
   else if (value == ktrue)
     mFlags |= NS_MATHML_OPERATOR_SYMMETRIC;
  }

  // minsize = number [ v-unit | h-unit ] | namedspace
  mMinSize = float(NS_UNCONSTRAINEDSIZE);
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
  mMaxSize = float(NS_UNCONSTRAINEDSIZE);
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
  if (NS_MATHML_STRETCH_WAS_DONE(mPresentationData.flags)) {
    NS_WARNING("it is wrong to fire stretch more than once on a frame");
    return NS_OK;
  }
  mPresentationData.flags |= NS_MATHML_STRETCH_DONE;

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

    // see if it is okay to stretch, starting from what the Operator Dictionary said
    PRBool isMutable = NS_MATHML_OPERATOR_IS_MUTABLE(mFlags);
    // if the stretchy and largeop attributes have been disabled,
    // the operator is not mutable
    if (!NS_MATHML_OPERATOR_IS_STRETCHY(mFlags) &&
        !NS_MATHML_OPERATOR_IS_LARGEOP(mFlags))
      isMutable = PR_FALSE;

    if (isMutable) {

      container = aContainerSize;

      if (((aStretchDirection == NS_STRETCH_DIRECTION_VERTICAL) ||
           (aStretchDirection == NS_STRETCH_DIRECTION_DEFAULT))  &&
          (mMathMLChar.GetStretchDirection() == NS_STRETCH_DIRECTION_VERTICAL))
      {
        isVertical = PR_TRUE;
      }

      // set the largeop or largeopOnly flags to suitably cover all the
      // 8 possible cases depending on whether displaystyle, largeop,
      // stretchy are true or false (see bug 69325).
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
        nscoord em;
        GetEmHeight(fm, em);
        leading = NSToCoordRound(0.2f * em); 

        aDesiredStretchSize.ascent = mBoundingMetrics.ascent + leading;
        aDesiredStretchSize.descent = mBoundingMetrics.descent + leading;
      }
      aDesiredStretchSize.height = aDesiredStretchSize.ascent + aDesiredStretchSize.descent;
      aDesiredStretchSize.width = mBoundingMetrics.width;
      aDesiredStretchSize.mBoundingMetrics = mBoundingMetrics;

      nscoord dy = aDesiredStretchSize.ascent - mBoundingMetrics.ascent;
      mMathMLChar.SetRect(
         nsRect(0, dy, charSize.width, charSize.ascent + charSize.descent));

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

    // Account the spacing
    mBoundingMetrics.width += mEmbellishData.leftSpace + mEmbellishData.rightSpace;
    aDesiredStretchSize.width = mBoundingMetrics.width;
    aDesiredStretchSize.mBoundingMetrics.width = mBoundingMetrics.width;

    nscoord dx = mEmbellishData.leftSpace;
    if (!dx) return NS_OK;

    // adjust the offsets
    mBoundingMetrics.leftBearing += dx;
    mBoundingMetrics.rightBearing += dx;
    aDesiredStretchSize.mBoundingMetrics.leftBearing += dx;
    aDesiredStretchSize.mBoundingMetrics.rightBearing += dx;

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
nsMathMLmoFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                     nsIAtom*        aListName,
                                     nsIFrame*       aChildList)
{
  // First, let the base class do its work
  nsresult rv = nsMathMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  if (NS_FAILED(rv)) return rv;

  // No need to tract the style context given to our MathML char. 
  // The Style System will use Get/SetAdditionalStyleContext() to keep it
  // up-to-date if dynamic changes arise.
  ProcessTextData(aPresContext);
  return rv;
}

NS_IMETHODIMP
nsMathMLmoFrame::TransmitAutomaticData(nsIPresContext* aPresContext)
{
  // this will cause us to re-sync our flags from scratch
  mEmbellishData.coreFrame = nsnull;
  ProcessOperatorData(aPresContext);
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmoFrame::Reflow(nsIPresContext*          aPresContext,
                        nsHTMLReflowMetrics&     aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&          aStatus)
{
  // certain values use units that depend on our style context, so
  // it is safer to just process the whole lot here
  ProcessOperatorData(aPresContext);
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

NS_IMETHODIMP
nsMathMLmoFrame::ReflowDirtyChild(nsIPresShell* aPresShell,
                                  nsIFrame*     aChild)
{
  // if we get this, it means it was called by the nsTextFrame beneath us, and 
  // this means something changed in the text content. So blow away everything
  // an re-build the automatic data from the parent of our outermost embellished
  // container (we ensure that we are the core, not just a sibling of the core)

  nsCOMPtr<nsIPresContext> presContext;
  aPresShell->GetPresContext(getter_AddRefs(presContext));

  ProcessTextData(presContext);

  nsIFrame* target = this;
  nsEmbellishData embellishData;
  do {
    target->GetParent(&target);
    GetEmbellishDataFrom(target, embellishData);
  } while (embellishData.coreFrame == this);

  // we have automatic data to update in the children of the target frame
  return ReLayoutChildren(presContext, target);
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

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
 *   Shyjan Mahamud <mahamud@cs.cmu.edu>
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

static const PRUnichar kInvisibleComma = PRUnichar(0x200B); // a.k.a. ZERO WIDTH SPACE
static const PRUnichar kApplyFunction  = PRUnichar(0x2061);
static const PRUnichar kInvisibleTimes = PRUnichar(0x2062);
static const PRUnichar kNullCh         = PRUnichar('\0');

nsIAtom*
nsMathMLmoFrame::GetType() const
{
  if (mFrames.GetLength() > 1) {
    return nsMathMLAtoms::operatorVisibleMathMLFrame;
  }

  nsAutoString data;
  ((nsMathMLChar&)mMathMLChar).GetData(data); // VC++ insists for this cast :-(
  PRInt32 length = data.Length();
  PRUnichar ch = (length == 0) ? kNullCh : data[0];
  if (length > 1)
    return nsMathMLAtoms::operatorVisibleMathMLFrame;
  
  if (ch == kInvisibleComma || 
      ch == kApplyFunction  ||
      ch == kInvisibleTimes ||
      ch == kNullCh)
    return nsMathMLAtoms::operatorInvisibleMathMLFrame;

  return nsMathMLAtoms::operatorVisibleMathMLFrame;
}

// since a mouse click implies selection, we cannot just rely on the
// frame's state bit in our child text frame. So we will first check
// its selected state bit, and use this little helper to double check.
PRBool
nsMathMLmoFrame::IsFrameInSelection(nsPresContext* aPresContext,
                                    nsIFrame*       aFrame)
{
  NS_ASSERTION(aFrame, "null arg");
  if (!aFrame)
    return PR_FALSE;

  PRBool isSelected = PR_FALSE;
  aFrame->GetSelected(&isSelected);
  if (!isSelected)
    return PR_FALSE;

  SelectionDetails* details = nsnull;
  nsIPresShell *shell = aPresContext->GetPresShell();
  if (shell) {
    nsCOMPtr<nsIFrameSelection> frameSelection;
    nsCOMPtr<nsISelectionController> selCon;
    nsresult rv = GetSelectionController(aPresContext, getter_AddRefs(selCon));
    if (NS_SUCCEEDED(rv) && selCon)
      frameSelection = do_QueryInterface(selCon);
    if (!frameSelection)
      rv = shell->GetFrameSelection(getter_AddRefs(frameSelection));
    if (NS_SUCCEEDED(rv) && frameSelection) {
      frameSelection->LookUpSelection(aFrame->GetContent(),
				      0, 1, &details, PR_TRUE);
    }
  }
  if (!details)
    return PR_FALSE;

  while (details) {
    SelectionDetails* next = details->mNext;
    delete details;
    details = next;
  }
  return PR_TRUE;
}

NS_IMETHODIMP
nsMathMLmoFrame::Paint(nsPresContext*      aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       const nsRect&        aDirtyRect,
                       nsFramePaintLayer    aWhichLayer,
                       PRUint32             aFlags)
{
  nsresult rv = NS_OK;
  PRBool useMathMLChar =
    (NS_MATHML_OPERATOR_GET_FORM(mFlags) &&
     NS_MATHML_OPERATOR_IS_MUTABLE(mFlags)) ||
    NS_MATHML_OPERATOR_IS_CENTERED(mFlags);

  if (!useMathMLChar || NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    // let the base class paint the background, border, outline
    rv = nsMathMLTokenFrame::Paint(aPresContext, aRenderingContext,
                                   aDirtyRect, aWhichLayer);
  }
  if (useMathMLChar) {
    // make our char selected if our inner child text frame is selected
    PRBool isSelected = PR_FALSE;
    nsRect selectedRect;
    nsIFrame* firstChild = mFrames.FirstChild();
    if (IsFrameInSelection(aPresContext, firstChild)) {
      selectedRect = firstChild->GetRect();
      isSelected = PR_TRUE;
    }
    rv = mMathMLChar.Paint(aPresContext, aRenderingContext, aDirtyRect,
                           aWhichLayer, this, isSelected ? &selectedRect : nsnull);
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
  return rv;
}

// get the text that we enclose and setup our nsMathMLChar
void
nsMathMLmoFrame::ProcessTextData(nsPresContext* aPresContext)
{
  mFlags = 0;

  // don't bother doing anything special if we don't have a
  // single child with a text content
  nsAutoString data;
  if (mFrames.GetLength() != 1) {
    mMathMLChar.SetData(aPresContext, data); // empty data to reset the char
    ResolveMathMLCharStyle(aPresContext, mContent, mStyleContext, &mMathMLChar, PR_FALSE);
    return;
  }

  // kids can be comment-nodes, attribute-nodes, text-nodes...
  // we use the DOM to ensure that we only look at text-nodes...
  PRUint32 numKids = mContent->GetChildCount();
  for (PRUint32 kid = 0; kid < numKids; ++kid) {
    nsCOMPtr<nsIDOMText> kidText(do_QueryInterface(mContent->GetChildAt(kid)));

    if (kidText) {
      nsAutoString kidData;
      kidText->GetData(kidData);
      data += kidData;
    }
  }
  PRInt32 length = data.Length();

  // special... in math mode, the usual minus sign '-' looks too short, so
  // what we do here is to remap <mo>-</mo> to the official Unicode minus
  // sign (U+2212) which looks much better. For background on this, see
  // http://groups.google.com/groups?hl=en&th=66488daf1ade7635&rnum=1
  if (1 == length && data[0] == '-') {
    data = PRUnichar(0x2212);
    mFlags |= NS_MATHML_OPERATOR_CENTERED;
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
  if (1 == length) {
    PRUnichar ch = data[0];
    if ((ch == '+') || (ch == '=') || (ch == '*') ||
        (ch == 0x2264) || // &le;
        (ch == 0x2265) || // &ge;
        (ch == 0x00D7)) { // &times;
      mFlags |= NS_MATHML_OPERATOR_CENTERED;
    }
  }

  // cache the operator
  mMathMLChar.SetData(aPresContext, data);
  ResolveMathMLCharStyle(aPresContext, mContent, mStyleContext, &mMathMLChar, isMutable);

  // cache the native direction -- beware of bug 133429...
  // mEmbellishData.direction must always retain our native direction, whereas
  // mMathMLChar.GetStretchDirection() may change later, when Stretch() is called
  mEmbellishData.direction = mMathMLChar.GetStretchDirection();
}

// get our 'form' and lookup in the Operator Dictionary to fetch 
// our default data that may come from there. Then complete our setup
// using attributes that we may have. To stay in sync, this function is
// called very often. We depend on many things that may change around us.
// However, we re-use unchanged values.
void
nsMathMLmoFrame::ProcessOperatorData(nsPresContext* aPresContext)
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
    mEmbellishData.leftSpace = 0;
    mEmbellishData.rightSpace = 0;
    if (mMathMLChar.Length() != 1)
      mEmbellishData.direction = NS_STRETCH_DIRECTION_UNSUPPORTED;  
    // else... retain the native direction obtained in ProcessTextData()

    if (!mFrames.FirstChild()) {
      return;
    }

    mEmbellishData.flags |= NS_MATHML_EMBELLISH_OPERATOR;
    mEmbellishData.coreFrame = this;

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
      if (value.EqualsLiteral("true"))
        mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENT;
      else if (value.EqualsLiteral("false"))
        mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENT;
    }
    // see if the movablelimits attribute is there
    if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                     nsMathMLAtoms::movablelimits_, value)) {
      if (value.EqualsLiteral("true"))
        mEmbellishData.flags |= NS_MATHML_EMBELLISH_MOVABLELIMITS;
      else if (value.EqualsLiteral("false"))
        mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_MOVABLELIMITS;
    }

     // ---------------------------------------------------------------------
     // we will be called again to re-sync the rest of our state next time...
     // (nobody needs the other values below at this stage)
     mFlags |= form;
     return;
  }

  // beware of bug 133814 - there is a two-way dependency in the
  // embellished hierarchy: our embellished ancestors need to set
  // their flags based on some of our state (set above), and here we
  // need to re-sync our 'form' depending on our outermost embellished
  // container. A null form here means that an earlier attempt to stretch
  // our mMathMLChar failed, in which case we don't bother re-stretching again
  if (form) {
    // get our outermost embellished container and its parent. 
    // (we ensure that we are the core, not just a sibling of the core)
    nsIFrame* embellishAncestor = this;
    nsEmbellishData embellishData;
    nsIFrame* parentAncestor = this;
    do {
      embellishAncestor = parentAncestor;
      parentAncestor = embellishAncestor->GetParent();
      GetEmbellishDataFrom(parentAncestor, embellishData);
    } while (embellishData.coreFrame == this);

    // flag if we have an embellished ancestor
    if (embellishAncestor != this)
      mFlags |= NS_MATHML_OPERATOR_EMBELLISH_ANCESTOR;
    else
      mFlags &= ~NS_MATHML_OPERATOR_EMBELLISH_ANCESTOR;

    // find the position of our outermost embellished container w.r.t
    // its siblings (frames are singly-linked together).
    nsFrameList frameList(parentAncestor->GetFirstChild(nsnull));

    nsIFrame* nextSibling = embellishAncestor->GetNextSibling();
    nsIFrame* prevSibling = frameList.GetPrevSiblingFor(embellishAncestor);

    // flag to distinguish from a real infix
    if (!prevSibling && !nextSibling)
      mFlags |= NS_MATHML_OPERATOR_EMBELLISH_ISOLATED;
    else
      mFlags &= ~NS_MATHML_OPERATOR_EMBELLISH_ISOLATED;

    // find our form
    form = NS_MATHML_OPERATOR_FORM_INFIX;
    if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                     nsMathMLAtoms::form_, value)) {
      if (value.EqualsLiteral("prefix"))
        form = NS_MATHML_OPERATOR_FORM_PREFIX;
      else if (value.EqualsLiteral("postfix"))
        form = NS_MATHML_OPERATOR_FORM_POSTFIX;
    }
    else {
      // set our form flag depending on the position
      if (!prevSibling && nextSibling)
        form = NS_MATHML_OPERATOR_FORM_PREFIX;
      else if (prevSibling && !nextSibling)
        form = NS_MATHML_OPERATOR_FORM_POSTFIX;
    }
    mFlags &= ~NS_MATHML_OPERATOR_FORM; // clear the old form bits
    mFlags |= form;

    // lookup the operator dictionary
    float lspace = 0.0f;
    float rspace = 0.0f;
    nsAutoString data;
    mMathMLChar.GetData(data);
    PRBool found = nsMathMLOperators::LookupOperator(data, form, &mFlags, &lspace, &rspace);
    if (found && (lspace || rspace)) {
      // cache the default values of lspace & rspace that we get from the dictionary.
      // since these values are relative to the 'em' unit, convert to twips now
      nscoord em;
      nsCOMPtr<nsIFontMetrics> fm =
	aPresContext->GetMetricsFor(GetStyleFont()->mFont);
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
      mFlags |= NS_MATHML_OPERATOR_LEFTSPACE_ATTR;
    }
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
      mFlags |= NS_MATHML_OPERATOR_RIGHTSPACE_ATTR;
    }
  }

  // little extra tuning to round lspace & rspace to at least a pixel so that
  // operators don't look as if they are colliding with their operands
  if (leftSpace || rightSpace) {
    nscoord onePixel = aPresContext->IntScaledPixelsToTwips(1);
    if (leftSpace && leftSpace < onePixel)
      leftSpace = onePixel;
    if (rightSpace && rightSpace < onePixel)
      rightSpace = onePixel;
  }

  // the values that we get from our attributes override the dictionary
  mEmbellishData.leftSpace = leftSpace;
  mEmbellishData.rightSpace = rightSpace;

  // Now see if there are user-defined attributes that override the dictionary.
  // XXX If an attribute can be forced to be true when it is false in the
  // dictionary, then the following code has to change...

  // For each attribute overriden by the user, turn off its bit flag.
  // symmetric|movablelimits|separator|largeop|accent|fence|stretchy|form
  // special: accent and movablelimits are handled in ProcessEmbellishData()
  // don't process them here

  nsAutoString kfalse, ktrue;
  kfalse.AssignLiteral("false");
  ktrue.AssignLiteral("true");

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
nsMathMLmoFrame::Stretch(nsPresContext*      aPresContext,
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

  nsIFrame* firstChild = mFrames.FirstChild();

  // get the axis height;
  nsCOMPtr<nsIFontMetrics> fm;
  aRenderingContext.SetFont(GetStyleFont()->mFont, nsnull);
  aRenderingContext.GetFontMetrics(*getter_AddRefs(fm));
  nscoord axisHeight, height;
  GetAxisHeight(aRenderingContext, fm, axisHeight);

  // get the leading to be left at the top and the bottom of the stretched char
  // this seems more reliable than using fm->GetLeading() on suspicious fonts               
  nscoord em;
  GetEmHeight(fm, em);
  nscoord leading = NSToCoordRound(0.2f * em);

  // Operators that are stretchy, or those that are to be centered
  // to cater for fonts that are not math-aware, are handled by the MathMLChar
  // ('form' is reset if stretch fails -- i.e., we don't bother to stretch next time)
  PRBool useMathMLChar =
    (NS_MATHML_OPERATOR_GET_FORM(mFlags) &&
     NS_MATHML_OPERATOR_IS_MUTABLE(mFlags)) ||
    NS_MATHML_OPERATOR_IS_CENTERED(mFlags);

  nsBoundingMetrics charSize;
  if (useMathMLChar) {
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
          (mEmbellishData.direction == NS_STRETCH_DIRECTION_VERTICAL))
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
      useMathMLChar = PR_FALSE;
    }
    else {
      // update our bounding metrics... it becomes that of our MathML char
      mBoundingMetrics = charSize;

      // if the returned direction is 'unsupported', the char didn't actually change. 
      // So we do the centering only if necessary
      if (mMathMLChar.GetStretchDirection() != NS_STRETCH_DIRECTION_UNSUPPORTED ||
          NS_MATHML_OPERATOR_IS_CENTERED(mFlags)) {

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
      }
    }
  }

  // Place our children using the default method
  // This will allow our child text frame to get its DidReflow()
  Place(aPresContext, aRenderingContext, PR_TRUE, aDesiredStretchSize);

  // Fixup for the final height.
  // On one hand, our stretchy height can sometimes be shorter than surrounding
  // ASCII chars, e.g., arrow symbols have |mBoundingMetrics.ascent + leading|
  // that is smaller than the ASCII's ascent, hence when painting the background
  // later, it won't look uniform along the line.
  // On the other hand, sometimes we may leave too much gap when our glyph happens
  // to come from a font with tall glyphs. For example, since CMEX10 has very tall
  // glyphs, its natural font metrics are large, even if we pick a small glyph
  // whose size is comparable to the size of a normal ASCII glyph.
  // So to avoid uneven spacing in either of these two cases, we use the height
  // of the ASCII font as a reference and try to match it if possible.

  // special case for accents... keep them short to improve mouse operations...
  // an accent can only be the non-first child of <mover>, <munder>, <munderover>
  PRBool isAccent =
    NS_MATHML_EMBELLISH_IS_ACCENT(mEmbellishData.flags);
  if (isAccent) {
    nsEmbellishData parentData;
    GetEmbellishDataFrom(mParent, parentData);
    isAccent =
       (NS_MATHML_EMBELLISH_IS_ACCENTOVER(parentData.flags) ||
        NS_MATHML_EMBELLISH_IS_ACCENTUNDER(parentData.flags)) &&
       parentData.coreFrame != this;
  }
  if (isAccent && firstChild) {
    // see bug 188467 for what is going on here
    nscoord dy = aDesiredStretchSize.ascent - (mBoundingMetrics.ascent + leading);
    aDesiredStretchSize.ascent = mBoundingMetrics.ascent + leading;
    aDesiredStretchSize.descent = mBoundingMetrics.descent;

    firstChild->SetPosition(firstChild->GetPosition() - nsPoint(0, dy));
  }
  else if (useMathMLChar) {
    nscoord ascent, descent;
    fm->GetMaxAscent(ascent);
    fm->GetMaxDescent(descent);
    aDesiredStretchSize.ascent = PR_MAX(mBoundingMetrics.ascent + leading, ascent);
    aDesiredStretchSize.descent = PR_MAX(mBoundingMetrics.descent + leading, descent);
  }
  aDesiredStretchSize.height = aDesiredStretchSize.ascent + aDesiredStretchSize.descent;
  aDesiredStretchSize.width = mBoundingMetrics.width;
  aDesiredStretchSize.mBoundingMetrics = mBoundingMetrics;
  mReference.x = 0;
  mReference.y = aDesiredStretchSize.ascent;
  // Place our mMathMLChar, its origin is in our coordinate system
  if (useMathMLChar) {
    nscoord dy = aDesiredStretchSize.ascent - mBoundingMetrics.ascent;
    mMathMLChar.SetRect(nsRect(0, dy, charSize.width, charSize.ascent + charSize.descent));
  }

  // Before we leave... there is a last item in the check-list:
  // If our parent is not embellished, it means we are the outermost embellished
  // container and so we put the spacing, otherwise we don't include the spacing,
  // the outermost embellished container will take care of it.

  if (!NS_MATHML_OPERATOR_HAS_EMBELLISH_ANCESTOR(mFlags)) {

    // Account the spacing if we are not an accent with explicit attributes
    nscoord leftSpace = mEmbellishData.leftSpace;
    if (isAccent && !NS_MATHML_OPERATOR_HAS_LEFTSPACE_ATTR(mFlags)) {
      leftSpace = 0;
    }
    nscoord rightSpace = mEmbellishData.rightSpace;
    if (isAccent && !NS_MATHML_OPERATOR_HAS_RIGHTSPACE_ATTR(mFlags)) {
      rightSpace = 0;
    }

    mBoundingMetrics.width += leftSpace + rightSpace;
    aDesiredStretchSize.width = mBoundingMetrics.width;
    aDesiredStretchSize.mBoundingMetrics.width = mBoundingMetrics.width;

    if (leftSpace) {
      // adjust the offsets
      mBoundingMetrics.leftBearing += leftSpace;
      mBoundingMetrics.rightBearing += leftSpace;
      aDesiredStretchSize.mBoundingMetrics.leftBearing += leftSpace;
      aDesiredStretchSize.mBoundingMetrics.rightBearing += leftSpace;

      if (useMathMLChar) {
	nsRect rect;
        mMathMLChar.GetRect(rect);
        mMathMLChar.SetRect(nsRect(rect.x + leftSpace, rect.y, rect.width, rect.height));
      }
      else {
        nsIFrame* childFrame = firstChild;
        while (childFrame) {
          childFrame->SetPosition(childFrame->GetPosition()
				  + nsPoint(leftSpace, 0));
          childFrame = childFrame->GetNextSibling();
        }
      }
    }
  }

  if (mFrames.GetLength() != 1)
    return NS_OK;

  nsRect rect = firstChild->GetRect();
  if (useMathMLChar) {
    // even though our child text frame is not doing the rendering, we make it play
    // nice with other operations that the MathMLChar doesn't handle (e.g., caret)
    // use our whole height (i.e., with the leading that isn't part of the MathMLChar)
    mMathMLChar.GetRect(rect);
    rect.y = 0;
  }
  rect.height = aDesiredStretchSize.height;
  firstChild->SetRect(rect);
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmoFrame::InheritAutomaticData(nsPresContext* aPresContext,
                                      nsIFrame*       aParent)
{
  // retain our native direction, it only changes if our text content changes
  nsStretchDirection direction = mEmbellishData.direction;
  nsMathMLTokenFrame::InheritAutomaticData(aPresContext, aParent);
  mEmbellishData.direction = direction;
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmoFrame::TransmitAutomaticData(nsPresContext* aPresContext)
{
  // this will cause us to re-sync our flags from scratch
  // but our returned 'form' is still not final (bug 133429), it will
  // be recomputed to its final value during the next call in Reflow()
  mEmbellishData.coreFrame = nsnull;
  ProcessOperatorData(aPresContext);
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmoFrame::Reflow(nsPresContext*          aPresContext,
                        nsHTMLReflowMetrics&     aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&          aStatus)
{
  // certain values use units that depend on our style context, so
  // it is safer to just process the whole lot here
  ProcessOperatorData(aPresContext);
  return nsMathMLTokenFrame::Reflow(aPresContext, aDesiredSize,
                                    aReflowState, aStatus);
}

NS_IMETHODIMP
nsMathMLmoFrame::ReflowDirtyChild(nsIPresShell* aPresShell,
                                  nsIFrame*     aChild)
{
  // if we get this, it means it was called by the nsTextFrame beneath us, and 
  // this means something changed in the text content. So blow away everything
  // an re-build the automatic data from the parent of our outermost embellished
  // container (we ensure that we are the core, not just a sibling of the core)

  nsCOMPtr<nsPresContext> presContext;
  aPresShell->GetPresContext(getter_AddRefs(presContext));

  ProcessTextData(presContext);

  nsIFrame* target = this;
  nsEmbellishData embellishData;
  do {
    target = target->GetParent();
    GetEmbellishDataFrom(target, embellishData);
  } while (embellishData.coreFrame == this);

  // we have automatic data to update in the children of the target frame
  return ReLayoutChildren(presContext, target);
}

NS_IMETHODIMP
nsMathMLmoFrame::AttributeChanged(nsPresContext* aPresContext,
                                  nsIContent*     aContent,
                                  PRInt32         aNameSpaceID,
                                  nsIAtom*        aAttribute,
                                  PRInt32         aModType)
{
  // check if this is an attribute that can affect the embellished hierarchy
  // in a significant way and re-layout the entire hierarchy.
  if (nsMathMLAtoms::accent_ == aAttribute ||
      nsMathMLAtoms::movablelimits_ == aAttribute) {

    // set the target as the parent of our outermost embellished container
    // (we ensure that we are the core, not just a sibling of the core)
    nsIFrame* target = this;
    nsEmbellishData embellishData;
    do {
      target = target->GetParent();
      GetEmbellishDataFrom(target, embellishData);
    } while (embellishData.coreFrame == this);

    // we have automatic data to update in the children of the target frame
    return ReLayoutChildren(aPresContext, target);
  }

  return nsMathMLTokenFrame::
         AttributeChanged(aPresContext, aContent, aNameSpaceID,
                          aAttribute, aModType);
}

// ----------------------
// No need to tract the style context given to our MathML char. 
// the Style System will use these to pass the proper style context to our MathMLChar
nsStyleContext*
nsMathMLmoFrame::GetAdditionalStyleContext(PRInt32 aIndex) const
{
  switch (aIndex) {
  case NS_MATHML_CHAR_STYLE_CONTEXT_INDEX:
    return mMathMLChar.GetStyleContext();
  default:
    return nsnull;
  }
}

void
nsMathMLmoFrame::SetAdditionalStyleContext(PRInt32          aIndex,
                                           nsStyleContext*  aStyleContext)
{
  switch (aIndex) {
  case NS_MATHML_CHAR_STYLE_CONTEXT_INDEX:
    mMathMLChar.SetStyleContext(aStyleContext);
    break;
  }
}

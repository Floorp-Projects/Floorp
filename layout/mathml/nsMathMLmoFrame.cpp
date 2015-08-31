/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMathMLmoFrame.h"
#include "nsPresContext.h"
#include "nsRenderingContext.h"
#include "nsContentUtils.h"
#include "nsFrameSelection.h"
#include "nsMathMLElement.h"
#include <algorithm>

//
// <mo> -- operator, fence, or separator - implementation
//

// additional style context to be used by our MathMLChar.
#define NS_MATHML_CHAR_STYLE_CONTEXT_INDEX   0

nsIFrame*
NS_NewMathMLmoFrame(nsIPresShell* aPresShell, nsStyleContext *aContext)
{
  return new (aPresShell) nsMathMLmoFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmoFrame)

nsMathMLmoFrame::~nsMathMLmoFrame()
{
}

static const char16_t kApplyFunction  = char16_t(0x2061);
static const char16_t kInvisibleTimes = char16_t(0x2062);
static const char16_t kInvisibleSeparator = char16_t(0x2063);
static const char16_t kInvisiblePlus = char16_t(0x2064);

eMathMLFrameType
nsMathMLmoFrame::GetMathMLFrameType()
{
  return NS_MATHML_OPERATOR_IS_INVISIBLE(mFlags)
    ? eMathMLFrameType_OperatorInvisible
    : eMathMLFrameType_OperatorOrdinary;
}

// since a mouse click implies selection, we cannot just rely on the
// frame's state bit in our child text frame. So we will first check
// its selected state bit, and use this little helper to double check.
bool
nsMathMLmoFrame::IsFrameInSelection(nsIFrame* aFrame)
{
  NS_ASSERTION(aFrame, "null arg");
  if (!aFrame || !aFrame->IsSelected())
    return false;

  const nsFrameSelection* frameSelection = aFrame->GetConstFrameSelection();
  SelectionDetails* details =
    frameSelection->LookUpSelection(aFrame->GetContent(), 0, 1, true);

  if (!details)
    return false;

  while (details) {
    SelectionDetails* next = details->mNext;
    delete details;
    details = next;
  }
  return true;
}

bool
nsMathMLmoFrame::UseMathMLChar()
{
  return (NS_MATHML_OPERATOR_GET_FORM(mFlags) &&
          NS_MATHML_OPERATOR_IS_MUTABLE(mFlags)) ||
    NS_MATHML_OPERATOR_IS_CENTERED(mFlags);
}

void
nsMathMLmoFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                  const nsRect&           aDirtyRect,
                                  const nsDisplayListSet& aLists)
{
  bool useMathMLChar = UseMathMLChar();

  if (!useMathMLChar) {
    // let the base class do everything
    nsMathMLTokenFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
  } else {
    DisplayBorderBackgroundOutline(aBuilder, aLists);
    
    // make our char selected if our inner child text frame is selected
    bool isSelected = false;
    nsRect selectedRect;
    nsIFrame* firstChild = mFrames.FirstChild();
    if (IsFrameInSelection(firstChild)) {
      mMathMLChar.GetRect(selectedRect);
      // add a one pixel border (it renders better for operators like minus)
      selectedRect.Inflate(nsPresContext::CSSPixelsToAppUnits(1));
      isSelected = true;
    }
    mMathMLChar.Display(aBuilder, this, aLists, 0, isSelected ? &selectedRect : nullptr);
  
#if defined(DEBUG) && defined(SHOW_BOUNDING_BOX)
    // for visual debug
    DisplayBoundingMetrics(aBuilder, this, mReference, mBoundingMetrics, aLists);
#endif
  }
}

// get the text that we enclose and setup our nsMathMLChar
void
nsMathMLmoFrame::ProcessTextData()
{
  mFlags = 0;

  nsAutoString data;
  nsContentUtils::GetNodeTextContent(mContent, false, data);

  data.CompressWhitespace();
  int32_t length = data.Length();
  char16_t ch = (length == 0) ? char16_t('\0') : data[0];

  if ((length == 1) && 
      (ch == kApplyFunction  ||
       ch == kInvisibleSeparator ||
       ch == kInvisiblePlus ||
       ch == kInvisibleTimes)) {
    mFlags |= NS_MATHML_OPERATOR_INVISIBLE;
  }

  // don't bother doing anything special if we don't have a single child
  nsPresContext* presContext = PresContext();
  if (mFrames.GetLength() != 1) {
    data.Truncate(); // empty data to reset the char
    mMathMLChar.SetData(presContext, data);
    ResolveMathMLCharStyle(presContext, mContent, mStyleContext, &mMathMLChar);
    return;
  }

  // special... in math mode, the usual minus sign '-' looks too short, so
  // what we do here is to remap <mo>-</mo> to the official Unicode minus
  // sign (U+2212) which looks much better. For background on this, see
  // http://groups.google.com/groups?hl=en&th=66488daf1ade7635&rnum=1
  if (1 == length && ch == '-') {
    ch = 0x2212;
    data = ch;
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

  // see if this is an operator that should be centered to cater for 
  // fonts that are not math-aware
  if (1 == length) {
    if ((ch == '+') || (ch == '=') || (ch == '*') ||
        (ch == 0x2212) || // &minus;
        (ch == 0x2264) || // &le;
        (ch == 0x2265) || // &ge;
        (ch == 0x00D7)) { // &times;
      mFlags |= NS_MATHML_OPERATOR_CENTERED;
    }
  }

  // cache the operator
  mMathMLChar.SetData(presContext, data);

  // cache the native direction -- beware of bug 133429...
  // mEmbellishData.direction must always retain our native direction, whereas
  // mMathMLChar.GetStretchDirection() may change later, when Stretch() is called
  mEmbellishData.direction = mMathMLChar.GetStretchDirection();

  bool isMutable =
    NS_MATHML_OPERATOR_IS_LARGEOP(allFlags) ||
    (mEmbellishData.direction != NS_STRETCH_DIRECTION_UNSUPPORTED);
  if (isMutable)
    mFlags |= NS_MATHML_OPERATOR_MUTABLE;

  ResolveMathMLCharStyle(presContext, mContent, mStyleContext, &mMathMLChar);
}

// get our 'form' and lookup in the Operator Dictionary to fetch 
// our default data that may come from there. Then complete our setup
// using attributes that we may have. To stay in sync, this function is
// called very often. We depend on many things that may change around us.
// However, we re-use unchanged values.
void
nsMathMLmoFrame::ProcessOperatorData()
{
  // if we have been here before, we will just use our cached form
  nsOperatorFlags form = NS_MATHML_OPERATOR_GET_FORM(mFlags);
  nsAutoString value;
  float fontSizeInflation = nsLayoutUtils::FontSizeInflationFor(this);

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
            NS_MATHML_OPERATOR_CENTERED |
            NS_MATHML_OPERATOR_INVISIBLE;

  if (!mEmbellishData.coreFrame) {
    // i.e., we haven't been here before, the default form is infix
    form = NS_MATHML_OPERATOR_FORM_INFIX;

    // reset everything so that we don't keep outdated values around
    // in case of dynamic changes
    mEmbellishData.flags = 0;
    mEmbellishData.coreFrame = nullptr;
    mEmbellishData.leadingSpace = 0;
    mEmbellishData.trailingSpace = 0;
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
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::accent_, value);
    if (value.EqualsLiteral("true"))
      mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENT;
    else if (value.EqualsLiteral("false"))
      mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENT;

    // see if the movablelimits attribute is there
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::movablelimits_, value);
    if (value.EqualsLiteral("true"))
      mEmbellishData.flags |= NS_MATHML_EMBELLISH_MOVABLELIMITS;
    else if (value.EqualsLiteral("false"))
      mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_MOVABLELIMITS;

     // ---------------------------------------------------------------------
     // we will be called again to re-sync the rest of our state next time...
     // (nobody needs the other values below at this stage)
     mFlags |= form;
     return;
  }

  nsPresContext* presContext = PresContext();

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
    // its siblings.

    nsIFrame* nextSibling = embellishAncestor->GetNextSibling();
    nsIFrame* prevSibling = embellishAncestor->GetPrevSibling();

    // flag to distinguish from a real infix.  Set for (embellished) operators
    // that live in (inferred) mrows.
    nsIMathMLFrame* mathAncestor = do_QueryFrame(parentAncestor);
    bool zeroSpacing = false;
    if (mathAncestor) {
      zeroSpacing =  !mathAncestor->IsMrowLike();
    } else {
      nsMathMLmathBlockFrame* blockFrame = do_QueryFrame(parentAncestor);
      if (blockFrame) {
        zeroSpacing = !blockFrame->IsMrowLike();
      }
    }
    if (zeroSpacing) {
      mFlags |= NS_MATHML_OPERATOR_EMBELLISH_ISOLATED;
    } else {
      mFlags &= ~NS_MATHML_OPERATOR_EMBELLISH_ISOLATED;
    }

    // find our form
    form = NS_MATHML_OPERATOR_FORM_INFIX;
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::form, value);
    if (!value.IsEmpty()) {
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

    // Use the default value suggested by the MathML REC.
    // http://www.w3.org/TR/MathML/chapter3.html#presm.mo.attrs
    // thickmathspace = 5/18em
    float lspace = 5.0f/18.0f;
    float rspace = 5.0f/18.0f;
    // lookup the operator dictionary
    nsAutoString data;
    mMathMLChar.GetData(data);
    nsMathMLOperators::LookupOperator(data, form, &mFlags, &lspace, &rspace);
    // Spacing is zero if our outermost embellished operator is not in an
    // inferred mrow.
    if (!NS_MATHML_OPERATOR_EMBELLISH_IS_ISOLATED(mFlags) &&
        (lspace || rspace)) {
      // Cache the default values of lspace and rspace.
      // since these values are relative to the 'em' unit, convert to twips now
      nscoord em;
      nsRefPtr<nsFontMetrics> fm;
      nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm),
                                            fontSizeInflation);
      GetEmHeight(fm, em);

      mEmbellishData.leadingSpace = NSToCoordRound(lspace * em);
      mEmbellishData.trailingSpace = NSToCoordRound(rspace * em);

      // tuning if we don't want too much extra space when we are a script.
      // (with its fonts, TeX sets lspace=0 & rspace=0 as soon as scriptlevel>0.
      // Our fonts can be anything, so...)
      if (StyleFont()->mScriptLevel > 0 &&
          !NS_MATHML_OPERATOR_HAS_EMBELLISH_ANCESTOR(mFlags)) {
        mEmbellishData.leadingSpace /= 2;
        mEmbellishData.trailingSpace /= 2;
      }
    }
  }

  // If we are an accent without explicit lspace="." or rspace=".",
  // we will ignore our default leading/trailing space

  // lspace
  //
  // "Specifies the leading space appearing before the operator"
  //
  // values: length
  // default: set by dictionary (thickmathspace) 
  //
  // XXXfredw Support for negative and relative values is not implemented
  // (bug 805926).
  // Relative values will give a multiple of the current leading space,
  // which is not necessarily the default one.
  //
  nscoord leadingSpace = mEmbellishData.leadingSpace;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::lspace_, value);
  if (!value.IsEmpty()) {
    nsCSSValue cssValue;
    if (nsMathMLElement::ParseNumericValue(value, cssValue, 0,
                                           mContent->OwnerDoc())) {
      if ((eCSSUnit_Number == cssValue.GetUnit()) && !cssValue.GetFloatValue())
        leadingSpace = 0;
      else if (cssValue.IsLengthUnit())
        leadingSpace = CalcLength(presContext, mStyleContext, cssValue,
                                  fontSizeInflation);
      mFlags |= NS_MATHML_OPERATOR_LSPACE_ATTR;
    }
  }

  // rspace
  //
  // "Specifies the trailing space appearing after the operator"
  //
  // values: length
  // default: set by dictionary (thickmathspace) 
  //
  // XXXfredw Support for negative and relative values is not implemented
  // (bug 805926).
  // Relative values will give a multiple of the current leading space,
  // which is not necessarily the default one.
  //
  nscoord trailingSpace = mEmbellishData.trailingSpace;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::rspace_, value);
  if (!value.IsEmpty()) {
    nsCSSValue cssValue;
    if (nsMathMLElement::ParseNumericValue(value, cssValue, 0,
                                           mContent->OwnerDoc())) {
      if ((eCSSUnit_Number == cssValue.GetUnit()) && !cssValue.GetFloatValue())
        trailingSpace = 0;
      else if (cssValue.IsLengthUnit())
        trailingSpace = CalcLength(presContext, mStyleContext, cssValue,
                                   fontSizeInflation);
      mFlags |= NS_MATHML_OPERATOR_RSPACE_ATTR;
    }
  }

  // little extra tuning to round lspace & rspace to at least a pixel so that
  // operators don't look as if they are colliding with their operands
  if (leadingSpace || trailingSpace) {
    nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);
    if (leadingSpace && leadingSpace < onePixel)
      leadingSpace = onePixel;
    if (trailingSpace && trailingSpace < onePixel)
      trailingSpace = onePixel;
  }

  // the values that we get from our attributes override the dictionary
  mEmbellishData.leadingSpace = leadingSpace;
  mEmbellishData.trailingSpace = trailingSpace;

  // Now see if there are user-defined attributes that override the dictionary.
  // XXX Bug 1197771 - forcing an attribute to true when it is false in the
  // dictionary can cause conflicts in the rest of the stretching algorithms
  // (e.g. all largeops are assumed to have a vertical direction)

  // For each attribute overriden by the user, turn off its bit flag.
  // symmetric|movablelimits|separator|largeop|accent|fence|stretchy|form
  // special: accent and movablelimits are handled above,
  // don't process them here

  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::stretchy_, value);
  if (value.EqualsLiteral("false")) {
    mFlags &= ~NS_MATHML_OPERATOR_STRETCHY;
  } else if (value.EqualsLiteral("true")) {
    mFlags |= NS_MATHML_OPERATOR_STRETCHY;
  }
  if (NS_MATHML_OPERATOR_IS_FENCE(mFlags)) {
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::fence_, value);
    if (value.EqualsLiteral("false"))
      mFlags &= ~NS_MATHML_OPERATOR_FENCE;
    else
      mEmbellishData.flags |= NS_MATHML_EMBELLISH_FENCE;
  }
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::largeop_, value);
  if (value.EqualsLiteral("false")) {
    mFlags &= ~NS_MATHML_OPERATOR_LARGEOP;
  } else if (value.EqualsLiteral("true")) {
    mFlags |= NS_MATHML_OPERATOR_LARGEOP;
  }
  if (NS_MATHML_OPERATOR_IS_SEPARATOR(mFlags)) {
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::separator_, value);
    if (value.EqualsLiteral("false"))
      mFlags &= ~NS_MATHML_OPERATOR_SEPARATOR;
    else
      mEmbellishData.flags |= NS_MATHML_EMBELLISH_SEPARATOR;
  }
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::symmetric_, value);
  if (value.EqualsLiteral("false"))
    mFlags &= ~NS_MATHML_OPERATOR_SYMMETRIC;
  else if (value.EqualsLiteral("true"))
    mFlags |= NS_MATHML_OPERATOR_SYMMETRIC;


  // minsize
  //
  // "Specifies the minimum size of the operator when stretchy"
  //
  // values: length
  // default: set by dictionary (1em)
  //
  // We don't allow negative values.
  // Note: Contrary to other "length" values, unitless and percentage do not
  // give a multiple of the defaut value but a multiple of the operator at
  // normal size.
  //
  mMinSize = 0;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::minsize_, value);
  if (!value.IsEmpty()) {
    nsCSSValue cssValue;
    if (nsMathMLElement::ParseNumericValue(value, cssValue,
                                           nsMathMLElement::
                                           PARSE_ALLOW_UNITLESS,
                                           mContent->OwnerDoc())) {
      nsCSSUnit unit = cssValue.GetUnit();
      if (eCSSUnit_Number == unit)
        mMinSize = cssValue.GetFloatValue();
      else if (eCSSUnit_Percent == unit)
        mMinSize = cssValue.GetPercentValue();
      else if (eCSSUnit_Null != unit) {
        mMinSize = float(CalcLength(presContext, mStyleContext, cssValue,
                                    fontSizeInflation));
        mFlags |= NS_MATHML_OPERATOR_MINSIZE_ABSOLUTE;
      }
    }
  }

  // maxsize
  //
  // "Specifies the maximum size of the operator when stretchy"
  //
  // values: length | "infinity"
  // default: set by dictionary (infinity)
  //
  // We don't allow negative values.
  // Note: Contrary to other "length" values, unitless and percentage do not
  // give a multiple of the defaut value but a multiple of the operator at
  // normal size.
  //
  mMaxSize = NS_MATHML_OPERATOR_SIZE_INFINITY;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::maxsize_, value);
  if (!value.IsEmpty()) {
    nsCSSValue cssValue;
    if (nsMathMLElement::ParseNumericValue(value, cssValue,
                                           nsMathMLElement::
                                           PARSE_ALLOW_UNITLESS,
                                           mContent->OwnerDoc())) {
      nsCSSUnit unit = cssValue.GetUnit();
      if (eCSSUnit_Number == unit)
        mMaxSize = cssValue.GetFloatValue();
      else if (eCSSUnit_Percent == unit)
        mMaxSize = cssValue.GetPercentValue();
      else if (eCSSUnit_Null != unit) {
        mMaxSize = float(CalcLength(presContext, mStyleContext, cssValue,
                                    fontSizeInflation));
        mFlags |= NS_MATHML_OPERATOR_MAXSIZE_ABSOLUTE;
      }
    }
  }
}

static uint32_t
GetStretchHint(nsOperatorFlags aFlags, nsPresentationData aPresentationData,
               bool aIsVertical, const nsStyleFont* aStyleFont)
{
  uint32_t stretchHint = NS_STRETCH_NONE;
  // See if it is okay to stretch,
  // starting from what the Operator Dictionary said
  if (NS_MATHML_OPERATOR_IS_MUTABLE(aFlags)) {
    // set the largeop or largeopOnly flags to suitably cover all the
    // 8 possible cases depending on whether displaystyle, largeop,
    // stretchy are true or false (see bug 69325).
    // . largeopOnly is taken if largeop=true and stretchy=false
    // . largeop is taken if largeop=true and stretchy=true
    if (aStyleFont->mMathDisplay == NS_MATHML_DISPLAYSTYLE_BLOCK &&
        NS_MATHML_OPERATOR_IS_LARGEOP(aFlags)) {
      stretchHint = NS_STRETCH_LARGEOP; // (largeopOnly, not mask!)
      if (NS_MATHML_OPERATOR_IS_INTEGRAL(aFlags)) {
        stretchHint |= NS_STRETCH_INTEGRAL;
      }
      if (NS_MATHML_OPERATOR_IS_STRETCHY(aFlags)) {
        stretchHint |= NS_STRETCH_NEARER | NS_STRETCH_LARGER;
      }
    }
    else if(NS_MATHML_OPERATOR_IS_STRETCHY(aFlags)) {
      if (aIsVertical) {
        // TeX hint. Can impact some sloppy markups missing <mrow></mrow>
        stretchHint = NS_STRETCH_NEARER;
      }
      else {
        stretchHint = NS_STRETCH_NORMAL;
      }
    }
    // else if the stretchy and largeop attributes have been disabled,
    // the operator is not mutable
  }
  return stretchHint;
}

// NOTE: aDesiredStretchSize is an IN/OUT parameter
//       On input  - it contains our current size
//       On output - the same size or the new size that we want
NS_IMETHODIMP
nsMathMLmoFrame::Stretch(nsRenderingContext& aRenderingContext,
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
  float fontSizeInflation = nsLayoutUtils::FontSizeInflationFor(this);
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm),
                                        fontSizeInflation);
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
  bool useMathMLChar = UseMathMLChar();

  nsBoundingMetrics charSize;
  nsBoundingMetrics container = aDesiredStretchSize.mBoundingMetrics;
  bool isVertical = false;

  if (((aStretchDirection == NS_STRETCH_DIRECTION_VERTICAL) ||
       (aStretchDirection == NS_STRETCH_DIRECTION_DEFAULT))  &&
      (mEmbellishData.direction == NS_STRETCH_DIRECTION_VERTICAL)) {
    isVertical = true;
  }

  uint32_t stretchHint =
    GetStretchHint(mFlags, mPresentationData, isVertical, StyleFont());

  if (useMathMLChar) {
    nsBoundingMetrics initialSize = aDesiredStretchSize.mBoundingMetrics;

    if (stretchHint != NS_STRETCH_NONE) {

      container = aContainerSize;

      // some adjustments if the operator is symmetric and vertical

      if (isVertical && NS_MATHML_OPERATOR_IS_SYMMETRIC(mFlags)) {
        // we need to center about the axis
        nscoord delta = std::max(container.ascent - axisHeight,
                               container.descent + axisHeight);
        container.ascent = delta + axisHeight;
        container.descent = delta - axisHeight;

        // get ready in case we encounter user-desired min-max size
        delta = std::max(initialSize.ascent - axisHeight,
                       initialSize.descent + axisHeight);
        initialSize.ascent = delta + axisHeight;
        initialSize.descent = delta - axisHeight;
      }

      // check for user-desired min-max size

      if (mMaxSize != NS_MATHML_OPERATOR_SIZE_INFINITY && mMaxSize > 0.0f) {
        // if we are here, there is a user defined maxsize ...
        //XXX Set stretchHint = NS_STRETCH_NORMAL? to honor the maxsize as close as possible?
        if (NS_MATHML_OPERATOR_MAXSIZE_IS_ABSOLUTE(mFlags)) {
          // there is an explicit value like maxsize="20pt"
          // try to maintain the aspect ratio of the char
          float aspect = mMaxSize / float(initialSize.ascent + initialSize.descent);
          container.ascent =
            std::min(container.ascent, nscoord(initialSize.ascent * aspect));
          container.descent =
            std::min(container.descent, nscoord(initialSize.descent * aspect));
          // below we use a type cast instead of a conversion to avoid a VC++ bug
          // see http://support.microsoft.com/support/kb/articles/Q115/7/05.ASP
          container.width =
            std::min(container.width, (nscoord)mMaxSize);
        }
        else { // multiplicative value
          container.ascent =
            std::min(container.ascent, nscoord(initialSize.ascent * mMaxSize));
          container.descent =
            std::min(container.descent, nscoord(initialSize.descent * mMaxSize));
          container.width =
            std::min(container.width, nscoord(initialSize.width * mMaxSize));
        }

        if (isVertical && !NS_MATHML_OPERATOR_IS_SYMMETRIC(mFlags)) {
          // re-adjust to align the char with the bottom of the initial container
          height = container.ascent + container.descent;
          container.descent = aContainerSize.descent;
          container.ascent = height - container.descent;
        }
      }

      if (mMinSize > 0.0f) {
        // if we are here, there is a user defined minsize ...
        // always allow the char to stretch in its natural direction,
        // even if it is different from the caller's direction 
        if (aStretchDirection != NS_STRETCH_DIRECTION_DEFAULT &&
            aStretchDirection != mEmbellishData.direction) {
          aStretchDirection = NS_STRETCH_DIRECTION_DEFAULT;
          // but when we are not honoring the requested direction
          // we should not use the caller's container size either
          container = initialSize;
        }
        if (NS_MATHML_OPERATOR_MINSIZE_IS_ABSOLUTE(mFlags)) {
          // there is an explicit value like minsize="20pt"
          // try to maintain the aspect ratio of the char
          float aspect = mMinSize / float(initialSize.ascent + initialSize.descent);
          container.ascent =
            std::max(container.ascent, nscoord(initialSize.ascent * aspect));
          container.descent =
            std::max(container.descent, nscoord(initialSize.descent * aspect));
          container.width =
            std::max(container.width, (nscoord)mMinSize);
        }
        else { // multiplicative value
          container.ascent =
            std::max(container.ascent, nscoord(initialSize.ascent * mMinSize));
          container.descent =
            std::max(container.descent, nscoord(initialSize.descent * mMinSize));
          container.width =
            std::max(container.width, nscoord(initialSize.width * mMinSize));
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
    nsresult res = mMathMLChar.Stretch(PresContext(), aRenderingContext,
                                       fontSizeInflation,
                                       aStretchDirection, container, charSize,
                                       stretchHint,
                                       StyleVisibility()->mDirection);
    if (NS_FAILED(res)) {
      // gracefully handle cases where stretching the char failed (i.e., GetBoundingMetrics failed)
      // clear our 'form' to behave as if the operator wasn't in the dictionary
      mFlags &= ~NS_MATHML_OPERATOR_FORM;
      useMathMLChar = false;
    }
  }

  // Place our children using the default method
  // This will allow our child text frame to get its DidReflow()
  nsresult rv = Place(aRenderingContext, true, aDesiredStretchSize);
  if (NS_MATHML_HAS_ERROR(mPresentationData.flags) || NS_FAILED(rv)) {
    // Make sure the child frames get their DidReflow() calls.
    DidReflowChildren(mFrames.FirstChild());
  }

  if (useMathMLChar) {
    // update our bounding metrics... it becomes that of our MathML char
    mBoundingMetrics = charSize;

    // if the returned direction is 'unsupported', the char didn't actually change. 
    // So we do the centering only if necessary
    if (mMathMLChar.GetStretchDirection() != NS_STRETCH_DIRECTION_UNSUPPORTED ||
        NS_MATHML_OPERATOR_IS_CENTERED(mFlags)) {

      bool largeopOnly =
        (NS_STRETCH_LARGEOP & stretchHint) != 0 &&
        (NS_STRETCH_VARIABLE_MASK & stretchHint) == 0;

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
        } else if (!largeopOnly) {
          // Align the center of the char with the center of the container
          mBoundingMetrics.descent = height/2 +
            (container.ascent + container.descent)/2 - container.ascent;
        } // else align the baselines
        mBoundingMetrics.ascent = height - mBoundingMetrics.descent;
      }
    }
  }

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
  bool isAccent =
    NS_MATHML_EMBELLISH_IS_ACCENT(mEmbellishData.flags);
  if (isAccent) {
    nsEmbellishData parentData;
    GetEmbellishDataFrom(GetParent(), parentData);
    isAccent =
       (NS_MATHML_EMBELLISH_IS_ACCENTOVER(parentData.flags) ||
        NS_MATHML_EMBELLISH_IS_ACCENTUNDER(parentData.flags)) &&
       parentData.coreFrame != this;
  }
  if (isAccent && firstChild) {
    // see bug 188467 for what is going on here
    nscoord dy = aDesiredStretchSize.BlockStartAscent() -
      (mBoundingMetrics.ascent + leading);
    aDesiredStretchSize.SetBlockStartAscent(mBoundingMetrics.ascent + leading);
    aDesiredStretchSize.Height() = aDesiredStretchSize.BlockStartAscent() +
                                   mBoundingMetrics.descent;

    firstChild->SetPosition(firstChild->GetPosition() - nsPoint(0, dy));
  }
  else if (useMathMLChar) {
    nscoord ascent = fm->MaxAscent();
    nscoord descent = fm->MaxDescent();
    aDesiredStretchSize.SetBlockStartAscent(std::max(mBoundingMetrics.ascent + leading, ascent));
    aDesiredStretchSize.Height() = aDesiredStretchSize.BlockStartAscent() +
                                 std::max(mBoundingMetrics.descent + leading, descent);
  }
  aDesiredStretchSize.Width() = mBoundingMetrics.width;
  aDesiredStretchSize.mBoundingMetrics = mBoundingMetrics;
  mReference.x = 0;
  mReference.y = aDesiredStretchSize.BlockStartAscent();
  // Place our mMathMLChar, its origin is in our coordinate system
  if (useMathMLChar) {
    nscoord dy = aDesiredStretchSize.BlockStartAscent() - mBoundingMetrics.ascent;
    mMathMLChar.SetRect(nsRect(0, dy, charSize.width, charSize.ascent + charSize.descent));
  }

  // Before we leave... there is a last item in the check-list:
  // If our parent is not embellished, it means we are the outermost embellished
  // container and so we put the spacing, otherwise we don't include the spacing,
  // the outermost embellished container will take care of it.

  if (!NS_MATHML_OPERATOR_HAS_EMBELLISH_ANCESTOR(mFlags)) {

    // Account the spacing if we are not an accent with explicit attributes
    nscoord leadingSpace = mEmbellishData.leadingSpace;
    if (isAccent && !NS_MATHML_OPERATOR_HAS_LSPACE_ATTR(mFlags)) {
      leadingSpace = 0;
    }
    nscoord trailingSpace = mEmbellishData.trailingSpace;
    if (isAccent && !NS_MATHML_OPERATOR_HAS_RSPACE_ATTR(mFlags)) {
      trailingSpace = 0;
    }

    mBoundingMetrics.width += leadingSpace + trailingSpace;
    aDesiredStretchSize.Width() = mBoundingMetrics.width;
    aDesiredStretchSize.mBoundingMetrics.width = mBoundingMetrics.width;

    nscoord dx = (StyleVisibility()->mDirection ?
                  trailingSpace : leadingSpace);
    if (dx) {
      // adjust the offsets
      mBoundingMetrics.leftBearing += dx;
      mBoundingMetrics.rightBearing += dx;
      aDesiredStretchSize.mBoundingMetrics.leftBearing += dx;
      aDesiredStretchSize.mBoundingMetrics.rightBearing += dx;

      if (useMathMLChar) {
        nsRect rect;
        mMathMLChar.GetRect(rect);
        mMathMLChar.SetRect(nsRect(rect.x + dx, rect.y,
                                   rect.width, rect.height));
      }
      else {
        nsIFrame* childFrame = firstChild;
        while (childFrame) {
          childFrame->SetPosition(childFrame->GetPosition() +
                                  nsPoint(dx, 0));
          childFrame = childFrame->GetNextSibling();
        }
      }
    }
  }

  // Finished with these:
  ClearSavedChildMetrics();
  // Set our overflow area
  GatherAndStoreOverflow(&aDesiredStretchSize);

  // There used to be code here to change the height of the child frame to
  // change the caret height, but the text frame that manages the caret is now
  // not a direct child but wrapped in a block frame.  See also bug 412033.

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmoFrame::InheritAutomaticData(nsIFrame* aParent)
{
  // retain our native direction, it only changes if our text content changes
  nsStretchDirection direction = mEmbellishData.direction;
  nsMathMLTokenFrame::InheritAutomaticData(aParent);
  ProcessTextData();
  mEmbellishData.direction = direction;
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmoFrame::TransmitAutomaticData()
{
  // this will cause us to re-sync our flags from scratch
  // but our returned 'form' is still not final (bug 133429), it will
  // be recomputed to its final value during the next call in Reflow()
  mEmbellishData.coreFrame = nullptr;
  ProcessOperatorData();
  return NS_OK;
}

void
nsMathMLmoFrame::SetInitialChildList(ChildListID     aListID,
                                     nsFrameList&    aChildList)
{
  // First, let the parent class do its work
  nsMathMLTokenFrame::SetInitialChildList(aListID, aChildList);
  ProcessTextData();
}

void
nsMathMLmoFrame::Reflow(nsPresContext*          aPresContext,
                        nsHTMLReflowMetrics&     aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&          aStatus)
{
  // certain values use units that depend on our style context, so
  // it is safer to just process the whole lot here
  ProcessOperatorData();

  nsMathMLTokenFrame::Reflow(aPresContext, aDesiredSize,
                             aReflowState, aStatus);
}

nsresult
nsMathMLmoFrame::Place(nsRenderingContext&  aRenderingContext,
                       bool                 aPlaceOrigin,
                       nsHTMLReflowMetrics& aDesiredSize)
{
  nsresult rv = nsMathMLTokenFrame::Place(aRenderingContext, aPlaceOrigin,
                                          aDesiredSize);

  if (NS_FAILED(rv)) {
    return rv;
  }

  /* Special behaviour for largeops.
     In MathML "stretchy" and displaystyle "largeop" are different notions,
     even if we use the same technique to draw them (picking size variants).
     So largeop display operators should be considered "non-stretchy" and
     thus their sizes should be taken into account for the stretch size of
     other elements.

     This is a preliminary stretch - exact sizing/placement is handled by the
     Stretch() method.
  */

  if (!aPlaceOrigin &&
      StyleFont()->mMathDisplay == NS_MATHML_DISPLAYSTYLE_BLOCK &&
      NS_MATHML_OPERATOR_IS_LARGEOP(mFlags) && UseMathMLChar()) {
    nsBoundingMetrics newMetrics;
    rv = mMathMLChar.Stretch(PresContext(), aRenderingContext,
                                      nsLayoutUtils::FontSizeInflationFor(this),
                                      NS_STRETCH_DIRECTION_VERTICAL,
                                      aDesiredSize.mBoundingMetrics, newMetrics,
                                      NS_STRETCH_LARGEOP, StyleVisibility()->mDirection);

    if (NS_FAILED(rv)) {
      // Just use the initial size
      return NS_OK;
    }

    aDesiredSize.mBoundingMetrics = newMetrics;
     /* Treat the ascent/descent values calculated in the TokenFrame place
       calculations as the minimum for aDesiredSize calculations, rather
       than fetching them from font metrics again.
    */
    aDesiredSize.SetBlockStartAscent(std::max(mBoundingMetrics.ascent,
                                              newMetrics.ascent));
    aDesiredSize.Height() = aDesiredSize.BlockStartAscent() +
                            std::max(mBoundingMetrics.descent,
                                     newMetrics.descent);
    aDesiredSize.Width() = newMetrics.width;
    mBoundingMetrics = newMetrics;
  }
  return NS_OK;
}

/* virtual */ void
nsMathMLmoFrame::MarkIntrinsicISizesDirty()
{
  // if we get this, it may mean that something changed in the text
  // content. So blow away everything an re-build the automatic data
  // from the parent of our outermost embellished container (we ensure
  // that we are the core, not just a sibling of the core)

  ProcessTextData();

  nsIFrame* target = this;
  nsEmbellishData embellishData;
  do {
    target = target->GetParent();
    GetEmbellishDataFrom(target, embellishData);
  } while (embellishData.coreFrame == this);

  // we have automatic data to update in the children of the target frame
  // XXXldb This should really be marking dirty rather than rebuilding
  // so that we don't rebuild multiple times for the same change.
  RebuildAutomaticDataForChildren(target);

  nsMathMLContainerFrame::MarkIntrinsicISizesDirty();
}

/* virtual */ void
nsMathMLmoFrame::GetIntrinsicISizeMetrics(nsRenderingContext* aRenderingContext,
                                          nsHTMLReflowMetrics& aDesiredSize)
{
  ProcessOperatorData();
  if (UseMathMLChar()) {
    uint32_t stretchHint = GetStretchHint(mFlags, mPresentationData, true,
                                          StyleFont());
    aDesiredSize.Width() = mMathMLChar.
      GetMaxWidth(PresContext(), *aRenderingContext,
                  nsLayoutUtils::FontSizeInflationFor(this),
                  stretchHint);
  }
  else {
    nsMathMLTokenFrame::GetIntrinsicISizeMetrics(aRenderingContext,
                                                 aDesiredSize);
  }

  // leadingSpace and trailingSpace are actually applied to the outermost
  // embellished container but for determining total intrinsic width it should
  // be safe to include it for the core here instead.
  bool isRTL = StyleVisibility()->mDirection;
  aDesiredSize.Width() +=
    mEmbellishData.leadingSpace + mEmbellishData.trailingSpace;
  aDesiredSize.mBoundingMetrics.width = aDesiredSize.Width();
  if (isRTL) {
    aDesiredSize.mBoundingMetrics.leftBearing += mEmbellishData.trailingSpace;
    aDesiredSize.mBoundingMetrics.rightBearing += mEmbellishData.trailingSpace;
  } else {
    aDesiredSize.mBoundingMetrics.leftBearing += mEmbellishData.leadingSpace;
    aDesiredSize.mBoundingMetrics.rightBearing += mEmbellishData.leadingSpace;
  }
}

nsresult
nsMathMLmoFrame::AttributeChanged(int32_t         aNameSpaceID,
                                  nsIAtom*        aAttribute,
                                  int32_t         aModType)
{
  // check if this is an attribute that can affect the embellished hierarchy
  // in a significant way and re-layout the entire hierarchy.
  if (nsGkAtoms::accent_ == aAttribute ||
      nsGkAtoms::movablelimits_ == aAttribute) {

    // set the target as the parent of our outermost embellished container
    // (we ensure that we are the core, not just a sibling of the core)
    nsIFrame* target = this;
    nsEmbellishData embellishData;
    do {
      target = target->GetParent();
      GetEmbellishDataFrom(target, embellishData);
    } while (embellishData.coreFrame == this);

    // we have automatic data to update in the children of the target frame
    return ReLayoutChildren(target);
  }

  return nsMathMLTokenFrame::
         AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

// ----------------------
// No need to track the style context given to our MathML char. 
// the Style System will use these to pass the proper style context to our MathMLChar
nsStyleContext*
nsMathMLmoFrame::GetAdditionalStyleContext(int32_t aIndex) const
{
  switch (aIndex) {
  case NS_MATHML_CHAR_STYLE_CONTEXT_INDEX:
    return mMathMLChar.GetStyleContext();
  default:
    return nullptr;
  }
}

void
nsMathMLmoFrame::SetAdditionalStyleContext(int32_t          aIndex,
                                           nsStyleContext*  aStyleContext)
{
  switch (aIndex) {
  case NS_MATHML_CHAR_STYLE_CONTEXT_INDEX:
    mMathMLChar.SetStyleContext(aStyleContext);
    break;
  }
}

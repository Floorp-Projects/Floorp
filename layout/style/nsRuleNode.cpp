/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * a node in the lexicographic tree of rules that match an element,
 * responsible for converting the rules' information into computed style
 */

#include <algorithm>

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Likely.h"
#include "mozilla/LookAndFeel.h"

#include "nsAlgorithm.h" // for clamped()
#include "nsRuleNode.h"
#include "nscore.h"
#include "nsIWidget.h"
#include "nsIPresShell.h"
#include "nsFontMetrics.h"
#include "gfxFont.h"
#include "nsCSSPseudoElements.h"
#include "nsThemeConstants.h"
#include "pldhash.h"
#include "nsStyleContext.h"
#include "nsStyleSet.h"
#include "nsStyleStruct.h"
#include "nsSize.h"
#include "nsRuleData.h"
#include "nsIStyleRule.h"
#include "nsBidiUtils.h"
#include "nsStyleStructInlines.h"
#include "nsCSSProps.h"
#include "nsTArray.h"
#include "nsContentUtils.h"
#include "CSSCalc.h"
#include "nsPrintfCString.h"
#include "nsRenderingContext.h"
#include "nsStyleUtil.h"
#include "nsIDocument.h"
#include "prtime.h"
#include "CSSVariableResolver.h"
#include "nsCSSParser.h"
#include "CounterStyleManager.h"
#include "nsCSSPropertySet.h"
#include "mozilla/RuleNodeCacheConditions.h"
#include "nsDeviceContext.h"

#if defined(_MSC_VER) || defined(__MINGW32__)
#include <malloc.h>
#ifdef _MSC_VER
#define alloca _alloca
#endif
#endif
#ifdef SOLARIS
#include <alloca.h>
#endif

using std::max;
using std::min;
using namespace mozilla;
using namespace mozilla::dom;

#define NS_SET_IMAGE_REQUEST(method_, context_, request_)                   \
  if ((context_)->PresContext()->IsDynamic()) {                               \
    method_(request_);                                                      \
  } else {                                                                  \
    nsRefPtr<imgRequestProxy> req = nsContentUtils::GetStaticRequest(request_); \
    method_(req);                                                           \
  }

#define NS_SET_IMAGE_REQUEST_WITH_DOC(method_, context_, requestgetter_)      \
  {                                                                           \
    nsIDocument* doc = (context_)->PresContext()->Document();                 \
    NS_SET_IMAGE_REQUEST(method_, context_, requestgetter_(doc))              \
  }

/* Helper function to convert a CSS <position> specified value into its
 * computed-style form. */
static void
  ComputePositionValue(nsStyleContext* aStyleContext,
                       const nsCSSValue& aValue,
                       nsStyleBackground::Position& aComputedValue,
                       RuleNodeCacheConditions& aConditions);

/*
 * For storage of an |nsRuleNode|'s children in a PLDHashTable.
 */

struct ChildrenHashEntry : public PLDHashEntryHdr {
  // key is |mRuleNode->GetKey()|
  nsRuleNode *mRuleNode;
};

/* static */ PLDHashNumber
nsRuleNode::ChildrenHashHashKey(PLDHashTable *aTable, const void *aKey)
{
  const nsRuleNode::Key *key =
    static_cast<const nsRuleNode::Key*>(aKey);
  // Disagreement on importance and level for the same rule is extremely
  // rare, so hash just on the rule.
  return PL_DHashVoidPtrKeyStub(aTable, key->mRule);
}

/* static */ bool
nsRuleNode::ChildrenHashMatchEntry(PLDHashTable *aTable,
                                   const PLDHashEntryHdr *aHdr,
                                   const void *aKey)
{
  const ChildrenHashEntry *entry =
    static_cast<const ChildrenHashEntry*>(aHdr);
  const nsRuleNode::Key *key =
    static_cast<const nsRuleNode::Key*>(aKey);
  return entry->mRuleNode->GetKey() == *key;
}

/* static */ const PLDHashTableOps
nsRuleNode::ChildrenHashOps = {
  // It's probably better to allocate the table itself using malloc and
  // free rather than the pres shell's arena because the table doesn't
  // grow very often and the pres shell's arena doesn't recycle very
  // large size allocations.
  ChildrenHashHashKey,
  ChildrenHashMatchEntry,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  nullptr
};


// EnsureBlockDisplay:
// Never change display:none or display:contents *ever*, otherwise:
//  - if the display value (argument) is not a block-type
//    then we set it to a valid block display value
//  - For enforcing the floated/positioned element CSS2 rules
//  - We allow the behavior of "list-item" to be customized.
//    CSS21 says that position/float do not convert 'list-item' to 'block',
//    but it explicitly does not define whether 'list-item' should be
//    converted to block *on the root node*. To allow for flexibility
//    (so that we don't have to support a list-item root node), this method
//    lets the caller pick either behavior, using the 'aConvertListItem' arg.
//    Reference: http://www.w3.org/TR/CSS21/visuren.html#dis-pos-flo
/* static */
void
nsRuleNode::EnsureBlockDisplay(uint8_t& display,
                               bool aConvertListItem /* = false */)
{
  // see if the display value is already a block
  switch (display) {
  case NS_STYLE_DISPLAY_LIST_ITEM :
    if (aConvertListItem) {
      display = NS_STYLE_DISPLAY_BLOCK;
      break;
    } // else, fall through to share the 'break' for non-changing display vals
  case NS_STYLE_DISPLAY_NONE :
  case NS_STYLE_DISPLAY_CONTENTS :
    // never change display:none or display:contents *ever*
  case NS_STYLE_DISPLAY_TABLE :
  case NS_STYLE_DISPLAY_BLOCK :
  case NS_STYLE_DISPLAY_FLEX :
  case NS_STYLE_DISPLAY_GRID :
    // do not muck with these at all - already blocks
    // This is equivalent to nsStyleDisplay::IsBlockOutside.  (XXX Maybe we
    // should just call that?)
    // This needs to match the check done in
    // nsCSSFrameConstructor::FindMathMLData for <math>.
    break;

  case NS_STYLE_DISPLAY_INLINE_TABLE :
    // make inline tables into tables
    display = NS_STYLE_DISPLAY_TABLE;
    break;

  case NS_STYLE_DISPLAY_INLINE_FLEX:
    // make inline flex containers into flex containers
    display = NS_STYLE_DISPLAY_FLEX;
    break;

  case NS_STYLE_DISPLAY_INLINE_GRID:
    // make inline grid containers into grid containers
    display = NS_STYLE_DISPLAY_GRID;
    break;

  default :
    // make it a block
    display = NS_STYLE_DISPLAY_BLOCK;
  }
}

// EnsureInlineDisplay:
//  - if the display value (argument) is not an inline type
//    then we set it to a valid inline display value
/* static */
void
nsRuleNode::EnsureInlineDisplay(uint8_t& display)
{
  // see if the display value is already inline
  switch (display) {
    case NS_STYLE_DISPLAY_BLOCK :
      display = NS_STYLE_DISPLAY_INLINE_BLOCK;
      break;
    case NS_STYLE_DISPLAY_TABLE :
      display = NS_STYLE_DISPLAY_INLINE_TABLE;
      break;
    case NS_STYLE_DISPLAY_FLEX :
      display = NS_STYLE_DISPLAY_INLINE_FLEX;
      break;
    case NS_STYLE_DISPLAY_GRID :
      display = NS_STYLE_DISPLAY_INLINE_GRID;
      break;
    case NS_STYLE_DISPLAY_BOX:
      display = NS_STYLE_DISPLAY_INLINE_BOX;
      break;
    case NS_STYLE_DISPLAY_STACK:
      display = NS_STYLE_DISPLAY_INLINE_STACK;
      break;
  }
}

static nscoord CalcLengthWith(const nsCSSValue& aValue,
                              nscoord aFontSize,
                              const nsStyleFont* aStyleFont,
                              nsStyleContext* aStyleContext,
                              nsPresContext* aPresContext,
                              bool aUseProvidedRootEmSize,
                              bool aUseUserFontSet,
                              RuleNodeCacheConditions& aConditions);

struct CalcLengthCalcOps : public css::BasicCoordCalcOps,
                           public css::NumbersAlreadyNormalizedOps
{
  // All of the parameters to CalcLengthWith except aValue.
  const nscoord mFontSize;
  const nsStyleFont* const mStyleFont;
  nsStyleContext* const mStyleContext;
  nsPresContext* const mPresContext;
  const bool mUseProvidedRootEmSize;
  const bool mUseUserFontSet;
  RuleNodeCacheConditions& mConditions;

  CalcLengthCalcOps(nscoord aFontSize, const nsStyleFont* aStyleFont,
                    nsStyleContext* aStyleContext, nsPresContext* aPresContext,
                    bool aUseProvidedRootEmSize, bool aUseUserFontSet,
                    RuleNodeCacheConditions& aConditions)
    : mFontSize(aFontSize),
      mStyleFont(aStyleFont),
      mStyleContext(aStyleContext),
      mPresContext(aPresContext),
      mUseProvidedRootEmSize(aUseProvidedRootEmSize),
      mUseUserFontSet(aUseUserFontSet),
      mConditions(aConditions)
  {
  }

  result_type ComputeLeafValue(const nsCSSValue& aValue)
  {
    return CalcLengthWith(aValue, mFontSize, mStyleFont,
                          mStyleContext, mPresContext, mUseProvidedRootEmSize,
                          mUseUserFontSet, mConditions);
  }
};

static inline nscoord ScaleCoordRound(const nsCSSValue& aValue, float aFactor)
{
  return NSToCoordRoundWithClamp(aValue.GetFloatValue() * aFactor);
}

static inline nscoord ScaleViewportCoordTrunc(const nsCSSValue& aValue,
                                              nscoord aViewportSize)
{
  // For units (like percentages and viewport units) where authors might
  // repeatedly use a value and expect some multiple of the value to be
  // smaller than a container, we need to use floor rather than round.
  // We need to use division by 100.0 rather than multiplication by 0.1f
  // to avoid introducing error.
  return NSToCoordTruncClamped(aValue.GetFloatValue() *
                               aViewportSize / 100.0f);
}

already_AddRefed<nsFontMetrics>
GetMetricsFor(nsPresContext* aPresContext,
              nsStyleContext* aStyleContext,
              const nsStyleFont* aStyleFont,
              nscoord aFontSize, // overrides value from aStyleFont
              bool aUseUserFontSet)
{
  nsFont font = aStyleFont->mFont;
  font.size = aFontSize;
  gfxUserFontSet *fs = nullptr;
  if (aUseUserFontSet) {
    fs = aPresContext->GetUserFontSet();
  }
  gfxTextPerfMetrics *tp = aPresContext->GetTextPerfMetrics();
  gfxFont::Orientation orientation = gfxFont::eHorizontal;
  if (aStyleContext) {
    WritingMode wm(aStyleContext);
    if (wm.IsVertical() && !wm.IsSideways()) {
      orientation = gfxFont::eVertical;
    }
  }
  nsRefPtr<nsFontMetrics> fm;
  aPresContext->DeviceContext()->GetMetricsFor(font,
                                               aStyleFont->mLanguage,
                                               aStyleFont->mExplicitLanguage,
                                               orientation,
                                               fs, tp, *getter_AddRefs(fm));
  return fm.forget();
}


static nsSize CalcViewportUnitsScale(nsPresContext* aPresContext)
{
  // The caller is making use of viewport units, so notify the pres context
  // that it will need to rebuild the rule tree if the size of the viewport
  // changes.
  aPresContext->SetUsesViewportUnits(true);

  // The default (when we have 'overflow: auto' on the root element, or
  // trivially for 'overflow: hidden' since we never have scrollbars in that
  // case) is to define the scale of the viewport units without considering
  // scrollbars.
  nsSize viewportSize(aPresContext->GetVisibleArea().Size());

  // Check for 'overflow: scroll' styles on the root scroll frame. If we find
  // any, the standard requires us to take scrollbars into account.
  nsIScrollableFrame* scrollFrame =
    aPresContext->PresShell()->GetRootScrollFrameAsScrollable();
  if (scrollFrame) {
    ScrollbarStyles styles(scrollFrame->GetScrollbarStyles());

    if (styles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL ||
        styles.mVertical == NS_STYLE_OVERFLOW_SCROLL) {
      // Gather scrollbar size information.
      nsRenderingContext context(
        aPresContext->PresShell()->CreateReferenceRenderingContext());
      nsMargin sizes(scrollFrame->GetDesiredScrollbarSizes(aPresContext, &context));

      if (styles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL) {
        // 'overflow-x: scroll' means we must consider the horizontal scrollbar,
        // which affects the scale of viewport height units.
        viewportSize.height -= sizes.TopBottom();
      }

      if (styles.mVertical == NS_STYLE_OVERFLOW_SCROLL) {
        // 'overflow-y: scroll' means we must consider the vertical scrollbar,
        // which affects the scale of viewport width units.
        viewportSize.width -= sizes.LeftRight();
      }
    }
  }

  return viewportSize;
}

static nscoord CalcLengthWith(const nsCSSValue& aValue,
                              nscoord aFontSize,
                              const nsStyleFont* aStyleFont,
                              nsStyleContext* aStyleContext,
                              nsPresContext* aPresContext,
                              bool aUseProvidedRootEmSize,
                              // aUseUserFontSet should always be true
                              // except when called from
                              // CalcLengthWithInitialFont.
                              bool aUseUserFontSet,
                              RuleNodeCacheConditions& aConditions)
{
  NS_ASSERTION(aValue.IsLengthUnit() || aValue.IsCalcUnit(),
               "not a length or calc unit");
  NS_ASSERTION(aStyleFont || aStyleContext,
               "Must have style data");
  NS_ASSERTION(!aStyleFont || !aStyleContext,
               "Duplicate sources of data");
  NS_ASSERTION(aPresContext, "Must have prescontext");

  if (aValue.IsFixedLengthUnit()) {
    return aValue.GetFixedLength(aPresContext);
  }
  if (aValue.IsPixelLengthUnit()) {
    return aValue.GetPixelLength();
  }
  if (aValue.IsCalcUnit()) {
    // For properties for which lengths are the *only* units accepted in
    // calc(), we can handle calc() here and just compute a final
    // result.  We ensure that we don't get to this code for other
    // properties by not calling CalcLength in those cases:  SetCoord
    // only calls CalcLength for a calc when it is appropriate to do so.
    CalcLengthCalcOps ops(aFontSize, aStyleFont,
                          aStyleContext, aPresContext,
                          aUseProvidedRootEmSize, aUseUserFontSet,
                          aConditions);
    return css::ComputeCalc(aValue, ops);
  }
  switch (aValue.GetUnit()) {
    // nsPresContext::SetVisibleArea and
    // nsPresContext::MediaFeatureValuesChanged handle dynamic changes
    // of the basis for viewport units by rebuilding the rule tree and
    // style context tree.  Not caching them in the rule tree wouldn't
    // be sufficient to handle these changes because we also need a way
    // to get rid of cached values in the style context tree without any
    // changes in specified style.  We can either do this by not caching
    // in the rule tree and then throwing away the style context tree
    // for dynamic viewport size changes, or by allowing caching in the
    // rule tree and using the existing rebuild style data path that
    // throws away the style context and the rule tree.
    // Thus we do cache viewport units in the rule tree.  This allows us
    // to benefit from the performance advantages of the rule tree
    // (e.g., faster dynamic changes on other things, like transforms)
    // and allows us not to need an additional code path, in exchange
    // for an increased cost to dynamic changes to the viewport size
    // when viewport units are in use.
    case eCSSUnit_ViewportWidth: {
      nscoord viewportWidth = CalcViewportUnitsScale(aPresContext).width;
      return ScaleViewportCoordTrunc(aValue, viewportWidth);
    }
    case eCSSUnit_ViewportHeight: {
      nscoord viewportHeight = CalcViewportUnitsScale(aPresContext).height;
      return ScaleViewportCoordTrunc(aValue, viewportHeight);
    }
    case eCSSUnit_ViewportMin: {
      nsSize vuScale(CalcViewportUnitsScale(aPresContext));
      nscoord viewportMin = min(vuScale.width, vuScale.height);
      return ScaleViewportCoordTrunc(aValue, viewportMin);
    }
    case eCSSUnit_ViewportMax: {
      nsSize vuScale(CalcViewportUnitsScale(aPresContext));
      nscoord viewportMax = max(vuScale.width, vuScale.height);
      return ScaleViewportCoordTrunc(aValue, viewportMax);
    }
    // While we could deal with 'rem' units correctly by simply not
    // caching any data that uses them in the rule tree, it's valuable
    // to store them in the rule tree (for faster dynamic changes of
    // other things).  And since the font size of the root element
    // changes rarely, we instead handle dynamic changes to the root
    // element's font size by rebuilding all style data in
    // nsCSSFrameConstructor::RestyleElement.
    case eCSSUnit_RootEM: {
      aPresContext->SetUsesRootEMUnits(true);
      nscoord rootFontSize;

      // NOTE: Be very careful with |styleFont|, since we haven't added any
      // conditions to aConditions or set it to uncacheable yet, so we don't
      // want to introduce any dependencies on aStyleContext's data here.
      const nsStyleFont *styleFont =
        aStyleFont ? aStyleFont : aStyleContext->StyleFont();

      if (aUseProvidedRootEmSize) {
        // We should use the provided aFontSize as the reference length to
        // scale. This only happens when we are calculating font-size or
        // an equivalent (scriptminsize or CalcLengthWithInitialFont) on
        // the root element, in which case aFontSize is already the
        // value we want.
        if (aFontSize == -1) {
          // XXX Should this be styleFont->mSize instead to avoid taking
          // minfontsize prefs into account?
          aFontSize = styleFont->mFont.size;
        }
        rootFontSize = aFontSize;
      } else if (aStyleContext && !aStyleContext->GetParent()) {
        // This is the root element (XXX we don't really know this, but
        // nsRuleNode::SetFont makes the same assumption!), so we should
        // use StyleFont on this context to get the root element's
        // font size.
        rootFontSize = styleFont->mFont.size;
      } else {
        // This is not the root element or we are calculating something other
        // than font size, so rem is relative to the root element's font size.
        nsRefPtr<nsStyleContext> rootStyle;
        const nsStyleFont *rootStyleFont = styleFont;
        Element* docElement = aPresContext->Document()->GetRootElement();

        if (docElement) {
          nsIFrame* rootFrame = docElement->GetPrimaryFrame();
          if (rootFrame) {
            rootStyle = rootFrame->StyleContext();
          } else {
            rootStyle = aPresContext->StyleSet()->ResolveStyleFor(docElement,
                                                                  nullptr);
          }
          rootStyleFont = rootStyle->StyleFont();
        }

        rootFontSize = rootStyleFont->mFont.size;
      }

      return ScaleCoordRound(aValue, float(rootFontSize));
    }
    default:
      // Fall through to the code for units that can't be stored in the
      // rule tree because they depend on font data.
      break;
  }
  // Common code for units that depend on the element's font data and
  // thus can't be stored in the rule tree:
  const nsStyleFont *styleFont =
    aStyleFont ? aStyleFont : aStyleContext->StyleFont();
  if (aFontSize == -1) {
    // XXX Should this be styleFont->mSize instead to avoid taking minfontsize
    // prefs into account?
    aFontSize = styleFont->mFont.size;
  }
  switch (aValue.GetUnit()) {
    case eCSSUnit_EM: {
      // CSS2.1 specifies that this unit scales to the computed font
      // size, not the em-width in the font metrics, despite the name.
      aConditions.SetFontSizeDependency(aFontSize);
      return ScaleCoordRound(aValue, float(aFontSize));
    }
    case eCSSUnit_XHeight: {
      aPresContext->SetUsesExChUnits(true);
      nsRefPtr<nsFontMetrics> fm =
        GetMetricsFor(aPresContext, aStyleContext, styleFont,
                      aFontSize, aUseUserFontSet);
      aConditions.SetUncacheable();
      return ScaleCoordRound(aValue, float(fm->XHeight()));
    }
    case eCSSUnit_Char: {
      aPresContext->SetUsesExChUnits(true);
      nsRefPtr<nsFontMetrics> fm =
        GetMetricsFor(aPresContext, aStyleContext, styleFont,
                      aFontSize, aUseUserFontSet);
      gfxFloat zeroWidth =
        fm->GetThebesFontGroup()->GetFirstValidFont()->
          GetMetrics(fm->Orientation()).zeroOrAveCharWidth;

      aConditions.SetUncacheable();
      return ScaleCoordRound(aValue, ceil(aPresContext->AppUnitsPerDevPixel() *
                                          zeroWidth));
    }
    default:
      NS_NOTREACHED("unexpected unit");
      break;
  }
  return 0;
}

/* static */ nscoord
nsRuleNode::CalcLength(const nsCSSValue& aValue,
                       nsStyleContext* aStyleContext,
                       nsPresContext* aPresContext,
                       RuleNodeCacheConditions& aConditions)
{
  NS_ASSERTION(aStyleContext, "Must have style data");

  return CalcLengthWith(aValue, -1, nullptr,
                        aStyleContext, aPresContext,
                        false, true, aConditions);
}

/* Inline helper function to redirect requests to CalcLength. */
static inline nscoord CalcLength(const nsCSSValue& aValue,
                                 nsStyleContext* aStyleContext,
                                 nsPresContext* aPresContext,
                                 RuleNodeCacheConditions& aConditions)
{
  return nsRuleNode::CalcLength(aValue, aStyleContext,
                                aPresContext, aConditions);
}

/* static */ nscoord
nsRuleNode::CalcLengthWithInitialFont(nsPresContext* aPresContext,
                                      const nsCSSValue& aValue)
{
  nsStyleFont defaultFont(aPresContext); // FIXME: best language?
  RuleNodeCacheConditions conditions;
  return CalcLengthWith(aValue, -1, &defaultFont,
                        nullptr, aPresContext,
                        true, false, conditions);
}

struct LengthPercentPairCalcOps : public css::NumbersAlreadyNormalizedOps
{
  typedef nsRuleNode::ComputedCalc result_type;

  LengthPercentPairCalcOps(nsStyleContext* aContext,
                           nsPresContext* aPresContext,
                           RuleNodeCacheConditions& aConditions)
    : mContext(aContext),
      mPresContext(aPresContext),
      mConditions(aConditions),
      mHasPercent(false) {}

  nsStyleContext* mContext;
  nsPresContext* mPresContext;
  RuleNodeCacheConditions& mConditions;
  bool mHasPercent;

  result_type ComputeLeafValue(const nsCSSValue& aValue)
  {
    if (aValue.GetUnit() == eCSSUnit_Percent) {
      mHasPercent = true;
      return result_type(0, aValue.GetPercentValue());
    }
    return result_type(CalcLength(aValue, mContext, mPresContext,
                                  mConditions),
                       0.0f);
  }

  result_type
  MergeAdditive(nsCSSUnit aCalcFunction,
                result_type aValue1, result_type aValue2)
  {
    if (aCalcFunction == eCSSUnit_Calc_Plus) {
      return result_type(NSCoordSaturatingAdd(aValue1.mLength,
                                              aValue2.mLength),
                         aValue1.mPercent + aValue2.mPercent);
    }
    MOZ_ASSERT(aCalcFunction == eCSSUnit_Calc_Minus,
               "min() and max() are not allowed in calc() on transform");
    return result_type(NSCoordSaturatingSubtract(aValue1.mLength,
                                                 aValue2.mLength, 0),
                       aValue1.mPercent - aValue2.mPercent);
  }

  result_type
  MergeMultiplicativeL(nsCSSUnit aCalcFunction,
                       float aValue1, result_type aValue2)
  {
    MOZ_ASSERT(aCalcFunction == eCSSUnit_Calc_Times_L,
               "unexpected unit");
    return result_type(NSCoordSaturatingMultiply(aValue2.mLength, aValue1),
                       aValue1 * aValue2.mPercent);
  }

  result_type
  MergeMultiplicativeR(nsCSSUnit aCalcFunction,
                       result_type aValue1, float aValue2)
  {
    MOZ_ASSERT(aCalcFunction == eCSSUnit_Calc_Times_R ||
               aCalcFunction == eCSSUnit_Calc_Divided,
               "unexpected unit");
    if (aCalcFunction == eCSSUnit_Calc_Divided) {
      aValue2 = 1.0f / aValue2;
    }
    return result_type(NSCoordSaturatingMultiply(aValue1.mLength, aValue2),
                       aValue1.mPercent * aValue2);
  }

};

static void
SpecifiedCalcToComputedCalc(const nsCSSValue& aValue, nsStyleCoord& aCoord, 
                            nsStyleContext* aStyleContext,
                            RuleNodeCacheConditions& aConditions)
{
  LengthPercentPairCalcOps ops(aStyleContext, aStyleContext->PresContext(),
                               aConditions);
  nsRuleNode::ComputedCalc vals = ComputeCalc(aValue, ops);

  nsStyleCoord::Calc* calcObj = new nsStyleCoord::Calc;

  calcObj->mLength = vals.mLength;
  calcObj->mPercent = vals.mPercent;
  calcObj->mHasPercent = ops.mHasPercent;

  aCoord.SetCalcValue(calcObj);
}

/* static */ nsRuleNode::ComputedCalc
nsRuleNode::SpecifiedCalcToComputedCalc(const nsCSSValue& aValue,
                                        nsStyleContext* aStyleContext,
                                        nsPresContext* aPresContext,
                                        RuleNodeCacheConditions& aConditions)
{
  LengthPercentPairCalcOps ops(aStyleContext, aPresContext,
                               aConditions);
  return ComputeCalc(aValue, ops);
}

// This is our public API for handling calc() expressions that involve
// percentages.
/* static */ nscoord
nsRuleNode::ComputeComputedCalc(const nsStyleCoord& aValue,
                                nscoord aPercentageBasis)
{
  nsStyleCoord::Calc* calc = aValue.GetCalcValue();
  return calc->mLength +
         NSToCoordFloorClamped(aPercentageBasis * calc->mPercent);
}

/* static */ nscoord
nsRuleNode::ComputeCoordPercentCalc(const nsStyleCoord& aCoord,
                                    nscoord aPercentageBasis)
{
  switch (aCoord.GetUnit()) {
    case eStyleUnit_Coord:
      return aCoord.GetCoordValue();
    case eStyleUnit_Percent:
      return NSToCoordFloorClamped(aPercentageBasis * aCoord.GetPercentValue());
    case eStyleUnit_Calc:
      return ComputeComputedCalc(aCoord, aPercentageBasis);
    default:
      MOZ_ASSERT(false, "unexpected unit");
      return 0;
  }
}

/* Given an enumerated value that represents a box position, converts it to
 * a float representing the percentage of the box it corresponds to.  For
 * example, "center" becomes 0.5f.
 *
 * @param aEnumValue The enumerated value.
 * @return The float percent it corresponds to.
 */
static float
GetFloatFromBoxPosition(int32_t aEnumValue)
{
  switch (aEnumValue) {
  case NS_STYLE_BG_POSITION_LEFT:
  case NS_STYLE_BG_POSITION_TOP:
    return 0.0f;
  case NS_STYLE_BG_POSITION_RIGHT:
  case NS_STYLE_BG_POSITION_BOTTOM:
    return 1.0f;
  default:
    NS_NOTREACHED("unexpected value");
    // fall through
  case NS_STYLE_BG_POSITION_CENTER:
    return 0.5f;
  }
}

#define SETCOORD_NORMAL                 0x01   // N
#define SETCOORD_AUTO                   0x02   // A
#define SETCOORD_INHERIT                0x04   // H
#define SETCOORD_PERCENT                0x08   // P
#define SETCOORD_FACTOR                 0x10   // F
#define SETCOORD_LENGTH                 0x20   // L
#define SETCOORD_INTEGER                0x40   // I
#define SETCOORD_ENUMERATED             0x80   // E
#define SETCOORD_NONE                   0x100  // O
#define SETCOORD_INITIAL_ZERO           0x200
#define SETCOORD_INITIAL_AUTO           0x400
#define SETCOORD_INITIAL_NONE           0x800
#define SETCOORD_INITIAL_NORMAL         0x1000
#define SETCOORD_INITIAL_HALF           0x2000
#define SETCOORD_INITIAL_HUNDRED_PCT    0x00004000
#define SETCOORD_INITIAL_FACTOR_ONE     0x00008000
#define SETCOORD_INITIAL_FACTOR_ZERO    0x00010000
#define SETCOORD_CALC_LENGTH_ONLY       0x00020000
#define SETCOORD_CALC_CLAMP_NONNEGATIVE 0x00040000 // modifier for CALC_LENGTH_ONLY
#define SETCOORD_STORE_CALC             0x00080000
#define SETCOORD_BOX_POSITION           0x00100000 // exclusive with _ENUMERATED
#define SETCOORD_ANGLE                  0x00200000
#define SETCOORD_UNSET_INHERIT          0x00400000
#define SETCOORD_UNSET_INITIAL          0x00800000

#define SETCOORD_LP     (SETCOORD_LENGTH | SETCOORD_PERCENT)
#define SETCOORD_LH     (SETCOORD_LENGTH | SETCOORD_INHERIT)
#define SETCOORD_AH     (SETCOORD_AUTO | SETCOORD_INHERIT)
#define SETCOORD_LAH    (SETCOORD_AUTO | SETCOORD_LENGTH | SETCOORD_INHERIT)
#define SETCOORD_LPH    (SETCOORD_LP | SETCOORD_INHERIT)
#define SETCOORD_LPAH   (SETCOORD_LP | SETCOORD_AH)
#define SETCOORD_LPE    (SETCOORD_LP | SETCOORD_ENUMERATED)
#define SETCOORD_LPEH   (SETCOORD_LPE | SETCOORD_INHERIT)
#define SETCOORD_LPAEH  (SETCOORD_LPAH | SETCOORD_ENUMERATED)
#define SETCOORD_LPO    (SETCOORD_LP | SETCOORD_NONE)
#define SETCOORD_LPOH   (SETCOORD_LPH | SETCOORD_NONE)
#define SETCOORD_LPOEH  (SETCOORD_LPOH | SETCOORD_ENUMERATED)
#define SETCOORD_LE     (SETCOORD_LENGTH | SETCOORD_ENUMERATED)
#define SETCOORD_LEH    (SETCOORD_LE | SETCOORD_INHERIT)
#define SETCOORD_IA     (SETCOORD_INTEGER | SETCOORD_AUTO)
#define SETCOORD_LAE    (SETCOORD_LENGTH | SETCOORD_AUTO | SETCOORD_ENUMERATED)

// changes aCoord iff it returns true
static bool SetCoord(const nsCSSValue& aValue, nsStyleCoord& aCoord,
                       const nsStyleCoord& aParentCoord,
                       int32_t aMask, nsStyleContext* aStyleContext,
                       nsPresContext* aPresContext,
                       RuleNodeCacheConditions& aConditions)
{
  bool result = true;
  if (aValue.GetUnit() == eCSSUnit_Null) {
    result = false;
  }
  else if ((((aMask & SETCOORD_LENGTH) != 0) &&
            aValue.IsLengthUnit()) ||
           (((aMask & SETCOORD_CALC_LENGTH_ONLY) != 0) &&
            aValue.IsCalcUnit())) {
    nscoord len = CalcLength(aValue, aStyleContext, aPresContext,
                             aConditions);
    if ((aMask & SETCOORD_CALC_CLAMP_NONNEGATIVE) && len < 0) {
      NS_ASSERTION(aValue.IsCalcUnit(),
                   "parser should have ensured no nonnegative lengths");
      len = 0;
    }
    aCoord.SetCoordValue(len);
  }
  else if (((aMask & SETCOORD_PERCENT) != 0) &&
           (aValue.GetUnit() == eCSSUnit_Percent)) {
    aCoord.SetPercentValue(aValue.GetPercentValue());
  }
  else if (((aMask & SETCOORD_INTEGER) != 0) &&
           (aValue.GetUnit() == eCSSUnit_Integer)) {
    aCoord.SetIntValue(aValue.GetIntValue(), eStyleUnit_Integer);
  }
  else if (((aMask & SETCOORD_ENUMERATED) != 0) &&
           (aValue.GetUnit() == eCSSUnit_Enumerated)) {
    aCoord.SetIntValue(aValue.GetIntValue(), eStyleUnit_Enumerated);
  }
  else if (((aMask & SETCOORD_BOX_POSITION) != 0) &&
           (aValue.GetUnit() == eCSSUnit_Enumerated)) {
    aCoord.SetPercentValue(GetFloatFromBoxPosition(aValue.GetIntValue()));
  }
  else if (((aMask & SETCOORD_AUTO) != 0) &&
           (aValue.GetUnit() == eCSSUnit_Auto)) {
    aCoord.SetAutoValue();
  }
  else if ((((aMask & SETCOORD_INHERIT) != 0) &&
            aValue.GetUnit() == eCSSUnit_Inherit) ||
           (((aMask & SETCOORD_UNSET_INHERIT) != 0) &&
            aValue.GetUnit() == eCSSUnit_Unset)) {
    aCoord = aParentCoord;  // just inherit value from parent
    aConditions.SetUncacheable();
  }
  else if (((aMask & SETCOORD_NORMAL) != 0) &&
           (aValue.GetUnit() == eCSSUnit_Normal)) {
    aCoord.SetNormalValue();
  }
  else if (((aMask & SETCOORD_NONE) != 0) &&
           (aValue.GetUnit() == eCSSUnit_None)) {
    aCoord.SetNoneValue();
  }
  else if (((aMask & SETCOORD_FACTOR) != 0) &&
           (aValue.GetUnit() == eCSSUnit_Number)) {
    aCoord.SetFactorValue(aValue.GetFloatValue());
  }
  else if (((aMask & SETCOORD_STORE_CALC) != 0) &&
           (aValue.IsCalcUnit())) {
    SpecifiedCalcToComputedCalc(aValue, aCoord, aStyleContext,
                                aConditions);
  }
  else if (aValue.GetUnit() == eCSSUnit_Initial ||
           (aValue.GetUnit() == eCSSUnit_Unset &&
            ((aMask & SETCOORD_UNSET_INITIAL) != 0))) {
    if ((aMask & SETCOORD_INITIAL_AUTO) != 0) {
      aCoord.SetAutoValue();
    }
    else if ((aMask & SETCOORD_INITIAL_ZERO) != 0) {
      aCoord.SetCoordValue(0);
    }
    else if ((aMask & SETCOORD_INITIAL_FACTOR_ZERO) != 0) {
      aCoord.SetFactorValue(0.0f);
    }
    else if ((aMask & SETCOORD_INITIAL_NONE) != 0) {
      aCoord.SetNoneValue();
    }
    else if ((aMask & SETCOORD_INITIAL_NORMAL) != 0) {
      aCoord.SetNormalValue();
    }
    else if ((aMask & SETCOORD_INITIAL_HALF) != 0) {
      aCoord.SetPercentValue(0.5f);
    }
    else if ((aMask & SETCOORD_INITIAL_HUNDRED_PCT) != 0) {
      aCoord.SetPercentValue(1.0f);
    }
    else if ((aMask & SETCOORD_INITIAL_FACTOR_ONE) != 0) {
      aCoord.SetFactorValue(1.0f);
    }
    else {
      result = false;  // didn't set anything
    }
  }
  else if ((aMask & SETCOORD_ANGLE) != 0 &&
           (aValue.IsAngularUnit())) {
    nsStyleUnit unit;
    switch (aValue.GetUnit()) {
      case eCSSUnit_Degree: unit = eStyleUnit_Degree; break;
      case eCSSUnit_Grad:   unit = eStyleUnit_Grad; break;
      case eCSSUnit_Radian: unit = eStyleUnit_Radian; break;
      case eCSSUnit_Turn:   unit = eStyleUnit_Turn; break;
      default: NS_NOTREACHED("unrecognized angular unit");
        unit = eStyleUnit_Degree;
    }
    aCoord.SetAngleValue(aValue.GetAngleValue(), unit);
  }
  else {
    result = false;  // didn't set anything
  }
  return result;
}

// This inline function offers a shortcut for SetCoord() by refusing to accept
// SETCOORD_LENGTH, SETCOORD_INHERIT and SETCOORD_UNSET_* masks.
static inline bool SetAbsCoord(const nsCSSValue& aValue,
                                 nsStyleCoord& aCoord,
                                 int32_t aMask)
{
  MOZ_ASSERT((aMask & (SETCOORD_LH | SETCOORD_UNSET_INHERIT |
                       SETCOORD_UNSET_INITIAL)) == 0,
             "does not handle SETCOORD_LENGTH, SETCOORD_INHERIT and "
             "SETCOORD_UNSET_*");

  // The values of the following variables will never be used; so it does not
  // matter what to set.
  const nsStyleCoord dummyParentCoord;
  nsStyleContext* dummyStyleContext = nullptr;
  nsPresContext* dummyPresContext = nullptr;
  RuleNodeCacheConditions dummyCacheKey;

  bool rv = SetCoord(aValue, aCoord, dummyParentCoord, aMask,
                       dummyStyleContext, dummyPresContext,
                       dummyCacheKey);
  MOZ_ASSERT(dummyCacheKey.CacheableWithoutDependencies(),
             "SetCoord() should not modify dummyCacheKey.");

  return rv;
}

/* Given a specified value that might be a pair value, call SetCoord twice,
 * either using each member of the pair, or using the unpaired value twice.
 */
static bool
SetPairCoords(const nsCSSValue& aValue,
              nsStyleCoord& aCoordX, nsStyleCoord& aCoordY,
              const nsStyleCoord& aParentX, const nsStyleCoord& aParentY,
              int32_t aMask, nsStyleContext* aStyleContext,
              nsPresContext* aPresContext, RuleNodeCacheConditions& aConditions)
{
  const nsCSSValue& valX =
    aValue.GetUnit() == eCSSUnit_Pair ? aValue.GetPairValue().mXValue : aValue;
  const nsCSSValue& valY =
    aValue.GetUnit() == eCSSUnit_Pair ? aValue.GetPairValue().mYValue : aValue;

  bool cX = SetCoord(valX, aCoordX, aParentX, aMask, aStyleContext,
                       aPresContext, aConditions);
  mozilla::DebugOnly<bool> cY = SetCoord(valY, aCoordY, aParentY, aMask, 
                       aStyleContext, aPresContext, aConditions);
  MOZ_ASSERT(cX == cY, "changed one but not the other");
  return cX;
}

static bool SetColor(const nsCSSValue& aValue, const nscolor aParentColor,
                       nsPresContext* aPresContext, nsStyleContext *aContext,
                       nscolor& aResult, RuleNodeCacheConditions& aConditions)
{
  bool    result = false;
  nsCSSUnit unit = aValue.GetUnit();

  if (aValue.IsNumericColorUnit()) {
    aResult = aValue.GetColorValue();
    result = true;
  }
  else if (eCSSUnit_Ident == unit) {
    nsAutoString  value;
    aValue.GetStringValue(value);
    nscolor rgba;
    if (NS_ColorNameToRGB(value, &rgba)) {
      aResult = rgba;
      result = true;
    }
  }
  else if (eCSSUnit_EnumColor == unit) {
    int32_t intValue = aValue.GetIntValue();
    if (0 <= intValue) {
      LookAndFeel::ColorID colorID = (LookAndFeel::ColorID) intValue;
      if (NS_SUCCEEDED(LookAndFeel::GetColor(colorID, &aResult))) {
        result = true;
      }
    }
    else {
      aResult = NS_RGB(0, 0, 0);
      result = false;
      switch (intValue) {
        case NS_COLOR_MOZ_HYPERLINKTEXT:
          if (aPresContext) {
            aResult = aPresContext->DefaultLinkColor();
            result = true;
          }
          break;
        case NS_COLOR_MOZ_VISITEDHYPERLINKTEXT:
          if (aPresContext) {
            aResult = aPresContext->DefaultVisitedLinkColor();
            result = true;
          }
          break;
        case NS_COLOR_MOZ_ACTIVEHYPERLINKTEXT:
          if (aPresContext) {
            aResult = aPresContext->DefaultActiveLinkColor();
            result = true;
          }
          break;
        case NS_COLOR_CURRENTCOLOR:
          // The data computed from this can't be shared in the rule tree
          // because they could be used on a node with a different color
          aConditions.SetUncacheable();
          if (aContext) {
            aResult = aContext->StyleColor()->mColor;
            result = true;
          }
          break;
        case NS_COLOR_MOZ_DEFAULT_COLOR:
          if (aPresContext) {
            aResult = aPresContext->DefaultColor();
            result = true;
          }
          break;
        case NS_COLOR_MOZ_DEFAULT_BACKGROUND_COLOR:
          if (aPresContext) {
            aResult = aPresContext->DefaultBackgroundColor();
            result = true;
          }
          break;
        default:
          NS_NOTREACHED("Should never have an unknown negative colorID.");
          break;
      }
    }
  }
  else if (eCSSUnit_Inherit == unit) {
    aResult = aParentColor;
    result = true;
    aConditions.SetUncacheable();
  }
  else if (eCSSUnit_Enumerated == unit &&
           aValue.GetIntValue() == NS_STYLE_COLOR_INHERIT_FROM_BODY) {
    NS_ASSERTION(aPresContext->CompatibilityMode() == eCompatibility_NavQuirks,
                 "Should only get this value in quirks mode");
    // We just grab the color from the prescontext, and rely on the fact that
    // if the body color ever changes all its descendants will get new style
    // contexts (but NOT necessarily new rulenodes).
    aResult = aPresContext->BodyTextColor();
    result = true;
    aConditions.SetUncacheable();
  }
  return result;
}

static void SetGradientCoord(const nsCSSValue& aValue, nsPresContext* aPresContext,
                             nsStyleContext* aContext, nsStyleCoord& aResult,
                             RuleNodeCacheConditions& aConditions)
{
  // OK to pass bad aParentCoord since we're not passing SETCOORD_INHERIT
  if (!SetCoord(aValue, aResult, nsStyleCoord(),
                SETCOORD_LPO | SETCOORD_BOX_POSITION | SETCOORD_STORE_CALC,
                aContext, aPresContext, aConditions)) {
    NS_NOTREACHED("unexpected unit for gradient anchor point");
    aResult.SetNoneValue();
  }
}

static void SetGradient(const nsCSSValue& aValue, nsPresContext* aPresContext,
                        nsStyleContext* aContext, nsStyleGradient& aResult,
                        RuleNodeCacheConditions& aConditions)
{
  MOZ_ASSERT(aValue.GetUnit() == eCSSUnit_Gradient,
             "The given data is not a gradient");

  const nsCSSValueGradient* gradient = aValue.GetGradientValue();

  if (gradient->mIsExplicitSize) {
    SetCoord(gradient->GetRadiusX(), aResult.mRadiusX, nsStyleCoord(),
             SETCOORD_LP | SETCOORD_STORE_CALC,
             aContext, aPresContext, aConditions);
    if (gradient->GetRadiusY().GetUnit() != eCSSUnit_None) {
      SetCoord(gradient->GetRadiusY(), aResult.mRadiusY, nsStyleCoord(),
               SETCOORD_LP | SETCOORD_STORE_CALC,
               aContext, aPresContext, aConditions);
      aResult.mShape = NS_STYLE_GRADIENT_SHAPE_ELLIPTICAL;
    } else {
      aResult.mRadiusY = aResult.mRadiusX;
      aResult.mShape = NS_STYLE_GRADIENT_SHAPE_CIRCULAR;
    }
    aResult.mSize = NS_STYLE_GRADIENT_SIZE_EXPLICIT_SIZE;
  } else if (gradient->mIsRadial) {
    if (gradient->GetRadialShape().GetUnit() == eCSSUnit_Enumerated) {
      aResult.mShape = gradient->GetRadialShape().GetIntValue();
    } else {
      NS_ASSERTION(gradient->GetRadialShape().GetUnit() == eCSSUnit_None,
                   "bad unit for radial shape");
      aResult.mShape = NS_STYLE_GRADIENT_SHAPE_ELLIPTICAL;
    }
    if (gradient->GetRadialSize().GetUnit() == eCSSUnit_Enumerated) {
      aResult.mSize = gradient->GetRadialSize().GetIntValue();
    } else {
      NS_ASSERTION(gradient->GetRadialSize().GetUnit() == eCSSUnit_None,
                   "bad unit for radial shape");
      aResult.mSize = NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER;
    }
  } else {
    NS_ASSERTION(gradient->GetRadialShape().GetUnit() == eCSSUnit_None,
                 "bad unit for linear shape");
    NS_ASSERTION(gradient->GetRadialSize().GetUnit() == eCSSUnit_None,
                 "bad unit for linear size");
    aResult.mShape = NS_STYLE_GRADIENT_SHAPE_LINEAR;
    aResult.mSize = NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER;
  }

  aResult.mLegacySyntax = gradient->mIsLegacySyntax;

  // bg-position
  SetGradientCoord(gradient->mBgPos.mXValue, aPresContext, aContext,
                   aResult.mBgPosX, aConditions);

  SetGradientCoord(gradient->mBgPos.mYValue, aPresContext, aContext,
                   aResult.mBgPosY, aConditions);

  aResult.mRepeating = gradient->mIsRepeating;

  // angle
  const nsStyleCoord dummyParentCoord;
  if (!SetCoord(gradient->mAngle, aResult.mAngle, dummyParentCoord, SETCOORD_ANGLE,
                aContext, aPresContext, aConditions)) {
    NS_ASSERTION(gradient->mAngle.GetUnit() == eCSSUnit_None,
                 "bad unit for gradient angle");
    aResult.mAngle.SetNoneValue();
  }

  // stops
  for (uint32_t i = 0; i < gradient->mStops.Length(); i++) {
    nsStyleGradientStop stop;
    const nsCSSValueGradientStop &valueStop = gradient->mStops[i];

    if (!SetCoord(valueStop.mLocation, stop.mLocation,
                  nsStyleCoord(), SETCOORD_LPO | SETCOORD_STORE_CALC,
                  aContext, aPresContext, aConditions)) {
      NS_NOTREACHED("unexpected unit for gradient stop location");
    }

    stop.mIsInterpolationHint = valueStop.mIsInterpolationHint;

    // inherit is not a valid color for stops, so we pass in a dummy
    // parent color
    NS_ASSERTION(valueStop.mColor.GetUnit() != eCSSUnit_Inherit,
                 "inherit is not a valid color for gradient stops");
    if (!valueStop.mIsInterpolationHint) {
      SetColor(valueStop.mColor, NS_RGB(0, 0, 0), aPresContext,
              aContext, stop.mColor, aConditions);
    } else {
      // Always initialize to the same color so we don't need to worry
      // about comparisons.
      stop.mColor = NS_RGB(0, 0, 0);
    }

    aResult.mStops.AppendElement(stop);
  }
}

// -moz-image-rect(<uri>, <top>, <right>, <bottom>, <left>)
static void SetStyleImageToImageRect(nsStyleContext* aStyleContext,
                                     const nsCSSValue& aValue,
                                     nsStyleImage& aResult)
{
  MOZ_ASSERT(aValue.GetUnit() == eCSSUnit_Function &&
             aValue.EqualsFunction(eCSSKeyword__moz_image_rect),
             "the value is not valid -moz-image-rect()");

  nsCSSValue::Array* arr = aValue.GetArrayValue();
  MOZ_ASSERT(arr && arr->Count() == 6, "invalid number of arguments");

  // <uri>
  if (arr->Item(1).GetUnit() == eCSSUnit_Image) {
    NS_SET_IMAGE_REQUEST_WITH_DOC(aResult.SetImageData,
                                  aStyleContext,
                                  arr->Item(1).GetImageValue)
  } else {
    NS_WARNING("nsCSSValue::Image::Image() failed?");
  }

  // <top>, <right>, <bottom>, <left>
  nsStyleSides cropRect;
  NS_FOR_CSS_SIDES(side) {
    nsStyleCoord coord;
    const nsCSSValue& val = arr->Item(2 + side);

#ifdef DEBUG
    bool unitOk =
#endif
      SetAbsCoord(val, coord, SETCOORD_FACTOR | SETCOORD_PERCENT);
    MOZ_ASSERT(unitOk, "Incorrect data structure created by CSS parser");
    cropRect.Set(side, coord);
  }
  aResult.SetCropRect(&cropRect);
}

static void SetStyleImage(nsStyleContext* aStyleContext,
                          const nsCSSValue& aValue,
                          nsStyleImage& aResult,
                          RuleNodeCacheConditions& aConditions)
{
  if (aValue.GetUnit() == eCSSUnit_Null) {
    return;
  }

  aResult.SetNull();

  switch (aValue.GetUnit()) {
    case eCSSUnit_Image:
      NS_SET_IMAGE_REQUEST_WITH_DOC(aResult.SetImageData,
                                    aStyleContext,
                                    aValue.GetImageValue)
      break;
    case eCSSUnit_Function:
      if (aValue.EqualsFunction(eCSSKeyword__moz_image_rect)) {
        SetStyleImageToImageRect(aStyleContext, aValue, aResult);
      } else {
        NS_NOTREACHED("-moz-image-rect() is the only expected function");
      }
      break;
    case eCSSUnit_Gradient:
    {
      nsStyleGradient* gradient = new nsStyleGradient();
      if (gradient) {
        SetGradient(aValue, aStyleContext->PresContext(), aStyleContext,
                    *gradient, aConditions);
        aResult.SetGradientData(gradient);
      }
      break;
    }
    case eCSSUnit_Element:
      aResult.SetElementId(aValue.GetStringBufferValue());
      break;
    case eCSSUnit_Initial:
    case eCSSUnit_Unset:
    case eCSSUnit_None:
      break;
    default:
      // We might have eCSSUnit_URL values for if-visited style
      // contexts, which we can safely treat like 'none'.  Otherwise
      // this is an unexpected unit.
      NS_ASSERTION(aStyleContext->IsStyleIfVisited() &&
                   aValue.GetUnit() == eCSSUnit_URL,
                   "unexpected unit; maybe nsCSSValue::Image::Image() failed?");
      break;
  }
}

// flags for SetDiscrete - align values with SETCOORD_* constants
// where possible

#define SETDSC_NORMAL                 0x01   // N
#define SETDSC_AUTO                   0x02   // A
#define SETDSC_INTEGER                0x40   // I
#define SETDSC_ENUMERATED             0x80   // E
#define SETDSC_NONE                   0x100  // O
#define SETDSC_SYSTEM_FONT            0x2000
#define SETDSC_UNSET_INHERIT          0x00400000
#define SETDSC_UNSET_INITIAL          0x00800000

// no caller cares whether aField was changed or not
template <typename FieldT,
          typename T1, typename T2, typename T3, typename T4, typename T5>
static void
SetDiscrete(const nsCSSValue& aValue, FieldT & aField,
            RuleNodeCacheConditions& aConditions, uint32_t aMask,
            FieldT aParentValue,
            T1 aInitialValue,
            T2 aAutoValue,
            T3 aNoneValue,
            T4 aNormalValue,
            T5 aSystemFontValue)
{
  switch (aValue.GetUnit()) {
  case eCSSUnit_Null:
    return;

    // every caller of SetDiscrete provides inherit and initial
    // alternatives, so we don't require them to say so in the mask
  case eCSSUnit_Inherit:
    aConditions.SetUncacheable();
    aField = aParentValue;
    return;

  case eCSSUnit_Initial:
    aField = aInitialValue;
    return;

    // every caller provides one or other of these alternatives,
    // but they have to say which
  case eCSSUnit_Enumerated:
    if (aMask & SETDSC_ENUMERATED) {
      aField = aValue.GetIntValue();
      return;
    }
    break;

  case eCSSUnit_Integer:
    if (aMask & SETDSC_INTEGER) {
      aField = aValue.GetIntValue();
      return;
    }
    break;

    // remaining possibilities in descending order of frequency of use
  case eCSSUnit_Auto:
    if (aMask & SETDSC_AUTO) {
      aField = aAutoValue;
      return;
    }
    break;

  case eCSSUnit_None:
    if (aMask & SETDSC_NONE) {
      aField = aNoneValue;
      return;
    }
    break;

  case eCSSUnit_Normal:
    if (aMask & SETDSC_NORMAL) {
      aField = aNormalValue;
      return;
    }
    break;

  case eCSSUnit_System_Font:
    if (aMask & SETDSC_SYSTEM_FONT) {
      aField = aSystemFontValue;
      return;
    }
    break;

  case eCSSUnit_Unset:
    if (aMask & SETDSC_UNSET_INHERIT) {
      aConditions.SetUncacheable();
      aField = aParentValue;
      return;
    }
    if (aMask & SETDSC_UNSET_INITIAL) {
      aField = aInitialValue;
      return;
    }
    break;

  default:
    break;
  }

  NS_NOTREACHED("SetDiscrete: inappropriate unit");
}

// flags for SetFactor
#define SETFCT_POSITIVE 0x01        // assert value is >= 0.0f
#define SETFCT_OPACITY  0x02        // clamp value to [0.0f .. 1.0f]
#define SETFCT_NONE     0x04        // allow _None (uses aInitialValue).
#define SETFCT_UNSET_INHERIT  0x00400000
#define SETFCT_UNSET_INITIAL  0x00800000

static void
SetFactor(const nsCSSValue& aValue, float& aField, RuleNodeCacheConditions& aConditions,
          float aParentValue, float aInitialValue, uint32_t aFlags = 0)
{
  switch (aValue.GetUnit()) {
  case eCSSUnit_Null:
    return;

  case eCSSUnit_Number:
    aField = aValue.GetFloatValue();
    if (aFlags & SETFCT_POSITIVE) {
      NS_ASSERTION(aField >= 0.0f, "negative value for positive-only property");
      if (aField < 0.0f)
        aField = 0.0f;
    }
    if (aFlags & SETFCT_OPACITY) {
      if (aField < 0.0f)
        aField = 0.0f;
      if (aField > 1.0f)
        aField = 1.0f;
    }
    return;

  case eCSSUnit_Inherit:
    aConditions.SetUncacheable();
    aField = aParentValue;
    return;

  case eCSSUnit_Initial:
    aField = aInitialValue;
    return;

  case eCSSUnit_None:
    if (aFlags & SETFCT_NONE) {
      aField = aInitialValue;
      return;
    }
    break;

  case eCSSUnit_Unset:
    if (aFlags & SETFCT_UNSET_INHERIT) {
      aConditions.SetUncacheable();
      aField = aParentValue;
      return;
    }
    if (aFlags & SETFCT_UNSET_INITIAL) {
      aField = aInitialValue;
      return;
    }
    break;

  default:
    break;
  }

  NS_NOTREACHED("SetFactor: inappropriate unit");
}

void*
nsRuleNode::operator new(size_t sz, nsPresContext* aPresContext) CPP_THROW_NEW
{
  // Check the recycle list first.
  return aPresContext->PresShell()->AllocateByObjectID(nsPresArena::nsRuleNode_id, sz);
}

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void
nsRuleNode::DestroyInternal(nsRuleNode ***aDestroyQueueTail)
{
  nsRuleNode *destroyQueue, **destroyQueueTail;
  if (aDestroyQueueTail) {
    destroyQueueTail = *aDestroyQueueTail;
  } else {
    destroyQueue = nullptr;
    destroyQueueTail = &destroyQueue;
  }

  if (ChildrenAreHashed()) {
    PLDHashTable *children = ChildrenHash();
    for (auto iter = children->Iter(); !iter.Done(); iter.Next()) {
      auto entry = static_cast<ChildrenHashEntry*>(iter.Get());
      *destroyQueueTail = entry->mRuleNode;
      destroyQueueTail = &entry->mRuleNode->mNextSibling;
    }
    *destroyQueueTail = nullptr; // ensure null-termination
    delete children;
  } else if (HaveChildren()) {
    *destroyQueueTail = ChildrenList();
    do {
      destroyQueueTail = &(*destroyQueueTail)->mNextSibling;
    } while (*destroyQueueTail);
  }
  mChildren.asVoid = nullptr;

  if (aDestroyQueueTail) {
    // Our caller destroys the queue.
    *aDestroyQueueTail = destroyQueueTail;
  } else {
    // We have to do destroy the queue.  When we destroy each node, it
    // will add its children to the queue.
    while (destroyQueue) {
      nsRuleNode *cur = destroyQueue;
      destroyQueue = destroyQueue->mNextSibling;
      if (!destroyQueue) {
        NS_ASSERTION(destroyQueueTail == &cur->mNextSibling, "mangled list");
        destroyQueueTail = &destroyQueue;
      }
      cur->DestroyInternal(&destroyQueueTail);
    }
  }

  // Destroy ourselves.
  this->~nsRuleNode();

  // Don't let the memory be freed, since it will be recycled
  // instead. Don't call the global operator delete.
  mPresContext->PresShell()->FreeByObjectID(nsPresArena::nsRuleNode_id, this);
}

nsRuleNode* nsRuleNode::CreateRootNode(nsPresContext* aPresContext)
{
  return new (aPresContext)
    nsRuleNode(aPresContext, nullptr, nullptr, 0xff, false);
}

nsRuleNode::nsRuleNode(nsPresContext* aContext, nsRuleNode* aParent,
                       nsIStyleRule* aRule, uint8_t aLevel,
                       bool aIsImportant)
  : mPresContext(aContext),
    mParent(aParent),
    mRule(aRule),
    mNextSibling(nullptr),
    mDependentBits((uint32_t(aLevel) << NS_RULE_NODE_LEVEL_SHIFT) |
                   (aIsImportant ? NS_RULE_NODE_IS_IMPORTANT : 0)),
    mNoneBits(aParent ? aParent->mNoneBits & NS_RULE_NODE_HAS_ANIMATION_DATA :
                        0),
    mRefCnt(0)
{
  MOZ_ASSERT(aContext);
  MOZ_ASSERT(IsRoot() == !aRule,
             "non-root rule nodes must have a rule");

  mChildren.asVoid = nullptr;
  MOZ_COUNT_CTOR(nsRuleNode);

  if (mRule) {
    mRule->AddRef();
  }

  NS_ASSERTION(IsRoot() || GetLevel() == aLevel, "not enough bits");
  NS_ASSERTION(IsRoot() || IsImportantRule() == aIsImportant, "yikes");
  /* If IsRoot(), then aContext->StyleSet() is typically null at this
     point.  In any case, we don't want to treat the root rulenode as
     unused.  */
  if (!IsRoot()) {
    mParent->AddRef();
    aContext->StyleSet()->RuleNodeUnused();
  }

  // nsStyleSet::GetContext depends on there being only one animation
  // rule.
  MOZ_ASSERT(IsRoot() || GetLevel() != nsStyleSet::eAnimationSheet ||
             mParent->IsRoot() ||
             mParent->GetLevel() != nsStyleSet::eAnimationSheet,
             "must be only one rule at animation level");
}

nsRuleNode::~nsRuleNode()
{
  MOZ_COUNT_DTOR(nsRuleNode);
  if (mStyleData.mResetData || mStyleData.mInheritedData)
    mStyleData.Destroy(mDependentBits, mPresContext);
  if (mRule) {
    mRule->Release();
  }
}

nsRuleNode*
nsRuleNode::Transition(nsIStyleRule* aRule, uint8_t aLevel,
                       bool aIsImportantRule)
{
  nsRuleNode* next = nullptr;
  nsRuleNode::Key key(aRule, aLevel, aIsImportantRule);

  if (HaveChildren() && !ChildrenAreHashed()) {
    int32_t numKids = 0;
    nsRuleNode* curr = ChildrenList();
    while (curr && curr->GetKey() != key) {
      curr = curr->mNextSibling;
      ++numKids;
    }
    if (curr)
      next = curr;
    else if (numKids >= kMaxChildrenInList)
      ConvertChildrenToHash(numKids);
  }

  if (ChildrenAreHashed()) {
    ChildrenHashEntry *entry = static_cast<ChildrenHashEntry*>
      (PL_DHashTableAdd(ChildrenHash(), &key, fallible));
    if (!entry) {
      NS_WARNING("out of memory");
      return this;
    }
    if (entry->mRuleNode)
      next = entry->mRuleNode;
    else {
      next = entry->mRuleNode = new (mPresContext)
        nsRuleNode(mPresContext, this, aRule, aLevel, aIsImportantRule);
    }
  } else if (!next) {
    // Create the new entry in our list.
    next = new (mPresContext)
      nsRuleNode(mPresContext, this, aRule, aLevel, aIsImportantRule);
    next->mNextSibling = ChildrenList();
    SetChildrenList(next);
  }

  return next;
}

nsRuleNode*
nsRuleNode::RuleTree()
{
  nsRuleNode* n = this;
  while (n->mParent) {
    n = n->mParent;
  }
  return n;
}

void nsRuleNode::SetUsedDirectly()
{
  mDependentBits |= NS_RULE_NODE_USED_DIRECTLY;

  // Maintain the invariant that any rule node that is used directly has
  // all structs that live in the rule tree cached (which
  // nsRuleNode::GetStyleData depends on for speed).
  if (mDependentBits & NS_STYLE_INHERIT_MASK) {
    for (nsStyleStructID sid = nsStyleStructID(0); sid < nsStyleStructID_Length;
         sid = nsStyleStructID(sid + 1)) {
      uint32_t bit = nsCachedStyleData::GetBitForSID(sid);
      if (mDependentBits & bit) {
        nsRuleNode *source = mParent;
        while ((source->mDependentBits & bit) && !source->IsUsedDirectly()) {
          source = source->mParent;
        }
        void *data = source->mStyleData.GetStyleData(sid);
        NS_ASSERTION(data, "unexpected null struct");
        mStyleData.SetStyleData(sid, mPresContext, data);
      }
    }
  }
}

void
nsRuleNode::ConvertChildrenToHash(int32_t aNumKids)
{
  NS_ASSERTION(!ChildrenAreHashed() && HaveChildren(),
               "must have a non-empty list of children");
  PLDHashTable *hash = new PLDHashTable(&ChildrenHashOps,
                                        sizeof(ChildrenHashEntry),
                                        aNumKids);
  for (nsRuleNode* curr = ChildrenList(); curr; curr = curr->mNextSibling) {
    // This will never fail because of the initial size we gave the table.
    ChildrenHashEntry *entry = static_cast<ChildrenHashEntry*>(
      PL_DHashTableAdd(hash, curr->mRule, fallible));
    NS_ASSERTION(!entry->mRuleNode, "duplicate entries in list");
    entry->mRuleNode = curr;
  }
  SetChildrenHash(hash);
}

inline void
nsRuleNode::PropagateNoneBit(uint32_t aBit, nsRuleNode* aHighestNode)
{
  nsRuleNode* curr = this;
  for (;;) {
    NS_ASSERTION(!(curr->mNoneBits & aBit), "propagating too far");
    curr->mNoneBits |= aBit;
    if (curr == aHighestNode)
      break;
    curr = curr->mParent;
  }
}

inline void
nsRuleNode::PropagateDependentBit(nsStyleStructID aSID, nsRuleNode* aHighestNode,
                                  void* aStruct)
{
  NS_ASSERTION(aStruct, "expected struct");

  uint32_t bit = nsCachedStyleData::GetBitForSID(aSID);
  for (nsRuleNode* curr = this; curr != aHighestNode; curr = curr->mParent) {
    if (curr->mDependentBits & bit) {
#ifdef DEBUG
      while (curr != aHighestNode) {
        NS_ASSERTION(curr->mDependentBits & bit, "bit not set");
        curr = curr->mParent;
      }
#endif
      break;
    }

    curr->mDependentBits |= bit;

    if (curr->IsUsedDirectly()) {
      curr->mStyleData.SetStyleData(aSID, mPresContext, aStruct);
    }
  }
}

/* static */ void
nsRuleNode::PropagateGrandancestorBit(nsStyleContext* aContext,
                                      nsStyleContext* aContextInheritedFrom)
{
  MOZ_ASSERT(aContext);
  MOZ_ASSERT(aContextInheritedFrom &&
             aContextInheritedFrom != aContext &&
             aContextInheritedFrom != aContext->GetParent(),
             "aContextInheritedFrom must be an ancestor of aContext's parent");

  aContext->AddStyleBit(NS_STYLE_USES_GRANDANCESTOR_STYLE);

  nsStyleContext* context = aContext->GetParent();
  if (!context) {
    return;
  }

  for (;;) {
    nsStyleContext* parent = context->GetParent();
    if (!parent) {
      MOZ_ASSERT(false, "aContextInheritedFrom must be an ancestor of "
                        "aContext's parent");
      break;
    }
    if (parent == aContextInheritedFrom) {
      break;
    }
    context->AddStyleBit(NS_STYLE_USES_GRANDANCESTOR_STYLE);
    context = parent;
  }
}

/*
 * The following "Check" functions are used for determining what type of
 * sharing can be used for the data on this rule node.  MORE HERE...
 */

/*
 * a callback function that that can revise the result of
 * CheckSpecifiedProperties before finishing; aResult is the current
 * result, and it returns the revised one.
 */
typedef nsRuleNode::RuleDetail
  (* CheckCallbackFn)(const nsRuleData* aRuleData,
                      nsRuleNode::RuleDetail aResult);

/**
 * @param aValue the value being examined
 * @param aSpecifiedCount to be incremented by one if the value is specified
 * @param aInheritedCount to be incremented by one if the value is set to inherit
 * @param aUnsetCount to be incremented by one if the value is set to unset
 */
inline void
ExamineCSSValue(const nsCSSValue& aValue,
                uint32_t& aSpecifiedCount,
                uint32_t& aInheritedCount,
                uint32_t& aUnsetCount)
{
  if (aValue.GetUnit() != eCSSUnit_Null) {
    ++aSpecifiedCount;
    if (aValue.GetUnit() == eCSSUnit_Inherit) {
      ++aInheritedCount;
    } else if (aValue.GetUnit() == eCSSUnit_Unset) {
      ++aUnsetCount;
    }
  }
}

static nsRuleNode::RuleDetail
CheckFontCallback(const nsRuleData* aRuleData,
                  nsRuleNode::RuleDetail aResult)
{
  // em, ex, percent, 'larger', and 'smaller' values on font-size depend
  // on the parent context's font-size
  // Likewise, 'lighter' and 'bolder' values of 'font-weight', and 'wider'
  // and 'narrower' values of 'font-stretch' depend on the parent.
  const nsCSSValue& size = *aRuleData->ValueForFontSize();
  const nsCSSValue& weight = *aRuleData->ValueForFontWeight();
  if ((size.IsRelativeLengthUnit() && size.GetUnit() != eCSSUnit_RootEM) ||
      size.GetUnit() == eCSSUnit_Percent ||
      (size.GetUnit() == eCSSUnit_Enumerated &&
       (size.GetIntValue() == NS_STYLE_FONT_SIZE_SMALLER ||
        size.GetIntValue() == NS_STYLE_FONT_SIZE_LARGER)) ||
      aRuleData->ValueForScriptLevel()->GetUnit() == eCSSUnit_Integer ||
      (weight.GetUnit() == eCSSUnit_Enumerated &&
       (weight.GetIntValue() == NS_STYLE_FONT_WEIGHT_BOLDER ||
        weight.GetIntValue() == NS_STYLE_FONT_WEIGHT_LIGHTER))) {
    NS_ASSERTION(aResult == nsRuleNode::eRulePartialReset ||
                 aResult == nsRuleNode::eRuleFullReset ||
                 aResult == nsRuleNode::eRulePartialMixed ||
                 aResult == nsRuleNode::eRuleFullMixed,
                 "we know we already have a reset-counted property");
    // Promote reset to mixed since we have something that depends on
    // the parent.  But never promote to inherited since that could
    // cause inheritance of the exact value.
    if (aResult == nsRuleNode::eRulePartialReset)
      aResult = nsRuleNode::eRulePartialMixed;
    else if (aResult == nsRuleNode::eRuleFullReset)
      aResult = nsRuleNode::eRuleFullMixed;
  }

  return aResult;
}

static nsRuleNode::RuleDetail
CheckColorCallback(const nsRuleData* aRuleData,
                   nsRuleNode::RuleDetail aResult)
{
  // currentColor values for color require inheritance
  const nsCSSValue* colorValue = aRuleData->ValueForColor();
  if (colorValue->GetUnit() == eCSSUnit_EnumColor &&
      colorValue->GetIntValue() == NS_COLOR_CURRENTCOLOR) {
    NS_ASSERTION(aResult == nsRuleNode::eRuleFullReset,
                 "we should already be counted as full-reset");
    aResult = nsRuleNode::eRuleFullInherited;
  }

  return aResult;
}

static nsRuleNode::RuleDetail
CheckTextCallback(const nsRuleData* aRuleData,
                  nsRuleNode::RuleDetail aResult)
{
  const nsCSSValue* textAlignValue = aRuleData->ValueForTextAlign();
  if (textAlignValue->GetUnit() == eCSSUnit_Enumerated &&
      (textAlignValue->GetIntValue() ==
        NS_STYLE_TEXT_ALIGN_MOZ_CENTER_OR_INHERIT ||
       textAlignValue->GetIntValue() == NS_STYLE_TEXT_ALIGN_MATCH_PARENT)) {
    // Promote reset to mixed since we have something that depends on
    // the parent.
    if (aResult == nsRuleNode::eRulePartialReset)
      aResult = nsRuleNode::eRulePartialMixed;
    else if (aResult == nsRuleNode::eRuleFullReset)
      aResult = nsRuleNode::eRuleFullMixed;
  }

  return aResult;
}

static nsRuleNode::RuleDetail
CheckVariablesCallback(const nsRuleData* aRuleData,
                       nsRuleNode::RuleDetail aResult)
{
  // We don't actually have any properties on nsStyleVariables, so we do
  // all of the RuleDetail calculation in here.
  if (aRuleData->mVariables) {
    return nsRuleNode::eRulePartialMixed;
  }
  return nsRuleNode::eRuleNone;
}

#define FLAG_DATA_FOR_PROPERTY(name_, id_, method_, flags_, pref_,          \
                               parsevariant_, kwtable_, stylestructoffset_, \
                               animtype_)                                   \
  flags_,

// The order here must match the enums in *CheckCounter in nsCSSProps.cpp.

static const uint32_t gFontFlags[] = {
#define CSS_PROP_FONT FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_FONT
};

static const uint32_t gDisplayFlags[] = {
#define CSS_PROP_DISPLAY FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_DISPLAY
};

static const uint32_t gVisibilityFlags[] = {
#define CSS_PROP_VISIBILITY FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_VISIBILITY
};

static const uint32_t gMarginFlags[] = {
#define CSS_PROP_MARGIN FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_MARGIN
};

static const uint32_t gBorderFlags[] = {
#define CSS_PROP_BORDER FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_BORDER
};

static const uint32_t gPaddingFlags[] = {
#define CSS_PROP_PADDING FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_PADDING
};

static const uint32_t gOutlineFlags[] = {
#define CSS_PROP_OUTLINE FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_OUTLINE
};

static const uint32_t gListFlags[] = {
#define CSS_PROP_LIST FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_LIST
};

static const uint32_t gColorFlags[] = {
#define CSS_PROP_COLOR FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_COLOR
};

static const uint32_t gBackgroundFlags[] = {
#define CSS_PROP_BACKGROUND FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_BACKGROUND
};

static const uint32_t gPositionFlags[] = {
#define CSS_PROP_POSITION FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_POSITION
};

static const uint32_t gTableFlags[] = {
#define CSS_PROP_TABLE FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_TABLE
};

static const uint32_t gTableBorderFlags[] = {
#define CSS_PROP_TABLEBORDER FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_TABLEBORDER
};

static const uint32_t gContentFlags[] = {
#define CSS_PROP_CONTENT FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_CONTENT
};

static const uint32_t gQuotesFlags[] = {
#define CSS_PROP_QUOTES FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_QUOTES
};

static const uint32_t gTextFlags[] = {
#define CSS_PROP_TEXT FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_TEXT
};

static const uint32_t gTextResetFlags[] = {
#define CSS_PROP_TEXTRESET FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_TEXTRESET
};

static const uint32_t gUserInterfaceFlags[] = {
#define CSS_PROP_USERINTERFACE FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_USERINTERFACE
};

static const uint32_t gUIResetFlags[] = {
#define CSS_PROP_UIRESET FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_UIRESET
};

static const uint32_t gXULFlags[] = {
#define CSS_PROP_XUL FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_XUL
};

static const uint32_t gSVGFlags[] = {
#define CSS_PROP_SVG FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_SVG
};

static const uint32_t gSVGResetFlags[] = {
#define CSS_PROP_SVGRESET FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_SVGRESET
};

static const uint32_t gColumnFlags[] = {
#define CSS_PROP_COLUMN FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_COLUMN
};

// There are no properties in nsStyleVariables, but we can't have a
// zero length array.
static const uint32_t gVariablesFlags[] = {
  0,
#define CSS_PROP_VARIABLES FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_VARIABLES
};
static_assert(sizeof(gVariablesFlags) == sizeof(uint32_t),
              "if nsStyleVariables has properties now you can remove the dummy "
              "gVariablesFlags entry");

#undef FLAG_DATA_FOR_PROPERTY

static const uint32_t* gFlagsByStruct[] = {

#define STYLE_STRUCT(name, checkdata_cb) \
  g##name##Flags,
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

};

static const CheckCallbackFn gCheckCallbacks[] = {

#define STYLE_STRUCT(name, checkdata_cb) \
  checkdata_cb,
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

};

#ifdef DEBUG
static bool
AreAllMathMLPropertiesUndefined(const nsRuleData* aRuleData)
{
  return
    aRuleData->ValueForScriptLevel()->GetUnit() == eCSSUnit_Null &&
    aRuleData->ValueForScriptSizeMultiplier()->GetUnit() == eCSSUnit_Null &&
    aRuleData->ValueForScriptMinSize()->GetUnit() == eCSSUnit_Null &&
    aRuleData->ValueForMathVariant()->GetUnit() == eCSSUnit_Null &&
    aRuleData->ValueForMathDisplay()->GetUnit() == eCSSUnit_Null;
}
#endif

inline nsRuleNode::RuleDetail
nsRuleNode::CheckSpecifiedProperties(const nsStyleStructID aSID,
                                     const nsRuleData* aRuleData)
{
  // Build a count of the:
  uint32_t total = 0,      // total number of props in the struct
           specified = 0,  // number that were specified for this node
           inherited = 0,  // number that were 'inherit' (and not
                           //   eCSSUnit_Inherit) for this node
           unset = 0;      // number that were 'unset'

  // See comment in nsRuleData.h above mValueOffsets.
  MOZ_ASSERT(aRuleData->mValueOffsets[aSID] == 0,
             "we assume the value offset is zero instead of adding it");
  for (nsCSSValue *values = aRuleData->mValueStorage,
              *values_end = values + nsCSSProps::PropertyCountInStruct(aSID);
       values != values_end; ++values) {
    ++total;
    ExamineCSSValue(*values, specified, inherited, unset);
  }

  if (!nsCachedStyleData::IsReset(aSID)) {
    // For inherited properties, 'unset' means the same as 'inherit'.
    inherited += unset;
    unset = 0;
  }

#if 0
  printf("CheckSpecifiedProperties: SID=%d total=%d spec=%d inh=%d.\n",
         aSID, total, specified, inherited);
#endif

  NS_ASSERTION(aSID != eStyleStruct_Font ||
               mPresContext->Document()->GetMathMLEnabled() ||
               AreAllMathMLPropertiesUndefined(aRuleData),
               "MathML style property was defined even though MathML is disabled");

  /*
   * Return the most specific information we can: prefer None or Full
   * over Partial, and Reset or Inherited over Mixed, since we can
   * optimize based on the edge cases and not the in-between cases.
   */
  nsRuleNode::RuleDetail result;
  if (inherited == total)
    result = eRuleFullInherited;
  else if (specified == total
           // MathML defines 5 properties in Font that will never be set when
           // MathML is not in use. Therefore if all but five
           // properties have been set, and MathML is not enabled, we can treat
           // this as fully specified. Code in nsMathMLElementFactory will
           // rebuild the rule tree and style data when MathML is first enabled
           // (see nsMathMLElement::BindToTree).
           || (aSID == eStyleStruct_Font && specified + 5 == total &&
               !mPresContext->Document()->GetMathMLEnabled())
          ) {
    if (inherited == 0)
      result = eRuleFullReset;
    else
      result = eRuleFullMixed;
  } else if (specified == 0)
    result = eRuleNone;
  else if (specified == inherited)
    result = eRulePartialInherited;
  else if (inherited == 0)
    result = eRulePartialReset;
  else
    result = eRulePartialMixed;

  CheckCallbackFn cb = gCheckCallbacks[aSID];
  if (cb) {
    result = (*cb)(aRuleData, result);
  }

  return result;
}

// If we need to restrict which properties apply to the style context,
// return the bit to check in nsCSSProp's flags table.  Otherwise,
// return 0.
inline uint32_t
GetPseudoRestriction(nsStyleContext *aContext)
{
  // This needs to match nsStyleSet::WalkRestrictionRule.
  uint32_t pseudoRestriction = 0;
  nsIAtom *pseudoType = aContext->GetPseudo();
  if (pseudoType) {
    if (pseudoType == nsCSSPseudoElements::firstLetter) {
      pseudoRestriction = CSS_PROPERTY_APPLIES_TO_FIRST_LETTER;
    } else if (pseudoType == nsCSSPseudoElements::firstLine) {
      pseudoRestriction = CSS_PROPERTY_APPLIES_TO_FIRST_LINE;
    } else if (pseudoType == nsCSSPseudoElements::mozPlaceholder) {
      pseudoRestriction = CSS_PROPERTY_APPLIES_TO_PLACEHOLDER;
    }
  }
  return pseudoRestriction;
}

static void
UnsetPropertiesWithoutFlags(const nsStyleStructID aSID,
                            nsRuleData* aRuleData,
                            uint32_t aFlags)
{
  NS_ASSERTION(aFlags != 0, "aFlags must be nonzero");

  const uint32_t *flagData = gFlagsByStruct[aSID];

  // See comment in nsRuleData.h above mValueOffsets.
  MOZ_ASSERT(aRuleData->mValueOffsets[aSID] == 0,
             "we assume the value offset is zero instead of adding it");
  nsCSSValue *values = aRuleData->mValueStorage;

  for (size_t i = 0, i_end = nsCSSProps::PropertyCountInStruct(aSID);
       i != i_end; ++i) {
    if ((flagData[i] & aFlags) != aFlags)
      values[i].Reset();
  }
}

/**
 * We allocate arrays of CSS values with alloca.  (These arrays are a
 * fixed size per style struct, but we don't want to waste the
 * allocation and construction/destruction costs of the big structs when
 * we're handling much smaller ones.)  Since the lifetime of an alloca
 * allocation is the life of the calling function, the caller must call
 * alloca.  However, to ensure that constructors and destructors are
 * balanced, we do the constructor and destructor calling from this RAII
 * class, AutoCSSValueArray.
 */
struct AutoCSSValueArray {
  /**
   * aStorage must be the result of alloca(aCount * sizeof(nsCSSValue))
   */
  AutoCSSValueArray(void* aStorage, size_t aCount) {
    MOZ_ASSERT(size_t(aStorage) % NS_ALIGNMENT_OF(nsCSSValue) == 0,
               "bad alignment from alloca");
    mCount = aCount;
    // Don't use placement new[], since it might store extra data
    // for the count (on Windows!).
    mArray = static_cast<nsCSSValue*>(aStorage);
    for (size_t i = 0; i < mCount; ++i) {
      new (mArray + i) nsCSSValue();
    }
  }

  ~AutoCSSValueArray() {
    for (size_t i = 0; i < mCount; ++i) {
      mArray[i].~nsCSSValue();
    }
  }

  nsCSSValue* get() { return mArray; }

private:
  nsCSSValue *mArray;
  size_t mCount;
};

/* static */ bool
nsRuleNode::ResolveVariableReferences(const nsStyleStructID aSID,
                                      nsRuleData* aRuleData,
                                      nsStyleContext* aContext)
{
  MOZ_ASSERT(aSID != eStyleStruct_Variables);
  MOZ_ASSERT(aRuleData->mSIDs & nsCachedStyleData::GetBitForSID(aSID));
  MOZ_ASSERT(aRuleData->mValueOffsets[aSID] == 0);

  nsCSSParser parser;
  bool anyTokenStreams = false;

  // Look at each property in the nsRuleData for the given style struct.
  size_t nprops = nsCSSProps::PropertyCountInStruct(aSID);
  for (nsCSSValue* value = aRuleData->mValueStorage,
                  *values_end = aRuleData->mValueStorage + nprops;
       value != values_end; value++) {
    if (value->GetUnit() != eCSSUnit_TokenStream) {
      continue;
    }

    const CSSVariableValues* variables =
      &aContext->StyleVariables()->mVariables;
    nsCSSValueTokenStream* tokenStream = value->GetTokenStreamValue();

    // Note that ParsePropertyWithVariableReferences relies on the fact
    // that the nsCSSValue in aRuleData for the property we are re-parsing
    // is still the token stream value.  When
    // ParsePropertyWithVariableReferences calls
    // nsCSSExpandedDataBlock::MapRuleInfoInto, that function will add
    // the ImageValue that is created into the token stream object's
    // mImageValues table; see the comment above mImageValues for why.

    // XXX Should pass in sheet here (see bug 952338).
    parser.ParsePropertyWithVariableReferences(
        tokenStream->mPropertyID, tokenStream->mShorthandPropertyID,
        tokenStream->mTokenStream, variables, aRuleData,
        tokenStream->mSheetURI, tokenStream->mBaseURI,
        tokenStream->mSheetPrincipal, nullptr,
        tokenStream->mLineNumber, tokenStream->mLineOffset);
    aRuleData->mConditions.SetUncacheable();
    anyTokenStreams = true;
  }

  return anyTokenStreams;
}

const void*
nsRuleNode::WalkRuleTree(const nsStyleStructID aSID,
                         nsStyleContext* aContext)
{
  // use placement new[] on the result of alloca() to allocate a
  // variable-sized stack array, including execution of constructors,
  // and use an RAII class to run the destructors too.
  size_t nprops = nsCSSProps::PropertyCountInStruct(aSID);
  void* dataStorage = alloca(nprops * sizeof(nsCSSValue));
  AutoCSSValueArray dataArray(dataStorage, nprops);

  nsRuleData ruleData(nsCachedStyleData::GetBitForSID(aSID),
                      dataArray.get(), mPresContext, aContext);
  ruleData.mValueOffsets[aSID] = 0;

  // We start at the most specific rule in the tree.
  void* startStruct = nullptr;

  nsRuleNode* ruleNode = this;
  nsRuleNode* highestNode = nullptr; // The highest node in the rule tree
                                    // that has the same properties
                                    // specified for struct |aSID| as
                                    // |this| does.
  nsRuleNode* rootNode = this; // After the loop below, this will be the
                               // highest node that we've walked without
                               // finding cached data on the rule tree.
                               // If we don't find any cached data, it
                               // will be the root.  (XXX misnamed)
  RuleDetail detail = eRuleNone;
  uint32_t bit = nsCachedStyleData::GetBitForSID(aSID);

  while (ruleNode) {
    // See if this rule node has cached the fact that the remaining
    // nodes along this path specify no data whatsoever.
    if (ruleNode->mNoneBits & bit)
      break;

    // If the dependent bit is set on a rule node for this struct, that
    // means its rule won't have any information to add, so skip it.
    // NOTE: If we exit the loop because of the !IsUsedDirectly() check,
    // then we're guaranteed to break immediately afterwards due to a
    // non-null startStruct.
    while ((ruleNode->mDependentBits & bit) && !ruleNode->IsUsedDirectly()) {
      NS_ASSERTION(ruleNode->mStyleData.GetStyleData(aSID) == nullptr,
                   "dependent bit with cached data makes no sense");
      // Climb up to the next rule in the tree (a less specific rule).
      rootNode = ruleNode;
      ruleNode = ruleNode->mParent;
      NS_ASSERTION(!(ruleNode->mNoneBits & bit), "can't have both bits set");
    }

    // Check for cached data after the inner loop above -- otherwise
    // we'll miss it.
    startStruct = ruleNode->mStyleData.GetStyleData(aSID);
    if (startStruct)
      break; // We found a rule with fully specified data.  We don't
             // need to go up the tree any further, since the remainder
             // of this branch has already been computed.

    // Ask the rule to fill in the properties that it specifies.
    nsIStyleRule *rule = ruleNode->mRule;
    if (rule) {
      ruleData.mLevel = ruleNode->GetLevel();
      ruleData.mIsImportantRule = ruleNode->IsImportantRule();
      rule->MapRuleInfoInto(&ruleData);
    }

    // Now we check to see how many properties have been specified by
    // the rules we've examined so far.
    RuleDetail oldDetail = detail;
    detail = CheckSpecifiedProperties(aSID, &ruleData);

    if (oldDetail == eRuleNone && detail != eRuleNone)
      highestNode = ruleNode;

    if (detail == eRuleFullReset ||
        detail == eRuleFullMixed ||
        detail == eRuleFullInherited)
      break; // We don't need to examine any more rules.  All properties
             // have been fully specified.

    // Climb up to the next rule in the tree (a less specific rule).
    rootNode = ruleNode;
    ruleNode = ruleNode->mParent;
  }

  bool recomputeDetail = false;

  // If we are computing a style struct other than nsStyleVariables, and
  // ruleData has any properties with variable references (nsCSSValues of
  // type eCSSUnit_TokenStream), then we need to resolve these.
  if (aSID != eStyleStruct_Variables) {
    // A property's value might have became 'inherit' after resolving
    // variable references.  (This happens when an inherited property
    // fails to parse its resolved value.)  We need to recompute
    // |detail| in case this happened.
    recomputeDetail = ResolveVariableReferences(aSID, &ruleData, aContext);
  }

  // If needed, unset the properties that don't have a flag that allows
  // them to be set for this style context.  (For example, only some
  // properties apply to :first-line and :first-letter.)
  uint32_t pseudoRestriction = GetPseudoRestriction(aContext);
  if (pseudoRestriction) {
    UnsetPropertiesWithoutFlags(aSID, &ruleData, pseudoRestriction);

    // We need to recompute |detail| based on the restrictions we just applied.
    // We can adjust |detail| arbitrarily because of the restriction
    // rule added in nsStyleSet::WalkRestrictionRule.
    recomputeDetail = true;
  }

  if (recomputeDetail) {
    detail = CheckSpecifiedProperties(aSID, &ruleData);
  }

  NS_ASSERTION(!startStruct || (detail != eRuleFullReset &&
                                detail != eRuleFullMixed &&
                                detail != eRuleFullInherited),
               "can't have start struct and be fully specified");

  bool isReset = nsCachedStyleData::IsReset(aSID);
  if (!highestNode)
    highestNode = rootNode;

  if (!ruleData.mConditions.CacheableWithoutDependencies())
    detail = eRulePartialMixed; // Treat as though some data is specified to avoid
                                // the optimizations and force data computation.

  if (detail == eRuleNone && startStruct) {
    // We specified absolutely no rule information, but a parent rule in the tree
    // specified all the rule information.  We set a bit along the branch from our
    // node in the tree to the node that specified the data that tells nodes on that
    // branch that they never need to examine their rules for this particular struct type
    // ever again.
    PropagateDependentBit(aSID, ruleNode, startStruct);
    return startStruct;
  }
  if ((!startStruct && !isReset &&
       (detail == eRuleNone || detail == eRulePartialInherited)) ||
      detail == eRuleFullInherited) {
    // We specified no non-inherited information and neither did any of
    // our parent rules.

    // We set a bit along the branch from the highest node (ruleNode)
    // down to our node (this) indicating that no non-inherited data was
    // specified.  This bit is guaranteed to be set already on the path
    // from the highest node to the root node in the case where
    // (detail == eRuleNone), which is the most common case here.
    // We must check |!isReset| because the Compute*Data functions for
    // reset structs wouldn't handle none bits correctly.
    if (highestNode != this && !isReset)
      PropagateNoneBit(bit, highestNode);

    // All information must necessarily be inherited from our parent style context.
    // In the absence of any computed data in the rule tree and with
    // no rules specified that didn't have values of 'inherit', we should check our parent.
    nsStyleContext* parentContext = aContext->GetParent();
    if (isReset) {
      /* Reset structs don't inherit from first-line. */
      /* See similar code in COMPUTE_START_RESET */
      while (parentContext &&
             parentContext->GetPseudo() == nsCSSPseudoElements::firstLine) {
        parentContext = parentContext->GetParent();
      }
      if (parentContext && parentContext != aContext->GetParent()) {
        PropagateGrandancestorBit(aContext, parentContext);
      }
    }
    if (parentContext) {
      // We have a parent, and so we should just inherit from the parent.
      // Set the inherit bits on our context.  These bits tell the style context that
      // it never has to go back to the rule tree for data.  Instead the style context tree
      // should be walked to find the data.
      const void* parentStruct = parentContext->StyleData(aSID);
      aContext->AddStyleBit(bit); // makes const_cast OK.
      aContext->SetStyle(aSID, const_cast<void*>(parentStruct));
      return parentStruct;
    }
    else
      // We are the root.  In the case of fonts, the default values just
      // come from the pres context.
      return SetDefaultOnRoot(aSID, aContext);
  }

  typedef const void* (nsRuleNode::*ComputeFunc)(void*, const nsRuleData*,
                                                 nsStyleContext*, nsRuleNode*,
                                                 RuleDetail,
                                                 const RuleNodeCacheConditions);
  static const ComputeFunc sComputeFuncs[] = {
#define STYLE_STRUCT(name, checkdata_cb) &nsRuleNode::Compute##name##Data,
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
  };

  // We need to compute the data from the information that the rules specified.
  return (this->*sComputeFuncs[aSID])(startStruct, &ruleData, aContext,
                                      highestNode, detail,
                                      ruleData.mConditions);
}

const void*
nsRuleNode::SetDefaultOnRoot(const nsStyleStructID aSID, nsStyleContext* aContext)
{
  switch (aSID) {
    case eStyleStruct_Font:
    {
      nsStyleFont* fontData = new (mPresContext) nsStyleFont(mPresContext);
      nscoord minimumFontSize = mPresContext->MinFontSize(fontData->mLanguage);

      if (minimumFontSize > 0 && !mPresContext->IsChrome()) {
        fontData->mFont.size = std::max(fontData->mSize, minimumFontSize);
      }
      else {
        fontData->mFont.size = fontData->mSize;
      }
      aContext->SetStyle(eStyleStruct_Font, fontData);
      return fontData;
    }
    case eStyleStruct_Display:
    {
      nsStyleDisplay* disp = new (mPresContext) nsStyleDisplay();
      aContext->SetStyle(eStyleStruct_Display, disp);
      return disp;
    }
    case eStyleStruct_Visibility:
    {
      nsStyleVisibility* vis = new (mPresContext) nsStyleVisibility(mPresContext);
      aContext->SetStyle(eStyleStruct_Visibility, vis);
      return vis;
    }
    case eStyleStruct_Text:
    {
      nsStyleText* text = new (mPresContext) nsStyleText();
      aContext->SetStyle(eStyleStruct_Text, text);
      return text;
    }
    case eStyleStruct_TextReset:
    {
      nsStyleTextReset* text = new (mPresContext) nsStyleTextReset();
      aContext->SetStyle(eStyleStruct_TextReset, text);
      return text;
    }
    case eStyleStruct_Color:
    {
      nsStyleColor* color = new (mPresContext) nsStyleColor(mPresContext);
      aContext->SetStyle(eStyleStruct_Color, color);
      return color;
    }
    case eStyleStruct_Background:
    {
      nsStyleBackground* bg = new (mPresContext) nsStyleBackground();
      aContext->SetStyle(eStyleStruct_Background, bg);
      return bg;
    }
    case eStyleStruct_Margin:
    {
      nsStyleMargin* margin = new (mPresContext) nsStyleMargin();
      aContext->SetStyle(eStyleStruct_Margin, margin);
      return margin;
    }
    case eStyleStruct_Border:
    {
      nsStyleBorder* border = new (mPresContext) nsStyleBorder(mPresContext);
      aContext->SetStyle(eStyleStruct_Border, border);
      return border;
    }
    case eStyleStruct_Padding:
    {
      nsStylePadding* padding = new (mPresContext) nsStylePadding();
      aContext->SetStyle(eStyleStruct_Padding, padding);
      return padding;
    }
    case eStyleStruct_Outline:
    {
      nsStyleOutline* outline = new (mPresContext) nsStyleOutline(mPresContext);
      aContext->SetStyle(eStyleStruct_Outline, outline);
      return outline;
    }
    case eStyleStruct_List:
    {
      nsStyleList* list = new (mPresContext) nsStyleList(mPresContext);
      aContext->SetStyle(eStyleStruct_List, list);
      return list;
    }
    case eStyleStruct_Position:
    {
      nsStylePosition* pos = new (mPresContext) nsStylePosition();
      aContext->SetStyle(eStyleStruct_Position, pos);
      return pos;
    }
    case eStyleStruct_Table:
    {
      nsStyleTable* table = new (mPresContext) nsStyleTable();
      aContext->SetStyle(eStyleStruct_Table, table);
      return table;
    }
    case eStyleStruct_TableBorder:
    {
      nsStyleTableBorder* table = new (mPresContext) nsStyleTableBorder();
      aContext->SetStyle(eStyleStruct_TableBorder, table);
      return table;
    }
    case eStyleStruct_Content:
    {
      nsStyleContent* content = new (mPresContext) nsStyleContent();
      aContext->SetStyle(eStyleStruct_Content, content);
      return content;
    }
    case eStyleStruct_Quotes:
    {
      nsStyleQuotes* quotes = new (mPresContext) nsStyleQuotes();
      aContext->SetStyle(eStyleStruct_Quotes, quotes);
      return quotes;
    }
    case eStyleStruct_UserInterface:
    {
      nsStyleUserInterface* ui = new (mPresContext) nsStyleUserInterface();
      aContext->SetStyle(eStyleStruct_UserInterface, ui);
      return ui;
    }
    case eStyleStruct_UIReset:
    {
      nsStyleUIReset* ui = new (mPresContext) nsStyleUIReset();
      aContext->SetStyle(eStyleStruct_UIReset, ui);
      return ui;
    }
    case eStyleStruct_XUL:
    {
      nsStyleXUL* xul = new (mPresContext) nsStyleXUL();
      aContext->SetStyle(eStyleStruct_XUL, xul);
      return xul;
    }
    case eStyleStruct_Column:
    {
      nsStyleColumn* column = new (mPresContext) nsStyleColumn(mPresContext);
      aContext->SetStyle(eStyleStruct_Column, column);
      return column;
    }
    case eStyleStruct_SVG:
    {
      nsStyleSVG* svg = new (mPresContext) nsStyleSVG();
      aContext->SetStyle(eStyleStruct_SVG, svg);
      return svg;
    }
    case eStyleStruct_SVGReset:
    {
      nsStyleSVGReset* svgReset = new (mPresContext) nsStyleSVGReset();
      aContext->SetStyle(eStyleStruct_SVGReset, svgReset);
      return svgReset;
    }
    case eStyleStruct_Variables:
    {
      nsStyleVariables* vars = new (mPresContext) nsStyleVariables();
      aContext->SetStyle(eStyleStruct_Variables, vars);
      return vars;
    }
    default:
      /*
       * unhandled case: nsStyleStructID_Length.
       * last item of nsStyleStructID, to know its length.
       */
      MOZ_ASSERT(false, "unexpected SID");
      return nullptr;
  }
  return nullptr;
}

/**
 * Begin an nsRuleNode::Compute*Data function for an inherited struct.
 *
 * @param type_ The nsStyle* type this function computes.
 * @param ctorargs_ The arguments used for the default nsStyle* constructor.
 * @param data_ Variable (declared here) holding the result of this
 *              function.
 * @param parentdata_ Variable (declared here) holding the parent style
 *                    context's data for this struct.
 */
#define COMPUTE_START_INHERITED(type_, ctorargs_, data_, parentdata_)         \
  NS_ASSERTION(aRuleDetail != eRuleFullInherited,                             \
               "should not have bothered calling Compute*Data");              \
                                                                              \
  nsStyleContext* parentContext = aContext->GetParent();                      \
                                                                              \
  nsStyle##type_* data_ = nullptr;                                            \
  mozilla::Maybe<nsStyle##type_> maybeFakeParentData;                         \
  const nsStyle##type_* parentdata_ = nullptr;                                \
  RuleNodeCacheConditions conditions = aConditions;                                      \
                                                                              \
  /* If |conditions.Cacheable()| might be true by the time we're done, we */    \
  /* can't call parentContext->Style##type_() since it could recur into */    \
  /* setting the same struct on the same rule node, causing a leak. */        \
  if (aRuleDetail != eRuleFullReset &&                                        \
      (!aStartStruct || (aRuleDetail != eRulePartialReset &&                  \
                         aRuleDetail != eRuleNone))) {                        \
    if (parentContext) {                                                      \
      parentdata_ = parentContext->Style##type_();                            \
    } else {                                                                  \
      maybeFakeParentData.emplace ctorargs_;                                  \
      parentdata_ = maybeFakeParentData.ptr();                                \
    }                                                                         \
  }                                                                           \
  if (eStyleStruct_##type_ == eStyleStruct_Variables)                         \
    /* no need to copy construct an nsStyleVariables, as we will copy */      \
    /* inherited variables (and call SetUncacheable()) in */                  \
    /* ComputeVariablesData */                                                \
    data_ = new (mPresContext) nsStyle##type_ ctorargs_;                      \
  else if (aStartStruct)                                                      \
    /* We only need to compute the delta between this computed data and */    \
    /* our computed data. */                                                  \
    data_ = new (mPresContext)                                                \
            nsStyle##type_(*static_cast<nsStyle##type_*>(aStartStruct));      \
  else {                                                                      \
    if (aRuleDetail != eRuleFullMixed && aRuleDetail != eRuleFullReset) {     \
      /* No question. We will have to inherit. Go ahead and init */           \
      /* with inherited vals from parent. */                                  \
      conditions.SetUncacheable();                                              \
      if (parentdata_)                                                        \
        data_ = new (mPresContext) nsStyle##type_(*parentdata_);              \
      else                                                                    \
        data_ = new (mPresContext) nsStyle##type_ ctorargs_;                  \
    }                                                                         \
    else                                                                      \
      data_ = new (mPresContext) nsStyle##type_ ctorargs_;                    \
  }                                                                           \
                                                                              \
  if (!parentdata_)                                                           \
    parentdata_ = data_;

/**
 * Begin an nsRuleNode::Compute*Data function for a reset struct.
 *
 * @param type_ The nsStyle* type this function computes.
 * @param ctorargs_ The arguments used for the default nsStyle* constructor.
 * @param data_ Variable (declared here) holding the result of this
 *              function.
 * @param parentdata_ Variable (declared here) holding the parent style
 *                    context's data for this struct.
 */
#define COMPUTE_START_RESET(type_, ctorargs_, data_, parentdata_)             \
  NS_ASSERTION(aRuleDetail != eRuleFullInherited,                             \
               "should not have bothered calling Compute*Data");              \
                                                                              \
  nsStyleContext* parentContext = aContext->GetParent();                      \
  /* Reset structs don't inherit from first-line */                           \
  /* See similar code in WalkRuleTree */                                      \
  while (parentContext &&                                                     \
         parentContext->GetPseudo() == nsCSSPseudoElements::firstLine) {      \
    parentContext = parentContext->GetParent();                               \
  }                                                                           \
                                                                              \
  nsStyle##type_* data_;                                                      \
  if (aStartStruct)                                                           \
    /* We only need to compute the delta between this computed data and */    \
    /* our computed data. */                                                  \
    data_ = new (mPresContext)                                                \
            nsStyle##type_(*static_cast<nsStyle##type_*>(aStartStruct));      \
  else                                                                        \
    data_ = new (mPresContext) nsStyle##type_ ctorargs_;                      \
                                                                              \
  /* If |conditions.Cacheable()| might be true by the time we're done, we */    \
  /* can't call parentContext->Style##type_() since it could recur into */    \
  /* setting the same struct on the same rule node, causing a leak. */        \
  mozilla::Maybe<nsStyle##type_> maybeFakeParentData;                         \
  const nsStyle##type_* parentdata_ = data_;                                  \
  if (aRuleDetail != eRuleFullReset &&                                        \
      aRuleDetail != eRulePartialReset &&                                     \
      aRuleDetail != eRuleNone) {                                             \
    if (parentContext) {                                                      \
      parentdata_ = parentContext->Style##type_();                            \
    } else {                                                                  \
      maybeFakeParentData.emplace ctorargs_;                                  \
      parentdata_ = maybeFakeParentData.ptr();                                \
    }                                                                         \
  }                                                                           \
  RuleNodeCacheConditions conditions = aConditions;

/**
 * End an nsRuleNode::Compute*Data function for an inherited struct.
 *
 * @param type_ The nsStyle* type this function computes.
 * @param data_ Variable holding the result of this function.
 */
#define COMPUTE_END_INHERITED(type_, data_)                                   \
  NS_POSTCONDITION(!conditions.CacheableWithoutDependencies() ||                \
                   aRuleDetail == eRuleFullReset ||                           \
                   (aStartStruct && aRuleDetail == eRulePartialReset),        \
                   "conditions.CacheableWithoutDependencies() must be false "   \
                   "for inherited structs unless all properties have been "   \
                   "specified with values other than inherit");               \
  if (conditions.CacheableWithoutDependencies()) {                              \
    /* We were fully specified and can therefore be cached right on the */    \
    /* rule node. */                                                          \
    if (!aHighestNode->mStyleData.mInheritedData) {                           \
      aHighestNode->mStyleData.mInheritedData =                               \
        new (mPresContext) nsInheritedStyleData;                              \
    }                                                                         \
    NS_ASSERTION(!aHighestNode->mStyleData.mInheritedData->                   \
                   mStyleStructs[eStyleStruct_##type_],                       \
                 "Going to leak style data");                                 \
    aHighestNode->mStyleData.mInheritedData->                                 \
      mStyleStructs[eStyleStruct_##type_] = data_;                            \
    /* Propagate the bit down. */                                             \
    PropagateDependentBit(eStyleStruct_##type_, aHighestNode, data_);         \
    /* Tell the style context that it doesn't own the data */                 \
    aContext->                                                                \
      AddStyleBit(nsCachedStyleData::GetBitForSID(eStyleStruct_##type_));     \
  }                                                                           \
  /* Always cache inherited data on the style context */                      \
  aContext->SetStyle##type_(data_);                                           \
                                                                              \
  return data_;

/**
 * End an nsRuleNode::Compute*Data function for a reset struct.
 *
 * @param type_ The nsStyle* type this function computes.
 * @param data_ Variable holding the result of this function.
 */
#define COMPUTE_END_RESET(type_, data_)                                       \
  NS_POSTCONDITION(!conditions.CacheableWithoutDependencies() ||                \
                   aRuleDetail == eRuleNone ||                                \
                   aRuleDetail == eRulePartialReset ||                        \
                   aRuleDetail == eRuleFullReset,                             \
                   "conditions.CacheableWithoutDependencies() must be false "   \
                   "for reset structs if any properties were specified as "   \
                   "inherit");                                                \
  if (conditions.CacheableWithoutDependencies()) {                              \
    /* We were fully specified and can therefore be cached right on the */    \
    /* rule node. */                                                          \
    if (!aHighestNode->mStyleData.mResetData) {                               \
      aHighestNode->mStyleData.mResetData =                                   \
        new (mPresContext) nsConditionalResetStyleData;                       \
    }                                                                         \
    NS_ASSERTION(!aHighestNode->mStyleData.mResetData->                       \
                   GetStyleData(eStyleStruct_##type_),                        \
                 "Going to leak style data");                                 \
    aHighestNode->mStyleData.mResetData->                                     \
      SetStyleData(eStyleStruct_##type_, data_);                              \
    /* Propagate the bit down. */                                             \
    PropagateDependentBit(eStyleStruct_##type_, aHighestNode, data_);         \
  } else if (conditions.Cacheable()) {                                        \
    if (!mStyleData.mResetData) {                                             \
      mStyleData.mResetData = new (mPresContext) nsConditionalResetStyleData; \
    }                                                                         \
    mStyleData.mResetData->                                                   \
      SetStyleData(eStyleStruct_##type_, mPresContext, data_, conditions);    \
    /* Tell the style context that it doesn't own the data */                 \
    aContext->                                                                \
      AddStyleBit(nsCachedStyleData::GetBitForSID(eStyleStruct_##type_));     \
    aContext->SetStyle(eStyleStruct_##type_, data_);                          \
  } else {                                                                    \
    /* We can't be cached in the rule node.  We have to be put right */       \
    /* on the style context. */                                               \
    aContext->SetStyle(eStyleStruct_##type_, data_);                          \
    if (aContext->GetParent()) {                                              \
      /* This is pessimistic; we could be uncacheable because we had a */     \
      /* relative font-weight, for example, which does not need to defeat */  \
      /* the restyle optimizations in RestyleManager.cpp that look at */      \
      /* NS_STYLE_HAS_CHILD_THAT_USES_RESET_STYLE. */                         \
      aContext->GetParent()->                                                 \
        AddStyleBit(NS_STYLE_HAS_CHILD_THAT_USES_RESET_STYLE);                \
    }                                                                         \
  }                                                                           \
                                                                              \
  return data_;

// This function figures out how much scaling should be suppressed to
// satisfy scriptminsize. This is our attempt to implement
// http://www.w3.org/TR/MathML2/chapter3.html#id.3.3.4.2.2
// This is called after mScriptLevel, mScriptMinSize and mScriptSizeMultiplier
// have been set in aFont.
//
// Here are the invariants we enforce:
// 1) A decrease in size must not reduce the size below minscriptsize.
// 2) An increase in size must not increase the size above the size we would
// have if minscriptsize had not been applied anywhere.
// 3) The scriptlevel-induced size change must between 1.0 and the parent's
// scriptsizemultiplier^(new script level - old script level), as close to the
// latter as possible subject to constraints 1 and 2.
static nscoord
ComputeScriptLevelSize(const nsStyleFont* aFont, const nsStyleFont* aParentFont,
                       nsPresContext* aPresContext, nscoord* aUnconstrainedSize)
{
  int32_t scriptLevelChange =
    aFont->mScriptLevel - aParentFont->mScriptLevel;
  if (scriptLevelChange == 0) {
    *aUnconstrainedSize = aParentFont->mScriptUnconstrainedSize;
    // Constraint #3 says that we cannot change size, and #1 and #2 are always
    // satisfied with no change. It's important this be fast because it covers
    // all non-MathML content.
    return aParentFont->mSize;
  }

  // Compute actual value of minScriptSize
  nscoord minScriptSize = aParentFont->mScriptMinSize;
  if (aFont->mAllowZoom) {
    minScriptSize = nsStyleFont::ZoomText(aPresContext, minScriptSize);
  }

  double scriptLevelScale =
    pow(aParentFont->mScriptSizeMultiplier, scriptLevelChange);
  // Compute the size we would have had if minscriptsize had never been
  // applied, also prevent overflow (bug 413274)
  *aUnconstrainedSize =
    NSToCoordRoundWithClamp(aParentFont->mScriptUnconstrainedSize*scriptLevelScale);
  // Compute the size we could get via scriptlevel change
  nscoord scriptLevelSize =
    NSToCoordRoundWithClamp(aParentFont->mSize*scriptLevelScale);
  if (scriptLevelScale <= 1.0) {
    if (aParentFont->mSize <= minScriptSize) {
      // We can't decrease the font size at all, so just stick to no change
      // (authors are allowed to explicitly set the font size smaller than
      // minscriptsize)
      return aParentFont->mSize;
    }
    // We can decrease, so apply constraint #1
    return std::max(minScriptSize, scriptLevelSize);
  } else {
    // scriptminsize can only make sizes larger than the unconstrained size
    NS_ASSERTION(*aUnconstrainedSize <= scriptLevelSize, "How can this ever happen?");
    // Apply constraint #2
    return std::min(scriptLevelSize, std::max(*aUnconstrainedSize, minScriptSize));
  }
}


/* static */ nscoord
nsRuleNode::CalcFontPointSize(int32_t aHTMLSize, int32_t aBasePointSize,
                              nsPresContext* aPresContext,
                              nsFontSizeType aFontSizeType)
{
#define sFontSizeTableMin  9 
#define sFontSizeTableMax 16 

// This table seems to be the one used by MacIE5. We hope its adoption in Mozilla
// and eventually in WinIE5.5 will help to establish a standard rendering across
// platforms and browsers. For now, it is used only in Strict mode. More can be read
// in the document written by Todd Farhner at:
// http://style.verso.com/font_size_intervals/altintervals.html
//
  static int32_t sStrictFontSizeTable[sFontSizeTableMax - sFontSizeTableMin + 1][8] =
  {
      { 9,    9,     9,     9,    11,    14,    18,    27},
      { 9,    9,     9,    10,    12,    15,    20,    30},
      { 9,    9,    10,    11,    13,    17,    22,    33},
      { 9,    9,    10,    12,    14,    18,    24,    36},
      { 9,   10,    12,    13,    16,    20,    26,    39},
      { 9,   10,    12,    14,    17,    21,    28,    42},
      { 9,   10,    13,    15,    18,    23,    30,    45},
      { 9,   10,    13,    16,    18,    24,    32,    48}
  };
// HTML       1      2      3      4      5      6      7
// CSS  xxs   xs     s      m      l     xl     xxl
//                          |
//                      user pref
//
//------------------------------------------------------------
//
// This table gives us compatibility with WinNav4 for the default fonts only.
// In WinNav4, the default fonts were:
//
//     Times/12pt ==   Times/16px at 96ppi
//   Courier/10pt == Courier/13px at 96ppi
//
// The 2 lines below marked "anchored" have the exact pixel sizes used by
// WinNav4 for Times/12pt and Courier/10pt at 96ppi. As you can see, the
// HTML size 3 (user pref) for those 2 anchored lines is 13px and 16px.
//
// All values other than the anchored values were filled in by hand, never
// going below 9px, and maintaining a "diagonal" relationship. See for
// example the 13s -- they follow a diagonal line through the table.
//
  static int32_t sQuirksFontSizeTable[sFontSizeTableMax - sFontSizeTableMin + 1][8] =
  {
      { 9,    9,     9,     9,    11,    14,    18,    28 },
      { 9,    9,     9,    10,    12,    15,    20,    31 },
      { 9,    9,     9,    11,    13,    17,    22,    34 },
      { 9,    9,    10,    12,    14,    18,    24,    37 },
      { 9,    9,    10,    13,    16,    20,    26,    40 }, // anchored (13)
      { 9,    9,    11,    14,    17,    21,    28,    42 },
      { 9,   10,    12,    15,    17,    23,    30,    45 },
      { 9,   10,    13,    16,    18,    24,    32,    48 }  // anchored (16)
  };
// HTML       1      2      3      4      5      6      7
// CSS  xxs   xs     s      m      l     xl     xxl
//                          |
//                      user pref

#if 0
//
// These are the exact pixel values used by WinIE5 at 96ppi.
//
      { ?,    8,    11,    12,    13,    16,    21,    32 }, // smallest
      { ?,    9,    12,    13,    16,    21,    27,    40 }, // smaller
      { ?,   10,    13,    16,    18,    24,    32,    48 }, // medium
      { ?,   13,    16,    19,    21,    27,    37,    ?? }, // larger
      { ?,   16,    19,    21,    24,    32,    43,    ?? }  // largest
//
// HTML       1      2      3      4      5      6      7
// CSS  ?     ?      ?      ?      ?      ?      ?      ?
//
// (CSS not tested yet.)
//
#endif

  static int32_t sFontSizeFactors[8] = { 60,75,89,100,120,150,200,300 };

  static int32_t sCSSColumns[7]  = {0, 1, 2, 3, 4, 5, 6}; // xxs...xxl
  static int32_t sHTMLColumns[7] = {1, 2, 3, 4, 5, 6, 7}; // 1...7

  double dFontSize;

  if (aFontSizeType == eFontSize_HTML) {
    aHTMLSize--;    // input as 1-7
  }

  if (aHTMLSize < 0)
    aHTMLSize = 0;
  else if (aHTMLSize > 6)
    aHTMLSize = 6;

  int32_t* column;
  switch (aFontSizeType)
  {
    case eFontSize_HTML: column = sHTMLColumns; break;
    case eFontSize_CSS:  column = sCSSColumns;  break;
  }

  // Make special call specifically for fonts (needed PrintPreview)
  int32_t fontSize = nsPresContext::AppUnitsToIntCSSPixels(aBasePointSize);

  if ((fontSize >= sFontSizeTableMin) && (fontSize <= sFontSizeTableMax))
  {
    int32_t row = fontSize - sFontSizeTableMin;

    if (aPresContext->CompatibilityMode() == eCompatibility_NavQuirks) {
      dFontSize = nsPresContext::CSSPixelsToAppUnits(sQuirksFontSizeTable[row][column[aHTMLSize]]);
    } else {
      dFontSize = nsPresContext::CSSPixelsToAppUnits(sStrictFontSizeTable[row][column[aHTMLSize]]);
    }
  }
  else
  {
    int32_t factor = sFontSizeFactors[column[aHTMLSize]];
    dFontSize = (factor * aBasePointSize) / 100;
  }


  if (1.0 < dFontSize) {
    return (nscoord)dFontSize;
  }
  return (nscoord)1;
}


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

/* static */ nscoord
nsRuleNode::FindNextSmallerFontSize(nscoord aFontSize, int32_t aBasePointSize, 
                                    nsPresContext* aPresContext,
                                    nsFontSizeType aFontSizeType)
{
  int32_t index;
  int32_t indexMin;
  int32_t indexMax;
  float relativePosition;
  nscoord smallerSize;
  nscoord indexFontSize = aFontSize; // XXX initialize to quell a spurious gcc3.2 warning
  nscoord smallestIndexFontSize;
  nscoord largestIndexFontSize;
  nscoord smallerIndexFontSize;
  nscoord largerIndexFontSize;

  nscoord onePx = nsPresContext::CSSPixelsToAppUnits(1);

  if (aFontSizeType == eFontSize_HTML) {
    indexMin = 1;
    indexMax = 7;
  } else {
    indexMin = 0;
    indexMax = 6;
  }
  
  smallestIndexFontSize = CalcFontPointSize(indexMin, aBasePointSize, aPresContext, aFontSizeType);
  largestIndexFontSize = CalcFontPointSize(indexMax, aBasePointSize, aPresContext, aFontSizeType); 
  if (aFontSize > smallestIndexFontSize) {
    if (aFontSize < NSToCoordRound(float(largestIndexFontSize) * 1.5)) { // smaller will be in HTML table
      // find largest index smaller than current
      for (index = indexMax; index >= indexMin; index--) {
        indexFontSize = CalcFontPointSize(index, aBasePointSize, aPresContext, aFontSizeType);
        if (indexFontSize < aFontSize)
          break;
      } 
      // set up points beyond table for interpolation purposes
      if (indexFontSize == smallestIndexFontSize) {
        smallerIndexFontSize = indexFontSize - onePx;
        largerIndexFontSize = CalcFontPointSize(index+1, aBasePointSize, aPresContext, aFontSizeType);
      } else if (indexFontSize == largestIndexFontSize) {
        smallerIndexFontSize = CalcFontPointSize(index-1, aBasePointSize, aPresContext, aFontSizeType);
        largerIndexFontSize = NSToCoordRound(float(largestIndexFontSize) * 1.5);
      } else {
        smallerIndexFontSize = CalcFontPointSize(index-1, aBasePointSize, aPresContext, aFontSizeType);
        largerIndexFontSize = CalcFontPointSize(index+1, aBasePointSize, aPresContext, aFontSizeType);
      }
      // compute the relative position of the parent size between the two closest indexed sizes
      relativePosition = float(aFontSize - indexFontSize) / float(largerIndexFontSize - indexFontSize);            
      // set the new size to have the same relative position between the next smallest two indexed sizes
      smallerSize = smallerIndexFontSize + NSToCoordRound(relativePosition * (indexFontSize - smallerIndexFontSize));      
    }
    else {  // larger than HTML table, drop by 33%
      smallerSize = NSToCoordRound(float(aFontSize) / 1.5);
    }
  }
  else { // smaller than HTML table, drop by 1px
    smallerSize = std::max(aFontSize - onePx, onePx);
  }
  return smallerSize;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

/* static */ nscoord
nsRuleNode::FindNextLargerFontSize(nscoord aFontSize, int32_t aBasePointSize, 
                                   nsPresContext* aPresContext,
                                   nsFontSizeType aFontSizeType)
{
  int32_t index;
  int32_t indexMin;
  int32_t indexMax;
  float relativePosition;
  nscoord adjustment;
  nscoord largerSize;
  nscoord indexFontSize = aFontSize; // XXX initialize to quell a spurious gcc3.2 warning
  nscoord smallestIndexFontSize;
  nscoord largestIndexFontSize;
  nscoord smallerIndexFontSize;
  nscoord largerIndexFontSize;

  nscoord onePx = nsPresContext::CSSPixelsToAppUnits(1);

  if (aFontSizeType == eFontSize_HTML) {
    indexMin = 1;
    indexMax = 7;
  } else {
    indexMin = 0;
    indexMax = 6;
  }
  
  smallestIndexFontSize = CalcFontPointSize(indexMin, aBasePointSize, aPresContext, aFontSizeType);
  largestIndexFontSize = CalcFontPointSize(indexMax, aBasePointSize, aPresContext, aFontSizeType); 
  if (aFontSize > (smallestIndexFontSize - onePx)) {
    if (aFontSize < largestIndexFontSize) { // larger will be in HTML table
      // find smallest index larger than current
      for (index = indexMin; index <= indexMax; index++) { 
        indexFontSize = CalcFontPointSize(index, aBasePointSize, aPresContext, aFontSizeType);
        if (indexFontSize > aFontSize)
          break;
      }
      // set up points beyond table for interpolation purposes
      if (indexFontSize == smallestIndexFontSize) {
        smallerIndexFontSize = indexFontSize - onePx;
        largerIndexFontSize = CalcFontPointSize(index+1, aBasePointSize, aPresContext, aFontSizeType);
      } else if (indexFontSize == largestIndexFontSize) {
        smallerIndexFontSize = CalcFontPointSize(index-1, aBasePointSize, aPresContext, aFontSizeType);
        largerIndexFontSize = NSCoordSaturatingMultiply(largestIndexFontSize, 1.5);
      } else {
        smallerIndexFontSize = CalcFontPointSize(index-1, aBasePointSize, aPresContext, aFontSizeType);
        largerIndexFontSize = CalcFontPointSize(index+1, aBasePointSize, aPresContext, aFontSizeType);
      }
      // compute the relative position of the parent size between the two closest indexed sizes
      relativePosition = float(aFontSize - smallerIndexFontSize) / float(indexFontSize - smallerIndexFontSize);
      // set the new size to have the same relative position between the next largest two indexed sizes
      adjustment = NSCoordSaturatingNonnegativeMultiply(largerIndexFontSize - indexFontSize, relativePosition);
      largerSize = NSCoordSaturatingAdd(indexFontSize, adjustment);
    }
    else {  // larger than HTML table, increase by 50%
      largerSize = NSCoordSaturatingMultiply(aFontSize, 1.5);
    }
  }
  else { // smaller than HTML table, increase by 1px
    largerSize = NSCoordSaturatingAdd(aFontSize, onePx);
  }
  return largerSize;
}

struct SetFontSizeCalcOps : public css::BasicCoordCalcOps,
                            public css::NumbersAlreadyNormalizedOps
{
  // The parameters beyond aValue that we need for CalcLengthWith.
  const nscoord mParentSize;
  const nsStyleFont* const mParentFont;
  nsPresContext* const mPresContext;
  const bool mAtRoot;
  RuleNodeCacheConditions& mConditions;

  SetFontSizeCalcOps(nscoord aParentSize, const nsStyleFont* aParentFont,
                     nsPresContext* aPresContext, bool aAtRoot,
                     RuleNodeCacheConditions& aConditions)
    : mParentSize(aParentSize),
      mParentFont(aParentFont),
      mPresContext(aPresContext),
      mAtRoot(aAtRoot),
      mConditions(aConditions)
  {
  }

  result_type ComputeLeafValue(const nsCSSValue& aValue)
  {
    nscoord size;
    if (aValue.IsLengthUnit()) {
      // Note that font-based length units use the parent's size
      // unadjusted for scriptlevel changes. A scriptlevel change
      // between us and the parent is simply ignored.
      size = CalcLengthWith(aValue, mParentSize,
                            mParentFont,
                            nullptr, mPresContext, mAtRoot,
                            true, mConditions);
      if (!aValue.IsRelativeLengthUnit() && mParentFont->mAllowZoom) {
        size = nsStyleFont::ZoomText(mPresContext, size);
      }
    }
    else if (eCSSUnit_Percent == aValue.GetUnit()) {
      mConditions.SetUncacheable();
      // Note that % units use the parent's size unadjusted for scriptlevel
      // changes. A scriptlevel change between us and the parent is simply
      // ignored.
      // aValue.GetPercentValue() may be negative for, e.g., calc(-50%)
      size = NSCoordSaturatingMultiply(mParentSize, aValue.GetPercentValue());
    } else {
      MOZ_ASSERT(false, "unexpected value");
      size = mParentSize;
    }

    return size;
  }
};

/* static */ void
nsRuleNode::SetFontSize(nsPresContext* aPresContext,
                        const nsRuleData* aRuleData,
                        const nsStyleFont* aFont,
                        const nsStyleFont* aParentFont,
                        nscoord* aSize,
                        const nsFont& aSystemFont,
                        nscoord aParentSize,
                        nscoord aScriptLevelAdjustedParentSize,
                        bool aUsedStartStruct,
                        bool aAtRoot,
                        RuleNodeCacheConditions& aConditions)
{
  // If false, means that *aSize has not been zoomed.  If true, means that
  // *aSize has been zoomed iff aParentFont->mAllowZoom is true.
  bool sizeIsZoomedAccordingToParent = false;

  int32_t baseSize = (int32_t) aPresContext->
    GetDefaultFont(aFont->mGenericID, aFont->mLanguage)->size;
  const nsCSSValue* sizeValue = aRuleData->ValueForFontSize();
  if (eCSSUnit_Enumerated == sizeValue->GetUnit()) {
    int32_t value = sizeValue->GetIntValue();

    if ((NS_STYLE_FONT_SIZE_XXSMALL <= value) &&
        (value <= NS_STYLE_FONT_SIZE_XXLARGE)) {
      *aSize = CalcFontPointSize(value, baseSize,
                       aPresContext, eFontSize_CSS);
    }
    else if (NS_STYLE_FONT_SIZE_XXXLARGE == value) {
      // <font size="7"> is not specified in CSS, so we don't use eFontSize_CSS.
      *aSize = CalcFontPointSize(value, baseSize, aPresContext);
    }
    else if (NS_STYLE_FONT_SIZE_LARGER  == value ||
             NS_STYLE_FONT_SIZE_SMALLER == value) {
      aConditions.SetUncacheable();

      // Un-zoom so we use the tables correctly.  We'll then rezoom due
      // to the |zoom = true| above.
      // Note that relative units here use the parent's size unadjusted
      // for scriptlevel changes. A scriptlevel change between us and the parent
      // is simply ignored.
      nscoord parentSize = aParentSize;
      if (aParentFont->mAllowZoom) {
        parentSize = nsStyleFont::UnZoomText(aPresContext, parentSize);
      }

      if (NS_STYLE_FONT_SIZE_LARGER == value) {
        *aSize = FindNextLargerFontSize(parentSize,
                         baseSize, aPresContext, eFontSize_CSS);

        NS_ASSERTION(*aSize >= parentSize,
                     "FindNextLargerFontSize failed");
      }
      else {
        *aSize = FindNextSmallerFontSize(parentSize,
                         baseSize, aPresContext, eFontSize_CSS);
        NS_ASSERTION(*aSize < parentSize ||
                     parentSize <= nsPresContext::CSSPixelsToAppUnits(1),
                     "FindNextSmallerFontSize failed");
      }
    } else {
      NS_NOTREACHED("unexpected value");
    }
  }
  else if (sizeValue->IsLengthUnit() ||
           sizeValue->GetUnit() == eCSSUnit_Percent ||
           sizeValue->IsCalcUnit()) {
    SetFontSizeCalcOps ops(aParentSize, aParentFont,
                           aPresContext, aAtRoot,
                           aConditions);
    *aSize = css::ComputeCalc(*sizeValue, ops);
    if (*aSize < 0) {
      MOZ_ASSERT(sizeValue->IsCalcUnit(),
                 "negative lengths and percents should be rejected by parser");
      *aSize = 0;
    }
    // The calc ops will always zoom its result according to the value
    // of aParentFont->mAllowZoom.
    sizeIsZoomedAccordingToParent = true;
  }
  else if (eCSSUnit_System_Font == sizeValue->GetUnit()) {
    // this becomes our cascading size
    *aSize = aSystemFont.size;
  }
  else if (eCSSUnit_Inherit == sizeValue->GetUnit() ||
           eCSSUnit_Unset == sizeValue->GetUnit()) {
    aConditions.SetUncacheable();
    // We apply scriptlevel change for this case, because the default is
    // to inherit and we don't want explicit "inherit" to differ from the
    // default.
    *aSize = aScriptLevelAdjustedParentSize;
    sizeIsZoomedAccordingToParent = true;
  }
  else if (eCSSUnit_Initial == sizeValue->GetUnit()) {
    // The initial value is 'medium', which has magical sizing based on
    // the generic font family, so do that here too.
    *aSize = baseSize;
  } else {
    NS_ASSERTION(eCSSUnit_Null == sizeValue->GetUnit(),
                 "What kind of font-size value is this?");
    // if aUsedStartStruct is true, then every single property in the
    // font struct is being set all at once. This means scriptlevel is not
    // going to have any influence on the font size; there is no need to
    // do anything here.
    if (!aUsedStartStruct && aParentSize != aScriptLevelAdjustedParentSize) {
      // There was no rule affecting the size but the size has been
      // affected by the parent's size via scriptlevel change. So we cannot
      // store the data in the rule tree.
      aConditions.SetUncacheable();
      *aSize = aScriptLevelAdjustedParentSize;
      sizeIsZoomedAccordingToParent = true;
    } else {
      return;
    }
  }

  // We want to zoom the cascaded size so that em-based measurements,
  // line-heights, etc., work.
  bool currentlyZoomed = sizeIsZoomedAccordingToParent &&
                         aParentFont->mAllowZoom;
  if (!currentlyZoomed && aFont->mAllowZoom) {
    *aSize = nsStyleFont::ZoomText(aPresContext, *aSize);
  } else if (currentlyZoomed && !aFont->mAllowZoom) {
    *aSize = nsStyleFont::UnZoomText(aPresContext, *aSize);
  }
}

static int8_t ClampTo8Bit(int32_t aValue) {
  if (aValue < -128)
    return -128;
  if (aValue > 127)
    return 127;
  return int8_t(aValue);
}

/* static */ void
nsRuleNode::SetFont(nsPresContext* aPresContext, nsStyleContext* aContext,
                    uint8_t aGenericFontID, const nsRuleData* aRuleData,
                    const nsStyleFont* aParentFont,
                    nsStyleFont* aFont, bool aUsedStartStruct,
                    RuleNodeCacheConditions& aConditions)
{
  bool atRoot = !aContext->GetParent();

  // -x-text-zoom: none, inherit, initial
  bool allowZoom;
  const nsCSSValue* textZoomValue = aRuleData->ValueForTextZoom();
  if (eCSSUnit_Null != textZoomValue->GetUnit()) {
    if (eCSSUnit_Inherit == textZoomValue->GetUnit()) {
      allowZoom = aParentFont->mAllowZoom;
    } else if (eCSSUnit_None == textZoomValue->GetUnit()) {
      allowZoom = false;
    } else {
      MOZ_ASSERT(eCSSUnit_Initial == textZoomValue->GetUnit(),
                 "unexpected unit");
      allowZoom = true;
    }
    aFont->EnableZoom(aPresContext, allowZoom);
  }

  // mLanguage must be set before before any of the CalcLengthWith calls
  // (direct calls or calls via SetFontSize) for the cases where |aParentFont|
  // is the same as |aFont|.
  //
  // -x-lang: string, inherit
  // This is not a real CSS property, it is an HTML attribute mapped to CSS.
  const nsCSSValue* langValue = aRuleData->ValueForLang();
  if (eCSSUnit_Ident == langValue->GetUnit()) {
    nsAutoString lang;
    langValue->GetStringValue(lang);

    nsContentUtils::ASCIIToLower(lang);
    aFont->mLanguage = do_GetAtom(lang);
    aFont->mExplicitLanguage = true;
  }

  const nsFont* defaultVariableFont =
    aPresContext->GetDefaultFont(kPresContext_DefaultVariableFont_ID,
                                 aFont->mLanguage);

  // -moz-system-font: enum (never inherit!)
  static_assert(
    NS_STYLE_FONT_CAPTION        == LookAndFeel::eFont_Caption &&
    NS_STYLE_FONT_ICON           == LookAndFeel::eFont_Icon &&
    NS_STYLE_FONT_MENU           == LookAndFeel::eFont_Menu &&
    NS_STYLE_FONT_MESSAGE_BOX    == LookAndFeel::eFont_MessageBox &&
    NS_STYLE_FONT_SMALL_CAPTION  == LookAndFeel::eFont_SmallCaption &&
    NS_STYLE_FONT_STATUS_BAR     == LookAndFeel::eFont_StatusBar &&
    NS_STYLE_FONT_WINDOW         == LookAndFeel::eFont_Window &&
    NS_STYLE_FONT_DOCUMENT       == LookAndFeel::eFont_Document &&
    NS_STYLE_FONT_WORKSPACE      == LookAndFeel::eFont_Workspace &&
    NS_STYLE_FONT_DESKTOP        == LookAndFeel::eFont_Desktop &&
    NS_STYLE_FONT_INFO           == LookAndFeel::eFont_Info &&
    NS_STYLE_FONT_DIALOG         == LookAndFeel::eFont_Dialog &&
    NS_STYLE_FONT_BUTTON         == LookAndFeel::eFont_Button &&
    NS_STYLE_FONT_PULL_DOWN_MENU == LookAndFeel::eFont_PullDownMenu &&
    NS_STYLE_FONT_LIST           == LookAndFeel::eFont_List &&
    NS_STYLE_FONT_FIELD          == LookAndFeel::eFont_Field,
    "LookAndFeel.h system-font constants out of sync with nsStyleConsts.h");

  // Fall back to defaultVariableFont.
  nsFont systemFont = *defaultVariableFont;
  const nsCSSValue* systemFontValue = aRuleData->ValueForSystemFont();
  if (eCSSUnit_Enumerated == systemFontValue->GetUnit()) {
    gfxFontStyle fontStyle;
    LookAndFeel::FontID fontID =
      (LookAndFeel::FontID)systemFontValue->GetIntValue();
    float devPerCSS =
      (float)nsPresContext::AppUnitsPerCSSPixel() /
      aPresContext->DeviceContext()->AppUnitsPerDevPixelAtUnitFullZoom();
    nsAutoString systemFontName;
    if (LookAndFeel::GetFont(fontID, systemFontName, fontStyle, devPerCSS)) {
      systemFontName.Trim("\"'");
      systemFont.fontlist = FontFamilyList(systemFontName, eUnquotedName);
      systemFont.fontlist.SetDefaultFontType(eFamily_none);
      systemFont.style = fontStyle.style;
      systemFont.systemFont = fontStyle.systemFont;
      systemFont.weight = fontStyle.weight;
      systemFont.stretch = fontStyle.stretch;
      systemFont.decorations = NS_FONT_DECORATION_NONE;
      systemFont.size =
        NSFloatPixelsToAppUnits(fontStyle.size,
                                aPresContext->DeviceContext()->
                                  AppUnitsPerDevPixelAtUnitFullZoom());
      //systemFont.langGroup = fontStyle.langGroup;
      systemFont.sizeAdjust = fontStyle.sizeAdjust;

#ifdef XP_WIN
      // XXXldb This platform-specific stuff should be in the
      // LookAndFeel implementation, not here.
      // XXXzw Should we even still *have* this code?  It looks to be making
      // old, probably obsolete assumptions.

      if (fontID == LookAndFeel::eFont_Field ||
          fontID == LookAndFeel::eFont_Button ||
          fontID == LookAndFeel::eFont_List) {
        // As far as I can tell the system default fonts and sizes
        // on MS-Windows for Buttons, Listboxes/Comboxes and Text Fields are
        // all pre-determined and cannot be changed by either the control panel
        // or programmatically.
        // Fields (text fields)
        // Button and Selects (listboxes/comboboxes)
        //    We use whatever font is defined by the system. Which it appears
        //    (and the assumption is) it is always a proportional font. Then we
        //    always use 2 points smaller than what the browser has defined as
        //    the default proportional font.
        // Assumption: system defined font is proportional
        systemFont.size =
          std::max(defaultVariableFont->size -
                 nsPresContext::CSSPointsToAppUnits(2), 0);
      }
#endif
    }
  }

  // font-family: font family list, enum, inherit
  const nsCSSValue* familyValue = aRuleData->ValueForFontFamily();
  NS_ASSERTION(eCSSUnit_Enumerated != familyValue->GetUnit(),
               "system fonts should not be in mFamily anymore");
  if (eCSSUnit_FontFamilyList == familyValue->GetUnit()) {
    // set the correct font if we are using DocumentFonts OR we are overriding for XUL
    // MJA: bug 31816
    bool useDocumentFonts =
      aPresContext->GetCachedBoolPref(kPresContext_UseDocumentFonts);
    if (aGenericFontID == kGenericFont_NONE ||
        (!useDocumentFonts && (aGenericFontID == kGenericFont_cursive ||
                               aGenericFontID == kGenericFont_fantasy))) {
      FontFamilyType defaultGeneric =
        defaultVariableFont->fontlist.FirstGeneric();
      MOZ_ASSERT(defaultVariableFont->fontlist.Length() == 1 &&
                 (defaultGeneric == eFamily_serif ||
                  defaultGeneric == eFamily_sans_serif));
      if (defaultGeneric != eFamily_none) {
        if (useDocumentFonts) {
          aFont->mFont.fontlist.SetDefaultFontType(defaultGeneric);
        } else {
          // Either prioritize the first generic in the list,
          // or (if there isn't one) prepend the default variable font.
          if (!aFont->mFont.fontlist.PrioritizeFirstGeneric()) {
            aFont->mFont.fontlist.PrependGeneric(defaultGeneric);
          }
        }
      }
    } else {
      aFont->mFont.fontlist.SetDefaultFontType(eFamily_none);
    }
    aFont->mFont.systemFont = false;
    // Technically this is redundant with the code below, but it's good
    // to have since we'll still want it once we get rid of
    // SetGenericFont (bug 380915).
    aFont->mGenericID = aGenericFontID;
  }
  else if (eCSSUnit_System_Font == familyValue->GetUnit()) {
    aFont->mFont.fontlist = systemFont.fontlist;
    aFont->mFont.systemFont = true;
    aFont->mGenericID = kGenericFont_NONE;
  }
  else if (eCSSUnit_Inherit == familyValue->GetUnit() ||
           eCSSUnit_Unset == familyValue->GetUnit()) {
    aConditions.SetUncacheable();
    aFont->mFont.fontlist = aParentFont->mFont.fontlist;
    aFont->mFont.systemFont = aParentFont->mFont.systemFont;
    aFont->mGenericID = aParentFont->mGenericID;
  }
  else if (eCSSUnit_Initial == familyValue->GetUnit()) {
    aFont->mFont.fontlist = defaultVariableFont->fontlist;
    aFont->mFont.systemFont = defaultVariableFont->systemFont;
    aFont->mGenericID = kGenericFont_NONE;
  }

  // When we're in the loop in SetGenericFont, we must ensure that we
  // always keep aFont->mFlags set to the correct generic.  But we have
  // to be careful not to touch it when we're called directly from
  // ComputeFontData, because we could have a start struct.
  if (aGenericFontID != kGenericFont_NONE) {
    aFont->mGenericID = aGenericFontID;
  }

  // -moz-math-variant: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForMathVariant(), aFont->mMathVariant,
              aConditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              aParentFont->mMathVariant, NS_MATHML_MATHVARIANT_NONE,
              0, 0, 0, 0);

  // -moz-math-display: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForMathDisplay(), aFont->mMathDisplay,
              aConditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              aParentFont->mMathDisplay, NS_MATHML_DISPLAYSTYLE_INLINE,
              0, 0, 0, 0);

  // font-smoothing: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForOsxFontSmoothing(),
              aFont->mFont.smoothing, aConditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              aParentFont->mFont.smoothing,
              defaultVariableFont->smoothing,
              0, 0, 0, 0);

  // font-style: enum, inherit, initial, -moz-system-font
  if (aFont->mMathVariant != NS_MATHML_MATHVARIANT_NONE) {
    // -moz-math-variant overrides font-style
    aFont->mFont.style = NS_FONT_STYLE_NORMAL;
  } else {
    SetDiscrete(*aRuleData->ValueForFontStyle(),
                aFont->mFont.style, aConditions,
                SETDSC_ENUMERATED | SETDSC_SYSTEM_FONT | SETDSC_UNSET_INHERIT,
                aParentFont->mFont.style,
                defaultVariableFont->style,
                0, 0, 0, systemFont.style);
  }

  // font-weight: int, enum, inherit, initial, -moz-system-font
  // special handling for enum
  const nsCSSValue* weightValue = aRuleData->ValueForFontWeight();
  if (aFont->mMathVariant != NS_MATHML_MATHVARIANT_NONE) {
    // -moz-math-variant overrides font-weight
    aFont->mFont.weight = NS_FONT_WEIGHT_NORMAL;
  } else if (eCSSUnit_Enumerated == weightValue->GetUnit()) {
    int32_t value = weightValue->GetIntValue();
    switch (value) {
      case NS_STYLE_FONT_WEIGHT_NORMAL:
      case NS_STYLE_FONT_WEIGHT_BOLD:
        aFont->mFont.weight = value;
        break;
      case NS_STYLE_FONT_WEIGHT_BOLDER: {
        aConditions.SetUncacheable();
        int32_t inheritedValue = aParentFont->mFont.weight;
        if (inheritedValue <= 300) {
          aFont->mFont.weight = 400;
        } else if (inheritedValue <= 500) {
          aFont->mFont.weight = 700;
        } else {
          aFont->mFont.weight = 900;
        }
        break;
      }
      case NS_STYLE_FONT_WEIGHT_LIGHTER: {
        aConditions.SetUncacheable();
        int32_t inheritedValue = aParentFont->mFont.weight;
        if (inheritedValue < 600) {
          aFont->mFont.weight = 100;
        } else if (inheritedValue < 800) {
          aFont->mFont.weight = 400;
        } else {
          aFont->mFont.weight = 700;
        }
        break;
      }
    }
  } else
    SetDiscrete(*weightValue, aFont->mFont.weight, aConditions,
                SETDSC_INTEGER | SETDSC_SYSTEM_FONT | SETDSC_UNSET_INHERIT,
                aParentFont->mFont.weight,
                defaultVariableFont->weight,
                0, 0, 0, systemFont.weight);

  // font-stretch: enum, inherit, initial, -moz-system-font
  SetDiscrete(*aRuleData->ValueForFontStretch(),
              aFont->mFont.stretch, aConditions,
              SETDSC_SYSTEM_FONT | SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              aParentFont->mFont.stretch,
              defaultVariableFont->stretch,
              0, 0, 0, systemFont.stretch);

  // Compute scriptlevel, scriptminsize and scriptsizemultiplier now so
  // they're available for font-size computation.

  // -moz-script-min-size: length
  const nsCSSValue* scriptMinSizeValue = aRuleData->ValueForScriptMinSize();
  if (scriptMinSizeValue->IsLengthUnit()) {
    // scriptminsize in font units (em, ex) has to be interpreted relative
    // to the parent font, or the size definitions are circular and we
    //
    aFont->mScriptMinSize =
      CalcLengthWith(*scriptMinSizeValue, aParentFont->mSize,
                     aParentFont,
                     nullptr, aPresContext, atRoot, true,
                     aConditions);
  }

  // -moz-script-size-multiplier: factor, inherit, initial
  SetFactor(*aRuleData->ValueForScriptSizeMultiplier(),
            aFont->mScriptSizeMultiplier,
            aConditions, aParentFont->mScriptSizeMultiplier,
            NS_MATHML_DEFAULT_SCRIPT_SIZE_MULTIPLIER,
            SETFCT_POSITIVE | SETFCT_UNSET_INHERIT);

  // -moz-script-level: integer, number, inherit
  const nsCSSValue* scriptLevelValue = aRuleData->ValueForScriptLevel();
  if (eCSSUnit_Integer == scriptLevelValue->GetUnit()) {
    // "relative"
    aConditions.SetUncacheable();
    aFont->mScriptLevel = ClampTo8Bit(aParentFont->mScriptLevel + scriptLevelValue->GetIntValue());
  }
  else if (eCSSUnit_Number == scriptLevelValue->GetUnit()) {
    // "absolute"
    aFont->mScriptLevel = ClampTo8Bit(int32_t(scriptLevelValue->GetFloatValue()));
  }
  else if (eCSSUnit_Auto == scriptLevelValue->GetUnit()) {
    // auto
    aConditions.SetUncacheable();
    aFont->mScriptLevel = ClampTo8Bit(aParentFont->mScriptLevel +
                                      (aParentFont->mMathDisplay ==
                                       NS_MATHML_DISPLAYSTYLE_INLINE ? 1 : 0));
  }
  else if (eCSSUnit_Inherit == scriptLevelValue->GetUnit() ||
           eCSSUnit_Unset == scriptLevelValue->GetUnit()) {
    aConditions.SetUncacheable();
    aFont->mScriptLevel = aParentFont->mScriptLevel;
  }
  else if (eCSSUnit_Initial == scriptLevelValue->GetUnit()) {
    aFont->mScriptLevel = 0;
  }

  // font-kerning: none, enum, inherit, initial, -moz-system-font
  SetDiscrete(*aRuleData->ValueForFontKerning(),
              aFont->mFont.kerning, aConditions,
              SETDSC_ENUMERATED | SETDSC_SYSTEM_FONT | SETDSC_UNSET_INHERIT,
              aParentFont->mFont.kerning,
              defaultVariableFont->kerning,
              0, 0, 0, systemFont.kerning);

  // font-synthesis: none, enum (bit field), inherit, initial, -moz-system-font
  SetDiscrete(*aRuleData->ValueForFontSynthesis(),
              aFont->mFont.synthesis, aConditions,
              SETDSC_NONE | SETDSC_ENUMERATED | SETDSC_SYSTEM_FONT |
                SETDSC_UNSET_INHERIT,
              aParentFont->mFont.synthesis,
              defaultVariableFont->synthesis,
              0, 0, 0, systemFont.synthesis);

  // font-variant-alternates: normal, enum (bit field) + functions, inherit,
  //                          initial, -moz-system-font
  const nsCSSValue* variantAlternatesValue =
    aRuleData->ValueForFontVariantAlternates();
  int32_t variantAlternates = 0;

  switch (variantAlternatesValue->GetUnit()) {
  case eCSSUnit_Inherit:
  case eCSSUnit_Unset:
    aFont->mFont.CopyAlternates(aParentFont->mFont);
    aConditions.SetUncacheable();
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_Normal:
    aFont->mFont.variantAlternates = 0;
    aFont->mFont.alternateValues.Clear();
    aFont->mFont.featureValueLookup = nullptr;
    break;

  case eCSSUnit_Pair:
    NS_ASSERTION(variantAlternatesValue->GetPairValue().mXValue.GetUnit() ==
                   eCSSUnit_Enumerated, "strange unit for variantAlternates");
    variantAlternates =
      variantAlternatesValue->GetPairValue().mXValue.GetIntValue();
    aFont->mFont.variantAlternates = variantAlternates;

    if (variantAlternates & NS_FONT_VARIANT_ALTERNATES_FUNCTIONAL_MASK) {
      // fetch the feature lookup object from the styleset
      aFont->mFont.featureValueLookup =
        aPresContext->StyleSet()->GetFontFeatureValuesLookup();

      NS_ASSERTION(variantAlternatesValue->GetPairValue().mYValue.GetUnit() ==
                   eCSSUnit_List, "function list not a list value");
      nsStyleUtil::ComputeFunctionalAlternates(
        variantAlternatesValue->GetPairValue().mYValue.GetListValue(),
        aFont->mFont.alternateValues);
    }
    break;

  default:
    break;
  }

  // font-variant-caps: normal, enum, inherit, initial, -moz-system-font
  SetDiscrete(*aRuleData->ValueForFontVariantCaps(),
              aFont->mFont.variantCaps, aConditions,
              SETDSC_NORMAL | SETDSC_ENUMERATED | SETDSC_SYSTEM_FONT |
                SETDSC_UNSET_INHERIT,
              aParentFont->mFont.variantCaps,
              defaultVariableFont->variantCaps,
              0, 0, 0, systemFont.variantCaps);

  // font-variant-east-asian: normal, enum (bit field), inherit, initial,
  //                          -moz-system-font
  SetDiscrete(*aRuleData->ValueForFontVariantEastAsian(),
              aFont->mFont.variantEastAsian, aConditions,
              SETDSC_NORMAL | SETDSC_ENUMERATED | SETDSC_SYSTEM_FONT |
                SETDSC_UNSET_INHERIT,
              aParentFont->mFont.variantEastAsian,
              defaultVariableFont->variantEastAsian,
              0, 0, 0, systemFont.variantEastAsian);

  // font-variant-ligatures: normal, none, enum (bit field), inherit, initial,
  //                         -moz-system-font
  SetDiscrete(*aRuleData->ValueForFontVariantLigatures(),
              aFont->mFont.variantLigatures, aConditions,
              SETDSC_NORMAL | SETDSC_NONE | SETDSC_ENUMERATED |
                SETDSC_SYSTEM_FONT | SETDSC_UNSET_INHERIT,
              aParentFont->mFont.variantLigatures,
              defaultVariableFont->variantLigatures,
              0, NS_FONT_VARIANT_LIGATURES_NONE, 0, systemFont.variantLigatures);

  // font-variant-numeric: normal, enum (bit field), inherit, initial,
  //                       -moz-system-font
  SetDiscrete(*aRuleData->ValueForFontVariantNumeric(),
              aFont->mFont.variantNumeric, aConditions,
              SETDSC_NORMAL | SETDSC_ENUMERATED | SETDSC_SYSTEM_FONT |
                SETDSC_UNSET_INHERIT,
              aParentFont->mFont.variantNumeric,
              defaultVariableFont->variantNumeric,
              0, 0, 0, systemFont.variantNumeric);

  // font-variant-position: normal, enum, inherit, initial,
  //                        -moz-system-font
  SetDiscrete(*aRuleData->ValueForFontVariantPosition(),
              aFont->mFont.variantPosition, aConditions,
              SETDSC_NORMAL | SETDSC_ENUMERATED | SETDSC_SYSTEM_FONT |
                SETDSC_UNSET_INHERIT,
              aParentFont->mFont.variantPosition,
              defaultVariableFont->variantPosition,
              0, 0, 0, systemFont.variantPosition);

  // font-feature-settings
  const nsCSSValue* featureSettingsValue =
    aRuleData->ValueForFontFeatureSettings();

  switch (featureSettingsValue->GetUnit()) {
  case eCSSUnit_Null:
    break;

  case eCSSUnit_Normal:
  case eCSSUnit_Initial:
    aFont->mFont.fontFeatureSettings.Clear();
    break;

  case eCSSUnit_Inherit:
  case eCSSUnit_Unset:
    aConditions.SetUncacheable();
    aFont->mFont.fontFeatureSettings = aParentFont->mFont.fontFeatureSettings;
    break;

  case eCSSUnit_System_Font:
    aFont->mFont.fontFeatureSettings = systemFont.fontFeatureSettings;
    break;

  case eCSSUnit_PairList:
  case eCSSUnit_PairListDep:
    ComputeFontFeatures(featureSettingsValue->GetPairListValue(),
                        aFont->mFont.fontFeatureSettings);
    break;

  default:
    MOZ_ASSERT(false, "unexpected value unit");
    break;
  }

  // font-language-override
  const nsCSSValue* languageOverrideValue =
    aRuleData->ValueForFontLanguageOverride();
  if (eCSSUnit_Inherit == languageOverrideValue->GetUnit() ||
      eCSSUnit_Unset == languageOverrideValue->GetUnit()) {
    aConditions.SetUncacheable();
    aFont->mFont.languageOverride = aParentFont->mFont.languageOverride;
  } else if (eCSSUnit_Normal == languageOverrideValue->GetUnit() ||
             eCSSUnit_Initial == languageOverrideValue->GetUnit()) {
    aFont->mFont.languageOverride.Truncate();
  } else if (eCSSUnit_System_Font == languageOverrideValue->GetUnit()) {
    aFont->mFont.languageOverride = systemFont.languageOverride;
  } else if (eCSSUnit_String == languageOverrideValue->GetUnit()) {
    languageOverrideValue->GetStringValue(aFont->mFont.languageOverride);
  }

  // font-size: enum, length, percent, inherit
  nscoord scriptLevelAdjustedParentSize = aParentFont->mSize;
  nscoord scriptLevelAdjustedUnconstrainedParentSize;
  scriptLevelAdjustedParentSize =
    ComputeScriptLevelSize(aFont, aParentFont, aPresContext,
                           &scriptLevelAdjustedUnconstrainedParentSize);
  NS_ASSERTION(!aUsedStartStruct || aFont->mScriptUnconstrainedSize == aFont->mSize,
               "If we have a start struct, we should have reset everything coming in here");
  SetFontSize(aPresContext, aRuleData, aFont, aParentFont,
              &aFont->mSize,
              systemFont, aParentFont->mSize, scriptLevelAdjustedParentSize,
              aUsedStartStruct, atRoot, aConditions);
  if (aParentFont->mSize == aParentFont->mScriptUnconstrainedSize &&
      scriptLevelAdjustedParentSize == scriptLevelAdjustedUnconstrainedParentSize) {
    // Fast path: we have not been affected by scriptminsize so we don't
    // need to call SetFontSize again to compute the
    // scriptminsize-unconstrained size. This is OK even if we have a
    // start struct, because if we have a start struct then 'font-size'
    // was specified and so scriptminsize has no effect.
    aFont->mScriptUnconstrainedSize = aFont->mSize;
  } else {
    SetFontSize(aPresContext, aRuleData, aFont, aParentFont,
                &aFont->mScriptUnconstrainedSize,
                systemFont, aParentFont->mScriptUnconstrainedSize,
                scriptLevelAdjustedUnconstrainedParentSize,
                aUsedStartStruct, atRoot, aConditions);
  }
  NS_ASSERTION(aFont->mScriptUnconstrainedSize <= aFont->mSize,
               "scriptminsize should never be making things bigger");

  nscoord fontSize = aFont->mSize;

  // enforce the user' specified minimum font-size on the value that we expose
  // (but don't change font-size:0, since that would unhide hidden text)
  if (fontSize > 0) {
    nscoord minFontSize = aPresContext->MinFontSize(aFont->mLanguage);
    if (minFontSize < 0) {
      minFontSize = 0;
    }
    if (fontSize < minFontSize && !aPresContext->IsChrome()) {
      // override the minimum font-size constraint
      fontSize = minFontSize;
    }
  }
  aFont->mFont.size = fontSize;

  // font-size-adjust: number, none, inherit, initial, -moz-system-font
  const nsCSSValue* sizeAdjustValue = aRuleData->ValueForFontSizeAdjust();
  if (eCSSUnit_System_Font == sizeAdjustValue->GetUnit()) {
    aFont->mFont.sizeAdjust = systemFont.sizeAdjust;
  } else
    SetFactor(*sizeAdjustValue, aFont->mFont.sizeAdjust,
              aConditions, aParentFont->mFont.sizeAdjust, -1.0f,
              SETFCT_NONE | SETFCT_UNSET_INHERIT);
}

/* static */ void
nsRuleNode::ComputeFontFeatures(const nsCSSValuePairList *aFeaturesList,
                                nsTArray<gfxFontFeature>& aFeatureSettings)
{
  aFeatureSettings.Clear();
  for (const nsCSSValuePairList* p = aFeaturesList; p; p = p->mNext) {
    gfxFontFeature feat = {0, 0};

    MOZ_ASSERT(aFeaturesList->mXValue.GetUnit() == eCSSUnit_String,
               "unexpected value unit");

    // tag is a 4-byte ASCII sequence
    nsAutoString tag;
    p->mXValue.GetStringValue(tag);
    if (tag.Length() != 4) {
      continue;
    }
    // parsing validates that these are ASCII chars
    // tags are always big-endian
    feat.mTag = (tag[0] << 24) | (tag[1] << 16) | (tag[2] << 8)  | tag[3];

    // value
    NS_ASSERTION(p->mYValue.GetUnit() == eCSSUnit_Integer,
                 "should have found an integer unit");
    feat.mValue = p->mYValue.GetIntValue();

    aFeatureSettings.AppendElement(feat);
  }
}

// This should die (bug 380915).
//
// SetGenericFont:
//  - backtrack to an ancestor with the same generic font name (possibly
//    up to the root where default values come from the presentation context)
//  - re-apply cascading rules from there without caching intermediate values
/* static */ void
nsRuleNode::SetGenericFont(nsPresContext* aPresContext,
                           nsStyleContext* aContext,
                           uint8_t aGenericFontID,
                           nsStyleFont* aFont)
{
  // walk up the contexts until a context with the desired generic font
  nsAutoTArray<nsStyleContext*, 8> contextPath;
  contextPath.AppendElement(aContext);
  nsStyleContext* higherContext = aContext->GetParent();
  while (higherContext) {
    if (higherContext->StyleFont()->mGenericID == aGenericFontID) {
      // done walking up the higher contexts
      break;
    }
    contextPath.AppendElement(higherContext);
    higherContext = higherContext->GetParent();
  }

  // re-apply the cascading rules, starting from the higher context

  // If we stopped earlier because we reached the root of the style tree,
  // we will start with the default generic font from the presentation
  // context. Otherwise we start with the higher context.
  const nsFont* defaultFont =
    aPresContext->GetDefaultFont(aGenericFontID, aFont->mLanguage);
  nsStyleFont parentFont(*defaultFont, aPresContext);
  if (higherContext) {
    const nsStyleFont* tmpFont = higherContext->StyleFont();
    parentFont = *tmpFont;
  }
  *aFont = parentFont;

  uint32_t fontBit = nsCachedStyleData::GetBitForSID(eStyleStruct_Font);

  // use placement new[] on the result of alloca() to allocate a
  // variable-sized stack array, including execution of constructors,
  // and use an RAII class to run the destructors too.
  size_t nprops = nsCSSProps::PropertyCountInStruct(eStyleStruct_Font);
  void* dataStorage = alloca(nprops * sizeof(nsCSSValue));

  for (int32_t i = contextPath.Length() - 1; i >= 0; --i) {
    nsStyleContext* context = contextPath[i];
    AutoCSSValueArray dataArray(dataStorage, nprops);

    nsRuleData ruleData(NS_STYLE_INHERIT_BIT(Font), dataArray.get(),
                        aPresContext, context);
    ruleData.mValueOffsets[eStyleStruct_Font] = 0;

    // Trimmed down version of ::WalkRuleTree() to re-apply the style rules
    // Note that we *do* need to do this for our own data, since what is
    // in |fontData| in ComputeFontData is only for the rules below
    // aStartStruct.
    for (nsRuleNode* ruleNode = context->RuleNode(); ruleNode;
         ruleNode = ruleNode->GetParent()) {
      if (ruleNode->mNoneBits & fontBit)
        // no more font rules on this branch, get out
        break;

      nsIStyleRule *rule = ruleNode->GetRule();
      if (rule) {
        ruleData.mLevel = ruleNode->GetLevel();
        ruleData.mIsImportantRule = ruleNode->IsImportantRule();
        rule->MapRuleInfoInto(&ruleData);
      }
    }

    // Compute the delta from the information that the rules specified

    // Avoid unnecessary operations in SetFont().  But we care if it's
    // the final value that we're computing.
    if (i != 0)
      ruleData.ValueForFontFamily()->Reset();

    ResolveVariableReferences(eStyleStruct_Font, &ruleData, aContext);

    RuleNodeCacheConditions dummy;
    nsRuleNode::SetFont(aPresContext, context,
                        aGenericFontID, &ruleData, &parentFont, aFont,
                        false, dummy);

    parentFont = *aFont;
  }

  if (higherContext && contextPath.Length() > 1) {
    // contextPath is a list of all ancestor style contexts, so it must have
    // at least two elements for it to result in a dependency on grandancestor
    // styles.
    PropagateGrandancestorBit(aContext, higherContext);
  }
}

const void*
nsRuleNode::ComputeFontData(void* aStartStruct,
                            const nsRuleData* aRuleData,
                            nsStyleContext* aContext,
                            nsRuleNode* aHighestNode,
                            const RuleDetail aRuleDetail,
                            const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_INHERITED(Font, (mPresContext), font, parentFont)

  // NOTE:  The |aRuleDetail| passed in is a little bit conservative due
  // to the -moz-system-font property.  We really don't need to consider
  // it here in determining whether to cache in the rule tree.  However,
  // we do need to consider it in WalkRuleTree when deciding whether to
  // walk further up the tree.  So this means that when the font struct
  // is fully specified using *longhand* properties (excluding
  // -moz-system-font), we won't cache in the rule tree even though we
  // could.  However, it's pretty unlikely authors will do that
  // (although there is a pretty good chance they'll fully specify it
  // using the 'font' shorthand).

  // Figure out if we are a generic font
  uint8_t generic = kGenericFont_NONE;
  // XXXldb What if we would have had a string if we hadn't been doing
  // the optimization with a non-null aStartStruct?
  const nsCSSValue* familyValue = aRuleData->ValueForFontFamily();
  if (eCSSUnit_FontFamilyList == familyValue->GetUnit()) {
    const FontFamilyList* fontlist = familyValue->GetFontFamilyListValue();
    FontFamilyList& fl = font->mFont.fontlist;
    fl = *fontlist;

    // extract the first generic in the fontlist, if exists
    FontFamilyType fontType = fontlist->FirstGeneric();

    // if only a single generic, set the generic type
    if (fontlist->Length() == 1) {
      switch (fontType) {
        case eFamily_serif:
          generic = kGenericFont_serif;
          break;
        case eFamily_sans_serif:
          generic = kGenericFont_sans_serif;
          break;
        case eFamily_monospace:
          generic = kGenericFont_monospace;
          break;
        case eFamily_cursive:
          generic = kGenericFont_cursive;
          break;
        case eFamily_fantasy:
          generic = kGenericFont_fantasy;
          break;
        case eFamily_moz_fixed:
          generic = kGenericFont_moz_fixed;
          break;
        default:
          break;
      }
    }
  }

  // Now compute our font struct
  if (generic == kGenericFont_NONE) {
    // continue the normal processing
    nsRuleNode::SetFont(mPresContext, aContext, generic,
                        aRuleData, parentFont, font,
                        aStartStruct != nullptr, conditions);
  }
  else {
    // re-calculate the font as a generic font
    conditions.SetUncacheable();
    nsRuleNode::SetGenericFont(mPresContext, aContext, generic,
                               font);
  }

  COMPUTE_END_INHERITED(Font, font)
}

template <typename T>
inline uint32_t ListLength(const T* aList)
{
  uint32_t len = 0;
  while (aList) {
    len++;
    aList = aList->mNext;
  }
  return len;
}



already_AddRefed<nsCSSShadowArray>
nsRuleNode::GetShadowData(const nsCSSValueList* aList,
                          nsStyleContext* aContext,
                          bool aIsBoxShadow,
                          RuleNodeCacheConditions& aConditions)
{
  uint32_t arrayLength = ListLength(aList);

  MOZ_ASSERT(arrayLength > 0,
             "Non-null text-shadow list, yet we counted 0 items.");
  nsRefPtr<nsCSSShadowArray> shadowList =
    new(arrayLength) nsCSSShadowArray(arrayLength);

  if (!shadowList)
    return nullptr;

  nsStyleCoord tempCoord;
  DebugOnly<bool> unitOK;
  for (nsCSSShadowItem* item = shadowList->ShadowAt(0);
       aList;
       aList = aList->mNext, ++item) {
    MOZ_ASSERT(aList->mValue.GetUnit() == eCSSUnit_Array,
               "expecting a plain array value");
    nsCSSValue::Array *arr = aList->mValue.GetArrayValue();
    // OK to pass bad aParentCoord since we're not passing SETCOORD_INHERIT
    unitOK = SetCoord(arr->Item(0), tempCoord, nsStyleCoord(),
                      SETCOORD_LENGTH | SETCOORD_CALC_LENGTH_ONLY,
                      aContext, mPresContext, aConditions);
    NS_ASSERTION(unitOK, "unexpected unit");
    item->mXOffset = tempCoord.GetCoordValue();

    unitOK = SetCoord(arr->Item(1), tempCoord, nsStyleCoord(),
                      SETCOORD_LENGTH | SETCOORD_CALC_LENGTH_ONLY,
                      aContext, mPresContext, aConditions);
    NS_ASSERTION(unitOK, "unexpected unit");
    item->mYOffset = tempCoord.GetCoordValue();

    // Blur radius is optional in the current box-shadow spec
    if (arr->Item(2).GetUnit() != eCSSUnit_Null) {
      unitOK = SetCoord(arr->Item(2), tempCoord, nsStyleCoord(),
                        SETCOORD_LENGTH | SETCOORD_CALC_LENGTH_ONLY |
                          SETCOORD_CALC_CLAMP_NONNEGATIVE,
                        aContext, mPresContext, aConditions);
      NS_ASSERTION(unitOK, "unexpected unit");
      item->mRadius = tempCoord.GetCoordValue();
    } else {
      item->mRadius = 0;
    }

    // Find the spread radius
    if (aIsBoxShadow && arr->Item(3).GetUnit() != eCSSUnit_Null) {
      unitOK = SetCoord(arr->Item(3), tempCoord, nsStyleCoord(),
                        SETCOORD_LENGTH | SETCOORD_CALC_LENGTH_ONLY,
                        aContext, mPresContext, aConditions);
      NS_ASSERTION(unitOK, "unexpected unit");
      item->mSpread = tempCoord.GetCoordValue();
    } else {
      item->mSpread = 0;
    }

    if (arr->Item(4).GetUnit() != eCSSUnit_Null) {
      item->mHasColor = true;
      // 2nd argument can be bogus since inherit is not a valid color
      unitOK = SetColor(arr->Item(4), 0, mPresContext, aContext, item->mColor,
                        aConditions);
      NS_ASSERTION(unitOK, "unexpected unit");
    }

    if (aIsBoxShadow && arr->Item(5).GetUnit() == eCSSUnit_Enumerated) {
      NS_ASSERTION(arr->Item(5).GetIntValue() == NS_STYLE_BOX_SHADOW_INSET,
                   "invalid keyword type for box shadow");
      item->mInset = true;
    } else {
      item->mInset = false;
    }
  }

  return shadowList.forget();
}

const void*
nsRuleNode::ComputeTextData(void* aStartStruct,
                            const nsRuleData* aRuleData,
                            nsStyleContext* aContext,
                            nsRuleNode* aHighestNode,
                            const RuleDetail aRuleDetail,
                            const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_INHERITED(Text, (), text, parentText)

  // tab-size: integer, inherit
  SetDiscrete(*aRuleData->ValueForTabSize(),
              text->mTabSize, conditions,
              SETDSC_INTEGER | SETDSC_UNSET_INHERIT, parentText->mTabSize,
              NS_STYLE_TABSIZE_INITIAL, 0, 0, 0, 0);

  // letter-spacing: normal, length, inherit
  SetCoord(*aRuleData->ValueForLetterSpacing(),
           text->mLetterSpacing, parentText->mLetterSpacing,
           SETCOORD_LH | SETCOORD_NORMAL | SETCOORD_INITIAL_NORMAL |
             SETCOORD_CALC_LENGTH_ONLY | SETCOORD_UNSET_INHERIT,
           aContext, mPresContext, conditions);

  // text-shadow: none, list, inherit, initial
  const nsCSSValue* textShadowValue = aRuleData->ValueForTextShadow();
  if (textShadowValue->GetUnit() != eCSSUnit_Null) {
    text->mTextShadow = nullptr;

    // Don't need to handle none/initial explicitly: The above assignment
    // takes care of that
    if (textShadowValue->GetUnit() == eCSSUnit_Inherit ||
        textShadowValue->GetUnit() == eCSSUnit_Unset) {
      conditions.SetUncacheable();
      text->mTextShadow = parentText->mTextShadow;
    } else if (textShadowValue->GetUnit() == eCSSUnit_List ||
               textShadowValue->GetUnit() == eCSSUnit_ListDep) {
      // List of arrays
      text->mTextShadow = GetShadowData(textShadowValue->GetListValue(),
                                        aContext, false, conditions);
    }
  }

  // line-height: normal, number, length, percent, inherit
  const nsCSSValue* lineHeightValue = aRuleData->ValueForLineHeight();
  if (eCSSUnit_Percent == lineHeightValue->GetUnit()) {
    conditions.SetUncacheable();
    // Use |mFont.size| to pick up minimum font size.
    text->mLineHeight.SetCoordValue(
        NSToCoordRound(float(aContext->StyleFont()->mFont.size) *
                       lineHeightValue->GetPercentValue()));
  }
  else if (eCSSUnit_Initial == lineHeightValue->GetUnit() ||
           eCSSUnit_System_Font == lineHeightValue->GetUnit()) {
    text->mLineHeight.SetNormalValue();
  }
  else {
    SetCoord(*lineHeightValue, text->mLineHeight, parentText->mLineHeight,
             SETCOORD_LEH | SETCOORD_FACTOR | SETCOORD_NORMAL |
               SETCOORD_UNSET_INHERIT,
             aContext, mPresContext, conditions);
    if (lineHeightValue->IsLengthUnit() &&
        !lineHeightValue->IsRelativeLengthUnit()) {
      nscoord lh = nsStyleFont::ZoomText(mPresContext,
                                         text->mLineHeight.GetCoordValue());

      conditions.SetUncacheable();
      const nsStyleFont *font = aContext->StyleFont();
      nscoord minimumFontSize = mPresContext->MinFontSize(font->mLanguage);

      if (minimumFontSize > 0 && !mPresContext->IsChrome()) {
        if (font->mSize != 0) {
          lh = nscoord(float(lh) * float(font->mFont.size) / float(font->mSize));
        } else {
          lh = minimumFontSize;
        }
      }
      text->mLineHeight.SetCoordValue(lh);
    }
  }


  // text-align: enum, string, pair(enum|string), inherit, initial
  // NOTE: string is not implemented yet.
  const nsCSSValue* textAlignValue = aRuleData->ValueForTextAlign();
  text->mTextAlignTrue = false;
  if (eCSSUnit_String == textAlignValue->GetUnit()) {
    NS_NOTYETIMPLEMENTED("align string");
  } else if (eCSSUnit_Enumerated == textAlignValue->GetUnit() &&
             NS_STYLE_TEXT_ALIGN_MOZ_CENTER_OR_INHERIT ==
               textAlignValue->GetIntValue()) {
    conditions.SetUncacheable();
    uint8_t parentAlign = parentText->mTextAlign;
    text->mTextAlign = (NS_STYLE_TEXT_ALIGN_DEFAULT == parentAlign) ?
      NS_STYLE_TEXT_ALIGN_CENTER : parentAlign;
  } else if (eCSSUnit_Enumerated == textAlignValue->GetUnit() &&
             NS_STYLE_TEXT_ALIGN_MATCH_PARENT ==
               textAlignValue->GetIntValue()) {
    conditions.SetUncacheable();
    nsStyleContext* parent = aContext->GetParent();
    if (parent) {
      uint8_t parentAlign = parentText->mTextAlign;
      uint8_t parentDirection = parent->StyleVisibility()->mDirection;
      switch (parentAlign) {
        case NS_STYLE_TEXT_ALIGN_DEFAULT:
          text->mTextAlign = parentDirection == NS_STYLE_DIRECTION_RTL ?
            NS_STYLE_TEXT_ALIGN_RIGHT : NS_STYLE_TEXT_ALIGN_LEFT;
          break;

        case NS_STYLE_TEXT_ALIGN_END:
          text->mTextAlign = parentDirection == NS_STYLE_DIRECTION_RTL ?
            NS_STYLE_TEXT_ALIGN_LEFT : NS_STYLE_TEXT_ALIGN_RIGHT;
          break;

        default:
          text->mTextAlign = parentAlign;
      }
    }
  } else {
    if (eCSSUnit_Pair == textAlignValue->GetUnit()) {
      // Two values were specified, one must be 'true'.
      text->mTextAlignTrue = true;
      const nsCSSValuePair& textAlignValuePair = textAlignValue->GetPairValue();
      textAlignValue = &textAlignValuePair.mXValue;
      if (eCSSUnit_Enumerated == textAlignValue->GetUnit()) {
        if (textAlignValue->GetIntValue() == NS_STYLE_TEXT_ALIGN_TRUE) {
          textAlignValue = &textAlignValuePair.mYValue;
        }
      } else if (eCSSUnit_String == textAlignValue->GetUnit()) {
        NS_NOTYETIMPLEMENTED("align string");
      }
    } else if (eCSSUnit_Inherit == textAlignValue->GetUnit() ||
               eCSSUnit_Unset == textAlignValue->GetUnit()) {
      text->mTextAlignTrue = parentText->mTextAlignTrue;
    }
    SetDiscrete(*textAlignValue, text->mTextAlign, conditions,
                SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
                parentText->mTextAlign,
                NS_STYLE_TEXT_ALIGN_DEFAULT, 0, 0, 0, 0);
  }

  // text-align-last: enum, pair(enum), inherit, initial
  const nsCSSValue* textAlignLastValue = aRuleData->ValueForTextAlignLast();
  text->mTextAlignLastTrue = false;
  if (eCSSUnit_Pair == textAlignLastValue->GetUnit()) {
    // Two values were specified, one must be 'true'.
    text->mTextAlignLastTrue = true;
    const nsCSSValuePair& textAlignLastValuePair = textAlignLastValue->GetPairValue();
    textAlignLastValue = &textAlignLastValuePair.mXValue;
    if (eCSSUnit_Enumerated == textAlignLastValue->GetUnit()) {
      if (textAlignLastValue->GetIntValue() == NS_STYLE_TEXT_ALIGN_TRUE) {
        textAlignLastValue = &textAlignLastValuePair.mYValue;
      }
    }
  } else if (eCSSUnit_Inherit == textAlignLastValue->GetUnit() ||
             eCSSUnit_Unset == textAlignLastValue->GetUnit()) {
    text->mTextAlignLastTrue = parentText->mTextAlignLastTrue;
  }
  SetDiscrete(*textAlignLastValue, text->mTextAlignLast,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentText->mTextAlignLast,
              NS_STYLE_TEXT_ALIGN_AUTO, 0, 0, 0, 0);

  // text-indent: length, percent, calc, inherit, initial
  SetCoord(*aRuleData->ValueForTextIndent(), text->mTextIndent, parentText->mTextIndent,
           SETCOORD_LPH | SETCOORD_INITIAL_ZERO | SETCOORD_STORE_CALC |
             SETCOORD_UNSET_INHERIT,
           aContext, mPresContext, conditions);

  // text-transform: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForTextTransform(), text->mTextTransform, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentText->mTextTransform,
              NS_STYLE_TEXT_TRANSFORM_NONE, 0, 0, 0, 0);

  // white-space: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForWhiteSpace(), text->mWhiteSpace, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentText->mWhiteSpace,
              NS_STYLE_WHITESPACE_NORMAL, 0, 0, 0, 0);

  // word-break: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForWordBreak(), text->mWordBreak, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentText->mWordBreak,
              NS_STYLE_WORDBREAK_NORMAL, 0, 0, 0, 0);

  // word-spacing: normal, length, inherit
  nsStyleCoord tempCoord;
  const nsCSSValue* wordSpacingValue = aRuleData->ValueForWordSpacing();
  if (SetCoord(*wordSpacingValue, tempCoord,
               nsStyleCoord(parentText->mWordSpacing,
                            nsStyleCoord::CoordConstructor),
               SETCOORD_LH | SETCOORD_NORMAL | SETCOORD_INITIAL_NORMAL |
                 SETCOORD_CALC_LENGTH_ONLY | SETCOORD_UNSET_INHERIT,
               aContext, mPresContext, conditions)) {
    if (tempCoord.GetUnit() == eStyleUnit_Coord) {
      text->mWordSpacing = tempCoord.GetCoordValue();
    } else if (tempCoord.GetUnit() == eStyleUnit_Normal) {
      text->mWordSpacing = 0;
    } else {
      NS_NOTREACHED("unexpected unit");
    }
  } else {
    NS_ASSERTION(wordSpacingValue->GetUnit() == eCSSUnit_Null,
                 "unexpected unit");
  }

  // word-wrap: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForWordWrap(), text->mWordWrap, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentText->mWordWrap,
              NS_STYLE_WORDWRAP_NORMAL, 0, 0, 0, 0);

  // hyphens: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForHyphens(), text->mHyphens, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentText->mHyphens,
              NS_STYLE_HYPHENS_MANUAL, 0, 0, 0, 0);

  // ruby-align: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForRubyAlign(),
              text->mRubyAlign, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentText->mRubyAlign,
              NS_STYLE_RUBY_ALIGN_SPACE_AROUND, 0, 0, 0, 0);

  // ruby-position: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForRubyPosition(),
              text->mRubyPosition, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentText->mRubyPosition,
              NS_STYLE_RUBY_POSITION_OVER, 0, 0, 0, 0);

  // text-size-adjust: none, auto, inherit, initial
  SetDiscrete(*aRuleData->ValueForTextSizeAdjust(), text->mTextSizeAdjust,
              conditions,
              SETDSC_NONE | SETDSC_AUTO | SETDSC_UNSET_INHERIT,
              parentText->mTextSizeAdjust,
              NS_STYLE_TEXT_SIZE_ADJUST_AUTO, // initial value
              NS_STYLE_TEXT_SIZE_ADJUST_AUTO, // auto value
              NS_STYLE_TEXT_SIZE_ADJUST_NONE, // none value
              0, 0);

  // text-combine-upright: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForTextCombineUpright(),
              text->mTextCombineUpright,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentText->mTextCombineUpright,
              NS_STYLE_TEXT_COMBINE_UPRIGHT_NONE, 0, 0, 0, 0);

  // -moz-control-character-visibility: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForControlCharacterVisibility(),
              text->mControlCharacterVisibility,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentText->mControlCharacterVisibility,
              NS_STYLE_CONTROL_CHARACTER_VISIBILITY_VISIBLE, 0, 0, 0, 0);

  COMPUTE_END_INHERITED(Text, text)
}

const void*
nsRuleNode::ComputeTextResetData(void* aStartStruct,
                                 const nsRuleData* aRuleData,
                                 nsStyleContext* aContext,
                                 nsRuleNode* aHighestNode,
                                 const RuleDetail aRuleDetail,
                                 const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(TextReset, (), text, parentText)

  // vertical-align: enum, length, percent, calc, inherit
  const nsCSSValue* verticalAlignValue = aRuleData->ValueForVerticalAlign();
  if (!SetCoord(*verticalAlignValue, text->mVerticalAlign,
                parentText->mVerticalAlign,
                SETCOORD_LPH | SETCOORD_ENUMERATED | SETCOORD_STORE_CALC,
                aContext, mPresContext, conditions)) {
    if (eCSSUnit_Initial == verticalAlignValue->GetUnit() ||
        eCSSUnit_Unset == verticalAlignValue->GetUnit()) {
      text->mVerticalAlign.SetIntValue(NS_STYLE_VERTICAL_ALIGN_BASELINE,
                                       eStyleUnit_Enumerated);
    }
  }

  // text-decoration-line: enum (bit field), inherit, initial
  const nsCSSValue* decorationLineValue =
    aRuleData->ValueForTextDecorationLine();
  if (eCSSUnit_Enumerated == decorationLineValue->GetUnit()) {
    int32_t td = decorationLineValue->GetIntValue();
    text->mTextDecorationLine = td;
    if (td & NS_STYLE_TEXT_DECORATION_LINE_PREF_ANCHORS) {
      bool underlineLinks =
        mPresContext->GetCachedBoolPref(kPresContext_UnderlineLinks);
      if (underlineLinks) {
        text->mTextDecorationLine |= NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE;
      }
      else {
        text->mTextDecorationLine &= ~NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE;
      }
    }
  } else if (eCSSUnit_Inherit == decorationLineValue->GetUnit()) {
    conditions.SetUncacheable();
    text->mTextDecorationLine = parentText->mTextDecorationLine;
  } else if (eCSSUnit_Initial == decorationLineValue->GetUnit() ||
             eCSSUnit_Unset == decorationLineValue->GetUnit()) {
    text->mTextDecorationLine = NS_STYLE_TEXT_DECORATION_LINE_NONE;
  }

  // text-decoration-color: color, string, enum, inherit, initial
  const nsCSSValue* decorationColorValue =
    aRuleData->ValueForTextDecorationColor();
  nscolor decorationColor;
  if (eCSSUnit_Inherit == decorationColorValue->GetUnit()) {
    conditions.SetUncacheable();
    if (parentContext) {
      bool isForeground;
      parentText->GetDecorationColor(decorationColor, isForeground);
      if (isForeground) {
        text->SetDecorationColor(parentContext->StyleColor()->mColor);
      } else {
        text->SetDecorationColor(decorationColor);
      }
    } else {
      text->SetDecorationColorToForeground();
    }
  }
  else if (eCSSUnit_EnumColor == decorationColorValue->GetUnit() &&
           decorationColorValue->GetIntValue() == NS_COLOR_CURRENTCOLOR) {
    text->SetDecorationColorToForeground();
  }
  else if (SetColor(*decorationColorValue, 0, mPresContext, aContext,
                    decorationColor, conditions)) {
    text->SetDecorationColor(decorationColor);
  }
  else if (eCSSUnit_Initial == decorationColorValue->GetUnit() ||
           eCSSUnit_Unset == decorationColorValue->GetUnit() ||
           eCSSUnit_Enumerated == decorationColorValue->GetUnit()) {
    MOZ_ASSERT(eCSSUnit_Enumerated != decorationColorValue->GetUnit() ||
               decorationColorValue->GetIntValue() ==
                 NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR,
               "unexpected enumerated value");
    text->SetDecorationColorToForeground();
  }

  // text-decoration-style: enum, inherit, initial
  const nsCSSValue* decorationStyleValue =
    aRuleData->ValueForTextDecorationStyle();
  if (eCSSUnit_Enumerated == decorationStyleValue->GetUnit()) {
    text->SetDecorationStyle(decorationStyleValue->GetIntValue());
  } else if (eCSSUnit_Inherit == decorationStyleValue->GetUnit()) {
    text->SetDecorationStyle(parentText->GetDecorationStyle());
    conditions.SetUncacheable();
  } else if (eCSSUnit_Initial == decorationStyleValue->GetUnit() ||
             eCSSUnit_Unset == decorationStyleValue->GetUnit()) {
    text->SetDecorationStyle(NS_STYLE_TEXT_DECORATION_STYLE_SOLID);
  }

  // text-overflow: enum, string, pair(enum|string), inherit, initial
  const nsCSSValue* textOverflowValue =
    aRuleData->ValueForTextOverflow();
  if (eCSSUnit_Initial == textOverflowValue->GetUnit() ||
      eCSSUnit_Unset == textOverflowValue->GetUnit()) {
    text->mTextOverflow = nsStyleTextOverflow();
  } else if (eCSSUnit_Inherit == textOverflowValue->GetUnit()) {
    conditions.SetUncacheable();
    text->mTextOverflow = parentText->mTextOverflow;
  } else if (eCSSUnit_Enumerated == textOverflowValue->GetUnit()) {
    // A single enumerated value.
    SetDiscrete(*textOverflowValue, text->mTextOverflow.mRight.mType,
                conditions,
                SETDSC_ENUMERATED, parentText->mTextOverflow.mRight.mType,
                NS_STYLE_TEXT_OVERFLOW_CLIP, 0, 0, 0, 0);
    text->mTextOverflow.mRight.mString.Truncate();
    text->mTextOverflow.mLeft.mType = NS_STYLE_TEXT_OVERFLOW_CLIP;
    text->mTextOverflow.mLeft.mString.Truncate();
    text->mTextOverflow.mLogicalDirections = true;
  } else if (eCSSUnit_String == textOverflowValue->GetUnit()) {
    // A single string value.
    text->mTextOverflow.mRight.mType = NS_STYLE_TEXT_OVERFLOW_STRING;
    textOverflowValue->GetStringValue(text->mTextOverflow.mRight.mString);
    text->mTextOverflow.mLeft.mType = NS_STYLE_TEXT_OVERFLOW_CLIP;
    text->mTextOverflow.mLeft.mString.Truncate();
    text->mTextOverflow.mLogicalDirections = true;
  } else if (eCSSUnit_Pair == textOverflowValue->GetUnit()) {
    // Two values were specified.
    text->mTextOverflow.mLogicalDirections = false;
    const nsCSSValuePair& textOverflowValuePair =
      textOverflowValue->GetPairValue();

    const nsCSSValue *textOverflowLeftValue = &textOverflowValuePair.mXValue;
    if (eCSSUnit_Enumerated == textOverflowLeftValue->GetUnit()) {
      SetDiscrete(*textOverflowLeftValue, text->mTextOverflow.mLeft.mType,
                  conditions,
                  SETDSC_ENUMERATED, parentText->mTextOverflow.mLeft.mType,
                  NS_STYLE_TEXT_OVERFLOW_CLIP, 0, 0, 0, 0);
      text->mTextOverflow.mLeft.mString.Truncate();
    } else if (eCSSUnit_String == textOverflowLeftValue->GetUnit()) {
      textOverflowLeftValue->GetStringValue(text->mTextOverflow.mLeft.mString);
      text->mTextOverflow.mLeft.mType = NS_STYLE_TEXT_OVERFLOW_STRING;
    }

    const nsCSSValue *textOverflowRightValue = &textOverflowValuePair.mYValue;
    if (eCSSUnit_Enumerated == textOverflowRightValue->GetUnit()) {
      SetDiscrete(*textOverflowRightValue, text->mTextOverflow.mRight.mType,
                  conditions,
                  SETDSC_ENUMERATED, parentText->mTextOverflow.mRight.mType,
                  NS_STYLE_TEXT_OVERFLOW_CLIP, 0, 0, 0, 0);
      text->mTextOverflow.mRight.mString.Truncate();
    } else if (eCSSUnit_String == textOverflowRightValue->GetUnit()) {
      textOverflowRightValue->GetStringValue(text->mTextOverflow.mRight.mString);
      text->mTextOverflow.mRight.mType = NS_STYLE_TEXT_OVERFLOW_STRING;
    }
  }

  // unicode-bidi: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForUnicodeBidi(), text->mUnicodeBidi, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentText->mUnicodeBidi,
              NS_STYLE_UNICODE_BIDI_NORMAL, 0, 0, 0, 0);

  COMPUTE_END_RESET(TextReset, text)
}

const void*
nsRuleNode::ComputeUserInterfaceData(void* aStartStruct,
                                     const nsRuleData* aRuleData,
                                     nsStyleContext* aContext,
                                     nsRuleNode* aHighestNode,
                                     const RuleDetail aRuleDetail,
                                     const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_INHERITED(UserInterface, (), ui, parentUI)

  // cursor: enum, url, inherit
  const nsCSSValue* cursorValue = aRuleData->ValueForCursor();
  nsCSSUnit cursorUnit = cursorValue->GetUnit();
  if (cursorUnit != eCSSUnit_Null) {
    delete [] ui->mCursorArray;
    ui->mCursorArray = nullptr;
    ui->mCursorArrayLength = 0;

    if (cursorUnit == eCSSUnit_Inherit ||
        cursorUnit == eCSSUnit_Unset) {
      conditions.SetUncacheable();
      ui->mCursor = parentUI->mCursor;
      ui->CopyCursorArrayFrom(*parentUI);
    }
    else if (cursorUnit == eCSSUnit_Initial) {
      ui->mCursor = NS_STYLE_CURSOR_AUTO;
    }
    else {
      // The parser will never create a list that is *all* URL values --
      // that's invalid.
      MOZ_ASSERT(cursorUnit == eCSSUnit_List || cursorUnit == eCSSUnit_ListDep,
                 "unrecognized cursor unit");
      const nsCSSValueList* list = cursorValue->GetListValue();
      const nsCSSValueList* list2 = list;
      nsIDocument* doc = aContext->PresContext()->Document();
      uint32_t arrayLength = 0;
      for ( ; list->mValue.GetUnit() == eCSSUnit_Array; list = list->mNext)
        if (list->mValue.GetArrayValue()->Item(0).GetImageValue(doc))
          ++arrayLength;

      if (arrayLength != 0) {
        ui->mCursorArray = new nsCursorImage[arrayLength];
        if (ui->mCursorArray) {
          ui->mCursorArrayLength = arrayLength;

          for (nsCursorImage *item = ui->mCursorArray;
               list2->mValue.GetUnit() == eCSSUnit_Array;
               list2 = list2->mNext) {
            nsCSSValue::Array *arr = list2->mValue.GetArrayValue();
            imgIRequest *req = arr->Item(0).GetImageValue(doc);
            if (req) {
              item->SetImage(req);
              if (arr->Item(1).GetUnit() != eCSSUnit_Null) {
                item->mHaveHotspot = true;
                item->mHotspotX = arr->Item(1).GetFloatValue(),
                item->mHotspotY = arr->Item(2).GetFloatValue();
              }
              ++item;
            }
          }
        }
      }

      NS_ASSERTION(list, "Must have non-array value at the end");
      NS_ASSERTION(list->mValue.GetUnit() == eCSSUnit_Enumerated,
                   "Unexpected fallback value at end of cursor list");
      ui->mCursor = list->mValue.GetIntValue();
    }
  }

  // user-input: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForUserInput(),
              ui->mUserInput, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentUI->mUserInput,
              NS_STYLE_USER_INPUT_AUTO, 0, 0, 0, 0);

  // user-modify: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForUserModify(),
              ui->mUserModify, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentUI->mUserModify,
              NS_STYLE_USER_MODIFY_READ_ONLY,
              0, 0, 0, 0);

  // user-focus: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForUserFocus(),
              ui->mUserFocus, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentUI->mUserFocus,
              NS_STYLE_USER_FOCUS_NONE, 0, 0, 0, 0);

  // -moz-window-dragging: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForWindowDragging(),
              ui->mWindowDragging, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentUI->mWindowDragging,
              NS_STYLE_WINDOW_DRAGGING_NO_DRAG, 0, 0, 0, 0);

  COMPUTE_END_INHERITED(UserInterface, ui)
}

const void*
nsRuleNode::ComputeUIResetData(void* aStartStruct,
                               const nsRuleData* aRuleData,
                               nsStyleContext* aContext,
                               nsRuleNode* aHighestNode,
                               const RuleDetail aRuleDetail,
                               const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(UIReset, (), ui, parentUI)

  // user-select: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForUserSelect(),
              ui->mUserSelect, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentUI->mUserSelect,
              NS_STYLE_USER_SELECT_AUTO, 0, 0, 0, 0);

  // ime-mode: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForImeMode(),
              ui->mIMEMode, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentUI->mIMEMode,
              NS_STYLE_IME_MODE_AUTO, 0, 0, 0, 0);

  // force-broken-image-icons: integer, inherit, initial
  SetDiscrete(*aRuleData->ValueForForceBrokenImageIcon(),
              ui->mForceBrokenImageIcon,
              conditions,
              SETDSC_INTEGER | SETDSC_UNSET_INITIAL,
              parentUI->mForceBrokenImageIcon,
              0, 0, 0, 0, 0);

  // -moz-window-shadow: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForWindowShadow(),
              ui->mWindowShadow, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentUI->mWindowShadow,
              NS_STYLE_WINDOW_SHADOW_DEFAULT, 0, 0, 0, 0);

  COMPUTE_END_RESET(UIReset, ui)
}

// Information about each transition or animation property that is
// constant.
struct TransitionPropInfo {
  nsCSSProperty property;
  // Location of the count of the property's computed value.
  uint32_t nsStyleDisplay::* sdCount;
};

// Each property's index in this array must match its index in the
// mutable array |transitionPropData| below.
static const TransitionPropInfo transitionPropInfo[4] = {
  { eCSSProperty_transition_delay,
    &nsStyleDisplay::mTransitionDelayCount },
  { eCSSProperty_transition_duration,
    &nsStyleDisplay::mTransitionDurationCount },
  { eCSSProperty_transition_property,
    &nsStyleDisplay::mTransitionPropertyCount },
  { eCSSProperty_transition_timing_function,
    &nsStyleDisplay::mTransitionTimingFunctionCount },
};

// Each property's index in this array must match its index in the
// mutable array |animationPropData| below.
static const TransitionPropInfo animationPropInfo[8] = {
  { eCSSProperty_animation_delay,
    &nsStyleDisplay::mAnimationDelayCount },
  { eCSSProperty_animation_duration,
    &nsStyleDisplay::mAnimationDurationCount },
  { eCSSProperty_animation_name,
    &nsStyleDisplay::mAnimationNameCount },
  { eCSSProperty_animation_timing_function,
    &nsStyleDisplay::mAnimationTimingFunctionCount },
  { eCSSProperty_animation_direction,
    &nsStyleDisplay::mAnimationDirectionCount },
  { eCSSProperty_animation_fill_mode,
    &nsStyleDisplay::mAnimationFillModeCount },
  { eCSSProperty_animation_play_state,
    &nsStyleDisplay::mAnimationPlayStateCount },
  { eCSSProperty_animation_iteration_count,
    &nsStyleDisplay::mAnimationIterationCountCount },
};

// Information about each transition or animation property that changes
// during ComputeDisplayData.
struct TransitionPropData {
  const nsCSSValueList *list;
  nsCSSUnit unit;
  uint32_t num;
};

static uint32_t
CountTransitionProps(const TransitionPropInfo* aInfo,
                     TransitionPropData* aData,
                     size_t aLength,
                     nsStyleDisplay* aDisplay,
                     const nsStyleDisplay* aParentDisplay,
                     const nsRuleData* aRuleData,
                     RuleNodeCacheConditions& aConditions)
{
  // The four transition properties or eight animation properties are
  // stored in nsCSSDisplay in a single array for all properties.  The
  // number of transitions is equal to the number of items in the
  // longest property's value.  Properties that have fewer values than
  // the longest are filled in by repeating the list.  However, this
  // repetition does not extend the computed value of that particular
  // property (for purposes of inheritance, or, in our code, for when
  // other properties are overridden by a more specific rule).

  // But actually, since the spec isn't clear yet, we'll fully compute
  // all of them (so we can switch easily later), but only care about
  // the ones up to the number of items for 'transition-property', per
  // http://lists.w3.org/Archives/Public/www-style/2009Aug/0109.html .

  // Transitions are difficult to handle correctly because of this.  For
  // example, we need to handle scenarios such as:
  //  * a more general rule specifies transition-property: a, b, c;
  //  * a more specific rule overrides as transition-property: d;
  //
  // If only the general rule applied, we would fill in the extra
  // properties (duration, delay, etc) with initial values to create 3
  // fully-specified transitions.  But when the more specific rule
  // applies, we should only create a single transition.  In order to do
  // this we need to remember which properties were explicitly specified
  // and which ones were just filled in with initial values to get a
  // fully-specified transition, which we do by remembering the number
  // of values for each property.

  uint32_t numTransitions = 0;
  for (size_t i = 0; i < aLength; ++i) {
    const TransitionPropInfo& info = aInfo[i];
    TransitionPropData& data = aData[i];

    // cache whether any of the properties are specified as 'inherit' so
    // we can use it below

    const nsCSSValue& value = *aRuleData->ValueFor(info.property);
    data.unit = value.GetUnit();
    data.list = (value.GetUnit() == eCSSUnit_List ||
                 value.GetUnit() == eCSSUnit_ListDep)
                  ? value.GetListValue() : nullptr;

    // General algorithm to determine how many total transitions we need
    // to build.  For each property:
    //  - if there is no value specified in for the property in
    //    displayData, use the values from the start struct, but only if
    //    they were explicitly specified
    //  - if there is a value specified for the property in displayData:
    //    - if the value is 'inherit', count the number of values for
    //      that property are specified by the parent, but only those
    //      that were explicitly specified
    //    - otherwise, count the number of values specified in displayData


    // calculate number of elements
    if (data.unit == eCSSUnit_Inherit) {
      data.num = aParentDisplay->*(info.sdCount);
      aConditions.SetUncacheable();
    } else if (data.list) {
      data.num = ListLength(data.list);
    } else {
      data.num = aDisplay->*(info.sdCount);
    }
    if (data.num > numTransitions)
      numTransitions = data.num;
  }

  return numTransitions;
}

static void
ComputeTimingFunction(const nsCSSValue& aValue, nsTimingFunction& aResult)
{
  switch (aValue.GetUnit()) {
    case eCSSUnit_Enumerated:
      aResult = nsTimingFunction(aValue.GetIntValue());
      break;
    case eCSSUnit_Cubic_Bezier:
      {
        nsCSSValue::Array* array = aValue.GetArrayValue();
        NS_ASSERTION(array && array->Count() == 4,
                     "Need 4 control points");
        aResult = nsTimingFunction(array->Item(0).GetFloatValue(),
                                   array->Item(1).GetFloatValue(),
                                   array->Item(2).GetFloatValue(),
                                   array->Item(3).GetFloatValue());
      }
      break;
    case eCSSUnit_Steps:
      {
        nsCSSValue::Array* array = aValue.GetArrayValue();
        NS_ASSERTION(array && array->Count() == 2,
                     "Need 2 items");
        NS_ASSERTION(array->Item(0).GetUnit() == eCSSUnit_Integer,
                     "unexpected first value");
        NS_ASSERTION(array->Item(1).GetUnit() == eCSSUnit_Enumerated &&
                     (array->Item(1).GetIntValue() ==
                       NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_START ||
                      array->Item(1).GetIntValue() ==
                       NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_END),
                     "unexpected second value");
        nsTimingFunction::Type type =
          (array->Item(1).GetIntValue() ==
            NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_END)
            ? nsTimingFunction::StepEnd : nsTimingFunction::StepStart;
        aResult = nsTimingFunction(type, array->Item(0).GetIntValue());
      }
      break;
    default:
      NS_NOTREACHED("Invalid transition property unit");
  }
}

const void*
nsRuleNode::ComputeDisplayData(void* aStartStruct,
                               const nsRuleData* aRuleData,
                               nsStyleContext* aContext,
                               nsRuleNode* aHighestNode,
                               const RuleDetail aRuleDetail,
                               const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(Display, (), display, parentDisplay)

  // We may have ended up with aStartStruct's values of mDisplay and
  // mFloats, but those may not be correct if our style data overrides
  // its position or float properties.  Reset to mOriginalDisplay and
  // mOriginalFloats; it if turns out we still need the display/floats
  // adjustments we'll do them below.
  display->mDisplay = display->mOriginalDisplay;
  display->mFloats = display->mOriginalFloats;

  // Each property's index in this array must match its index in the
  // const array |transitionPropInfo| above.
  TransitionPropData transitionPropData[4];
  TransitionPropData& delay = transitionPropData[0];
  TransitionPropData& duration = transitionPropData[1];
  TransitionPropData& property = transitionPropData[2];
  TransitionPropData& timingFunction = transitionPropData[3];

#define FOR_ALL_TRANSITION_PROPS(var_) \
                                      for (uint32_t var_ = 0; var_ < 4; ++var_)

  // CSS Transitions
  uint32_t numTransitions =
    CountTransitionProps(transitionPropInfo, transitionPropData,
                         ArrayLength(transitionPropData),
                         display, parentDisplay, aRuleData,
                         conditions);

  display->mTransitions.SetLength(numTransitions);

  FOR_ALL_TRANSITION_PROPS(p) {
    const TransitionPropInfo& i = transitionPropInfo[p];
    TransitionPropData& d = transitionPropData[p];

    display->*(i.sdCount) = d.num;
  }

  // Fill in the transitions we just allocated with the appropriate values.
  for (uint32_t i = 0; i < numTransitions; ++i) {
    StyleTransition *transition = &display->mTransitions[i];

    if (i >= delay.num) {
      transition->SetDelay(display->mTransitions[i % delay.num].GetDelay());
    } else if (delay.unit == eCSSUnit_Inherit) {
      // FIXME (Bug 522599) (for all transition properties): write a test that
      // detects when this was wrong for i >= delay.num if parent had
      // count for this property not equal to length
      MOZ_ASSERT(i < parentDisplay->mTransitionDelayCount,
                 "delay.num computed incorrectly");
      MOZ_ASSERT(!conditions.Cacheable(),
                 "should have made conditions.Cacheable() false above");
      transition->SetDelay(parentDisplay->mTransitions[i].GetDelay());
    } else if (delay.unit == eCSSUnit_Initial ||
               delay.unit == eCSSUnit_Unset) {
      transition->SetDelay(0.0);
    } else if (delay.list) {
      switch (delay.list->mValue.GetUnit()) {
        case eCSSUnit_Seconds:
          transition->SetDelay(PR_MSEC_PER_SEC *
                               delay.list->mValue.GetFloatValue());
          break;
        case eCSSUnit_Milliseconds:
          transition->SetDelay(delay.list->mValue.GetFloatValue());
          break;
        default:
          NS_NOTREACHED("Invalid delay unit");
      }
    }

    if (i >= duration.num) {
      transition->SetDuration(
        display->mTransitions[i % duration.num].GetDuration());
    } else if (duration.unit == eCSSUnit_Inherit) {
      MOZ_ASSERT(i < parentDisplay->mTransitionDurationCount,
                 "duration.num computed incorrectly");
      MOZ_ASSERT(!conditions.Cacheable(),
                 "should have made conditions.Cacheable() false above");
      transition->SetDuration(parentDisplay->mTransitions[i].GetDuration());
    } else if (duration.unit == eCSSUnit_Initial ||
               duration.unit == eCSSUnit_Unset) {
      transition->SetDuration(0.0);
    } else if (duration.list) {
      switch (duration.list->mValue.GetUnit()) {
        case eCSSUnit_Seconds:
          transition->SetDuration(PR_MSEC_PER_SEC *
                                  duration.list->mValue.GetFloatValue());
          break;
        case eCSSUnit_Milliseconds:
          transition->SetDuration(duration.list->mValue.GetFloatValue());
          break;
        default:
          NS_NOTREACHED("Invalid duration unit");
      }
    }

    if (i >= property.num) {
      transition->CopyPropertyFrom(display->mTransitions[i % property.num]);
    } else if (property.unit == eCSSUnit_Inherit) {
      MOZ_ASSERT(i < parentDisplay->mTransitionPropertyCount,
                 "property.num computed incorrectly");
      MOZ_ASSERT(!conditions.Cacheable(),
                 "should have made conditions.Cacheable() false above");
      transition->CopyPropertyFrom(parentDisplay->mTransitions[i]);
    } else if (property.unit == eCSSUnit_Initial ||
               property.unit == eCSSUnit_Unset) {
      transition->SetProperty(eCSSPropertyExtra_all_properties);
    } else if (property.unit == eCSSUnit_None) {
      transition->SetProperty(eCSSPropertyExtra_no_properties);
    } else if (property.list) {
      const nsCSSValue &val = property.list->mValue;

      if (val.GetUnit() == eCSSUnit_Ident) {
        nsDependentString
          propertyStr(property.list->mValue.GetStringBufferValue());
        nsCSSProperty prop =
          nsCSSProps::LookupProperty(propertyStr,
                                     nsCSSProps::eEnabledForAllContent);
        if (prop == eCSSProperty_UNKNOWN) {
          transition->SetUnknownProperty(propertyStr);
        } else {
          transition->SetProperty(prop);
        }
      } else {
        MOZ_ASSERT(val.GetUnit() == eCSSUnit_All,
                   "Invalid transition property unit");
        transition->SetProperty(eCSSPropertyExtra_all_properties);
      }
    }

    if (i >= timingFunction.num) {
      transition->SetTimingFunction(
        display->mTransitions[i % timingFunction.num].GetTimingFunction());
    } else if (timingFunction.unit == eCSSUnit_Inherit) {
      MOZ_ASSERT(i < parentDisplay->mTransitionTimingFunctionCount,
                 "timingFunction.num computed incorrectly");
      MOZ_ASSERT(!conditions.Cacheable(),
                 "should have made conditions.Cacheable() false above");
      transition->SetTimingFunction(
        parentDisplay->mTransitions[i].GetTimingFunction());
    } else if (timingFunction.unit == eCSSUnit_Initial ||
               timingFunction.unit == eCSSUnit_Unset) {
      transition->SetTimingFunction(
        nsTimingFunction(NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE));
    } else if (timingFunction.list) {
      ComputeTimingFunction(timingFunction.list->mValue,
                            transition->TimingFunctionSlot());
    }

    FOR_ALL_TRANSITION_PROPS(p) {
      const TransitionPropInfo& info = transitionPropInfo[p];
      TransitionPropData& d = transitionPropData[p];

      // if we're at the end of the list, start at the beginning and repeat
      // until we're out of transitions to populate
      if (d.list) {
        d.list = d.list->mNext ? d.list->mNext :
          aRuleData->ValueFor(info.property)->GetListValue();
      }
    }
  }

  // Each property's index in this array must match its index in the
  // const array |animationPropInfo| above.
  TransitionPropData animationPropData[8];
  TransitionPropData& animDelay = animationPropData[0];
  TransitionPropData& animDuration = animationPropData[1];
  TransitionPropData& animName = animationPropData[2];
  TransitionPropData& animTimingFunction = animationPropData[3];
  TransitionPropData& animDirection = animationPropData[4];
  TransitionPropData& animFillMode = animationPropData[5];
  TransitionPropData& animPlayState = animationPropData[6];
  TransitionPropData& animIterationCount = animationPropData[7];

#define FOR_ALL_ANIMATION_PROPS(var_) \
    for (uint32_t var_ = 0; var_ < 8; ++var_)

  // CSS Animations.

  uint32_t numAnimations =
    CountTransitionProps(animationPropInfo, animationPropData,
                         ArrayLength(animationPropData),
                         display, parentDisplay, aRuleData,
                         conditions);

  display->mAnimations.SetLength(numAnimations);

  FOR_ALL_ANIMATION_PROPS(p) {
    const TransitionPropInfo& i = animationPropInfo[p];
    TransitionPropData& d = animationPropData[p];

    display->*(i.sdCount) = d.num;
  }

  // Fill in the animations we just allocated with the appropriate values.
  for (uint32_t i = 0; i < numAnimations; ++i) {
    StyleAnimation *animation = &display->mAnimations[i];

    if (i >= animDelay.num) {
      animation->SetDelay(display->mAnimations[i % animDelay.num].GetDelay());
    } else if (animDelay.unit == eCSSUnit_Inherit) {
      // FIXME (Bug 522599) (for all animation properties): write a test that
      // detects when this was wrong for i >= animDelay.num if parent had
      // count for this property not equal to length
      MOZ_ASSERT(i < parentDisplay->mAnimationDelayCount,
                 "animDelay.num computed incorrectly");
      MOZ_ASSERT(!conditions.Cacheable(),
                 "should have made conditions.Cacheable() false above");
      animation->SetDelay(parentDisplay->mAnimations[i].GetDelay());
    } else if (animDelay.unit == eCSSUnit_Initial ||
               animDelay.unit == eCSSUnit_Unset) {
      animation->SetDelay(0.0);
    } else if (animDelay.list) {
      switch (animDelay.list->mValue.GetUnit()) {
        case eCSSUnit_Seconds:
          animation->SetDelay(PR_MSEC_PER_SEC *
                              animDelay.list->mValue.GetFloatValue());
          break;
        case eCSSUnit_Milliseconds:
          animation->SetDelay(animDelay.list->mValue.GetFloatValue());
          break;
        default:
          NS_NOTREACHED("Invalid delay unit");
      }
    }

    if (i >= animDuration.num) {
      animation->SetDuration(
        display->mAnimations[i % animDuration.num].GetDuration());
    } else if (animDuration.unit == eCSSUnit_Inherit) {
      MOZ_ASSERT(i < parentDisplay->mAnimationDurationCount,
                 "animDuration.num computed incorrectly");
      MOZ_ASSERT(!conditions.Cacheable(),
                 "should have made conditions.Cacheable() false above");
      animation->SetDuration(parentDisplay->mAnimations[i].GetDuration());
    } else if (animDuration.unit == eCSSUnit_Initial ||
               animDuration.unit == eCSSUnit_Unset) {
      animation->SetDuration(0.0);
    } else if (animDuration.list) {
      switch (animDuration.list->mValue.GetUnit()) {
        case eCSSUnit_Seconds:
          animation->SetDuration(PR_MSEC_PER_SEC *
                                 animDuration.list->mValue.GetFloatValue());
          break;
        case eCSSUnit_Milliseconds:
          animation->SetDuration(animDuration.list->mValue.GetFloatValue());
          break;
        default:
          NS_NOTREACHED("Invalid duration unit");
      }
    }

    if (i >= animName.num) {
      animation->SetName(display->mAnimations[i % animName.num].GetName());
    } else if (animName.unit == eCSSUnit_Inherit) {
      MOZ_ASSERT(i < parentDisplay->mAnimationNameCount,
                 "animName.num computed incorrectly");
      MOZ_ASSERT(!conditions.Cacheable(),
                 "should have made conditions.Cacheable() false above");
      animation->SetName(parentDisplay->mAnimations[i].GetName());
    } else if (animName.unit == eCSSUnit_Initial ||
               animName.unit == eCSSUnit_Unset) {
      animation->SetName(EmptyString());
    } else if (animName.list) {
      switch (animName.list->mValue.GetUnit()) {
        case eCSSUnit_Ident: {
          nsDependentString
            nameStr(animName.list->mValue.GetStringBufferValue());
          animation->SetName(nameStr);
          break;
        }
        case eCSSUnit_None: {
          animation->SetName(EmptyString());
          break;
        }
        default:
          MOZ_ASSERT(false, "Invalid animation-name unit");
      }
    }

    if (i >= animTimingFunction.num) {
      animation->SetTimingFunction(
        display->mAnimations[i % animTimingFunction.num].GetTimingFunction());
    } else if (animTimingFunction.unit == eCSSUnit_Inherit) {
      MOZ_ASSERT(i < parentDisplay->mAnimationTimingFunctionCount,
                 "animTimingFunction.num computed incorrectly");
      MOZ_ASSERT(!conditions.Cacheable(),
                 "should have made conditions.Cacheable() false above");
      animation->SetTimingFunction(
        parentDisplay->mAnimations[i].GetTimingFunction());
    } else if (animTimingFunction.unit == eCSSUnit_Initial ||
               animTimingFunction.unit == eCSSUnit_Unset) {
      animation->SetTimingFunction(
        nsTimingFunction(NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE));
    } else if (animTimingFunction.list) {
      ComputeTimingFunction(animTimingFunction.list->mValue,
                            animation->TimingFunctionSlot());
    }

    if (i >= animDirection.num) {
      animation->SetDirection(display->mAnimations[i % animDirection.num].GetDirection());
    } else if (animDirection.unit == eCSSUnit_Inherit) {
      MOZ_ASSERT(i < parentDisplay->mAnimationDirectionCount,
                 "animDirection.num computed incorrectly");
      MOZ_ASSERT(!conditions.Cacheable(),
                 "should have made conditions.Cacheable() false above");
      animation->SetDirection(parentDisplay->mAnimations[i].GetDirection());
    } else if (animDirection.unit == eCSSUnit_Initial ||
               animDirection.unit == eCSSUnit_Unset) {
      animation->SetDirection(NS_STYLE_ANIMATION_DIRECTION_NORMAL);
    } else if (animDirection.list) {
      MOZ_ASSERT(animDirection.list->mValue.GetUnit() == eCSSUnit_Enumerated,
                 "Invalid animation-direction unit");

      animation->SetDirection(animDirection.list->mValue.GetIntValue());
    }

    if (i >= animFillMode.num) {
      animation->SetFillMode(display->mAnimations[i % animFillMode.num].GetFillMode());
    } else if (animFillMode.unit == eCSSUnit_Inherit) {
      MOZ_ASSERT(i < parentDisplay->mAnimationFillModeCount,
                 "animFillMode.num computed incorrectly");
      MOZ_ASSERT(!conditions.Cacheable(),
                 "should have made conditions.Cacheable() false above");
      animation->SetFillMode(parentDisplay->mAnimations[i].GetFillMode());
    } else if (animFillMode.unit == eCSSUnit_Initial ||
               animFillMode.unit == eCSSUnit_Unset) {
      animation->SetFillMode(NS_STYLE_ANIMATION_FILL_MODE_NONE);
    } else if (animFillMode.list) {
      MOZ_ASSERT(animFillMode.list->mValue.GetUnit() == eCSSUnit_Enumerated,
                 "Invalid animation-fill-mode unit");

      animation->SetFillMode(animFillMode.list->mValue.GetIntValue());
    }

    if (i >= animPlayState.num) {
      animation->SetPlayState(display->mAnimations[i % animPlayState.num].GetPlayState());
    } else if (animPlayState.unit == eCSSUnit_Inherit) {
      MOZ_ASSERT(i < parentDisplay->mAnimationPlayStateCount,
                 "animPlayState.num computed incorrectly");
      MOZ_ASSERT(!conditions.Cacheable(),
                 "should have made conditions.Cacheable() false above");
      animation->SetPlayState(parentDisplay->mAnimations[i].GetPlayState());
    } else if (animPlayState.unit == eCSSUnit_Initial ||
               animPlayState.unit == eCSSUnit_Unset) {
      animation->SetPlayState(NS_STYLE_ANIMATION_PLAY_STATE_RUNNING);
    } else if (animPlayState.list) {
      MOZ_ASSERT(animPlayState.list->mValue.GetUnit() == eCSSUnit_Enumerated,
                 "Invalid animation-play-state unit");

      animation->SetPlayState(animPlayState.list->mValue.GetIntValue());
    }

    if (i >= animIterationCount.num) {
      animation->SetIterationCount(display->mAnimations[i % animIterationCount.num].GetIterationCount());
    } else if (animIterationCount.unit == eCSSUnit_Inherit) {
      MOZ_ASSERT(i < parentDisplay->mAnimationIterationCountCount,
                 "animIterationCount.num computed incorrectly");
      MOZ_ASSERT(!conditions.Cacheable(),
                 "should have made conditions.Cacheable() false above");
      animation->SetIterationCount(parentDisplay->mAnimations[i].GetIterationCount());
    } else if (animIterationCount.unit == eCSSUnit_Initial ||
               animIterationCount.unit == eCSSUnit_Unset) {
      animation->SetIterationCount(1.0f);
    } else if (animIterationCount.list) {
      switch (animIterationCount.list->mValue.GetUnit()) {
        case eCSSUnit_Enumerated:
          MOZ_ASSERT(animIterationCount.list->mValue.GetIntValue() ==
                       NS_STYLE_ANIMATION_ITERATION_COUNT_INFINITE,
                     "unexpected value");
          animation->SetIterationCount(NS_IEEEPositiveInfinity());
          break;
        case eCSSUnit_Number:
          animation->SetIterationCount(
            animIterationCount.list->mValue.GetFloatValue());
          break;
        default:
          MOZ_ASSERT(false,
                     "unexpected animation-iteration-count unit");
      }
    }

    FOR_ALL_ANIMATION_PROPS(p) {
      const TransitionPropInfo& info = animationPropInfo[p];
      TransitionPropData& d = animationPropData[p];

      // if we're at the end of the list, start at the beginning and repeat
      // until we're out of animations to populate
      if (d.list) {
        d.list = d.list->mNext ? d.list->mNext :
          aRuleData->ValueFor(info.property)->GetListValue();
      }
    }
  }

  // opacity: factor, inherit, initial
  SetFactor(*aRuleData->ValueForOpacity(), display->mOpacity, conditions,
            parentDisplay->mOpacity, 1.0f,
            SETFCT_OPACITY | SETFCT_UNSET_INITIAL);

  // display: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForDisplay(), display->mDisplay, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentDisplay->mDisplay,
              NS_STYLE_DISPLAY_INLINE, 0, 0, 0, 0);

  // contain: none, enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForContain(), display->mContain, conditions,
              SETDSC_ENUMERATED | SETDSC_NONE | SETDSC_UNSET_INITIAL,
              parentDisplay->mContain,
              NS_STYLE_CONTAIN_NONE, 0, NS_STYLE_CONTAIN_NONE, 0, 0);

  // mix-blend-mode: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForMixBlendMode(), display->mMixBlendMode,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentDisplay->mMixBlendMode, NS_STYLE_BLEND_NORMAL,
              0, 0, 0, 0);

  // scroll-behavior: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForScrollBehavior(), display->mScrollBehavior,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentDisplay->mScrollBehavior, NS_STYLE_SCROLL_BEHAVIOR_AUTO,
              0, 0, 0, 0);

  // scroll-snap-type-x: none, enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForScrollSnapTypeX(), display->mScrollSnapTypeX,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentDisplay->mScrollSnapTypeX, NS_STYLE_SCROLL_SNAP_TYPE_NONE,
              0, 0, 0, 0);

  // scroll-snap-type-y: none, enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForScrollSnapTypeY(), display->mScrollSnapTypeY,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentDisplay->mScrollSnapTypeY, NS_STYLE_SCROLL_SNAP_TYPE_NONE,
              0, 0, 0, 0);

  // scroll-snap-points-x: none, inherit, initial
  const nsCSSValue& scrollSnapPointsX = *aRuleData->ValueForScrollSnapPointsX();
  switch (scrollSnapPointsX.GetUnit()) {
    case eCSSUnit_Null:
      break;
    case eCSSUnit_Initial:
    case eCSSUnit_Unset:
    case eCSSUnit_None:
      display->mScrollSnapPointsX.SetNoneValue();
      break;
    case eCSSUnit_Inherit:
      display->mScrollSnapPointsX = parentDisplay->mScrollSnapPointsX;
      conditions.SetUncacheable();
      break;
    case eCSSUnit_Function: {
      nsCSSValue::Array* func = scrollSnapPointsX.GetArrayValue();
      NS_ASSERTION(func->Item(0).GetKeywordValue() == eCSSKeyword_repeat,
                   "Expected repeat(), got another function name");
      nsStyleCoord coord;
      if (SetCoord(func->Item(1), coord, nsStyleCoord(),
                   SETCOORD_LP | SETCOORD_STORE_CALC |
                   SETCOORD_CALC_CLAMP_NONNEGATIVE,
                   aContext, mPresContext, conditions)) {
        NS_ASSERTION(coord.GetUnit() == eStyleUnit_Coord ||
                     coord.GetUnit() == eStyleUnit_Percent ||
                     coord.GetUnit() == eStyleUnit_Calc,
                     "unexpected unit");
        display->mScrollSnapPointsX = coord;
      }
      break;
    }
    default:
      NS_NOTREACHED("unexpected unit");
  }

  // scroll-snap-points-y: none, inherit, initial
  const nsCSSValue& scrollSnapPointsY = *aRuleData->ValueForScrollSnapPointsY();
  switch (scrollSnapPointsY.GetUnit()) {
    case eCSSUnit_Null:
      break;
    case eCSSUnit_Initial:
    case eCSSUnit_Unset:
    case eCSSUnit_None:
      display->mScrollSnapPointsY.SetNoneValue();
      break;
    case eCSSUnit_Inherit:
      display->mScrollSnapPointsY = parentDisplay->mScrollSnapPointsY;
      conditions.SetUncacheable();
      break;
    case eCSSUnit_Function: {
      nsCSSValue::Array* func = scrollSnapPointsY.GetArrayValue();
      NS_ASSERTION(func->Item(0).GetKeywordValue() == eCSSKeyword_repeat,
                   "Expected repeat(), got another function name");
      nsStyleCoord coord;
      if (SetCoord(func->Item(1), coord, nsStyleCoord(),
                   SETCOORD_LP | SETCOORD_STORE_CALC |
                   SETCOORD_CALC_CLAMP_NONNEGATIVE,
                   aContext, mPresContext, conditions)) {
        NS_ASSERTION(coord.GetUnit() == eStyleUnit_Coord ||
                     coord.GetUnit() == eStyleUnit_Percent ||
                     coord.GetUnit() == eStyleUnit_Calc,
                     "unexpected unit");
        display->mScrollSnapPointsY = coord;
      }
      break;
    }
    default:
      NS_NOTREACHED("unexpected unit");
  }

  // scroll-snap-destination: inherit, initial
  const nsCSSValue& snapDestination = *aRuleData->ValueForScrollSnapDestination();
  switch (snapDestination.GetUnit()) {
    case eCSSUnit_Null:
      break;
    case eCSSUnit_Initial:
    case eCSSUnit_Unset:
      display->mScrollSnapDestination.SetInitialZeroValues();
      break;
    case eCSSUnit_Inherit:
      display->mScrollSnapDestination = parentDisplay->mScrollSnapDestination;
      conditions.SetUncacheable();
      break;
    default: {
        ComputePositionValue(aContext, snapDestination,
                             display->mScrollSnapDestination, conditions);
      }
  }

  // scroll-snap-coordinate: none, inherit, initial
  typedef nsStyleBackground::Position Position;

  const nsCSSValue& snapCoordinate = *aRuleData->ValueForScrollSnapCoordinate();
  switch (snapCoordinate.GetUnit()) {
    case eCSSUnit_Null:
      break;
    case eCSSUnit_Initial:
    case eCSSUnit_Unset:
    case eCSSUnit_None:
      // Unset and Initial is none, indicated by an empty array
      display->mScrollSnapCoordinate.Clear();
      break;
    case eCSSUnit_Inherit:
      display->mScrollSnapCoordinate = parentDisplay->mScrollSnapCoordinate;
      conditions.SetUncacheable();
      break;
    case eCSSUnit_List: {
      display->mScrollSnapCoordinate.Clear();
      const nsCSSValueList* item = snapCoordinate.GetListValue();
      do {
        NS_ASSERTION(item->mValue.GetUnit() != eCSSUnit_Null &&
                     item->mValue.GetUnit() != eCSSUnit_Inherit &&
                     item->mValue.GetUnit() != eCSSUnit_Initial &&
                     item->mValue.GetUnit() != eCSSUnit_Unset,
                     "unexpected unit");
        Position* pos = display->mScrollSnapCoordinate.AppendElement();
        ComputePositionValue(aContext, item->mValue, *pos, conditions);
        item = item->mNext;
      } while(item);
      break;
    }
    default:
      NS_NOTREACHED("unexpected unit");
  }

  // isolation: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForIsolation(), display->mIsolation,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentDisplay->mIsolation, NS_STYLE_ISOLATION_AUTO,
              0, 0, 0, 0);

  // Backup original display value for calculation of a hypothetical
  // box (CSS2 10.6.4/10.6.5), in addition to getting our style data right later.
  // See nsHTMLReflowState::CalculateHypotheticalBox
  display->mOriginalDisplay = display->mDisplay;

  // appearance: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForAppearance(),
              display->mAppearance, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentDisplay->mAppearance,
              NS_THEME_NONE, 0, 0, 0, 0);

  // binding: url, none, inherit
  const nsCSSValue* bindingValue = aRuleData->ValueForBinding();
  if (eCSSUnit_URL == bindingValue->GetUnit()) {
    mozilla::css::URLValue* url = bindingValue->GetURLStructValue();
    NS_ASSERTION(url, "What's going on here?");

    if (MOZ_LIKELY(url->GetURI())) {
      display->mBinding = url;
    } else {
      display->mBinding = nullptr;
    }
  }
  else if (eCSSUnit_None == bindingValue->GetUnit() ||
           eCSSUnit_Initial == bindingValue->GetUnit() ||
           eCSSUnit_Unset == bindingValue->GetUnit()) {
    display->mBinding = nullptr;
  }
  else if (eCSSUnit_Inherit == bindingValue->GetUnit()) {
    conditions.SetUncacheable();
    display->mBinding = parentDisplay->mBinding;
  }

  // position: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForPosition(), display->mPosition, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentDisplay->mPosition,
              NS_STYLE_POSITION_STATIC, 0, 0, 0, 0);

  // clear: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForClear(), display->mBreakType, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentDisplay->mBreakType,
              NS_STYLE_CLEAR_NONE, 0, 0, 0, 0);

  // temp fix for bug 24000
  // Map 'auto' and 'avoid' to false, and 'always', 'left', and
  // 'right' to true.
  // "A conforming user agent may interpret the values 'left' and
  // 'right' as 'always'." - CSS2.1, section 13.3.1
  const nsCSSValue* breakBeforeValue = aRuleData->ValueForPageBreakBefore();
  if (eCSSUnit_Enumerated == breakBeforeValue->GetUnit()) {
    display->mBreakBefore =
      (NS_STYLE_PAGE_BREAK_AVOID != breakBeforeValue->GetIntValue() &&
       NS_STYLE_PAGE_BREAK_AUTO  != breakBeforeValue->GetIntValue());
  }
  else if (eCSSUnit_Initial == breakBeforeValue->GetUnit() ||
           eCSSUnit_Unset == breakBeforeValue->GetUnit()) {
    display->mBreakBefore = false;
  }
  else if (eCSSUnit_Inherit == breakBeforeValue->GetUnit()) {
    conditions.SetUncacheable();
    display->mBreakBefore = parentDisplay->mBreakBefore;
  }

  const nsCSSValue* breakAfterValue = aRuleData->ValueForPageBreakAfter();
  if (eCSSUnit_Enumerated == breakAfterValue->GetUnit()) {
    display->mBreakAfter =
      (NS_STYLE_PAGE_BREAK_AVOID != breakAfterValue->GetIntValue() &&
       NS_STYLE_PAGE_BREAK_AUTO  != breakAfterValue->GetIntValue());
  }
  else if (eCSSUnit_Initial == breakAfterValue->GetUnit() ||
           eCSSUnit_Unset == breakAfterValue->GetUnit()) {
    display->mBreakAfter = false;
  }
  else if (eCSSUnit_Inherit == breakAfterValue->GetUnit()) {
    conditions.SetUncacheable();
    display->mBreakAfter = parentDisplay->mBreakAfter;
  }
  // end temp fix

  // page-break-inside: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForPageBreakInside(),
              display->mBreakInside, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentDisplay->mBreakInside,
              NS_STYLE_PAGE_BREAK_AUTO, 0, 0, 0, 0);

  // touch-action: none, auto, enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForTouchAction(), display->mTouchAction,
              conditions,
              SETDSC_ENUMERATED | SETDSC_AUTO | SETDSC_NONE |
                SETDSC_UNSET_INITIAL,
              parentDisplay->mTouchAction,
              NS_STYLE_TOUCH_ACTION_AUTO,
              NS_STYLE_TOUCH_ACTION_AUTO,
              NS_STYLE_TOUCH_ACTION_NONE, 0, 0);

  // float: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForFloat(),
              display->mFloats, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentDisplay->mFloats,
              NS_STYLE_FLOAT_NONE, 0, 0, 0, 0);
  // Save mFloats in mOriginalFloats in case we need it later
  display->mOriginalFloats = display->mFloats;

  // overflow-x: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForOverflowX(),
              display->mOverflowX, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentDisplay->mOverflowX,
              NS_STYLE_OVERFLOW_VISIBLE, 0, 0, 0, 0);

  // overflow-y: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForOverflowY(),
              display->mOverflowY, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentDisplay->mOverflowY,
              NS_STYLE_OVERFLOW_VISIBLE, 0, 0, 0, 0);

  // CSS3 overflow-x and overflow-y require some fixup as well in some
  // cases.  NS_STYLE_OVERFLOW_VISIBLE and NS_STYLE_OVERFLOW_CLIP are
  // meaningful only when used in both dimensions.
  if (display->mOverflowX != display->mOverflowY &&
      (display->mOverflowX == NS_STYLE_OVERFLOW_VISIBLE ||
       display->mOverflowX == NS_STYLE_OVERFLOW_CLIP ||
       display->mOverflowY == NS_STYLE_OVERFLOW_VISIBLE ||
       display->mOverflowY == NS_STYLE_OVERFLOW_CLIP)) {
    // We can't store in the rule tree since a more specific rule might
    // change these conditions.
    conditions.SetUncacheable();

    // NS_STYLE_OVERFLOW_CLIP is a deprecated value, so if it's specified
    // in only one dimension, convert it to NS_STYLE_OVERFLOW_HIDDEN.
    if (display->mOverflowX == NS_STYLE_OVERFLOW_CLIP)
      display->mOverflowX = NS_STYLE_OVERFLOW_HIDDEN;
    if (display->mOverflowY == NS_STYLE_OVERFLOW_CLIP)
      display->mOverflowY = NS_STYLE_OVERFLOW_HIDDEN;

    // If 'visible' is specified but doesn't match the other dimension, it
    // turns into 'auto'.
    if (display->mOverflowX == NS_STYLE_OVERFLOW_VISIBLE)
      display->mOverflowX = NS_STYLE_OVERFLOW_AUTO;
    if (display->mOverflowY == NS_STYLE_OVERFLOW_VISIBLE)
      display->mOverflowY = NS_STYLE_OVERFLOW_AUTO;
  }

  SetDiscrete(*aRuleData->ValueForOverflowClipBox(), display->mOverflowClipBox,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentDisplay->mOverflowClipBox,
              NS_STYLE_OVERFLOW_CLIP_BOX_PADDING_BOX, 0, 0, 0, 0);

  SetDiscrete(*aRuleData->ValueForResize(), display->mResize, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentDisplay->mResize,
              NS_STYLE_RESIZE_NONE, 0, 0, 0, 0);

  // clip property: length, auto, inherit
  const nsCSSValue* clipValue = aRuleData->ValueForClip();
  switch (clipValue->GetUnit()) {
  case eCSSUnit_Inherit:
    conditions.SetUncacheable();
    display->mClipFlags = parentDisplay->mClipFlags;
    display->mClip = parentDisplay->mClip;
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
  case eCSSUnit_Auto:
    display->mClipFlags = NS_STYLE_CLIP_AUTO;
    display->mClip.SetRect(0,0,0,0);
    break;

  case eCSSUnit_Null:
    break;

  case eCSSUnit_Rect: {
    const nsCSSRect& clipRect = clipValue->GetRectValue();

    display->mClipFlags = NS_STYLE_CLIP_RECT;

    if (clipRect.mTop.GetUnit() == eCSSUnit_Auto) {
      display->mClip.y = 0;
      display->mClipFlags |= NS_STYLE_CLIP_TOP_AUTO;
    }
    else if (clipRect.mTop.IsLengthUnit()) {
      display->mClip.y = CalcLength(clipRect.mTop, aContext,
                                    mPresContext, conditions);
    }

    if (clipRect.mBottom.GetUnit() == eCSSUnit_Auto) {
      // Setting to NS_MAXSIZE for the 'auto' case ensures that
      // the clip rect is nonempty. It is important that mClip be
      // nonempty if the actual clip rect could be nonempty.
      display->mClip.height = NS_MAXSIZE;
      display->mClipFlags |= NS_STYLE_CLIP_BOTTOM_AUTO;
    }
    else if (clipRect.mBottom.IsLengthUnit()) {
      display->mClip.height = CalcLength(clipRect.mBottom, aContext,
                                         mPresContext, conditions) -
                              display->mClip.y;
    }

    if (clipRect.mLeft.GetUnit() == eCSSUnit_Auto) {
      display->mClip.x = 0;
      display->mClipFlags |= NS_STYLE_CLIP_LEFT_AUTO;
    }
    else if (clipRect.mLeft.IsLengthUnit()) {
      display->mClip.x = CalcLength(clipRect.mLeft, aContext,
                                    mPresContext, conditions);
    }

    if (clipRect.mRight.GetUnit() == eCSSUnit_Auto) {
      // Setting to NS_MAXSIZE for the 'auto' case ensures that
      // the clip rect is nonempty. It is important that mClip be
      // nonempty if the actual clip rect could be nonempty.
      display->mClip.width = NS_MAXSIZE;
      display->mClipFlags |= NS_STYLE_CLIP_RIGHT_AUTO;
    }
    else if (clipRect.mRight.IsLengthUnit()) {
      display->mClip.width = CalcLength(clipRect.mRight, aContext,
                                        mPresContext, conditions) -
                             display->mClip.x;
    }
    break;
  }

  default:
    MOZ_ASSERT(false, "unrecognized clip unit");
  }

  if (display->mDisplay != NS_STYLE_DISPLAY_NONE) {
    // CSS2 9.7 specifies display type corrections dealing with 'float'
    // and 'position'.  Since generated content can't be floated or
    // positioned, we can deal with it here.

    nsIAtom* pseudo = aContext->GetPseudo();
    if (pseudo && display->mDisplay == NS_STYLE_DISPLAY_CONTENTS) {
      // We don't want to create frames for anonymous content using a parent
      // frame that is for content above the root of the anon tree.
      // (XXX what we really should check here is not GetPseudo() but if there's
      //  a 'content' property value that implies anon content but we can't
      //  check that here since that's a different struct(?))
      // We might get display:contents to work for CSS_PSEUDO_ELEMENT_CONTAINS_ELEMENTS
      // pseudos (:first-letter etc) in the future, but those have a lot of
      // special handling in frame construction so they are also unsupported
      // for now.
      display->mOriginalDisplay = display->mDisplay = NS_STYLE_DISPLAY_INLINE;
      conditions.SetUncacheable();
    }

    if (nsCSSPseudoElements::firstLetter == pseudo) {
      // a non-floating first-letter must be inline
      // XXX this fix can go away once bug 103189 is fixed correctly
      // Note that we reset mOriginalDisplay to enforce the invariant that it equals mDisplay if we're not positioned or floating.
      display->mOriginalDisplay = display->mDisplay = NS_STYLE_DISPLAY_INLINE;

      // We can't cache the data in the rule tree since if a more specific
      // rule has 'float: left' we'll end up with the wrong 'display'
      // property.
      conditions.SetUncacheable();
    }

    if (display->IsAbsolutelyPositionedStyle()) {
      // 1) if position is 'absolute' or 'fixed' then display must be
      // block-level and float must be 'none'
      EnsureBlockDisplay(display->mDisplay);
      display->mFloats = NS_STYLE_FLOAT_NONE;

      // Note that it's OK to cache this struct in the ruletree
      // because it's fine as-is for any style context that points to
      // it directly, and any use of it as aStartStruct (e.g. if a
      // more specific rule sets "position: static") will use
      // mOriginalDisplay and mOriginalFloats, which we have carefully
      // not changed.
    } else if (display->mFloats != NS_STYLE_FLOAT_NONE) {
      // 2) if float is not none, and display is not none, then we must
      // set a block-level 'display' type per CSS2.1 section 9.7.
      EnsureBlockDisplay(display->mDisplay);

      // Note that it's OK to cache this struct in the ruletree
      // because it's fine as-is for any style context that points to
      // it directly, and any use of it as aStartStruct (e.g. if a
      // more specific rule sets "float: none") will use
      // mOriginalDisplay, which we have carefully not changed.
    }

  }

  /* Convert the nsCSSValueList into an nsTArray<nsTransformFunction *>. */
  const nsCSSValue* transformValue = aRuleData->ValueForTransform();
  switch (transformValue->GetUnit()) {
  case eCSSUnit_Null:
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
  case eCSSUnit_None:
    display->mSpecifiedTransform = nullptr;
    break;

  case eCSSUnit_Inherit:
    display->mSpecifiedTransform = parentDisplay->mSpecifiedTransform;
    conditions.SetUncacheable();
    break;

  case eCSSUnit_SharedList: {
    nsCSSValueSharedList* list = transformValue->GetSharedListValue();
    nsCSSValueList* head = list->mHead;
    MOZ_ASSERT(head, "transform list must have at least one item");
    // can get a _None in here from transform animation
    if (head->mValue.GetUnit() == eCSSUnit_None) {
      MOZ_ASSERT(head->mNext == nullptr, "none must be alone");
      display->mSpecifiedTransform = nullptr;
    } else {
      display->mSpecifiedTransform = list;
    }
    break;
  }

  default:
    MOZ_ASSERT(false, "unrecognized transform unit");
  }

  /* Convert the nsCSSValueList into a will-change bitfield for fast lookup */
  const nsCSSValue* willChangeValue = aRuleData->ValueForWillChange();
  switch (willChangeValue->GetUnit()) {
  case eCSSUnit_Null:
    break;

  case eCSSUnit_List:
  case eCSSUnit_ListDep: {
    display->mWillChange.Clear();
    display->mWillChangeBitField = 0;
    for (const nsCSSValueList* item = willChangeValue->GetListValue();
         item; item = item->mNext)
    {
      if (item->mValue.UnitHasStringValue()) {
        nsAutoString buffer;
        item->mValue.GetStringValue(buffer);
        display->mWillChange.AppendElement(buffer);

        if (buffer.EqualsLiteral("transform")) {
          display->mWillChangeBitField |= NS_STYLE_WILL_CHANGE_TRANSFORM;
        }
        if (buffer.EqualsLiteral("opacity")) {
          display->mWillChangeBitField |= NS_STYLE_WILL_CHANGE_OPACITY;
        }
        if (buffer.EqualsLiteral("scroll-position")) {
          display->mWillChangeBitField |= NS_STYLE_WILL_CHANGE_SCROLL;
        }

        nsCSSProperty prop =
          nsCSSProps::LookupProperty(buffer,
                                     nsCSSProps::eEnabledForAllContent);
        if (prop != eCSSProperty_UNKNOWN &&
            nsCSSProps::PropHasFlags(prop,
                                     CSS_PROPERTY_CREATES_STACKING_CONTEXT))
        {
          display->mWillChangeBitField |= NS_STYLE_WILL_CHANGE_STACKING_CONTEXT;
        }
      }
    }
    break;
  }

  case eCSSUnit_Inherit:
    display->mWillChange = parentDisplay->mWillChange;
    display->mWillChangeBitField = parentDisplay->mWillChangeBitField;
    conditions.SetUncacheable();
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
  case eCSSUnit_Auto:
    display->mWillChange.Clear();
    display->mWillChangeBitField = 0;
    break;

  default:
    MOZ_ASSERT(false, "unrecognized will-change unit");
  }

  /* Convert -moz-transform-origin. */
  const nsCSSValue* transformOriginValue =
    aRuleData->ValueForTransformOrigin();
  if (transformOriginValue->GetUnit() != eCSSUnit_Null) {
    const nsCSSValue& valX =
      transformOriginValue->GetUnit() == eCSSUnit_Triplet ?
        transformOriginValue->GetTripletValue().mXValue : *transformOriginValue;
    const nsCSSValue& valY =
      transformOriginValue->GetUnit() == eCSSUnit_Triplet ?
        transformOriginValue->GetTripletValue().mYValue : *transformOriginValue;
    const nsCSSValue& valZ =
      transformOriginValue->GetUnit() == eCSSUnit_Triplet ?
        transformOriginValue->GetTripletValue().mZValue : *transformOriginValue;

    mozilla::DebugOnly<bool> cX =
       SetCoord(valX, display->mTransformOrigin[0],
                parentDisplay->mTransformOrigin[0],
                SETCOORD_LPH | SETCOORD_INITIAL_HALF |
                  SETCOORD_BOX_POSITION | SETCOORD_STORE_CALC |
                  SETCOORD_UNSET_INITIAL,
                aContext, mPresContext, conditions);

     mozilla::DebugOnly<bool> cY =
       SetCoord(valY, display->mTransformOrigin[1],
                parentDisplay->mTransformOrigin[1],
                SETCOORD_LPH | SETCOORD_INITIAL_HALF |
                  SETCOORD_BOX_POSITION | SETCOORD_STORE_CALC |
                  SETCOORD_UNSET_INITIAL,
                aContext, mPresContext, conditions);

     if (valZ.GetUnit() == eCSSUnit_Null) {
       // Null for the z component means a 0 translation, not
       // unspecified, as we have already checked the triplet
       // value for Null.
       display->mTransformOrigin[2].SetCoordValue(0);
     } else {
       mozilla::DebugOnly<bool> cZ =
         SetCoord(valZ, display->mTransformOrigin[2],
                  parentDisplay->mTransformOrigin[2],
                  SETCOORD_LH | SETCOORD_INITIAL_ZERO | SETCOORD_STORE_CALC |
                    SETCOORD_UNSET_INITIAL,
                  aContext, mPresContext, conditions);
       MOZ_ASSERT(cY == cZ, "changed one but not the other");
     }
     MOZ_ASSERT(cX == cY, "changed one but not the other");
     NS_ASSERTION(cX, "Malformed -moz-transform-origin parse!");
  }

  const nsCSSValue* perspectiveOriginValue =
    aRuleData->ValueForPerspectiveOrigin();
  if (perspectiveOriginValue->GetUnit() != eCSSUnit_Null) {
    mozilla::DebugOnly<bool> result =
      SetPairCoords(*perspectiveOriginValue,
                    display->mPerspectiveOrigin[0],
                    display->mPerspectiveOrigin[1],
                    parentDisplay->mPerspectiveOrigin[0],
                    parentDisplay->mPerspectiveOrigin[1],
                    SETCOORD_LPH | SETCOORD_INITIAL_HALF |
                      SETCOORD_BOX_POSITION | SETCOORD_STORE_CALC |
                      SETCOORD_UNSET_INITIAL,
                    aContext, mPresContext, conditions);
    NS_ASSERTION(result, "Malformed -moz-perspective-origin parse!");
  }

  SetCoord(*aRuleData->ValueForPerspective(), 
           display->mChildPerspective, parentDisplay->mChildPerspective,
           SETCOORD_LAH | SETCOORD_INITIAL_NONE | SETCOORD_NONE |
             SETCOORD_UNSET_INITIAL,
           aContext, mPresContext, conditions);

  SetDiscrete(*aRuleData->ValueForBackfaceVisibility(),
              display->mBackfaceVisibility, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentDisplay->mBackfaceVisibility,
              NS_STYLE_BACKFACE_VISIBILITY_VISIBLE, 0, 0, 0, 0);

  // transform-style: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForTransformStyle(),
              display->mTransformStyle, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentDisplay->mTransformStyle,
              NS_STYLE_TRANSFORM_STYLE_FLAT, 0, 0, 0, 0);

  // transform-box: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForTransformBox(),
              display->mTransformBox, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentDisplay->mTransformBox,
              NS_STYLE_TRANSFORM_BOX_BORDER_BOX, 0, 0, 0, 0);

  // orient: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForOrient(),
              display->mOrient, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentDisplay->mOrient,
              NS_STYLE_ORIENT_INLINE, 0, 0, 0, 0);

  COMPUTE_END_RESET(Display, display)
}

const void*
nsRuleNode::ComputeVisibilityData(void* aStartStruct,
                                  const nsRuleData* aRuleData,
                                  nsStyleContext* aContext,
                                  nsRuleNode* aHighestNode,
                                  const RuleDetail aRuleDetail,
                                  const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_INHERITED(Visibility, (mPresContext),
                          visibility, parentVisibility)

  // IMPORTANT: No properties in this struct have lengths in them.  We
  // depend on this since CalcLengthWith can call StyleVisibility()
  // to get the language for resolving fonts!

  // direction: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForDirection(), visibility->mDirection,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentVisibility->mDirection,
              (GET_BIDI_OPTION_DIRECTION(mPresContext->GetBidi())
               == IBMBIDI_TEXTDIRECTION_RTL)
              ? NS_STYLE_DIRECTION_RTL : NS_STYLE_DIRECTION_LTR,
              0, 0, 0, 0);

  // visibility: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForVisibility(), visibility->mVisible,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentVisibility->mVisible,
              NS_STYLE_VISIBILITY_VISIBLE, 0, 0, 0, 0);

  // pointer-events: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForPointerEvents(), visibility->mPointerEvents,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentVisibility->mPointerEvents,
              NS_STYLE_POINTER_EVENTS_AUTO, 0, 0, 0, 0);

  // writing-mode: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForWritingMode(), visibility->mWritingMode,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentVisibility->mWritingMode,
              NS_STYLE_WRITING_MODE_HORIZONTAL_TB, 0, 0, 0, 0);

  // text-orientation: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForTextOrientation(), visibility->mTextOrientation,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentVisibility->mTextOrientation,
              NS_STYLE_TEXT_ORIENTATION_MIXED, 0, 0, 0, 0);

  // image-orientation: enum, inherit, initial
  const nsCSSValue* orientation = aRuleData->ValueForImageOrientation();
  if (orientation->GetUnit() == eCSSUnit_Inherit ||
      orientation->GetUnit() == eCSSUnit_Unset) {
    conditions.SetUncacheable();
    visibility->mImageOrientation = parentVisibility->mImageOrientation;
  } else if (orientation->GetUnit() == eCSSUnit_Initial) {
    visibility->mImageOrientation = nsStyleImageOrientation();
  } else if (orientation->IsAngularUnit()) {
    double angle = orientation->GetAngleValueInRadians();
    visibility->mImageOrientation =
      nsStyleImageOrientation::CreateAsAngleAndFlip(angle, false);
  } else if (orientation->GetUnit() == eCSSUnit_Array) {
    const nsCSSValue::Array* array = orientation->GetArrayValue();
    MOZ_ASSERT(array->Item(0).IsAngularUnit(),
               "First image-orientation value is not an angle");
    MOZ_ASSERT(array->Item(1).GetUnit() == eCSSUnit_Enumerated &&
               array->Item(1).GetIntValue() == NS_STYLE_IMAGE_ORIENTATION_FLIP,
               "Second image-orientation value is not 'flip'");
    double angle = array->Item(0).GetAngleValueInRadians();
    visibility->mImageOrientation =
      nsStyleImageOrientation::CreateAsAngleAndFlip(angle, true);
    
  } else if (orientation->GetUnit() == eCSSUnit_Enumerated) {
    switch (orientation->GetIntValue()) {
      case NS_STYLE_IMAGE_ORIENTATION_FLIP:
        visibility->mImageOrientation = nsStyleImageOrientation::CreateAsFlip();
        break;
      case NS_STYLE_IMAGE_ORIENTATION_FROM_IMAGE:
        visibility->mImageOrientation = nsStyleImageOrientation::CreateAsFromImage();
        break;
      default:
        NS_NOTREACHED("Invalid image-orientation enumerated value");
    }
  } else {
    MOZ_ASSERT(orientation->GetUnit() == eCSSUnit_Null, "Should be null unit");
  }

  COMPUTE_END_INHERITED(Visibility, visibility)
}

const void*
nsRuleNode::ComputeColorData(void* aStartStruct,
                             const nsRuleData* aRuleData,
                             nsStyleContext* aContext,
                             nsRuleNode* aHighestNode,
                             const RuleDetail aRuleDetail,
                             const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_INHERITED(Color, (mPresContext), color, parentColor)

  // color: color, string, inherit
  // Special case for currentColor.  According to CSS3, setting color to 'currentColor'
  // should behave as if it is inherited
  const nsCSSValue* colorValue = aRuleData->ValueForColor();
  if ((colorValue->GetUnit() == eCSSUnit_EnumColor &&
       colorValue->GetIntValue() == NS_COLOR_CURRENTCOLOR) ||
      colorValue->GetUnit() == eCSSUnit_Unset) {
    color->mColor = parentColor->mColor;
    conditions.SetUncacheable();
  }
  else if (colorValue->GetUnit() == eCSSUnit_Initial) {
    color->mColor = mPresContext->DefaultColor();
  }
  else {
    SetColor(*colorValue, parentColor->mColor, mPresContext, aContext,
             color->mColor, conditions);
  }

  COMPUTE_END_INHERITED(Color, color)
}

// information about how to compute values for background-* properties
template <class SpecifiedValueItem, class ComputedValueItem>
struct BackgroundItemComputer {
};

template <>
struct BackgroundItemComputer<nsCSSValueList, uint8_t>
{
  static void ComputeValue(nsStyleContext* aStyleContext,
                           const nsCSSValueList* aSpecifiedValue,
                           uint8_t& aComputedValue,
                           RuleNodeCacheConditions& aConditions)
  {
    SetDiscrete(aSpecifiedValue->mValue, aComputedValue, aConditions,
                SETDSC_ENUMERATED, uint8_t(0), 0, 0, 0, 0, 0);
  }
};

template <>
struct BackgroundItemComputer<nsCSSValuePairList, nsStyleBackground::Repeat>
{
  static void ComputeValue(nsStyleContext* aStyleContext,
                           const nsCSSValuePairList* aSpecifiedValue,
                           nsStyleBackground::Repeat& aComputedValue,
                           RuleNodeCacheConditions& aConditions)
  {
    NS_ASSERTION(aSpecifiedValue->mXValue.GetUnit() == eCSSUnit_Enumerated &&
                 (aSpecifiedValue->mYValue.GetUnit() == eCSSUnit_Enumerated ||
                  aSpecifiedValue->mYValue.GetUnit() == eCSSUnit_Null),
                 "Invalid unit");
    
    bool hasContraction = true;
    uint8_t value = aSpecifiedValue->mXValue.GetIntValue();
    switch (value) {
    case NS_STYLE_BG_REPEAT_REPEAT_X:
      aComputedValue.mXRepeat = NS_STYLE_BG_REPEAT_REPEAT;
      aComputedValue.mYRepeat = NS_STYLE_BG_REPEAT_NO_REPEAT;
      break;
    case NS_STYLE_BG_REPEAT_REPEAT_Y:
      aComputedValue.mXRepeat = NS_STYLE_BG_REPEAT_NO_REPEAT;
      aComputedValue.mYRepeat = NS_STYLE_BG_REPEAT_REPEAT;
      break;
    default:
      aComputedValue.mXRepeat = value;
      hasContraction = false;
      break;
    }
    
    if (hasContraction) {
      NS_ASSERTION(aSpecifiedValue->mYValue.GetUnit() == eCSSUnit_Null,
                   "Invalid unit.");
      return;
    }
    
    switch (aSpecifiedValue->mYValue.GetUnit()) {
    case eCSSUnit_Null:
      aComputedValue.mYRepeat = aComputedValue.mXRepeat;
      break;
    case eCSSUnit_Enumerated:
      value = aSpecifiedValue->mYValue.GetIntValue();
      NS_ASSERTION(value == NS_STYLE_BG_REPEAT_NO_REPEAT ||
                   value == NS_STYLE_BG_REPEAT_REPEAT, "Unexpected value");
      aComputedValue.mYRepeat = value;
      break;
    default:
      NS_NOTREACHED("Unexpected CSS value");
      break;
    }
  }
};

template <>
struct BackgroundItemComputer<nsCSSValueList, nsStyleImage>
{
  static void ComputeValue(nsStyleContext* aStyleContext,
                           const nsCSSValueList* aSpecifiedValue,
                           nsStyleImage& aComputedValue,
                           RuleNodeCacheConditions& aConditions)
  {
    SetStyleImage(aStyleContext, aSpecifiedValue->mValue, aComputedValue,
                  aConditions);
  }
};

/* Helper function for ComputePositionValue.
 * This function computes a single PositionCoord from two nsCSSValue objects,
 * which represent an edge and an offset from that edge.
 */
typedef nsStyleBackground::Position::PositionCoord PositionCoord;
static void
ComputePositionCoord(nsStyleContext* aStyleContext,
                     const nsCSSValue& aEdge,
                     const nsCSSValue& aOffset,
                     PositionCoord* aResult,
                     RuleNodeCacheConditions& aConditions)
{
  if (eCSSUnit_Percent == aOffset.GetUnit()) {
    aResult->mLength = 0;
    aResult->mPercent = aOffset.GetPercentValue();
    aResult->mHasPercent = true;
  } else if (aOffset.IsLengthUnit()) {
    aResult->mLength = CalcLength(aOffset, aStyleContext,
                                  aStyleContext->PresContext(),
                                  aConditions);
    aResult->mPercent = 0.0f;
    aResult->mHasPercent = false;
  } else if (aOffset.IsCalcUnit()) {
    LengthPercentPairCalcOps ops(aStyleContext,
                                 aStyleContext->PresContext(),
                                 aConditions);
    nsRuleNode::ComputedCalc vals = ComputeCalc(aOffset, ops);
    aResult->mLength = vals.mLength;
    aResult->mPercent = vals.mPercent;
    aResult->mHasPercent = ops.mHasPercent;
  } else {
    aResult->mLength = 0;
    aResult->mPercent = 0.0f;
    aResult->mHasPercent = false;
    NS_ASSERTION(aOffset.GetUnit() == eCSSUnit_Null, "unexpected unit");
  }

  if (eCSSUnit_Enumerated == aEdge.GetUnit()) {
    int sign;
    if (aEdge.GetIntValue() & (NS_STYLE_BG_POSITION_BOTTOM |
                               NS_STYLE_BG_POSITION_RIGHT)) {
      sign = -1;
    } else {
      sign = 1;
    }
    aResult->mPercent = GetFloatFromBoxPosition(aEdge.GetIntValue()) +
                        sign * aResult->mPercent;
    aResult->mLength = sign * aResult->mLength;
    aResult->mHasPercent = true;
  } else {
    NS_ASSERTION(eCSSUnit_Null == aEdge.GetUnit(), "unexpected unit");
  }
}

/* Helper function to convert a CSS <position> specified value into its
 * computed-style form. */
static void
ComputePositionValue(nsStyleContext* aStyleContext,
                     const nsCSSValue& aValue,
                     nsStyleBackground::Position& aComputedValue,
                     RuleNodeCacheConditions& aConditions)
{
  NS_ASSERTION(aValue.GetUnit() == eCSSUnit_Array,
               "unexpected unit for CSS <position> value");

  nsRefPtr<nsCSSValue::Array> positionArray = aValue.GetArrayValue();
  const nsCSSValue &xEdge   = positionArray->Item(0);
  const nsCSSValue &xOffset = positionArray->Item(1);
  const nsCSSValue &yEdge   = positionArray->Item(2);
  const nsCSSValue &yOffset = positionArray->Item(3);

  NS_ASSERTION((eCSSUnit_Enumerated == xEdge.GetUnit()  ||
                eCSSUnit_Null       == xEdge.GetUnit()) &&
               (eCSSUnit_Enumerated == yEdge.GetUnit()  ||
                eCSSUnit_Null       == yEdge.GetUnit()) &&
               eCSSUnit_Enumerated != xOffset.GetUnit()  &&
               eCSSUnit_Enumerated != yOffset.GetUnit(),
               "Invalid background position");

  ComputePositionCoord(aStyleContext, xEdge, xOffset,
                       &aComputedValue.mXPosition,
                       aConditions);

  ComputePositionCoord(aStyleContext, yEdge, yOffset,
                       &aComputedValue.mYPosition,
                       aConditions);
}

template <>
struct BackgroundItemComputer<nsCSSValueList, nsStyleBackground::Position>
{
  static void ComputeValue(nsStyleContext* aStyleContext,
                           const nsCSSValueList* aSpecifiedValue,
                           nsStyleBackground::Position& aComputedValue,
                           RuleNodeCacheConditions& aConditions)
  {
    ComputePositionValue(aStyleContext, aSpecifiedValue->mValue,
                         aComputedValue, aConditions);
  }
};


struct BackgroundSizeAxis {
  nsCSSValue nsCSSValuePairList::* specified;
  nsStyleBackground::Size::Dimension nsStyleBackground::Size::* result;
  uint8_t nsStyleBackground::Size::* type;
};

static const BackgroundSizeAxis gBGSizeAxes[] = {
  { &nsCSSValuePairList::mXValue,
    &nsStyleBackground::Size::mWidth,
    &nsStyleBackground::Size::mWidthType },
  { &nsCSSValuePairList::mYValue,
    &nsStyleBackground::Size::mHeight,
    &nsStyleBackground::Size::mHeightType }
};

template <>
struct BackgroundItemComputer<nsCSSValuePairList, nsStyleBackground::Size>
{
  static void ComputeValue(nsStyleContext* aStyleContext,
                           const nsCSSValuePairList* aSpecifiedValue,
                           nsStyleBackground::Size& aComputedValue,
                           RuleNodeCacheConditions& aConditions)
  {
    nsStyleBackground::Size &size = aComputedValue;
    for (const BackgroundSizeAxis *axis = gBGSizeAxes,
                        *axis_end = ArrayEnd(gBGSizeAxes);
         axis < axis_end; ++axis) {
      const nsCSSValue &specified = aSpecifiedValue->*(axis->specified);
      if (eCSSUnit_Auto == specified.GetUnit()) {
        size.*(axis->type) = nsStyleBackground::Size::eAuto;
      }
      else if (eCSSUnit_Enumerated == specified.GetUnit()) {
        static_assert(nsStyleBackground::Size::eContain ==
                      NS_STYLE_BG_SIZE_CONTAIN &&
                      nsStyleBackground::Size::eCover ==
                      NS_STYLE_BG_SIZE_COVER,
                      "background size constants out of sync");
        MOZ_ASSERT(specified.GetIntValue() == NS_STYLE_BG_SIZE_CONTAIN ||
                   specified.GetIntValue() == NS_STYLE_BG_SIZE_COVER,
                   "invalid enumerated value for size coordinate");
        size.*(axis->type) = specified.GetIntValue();
      }
      else if (eCSSUnit_Null == specified.GetUnit()) {
        MOZ_ASSERT(axis == gBGSizeAxes + 1,
                   "null allowed only as height value, and only "
                   "for contain/cover/initial/inherit");
#ifdef DEBUG
        {
          const nsCSSValue &widthValue = aSpecifiedValue->mXValue;
          MOZ_ASSERT(widthValue.GetUnit() != eCSSUnit_Inherit &&
                     widthValue.GetUnit() != eCSSUnit_Initial &&
                     widthValue.GetUnit() != eCSSUnit_Unset,
                     "initial/inherit/unset should already have been handled");
          MOZ_ASSERT(widthValue.GetUnit() == eCSSUnit_Enumerated &&
                     (widthValue.GetIntValue() == NS_STYLE_BG_SIZE_CONTAIN ||
                      widthValue.GetIntValue() == NS_STYLE_BG_SIZE_COVER),
                     "null height value not corresponding to allowable "
                     "non-null width value");
        }
#endif
        size.*(axis->type) = size.mWidthType;
      }
      else if (eCSSUnit_Percent == specified.GetUnit()) {
        (size.*(axis->result)).mLength = 0;
        (size.*(axis->result)).mPercent = specified.GetPercentValue();
        (size.*(axis->result)).mHasPercent = true;
        size.*(axis->type) = nsStyleBackground::Size::eLengthPercentage;
      }
      else if (specified.IsLengthUnit()) {
        (size.*(axis->result)).mLength =
          CalcLength(specified, aStyleContext, aStyleContext->PresContext(),
                     aConditions);
        (size.*(axis->result)).mPercent = 0.0f;
        (size.*(axis->result)).mHasPercent = false;
        size.*(axis->type) = nsStyleBackground::Size::eLengthPercentage;
      } else {
        MOZ_ASSERT(specified.IsCalcUnit(), "unexpected unit");
        LengthPercentPairCalcOps ops(aStyleContext,
                                     aStyleContext->PresContext(),
                                     aConditions);
        nsRuleNode::ComputedCalc vals = ComputeCalc(specified, ops);
        (size.*(axis->result)).mLength = vals.mLength;
        (size.*(axis->result)).mPercent = vals.mPercent;
        (size.*(axis->result)).mHasPercent = ops.mHasPercent;
        size.*(axis->type) = nsStyleBackground::Size::eLengthPercentage;
      }
    }

    MOZ_ASSERT(size.mWidthType < nsStyleBackground::Size::eDimensionType_COUNT,
               "bad width type");
    MOZ_ASSERT(size.mHeightType < nsStyleBackground::Size::eDimensionType_COUNT,
               "bad height type");
    MOZ_ASSERT((size.mWidthType != nsStyleBackground::Size::eContain &&
                size.mWidthType != nsStyleBackground::Size::eCover) ||
               size.mWidthType == size.mHeightType,
               "contain/cover apply to both dimensions or to neither");
  }
};

template <class ComputedValueItem>
static void
SetBackgroundList(nsStyleContext* aStyleContext,
                  const nsCSSValue& aValue,
                  nsAutoTArray< nsStyleBackground::Layer, 1> &aLayers,
                  const nsAutoTArray<nsStyleBackground::Layer, 1> &aParentLayers,
                  ComputedValueItem nsStyleBackground::Layer::* aResultLocation,
                  ComputedValueItem aInitialValue,
                  uint32_t aParentItemCount,
                  uint32_t& aItemCount,
                  uint32_t& aMaxItemCount,
                  bool& aRebuild,
                  RuleNodeCacheConditions& aConditions)
{
  switch (aValue.GetUnit()) {
  case eCSSUnit_Null:
    break;

  case eCSSUnit_Inherit:
    aRebuild = true;
    aConditions.SetUncacheable();
    aLayers.EnsureLengthAtLeast(aParentItemCount);
    aItemCount = aParentItemCount;
    for (uint32_t i = 0; i < aParentItemCount; ++i) {
      aLayers[i].*aResultLocation = aParentLayers[i].*aResultLocation;
    }
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
    aRebuild = true;
    aItemCount = 1;
    aLayers[0].*aResultLocation = aInitialValue;
    break;

  case eCSSUnit_List:
  case eCSSUnit_ListDep: {
    aRebuild = true;
    aItemCount = 0;
    const nsCSSValueList* item = aValue.GetListValue();
    do {
      NS_ASSERTION(item->mValue.GetUnit() != eCSSUnit_Null &&
                   item->mValue.GetUnit() != eCSSUnit_Inherit &&
                   item->mValue.GetUnit() != eCSSUnit_Initial &&
                   item->mValue.GetUnit() != eCSSUnit_Unset,
                   "unexpected unit");
      ++aItemCount;
      aLayers.EnsureLengthAtLeast(aItemCount);
      BackgroundItemComputer<nsCSSValueList, ComputedValueItem>
        ::ComputeValue(aStyleContext, item,
                       aLayers[aItemCount-1].*aResultLocation,
                       aConditions);
      item = item->mNext;
    } while (item);
    break;
  }

  default:
    MOZ_ASSERT(false, "unexpected unit");
  }

  if (aItemCount > aMaxItemCount)
    aMaxItemCount = aItemCount;
}

template <class ComputedValueItem>
static void
SetBackgroundPairList(nsStyleContext* aStyleContext,
                      const nsCSSValue& aValue,
                      nsAutoTArray< nsStyleBackground::Layer, 1> &aLayers,
                      const nsAutoTArray<nsStyleBackground::Layer, 1>
                                                                 &aParentLayers,
                      ComputedValueItem nsStyleBackground::Layer::*
                                                                aResultLocation,
                      ComputedValueItem aInitialValue,
                      uint32_t aParentItemCount,
                      uint32_t& aItemCount,
                      uint32_t& aMaxItemCount,
                      bool& aRebuild,
                      RuleNodeCacheConditions& aConditions)
{
  switch (aValue.GetUnit()) {
  case eCSSUnit_Null:
    break;

  case eCSSUnit_Inherit:
    aRebuild = true;
    aConditions.SetUncacheable();
    aLayers.EnsureLengthAtLeast(aParentItemCount);
    aItemCount = aParentItemCount;
    for (uint32_t i = 0; i < aParentItemCount; ++i) {
      aLayers[i].*aResultLocation = aParentLayers[i].*aResultLocation;
    }
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
    aRebuild = true;
    aItemCount = 1;
    aLayers[0].*aResultLocation = aInitialValue;
    break;

  case eCSSUnit_PairList:
  case eCSSUnit_PairListDep: {
    aRebuild = true;
    aItemCount = 0;
    const nsCSSValuePairList* item = aValue.GetPairListValue();
    do {
      NS_ASSERTION(item->mXValue.GetUnit() != eCSSUnit_Inherit &&
                   item->mXValue.GetUnit() != eCSSUnit_Initial &&
                   item->mXValue.GetUnit() != eCSSUnit_Unset &&
                   item->mYValue.GetUnit() != eCSSUnit_Inherit &&
                   item->mYValue.GetUnit() != eCSSUnit_Initial &&
                   item->mYValue.GetUnit() != eCSSUnit_Unset,
                   "unexpected unit");
      ++aItemCount;
      aLayers.EnsureLengthAtLeast(aItemCount);
      BackgroundItemComputer<nsCSSValuePairList, ComputedValueItem>
        ::ComputeValue(aStyleContext, item,
                       aLayers[aItemCount-1].*aResultLocation,
                       aConditions);
      item = item->mNext;
    } while (item);
    break;
  }

  default:
    MOZ_ASSERT(false, "unexpected unit");
  }

  if (aItemCount > aMaxItemCount)
    aMaxItemCount = aItemCount;
}

template <class ComputedValueItem>
static void
FillBackgroundList(nsAutoTArray< nsStyleBackground::Layer, 1> &aLayers,
    ComputedValueItem nsStyleBackground::Layer::* aResultLocation,
    uint32_t aItemCount, uint32_t aFillCount)
{
  NS_PRECONDITION(aFillCount <= aLayers.Length(), "unexpected array length");
  for (uint32_t sourceLayer = 0, destLayer = aItemCount;
       destLayer < aFillCount;
       ++sourceLayer, ++destLayer) {
    aLayers[destLayer].*aResultLocation =
      aLayers[sourceLayer].*aResultLocation;
  }
}

const void*
nsRuleNode::ComputeBackgroundData(void* aStartStruct,
                                  const nsRuleData* aRuleData,
                                  nsStyleContext* aContext,
                                  nsRuleNode* aHighestNode,
                                  const RuleDetail aRuleDetail,
                                  const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(Background, (), bg, parentBG)

  // background-color: color, string, inherit
  const nsCSSValue* backColorValue = aRuleData->ValueForBackgroundColor();
  if (eCSSUnit_Initial == backColorValue->GetUnit() ||
      eCSSUnit_Unset == backColorValue->GetUnit()) {
    bg->mBackgroundColor = NS_RGBA(0, 0, 0, 0);
  } else if (!SetColor(*backColorValue, parentBG->mBackgroundColor,
                       mPresContext, aContext, bg->mBackgroundColor,
                       conditions)) {
    NS_ASSERTION(eCSSUnit_Null == backColorValue->GetUnit(),
                 "unexpected color unit");
  }

  uint32_t maxItemCount = 1;
  bool rebuild = false;

  // background-image: url (stored as image), none, inherit [list]
  nsStyleImage initialImage;
  SetBackgroundList(aContext, *aRuleData->ValueForBackgroundImage(),
                    bg->mLayers,
                    parentBG->mLayers, &nsStyleBackground::Layer::mImage,
                    initialImage, parentBG->mImageCount, bg->mImageCount,
                    maxItemCount, rebuild, conditions);

  // background-repeat: enum, inherit, initial [pair list]
  nsStyleBackground::Repeat initialRepeat;
  initialRepeat.SetInitialValues();
  SetBackgroundPairList(aContext, *aRuleData->ValueForBackgroundRepeat(),
                        bg->mLayers,
                        parentBG->mLayers, &nsStyleBackground::Layer::mRepeat,
                        initialRepeat, parentBG->mRepeatCount,
                        bg->mRepeatCount, maxItemCount, rebuild, 
                        conditions);

  // background-attachment: enum, inherit, initial [list]
  SetBackgroundList(aContext, *aRuleData->ValueForBackgroundAttachment(),
                    bg->mLayers, parentBG->mLayers,
                    &nsStyleBackground::Layer::mAttachment,
                    uint8_t(NS_STYLE_BG_ATTACHMENT_SCROLL),
                    parentBG->mAttachmentCount,
                    bg->mAttachmentCount, maxItemCount, rebuild,
                    conditions);

  // background-clip: enum, inherit, initial [list]
  SetBackgroundList(aContext, *aRuleData->ValueForBackgroundClip(),
                    bg->mLayers,
                    parentBG->mLayers, &nsStyleBackground::Layer::mClip,
                    uint8_t(NS_STYLE_BG_CLIP_BORDER), parentBG->mClipCount,
                    bg->mClipCount, maxItemCount, rebuild, conditions);

  // background-blend-mode: enum, inherit, initial [list]
  SetBackgroundList(aContext, *aRuleData->ValueForBackgroundBlendMode(),
                    bg->mLayers,
                    parentBG->mLayers, &nsStyleBackground::Layer::mBlendMode,
                    uint8_t(NS_STYLE_BLEND_NORMAL), parentBG->mBlendModeCount,
                    bg->mBlendModeCount, maxItemCount, rebuild,
                    conditions);

  // background-origin: enum, inherit, initial [list]
  SetBackgroundList(aContext, *aRuleData->ValueForBackgroundOrigin(),
                    bg->mLayers,
                    parentBG->mLayers, &nsStyleBackground::Layer::mOrigin,
                    uint8_t(NS_STYLE_BG_ORIGIN_PADDING), parentBG->mOriginCount,
                    bg->mOriginCount, maxItemCount, rebuild,
                    conditions);

  // background-position: enum, length, percent (flags), inherit [pair list]
  nsStyleBackground::Position initialPosition;
  initialPosition.SetInitialPercentValues(0.0f);
  SetBackgroundList(aContext, *aRuleData->ValueForBackgroundPosition(),
                    bg->mLayers,
                    parentBG->mLayers, &nsStyleBackground::Layer::mPosition,
                    initialPosition, parentBG->mPositionCount,
                    bg->mPositionCount, maxItemCount, rebuild,
                    conditions);

  // background-size: enum, length, auto, inherit, initial [pair list]
  nsStyleBackground::Size initialSize;
  initialSize.SetInitialValues();
  SetBackgroundPairList(aContext, *aRuleData->ValueForBackgroundSize(),
                        bg->mLayers,
                        parentBG->mLayers, &nsStyleBackground::Layer::mSize,
                        initialSize, parentBG->mSizeCount,
                        bg->mSizeCount, maxItemCount, rebuild,
                        conditions);

  if (rebuild) {
    // Delete any extra items.  We need to keep layers in which any
    // property was specified.
    bg->mLayers.TruncateLength(maxItemCount);

    uint32_t fillCount = bg->mImageCount;
    FillBackgroundList(bg->mLayers, &nsStyleBackground::Layer::mImage,
                       bg->mImageCount, fillCount);
    FillBackgroundList(bg->mLayers, &nsStyleBackground::Layer::mRepeat,
                       bg->mRepeatCount, fillCount);
    FillBackgroundList(bg->mLayers, &nsStyleBackground::Layer::mAttachment,
                       bg->mAttachmentCount, fillCount);
    FillBackgroundList(bg->mLayers, &nsStyleBackground::Layer::mClip,
                       bg->mClipCount, fillCount);
    FillBackgroundList(bg->mLayers, &nsStyleBackground::Layer::mBlendMode,
                       bg->mBlendModeCount, fillCount);
    FillBackgroundList(bg->mLayers, &nsStyleBackground::Layer::mOrigin,
                       bg->mOriginCount, fillCount);
    FillBackgroundList(bg->mLayers, &nsStyleBackground::Layer::mPosition,
                       bg->mPositionCount, fillCount);
    FillBackgroundList(bg->mLayers, &nsStyleBackground::Layer::mSize,
                       bg->mSizeCount, fillCount);
  }

  // Now that the dust has settled, register the images with the document
  for (uint32_t i = 0; i < bg->mImageCount; ++i)
    bg->mLayers[i].TrackImages(aContext->PresContext());

  COMPUTE_END_RESET(Background, bg)
}

const void*
nsRuleNode::ComputeMarginData(void* aStartStruct,
                              const nsRuleData* aRuleData,
                              nsStyleContext* aContext,
                              nsRuleNode* aHighestNode,
                              const RuleDetail aRuleDetail,
                              const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(Margin, (), margin, parentMargin)

  // margin: length, percent, calc, inherit
  const nsCSSProperty* subprops =
    nsCSSProps::SubpropertyEntryFor(eCSSProperty_margin);
  nsStyleCoord coord;
  NS_FOR_CSS_SIDES(side) {
    nsStyleCoord parentCoord = parentMargin->mMargin.Get(side);
    if (SetCoord(*aRuleData->ValueFor(subprops[side]),
                 coord, parentCoord,
                 SETCOORD_LPAH | SETCOORD_INITIAL_ZERO | SETCOORD_STORE_CALC |
                   SETCOORD_UNSET_INITIAL,
                 aContext, mPresContext, conditions)) {
      margin->mMargin.Set(side, coord);
    }
  }

  margin->RecalcData();
  COMPUTE_END_RESET(Margin, margin)
}

static void
SetBorderImageRect(const nsCSSValue& aValue,
                   /** outparam */ nsCSSRect& aRect)
{
  switch (aValue.GetUnit()) {
  case eCSSUnit_Null:
    aRect.Reset();
    break;
  case eCSSUnit_Rect:
    aRect = aValue.GetRectValue();
    break;
  case eCSSUnit_Inherit:
  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
    aRect.SetAllSidesTo(aValue);
    break;
  default:
    NS_ASSERTION(false, "Unexpected border image value for rect.");
  }
}

static void
SetBorderImagePair(const nsCSSValue& aValue,
                   /** outparam */ nsCSSValuePair& aPair)
{
  switch (aValue.GetUnit()) {
  case eCSSUnit_Null:
    aPair.Reset();
    break;
  case eCSSUnit_Pair:
    aPair = aValue.GetPairValue();
    break;
  case eCSSUnit_Inherit:
  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
    aPair.SetBothValuesTo(aValue);
    break;
  default:
    NS_ASSERTION(false, "Unexpected border image value for pair.");
  }
}

static void
SetBorderImageSlice(const nsCSSValue& aValue,
                    /** outparam */ nsCSSValue& aSlice,
                    /** outparam */ nsCSSValue& aFill)
{
  const nsCSSValueList* valueList;
  switch (aValue.GetUnit()) {
  case eCSSUnit_Null:
    aSlice.Reset();
    aFill.Reset();
    break;
  case eCSSUnit_List:
    // Get slice dimensions.
    valueList = aValue.GetListValue();
    aSlice = valueList->mValue;

    // Get "fill" keyword.
    valueList = valueList->mNext;
    if (valueList) {
      aFill = valueList->mValue;
    } else {
      aFill.SetInitialValue();
    }
    break;
  case eCSSUnit_Inherit:
  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
    aSlice = aValue;
    aFill = aValue;
    break;
  default:
    NS_ASSERTION(false, "Unexpected border image value for pair.");
  }
}

const void*
nsRuleNode::ComputeBorderData(void* aStartStruct,
                              const nsRuleData* aRuleData,
                              nsStyleContext* aContext,
                              nsRuleNode* aHighestNode,
                              const RuleDetail aRuleDetail,
                              const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(Border, (mPresContext), border, parentBorder)

  // box-decoration-break: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForBoxDecorationBreak(),
              border->mBoxDecorationBreak, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentBorder->mBoxDecorationBreak,
              NS_STYLE_BOX_DECORATION_BREAK_SLICE, 0, 0, 0, 0);

  // box-shadow: none, list, inherit, initial
  const nsCSSValue* boxShadowValue = aRuleData->ValueForBoxShadow();
  switch (boxShadowValue->GetUnit()) {
  case eCSSUnit_Null:
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
  case eCSSUnit_None:
    border->mBoxShadow = nullptr;
    break;

  case eCSSUnit_Inherit:
    border->mBoxShadow = parentBorder->mBoxShadow;
    conditions.SetUncacheable();
    break;

  case eCSSUnit_List:
  case eCSSUnit_ListDep:
    border->mBoxShadow = GetShadowData(boxShadowValue->GetListValue(),
                                       aContext, true, conditions);
    break;

  default:
    MOZ_ASSERT(false, "unrecognized shadow unit");
  }

  // border-width, border-*-width: length, enum, inherit
  nsStyleCoord coord;
  {
    const nsCSSProperty* subprops =
      nsCSSProps::SubpropertyEntryFor(eCSSProperty_border_width);
    NS_FOR_CSS_SIDES(side) {
      const nsCSSValue& value = *aRuleData->ValueFor(subprops[side]);
      NS_ASSERTION(eCSSUnit_Percent != value.GetUnit(),
                   "Percentage borders not implemented yet "
                   "If implementing, make sure to fix all consumers of "
                   "nsStyleBorder, the IsPercentageAwareChild method, "
                   "the nsAbsoluteContainingBlock::FrameDependsOnContainer "
                   "method, the "
                   "nsLineLayout::IsPercentageAwareReplacedElement method "
                   "and probably some other places");
      if (eCSSUnit_Enumerated == value.GetUnit()) {
        NS_ASSERTION(value.GetIntValue() == NS_STYLE_BORDER_WIDTH_THIN ||
                     value.GetIntValue() == NS_STYLE_BORDER_WIDTH_MEDIUM ||
                     value.GetIntValue() == NS_STYLE_BORDER_WIDTH_THICK,
                     "Unexpected enum value");
        border->SetBorderWidth(side,
                               (mPresContext->GetBorderWidthTable())[value.GetIntValue()]);
      }
      // OK to pass bad aParentCoord since we're not passing SETCOORD_INHERIT
      else if (SetCoord(value, coord, nsStyleCoord(),
                        SETCOORD_LENGTH | SETCOORD_CALC_LENGTH_ONLY,
                        aContext, mPresContext, conditions)) {
        NS_ASSERTION(coord.GetUnit() == eStyleUnit_Coord, "unexpected unit");
        // clamp negative calc() to 0.
        border->SetBorderWidth(side, std::max(coord.GetCoordValue(), 0));
      }
      else if (eCSSUnit_Inherit == value.GetUnit()) {
        conditions.SetUncacheable();
        border->SetBorderWidth(side,
                               parentBorder->GetComputedBorder().Side(side));
      }
      else if (eCSSUnit_Initial == value.GetUnit() ||
               eCSSUnit_Unset == value.GetUnit()) {
        border->SetBorderWidth(side,
          (mPresContext->GetBorderWidthTable())[NS_STYLE_BORDER_WIDTH_MEDIUM]);
      }
      else {
        NS_ASSERTION(eCSSUnit_Null == value.GetUnit(),
                     "missing case handling border width");
      }
    }
  }

  // border-style, border-*-style: enum, inherit
  {
    const nsCSSProperty* subprops =
      nsCSSProps::SubpropertyEntryFor(eCSSProperty_border_style);
    NS_FOR_CSS_SIDES(side) {
      const nsCSSValue& value = *aRuleData->ValueFor(subprops[side]);
      nsCSSUnit unit = value.GetUnit();
      MOZ_ASSERT(eCSSUnit_None != unit,
                 "'none' should be handled as enumerated value");
      if (eCSSUnit_Enumerated == unit) {
        border->SetBorderStyle(side, value.GetIntValue());
      }
      else if (eCSSUnit_Initial == unit ||
               eCSSUnit_Unset == unit) {
        border->SetBorderStyle(side, NS_STYLE_BORDER_STYLE_NONE);
      }
      else if (eCSSUnit_Inherit == unit) {
        conditions.SetUncacheable();
        border->SetBorderStyle(side, parentBorder->GetBorderStyle(side));
      }
    }
  }

  // -moz-border-*-colors: color, string, enum, none, inherit/initial
  nscolor borderColor;
  nscolor unused = NS_RGB(0,0,0);

  static const nsCSSProperty borderColorsProps[] = {
    eCSSProperty_border_top_colors,
    eCSSProperty_border_right_colors,
    eCSSProperty_border_bottom_colors,
    eCSSProperty_border_left_colors
  };

  NS_FOR_CSS_SIDES(side) {
    const nsCSSValue& value = *aRuleData->ValueFor(borderColorsProps[side]);
    switch (value.GetUnit()) {
    case eCSSUnit_Null:
      break;

    case eCSSUnit_Initial:
    case eCSSUnit_Unset:
    case eCSSUnit_None:
      border->ClearBorderColors(side);
      break;

    case eCSSUnit_Inherit: {
      conditions.SetUncacheable();
      border->ClearBorderColors(side);
      if (parentContext) {
        nsBorderColors *parentColors;
        parentBorder->GetCompositeColors(side, &parentColors);
        if (parentColors) {
          border->EnsureBorderColors();
          border->mBorderColors[side] = parentColors->Clone();
        }
      }
      break;
    }

    case eCSSUnit_List:
    case eCSSUnit_ListDep: {
      // Some composite border color information has been specified for this
      // border side.
      border->EnsureBorderColors();
      border->ClearBorderColors(side);
      const nsCSSValueList* list = value.GetListValue();
      while (list) {
        if (SetColor(list->mValue, unused, mPresContext,
                     aContext, borderColor, conditions))
          border->AppendBorderColor(side, borderColor);
        else {
          NS_NOTREACHED("unexpected item in -moz-border-*-colors list");
        }
        list = list->mNext;
      }
      break;
    }

    default:
      MOZ_ASSERT(false, "unrecognized border color unit");
    }
  }

  // border-color, border-*-color: color, string, enum, inherit
  {
    const nsCSSProperty* subprops =
      nsCSSProps::SubpropertyEntryFor(eCSSProperty_border_color);
    bool foreground;
    NS_FOR_CSS_SIDES(side) {
      const nsCSSValue& value = *aRuleData->ValueFor(subprops[side]);
      if (eCSSUnit_Inherit == value.GetUnit()) {
        conditions.SetUncacheable();
        if (parentContext) {
          parentBorder->GetBorderColor(side, borderColor, foreground);
          if (foreground) {
            // We want to inherit the color from the parent, not use the
            // color on the element where this chunk of style data will be
            // used.  We can ensure that the data for the parent are fully
            // computed (unlike for the element where this will be used, for
            // which the color could be specified on a more specific rule).
            border->SetBorderColor(side, parentContext->StyleColor()->mColor);
          } else
            border->SetBorderColor(side, borderColor);
        } else {
          // We're the root
          border->SetBorderToForeground(side);
        }
      }
      else if (SetColor(value, unused, mPresContext, aContext, borderColor,
                        conditions)) {
        border->SetBorderColor(side, borderColor);
      }
      else if (eCSSUnit_Enumerated == value.GetUnit()) {
        switch (value.GetIntValue()) {
          case NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR:
            border->SetBorderToForeground(side);
            break;
          default:
            NS_NOTREACHED("Unexpected enumerated color");
            break;
        }
      }
      else if (eCSSUnit_Initial == value.GetUnit() ||
               eCSSUnit_Unset == value.GetUnit()) {
        border->SetBorderToForeground(side);
      }
    }
  }

  // border-radius: length, percent, inherit
  {
    const nsCSSProperty* subprops =
      nsCSSProps::SubpropertyEntryFor(eCSSProperty_border_radius);
    NS_FOR_CSS_FULL_CORNERS(corner) {
      int cx = NS_FULL_TO_HALF_CORNER(corner, false);
      int cy = NS_FULL_TO_HALF_CORNER(corner, true);
      const nsCSSValue& radius = *aRuleData->ValueFor(subprops[corner]);
      nsStyleCoord parentX = parentBorder->mBorderRadius.Get(cx);
      nsStyleCoord parentY = parentBorder->mBorderRadius.Get(cy);
      nsStyleCoord coordX, coordY;

      if (SetPairCoords(radius, coordX, coordY, parentX, parentY,
                        SETCOORD_LPH | SETCOORD_INITIAL_ZERO |
                          SETCOORD_STORE_CALC | SETCOORD_UNSET_INITIAL,
                        aContext, mPresContext, conditions)) {
        border->mBorderRadius.Set(cx, coordX);
        border->mBorderRadius.Set(cy, coordY);
      }
    }
  }

  // float-edge: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForFloatEdge(),
              border->mFloatEdge, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentBorder->mFloatEdge,
              NS_STYLE_FLOAT_EDGE_CONTENT, 0, 0, 0, 0);

  // border-image-source
  const nsCSSValue* borderImageSource = aRuleData->ValueForBorderImageSource();
  if (borderImageSource->GetUnit() == eCSSUnit_Inherit) {
    conditions.SetUncacheable();
    border->mBorderImageSource = parentBorder->mBorderImageSource;
  } else {
    SetStyleImage(aContext,
                  *borderImageSource,
                  border->mBorderImageSource,
                  conditions);
  }

  nsCSSValue borderImageSliceValue;
  nsCSSValue borderImageSliceFill;
  SetBorderImageSlice(*aRuleData->ValueForBorderImageSlice(),
                      borderImageSliceValue, borderImageSliceFill);

  // border-image-slice: fill
  SetDiscrete(borderImageSliceFill,
              border->mBorderImageFill,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentBorder->mBorderImageFill,
              NS_STYLE_BORDER_IMAGE_SLICE_NOFILL, 0, 0, 0, 0);

  nsCSSRect borderImageSlice;
  SetBorderImageRect(borderImageSliceValue, borderImageSlice);

  nsCSSRect borderImageWidth;
  SetBorderImageRect(*aRuleData->ValueForBorderImageWidth(),
                     borderImageWidth);

  nsCSSRect borderImageOutset;
  SetBorderImageRect(*aRuleData->ValueForBorderImageOutset(),
                     borderImageOutset);

  NS_FOR_CSS_SIDES (side) {
    // border-image-slice
    if (SetCoord(borderImageSlice.*(nsCSSRect::sides[side]), coord,
                 parentBorder->mBorderImageSlice.Get(side),
                 SETCOORD_FACTOR | SETCOORD_PERCENT |
                   SETCOORD_INHERIT | SETCOORD_INITIAL_HUNDRED_PCT |
                   SETCOORD_UNSET_INITIAL,
                 aContext, mPresContext, conditions)) {
      border->mBorderImageSlice.Set(side, coord);
    }

    // border-image-width
    // 'auto' here means "same as slice"
    if (SetCoord(borderImageWidth.*(nsCSSRect::sides[side]), coord,
                 parentBorder->mBorderImageWidth.Get(side),
                 SETCOORD_LPAH | SETCOORD_FACTOR | SETCOORD_INITIAL_FACTOR_ONE |
                   SETCOORD_UNSET_INITIAL,
                 aContext, mPresContext, conditions)) {
      border->mBorderImageWidth.Set(side, coord);
    }

    // border-image-outset
    if (SetCoord(borderImageOutset.*(nsCSSRect::sides[side]), coord,
                 parentBorder->mBorderImageOutset.Get(side),
                 SETCOORD_LENGTH | SETCOORD_FACTOR |
                   SETCOORD_INHERIT | SETCOORD_INITIAL_FACTOR_ZERO |
                   SETCOORD_UNSET_INITIAL,
                 aContext, mPresContext, conditions)) {
      border->mBorderImageOutset.Set(side, coord);
    }
  }

  // border-image-repeat
  nsCSSValuePair borderImageRepeat;
  SetBorderImagePair(*aRuleData->ValueForBorderImageRepeat(),
                     borderImageRepeat);

  SetDiscrete(borderImageRepeat.mXValue,
              border->mBorderImageRepeatH,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentBorder->mBorderImageRepeatH,
              NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH, 0, 0, 0, 0);

  SetDiscrete(borderImageRepeat.mYValue,
              border->mBorderImageRepeatV,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentBorder->mBorderImageRepeatV,
              NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH, 0, 0, 0, 0);

  border->TrackImage(aContext->PresContext());

  COMPUTE_END_RESET(Border, border)
}

const void*
nsRuleNode::ComputePaddingData(void* aStartStruct,
                               const nsRuleData* aRuleData,
                               nsStyleContext* aContext,
                               nsRuleNode* aHighestNode,
                               const RuleDetail aRuleDetail,
                               const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(Padding, (), padding, parentPadding)

  // padding: length, percent, calc, inherit
  const nsCSSProperty* subprops =
    nsCSSProps::SubpropertyEntryFor(eCSSProperty_padding);
  nsStyleCoord coord;
  NS_FOR_CSS_SIDES(side) {
    nsStyleCoord parentCoord = parentPadding->mPadding.Get(side);
    if (SetCoord(*aRuleData->ValueFor(subprops[side]),
                 coord, parentCoord,
                 SETCOORD_LPH | SETCOORD_INITIAL_ZERO | SETCOORD_STORE_CALC |
                   SETCOORD_UNSET_INITIAL,
                 aContext, mPresContext, conditions)) {
      padding->mPadding.Set(side, coord);
    }
  }

  padding->RecalcData();
  COMPUTE_END_RESET(Padding, padding)
}

const void*
nsRuleNode::ComputeOutlineData(void* aStartStruct,
                               const nsRuleData* aRuleData,
                               nsStyleContext* aContext,
                               nsRuleNode* aHighestNode,
                               const RuleDetail aRuleDetail,
                               const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(Outline, (mPresContext), outline, parentOutline)

  // outline-width: length, enum, inherit
  const nsCSSValue* outlineWidthValue = aRuleData->ValueForOutlineWidth();
  if (eCSSUnit_Initial == outlineWidthValue->GetUnit() ||
      eCSSUnit_Unset == outlineWidthValue->GetUnit()) {
    outline->mOutlineWidth =
      nsStyleCoord(NS_STYLE_BORDER_WIDTH_MEDIUM, eStyleUnit_Enumerated);
  }
  else {
    SetCoord(*outlineWidthValue, outline->mOutlineWidth,
             parentOutline->mOutlineWidth,
             SETCOORD_LEH | SETCOORD_CALC_LENGTH_ONLY, aContext,
             mPresContext, conditions);
  }

  // outline-offset: length, inherit
  nsStyleCoord tempCoord;
  const nsCSSValue* outlineOffsetValue = aRuleData->ValueForOutlineOffset();
  if (SetCoord(*outlineOffsetValue, tempCoord,
               nsStyleCoord(parentOutline->mOutlineOffset,
                            nsStyleCoord::CoordConstructor),
               SETCOORD_LH | SETCOORD_INITIAL_ZERO | SETCOORD_CALC_LENGTH_ONLY |
                 SETCOORD_UNSET_INITIAL,
               aContext, mPresContext, conditions)) {
    outline->mOutlineOffset = tempCoord.GetCoordValue();
  } else {
    NS_ASSERTION(outlineOffsetValue->GetUnit() == eCSSUnit_Null,
                 "unexpected unit");
  }

  // outline-color: color, string, enum, inherit
  nscolor outlineColor;
  nscolor unused = NS_RGB(0,0,0);
  const nsCSSValue* outlineColorValue = aRuleData->ValueForOutlineColor();
  if (eCSSUnit_Inherit == outlineColorValue->GetUnit()) {
    conditions.SetUncacheable();
    if (parentContext) {
      if (parentOutline->GetOutlineColor(outlineColor))
        outline->SetOutlineColor(outlineColor);
      else {
        // We want to inherit the color from the parent, not use the
        // color on the element where this chunk of style data will be
        // used.  We can ensure that the data for the parent are fully
        // computed (unlike for the element where this will be used, for
        // which the color could be specified on a more specific rule).
        outline->SetOutlineColor(parentContext->StyleColor()->mColor);
      }
    } else {
      outline->SetOutlineInitialColor();
    }
  }
  else if (SetColor(*outlineColorValue, unused, mPresContext,
                    aContext, outlineColor, conditions))
    outline->SetOutlineColor(outlineColor);
  else if (eCSSUnit_Enumerated == outlineColorValue->GetUnit() ||
           eCSSUnit_Initial == outlineColorValue->GetUnit() ||
           eCSSUnit_Unset == outlineColorValue->GetUnit()) {
    outline->SetOutlineInitialColor();
  }

  // -moz-outline-radius: length, percent, inherit
  {
    const nsCSSProperty* subprops =
      nsCSSProps::SubpropertyEntryFor(eCSSProperty__moz_outline_radius);
    NS_FOR_CSS_FULL_CORNERS(corner) {
      int cx = NS_FULL_TO_HALF_CORNER(corner, false);
      int cy = NS_FULL_TO_HALF_CORNER(corner, true);
      const nsCSSValue& radius = *aRuleData->ValueFor(subprops[corner]);
      nsStyleCoord parentX = parentOutline->mOutlineRadius.Get(cx);
      nsStyleCoord parentY = parentOutline->mOutlineRadius.Get(cy);
      nsStyleCoord coordX, coordY;

      if (SetPairCoords(radius, coordX, coordY, parentX, parentY,
                        SETCOORD_LPH | SETCOORD_INITIAL_ZERO |
                          SETCOORD_STORE_CALC | SETCOORD_UNSET_INITIAL,
                        aContext, mPresContext, conditions)) {
        outline->mOutlineRadius.Set(cx, coordX);
        outline->mOutlineRadius.Set(cy, coordY);
      }
    }
  }

  // outline-style: enum, inherit, initial
  // cannot use SetDiscrete because of SetOutlineStyle
  const nsCSSValue* outlineStyleValue = aRuleData->ValueForOutlineStyle();
  nsCSSUnit unit = outlineStyleValue->GetUnit();
  MOZ_ASSERT(eCSSUnit_None != unit && eCSSUnit_Auto != unit,
             "'none' and 'auto' should be handled as enumerated values");
  if (eCSSUnit_Enumerated == unit) {
    outline->SetOutlineStyle(outlineStyleValue->GetIntValue());
  } else if (eCSSUnit_Initial == unit ||
             eCSSUnit_Unset == unit) {
    outline->SetOutlineStyle(NS_STYLE_BORDER_STYLE_NONE);
  } else if (eCSSUnit_Inherit == unit) {
    conditions.SetUncacheable();
    outline->SetOutlineStyle(parentOutline->GetOutlineStyle());
  }

  outline->RecalcData(mPresContext);
  COMPUTE_END_RESET(Outline, outline)
}

const void*
nsRuleNode::ComputeListData(void* aStartStruct,
                            const nsRuleData* aRuleData,
                            nsStyleContext* aContext,
                            nsRuleNode* aHighestNode,
                            const RuleDetail aRuleDetail,
                            const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_INHERITED(List, (mPresContext), list, parentList)

  // list-style-type: string, none, inherit, initial
  const nsCSSValue* typeValue = aRuleData->ValueForListStyleType();
  switch (typeValue->GetUnit()) {
    case eCSSUnit_Unset:
    case eCSSUnit_Inherit: {
      conditions.SetUncacheable();
      nsString type;
      parentList->GetListStyleType(type);
      list->SetListStyleType(type, parentList->GetCounterStyle());
      break;
    }
    case eCSSUnit_Initial:
      list->SetListStyleType(NS_LITERAL_STRING("disc"), mPresContext);
      break;
    case eCSSUnit_Ident: {
      nsString typeIdent;
      typeValue->GetStringValue(typeIdent);
      list->SetListStyleType(typeIdent, mPresContext);
      break;
    }
    case eCSSUnit_String: {
      nsString str;
      typeValue->GetStringValue(str);
      list->SetListStyleType(NS_LITERAL_STRING(""),
                             new AnonymousCounterStyle(str));
      break;
    }
    case eCSSUnit_Enumerated: {
      // For compatibility with html attribute map.
      // This branch should never be called for value from CSS.
      int32_t intValue = typeValue->GetIntValue();
      nsAutoString name;
      switch (intValue) {
        case NS_STYLE_LIST_STYLE_LOWER_ROMAN:
          name.AssignLiteral(MOZ_UTF16("lower-roman"));
          break;
        case NS_STYLE_LIST_STYLE_UPPER_ROMAN:
          name.AssignLiteral(MOZ_UTF16("upper-roman"));
          break;
        case NS_STYLE_LIST_STYLE_LOWER_ALPHA:
          name.AssignLiteral(MOZ_UTF16("lower-alpha"));
          break;
        case NS_STYLE_LIST_STYLE_UPPER_ALPHA:
          name.AssignLiteral(MOZ_UTF16("upper-alpha"));
          break;
        default:
          CopyASCIItoUTF16(nsCSSProps::ValueToKeyword(
                  intValue, nsCSSProps::kListStyleKTable), name);
          break;
      }
      list->SetListStyleType(name, mPresContext);
      break;
    }
    case eCSSUnit_Symbols:
      list->SetListStyleType(
        NS_LITERAL_STRING(""),
        new AnonymousCounterStyle(typeValue->GetArrayValue()));
      break;
    case eCSSUnit_Null:
      break;
    default:
      NS_NOTREACHED("Unexpected value unit");
  }

  // list-style-image: url, none, inherit
  const nsCSSValue* imageValue = aRuleData->ValueForListStyleImage();
  if (eCSSUnit_Image == imageValue->GetUnit()) {
    NS_SET_IMAGE_REQUEST_WITH_DOC(list->SetListStyleImage,
                                  aContext,
                                  imageValue->GetImageValue)
  }
  else if (eCSSUnit_None == imageValue->GetUnit() ||
           eCSSUnit_Initial == imageValue->GetUnit()) {
    list->SetListStyleImage(nullptr);
  }
  else if (eCSSUnit_Inherit == imageValue->GetUnit() ||
           eCSSUnit_Unset == imageValue->GetUnit()) {
    conditions.SetUncacheable();
    NS_SET_IMAGE_REQUEST(list->SetListStyleImage,
                         aContext,
                         parentList->GetListStyleImage())
  }

  // list-style-position: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForListStylePosition(),
              list->mListStylePosition, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentList->mListStylePosition,
              NS_STYLE_LIST_STYLE_POSITION_OUTSIDE, 0, 0, 0, 0);

  // image region property: length, auto, inherit
  const nsCSSValue* imageRegionValue = aRuleData->ValueForImageRegion();
  switch (imageRegionValue->GetUnit()) {
  case eCSSUnit_Inherit:
  case eCSSUnit_Unset:
    conditions.SetUncacheable();
    list->mImageRegion = parentList->mImageRegion;
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_Auto:
    list->mImageRegion.SetRect(0,0,0,0);
    break;

  case eCSSUnit_Null:
    break;

  case eCSSUnit_Rect: {
    const nsCSSRect& rgnRect = imageRegionValue->GetRectValue();

    if (rgnRect.mTop.GetUnit() == eCSSUnit_Auto)
      list->mImageRegion.y = 0;
    else if (rgnRect.mTop.IsLengthUnit())
      list->mImageRegion.y =
        CalcLength(rgnRect.mTop, aContext, mPresContext, conditions);

    if (rgnRect.mBottom.GetUnit() == eCSSUnit_Auto)
      list->mImageRegion.height = 0;
    else if (rgnRect.mBottom.IsLengthUnit())
      list->mImageRegion.height =
        CalcLength(rgnRect.mBottom, aContext, mPresContext,
                   conditions) - list->mImageRegion.y;

    if (rgnRect.mLeft.GetUnit() == eCSSUnit_Auto)
      list->mImageRegion.x = 0;
    else if (rgnRect.mLeft.IsLengthUnit())
      list->mImageRegion.x =
        CalcLength(rgnRect.mLeft, aContext, mPresContext, conditions);

    if (rgnRect.mRight.GetUnit() == eCSSUnit_Auto)
      list->mImageRegion.width = 0;
    else if (rgnRect.mRight.IsLengthUnit())
      list->mImageRegion.width =
        CalcLength(rgnRect.mRight, aContext, mPresContext,
                   conditions) - list->mImageRegion.x;
    break;
  }

  default:
    MOZ_ASSERT(false, "unrecognized image-region unit");
  }

  COMPUTE_END_INHERITED(List, list)
}

static void
SetGridTrackBreadth(const nsCSSValue& aValue,
                    nsStyleCoord& aResult,
                    nsStyleContext* aStyleContext,
                    nsPresContext* aPresContext,
                    RuleNodeCacheConditions& aConditions)
{
  nsCSSUnit unit = aValue.GetUnit();
  if (unit == eCSSUnit_FlexFraction) {
    aResult.SetFlexFractionValue(aValue.GetFloatValue());
  } else if (unit == eCSSUnit_Auto) {
    aResult.SetAutoValue();
  } else {
    MOZ_ASSERT(unit != eCSSUnit_Inherit && unit != eCSSUnit_Unset,
               "Unexpected value that would use dummyParentCoord");
    const nsStyleCoord dummyParentCoord;
    SetCoord(aValue, aResult, dummyParentCoord,
             SETCOORD_LPE | SETCOORD_STORE_CALC,
             aStyleContext, aPresContext, aConditions);
  }
}

static void
SetGridTrackSize(const nsCSSValue& aValue,
                 nsStyleCoord& aResultMin,
                 nsStyleCoord& aResultMax,
                 nsStyleContext* aStyleContext,
                 nsPresContext* aPresContext,
                 RuleNodeCacheConditions& aConditions)
{
  if (aValue.GetUnit() == eCSSUnit_Function) {
    // A minmax() function.
    nsCSSValue::Array* func = aValue.GetArrayValue();
    NS_ASSERTION(func->Item(0).GetKeywordValue() == eCSSKeyword_minmax,
                 "Expected minmax(), got another function name");
    SetGridTrackBreadth(func->Item(1), aResultMin,
                        aStyleContext, aPresContext, aConditions);
    SetGridTrackBreadth(func->Item(2), aResultMax,
                        aStyleContext, aPresContext, aConditions);
  } else {
    // A single <track-breadth>,
    // specifies identical min and max sizing functions.
    SetGridTrackBreadth(aValue, aResultMin,
                        aStyleContext, aPresContext, aConditions);
    aResultMax = aResultMin;
  }
}

static void
SetGridAutoColumnsRows(const nsCSSValue& aValue,
                       nsStyleCoord& aResultMin,
                       nsStyleCoord& aResultMax,
                       const nsStyleCoord& aParentValueMin,
                       const nsStyleCoord& aParentValueMax,
                       nsStyleContext* aStyleContext,
                       nsPresContext* aPresContext,
                       RuleNodeCacheConditions& aConditions)

{
  switch (aValue.GetUnit()) {
  case eCSSUnit_Null:
    break;

  case eCSSUnit_Inherit:
    aConditions.SetUncacheable();
    aResultMin = aParentValueMin;
    aResultMax = aParentValueMax;
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
    // The initial value is 'auto',
    // which computes to 'minmax(auto, auto)'.
    // (Explicitly-specified 'auto' values are handled in SetGridTrackSize.)
    aResultMin.SetAutoValue();
    aResultMax.SetAutoValue();
    break;

  default:
    SetGridTrackSize(aValue, aResultMin, aResultMax,
                     aStyleContext, aPresContext, aConditions);
  }
}

static void
AppendGridLineNames(const nsCSSValue& aValue,
                    nsStyleGridTemplate& aResult)
{
  // Compute a <line-names> value
  nsTArray<nsString>* nameList = aResult.mLineNameLists.AppendElement();
  // Null unit means empty list, nothing more to do.
  if (aValue.GetUnit() != eCSSUnit_Null) {
    const nsCSSValueList* item = aValue.GetListValue();
    do {
      nsString* name = nameList->AppendElement();
      item->mValue.GetStringValue(*name);
      item = item->mNext;
    } while (item);
  }
}

static void
SetGridTrackList(const nsCSSValue& aValue,
                 nsStyleGridTemplate& aResult,
                 const nsStyleGridTemplate& aParentValue,
                 nsStyleContext* aStyleContext,
                 nsPresContext* aPresContext,
                 RuleNodeCacheConditions& aConditions)

{
  switch (aValue.GetUnit()) {
  case eCSSUnit_Null:
    break;

  case eCSSUnit_Inherit:
    aConditions.SetUncacheable();
    aResult.mIsSubgrid = aParentValue.mIsSubgrid;
    aResult.mLineNameLists = aParentValue.mLineNameLists;
    aResult.mMinTrackSizingFunctions = aParentValue.mMinTrackSizingFunctions;
    aResult.mMaxTrackSizingFunctions = aParentValue.mMaxTrackSizingFunctions;
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
  case eCSSUnit_None:
    aResult.mIsSubgrid = false;
    aResult.mLineNameLists.Clear();
    aResult.mMinTrackSizingFunctions.Clear();
    aResult.mMaxTrackSizingFunctions.Clear();
    break;

  default:
    aResult.mLineNameLists.Clear();
    aResult.mMinTrackSizingFunctions.Clear();
    aResult.mMaxTrackSizingFunctions.Clear();
    const nsCSSValueList* item = aValue.GetListValue();
    if (item->mValue.GetUnit() == eCSSUnit_Enumerated &&
        item->mValue.GetIntValue() == NS_STYLE_GRID_TEMPLATE_SUBGRID) {
      // subgrid <line-name-list>?
      aResult.mIsSubgrid = true;
      item = item->mNext;
      while (item) {
        AppendGridLineNames(item->mValue, aResult);
        item = item->mNext;
      }
    } else {
      // <track-list>
      // The list is expected to have odd number of items, at least 3
      // starting with a <line-names> (sub list of identifiers),
      // and alternating between that and <track-size>.
      aResult.mIsSubgrid = false;
      for (;;) {
        AppendGridLineNames(item->mValue, aResult);
        item = item->mNext;

        if (!item) {
          break;
        }

        nsStyleCoord& min = *aResult.mMinTrackSizingFunctions.AppendElement();
        nsStyleCoord& max = *aResult.mMaxTrackSizingFunctions.AppendElement();
        SetGridTrackSize(item->mValue, min, max,
                         aStyleContext, aPresContext, aConditions);

        item = item->mNext;
        MOZ_ASSERT(item, "Expected a eCSSUnit_List of odd length");
      }
      MOZ_ASSERT(!aResult.mMinTrackSizingFunctions.IsEmpty() &&
                 aResult.mMinTrackSizingFunctions.Length() ==
                 aResult.mMaxTrackSizingFunctions.Length() &&
                 aResult.mMinTrackSizingFunctions.Length() + 1 ==
                 aResult.mLineNameLists.Length(),
                 "Inconstistent array lengths for nsStyleGridTemplate");
    }
  }
}

static void
SetGridTemplateAreas(const nsCSSValue& aValue,
                     nsRefPtr<css::GridTemplateAreasValue>* aResult,
                     css::GridTemplateAreasValue* aParentValue,
                     RuleNodeCacheConditions& aConditions)
{
  switch (aValue.GetUnit()) {
  case eCSSUnit_Null:
    break;

  case eCSSUnit_Inherit:
    aConditions.SetUncacheable();
    *aResult = aParentValue;
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
  case eCSSUnit_None:
    *aResult = nullptr;
    break;

  default:
    *aResult = aValue.GetGridTemplateAreas();
  }
}

static void
SetGridLine(const nsCSSValue& aValue,
            nsStyleGridLine& aResult,
            const nsStyleGridLine& aParentValue,
            RuleNodeCacheConditions& aConditions)

{
  switch (aValue.GetUnit()) {
  case eCSSUnit_Null:
    break;

  case eCSSUnit_Inherit:
    aConditions.SetUncacheable();
    aResult = aParentValue;
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
  case eCSSUnit_Auto:
    aResult.SetAuto();
    break;

  default:
    aResult.SetAuto();  // Reset any existing value.
    const nsCSSValueList* item = aValue.GetListValue();
    do {
      if (item->mValue.GetUnit() == eCSSUnit_Enumerated) {
        aResult.mHasSpan = true;
      } else if (item->mValue.GetUnit() == eCSSUnit_Integer) {
        aResult.mInteger = clamped(item->mValue.GetIntValue(),
                                   nsStyleGridLine::kMinLine,
                                   nsStyleGridLine::kMaxLine);
      } else if (item->mValue.GetUnit() == eCSSUnit_Ident) {
        item->mValue.GetStringValue(aResult.mLineName);
      } else {
        NS_ASSERTION(false, "Unexpected unit");
      }
      item = item->mNext;
    } while (item);
    MOZ_ASSERT(!aResult.IsAuto(),
               "should have set something away from default value");
  }
}

const void*
nsRuleNode::ComputePositionData(void* aStartStruct,
                                const nsRuleData* aRuleData,
                                nsStyleContext* aContext,
                                nsRuleNode* aHighestNode,
                                const RuleDetail aRuleDetail,
                                const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(Position, (), pos, parentPos)

  // box offsets: length, percent, calc, auto, inherit
  static const nsCSSProperty offsetProps[] = {
    eCSSProperty_top,
    eCSSProperty_right,
    eCSSProperty_bottom,
    eCSSProperty_left
  };
  nsStyleCoord  coord;
  NS_FOR_CSS_SIDES(side) {
    nsStyleCoord parentCoord = parentPos->mOffset.Get(side);
    if (SetCoord(*aRuleData->ValueFor(offsetProps[side]),
                 coord, parentCoord,
                 SETCOORD_LPAH | SETCOORD_INITIAL_AUTO | SETCOORD_STORE_CALC |
                   SETCOORD_UNSET_INITIAL,
                 aContext, mPresContext, conditions)) {
      pos->mOffset.Set(side, coord);
    }
  }

  // We allow the enumerated box size property values -moz-min-content, etc. to
  // be specified on both the {,min-,max-}width properties and the
  // {,min-,max-}height properties, regardless of the writing mode.  This is
  // because the writing mode is not determined until here, at computed value
  // time.  Since we do not support layout behavior of these keywords on the
  // block-axis properties, we turn them into unset if we find them in
  // that case.

  bool vertical;
  switch (aContext->StyleVisibility()->mWritingMode) {
    default:
      MOZ_ASSERT(false, "unexpected writing-mode value");
      // fall through
    case NS_STYLE_WRITING_MODE_HORIZONTAL_TB:
      vertical = false;
      break;
    case NS_STYLE_WRITING_MODE_VERTICAL_RL:
    case NS_STYLE_WRITING_MODE_VERTICAL_LR:
      vertical = true;
      break;
  }

  const nsCSSValue* width = aRuleData->ValueForWidth();
  SetCoord(width->GetUnit() == eCSSUnit_Enumerated && vertical ?
             nsCSSValue(eCSSUnit_Unset) : *width,
           pos->mWidth, parentPos->mWidth,
           SETCOORD_LPAEH | SETCOORD_INITIAL_AUTO | SETCOORD_STORE_CALC |
             SETCOORD_UNSET_INITIAL,
           aContext, mPresContext, conditions);

  const nsCSSValue* minWidth = aRuleData->ValueForMinWidth();
  SetCoord(minWidth->GetUnit() == eCSSUnit_Enumerated && vertical ?
             nsCSSValue(eCSSUnit_Unset) : *minWidth,
           pos->mMinWidth, parentPos->mMinWidth,
           SETCOORD_LPAEH | SETCOORD_INITIAL_AUTO | SETCOORD_STORE_CALC |
             SETCOORD_UNSET_INITIAL,
           aContext, mPresContext, conditions);

  const nsCSSValue* maxWidth = aRuleData->ValueForMaxWidth();
  SetCoord(maxWidth->GetUnit() == eCSSUnit_Enumerated && vertical ?
             nsCSSValue(eCSSUnit_Unset) : *maxWidth,
           pos->mMaxWidth, parentPos->mMaxWidth,
           SETCOORD_LPOEH | SETCOORD_INITIAL_NONE | SETCOORD_STORE_CALC |
             SETCOORD_UNSET_INITIAL,
           aContext, mPresContext, conditions);

  const nsCSSValue* height = aRuleData->ValueForHeight();
  SetCoord(height->GetUnit() == eCSSUnit_Enumerated && !vertical ?
             nsCSSValue(eCSSUnit_Unset) : *height,
           pos->mHeight, parentPos->mHeight,
           SETCOORD_LPAEH | SETCOORD_INITIAL_AUTO | SETCOORD_STORE_CALC |
             SETCOORD_UNSET_INITIAL,
           aContext, mPresContext, conditions);

  const nsCSSValue* minHeight = aRuleData->ValueForMinHeight();
  SetCoord(minHeight->GetUnit() == eCSSUnit_Enumerated && !vertical ?
             nsCSSValue(eCSSUnit_Unset) : *minHeight,
           pos->mMinHeight, parentPos->mMinHeight,
           SETCOORD_LPAEH | SETCOORD_INITIAL_AUTO | SETCOORD_STORE_CALC |
             SETCOORD_UNSET_INITIAL,
           aContext, mPresContext, conditions);

  const nsCSSValue* maxHeight = aRuleData->ValueForMaxHeight();
  SetCoord(maxHeight->GetUnit() == eCSSUnit_Enumerated && !vertical ?
             nsCSSValue(eCSSUnit_Unset) : *maxHeight,
           pos->mMaxHeight, parentPos->mMaxHeight,
           SETCOORD_LPOEH | SETCOORD_INITIAL_NONE | SETCOORD_STORE_CALC |
             SETCOORD_UNSET_INITIAL,
           aContext, mPresContext, conditions);

  // box-sizing: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForBoxSizing(),
              pos->mBoxSizing, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentPos->mBoxSizing,
              NS_STYLE_BOX_SIZING_CONTENT, 0, 0, 0, 0);

  // align-content: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForAlignContent(),
              pos->mAlignContent, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentPos->mAlignContent,
              NS_STYLE_ALIGN_CONTENT_STRETCH, 0, 0, 0, 0);

  // align-items: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForAlignItems(),
              pos->mAlignItems, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentPos->mAlignItems,
              NS_STYLE_ALIGN_ITEMS_INITIAL_VALUE, 0, 0, 0, 0);

  // align-self: enum, inherit, initial
  // NOTE: align-self's initial value is the special keyword "auto", which is
  // supposed to compute to our parent's computed value of "align-items".  So
  // technically, "auto" itself is never a valid computed value for align-self,
  // since it always computes to something else.  Despite that, we do actually
  // store "auto" in nsStylePosition::mAlignSelf, as NS_STYLE_ALIGN_SELF_AUTO
  // (and then resolve it as-necessary).  We do this because "auto" is the
  // initial value for this property, so if we were to actually resolve it in
  // nsStylePosition, we'd never be able to share any nsStylePosition structs
  // in the rule tree, since their mAlignSelf values would depend on the parent
  // style, by default.
  if (aRuleData->ValueForAlignSelf()->GetUnit() == eCSSUnit_Inherit) {
    // Special handling for "align-self: inherit", in case we're inheriting
    // "align-self: auto", in which case we need to resolve the parent's "auto"
    // and inherit that resolved value.
    uint8_t inheritedAlignSelf = parentPos->mAlignSelf;
    if (inheritedAlignSelf == NS_STYLE_ALIGN_SELF_AUTO) {
      if (!parentContext) {
        // We're the root node. Nothing to inherit from --> just use default
        // value.
        inheritedAlignSelf = NS_STYLE_ALIGN_ITEMS_INITIAL_VALUE;
      } else {
        // Our parent's "auto" value should resolve to our grandparent's value
        // for "align-items".  So, that's what we're supposed to inherit.
        nsStyleContext* grandparentContext = parentContext->GetParent();
        if (!grandparentContext) {
          // No grandparent --> our parent is the root node, so its
          // "align-self: auto" computes to the default "align-items" value:
          inheritedAlignSelf = NS_STYLE_ALIGN_ITEMS_INITIAL_VALUE;
        } else {
          // Normal case -- we have a grandparent.
          // Its "align-items" value is what we should end up inheriting.
          const nsStylePosition* grandparentPos =
            grandparentContext->StylePosition();
          inheritedAlignSelf = grandparentPos->mAlignItems;
          aContext->AddStyleBit(NS_STYLE_USES_GRANDANCESTOR_STYLE);
        }
      }
    }

    pos->mAlignSelf = inheritedAlignSelf;
    conditions.SetUncacheable();
  } else {
    SetDiscrete(*aRuleData->ValueForAlignSelf(),
                pos->mAlignSelf, conditions,
                SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
                parentPos->mAlignSelf, // (unused -- we handled inherit above)
                NS_STYLE_ALIGN_SELF_AUTO, // initial == auto
                0, 0, 0, 0);
  }

  // flex-basis: auto, length, percent, enum, calc, inherit, initial
  // (Note: The flags here should match those used for 'width' property above.)
  SetCoord(*aRuleData->ValueForFlexBasis(), pos->mFlexBasis, parentPos->mFlexBasis,
           SETCOORD_LPAEH | SETCOORD_INITIAL_AUTO | SETCOORD_STORE_CALC |
             SETCOORD_UNSET_INITIAL,
           aContext, mPresContext, conditions);

  // flex-direction: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForFlexDirection(),
              pos->mFlexDirection, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentPos->mFlexDirection,
              NS_STYLE_FLEX_DIRECTION_ROW, 0, 0, 0, 0);

  // flex-grow: float, inherit, initial
  SetFactor(*aRuleData->ValueForFlexGrow(),
            pos->mFlexGrow, conditions,
            parentPos->mFlexGrow, 0.0f,
            SETFCT_UNSET_INITIAL);

  // flex-shrink: float, inherit, initial
  SetFactor(*aRuleData->ValueForFlexShrink(),
            pos->mFlexShrink, conditions,
            parentPos->mFlexShrink, 1.0f,
            SETFCT_UNSET_INITIAL);

  // flex-wrap: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForFlexWrap(),
              pos->mFlexWrap, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentPos->mFlexWrap,
              NS_STYLE_FLEX_WRAP_NOWRAP, 0, 0, 0, 0);

  // order: integer, inherit, initial
  SetDiscrete(*aRuleData->ValueForOrder(),
              pos->mOrder, conditions,
              SETDSC_INTEGER | SETDSC_UNSET_INITIAL,
              parentPos->mOrder,
              NS_STYLE_ORDER_INITIAL, 0, 0, 0, 0);

  // justify-content: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForJustifyContent(),
              pos->mJustifyContent, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentPos->mJustifyContent,
              NS_STYLE_JUSTIFY_CONTENT_FLEX_START, 0, 0, 0, 0);

  // object-fit: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForObjectFit(),
              pos->mObjectFit, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentPos->mObjectFit,
              NS_STYLE_OBJECT_FIT_FILL, 0, 0, 0, 0);

  // object-position
  const nsCSSValue& objectPosition = *aRuleData->ValueForObjectPosition();
  switch (objectPosition.GetUnit()) {
    case eCSSUnit_Null:
      break;
    case eCSSUnit_Inherit:
      conditions.SetUncacheable();
      pos->mObjectPosition = parentPos->mObjectPosition;
      break;
    case eCSSUnit_Initial:
    case eCSSUnit_Unset:
      pos->mObjectPosition.SetInitialPercentValues(0.5f);
      break;
    default:
      ComputePositionValue(aContext, objectPosition,
                           pos->mObjectPosition, conditions);
  }

  // grid-auto-flow
  const nsCSSValue& gridAutoFlow = *aRuleData->ValueForGridAutoFlow();
  switch (gridAutoFlow.GetUnit()) {
    case eCSSUnit_Null:
      break;
    case eCSSUnit_Inherit:
      conditions.SetUncacheable();
      pos->mGridAutoFlow = parentPos->mGridAutoFlow;
      break;
    case eCSSUnit_Initial:
    case eCSSUnit_Unset:
      pos->mGridAutoFlow = NS_STYLE_GRID_AUTO_FLOW_ROW;
      break;
    default:
      NS_ASSERTION(gridAutoFlow.GetUnit() == eCSSUnit_Enumerated,
                   "Unexpected unit");
      pos->mGridAutoFlow = gridAutoFlow.GetIntValue();
  }

  // grid-auto-columns
  SetGridAutoColumnsRows(*aRuleData->ValueForGridAutoColumns(),
                         pos->mGridAutoColumnsMin,
                         pos->mGridAutoColumnsMax,
                         parentPos->mGridAutoColumnsMin,
                         parentPos->mGridAutoColumnsMax,
                         aContext, mPresContext, conditions);

  // grid-auto-rows
  SetGridAutoColumnsRows(*aRuleData->ValueForGridAutoRows(),
                         pos->mGridAutoRowsMin,
                         pos->mGridAutoRowsMax,
                         parentPos->mGridAutoRowsMin,
                         parentPos->mGridAutoRowsMax,
                         aContext, mPresContext, conditions);

  // grid-template-columns
  SetGridTrackList(*aRuleData->ValueForGridTemplateColumns(),
                   pos->mGridTemplateColumns, parentPos->mGridTemplateColumns,
                   aContext, mPresContext, conditions);

  // grid-template-rows
  SetGridTrackList(*aRuleData->ValueForGridTemplateRows(),
                   pos->mGridTemplateRows, parentPos->mGridTemplateRows,
                   aContext, mPresContext, conditions);

  // grid-tempate-areas
  SetGridTemplateAreas(*aRuleData->ValueForGridTemplateAreas(),
                       &pos->mGridTemplateAreas,
                       parentPos->mGridTemplateAreas,
                       conditions);

  // grid-column-start
  SetGridLine(*aRuleData->ValueForGridColumnStart(),
              pos->mGridColumnStart,
              parentPos->mGridColumnStart,
              conditions);

  // grid-column-end
  SetGridLine(*aRuleData->ValueForGridColumnEnd(),
              pos->mGridColumnEnd,
              parentPos->mGridColumnEnd,
              conditions);

  // grid-row-start
  SetGridLine(*aRuleData->ValueForGridRowStart(),
              pos->mGridRowStart,
              parentPos->mGridRowStart,
              conditions);

  // grid-row-end
  SetGridLine(*aRuleData->ValueForGridRowEnd(),
              pos->mGridRowEnd,
              parentPos->mGridRowEnd,
              conditions);

  // z-index
  const nsCSSValue* zIndexValue = aRuleData->ValueForZIndex();
  if (! SetCoord(*zIndexValue, pos->mZIndex, parentPos->mZIndex,
                 SETCOORD_IA | SETCOORD_INITIAL_AUTO | SETCOORD_UNSET_INITIAL,
                 aContext, nullptr, conditions)) {
    if (eCSSUnit_Inherit == zIndexValue->GetUnit()) {
      // handle inherit, because it's ok to inherit 'auto' here
      conditions.SetUncacheable();
      pos->mZIndex = parentPos->mZIndex;
    }
  }

  COMPUTE_END_RESET(Position, pos)
}

const void*
nsRuleNode::ComputeTableData(void* aStartStruct,
                             const nsRuleData* aRuleData,
                             nsStyleContext* aContext,
                             nsRuleNode* aHighestNode,
                             const RuleDetail aRuleDetail,
                             const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(Table, (), table, parentTable)

  // table-layout: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForTableLayout(),
              table->mLayoutStrategy, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentTable->mLayoutStrategy,
              NS_STYLE_TABLE_LAYOUT_AUTO, 0, 0, 0, 0);

  // span: pixels (not a real CSS prop)
  const nsCSSValue* spanValue = aRuleData->ValueForSpan();
  if (eCSSUnit_Enumerated == spanValue->GetUnit() ||
      eCSSUnit_Integer == spanValue->GetUnit())
    table->mSpan = spanValue->GetIntValue();

  COMPUTE_END_RESET(Table, table)
}

const void*
nsRuleNode::ComputeTableBorderData(void* aStartStruct,
                                   const nsRuleData* aRuleData,
                                   nsStyleContext* aContext,
                                   nsRuleNode* aHighestNode,
                                   const RuleDetail aRuleDetail,
                                   const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_INHERITED(TableBorder, (), table, parentTable)

  // border-collapse: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForBorderCollapse(), table->mBorderCollapse,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentTable->mBorderCollapse,
              NS_STYLE_BORDER_SEPARATE, 0, 0, 0, 0);

  const nsCSSValue* borderSpacingValue = aRuleData->ValueForBorderSpacing();
  // border-spacing: pair(length), inherit
  if (borderSpacingValue->GetUnit() != eCSSUnit_Null) {
    nsStyleCoord parentCol(parentTable->mBorderSpacingCol,
                           nsStyleCoord::CoordConstructor);
    nsStyleCoord parentRow(parentTable->mBorderSpacingRow,
                           nsStyleCoord::CoordConstructor);
    nsStyleCoord coordCol, coordRow;

#ifdef DEBUG
    bool result =
#endif
      SetPairCoords(*borderSpacingValue,
                    coordCol, coordRow, parentCol, parentRow,
                    SETCOORD_LH | SETCOORD_INITIAL_ZERO |
                      SETCOORD_CALC_LENGTH_ONLY |
                      SETCOORD_CALC_CLAMP_NONNEGATIVE | SETCOORD_UNSET_INHERIT,
                    aContext, mPresContext, conditions);
    NS_ASSERTION(result, "malformed table border value");
    table->mBorderSpacingCol = coordCol.GetCoordValue();
    table->mBorderSpacingRow = coordRow.GetCoordValue();
  }

  // caption-side: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForCaptionSide(),
              table->mCaptionSide, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentTable->mCaptionSide,
              NS_STYLE_CAPTION_SIDE_BSTART, 0, 0, 0, 0);

  // empty-cells: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForEmptyCells(),
              table->mEmptyCells, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentTable->mEmptyCells,
              NS_STYLE_TABLE_EMPTY_CELLS_SHOW,
              0, 0, 0, 0);

  COMPUTE_END_INHERITED(TableBorder, table)
}

const void*
nsRuleNode::ComputeContentData(void* aStartStruct,
                               const nsRuleData* aRuleData,
                               nsStyleContext* aContext,
                               nsRuleNode* aHighestNode,
                               const RuleDetail aRuleDetail,
                               const RuleNodeCacheConditions aConditions)
{
  uint32_t count;
  nsAutoString buffer;

  COMPUTE_START_RESET(Content, (), content, parentContent)

  // content: [string, url, counter, attr, enum]+, normal, none, inherit
  const nsCSSValue* contentValue = aRuleData->ValueForContent();
  switch (contentValue->GetUnit()) {
  case eCSSUnit_Null:
    break;

  case eCSSUnit_Normal:
  case eCSSUnit_None:
  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
    // "normal", "none", "initial" and "unset" all mean no content
    content->AllocateContents(0);
    break;

  case eCSSUnit_Inherit:
    conditions.SetUncacheable();
    count = parentContent->ContentCount();
    if (NS_SUCCEEDED(content->AllocateContents(count))) {
      while (0 < count--) {
        content->ContentAt(count) = parentContent->ContentAt(count);
      }
    }
    break;

  case eCSSUnit_Enumerated: {
    MOZ_ASSERT(contentValue->GetIntValue() == NS_STYLE_CONTENT_ALT_CONTENT,
               "unrecognized solitary content keyword");
    content->AllocateContents(1);
    nsStyleContentData& data = content->ContentAt(0);
    data.mType = eStyleContentType_AltContent;
    data.mContent.mString = nullptr;
    break;
  }

  case eCSSUnit_List:
  case eCSSUnit_ListDep: {
    const nsCSSValueList* contentValueList = contentValue->GetListValue();
      count = 0;
      while (contentValueList) {
        count++;
        contentValueList = contentValueList->mNext;
      }
      if (NS_SUCCEEDED(content->AllocateContents(count))) {
        const nsAutoString  nullStr;
        count = 0;
        contentValueList = contentValue->GetListValue();
        while (contentValueList) {
          const nsCSSValue& value = contentValueList->mValue;
          nsCSSUnit unit = value.GetUnit();
          nsStyleContentType type;
          nsStyleContentData &data = content->ContentAt(count++);
          switch (unit) {
          case eCSSUnit_String:   type = eStyleContentType_String;    break;
          case eCSSUnit_Image:    type = eStyleContentType_Image;     break;
          case eCSSUnit_Attr:     type = eStyleContentType_Attr;      break;
          case eCSSUnit_Counter:  type = eStyleContentType_Counter;   break;
          case eCSSUnit_Counters: type = eStyleContentType_Counters;  break;
          case eCSSUnit_Enumerated:
            switch (value.GetIntValue()) {
            case NS_STYLE_CONTENT_OPEN_QUOTE:
              type = eStyleContentType_OpenQuote;     break;
            case NS_STYLE_CONTENT_CLOSE_QUOTE:
              type = eStyleContentType_CloseQuote;    break;
            case NS_STYLE_CONTENT_NO_OPEN_QUOTE:
              type = eStyleContentType_NoOpenQuote;   break;
            case NS_STYLE_CONTENT_NO_CLOSE_QUOTE:
              type = eStyleContentType_NoCloseQuote;  break;
            default:
              NS_ERROR("bad content value");
              type = eStyleContentType_Uninitialized;
            }
            break;
          default:
            NS_ERROR("bad content type");
            type = eStyleContentType_Uninitialized;
          }
          data.mType = type;
          if (type == eStyleContentType_Image) {
            NS_SET_IMAGE_REQUEST_WITH_DOC(data.SetImage,
                                          aContext,
                                          value.GetImageValue);
          }
          else if (type <= eStyleContentType_Attr) {
            value.GetStringValue(buffer);
            data.mContent.mString = NS_strdup(buffer.get());
          }
          else if (type <= eStyleContentType_Counters) {
            data.mContent.mCounters = value.GetArrayValue();
            data.mContent.mCounters->AddRef();
          }
          else {
            data.mContent.mString = nullptr;
          }
          contentValueList = contentValueList->mNext;
        }
      }
      break;
  }

  default:
    MOZ_ASSERT(false, "unrecognized content unit");
  }

  // counter-increment: [string [int]]+, none, inherit
  const nsCSSValue* counterIncrementValue =
    aRuleData->ValueForCounterIncrement();
  switch (counterIncrementValue->GetUnit()) {
  case eCSSUnit_Null:
    break;

  case eCSSUnit_None:
  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
    content->AllocateCounterIncrements(0);
    break;

  case eCSSUnit_Inherit:
    conditions.SetUncacheable();
    count = parentContent->CounterIncrementCount();
    if (NS_SUCCEEDED(content->AllocateCounterIncrements(count))) {
      while (0 < count--) {
        const nsStyleCounterData *data =
          parentContent->GetCounterIncrementAt(count);
        content->SetCounterIncrementAt(count, data->mCounter, data->mValue);
      }
    }
    break;

  case eCSSUnit_PairList:
  case eCSSUnit_PairListDep: {
    const nsCSSValuePairList* ourIncrement =
      counterIncrementValue->GetPairListValue();
    MOZ_ASSERT(ourIncrement->mXValue.GetUnit() == eCSSUnit_Ident,
               "unexpected value unit");
    count = ListLength(ourIncrement);
    if (NS_FAILED(content->AllocateCounterIncrements(count))) {
      break;
    }

    count = 0;
    for (const nsCSSValuePairList* p = ourIncrement; p; p = p->mNext, count++) {
      int32_t increment;
      if (p->mYValue.GetUnit() == eCSSUnit_Integer) {
        increment = p->mYValue.GetIntValue();
      } else {
        increment = 1;
      }
      p->mXValue.GetStringValue(buffer);
      content->SetCounterIncrementAt(count, buffer, increment);
    }
    break;
  }

  default:
    MOZ_ASSERT(false, "unexpected value unit");
  }

  // counter-reset: [string [int]]+, none, inherit
  const nsCSSValue* counterResetValue = aRuleData->ValueForCounterReset();
  switch (counterResetValue->GetUnit()) {
  case eCSSUnit_Null:
    break;

  case eCSSUnit_None:
  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
    content->AllocateCounterResets(0);
    break;

  case eCSSUnit_Inherit:
    conditions.SetUncacheable();
    count = parentContent->CounterResetCount();
    if (NS_SUCCEEDED(content->AllocateCounterResets(count))) {
      while (0 < count--) {
        const nsStyleCounterData *data =
          parentContent->GetCounterResetAt(count);
        content->SetCounterResetAt(count, data->mCounter, data->mValue);
      }
    }
    break;

  case eCSSUnit_PairList:
  case eCSSUnit_PairListDep: {
    const nsCSSValuePairList* ourReset =
      counterResetValue->GetPairListValue();
    MOZ_ASSERT(ourReset->mXValue.GetUnit() == eCSSUnit_Ident,
               "unexpected value unit");
    count = ListLength(ourReset);
    if (NS_FAILED(content->AllocateCounterResets(count))) {
      break;
    }

    count = 0;
    for (const nsCSSValuePairList* p = ourReset; p; p = p->mNext, count++) {
      int32_t reset;
      if (p->mYValue.GetUnit() == eCSSUnit_Integer) {
        reset = p->mYValue.GetIntValue();
      } else {
        reset = 0;
      }
      p->mXValue.GetStringValue(buffer);
      content->SetCounterResetAt(count, buffer, reset);
    }
    break;
  }

  default:
    MOZ_ASSERT(false, "unexpected value unit");
  }

  // marker-offset: length, auto, inherit
  SetCoord(*aRuleData->ValueForMarkerOffset(), content->mMarkerOffset, parentContent->mMarkerOffset,
           SETCOORD_LH | SETCOORD_AUTO | SETCOORD_INITIAL_AUTO |
             SETCOORD_CALC_LENGTH_ONLY | SETCOORD_UNSET_INITIAL,
           aContext, mPresContext, conditions);

  // If we ended up with an image, track it.
  for (uint32_t i = 0; i < content->ContentCount(); ++i) {
    if ((content->ContentAt(i).mType == eStyleContentType_Image) &&
        content->ContentAt(i).mContent.mImage) {
      content->ContentAt(i).TrackImage(aContext->PresContext());
    }
  }

  COMPUTE_END_RESET(Content, content)
}

const void*
nsRuleNode::ComputeQuotesData(void* aStartStruct,
                              const nsRuleData* aRuleData,
                              nsStyleContext* aContext,
                              nsRuleNode* aHighestNode,
                              const RuleDetail aRuleDetail,
                              const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_INHERITED(Quotes, (), quotes, parentQuotes)

  // quotes: inherit, initial, none, [string string]+
  const nsCSSValue* quotesValue = aRuleData->ValueForQuotes();
  switch (quotesValue->GetUnit()) {
  case eCSSUnit_Null:
    break;
  case eCSSUnit_Inherit:
  case eCSSUnit_Unset:
    conditions.SetUncacheable();
    quotes->CopyFrom(*parentQuotes);
    break;
  case eCSSUnit_Initial:
    quotes->SetInitial();
    break;
  case eCSSUnit_None:
    quotes->AllocateQuotes(0);
    break;
  case eCSSUnit_PairList:
  case eCSSUnit_PairListDep: {
    const nsCSSValuePairList* ourQuotes
      = quotesValue->GetPairListValue();
    nsAutoString buffer;
    nsAutoString closeBuffer;
    uint32_t count = ListLength(ourQuotes);
    if (NS_FAILED(quotes->AllocateQuotes(count))) {
      break;
    }
    count = 0;
    while (ourQuotes) {
      MOZ_ASSERT(ourQuotes->mXValue.GetUnit() == eCSSUnit_String &&
                 ourQuotes->mYValue.GetUnit() == eCSSUnit_String,
                 "improper list contents for quotes");
      ourQuotes->mXValue.GetStringValue(buffer);
      ourQuotes->mYValue.GetStringValue(closeBuffer);
      quotes->SetQuotesAt(count++, buffer, closeBuffer);
      ourQuotes = ourQuotes->mNext;
    }
    break;
  }
  default:
    MOZ_ASSERT(false, "unexpected value unit");
  }

  COMPUTE_END_INHERITED(Quotes, quotes)
}

const void*
nsRuleNode::ComputeXULData(void* aStartStruct,
                           const nsRuleData* aRuleData,
                           nsStyleContext* aContext,
                           nsRuleNode* aHighestNode,
                           const RuleDetail aRuleDetail,
                           const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(XUL, (), xul, parentXUL)

  // box-align: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForBoxAlign(),
              xul->mBoxAlign, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentXUL->mBoxAlign,
              NS_STYLE_BOX_ALIGN_STRETCH, 0, 0, 0, 0);

  // box-direction: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForBoxDirection(),
              xul->mBoxDirection, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentXUL->mBoxDirection,
              NS_STYLE_BOX_DIRECTION_NORMAL, 0, 0, 0, 0);

  // box-flex: factor, inherit
  SetFactor(*aRuleData->ValueForBoxFlex(),
            xul->mBoxFlex, conditions,
            parentXUL->mBoxFlex, 0.0f,
            SETFCT_UNSET_INITIAL);

  // box-orient: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForBoxOrient(),
              xul->mBoxOrient, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentXUL->mBoxOrient,
              NS_STYLE_BOX_ORIENT_HORIZONTAL, 0, 0, 0, 0);

  // box-pack: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForBoxPack(),
              xul->mBoxPack, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentXUL->mBoxPack,
              NS_STYLE_BOX_PACK_START, 0, 0, 0, 0);

  // box-ordinal-group: integer, inherit, initial
  SetDiscrete(*aRuleData->ValueForBoxOrdinalGroup(),
              xul->mBoxOrdinal, conditions,
              SETDSC_INTEGER | SETDSC_UNSET_INITIAL,
              parentXUL->mBoxOrdinal, 1,
              0, 0, 0, 0);

  const nsCSSValue* stackSizingValue = aRuleData->ValueForStackSizing();
  if (eCSSUnit_Inherit == stackSizingValue->GetUnit()) {
    conditions.SetUncacheable();
    xul->mStretchStack = parentXUL->mStretchStack;
  } else if (eCSSUnit_Initial == stackSizingValue->GetUnit() ||
             eCSSUnit_Unset == stackSizingValue->GetUnit()) {
    xul->mStretchStack = true;
  } else if (eCSSUnit_Enumerated == stackSizingValue->GetUnit()) {
    xul->mStretchStack = stackSizingValue->GetIntValue() ==
      NS_STYLE_STACK_SIZING_STRETCH_TO_FIT;
  }

  COMPUTE_END_RESET(XUL, xul)
}

const void*
nsRuleNode::ComputeColumnData(void* aStartStruct,
                              const nsRuleData* aRuleData,
                              nsStyleContext* aContext,
                              nsRuleNode* aHighestNode,
                              const RuleDetail aRuleDetail,
                              const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(Column, (mPresContext), column, parent)

  // column-width: length, auto, inherit
  SetCoord(*aRuleData->ValueForColumnWidth(),
           column->mColumnWidth, parent->mColumnWidth,
           SETCOORD_LAH | SETCOORD_INITIAL_AUTO |
             SETCOORD_CALC_LENGTH_ONLY | SETCOORD_CALC_CLAMP_NONNEGATIVE |
             SETCOORD_UNSET_INITIAL,
           aContext, mPresContext, conditions);

  // column-gap: length, inherit, normal
  SetCoord(*aRuleData->ValueForColumnGap(),
           column->mColumnGap, parent->mColumnGap,
           SETCOORD_LH | SETCOORD_NORMAL | SETCOORD_INITIAL_NORMAL |
             SETCOORD_CALC_LENGTH_ONLY | SETCOORD_UNSET_INITIAL,
           aContext, mPresContext, conditions);
  // clamp negative calc() to 0
  if (column->mColumnGap.GetUnit() == eStyleUnit_Coord) {
    column->mColumnGap.SetCoordValue(
      std::max(column->mColumnGap.GetCoordValue(), 0));
  }

  // column-count: auto, integer, inherit
  const nsCSSValue* columnCountValue = aRuleData->ValueForColumnCount();
  if (eCSSUnit_Auto == columnCountValue->GetUnit() ||
      eCSSUnit_Initial == columnCountValue->GetUnit() ||
      eCSSUnit_Unset == columnCountValue->GetUnit()) {
    column->mColumnCount = NS_STYLE_COLUMN_COUNT_AUTO;
  } else if (eCSSUnit_Integer == columnCountValue->GetUnit()) {
    column->mColumnCount = columnCountValue->GetIntValue();
    // Max kMaxColumnCount columns - wallpaper for bug 345583.
    column->mColumnCount = std::min(column->mColumnCount,
                                    nsStyleColumn::kMaxColumnCount);
  } else if (eCSSUnit_Inherit == columnCountValue->GetUnit()) {
    conditions.SetUncacheable();
    column->mColumnCount = parent->mColumnCount;
  }

  // column-rule-width: length, enum, inherit
  const nsCSSValue& widthValue = *aRuleData->ValueForColumnRuleWidth();
  if (eCSSUnit_Initial == widthValue.GetUnit() ||
      eCSSUnit_Unset == widthValue.GetUnit()) {
    column->SetColumnRuleWidth(
        (mPresContext->GetBorderWidthTable())[NS_STYLE_BORDER_WIDTH_MEDIUM]);
  }
  else if (eCSSUnit_Enumerated == widthValue.GetUnit()) {
    NS_ASSERTION(widthValue.GetIntValue() == NS_STYLE_BORDER_WIDTH_THIN ||
                 widthValue.GetIntValue() == NS_STYLE_BORDER_WIDTH_MEDIUM ||
                 widthValue.GetIntValue() == NS_STYLE_BORDER_WIDTH_THICK,
                 "Unexpected enum value");
    column->SetColumnRuleWidth(
        (mPresContext->GetBorderWidthTable())[widthValue.GetIntValue()]);
  }
  else if (eCSSUnit_Inherit == widthValue.GetUnit()) {
    column->SetColumnRuleWidth(parent->GetComputedColumnRuleWidth());
    conditions.SetUncacheable();
  }
  else if (widthValue.IsLengthUnit() || widthValue.IsCalcUnit()) {
    nscoord len =
      CalcLength(widthValue, aContext, mPresContext, conditions);
    if (len < 0) {
      // FIXME: This is untested (by test_value_storage.html) for
      // column-rule-width since it gets covered up by the border
      // rounding code.
      NS_ASSERTION(widthValue.IsCalcUnit(),
                   "parser should have rejected negative length");
      len = 0;
    }
    column->SetColumnRuleWidth(len);
  }

  // column-rule-style: enum, inherit
  const nsCSSValue& styleValue = *aRuleData->ValueForColumnRuleStyle();
  MOZ_ASSERT(eCSSUnit_None != styleValue.GetUnit(),
             "'none' should be handled as enumerated value");
  if (eCSSUnit_Enumerated == styleValue.GetUnit()) {
    column->mColumnRuleStyle = styleValue.GetIntValue();
  }
  else if (eCSSUnit_Initial == styleValue.GetUnit() ||
           eCSSUnit_Unset == styleValue.GetUnit()) {
    column->mColumnRuleStyle = NS_STYLE_BORDER_STYLE_NONE;
  }
  else if (eCSSUnit_Inherit == styleValue.GetUnit()) {
    conditions.SetUncacheable();
    column->mColumnRuleStyle = parent->mColumnRuleStyle;
  }

  // column-rule-color: color, inherit
  const nsCSSValue& colorValue = *aRuleData->ValueForColumnRuleColor();
  if (eCSSUnit_Inherit == colorValue.GetUnit()) {
    conditions.SetUncacheable();
    column->mColumnRuleColorIsForeground = false;
    if (parent->mColumnRuleColorIsForeground) {
      if (parentContext) {
        column->mColumnRuleColor = parentContext->StyleColor()->mColor;
      } else {
        nsStyleColor defaultColumnRuleColor(mPresContext);
        column->mColumnRuleColor = defaultColumnRuleColor.mColor;
      }
    } else {
      column->mColumnRuleColor = parent->mColumnRuleColor;
    }
  }
  else if (eCSSUnit_Initial == colorValue.GetUnit() ||
           eCSSUnit_Unset == colorValue.GetUnit() ||
           eCSSUnit_Enumerated == colorValue.GetUnit()) {
    column->mColumnRuleColorIsForeground = true;
  }
  else if (SetColor(colorValue, 0, mPresContext, aContext,
                    column->mColumnRuleColor, conditions)) {
    column->mColumnRuleColorIsForeground = false;
  }

  // column-fill: enum
  SetDiscrete(*aRuleData->ValueForColumnFill(),
                column->mColumnFill, conditions,
                SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
                parent->mColumnFill,
                NS_STYLE_COLUMN_FILL_BALANCE,
                0, 0, 0, 0);

  COMPUTE_END_RESET(Column, column)
}

static void
SetSVGPaint(const nsCSSValue& aValue, const nsStyleSVGPaint& parentPaint,
            nsPresContext* aPresContext, nsStyleContext *aContext,
            nsStyleSVGPaint& aResult, nsStyleSVGPaintType aInitialPaintType,
            RuleNodeCacheConditions& aConditions)
{
  nscolor color;

  if (aValue.GetUnit() == eCSSUnit_Inherit ||
      aValue.GetUnit() == eCSSUnit_Unset) {
    aResult = parentPaint;
    aConditions.SetUncacheable();
  } else if (aValue.GetUnit() == eCSSUnit_None) {
    aResult.SetType(eStyleSVGPaintType_None);
  } else if (aValue.GetUnit() == eCSSUnit_Initial) {
    aResult.SetType(aInitialPaintType);
    aResult.mPaint.mColor = NS_RGB(0, 0, 0);
    aResult.mFallbackColor = NS_RGB(0, 0, 0);
  } else if (SetColor(aValue, NS_RGB(0, 0, 0), aPresContext, aContext,
                      color, aConditions)) {
    aResult.SetType(eStyleSVGPaintType_Color);
    aResult.mPaint.mColor = color;
  } else if (aValue.GetUnit() == eCSSUnit_Pair) {
    const nsCSSValuePair& pair = aValue.GetPairValue();

    if (pair.mXValue.GetUnit() == eCSSUnit_URL) {
      aResult.SetType(eStyleSVGPaintType_Server);
      aResult.mPaint.mPaintServer = pair.mXValue.GetURLValue();
      NS_IF_ADDREF(aResult.mPaint.mPaintServer);
    } else if (pair.mXValue.GetUnit() == eCSSUnit_Enumerated) {

      switch (pair.mXValue.GetIntValue()) {
      case NS_COLOR_CONTEXT_FILL:
        aResult.SetType(eStyleSVGPaintType_ContextFill);
        break;
      case NS_COLOR_CONTEXT_STROKE:
        aResult.SetType(eStyleSVGPaintType_ContextStroke);
        break;
      default:
        NS_NOTREACHED("unknown keyword as paint server value");
      }

    } else {
      NS_NOTREACHED("malformed paint server value");
    }

    if (pair.mYValue.GetUnit() == eCSSUnit_None) {
      aResult.mFallbackColor = NS_RGBA(0, 0, 0, 0);
    } else {
      MOZ_ASSERT(pair.mYValue.GetUnit() != eCSSUnit_Inherit,
                 "cannot inherit fallback colour");
      SetColor(pair.mYValue, NS_RGB(0, 0, 0), aPresContext, aContext,
               aResult.mFallbackColor, aConditions);
    }
  } else {
    MOZ_ASSERT(aValue.GetUnit() == eCSSUnit_Null,
               "malformed paint server value");
  }
}

static void
SetSVGOpacity(const nsCSSValue& aValue,
              float& aOpacityField, nsStyleSVGOpacitySource& aOpacityTypeField,
              RuleNodeCacheConditions& aConditions,
              float aParentOpacity, nsStyleSVGOpacitySource aParentOpacityType)
{
  if (eCSSUnit_Enumerated == aValue.GetUnit()) {
    switch (aValue.GetIntValue()) {
    case NS_STYLE_CONTEXT_FILL_OPACITY:
      aOpacityTypeField = eStyleSVGOpacitySource_ContextFillOpacity;
      break;
    case NS_STYLE_CONTEXT_STROKE_OPACITY:
      aOpacityTypeField = eStyleSVGOpacitySource_ContextStrokeOpacity;
      break;
    default:
      NS_NOTREACHED("SetSVGOpacity: Unknown keyword");
    }
    // Fall back on fully opaque
    aOpacityField = 1.0f;
  } else if (eCSSUnit_Inherit == aValue.GetUnit() ||
             eCSSUnit_Unset == aValue.GetUnit()) {
    aConditions.SetUncacheable();
    aOpacityField = aParentOpacity;
    aOpacityTypeField = aParentOpacityType;
  } else if (eCSSUnit_Null != aValue.GetUnit()) {
    SetFactor(aValue, aOpacityField, aConditions,
              aParentOpacity, 1.0f, SETFCT_OPACITY);
    aOpacityTypeField = eStyleSVGOpacitySource_Normal;
  }
}

const void*
nsRuleNode::ComputeSVGData(void* aStartStruct,
                           const nsRuleData* aRuleData,
                           nsStyleContext* aContext,
                           nsRuleNode* aHighestNode,
                           const RuleDetail aRuleDetail,
                           const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_INHERITED(SVG, (), svg, parentSVG)

  // clip-rule: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForClipRule(),
              svg->mClipRule, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentSVG->mClipRule,
              NS_STYLE_FILL_RULE_NONZERO, 0, 0, 0, 0);

  // color-interpolation: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForColorInterpolation(),
              svg->mColorInterpolation, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentSVG->mColorInterpolation,
              NS_STYLE_COLOR_INTERPOLATION_SRGB, 0, 0, 0, 0);

  // color-interpolation-filters: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForColorInterpolationFilters(),
              svg->mColorInterpolationFilters, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentSVG->mColorInterpolationFilters,
              NS_STYLE_COLOR_INTERPOLATION_LINEARRGB, 0, 0, 0, 0);

  // fill:
  SetSVGPaint(*aRuleData->ValueForFill(),
              parentSVG->mFill, mPresContext, aContext,
              svg->mFill, eStyleSVGPaintType_Color, conditions);

  // fill-opacity: factor, inherit, initial,
  // context-fill-opacity, context-stroke-opacity
  nsStyleSVGOpacitySource contextFillOpacity = svg->mFillOpacitySource;
  SetSVGOpacity(*aRuleData->ValueForFillOpacity(),
                svg->mFillOpacity, contextFillOpacity, conditions,
                parentSVG->mFillOpacity, parentSVG->mFillOpacitySource);
  svg->mFillOpacitySource = contextFillOpacity;

  // fill-rule: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForFillRule(),
              svg->mFillRule, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentSVG->mFillRule,
              NS_STYLE_FILL_RULE_NONZERO, 0, 0, 0, 0);

  // image-rendering: enum, inherit
  SetDiscrete(*aRuleData->ValueForImageRendering(),
              svg->mImageRendering, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentSVG->mImageRendering,
              NS_STYLE_IMAGE_RENDERING_AUTO, 0, 0, 0, 0);

  // marker-end: url, none, inherit
  const nsCSSValue* markerEndValue = aRuleData->ValueForMarkerEnd();
  if (eCSSUnit_URL == markerEndValue->GetUnit()) {
    svg->mMarkerEnd = markerEndValue->GetURLValue();
  } else if (eCSSUnit_None == markerEndValue->GetUnit() ||
             eCSSUnit_Initial == markerEndValue->GetUnit()) {
    svg->mMarkerEnd = nullptr;
  } else if (eCSSUnit_Inherit == markerEndValue->GetUnit() ||
             eCSSUnit_Unset == markerEndValue->GetUnit()) {
    conditions.SetUncacheable();
    svg->mMarkerEnd = parentSVG->mMarkerEnd;
  }

  // marker-mid: url, none, inherit
  const nsCSSValue* markerMidValue = aRuleData->ValueForMarkerMid();
  if (eCSSUnit_URL == markerMidValue->GetUnit()) {
    svg->mMarkerMid = markerMidValue->GetURLValue();
  } else if (eCSSUnit_None == markerMidValue->GetUnit() ||
             eCSSUnit_Initial == markerMidValue->GetUnit()) {
    svg->mMarkerMid = nullptr;
  } else if (eCSSUnit_Inherit == markerMidValue->GetUnit() ||
             eCSSUnit_Unset == markerMidValue->GetUnit()) {
    conditions.SetUncacheable();
    svg->mMarkerMid = parentSVG->mMarkerMid;
  }

  // marker-start: url, none, inherit
  const nsCSSValue* markerStartValue = aRuleData->ValueForMarkerStart();
  if (eCSSUnit_URL == markerStartValue->GetUnit()) {
    svg->mMarkerStart = markerStartValue->GetURLValue();
  } else if (eCSSUnit_None == markerStartValue->GetUnit() ||
             eCSSUnit_Initial == markerStartValue->GetUnit()) {
    svg->mMarkerStart = nullptr;
  } else if (eCSSUnit_Inherit == markerStartValue->GetUnit() ||
             eCSSUnit_Unset == markerStartValue->GetUnit()) {
    conditions.SetUncacheable();
    svg->mMarkerStart = parentSVG->mMarkerStart;
  }

  // paint-order: enum (bit field), inherit, initial
  const nsCSSValue* paintOrderValue = aRuleData->ValueForPaintOrder();
  switch (paintOrderValue->GetUnit()) {
    case eCSSUnit_Null:
      break;

    case eCSSUnit_Enumerated:
      static_assert
        (NS_STYLE_PAINT_ORDER_BITWIDTH * NS_STYLE_PAINT_ORDER_LAST_VALUE <= 8,
         "SVGStyleStruct::mPaintOrder not big enough");
      svg->mPaintOrder = static_cast<uint8_t>(paintOrderValue->GetIntValue());
      break;

    case eCSSUnit_Inherit:
    case eCSSUnit_Unset:
      conditions.SetUncacheable();
      svg->mPaintOrder = parentSVG->mPaintOrder;
      break;

    case eCSSUnit_Initial:
      svg->mPaintOrder = NS_STYLE_PAINT_ORDER_NORMAL;
      break;

    default:
      NS_NOTREACHED("unexpected unit");
  }

  // shape-rendering: enum, inherit
  SetDiscrete(*aRuleData->ValueForShapeRendering(),
              svg->mShapeRendering, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentSVG->mShapeRendering,
              NS_STYLE_SHAPE_RENDERING_AUTO, 0, 0, 0, 0);

  // stroke:
  SetSVGPaint(*aRuleData->ValueForStroke(),
              parentSVG->mStroke, mPresContext, aContext,
              svg->mStroke, eStyleSVGPaintType_None, conditions);

  // stroke-dasharray: <dasharray>, none, inherit, context-value
  const nsCSSValue* strokeDasharrayValue = aRuleData->ValueForStrokeDasharray();
  switch (strokeDasharrayValue->GetUnit()) {
  case eCSSUnit_Null:
    break;

  case eCSSUnit_Inherit:
  case eCSSUnit_Unset:
    conditions.SetUncacheable();
    svg->mStrokeDasharrayFromObject = parentSVG->mStrokeDasharrayFromObject;
    // only do the copy if weren't already set up by the copy constructor
    // FIXME Bug 389408: This is broken when aStartStruct is non-null!
    if (!svg->mStrokeDasharray) {
      svg->mStrokeDasharrayLength = parentSVG->mStrokeDasharrayLength;
      if (svg->mStrokeDasharrayLength) {
        svg->mStrokeDasharray = new nsStyleCoord[svg->mStrokeDasharrayLength];
        if (svg->mStrokeDasharray)
          memcpy(svg->mStrokeDasharray,
                 parentSVG->mStrokeDasharray,
                 svg->mStrokeDasharrayLength * sizeof(nsStyleCoord));
        else
          svg->mStrokeDasharrayLength = 0;
      }
    }
    break;

  case eCSSUnit_Enumerated:
    MOZ_ASSERT(strokeDasharrayValue->GetIntValue() ==
                     NS_STYLE_STROKE_PROP_CONTEXT_VALUE,
               "Unknown keyword for stroke-dasharray");
    svg->mStrokeDasharrayFromObject = true;
    delete [] svg->mStrokeDasharray;
    svg->mStrokeDasharray = nullptr;
    svg->mStrokeDasharrayLength = 0;
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_None:
    svg->mStrokeDasharrayFromObject = false;
    delete [] svg->mStrokeDasharray;
    svg->mStrokeDasharray = nullptr;
    svg->mStrokeDasharrayLength = 0;
    break;

  case eCSSUnit_List:
  case eCSSUnit_ListDep: {
    svg->mStrokeDasharrayFromObject = false;
    delete [] svg->mStrokeDasharray;
    svg->mStrokeDasharray = nullptr;
    svg->mStrokeDasharrayLength = 0;

    // count number of values
    const nsCSSValueList *value = strokeDasharrayValue->GetListValue();
    svg->mStrokeDasharrayLength = ListLength(value);

    NS_ASSERTION(svg->mStrokeDasharrayLength != 0, "no dasharray items");

    svg->mStrokeDasharray = new nsStyleCoord[svg->mStrokeDasharrayLength];

    if (svg->mStrokeDasharray) {
      uint32_t i = 0;
      while (nullptr != value) {
        SetCoord(value->mValue,
                 svg->mStrokeDasharray[i++], nsStyleCoord(),
                 SETCOORD_LP | SETCOORD_FACTOR,
                 aContext, mPresContext, conditions);
        value = value->mNext;
      }
    } else {
      svg->mStrokeDasharrayLength = 0;
    }
    break;
  }

  default:
    MOZ_ASSERT(false, "unrecognized dasharray unit");
  }

  // stroke-dashoffset: <dashoffset>, inherit
  const nsCSSValue *strokeDashoffsetValue =
    aRuleData->ValueForStrokeDashoffset();
  svg->mStrokeDashoffsetFromObject =
    strokeDashoffsetValue->GetUnit() == eCSSUnit_Enumerated &&
    strokeDashoffsetValue->GetIntValue() == NS_STYLE_STROKE_PROP_CONTEXT_VALUE;
  if (svg->mStrokeDashoffsetFromObject) {
    svg->mStrokeDashoffset.SetCoordValue(0);
  } else {
    SetCoord(*aRuleData->ValueForStrokeDashoffset(),
             svg->mStrokeDashoffset, parentSVG->mStrokeDashoffset,
             SETCOORD_LPH | SETCOORD_FACTOR | SETCOORD_INITIAL_ZERO |
               SETCOORD_UNSET_INHERIT,
             aContext, mPresContext, conditions);
  }

  // stroke-linecap: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForStrokeLinecap(),
              svg->mStrokeLinecap, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentSVG->mStrokeLinecap,
              NS_STYLE_STROKE_LINECAP_BUTT, 0, 0, 0, 0);

  // stroke-linejoin: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForStrokeLinejoin(),
              svg->mStrokeLinejoin, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentSVG->mStrokeLinejoin,
              NS_STYLE_STROKE_LINEJOIN_MITER, 0, 0, 0, 0);

  // stroke-miterlimit: <miterlimit>, inherit
  SetFactor(*aRuleData->ValueForStrokeMiterlimit(),
            svg->mStrokeMiterlimit,
            conditions,
            parentSVG->mStrokeMiterlimit, 4.0f,
            SETFCT_UNSET_INHERIT);

  // stroke-opacity:
  nsStyleSVGOpacitySource contextStrokeOpacity = svg->mStrokeOpacitySource;
  SetSVGOpacity(*aRuleData->ValueForStrokeOpacity(),
                svg->mStrokeOpacity, contextStrokeOpacity, conditions,
                parentSVG->mStrokeOpacity, parentSVG->mStrokeOpacitySource);
  svg->mStrokeOpacitySource = contextStrokeOpacity;

  // stroke-width:
  const nsCSSValue* strokeWidthValue = aRuleData->ValueForStrokeWidth();
  switch (strokeWidthValue->GetUnit()) {
  case eCSSUnit_Enumerated:
    MOZ_ASSERT(strokeWidthValue->GetIntValue() ==
                 NS_STYLE_STROKE_PROP_CONTEXT_VALUE,
               "Unrecognized keyword for stroke-width");
    svg->mStrokeWidthFromObject = true;
    svg->mStrokeWidth.SetCoordValue(nsPresContext::CSSPixelsToAppUnits(1));
    break;

  case eCSSUnit_Initial:
    svg->mStrokeWidthFromObject = false;
    svg->mStrokeWidth.SetCoordValue(nsPresContext::CSSPixelsToAppUnits(1));
    break;

  default:
    svg->mStrokeWidthFromObject = false;
    SetCoord(*strokeWidthValue,
             svg->mStrokeWidth, parentSVG->mStrokeWidth,
             SETCOORD_LPH | SETCOORD_FACTOR | SETCOORD_UNSET_INHERIT,
             aContext, mPresContext, conditions);
  }

  // text-anchor: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForTextAnchor(),
              svg->mTextAnchor, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentSVG->mTextAnchor,
              NS_STYLE_TEXT_ANCHOR_START, 0, 0, 0, 0);

  // text-rendering: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForTextRendering(),
              svg->mTextRendering, conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INHERIT,
              parentSVG->mTextRendering,
              NS_STYLE_TEXT_RENDERING_AUTO, 0, 0, 0, 0);

  COMPUTE_END_INHERITED(SVG, svg)
}

void
nsRuleNode::SetStyleClipPathToCSSValue(nsStyleClipPath* aStyleClipPath,
                                       const nsCSSValue* aValue,
                                       nsStyleContext* aStyleContext,
                                       nsPresContext* aPresContext,
                                       RuleNodeCacheConditions& aConditions)
{
  MOZ_ASSERT(aValue->GetUnit() != eCSSUnit_ListDep ||
             aValue->GetUnit() != eCSSUnit_List,
             "expected a basic shape or reference box");

  const nsCSSValueList* cur = aValue->GetListValue();

  uint8_t sizingBox = NS_STYLE_CLIP_SHAPE_SIZING_NOBOX;
  nsRefPtr<nsStyleBasicShape> basicShape;
  for (unsigned i = 0; i < 2; ++i) {
    if (!cur) {
      break;
    }
    if (cur->mValue.GetUnit() == eCSSUnit_Function) {
      nsCSSValue::Array* shapeFunction = cur->mValue.GetArrayValue();
      nsCSSKeyword functionName =
        (nsCSSKeyword)shapeFunction->Item(0).GetIntValue();
      if (functionName == eCSSKeyword_polygon) {
        MOZ_ASSERT(!basicShape, "did not expect value");
        basicShape = new nsStyleBasicShape(nsStyleBasicShape::ePolygon);
        MOZ_ASSERT(shapeFunction->Count() > 1,
                   "polygon has wrong number of arguments");
        size_t j = 1;
        if (shapeFunction->Item(j).GetUnit() == eCSSUnit_Enumerated) {
          basicShape->SetFillRule(shapeFunction->Item(j).GetIntValue());
          ++j;
        }
        const int32_t mask = SETCOORD_PERCENT | SETCOORD_LENGTH |
                             SETCOORD_STORE_CALC;
        const nsCSSValuePairList* curPair =
          shapeFunction->Item(j).GetPairListValue();
        nsTArray<nsStyleCoord>& coordinates = basicShape->Coordinates();
        while (curPair) {
          nsStyleCoord xCoord, yCoord;
          DebugOnly<bool> didSetCoordX = SetCoord(curPair->mXValue, xCoord,
                                                  nsStyleCoord(), mask,
                                                  aStyleContext, aPresContext,
                                                  aConditions);
          coordinates.AppendElement(xCoord);
          MOZ_ASSERT(didSetCoordX, "unexpected x coordinate unit");
          DebugOnly<bool> didSetCoordY = SetCoord(curPair->mYValue, yCoord,
                                                  nsStyleCoord(), mask,
                                                  aStyleContext, aPresContext,
                                                  aConditions);
          coordinates.AppendElement(yCoord);
          MOZ_ASSERT(didSetCoordY, "unexpected y coordinate unit");
          curPair = curPair->mNext;
        }
      } else if (functionName == eCSSKeyword_circle ||
                 functionName == eCSSKeyword_ellipse) {
        nsStyleBasicShape::Type type = functionName == eCSSKeyword_circle ?
                                       nsStyleBasicShape::eCircle :
                                       nsStyleBasicShape::eEllipse;
        MOZ_ASSERT(!basicShape, "did not expect value");
        basicShape = new nsStyleBasicShape(type);
        const int32_t mask = SETCOORD_PERCENT | SETCOORD_LENGTH |
                             SETCOORD_STORE_CALC | SETCOORD_ENUMERATED;
        size_t count = type == nsStyleBasicShape::eCircle ? 2 : 3;
        MOZ_ASSERT(shapeFunction->Count() == count + 1,
                   "unexpected arguments count");
        MOZ_ASSERT(type == nsStyleBasicShape::eCircle ||
                   (shapeFunction->Item(1).GetUnit() == eCSSUnit_Null) ==
                   (shapeFunction->Item(2).GetUnit() == eCSSUnit_Null),
                   "ellipse should have two radii or none");
        for (size_t j = 1; j < count; ++j) {
          const nsCSSValue& val = shapeFunction->Item(j);
          nsStyleCoord radius;
          if (val.GetUnit() != eCSSUnit_Null) {
            DebugOnly<bool> didSetRadius = SetCoord(val, radius,
                                                    nsStyleCoord(), mask,
                                                    aStyleContext,
                                                    aPresContext,
                                                    aConditions);
            MOZ_ASSERT(didSetRadius, "unexpected radius unit");
          } else {
            radius.SetIntValue(NS_RADIUS_CLOSEST_SIDE, eStyleUnit_Enumerated);
          }
          basicShape->Coordinates().AppendElement(radius);
        }
        const nsCSSValue& positionVal = shapeFunction->Item(count);
        if (positionVal.GetUnit() == eCSSUnit_Array) {
          ComputePositionValue(aStyleContext, positionVal,
                               basicShape->GetPosition(),
                               aConditions);
        } else {
            MOZ_ASSERT(positionVal.GetUnit() == eCSSUnit_Null,
                       "expected no value");
        }
      } else if (functionName == eCSSKeyword_inset) {
        MOZ_ASSERT(!basicShape, "did not expect value");
        basicShape = new nsStyleBasicShape(nsStyleBasicShape::eInset);
        MOZ_ASSERT(shapeFunction->Count() == 6,
                   "inset function has wrong number of arguments");
        MOZ_ASSERT(shapeFunction->Item(1).GetUnit() != eCSSUnit_Null,
                   "no shape arguments defined");
        const int32_t mask = SETCOORD_PERCENT | SETCOORD_LENGTH |
                             SETCOORD_STORE_CALC;
        nsTArray<nsStyleCoord>& coords = basicShape->Coordinates();
        for (size_t j = 1; j <= 4; ++j) {
          const nsCSSValue& val = shapeFunction->Item(j);
          nsStyleCoord inset;
          // Fill missing values to get 4 at the end.
          if (val.GetUnit() == eCSSUnit_Null) {
            if (j == 4) {
              inset = coords[1];
            } else {
              MOZ_ASSERT(j != 1, "first argument not specified");
              inset = coords[0];
            }
          } else {
            DebugOnly<bool> didSetInset = SetCoord(val, inset,
                                                   nsStyleCoord(), mask,
                                                   aStyleContext, aPresContext,
                                                   aConditions);
            MOZ_ASSERT(didSetInset, "unexpected inset unit");
          }
          coords.AppendElement(inset);
        }

        nsStyleCorners& insetRadius = basicShape->GetRadius();
        if (shapeFunction->Item(5).GetUnit() == eCSSUnit_Array) {
          nsCSSValue::Array* radiiArray = shapeFunction->Item(5).GetArrayValue();
          NS_FOR_CSS_FULL_CORNERS(corner) {
            int cx = NS_FULL_TO_HALF_CORNER(corner, false);
            int cy = NS_FULL_TO_HALF_CORNER(corner, true);
            const nsCSSValue& radius = radiiArray->Item(corner);
            nsStyleCoord coordX, coordY;
            DebugOnly<bool> didSetRadii = SetPairCoords(radius, coordX, coordY,
                                                        nsStyleCoord(),
                                                        nsStyleCoord(), mask,
                                                        aStyleContext,
                                                        aPresContext,
                                                        aConditions);
            MOZ_ASSERT(didSetRadii, "unexpected radius unit");
            insetRadius.Set(cx, coordX);
            insetRadius.Set(cy, coordY);
          }
        } else {
          MOZ_ASSERT(shapeFunction->Item(5).GetUnit() == eCSSUnit_Null,
                     "unexpected value");
          // Initialize border-radius
          nsStyleCoord zero;
          zero.SetCoordValue(0);
          NS_FOR_CSS_HALF_CORNERS(j) {
            insetRadius.Set(j, zero);
          }
        }
      } else {
        NS_NOTREACHED("unexpected basic shape function");
        return;
      }
    } else if (cur->mValue.GetUnit() == eCSSUnit_Enumerated) {
      int32_t type = cur->mValue.GetIntValue();
      if (type > NS_STYLE_CLIP_SHAPE_SIZING_VIEW ||
          type < NS_STYLE_CLIP_SHAPE_SIZING_NOBOX) {
        NS_NOTREACHED("unexpected reference box");
        return;
      }
      sizingBox = (uint8_t)type;
    } else {
      NS_NOTREACHED("unexpected value");
      return;
    }
    cur = cur->mNext;
  }

  if (basicShape) {
    aStyleClipPath->SetBasicShape(basicShape, sizingBox);
  } else {
    aStyleClipPath->SetSizingBox(sizingBox);
  }
}

// Returns true if the nsStyleFilter was successfully set using the nsCSSValue.
bool
nsRuleNode::SetStyleFilterToCSSValue(nsStyleFilter* aStyleFilter,
                                     const nsCSSValue& aValue,
                                     nsStyleContext* aStyleContext,
                                     nsPresContext* aPresContext,
                                     RuleNodeCacheConditions& aConditions)
{
  nsCSSUnit unit = aValue.GetUnit();
  if (unit == eCSSUnit_URL) {
    nsIURI* url = aValue.GetURLValue();
    if (!url)
      return false;
    aStyleFilter->SetURL(url);
    return true;
  }

  MOZ_ASSERT(unit == eCSSUnit_Function, "expected a filter function");

  nsCSSValue::Array* filterFunction = aValue.GetArrayValue();
  nsCSSKeyword functionName =
    (nsCSSKeyword)filterFunction->Item(0).GetIntValue();

  int32_t type;
  DebugOnly<bool> foundKeyword =
    nsCSSProps::FindKeyword(functionName,
                            nsCSSProps::kFilterFunctionKTable,
                            type);
  MOZ_ASSERT(foundKeyword, "unknown filter type");
  if (type == NS_STYLE_FILTER_DROP_SHADOW) {
    nsRefPtr<nsCSSShadowArray> shadowArray = GetShadowData(
      filterFunction->Item(1).GetListValue(),
      aStyleContext,
      false,
      aConditions);
    aStyleFilter->SetDropShadow(shadowArray);
    return true;
  }

  int32_t mask = SETCOORD_PERCENT | SETCOORD_FACTOR;
  if (type == NS_STYLE_FILTER_BLUR) {
    mask = SETCOORD_LENGTH |
           SETCOORD_CALC_LENGTH_ONLY |
           SETCOORD_CALC_CLAMP_NONNEGATIVE;
  } else if (type == NS_STYLE_FILTER_HUE_ROTATE) {
    mask = SETCOORD_ANGLE;
  }

  MOZ_ASSERT(filterFunction->Count() == 2,
             "all filter functions should have exactly one argument");

  nsCSSValue& arg = filterFunction->Item(1);
  nsStyleCoord filterParameter;
  DebugOnly<bool> didSetCoord = SetCoord(arg, filterParameter,
                                         nsStyleCoord(), mask,
                                         aStyleContext, aPresContext,
                                         aConditions);
  aStyleFilter->SetFilterParameter(filterParameter, type);
  MOZ_ASSERT(didSetCoord, "unexpected unit");
  return true;
}

const void*
nsRuleNode::ComputeSVGResetData(void* aStartStruct,
                                const nsRuleData* aRuleData,
                                nsStyleContext* aContext,
                                nsRuleNode* aHighestNode,
                                const RuleDetail aRuleDetail,
                                const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(SVGReset, (), svgReset, parentSVGReset)

  // stop-color:
  const nsCSSValue* stopColorValue = aRuleData->ValueForStopColor();
  if (eCSSUnit_Initial == stopColorValue->GetUnit() ||
      eCSSUnit_Unset == stopColorValue->GetUnit()) {
    svgReset->mStopColor = NS_RGB(0, 0, 0);
  } else {
    SetColor(*stopColorValue, parentSVGReset->mStopColor,
             mPresContext, aContext, svgReset->mStopColor, conditions);
  }

  // flood-color:
  const nsCSSValue* floodColorValue = aRuleData->ValueForFloodColor();
  if (eCSSUnit_Initial == floodColorValue->GetUnit() ||
      eCSSUnit_Unset == floodColorValue->GetUnit()) {
    svgReset->mFloodColor = NS_RGB(0, 0, 0);
  } else {
    SetColor(*floodColorValue, parentSVGReset->mFloodColor,
             mPresContext, aContext, svgReset->mFloodColor, conditions);
  }

  // lighting-color:
  const nsCSSValue* lightingColorValue = aRuleData->ValueForLightingColor();
  if (eCSSUnit_Initial == lightingColorValue->GetUnit() ||
      eCSSUnit_Unset == lightingColorValue->GetUnit()) {
    svgReset->mLightingColor = NS_RGB(255, 255, 255);
  } else {
    SetColor(*lightingColorValue, parentSVGReset->mLightingColor,
             mPresContext, aContext, svgReset->mLightingColor,
             conditions);
  }

  // clip-path: url, <basic-shape> || <geometry-box>, none, inherit
  const nsCSSValue* clipPathValue = aRuleData->ValueForClipPath();
  switch (clipPathValue->GetUnit()) {
    case eCSSUnit_Null:
      break;
    case eCSSUnit_None:
    case eCSSUnit_Initial:
    case eCSSUnit_Unset:
      svgReset->mClipPath = nsStyleClipPath();
      break;
    case eCSSUnit_Inherit:
      conditions.SetUncacheable();
      svgReset->mClipPath = parentSVGReset->mClipPath;
      break;
    case eCSSUnit_URL: {
      svgReset->mClipPath = nsStyleClipPath();
      nsIURI* url = clipPathValue->GetURLValue();
      if (url) {
        svgReset->mClipPath.SetURL(url);
      }
      break;
    }
    case eCSSUnit_List:
    case eCSSUnit_ListDep: {
      svgReset->mClipPath = nsStyleClipPath();
      SetStyleClipPathToCSSValue(&svgReset->mClipPath, clipPathValue, aContext,
                                 mPresContext, conditions);
      break;
    }
    default:
      NS_NOTREACHED("unexpected unit");
  }

  // stop-opacity:
  SetFactor(*aRuleData->ValueForStopOpacity(),
            svgReset->mStopOpacity, conditions,
            parentSVGReset->mStopOpacity, 1.0f,
            SETFCT_OPACITY | SETFCT_UNSET_INITIAL);

  // flood-opacity:
  SetFactor(*aRuleData->ValueForFloodOpacity(),
            svgReset->mFloodOpacity, conditions,
            parentSVGReset->mFloodOpacity, 1.0f,
            SETFCT_OPACITY | SETFCT_UNSET_INITIAL);

  // dominant-baseline: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForDominantBaseline(),
              svgReset->mDominantBaseline,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentSVGReset->mDominantBaseline,
              NS_STYLE_DOMINANT_BASELINE_AUTO, 0, 0, 0, 0);

  // vector-effect: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForVectorEffect(),
              svgReset->mVectorEffect,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentSVGReset->mVectorEffect,
              NS_STYLE_VECTOR_EFFECT_NONE, 0, 0, 0, 0);

  // filter: url, none, inherit
  const nsCSSValue* filterValue = aRuleData->ValueForFilter();
  switch (filterValue->GetUnit()) {
    case eCSSUnit_Null:
      break;
    case eCSSUnit_None:
    case eCSSUnit_Initial:
    case eCSSUnit_Unset:
      svgReset->mFilters.Clear();
      break;
    case eCSSUnit_Inherit:
      conditions.SetUncacheable();
      svgReset->mFilters = parentSVGReset->mFilters;
      break;
    case eCSSUnit_List:
    case eCSSUnit_ListDep: {
      svgReset->mFilters.Clear();
      const nsCSSValueList* cur = filterValue->GetListValue();
      while (cur) {
        nsStyleFilter styleFilter;
        if (!SetStyleFilterToCSSValue(&styleFilter, cur->mValue, aContext,
                                      mPresContext, conditions)) {
          svgReset->mFilters.Clear();
          break;
        }
        MOZ_ASSERT(styleFilter.GetType() != NS_STYLE_FILTER_NONE,
                   "filter should be set");
        svgReset->mFilters.AppendElement(styleFilter);
        cur = cur->mNext;
      }
      break;
    }
    default:
      NS_NOTREACHED("unexpected unit");
  }

  // mask: url, none, inherit
  const nsCSSValue* maskValue = aRuleData->ValueForMask();
  if (eCSSUnit_URL == maskValue->GetUnit()) {
    svgReset->mMask = maskValue->GetURLValue();
  } else if (eCSSUnit_None == maskValue->GetUnit() ||
             eCSSUnit_Initial == maskValue->GetUnit() ||
             eCSSUnit_Unset == maskValue->GetUnit()) {
    svgReset->mMask = nullptr;
  } else if (eCSSUnit_Inherit == maskValue->GetUnit()) {
    conditions.SetUncacheable();
    svgReset->mMask = parentSVGReset->mMask;
  }

  // mask-type: enum, inherit, initial
  SetDiscrete(*aRuleData->ValueForMaskType(),
              svgReset->mMaskType,
              conditions,
              SETDSC_ENUMERATED | SETDSC_UNSET_INITIAL,
              parentSVGReset->mMaskType,
              NS_STYLE_MASK_TYPE_LUMINANCE, 0, 0, 0, 0);

  COMPUTE_END_RESET(SVGReset, svgReset)
}

const void*
nsRuleNode::ComputeVariablesData(void* aStartStruct,
                                 const nsRuleData* aRuleData,
                                 nsStyleContext* aContext,
                                 nsRuleNode* aHighestNode,
                                 const RuleDetail aRuleDetail,
                                 const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_INHERITED(Variables, (), variables, parentVariables)

  MOZ_ASSERT(aRuleData->mVariables,
             "shouldn't be in ComputeVariablesData if there were no variable "
             "declarations specified");

  CSSVariableResolver resolver(&variables->mVariables);
  resolver.Resolve(&parentVariables->mVariables,
                   aRuleData->mVariables);
  conditions.SetUncacheable();

  COMPUTE_END_INHERITED(Variables, variables)
}

const void*
nsRuleNode::GetStyleData(nsStyleStructID aSID,
                         nsStyleContext* aContext,
                         bool aComputeData)
{
  NS_ASSERTION(IsUsedDirectly(),
               "if we ever call this on rule nodes that aren't used "
               "directly, we should adjust handling of mDependentBits "
               "in some way.");

  const void *data;

  // Never use cached data for animated style inside a pseudo-element;
  // see comment on cacheability in AnimValuesStyleRule::MapRuleInfoInto.
  if (!(HasAnimationData() && ParentHasPseudoElementData(aContext))) {
    data = mStyleData.GetStyleData(aSID, aContext);
    if (MOZ_LIKELY(data != nullptr))
      return data; // We have a fully specified struct. Just return it.
  }

  if (MOZ_UNLIKELY(!aComputeData))
    return nullptr;

  // Nothing is cached.  We'll have to delve further and examine our rules.
  data = WalkRuleTree(aSID, aContext);

  MOZ_ASSERT(data, "should have aborted on out-of-memory");
  return data;
}

void
nsRuleNode::Mark()
{
  for (nsRuleNode *node = this;
       node && !(node->mDependentBits & NS_RULE_NODE_GC_MARK);
       node = node->mParent)
    node->mDependentBits |= NS_RULE_NODE_GC_MARK;
}

bool
nsRuleNode::DestroyIfNotMarked()
{
  // If we're not marked, then we have to delete ourself.
  // However, we never allow the root node to GC itself, because nsStyleSet
  // wants to hold onto the root node and not worry about re-creating a
  // rule walker if the root node is deleted.
  if (!(mDependentBits & NS_RULE_NODE_GC_MARK) &&
      // Skip this only if we're the *current* root and not an old one.
      !(IsRoot() && mPresContext->StyleSet()->GetRuleTree() == this)) {
    Destroy();
    return true;
  }

  // Clear our mark, for the next time around.
  mDependentBits &= ~NS_RULE_NODE_GC_MARK;
  return false;
}

void
nsRuleNode::SweepChildren(nsTArray<nsRuleNode*>& aSweepQueue)
{
  NS_ASSERTION(!(mDependentBits & NS_RULE_NODE_GC_MARK),
               "missing DestroyIfNotMarked() call");
  NS_ASSERTION(HaveChildren(),
               "why call SweepChildren with no children?");
  uint32_t childrenDestroyed = 0;
  nsRuleNode* survivorsWithChildren = nullptr;
  if (ChildrenAreHashed()) {
    PLDHashTable* children = ChildrenHash();
    uint32_t oldChildCount = children->EntryCount();
    for (auto iter = children->Iter(); !iter.Done(); iter.Next()) {
      auto entry = static_cast<ChildrenHashEntry*>(iter.Get());
      nsRuleNode* node = entry->mRuleNode;
      if (node->DestroyIfNotMarked()) {
        iter.Remove();
      } else if (node->HaveChildren()) {
        // When children are hashed mNextSibling is not normally used but we
        // use it here to build a list of children that needs to be swept.
        nsRuleNode** headQ = &survivorsWithChildren;
        node->mNextSibling = *headQ;
        *headQ = node;
      }
    }
    childrenDestroyed = oldChildCount - children->EntryCount();
    if (childrenDestroyed == oldChildCount) {
      delete children;
      mChildren.asVoid = nullptr;
    }
  } else {
    for (nsRuleNode** children = ChildrenListPtr(); *children; ) {
      nsRuleNode* next = (*children)->mNextSibling;
      if ((*children)->DestroyIfNotMarked()) {
        // This rule node was destroyed, unlink it from the list by
        // making *children point to the next entry.
        *children = next;
        ++childrenDestroyed;
      } else {
        children = &(*children)->mNextSibling;
      }
    }
    survivorsWithChildren = ChildrenList();
  }
  if (survivorsWithChildren) {
    aSweepQueue.AppendElement(survivorsWithChildren);
  }
  NS_ASSERTION(childrenDestroyed <= mRefCnt, "wrong ref count");
  mRefCnt -= childrenDestroyed;
  NS_POSTCONDITION(IsRoot() || mRefCnt > 0,
                   "We didn't get swept, so we'd better have style contexts "
                   "pointing to us or to one of our descendants, which means "
                   "we'd better have a nonzero mRefCnt here!");
}

bool
nsRuleNode::Sweep()
{
  NS_ASSERTION(IsRoot(), "must start sweeping at a root");
  NS_ASSERTION(!mNextSibling, "root must not have mNextSibling");

  if (DestroyIfNotMarked()) {
    return true;
  }

  nsAutoTArray<nsRuleNode*, 70> sweepQueue;
  sweepQueue.AppendElement(this);
  while (!sweepQueue.IsEmpty()) {
    nsTArray<nsRuleNode*>::index_type last = sweepQueue.Length() - 1;
    nsRuleNode* ruleNode = sweepQueue[last];
    sweepQueue.RemoveElementAt(last);
    for (; ruleNode; ruleNode = ruleNode->mNextSibling) {
      if (ruleNode->HaveChildren()) {
        ruleNode->SweepChildren(sweepQueue);
      }
    }
  }
  return false;
}

/* static */ bool
nsRuleNode::HasAuthorSpecifiedRules(nsStyleContext* aStyleContext,
                                    uint32_t ruleTypeMask,
                                    bool aAuthorColorsAllowed)
{
  uint32_t inheritBits = 0;
  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_BACKGROUND)
    inheritBits |= NS_STYLE_INHERIT_BIT(Background);

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_BORDER)
    inheritBits |= NS_STYLE_INHERIT_BIT(Border);

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_PADDING)
    inheritBits |= NS_STYLE_INHERIT_BIT(Padding);

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_TEXT_SHADOW)
    inheritBits |= NS_STYLE_INHERIT_BIT(Text);

  // properties in the SIDS, whether or not we care about them
  size_t nprops = 0,
         backgroundOffset, borderOffset, paddingOffset, textShadowOffset;

  // We put the reset properties the start of the nsCSSValue array....

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_BACKGROUND) {
    backgroundOffset = nprops;
    nprops += nsCSSProps::PropertyCountInStruct(eStyleStruct_Background);
  }

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_BORDER) {
    borderOffset = nprops;
    nprops += nsCSSProps::PropertyCountInStruct(eStyleStruct_Border);
  }

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_PADDING) {
    paddingOffset = nprops;
    nprops += nsCSSProps::PropertyCountInStruct(eStyleStruct_Padding);
  }

  // ...and the inherited properties at the end of the array.
  size_t inheritedOffset = nprops;

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_TEXT_SHADOW) {
    textShadowOffset = nprops;
    nprops += nsCSSProps::PropertyCountInStruct(eStyleStruct_Text);
  }

  void* dataStorage = alloca(nprops * sizeof(nsCSSValue));
  AutoCSSValueArray dataArray(dataStorage, nprops);

  /* We're relying on the use of |aStyleContext| not mutating it! */
  nsRuleData ruleData(inheritBits, dataArray.get(),
                      aStyleContext->PresContext(), aStyleContext);

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_BACKGROUND) {
    ruleData.mValueOffsets[eStyleStruct_Background] = backgroundOffset;
  }

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_BORDER) {
    ruleData.mValueOffsets[eStyleStruct_Border] = borderOffset;
  }

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_PADDING) {
    ruleData.mValueOffsets[eStyleStruct_Padding] = paddingOffset;
  }

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_TEXT_SHADOW) {
    ruleData.mValueOffsets[eStyleStruct_Text] = textShadowOffset;
  }

  static const nsCSSProperty backgroundValues[] = {
    eCSSProperty_background_color,
    eCSSProperty_background_image,
  };

  static const nsCSSProperty borderValues[] = {
    eCSSProperty_border_top_color,
    eCSSProperty_border_top_style,
    eCSSProperty_border_top_width,
    eCSSProperty_border_right_color,
    eCSSProperty_border_right_style,
    eCSSProperty_border_right_width,
    eCSSProperty_border_bottom_color,
    eCSSProperty_border_bottom_style,
    eCSSProperty_border_bottom_width,
    eCSSProperty_border_left_color,
    eCSSProperty_border_left_style,
    eCSSProperty_border_left_width,
    eCSSProperty_border_top_left_radius,
    eCSSProperty_border_top_right_radius,
    eCSSProperty_border_bottom_right_radius,
    eCSSProperty_border_bottom_left_radius,
  };

  static const nsCSSProperty paddingValues[] = {
    eCSSProperty_padding_top,
    eCSSProperty_padding_right,
    eCSSProperty_padding_bottom,
    eCSSProperty_padding_left,
  };

  static const nsCSSProperty textShadowValues[] = {
    eCSSProperty_text_shadow
  };

  // Number of properties we care about
  size_t nValues = 0;

  nsCSSValue* values[MOZ_ARRAY_LENGTH(backgroundValues) +
                     MOZ_ARRAY_LENGTH(borderValues) +
                     MOZ_ARRAY_LENGTH(paddingValues) +
                     MOZ_ARRAY_LENGTH(textShadowValues)];

  nsCSSProperty properties[MOZ_ARRAY_LENGTH(backgroundValues) +
                           MOZ_ARRAY_LENGTH(borderValues) +
                           MOZ_ARRAY_LENGTH(paddingValues) +
                           MOZ_ARRAY_LENGTH(textShadowValues)];

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_BACKGROUND) {
    for (uint32_t i = 0, i_end = ArrayLength(backgroundValues);
         i < i_end; ++i) {
      properties[nValues] = backgroundValues[i];
      values[nValues++] = ruleData.ValueFor(backgroundValues[i]);
    }
  }

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_BORDER) {
    for (uint32_t i = 0, i_end = ArrayLength(borderValues);
         i < i_end; ++i) {
      properties[nValues] = borderValues[i];
      values[nValues++] = ruleData.ValueFor(borderValues[i]);
    }
  }

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_PADDING) {
    for (uint32_t i = 0, i_end = ArrayLength(paddingValues);
         i < i_end; ++i) {
      properties[nValues] = paddingValues[i];
      values[nValues++] = ruleData.ValueFor(paddingValues[i]);
    }
  }

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_TEXT_SHADOW) {
    for (uint32_t i = 0, i_end = ArrayLength(textShadowValues);
         i < i_end; ++i) {
      properties[nValues] = textShadowValues[i];
      values[nValues++] = ruleData.ValueFor(textShadowValues[i]);
    }
  }

  nsStyleContext* styleContext = aStyleContext;

  // We need to be careful not to count styles covered up by user-important or
  // UA-important declarations.  But we do want to catch explicit inherit
  // styling in those and check our parent style context to see whether we have
  // user styling for those properties.  Note that we don't care here about
  // inheritance due to lack of a specified value, since all the properties we
  // care about are reset properties.
  bool haveExplicitUAInherit;
  do {
    haveExplicitUAInherit = false;
    for (nsRuleNode* ruleNode = styleContext->RuleNode(); ruleNode;
         ruleNode = ruleNode->GetParent()) {
      nsIStyleRule *rule = ruleNode->GetRule();
      if (rule) {
        ruleData.mLevel = ruleNode->GetLevel();
        ruleData.mIsImportantRule = ruleNode->IsImportantRule();

        rule->MapRuleInfoInto(&ruleData);

        if (ruleData.mLevel == nsStyleSet::eAgentSheet ||
            ruleData.mLevel == nsStyleSet::eUserSheet) {
          // This is a rule whose effect we want to ignore, so if any of
          // the properties we care about were set, set them to the dummy
          // value that they'll never otherwise get.
          for (uint32_t i = 0; i < nValues; ++i) {
            nsCSSUnit unit = values[i]->GetUnit();
            if (unit != eCSSUnit_Null &&
                unit != eCSSUnit_Dummy &&
                unit != eCSSUnit_DummyInherit) {
              if (unit == eCSSUnit_Inherit ||
                  (i >= inheritedOffset && unit == eCSSUnit_Unset)) {
                haveExplicitUAInherit = true;
                values[i]->SetDummyInheritValue();
              } else {
                values[i]->SetDummyValue();
              }
            }
          }
        } else {
          // If any of the values we care about was set by the above rule,
          // we have author style.
          for (uint32_t i = 0; i < nValues; ++i) {
            if (values[i]->GetUnit() != eCSSUnit_Null &&
                values[i]->GetUnit() != eCSSUnit_Dummy && // see above
                values[i]->GetUnit() != eCSSUnit_DummyInherit) {
              // If author colors are not allowed, only claim to have
              // author-specified rules if we're looking at a non-color
              // property or if we're looking at the background color and it's
              // set to transparent.  Anything else should get set to a dummy
              // value instead.
              if (aAuthorColorsAllowed ||
                  !nsCSSProps::PropHasFlags(properties[i],
                     CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED) ||
                  (properties[i] == eCSSProperty_background_color &&
                   !values[i]->IsNonTransparentColor())) {
                return true;
              }

              values[i]->SetDummyValue();
            }
          }
        }
      }
    }

    if (haveExplicitUAInherit) {
      // reset all the eCSSUnit_Null values to eCSSUnit_Dummy (since they're
      // not styled by the author, or by anyone else), and then reset all the
      // eCSSUnit_DummyInherit values to eCSSUnit_Null (so we will be able to
      // detect them being styled by the author) and move up to our parent
      // style context.
      for (uint32_t i = 0; i < nValues; ++i)
        if (values[i]->GetUnit() == eCSSUnit_Null)
          values[i]->SetDummyValue();
      for (uint32_t i = 0; i < nValues; ++i)
        if (values[i]->GetUnit() == eCSSUnit_DummyInherit)
          values[i]->Reset();
      styleContext = styleContext->GetParent();
    }
  } while (haveExplicitUAInherit && styleContext);

  return false;
}

/* static */ void
nsRuleNode::ComputePropertiesOverridingAnimation(
                              const nsTArray<nsCSSProperty>& aProperties,
                              nsStyleContext* aStyleContext,
                              nsCSSPropertySet& aPropertiesOverridden)
{
  /*
   * Set up an nsRuleData with all the structs needed for all of the
   * properties in aProperties.
   */
  uint32_t structBits = 0;
  size_t nprops = 0;
  size_t offsets[nsStyleStructID_Length];
  for (size_t propIdx = 0, propEnd = aProperties.Length();
       propIdx < propEnd; ++propIdx) {
    nsCSSProperty prop = aProperties[propIdx];
    nsStyleStructID sid = nsCSSProps::kSIDTable[prop];
    uint32_t bit = nsCachedStyleData::GetBitForSID(sid);
    if (!(structBits & bit)) {
      structBits |= bit;
      offsets[sid] = nprops;
      nprops += nsCSSProps::PropertyCountInStruct(sid);
    }
  }

  void* dataStorage = alloca(nprops * sizeof(nsCSSValue));
  AutoCSSValueArray dataArray(dataStorage, nprops);

  // We're relying on the use of |aStyleContext| not mutating it!
  nsRuleData ruleData(structBits, dataArray.get(),
                      aStyleContext->PresContext(), aStyleContext);
  for (nsStyleStructID sid = nsStyleStructID(0);
       sid < nsStyleStructID_Length; sid = nsStyleStructID(sid + 1)) {
    if (structBits & nsCachedStyleData::GetBitForSID(sid)) {
      ruleData.mValueOffsets[sid] = offsets[sid];
    }
  }

  /*
   * Actually walk up the rule tree until we're someplace less
   * specific than animations.
   */
  for (nsRuleNode* ruleNode = aStyleContext->RuleNode(); ruleNode;
       ruleNode = ruleNode->GetParent()) {
    nsIStyleRule *rule = ruleNode->GetRule();
    if (rule) {
      ruleData.mLevel = ruleNode->GetLevel();
      ruleData.mIsImportantRule = ruleNode->IsImportantRule();

      // Transitions are the only non-!important level overriding
      // animations in the cascade ordering.  They also don't actually
      // override animations, since transitions are suppressed when both
      // are present.  And since we might not have called
      // UpdateCascadeResults (which updates when they are suppressed
      // due to the presence of animations for the same element and
      // property) for transitions yet (which will make their
      // MapRuleInfoInto skip the properties that are currently
      // animating), we should skip them explicitly.
      if (ruleData.mLevel == nsStyleSet::eTransitionSheet) {
        continue;
      }

      if (!ruleData.mIsImportantRule) {
        // We're now equal to or less than the animation level; stop.
        break;
      }

      rule->MapRuleInfoInto(&ruleData);
    }
  }

  /*
   * Fill in which properties were overridden.
   */
  for (size_t propIdx = 0, propEnd = aProperties.Length();
       propIdx < propEnd; ++propIdx) {
    nsCSSProperty prop = aProperties[propIdx];
    if (ruleData.ValueFor(prop)->GetUnit() != eCSSUnit_Null) {
      aPropertiesOverridden.AddProperty(prop);
    }
  }
}

/* static */
bool
nsRuleNode::ComputeColor(const nsCSSValue& aValue, nsPresContext* aPresContext,
                         nsStyleContext* aStyleContext, nscolor& aResult)
{
  MOZ_ASSERT(aValue.GetUnit() != eCSSUnit_Inherit,
             "aValue shouldn't have eCSSUnit_Inherit");
  MOZ_ASSERT(aValue.GetUnit() != eCSSUnit_Initial,
             "aValue shouldn't have eCSSUnit_Initial");
  MOZ_ASSERT(aValue.GetUnit() != eCSSUnit_Unset,
             "aValue shouldn't have eCSSUnit_Unset");

  RuleNodeCacheConditions conditions;
  bool ok = SetColor(aValue, NS_RGB(0, 0, 0), aPresContext, aStyleContext,
                     aResult, conditions);
  MOZ_ASSERT(ok || !(aPresContext && aStyleContext));
  return ok;
}

/* static */ bool
nsRuleNode::ParentHasPseudoElementData(nsStyleContext* aContext)
{
  nsStyleContext* parent = aContext->GetParent();
  return parent && parent->HasPseudoElementData();
}

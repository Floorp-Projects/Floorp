/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * a node in the lexicographic tree of rules that match an element,
 * responsible for converting the rules' information into computed style
 */

#include "nsRuleNode.h"

#include <algorithm>
#include <functional>

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/dom/AnimationEffectReadOnlyBinding.h" // for PlaybackDirection
#include "mozilla/Likely.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/Maybe.h"
#include "mozilla/OperatorNewExtensions.h"
#include "mozilla/Unused.h"

#include "mozilla/css/Declaration.h"
#include "mozilla/TypeTraits.h"

#include "gfxContext.h"
#include "nsAlgorithm.h" // for clamped()
#include "nscore.h"
#include "nsCRT.h" // for IsAscii()
#include "nsIWidget.h"
#include "nsIPresShell.h"
#include "nsFontMetrics.h"
#include "gfxFont.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSPseudoElements.h"
#include "nsThemeConstants.h"
#include "PLDHashTable.h"
#include "GeckoStyleContext.h"
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
#include "nsStyleUtil.h"
#include "nsIDocument.h"
#include "prtime.h"
#include "CSSVariableResolver.h"
#include "nsCSSParser.h"
#include "CounterStyleManager.h"
#include "nsCSSPropertyIDSet.h"
#include "mozilla/RuleNodeCacheConditions.h"
#include "nsDeviceContext.h"
#include "nsQueryObject.h"
#include "nsUnicodeProperties.h"

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

namespace mozilla {

enum UnsetAction
{
  eUnsetInitial,
  eUnsetInherit
};

} // namespace mozilla

void*
nsConditionalResetStyleData::GetConditionalStyleData(nsStyleStructID aSID,
                               GeckoStyleContext* aStyleContext) const
{
  Entry* e = static_cast<Entry*>(mEntries[aSID]);
  MOZ_ASSERT(e, "if mConditionalBits bit is set, we must have at least one "
                "conditional style struct");
  do {
    if (e->mConditions.Matches(aStyleContext)) {
      void* data = e->mStyleStruct;

      // For reset structs with conditions, we cache the data on the
      // style context.
      // Tell the style context that it doesn't own the data
      aStyleContext->AddStyleBit(GetBitForSID(aSID));
      aStyleContext->SetStyle(aSID, data);

      return data;
    }
    e = e->mNext;
  } while (e);
  return nullptr;
}

// Creates an imgRequestProxy based on the specified value in aValue and
// returns it.  If the nsPresContext is static (e.g. for printing), then
// a static request (i.e. showing the first frame, without animation)
// will be created.
static already_AddRefed<imgRequestProxy>
CreateImageRequest(nsPresContext* aPresContext, const nsCSSValue& aValue)
{
  RefPtr<imgRequestProxy> req =
    aValue.GetPossiblyStaticImageValue(aPresContext->Document(),
                                       aPresContext);
  return req.forget();
}

static already_AddRefed<nsStyleImageRequest>
CreateStyleImageRequest(nsPresContext* aPresContext, const nsCSSValue& aValue,
                        nsStyleImageRequest::Mode aModeFlags =
                          nsStyleImageRequest::Mode::Track)
{
  css::ImageValue* imageValue = aValue.GetImageStructValue();
  ImageTracker* imageTracker =
    (aModeFlags & nsStyleImageRequest::Mode::Track)
    ? aPresContext->Document()->ImageTracker()
    : nullptr;
  RefPtr<imgRequestProxy> proxy = CreateImageRequest(aPresContext, aValue);
  RefPtr<nsStyleImageRequest> request =
    new nsStyleImageRequest(aModeFlags, proxy, imageValue, imageTracker);
  return request.forget();
}

static void
SetStyleShapeSourceToCSSValue(StyleShapeSource* aShapeSource,
                              const nsCSSValue* aValue,
                              GeckoStyleContext* aStyleContext,
                              nsPresContext* aPresContext,
                              RuleNodeCacheConditions& aConditions);

/* Helper function to convert a CSS <position> specified value into its
 * computed-style form. */
static void
ComputePositionValue(GeckoStyleContext* aStyleContext,
                     const nsCSSValue& aValue,
                     Position& aComputedValue,
                     RuleNodeCacheConditions& aConditions);

/*
 * For storage of an |nsRuleNode|'s children in a PLDHashTable.
 */

struct ChildrenHashEntry : public PLDHashEntryHdr {
  // key is |mRuleNode->GetKey()|
  nsRuleNode *mRuleNode;
};

/* static */ PLDHashNumber
nsRuleNode::ChildrenHashHashKey(const void *aKey)
{
  const nsRuleNode::Key *key =
    static_cast<const nsRuleNode::Key*>(aKey);
  // Disagreement on importance and level for the same rule is extremely
  // rare, so hash just on the rule.
  return PLDHashTable::HashVoidPtrKeyStub(key->mRule);
}

/* static */ bool
nsRuleNode::ChildrenHashMatchEntry(const PLDHashEntryHdr *aHdr,
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
  PLDHashTable::MoveEntryStub,
  PLDHashTable::ClearEntryStub,
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
nsRuleNode::EnsureBlockDisplay(StyleDisplay& display,
                               bool aConvertListItem /* = false */)
{
  // see if the display value is already a block
  switch (display) {
  case StyleDisplay::ListItem:
    if (aConvertListItem) {
      display = StyleDisplay::Block;
      break;
    } // else, fall through to share the 'break' for non-changing display vals
    MOZ_FALLTHROUGH;
  case StyleDisplay::None:
  case StyleDisplay::Contents:
    // never change display:none or display:contents *ever*
  case StyleDisplay::Table:
  case StyleDisplay::Block:
  case StyleDisplay::Flex:
  case StyleDisplay::WebkitBox:
  case StyleDisplay::Grid:
  case StyleDisplay::FlowRoot:
    // do not muck with these at all - already blocks
    // This is equivalent to nsStyleDisplay::IsBlockOutside.  (XXX Maybe we
    // should just call that?)
    // This needs to match the check done in
    // nsCSSFrameConstructor::FindMathMLData for <math>.
    break;

  case StyleDisplay::InlineTable:
    // make inline tables into tables
    display = StyleDisplay::Table;
    break;

  case StyleDisplay::InlineFlex:
    // make inline flex containers into flex containers
    display = StyleDisplay::Flex;
    break;

  case StyleDisplay::WebkitInlineBox:
    // make -webkit-inline-box containers into -webkit-box containers
    display = StyleDisplay::WebkitBox;
    break;

  case StyleDisplay::InlineGrid:
    // make inline grid containers into grid containers
    display = StyleDisplay::Grid;
    break;

  default:
    // make it a block
    display = StyleDisplay::Block;
  }
}

// EnsureInlineDisplay:
//  - if the display value (argument) is not an inline type
//    then we set it to a valid inline display value
/* static */
void
nsRuleNode::EnsureInlineDisplay(StyleDisplay& display)
{
  // see if the display value is already inline
  switch (display) {
    case StyleDisplay::Block:
    case StyleDisplay::FlowRoot:
      display = StyleDisplay::InlineBlock;
      break;
    case StyleDisplay::Table:
      display = StyleDisplay::InlineTable;
      break;
    case StyleDisplay::Flex:
      display = StyleDisplay::InlineFlex;
      break;
    case StyleDisplay::WebkitBox:
      display = StyleDisplay::WebkitInlineBox;
      break;
    case StyleDisplay::Grid:
      display = StyleDisplay::InlineGrid;
      break;
    case StyleDisplay::MozBox:
      display = StyleDisplay::MozInlineBox;
      break;
    case StyleDisplay::MozStack:
      display = StyleDisplay::MozInlineStack;
      break;
    default:
      break; // Do nothing
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
                           public css::FloatCoeffsAlreadyNormalizedOps
{
  // Declare that we have floats as coefficients so that we unambiguously
  // resolve coeff_type (BasicCoordCalcOps and FloatCoeffsAlreadyNormalizedOps
  // both have |typedef float coeff_type|).
  typedef float coeff_type;

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

  bool ComputeLeafValue(result_type& aResult, const nsCSSValue& aValue)
  {
    aResult = CalcLengthWith(aValue, mFontSize, mStyleFont,
                             mStyleContext, mPresContext,
                             mUseProvidedRootEmSize, mUseUserFontSet,
                             mConditions);
    return true;
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

/* static */
already_AddRefed<nsFontMetrics>
nsRuleNode::GetMetricsFor(nsPresContext* aPresContext,
                          nsStyleContext* aStyleContext,
                          const nsStyleFont* aStyleFont,
                          nscoord aFontSize, // overrides value from aStyleFont
                          bool aUseUserFontSet)
{
  bool isVertical = false;
  if (aStyleContext) {
    WritingMode wm(aStyleContext);
    if (wm.IsVertical() && !wm.IsSideways()) {
      isVertical = true;
    }
  }
  return nsLayoutUtils::GetMetricsFor(
      aPresContext, isVertical, aStyleFont, aFontSize, aUseUserFontSet,
      nsLayoutUtils::FlushUserFontSet::Yes);
}

static nsSize CalcViewportUnitsScale(nsPresContext* aPresContext)
{
  // The caller is making use of viewport units, so notify the style set
  // that it will need to rebuild the rule tree if the size of the viewport
  // changes.
  // It is possible for this to be called on a Servo-styled document,from
  // media query evaluation outside stylesheets.
  if (nsStyleSet* styleSet = aPresContext->StyleSet()->GetAsGecko()) {
    styleSet->SetUsesViewportUnits(true);
  }

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
      RefPtr<gfxContext> context =
        aPresContext->PresShell()->CreateReferenceRenderingContext();
      nsMargin sizes(scrollFrame->GetDesiredScrollbarSizes(aPresContext, context));

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

// If |aStyleFont| is nullptr, aStyleContext->StyleFont() is used.
//
// In case that |aValue| is rem unit, if |aStyleContext| is null, callers must
// specify a valid |aStyleFont| and |aUseProvidedRootEmSize| must be true so
// that we can get the length from |aStyleFont|.
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
  NS_ASSERTION(aStyleContext || aUseProvidedRootEmSize,
               "Must have style context or specify aUseProvidedRootEmSize");
  NS_ASSERTION(aPresContext, "Must have prescontext");

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
    nscoord result;
    if (!css::ComputeCalc(result, aValue, ops)) {
      MOZ_ASSERT_UNREACHABLE("unexpected ComputeCalc failure");
    }
    return result;
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
      // FIXME(emilio, bug 1384656): We can reach this from servo right now...
      } else if (aStyleContext && !aStyleContext->AsGecko()->GetParent()) {
        // This is the root element (XXX we don't really know this, but
        // nsRuleNode::SetFont makes the same assumption!), so we should
        // use StyleFont on this context to get the root element's
        // font size.
        rootFontSize = styleFont->mFont.size;
      } else {
        // This is not the root element or we are calculating something other
        // than font size, so rem is relative to the root element's font size.
        // Find the root style context by walking up the style context tree.
        // NOTE: We should not call ResolveStyleFor() against the root element
        // to obtain the root style here because it may lead to reentrant call
        // of nsStyleSet::GetContext().
        GeckoStyleContext* rootStyle = aStyleContext->AsGecko();
        while (rootStyle->GetParent()) {
          rootStyle = rootStyle->GetParent();
        }
        const nsStyleFont *rootStyleFont = rootStyle->StyleFont();
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
      if (aValue.GetFloatValue() == 0.0f) {
        // Don't call SetFontSizeDependency for '0em'.
        return 0;
      }
      // CSS2.1 specifies that this unit scales to the computed font
      // size, not the em-width in the font metrics, despite the name.
      aConditions.SetFontSizeDependency(aFontSize);
      return ScaleCoordRound(aValue, float(aFontSize));
    }
    case eCSSUnit_XHeight: {
      aPresContext->SetUsesExChUnits(true);
      RefPtr<nsFontMetrics> fm =
        nsRuleNode::GetMetricsFor(aPresContext, aStyleContext, styleFont,
                                  aFontSize, aUseUserFontSet);
      aConditions.SetUncacheable();
      return ScaleCoordRound(aValue, float(fm->XHeight()));
    }
    case eCSSUnit_Char: {
      aPresContext->SetUsesExChUnits(true);
      RefPtr<nsFontMetrics> fm =
        nsRuleNode::GetMetricsFor(aPresContext, aStyleContext, styleFont,
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

struct LengthPercentPairCalcOps : public css::FloatCoeffsAlreadyNormalizedOps
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

  bool ComputeLeafValue(result_type& aResult, const nsCSSValue& aValue)
  {
    if (aValue.GetUnit() == eCSSUnit_Percent) {
      mHasPercent = true;
      aResult = result_type(0, aValue.GetPercentValue());
      return true;
    }
    aResult = result_type(CalcLength(aValue, mContext, mPresContext,
                                     mConditions),
                          0.0f);
    return true;
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
  nsRuleNode::ComputedCalc vals;
  if (!ComputeCalc(vals, aValue, ops)) {
    MOZ_ASSERT_UNREACHABLE("unexpected ComputeCalc failure");
  }

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
  nsRuleNode::ComputedCalc result;
  if (!ComputeCalc(result, aValue, ops)) {
    MOZ_ASSERT_UNREACHABLE("unexpected ComputeCalc failure");
  }
  return result;
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
  case NS_STYLE_IMAGELAYER_POSITION_LEFT:
  case NS_STYLE_IMAGELAYER_POSITION_TOP:
    return 0.0f;
  case NS_STYLE_IMAGELAYER_POSITION_RIGHT:
  case NS_STYLE_IMAGELAYER_POSITION_BOTTOM:
    return 1.0f;
  default:
    MOZ_FALLTHROUGH_ASSERT("unexpected box position value");
  case NS_STYLE_IMAGELAYER_POSITION_CENTER:
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
                       int32_t aMask, GeckoStyleContext* aStyleContext,
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
  GeckoStyleContext* dummyStyleContext = nullptr;
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
              int32_t aMask, GeckoStyleContext* aStyleContext,
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
                       nsPresContext* aPresContext, nsStyleContext* aContext,
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
      bool useStandinsForNativeColors = aPresContext &&
                                        !aPresContext->IsChrome();
      DebugOnly<nsresult> rv =
        LookAndFeel::GetColor(colorID, useStandinsForNativeColors, &aResult);
      MOZ_ASSERT(NS_SUCCEEDED(rv),
                 "Unknown enum colors should have been rejected by parser");
      result = true;
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

template<UnsetAction UnsetTo>
static void
SetComplexColor(const nsCSSValue& aValue,
                const StyleComplexColor& aParentColor,
                const StyleComplexColor& aInitialColor,
                nsPresContext* aPresContext,
                StyleComplexColor& aResult,
                RuleNodeCacheConditions& aConditions)
{
  nsCSSUnit unit = aValue.GetUnit();
  if (unit == eCSSUnit_Null) {
    return;
  }
  if (unit == eCSSUnit_Initial ||
      (UnsetTo == eUnsetInitial && unit == eCSSUnit_Unset)) {
    aResult = aInitialColor;
  } else if (unit == eCSSUnit_Inherit ||
             (UnsetTo == eUnsetInherit && unit == eCSSUnit_Unset)) {
    aConditions.SetUncacheable();
    aResult = aParentColor;
  } else if (unit == eCSSUnit_EnumColor &&
             aValue.GetIntValue() == NS_COLOR_CURRENTCOLOR) {
    aResult = StyleComplexColor::CurrentColor();
  } else if (unit == eCSSUnit_ComplexColor) {
    aResult = aValue.GetStyleComplexColorValue();
  } else if (unit == eCSSUnit_Auto) {
    aResult = StyleComplexColor::Auto();
  } else {
    nscolor resultColor;
    if (!SetColor(aValue, aParentColor.mColor, aPresContext,
                  nullptr, resultColor, aConditions)) {
      MOZ_ASSERT_UNREACHABLE("Unknown color value");
      return;
    }
    aResult = StyleComplexColor::FromColor(resultColor);
  }
}

template<UnsetAction UnsetTo>
static Maybe<nscoord>
ComputeLineWidthValue(const nsCSSValue& aValue,
                      const nscoord aParentCoord,
                      const nscoord aInitialCoord,
                      GeckoStyleContext* aStyleContext,
                      nsPresContext* aPresContext,
                      RuleNodeCacheConditions& aConditions)
{
  nsCSSUnit unit = aValue.GetUnit();
  if (unit == eCSSUnit_Initial ||
      (UnsetTo == eUnsetInitial && unit == eCSSUnit_Unset)) {
    return Some(aInitialCoord);
  } else if (unit == eCSSUnit_Inherit ||
             (UnsetTo == eUnsetInherit && unit == eCSSUnit_Unset)) {
    aConditions.SetUncacheable();
    return Some(aParentCoord);
  } else if (unit == eCSSUnit_Enumerated) {
    NS_ASSERTION(aValue.GetIntValue() == NS_STYLE_BORDER_WIDTH_THIN ||
                 aValue.GetIntValue() == NS_STYLE_BORDER_WIDTH_MEDIUM ||
                 aValue.GetIntValue() == NS_STYLE_BORDER_WIDTH_THICK,
                 "Unexpected line-width keyword!");
    return Some(nsPresContext::GetBorderWidthForKeyword(aValue.GetIntValue()));
  } else if (aValue.IsLengthUnit() ||
             aValue.IsCalcUnit()) {
    nscoord len =
      CalcLength(aValue, aStyleContext, aPresContext, aConditions);
    if (len < 0) {
      NS_ASSERTION(aValue.IsCalcUnit(),
                   "Parser should have rejected negative length!");
      len = 0;
    }
    return Some(len);
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "Missing case handling for line-width computing!");
    return Maybe<nscoord>(Nothing());
  }
}

static void SetGradientCoord(const nsCSSValue& aValue, nsPresContext* aPresContext,
                             GeckoStyleContext* aContext, nsStyleCoord& aResult,
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
                        GeckoStyleContext* aContext, nsStyleGradient& aResult,
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
  aResult.mMozLegacySyntax = gradient->mIsMozLegacySyntax;

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
static void SetStyleImageToImageRect(GeckoStyleContext* aStyleContext,
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
    nsPresContext* pc = aStyleContext->PresContext();
    aResult.SetImageRequest(CreateStyleImageRequest(pc, arr->Item(1)));
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
  aResult.SetCropRect(MakeUnique<nsStyleSides>(cropRect));
}

static void SetStyleImage(GeckoStyleContext* aStyleContext,
                          const nsCSSValue& aValue,
                          nsStyleImage& aResult,
                          RuleNodeCacheConditions& aConditions)
{
  if (aValue.GetUnit() == eCSSUnit_Null) {
    return;
  }

  aResult.SetNull();

  nsPresContext* presContext = aStyleContext->PresContext();
  switch (aValue.GetUnit()) {
    case eCSSUnit_Image:
      aResult.SetImageRequest(CreateStyleImageRequest(presContext, aValue));
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
      SetGradient(aValue, presContext, aStyleContext, *gradient, aConditions);
      aResult.SetGradientData(gradient);
      break;
    }
    case eCSSUnit_Element:
    {
      RefPtr<nsAtom> atom = NS_Atomize(aValue.GetStringBufferValue());
      aResult.SetElementId(atom.forget());
      break;
    }
    case eCSSUnit_Initial:
    case eCSSUnit_Unset:
    case eCSSUnit_None:
      break;
    case eCSSUnit_URL:
    {
#ifdef DEBUG
      // eCSSUnit_URL is expected only if
      // 1. we have eCSSUnit_URL values for if-visited style contexts, which
      //    we can safely treat like 'none'.
      // 2. aValue is a local-ref URL, e.g. url(#foo).
      // 3. aValue is a not a local-ref URL, but it refers to an element in
      //    the current document. For example, the url of the current document
      //    is "http://foo.html" and aValue is url(http://foo.html#foo).
      //
      // We skip image download in TryToStartImageLoadOnValue under #2 and #3,
      // and that's part of reasons we get eCSSUnit_URL instead of
      // eCSSUnit_Image here.

      // Check #2.
      bool isLocalRef = aValue.GetURLStructValue()->IsLocalRef();

      // Check #3.
      bool isEqualExceptRef = false;
      if (!isLocalRef) {
        nsIDocument* currentDoc = presContext->Document();
        nsIURI* docURI = currentDoc->GetDocumentURI();
        nsIURI* imageURI = aValue.GetURLValue();
        imageURI->EqualsExceptRef(docURI, &isEqualExceptRef);
      }

      MOZ_ASSERT(aStyleContext->IsStyleIfVisited() || isEqualExceptRef ||
                 isLocalRef,
                 "unexpected unit; maybe nsCSSValue::Image::Image() failed?");
#endif
      aResult.SetURLValue(do_AddRef(aValue.GetURLStructValue()));
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected Unit type.");
      break;
  }
}

struct SetEnumValueHelper
{
  template<typename FieldT>
  static void SetIntegerValue(FieldT&, const nsCSSValue&)
  {
    // FIXME Is it possible to turn this assertion into a compilation error?
    MOZ_ASSERT_UNREACHABLE("inappropriate unit");
  }

#define DEFINE_ENUM_CLASS_SETTER(type_, min_, max_) \
  static void SetEnumeratedValue(type_& aField, const nsCSSValue& aValue) \
  { \
    auto value = aValue.GetIntValue(); \
    MOZ_ASSERT(value >= static_cast<decltype(value)>(type_::min_) && \
               value <= static_cast<decltype(value)>(type_::max_), \
               "inappropriate value"); \
    aField = static_cast<type_>(value); \
  }

  DEFINE_ENUM_CLASS_SETTER(StyleBoxAlign, Stretch, End)
  DEFINE_ENUM_CLASS_SETTER(StyleBoxDecorationBreak, Slice, Clone)
  DEFINE_ENUM_CLASS_SETTER(StyleBoxDirection, Normal, Reverse)
  DEFINE_ENUM_CLASS_SETTER(StyleBoxOrient, Horizontal, Vertical)
  DEFINE_ENUM_CLASS_SETTER(StyleBoxPack, Start, Justify)
  DEFINE_ENUM_CLASS_SETTER(StyleBoxSizing, Content, Border)
  DEFINE_ENUM_CLASS_SETTER(StyleClear, None, Both)
  DEFINE_ENUM_CLASS_SETTER(StyleContent, OpenQuote, AltContent)
  DEFINE_ENUM_CLASS_SETTER(StyleFillRule, Nonzero, Evenodd)
  DEFINE_ENUM_CLASS_SETTER(StyleFloat, None, InlineEnd)
  DEFINE_ENUM_CLASS_SETTER(StyleFloatEdge, ContentBox, MarginBox)
  DEFINE_ENUM_CLASS_SETTER(StyleHyphens, None, Auto)
  DEFINE_ENUM_CLASS_SETTER(StyleOverscrollBehavior, Auto, None)
  DEFINE_ENUM_CLASS_SETTER(StyleStackSizing, Ignore, IgnoreVertical)
  DEFINE_ENUM_CLASS_SETTER(StyleTextJustify, None, InterCharacter)
  DEFINE_ENUM_CLASS_SETTER(StyleUserFocus, None, SelectMenu)
  DEFINE_ENUM_CLASS_SETTER(StyleUserSelect, None, MozText)
  DEFINE_ENUM_CLASS_SETTER(StyleUserInput, None, Auto)
  DEFINE_ENUM_CLASS_SETTER(StyleUserModify, ReadOnly, WriteOnly)
  DEFINE_ENUM_CLASS_SETTER(StyleWindowDragging, Default, NoDrag)
  DEFINE_ENUM_CLASS_SETTER(StyleOrient, Inline, Vertical)
  DEFINE_ENUM_CLASS_SETTER(StyleGeometryBox, BorderBox, ViewBox)
  DEFINE_ENUM_CLASS_SETTER(StyleWhiteSpace, Normal, PreSpace)
#ifdef MOZ_XUL
  DEFINE_ENUM_CLASS_SETTER(StyleDisplay, None, MozPopup)
#else
  DEFINE_ENUM_CLASS_SETTER(StyleDisplay, None, InlineBox)
#endif

#undef DEF_SET_ENUMERATED_VALUE
};

template<typename FieldT>
struct SetIntegerValueHelper
{
  static void SetIntegerValue(FieldT& aField, const nsCSSValue& aValue)
  {
    aField = aValue.GetIntValue();
  }
  static void SetEnumeratedValue(FieldT& aField, const nsCSSValue& aValue)
  {
    aField = aValue.GetIntValue();
  }
};

template<typename FieldT>
struct SetValueHelper : Conditional<IsEnum<FieldT>::value,
                                    SetEnumValueHelper,
                                    SetIntegerValueHelper<FieldT>>::Type
{
  template<typename ValueT>
  static void SetValue(FieldT& aField, const ValueT& aValue)
  {
    aField = aValue;
  }
  static void SetValue(FieldT&, unused_t)
  {
    // FIXME Is it possible to turn this assertion into a compilation error?
    MOZ_ASSERT_UNREACHABLE("inappropriate unit");
  }
};


// flags for SetValue - align values with SETCOORD_* constants
// where possible

#define SETVAL_INTEGER                0x40   // I
#define SETVAL_ENUMERATED             0x80   // E
#define SETVAL_UNSET_INHERIT          0x00400000
#define SETVAL_UNSET_INITIAL          0x00800000

// no caller cares whether aField was changed or not
template<typename FieldT, typename InitialT,
         typename AutoT, typename NoneT, typename NormalT, typename SysFontT>
static void
SetValue(const nsCSSValue& aValue, FieldT& aField,
         RuleNodeCacheConditions& aConditions, uint32_t aMask,
         FieldT aParentValue,
         InitialT aInitialValue,
         AutoT aAutoValue,
         NoneT aNoneValue,
         NormalT aNormalValue,
         SysFontT aSystemFontValue)
{
  typedef SetValueHelper<FieldT> Helper;

  switch (aValue.GetUnit()) {
  case eCSSUnit_Null:
    return;

    // every caller of SetValue provides inherit and initial
    // alternatives, so we don't require them to say so in the mask
  case eCSSUnit_Inherit:
    aConditions.SetUncacheable();
    aField = aParentValue;
    return;

  case eCSSUnit_Initial:
    Helper::SetValue(aField, aInitialValue);
    return;

    // every caller provides one or other of these alternatives,
    // but they have to say which
  case eCSSUnit_Enumerated:
    if (aMask & SETVAL_ENUMERATED) {
      Helper::SetEnumeratedValue(aField, aValue);
      return;
    }
    break;

  case eCSSUnit_Integer:
    if (aMask & SETVAL_INTEGER) {
      Helper::SetIntegerValue(aField, aValue);
      return;
    }
    break;

    // remaining possibilities in descending order of frequency of use
  case eCSSUnit_Auto:
    Helper::SetValue(aField, aAutoValue);
    return;

  case eCSSUnit_None:
    Helper::SetValue(aField, aNoneValue);
    return;

  case eCSSUnit_Normal:
    Helper::SetValue(aField, aNormalValue);
    return;

  case eCSSUnit_System_Font:
    Helper::SetValue(aField, aSystemFontValue);
    return;

  case eCSSUnit_Unset:
    if (aMask & SETVAL_UNSET_INHERIT) {
      aConditions.SetUncacheable();
      aField = aParentValue;
      return;
    }
    if (aMask & SETVAL_UNSET_INITIAL) {
      Helper::SetValue(aField, aInitialValue);
      return;
    }
    break;

  default:
    break;
  }

  NS_NOTREACHED("SetValue: inappropriate unit");
}

template <typename FieldT, typename T1>
static void
SetValue(const nsCSSValue& aValue, FieldT& aField,
         RuleNodeCacheConditions& aConditions, uint32_t aMask,
         FieldT aParentValue, T1 aInitialValue)
{
  SetValue(aValue, aField, aConditions, aMask, aParentValue,
           aInitialValue, Unused, Unused, Unused, Unused);
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

static void
SetTransformValue(const nsCSSValue& aValue,
                  RefPtr<nsCSSValueSharedList>& aField,
                  RuleNodeCacheConditions& aConditions,
                  nsCSSValueSharedList* const aParentValue)
{
  /* Convert the nsCSSValueList into an nsTArray<nsTransformFunction *>. */
  switch (aValue.GetUnit()) {
  case eCSSUnit_Null:
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
  case eCSSUnit_None:
    aField = nullptr;
    break;

  case eCSSUnit_Inherit:
    aField = aParentValue;
    aConditions.SetUncacheable();
    break;

  case eCSSUnit_SharedList: {
    nsCSSValueSharedList* list = aValue.GetSharedListValue();
    nsCSSValueList* head = list->mHead;
    MOZ_ASSERT(head, "transform list must have at least one item");
    // can get a _None in here from transform animation
    if (head->mValue.GetUnit() == eCSSUnit_None) {
      MOZ_ASSERT(head->mNext == nullptr, "none must be alone");
      aField = nullptr;
    } else {
      aField = list;
    }
    break;
  }

  default:
    MOZ_ASSERT(false, "unrecognized transform unit");
  }
}

void*
nsRuleNode::operator new(size_t sz, nsPresContext* aPresContext)
{
  // Check the recycle list first.
  return aPresContext->PresShell()->AllocateByObjectID(eArenaObjectID_nsRuleNode, sz);
}

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void
nsRuleNode::Destroy()
{
  // Destroy ourselves.
  this->~nsRuleNode();

  // Don't let the memory be freed, since it will be recycled
  // instead. Don't call the global operator delete.
  mPresContext->PresShell()->FreeByObjectID(eArenaObjectID_nsRuleNode, this);
}

already_AddRefed<nsRuleNode>
nsRuleNode::CreateRootNode(nsPresContext* aPresContext)
{
  return do_AddRef(new (aPresContext)
    nsRuleNode(aPresContext, nullptr, nullptr, SheetType::Unknown, false));
}

nsRuleNode::nsRuleNode(nsPresContext* aContext, nsRuleNode* aParent,
                       nsIStyleRule* aRule, SheetType aLevel,
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

  NS_ASSERTION(IsRoot() || GetLevel() == aLevel, "not enough bits");
  NS_ASSERTION(IsRoot() || IsImportantRule() == aIsImportant, "yikes");
  MOZ_ASSERT(aContext->StyleSet()->IsGecko(),
             "ServoStyleSets should not have rule nodes");
  aContext->StyleSet()->AsGecko()->RuleNodeUnused(this, /* aMayGC = */ false);

  // nsStyleSet::GetContext depends on there being only one animation
  // rule.
  MOZ_ASSERT(IsRoot() || GetLevel() != SheetType::Animation ||
             mParent->IsRoot() ||
             mParent->GetLevel() != SheetType::Animation,
             "must be only one rule at animation level");
}

nsRuleNode::~nsRuleNode()
{
  MOZ_ASSERT(!HaveChildren());
  MOZ_COUNT_DTOR(nsRuleNode);
  if (mParent) {
    mParent->RemoveChild(this);
  }

  if (mStyleData.mResetData || mStyleData.mInheritedData)
    mStyleData.Destroy(mDependentBits, mPresContext);
}

nsRuleNode*
nsRuleNode::Transition(nsIStyleRule* aRule, SheetType aLevel,
                       bool aIsImportantRule)
{
#ifdef DEBUG
  {
    RefPtr<css::Declaration> declaration(do_QueryObject(aRule));
    MOZ_ASSERT(!declaration || !declaration->IsMutable(),
               "caller must call Declaration::SetImmutable first");
  }
#endif

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
    auto entry =
      static_cast<ChildrenHashEntry*>(ChildrenHash()->Add(&key, fallible));
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
    Key key = curr->GetKey();
    // This will never fail because of the initial size we gave the table.
    auto entry =
      static_cast<ChildrenHashEntry*>(hash->Add(&key));
    NS_ASSERTION(!entry->mRuleNode, "duplicate entries in list");
    entry->mRuleNode = curr;
  }
  SetChildrenHash(hash);
}

void
nsRuleNode::RemoveChild(nsRuleNode* aNode)
{
  MOZ_ASSERT(HaveChildren());
  if (ChildrenAreHashed()) {
    PLDHashTable* children = ChildrenHash();
    Key key = aNode->GetKey();
    MOZ_ASSERT(children->Search(&key));
    children->Remove(&key);
    if (children->EntryCount() == 0) {
      delete children;
      mChildren.asVoid = nullptr;
    }
  } else {
    // This linear traversal is unfortunate, but we do the same thing when
    // adding nodes. The traversal is bounded by kMaxChildrenInList.
    nsRuleNode** curr = &mChildren.asList;
    while (*curr != aNode) {
      curr = &((*curr)->mNextSibling);
      MOZ_ASSERT(*curr);
    }
    *curr = (*curr)->mNextSibling;

    // If there was one element in the list, this sets mChildren.asList
    // to 0, and HaveChildren() will return false.
  }
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
nsRuleNode::PropagateGrandancestorBit(GeckoStyleContext* aContext,
                                      GeckoStyleContext* aContextInheritedFrom)
{
  MOZ_ASSERT(aContext);
  MOZ_ASSERT(aContextInheritedFrom &&
             aContextInheritedFrom != aContext,
             "aContextInheritedFrom must be an ancestor of aContext");

  for (GeckoStyleContext* context = aContext->GetParent();
       context != aContextInheritedFrom;
       context = context->GetParent()) {
    if (!context) {
      MOZ_ASSERT(false, "aContextInheritedFrom must be an ancestor of "
                        "aContext's parent");
      break;
    }
    context->AddStyleBit(NS_STYLE_CHILD_USES_GRANDANCESTOR_STYLE);
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

static const uint32_t gEffectsFlags[] = {
#define CSS_PROP_EFFECTS FLAG_DATA_FOR_PROPERTY
#include "nsCSSPropList.h"
#undef CSS_PROP_EFFECTS
};

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
GetPseudoRestriction(GeckoStyleContext *aContext)
{
  // This needs to match nsStyleSet::WalkRestrictionRule.
  uint32_t pseudoRestriction = 0;
  nsAtom *pseudoType = aContext->GetPseudo();
  if (pseudoType) {
    if (pseudoType == nsCSSPseudoElements::firstLetter) {
      pseudoRestriction = CSS_PROPERTY_APPLIES_TO_FIRST_LETTER;
    } else if (pseudoType == nsCSSPseudoElements::firstLine) {
      pseudoRestriction = CSS_PROPERTY_APPLIES_TO_FIRST_LINE;
    } else if (pseudoType == nsCSSPseudoElements::placeholder) {
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

AutoCSSValueArray::AutoCSSValueArray(void* aStorage, size_t aCount)
{
  MOZ_ASSERT(size_t(aStorage) % NS_ALIGNMENT_OF(nsCSSValue) == 0,
             "bad alignment from alloca");
  mCount = aCount;
  // Don't use placement new[], since it might store extra data
  // for the count (on Windows!).
  mArray = static_cast<nsCSSValue*>(aStorage);
  for (size_t i = 0; i < mCount; ++i) {
    new (KnownNotNull, mArray + i) nsCSSValue();
  }
}

AutoCSSValueArray::~AutoCSSValueArray()
{
  for (size_t i = 0; i < mCount; ++i) {
    mArray[i].~nsCSSValue();
  }
}

/* static */ bool
nsRuleNode::ResolveVariableReferences(const nsStyleStructID aSID,
                                      nsRuleData* aRuleData,
                                      GeckoStyleContext* aContext)
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

    MOZ_ASSERT(tokenStream->mLevel != SheetType::Count,
               "Token stream should have a defined level");

    AutoRestore<SheetType> saveLevel(aRuleData->mLevel);
    aRuleData->mLevel = tokenStream->mLevel;

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
                         GeckoStyleContext* aContext)
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

  MOZ_ASSERT(!(aSID == eStyleStruct_Variables && startStruct),
             "if we start caching Variables structs in the rule tree, then "
             "not forcing detail to eRulePartialMixed just below is no "
             "longer valid");

  if (detail == eRuleNone && isReset) {
    // We specified absolutely no rule information for a reset struct, and we
    // may or may not have found a parent rule in the tree that specified all
    // the rule information.  Regardless, we don't need to use any cache
    // conditions if we cache this struct in the rule tree.
    //
    // Normally ruleData.mConditions would already indicate that the struct
    // is cacheable without conditions if detail is eRuleNone, but because
    // of the UnsetPropertiesWithoutFlags call above, we may have encountered
    // some rules with dependencies, which we then cleared out of ruleData.
    //
    // ruleData.mConditions could also indicate we are not cacheable at all,
    // such as when AnimValuesStyleRule prevents us from caching structs
    // when attempting to apply animations to pseudos.
    //
    // So if we we are uncacheable, we leave it, but if we are cacheable
    // with dependencies, we convert that to cacheable without dependencies.
    if (ruleData.mConditions.CacheableWithDependencies()) {
      MOZ_ASSERT(pseudoRestriction,
                 "should only be cacheable with dependencies if we had a "
                 "pseudo restriction");
      ruleData.mConditions.Clear();
    } else {
      // XXXheycam We shouldn't have `|| GetLevel() == SheetType::Transition`
      // in the assertion condition, but rule nodes created by
      // ResolveStyleByAddingRules don't call SetIsAnimationRule().
      MOZ_ASSERT(ruleData.mConditions.CacheableWithoutDependencies() ||
                 ((HasAnimationData() ||
                   GetLevel() == SheetType::Transition) &&
                  aContext->GetParent() &&
                  aContext->GetParent()->HasPseudoElementData()),
                 "should only be uncacheable if we had an animation rule "
                 "and we're inside a pseudo");
    }
  }

  if (!ruleData.mConditions.CacheableWithoutDependencies() &&
      aSID != eStyleStruct_Variables) {
    // Treat as though some data is specified to avoid the optimizations and
    // force data computation.
    //
    // We don't need to do this for Variables structs since we know those are
    // never cached in the rule tree, and it avoids wasteful computation of a
    // new Variables struct when we have no additional variable declarations,
    // which otherwise could happen when there is an AnimValuesStyleRule
    // (which calls SetUncacheable for style contexts with pseudo data).
    detail = eRulePartialMixed;
  }

  if (detail == eRuleNone && startStruct) {
    // We specified absolutely no rule information, but a parent rule in the tree
    // specified all the rule information.  We set a bit along the branch from our
    // node in the tree to the node that specified the data that tells nodes on that
    // branch that they never need to examine their rules for this particular struct type
    // ever again.
    PropagateDependentBit(aSID, ruleNode, startStruct);
    // For inherited structs, mark the struct (which will be set on
    // the context by our caller) as not being owned by the context.
    if (!isReset) {
      aContext->AddStyleBit(nsCachedStyleData::GetBitForSID(aSID));
    } else if (HasAnimationData()) {
      // If we have animation data, the struct should be cached on the style
      // context so that we can peek the struct.
      // See comment in AnimValuesStyleRule::MapRuleInfoInto.
      StoreStyleOnContext(aContext, aSID, startStruct);
    }

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
    GeckoStyleContext* parentContext = aContext->GetParent();
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
      if (isReset) {
        parentContext->AddStyleBit(NS_STYLE_HAS_CHILD_THAT_USES_RESET_STYLE);
      }
      return parentStruct;
    }
    else
      // We are the root.  In the case of fonts, the default values just
      // come from the pres context.
      return SetDefaultOnRoot(aSID, aContext);
  }

  typedef const void* (nsRuleNode::*ComputeFunc)(void*, const nsRuleData*,
                                                 GeckoStyleContext*, nsRuleNode*,
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
nsRuleNode::SetDefaultOnRoot(const nsStyleStructID aSID, GeckoStyleContext* aContext)
{
  switch (aSID) {
    case eStyleStruct_Font:
    {
      nsStyleFont* fontData = new (mPresContext) nsStyleFont(mPresContext);
      aContext->SetStyle(eStyleStruct_Font, fontData);
      return fontData;
    }
    case eStyleStruct_Display:
    {
      nsStyleDisplay* disp = new (mPresContext) nsStyleDisplay(mPresContext);
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
      nsStyleText* text = new (mPresContext) nsStyleText(mPresContext);
      aContext->SetStyle(eStyleStruct_Text, text);
      return text;
    }
    case eStyleStruct_TextReset:
    {
      nsStyleTextReset* text = new (mPresContext) nsStyleTextReset(mPresContext);
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
      nsStyleBackground* bg = new (mPresContext) nsStyleBackground(mPresContext);
      aContext->SetStyle(eStyleStruct_Background, bg);
      return bg;
    }
    case eStyleStruct_Margin:
    {
      nsStyleMargin* margin = new (mPresContext) nsStyleMargin(mPresContext);
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
      nsStylePadding* padding = new (mPresContext) nsStylePadding(mPresContext);
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
      nsStylePosition* pos = new (mPresContext) nsStylePosition(mPresContext);
      aContext->SetStyle(eStyleStruct_Position, pos);
      return pos;
    }
    case eStyleStruct_Table:
    {
      nsStyleTable* table = new (mPresContext) nsStyleTable(mPresContext);
      aContext->SetStyle(eStyleStruct_Table, table);
      return table;
    }
    case eStyleStruct_TableBorder:
    {
      nsStyleTableBorder* table = new (mPresContext) nsStyleTableBorder(mPresContext);
      aContext->SetStyle(eStyleStruct_TableBorder, table);
      return table;
    }
    case eStyleStruct_Content:
    {
      nsStyleContent* content = new (mPresContext) nsStyleContent(mPresContext);
      aContext->SetStyle(eStyleStruct_Content, content);
      return content;
    }
    case eStyleStruct_UserInterface:
    {
      nsStyleUserInterface* ui = new (mPresContext) nsStyleUserInterface(mPresContext);
      aContext->SetStyle(eStyleStruct_UserInterface, ui);
      return ui;
    }
    case eStyleStruct_UIReset:
    {
      nsStyleUIReset* ui = new (mPresContext) nsStyleUIReset(mPresContext);
      aContext->SetStyle(eStyleStruct_UIReset, ui);
      return ui;
    }
    case eStyleStruct_XUL:
    {
      nsStyleXUL* xul = new (mPresContext) nsStyleXUL(mPresContext);
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
      nsStyleSVG* svg = new (mPresContext) nsStyleSVG(mPresContext);
      aContext->SetStyle(eStyleStruct_SVG, svg);
      return svg;
    }
    case eStyleStruct_SVGReset:
    {
      nsStyleSVGReset* svgReset = new (mPresContext) nsStyleSVGReset(mPresContext);
      aContext->SetStyle(eStyleStruct_SVGReset, svgReset);
      return svgReset;
    }
    case eStyleStruct_Variables:
    {
      nsStyleVariables* vars = new (mPresContext) nsStyleVariables(mPresContext);
      aContext->SetStyle(eStyleStruct_Variables, vars);
      return vars;
    }
    case eStyleStruct_Effects:
    {
      nsStyleEffects* effects = new (mPresContext) nsStyleEffects(mPresContext);
      aContext->SetStyle(eStyleStruct_Effects, effects);
      return effects;
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
 * @param data_ Variable (declared here) holding the result of this
 *              function.
 * @param parentdata_ Variable (declared here) holding the parent style
 *                    context's data for this struct.
 */
#define COMPUTE_START_INHERITED(type_, data_, parentdata_)                    \
  NS_ASSERTION(aRuleDetail != eRuleFullInherited,                             \
               "should not have bothered calling Compute*Data");              \
                                                                              \
  GeckoStyleContext* parentContext = aContext->GetParent();                   \
                                                                              \
  nsStyle##type_* data_ = nullptr;                                            \
  mozilla::Maybe<nsStyle##type_> maybeFakeParentData;                         \
  const nsStyle##type_* parentdata_ = nullptr;                                \
  RuleNodeCacheConditions conditions = aConditions;                           \
                                                                              \
  /* If |conditions.Cacheable()| might be true by the time we're done, we */  \
  /* can't call parentContext->Style##type_() since it could recur into */    \
  /* setting the same struct on the same rule node, causing a leak. */        \
  if (aRuleDetail != eRuleFullReset &&                                        \
      (!aStartStruct || (aRuleDetail != eRulePartialReset &&                  \
                         aRuleDetail != eRuleNone))) {                        \
    if (parentContext) {                                                      \
      parentdata_ = parentContext->Style##type_();                            \
    } else {                                                                  \
      maybeFakeParentData.emplace(mPresContext);                              \
      parentdata_ = maybeFakeParentData.ptr();                                \
    }                                                                         \
  }                                                                           \
  if (eStyleStruct_##type_ == eStyleStruct_Variables)                         \
    /* no need to copy construct an nsStyleVariables, as we will copy */      \
    /* inherited variables (and call SetUncacheable()) in */                  \
    /* ComputeVariablesData */                                                \
    data_ = new (mPresContext) nsStyle##type_(mPresContext);                  \
  else if (aStartStruct)                                                      \
    /* We only need to compute the delta between this computed data and */    \
    /* our computed data. */                                                  \
    data_ = new (mPresContext)                                                \
            nsStyle##type_(*static_cast<nsStyle##type_*>(aStartStruct));      \
  else {                                                                      \
    if (aRuleDetail != eRuleFullMixed && aRuleDetail != eRuleFullReset) {     \
      /* No question. We will have to inherit. Go ahead and init */           \
      /* with inherited vals from parent. */                                  \
      conditions.SetUncacheable();                                            \
      if (parentdata_)                                                        \
        data_ = new (mPresContext) nsStyle##type_(*parentdata_);              \
      else                                                                    \
        data_ = new (mPresContext) nsStyle##type_(mPresContext);              \
    }                                                                         \
    else                                                                      \
      data_ = new (mPresContext) nsStyle##type_(mPresContext);                \
  }                                                                           \
                                                                              \
  if (!parentdata_)                                                           \
    parentdata_ = data_;

/**
 * Begin an nsRuleNode::Compute*Data function for a reset struct.
 *
 * @param type_ The nsStyle* type this function computes.
 * @param data_ Variable (declared here) holding the result of this
 *              function.
 * @param parentdata_ Variable (declared here) holding the parent style
 *                    context's data for this struct.
 */
#define COMPUTE_START_RESET(type_, data_, parentdata_)                        \
  NS_ASSERTION(aRuleDetail != eRuleFullInherited,                             \
               "should not have bothered calling Compute*Data");              \
                                                                              \
  GeckoStyleContext* parentContext = aContext->GetParent();                   \
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
    data_ = new (mPresContext) nsStyle##type_(mPresContext);                  \
                                                                              \
  /* If |conditions.Cacheable()| might be true by the time we're done, we */  \
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
      maybeFakeParentData.emplace(mPresContext);                              \
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
  MOZ_ASSERT(!conditions.CacheableWithoutDependencies() ||                    \
             aRuleDetail == eRuleFullReset ||                                 \
             (aStartStruct && aRuleDetail == eRulePartialReset),              \
             "conditions.CacheableWithoutDependencies() must be false "       \
             "for inherited structs unless all properties have been "         \
             "specified with values other than inherit");                     \
  if (conditions.CacheableWithoutDependencies()) {                            \
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
    aContext->AddStyleBit(NS_STYLE_INHERIT_BIT(type_));                       \
  }                                                                           \
  /* For inherited structs, our caller will cache the data on the */          \
  /* style context */                                                         \
                                                                              \
  return data_;

/**
 * End an nsRuleNode::Compute*Data function for a reset struct.
 *
 * @param type_ The nsStyle* type this function computes.
 * @param data_ Variable holding the result of this function.
 */
#define COMPUTE_END_RESET(type_, data_)                                       \
  MOZ_ASSERT(!conditions.CacheableWithoutDependencies() ||                    \
             aRuleDetail == eRuleNone ||                                      \
             aRuleDetail == eRulePartialReset ||                              \
             aRuleDetail == eRuleFullReset,                                   \
             "conditions.CacheableWithoutDependencies() must be false "       \
             "for reset structs if any properties were specified as "         \
             "inherit");                                                      \
  if (conditions.CacheableWithoutDependencies()) {                            \
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
    if (HasAnimationData()) {                                                 \
      /* If we have animation data, the struct should be cached on the */     \
      /* style context so that we can peek the struct. */                     \
      /* See comment in AnimValuesStyleRule::MapRuleInfoInto. */              \
      StoreStyleOnContext(aContext, eStyleStruct_##type_, data_);             \
    }                                                                         \
  } else if (conditions.Cacheable()) {                                        \
    if (!mStyleData.mResetData) {                                             \
      mStyleData.mResetData = new (mPresContext) nsConditionalResetStyleData; \
    }                                                                         \
    mStyleData.mResetData->                                                   \
      SetStyleData(eStyleStruct_##type_, mPresContext, data_, conditions);    \
    /* Tell the style context that it doesn't own the data */                 \
    aContext->AddStyleBit(NS_STYLE_INHERIT_BIT(type_));                       \
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

struct SetFontSizeCalcOps : public css::BasicCoordCalcOps,
                            public css::FloatCoeffsAlreadyNormalizedOps
{
  // Declare that we have floats as coefficients so that we unambiguously
  // resolve coeff_type (BasicCoordCalcOps and FloatCoeffsAlreadyNormalizedOps
  // both have |typedef float coeff_type|).
  typedef float coeff_type;

  // The parameters beyond aValue that we need for CalcLengthWith.
  const nscoord mParentSize;
  const nsStyleFont* const mParentFont;
  nsPresContext* const mPresContext;
  GeckoStyleContext* const mStyleContext;
  const bool mAtRoot;
  RuleNodeCacheConditions& mConditions;

  SetFontSizeCalcOps(nscoord aParentSize, const nsStyleFont* aParentFont,
                     nsPresContext* aPresContext,
                     GeckoStyleContext* aStyleContext,
                     bool aAtRoot,
                     RuleNodeCacheConditions& aConditions)
    : mParentSize(aParentSize),
      mParentFont(aParentFont),
      mPresContext(aPresContext),
      mStyleContext(aStyleContext),
      mAtRoot(aAtRoot),
      mConditions(aConditions)
  {
  }

  bool ComputeLeafValue(result_type& aResult, const nsCSSValue& aValue)
  {
    nscoord size;
    if (aValue.IsLengthUnit()) {
      // Note that font-based length units use the parent's size
      // unadjusted for scriptlevel changes. A scriptlevel change
      // between us and the parent is simply ignored.
      size = CalcLengthWith(aValue, mParentSize,
                            mParentFont,
                            mStyleContext, mPresContext, mAtRoot,
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

    aResult = size;
    return true;
  }
};

/* static */ void
nsRuleNode::SetFontSize(nsPresContext* aPresContext,
                        GeckoStyleContext* aContext,
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

      float factor = (NS_STYLE_FONT_SIZE_LARGER == value) ? 1.2f : (1.0f / 1.2f);

      *aSize = NSToCoordFloorClamped(aParentSize * factor);

      sizeIsZoomedAccordingToParent = true;

    } else {
      NS_NOTREACHED("unexpected value");
    }
  }
  else if (sizeValue->IsLengthUnit() ||
           sizeValue->GetUnit() == eCSSUnit_Percent ||
           sizeValue->IsCalcUnit()) {
    SetFontSizeCalcOps ops(aParentSize, aParentFont,
                           aPresContext, aContext,
                           aAtRoot,
                           aConditions);
    if (!css::ComputeCalc(*aSize, *sizeValue, ops)) {
      MOZ_ASSERT_UNREACHABLE("unexpected ComputeCalc failure");
    }
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
nsRuleNode::SetFont(nsPresContext* aPresContext, GeckoStyleContext* aContext,
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
  if (eCSSUnit_AtomIdent == langValue->GetUnit()) {
    aFont->mLanguage = langValue->GetAtomValue();
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
  Maybe<nsFont> lazySystemFont;
  const nsCSSValue* systemFontValue = aRuleData->ValueForSystemFont();
  if (eCSSUnit_Enumerated == systemFontValue->GetUnit()) {
    lazySystemFont.emplace(*defaultVariableFont);
    LookAndFeel::FontID fontID =
      (LookAndFeel::FontID)systemFontValue->GetIntValue();
    nsLayoutUtils::ComputeSystemFont(lazySystemFont.ptr(), fontID, aPresContext,
                                     defaultVariableFont);
  }
  const nsFont& systemFont = lazySystemFont.refOr(*defaultVariableFont);

  // font-family: font family list, enum, inherit
  switch (aRuleData->ValueForFontFamily()->GetUnit()) {
    case eCSSUnit_FontFamilyList:
      // set the correct font if we are using DocumentFonts OR we are overriding
      // for XUL - MJA: bug 31816
      nsLayoutUtils::FixupNoneGeneric(&aFont->mFont, aPresContext,
                                      aGenericFontID, defaultVariableFont);

      aFont->mFont.systemFont = false;
      // Technically this is redundant with the code below, but it's good
      // to have since we'll still want it once we get rid of
      // SetGenericFont (bug 380915).
      aFont->mGenericID = aGenericFontID;
      break;
    case eCSSUnit_System_Font:
      aFont->mFont.fontlist = systemFont.fontlist;
      aFont->mFont.systemFont = true;
      aFont->mGenericID = kGenericFont_NONE;
      break;
    case eCSSUnit_Inherit:
    case eCSSUnit_Unset:
      aConditions.SetUncacheable();
      aFont->mFont.fontlist = aParentFont->mFont.fontlist;
      aFont->mFont.systemFont = aParentFont->mFont.systemFont;
      aFont->mGenericID = aParentFont->mGenericID;
      MOZ_FALLTHROUGH;  // Fall through here to check for a lang change.
    case eCSSUnit_Null:
      // If we have inheritance (cases eCSSUnit_Inherit, eCSSUnit_Unset, and
      // eCSSUnit_Null) and a (potentially different) language is explicitly
      // specified, then we need to overwrite the inherited default generic font
      // with the default generic from defaultVariableFont, which is computed
      // using aFont->mLanguage above.
      if (aRuleData->ValueForLang()->GetUnit() != eCSSUnit_Null) {
        nsLayoutUtils::FixupNoneGeneric(&aFont->mFont, aPresContext,
                                        aGenericFontID, defaultVariableFont);
      }
      break;
    case eCSSUnit_Initial:
      aFont->mFont.fontlist = defaultVariableFont->fontlist;
      aFont->mFont.systemFont = defaultVariableFont->systemFont;
      aFont->mGenericID = kGenericFont_NONE;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected unit for font-family");
  }

  // When we're in the loop in SetGenericFont, we must ensure that we
  // always keep aFont->mFlags set to the correct generic.  But we have
  // to be careful not to touch it when we're called directly from
  // ComputeFontData, because we could have a start struct.
  if (aGenericFontID != kGenericFont_NONE) {
    aFont->mGenericID = aGenericFontID;
  }

  // -moz-math-variant: enum, inherit, initial
  SetValue(*aRuleData->ValueForMathVariant(), aFont->mMathVariant,
           aConditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           aParentFont->mMathVariant, NS_MATHML_MATHVARIANT_NONE);

  // -moz-math-display: enum, inherit, initial
  SetValue(*aRuleData->ValueForMathDisplay(), aFont->mMathDisplay,
           aConditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           aParentFont->mMathDisplay, NS_MATHML_DISPLAYSTYLE_INLINE);

  // -moz-osx-font-smoothing: enum, inherit, initial
  SetValue(*aRuleData->ValueForOsxFontSmoothing(),
           aFont->mFont.smoothing, aConditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           aParentFont->mFont.smoothing,
           defaultVariableFont->smoothing);

  // -moz-font-smoothing-background-color: color, inherit, initial
  const nsCSSValue* fsbColorValue =
    aRuleData->ValueForFontSmoothingBackgroundColor();
  if (eCSSUnit_Initial == fsbColorValue->GetUnit()) {
    aFont->mFont.fontSmoothingBackgroundColor = NS_RGBA(0, 0, 0, 0);
  } else if (eCSSUnit_Inherit == fsbColorValue->GetUnit() ||
             eCSSUnit_Unset == fsbColorValue->GetUnit()) {
    aConditions.SetUncacheable();
    aFont->mFont.fontSmoothingBackgroundColor =
      aParentFont->mFont.fontSmoothingBackgroundColor;
  } else {
    SetColor(*fsbColorValue, aParentFont->mFont.fontSmoothingBackgroundColor,
             aPresContext, aContext, aFont->mFont.fontSmoothingBackgroundColor,
             aConditions);
  }

  // font-style: enum, inherit, initial, -moz-system-font
  if (aFont->mMathVariant != NS_MATHML_MATHVARIANT_NONE) {
    // -moz-math-variant overrides font-style
    aFont->mFont.style = NS_FONT_STYLE_NORMAL;
  } else {
    SetValue(*aRuleData->ValueForFontStyle(),
             aFont->mFont.style, aConditions,
             SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
             aParentFont->mFont.style,
             defaultVariableFont->style,
             Unused, Unused, Unused, systemFont.style);
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
    SetValue(*weightValue, aFont->mFont.weight, aConditions,
             SETVAL_INTEGER | SETVAL_UNSET_INHERIT,
             aParentFont->mFont.weight,
             defaultVariableFont->weight,
             Unused, Unused, Unused, systemFont.weight);

  // font-stretch: enum, inherit, initial, -moz-system-font
  SetValue(*aRuleData->ValueForFontStretch(),
           aFont->mFont.stretch, aConditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           aParentFont->mFont.stretch,
           defaultVariableFont->stretch,
           Unused, Unused, Unused, systemFont.stretch);

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
                     aContext, aPresContext, atRoot, true /* aUseUserFontSet */,
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
  SetValue(*aRuleData->ValueForFontKerning(),
           aFont->mFont.kerning, aConditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           aParentFont->mFont.kerning,
           defaultVariableFont->kerning,
           Unused, Unused, Unused, systemFont.kerning);

  // font-synthesis: none, enum (bit field), inherit, initial
  SetValue(*aRuleData->ValueForFontSynthesis(),
           aFont->mFont.synthesis, aConditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           aParentFont->mFont.synthesis,
           defaultVariableFont->synthesis,
           Unused, /* none */ 0, Unused, Unused);

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
      MOZ_ASSERT(aPresContext->StyleSet()->IsGecko(),
                 "ServoStyleSets should not have rule nodes");
      aFont->mFont.featureValueLookup = aPresContext->GetFontFeatureValuesLookup();

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
  SetValue(*aRuleData->ValueForFontVariantCaps(),
           aFont->mFont.variantCaps, aConditions,
           SETVAL_ENUMERATED |SETVAL_UNSET_INHERIT,
           aParentFont->mFont.variantCaps,
           defaultVariableFont->variantCaps,
           Unused, Unused, /* normal */ 0, systemFont.variantCaps);

  // font-variant-east-asian: normal, enum (bit field), inherit, initial,
  //                          -moz-system-font
  SetValue(*aRuleData->ValueForFontVariantEastAsian(),
           aFont->mFont.variantEastAsian, aConditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           aParentFont->mFont.variantEastAsian,
           defaultVariableFont->variantEastAsian,
           Unused, Unused, /* normal */ 0, systemFont.variantEastAsian);

  // font-variant-ligatures: normal, none, enum (bit field), inherit, initial,
  //                         -moz-system-font
  SetValue(*aRuleData->ValueForFontVariantLigatures(),
           aFont->mFont.variantLigatures, aConditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           aParentFont->mFont.variantLigatures,
           defaultVariableFont->variantLigatures,
           Unused, NS_FONT_VARIANT_LIGATURES_NONE, /* normal */ 0,
           systemFont.variantLigatures);

  // font-variant-numeric: normal, enum (bit field), inherit, initial,
  //                       -moz-system-font
  SetValue(*aRuleData->ValueForFontVariantNumeric(),
           aFont->mFont.variantNumeric, aConditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           aParentFont->mFont.variantNumeric,
           defaultVariableFont->variantNumeric,
           Unused, Unused, /* normal */ 0, systemFont.variantNumeric);

  // font-variant-position: normal, enum, inherit, initial,
  //                        -moz-system-font
  SetValue(*aRuleData->ValueForFontVariantPosition(),
           aFont->mFont.variantPosition, aConditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           aParentFont->mFont.variantPosition,
           defaultVariableFont->variantPosition,
           Unused, Unused, /* normal */ 0, systemFont.variantPosition);

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
      nsLayoutUtils::ComputeFontFeatures(
        featureSettingsValue->GetPairListValue(),
        aFont->mFont.fontFeatureSettings);
      break;

    default:
      MOZ_ASSERT(false, "unexpected value unit");
      break;
  }

  // font-variation-settings
  const nsCSSValue* variationSettingsValue =
    aRuleData->ValueForFontVariationSettings();

  switch (variationSettingsValue->GetUnit()) {
    case eCSSUnit_Null:
      break;

    case eCSSUnit_Normal:
    case eCSSUnit_Initial:
      aFont->mFont.fontVariationSettings.Clear();
      break;

    case eCSSUnit_Inherit:
    case eCSSUnit_Unset:
      aConditions.SetUncacheable();
      aFont->mFont.fontVariationSettings =
        aParentFont->mFont.fontVariationSettings;
      break;

    case eCSSUnit_System_Font:
      aFont->mFont.fontVariationSettings = systemFont.fontVariationSettings;
      break;

    case eCSSUnit_PairList:
    case eCSSUnit_PairListDep:
      nsLayoutUtils::ComputeFontVariations(
        variationSettingsValue->GetPairListValue(),
        aFont->mFont.fontVariationSettings);
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
    aFont->mFont.languageOverride = NO_FONT_LANGUAGE_OVERRIDE;
  } else if (eCSSUnit_System_Font == languageOverrideValue->GetUnit()) {
    aFont->mFont.languageOverride = systemFont.languageOverride;
  } else if (eCSSUnit_String == languageOverrideValue->GetUnit()) {
    nsAutoString lang;
    languageOverrideValue->GetStringValue(lang);
    aFont->mFont.languageOverride =
      nsLayoutUtils::ParseFontLanguageOverride(lang);
  }

  // -moz-min-font-size-ratio: percent, inherit
  const nsCSSValue* minFontSizeRatio = aRuleData->ValueForMinFontSizeRatio();
  switch (minFontSizeRatio->GetUnit()) {
    case eCSSUnit_Null:
      break;
    case eCSSUnit_Unset:
    case eCSSUnit_Inherit:
      aFont->mMinFontSizeRatio = aParentFont->mMinFontSizeRatio;
      aConditions.SetUncacheable();
      break;
    case eCSSUnit_Initial:
      aFont->mMinFontSizeRatio = 100; // 100%
      break;
    case eCSSUnit_Percent: {
      // While percentages are parsed as floating point numbers, we
      // only store an integer in the range [0, 255] since that's all
      // we need for now.
      float percent = minFontSizeRatio->GetPercentValue() * 100;
      if (percent < 0) {
        percent = 0;
      } else if (percent > 255) {
        percent = 255;
      }
      aFont->mMinFontSizeRatio = uint8_t(percent);
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown unit for -moz-min-font-size-ratio");
  }

  nscoord scriptLevelAdjustedUnconstrainedParentSize;

  // font-size: enum, length, percent, inherit
  nscoord scriptLevelAdjustedParentSize =
    ComputeScriptLevelSize(aFont, aParentFont, aPresContext,
                           &scriptLevelAdjustedUnconstrainedParentSize);
  NS_ASSERTION(!aUsedStartStruct || aFont->mScriptUnconstrainedSize == aFont->mSize,
               "If we have a start struct, we should have reset everything coming in here");

  // Compute whether we're affected by scriptMinSize *before* calling
  // SetFontSize, since aParentFont might be the same as aFont.  If it
  // is, calling SetFontSize might throw off our calculation.
  bool affectedByScriptMinSize =
    aParentFont->mSize != aParentFont->mScriptUnconstrainedSize ||
    scriptLevelAdjustedParentSize !=
      scriptLevelAdjustedUnconstrainedParentSize;

  SetFontSize(aPresContext, aContext,
              aRuleData, aFont, aParentFont,
              &aFont->mSize,
              systemFont, aParentFont->mSize, scriptLevelAdjustedParentSize,
              aUsedStartStruct, atRoot, aConditions);
  if (!aPresContext->Document()->GetMathMLEnabled()) {
    MOZ_ASSERT(!affectedByScriptMinSize);
    // If MathML is not enabled, we don't need to mark this node as
    // uncacheable.  If it becomes enabled, code in
    // nsMathMLElementFactory will rebuild the rule tree and style data
    // when MathML is first enabled (see nsMathMLElement::BindToTree).
    aFont->mScriptUnconstrainedSize = aFont->mSize;
  } else if (!affectedByScriptMinSize) {
    // Fast path: we have not been affected by scriptminsize so we don't
    // need to call SetFontSize again to compute the
    // scriptminsize-unconstrained size. This is OK even if we have a
    // start struct, because if we have a start struct then 'font-size'
    // was specified and so scriptminsize has no effect.
    aFont->mScriptUnconstrainedSize = aFont->mSize;
    // It's possible we could, in the future, have a different parent,
    // which would lead to a different affectedByScriptMinSize.
    aConditions.SetUncacheable();
  } else {
    // see previous else-if
    aConditions.SetUncacheable();

    // Use a separate conditions object because it might get a
    // *different* font-size dependency.  We can ignore it because we've
    // already called SetUncacheable.
    RuleNodeCacheConditions unconstrainedConditions;

    SetFontSize(aPresContext, aContext,
                aRuleData, aFont, aParentFont,
                &aFont->mScriptUnconstrainedSize,
                systemFont, aParentFont->mScriptUnconstrainedSize,
                scriptLevelAdjustedUnconstrainedParentSize,
                aUsedStartStruct, atRoot, unconstrainedConditions);
  }
  NS_ASSERTION(aFont->mScriptUnconstrainedSize <= aFont->mSize,
               "scriptminsize should never be making things bigger");

  nsLayoutUtils::ApplyMinFontSize(aFont, aPresContext,
                                  aPresContext->MinFontSize(aFont->mLanguage));

  // font-size-adjust: number, none, inherit, initial, -moz-system-font
  const nsCSSValue* sizeAdjustValue = aRuleData->ValueForFontSizeAdjust();
  if (eCSSUnit_System_Font == sizeAdjustValue->GetUnit()) {
    aFont->mFont.sizeAdjust = systemFont.sizeAdjust;
  } else
    SetFactor(*sizeAdjustValue, aFont->mFont.sizeAdjust,
              aConditions, aParentFont->mFont.sizeAdjust, -1.0f,
              SETFCT_NONE | SETFCT_UNSET_INHERIT);
}

// This should die (bug 380915).
//
// SetGenericFont:
//  - backtrack to an ancestor with the same generic font name (possibly
//    up to the root where default values come from the presentation context)
//  - re-apply cascading rules from there without caching intermediate values
/* static */ void
nsRuleNode::SetGenericFont(nsPresContext* aPresContext,
                           GeckoStyleContext* aContext,
                           uint8_t aGenericFontID,
                           nsStyleFont* aFont)
{
  // walk up the contexts until a context with the desired generic font
  AutoTArray<GeckoStyleContext*, 8> contextPath;
  contextPath.AppendElement(aContext);
  GeckoStyleContext* higherContext = aContext->GetParent();
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
    GeckoStyleContext* context = contextPath[i];
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
                            GeckoStyleContext* aContext,
                            nsRuleNode* aHighestNode,
                            const RuleDetail aRuleDetail,
                            const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_INHERITED(Font, font, parentFont)

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
    NotNull<SharedFontList*> fontlist = familyValue->GetFontFamilyListValue();
    font->mFont.fontlist = FontFamilyList(fontlist);

    // if only a single generic, set the generic type
    if (fontlist->mNames.Length() == 1) {
      // extract the first generic in the fontlist, if exists
      switch (font->mFont.fontlist.FirstGeneric()) {
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

static already_AddRefed<nsCSSShadowArray>
GetShadowData(const nsCSSValueList* aList,
              GeckoStyleContext* aContext,
              bool aIsBoxShadow,
              nsPresContext* aPresContext,
              RuleNodeCacheConditions& aConditions)
{
  uint32_t arrayLength = ListLength(aList);

  MOZ_ASSERT(arrayLength > 0,
             "Non-null text-shadow list, yet we counted 0 items.");
  RefPtr<nsCSSShadowArray> shadowList =
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
                      aContext, aPresContext, aConditions);
    NS_ASSERTION(unitOK, "unexpected unit");
    item->mXOffset = tempCoord.GetCoordValue();

    unitOK = SetCoord(arr->Item(1), tempCoord, nsStyleCoord(),
                      SETCOORD_LENGTH | SETCOORD_CALC_LENGTH_ONLY,
                      aContext, aPresContext, aConditions);
    NS_ASSERTION(unitOK, "unexpected unit");
    item->mYOffset = tempCoord.GetCoordValue();

    // Blur radius is optional in the current box-shadow spec
    if (arr->Item(2).GetUnit() != eCSSUnit_Null) {
      unitOK = SetCoord(arr->Item(2), tempCoord, nsStyleCoord(),
                        SETCOORD_LENGTH | SETCOORD_CALC_LENGTH_ONLY |
                          SETCOORD_CALC_CLAMP_NONNEGATIVE,
                        aContext, aPresContext, aConditions);
      NS_ASSERTION(unitOK, "unexpected unit");
      item->mRadius = tempCoord.GetCoordValue();
    } else {
      item->mRadius = 0;
    }

    // Find the spread radius
    if (aIsBoxShadow && arr->Item(3).GetUnit() != eCSSUnit_Null) {
      unitOK = SetCoord(arr->Item(3), tempCoord, nsStyleCoord(),
                        SETCOORD_LENGTH | SETCOORD_CALC_LENGTH_ONLY,
                        aContext, aPresContext, aConditions);
      NS_ASSERTION(unitOK, "unexpected unit");
      item->mSpread = tempCoord.GetCoordValue();
    } else {
      item->mSpread = 0;
    }

    if (arr->Item(4).GetUnit() != eCSSUnit_Null) {
      item->mHasColor = true;
      // 2nd argument can be bogus since inherit is not a valid color
      unitOK = SetColor(arr->Item(4), 0, aPresContext, aContext, item->mColor,
                        aConditions);
      NS_ASSERTION(unitOK, "unexpected unit");
    }

    if (aIsBoxShadow && arr->Item(5).GetUnit() == eCSSUnit_Enumerated) {
      NS_ASSERTION(arr->Item(5).GetIntValue()
                   == uint8_t(StyleBoxShadowType::Inset),
                   "invalid keyword type for box shadow");
      item->mInset = true;
    } else {
      item->mInset = false;
    }
  }

  return shadowList.forget();
}

struct TextEmphasisChars
{
  const char16_t* mFilled;
  const char16_t* mOpen;
};

#define TEXT_EMPHASIS_CHARS_LIST() \
  TEXT_EMPHASIS_CHARS_ITEM(u"", u"", NONE) \
  TEXT_EMPHASIS_CHARS_ITEM(u"\u2022", u"\u25e6", DOT) \
  TEXT_EMPHASIS_CHARS_ITEM(u"\u25cf", u"\u25cb", CIRCLE) \
  TEXT_EMPHASIS_CHARS_ITEM(u"\u25c9", u"\u25ce", DOUBLE_CIRCLE) \
  TEXT_EMPHASIS_CHARS_ITEM(u"\u25b2", u"\u25b3", TRIANGLE) \
  TEXT_EMPHASIS_CHARS_ITEM(u"\ufe45", u"\ufe46", SESAME)

static constexpr TextEmphasisChars kTextEmphasisChars[] =
{
#define TEXT_EMPHASIS_CHARS_ITEM(filled_, open_, type_) \
  { filled_, open_ }, // type_
  TEXT_EMPHASIS_CHARS_LIST()
#undef TEXT_EMPHASIS_CHARS_ITEM
};

#define TEXT_EMPHASIS_CHARS_ITEM(filled_, open_, type_) \
  static_assert(ArrayLength(filled_) <= 2 && \
                ArrayLength(open_) <= 2, \
                "emphasis marks should have no more than one char"); \
  static_assert( \
    *kTextEmphasisChars[NS_STYLE_TEXT_EMPHASIS_STYLE_##type_].mFilled == \
    *filled_, "filled " #type_ " should be " #filled_); \
  static_assert( \
    *kTextEmphasisChars[NS_STYLE_TEXT_EMPHASIS_STYLE_##type_].mOpen == \
    *open_, "open " #type_ " should be " #open_);
TEXT_EMPHASIS_CHARS_LIST()
#undef TEXT_EMPHASIS_CHARS_ITEM

#undef TEXT_EMPHASIS_CHARS_LIST

static void
TruncateStringToSingleGrapheme(nsAString& aStr)
{
  unicode::ClusterIterator iter(aStr.Data(), aStr.Length());
  if (!iter.AtEnd()) {
    iter.Next();
    if (!iter.AtEnd()) {
      // Not mutating the string for common cases helps memory use
      // since we share the buffer from the specified style into the
      // computed style.
      aStr.Truncate(iter - aStr.Data());
    }
  }
}

struct LengthNumberCalcObj
{
  float mValue;
  bool mIsNumber;
};

struct LengthNumberCalcOps : public css::FloatCoeffsAlreadyNormalizedOps
{
  typedef LengthNumberCalcObj result_type;
  GeckoStyleContext* const mStyleContext;
  nsPresContext* const mPresContext;
  RuleNodeCacheConditions& mConditions;

  LengthNumberCalcOps(GeckoStyleContext* aStyleContext,
                      nsPresContext* aPresContext,
                      RuleNodeCacheConditions& aConditions)
    : mStyleContext(aStyleContext),
      mPresContext(aPresContext),
      mConditions(aConditions)
  {
  }

  result_type
  MergeAdditive(nsCSSUnit aCalcFunction,
                result_type aValue1, result_type aValue2)
  {
    MOZ_ASSERT(aValue1.mIsNumber == aValue2.mIsNumber);

    LengthNumberCalcObj result;
    result.mIsNumber = aValue1.mIsNumber;
    if (aCalcFunction == eCSSUnit_Calc_Plus) {
      result.mValue = aValue1.mValue + aValue2.mValue;
      return result;
    }
    MOZ_ASSERT(aCalcFunction == eCSSUnit_Calc_Minus,
               "unexpected unit");
    result.mValue = aValue1.mValue - aValue2.mValue;
    return result;
  }

  result_type
  MergeMultiplicativeL(nsCSSUnit aCalcFunction,
                       float aValue1, result_type aValue2)
  {
    MOZ_ASSERT(aCalcFunction == eCSSUnit_Calc_Times_L,
               "unexpected unit");
    LengthNumberCalcObj result;
    result.mIsNumber = aValue2.mIsNumber;
    result.mValue = aValue1 * aValue2.mValue;
    return result;
  }

  result_type
  MergeMultiplicativeR(nsCSSUnit aCalcFunction,
                       result_type aValue1, float aValue2)
  {
    LengthNumberCalcObj result;
    result.mIsNumber = aValue1.mIsNumber;
    if (aCalcFunction == eCSSUnit_Calc_Times_R) {
      result.mValue = aValue1.mValue * aValue2;
      return result;
    }
    MOZ_ASSERT(aCalcFunction == eCSSUnit_Calc_Divided,
               "unexpected unit");
    result.mValue = aValue1.mValue / aValue2;
    return result;
  }

  bool ComputeLeafValue(result_type& aResult, const nsCSSValue& aValue)
  {
    if (aValue.IsLengthUnit()) {
      aResult.mIsNumber = false;
      aResult.mValue = CalcLength(aValue, mStyleContext,
                                      mPresContext, mConditions);
    }
    else if (eCSSUnit_Number == aValue.GetUnit()) {
      aResult.mIsNumber = true;
      aResult.mValue = aValue.GetFloatValue();
    } else {
      MOZ_ASSERT(false, "unexpected value");
      aResult.mIsNumber = true;
      aResult.mValue = 1.0f;
    }

    return true;
  }
};

struct SetLineHeightCalcOps : public LengthNumberCalcOps
{
  SetLineHeightCalcOps(GeckoStyleContext* aStyleContext,
                       nsPresContext* aPresContext,
                       RuleNodeCacheConditions& aConditions)
    : LengthNumberCalcOps(aStyleContext, aPresContext, aConditions)
  {
  }

  bool ComputeLeafValue(result_type& aResult, const nsCSSValue& aValue)
  {
    if (aValue.IsLengthUnit()) {
      aResult.mIsNumber = false;
      aResult.mValue = CalcLength(aValue, mStyleContext,
                                      mPresContext, mConditions);
    }
    else if (eCSSUnit_Percent == aValue.GetUnit()) {
      mConditions.SetUncacheable();
      aResult.mIsNumber = false;
      nscoord fontSize = mStyleContext->StyleFont()->mFont.size;
      aResult.mValue = fontSize * aValue.GetPercentValue();
    }
    else if (eCSSUnit_Number == aValue.GetUnit()) {
      aResult.mIsNumber = true;
      aResult.mValue = aValue.GetFloatValue();
    } else {
      MOZ_ASSERT(false, "unexpected value");
      aResult.mIsNumber = true;
      aResult.mValue = 1.0f;
    }

    return true;
  }
};

const void*
nsRuleNode::ComputeTextData(void* aStartStruct,
                            const nsRuleData* aRuleData,
                            GeckoStyleContext* aContext,
                            nsRuleNode* aHighestNode,
                            const RuleDetail aRuleDetail,
                            const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_INHERITED(Text, text, parentText)

  auto setComplexColor = [&](const nsCSSValue* aValue,
                             StyleComplexColor nsStyleText::* aField) {
    SetComplexColor<eUnsetInherit>(*aValue, parentText->*aField,
                                   StyleComplexColor::CurrentColor(),
                                   mPresContext, text->*aField, conditions);
  };

  // tab-size: number, length, calc, inherit
  const nsCSSValue* tabSizeValue = aRuleData->ValueForTabSize();
  if (tabSizeValue->GetUnit() == eCSSUnit_Initial) {
    text->mTabSize = nsStyleCoord(float(NS_STYLE_TABSIZE_INITIAL), eStyleUnit_Factor);
  } else if (eCSSUnit_Calc == tabSizeValue->GetUnit()) {
    LengthNumberCalcOps ops(aContext, mPresContext, conditions);
    LengthNumberCalcObj obj;
    if (!css::ComputeCalc(obj, *tabSizeValue, ops)) {
      MOZ_ASSERT_UNREACHABLE("unexpected ComputeCalc failure");
    }
    float value = obj.mValue < 0 ? 0 : obj.mValue;
    if (obj.mIsNumber) {
      text->mTabSize.SetFactorValue(value);
    } else {
      text->mTabSize.SetCoordValue(
        NSToCoordRoundWithClamp(value));
    }
  } else {
    SetCoord(*tabSizeValue, text->mTabSize, parentText->mTabSize,
             SETCOORD_LH | SETCOORD_FACTOR | SETCOORD_UNSET_INHERIT,
             aContext, mPresContext, conditions);
  }

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
                                        aContext, false, mPresContext, conditions);
    }
  }

  // line-height: normal, number, length, percent, calc, inherit
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
  else if (eCSSUnit_Calc == lineHeightValue->GetUnit()) {
    SetLineHeightCalcOps ops(aContext, mPresContext, conditions);
    LengthNumberCalcObj obj;
    if (!css::ComputeCalc(obj, *lineHeightValue, ops)) {
      MOZ_ASSERT_UNREACHABLE("unexpected ComputeCalc failure");
    }
    if (obj.mIsNumber) {
      text->mLineHeight.SetFactorValue(obj.mValue);
    } else {
      text->mLineHeight.SetCoordValue(
        NSToCoordRoundWithClamp(obj.mValue));
    }
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
    MOZ_ASSERT_UNREACHABLE("align string");
  } else if (eCSSUnit_Enumerated == textAlignValue->GetUnit() &&
             NS_STYLE_TEXT_ALIGN_MOZ_CENTER_OR_INHERIT ==
               textAlignValue->GetIntValue()) {
    conditions.SetUncacheable();
    uint8_t parentAlign = parentText->mTextAlign;
    text->mTextAlign = (NS_STYLE_TEXT_ALIGN_START == parentAlign) ?
      NS_STYLE_TEXT_ALIGN_CENTER : parentAlign;
  } else if (eCSSUnit_Enumerated == textAlignValue->GetUnit() &&
             NS_STYLE_TEXT_ALIGN_MATCH_PARENT ==
               textAlignValue->GetIntValue()) {
    conditions.SetUncacheable();
    GeckoStyleContext* parent = aContext->GetParent();
    if (parent) {
      uint8_t parentAlign = parentText->mTextAlign;
      uint8_t parentDirection = parent->StyleVisibility()->mDirection;
      switch (parentAlign) {
        case NS_STYLE_TEXT_ALIGN_START:
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
        if (textAlignValue->GetIntValue() == NS_STYLE_TEXT_ALIGN_UNSAFE) {
          textAlignValue = &textAlignValuePair.mYValue;
        }
      } else if (eCSSUnit_String == textAlignValue->GetUnit()) {
        MOZ_ASSERT_UNREACHABLE("align string");
      }
    } else if (eCSSUnit_Inherit == textAlignValue->GetUnit() ||
               eCSSUnit_Unset == textAlignValue->GetUnit()) {
      text->mTextAlignTrue = parentText->mTextAlignTrue;
    }
    SetValue(*textAlignValue, text->mTextAlign, conditions,
             SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
             parentText->mTextAlign,
             NS_STYLE_TEXT_ALIGN_START);
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
      if (textAlignLastValue->GetIntValue() == NS_STYLE_TEXT_ALIGN_UNSAFE) {
        textAlignLastValue = &textAlignLastValuePair.mYValue;
      }
    }
  } else if (eCSSUnit_Inherit == textAlignLastValue->GetUnit() ||
             eCSSUnit_Unset == textAlignLastValue->GetUnit()) {
    text->mTextAlignLastTrue = parentText->mTextAlignLastTrue;
  }
  SetValue(*textAlignLastValue, text->mTextAlignLast,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentText->mTextAlignLast,
           NS_STYLE_TEXT_ALIGN_AUTO);

  // text-indent: length, percent, calc, inherit, initial
  SetCoord(*aRuleData->ValueForTextIndent(), text->mTextIndent, parentText->mTextIndent,
           SETCOORD_LPH | SETCOORD_INITIAL_ZERO | SETCOORD_STORE_CALC |
             SETCOORD_UNSET_INHERIT,
           aContext, mPresContext, conditions);

  // text-justify: enum, inherit, initial
  SetValue(*aRuleData->ValueForTextJustify(), text->mTextJustify, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentText->mTextJustify,
           StyleTextJustify::Auto);

  // text-transform: enum, inherit, initial
  SetValue(*aRuleData->ValueForTextTransform(), text->mTextTransform, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentText->mTextTransform,
           NS_STYLE_TEXT_TRANSFORM_NONE);

  // white-space: enum, inherit, initial
  SetValue(*aRuleData->ValueForWhiteSpace(), text->mWhiteSpace, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentText->mWhiteSpace,
           StyleWhiteSpace::Normal);

  // word-break: enum, inherit, initial
  SetValue(*aRuleData->ValueForWordBreak(), text->mWordBreak, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentText->mWordBreak,
           NS_STYLE_WORDBREAK_NORMAL);

  // word-spacing: normal, length, percent, inherit
  const nsCSSValue* wordSpacingValue = aRuleData->ValueForWordSpacing();
  if (wordSpacingValue->GetUnit() == eCSSUnit_Normal) {
    // Do this so that "normal" computes to 0px, as the CSS 2.1 spec requires.
    text->mWordSpacing.SetCoordValue(0);
  } else {
    SetCoord(*aRuleData->ValueForWordSpacing(),
             text->mWordSpacing, parentText->mWordSpacing,
             SETCOORD_LPH | SETCOORD_INITIAL_ZERO |
               SETCOORD_STORE_CALC | SETCOORD_UNSET_INHERIT,
             aContext, mPresContext, conditions);
  }

  // overflow-wrap: enum, inherit, initial
  SetValue(*aRuleData->ValueForOverflowWrap(), text->mOverflowWrap, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentText->mOverflowWrap,
           NS_STYLE_OVERFLOWWRAP_NORMAL);

  // hyphens: enum, inherit, initial
  SetValue(*aRuleData->ValueForHyphens(), text->mHyphens, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentText->mHyphens,
           StyleHyphens::Manual);

  // ruby-align: enum, inherit, initial
  SetValue(*aRuleData->ValueForRubyAlign(),
           text->mRubyAlign, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentText->mRubyAlign,
           NS_STYLE_RUBY_ALIGN_SPACE_AROUND);

  // ruby-position: enum, inherit, initial
  SetValue(*aRuleData->ValueForRubyPosition(),
           text->mRubyPosition, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentText->mRubyPosition,
           NS_STYLE_RUBY_POSITION_OVER);

  // text-size-adjust: enum, inherit, initial
  SetValue(*aRuleData->ValueForTextSizeAdjust(),
           text->mTextSizeAdjust, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentText->mTextSizeAdjust,
           NS_STYLE_TEXT_SIZE_ADJUST_AUTO);

  // text-combine-upright: enum, inherit, initial
  SetValue(*aRuleData->ValueForTextCombineUpright(),
           text->mTextCombineUpright,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentText->mTextCombineUpright,
           NS_STYLE_TEXT_COMBINE_UPRIGHT_NONE);

  // text-emphasis-color: color, string, inherit, initial
  setComplexColor(aRuleData->ValueForTextEmphasisColor(),
                  &nsStyleText::mTextEmphasisColor);

  // text-emphasis-position: enum, inherit, initial
  SetValue(*aRuleData->ValueForTextEmphasisPosition(),
           text->mTextEmphasisPosition,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentText->mTextEmphasisPosition,
           NS_STYLE_TEXT_EMPHASIS_POSITION_OVER |
           NS_STYLE_TEXT_EMPHASIS_POSITION_RIGHT);

  // text-emphasis-style: string, enum, inherit, initial
  const nsCSSValue* textEmphasisStyleValue =
    aRuleData->ValueForTextEmphasisStyle();
  switch (textEmphasisStyleValue->GetUnit()) {
    case eCSSUnit_Null:
      break;
    case eCSSUnit_Initial:
    case eCSSUnit_None: {
      text->mTextEmphasisStyle = NS_STYLE_TEXT_EMPHASIS_STYLE_NONE;
      text->mTextEmphasisStyleString = u"";
      break;
    }
    case eCSSUnit_Inherit:
    case eCSSUnit_Unset: {
      conditions.SetUncacheable();
      text->mTextEmphasisStyle = parentText->mTextEmphasisStyle;
      text->mTextEmphasisStyleString = parentText->mTextEmphasisStyleString;
      break;
    }
    case eCSSUnit_Enumerated: {
      auto style = textEmphasisStyleValue->GetIntValue();
      // If shape part is not specified, compute it according to the
      // writing-mode. Note that, if the fill part (filled/open) is not
      // specified, we compute it to filled per spec. Since that value
      // is zero, no additional computation is needed. See the assertion
      // in CSSParserImpl::ParseTextEmphasisStyle().
      if (!(style & NS_STYLE_TEXT_EMPHASIS_STYLE_SHAPE_MASK)) {
        conditions.SetUncacheable();
        if (WritingMode(aContext).IsVertical()) {
          style |= NS_STYLE_TEXT_EMPHASIS_STYLE_SESAME;
        } else {
          style |= NS_STYLE_TEXT_EMPHASIS_STYLE_CIRCLE;
        }
      }
      text->mTextEmphasisStyle = style;
      size_t shape = style & NS_STYLE_TEXT_EMPHASIS_STYLE_SHAPE_MASK;
      MOZ_ASSERT(shape > 0 && shape < ArrayLength(kTextEmphasisChars));
      const TextEmphasisChars& chars = kTextEmphasisChars[shape];
      text->mTextEmphasisStyleString =
        (style & NS_STYLE_TEXT_EMPHASIS_STYLE_FILL_MASK) ==
        NS_STYLE_TEXT_EMPHASIS_STYLE_FILLED ? chars.mFilled : chars.mOpen;
      break;
    }
    case eCSSUnit_String: {
      text->mTextEmphasisStyle = NS_STYLE_TEXT_EMPHASIS_STYLE_STRING;
      nsString strValue;
      textEmphasisStyleValue->GetStringValue(strValue);
      TruncateStringToSingleGrapheme(strValue);
      text->mTextEmphasisStyleString = strValue;
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown value unit type");
  }

  // text-rendering: enum, inherit, initial
  SetValue(*aRuleData->ValueForTextRendering(),
           text->mTextRendering, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentText->mTextRendering,
           NS_STYLE_TEXT_RENDERING_AUTO);

  // -webkit-text-fill-color: color, string, inherit, initial
  setComplexColor(aRuleData->ValueForWebkitTextFillColor(),
                  &nsStyleText::mWebkitTextFillColor);

  // -webkit-text-stroke-color: color, string, inherit, initial
  setComplexColor(aRuleData->ValueForWebkitTextStrokeColor(),
                  &nsStyleText::mWebkitTextStrokeColor);

  // -webkit-text-stroke-width: length, inherit, initial, enum
  Maybe<nscoord> coord =
    ComputeLineWidthValue<eUnsetInherit>(
      *aRuleData->ValueForWebkitTextStrokeWidth(),
      parentText->mWebkitTextStrokeWidth, 0,
      aContext, mPresContext, conditions);
  if (coord.isSome()) {
    text->mWebkitTextStrokeWidth = *coord;
  }

  // -moz-control-character-visibility: enum, inherit, initial
  SetValue(*aRuleData->ValueForControlCharacterVisibility(),
           text->mControlCharacterVisibility,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentText->mControlCharacterVisibility,
           nsLayoutUtils::ControlCharVisibilityDefault());

  COMPUTE_END_INHERITED(Text, text)
}

const void*
nsRuleNode::ComputeTextResetData(void* aStartStruct,
                                 const nsRuleData* aRuleData,
                                 GeckoStyleContext* aContext,
                                 nsRuleNode* aHighestNode,
                                 const RuleDetail aRuleDetail,
                                 const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(TextReset, text, parentText)

  // text-decoration-line: enum (bit field), inherit, initial
  const nsCSSValue* decorationLineValue =
    aRuleData->ValueForTextDecorationLine();
  if (eCSSUnit_Enumerated == decorationLineValue->GetUnit()) {
    text->mTextDecorationLine = decorationLineValue->GetIntValue();
  } else if (eCSSUnit_Inherit == decorationLineValue->GetUnit()) {
    conditions.SetUncacheable();
    text->mTextDecorationLine = parentText->mTextDecorationLine;
  } else if (eCSSUnit_Initial == decorationLineValue->GetUnit() ||
             eCSSUnit_Unset == decorationLineValue->GetUnit()) {
    text->mTextDecorationLine = NS_STYLE_TEXT_DECORATION_LINE_NONE;
  }

  // text-decoration-color: color, string, enum, inherit, initial
  SetComplexColor<eUnsetInitial>(*aRuleData->ValueForTextDecorationColor(),
                                 parentText->mTextDecorationColor,
                                 StyleComplexColor::CurrentColor(),
                                 mPresContext,
                                 text->mTextDecorationColor, conditions);

  // text-decoration-style: enum, inherit, initial
  const nsCSSValue* decorationStyleValue =
    aRuleData->ValueForTextDecorationStyle();
  if (eCSSUnit_Enumerated == decorationStyleValue->GetUnit()) {
    text->mTextDecorationStyle = decorationStyleValue->GetIntValue();
  } else if (eCSSUnit_Inherit == decorationStyleValue->GetUnit()) {
    text->mTextDecorationStyle = parentText->mTextDecorationStyle;
    conditions.SetUncacheable();
  } else if (eCSSUnit_Initial == decorationStyleValue->GetUnit() ||
             eCSSUnit_Unset == decorationStyleValue->GetUnit()) {
    text->mTextDecorationStyle = NS_STYLE_TEXT_DECORATION_STYLE_SOLID;
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
    SetValue(*textOverflowValue, text->mTextOverflow.mRight.mType,
             conditions,
             SETVAL_ENUMERATED, parentText->mTextOverflow.mRight.mType,
             NS_STYLE_TEXT_OVERFLOW_CLIP);
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
      SetValue(*textOverflowLeftValue, text->mTextOverflow.mLeft.mType,
               conditions,
               SETVAL_ENUMERATED, parentText->mTextOverflow.mLeft.mType,
               NS_STYLE_TEXT_OVERFLOW_CLIP);
      text->mTextOverflow.mLeft.mString.Truncate();
    } else if (eCSSUnit_String == textOverflowLeftValue->GetUnit()) {
      textOverflowLeftValue->GetStringValue(text->mTextOverflow.mLeft.mString);
      text->mTextOverflow.mLeft.mType = NS_STYLE_TEXT_OVERFLOW_STRING;
    }

    const nsCSSValue *textOverflowRightValue = &textOverflowValuePair.mYValue;
    if (eCSSUnit_Enumerated == textOverflowRightValue->GetUnit()) {
      SetValue(*textOverflowRightValue, text->mTextOverflow.mRight.mType,
               conditions,
               SETVAL_ENUMERATED, parentText->mTextOverflow.mRight.mType,
               NS_STYLE_TEXT_OVERFLOW_CLIP);
      text->mTextOverflow.mRight.mString.Truncate();
    } else if (eCSSUnit_String == textOverflowRightValue->GetUnit()) {
      textOverflowRightValue->GetStringValue(text->mTextOverflow.mRight.mString);
      text->mTextOverflow.mRight.mType = NS_STYLE_TEXT_OVERFLOW_STRING;
    }
  }

  // unicode-bidi: enum, inherit, initial
  SetValue(*aRuleData->ValueForUnicodeBidi(), text->mUnicodeBidi, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentText->mUnicodeBidi,
           NS_STYLE_UNICODE_BIDI_NORMAL);

  // initial-letter: normal, number, array(number, integer?), initial
  const nsCSSValue* initialLetterValue = aRuleData->ValueForInitialLetter();
  if (initialLetterValue->GetUnit() == eCSSUnit_Null) {
    // We don't want to change anything in this case.
  } else if (initialLetterValue->GetUnit() == eCSSUnit_Inherit) {
    conditions.SetUncacheable();
    text->mInitialLetterSink = parentText->mInitialLetterSink;
    text->mInitialLetterSize = parentText->mInitialLetterSize;
  } else if (initialLetterValue->GetUnit() == eCSSUnit_Initial ||
             initialLetterValue->GetUnit() == eCSSUnit_Unset ||
             initialLetterValue->GetUnit() == eCSSUnit_Normal) {
    // Use invalid values in initial-letter property to mean normal. So we can
    // determine whether it is normal by checking mInitialLetterSink == 0.
    text->mInitialLetterSink = 0;
    text->mInitialLetterSize = 0.0f;
  } else if (initialLetterValue->GetUnit() == eCSSUnit_Array) {
    const nsCSSValue& firstValue = initialLetterValue->GetArrayValue()->Item(0);
    const nsCSSValue& secondValue = initialLetterValue->GetArrayValue()->Item(1);
    MOZ_ASSERT(firstValue.GetUnit() == eCSSUnit_Number &&
               secondValue.GetUnit() == eCSSUnit_Integer,
               "unexpected value unit");
    text->mInitialLetterSize = firstValue.GetFloatValue();
    text->mInitialLetterSink = secondValue.GetIntValue();
  } else if (initialLetterValue->GetUnit() == eCSSUnit_Number) {
    text->mInitialLetterSize = initialLetterValue->GetFloatValue();
    text->mInitialLetterSink = NSToCoordFloorClamped(text->mInitialLetterSize);
  } else {
    MOZ_ASSERT_UNREACHABLE("unknown unit for initial-letter");
  }

  COMPUTE_END_RESET(TextReset, text)
}

const void*
nsRuleNode::ComputeUserInterfaceData(void* aStartStruct,
                                     const nsRuleData* aRuleData,
                                     GeckoStyleContext* aContext,
                                     nsRuleNode* aHighestNode,
                                     const RuleDetail aRuleDetail,
                                     const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_INHERITED(UserInterface, ui, parentUI)

  // cursor: enum, url, inherit
  const nsCSSValue* cursorValue = aRuleData->ValueForCursor();
  nsCSSUnit cursorUnit = cursorValue->GetUnit();
  if (cursorUnit != eCSSUnit_Null) {
    ui->mCursorImages.Clear();

    if (cursorUnit == eCSSUnit_Inherit ||
        cursorUnit == eCSSUnit_Unset) {
      conditions.SetUncacheable();
      ui->mCursor = parentUI->mCursor;
      ui->mCursorImages = parentUI->mCursorImages;
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
      for ( ; list->mValue.GetUnit() == eCSSUnit_Array; list = list->mNext) {
        nsCSSValue::Array* arr = list->mValue.GetArrayValue();
        nsCursorImage* item = ui->mCursorImages.AppendElement();
        item->mImage =
          CreateStyleImageRequest(aContext->PresContext(), arr->Item(0),
                                  nsStyleImageRequest::Mode::Discard);
        if (arr->Item(1).GetUnit() != eCSSUnit_Null) {
          item->mHaveHotspot = true;
          item->mHotspotX = arr->Item(1).GetFloatValue();
          item->mHotspotY = arr->Item(2).GetFloatValue();
        }
      }

      NS_ASSERTION(list, "Must have non-array value at the end");
      NS_ASSERTION(list->mValue.GetUnit() == eCSSUnit_Enumerated,
                   "Unexpected fallback value at end of cursor list");
      ui->mCursor = list->mValue.GetIntValue();
    }
  }

  // user-input: enum, inherit, initial
  SetValue(*aRuleData->ValueForUserInput(),
           ui->mUserInput, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentUI->mUserInput,
           StyleUserInput::Auto);

  // user-modify: enum, inherit, initial
  SetValue(*aRuleData->ValueForUserModify(),
           ui->mUserModify, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentUI->mUserModify,
           StyleUserModify::ReadOnly);

  // user-focus: enum, inherit, initial
  SetValue(*aRuleData->ValueForUserFocus(),
           ui->mUserFocus, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentUI->mUserFocus,
           StyleUserFocus::None);

  // pointer-events: enum, inherit, initial
  SetValue(*aRuleData->ValueForPointerEvents(), ui->mPointerEvents,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentUI->mPointerEvents,
           NS_STYLE_POINTER_EVENTS_AUTO);

  // caret-color: auto, color, inherit
  const nsCSSValue* caretColorValue = aRuleData->ValueForCaretColor();
  SetComplexColor<eUnsetInherit>(*caretColorValue,
                                 parentUI->mCaretColor,
                                 StyleComplexColor::Auto(),
                                 mPresContext,
                                 ui->mCaretColor, conditions);

  COMPUTE_END_INHERITED(UserInterface, ui)
}

const void*
nsRuleNode::ComputeUIResetData(void* aStartStruct,
                               const nsRuleData* aRuleData,
                               GeckoStyleContext* aContext,
                               nsRuleNode* aHighestNode,
                               const RuleDetail aRuleDetail,
                               const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(UIReset, ui, parentUI)

  // user-select: enum, inherit, initial
  SetValue(*aRuleData->ValueForUserSelect(),
           ui->mUserSelect, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentUI->mUserSelect,
           StyleUserSelect::Auto);

  // ime-mode: enum, inherit, initial
  SetValue(*aRuleData->ValueForImeMode(),
           ui->mIMEMode, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentUI->mIMEMode,
           NS_STYLE_IME_MODE_AUTO);

  // force-broken-image-icons: integer, inherit, initial
  SetValue(*aRuleData->ValueForForceBrokenImageIcon(),
           ui->mForceBrokenImageIcon,
           conditions,
           SETVAL_INTEGER | SETVAL_UNSET_INITIAL,
           parentUI->mForceBrokenImageIcon, 0);

  // -moz-window-dragging: enum, inherit, initial
  SetValue(*aRuleData->ValueForWindowDragging(),
           ui->mWindowDragging, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentUI->mWindowDragging,
           StyleWindowDragging::Default);

  // -moz-window-shadow: enum, inherit, initial
  SetValue(*aRuleData->ValueForWindowShadow(),
           ui->mWindowShadow, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentUI->mWindowShadow,
           NS_STYLE_WINDOW_SHADOW_DEFAULT);

  // -moz-window-opacity: factor, inherit, initial
  SetFactor(*aRuleData->ValueForWindowOpacity(),
            ui->mWindowOpacity, conditions,
            parentUI->mWindowOpacity, 1.0f,
            SETFCT_OPACITY | SETFCT_UNSET_INITIAL);

  // -moz-window-transform
  SetTransformValue(*aRuleData->ValueForWindowTransform(),
                    ui->mSpecifiedWindowTransform, conditions,
                    parentUI->mSpecifiedWindowTransform);

  // -moz-window-transform-origin
  const nsCSSValue* windowTransformOriginValue =
    aRuleData->ValueForWindowTransformOrigin();
  if (windowTransformOriginValue->GetUnit() != eCSSUnit_Null) {
    mozilla::DebugOnly<bool> result =
      SetPairCoords(*windowTransformOriginValue,
                    ui->mWindowTransformOrigin[0],
                    ui->mWindowTransformOrigin[1],
                    parentUI->mWindowTransformOrigin[0],
                    parentUI->mWindowTransformOrigin[1],
                    SETCOORD_LPH | SETCOORD_INITIAL_HALF |
                      SETCOORD_BOX_POSITION | SETCOORD_STORE_CALC |
                      SETCOORD_UNSET_INITIAL,
                    aContext, mPresContext, conditions);
    NS_ASSERTION(result, "Malformed -moz-window-transform-origin parse!");
  }

  COMPUTE_END_RESET(UIReset, ui)
}

// Information about each transition or animation property that is
// constant.
struct TransitionPropInfo {
  nsCSSPropertyID property;
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

/* static */ void
nsRuleNode::ComputeTimingFunction(const nsCSSValue& aValue,
                                  nsTimingFunction& aResult)
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
                       NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_END ||
                      array->Item(1).GetIntValue() == -1),
                     "unexpected second value");
        nsTimingFunction::Type type =
          (array->Item(1).GetIntValue() ==
            NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_START) ?
              nsTimingFunction::Type::StepStart :
              nsTimingFunction::Type::StepEnd;
        aResult = nsTimingFunction(type, array->Item(0).GetIntValue());
      }
      break;
    case eCSSUnit_Function:
      {
        nsCSSValue::Array* array = aValue.GetArrayValue();
        NS_ASSERTION(array && array->Count() == 2, "Need 2 items");
        NS_ASSERTION(array->Item(0).GetKeywordValue() == eCSSKeyword_frames,
                     "should be frames function");
        NS_ASSERTION(array->Item(1).GetUnit() == eCSSUnit_Integer,
                     "unexpected frames function value");
        aResult = nsTimingFunction(nsTimingFunction::Type::Frames,
                                   array->Item(1).GetIntValue());
      }
      break;
    default:
      NS_NOTREACHED("Invalid transition property unit");
  }
}

static uint8_t
GetWillChangeBitFieldFromPropFlags(const nsCSSPropertyID& aProp)
{
  uint8_t willChangeBitField = 0;
  if (nsCSSProps::PropHasFlags(aProp, CSS_PROPERTY_CREATES_STACKING_CONTEXT)) {
    willChangeBitField |= NS_STYLE_WILL_CHANGE_STACKING_CONTEXT;
  }

  if (nsCSSProps::PropHasFlags(aProp, CSS_PROPERTY_FIXPOS_CB)) {
    willChangeBitField |= NS_STYLE_WILL_CHANGE_FIXPOS_CB;
  }

  if (nsCSSProps::PropHasFlags(aProp, CSS_PROPERTY_ABSPOS_CB)) {
    willChangeBitField |= NS_STYLE_WILL_CHANGE_ABSPOS_CB;
  }

  return willChangeBitField;
}

const void*
nsRuleNode::ComputeDisplayData(void* aStartStruct,
                               const nsRuleData* aRuleData,
                               GeckoStyleContext* aContext,
                               nsRuleNode* aHighestNode,
                               const RuleDetail aRuleDetail,
                               const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(Display, display, parentDisplay)

  // We may have ended up with aStartStruct's values of mDisplay and
  // mFloat, but those may not be correct if our style data overrides
  // its position or float properties.  Reset to mOriginalDisplay and
  // mOriginalFloat; if it turns out we still need the display/floats
  // adjustments, we'll do them below.
  display->mDisplay = display->mOriginalDisplay;
  display->mFloat = display->mOriginalFloat;

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

  display->mTransitions.SetLengthNonZero(numTransitions);

  FOR_ALL_TRANSITION_PROPS(p) {
    const TransitionPropInfo& i = transitionPropInfo[p];
    TransitionPropData& d = transitionPropData[p];

    display->*(i.sdCount) = d.num;
  }

  // Fill in the transitions we just allocated with the appropriate values.
  for (uint32_t i = 0; i < numTransitions; ++i) {
    StyleTransition *transition = &display->mTransitions[i];

    if (i >= delay.num) {
      MOZ_ASSERT(delay.num, "delay.num must be greater than 0");
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
      MOZ_ASSERT(duration.num, "duration.num must be greater than 0");
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
      MOZ_ASSERT(property.num, "property.num must be greater than 0");
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
        nsCSSPropertyID prop =
          nsCSSProps::LookupProperty(propertyStr,
                                     CSSEnabledState::eForAllContent);
        if (prop == eCSSProperty_UNKNOWN ||
            prop == eCSSPropertyExtra_variable) {
          transition->SetUnknownProperty(prop, propertyStr);
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
      MOZ_ASSERT(timingFunction.num,
        "timingFunction.num must be greater than 0");
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

  display->mAnimations.SetLengthNonZero(numAnimations);

  FOR_ALL_ANIMATION_PROPS(p) {
    const TransitionPropInfo& i = animationPropInfo[p];
    TransitionPropData& d = animationPropData[p];

    display->*(i.sdCount) = d.num;
  }

  // Fill in the animations we just allocated with the appropriate values.
  for (uint32_t i = 0; i < numAnimations; ++i) {
    StyleAnimation *animation = &display->mAnimations[i];

    if (i >= animDelay.num) {
      MOZ_ASSERT(animDelay.num, "animDelay.num must be greater than 0");
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
      MOZ_ASSERT(animDuration.num, "animDuration.num must be greater than 0");
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
      MOZ_ASSERT(animName.num, "animName.num must be greater than 0");
      animation->SetName(display->mAnimations[i % animName.num].GetName());
    } else if (animName.unit == eCSSUnit_Inherit) {
      MOZ_ASSERT(i < parentDisplay->mAnimationNameCount,
                 "animName.num computed incorrectly");
      MOZ_ASSERT(!conditions.Cacheable(),
                 "should have made conditions.Cacheable() false above");
      animation->SetName(parentDisplay->mAnimations[i].GetName());
    } else if (animName.unit == eCSSUnit_Initial ||
               animName.unit == eCSSUnit_Unset) {
      animation->SetName(nsGkAtoms::_empty);
    } else if (animName.list) {
      switch (animName.list->mValue.GetUnit()) {
        case eCSSUnit_String:
        case eCSSUnit_Ident: {
          animation->SetName(
            NS_Atomize(animName.list->mValue.GetStringBufferValue()));
          break;
        }
        case eCSSUnit_None: {
          animation->SetName(nsGkAtoms::_empty);
          break;
        }
        default:
          MOZ_ASSERT(false, "Invalid animation-name unit");
      }
    }

    if (i >= animTimingFunction.num) {
      MOZ_ASSERT(animTimingFunction.num,
        "animTimingFunction.num must be greater than 0");
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
      MOZ_ASSERT(animDirection.num,
        "animDirection.num must be greater than 0");
      animation->SetDirection(display->mAnimations[i % animDirection.num].GetDirection());
    } else if (animDirection.unit == eCSSUnit_Inherit) {
      MOZ_ASSERT(i < parentDisplay->mAnimationDirectionCount,
                 "animDirection.num computed incorrectly");
      MOZ_ASSERT(!conditions.Cacheable(),
                 "should have made conditions.Cacheable() false above");
      animation->SetDirection(parentDisplay->mAnimations[i].GetDirection());
    } else if (animDirection.unit == eCSSUnit_Initial ||
               animDirection.unit == eCSSUnit_Unset) {
      animation->SetDirection(dom::PlaybackDirection::Normal);
    } else if (animDirection.list) {
      MOZ_ASSERT(animDirection.list->mValue.GetUnit() == eCSSUnit_Enumerated,
                 "Invalid animation-direction unit");

      animation->SetDirection(
          static_cast<dom::PlaybackDirection>(animDirection.list->mValue.GetIntValue()));
    }

    if (i >= animFillMode.num) {
      MOZ_ASSERT(animFillMode.num, "animFillMode.num must be greater than 0");
      animation->SetFillMode(display->mAnimations[i % animFillMode.num].GetFillMode());
    } else if (animFillMode.unit == eCSSUnit_Inherit) {
      MOZ_ASSERT(i < parentDisplay->mAnimationFillModeCount,
                 "animFillMode.num computed incorrectly");
      MOZ_ASSERT(!conditions.Cacheable(),
                 "should have made conditions.Cacheable() false above");
      animation->SetFillMode(parentDisplay->mAnimations[i].GetFillMode());
    } else if (animFillMode.unit == eCSSUnit_Initial ||
               animFillMode.unit == eCSSUnit_Unset) {
      animation->SetFillMode(dom::FillMode::None);
    } else if (animFillMode.list) {
      MOZ_ASSERT(animFillMode.list->mValue.GetUnit() == eCSSUnit_Enumerated,
                 "Invalid animation-fill-mode unit");

      animation->SetFillMode(
          static_cast<dom::FillMode>(animFillMode.list->mValue.GetIntValue()));
    }

    if (i >= animPlayState.num) {
      MOZ_ASSERT(animPlayState.num,
        "animPlayState.num must be greater than 0");
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
      MOZ_ASSERT(animIterationCount.num,
        "animIterationCount.num must be greater than 0");
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

  // display: enum, inherit, initial
  SetValue(*aRuleData->ValueForDisplay(), display->mDisplay, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mDisplay,
           StyleDisplay::Inline);

  // contain: none, enum, inherit, initial
  SetValue(*aRuleData->ValueForContain(), display->mContain, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mContain,
           NS_STYLE_CONTAIN_NONE, Unused,
           NS_STYLE_CONTAIN_NONE, Unused, Unused);

  // scroll-behavior: enum, inherit, initial
  SetValue(*aRuleData->ValueForScrollBehavior(), display->mScrollBehavior,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mScrollBehavior, NS_STYLE_SCROLL_BEHAVIOR_AUTO);

  // overscroll-behavior-x: none, enum, inherit, initial
  SetValue(*aRuleData->ValueForOverscrollBehaviorX(),
           display->mOverscrollBehaviorX,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mOverscrollBehaviorX,
           StyleOverscrollBehavior::Auto);

  // overscroll-behavior-y: none, enum, inherit, initial
  SetValue(*aRuleData->ValueForOverscrollBehaviorY(),
           display->mOverscrollBehaviorY,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mOverscrollBehaviorY,
           StyleOverscrollBehavior::Auto);

  // scroll-snap-type-x: none, enum, inherit, initial
  SetValue(*aRuleData->ValueForScrollSnapTypeX(), display->mScrollSnapTypeX,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mScrollSnapTypeX, NS_STYLE_SCROLL_SNAP_TYPE_NONE);

  // scroll-snap-type-y: none, enum, inherit, initial
  SetValue(*aRuleData->ValueForScrollSnapTypeY(), display->mScrollSnapTypeY,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mScrollSnapTypeY, NS_STYLE_SCROLL_SNAP_TYPE_NONE);

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
  SetValue(*aRuleData->ValueForIsolation(), display->mIsolation,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mIsolation, NS_STYLE_ISOLATION_AUTO);

  // -moz-top-layer: enum, inherit, initial
  SetValue(*aRuleData->ValueForTopLayer(), display->mTopLayer,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mTopLayer, NS_STYLE_TOP_LAYER_NONE);

  // Backup original display value for calculation of a hypothetical
  // box (CSS2 10.6.4/10.6.5), in addition to getting our style data right later.
  // See ReflowInput::CalculateHypotheticalBox
  display->mOriginalDisplay = display->mDisplay;

  // appearance: enum, inherit, initial
  SetValue(*aRuleData->ValueForAppearance(),
           display->mAppearance, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mAppearance,
           NS_THEME_NONE);

  // binding: url, none, inherit
  const nsCSSValue* bindingValue = aRuleData->ValueForBinding();
  if (eCSSUnit_URL == bindingValue->GetUnit()) {
    mozilla::css::URLValue* url = bindingValue->GetURLStructValue();
    NS_ASSERTION(url, "What's going on here?");
    display->mBinding = url;
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
  SetValue(*aRuleData->ValueForPosition(), display->mPosition, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mPosition,
           NS_STYLE_POSITION_STATIC);
  // If an element is put in the top layer, while it is not absolutely
  // positioned, the position value should be computed to 'absolute' per
  // the Fullscreen API spec.
  if (display->mTopLayer != NS_STYLE_TOP_LAYER_NONE &&
      !display->IsAbsolutelyPositionedStyle()) {
    display->mPosition = NS_STYLE_POSITION_ABSOLUTE;
    // We cannot cache this struct because otherwise it may be used as
    // an aStartStruct for some other elements.
    conditions.SetUncacheable();
  }

  // clear: enum, inherit, initial
  SetValue(*aRuleData->ValueForClear(), display->mBreakType, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mBreakType,
           StyleClear::None);

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
  SetValue(*aRuleData->ValueForPageBreakInside(),
           display->mBreakInside, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mBreakInside,
           NS_STYLE_PAGE_BREAK_AUTO);

  // touch-action: none, auto, enum, inherit, initial
  SetValue(*aRuleData->ValueForTouchAction(), display->mTouchAction,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mTouchAction,
           /* initial */ NS_STYLE_TOUCH_ACTION_AUTO,
           /* auto */ NS_STYLE_TOUCH_ACTION_AUTO,
           /* none */ NS_STYLE_TOUCH_ACTION_NONE, Unused, Unused);

  // float: enum, inherit, initial
  SetValue(*aRuleData->ValueForFloat(),
           display->mFloat, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mFloat,
           StyleFloat::None);
  // Save mFloat in mOriginalFloat in case we need it later
  display->mOriginalFloat = display->mFloat;

  // overflow-x: enum, inherit, initial
  SetValue(*aRuleData->ValueForOverflowX(),
           display->mOverflowX, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mOverflowX,
           NS_STYLE_OVERFLOW_VISIBLE);

  // overflow-y: enum, inherit, initial
  SetValue(*aRuleData->ValueForOverflowY(),
           display->mOverflowY, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mOverflowY,
           NS_STYLE_OVERFLOW_VISIBLE);

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

  // When 'contain: paint', update overflow from 'visible' to 'clip'.
  if (display->IsContainPaint()) {
    // XXX This actually sets overflow-[x|y] to -moz-hidden-unscrollable.
    if (display->mOverflowX == NS_STYLE_OVERFLOW_VISIBLE) {
      // This uncacheability (and the one below) could be fixed by adding
      // mOriginalOverflowX and mOriginalOverflowY fields, if necessary.
      display->mOverflowX = NS_STYLE_OVERFLOW_CLIP;
      conditions.SetUncacheable();
    }
    if (display->mOverflowY == NS_STYLE_OVERFLOW_VISIBLE) {
      display->mOverflowY = NS_STYLE_OVERFLOW_CLIP;
      conditions.SetUncacheable();
    }
  }

  SetValue(*aRuleData->ValueForOverflowClipBoxBlock(),
           display->mOverflowClipBoxBlock,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mOverflowClipBoxBlock,
           NS_STYLE_OVERFLOW_CLIP_BOX_PADDING_BOX);
  SetValue(*aRuleData->ValueForOverflowClipBoxInline(),
           display->mOverflowClipBoxInline,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mOverflowClipBoxInline,
           NS_STYLE_OVERFLOW_CLIP_BOX_PADDING_BOX);

  SetValue(*aRuleData->ValueForResize(), display->mResize, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mResize,
           NS_STYLE_RESIZE_NONE);

  if (display->mDisplay != StyleDisplay::None) {
    // CSS2 9.7 specifies display type corrections dealing with 'float'
    // and 'position'.  Since generated content can't be floated or
    // positioned, we can deal with it here.

    nsAtom* pseudo = aContext->GetPseudo();
    if (pseudo && display->mDisplay == StyleDisplay::Contents) {
      // We don't want to create frames for anonymous content using a parent
      // frame that is for content above the root of the anon tree.
      // (XXX what we really should check here is not GetPseudo() but if there's
      //  a 'content' property value that implies anon content but we can't
      //  check that here since that's a different struct(?))
      // We might get display:contents to work for CSS_PSEUDO_ELEMENT_CONTAINS_ELEMENTS
      // pseudos (:first-letter etc) in the future, but those have a lot of
      // special handling in frame construction so they are also unsupported
      // for now.
      display->mOriginalDisplay = display->mDisplay = StyleDisplay::Inline;
      conditions.SetUncacheable();
    }

    // Inherit a <fieldset> grid/flex display type into its anon content frame.
    if (pseudo == nsCSSAnonBoxes::fieldsetContent) {
      MOZ_ASSERT(display->mDisplay == StyleDisplay::Block,
                 "forms.css should have set 'display:block'");
      switch (parentDisplay->mDisplay) {
        case StyleDisplay::Grid:
        case StyleDisplay::InlineGrid:
          display->mDisplay = StyleDisplay::Grid;
          conditions.SetUncacheable();
          break;
        case StyleDisplay::Flex:
        case StyleDisplay::InlineFlex:
          display->mDisplay = StyleDisplay::Flex;
          conditions.SetUncacheable();
          break;
        default:
          break; // Do nothing
      }
    }

    if (nsCSSPseudoElements::firstLetter == pseudo) {
      // a non-floating first-letter must be inline
      // XXX this fix can go away once bug 103189 is fixed correctly
      // Note that we reset mOriginalDisplay to enforce the invariant that it equals mDisplay if we're not positioned or floating.
      display->mOriginalDisplay = display->mDisplay = StyleDisplay::Inline;

      // We can't cache the data in the rule tree since if a more specific
      // rule has 'float: left' we'll end up with the wrong 'display'
      // property.
      conditions.SetUncacheable();
    }

    if (display->IsAbsolutelyPositionedStyle()) {
      // 1) if position is 'absolute' or 'fixed' then display must be
      // block-level and float must be 'none'
      EnsureBlockDisplay(display->mDisplay);
      display->mFloat = StyleFloat::None;

      // Note that it's OK to cache this struct in the ruletree
      // because it's fine as-is for any style context that points to
      // it directly, and any use of it as aStartStruct (e.g. if a
      // more specific rule sets "position: static") will use
      // mOriginalDisplay and mOriginalFloat, which we have carefully
      // not changed.
    } else if (display->mFloat != StyleFloat::None) {
      // 2) if float is not none, and display is not none, then we must
      // set a block-level 'display' type per CSS2.1 section 9.7.
      EnsureBlockDisplay(display->mDisplay);

      // Note that it's OK to cache this struct in the ruletree
      // because it's fine as-is for any style context that points to
      // it directly, and any use of it as aStartStruct (e.g. if a
      // more specific rule sets "float: none") will use
      // mOriginalDisplay, which we have carefully not changed.
    }

    if (display->IsContainPaint()) {
      // An element with contain:paint or contain:layout needs to "be a
      // formatting context". For the purposes of the "display" property, that
      // just means we need to promote "display:inline" to "inline-block".
      // XXX We may also need to promote ruby display vals; see bug 1179349.

      // It's okay to cache this change in the rule tree for the same
      // reasons as floats in the previous condition.
      if (display->mDisplay == StyleDisplay::Inline) {
        display->mDisplay = StyleDisplay::InlineBlock;
      }
    }
  }

  SetTransformValue(*aRuleData->ValueForTransform(),
                    display->mSpecifiedTransform, conditions,
                    parentDisplay->mSpecifiedTransform);

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
      nsAtom* atom = item->mValue.GetAtomValue();
      display->mWillChange.AppendElement(atom);
      if (atom == nsGkAtoms::transform) {
        display->mWillChangeBitField |= NS_STYLE_WILL_CHANGE_TRANSFORM;
      } else if (atom == nsGkAtoms::opacity) {
        display->mWillChangeBitField |= NS_STYLE_WILL_CHANGE_OPACITY;
      } else if (atom == nsGkAtoms::scrollPosition) {
        display->mWillChangeBitField |= NS_STYLE_WILL_CHANGE_SCROLL;
      }

      nsDependentAtomString buffer(atom);
      nsCSSPropertyID prop =
        nsCSSProps::LookupProperty(buffer, CSSEnabledState::eForAllContent);
      if (prop != eCSSProperty_UNKNOWN &&
          prop != eCSSPropertyExtra_variable) {
        // If the property given is a shorthand, it indicates the expectation
        // for all the longhands the shorthand expands to.
        if (nsCSSProps::IsShorthand(prop)) {
          for (const nsCSSPropertyID* shorthands =
                  nsCSSProps::SubpropertyEntryFor(prop);
                *shorthands != eCSSProperty_UNKNOWN; ++shorthands) {
            display->mWillChangeBitField |= GetWillChangeBitFieldFromPropFlags(*shorthands);
          }
        } else {
          display->mWillChangeBitField |= GetWillChangeBitFieldFromPropFlags(prop);
        }
      }
    }
    break;
  }

  case eCSSUnit_Inherit:
    display->mWillChange.Clear();
    display->mWillChange.AppendElements(parentDisplay->mWillChange);
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

  // vertical-align: enum, length, percent, calc, inherit
  const nsCSSValue* verticalAlignValue = aRuleData->ValueForVerticalAlign();
  if (!SetCoord(*verticalAlignValue, display->mVerticalAlign,
                parentDisplay->mVerticalAlign,
                SETCOORD_LPH | SETCOORD_ENUMERATED | SETCOORD_STORE_CALC,
                aContext, mPresContext, conditions)) {
    if (eCSSUnit_Initial == verticalAlignValue->GetUnit() ||
        eCSSUnit_Unset == verticalAlignValue->GetUnit()) {
      display->mVerticalAlign.SetIntValue(NS_STYLE_VERTICAL_ALIGN_BASELINE,
                                          eStyleUnit_Enumerated);
    }
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

  SetValue(*aRuleData->ValueForBackfaceVisibility(),
           display->mBackfaceVisibility, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mBackfaceVisibility,
           NS_STYLE_BACKFACE_VISIBILITY_VISIBLE);

  // transform-style: enum, inherit, initial
  SetValue(*aRuleData->ValueForTransformStyle(),
           display->mTransformStyle, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mTransformStyle,
           NS_STYLE_TRANSFORM_STYLE_FLAT);

  // transform-box: enum, inherit, initial
  SetValue(*aRuleData->ValueForTransformBox(),
           display->mTransformBox, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mTransformBox,
           StyleGeometryBox::BorderBox);

  // orient: enum, inherit, initial
  SetValue(*aRuleData->ValueForOrient(),
           display->mOrient, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentDisplay->mOrient,
           StyleOrient::Inline);

  // shape-image-threshold: number, inherit, initial
  SetFactor(*aRuleData->ValueForShapeImageThreshold(),
            display->mShapeImageThreshold, conditions,
            parentDisplay->mShapeImageThreshold, 0.0f,
            SETFCT_OPACITY | SETFCT_UNSET_INITIAL);

  // shape-outside: none | [ <basic-shape> || <shape-box> ] | <image>
  const nsCSSValue* shapeOutsideValue = aRuleData->ValueForShapeOutside();
  switch (shapeOutsideValue->GetUnit()) {
    case eCSSUnit_Null:
      break;
    case eCSSUnit_None:
    case eCSSUnit_Initial:
    case eCSSUnit_Unset:
      display->mShapeOutside = StyleShapeSource();
      break;
    case eCSSUnit_Inherit:
      conditions.SetUncacheable();
      display->mShapeOutside = parentDisplay->mShapeOutside;
      break;
    case eCSSUnit_Image:
    case eCSSUnit_Function:
    case eCSSUnit_Gradient:
    case eCSSUnit_Element: {
      auto shapeImage = MakeUnique<nsStyleImage>();
      SetStyleImage(aContext, *shapeOutsideValue, *shapeImage, conditions);
      display->mShapeOutside = StyleShapeSource();
      display->mShapeOutside.SetShapeImage(Move(shapeImage));
      break;
    }
    case eCSSUnit_Array: {
      display->mShapeOutside = StyleShapeSource();
      SetStyleShapeSourceToCSSValue(&display->mShapeOutside, shapeOutsideValue,
                                    aContext, mPresContext, conditions);
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unrecognized shape-outside unit!");
  }

  COMPUTE_END_RESET(Display, display)
}

const void*
nsRuleNode::ComputeVisibilityData(void* aStartStruct,
                                  const nsRuleData* aRuleData,
                                  GeckoStyleContext* aContext,
                                  nsRuleNode* aHighestNode,
                                  const RuleDetail aRuleDetail,
                                  const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_INHERITED(Visibility, visibility, parentVisibility)

  // IMPORTANT: No properties in this struct have lengths in them.  We
  // depend on this since CalcLengthWith can call StyleVisibility()
  // to get the language for resolving fonts!

  // direction: enum, inherit, initial
  SetValue(*aRuleData->ValueForDirection(), visibility->mDirection,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentVisibility->mDirection,
           (GET_BIDI_OPTION_DIRECTION(mPresContext->GetBidi())
            == IBMBIDI_TEXTDIRECTION_RTL)
            ? NS_STYLE_DIRECTION_RTL : NS_STYLE_DIRECTION_LTR);

  // visibility: enum, inherit, initial
  SetValue(*aRuleData->ValueForVisibility(), visibility->mVisible,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentVisibility->mVisible,
           NS_STYLE_VISIBILITY_VISIBLE);

  // image-rendering: enum, inherit
  SetValue(*aRuleData->ValueForImageRendering(),
           visibility->mImageRendering, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentVisibility->mImageRendering,
           NS_STYLE_IMAGE_RENDERING_AUTO);

  // writing-mode: enum, inherit, initial
  SetValue(*aRuleData->ValueForWritingMode(), visibility->mWritingMode,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentVisibility->mWritingMode,
           NS_STYLE_WRITING_MODE_HORIZONTAL_TB);

  // text-orientation: enum, inherit, initial
  SetValue(*aRuleData->ValueForTextOrientation(), visibility->mTextOrientation,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentVisibility->mTextOrientation,
           NS_STYLE_TEXT_ORIENTATION_MIXED);

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

  SetValue(*aRuleData->ValueForColorAdjust(), visibility->mColorAdjust,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentVisibility->mColorAdjust,
           NS_STYLE_COLOR_ADJUST_ECONOMY);

  COMPUTE_END_INHERITED(Visibility, visibility)
}

const void*
nsRuleNode::ComputeColorData(void* aStartStruct,
                             const nsRuleData* aRuleData,
                             GeckoStyleContext* aContext,
                             nsRuleNode* aHighestNode,
                             const RuleDetail aRuleDetail,
                             const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_INHERITED(Color, color, parentColor)

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
  static void ComputeValue(GeckoStyleContext* aStyleContext,
                           const nsCSSValueList* aSpecifiedValue,
                           uint8_t& aComputedValue,
                           RuleNodeCacheConditions& aConditions)
  {
    SetValue(aSpecifiedValue->mValue, aComputedValue, aConditions,
             SETVAL_ENUMERATED, uint8_t(0), 0);
  }
};

template <>
struct BackgroundItemComputer<nsCSSValuePairList, nsStyleImageLayers::Repeat>
{
  static void ComputeValue(GeckoStyleContext* aStyleContext,
                           const nsCSSValuePairList* aSpecifiedValue,
                           nsStyleImageLayers::Repeat& aComputedValue,
                           RuleNodeCacheConditions& aConditions)
  {
    NS_ASSERTION(aSpecifiedValue->mXValue.GetUnit() == eCSSUnit_Enumerated &&
                 (aSpecifiedValue->mYValue.GetUnit() == eCSSUnit_Enumerated ||
                  aSpecifiedValue->mYValue.GetUnit() == eCSSUnit_Null),
                 "Invalid unit");

    bool hasContraction = true;
    StyleImageLayerRepeat value =
      static_cast<StyleImageLayerRepeat>(aSpecifiedValue->mXValue.GetIntValue());
    switch (value) {
      case StyleImageLayerRepeat::RepeatX:
      aComputedValue.mXRepeat = StyleImageLayerRepeat::Repeat;
      aComputedValue.mYRepeat = StyleImageLayerRepeat::NoRepeat;
      break;
      case StyleImageLayerRepeat::RepeatY:
      aComputedValue.mXRepeat = StyleImageLayerRepeat::NoRepeat;
      aComputedValue.mYRepeat = StyleImageLayerRepeat::Repeat;
      break;
    default:
      NS_ASSERTION(value == StyleImageLayerRepeat::NoRepeat ||
                   value == StyleImageLayerRepeat::Repeat ||
                   value == StyleImageLayerRepeat::Space ||
                   value == StyleImageLayerRepeat::Round, "Unexpected value");
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
      value =
        static_cast<StyleImageLayerRepeat>(aSpecifiedValue->mYValue.GetIntValue());
      NS_ASSERTION(value == StyleImageLayerRepeat::NoRepeat ||
                   value == StyleImageLayerRepeat::Repeat ||
                   value == StyleImageLayerRepeat::Space ||
                   value == StyleImageLayerRepeat::Round, "Unexpected value");
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
  static void ComputeValue(GeckoStyleContext* aStyleContext,
                           const nsCSSValueList* aSpecifiedValue,
                           nsStyleImage& aComputedValue,
                           RuleNodeCacheConditions& aConditions)
  {
    SetStyleImage(aStyleContext, aSpecifiedValue->mValue, aComputedValue,
                  aConditions);
  }
};

template <typename T>
struct BackgroundItemComputer<nsCSSValueList, T>
{
  typedef typename EnableIf<IsEnum<T>::value, T>::Type ComputedType;

  static void ComputeValue(GeckoStyleContext* aStyleContext,
                           const nsCSSValueList* aSpecifiedValue,
                           ComputedType& aComputedValue,
                           RuleNodeCacheConditions& aConditions)
  {
    aComputedValue =
      static_cast<T>(aSpecifiedValue->mValue.GetIntValue());
  }
};

/* Helper function for ComputePositionValue.
 * This function computes a single PositionCoord from two nsCSSValue objects,
 * which represent an edge and an offset from that edge.
 */
static void
ComputePositionCoord(GeckoStyleContext* aStyleContext,
                     const nsCSSValue& aEdge,
                     const nsCSSValue& aOffset,
                     Position::Coord* aResult,
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
    nsRuleNode::ComputedCalc vals;
    if (!ComputeCalc(vals, aOffset, ops)) {
      MOZ_ASSERT_UNREACHABLE("unexpected ComputeCalc failure");
    }
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
    if (aEdge.GetIntValue() & (NS_STYLE_IMAGELAYER_POSITION_BOTTOM |
                               NS_STYLE_IMAGELAYER_POSITION_RIGHT)) {
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
ComputePositionValue(GeckoStyleContext* aStyleContext,
                     const nsCSSValue& aValue,
                     Position& aComputedValue,
                     RuleNodeCacheConditions& aConditions)
{
  NS_ASSERTION(aValue.GetUnit() == eCSSUnit_Array,
               "unexpected unit for CSS <position> value");

  RefPtr<nsCSSValue::Array> positionArray = aValue.GetArrayValue();
  NS_ASSERTION(positionArray->Count() == 4,
               "unexpected number of values in CSS <position> value");

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

/* Helper function to convert the -x or -y part of a CSS <position> specified
 * value into its computed-style form. */
static void
ComputePositionCoordValue(GeckoStyleContext* aStyleContext,
                          const nsCSSValue& aValue,
                          Position::Coord& aComputedValue,
                          RuleNodeCacheConditions& aConditions)
{
  NS_ASSERTION(aValue.GetUnit() == eCSSUnit_Array,
               "unexpected unit for position coord value");

  RefPtr<nsCSSValue::Array> positionArray = aValue.GetArrayValue();
  NS_ASSERTION(positionArray->Count() == 2,
               "unexpected number of values, expecting one edge and one offset");

  const nsCSSValue &edge   = positionArray->Item(0);
  const nsCSSValue &offset = positionArray->Item(1);

  NS_ASSERTION((eCSSUnit_Enumerated == edge.GetUnit() ||
                eCSSUnit_Null       == edge.GetUnit()) &&
               eCSSUnit_Enumerated != offset.GetUnit(),
               "Invalid background position");

  ComputePositionCoord(aStyleContext, edge, offset,
                       &aComputedValue,
                       aConditions);
}

struct BackgroundSizeAxis {
  nsCSSValue nsCSSValuePairList::* specified;
  nsStyleImageLayers::Size::Dimension nsStyleImageLayers::Size::* result;
  uint8_t nsStyleImageLayers::Size::* type;
};

static const BackgroundSizeAxis gBGSizeAxes[] = {
  { &nsCSSValuePairList::mXValue,
    &nsStyleImageLayers::Size::mWidth,
    &nsStyleImageLayers::Size::mWidthType },
  { &nsCSSValuePairList::mYValue,
    &nsStyleImageLayers::Size::mHeight,
    &nsStyleImageLayers::Size::mHeightType }
};

template <>
struct BackgroundItemComputer<nsCSSValuePairList, nsStyleImageLayers::Size>
{
  static void ComputeValue(GeckoStyleContext* aStyleContext,
                           const nsCSSValuePairList* aSpecifiedValue,
                           nsStyleImageLayers::Size& aComputedValue,
                           RuleNodeCacheConditions& aConditions)
  {
    nsStyleImageLayers::Size &size = aComputedValue;
    for (const BackgroundSizeAxis *axis = gBGSizeAxes,
                        *axis_end = ArrayEnd(gBGSizeAxes);
         axis < axis_end; ++axis) {
      const nsCSSValue &specified = aSpecifiedValue->*(axis->specified);
      if (eCSSUnit_Auto == specified.GetUnit()) {
        size.*(axis->type) = nsStyleImageLayers::Size::eAuto;
      }
      else if (eCSSUnit_Enumerated == specified.GetUnit()) {
        static_assert(nsStyleImageLayers::Size::eContain ==
                      NS_STYLE_IMAGELAYER_SIZE_CONTAIN &&
                      nsStyleImageLayers::Size::eCover ==
                      NS_STYLE_IMAGELAYER_SIZE_COVER,
                      "background size constants out of sync");
        MOZ_ASSERT(specified.GetIntValue() == NS_STYLE_IMAGELAYER_SIZE_CONTAIN ||
                   specified.GetIntValue() == NS_STYLE_IMAGELAYER_SIZE_COVER,
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
                     (widthValue.GetIntValue() == NS_STYLE_IMAGELAYER_SIZE_CONTAIN ||
                      widthValue.GetIntValue() == NS_STYLE_IMAGELAYER_SIZE_COVER),
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
        size.*(axis->type) = nsStyleImageLayers::Size::eLengthPercentage;
      }
      else if (specified.IsLengthUnit()) {
        (size.*(axis->result)).mLength =
          CalcLength(specified, aStyleContext, aStyleContext->PresContext(),
                     aConditions);
        (size.*(axis->result)).mPercent = 0.0f;
        (size.*(axis->result)).mHasPercent = false;
        size.*(axis->type) = nsStyleImageLayers::Size::eLengthPercentage;
      } else {
        MOZ_ASSERT(specified.IsCalcUnit(), "unexpected unit");
        LengthPercentPairCalcOps ops(aStyleContext,
                                     aStyleContext->PresContext(),
                                     aConditions);
        nsRuleNode::ComputedCalc vals;
        if (!ComputeCalc(vals, specified, ops)) {
          MOZ_ASSERT_UNREACHABLE("unexpected ComputeCalc failure");
        }
        (size.*(axis->result)).mLength = vals.mLength;
        (size.*(axis->result)).mPercent = vals.mPercent;
        (size.*(axis->result)).mHasPercent = ops.mHasPercent;
        size.*(axis->type) = nsStyleImageLayers::Size::eLengthPercentage;
      }
    }

    MOZ_ASSERT(size.mWidthType < nsStyleImageLayers::Size::eDimensionType_COUNT,
               "bad width type");
    MOZ_ASSERT(size.mHeightType < nsStyleImageLayers::Size::eDimensionType_COUNT,
               "bad height type");
    MOZ_ASSERT((size.mWidthType != nsStyleImageLayers::Size::eContain &&
                size.mWidthType != nsStyleImageLayers::Size::eCover) ||
               size.mWidthType == size.mHeightType,
               "contain/cover apply to both dimensions or to neither");
  }
};

template <class ComputedValueItem>
static void
SetImageLayerList(GeckoStyleContext* aStyleContext,
                  const nsCSSValue& aValue,
                  nsStyleAutoArray<nsStyleImageLayers::Layer>& aLayers,
                  const nsStyleAutoArray<nsStyleImageLayers::Layer>& aParentLayers,
                  ComputedValueItem nsStyleImageLayers::Layer::* aResultLocation,
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

// The same as SetImageLayerList, but for values stored in
// layer.mPosition.*aResultLocation instead of layer.*aResultLocation.
// This code is duplicated because it would be annoying to make
// SetImageLayerList generic enough to handle both cases.
static void
SetImageLayerPositionCoordList(
                  GeckoStyleContext* aStyleContext,
                  const nsCSSValue& aValue,
                  nsStyleAutoArray<nsStyleImageLayers::Layer>& aLayers,
                  const nsStyleAutoArray<nsStyleImageLayers::Layer>& aParentLayers,
                  Position::Coord
                      Position::* aResultLocation,
                  Position::Coord aInitialValue,
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
      aLayers[i].mPosition.*aResultLocation = aParentLayers[i].mPosition.*aResultLocation;
    }
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
    aRebuild = true;
    aItemCount = 1;
    aLayers[0].mPosition.*aResultLocation = aInitialValue;
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

      ComputePositionCoordValue(aStyleContext, item->mValue,
                                aLayers[aItemCount-1].mPosition.*aResultLocation,
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
SetImageLayerPairList(GeckoStyleContext* aStyleContext,
                      const nsCSSValue& aValue,
                      nsStyleAutoArray<nsStyleImageLayers::Layer>& aLayers,
                      const nsStyleAutoArray<nsStyleImageLayers::Layer>& aParentLayers,
                      ComputedValueItem nsStyleImageLayers::Layer::*
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

const void*
nsRuleNode::ComputeBackgroundData(void* aStartStruct,
                                  const nsRuleData* aRuleData,
                                  GeckoStyleContext* aContext,
                                  nsRuleNode* aHighestNode,
                                  const RuleDetail aRuleDetail,
                                  const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(Background, bg, parentBG)

  // background-color: color, inherit
  SetComplexColor<eUnsetInitial>(*aRuleData->ValueForBackgroundColor(),
                                 parentBG->mBackgroundColor,
                                 StyleComplexColor::FromColor(
                                     NS_RGBA(0, 0, 0, 0)),
                                 mPresContext,
                                 bg->mBackgroundColor, conditions);

  uint32_t maxItemCount = 1;
  bool rebuild = false;

  // background-image: url (stored as image), none, inherit [list]
  nsStyleImage initialImage;
  SetImageLayerList(aContext, *aRuleData->ValueForBackgroundImage(),
                    bg->mImage.mLayers,
                    parentBG->mImage.mLayers,
                    &nsStyleImageLayers::Layer::mImage,
                    initialImage, parentBG->mImage.mImageCount,
                    bg->mImage.mImageCount,
                    maxItemCount, rebuild, conditions);

  // background-repeat: enum, inherit, initial [pair list]
  nsStyleImageLayers::Repeat initialRepeat;
  initialRepeat.SetInitialValues();
  SetImageLayerPairList(aContext, *aRuleData->ValueForBackgroundRepeat(),
                        bg->mImage.mLayers,
                        parentBG->mImage.mLayers,
                        &nsStyleImageLayers::Layer::mRepeat,
                        initialRepeat, parentBG->mImage.mRepeatCount,
                        bg->mImage.mRepeatCount, maxItemCount, rebuild,
                        conditions);

  // background-attachment: enum, inherit, initial [list]
  SetImageLayerList(aContext, *aRuleData->ValueForBackgroundAttachment(),
                    bg->mImage.mLayers, parentBG->mImage.mLayers,
                    &nsStyleImageLayers::Layer::mAttachment,
                    uint8_t(NS_STYLE_IMAGELAYER_ATTACHMENT_SCROLL),
                    parentBG->mImage.mAttachmentCount,
                    bg->mImage.mAttachmentCount, maxItemCount, rebuild,
                    conditions);

  // background-clip: enum, inherit, initial [list]
  SetImageLayerList(aContext, *aRuleData->ValueForBackgroundClip(),
                    bg->mImage.mLayers,
                    parentBG->mImage.mLayers,
                    &nsStyleImageLayers::Layer::mClip,
                    StyleGeometryBox::BorderBox,
                    parentBG->mImage.mClipCount,
                    bg->mImage.mClipCount, maxItemCount, rebuild, conditions);

  // background-blend-mode: enum, inherit, initial [list]
  SetImageLayerList(aContext, *aRuleData->ValueForBackgroundBlendMode(),
                    bg->mImage.mLayers,
                    parentBG->mImage.mLayers,
                    &nsStyleImageLayers::Layer::mBlendMode,
                    uint8_t(NS_STYLE_BLEND_NORMAL),
                    parentBG->mImage.mBlendModeCount,
                    bg->mImage.mBlendModeCount, maxItemCount, rebuild,
                    conditions);

  // background-origin: enum, inherit, initial [list]
  SetImageLayerList(aContext, *aRuleData->ValueForBackgroundOrigin(),
                    bg->mImage.mLayers,
                    parentBG->mImage.mLayers,
                    &nsStyleImageLayers::Layer::mOrigin,
                    StyleGeometryBox::PaddingBox,
                    parentBG->mImage.mOriginCount,
                    bg->mImage.mOriginCount, maxItemCount, rebuild,
                    conditions);

  // background-position-x/y: enum, length, percent (flags), inherit [list]
  Position::Coord initialPositionCoord;
  initialPositionCoord.mPercent = 0.0f;
  initialPositionCoord.mLength = 0;
  initialPositionCoord.mHasPercent = true;

  SetImageLayerPositionCoordList(
                    aContext, *aRuleData->ValueForBackgroundPositionX(),
                    bg->mImage.mLayers,
                    parentBG->mImage.mLayers,
                    &Position::mXPosition,
                    initialPositionCoord, parentBG->mImage.mPositionXCount,
                    bg->mImage.mPositionXCount, maxItemCount, rebuild,
                    conditions);
  SetImageLayerPositionCoordList(
                    aContext, *aRuleData->ValueForBackgroundPositionY(),
                    bg->mImage.mLayers,
                    parentBG->mImage.mLayers,
                    &Position::mYPosition,
                    initialPositionCoord, parentBG->mImage.mPositionYCount,
                    bg->mImage.mPositionYCount, maxItemCount, rebuild,
                    conditions);

  // background-size: enum, length, auto, inherit, initial [pair list]
  nsStyleImageLayers::Size initialSize;
  initialSize.SetInitialValues();
  SetImageLayerPairList(aContext, *aRuleData->ValueForBackgroundSize(),
                        bg->mImage.mLayers,
                        parentBG->mImage.mLayers,
                        &nsStyleImageLayers::Layer::mSize,
                        initialSize, parentBG->mImage.mSizeCount,
                        bg->mImage.mSizeCount, maxItemCount, rebuild,
                        conditions);

  if (rebuild) {
    bg->mImage.FillAllLayers(maxItemCount);
  }

  COMPUTE_END_RESET(Background, bg)
}

const void*
nsRuleNode::ComputeMarginData(void* aStartStruct,
                              const nsRuleData* aRuleData,
                              GeckoStyleContext* aContext,
                              nsRuleNode* aHighestNode,
                              const RuleDetail aRuleDetail,
                              const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(Margin, margin, parentMargin)

  // margin: length, percent, calc, inherit
  const nsCSSPropertyID* subprops =
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
                              GeckoStyleContext* aContext,
                              nsRuleNode* aHighestNode,
                              const RuleDetail aRuleDetail,
                              const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(Border, border, parentBorder)

  // box-decoration-break: enum, inherit, initial
  SetValue(*aRuleData->ValueForBoxDecorationBreak(),
           border->mBoxDecorationBreak, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentBorder->mBoxDecorationBreak,
           StyleBoxDecorationBreak::Slice);

  // border-width, border-*-width: length, enum, inherit
  nsStyleCoord coord;
  {
    const nsCSSPropertyID* subprops =
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
      Maybe<nscoord> coord =
        ComputeLineWidthValue<eUnsetInitial>(
          value, parentBorder->GetComputedBorder().Side(side),
          nsPresContext::GetBorderWidthForKeyword(NS_STYLE_BORDER_WIDTH_MEDIUM),
          aContext, mPresContext, conditions);
      if (coord.isSome()) {
        border->SetBorderWidth(side, *coord);
      }
    }
  }

  // border-style, border-*-style: enum, inherit
  {
    const nsCSSPropertyID* subprops =
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

  static const nsCSSPropertyID borderColorsProps[] = {
    eCSSProperty__moz_border_top_colors,
    eCSSProperty__moz_border_right_colors,
    eCSSProperty__moz_border_bottom_colors,
    eCSSProperty__moz_border_left_colors
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
      if (parentBorder->mBorderColors) {
        border->EnsureBorderColors();
        border->mBorderColors->mColors[side] =
          parentBorder->mBorderColors->mColors[side];
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
          border->mBorderColors->mColors[side].AppendElement(borderColor);
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
    const nsCSSPropertyID* subprops =
      nsCSSProps::SubpropertyEntryFor(eCSSProperty_border_color);
    NS_FOR_CSS_SIDES(side) {
      SetComplexColor<eUnsetInitial>(*aRuleData->ValueFor(subprops[side]),
                                     parentBorder->mBorderColor[side],
                                     StyleComplexColor::CurrentColor(),
                                     mPresContext,
                                     border->mBorderColor[side], conditions);
    }
  }

  // border-radius: length, percent, inherit
  {
    const nsCSSPropertyID* subprops =
      nsCSSProps::SubpropertyEntryFor(eCSSProperty_border_radius);
    NS_FOR_CSS_FULL_CORNERS(corner) {
      int cx = FullToHalfCorner(corner, false);
      int cy = FullToHalfCorner(corner, true);
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
  SetValue(*aRuleData->ValueForFloatEdge(),
           border->mFloatEdge, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentBorder->mFloatEdge,
           StyleFloatEdge::ContentBox);

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
  SetValue(borderImageSliceFill,
           border->mBorderImageFill,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentBorder->mBorderImageFill,
           NS_STYLE_BORDER_IMAGE_SLICE_NOFILL);

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

  SetValue(borderImageRepeat.mXValue,
           border->mBorderImageRepeatH,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentBorder->mBorderImageRepeatH,
           NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH);

  SetValue(borderImageRepeat.mYValue,
           border->mBorderImageRepeatV,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentBorder->mBorderImageRepeatV,
           NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH);

  COMPUTE_END_RESET(Border, border)
}

const void*
nsRuleNode::ComputePaddingData(void* aStartStruct,
                               const nsRuleData* aRuleData,
                               GeckoStyleContext* aContext,
                               nsRuleNode* aHighestNode,
                               const RuleDetail aRuleDetail,
                               const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(Padding, padding, parentPadding)

  // padding: length, percent, calc, inherit
  const nsCSSPropertyID* subprops =
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

  COMPUTE_END_RESET(Padding, padding)
}

const void*
nsRuleNode::ComputeOutlineData(void* aStartStruct,
                               const nsRuleData* aRuleData,
                               GeckoStyleContext* aContext,
                               nsRuleNode* aHighestNode,
                               const RuleDetail aRuleDetail,
                               const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(Outline, outline, parentOutline)

  // outline-width: length, enum, inherit
  Maybe<nscoord> coord =
    ComputeLineWidthValue<eUnsetInitial>(
      *aRuleData->ValueForOutlineWidth(), parentOutline->mOutlineWidth,
      nsPresContext::GetBorderWidthForKeyword(NS_STYLE_BORDER_WIDTH_MEDIUM),
      aContext, mPresContext, conditions);
  if (coord.isSome()) {
    outline->mOutlineWidth = *coord;
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
  SetComplexColor<eUnsetInitial>(*aRuleData->ValueForOutlineColor(),
                                 parentOutline->mOutlineColor,
                                 StyleComplexColor::CurrentColor(),
                                 mPresContext,
                                 outline->mOutlineColor, conditions);

  // -moz-outline-radius: length, percent, inherit
  {
    const nsCSSPropertyID* subprops =
      nsCSSProps::SubpropertyEntryFor(eCSSProperty__moz_outline_radius);
    NS_FOR_CSS_FULL_CORNERS(corner) {
      int cx = FullToHalfCorner(corner, false);
      int cy = FullToHalfCorner(corner, true);
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
  // cannot use SetValue because of SetOutlineStyle
  const nsCSSValue* outlineStyleValue = aRuleData->ValueForOutlineStyle();
  nsCSSUnit unit = outlineStyleValue->GetUnit();
  MOZ_ASSERT(eCSSUnit_None != unit && eCSSUnit_Auto != unit,
             "'none' and 'auto' should be handled as enumerated values");
  if (eCSSUnit_Enumerated == unit) {
    outline->mOutlineStyle = outlineStyleValue->GetIntValue();
  } else if (eCSSUnit_Initial == unit ||
             eCSSUnit_Unset == unit) {
    outline->mOutlineStyle = NS_STYLE_BORDER_STYLE_NONE;
  } else if (eCSSUnit_Inherit == unit) {
    conditions.SetUncacheable();
    outline->mOutlineStyle = parentOutline->mOutlineStyle;
  }

  outline->RecalcData();
  COMPUTE_END_RESET(Outline, outline)
}

const void*
nsRuleNode::ComputeListData(void* aStartStruct,
                            const nsRuleData* aRuleData,
                            GeckoStyleContext* aContext,
                            nsRuleNode* aHighestNode,
                            const RuleDetail aRuleDetail,
                            const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_INHERITED(List, list, parentList)

  // quotes: inherit, initial, none, [string string]+
  const nsCSSValue* quotesValue = aRuleData->ValueForQuotes();
  switch (quotesValue->GetUnit()) {
  case eCSSUnit_Null:
    break;
  case eCSSUnit_Inherit:
  case eCSSUnit_Unset:
    conditions.SetUncacheable();
    list->SetQuotesInherit(parentList);
    break;
  case eCSSUnit_Initial:
    list->SetQuotesInitial();
    break;
  case eCSSUnit_None:
    list->SetQuotesNone();
    break;
  case eCSSUnit_PairList:
  case eCSSUnit_PairListDep: {
    const nsCSSValuePairList* ourQuotes = quotesValue->GetPairListValue();

    nsStyleQuoteValues::QuotePairArray quotePairs;
    quotePairs.SetLength(ListLength(ourQuotes));

    size_t index = 0;
    nsAutoString buffer;
    while (ourQuotes) {
      MOZ_ASSERT(ourQuotes->mXValue.GetUnit() == eCSSUnit_String &&
                 ourQuotes->mYValue.GetUnit() == eCSSUnit_String,
                 "improper list contents for quotes");
      quotePairs[index].first  = ourQuotes->mXValue.GetStringValue(buffer);
      quotePairs[index].second = ourQuotes->mYValue.GetStringValue(buffer);
      ++index;
      ourQuotes = ourQuotes->mNext;
    }
    list->SetQuotes(Move(quotePairs));
    break;
  }
  default:
    MOZ_ASSERT(false, "unexpected value unit");
  }

  // list-style-type: string, none, inherit, initial
  const nsCSSValue* typeValue = aRuleData->ValueForListStyleType();
  auto setListStyleType = [this, list](nsAtom* type) {
    list->mCounterStyle = mPresContext->
      CounterStyleManager()->BuildCounterStyle(type);
  };
  switch (typeValue->GetUnit()) {
    case eCSSUnit_Unset:
    case eCSSUnit_Inherit: {
      conditions.SetUncacheable();
      list->mCounterStyle = parentList->mCounterStyle;
      break;
    }
    case eCSSUnit_Initial:
      setListStyleType(nsGkAtoms::disc);
      break;
    case eCSSUnit_AtomIdent: {
      setListStyleType(typeValue->GetAtomValue());
      break;
    }
    case eCSSUnit_String: {
      nsString str;
      typeValue->GetStringValue(str);
      list->mCounterStyle = new AnonymousCounterStyle(str);
      break;
    }
    case eCSSUnit_Enumerated: {
      // For compatibility with html attribute map. This branch should
      // never be called for value from CSS. The values can only come
      // from the items in EnumTable listed in HTMLLIElement.cpp and
      // HTMLSharedListElement.cpp.
      int32_t intValue = typeValue->GetIntValue();
      RefPtr<nsAtom> name;
      switch (intValue) {
        case NS_STYLE_LIST_STYLE_LOWER_ROMAN:
          name = nsGkAtoms::lowerRoman;
          break;
        case NS_STYLE_LIST_STYLE_UPPER_ROMAN:
          name = nsGkAtoms::upperRoman;
          break;
        case NS_STYLE_LIST_STYLE_LOWER_ALPHA:
          name = nsGkAtoms::lowerAlpha;
          break;
        case NS_STYLE_LIST_STYLE_UPPER_ALPHA:
          name = nsGkAtoms::upperAlpha;
          break;
        default:
          name = CounterStyleManager::GetStyleNameFromType(intValue);
          break;
      }
      setListStyleType(name);
      break;
    }
    case eCSSUnit_Symbols:
      list->mCounterStyle =
        new AnonymousCounterStyle(typeValue->GetArrayValue());
      break;
    case eCSSUnit_Null:
      break;
    default:
      NS_NOTREACHED("Unexpected value unit");
  }

  // list-style-image: url, none, inherit
  const nsCSSValue* imageValue = aRuleData->ValueForListStyleImage();
  if (eCSSUnit_Image == imageValue->GetUnit()) {
    list->mListStyleImage = CreateStyleImageRequest(
      mPresContext, *imageValue, nsStyleImageRequest::Mode(0));
  }
  else if (eCSSUnit_None == imageValue->GetUnit() ||
           eCSSUnit_Initial == imageValue->GetUnit()) {
    list->mListStyleImage = nullptr;
  }
  else if (eCSSUnit_Inherit == imageValue->GetUnit() ||
           eCSSUnit_Unset == imageValue->GetUnit()) {
    conditions.SetUncacheable();
    list->mListStyleImage = parentList->mListStyleImage;
  }

  // list-style-position: enum, inherit, initial
  SetValue(*aRuleData->ValueForListStylePosition(),
           list->mListStylePosition, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentList->mListStylePosition,
           NS_STYLE_LIST_STYLE_POSITION_OUTSIDE);

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
                    GeckoStyleContext* aStyleContext,
                    nsPresContext* aPresContext,
                    RuleNodeCacheConditions& aConditions)
{
  nsCSSUnit unit = aValue.GetUnit();
  if (unit == eCSSUnit_FlexFraction) {
    aResult.SetFlexFractionValue(aValue.GetFloatValue());
  } else if (unit == eCSSUnit_Auto) {
    aResult.SetAutoValue();
  } else if (unit == eCSSUnit_None) {
    // For fit-content().
    aResult.SetNoneValue();
  } else {
    MOZ_ASSERT(unit != eCSSUnit_Inherit && unit != eCSSUnit_Unset,
               "Unexpected value that would use dummyParentCoord");
    const nsStyleCoord dummyParentCoord;
    DebugOnly<bool> stored =
      SetCoord(aValue, aResult, dummyParentCoord,
               SETCOORD_LPE | SETCOORD_STORE_CALC,
               aStyleContext, aPresContext, aConditions);
    MOZ_ASSERT(stored, "invalid <track-size> value");
  }
}

static void
SetGridTrackSize(const nsCSSValue& aValue,
                 nsStyleCoord& aResultMin,
                 nsStyleCoord& aResultMax,
                 GeckoStyleContext* aStyleContext,
                 nsPresContext* aPresContext,
                 RuleNodeCacheConditions& aConditions)
{
  if (aValue.GetUnit() == eCSSUnit_Function) {
    nsCSSValue::Array* func = aValue.GetArrayValue();
    auto funcName = func->Item(0).GetKeywordValue();
    if (funcName == eCSSKeyword_minmax) {
      SetGridTrackBreadth(func->Item(1), aResultMin,
                          aStyleContext, aPresContext, aConditions);
      SetGridTrackBreadth(func->Item(2), aResultMax,
                          aStyleContext, aPresContext, aConditions);
    } else if (funcName == eCSSKeyword_fit_content) {
      // We represent fit-content(L) as 'none' min-sizing and L max-sizing.
      SetGridTrackBreadth(nsCSSValue(eCSSUnit_None), aResultMin,
                          aStyleContext, aPresContext, aConditions);
      SetGridTrackBreadth(func->Item(1), aResultMax,
                          aStyleContext, aPresContext, aConditions);
    } else {
      NS_ERROR("Expected minmax() or fit-content(), got another function name");
    }
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
                       GeckoStyleContext* aStyleContext,
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
                    nsTArray<nsString>& aNameList)
{
  // Compute a <line-names> value
  // Null unit means empty list, nothing more to do.
  if (aValue.GetUnit() != eCSSUnit_Null) {
    const nsCSSValueList* item = aValue.GetListValue();
    do {
      nsString* name = aNameList.AppendElement();
      item->mValue.GetStringValue(*name);
      item = item->mNext;
    } while (item);
  }
}

static void
SetGridTrackList(const nsCSSValue& aValue,
                 UniquePtr<nsStyleGridTemplate>& aResult,
                 const nsStyleGridTemplate* aParentValue,
                 GeckoStyleContext* aStyleContext,
                 nsPresContext* aPresContext,
                 RuleNodeCacheConditions& aConditions)

{
  switch (aValue.GetUnit()) {
  case eCSSUnit_Null:
    break;

  case eCSSUnit_Inherit:
    aConditions.SetUncacheable();
    if (aParentValue) {
      aResult = MakeUnique<nsStyleGridTemplate>(*aParentValue);
    } else {
      aResult = nullptr;
    }
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
  case eCSSUnit_None:
    aResult = nullptr;
    break;

  default:
    aResult = MakeUnique<nsStyleGridTemplate>();
    const nsCSSValueList* item = aValue.GetListValue();
    if (item->mValue.GetUnit() == eCSSUnit_Enumerated &&
        item->mValue.GetIntValue() == NS_STYLE_GRID_TEMPLATE_SUBGRID) {
      // subgrid <line-name-list>?
      aResult->mIsSubgrid = true;
      item = item->mNext;
      for (int32_t i = 0; item && i < nsStyleGridLine::kMaxLine; ++i) {
        if (item->mValue.GetUnit() == eCSSUnit_Pair) {
          // This is a 'auto-fill' <name-repeat> expression.
          const nsCSSValuePair& pair = item->mValue.GetPairValue();
          MOZ_ASSERT(aResult->mRepeatAutoIndex == -1,
                     "can only have one <name-repeat> with auto-fill");
          aResult->mRepeatAutoIndex = i;
          aResult->mIsAutoFill = true;
          MOZ_ASSERT(pair.mXValue.GetIntValue() == NS_STYLE_GRID_REPEAT_AUTO_FILL,
                     "unexpected repeat() enum value for subgrid");
          const nsCSSValueList* list = pair.mYValue.GetListValue();
          AppendGridLineNames(list->mValue, aResult->mRepeatAutoLineNameListBefore);
        } else {
          AppendGridLineNames(item->mValue,
                              *aResult->mLineNameLists.AppendElement());
        }
        item = item->mNext;
      }
    } else {
      // <track-list>
      // The list is expected to have odd number of items, at least 3
      // starting with a <line-names> (sub list of identifiers),
      // and alternating between that and <track-size>.
      aResult->mIsSubgrid = false;
      for (int32_t line = 1;  ; ++line) {
        AppendGridLineNames(item->mValue,
                            *aResult->mLineNameLists.AppendElement());
        item = item->mNext;

        if (!item || line == nsStyleGridLine::kMaxLine) {
          break;
        }

        if (item->mValue.GetUnit() == eCSSUnit_Pair) {
          // This is a 'auto-fill' / 'auto-fit' <auto-repeat> expression.
          const nsCSSValuePair& pair = item->mValue.GetPairValue();
          MOZ_ASSERT(aResult->mRepeatAutoIndex == -1,
                     "can only have one <auto-repeat>");
          aResult->mRepeatAutoIndex = line - 1;
          switch (pair.mXValue.GetIntValue()) {
            case NS_STYLE_GRID_REPEAT_AUTO_FILL:
              aResult->mIsAutoFill = true;
              break;
            case NS_STYLE_GRID_REPEAT_AUTO_FIT:
              aResult->mIsAutoFill = false;
              break;
            default:
              MOZ_ASSERT_UNREACHABLE("unexpected repeat() enum value");
          }
          const nsCSSValueList* list = pair.mYValue.GetListValue();
          AppendGridLineNames(list->mValue, aResult->mRepeatAutoLineNameListBefore);
          list = list->mNext;
          nsStyleCoord& min = *aResult->mMinTrackSizingFunctions.AppendElement();
          nsStyleCoord& max = *aResult->mMaxTrackSizingFunctions.AppendElement();
          SetGridTrackSize(list->mValue, min, max,
                           aStyleContext, aPresContext, aConditions);
          list = list->mNext;
          AppendGridLineNames(list->mValue, aResult->mRepeatAutoLineNameListAfter);
        } else {
          nsStyleCoord& min = *aResult->mMinTrackSizingFunctions.AppendElement();
          nsStyleCoord& max = *aResult->mMaxTrackSizingFunctions.AppendElement();
          SetGridTrackSize(item->mValue, min, max,
                           aStyleContext, aPresContext, aConditions);
        }

        item = item->mNext;
        MOZ_ASSERT(item, "Expected a eCSSUnit_List of odd length");
      }
      MOZ_ASSERT(!aResult->mMinTrackSizingFunctions.IsEmpty() &&
                 aResult->mMinTrackSizingFunctions.Length() ==
                 aResult->mMaxTrackSizingFunctions.Length() &&
                 aResult->mMinTrackSizingFunctions.Length() + 1 ==
                 aResult->mLineNameLists.Length(),
                 "Inconstistent array lengths for nsStyleGridTemplate");
    }
  }
}

static void
SetGridTemplateAreas(const nsCSSValue& aValue,
                     RefPtr<css::GridTemplateAreasValue>* aResult,
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
                                GeckoStyleContext* aContext,
                                nsRuleNode* aHighestNode,
                                const RuleDetail aRuleDetail,
                                const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(Position, pos, parentPos)

  // box offsets: length, percent, calc, auto, inherit
  static const nsCSSPropertyID offsetProps[] = {
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

  WritingMode wm(aContext);
  bool vertical = wm.IsVertical();

  const nsCSSValue* width = aRuleData->ValueForWidth();
  if (width->GetUnit() == eCSSUnit_Enumerated) {
    conditions.SetWritingModeDependency(wm.GetBits());
  }
  SetCoord(width->GetUnit() == eCSSUnit_Enumerated && vertical ?
             nsCSSValue(eCSSUnit_Unset) : *width,
           pos->mWidth, parentPos->mWidth,
           SETCOORD_LPAEH | SETCOORD_INITIAL_AUTO | SETCOORD_STORE_CALC |
             SETCOORD_UNSET_INITIAL,
           aContext, mPresContext, conditions);

  const nsCSSValue* minWidth = aRuleData->ValueForMinWidth();
  if (minWidth->GetUnit() == eCSSUnit_Enumerated) {
    conditions.SetWritingModeDependency(wm.GetBits());
  }
  SetCoord(minWidth->GetUnit() == eCSSUnit_Enumerated && vertical ?
             nsCSSValue(eCSSUnit_Unset) : *minWidth,
           pos->mMinWidth, parentPos->mMinWidth,
           SETCOORD_LPAEH | SETCOORD_INITIAL_AUTO | SETCOORD_STORE_CALC |
             SETCOORD_UNSET_INITIAL,
           aContext, mPresContext, conditions);

  const nsCSSValue* maxWidth = aRuleData->ValueForMaxWidth();
  if (maxWidth->GetUnit() == eCSSUnit_Enumerated) {
    conditions.SetWritingModeDependency(wm.GetBits());
  }
  SetCoord(maxWidth->GetUnit() == eCSSUnit_Enumerated && vertical ?
             nsCSSValue(eCSSUnit_Unset) : *maxWidth,
           pos->mMaxWidth, parentPos->mMaxWidth,
           SETCOORD_LPOEH | SETCOORD_INITIAL_NONE | SETCOORD_STORE_CALC |
             SETCOORD_UNSET_INITIAL,
           aContext, mPresContext, conditions);

  const nsCSSValue* height = aRuleData->ValueForHeight();
  if (height->GetUnit() == eCSSUnit_Enumerated) {
    conditions.SetWritingModeDependency(wm.GetBits());
  }
  SetCoord(height->GetUnit() == eCSSUnit_Enumerated && !vertical ?
             nsCSSValue(eCSSUnit_Unset) : *height,
           pos->mHeight, parentPos->mHeight,
           SETCOORD_LPAEH | SETCOORD_INITIAL_AUTO | SETCOORD_STORE_CALC |
             SETCOORD_UNSET_INITIAL,
           aContext, mPresContext, conditions);

  const nsCSSValue* minHeight = aRuleData->ValueForMinHeight();
  if (minHeight->GetUnit() == eCSSUnit_Enumerated) {
    conditions.SetWritingModeDependency(wm.GetBits());
  }
  SetCoord(minHeight->GetUnit() == eCSSUnit_Enumerated && !vertical ?
             nsCSSValue(eCSSUnit_Unset) : *minHeight,
           pos->mMinHeight, parentPos->mMinHeight,
           SETCOORD_LPAEH | SETCOORD_INITIAL_AUTO | SETCOORD_STORE_CALC |
             SETCOORD_UNSET_INITIAL,
           aContext, mPresContext, conditions);

  const nsCSSValue* maxHeight = aRuleData->ValueForMaxHeight();
  if (maxHeight->GetUnit() == eCSSUnit_Enumerated) {
    conditions.SetWritingModeDependency(wm.GetBits());
  }
  SetCoord(maxHeight->GetUnit() == eCSSUnit_Enumerated && !vertical ?
             nsCSSValue(eCSSUnit_Unset) : *maxHeight,
           pos->mMaxHeight, parentPos->mMaxHeight,
           SETCOORD_LPOEH | SETCOORD_INITIAL_NONE | SETCOORD_STORE_CALC |
             SETCOORD_UNSET_INITIAL,
           aContext, mPresContext, conditions);

  // box-sizing: enum, inherit, initial
  SetValue(*aRuleData->ValueForBoxSizing(),
           pos->mBoxSizing, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentPos->mBoxSizing,
           StyleBoxSizing::Content);

  // align-content: enum, inherit, initial
  SetValue(*aRuleData->ValueForAlignContent(),
           pos->mAlignContent, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentPos->mAlignContent,
           NS_STYLE_ALIGN_NORMAL);

  // align-items: enum, inherit, initial
  SetValue(*aRuleData->ValueForAlignItems(),
           pos->mAlignItems, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentPos->mAlignItems,
           NS_STYLE_ALIGN_NORMAL);

  // align-self: enum, inherit, initial
  SetValue(*aRuleData->ValueForAlignSelf(),
           pos->mAlignSelf, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentPos->mAlignSelf,
           NS_STYLE_ALIGN_AUTO);

  // justify-content: enum, inherit, initial
  SetValue(*aRuleData->ValueForJustifyContent(),
           pos->mJustifyContent, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentPos->mJustifyContent,
           NS_STYLE_JUSTIFY_NORMAL);

  // justify-items: enum, inherit, initial
  const auto& justifyItemsValue = *aRuleData->ValueForJustifyItems();
  if (MOZ_UNLIKELY(justifyItemsValue.GetUnit() == eCSSUnit_Inherit)) {
    pos->mSpecifiedJustifyItems =
      MOZ_LIKELY(parentContext)
        ? parentPos->mJustifyItems
        : NS_STYLE_JUSTIFY_NORMAL;
    conditions.SetUncacheable();
  } else {
    SetValue(justifyItemsValue,
             pos->mSpecifiedJustifyItems, conditions,
             SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
             parentPos->mSpecifiedJustifyItems, // unused, we handle 'inherit' above
             NS_STYLE_JUSTIFY_AUTO);
  }

  // NOTE(emilio): Even though "auto" technically depends on the parent style
  // context, most of the time it'll resolve to "normal".  So, we
  // optimistically assume here that it does resolve to "normal", and we
  // handle the other cases in ApplyStyleFixups. This way, position structs
  // can be cached in the default/common case.
  pos->mJustifyItems =
    pos->mSpecifiedJustifyItems == NS_STYLE_JUSTIFY_AUTO
      ? NS_STYLE_JUSTIFY_NORMAL : pos->mSpecifiedJustifyItems;

  // justify-self: enum, inherit, initial
  SetValue(*aRuleData->ValueForJustifySelf(),
           pos->mJustifySelf, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentPos->mJustifySelf,
           NS_STYLE_JUSTIFY_AUTO);

  // flex-basis: auto, length, percent, enum, calc, inherit, initial
  // (Note: The flags here should match those used for 'width' property above.)
  SetCoord(*aRuleData->ValueForFlexBasis(), pos->mFlexBasis, parentPos->mFlexBasis,
           SETCOORD_LPAEH | SETCOORD_INITIAL_AUTO | SETCOORD_STORE_CALC |
             SETCOORD_UNSET_INITIAL,
           aContext, mPresContext, conditions);

  // flex-direction: enum, inherit, initial
  SetValue(*aRuleData->ValueForFlexDirection(),
           pos->mFlexDirection, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentPos->mFlexDirection,
           NS_STYLE_FLEX_DIRECTION_ROW);

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
  SetValue(*aRuleData->ValueForFlexWrap(),
           pos->mFlexWrap, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentPos->mFlexWrap,
           NS_STYLE_FLEX_WRAP_NOWRAP);

  // order: integer, inherit, initial
  SetValue(*aRuleData->ValueForOrder(),
           pos->mOrder, conditions,
           SETVAL_INTEGER | SETVAL_UNSET_INITIAL,
           parentPos->mOrder,
           NS_STYLE_ORDER_INITIAL);

  // object-fit: enum, inherit, initial
  SetValue(*aRuleData->ValueForObjectFit(),
           pos->mObjectFit, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentPos->mObjectFit,
           NS_STYLE_OBJECT_FIT_FILL);

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
                   pos->mGridTemplateColumns,
                   parentPos->mGridTemplateColumns.get(),
                   aContext, mPresContext, conditions);

  // grid-template-rows
  SetGridTrackList(*aRuleData->ValueForGridTemplateRows(),
                   pos->mGridTemplateRows,
                   parentPos->mGridTemplateRows.get(),
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

  // grid-column-gap
  if (SetCoord(*aRuleData->ValueForGridColumnGap(),
               pos->mGridColumnGap, parentPos->mGridColumnGap,
               SETCOORD_LPH | SETCOORD_INITIAL_ZERO | SETCOORD_STORE_CALC |
               SETCOORD_CALC_CLAMP_NONNEGATIVE | SETCOORD_UNSET_INITIAL,
               aContext, mPresContext, conditions)) {
  } else {
    MOZ_ASSERT(aRuleData->ValueForGridColumnGap()->GetUnit() == eCSSUnit_Null,
               "unexpected unit");
  }

  // grid-row-gap
  if (SetCoord(*aRuleData->ValueForGridRowGap(),
               pos->mGridRowGap, parentPos->mGridRowGap,
               SETCOORD_LPH | SETCOORD_INITIAL_ZERO | SETCOORD_STORE_CALC |
               SETCOORD_CALC_CLAMP_NONNEGATIVE | SETCOORD_UNSET_INITIAL,
               aContext, mPresContext, conditions)) {
  } else {
    MOZ_ASSERT(aRuleData->ValueForGridRowGap()->GetUnit() == eCSSUnit_Null,
               "unexpected unit");
  }

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
                             GeckoStyleContext* aContext,
                             nsRuleNode* aHighestNode,
                             const RuleDetail aRuleDetail,
                             const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(Table, table, parentTable)

  // table-layout: enum, inherit, initial
  SetValue(*aRuleData->ValueForTableLayout(),
           table->mLayoutStrategy, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentTable->mLayoutStrategy,
           NS_STYLE_TABLE_LAYOUT_AUTO);

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
                                   GeckoStyleContext* aContext,
                                   nsRuleNode* aHighestNode,
                                   const RuleDetail aRuleDetail,
                                   const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_INHERITED(TableBorder, table, parentTable)

  // border-collapse: enum, inherit, initial
  SetValue(*aRuleData->ValueForBorderCollapse(), table->mBorderCollapse,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentTable->mBorderCollapse,
           NS_STYLE_BORDER_SEPARATE);

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
  SetValue(*aRuleData->ValueForCaptionSide(),
           table->mCaptionSide, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentTable->mCaptionSide,
           NS_STYLE_CAPTION_SIDE_TOP);

  // empty-cells: enum, inherit, initial
  SetValue(*aRuleData->ValueForEmptyCells(),
           table->mEmptyCells, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentTable->mEmptyCells,
           NS_STYLE_TABLE_EMPTY_CELLS_SHOW);

  COMPUTE_END_INHERITED(TableBorder, table)
}

const void*
nsRuleNode::ComputeContentData(void* aStartStruct,
                               const nsRuleData* aRuleData,
                               GeckoStyleContext* aContext,
                               nsRuleNode* aHighestNode,
                               const RuleDetail aRuleDetail,
                               const RuleNodeCacheConditions aConditions)
{
  uint32_t count;
  nsAutoString buffer;

  COMPUTE_START_RESET(Content, content, parentContent)

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
    content->AllocateContents(count);
    while (0 < count--) {
      content->ContentAt(count) = parentContent->ContentAt(count);
    }
    break;

  case eCSSUnit_Enumerated: {
    MOZ_ASSERT(contentValue->GetIntValue() == int32_t(StyleContent::AltContent),
               "unrecognized solitary content keyword");
    content->AllocateContents(1);
    content->ContentAt(0).SetKeyword(eStyleContentType_AltContent);
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
    content->AllocateContents(count);
    const nsAutoString nullStr;
    count = 0;
    contentValueList = contentValue->GetListValue();
    while (contentValueList) {
      const nsCSSValue& value = contentValueList->mValue;
      nsCSSUnit unit = value.GetUnit();
      nsStyleContentData& data = content->ContentAt(count++);
      switch (unit) {
        case eCSSUnit_Image:
          data.SetImageRequest(CreateStyleImageRequest(mPresContext, value));
          break;
        case eCSSUnit_String:
        case eCSSUnit_Attr: {
          nsStyleContentType type =
            unit == eCSSUnit_String ? eStyleContentType_String
                                    : eStyleContentType_Attr;
          value.GetStringValue(buffer);
          data.SetString(type, buffer.get());
          break;
        }
        case eCSSUnit_Counter:
        case eCSSUnit_Counters: {
          nsStyleContentType type =
            unit == eCSSUnit_Counter ? eStyleContentType_Counter
                                     : eStyleContentType_Counters;
          RefPtr<nsStyleContentData::CounterFunction>
            counterFunc = new nsStyleContentData::CounterFunction();
          nsCSSValue::Array* arrayValue = value.GetArrayValue();
          arrayValue->Item(0).GetStringValue(counterFunc->mIdent);
          if (unit == eCSSUnit_Counters) {
            arrayValue->Item(1).GetStringValue(counterFunc->mSeparator);
          }
          const nsCSSValue& style =
            value.GetArrayValue()->Item(unit == eCSSUnit_Counters ? 2 : 1);
          if (style.GetUnit() == eCSSUnit_AtomIdent) {
            counterFunc->mCounterStyle = mPresContext->
              CounterStyleManager()->BuildCounterStyle(style.GetAtomValue());
          } else if (style.GetUnit() == eCSSUnit_Symbols) {
            counterFunc->mCounterStyle =
              new AnonymousCounterStyle(style.GetArrayValue());
          } else {
            MOZ_ASSERT_UNREACHABLE("Unknown counter style");
            counterFunc->mCounterStyle = CounterStyleManager::GetDecimalStyle();
          }
          data.SetCounters(type, counterFunc.forget());
          break;
        }
        case eCSSUnit_Enumerated:
          switch (value.GetIntValue()) {
		  case uint8_t(StyleContent::OpenQuote):
              data.SetKeyword(eStyleContentType_OpenQuote);
              break;
		  case uint8_t(StyleContent::CloseQuote):
              data.SetKeyword(eStyleContentType_CloseQuote);
              break;
		  case uint8_t(StyleContent::NoOpenQuote):
              data.SetKeyword(eStyleContentType_NoOpenQuote);
              break;
		  case uint8_t(StyleContent::NoCloseQuote):
              data.SetKeyword(eStyleContentType_NoCloseQuote);
              break;
            default:
              NS_ERROR("bad content value");
              break;
          }
          break;
        default:
          NS_ERROR("bad content type");
          break;
      }
      contentValueList = contentValueList->mNext;
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
    content->AllocateCounterIncrements(count);
    while (count--) {
      const nsStyleCounterData& data = parentContent->CounterIncrementAt(count);
      content->SetCounterIncrementAt(count, data.mCounter, data.mValue);
    }
    break;

  case eCSSUnit_PairList:
  case eCSSUnit_PairListDep: {
    const nsCSSValuePairList* ourIncrement =
      counterIncrementValue->GetPairListValue();
    MOZ_ASSERT(ourIncrement->mXValue.GetUnit() == eCSSUnit_Ident,
               "unexpected value unit");
    count = ListLength(ourIncrement);
    content->AllocateCounterIncrements(count);

    count = 0;
    for (const nsCSSValuePairList* p = ourIncrement; p; p = p->mNext, count++) {
      MOZ_ASSERT(p->mYValue.GetUnit() == eCSSUnit_Integer);
      p->mXValue.GetStringValue(buffer);
      content->SetCounterIncrementAt(count, buffer, p->mYValue.GetIntValue());
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
    content->AllocateCounterResets(count);
    while (0 < count--) {
      const nsStyleCounterData& data = parentContent->CounterResetAt(count);
      content->SetCounterResetAt(count, data.mCounter, data.mValue);
    }
    break;

  case eCSSUnit_PairList:
  case eCSSUnit_PairListDep: {
    const nsCSSValuePairList* ourReset =
      counterResetValue->GetPairListValue();
    MOZ_ASSERT(ourReset->mXValue.GetUnit() == eCSSUnit_Ident,
               "unexpected value unit");
    count = ListLength(ourReset);
    content->AllocateCounterResets(count);
    count = 0;
    for (const nsCSSValuePairList* p = ourReset; p; p = p->mNext, count++) {
      MOZ_ASSERT(p->mYValue.GetUnit() == eCSSUnit_Integer);
      p->mXValue.GetStringValue(buffer);
      content->SetCounterResetAt(count, buffer, p->mYValue.GetIntValue());
    }
    break;
  }

  default:
    MOZ_ASSERT(false, "unexpected value unit");
  }

  COMPUTE_END_RESET(Content, content)
}

const void*
nsRuleNode::ComputeXULData(void* aStartStruct,
                           const nsRuleData* aRuleData,
                           GeckoStyleContext* aContext,
                           nsRuleNode* aHighestNode,
                           const RuleDetail aRuleDetail,
                           const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(XUL, xul, parentXUL)

  // box-align: enum, inherit, initial
  SetValue(*aRuleData->ValueForBoxAlign(),
           xul->mBoxAlign, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentXUL->mBoxAlign,
           StyleBoxAlign::Stretch);

  // box-direction: enum, inherit, initial
  SetValue(*aRuleData->ValueForBoxDirection(),
           xul->mBoxDirection, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentXUL->mBoxDirection,
           StyleBoxDirection::Normal);

  // box-flex: factor, inherit
  SetFactor(*aRuleData->ValueForBoxFlex(),
            xul->mBoxFlex, conditions,
            parentXUL->mBoxFlex, 0.0f,
            SETFCT_UNSET_INITIAL);

  // box-orient: enum, inherit, initial
  SetValue(*aRuleData->ValueForBoxOrient(),
           xul->mBoxOrient, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentXUL->mBoxOrient,
           StyleBoxOrient::Horizontal);

  // box-pack: enum, inherit, initial
  SetValue(*aRuleData->ValueForBoxPack(),
           xul->mBoxPack, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentXUL->mBoxPack,
           StyleBoxPack::Start);

  // box-ordinal-group: integer, inherit, initial
  SetValue(*aRuleData->ValueForBoxOrdinalGroup(),
           xul->mBoxOrdinal, conditions,
           SETVAL_INTEGER | SETVAL_UNSET_INITIAL,
           parentXUL->mBoxOrdinal, 1);

  SetValue(*aRuleData->ValueForStackSizing(),
           xul->mStackSizing, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentXUL->mStackSizing,
           StyleStackSizing::StretchToFit);

  COMPUTE_END_RESET(XUL, xul)
}

const void*
nsRuleNode::ComputeColumnData(void* aStartStruct,
                              const nsRuleData* aRuleData,
                              GeckoStyleContext* aContext,
                              nsRuleNode* aHighestNode,
                              const RuleDetail aRuleDetail,
                              const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(Column, column, parent)

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
  Maybe<nscoord> coord =
    ComputeLineWidthValue<eUnsetInitial>(
      *aRuleData->ValueForColumnRuleWidth(),
      parent->GetComputedColumnRuleWidth(),
      nsPresContext::GetBorderWidthForKeyword(NS_STYLE_BORDER_WIDTH_MEDIUM),
      aContext, mPresContext, conditions);
  if (coord.isSome()) {
    column->SetColumnRuleWidth(*coord);
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
  SetComplexColor<eUnsetInitial>(*aRuleData->ValueForColumnRuleColor(),
                                 parent->mColumnRuleColor,
                                 StyleComplexColor::CurrentColor(),
                                 mPresContext,
                                 column->mColumnRuleColor, conditions);

  // column-fill: enum
  SetValue(*aRuleData->ValueForColumnFill(),
           column->mColumnFill, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parent->mColumnFill,
           NS_STYLE_COLUMN_FILL_BALANCE);

  // column-span: enum
  SetValue(*aRuleData->ValueForColumnSpan(),
           column->mColumnSpan, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parent->mColumnSpan,
           NS_STYLE_COLUMN_SPAN_NONE);

  COMPUTE_END_RESET(Column, column)
}

static void
SetSVGPaint(const nsCSSValue& aValue, const nsStyleSVGPaint& parentPaint,
            nsPresContext* aPresContext, GeckoStyleContext *aContext,
            nsStyleSVGPaint& aResult, nsStyleSVGPaintType aInitialPaintType,
            RuleNodeCacheConditions& aConditions)
{
  MOZ_ASSERT(aInitialPaintType == eStyleSVGPaintType_None ||
             aInitialPaintType == eStyleSVGPaintType_Color,
             "SetSVGPaint only supports initial values being either 'black' "
             "(represented by eStyleSVGPaintType_Color) or none (by "
             "eStyleSVGPaintType_None)");

  nscolor color;

  if (aValue.GetUnit() == eCSSUnit_Inherit ||
      aValue.GetUnit() == eCSSUnit_Unset) {
    aResult = parentPaint;
    aConditions.SetUncacheable();
  } else if (aValue.GetUnit() == eCSSUnit_None) {
    aResult.SetNone();
  } else if (aValue.GetUnit() == eCSSUnit_Initial) {
    if (aInitialPaintType == eStyleSVGPaintType_None) {
      aResult.SetNone();
    } else {
      aResult.SetColor(NS_RGB(0, 0, 0));
    }
  } else if (aValue.GetUnit() == eCSSUnit_URL) {
    aResult.SetPaintServer(aValue.GetURLStructValue());
  } else if (aValue.GetUnit() == eCSSUnit_Enumerated) {
    switch (aValue.GetIntValue()) {
    case NS_COLOR_CONTEXT_FILL:
      aResult.SetContextValue(eStyleSVGPaintType_ContextFill);
      break;
    case NS_COLOR_CONTEXT_STROKE:
      aResult.SetContextValue(eStyleSVGPaintType_ContextStroke);
      break;
    default:
      NS_NOTREACHED("unknown keyword as paint server value");
    }
  } else if (SetColor(aValue, NS_RGB(0, 0, 0), aPresContext, aContext,
                      color, aConditions)) {
    aResult.SetColor(color);
  } else if (aValue.GetUnit() == eCSSUnit_Pair) {
    const nsCSSValuePair& pair = aValue.GetPairValue();

    nsStyleSVGFallbackType fallbackType;
    nscolor fallback;
    if (pair.mYValue.GetUnit() == eCSSUnit_None) {
      fallbackType = eStyleSVGFallbackType_None;
      fallback = NS_RGBA(0, 0, 0, 0);
    } else {
      MOZ_ASSERT(pair.mYValue.GetUnit() != eCSSUnit_Inherit,
                 "cannot inherit fallback colour");
      fallbackType = eStyleSVGFallbackType_Color;
      SetColor(pair.mYValue, NS_RGB(0, 0, 0), aPresContext, aContext,
               fallback, aConditions);
    }

    if (pair.mXValue.GetUnit() == eCSSUnit_URL) {
      aResult.SetPaintServer(pair.mXValue.GetURLStructValue(),
                             fallbackType, fallback);
    } else if (pair.mXValue.GetUnit() == eCSSUnit_Enumerated) {

      switch (pair.mXValue.GetIntValue()) {
      case NS_COLOR_CONTEXT_FILL:
        aResult.SetContextValue(eStyleSVGPaintType_ContextFill,
                                fallbackType, fallback);
        break;
      case NS_COLOR_CONTEXT_STROKE:
        aResult.SetContextValue(eStyleSVGPaintType_ContextStroke,
                                fallbackType, fallback);
        break;
      default:
        NS_NOTREACHED("unknown keyword as paint server value");
      }

    } else {
      NS_NOTREACHED("malformed paint server value");
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
                           GeckoStyleContext* aContext,
                           nsRuleNode* aHighestNode,
                           const RuleDetail aRuleDetail,
                           const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_INHERITED(SVG, svg, parentSVG)

  // clip-rule: enum, inherit, initial
  SetValue(*aRuleData->ValueForClipRule(),
           svg->mClipRule, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentSVG->mClipRule,
           StyleFillRule::Nonzero);

  // color-interpolation: enum, inherit, initial
  SetValue(*aRuleData->ValueForColorInterpolation(),
           svg->mColorInterpolation, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentSVG->mColorInterpolation,
           NS_STYLE_COLOR_INTERPOLATION_SRGB);

  // color-interpolation-filters: enum, inherit, initial
  SetValue(*aRuleData->ValueForColorInterpolationFilters(),
           svg->mColorInterpolationFilters, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentSVG->mColorInterpolationFilters,
           NS_STYLE_COLOR_INTERPOLATION_LINEARRGB);

  // fill:
  SetSVGPaint(*aRuleData->ValueForFill(),
              parentSVG->mFill, mPresContext, aContext,
              svg->mFill, eStyleSVGPaintType_Color, conditions);

  // fill-opacity: factor, inherit, initial,
  // context-fill-opacity, context-stroke-opacity
  nsStyleSVGOpacitySource contextFillOpacity = svg->FillOpacitySource();
  SetSVGOpacity(*aRuleData->ValueForFillOpacity(),
                svg->mFillOpacity, contextFillOpacity, conditions,
                parentSVG->mFillOpacity, parentSVG->FillOpacitySource());
  svg->SetFillOpacitySource(contextFillOpacity);

  // fill-rule: enum, inherit, initial
  SetValue(*aRuleData->ValueForFillRule(),
           svg->mFillRule, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentSVG->mFillRule,
           StyleFillRule::Nonzero);

  // marker-end: url, none, inherit
  const nsCSSValue* markerEndValue = aRuleData->ValueForMarkerEnd();
  if (eCSSUnit_URL == markerEndValue->GetUnit()) {
    svg->mMarkerEnd = markerEndValue->GetURLStructValue();
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
    svg->mMarkerMid = markerMidValue->GetURLStructValue();
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
    svg->mMarkerStart = markerStartValue->GetURLStructValue();
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
  SetValue(*aRuleData->ValueForShapeRendering(),
           svg->mShapeRendering, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentSVG->mShapeRendering,
           NS_STYLE_SHAPE_RENDERING_AUTO);

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
    svg->SetStrokeDasharrayFromObject(parentSVG->StrokeDasharrayFromObject());
    svg->mStrokeDasharray = parentSVG->mStrokeDasharray;
    break;

  case eCSSUnit_Enumerated:
    MOZ_ASSERT(strokeDasharrayValue->GetIntValue() ==
                     NS_STYLE_STROKE_PROP_CONTEXT_VALUE,
               "Unknown keyword for stroke-dasharray");
    svg->SetStrokeDasharrayFromObject(true);
    svg->mStrokeDasharray.Clear();
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_None:
    svg->SetStrokeDasharrayFromObject(false);
    svg->mStrokeDasharray.Clear();
    break;

  case eCSSUnit_List:
  case eCSSUnit_ListDep: {
    svg->SetStrokeDasharrayFromObject(false);
    svg->mStrokeDasharray.Clear();

    // count number of values
    const nsCSSValueList *value = strokeDasharrayValue->GetListValue();
    uint32_t strokeDasharrayLength = ListLength(value);

    MOZ_ASSERT(strokeDasharrayLength != 0, "no dasharray items");

    svg->mStrokeDasharray.SetLength(strokeDasharrayLength);

    uint32_t i = 0;
    while (nullptr != value) {
      SetCoord(value->mValue,
               svg->mStrokeDasharray[i++], nsStyleCoord(),
               SETCOORD_LP | SETCOORD_FACTOR,
               aContext, mPresContext, conditions);
      value = value->mNext;
    }
    break;
  }

  default:
    MOZ_ASSERT(false, "unrecognized dasharray unit");
  }

  // stroke-dashoffset: <dashoffset>, inherit
  const nsCSSValue *strokeDashoffsetValue =
    aRuleData->ValueForStrokeDashoffset();
  svg->SetStrokeDashoffsetFromObject(
    strokeDashoffsetValue->GetUnit() == eCSSUnit_Enumerated &&
    strokeDashoffsetValue->GetIntValue() == NS_STYLE_STROKE_PROP_CONTEXT_VALUE);
  if (svg->StrokeDashoffsetFromObject()) {
    svg->mStrokeDashoffset.SetCoordValue(0);
  } else {
    SetCoord(*aRuleData->ValueForStrokeDashoffset(),
             svg->mStrokeDashoffset, parentSVG->mStrokeDashoffset,
             SETCOORD_LPH | SETCOORD_FACTOR | SETCOORD_INITIAL_ZERO |
               SETCOORD_UNSET_INHERIT,
             aContext, mPresContext, conditions);
  }

  // stroke-linecap: enum, inherit, initial
  SetValue(*aRuleData->ValueForStrokeLinecap(),
           svg->mStrokeLinecap, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentSVG->mStrokeLinecap,
           NS_STYLE_STROKE_LINECAP_BUTT);

  // stroke-linejoin: enum, inherit, initial
  SetValue(*aRuleData->ValueForStrokeLinejoin(),
           svg->mStrokeLinejoin, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentSVG->mStrokeLinejoin,
           NS_STYLE_STROKE_LINEJOIN_MITER);

  // stroke-miterlimit: <miterlimit>, inherit
  SetFactor(*aRuleData->ValueForStrokeMiterlimit(),
            svg->mStrokeMiterlimit,
            conditions,
            parentSVG->mStrokeMiterlimit, 4.0f,
            SETFCT_UNSET_INHERIT);

  // stroke-opacity:
  nsStyleSVGOpacitySource contextStrokeOpacity = svg->StrokeOpacitySource();
  SetSVGOpacity(*aRuleData->ValueForStrokeOpacity(),
                svg->mStrokeOpacity, contextStrokeOpacity, conditions,
                parentSVG->mStrokeOpacity, parentSVG->StrokeOpacitySource());
  svg->SetStrokeOpacitySource(contextStrokeOpacity);

  // stroke-width:
  const nsCSSValue* strokeWidthValue = aRuleData->ValueForStrokeWidth();
  switch (strokeWidthValue->GetUnit()) {
  case eCSSUnit_Enumerated:
    MOZ_ASSERT(strokeWidthValue->GetIntValue() ==
                 NS_STYLE_STROKE_PROP_CONTEXT_VALUE,
               "Unrecognized keyword for stroke-width");
    svg->SetStrokeWidthFromObject(true);
    svg->mStrokeWidth.SetCoordValue(nsPresContext::CSSPixelsToAppUnits(1));
    break;

  case eCSSUnit_Initial:
    svg->SetStrokeWidthFromObject(false);
    svg->mStrokeWidth.SetCoordValue(nsPresContext::CSSPixelsToAppUnits(1));
    break;

  default:
    svg->SetStrokeWidthFromObject(false);
    SetCoord(*strokeWidthValue,
             svg->mStrokeWidth, parentSVG->mStrokeWidth,
             SETCOORD_LPH | SETCOORD_FACTOR | SETCOORD_UNSET_INHERIT,
             aContext, mPresContext, conditions);
  }

  // text-anchor: enum, inherit, initial
  SetValue(*aRuleData->ValueForTextAnchor(),
           svg->mTextAnchor, conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INHERIT,
           parentSVG->mTextAnchor,
           NS_STYLE_TEXT_ANCHOR_START);

  // -moz-context-properties:
  const nsCSSValue* contextPropsValue = aRuleData->ValueForContextProperties();
  switch (contextPropsValue->GetUnit()) {
  case eCSSUnit_Null:
    break;

  case eCSSUnit_List:
  case eCSSUnit_ListDep: {
    svg->mContextProps.Clear();
    svg->mContextPropsBits = 0;
    for (const nsCSSValueList* item = contextPropsValue->GetListValue();
         item; item = item->mNext)
    {
      nsAtom* atom = item->mValue.GetAtomValue();
      svg->mContextProps.AppendElement(atom);
      if (atom == nsGkAtoms::fill) {
        svg->mContextPropsBits |= NS_STYLE_CONTEXT_PROPERTY_FILL;
      } else if (atom == nsGkAtoms::stroke) {
        svg->mContextPropsBits |= NS_STYLE_CONTEXT_PROPERTY_STROKE;
      } else if (atom == nsGkAtoms::fill_opacity) {
        svg->mContextPropsBits |= NS_STYLE_CONTEXT_PROPERTY_FILL_OPACITY;
      } else if (atom == nsGkAtoms::stroke_opacity) {
        svg->mContextPropsBits |= NS_STYLE_CONTEXT_PROPERTY_STROKE_OPACITY;
      }
    }
    break;
  }

  case eCSSUnit_Inherit:
  case eCSSUnit_Unset:
    svg->mContextProps = parentSVG->mContextProps;
    svg->mContextPropsBits = parentSVG->mContextPropsBits;
    conditions.SetUncacheable();
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_None:
    svg->mContextProps.Clear();
    svg->mContextPropsBits = 0;
    break;

  default:
    MOZ_ASSERT(false, "unrecognized -moz-context-properties value");
  }

  COMPUTE_END_INHERITED(SVG, svg)
}

static UniquePtr<StyleBasicShape>
GetStyleBasicShapeFromCSSValue(const nsCSSValue& aValue,
                               GeckoStyleContext* aStyleContext,
                               nsPresContext* aPresContext,
                               RuleNodeCacheConditions& aConditions)
{
  UniquePtr<StyleBasicShape> basicShape;

  nsCSSValue::Array* shapeFunction = aValue.GetArrayValue();
  nsCSSKeyword functionName =
    (nsCSSKeyword)shapeFunction->Item(0).GetIntValue();

  if (functionName == eCSSKeyword_polygon) {
    MOZ_ASSERT(!basicShape, "did not expect value");
    basicShape = MakeUnique<StyleBasicShape>(StyleBasicShapeType::Polygon);
    MOZ_ASSERT(shapeFunction->Count() > 1,
               "polygon has wrong number of arguments");
    size_t j = 1;
    if (shapeFunction->Item(j).GetUnit() == eCSSUnit_Enumerated) {
      StyleFillRule rule;
      SetEnumValueHelper::SetEnumeratedValue(rule, shapeFunction->Item(j));
      basicShape->SetFillRule(rule);
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
    StyleBasicShapeType type = functionName == eCSSKeyword_circle ?
      StyleBasicShapeType::Circle :
      StyleBasicShapeType::Ellipse;
    MOZ_ASSERT(!basicShape, "did not expect value");
    basicShape = MakeUnique<StyleBasicShape>(type);
    const int32_t mask = SETCOORD_PERCENT | SETCOORD_LENGTH |
      SETCOORD_STORE_CALC | SETCOORD_ENUMERATED;
    size_t count = type == StyleBasicShapeType::Circle ? 2 : 3;
    MOZ_ASSERT(shapeFunction->Count() == count + 1,
               "unexpected arguments count");
    MOZ_ASSERT(type == StyleBasicShapeType::Circle ||
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
        radius.SetEnumValue(StyleShapeRadius::ClosestSide);
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
    basicShape = MakeUnique<StyleBasicShape>(StyleBasicShapeType::Inset);
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
        int cx = FullToHalfCorner(corner, false);
        int cy = FullToHalfCorner(corner, true);
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
  }

  return basicShape;
}

static void
SetStyleShapeSourceToCSSValue(
  StyleShapeSource* aShapeSource,
  const nsCSSValue* aValue,
  GeckoStyleContext* aStyleContext,
  nsPresContext* aPresContext,
  RuleNodeCacheConditions& aConditions)
{
  MOZ_ASSERT(aValue->GetUnit() == eCSSUnit_Array,
             "expected a basic shape or reference box");

  const nsCSSValue::Array* array = aValue->GetArrayValue();
  MOZ_ASSERT(array->Count() == 1 || array->Count() == 2,
             "Expect one or both of a shape function and a reference box");

  StyleGeometryBox referenceBox = StyleGeometryBox::NoBox;
  UniquePtr<StyleBasicShape> basicShape;

  for (size_t i = 0; i < array->Count(); ++i) {
    const nsCSSValue& item = array->Item(i);
    if (item.GetUnit() == eCSSUnit_Enumerated) {
      referenceBox = static_cast<StyleGeometryBox>(item.GetIntValue());
    } else if (item.GetUnit() == eCSSUnit_Function) {
      basicShape = GetStyleBasicShapeFromCSSValue(item, aStyleContext,
                                                  aPresContext, aConditions);
    } else {
      MOZ_ASSERT_UNREACHABLE("Unexpected unit!");
      return;
    }
  }

  if (basicShape) {
    aShapeSource->SetBasicShape(Move(basicShape), referenceBox);
  } else {
    aShapeSource->SetReferenceBox(referenceBox);
  }
}

// Returns true if the nsStyleFilter was successfully set using the nsCSSValue.
static bool
SetStyleFilterToCSSValue(nsStyleFilter* aStyleFilter,
                         const nsCSSValue& aValue,
                         GeckoStyleContext* aStyleContext,
                         nsPresContext* aPresContext,
                         RuleNodeCacheConditions& aConditions)
{
  nsCSSUnit unit = aValue.GetUnit();
  if (unit == eCSSUnit_URL) {
    return aStyleFilter->SetURL(aValue.GetURLStructValue());
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
    RefPtr<nsCSSShadowArray> shadowArray = GetShadowData(
      filterFunction->Item(1).GetListValue(),
      aStyleContext,
      false,
      aPresContext,
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
                                GeckoStyleContext* aContext,
                                nsRuleNode* aHighestNode,
                                const RuleDetail aRuleDetail,
                                const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(SVGReset, svgReset, parentSVGReset)

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
      svgReset->mClipPath = StyleShapeSource();
      break;
    case eCSSUnit_Inherit:
      conditions.SetUncacheable();
      svgReset->mClipPath = parentSVGReset->mClipPath;
      break;
    case eCSSUnit_URL: {
      svgReset->mClipPath = StyleShapeSource();
      svgReset->mClipPath.SetURL(clipPathValue->GetURLStructValue());
      break;
    }
    case eCSSUnit_Array: {
      svgReset->mClipPath = StyleShapeSource();
      SetStyleShapeSourceToCSSValue(&svgReset->mClipPath, clipPathValue, aContext,
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
  SetValue(*aRuleData->ValueForDominantBaseline(),
           svgReset->mDominantBaseline,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentSVGReset->mDominantBaseline,
           NS_STYLE_DOMINANT_BASELINE_AUTO);

  // vector-effect: enum, inherit, initial
  SetValue(*aRuleData->ValueForVectorEffect(),
           svgReset->mVectorEffect,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentSVGReset->mVectorEffect,
           NS_STYLE_VECTOR_EFFECT_NONE);

  // mask-type: enum, inherit, initial
  SetValue(*aRuleData->ValueForMaskType(),
           svgReset->mMaskType,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentSVGReset->mMaskType,
           NS_STYLE_MASK_TYPE_LUMINANCE);

  uint32_t maxItemCount = 1;
  bool rebuild = false;

  // mask-image: none | <url> | <image-list> | <element-reference>  | <gradient>
  nsStyleImage initialImage;
  SetImageLayerList(aContext, *aRuleData->ValueForMaskImage(),
                    svgReset->mMask.mLayers,
                    parentSVGReset->mMask.mLayers,
                    &nsStyleImageLayers::Layer::mImage,
                    initialImage, parentSVGReset->mMask.mImageCount,
                    svgReset->mMask.mImageCount,
                    maxItemCount, rebuild, conditions);

  // mask-repeat: enum, inherit, initial [pair list]
  nsStyleImageLayers::Repeat initialRepeat;
  initialRepeat.SetInitialValues();
  SetImageLayerPairList(aContext, *aRuleData->ValueForMaskRepeat(),
                        svgReset->mMask.mLayers,
                        parentSVGReset->mMask.mLayers,
                        &nsStyleImageLayers::Layer::mRepeat,
                        initialRepeat, parentSVGReset->mMask.mRepeatCount,
                        svgReset->mMask.mRepeatCount, maxItemCount, rebuild,
                        conditions);

  // mask-clip: enum, inherit, initial [list]
  SetImageLayerList(aContext, *aRuleData->ValueForMaskClip(),
                    svgReset->mMask.mLayers,
                    parentSVGReset->mMask.mLayers,
                    &nsStyleImageLayers::Layer::mClip,
                    StyleGeometryBox::BorderBox,
                    parentSVGReset->mMask.mClipCount,
                    svgReset->mMask.mClipCount, maxItemCount, rebuild,
                    conditions);

  // mask-origin: enum, inherit, initial [list]
  SetImageLayerList(aContext, *aRuleData->ValueForMaskOrigin(),
                    svgReset->mMask.mLayers,
                    parentSVGReset->mMask.mLayers,
                    &nsStyleImageLayers::Layer::mOrigin,
                    StyleGeometryBox::BorderBox,
                    parentSVGReset->mMask.mOriginCount,
                    svgReset->mMask.mOriginCount, maxItemCount, rebuild,
                    conditions);

  // mask-position-x/y: enum, length, percent (flags), inherit [list]
  Position::Coord initialPositionCoord;
  initialPositionCoord.mPercent = 0.0f;
  initialPositionCoord.mLength = 0;
  initialPositionCoord.mHasPercent = true;

  SetImageLayerPositionCoordList(
                    aContext, *aRuleData->ValueForMaskPositionX(),
                    svgReset->mMask.mLayers,
                    parentSVGReset->mMask.mLayers,
                    &Position::mXPosition,
                    initialPositionCoord, parentSVGReset->mMask.mPositionXCount,
                    svgReset->mMask.mPositionXCount, maxItemCount, rebuild,
                    conditions);
  SetImageLayerPositionCoordList(
                    aContext, *aRuleData->ValueForMaskPositionY(),
                    svgReset->mMask.mLayers,
                    parentSVGReset->mMask.mLayers,
                    &Position::mYPosition,
                    initialPositionCoord, parentSVGReset->mMask.mPositionYCount,
                    svgReset->mMask.mPositionYCount, maxItemCount, rebuild,
                    conditions);

  // mask-size: enum, length, auto, inherit, initial [pair list]
  nsStyleImageLayers::Size initialSize;
  initialSize.SetInitialValues();
  SetImageLayerPairList(aContext, *aRuleData->ValueForMaskSize(),
                        svgReset->mMask.mLayers,
                        parentSVGReset->mMask.mLayers,
                        &nsStyleImageLayers::Layer::mSize,
                        initialSize, parentSVGReset->mMask.mSizeCount,
                        svgReset->mMask.mSizeCount, maxItemCount, rebuild,
                        conditions);

  // mask-mode: enum, inherit, initial [list]
  SetImageLayerList(aContext, *aRuleData->ValueForMaskMode(),
                    svgReset->mMask.mLayers,
                    parentSVGReset->mMask.mLayers,
                    &nsStyleImageLayers::Layer::mMaskMode,
                    uint8_t(NS_STYLE_MASK_MODE_MATCH_SOURCE),
                    parentSVGReset->mMask.mMaskModeCount,
                    svgReset->mMask.mMaskModeCount, maxItemCount, rebuild, conditions);

  // mask-composite: enum, inherit, initial [list]
  SetImageLayerList(aContext, *aRuleData->ValueForMaskComposite(),
                    svgReset->mMask.mLayers,
                    parentSVGReset->mMask.mLayers,
                    &nsStyleImageLayers::Layer::mComposite,
                    uint8_t(NS_STYLE_MASK_COMPOSITE_ADD),
                    parentSVGReset->mMask.mCompositeCount,
                    svgReset->mMask.mCompositeCount, maxItemCount, rebuild, conditions);

  if (rebuild) {
    svgReset->mMask.FillAllLayers(maxItemCount);
  }

  COMPUTE_END_RESET(SVGReset, svgReset)
}

const void*
nsRuleNode::ComputeVariablesData(void* aStartStruct,
                                 const nsRuleData* aRuleData,
                                 GeckoStyleContext* aContext,
                                 nsRuleNode* aHighestNode,
                                 const RuleDetail aRuleDetail,
                                 const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_INHERITED(Variables, variables, parentVariables)

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
nsRuleNode::ComputeEffectsData(void* aStartStruct,
                               const nsRuleData* aRuleData,
                               GeckoStyleContext* aContext,
                               nsRuleNode* aHighestNode,
                               const RuleDetail aRuleDetail,
                               const RuleNodeCacheConditions aConditions)
{
  COMPUTE_START_RESET(Effects, effects, parentEffects)

  // filter: url, none, inherit
  const nsCSSValue* filterValue = aRuleData->ValueForFilter();
  switch (filterValue->GetUnit()) {
    case eCSSUnit_Null:
      break;
    case eCSSUnit_None:
    case eCSSUnit_Initial:
    case eCSSUnit_Unset:
      effects->mFilters.Clear();
      break;
    case eCSSUnit_Inherit:
      conditions.SetUncacheable();
      effects->mFilters = parentEffects->mFilters;
      break;
    case eCSSUnit_List:
    case eCSSUnit_ListDep: {
      effects->mFilters.Clear();
      const nsCSSValueList* cur = filterValue->GetListValue();
      while (cur) {
        nsStyleFilter styleFilter;
        if (!SetStyleFilterToCSSValue(&styleFilter, cur->mValue, aContext,
                                      mPresContext, conditions)) {
          effects->mFilters.Clear();
          break;
        }
        MOZ_ASSERT(styleFilter.GetType() != NS_STYLE_FILTER_NONE,
                   "filter should be set");
        effects->mFilters.AppendElement(styleFilter);
        cur = cur->mNext;
      }
      break;
    }
    default:
      NS_NOTREACHED("unexpected unit");
  }

  // box-shadow: none, list, inherit, initial
  const nsCSSValue* boxShadowValue = aRuleData->ValueForBoxShadow();
  switch (boxShadowValue->GetUnit()) {
  case eCSSUnit_Null:
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
  case eCSSUnit_None:
    effects->mBoxShadow = nullptr;
    break;

  case eCSSUnit_Inherit:
    effects->mBoxShadow = parentEffects->mBoxShadow;
    conditions.SetUncacheable();
    break;

  case eCSSUnit_List:
  case eCSSUnit_ListDep:
    effects->mBoxShadow = GetShadowData(boxShadowValue->GetListValue(),
                                        aContext, true, mPresContext, conditions);
    break;

  default:
    MOZ_ASSERT(false, "unrecognized shadow unit");
  }

  // clip property: length, auto, inherit
  const nsCSSValue* clipValue = aRuleData->ValueForClip();
  switch (clipValue->GetUnit()) {
  case eCSSUnit_Inherit:
    conditions.SetUncacheable();
    effects->mClipFlags = parentEffects->mClipFlags;
    effects->mClip = parentEffects->mClip;
    break;

  case eCSSUnit_Initial:
  case eCSSUnit_Unset:
  case eCSSUnit_Auto:
    effects->mClipFlags = NS_STYLE_CLIP_AUTO;
    effects->mClip.SetRect(0,0,0,0);
    break;

  case eCSSUnit_Null:
    break;

  case eCSSUnit_Rect: {
    const nsCSSRect& clipRect = clipValue->GetRectValue();

    effects->mClipFlags = NS_STYLE_CLIP_RECT;

    if (clipRect.mTop.GetUnit() == eCSSUnit_Auto) {
      effects->mClip.y = 0;
      effects->mClipFlags |= NS_STYLE_CLIP_TOP_AUTO;
    }
    else if (clipRect.mTop.IsLengthUnit()) {
      effects->mClip.y = CalcLength(clipRect.mTop, aContext,
                                    mPresContext, conditions);
    }

    if (clipRect.mBottom.GetUnit() == eCSSUnit_Auto) {
      // Setting to NS_MAXSIZE for the 'auto' case ensures that
      // the clip rect is nonempty. It is important that mClip be
      // nonempty if the actual clip rect could be nonempty.
      effects->mClip.height = NS_MAXSIZE;
      effects->mClipFlags |= NS_STYLE_CLIP_BOTTOM_AUTO;
    }
    else if (clipRect.mBottom.IsLengthUnit()) {
      effects->mClip.height = CalcLength(clipRect.mBottom, aContext,
                                         mPresContext, conditions) -
                              effects->mClip.y;
    }

    if (clipRect.mLeft.GetUnit() == eCSSUnit_Auto) {
      effects->mClip.x = 0;
      effects->mClipFlags |= NS_STYLE_CLIP_LEFT_AUTO;
    }
    else if (clipRect.mLeft.IsLengthUnit()) {
      effects->mClip.x = CalcLength(clipRect.mLeft, aContext,
                                    mPresContext, conditions);
    }

    if (clipRect.mRight.GetUnit() == eCSSUnit_Auto) {
      // Setting to NS_MAXSIZE for the 'auto' case ensures that
      // the clip rect is nonempty. It is important that mClip be
      // nonempty if the actual clip rect could be nonempty.
      effects->mClip.width = NS_MAXSIZE;
      effects->mClipFlags |= NS_STYLE_CLIP_RIGHT_AUTO;
    }
    else if (clipRect.mRight.IsLengthUnit()) {
      effects->mClip.width = CalcLength(clipRect.mRight, aContext,
                                        mPresContext, conditions) -
                             effects->mClip.x;
    }
    break;
  }

  default:
    MOZ_ASSERT(false, "unrecognized clip unit");
  }

  // opacity: factor, inherit, initial
  SetFactor(*aRuleData->ValueForOpacity(), effects->mOpacity, conditions,
            parentEffects->mOpacity, 1.0f,
            SETFCT_OPACITY | SETFCT_UNSET_INITIAL);

  // mix-blend-mode: enum, inherit, initial
  SetValue(*aRuleData->ValueForMixBlendMode(), effects->mMixBlendMode,
           conditions,
           SETVAL_ENUMERATED | SETVAL_UNSET_INITIAL,
           parentEffects->mMixBlendMode, NS_STYLE_BLEND_NORMAL);

  COMPUTE_END_RESET(Effects, effects)
}

const void*
nsRuleNode::GetStyleData(nsStyleStructID aSID,
                         GeckoStyleContext* aContext,
                         bool aComputeData)
{
  NS_ASSERTION(IsUsedDirectly(),
               "if we ever call this on rule nodes that aren't used "
               "directly, we should adjust handling of mDependentBits "
               "in some way.");
  MOZ_ASSERT(!aContext->GetCachedStyleData(aSID),
             "style context should not have cached data for struct");

  const void *data;

  // Never use cached data for animated style inside a pseudo-element;
  // see comment on cacheability in AnimValuesStyleRule::MapRuleInfoInto.
  if (!(HasAnimationData() && ParentHasPseudoElementData(aContext))) {
    data = mStyleData.GetStyleData(aSID, aContext, aComputeData);
    if (MOZ_LIKELY(data != nullptr)) {
      // For inherited structs, mark the struct (which will be set on
      // the context by our caller) as not being owned by the context.
      if (!nsCachedStyleData::IsReset(aSID)) {
        aContext->AddStyleBit(nsCachedStyleData::GetBitForSID(aSID));
      } else if (HasAnimationData()) {
        // If we have animation data, the struct should be cached on the style
        // context so that we can peek the struct.
        // See comment in AnimValuesStyleRule::MapRuleInfoInto.
        StoreStyleOnContext(aContext, aSID, const_cast<void*>(data));
      }

      return data; // We have a fully specified struct. Just return it.
    }
  }

  if (MOZ_UNLIKELY(!aComputeData))
    return nullptr;

  // Nothing is cached.  We'll have to delve further and examine our rules.
  data = WalkRuleTree(aSID, aContext);

  MOZ_ASSERT(data, "should have aborted on out-of-memory");
  return data;
}

void
nsRuleNode::GetDiscretelyAnimatedCSSValue(nsCSSPropertyID aProperty,
                                          nsCSSValue* aValue)
{
  for (nsRuleNode* node = this; node; node = node->GetParent()) {
    nsIStyleRule* rule = node->GetRule();
    if (!rule) {
      continue;
    }
    if (rule->GetDiscretelyAnimatedCSSValue(aProperty, aValue)) {
      return;
    }
  }
}

/* static */ bool
nsRuleNode::HasAuthorSpecifiedRules(GeckoStyleContext* aStyleContext,
                                    uint32_t ruleTypeMask,
                                    bool aAuthorColorsAllowed)
{
  RefPtr<GeckoStyleContext> styleContext = aStyleContext;

  uint32_t inheritBits = 0;
  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_BACKGROUND)
    inheritBits |= NS_STYLE_INHERIT_BIT(Background);

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_BORDER)
    inheritBits |= NS_STYLE_INHERIT_BIT(Border);

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_PADDING)
    inheritBits |= NS_STYLE_INHERIT_BIT(Padding);

  // properties in the SIDS, whether or not we care about them
  size_t nprops = 0,
         backgroundOffset, borderOffset, paddingOffset;

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

  void* dataStorage = alloca(nprops * sizeof(nsCSSValue));
  AutoCSSValueArray dataArray(dataStorage, nprops);

  /* We're relying on the use of |styleContext| not mutating it! */
  nsRuleData ruleData(inheritBits, dataArray.get(),
                      styleContext->PresContext(), styleContext);

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_BACKGROUND) {
    ruleData.mValueOffsets[eStyleStruct_Background] = backgroundOffset;
  }

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_BORDER) {
    ruleData.mValueOffsets[eStyleStruct_Border] = borderOffset;
  }

  if (ruleTypeMask & NS_AUTHOR_SPECIFIED_PADDING) {
    ruleData.mValueOffsets[eStyleStruct_Padding] = paddingOffset;
  }

  static const nsCSSPropertyID backgroundValues[] = {
    eCSSProperty_background_color,
    eCSSProperty_background_image,
  };

  static const nsCSSPropertyID borderValues[] = {
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

  static const nsCSSPropertyID paddingValues[] = {
    eCSSProperty_padding_top,
    eCSSProperty_padding_right,
    eCSSProperty_padding_bottom,
    eCSSProperty_padding_left,
  };

  // Number of properties we care about
  size_t nValues = 0;

  nsCSSValue* values[MOZ_ARRAY_LENGTH(backgroundValues) +
                     MOZ_ARRAY_LENGTH(borderValues) +
                     MOZ_ARRAY_LENGTH(paddingValues)];

  nsCSSPropertyID properties[MOZ_ARRAY_LENGTH(backgroundValues) +
                           MOZ_ARRAY_LENGTH(borderValues) +
                           MOZ_ARRAY_LENGTH(paddingValues)];

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

  GeckoStyleContext* styleContextRef = styleContext;

  // We need to be careful not to count styles covered up by user-important or
  // UA-important declarations.  But we do want to catch explicit inherit
  // styling in those and check our parent style context to see whether we have
  // user styling for those properties.  Note that we don't care here about
  // inheritance due to lack of a specified value, since all the properties we
  // care about are reset properties.
  bool haveExplicitUAInherit;
  do {
    haveExplicitUAInherit = false;
    for (nsRuleNode* ruleNode = styleContextRef->RuleNode(); ruleNode;
         ruleNode = ruleNode->GetParent()) {
      nsIStyleRule *rule = ruleNode->GetRule();
      if (rule) {
        ruleData.mLevel = ruleNode->GetLevel();
        ruleData.mIsImportantRule = ruleNode->IsImportantRule();

        rule->MapRuleInfoInto(&ruleData);

        if (ruleData.mLevel == SheetType::Agent ||
            ruleData.mLevel == SheetType::User) {
          // This is a rule whose effect we want to ignore, so if any of
          // the properties we care about were set, set them to the dummy
          // value that they'll never otherwise get.
          for (uint32_t i = 0; i < nValues; ++i) {
            nsCSSUnit unit = values[i]->GetUnit();
            if (unit != eCSSUnit_Null &&
                unit != eCSSUnit_Dummy &&
                unit != eCSSUnit_DummyInherit) {
              if (unit == eCSSUnit_Inherit) {
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
      styleContextRef = styleContextRef->GetParent();
    }
  } while (haveExplicitUAInherit && styleContext);

  return false;
}

/* static */ void
nsRuleNode::ComputePropertiesOverridingAnimation(
                              const nsTArray<nsCSSPropertyID>& aProperties,
                              GeckoStyleContext* aStyleContext,
                              nsCSSPropertyIDSet& aPropertiesOverridden)
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
    nsCSSPropertyID prop = aProperties[propIdx];
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
      if (ruleData.mLevel == SheetType::Transition) {
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
    nsCSSPropertyID prop = aProperties[propIdx];
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
nsRuleNode::ParentHasPseudoElementData(GeckoStyleContext* aContext)
{
  GeckoStyleContext* parent = aContext->GetParent();
  return parent && parent->HasPseudoElementData();
}

/* static */ void
nsRuleNode::StoreStyleOnContext(GeckoStyleContext* aContext,
                                nsStyleStructID aSID,
                                void* aStruct)
{
  aContext->AddStyleBit(nsCachedStyleData::GetBitForSID(aSID));
  aContext->SetStyle(aSID, aStruct);
}

#ifdef DEBUG
bool
nsRuleNode::ContextHasCachedData(GeckoStyleContext* aContext,
                                 nsStyleStructID aSID)
{
  return !!aContext->GetCachedStyleData(aSID);
}
#endif

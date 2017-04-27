/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object returned from element.getComputedStyle() */

#include "nsComputedDOMStyle.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Preferences.h"

#include "nsError.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsIFrame.h"
#include "nsIFrameInlines.h"
#include "nsStyleContext.h"
#include "nsIScrollableFrame.h"
#include "nsContentUtils.h"
#include "nsIContent.h"

#include "nsDOMCSSRect.h"
#include "nsDOMCSSRGBColor.h"
#include "nsDOMCSSValueList.h"
#include "nsFlexContainerFrame.h"
#include "nsGridContainerFrame.h"
#include "nsGkAtoms.h"
#include "mozilla/ReflowInput.h"
#include "nsStyleUtil.h"
#include "nsStyleStructInlines.h"
#include "nsROCSSPrimitiveValue.h"

#include "nsPresContext.h"
#include "nsIDocument.h"

#include "nsCSSPseudoElements.h"
#include "mozilla/StyleSetHandle.h"
#include "mozilla/StyleSetHandleInlines.h"
#include "mozilla/GeckoRestyleManager.h"
#include "mozilla/RestyleManagerInlines.h"
#include "imgIRequest.h"
#include "nsLayoutUtils.h"
#include "nsCSSKeywords.h"
#include "nsStyleCoord.h"
#include "nsDisplayList.h"
#include "nsDOMCSSDeclaration.h"
#include "nsStyleTransformMatrix.h"
#include "mozilla/dom/Element.h"
#include "prtime.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/AppUnits.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;

#if defined(DEBUG_bzbarsky) || defined(DEBUG_caillon)
#define DEBUG_ComputedDOMStyle
#endif

/*
 * This is the implementation of the readonly CSSStyleDeclaration that is
 * returned by the getComputedStyle() function.
 */

already_AddRefed<nsComputedDOMStyle>
NS_NewComputedDOMStyle(dom::Element* aElement, const nsAString& aPseudoElt,
                       nsIPresShell* aPresShell,
                       nsComputedDOMStyle::AnimationFlag aFlag)
{
  RefPtr<nsComputedDOMStyle> computedStyle;
  computedStyle = new nsComputedDOMStyle(aElement, aPseudoElt,
                                         aPresShell, aFlag);
  return computedStyle.forget();
}

static nsDOMCSSValueList*
GetROCSSValueList(bool aCommaDelimited)
{
  return new nsDOMCSSValueList(aCommaDelimited, true);
}

template<typename T>
already_AddRefed<CSSValue>
GetBackgroundList(T nsStyleImageLayers::Layer::* aMember,
                  uint32_t nsStyleImageLayers::* aCount,
                  const nsStyleImageLayers& aLayers,
                  const nsCSSProps::KTableEntry aTable[])
{
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  for (uint32_t i = 0, i_end = aLayers.*aCount; i < i_end; ++i) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    val->SetIdent(nsCSSProps::ValueToKeywordEnum(aLayers.mLayers[i].*aMember, aTable));
    valueList->AppendCSSValue(val.forget());
  }

  return valueList.forget();
}

/**
 * An object that represents the ordered set of properties that are exposed on
 * an nsComputedDOMStyle object and how their computed values can be obtained.
 */
struct nsComputedStyleMap
{
  friend class nsComputedDOMStyle;

  struct Entry
  {
    // Create a pointer-to-member-function type.
    typedef already_AddRefed<CSSValue> (nsComputedDOMStyle::*ComputeMethod)();

    nsCSSPropertyID mProperty;
    ComputeMethod mGetter;

    bool IsLayoutFlushNeeded() const
    {
      return nsCSSProps::PropHasFlags(mProperty,
                                      CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH);
    }

    bool IsEnabled() const
    {
      return nsCSSProps::IsEnabled(mProperty, CSSEnabledState::eForAllContent);
    }
  };

  // We define this enum just to count the total number of properties that can
  // be exposed on an nsComputedDOMStyle, including properties that may be
  // disabled.
  enum {
#define COMPUTED_STYLE_PROP(prop_, method_) \
    eComputedStyleProperty_##prop_,
#include "nsComputedDOMStylePropertyList.h"
#undef COMPUTED_STYLE_PROP
    eComputedStyleProperty_COUNT
  };

  /**
   * Returns the number of properties that should be exposed on an
   * nsComputedDOMStyle, ecxluding any disabled properties.
   */
  uint32_t Length()
  {
    Update();
    return mExposedPropertyCount;
  }

  /**
   * Returns the property at the given index in the list of properties
   * that should be exposed on an nsComputedDOMStyle, excluding any
   * disabled properties.
   */
  nsCSSPropertyID PropertyAt(uint32_t aIndex)
  {
    Update();
    return kEntries[EntryIndex(aIndex)].mProperty;
  }

  /**
   * Searches for and returns the computed style map entry for the given
   * property, or nullptr if the property is not exposed on nsComputedDOMStyle
   * or is currently disabled.
   */
  const Entry* FindEntryForProperty(nsCSSPropertyID aPropID)
  {
    Update();
    for (uint32_t i = 0; i < mExposedPropertyCount; i++) {
      const Entry* entry = &kEntries[EntryIndex(i)];
      if (entry->mProperty == aPropID) {
        return entry;
      }
    }
    return nullptr;
  }

  /**
   * Records that mIndexMap needs updating, due to prefs changing that could
   * affect the set of properties exposed on an nsComputedDOMStyle.
   */
  void MarkDirty() { mExposedPropertyCount = 0; }

  // The member variables are public so that we can use an initializer in
  // nsComputedDOMStyle::GetComputedStyleMap.  Use the member functions
  // above to get information from this object.

  /**
   * An entry for each property that can be exposed on an nsComputedDOMStyle.
   */
  const Entry kEntries[eComputedStyleProperty_COUNT];

  /**
   * The number of properties that should be exposed on an nsComputedDOMStyle.
   * This will be less than eComputedStyleProperty_COUNT if some property
   * prefs are disabled.  A value of 0 indicates that it and mIndexMap are out
   * of date.
   */
  uint32_t mExposedPropertyCount;

  /**
   * A map of indexes on the nsComputedDOMStyle object to indexes into kEntries.
   */
  uint32_t mIndexMap[eComputedStyleProperty_COUNT];

private:
  /**
   * Returns whether mExposedPropertyCount and mIndexMap are out of date.
   */
  bool IsDirty() { return mExposedPropertyCount == 0; }

  /**
   * Updates mExposedPropertyCount and mIndexMap to take into account properties
   * whose prefs are currently disabled.
   */
  void Update();

  /**
   * Maps an nsComputedDOMStyle indexed getter index to an index into kEntries.
   */
  uint32_t EntryIndex(uint32_t aIndex) const
  {
    MOZ_ASSERT(aIndex < mExposedPropertyCount);
    return mIndexMap[aIndex];
  }
};

void
nsComputedStyleMap::Update()
{
  if (!IsDirty()) {
    return;
  }

  uint32_t index = 0;
  for (uint32_t i = 0; i < eComputedStyleProperty_COUNT; i++) {
    if (kEntries[i].IsEnabled()) {
      mIndexMap[index++] = i;
    }
  }
  mExposedPropertyCount = index;
}

nsComputedDOMStyle::nsComputedDOMStyle(dom::Element* aElement,
                                       const nsAString& aPseudoElt,
                                       nsIPresShell* aPresShell,
                                       AnimationFlag aFlag)
  : mDocumentWeak(nullptr)
  , mOuterFrame(nullptr)
  , mInnerFrame(nullptr)
  , mPresShell(nullptr)
  , mStyleContextGeneration(0)
  , mExposeVisitedStyle(false)
  , mResolvedStyleContext(false)
  , mAnimationFlag(aFlag)
{
  MOZ_ASSERT(aElement && aPresShell);

  mDocumentWeak = do_GetWeakReference(aPresShell->GetDocument());
  mContent = aElement;
  mPseudo = nsCSSPseudoElements::GetPseudoAtom(aPseudoElt);

  MOZ_ASSERT(aPresShell->GetPresContext());
}

nsComputedDOMStyle::~nsComputedDOMStyle()
{
  ClearStyleContext();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsComputedDOMStyle)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsComputedDOMStyle)
  tmp->ClearStyleContext();  // remove observer before clearing mContent
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsComputedDOMStyle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(nsComputedDOMStyle)

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsComputedDOMStyle)
  return tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsComputedDOMStyle)
  return tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsComputedDOMStyle)
  return tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

// QueryInterface implementation for nsComputedDOMStyle
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsComputedDOMStyle)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
NS_INTERFACE_MAP_END_INHERITING(nsDOMCSSDeclaration)


NS_IMPL_CYCLE_COLLECTING_ADDREF(nsComputedDOMStyle)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsComputedDOMStyle)

NS_IMETHODIMP
nsComputedDOMStyle::GetPropertyValue(const nsCSSPropertyID aPropID,
                                     nsAString& aValue)
{
  // This is mostly to avoid code duplication with GetPropertyCSSValue(); if
  // perf ever becomes an issue here (doubtful), we can look into changing
  // this.
  return GetPropertyValue(
    NS_ConvertASCIItoUTF16(nsCSSProps::GetStringValue(aPropID)),
    aValue);
}

NS_IMETHODIMP
nsComputedDOMStyle::SetPropertyValue(const nsCSSPropertyID aPropID,
                                     const nsAString& aValue)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetCssText(nsAString& aCssText)
{
  aCssText.Truncate();

  return NS_OK;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetCssText(const nsAString& aCssText)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetLength(uint32_t* aLength)
{
  NS_PRECONDITION(aLength, "Null aLength!  Prepare to die!");

  uint32_t length = GetComputedStyleMap()->Length();

  // Make sure we have up to date style so that we can include custom
  // properties.
  UpdateCurrentStyleSources(false);
  if (mStyleContext) {
    length += StyleVariables()->mVariables.Count();
  }

  *aLength = length;

  ClearCurrentStyleSources();

  return NS_OK;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  *aParentRule = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPropertyValue(const nsAString& aPropertyName,
                                     nsAString& aReturn)
{
  aReturn.Truncate();

  ErrorResult error;
  RefPtr<CSSValue> val = GetPropertyCSSValue(aPropertyName, error);
  if (error.Failed()) {
    return error.StealNSResult();
  }

  if (val) {
    nsString text;
    val->GetCssText(text, error);
    aReturn.Assign(text);
    return error.StealNSResult();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetAuthoredPropertyValue(const nsAString& aPropertyName,
                                             nsAString& aReturn)
{
  // Authored style doesn't make sense to return from computed DOM style,
  // so just return whatever GetPropertyValue() returns.
  return GetPropertyValue(aPropertyName, aReturn);
}

/* static */
already_AddRefed<nsStyleContext>
nsComputedDOMStyle::GetStyleContext(Element* aElement,
                                    nsIAtom* aPseudo,
                                    nsIPresShell* aPresShell)
{
  // If the content has a pres shell, we must use it.  Otherwise we'd
  // potentially mix rule trees by using the wrong pres shell's style
  // set.  Using the pres shell from the content also means that any
  // content that's actually *in* a document will get the style from the
  // correct document.
  nsCOMPtr<nsIPresShell> presShell = GetPresShellForContent(aElement);
  if (!presShell) {
    presShell = aPresShell;
    if (!presShell)
      return nullptr;
  }

  presShell->FlushPendingNotifications(FlushType::Style);

  return GetStyleContextNoFlush(aElement, aPseudo, presShell);
}

namespace {
class MOZ_STACK_CLASS StyleResolver final
{
public:
  StyleResolver(nsPresContext* aPresContext,
                nsComputedDOMStyle::AnimationFlag aAnimationFlag)
    : mAnimationFlag(aAnimationFlag)
  {
    MOZ_ASSERT(aPresContext);

    // Nothing to do if we are going to resolve style *with* animation.
    if (mAnimationFlag == nsComputedDOMStyle::eWithAnimation) {
      return;
    }

    // Set SkipAnimationRules flag if we are going to resolve style without
    // animation.
    if (aPresContext->RestyleManager()->IsGecko()) {
      mRestyleManager = aPresContext->RestyleManager()->AsGecko();

      mOldSkipAnimationRules = mRestyleManager->SkipAnimationRules();
      mRestyleManager->SetSkipAnimationRules(true);
    } else {
      NS_WARNING("stylo: can't skip animaition rules yet");
    }
  }

  already_AddRefed<nsStyleContext>
  ResolveWithAnimation(StyleSetHandle aStyleSet,
                       Element* aElement,
                       CSSPseudoElementType aType,
                       nsStyleContext* aParentContext,
                       bool aInDocWithShell)
  {
    MOZ_ASSERT(mAnimationFlag == nsComputedDOMStyle::eWithAnimation,
      "AnimationFlag should be eWithAnimation");

    RefPtr<nsStyleContext> result;

    if (aType != CSSPseudoElementType::NotPseudo) {
      nsIFrame* frame = nsLayoutUtils::GetStyleFrame(aElement);
      Element* pseudoElement =
        frame && aInDocWithShell ? frame->GetPseudoElement(aType) : nullptr;
      result = aStyleSet->ResolvePseudoElementStyle(aElement, aType,
                                                    aParentContext,
                                                    pseudoElement);
    } else {
      result = aStyleSet->ResolveStyleFor(aElement, aParentContext,
                                          LazyComputeBehavior::Allow);
    }
    return result.forget();
  }

  already_AddRefed<nsStyleContext>
  ResolveWithoutAnimation(StyleSetHandle aStyleSet,
                          Element* aElement,
                          CSSPseudoElementType aType,
                          nsStyleContext* aParentContext,
                          bool aInDocWithShell)
  {
    MOZ_ASSERT(!aStyleSet->IsServo(),
      "Servo backend should not use this function");
    MOZ_ASSERT(mAnimationFlag == nsComputedDOMStyle::eWithoutAnimation,
      "AnimationFlag should be eWithoutAnimation");

    RefPtr<nsStyleContext> result;

    if (aType != CSSPseudoElementType::NotPseudo) {
      nsIFrame* frame = nsLayoutUtils::GetStyleFrame(aElement);
      Element* pseudoElement =
        frame && aInDocWithShell ? frame->GetPseudoElement(aType) : nullptr;
      result =
        aStyleSet->AsGecko()->ResolvePseudoElementStyleWithoutAnimation(
          aElement, aType,
          aParentContext,
          pseudoElement);
    } else {
      result =
        aStyleSet->AsGecko()->ResolveStyleWithoutAnimation(aElement,
                                                           aParentContext);
    }
    return result.forget();
  }

  ~StyleResolver()
  {
    if (mRestyleManager) {
      mRestyleManager->SetSkipAnimationRules(mOldSkipAnimationRules);
    }
  }

private:
  GeckoRestyleManager* mRestyleManager = nullptr;
  bool mOldSkipAnimationRules = false;
  nsComputedDOMStyle::AnimationFlag mAnimationFlag;
};
}

already_AddRefed<nsStyleContext>
nsComputedDOMStyle::DoGetStyleContextNoFlush(Element* aElement,
                                             nsIAtom* aPseudo,
                                             nsIPresShell* aPresShell,
                                             AnimationFlag aAnimationFlag)
{
  MOZ_ASSERT(aElement, "NULL element");
  // If the content has a pres shell, we must use it.  Otherwise we'd
  // potentially mix rule trees by using the wrong pres shell's style
  // set.  Using the pres shell from the content also means that any
  // content that's actually *in* a document will get the style from the
  // correct document.
  nsIPresShell *presShell = GetPresShellForContent(aElement);
  bool inDocWithShell = true;
  if (!presShell) {
    inDocWithShell = false;
    presShell = aPresShell;
    if (!presShell)
      return nullptr;
  }

  // XXX the !aElement->IsHTMLElement(nsGkAtoms::area)
  // check is needed due to bug 135040 (to avoid using
  // mPrimaryFrame). Remove it once that's fixed.
  if (!aPseudo &&
      inDocWithShell &&
      !aElement->IsHTMLElement(nsGkAtoms::area)) {
    nsIFrame* frame = nsLayoutUtils::GetStyleFrame(aElement);
    if (frame) {
      nsStyleContext* result = frame->StyleContext();
      // Don't use the style context if it was influenced by
      // pseudo-elements, since then it's not the primary style
      // for this element.
      if (!result->HasPseudoElementData()) {
        // The existing style context may have animation styles so check if we
        // need to remove them.
        if (aAnimationFlag == eWithoutAnimation) {
          nsPresContext* presContext = presShell->GetPresContext();
          MOZ_ASSERT(presContext, "Should have a prescontext if we have a frame");
          MOZ_ASSERT(presContext->StyleSet()->IsGecko(),
                     "stylo: Need ResolveStyleByRemovingAnimation for stylo");
          if (presContext && presContext->StyleSet()->IsGecko()) {
            nsStyleSet* styleSet = presContext->StyleSet()->AsGecko();
            return styleSet->ResolveStyleByRemovingAnimation(
                     aElement, result, eRestyle_AllHintsWithAnimations);
          } else {
            return nullptr;
          }
        }

        // this function returns an addrefed style context
        RefPtr<nsStyleContext> ret = result;
        return ret.forget();
      }
    }
  }

  // No frame has been created, or we have a pseudo, or we're looking
  // for the default style, so resolve the style ourselves.

  nsPresContext *presContext = presShell->GetPresContext();
  if (!presContext)
    return nullptr;

  StyleSetHandle styleSet = presShell->StyleSet();

  auto type = CSSPseudoElementType::NotPseudo;
  if (aPseudo) {
    type = nsCSSPseudoElements::
      GetPseudoType(aPseudo, CSSEnabledState::eIgnoreEnabledState);
    if (type >= CSSPseudoElementType::Count) {
      return nullptr;
    }
  }

  // For Servo, compute the result directly without recursively building up
  // a throwaway style context chain.
  if (ServoStyleSet* servoSet = styleSet->GetAsServo()) {
    return servoSet->ResolveTransientStyle(aElement, aPseudo, type);
  }

  RefPtr<nsStyleContext> parentContext;
  nsIContent* parent = aPseudo ? aElement : aElement->GetParent();
  // Don't resolve parent context for document fragments.
  if (parent && parent->IsElement()) {
    parentContext = GetStyleContextNoFlush(parent->AsElement(), nullptr,
                                           aPresShell);
  }

  StyleResolver styleResolver(presContext, aAnimationFlag);

  if (aAnimationFlag == eWithAnimation) {
    return styleResolver.ResolveWithAnimation(styleSet,
                                              aElement, type,
                                              parentContext,
                                              inDocWithShell);
  }

  return styleResolver.ResolveWithoutAnimation(styleSet,
                                               aElement, type,
                                               parentContext,
                                               inDocWithShell);
}

nsMargin
nsComputedDOMStyle::GetAdjustedValuesForBoxSizing()
{
  // We want the width/height of whatever parts 'width' or 'height' controls,
  // which can be different depending on the value of the 'box-sizing' property.
  const nsStylePosition* stylePos = StylePosition();

  nsMargin adjustment;
  if (stylePos->mBoxSizing == StyleBoxSizing::Border) {
    adjustment = mInnerFrame->GetUsedBorderAndPadding();
  }

  return adjustment;
}

/* static */
nsIPresShell*
nsComputedDOMStyle::GetPresShellForContent(nsIContent* aContent)
{
  nsIDocument* composedDoc = aContent->GetComposedDoc();
  if (!composedDoc)
    return nullptr;

  return composedDoc->GetShell();
}

// nsDOMCSSDeclaration abstract methods which should never be called
// on a nsComputedDOMStyle object, but must be defined to avoid
// compile errors.
DeclarationBlock*
nsComputedDOMStyle::GetCSSDeclaration(Operation)
{
  NS_RUNTIMEABORT("called nsComputedDOMStyle::GetCSSDeclaration");
  return nullptr;
}

nsresult
nsComputedDOMStyle::SetCSSDeclaration(DeclarationBlock*)
{
  NS_RUNTIMEABORT("called nsComputedDOMStyle::SetCSSDeclaration");
  return NS_ERROR_FAILURE;
}

nsIDocument*
nsComputedDOMStyle::DocToUpdate()
{
  NS_RUNTIMEABORT("called nsComputedDOMStyle::DocToUpdate");
  return nullptr;
}

void
nsComputedDOMStyle::GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv)
{
  NS_RUNTIMEABORT("called nsComputedDOMStyle::GetCSSParsingEnvironment");
  // Just in case NS_RUNTIMEABORT ever stops killing us for some reason
  aCSSParseEnv.mPrincipal = nullptr;
}

URLExtraData*
nsComputedDOMStyle::GetURLData() const
{
  NS_RUNTIMEABORT("called nsComputedDOMStyle::GetURLData");
  return nullptr;
}

void
nsComputedDOMStyle::ClearStyleContext()
{
  if (mResolvedStyleContext) {
    mResolvedStyleContext = false;
    mContent->RemoveMutationObserver(this);
  }
  mStyleContext = nullptr;
}

void
nsComputedDOMStyle::SetResolvedStyleContext(RefPtr<nsStyleContext>&& aContext)
{
  if (!mResolvedStyleContext) {
    mResolvedStyleContext = true;
    mContent->AddMutationObserver(this);
  }
  mStyleContext = aContext;
}

void
nsComputedDOMStyle::SetFrameStyleContext(nsStyleContext* aContext)
{
  ClearStyleContext();
  mStyleContext = aContext;
}

/**
 * The following function checks whether we need to explicitly resolve the style
 * again, even though we have a style context coming from the frame.
 *
 * This basically checks whether the style is or may be under a ::first-line or
 * ::first-letter frame, in which case we can't return the frame style, and we
 * need to resolve it. See bug 505515.
 */
static bool
MustReresolveStyle(const nsStyleContext* aContext)
{
  MOZ_ASSERT(aContext);

  if (aContext->HasPseudoElementData()) {
    if (!aContext->GetPseudo() ||
        aContext->StyleSource().IsServoComputedValues()) {
      // TODO(emilio): When ::first-line is supported in Servo, we may want to
      // fix this to avoid re-resolving pseudo-element styles.
      return true;
    }

    return aContext->GetParent() &&
           aContext->GetParent()->HasPseudoElementData();
  }

  return false;
}

void
nsComputedDOMStyle::UpdateCurrentStyleSources(bool aNeedsLayoutFlush)
{
  nsCOMPtr<nsIDocument> document = do_QueryReferent(mDocumentWeak);
  if (!document) {
    ClearStyleContext();
    return;
  }

  document->FlushPendingLinkUpdates();

  // Flush _before_ getting the presshell, since that could create a new
  // presshell.  Also note that we want to flush the style on the document
  // we're computing style in, not on the document mContent is in -- the two
  // may be different.
  document->FlushPendingNotifications(
    aNeedsLayoutFlush ? FlushType::Layout : FlushType::Style);
#ifdef DEBUG
  mFlushedPendingReflows = aNeedsLayoutFlush;
#endif

  mPresShell = document->GetShell();
  if (!mPresShell || !mPresShell->GetPresContext()) {
    ClearStyleContext();
    return;
  }

  uint64_t currentGeneration =
    mPresShell->GetPresContext()->GetRestyleGeneration();

  if (mStyleContext) {
    if (mStyleContextGeneration == currentGeneration) {
      // Our cached style context is still valid.
      return;
    }
    // We've processed some restyles, so the cached style context might
    // be out of date.
    mStyleContext = nullptr;
  }

  // XXX the !mContent->IsHTMLElement(nsGkAtoms::area)
  // check is needed due to bug 135040 (to avoid using
  // mPrimaryFrame). Remove it once that's fixed.
  if (!mContent->IsHTMLElement(nsGkAtoms::area)) {
    mOuterFrame = nullptr;

    if (!mPseudo) {
      mOuterFrame = mContent->GetPrimaryFrame();
    } else if (mPseudo == nsCSSPseudoElements::before ||
               mPseudo == nsCSSPseudoElements::after) {
      nsIAtom* property = mPseudo == nsCSSPseudoElements::before
                            ? nsGkAtoms::beforePseudoProperty
                            : nsGkAtoms::afterPseudoProperty;

      auto* pseudo = static_cast<Element*>(mContent->GetProperty(property));
      mOuterFrame = pseudo ? pseudo->GetPrimaryFrame() : nullptr;
    }

    mInnerFrame = mOuterFrame;
    if (mOuterFrame) {
      nsIAtom* type = mOuterFrame->GetType();
      if (type == nsGkAtoms::tableWrapperFrame) {
        // If the frame is a table wrapper frame then we should get the style
        // from the inner table frame.
        mInnerFrame = mOuterFrame->PrincipalChildList().FirstChild();
        NS_ASSERTION(mInnerFrame, "table wrapper must have an inner");
        NS_ASSERTION(!mInnerFrame->GetNextSibling(),
                     "table wrapper frames should have just one child, "
                     "the inner table");
      }

      SetFrameStyleContext(mInnerFrame->StyleContext());
      NS_ASSERTION(mStyleContext, "Frame without style context?");
    }
  }

  if (!mStyleContext || MustReresolveStyle(mStyleContext)) {
#ifdef DEBUG
    if (mStyleContext && mStyleContext->StyleSource().IsGeckoRuleNodeOrNull()) {
      // We want to check that going through this path because of
      // HasPseudoElementData is rare, because it slows us down a good
      // bit.  So check that we're really inside something associated
      // with a pseudo-element that contains elements.  (We also allow
      // the element to be NAC, just in case some chrome JS calls
      // getComputedStyle on a NAC-implemented pseudo.)
      nsStyleContext* topWithPseudoElementData = mStyleContext;
      while (topWithPseudoElementData->GetParent()->HasPseudoElementData()) {
        topWithPseudoElementData = topWithPseudoElementData->GetParent();
      }
      CSSPseudoElementType pseudo = topWithPseudoElementData->GetPseudoType();
      nsIAtom* pseudoAtom = nsCSSPseudoElements::GetPseudoAtom(pseudo);
      nsAutoString assertMsg(
        NS_LITERAL_STRING("we should be in a pseudo-element that is expected to contain elements ("));
      assertMsg.Append(nsDependentString(pseudoAtom->GetUTF16String()));
      assertMsg.Append(')');
      NS_ASSERTION(nsCSSPseudoElements::PseudoElementContainsElements(pseudo) ||
                   mContent->IsNativeAnonymous(),
                   NS_LossyConvertUTF16toASCII(assertMsg).get());
    }
#endif
    // Need to resolve a style context
    RefPtr<nsStyleContext> resolvedStyleContext =
      nsComputedDOMStyle::GetStyleContext(mContent->AsElement(),
                                          mPseudo,
                                          mPresShell);
    if (!resolvedStyleContext) {
      ClearStyleContext();
      return;
    }

    // No need to re-get the generation, even though GetStyleContext
    // will flush, since we flushed style at the top of this function.
    NS_ASSERTION(mPresShell &&
                 currentGeneration ==
                   mPresShell->GetPresContext()->GetRestyleGeneration(),
                 "why should we have flushed style again?");

    SetResolvedStyleContext(Move(resolvedStyleContext));
    NS_ASSERTION(mPseudo || !mStyleContext->HasPseudoElementData(),
                 "should not have pseudo-element data");
  }

  if (mAnimationFlag == eWithoutAnimation) {
    // We will support Servo in bug 1311257.
    MOZ_ASSERT(mPresShell->StyleSet()->IsGecko(),
               "eWithoutAnimationRules support Gecko only");
    nsStyleSet* styleSet = mPresShell->StyleSet()->AsGecko();
    RefPtr<nsStyleContext> unanimatedStyleContext =
      styleSet->ResolveStyleByRemovingAnimation(
        mContent->AsElement(), mStyleContext, eRestyle_AllHintsWithAnimations);
    SetResolvedStyleContext(Move(unanimatedStyleContext));
  }

  // mExposeVisitedStyle is set to true only by testing APIs that
  // require chrome privilege.
  MOZ_ASSERT(!mExposeVisitedStyle || nsContentUtils::IsCallerChrome(),
             "mExposeVisitedStyle set incorrectly");
  if (mExposeVisitedStyle && mStyleContext->RelevantLinkVisited()) {
    nsStyleContext *styleIfVisited = mStyleContext->GetStyleIfVisited();
    if (styleIfVisited) {
      mStyleContext = styleIfVisited;
    }
  }
}

void
nsComputedDOMStyle::ClearCurrentStyleSources()
{
  mOuterFrame = nullptr;
  mInnerFrame = nullptr;
  mPresShell = nullptr;

  // Release the current style context if we got it off the frame.
  // For a style context we resolved, keep it around so that we
  // can re-use it next time this object is queried.
  if (!mResolvedStyleContext) {
    mStyleContext = nullptr;
  }
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetPropertyCSSValue(const nsAString& aPropertyName, ErrorResult& aRv)
{
  nsCSSPropertyID prop =
    nsCSSProps::LookupProperty(aPropertyName, CSSEnabledState::eForAllContent);

  bool needsLayoutFlush;
  nsComputedStyleMap::Entry::ComputeMethod getter;

  if (prop == eCSSPropertyExtra_variable) {
    needsLayoutFlush = false;
    getter = nullptr;
  } else {
    // We don't (for now, anyway, though it may make sense to change it
    // for all aliases, including those in nsCSSPropAliasList) want
    // aliases to be enumerable (via GetLength and IndexedGetter), so
    // handle them here rather than adding entries to
    // GetQueryablePropertyMap.
    if (prop != eCSSProperty_UNKNOWN &&
        nsCSSProps::PropHasFlags(prop, CSS_PROPERTY_IS_ALIAS)) {
      const nsCSSPropertyID* subprops = nsCSSProps::SubpropertyEntryFor(prop);
      MOZ_ASSERT(subprops[1] == eCSSProperty_UNKNOWN,
                 "must have list of length 1");
      prop = subprops[0];
    }

    const nsComputedStyleMap::Entry* propEntry =
      GetComputedStyleMap()->FindEntryForProperty(prop);

    if (!propEntry) {
#ifdef DEBUG_ComputedDOMStyle
      NS_WARNING(PromiseFlatCString(NS_ConvertUTF16toUTF8(aPropertyName) +
                                    NS_LITERAL_CSTRING(" is not queryable!")).get());
#endif

      // NOTE:  For branches, we should flush here for compatibility!
      return nullptr;
    }

    needsLayoutFlush = propEntry->IsLayoutFlushNeeded();
    getter = propEntry->mGetter;
  }

  UpdateCurrentStyleSources(needsLayoutFlush);
  if (!mStyleContext) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  RefPtr<CSSValue> val;
  if (prop == eCSSPropertyExtra_variable) {
    val = DoGetCustomProperty(aPropertyName);
  } else {
    // Call our pointer-to-member-function.
    val = (this->*getter)();
  }

  ClearCurrentStyleSources();

  return val.forget();
}

NS_IMETHODIMP
nsComputedDOMStyle::RemoveProperty(const nsAString& aPropertyName,
                                   nsAString& aReturn)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetPropertyPriority(const nsAString& aPropertyName,
                                        nsAString& aReturn)
{
  aReturn.Truncate();

  return NS_OK;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetProperty(const nsAString& aPropertyName,
                                const nsAString& aValue,
                                const nsAString& aPriority)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::Item(uint32_t aIndex, nsAString& aReturn)
{
  return nsDOMCSSDeclaration::Item(aIndex, aReturn);
}

void
nsComputedDOMStyle::IndexedGetter(uint32_t   aIndex,
                                  bool&      aFound,
                                  nsAString& aPropName)
{
  nsComputedStyleMap* map = GetComputedStyleMap();
  uint32_t length = map->Length();

  if (aIndex < length) {
    aFound = true;
    CopyASCIItoUTF16(nsCSSProps::GetStringValue(map->PropertyAt(aIndex)),
                     aPropName);
    return;
  }

  // Custom properties are exposed with indexed properties just after all
  // of the built-in properties.
  UpdateCurrentStyleSources(false);
  if (!mStyleContext) {
    aFound = false;
    return;
  }

  const nsStyleVariables* variables = StyleVariables();
  if (aIndex - length < variables->mVariables.Count()) {
    aFound = true;
    nsString varName;
    variables->mVariables.GetVariableAt(aIndex - length, varName);
    aPropName.AssignLiteral("--");
    aPropName.Append(varName);
  } else {
    aFound = false;
  }

  ClearCurrentStyleSources();
}

// Property getters...

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBinding()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  const nsStyleDisplay* display = StyleDisplay();

  if (display->mBinding) {
    val->SetURI(display->mBinding->GetURI());
  } else {
    val->SetIdent(eCSSKeyword_none);
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetClear()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mBreakType,
                                               nsCSSProps::kClearKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFloat()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mFloat,
                                               nsCSSProps::kFloatKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBottom()
{
  return GetOffsetWidthFor(eSideBottom);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetStackSizing()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(StyleXUL()->mStretchStack ? eCSSKeyword_stretch_to_fit :
                eCSSKeyword_ignore);
  return val.forget();
}

void
nsComputedDOMStyle::SetToRGBAColor(nsROCSSPrimitiveValue* aValue,
                                   nscolor aColor)
{
  nsROCSSPrimitiveValue *red   = new nsROCSSPrimitiveValue;
  nsROCSSPrimitiveValue *green = new nsROCSSPrimitiveValue;
  nsROCSSPrimitiveValue *blue  = new nsROCSSPrimitiveValue;
  nsROCSSPrimitiveValue *alpha  = new nsROCSSPrimitiveValue;

  uint8_t a = NS_GET_A(aColor);
  nsDOMCSSRGBColor *rgbColor =
    new nsDOMCSSRGBColor(red, green, blue, alpha, a < 255);

  red->SetNumber(NS_GET_R(aColor));
  green->SetNumber(NS_GET_G(aColor));
  blue->SetNumber(NS_GET_B(aColor));
  alpha->SetNumber(nsStyleUtil::ColorComponentToFloat(a));

  aValue->SetColor(rgbColor);
}

void
nsComputedDOMStyle::SetValueFromComplexColor(nsROCSSPrimitiveValue* aValue,
                                             const StyleComplexColor& aColor)
{
  SetToRGBAColor(aValue, StyleColor()->CalcComplexColor(aColor));
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetColor()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetToRGBAColor(val, StyleColor()->mColor);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetColorAdjust()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleVisibility()->mColorAdjust,
                                   nsCSSProps::kColorAdjustKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetOpacity()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleEffects()->mOpacity);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetColumnCount()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  const nsStyleColumn* column = StyleColumn();

  if (column->mColumnCount == NS_STYLE_COLUMN_COUNT_AUTO) {
    val->SetIdent(eCSSKeyword_auto);
  } else {
    val->SetNumber(column->mColumnCount);
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetColumnWidth()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  // XXX fix the auto case. When we actually have a column frame, I think
  // we should return the computed column width.
  SetValueToCoord(val, StyleColumn()->mColumnWidth, true);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetColumnGap()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  const nsStyleColumn* column = StyleColumn();
  if (column->mColumnGap.GetUnit() == eStyleUnit_Normal) {
    val->SetAppUnits(StyleFont()->mFont.size);
  } else {
    SetValueToCoord(val, StyleColumn()->mColumnGap, true);
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetColumnFill()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleColumn()->mColumnFill,
                                   nsCSSProps::kColumnFillKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetColumnSpan()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleColumn()->mColumnSpan,
                                               nsCSSProps::kColumnSpanKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetColumnRuleWidth()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetAppUnits(StyleColumn()->GetComputedColumnRuleWidth());
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetColumnRuleStyle()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleColumn()->mColumnRuleStyle,
                                   nsCSSProps::kBorderStyleKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetColumnRuleColor()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueFromComplexColor(val, StyleColumn()->mColumnRuleColor);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetContent()
{
  const nsStyleContent *content = StyleContent();

  if (content->ContentCount() == 0) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_none);
    return val.forget();
  }

  if (content->ContentCount() == 1 &&
      content->ContentAt(0).GetType() == eStyleContentType_AltContent) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword__moz_alt_content);
    return val.forget();
  }

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);

  for (uint32_t i = 0, i_end = content->ContentCount(); i < i_end; ++i) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

    const nsStyleContentData &data = content->ContentAt(i);
    nsStyleContentType type = data.GetType();
    switch (type) {
      case eStyleContentType_String: {
        nsAutoString str;
        nsStyleUtil::AppendEscapedCSSString(
          nsDependentString(data.GetString()), str);
        val->SetString(str);
        break;
      }
      case eStyleContentType_Image: {
        nsCOMPtr<nsIURI> uri;
        if (imgRequestProxy* image = data.GetImage()) {
          image->GetURI(getter_AddRefs(uri));
        }
        val->SetURI(uri);
        break;
      }
      case eStyleContentType_Attr: {
        nsAutoString str;
        nsStyleUtil::AppendEscapedCSSIdent(
          nsDependentString(data.GetString()), str);
        val->SetString(str, nsIDOMCSSPrimitiveValue::CSS_ATTR);
        break;
      }
      case eStyleContentType_Counter:
      case eStyleContentType_Counters: {
        /* FIXME: counters should really use an object */
        nsAutoString str;
        if (type == eStyleContentType_Counter) {
          str.AppendLiteral("counter(");
        }
        else {
          str.AppendLiteral("counters(");
        }
        // WRITE ME
        nsCSSValue::Array* a = data.GetCounters();

        nsStyleUtil::AppendEscapedCSSIdent(
          nsDependentString(a->Item(0).GetStringBufferValue()), str);
        int32_t typeItem = 1;
        if (type == eStyleContentType_Counters) {
          typeItem = 2;
          str.AppendLiteral(", ");
          nsStyleUtil::AppendEscapedCSSString(
            nsDependentString(a->Item(1).GetStringBufferValue()), str);
        }
        MOZ_ASSERT(eCSSUnit_None != a->Item(typeItem).GetUnit(),
                   "'none' should be handled as identifier value");
        nsString type;
        a->Item(typeItem).AppendToString(eCSSProperty_list_style_type,
                                         type, nsCSSValue::eNormalized);
        if (!type.LowerCaseEqualsLiteral("decimal")) {
          str.AppendLiteral(", ");
          str.Append(type);
        }

        str.Append(char16_t(')'));
        val->SetString(str, nsIDOMCSSPrimitiveValue::CSS_COUNTER);
        break;
      }
      case eStyleContentType_OpenQuote:
        val->SetIdent(eCSSKeyword_open_quote);
        break;
      case eStyleContentType_CloseQuote:
        val->SetIdent(eCSSKeyword_close_quote);
        break;
      case eStyleContentType_NoOpenQuote:
        val->SetIdent(eCSSKeyword_no_open_quote);
        break;
      case eStyleContentType_NoCloseQuote:
        val->SetIdent(eCSSKeyword_no_close_quote);
        break;
      case eStyleContentType_AltContent:
      default:
        NS_NOTREACHED("unexpected type");
        break;
    }
    valueList->AppendCSSValue(val.forget());
  }

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetCounterIncrement()
{
  const nsStyleContent *content = StyleContent();

  if (content->CounterIncrementCount() == 0) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_none);
    return val.forget();
  }

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);

  for (uint32_t i = 0, i_end = content->CounterIncrementCount(); i < i_end; ++i) {
    RefPtr<nsROCSSPrimitiveValue> name = new nsROCSSPrimitiveValue;
    RefPtr<nsROCSSPrimitiveValue> value = new nsROCSSPrimitiveValue;

    const nsStyleCounterData& data = content->CounterIncrementAt(i);
    nsAutoString escaped;
    nsStyleUtil::AppendEscapedCSSIdent(data.mCounter, escaped);
    name->SetString(escaped);
    value->SetNumber(data.mValue); // XXX This should really be integer

    valueList->AppendCSSValue(name.forget());
    valueList->AppendCSSValue(value.forget());
  }

  return valueList.forget();
}

/* Convert the stored representation into a list of two values and then hand
 * it back.
 */
already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTransformOrigin()
{
  /* We need to build up a list of two values.  We'll call them
   * width and height.
   */

  /* Store things as a value list */
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);

  /* Now, get the values. */
  const nsStyleDisplay* display = StyleDisplay();

  RefPtr<nsROCSSPrimitiveValue> width = new nsROCSSPrimitiveValue;
  SetValueToCoord(width, display->mTransformOrigin[0], false,
                  &nsComputedDOMStyle::GetFrameBoundsWidthForTransform);
  valueList->AppendCSSValue(width.forget());

  RefPtr<nsROCSSPrimitiveValue> height = new nsROCSSPrimitiveValue;
  SetValueToCoord(height, display->mTransformOrigin[1], false,
                  &nsComputedDOMStyle::GetFrameBoundsHeightForTransform);
  valueList->AppendCSSValue(height.forget());

  if (display->mTransformOrigin[2].GetUnit() != eStyleUnit_Coord ||
      display->mTransformOrigin[2].GetCoordValue() != 0) {
    RefPtr<nsROCSSPrimitiveValue> depth = new nsROCSSPrimitiveValue;
    SetValueToCoord(depth, display->mTransformOrigin[2], false,
                    nullptr);
    valueList->AppendCSSValue(depth.forget());
  }

  return valueList.forget();
}

/* Convert the stored representation into a list of two values and then hand
 * it back.
 */
already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetPerspectiveOrigin()
{
  /* We need to build up a list of two values.  We'll call them
   * width and height.
   */

  /* Store things as a value list */
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);

  /* Now, get the values. */
  const nsStyleDisplay* display = StyleDisplay();

  RefPtr<nsROCSSPrimitiveValue> width = new nsROCSSPrimitiveValue;
  SetValueToCoord(width, display->mPerspectiveOrigin[0], false,
                  &nsComputedDOMStyle::GetFrameBoundsWidthForTransform);
  valueList->AppendCSSValue(width.forget());

  RefPtr<nsROCSSPrimitiveValue> height = new nsROCSSPrimitiveValue;
  SetValueToCoord(height, display->mPerspectiveOrigin[1], false,
                  &nsComputedDOMStyle::GetFrameBoundsHeightForTransform);
  valueList->AppendCSSValue(height.forget());

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetPerspective()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StyleDisplay()->mChildPerspective, false);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBackfaceVisibility()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
      nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mBackfaceVisibility,
                                     nsCSSProps::kBackfaceVisibilityKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTransformStyle()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
      nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mTransformStyle,
                                     nsCSSProps::kTransformStyleKTable));
  return val.forget();
}

/* If the property is "none", hand back "none" wrapped in a value.
 * Otherwise, compute the aggregate transform matrix and hands it back in a
 * "matrix" wrapper.
 */
already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTransform()
{
  /* First, get the display data.  We'll need it. */
  const nsStyleDisplay* display = StyleDisplay();

  /* If there are no transforms, then we should construct a single-element
   * entry and hand it back.
   */
  if (!display->mSpecifiedTransform) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

    /* Set it to "none." */
    val->SetIdent(eCSSKeyword_none);
    return val.forget();
  }

  /* Otherwise, we need to compute the current value of the transform matrix,
   * store it in a string, and hand it back to the caller.
   */

  /* Use the inner frame for the reference box.  If we don't have an inner
   * frame we use empty dimensions to allow us to continue (and percentage
   * values in the transform will simply give broken results).
   * TODO: There is no good way for us to represent the case where there's no
   * frame, which is problematic.  The reason is that when we have percentage
   * transforms, there are a total of four stored matrix entries that influence
   * the transform based on the size of the element.  However, this poses a
   * problem, because only two of these values can be explicitly referenced
   * using the named transforms.  Until a real solution is found, we'll just
   * use this approach.
   */
  nsStyleTransformMatrix::TransformReferenceBox refBox(mInnerFrame,
                                                       nsSize(0, 0));

   RuleNodeCacheConditions dummy;
   bool dummyBool;
   gfx::Matrix4x4 matrix =
     nsStyleTransformMatrix::ReadTransforms(display->mSpecifiedTransform->mHead,
                                            mStyleContext,
                                            mStyleContext->PresContext(),
                                            dummy,
                                            refBox,
                                            float(mozilla::AppUnitsPerCSSPixel()),
                                            &dummyBool);

  return MatrixToCSSValue(matrix);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTransformBox()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
      nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mTransformBox,
                                     nsCSSProps::kTransformBoxKTable));
  return val.forget();
}

/* static */ already_AddRefed<nsROCSSPrimitiveValue>
nsComputedDOMStyle::MatrixToCSSValue(const mozilla::gfx::Matrix4x4& matrix)
{
  bool is3D = !matrix.Is2D();

  nsAutoString resultString(NS_LITERAL_STRING("matrix"));
  if (is3D) {
    resultString.AppendLiteral("3d");
  }

  resultString.Append('(');
  resultString.AppendFloat(matrix._11);
  resultString.AppendLiteral(", ");
  resultString.AppendFloat(matrix._12);
  resultString.AppendLiteral(", ");
  if (is3D) {
    resultString.AppendFloat(matrix._13);
    resultString.AppendLiteral(", ");
    resultString.AppendFloat(matrix._14);
    resultString.AppendLiteral(", ");
  }
  resultString.AppendFloat(matrix._21);
  resultString.AppendLiteral(", ");
  resultString.AppendFloat(matrix._22);
  resultString.AppendLiteral(", ");
  if (is3D) {
    resultString.AppendFloat(matrix._23);
    resultString.AppendLiteral(", ");
    resultString.AppendFloat(matrix._24);
    resultString.AppendLiteral(", ");
    resultString.AppendFloat(matrix._31);
    resultString.AppendLiteral(", ");
    resultString.AppendFloat(matrix._32);
    resultString.AppendLiteral(", ");
    resultString.AppendFloat(matrix._33);
    resultString.AppendLiteral(", ");
    resultString.AppendFloat(matrix._34);
    resultString.AppendLiteral(", ");
  }
  resultString.AppendFloat(matrix._41);
  resultString.AppendLiteral(", ");
  resultString.AppendFloat(matrix._42);
  if (is3D) {
    resultString.AppendLiteral(", ");
    resultString.AppendFloat(matrix._43);
    resultString.AppendLiteral(", ");
    resultString.AppendFloat(matrix._44);
  }
  resultString.Append(')');

  /* Create a value to hold our result. */
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  val->SetString(resultString);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetCounterReset()
{
  const nsStyleContent *content = StyleContent();

  if (content->CounterResetCount() == 0) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_none);
    return val.forget();
  }

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);

  for (uint32_t i = 0, i_end = content->CounterResetCount(); i < i_end; ++i) {
    RefPtr<nsROCSSPrimitiveValue> name = new nsROCSSPrimitiveValue;
    RefPtr<nsROCSSPrimitiveValue> value = new nsROCSSPrimitiveValue;

    const nsStyleCounterData& data = content->CounterResetAt(i);
    nsAutoString escaped;
    nsStyleUtil::AppendEscapedCSSIdent(data.mCounter, escaped);
    name->SetString(escaped);
    value->SetNumber(data.mValue); // XXX This should really be integer

    valueList->AppendCSSValue(name.forget());
    valueList->AppendCSSValue(value.forget());
  }

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetQuotes()
{
  const auto& quotePairs = StyleList()->GetQuotePairs();

  if (quotePairs.IsEmpty()) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_none);
    return val.forget();
  }

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);

  for (const auto& quotePair : quotePairs) {
    RefPtr<nsROCSSPrimitiveValue> openVal = new nsROCSSPrimitiveValue;
    RefPtr<nsROCSSPrimitiveValue> closeVal = new nsROCSSPrimitiveValue;

    nsAutoString s;
    nsStyleUtil::AppendEscapedCSSString(quotePair.first, s);
    openVal->SetString(s);
    s.Truncate();
    nsStyleUtil::AppendEscapedCSSString(quotePair.second, s);
    closeVal->SetString(s);

    valueList->AppendCSSValue(openVal.forget());
    valueList->AppendCSSValue(closeVal.forget());
  }

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFontFamily()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  const nsStyleFont* font = StyleFont();
  nsAutoString fontlistStr;
  nsStyleUtil::AppendEscapedCSSFontFamilyList(font->mFont.fontlist,
                                              fontlistStr);
  val->SetString(fontlistStr);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFontSize()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  // Note: StyleFont()->mSize is the 'computed size';
  // StyleFont()->mFont.size is the 'actual size'
  val->SetAppUnits(StyleFont()->mSize);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFontSizeAdjust()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  const nsStyleFont *font = StyleFont();

  if (font->mFont.sizeAdjust >= 0.0f) {
    val->SetNumber(font->mFont.sizeAdjust);
  } else {
    val->SetIdent(eCSSKeyword_none);
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetOsxFontSmoothing()
{
  if (nsContentUtils::ShouldResistFingerprinting(
        mPresShell->GetPresContext()->GetDocShell()))
    return nullptr;

  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleFont()->mFont.smoothing,
                                               nsCSSProps::kFontSmoothingKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFontStretch()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleFont()->mFont.stretch,
                                               nsCSSProps::kFontStretchKTable));

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFontStyle()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleFont()->mFont.style,
                                               nsCSSProps::kFontStyleKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFontWeight()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  const nsStyleFont* font = StyleFont();

  uint16_t weight = font->mFont.weight;
  NS_ASSERTION(weight % 100 == 0, "unexpected value of font-weight");
  val->SetNumber(weight);

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFontFeatureSettings()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  const nsStyleFont* font = StyleFont();
  if (font->mFont.fontFeatureSettings.IsEmpty()) {
    val->SetIdent(eCSSKeyword_normal);
  } else {
    nsAutoString result;
    nsStyleUtil::AppendFontFeatureSettings(font->mFont.fontFeatureSettings,
                                           result);
    val->SetString(result);
  }
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFontVariationSettings()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  const nsStyleFont* font = StyleFont();
  if (font->mFont.fontVariationSettings.IsEmpty()) {
    val->SetIdent(eCSSKeyword_normal);
  } else {
    nsAutoString result;
    nsStyleUtil::AppendFontVariationSettings(font->mFont.fontVariationSettings,
                                             result);
    val->SetString(result);
  }
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFontKerning()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleFont()->mFont.kerning,
                                   nsCSSProps::kFontKerningKTable));
  return val.forget();
}

static void
SerializeLanguageOverride(uint32_t aLanguageOverride, nsAString& aResult)
{
  aResult.Truncate();
  uint32_t i;
  for (i = 0; i < 4 ; i++) {
    char16_t ch = aLanguageOverride >> 24;
    MOZ_ASSERT(nsCRT::IsAscii(ch),
               "Invalid tags, we should've handled this during computing!");
    aResult.Append(ch);
    aLanguageOverride = aLanguageOverride << 8;
  }
  // strip trailing whitespaces
  while (i > 0 && aResult[i - 1] == ' ') {
    i--;
  }
  aResult.Truncate(i);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFontLanguageOverride()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  const nsStyleFont* font = StyleFont();
  if (font->mFont.languageOverride == 0) {
    val->SetIdent(eCSSKeyword_normal);
  } else {
    nsAutoString serializedStr, escapedStr;
    SerializeLanguageOverride(font->mFont.languageOverride, serializedStr);
    nsStyleUtil::AppendEscapedCSSString(serializedStr, escapedStr);
    val->SetString(escapedStr);
  }
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFontSynthesis()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  int32_t intValue = StyleFont()->mFont.synthesis;

  if (0 == intValue) {
    val->SetIdent(eCSSKeyword_none);
  } else {
    nsAutoString valueStr;

    nsStyleUtil::AppendBitmaskCSSValue(eCSSProperty_font_synthesis,
      intValue, NS_FONT_SYNTHESIS_WEIGHT,
      NS_FONT_SYNTHESIS_STYLE, valueStr);
    val->SetString(valueStr);
  }

  return val.forget();
}

// return a value *only* for valid longhand values from CSS 2.1, either
// normal or small-caps only
already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFontVariant()
{
  const nsFont& f = StyleFont()->mFont;

  // if any of the other font-variant subproperties other than
  // font-variant-caps are not normal then can't calculate a computed value
  if (f.variantAlternates || f.variantEastAsian || f.variantLigatures ||
      f.variantNumeric || f.variantPosition) {
    return nullptr;
  }

  nsCSSKeyword keyword;
  switch (f.variantCaps) {
    case 0:
      keyword = eCSSKeyword_normal;
      break;
    case NS_FONT_VARIANT_CAPS_SMALLCAPS:
      keyword = eCSSKeyword_small_caps;
      break;
    default:
      return nullptr;
  }

  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(keyword);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFontVariantAlternates()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  int32_t intValue = StyleFont()->mFont.variantAlternates;

  if (0 == intValue) {
    val->SetIdent(eCSSKeyword_normal);
    return val.forget();
  }

  // first, include enumerated values
  nsAutoString valueStr;

  nsStyleUtil::AppendBitmaskCSSValue(eCSSProperty_font_variant_alternates,
    intValue & NS_FONT_VARIANT_ALTERNATES_ENUMERATED_MASK,
    NS_FONT_VARIANT_ALTERNATES_HISTORICAL,
    NS_FONT_VARIANT_ALTERNATES_HISTORICAL, valueStr);

  // next, include functional values if present
  if (intValue & NS_FONT_VARIANT_ALTERNATES_FUNCTIONAL_MASK) {
    nsStyleUtil::SerializeFunctionalAlternates(StyleFont()->mFont.alternateValues,
                                               valueStr);
  }

  val->SetString(valueStr);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFontVariantCaps()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  int32_t intValue = StyleFont()->mFont.variantCaps;

  if (0 == intValue) {
    val->SetIdent(eCSSKeyword_normal);
  } else {
    val->SetIdent(
      nsCSSProps::ValueToKeywordEnum(intValue,
                                     nsCSSProps::kFontVariantCapsKTable));
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFontVariantEastAsian()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  int32_t intValue = StyleFont()->mFont.variantEastAsian;

  if (0 == intValue) {
    val->SetIdent(eCSSKeyword_normal);
  } else {
    nsAutoString valueStr;

    nsStyleUtil::AppendBitmaskCSSValue(eCSSProperty_font_variant_east_asian,
      intValue, NS_FONT_VARIANT_EAST_ASIAN_JIS78,
      NS_FONT_VARIANT_EAST_ASIAN_RUBY, valueStr);
    val->SetString(valueStr);
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFontVariantLigatures()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  int32_t intValue = StyleFont()->mFont.variantLigatures;

  if (0 == intValue) {
    val->SetIdent(eCSSKeyword_normal);
  } else if (NS_FONT_VARIANT_LIGATURES_NONE == intValue) {
    val->SetIdent(eCSSKeyword_none);
  } else {
    nsAutoString valueStr;

    nsStyleUtil::AppendBitmaskCSSValue(eCSSProperty_font_variant_ligatures,
      intValue, NS_FONT_VARIANT_LIGATURES_NONE,
      NS_FONT_VARIANT_LIGATURES_NO_CONTEXTUAL, valueStr);
    val->SetString(valueStr);
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFontVariantNumeric()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  int32_t intValue = StyleFont()->mFont.variantNumeric;

  if (0 == intValue) {
    val->SetIdent(eCSSKeyword_normal);
  } else {
    nsAutoString valueStr;

    nsStyleUtil::AppendBitmaskCSSValue(eCSSProperty_font_variant_numeric,
      intValue, NS_FONT_VARIANT_NUMERIC_LINING,
      NS_FONT_VARIANT_NUMERIC_ORDINAL, valueStr);
    val->SetString(valueStr);
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFontVariantPosition()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  int32_t intValue = StyleFont()->mFont.variantPosition;

  if (0 == intValue) {
    val->SetIdent(eCSSKeyword_normal);
  } else {
    val->SetIdent(
      nsCSSProps::ValueToKeywordEnum(intValue,
                                     nsCSSProps::kFontVariantPositionKTable));
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBackgroundAttachment()
{
  return GetBackgroundList(&nsStyleImageLayers::Layer::mAttachment,
                           &nsStyleImageLayers::mAttachmentCount,
                           StyleBackground()->mImage,
                           nsCSSProps::kImageLayerAttachmentKTable);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBackgroundClip()
{
  return GetBackgroundList(&nsStyleImageLayers::Layer::mClip,
                           &nsStyleImageLayers::mClipCount,
                           StyleBackground()->mImage,
                           nsCSSProps::kBackgroundClipKTable);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBackgroundColor()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueFromComplexColor(val, StyleBackground()->mBackgroundColor);
  return val.forget();
}

static void
SetValueToCalc(const nsStyleCoord::CalcValue* aCalc,
               nsROCSSPrimitiveValue*         aValue)
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  nsAutoString tmp, result;

  result.AppendLiteral("calc(");

  val->SetAppUnits(aCalc->mLength);
  val->GetCssText(tmp);
  result.Append(tmp);

  if (aCalc->mHasPercent) {
    result.AppendLiteral(" + ");

    val->SetPercent(aCalc->mPercent);
    val->GetCssText(tmp);
    result.Append(tmp);
  }

  result.Append(')');

  aValue->SetString(result); // not really SetString
}

static void
AppendCSSGradientLength(const nsStyleCoord&    aValue,
                        nsROCSSPrimitiveValue* aPrimitive,
                        nsAString&             aString)
{
  nsAutoString tokenString;
  if (aValue.IsCalcUnit())
    SetValueToCalc(aValue.GetCalcValue(), aPrimitive);
  else if (aValue.GetUnit() == eStyleUnit_Coord)
    aPrimitive->SetAppUnits(aValue.GetCoordValue());
  else
    aPrimitive->SetPercent(aValue.GetPercentValue());
  aPrimitive->GetCssText(tokenString);
  aString.Append(tokenString);
}

static void
AppendCSSGradientToBoxPosition(const nsStyleGradient* aGradient,
                               nsAString&             aString,
                               bool&                  aNeedSep)
{
  // This function only supports box position keywords. Make sure we're not
  // calling it with inputs that would have coordinates that aren't
  // representable with box-position keywords.
  MOZ_ASSERT(aGradient->mShape == NS_STYLE_GRADIENT_SHAPE_LINEAR &&
             !(aGradient->mLegacySyntax && aGradient->mMozLegacySyntax),
             "Only call me for linear-gradient and -webkit-linear-gradient");

  float xValue = aGradient->mBgPosX.GetPercentValue();
  float yValue = aGradient->mBgPosY.GetPercentValue();

  if (yValue == 1.0f && xValue == 0.5f) {
    // omit "to bottom"
    return;
  }
  NS_ASSERTION(yValue != 0.5f || xValue != 0.5f, "invalid box position");

  if (!aGradient->mLegacySyntax) {
    // Modern syntax explicitly includes the word "to". Old syntax does not
    // (and is implicitly "from" the given position instead).
    aString.AppendLiteral("to ");
  }

  if (yValue == 0.0f) {
    aString.AppendLiteral("top");
  } else if (yValue == 1.0f) {
    aString.AppendLiteral("bottom");
  } else if (yValue != 0.5f) { // do not write "center" keyword
    NS_NOTREACHED("invalid box position");
  }

  if (yValue != 0.5f && xValue != 0.5f) {
    // We're appending both a y-keyword and an x-keyword.
    // Add a space between them here.
    aString.AppendLiteral(" ");
  }

  if (xValue == 0.0f) {
    aString.AppendLiteral("left");
  } else if (xValue == 1.0f) {
    aString.AppendLiteral("right");
  } else if (xValue != 0.5f) { // do not write "center" keyword
    NS_NOTREACHED("invalid box position");
  }

  aNeedSep = true;
}

void
nsComputedDOMStyle::GetCSSGradientString(const nsStyleGradient* aGradient,
                                         nsAString& aString)
{
  if (!aGradient->mLegacySyntax) {
    aString.Truncate();
  } else {
    if (aGradient->mMozLegacySyntax) {
      aString.AssignLiteral("-moz-");
    } else {
      aString.AssignLiteral("-webkit-");
    }
  }
  if (aGradient->mRepeating) {
    aString.AppendLiteral("repeating-");
  }
  bool isRadial = aGradient->mShape != NS_STYLE_GRADIENT_SHAPE_LINEAR;
  if (isRadial) {
    aString.AppendLiteral("radial-gradient(");
  } else {
    aString.AppendLiteral("linear-gradient(");
  }

  bool needSep = false;
  nsAutoString tokenString;
  RefPtr<nsROCSSPrimitiveValue> tmpVal = new nsROCSSPrimitiveValue;

  if (isRadial && !aGradient->mLegacySyntax) {
    if (aGradient->mSize != NS_STYLE_GRADIENT_SIZE_EXPLICIT_SIZE) {
      if (aGradient->mShape == NS_STYLE_GRADIENT_SHAPE_CIRCULAR) {
        aString.AppendLiteral("circle");
        needSep = true;
      }
      if (aGradient->mSize != NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER) {
        if (needSep) {
          aString.Append(' ');
        }
        AppendASCIItoUTF16(nsCSSProps::
                           ValueToKeyword(aGradient->mSize,
                                          nsCSSProps::kRadialGradientSizeKTable),
                           aString);
        needSep = true;
      }
    } else {
      AppendCSSGradientLength(aGradient->mRadiusX, tmpVal, aString);
      if (aGradient->mShape != NS_STYLE_GRADIENT_SHAPE_CIRCULAR) {
        aString.Append(' ');
        AppendCSSGradientLength(aGradient->mRadiusY, tmpVal, aString);
      }
      needSep = true;
    }
  }
  if (aGradient->mBgPosX.GetUnit() != eStyleUnit_None) {
    MOZ_ASSERT(aGradient->mBgPosY.GetUnit() != eStyleUnit_None);
    if (!isRadial &&
        !(aGradient->mLegacySyntax && aGradient->mMozLegacySyntax)) {
      // linear-gradient() or -webkit-linear-gradient()
      AppendCSSGradientToBoxPosition(aGradient, aString, needSep);
    } else if (aGradient->mBgPosX.GetUnit() != eStyleUnit_Percent ||
               aGradient->mBgPosX.GetPercentValue() != 0.5f ||
               aGradient->mBgPosY.GetUnit() != eStyleUnit_Percent ||
               aGradient->mBgPosY.GetPercentValue() != (isRadial ? 0.5f : 1.0f)) {
      if (isRadial && !aGradient->mLegacySyntax) {
        if (needSep) {
          aString.Append(' ');
        }
        aString.AppendLiteral("at ");
        needSep = false;
      }
      AppendCSSGradientLength(aGradient->mBgPosX, tmpVal, aString);
      if (aGradient->mBgPosY.GetUnit() != eStyleUnit_None) {
        aString.Append(' ');
        AppendCSSGradientLength(aGradient->mBgPosY, tmpVal, aString);
      }
      needSep = true;
    }
  }
  if (aGradient->mAngle.GetUnit() != eStyleUnit_None) {
    MOZ_ASSERT(!isRadial || aGradient->mLegacySyntax);
    if (needSep) {
      aString.Append(' ');
    }
    nsStyleUtil::AppendAngleValue(aGradient->mAngle, aString);
    needSep = true;
  }

  if (isRadial && aGradient->mLegacySyntax &&
      (aGradient->mShape == NS_STYLE_GRADIENT_SHAPE_CIRCULAR ||
       aGradient->mSize != NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER)) {
    MOZ_ASSERT(aGradient->mSize != NS_STYLE_GRADIENT_SIZE_EXPLICIT_SIZE);
    if (needSep) {
      aString.AppendLiteral(", ");
      needSep = false;
    }
    if (aGradient->mShape == NS_STYLE_GRADIENT_SHAPE_CIRCULAR) {
      aString.AppendLiteral("circle");
      needSep = true;
    }
    if (aGradient->mSize != NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER) {
      if (needSep) {
        aString.Append(' ');
      }
      AppendASCIItoUTF16(nsCSSProps::
                         ValueToKeyword(aGradient->mSize,
                                        nsCSSProps::kRadialGradientSizeKTable),
                         aString);
    }
    needSep = true;
  }


  // color stops
  for (uint32_t i = 0; i < aGradient->mStops.Length(); ++i) {
    if (needSep) {
      aString.AppendLiteral(", ");
    }

    const auto& stop = aGradient->mStops[i];
    if (!stop.mIsInterpolationHint) {
      SetToRGBAColor(tmpVal, stop.mColor);
      tmpVal->GetCssText(tokenString);
      aString.Append(tokenString);
    }

    if (stop.mLocation.GetUnit() != eStyleUnit_None) {
      if (!stop.mIsInterpolationHint) {
        aString.Append(' ');
      }
      AppendCSSGradientLength(stop.mLocation, tmpVal, aString);
    }
    needSep = true;
  }

  aString.Append(')');
}

// -moz-image-rect(<uri>, <top>, <right>, <bottom>, <left>)
void
nsComputedDOMStyle::GetImageRectString(nsIURI* aURI,
                                       const nsStyleSides& aCropRect,
                                       nsString& aString)
{
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  // <uri>
  RefPtr<nsROCSSPrimitiveValue> valURI = new nsROCSSPrimitiveValue;
  valURI->SetURI(aURI);
  valueList->AppendCSSValue(valURI.forget());

  // <top>, <right>, <bottom>, <left>
  NS_FOR_CSS_SIDES(side) {
    RefPtr<nsROCSSPrimitiveValue> valSide = new nsROCSSPrimitiveValue;
    SetValueToCoord(valSide, aCropRect.Get(side), false);
    valueList->AppendCSSValue(valSide.forget());
  }

  nsAutoString argumentString;
  valueList->GetCssText(argumentString);

  aString = NS_LITERAL_STRING("-moz-image-rect(") +
            argumentString +
            NS_LITERAL_STRING(")");
}

void
nsComputedDOMStyle::SetValueToStyleImage(const nsStyleImage& aStyleImage,
                                         nsROCSSPrimitiveValue* aValue)
{
  switch (aStyleImage.GetType()) {
    case eStyleImageType_Image:
    {
      nsCOMPtr<nsIURI> uri = aStyleImage.GetImageURI();
      if (!uri) {
        aValue->SetIdent(eCSSKeyword_none);
        break;
      }

      const UniquePtr<nsStyleSides>& cropRect = aStyleImage.GetCropRect();
      if (cropRect) {
        nsAutoString imageRectString;
        GetImageRectString(uri, *cropRect, imageRectString);
        aValue->SetString(imageRectString);
      } else {
        aValue->SetURI(uri);
      }
      break;
    }
    case eStyleImageType_Gradient:
    {
      nsAutoString gradientString;
      GetCSSGradientString(aStyleImage.GetGradientData(),
                           gradientString);
      aValue->SetString(gradientString);
      break;
    }
    case eStyleImageType_Element:
    {
      nsAutoString elementId;
      nsStyleUtil::AppendEscapedCSSIdent(
        nsDependentAtomString(aStyleImage.GetElementId()),
                              elementId);
      nsAutoString elementString = NS_LITERAL_STRING("-moz-element(#") +
                                   elementId +
                                   NS_LITERAL_STRING(")");
      aValue->SetString(elementString);
      break;
    }
    case eStyleImageType_Null:
      aValue->SetIdent(eCSSKeyword_none);
      break;
    case eStyleImageType_URL:
      SetValueToURLValue(aStyleImage.GetURLValue(), aValue);
      break;
    default:
      NS_NOTREACHED("unexpected image type");
      break;
  }
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetImageLayerImage(const nsStyleImageLayers& aLayers)
{
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  for (uint32_t i = 0, i_end = aLayers.mImageCount; i < i_end; ++i) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

    SetValueToStyleImage(aLayers.mLayers[i].mImage, val);
    valueList->AppendCSSValue(val.forget());
  }

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetImageLayerPosition(const nsStyleImageLayers& aLayers)
{
  if (aLayers.mPositionXCount != aLayers.mPositionYCount) {
    // No value to return.  We can't express this combination of
    // values as a shorthand.
    return nullptr;
  }

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);
  for (uint32_t i = 0, i_end = aLayers.mPositionXCount; i < i_end; ++i) {
    RefPtr<nsDOMCSSValueList> itemList = GetROCSSValueList(false);

    SetValueToPosition(aLayers.mLayers[i].mPosition, itemList);
    valueList->AppendCSSValue(itemList.forget());
  }

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetImageLayerPositionX(const nsStyleImageLayers& aLayers)
{
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);
  for (uint32_t i = 0, i_end = aLayers.mPositionXCount; i < i_end; ++i) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    SetValueToPositionCoord(aLayers.mLayers[i].mPosition.mXPosition, val);
    valueList->AppendCSSValue(val.forget());
  }

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetImageLayerPositionY(const nsStyleImageLayers& aLayers)
{
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);
  for (uint32_t i = 0, i_end = aLayers.mPositionYCount; i < i_end; ++i) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    SetValueToPositionCoord(aLayers.mLayers[i].mPosition.mYPosition, val);
    valueList->AppendCSSValue(val.forget());
  }

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetImageLayerRepeat(const nsStyleImageLayers& aLayers)
{
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  for (uint32_t i = 0, i_end = aLayers.mRepeatCount; i < i_end; ++i) {
    RefPtr<nsDOMCSSValueList> itemList = GetROCSSValueList(false);
    RefPtr<nsROCSSPrimitiveValue> valX = new nsROCSSPrimitiveValue;

    const uint8_t& xRepeat = aLayers.mLayers[i].mRepeat.mXRepeat;
    const uint8_t& yRepeat = aLayers.mLayers[i].mRepeat.mYRepeat;

    bool hasContraction = true;
    unsigned contraction;
    if (xRepeat == yRepeat) {
      contraction = xRepeat;
    } else if (xRepeat == NS_STYLE_IMAGELAYER_REPEAT_REPEAT &&
               yRepeat == NS_STYLE_IMAGELAYER_REPEAT_NO_REPEAT) {
      contraction = NS_STYLE_IMAGELAYER_REPEAT_REPEAT_X;
    } else if (xRepeat == NS_STYLE_IMAGELAYER_REPEAT_NO_REPEAT &&
               yRepeat == NS_STYLE_IMAGELAYER_REPEAT_REPEAT) {
      contraction = NS_STYLE_IMAGELAYER_REPEAT_REPEAT_Y;
    } else {
      hasContraction = false;
    }

    RefPtr<nsROCSSPrimitiveValue> valY;
    if (hasContraction) {
      valX->SetIdent(nsCSSProps::ValueToKeywordEnum(contraction,
                                         nsCSSProps::kImageLayerRepeatKTable));
    } else {
      valY = new nsROCSSPrimitiveValue;

      valX->SetIdent(nsCSSProps::ValueToKeywordEnum(xRepeat,
                                          nsCSSProps::kImageLayerRepeatKTable));
      valY->SetIdent(nsCSSProps::ValueToKeywordEnum(yRepeat,
                                          nsCSSProps::kImageLayerRepeatKTable));
    }
    itemList->AppendCSSValue(valX.forget());
    if (valY) {
      itemList->AppendCSSValue(valY.forget());
    }
    valueList->AppendCSSValue(itemList.forget());
  }

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetImageLayerSize(const nsStyleImageLayers& aLayers)
{
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  for (uint32_t i = 0, i_end = aLayers.mSizeCount; i < i_end; ++i) {
    const nsStyleImageLayers::Size &size = aLayers.mLayers[i].mSize;

    switch (size.mWidthType) {
      case nsStyleImageLayers::Size::eContain:
      case nsStyleImageLayers::Size::eCover: {
        MOZ_ASSERT(size.mWidthType == size.mHeightType,
                   "unsynced types");
        nsCSSKeyword keyword = size.mWidthType == nsStyleImageLayers::Size::eContain
                             ? eCSSKeyword_contain
                             : eCSSKeyword_cover;
        RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
        val->SetIdent(keyword);
        valueList->AppendCSSValue(val.forget());
        break;
      }
      default: {
        RefPtr<nsDOMCSSValueList> itemList = GetROCSSValueList(false);

        RefPtr<nsROCSSPrimitiveValue> valX = new nsROCSSPrimitiveValue;
        RefPtr<nsROCSSPrimitiveValue> valY = new nsROCSSPrimitiveValue;

        if (size.mWidthType == nsStyleImageLayers::Size::eAuto) {
          valX->SetIdent(eCSSKeyword_auto);
        } else {
          MOZ_ASSERT(size.mWidthType ==
                       nsStyleImageLayers::Size::eLengthPercentage,
                     "bad mWidthType");
          if (!size.mWidth.mHasPercent &&
              // negative values must have come from calc()
              size.mWidth.mLength >= 0) {
            MOZ_ASSERT(size.mWidth.mPercent == 0.0f,
                       "Shouldn't have mPercent");
            valX->SetAppUnits(size.mWidth.mLength);
          } else if (size.mWidth.mLength == 0 &&
                     // negative values must have come from calc()
                     size.mWidth.mPercent >= 0.0f) {
            valX->SetPercent(size.mWidth.mPercent);
          } else {
            SetValueToCalc(&size.mWidth, valX);
          }
        }

        if (size.mHeightType == nsStyleImageLayers::Size::eAuto) {
          valY->SetIdent(eCSSKeyword_auto);
        } else {
          MOZ_ASSERT(size.mHeightType ==
                       nsStyleImageLayers::Size::eLengthPercentage,
                     "bad mHeightType");
          if (!size.mHeight.mHasPercent &&
              // negative values must have come from calc()
              size.mHeight.mLength >= 0) {
            MOZ_ASSERT(size.mHeight.mPercent == 0.0f,
                       "Shouldn't have mPercent");
            valY->SetAppUnits(size.mHeight.mLength);
          } else if (size.mHeight.mLength == 0 &&
                     // negative values must have come from calc()
                     size.mHeight.mPercent >= 0.0f) {
            valY->SetPercent(size.mHeight.mPercent);
          } else {
            SetValueToCalc(&size.mHeight, valY);
          }
        }
        itemList->AppendCSSValue(valX.forget());
        itemList->AppendCSSValue(valY.forget());
        valueList->AppendCSSValue(itemList.forget());
        break;
      }
    }
  }

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBackgroundImage()
{
  const nsStyleImageLayers& layers = StyleBackground()->mImage;
  return DoGetImageLayerImage(layers);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBackgroundBlendMode()
{
  return GetBackgroundList(&nsStyleImageLayers::Layer::mBlendMode,
                           &nsStyleImageLayers::mBlendModeCount,
                           StyleBackground()->mImage,
                           nsCSSProps::kBlendModeKTable);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBackgroundOrigin()
{
  return GetBackgroundList(&nsStyleImageLayers::Layer::mOrigin,
                           &nsStyleImageLayers::mOriginCount,
                           StyleBackground()->mImage,
                           nsCSSProps::kBackgroundOriginKTable);
}

void
nsComputedDOMStyle::SetValueToPositionCoord(
    const Position::Coord& aCoord,
    nsROCSSPrimitiveValue* aValue)
{
  if (!aCoord.mHasPercent) {
    MOZ_ASSERT(aCoord.mPercent == 0.0f,
               "Shouldn't have mPercent!");
    aValue->SetAppUnits(aCoord.mLength);
  } else if (aCoord.mLength == 0) {
    aValue->SetPercent(aCoord.mPercent);
  } else {
    SetValueToCalc(&aCoord, aValue);
  }
}

void
nsComputedDOMStyle::SetValueToPosition(
    const Position& aPosition,
    nsDOMCSSValueList* aValueList)
{
  RefPtr<nsROCSSPrimitiveValue> valX = new nsROCSSPrimitiveValue;
  SetValueToPositionCoord(aPosition.mXPosition, valX);
  aValueList->AppendCSSValue(valX.forget());

  RefPtr<nsROCSSPrimitiveValue> valY = new nsROCSSPrimitiveValue;
  SetValueToPositionCoord(aPosition.mYPosition, valY);
  aValueList->AppendCSSValue(valY.forget());
}


void
nsComputedDOMStyle::SetValueToURLValue(const css::URLValueData* aURL,
                                       nsROCSSPrimitiveValue* aValue)
{
  if (!aURL) {
    aValue->SetIdent(eCSSKeyword_none);
    return;
  }

  // If we have a usable nsIURI in the URLValueData, and the url() wasn't
  // a fragment-only URL, serialize the nsIURI.
  if (!aURL->IsLocalRef()) {
    if (nsIURI* uri = aURL->GetURI()) {
      aValue->SetURI(uri);
      return;
    }
  }

  // Otherwise, serialize the specified URL value.
  nsAutoString source;
  aURL->GetSourceString(source);

  nsAutoString url;
  url.AppendLiteral(u"url(");
  nsStyleUtil::AppendEscapedCSSString(source, url, '"');
  url.Append(')');
  aValue->SetString(url);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBackgroundPosition()
{
  const nsStyleImageLayers& layers = StyleBackground()->mImage;
  return DoGetImageLayerPosition(layers);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBackgroundPositionX()
{
  const nsStyleImageLayers& layers = StyleBackground()->mImage;
  return DoGetImageLayerPositionX(layers);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBackgroundPositionY()
{
  const nsStyleImageLayers& layers = StyleBackground()->mImage;
  return DoGetImageLayerPositionY(layers);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBackgroundRepeat()
{
  const nsStyleImageLayers& layers = StyleBackground()->mImage;
  return DoGetImageLayerRepeat(layers);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBackgroundSize()
{
  const nsStyleImageLayers& layers = StyleBackground()->mImage;
  return DoGetImageLayerSize(layers);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetGridTemplateAreas()
{
  const css::GridTemplateAreasValue* areas =
    StylePosition()->mGridTemplateAreas;
  if (!areas) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_none);
    return val.forget();
  }

  MOZ_ASSERT(!areas->mTemplates.IsEmpty(),
             "Unexpected empty array in GridTemplateAreasValue");
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);
  for (uint32_t i = 0; i < areas->mTemplates.Length(); i++) {
    nsAutoString str;
    nsStyleUtil::AppendEscapedCSSString(areas->mTemplates[i], str);
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    val->SetString(str);
    valueList->AppendCSSValue(val.forget());
  }
  return valueList.forget();
}

void
nsComputedDOMStyle::AppendGridLineNames(nsString& aResult,
                                        const nsTArray<nsString>& aLineNames)
{
  uint32_t numLines = aLineNames.Length();
  if (numLines == 0) {
    return;
  }
  for (uint32_t i = 0;;) {
    nsStyleUtil::AppendEscapedCSSIdent(aLineNames[i], aResult);
    if (++i == numLines) {
      break;
    }
    aResult.Append(' ');
  }
}

void
nsComputedDOMStyle::AppendGridLineNames(nsDOMCSSValueList* aValueList,
                                        const nsTArray<nsString>& aLineNames,
                                        bool aSuppressEmptyList)
{
  if (aLineNames.IsEmpty() && aSuppressEmptyList) {
    return;
  }
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  nsAutoString lineNamesString;
  lineNamesString.Assign('[');
  AppendGridLineNames(lineNamesString, aLineNames);
  lineNamesString.Append(']');
  val->SetString(lineNamesString);
  aValueList->AppendCSSValue(val.forget());
}

void
nsComputedDOMStyle::AppendGridLineNames(nsDOMCSSValueList* aValueList,
                                        const nsTArray<nsString>& aLineNames1,
                                        const nsTArray<nsString>& aLineNames2)
{
  if (aLineNames1.IsEmpty() && aLineNames2.IsEmpty()) {
    return;
  }
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  nsAutoString lineNamesString;
  lineNamesString.Assign('[');
  if (!aLineNames1.IsEmpty()) {
    AppendGridLineNames(lineNamesString, aLineNames1);
  }
  if (!aLineNames2.IsEmpty()) {
    if (!aLineNames1.IsEmpty()) {
      lineNamesString.Append(' ');
    }
    AppendGridLineNames(lineNamesString, aLineNames2);
  }
  lineNamesString.Append(']');
  val->SetString(lineNamesString);
  aValueList->AppendCSSValue(val.forget());
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetGridTrackSize(const nsStyleCoord& aMinValue,
                                     const nsStyleCoord& aMaxValue)
{
  if (aMinValue.GetUnit() == eStyleUnit_None) {
    // A fit-content() function.
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    nsAutoString argumentStr, fitContentStr;
    fitContentStr.AppendLiteral("fit-content(");
    MOZ_ASSERT(aMaxValue.IsCoordPercentCalcUnit(),
               "unexpected unit for fit-content() argument value");
    SetValueToCoord(val, aMaxValue, true);
    val->GetCssText(argumentStr);
    fitContentStr.Append(argumentStr);
    fitContentStr.Append(char16_t(')'));
    val->SetString(fitContentStr);
    return val.forget();
  }

  if (aMinValue == aMaxValue) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    SetValueToCoord(val, aMinValue, true,
                    nullptr, nsCSSProps::kGridTrackBreadthKTable);
    return val.forget();
  }

  // minmax(auto, <flex>) is equivalent to (and is our internal representation
  // of) <flex>, and both compute to <flex>
  if (aMinValue.GetUnit() == eStyleUnit_Auto &&
      aMaxValue.GetUnit() == eStyleUnit_FlexFraction) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    SetValueToCoord(val, aMaxValue, true);
    return val.forget();
  }

  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  nsAutoString argumentStr, minmaxStr;
  minmaxStr.AppendLiteral("minmax(");

  SetValueToCoord(val, aMinValue, true,
                  nullptr, nsCSSProps::kGridTrackBreadthKTable);
  val->GetCssText(argumentStr);
  minmaxStr.Append(argumentStr);

  minmaxStr.AppendLiteral(", ");

  SetValueToCoord(val, aMaxValue, true,
                  nullptr, nsCSSProps::kGridTrackBreadthKTable);
  val->GetCssText(argumentStr);
  minmaxStr.Append(argumentStr);

  minmaxStr.Append(char16_t(')'));
  val->SetString(minmaxStr);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetGridTemplateColumnsRows(
  const nsStyleGridTemplate&   aTrackList,
  const ComputedGridTrackInfo* aTrackInfo)
{
  if (aTrackList.mIsSubgrid) {
    // XXX TODO: add support for repeat(auto-fill) for 'subgrid' (bug 1234311)
    NS_ASSERTION(aTrackList.mMinTrackSizingFunctions.IsEmpty() &&
                 aTrackList.mMaxTrackSizingFunctions.IsEmpty(),
                 "Unexpected sizing functions with subgrid");
    RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);

    RefPtr<nsROCSSPrimitiveValue> subgridKeyword = new nsROCSSPrimitiveValue;
    subgridKeyword->SetIdent(eCSSKeyword_subgrid);
    valueList->AppendCSSValue(subgridKeyword.forget());

    for (uint32_t i = 0, len = aTrackList.mLineNameLists.Length(); ; ++i) {
      if (MOZ_UNLIKELY(aTrackList.IsRepeatAutoIndex(i))) {
        MOZ_ASSERT(aTrackList.mIsAutoFill, "subgrid can only have 'auto-fill'");
        MOZ_ASSERT(aTrackList.mRepeatAutoLineNameListAfter.IsEmpty(),
                   "mRepeatAutoLineNameListAfter isn't used for subgrid");
        RefPtr<nsROCSSPrimitiveValue> start = new nsROCSSPrimitiveValue;
        start->SetString(NS_LITERAL_STRING("repeat(auto-fill,"));
        valueList->AppendCSSValue(start.forget());
        AppendGridLineNames(valueList, aTrackList.mRepeatAutoLineNameListBefore,
                            /*aSuppressEmptyList*/ false);
        RefPtr<nsROCSSPrimitiveValue> end = new nsROCSSPrimitiveValue;
        end->SetString(NS_LITERAL_STRING(")"));
        valueList->AppendCSSValue(end.forget());
      }
      if (i == len) {
        break;
      }
      AppendGridLineNames(valueList, aTrackList.mLineNameLists[i],
                          /*aSuppressEmptyList*/ false);
    }
    return valueList.forget();
  }

  uint32_t numSizes = aTrackList.mMinTrackSizingFunctions.Length();
  MOZ_ASSERT(aTrackList.mMaxTrackSizingFunctions.Length() == numSizes,
             "Different number of min and max track sizing functions");
  if (aTrackInfo) {
    DebugOnly<bool> isAutoFill =
      aTrackList.HasRepeatAuto() && aTrackList.mIsAutoFill;
    DebugOnly<bool> isAutoFit =
      aTrackList.HasRepeatAuto() && !aTrackList.mIsAutoFill;
    DebugOnly<uint32_t> numExplicitTracks = aTrackInfo->mNumExplicitTracks;
    MOZ_ASSERT(numExplicitTracks == numSizes ||
               (isAutoFill && numExplicitTracks >= numSizes) ||
               (isAutoFit && numExplicitTracks + 1 >= numSizes),
               "expected all explicit tracks (or possibly one less, if there's "
               "an 'auto-fit' track, since that can collapse away)");
    numSizes = aTrackInfo->mSizes.Length();
  }

  // An empty <track-list> without repeats is represented as "none" in syntax.
  if (numSizes == 0 && !aTrackList.HasRepeatAuto()) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_none);
    return val.forget();
  }

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);
  if (aTrackInfo) {
    // We've done layout on the grid and have resolved the sizes of its tracks,
    // so we'll return those sizes here.  The grid spec says we MAY use
    // repeat(<positive-integer>, Npx) here for consecutive tracks with the same
    // size, but that doesn't seem worth doing since even for repeat(auto-*)
    // the resolved size might differ for the repeated tracks.
    const nsTArray<nscoord>& trackSizes = aTrackInfo->mSizes;
    const uint32_t numExplicitTracks = aTrackInfo->mNumExplicitTracks;
    const uint32_t numLeadingImplicitTracks = aTrackInfo->mNumLeadingImplicitTracks;
    MOZ_ASSERT(numSizes >= numLeadingImplicitTracks + numExplicitTracks);

    // Add any leading implicit tracks.
    for (uint32_t i = 0; i < numLeadingImplicitTracks; ++i) {
      RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
      val->SetAppUnits(trackSizes[i]);
      valueList->AppendCSSValue(val.forget());
    }

    // Then add any explicit tracks and removed auto-fit tracks.
    if (numExplicitTracks || aTrackList.HasRepeatAuto()) {
      int32_t endOfRepeat = 0;  // first index after any repeat() tracks
      int32_t offsetToLastRepeat = 0;
      if (aTrackList.HasRepeatAuto()) {
        // offsetToLastRepeat is -1 if all repeat(auto-fit) tracks are empty
        offsetToLastRepeat = numExplicitTracks + 1 - aTrackList.mLineNameLists.Length();
        endOfRepeat = aTrackList.mRepeatAutoIndex + offsetToLastRepeat + 1;
      }

      uint32_t repeatIndex = 0;
      uint32_t numRepeatTracks = aTrackInfo->mRemovedRepeatTracks.Length();
      enum LinePlacement { LinesPrecede, LinesFollow, LinesBetween };
      auto AppendRemovedAutoFits = [this, aTrackInfo, &valueList, aTrackList,
                                    &repeatIndex,
                                    numRepeatTracks](LinePlacement aPlacement)
      {
        // Add in removed auto-fit tracks and lines here, if necessary
        bool atLeastOneTrackReported = false;
        while (repeatIndex < numRepeatTracks &&
             aTrackInfo->mRemovedRepeatTracks[repeatIndex]) {
          if ((aPlacement == LinesPrecede) ||
              ((aPlacement == LinesBetween) && atLeastOneTrackReported)) {
            // Precede it with the lines between repeats.
            AppendGridLineNames(valueList,
                                aTrackList.mRepeatAutoLineNameListAfter,
                                aTrackList.mRepeatAutoLineNameListBefore);
          }

          // Removed 'auto-fit' tracks are reported as 0px.
          RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
          val->SetAppUnits(0);
          valueList->AppendCSSValue(val.forget());
          atLeastOneTrackReported = true;

          if (aPlacement == LinesFollow) {
            // Follow it with the lines between repeats.
            AppendGridLineNames(valueList,
                                aTrackList.mRepeatAutoLineNameListAfter,
                                aTrackList.mRepeatAutoLineNameListBefore);
          }
          repeatIndex++;
        }
        repeatIndex++;
      };

      for (int32_t i = 0;; i++) {
        if (aTrackList.HasRepeatAuto()) {
          if (i == aTrackList.mRepeatAutoIndex) {
            const nsTArray<nsString>& lineNames = aTrackList.mLineNameLists[i];
            if (i == endOfRepeat) {
              // All auto-fit tracks are empty, but we report them anyway.
              AppendGridLineNames(valueList, lineNames,
                                  aTrackList.mRepeatAutoLineNameListBefore);

              AppendRemovedAutoFits(LinesBetween);

              AppendGridLineNames(valueList,
                                  aTrackList.mRepeatAutoLineNameListAfter,
                                  aTrackList.mLineNameLists[i + 1]);
            } else {
              AppendGridLineNames(valueList, lineNames,
                                  aTrackList.mRepeatAutoLineNameListBefore);
              AppendRemovedAutoFits(LinesFollow);
            }
          } else if (i == endOfRepeat) {
            // Before appending the last line, finish off any removed auto-fits.
            AppendRemovedAutoFits(LinesPrecede);

            const nsTArray<nsString>& lineNames =
              aTrackList.mLineNameLists[aTrackList.mRepeatAutoIndex + 1];
            AppendGridLineNames(valueList,
                                aTrackList.mRepeatAutoLineNameListAfter,
                                lineNames);
          } else if (i > aTrackList.mRepeatAutoIndex && i < endOfRepeat) {
            AppendGridLineNames(valueList,
                                aTrackList.mRepeatAutoLineNameListAfter,
                                aTrackList.mRepeatAutoLineNameListBefore);
            AppendRemovedAutoFits(LinesFollow);
          } else {
            uint32_t j = i > endOfRepeat ? i - offsetToLastRepeat : i;
            const nsTArray<nsString>& lineNames = aTrackList.mLineNameLists[j];
            AppendGridLineNames(valueList, lineNames);
          }
        } else {
          const nsTArray<nsString>& lineNames = aTrackList.mLineNameLists[i];
          AppendGridLineNames(valueList, lineNames);
        }
        if (uint32_t(i) == numExplicitTracks) {
          break;
        }
        RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
        val->SetAppUnits(trackSizes[i + numLeadingImplicitTracks]);
        valueList->AppendCSSValue(val.forget());
      }
    }

    // Add any trailing implicit tracks.
    for (uint32_t i = numLeadingImplicitTracks + numExplicitTracks;
         i < numSizes; ++i) {
      RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
      val->SetAppUnits(trackSizes[i]);
      valueList->AppendCSSValue(val.forget());
    }
  } else {
    // We don't have a frame.  So, we'll just return a serialization of
    // the tracks from the style (without resolved sizes).
    for (uint32_t i = 0;; i++) {
      const nsTArray<nsString>& lineNames = aTrackList.mLineNameLists[i];
      if (!lineNames.IsEmpty()) {
        AppendGridLineNames(valueList, lineNames);
      }
      if (i == numSizes) {
        break;
      }
      if (MOZ_UNLIKELY(aTrackList.IsRepeatAutoIndex(i))) {
        RefPtr<nsROCSSPrimitiveValue> start = new nsROCSSPrimitiveValue;
        start->SetString(aTrackList.mIsAutoFill ? NS_LITERAL_STRING("repeat(auto-fill,")
                                                : NS_LITERAL_STRING("repeat(auto-fit,"));
        valueList->AppendCSSValue(start.forget());
        if (!aTrackList.mRepeatAutoLineNameListBefore.IsEmpty()) {
          AppendGridLineNames(valueList, aTrackList.mRepeatAutoLineNameListBefore);
        }

        valueList->AppendCSSValue(
          GetGridTrackSize(aTrackList.mMinTrackSizingFunctions[i],
                           aTrackList.mMaxTrackSizingFunctions[i]));
        if (!aTrackList.mRepeatAutoLineNameListAfter.IsEmpty()) {
          AppendGridLineNames(valueList, aTrackList.mRepeatAutoLineNameListAfter);
        }
        RefPtr<nsROCSSPrimitiveValue> end = new nsROCSSPrimitiveValue;
        end->SetString(NS_LITERAL_STRING(")"));
        valueList->AppendCSSValue(end.forget());
      } else {
        valueList->AppendCSSValue(
          GetGridTrackSize(aTrackList.mMinTrackSizingFunctions[i],
                           aTrackList.mMaxTrackSizingFunctions[i]));
      }
    }
  }

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetGridAutoFlow()
{
  nsAutoString str;
  nsStyleUtil::AppendBitmaskCSSValue(eCSSProperty_grid_auto_flow,
                                     StylePosition()->mGridAutoFlow,
                                     NS_STYLE_GRID_AUTO_FLOW_ROW,
                                     NS_STYLE_GRID_AUTO_FLOW_DENSE,
                                     str);
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetString(str);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetGridAutoColumns()
{
  return GetGridTrackSize(StylePosition()->mGridAutoColumnsMin,
                          StylePosition()->mGridAutoColumnsMax);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetGridAutoRows()
{
  return GetGridTrackSize(StylePosition()->mGridAutoRowsMin,
                          StylePosition()->mGridAutoRowsMax);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetGridTemplateColumns()
{
  const ComputedGridTrackInfo* info = nullptr;

  nsGridContainerFrame* gridFrame =
    nsGridContainerFrame::GetGridFrameWithComputedInfo(
      mContent->GetPrimaryFrame());

  if (gridFrame) {
    info = gridFrame->GetComputedTemplateColumns();
  }

  return GetGridTemplateColumnsRows(StylePosition()->mGridTemplateColumns, info);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetGridTemplateRows()
{
  const ComputedGridTrackInfo* info = nullptr;

  nsGridContainerFrame* gridFrame =
    nsGridContainerFrame::GetGridFrameWithComputedInfo(
      mContent->GetPrimaryFrame());

  if (gridFrame) {
    info = gridFrame->GetComputedTemplateRows();
  }

  return GetGridTemplateColumnsRows(StylePosition()->mGridTemplateRows, info);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetGridLine(const nsStyleGridLine& aGridLine)
{
  if (aGridLine.IsAuto()) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_auto);
    return val.forget();
  }

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);

  if (aGridLine.mHasSpan) {
    RefPtr<nsROCSSPrimitiveValue> span = new nsROCSSPrimitiveValue;
    span->SetIdent(eCSSKeyword_span);
    valueList->AppendCSSValue(span.forget());
  }

  if (aGridLine.mInteger != 0) {
    RefPtr<nsROCSSPrimitiveValue> integer = new nsROCSSPrimitiveValue;
    integer->SetNumber(aGridLine.mInteger);
    valueList->AppendCSSValue(integer.forget());
  }

  if (!aGridLine.mLineName.IsEmpty()) {
    RefPtr<nsROCSSPrimitiveValue> lineName = new nsROCSSPrimitiveValue;
    nsString escapedLineName;
    nsStyleUtil::AppendEscapedCSSIdent(aGridLine.mLineName, escapedLineName);
    lineName->SetString(escapedLineName);
    valueList->AppendCSSValue(lineName.forget());
  }

  NS_ASSERTION(valueList->Length() > 0,
               "Should have appended at least one value");
  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetGridColumnStart()
{
  return GetGridLine(StylePosition()->mGridColumnStart);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetGridColumnEnd()
{
  return GetGridLine(StylePosition()->mGridColumnEnd);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetGridRowStart()
{
  return GetGridLine(StylePosition()->mGridRowStart);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetGridRowEnd()
{
  return GetGridLine(StylePosition()->mGridRowEnd);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetGridColumnGap()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StylePosition()->mGridColumnGap, true);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetGridRowGap()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StylePosition()->mGridRowGap, true);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetPaddingTop()
{
  return GetPaddingWidthFor(eSideTop);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetPaddingBottom()
{
  return GetPaddingWidthFor(eSideBottom);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetPaddingLeft()
{
  return GetPaddingWidthFor(eSideLeft);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetPaddingRight()
{
  return GetPaddingWidthFor(eSideRight);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderCollapse()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleTableBorder()->mBorderCollapse,
                                   nsCSSProps::kBorderCollapseKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderSpacing()
{
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);

  RefPtr<nsROCSSPrimitiveValue> xSpacing = new nsROCSSPrimitiveValue;
  RefPtr<nsROCSSPrimitiveValue> ySpacing = new nsROCSSPrimitiveValue;

  const nsStyleTableBorder *border = StyleTableBorder();
  xSpacing->SetAppUnits(border->mBorderSpacingCol);
  ySpacing->SetAppUnits(border->mBorderSpacingRow);

  valueList->AppendCSSValue(xSpacing.forget());
  valueList->AppendCSSValue(ySpacing.forget());

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetCaptionSide()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleTableBorder()->mCaptionSide,
                                   nsCSSProps::kCaptionSideKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetEmptyCells()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleTableBorder()->mEmptyCells,
                                   nsCSSProps::kEmptyCellsKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTableLayout()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleTable()->mLayoutStrategy,
                                   nsCSSProps::kTableLayoutKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderTopStyle()
{
  return GetBorderStyleFor(eSideTop);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderBottomStyle()
{
  return GetBorderStyleFor(eSideBottom);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderLeftStyle()
{
  return GetBorderStyleFor(eSideLeft);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderRightStyle()
{
  return GetBorderStyleFor(eSideRight);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderBottomColors()
{
  return GetBorderColorsFor(eSideBottom);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderLeftColors()
{
  return GetBorderColorsFor(eSideLeft);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderRightColors()
{
  return GetBorderColorsFor(eSideRight);
}


already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderTopColors()
{
  return GetBorderColorsFor(eSideTop);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderBottomLeftRadius()
{
  return GetEllipseRadii(StyleBorder()->mBorderRadius,
                         eCornerBottomLeft);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderBottomRightRadius()
{
  return GetEllipseRadii(StyleBorder()->mBorderRadius,
                         eCornerBottomRight);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderTopLeftRadius()
{
  return GetEllipseRadii(StyleBorder()->mBorderRadius,
                         eCornerTopLeft);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderTopRightRadius()
{
  return GetEllipseRadii(StyleBorder()->mBorderRadius,
                         eCornerTopRight);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderTopWidth()
{
  return GetBorderWidthFor(eSideTop);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderBottomWidth()
{
  return GetBorderWidthFor(eSideBottom);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderLeftWidth()
{
  return GetBorderWidthFor(eSideLeft);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderRightWidth()
{
  return GetBorderWidthFor(eSideRight);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderTopColor()
{
  return GetBorderColorFor(eSideTop);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderBottomColor()
{
  return GetBorderColorFor(eSideBottom);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderLeftColor()
{
  return GetBorderColorFor(eSideLeft);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderRightColor()
{
  return GetBorderColorFor(eSideRight);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMarginTopWidth()
{
  return GetMarginWidthFor(eSideTop);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMarginBottomWidth()
{
  return GetMarginWidthFor(eSideBottom);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMarginLeftWidth()
{
  return GetMarginWidthFor(eSideLeft);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMarginRightWidth()
{
  return GetMarginWidthFor(eSideRight);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetOrient()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mOrient,
                                   nsCSSProps::kOrientKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetScrollBehavior()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mScrollBehavior,
                                   nsCSSProps::kScrollBehaviorKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetScrollSnapType()
{
  const nsStyleDisplay* display = StyleDisplay();
  if (display->mScrollSnapTypeX != display->mScrollSnapTypeY) {
    // No value to return.  We can't express this combination of
    // values as a shorthand.
    return nullptr;
  }
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mScrollSnapTypeX,
                                   nsCSSProps::kScrollSnapTypeKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetScrollSnapTypeX()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mScrollSnapTypeX,
                                   nsCSSProps::kScrollSnapTypeKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetScrollSnapTypeY()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mScrollSnapTypeY,
                                   nsCSSProps::kScrollSnapTypeKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetScrollSnapPoints(const nsStyleCoord& aCoord)
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  if (aCoord.GetUnit() == eStyleUnit_None) {
    val->SetIdent(eCSSKeyword_none);
  } else {
    nsAutoString argumentString;
    SetCssTextToCoord(argumentString, aCoord);
    nsAutoString tmp;
    tmp.AppendLiteral("repeat(");
    tmp.Append(argumentString);
    tmp.Append(')');
    val->SetString(tmp);
  }
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetScrollSnapPointsX()
{
  return GetScrollSnapPoints(StyleDisplay()->mScrollSnapPointsX);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetScrollSnapPointsY()
{
  return GetScrollSnapPoints(StyleDisplay()->mScrollSnapPointsY);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetScrollSnapDestination()
{
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);
  SetValueToPosition(StyleDisplay()->mScrollSnapDestination, valueList);
  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetScrollSnapCoordinate()
{
  const nsStyleDisplay* sd = StyleDisplay();
  if (sd->mScrollSnapCoordinate.IsEmpty()) {
    // Having no snap coordinates is interpreted as "none"
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_none);
    return val.forget();
  } else {
    RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);
    for (size_t i = 0, i_end = sd->mScrollSnapCoordinate.Length(); i < i_end; ++i) {
      RefPtr<nsDOMCSSValueList> itemList = GetROCSSValueList(false);
      SetValueToPosition(sd->mScrollSnapCoordinate[i], itemList);
      valueList->AppendCSSValue(itemList.forget());
    }
    return valueList.forget();
  }
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetOutlineWidth()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  const nsStyleOutline* outline = StyleOutline();

  nscoord width;
  if (outline->mOutlineStyle == NS_STYLE_BORDER_STYLE_NONE) {
    NS_ASSERTION(outline->GetOutlineWidth() == 0, "unexpected width");
    width = 0;
  } else {
    width = outline->GetOutlineWidth();
  }
  val->SetAppUnits(width);

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetOutlineStyle()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleOutline()->mOutlineStyle,
                                   nsCSSProps::kOutlineStyleKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetOutlineOffset()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetAppUnits(StyleOutline()->mOutlineOffset);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetOutlineRadiusBottomLeft()
{
  return GetEllipseRadii(StyleOutline()->mOutlineRadius,
                         eCornerBottomLeft);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetOutlineRadiusBottomRight()
{
  return GetEllipseRadii(StyleOutline()->mOutlineRadius,
                         eCornerBottomRight);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetOutlineRadiusTopLeft()
{
  return GetEllipseRadii(StyleOutline()->mOutlineRadius,
                         eCornerTopLeft);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetOutlineRadiusTopRight()
{
  return GetEllipseRadii(StyleOutline()->mOutlineRadius,
                         eCornerTopRight);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetOutlineColor()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueFromComplexColor(val, StyleOutline()->mOutlineColor);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetEllipseRadii(const nsStyleCorners& aRadius,
                                    Corner aFullCorner)
{
  nsStyleCoord radiusX = aRadius.Get(FullToHalfCorner(aFullCorner, false));
  nsStyleCoord radiusY = aRadius.Get(FullToHalfCorner(aFullCorner, true));

  // for compatibility, return a single value if X and Y are equal
  if (radiusX == radiusY) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    SetValueToCoord(val, radiusX, true);
    return val.forget();
  }

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);

  RefPtr<nsROCSSPrimitiveValue> valX = new nsROCSSPrimitiveValue;
  RefPtr<nsROCSSPrimitiveValue> valY = new nsROCSSPrimitiveValue;

  SetValueToCoord(valX, radiusX, true);
  SetValueToCoord(valY, radiusY, true);

  valueList->AppendCSSValue(valX.forget());
  valueList->AppendCSSValue(valY.forget());

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetCSSShadowArray(nsCSSShadowArray* aArray,
                                      const nscolor& aDefaultColor,
                                      bool aIsBoxShadow)
{
  if (!aArray) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_none);
    return val.forget();
  }

  static nscoord nsCSSShadowItem::* const shadowValuesNoSpread[] = {
    &nsCSSShadowItem::mXOffset,
    &nsCSSShadowItem::mYOffset,
    &nsCSSShadowItem::mRadius
  };

  static nscoord nsCSSShadowItem::* const shadowValuesWithSpread[] = {
    &nsCSSShadowItem::mXOffset,
    &nsCSSShadowItem::mYOffset,
    &nsCSSShadowItem::mRadius,
    &nsCSSShadowItem::mSpread
  };

  nscoord nsCSSShadowItem::* const * shadowValues;
  uint32_t shadowValuesLength;
  if (aIsBoxShadow) {
    shadowValues = shadowValuesWithSpread;
    shadowValuesLength = ArrayLength(shadowValuesWithSpread);
  } else {
    shadowValues = shadowValuesNoSpread;
    shadowValuesLength = ArrayLength(shadowValuesNoSpread);
  }

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  for (nsCSSShadowItem *item = aArray->ShadowAt(0),
                   *item_end = item + aArray->Length();
       item < item_end; ++item) {
    RefPtr<nsDOMCSSValueList> itemList = GetROCSSValueList(false);

    // Color is either the specified shadow color or the foreground color
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    nscolor shadowColor;
    if (item->mHasColor) {
      shadowColor = item->mColor;
    } else {
      shadowColor = aDefaultColor;
    }
    SetToRGBAColor(val, shadowColor);
    itemList->AppendCSSValue(val.forget());

    // Set the offsets, blur radius, and spread if available
    for (uint32_t i = 0; i < shadowValuesLength; ++i) {
      val = new nsROCSSPrimitiveValue;
      val->SetAppUnits(item->*(shadowValues[i]));
      itemList->AppendCSSValue(val.forget());
    }

    if (item->mInset && aIsBoxShadow) {
      // This is an inset box-shadow
      val = new nsROCSSPrimitiveValue;
      val->SetIdent(
        nsCSSProps::ValueToKeywordEnum(
            uint8_t(StyleBoxShadowType::Inset),
            nsCSSProps::kBoxShadowTypeKTable));
      itemList->AppendCSSValue(val.forget());
    }
    valueList->AppendCSSValue(itemList.forget());
  }

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBoxDecorationBreak()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleBorder()->mBoxDecorationBreak,
                                   nsCSSProps::kBoxDecorationBreakKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBoxShadow()
{
  return GetCSSShadowArray(StyleEffects()->mBoxShadow,
                           StyleColor()->mColor,
                           true);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetZIndex()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StylePosition()->mZIndex, false);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetListStyleImage()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  nsCOMPtr<nsIURI> uri = StyleList()->GetListStyleImageURI();
  if (!uri) {
    val->SetIdent(eCSSKeyword_none);
  } else {
    val->SetURI(uri);
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetListStylePosition()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleList()->mListStylePosition,
                                   nsCSSProps::kListStylePositionKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetListStyleType()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  CounterStyle* style = StyleList()->GetCounterStyle();
  AnonymousCounterStyle* anonymous = style->AsAnonymous();
  nsAutoString tmp;
  if (!anonymous) {
    // want SetIdent
    nsString type;
    StyleList()->GetListStyleType(type);
    nsStyleUtil::AppendEscapedCSSIdent(type, tmp);
  } else if (anonymous->IsSingleString()) {
    const nsTArray<nsString>& symbols = anonymous->GetSymbols();
    MOZ_ASSERT(symbols.Length() == 1);
    nsStyleUtil::AppendEscapedCSSString(symbols[0], tmp);
  } else {
    tmp.AppendLiteral("symbols(");

    uint8_t system = anonymous->GetSystem();
    NS_ASSERTION(system == NS_STYLE_COUNTER_SYSTEM_CYCLIC ||
                 system == NS_STYLE_COUNTER_SYSTEM_NUMERIC ||
                 system == NS_STYLE_COUNTER_SYSTEM_ALPHABETIC ||
                 system == NS_STYLE_COUNTER_SYSTEM_SYMBOLIC ||
                 system == NS_STYLE_COUNTER_SYSTEM_FIXED,
                 "Invalid system for anonymous counter style.");
    if (system != NS_STYLE_COUNTER_SYSTEM_SYMBOLIC) {
      AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(
              system, nsCSSProps::kCounterSystemKTable), tmp);
      tmp.Append(' ');
    }

    const nsTArray<nsString>& symbols = anonymous->GetSymbols();
    NS_ASSERTION(symbols.Length() > 0,
                 "No symbols in the anonymous counter style");
    for (size_t i = 0, iend = symbols.Length(); i < iend; i++) {
      nsStyleUtil::AppendEscapedCSSString(symbols[i], tmp);
      tmp.Append(' ');
    }
    tmp.Replace(tmp.Length() - 1, 1, char16_t(')'));
  }
  val->SetString(tmp);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetImageRegion()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  const nsStyleList* list = StyleList();

  if (list->mImageRegion.width <= 0 || list->mImageRegion.height <= 0) {
    val->SetIdent(eCSSKeyword_auto);
  } else {
    // create the cssvalues for the sides, stick them in the rect object
    nsROCSSPrimitiveValue *topVal    = new nsROCSSPrimitiveValue;
    nsROCSSPrimitiveValue *rightVal  = new nsROCSSPrimitiveValue;
    nsROCSSPrimitiveValue *bottomVal = new nsROCSSPrimitiveValue;
    nsROCSSPrimitiveValue *leftVal   = new nsROCSSPrimitiveValue;
    nsDOMCSSRect * domRect = new nsDOMCSSRect(topVal, rightVal,
                                              bottomVal, leftVal);
    topVal->SetAppUnits(list->mImageRegion.y);
    rightVal->SetAppUnits(list->mImageRegion.width + list->mImageRegion.x);
    bottomVal->SetAppUnits(list->mImageRegion.height + list->mImageRegion.y);
    leftVal->SetAppUnits(list->mImageRegion.x);
    val->SetRect(domRect);
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetInitialLetter()
{
  const nsStyleTextReset* textReset = StyleTextReset();
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  if (textReset->mInitialLetterSink == 0) {
    val->SetIdent(eCSSKeyword_normal);
    return val.forget();
  } else {
    RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);
    val->SetNumber(textReset->mInitialLetterSize);
    valueList->AppendCSSValue(val.forget());
    RefPtr<nsROCSSPrimitiveValue> second = new nsROCSSPrimitiveValue;
    second->SetNumber(textReset->mInitialLetterSink);
    valueList->AppendCSSValue(second.forget());
    return valueList.forget();
  }
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetLineHeight()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  nscoord lineHeight;
  if (GetLineHeightCoord(lineHeight)) {
    val->SetAppUnits(lineHeight);
  } else {
    SetValueToCoord(val, StyleText()->mLineHeight, true,
                    nullptr, nsCSSProps::kLineHeightKTable);
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetRubyAlign()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(
    StyleText()->mRubyAlign, nsCSSProps::kRubyAlignKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetRubyPosition()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(
    StyleText()->mRubyPosition, nsCSSProps::kRubyPositionKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetVerticalAlign()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StyleDisplay()->mVerticalAlign, false,
                  nullptr, nsCSSProps::kVerticalAlignKTable);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::CreateTextAlignValue(uint8_t aAlign, bool aAlignTrue,
                                         const KTableEntry aTable[])
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(aAlign, aTable));
  if (!aAlignTrue) {
    return val.forget();
  }

  RefPtr<nsROCSSPrimitiveValue> first = new nsROCSSPrimitiveValue;
  first->SetIdent(eCSSKeyword_unsafe);

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);
  valueList->AppendCSSValue(first.forget());
  valueList->AppendCSSValue(val.forget());
  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTextAlign()
{
  const nsStyleText* style = StyleText();
  return CreateTextAlignValue(style->mTextAlign, style->mTextAlignTrue,
                              nsCSSProps::kTextAlignKTable);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTextAlignLast()
{
  const nsStyleText* style = StyleText();
  return CreateTextAlignValue(style->mTextAlignLast, style->mTextAlignLastTrue,
                              nsCSSProps::kTextAlignLastKTable);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTextCombineUpright()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  uint8_t tch = StyleText()->mTextCombineUpright;

  if (tch <= NS_STYLE_TEXT_COMBINE_UPRIGHT_ALL) {
    val->SetIdent(
      nsCSSProps::ValueToKeywordEnum(tch,
                                     nsCSSProps::kTextCombineUprightKTable));
  } else if (tch <= NS_STYLE_TEXT_COMBINE_UPRIGHT_DIGITS_2) {
    val->SetString(NS_LITERAL_STRING("digits 2"));
  } else if (tch <= NS_STYLE_TEXT_COMBINE_UPRIGHT_DIGITS_3) {
    val->SetString(NS_LITERAL_STRING("digits 3"));
  } else {
    val->SetString(NS_LITERAL_STRING("digits 4"));
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTextDecoration()
{
  const nsStyleTextReset* textReset = StyleTextReset();

  bool isInitialStyle =
    textReset->mTextDecorationStyle == NS_STYLE_TEXT_DECORATION_STYLE_SOLID;
  StyleComplexColor color = textReset->mTextDecorationColor;

  if (isInitialStyle && color.IsCurrentColor()) {
    return DoGetTextDecorationLine();
  }

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);

  valueList->AppendCSSValue(DoGetTextDecorationLine());
  if (!isInitialStyle) {
    valueList->AppendCSSValue(DoGetTextDecorationStyle());
  }
  if (!color.IsCurrentColor()) {
    valueList->AppendCSSValue(DoGetTextDecorationColor());
  }

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTextDecorationColor()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueFromComplexColor(val, StyleTextReset()->mTextDecorationColor);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTextDecorationLine()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  int32_t intValue = StyleTextReset()->mTextDecorationLine;

  if (NS_STYLE_TEXT_DECORATION_LINE_NONE == intValue) {
    val->SetIdent(eCSSKeyword_none);
  } else {
    nsAutoString decorationLineString;
    // Clear the OVERRIDE_ALL bits -- we don't want these to appear in
    // the computed style.
    intValue &= ~NS_STYLE_TEXT_DECORATION_LINE_OVERRIDE_ALL;
    nsStyleUtil::AppendBitmaskCSSValue(eCSSProperty_text_decoration_line,
      intValue, NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE,
      NS_STYLE_TEXT_DECORATION_LINE_BLINK, decorationLineString);
    val->SetString(decorationLineString);
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTextDecorationStyle()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleTextReset()->mTextDecorationStyle,
                                   nsCSSProps::kTextDecorationStyleKTable));

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTextEmphasisColor()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueFromComplexColor(val, StyleText()->mTextEmphasisColor);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTextEmphasisPosition()
{
  auto position = StyleText()->mTextEmphasisPosition;

  MOZ_ASSERT(!(position & NS_STYLE_TEXT_EMPHASIS_POSITION_OVER) !=
             !(position & NS_STYLE_TEXT_EMPHASIS_POSITION_UNDER));
  RefPtr<nsROCSSPrimitiveValue> first = new nsROCSSPrimitiveValue;
  first->SetIdent((position & NS_STYLE_TEXT_EMPHASIS_POSITION_OVER) ?
                  eCSSKeyword_over : eCSSKeyword_under);

  MOZ_ASSERT(!(position & NS_STYLE_TEXT_EMPHASIS_POSITION_LEFT) !=
             !(position & NS_STYLE_TEXT_EMPHASIS_POSITION_RIGHT));
  RefPtr<nsROCSSPrimitiveValue> second = new nsROCSSPrimitiveValue;
  second->SetIdent((position & NS_STYLE_TEXT_EMPHASIS_POSITION_LEFT) ?
                   eCSSKeyword_left : eCSSKeyword_right);

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);
  valueList->AppendCSSValue(first.forget());
  valueList->AppendCSSValue(second.forget());
  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTextEmphasisStyle()
{
  auto style = StyleText()->mTextEmphasisStyle;
  if (style == NS_STYLE_TEXT_EMPHASIS_STYLE_NONE) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_none);
    return val.forget();
  }
  if (style == NS_STYLE_TEXT_EMPHASIS_STYLE_STRING) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    nsAutoString tmp;
    nsStyleUtil::AppendEscapedCSSString(
      StyleText()->mTextEmphasisStyleString, tmp);
    val->SetString(tmp);
    return val.forget();
  }

  RefPtr<nsROCSSPrimitiveValue> fillVal = new nsROCSSPrimitiveValue;
  if ((style & NS_STYLE_TEXT_EMPHASIS_STYLE_FILL_MASK) ==
      NS_STYLE_TEXT_EMPHASIS_STYLE_FILLED) {
    fillVal->SetIdent(eCSSKeyword_filled);
  } else {
    MOZ_ASSERT((style & NS_STYLE_TEXT_EMPHASIS_STYLE_FILL_MASK) ==
               NS_STYLE_TEXT_EMPHASIS_STYLE_OPEN);
    fillVal->SetIdent(eCSSKeyword_open);
  }

  RefPtr<nsROCSSPrimitiveValue> shapeVal = new nsROCSSPrimitiveValue;
  shapeVal->SetIdent(nsCSSProps::ValueToKeywordEnum(
    style & NS_STYLE_TEXT_EMPHASIS_STYLE_SHAPE_MASK,
    nsCSSProps::kTextEmphasisStyleShapeKTable));

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);
  valueList->AppendCSSValue(fillVal.forget());
  valueList->AppendCSSValue(shapeVal.forget());
  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTextIndent()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StyleText()->mTextIndent, false);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTextJustify()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleText()->mTextJustify,
                                   nsCSSProps::kTextJustifyKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTextOrientation()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleVisibility()->mTextOrientation,
                                   nsCSSProps::kTextOrientationKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTextOverflow()
{
  const nsStyleTextReset *style = StyleTextReset();
  RefPtr<nsROCSSPrimitiveValue> first = new nsROCSSPrimitiveValue;
  const nsStyleTextOverflowSide *side = style->mTextOverflow.GetFirstValue();
  if (side->mType == NS_STYLE_TEXT_OVERFLOW_STRING) {
    nsAutoString str;
    nsStyleUtil::AppendEscapedCSSString(side->mString, str);
    first->SetString(str);
  } else {
    first->SetIdent(
      nsCSSProps::ValueToKeywordEnum(side->mType,
                                     nsCSSProps::kTextOverflowKTable));
  }
  side = style->mTextOverflow.GetSecondValue();
  if (!side) {
    return first.forget();
  }
  RefPtr<nsROCSSPrimitiveValue> second = new nsROCSSPrimitiveValue;
  if (side->mType == NS_STYLE_TEXT_OVERFLOW_STRING) {
    nsAutoString str;
    nsStyleUtil::AppendEscapedCSSString(side->mString, str);
    second->SetString(str);
  } else {
    second->SetIdent(
      nsCSSProps::ValueToKeywordEnum(side->mType,
                                     nsCSSProps::kTextOverflowKTable));
  }

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);
  valueList->AppendCSSValue(first.forget());
  valueList->AppendCSSValue(second.forget());
  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTextShadow()
{
  return GetCSSShadowArray(StyleText()->mTextShadow,
                           StyleColor()->mColor,
                           false);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTextSizeAdjust()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleText()->mTextSizeAdjust,
                                               nsCSSProps::kTextSizeAdjustKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTextTransform()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleText()->mTextTransform,
                                   nsCSSProps::kTextTransformKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTabSize()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StyleText()->mTabSize, true);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetLetterSpacing()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StyleText()->mLetterSpacing, false);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetWordSpacing()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StyleText()->mWordSpacing, false);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetWhiteSpace()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleText()->mWhiteSpace,
                                   nsCSSProps::kWhitespaceKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetWindowDragging()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleUIReset()->mWindowDragging,
                                   nsCSSProps::kWindowDraggingKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetWindowShadow()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleUIReset()->mWindowShadow,
                                   nsCSSProps::kWindowShadowKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetWordBreak()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleText()->mWordBreak,
                                   nsCSSProps::kWordBreakKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetOverflowWrap()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleText()->mOverflowWrap,
                                   nsCSSProps::kOverflowWrapKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetHyphens()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleText()->mHyphens,
                                   nsCSSProps::kHyphensKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetWebkitTextFillColor()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueFromComplexColor(val, StyleText()->mWebkitTextFillColor);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetWebkitTextStrokeColor()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueFromComplexColor(val, StyleText()->mWebkitTextStrokeColor);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetWebkitTextStrokeWidth()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetAppUnits(StyleText()->mWebkitTextStrokeWidth);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetPointerEvents()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleUserInterface()->mPointerEvents,
                                   nsCSSProps::kPointerEventsKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetVisibility()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleVisibility()->mVisible,
                                               nsCSSProps::kVisibilityKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetWritingMode()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleVisibility()->mWritingMode,
                                   nsCSSProps::kWritingModeKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetDirection()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleVisibility()->mDirection,
                                   nsCSSProps::kDirectionKTable));
  return val.forget();
}

static_assert(NS_STYLE_UNICODE_BIDI_NORMAL == 0,
              "unicode-bidi style constants not as expected");

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetUnicodeBidi()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleTextReset()->mUnicodeBidi,
                                   nsCSSProps::kUnicodeBidiKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetCaretColor()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueFromComplexColor(val, StyleUserInterface()->mCaretColor);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetCursor()
{
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  const nsStyleUserInterface *ui = StyleUserInterface();

  for (const nsCursorImage& item : ui->mCursorImages) {
    RefPtr<nsDOMCSSValueList> itemList = GetROCSSValueList(false);

    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    SetValueToURLValue(item.mImage->GetImageValue(), val);
    itemList->AppendCSSValue(val.forget());

    if (item.mHaveHotspot) {
      RefPtr<nsROCSSPrimitiveValue> valX = new nsROCSSPrimitiveValue;
      RefPtr<nsROCSSPrimitiveValue> valY = new nsROCSSPrimitiveValue;

      valX->SetNumber(item.mHotspotX);
      valY->SetNumber(item.mHotspotY);

      itemList->AppendCSSValue(valX.forget());
      itemList->AppendCSSValue(valY.forget());
    }
    valueList->AppendCSSValue(itemList.forget());
  }

  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(ui->mCursor,
                                               nsCSSProps::kCursorKTable));
  valueList->AppendCSSValue(val.forget());
  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetAppearance()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mAppearance,
                                               nsCSSProps::kAppearanceKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMozAppearance()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mMozAppearance,
                                               nsCSSProps::kMozAppearanceKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBoxAlign()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleXUL()->mBoxAlign,
                                               nsCSSProps::kBoxAlignKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBoxDirection()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleXUL()->mBoxDirection,
                                   nsCSSProps::kBoxDirectionKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBoxFlex()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleXUL()->mBoxFlex);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBoxOrdinalGroup()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleXUL()->mBoxOrdinal);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBoxOrient()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleXUL()->mBoxOrient,
                                   nsCSSProps::kBoxOrientKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBoxPack()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleXUL()->mBoxPack,
                                               nsCSSProps::kBoxPackKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBoxSizing()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StylePosition()->mBoxSizing,
                                   nsCSSProps::kBoxSizingKTable));
  return val.forget();
}

/* Border image properties */

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderImageSource()
{
  const nsStyleBorder* border = StyleBorder();

  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  const nsStyleImage& image = border->mBorderImageSource;
  SetValueToStyleImage(image, val);

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderImageSlice()
{
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);

  const nsStyleBorder* border = StyleBorder();
  // Four slice numbers.
  NS_FOR_CSS_SIDES (side) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    SetValueToCoord(val, border->mBorderImageSlice.Get(side), true, nullptr);
    valueList->AppendCSSValue(val.forget());
  }

  // Fill keyword.
  if (NS_STYLE_BORDER_IMAGE_SLICE_FILL == border->mBorderImageFill) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_fill);
    valueList->AppendCSSValue(val.forget());
  }

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderImageWidth()
{
  const nsStyleBorder* border = StyleBorder();
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);
  NS_FOR_CSS_SIDES (side) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    SetValueToCoord(val, border->mBorderImageWidth.Get(side),
                    true, nullptr);
    valueList->AppendCSSValue(val.forget());
  }

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderImageOutset()
{
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);

  const nsStyleBorder* border = StyleBorder();
  // four slice numbers
  NS_FOR_CSS_SIDES (side) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    SetValueToCoord(val, border->mBorderImageOutset.Get(side),
                    true, nullptr);
    valueList->AppendCSSValue(val.forget());
  }

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderImageRepeat()
{
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);

  const nsStyleBorder* border = StyleBorder();

  // horizontal repeat
  RefPtr<nsROCSSPrimitiveValue> valX = new nsROCSSPrimitiveValue;
  valX->SetIdent(
    nsCSSProps::ValueToKeywordEnum(border->mBorderImageRepeatH,
                                   nsCSSProps::kBorderImageRepeatKTable));
  valueList->AppendCSSValue(valX.forget());

  // vertical repeat
  RefPtr<nsROCSSPrimitiveValue> valY = new nsROCSSPrimitiveValue;
  valY->SetIdent(
    nsCSSProps::ValueToKeywordEnum(border->mBorderImageRepeatV,
                                   nsCSSProps::kBorderImageRepeatKTable));
  valueList->AppendCSSValue(valY.forget());
  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFlexBasis()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  // XXXdholbert We could make this more automagic and resolve percentages
  // if we wanted, by passing in a PercentageBaseGetter instead of nullptr
  // below.  Logic would go like this:
  //   if (i'm a flex item) {
  //     if (my flex container is horizontal) {
  //       percentageBaseGetter = &nsComputedDOMStyle::GetCBContentWidth;
  //     } else {
  //       percentageBaseGetter = &nsComputedDOMStyle::GetCBContentHeight;
  //     }
  //   }

  SetValueToCoord(val, StylePosition()->mFlexBasis, true,
                  nullptr, nsCSSProps::kWidthKTable);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFlexDirection()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StylePosition()->mFlexDirection,
                                   nsCSSProps::kFlexDirectionKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFlexGrow()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetNumber(StylePosition()->mFlexGrow);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFlexShrink()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetNumber(StylePosition()->mFlexShrink);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFlexWrap()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StylePosition()->mFlexWrap,
                                   nsCSSProps::kFlexWrapKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetOrder()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetNumber(StylePosition()->mOrder);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetAlignContent()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  nsAutoString str;
  auto align = StylePosition()->mAlignContent;
  nsCSSValue::AppendAlignJustifyValueToString(align & NS_STYLE_ALIGN_ALL_BITS, str);
  auto fallback = align >> NS_STYLE_ALIGN_ALL_SHIFT;
  if (fallback) {
    str.Append(' ');
    nsCSSValue::AppendAlignJustifyValueToString(fallback, str);
  }
  val->SetString(str);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetAlignItems()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  nsAutoString str;
  auto align = StylePosition()->mAlignItems;
  nsCSSValue::AppendAlignJustifyValueToString(align, str);
  val->SetString(str);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetAlignSelf()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  nsAutoString str;
  auto align = StylePosition()->mAlignSelf;
  nsCSSValue::AppendAlignJustifyValueToString(align, str);
  val->SetString(str);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetJustifyContent()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  nsAutoString str;
  auto justify = StylePosition()->mJustifyContent;
  nsCSSValue::AppendAlignJustifyValueToString(justify & NS_STYLE_JUSTIFY_ALL_BITS, str);
  auto fallback = justify >> NS_STYLE_JUSTIFY_ALL_SHIFT;
  if (fallback) {
    MOZ_ASSERT(nsCSSProps::ValueToKeywordEnum(fallback & ~NS_STYLE_JUSTIFY_FLAG_BITS,
                                              nsCSSProps::kAlignSelfPosition)
               != eCSSKeyword_UNKNOWN, "unknown fallback value");
    str.Append(' ');
    nsCSSValue::AppendAlignJustifyValueToString(fallback, str);
  }
  val->SetString(str);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetJustifyItems()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  nsAutoString str;
  auto justify =
    StylePosition()->ComputedJustifyItems(mStyleContext->GetParentAllowServo());
  nsCSSValue::AppendAlignJustifyValueToString(justify, str);
  val->SetString(str);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetJustifySelf()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  nsAutoString str;
  auto justify = StylePosition()->mJustifySelf;
  nsCSSValue::AppendAlignJustifyValueToString(justify, str);
  val->SetString(str);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFloatEdge()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(uint8_t(StyleBorder()->mFloatEdge),
                                   nsCSSProps::kFloatEdgeKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetForceBrokenImageIcon()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleUIReset()->mForceBrokenImageIcon);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetImageOrientation()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  nsAutoString string;
  nsStyleImageOrientation orientation = StyleVisibility()->mImageOrientation;

  if (orientation.IsFromImage()) {
    string.AppendLiteral("from-image");
  } else {
    nsStyleUtil::AppendAngleValue(orientation.AngleAsCoord(), string);

    if (orientation.IsFlipped()) {
      string.AppendLiteral(" flip");
    }
  }

  val->SetString(string);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetIMEMode()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleUIReset()->mIMEMode,
                                   nsCSSProps::kIMEModeKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetUserFocus()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(uint8_t(StyleUserInterface()->mUserFocus),
                                   nsCSSProps::kUserFocusKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetUserInput()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleUserInterface()->mUserInput,
                                   nsCSSProps::kUserInputKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetUserModify()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleUserInterface()->mUserModify,
                                   nsCSSProps::kUserModifyKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetUserSelect()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleUIReset()->mUserSelect,
                                   nsCSSProps::kUserSelectKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetDisplay()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mDisplay,
                                               nsCSSProps::kDisplayKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetContain()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  int32_t mask = StyleDisplay()->mContain;

  if (mask == 0) {
    val->SetIdent(eCSSKeyword_none);
  } else if (mask & NS_STYLE_CONTAIN_STRICT) {
    NS_ASSERTION(mask == (NS_STYLE_CONTAIN_STRICT | NS_STYLE_CONTAIN_ALL_BITS),
                 "contain: strict should imply contain: layout style paint");
    val->SetIdent(eCSSKeyword_strict);
  } else {
    nsAutoString valueStr;

    nsStyleUtil::AppendBitmaskCSSValue(eCSSProperty_contain,
                                       mask, NS_STYLE_CONTAIN_LAYOUT,
                                       NS_STYLE_CONTAIN_PAINT, valueStr);
    val->SetString(valueStr);
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetPosition()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mPosition,
                                               nsCSSProps::kPositionKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetClip()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  const nsStyleEffects* effects = StyleEffects();

  if (effects->mClipFlags == NS_STYLE_CLIP_AUTO) {
    val->SetIdent(eCSSKeyword_auto);
  } else {
    // create the cssvalues for the sides, stick them in the rect object
    nsROCSSPrimitiveValue *topVal    = new nsROCSSPrimitiveValue;
    nsROCSSPrimitiveValue *rightVal  = new nsROCSSPrimitiveValue;
    nsROCSSPrimitiveValue *bottomVal = new nsROCSSPrimitiveValue;
    nsROCSSPrimitiveValue *leftVal   = new nsROCSSPrimitiveValue;
    nsDOMCSSRect * domRect = new nsDOMCSSRect(topVal, rightVal,
                                              bottomVal, leftVal);
    if (effects->mClipFlags & NS_STYLE_CLIP_TOP_AUTO) {
      topVal->SetIdent(eCSSKeyword_auto);
    } else {
      topVal->SetAppUnits(effects->mClip.y);
    }

    if (effects->mClipFlags & NS_STYLE_CLIP_RIGHT_AUTO) {
      rightVal->SetIdent(eCSSKeyword_auto);
    } else {
      rightVal->SetAppUnits(effects->mClip.width + effects->mClip.x);
    }

    if (effects->mClipFlags & NS_STYLE_CLIP_BOTTOM_AUTO) {
      bottomVal->SetIdent(eCSSKeyword_auto);
    } else {
      bottomVal->SetAppUnits(effects->mClip.height + effects->mClip.y);
    }

    if (effects->mClipFlags & NS_STYLE_CLIP_LEFT_AUTO) {
      leftVal->SetIdent(eCSSKeyword_auto);
    } else {
      leftVal->SetAppUnits(effects->mClip.x);
    }
    val->SetRect(domRect);
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetWillChange()
{
  const nsCOMArray<nsIAtom>& willChange = StyleDisplay()->mWillChange;

  if (willChange.IsEmpty()) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_auto);
    return val.forget();
  }

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);
  for (const nsIAtom* ident : willChange) {
    RefPtr<nsROCSSPrimitiveValue> property = new nsROCSSPrimitiveValue;
    property->SetString(nsDependentAtomString(ident));
    valueList->AppendCSSValue(property.forget());
  }

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetOverflow()
{
  const nsStyleDisplay* display = StyleDisplay();

  if (display->mOverflowX != display->mOverflowY) {
    // No value to return.  We can't express this combination of
    // values as a shorthand.
    return nullptr;
  }

  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(display->mOverflowX,
                                               nsCSSProps::kOverflowKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetOverflowX()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mOverflowX,
                                   nsCSSProps::kOverflowSubKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetOverflowY()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mOverflowY,
                                   nsCSSProps::kOverflowSubKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetOverflowClipBox()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mOverflowClipBox,
                                   nsCSSProps::kOverflowClipBoxKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetResize()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mResize,
                                               nsCSSProps::kResizeKTable));
  return val.forget();
}


already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetPageBreakAfter()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  const nsStyleDisplay *display = StyleDisplay();

  if (display->mBreakAfter) {
    val->SetIdent(eCSSKeyword_always);
  } else {
    val->SetIdent(eCSSKeyword_auto);
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetPageBreakBefore()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  const nsStyleDisplay *display = StyleDisplay();

  if (display->mBreakBefore) {
    val->SetIdent(eCSSKeyword_always);
  } else {
    val->SetIdent(eCSSKeyword_auto);
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetPageBreakInside()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mBreakInside,
                                               nsCSSProps::kPageBreakInsideKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTouchAction()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  int32_t intValue = StyleDisplay()->mTouchAction;

  // None and Auto and Manipulation values aren't allowed
  // to be in conjunction with other values.
  // But there are all checks in CSSParserImpl::ParseTouchAction
  nsAutoString valueStr;
  nsStyleUtil::AppendBitmaskCSSValue(eCSSProperty_touch_action, intValue,
    NS_STYLE_TOUCH_ACTION_NONE, NS_STYLE_TOUCH_ACTION_MANIPULATION,
    valueStr);
  val->SetString(valueStr);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetHeight()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  bool calcHeight = false;

  if (mInnerFrame) {
    calcHeight = true;

    const nsStyleDisplay* displayData = StyleDisplay();
    if (displayData->mDisplay == mozilla::StyleDisplay::Inline &&
        !(mInnerFrame->IsFrameOfType(nsIFrame::eReplaced)) &&
        // An outer SVG frame should behave the same as eReplaced in this case
        mInnerFrame->GetType() != nsGkAtoms::svgOuterSVGFrame) {

      calcHeight = false;
    }
  }

  if (calcHeight) {
    AssertFlushedPendingReflows();
    nsMargin adjustedValues = GetAdjustedValuesForBoxSizing();
    val->SetAppUnits(mInnerFrame->GetContentRect().height +
      adjustedValues.TopBottom());
  } else {
    const nsStylePosition *positionData = StylePosition();

    nscoord minHeight =
      StyleCoordToNSCoord(positionData->mMinHeight,
                          &nsComputedDOMStyle::GetCBContentHeight, 0, true);

    nscoord maxHeight =
      StyleCoordToNSCoord(positionData->mMaxHeight,
                          &nsComputedDOMStyle::GetCBContentHeight,
                          nscoord_MAX, true);

    SetValueToCoord(val, positionData->mHeight, true, nullptr,
                    nsCSSProps::kWidthKTable, minHeight, maxHeight);
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetWidth()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  bool calcWidth = false;

  if (mInnerFrame) {
    calcWidth = true;

    const nsStyleDisplay *displayData = StyleDisplay();
    if (displayData->mDisplay == mozilla::StyleDisplay::Inline &&
        !(mInnerFrame->IsFrameOfType(nsIFrame::eReplaced)) &&
        // An outer SVG frame should behave the same as eReplaced in this case
        mInnerFrame->GetType() != nsGkAtoms::svgOuterSVGFrame) {

      calcWidth = false;
    }
  }

  if (calcWidth) {
    AssertFlushedPendingReflows();
    nsMargin adjustedValues = GetAdjustedValuesForBoxSizing();
    val->SetAppUnits(mInnerFrame->GetContentRect().width +
      adjustedValues.LeftRight());
  } else {
    const nsStylePosition *positionData = StylePosition();

    nscoord minWidth =
      StyleCoordToNSCoord(positionData->mMinWidth,
                          &nsComputedDOMStyle::GetCBContentWidth, 0, true);

    nscoord maxWidth =
      StyleCoordToNSCoord(positionData->mMaxWidth,
                          &nsComputedDOMStyle::GetCBContentWidth,
                          nscoord_MAX, true);

    SetValueToCoord(val, positionData->mWidth, true, nullptr,
                    nsCSSProps::kWidthKTable, minWidth, maxWidth);
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMaxHeight()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StylePosition()->mMaxHeight, true,
                  nullptr, nsCSSProps::kWidthKTable);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMaxWidth()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StylePosition()->mMaxWidth, true,
                  nullptr, nsCSSProps::kWidthKTable);
  return val.forget();
}

bool
nsComputedDOMStyle::ShouldHonorMinSizeAutoInAxis(PhysicalAxis aAxis)
{
  // A {flex,grid} item's min-{width|height} "auto" value gets special
  // treatment in getComputedStyle().
  // https://drafts.csswg.org/css-flexbox-1/#valdef-min-width-auto
  // https://drafts.csswg.org/css-grid/#min-size-auto
  // In most cases, "min-{width|height}: auto" is mapped to "0px", unless
  // we're a flex item (and the min-size is in the flex container's main
  // axis), or we're a grid item, AND we also have overflow:visible.

  // Note: We only need to bother checking one "overflow" subproperty for
  // "visible", because a non-"visible" value in either axis would force the
  // other axis to also be non-"visible" as well.

  if (mOuterFrame) {
    nsIFrame* containerFrame = mOuterFrame->GetParent();
    if (containerFrame &&
        StyleDisplay()->mOverflowX == NS_STYLE_OVERFLOW_VISIBLE) {
      auto containerType = containerFrame->GetType();
      if (containerType == nsGkAtoms::flexContainerFrame &&
          (static_cast<nsFlexContainerFrame*>(containerFrame)->IsHorizontal() ==
           (aAxis == eAxisHorizontal))) {
        return true;
      }
      if (containerType == nsGkAtoms::gridContainerFrame) {
        return true;
      }
    }
  }
  return false;
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMinHeight()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  nsStyleCoord minHeight = StylePosition()->mMinHeight;

  if (eStyleUnit_Auto == minHeight.GetUnit() &&
      !ShouldHonorMinSizeAutoInAxis(eAxisVertical)) {
    minHeight.SetCoordValue(0);
  }

  SetValueToCoord(val, minHeight, true, nullptr, nsCSSProps::kWidthKTable);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMinWidth()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  nsStyleCoord minWidth = StylePosition()->mMinWidth;

  if (eStyleUnit_Auto == minWidth.GetUnit() &&
      !ShouldHonorMinSizeAutoInAxis(eAxisHorizontal)) {
    minWidth.SetCoordValue(0);
  }

  SetValueToCoord(val, minWidth, true, nullptr, nsCSSProps::kWidthKTable);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMixBlendMode()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleEffects()->mMixBlendMode,
                                               nsCSSProps::kBlendModeKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetIsolation()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mIsolation,
                                               nsCSSProps::kIsolationKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetObjectFit()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StylePosition()->mObjectFit,
                                               nsCSSProps::kObjectFitKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetObjectPosition()
{
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);
  SetValueToPosition(StylePosition()->mObjectPosition, valueList);
  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetLeft()
{
  return GetOffsetWidthFor(eSideLeft);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetRight()
{
  return GetOffsetWidthFor(eSideRight);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTop()
{
  return GetOffsetWidthFor(eSideTop);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetOffsetWidthFor(mozilla::Side aSide)
{
  const nsStyleDisplay* display = StyleDisplay();

  AssertFlushedPendingReflows();

  uint8_t position = display->mPosition;
  if (!mOuterFrame) {
    // GetRelativeOffset and GetAbsoluteOffset don't handle elements
    // without frames in any sensible way.  GetStaticOffset, however,
    // is perfect for that case.
    position = NS_STYLE_POSITION_STATIC;
  }

  switch (position) {
    case NS_STYLE_POSITION_STATIC:
      return GetStaticOffset(aSide);
    case NS_STYLE_POSITION_RELATIVE:
      return GetRelativeOffset(aSide);
    case NS_STYLE_POSITION_STICKY:
      return GetStickyOffset(aSide);
    case NS_STYLE_POSITION_ABSOLUTE:
    case NS_STYLE_POSITION_FIXED:
      return GetAbsoluteOffset(aSide);
    default:
      NS_ERROR("Invalid position");
      return nullptr;
  }
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetAbsoluteOffset(mozilla::Side aSide)
{
  MOZ_ASSERT(mOuterFrame, "need a frame, so we can call GetContainingBlock()");

  nsIFrame* container = mOuterFrame->GetContainingBlock();
  nsMargin margin = mOuterFrame->GetUsedMargin();
  nsMargin border = container->GetUsedBorder();
  nsMargin scrollbarSizes(0, 0, 0, 0);
  nsRect rect = mOuterFrame->GetRect();
  nsRect containerRect = container->GetRect();

  if (container->GetType() == nsGkAtoms::viewportFrame) {
    // For absolutely positioned frames scrollbars are taken into
    // account by virtue of getting a containing block that does
    // _not_ include the scrollbars.  For fixed positioned frames,
    // the containing block is the viewport, which _does_ include
    // scrollbars.  We have to do some extra work.
    // the first child in the default frame list is what we want
    nsIFrame* scrollingChild = container->PrincipalChildList().FirstChild();
    nsIScrollableFrame *scrollFrame = do_QueryFrame(scrollingChild);
    if (scrollFrame) {
      scrollbarSizes = scrollFrame->GetActualScrollbarSizes();
    }
  }

  nscoord offset = 0;
  switch (aSide) {
    case eSideTop:
      offset = rect.y - margin.top - border.top - scrollbarSizes.top;

      break;
    case eSideRight:
      offset = containerRect.width - rect.width -
        rect.x - margin.right - border.right - scrollbarSizes.right;

      break;
    case eSideBottom:
      offset = containerRect.height - rect.height -
        rect.y - margin.bottom - border.bottom - scrollbarSizes.bottom;

      break;
    case eSideLeft:
      offset = rect.x - margin.left - border.left - scrollbarSizes.left;

      break;
    default:
      NS_ERROR("Invalid side");
      break;
  }

  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetAppUnits(offset);
  return val.forget();
}

static_assert(eSideTop == 0 && eSideRight == 1 &&
              eSideBottom == 2 && eSideLeft == 3,
              "box side constants not as expected for NS_OPPOSITE_SIDE");
#define NS_OPPOSITE_SIDE(s_) mozilla::Side(((s_) + 2) & 3)

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetRelativeOffset(mozilla::Side aSide)
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  const nsStylePosition* positionData = StylePosition();
  int32_t sign = 1;
  nsStyleCoord coord = positionData->mOffset.Get(aSide);

  NS_ASSERTION(coord.GetUnit() == eStyleUnit_Coord ||
               coord.GetUnit() == eStyleUnit_Percent ||
               coord.GetUnit() == eStyleUnit_Auto ||
               coord.IsCalcUnit(),
               "Unexpected unit");

  if (coord.GetUnit() == eStyleUnit_Auto) {
    coord = positionData->mOffset.Get(NS_OPPOSITE_SIDE(aSide));
    sign = -1;
  }
  PercentageBaseGetter baseGetter;
  if (aSide == eSideLeft || aSide == eSideRight) {
    baseGetter = &nsComputedDOMStyle::GetCBContentWidth;
  } else {
    baseGetter = &nsComputedDOMStyle::GetCBContentHeight;
  }

  val->SetAppUnits(sign * StyleCoordToNSCoord(coord, baseGetter, 0, false));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetStickyOffset(mozilla::Side aSide)
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  const nsStylePosition* positionData = StylePosition();
  nsStyleCoord coord = positionData->mOffset.Get(aSide);

  NS_ASSERTION(coord.GetUnit() == eStyleUnit_Coord ||
               coord.GetUnit() == eStyleUnit_Percent ||
               coord.GetUnit() == eStyleUnit_Auto ||
               coord.IsCalcUnit(),
               "Unexpected unit");

  if (coord.GetUnit() == eStyleUnit_Auto) {
    val->SetIdent(eCSSKeyword_auto);
    return val.forget();
  }
  PercentageBaseGetter baseGetter;
  if (aSide == eSideLeft || aSide == eSideRight) {
    baseGetter = &nsComputedDOMStyle::GetScrollFrameContentWidth;
  } else {
    baseGetter = &nsComputedDOMStyle::GetScrollFrameContentHeight;
  }

  val->SetAppUnits(StyleCoordToNSCoord(coord, baseGetter, 0, false));
  return val.forget();
}


already_AddRefed<CSSValue>
nsComputedDOMStyle::GetStaticOffset(mozilla::Side aSide)

{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StylePosition()->mOffset.Get(aSide), false);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetPaddingWidthFor(mozilla::Side aSide)
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  if (!mInnerFrame) {
    SetValueToCoord(val, StylePadding()->mPadding.Get(aSide), true);
  } else {
    AssertFlushedPendingReflows();

    val->SetAppUnits(mInnerFrame->GetUsedPadding().Side(aSide));
  }

  return val.forget();
}

bool
nsComputedDOMStyle::GetLineHeightCoord(nscoord& aCoord)
{
  AssertFlushedPendingReflows();

  nscoord blockHeight = NS_AUTOHEIGHT;
  if (StyleText()->mLineHeight.GetUnit() == eStyleUnit_Enumerated) {
    if (!mInnerFrame)
      return false;

    if (nsLayoutUtils::IsNonWrapperBlock(mInnerFrame)) {
      blockHeight = mInnerFrame->GetContentRect().height;
    } else {
      GetCBContentHeight(blockHeight);
    }
  }

  // lie about font size inflation since we lie about font size (since
  // the inflation only applies to text)
  aCoord = ReflowInput::CalcLineHeight(mContent, mStyleContext,
                                             blockHeight, 1.0f);

  // CalcLineHeight uses font->mFont.size, but we want to use
  // font->mSize as the font size.  Adjust for that.  Also adjust for
  // the text zoom, if any.
  const nsStyleFont* font = StyleFont();
  float fCoord = float(aCoord);
  if (font->mAllowZoom) {
    fCoord /= mPresShell->GetPresContext()->EffectiveTextZoom();
  }
  if (font->mFont.size != font->mSize) {
    fCoord = fCoord * (float(font->mSize) / float(font->mFont.size));
  }
  aCoord = NSToCoordRound(fCoord);

  return true;
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetBorderColorsFor(mozilla::Side aSide)
{
  const nsStyleBorder *border = StyleBorder();

  if (border->mBorderColors) {
    nsBorderColors* borderColors = border->mBorderColors[aSide];
    if (borderColors) {
      RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);

      do {
        RefPtr<nsROCSSPrimitiveValue> primitive = new nsROCSSPrimitiveValue;

        SetToRGBAColor(primitive, borderColors->mColor);

        valueList->AppendCSSValue(primitive.forget());
        borderColors = borderColors->mNext;
      } while (borderColors);

      return valueList.forget();
    }
  }

  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(eCSSKeyword_none);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetBorderWidthFor(mozilla::Side aSide)
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  nscoord width;
  if (mInnerFrame) {
    AssertFlushedPendingReflows();
    width = mInnerFrame->GetUsedBorder().Side(aSide);
  } else {
    width = StyleBorder()->GetComputedBorderWidth(aSide);
  }
  val->SetAppUnits(width);

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetBorderColorFor(mozilla::Side aSide)
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueFromComplexColor(val, StyleBorder()->mBorderColor[aSide]);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetMarginWidthFor(mozilla::Side aSide)
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  if (!mInnerFrame) {
    SetValueToCoord(val, StyleMargin()->mMargin.Get(aSide), false);
  } else {
    AssertFlushedPendingReflows();

    // For tables, GetUsedMargin always returns an empty margin, so we
    // should read the margin from the table wrapper frame instead.
    val->SetAppUnits(mOuterFrame->GetUsedMargin().Side(aSide));
    NS_ASSERTION(mOuterFrame == mInnerFrame ||
                 mInnerFrame->GetUsedMargin() == nsMargin(0, 0, 0, 0),
                 "Inner tables must have zero margins");
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetBorderStyleFor(mozilla::Side aSide)
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleBorder()->GetBorderStyle(aSide),
                                   nsCSSProps::kBorderStyleKTable));
  return val.forget();
}

void
nsComputedDOMStyle::SetValueToCoord(nsROCSSPrimitiveValue* aValue,
                                    const nsStyleCoord& aCoord,
                                    bool aClampNegativeCalc,
                                    PercentageBaseGetter aPercentageBaseGetter,
                                    const KTableEntry aTable[],
                                    nscoord aMinAppUnits,
                                    nscoord aMaxAppUnits)
{
  NS_PRECONDITION(aValue, "Must have a value to work with");

  switch (aCoord.GetUnit()) {
    case eStyleUnit_Normal:
      aValue->SetIdent(eCSSKeyword_normal);
      break;

    case eStyleUnit_Auto:
      aValue->SetIdent(eCSSKeyword_auto);
      break;

    case eStyleUnit_Percent:
      {
        nscoord percentageBase;
        if (aPercentageBaseGetter &&
            (this->*aPercentageBaseGetter)(percentageBase)) {
          nscoord val = NSCoordSaturatingMultiply(percentageBase,
                                                  aCoord.GetPercentValue());
          aValue->SetAppUnits(std::max(aMinAppUnits, std::min(val, aMaxAppUnits)));
        } else {
          aValue->SetPercent(aCoord.GetPercentValue());
        }
      }
      break;

    case eStyleUnit_Factor:
      aValue->SetNumber(aCoord.GetFactorValue());
      break;

    case eStyleUnit_Coord:
      {
        nscoord val = aCoord.GetCoordValue();
        aValue->SetAppUnits(std::max(aMinAppUnits, std::min(val, aMaxAppUnits)));
      }
      break;

    case eStyleUnit_Integer:
      aValue->SetNumber(aCoord.GetIntValue());
      break;

    case eStyleUnit_Enumerated:
      NS_ASSERTION(aTable, "Must have table to handle this case");
      aValue->SetIdent(nsCSSProps::ValueToKeywordEnum(aCoord.GetIntValue(),
                                                      aTable));
      break;

    case eStyleUnit_None:
      aValue->SetIdent(eCSSKeyword_none);
      break;

    case eStyleUnit_Calc:
      nscoord percentageBase;
      if (!aCoord.CalcHasPercent()) {
        nscoord val = nsRuleNode::ComputeCoordPercentCalc(aCoord, 0);
        if (aClampNegativeCalc && val < 0) {
          MOZ_ASSERT(aCoord.IsCalcUnit(),
                     "parser should have rejected value");
          val = 0;
        }
        aValue->SetAppUnits(std::max(aMinAppUnits, std::min(val, aMaxAppUnits)));
      } else if (aPercentageBaseGetter &&
                 (this->*aPercentageBaseGetter)(percentageBase)) {
        nscoord val =
          nsRuleNode::ComputeCoordPercentCalc(aCoord, percentageBase);
        if (aClampNegativeCalc && val < 0) {
          MOZ_ASSERT(aCoord.IsCalcUnit(),
                     "parser should have rejected value");
          val = 0;
        }
        aValue->SetAppUnits(std::max(aMinAppUnits, std::min(val, aMaxAppUnits)));
      } else {
        nsStyleCoord::Calc *calc = aCoord.GetCalcValue();
        SetValueToCalc(calc, aValue);
      }
      break;

    case eStyleUnit_Degree:
      aValue->SetDegree(aCoord.GetAngleValue());
      break;

    case eStyleUnit_Grad:
      aValue->SetGrad(aCoord.GetAngleValue());
      break;

    case eStyleUnit_Radian:
      aValue->SetRadian(aCoord.GetAngleValue());
      break;

    case eStyleUnit_Turn:
      aValue->SetTurn(aCoord.GetAngleValue());
      break;

    case eStyleUnit_FlexFraction: {
      nsAutoString tmpStr;
      nsStyleUtil::AppendCSSNumber(aCoord.GetFlexFractionValue(), tmpStr);
      tmpStr.AppendLiteral("fr");
      aValue->SetString(tmpStr);
      break;
    }

    default:
      NS_ERROR("Can't handle this unit");
      break;
  }
}

nscoord
nsComputedDOMStyle::StyleCoordToNSCoord(const nsStyleCoord& aCoord,
                                        PercentageBaseGetter aPercentageBaseGetter,
                                        nscoord aDefaultValue,
                                        bool aClampNegativeCalc)
{
  NS_PRECONDITION(aPercentageBaseGetter, "Must have a percentage base getter");
  if (aCoord.GetUnit() == eStyleUnit_Coord) {
    return aCoord.GetCoordValue();
  }
  if (aCoord.GetUnit() == eStyleUnit_Percent || aCoord.IsCalcUnit()) {
    nscoord percentageBase;
    if ((this->*aPercentageBaseGetter)(percentageBase)) {
      nscoord result =
        nsRuleNode::ComputeCoordPercentCalc(aCoord, percentageBase);
      if (aClampNegativeCalc && result < 0) {
        // It's expected that we can get a negative value here with calc().
        // We can also get a negative value with a percentage value if
        // percentageBase is negative; this isn't expected, but can happen
        // when large length values overflow.
        NS_WARNING_ASSERTION(
          percentageBase >= 0,
          "percentage base value overflowed to become negative for a property "
          "that disallows negative values");
        MOZ_ASSERT(aCoord.IsCalcUnit() ||
                   (aCoord.HasPercent() && percentageBase < 0),
                   "parser should have rejected value");
        result = 0;
      }
      return result;
    }
    // Fall through to returning aDefaultValue if we have no percentage base.
  }

  return aDefaultValue;
}

bool
nsComputedDOMStyle::GetCBContentWidth(nscoord& aWidth)
{
  if (!mOuterFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  nsIFrame* container = mOuterFrame->GetContainingBlock();
  aWidth = container->GetContentRect().width;
  return true;
}

bool
nsComputedDOMStyle::GetCBContentHeight(nscoord& aHeight)
{
  if (!mOuterFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  nsIFrame* container = mOuterFrame->GetContainingBlock();
  aHeight = container->GetContentRect().height;
  return true;
}

bool
nsComputedDOMStyle::GetScrollFrameContentWidth(nscoord& aWidth)
{
  if (!mOuterFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  nsIScrollableFrame* scrollableFrame =
    nsLayoutUtils::GetNearestScrollableFrame(mOuterFrame->GetParent(),
      nsLayoutUtils::SCROLLABLE_SAME_DOC |
      nsLayoutUtils::SCROLLABLE_INCLUDE_HIDDEN);

  if (!scrollableFrame) {
    return false;
  }
  aWidth =
    scrollableFrame->GetScrolledFrame()->GetContentRectRelativeToSelf().width;
  return true;
}

bool
nsComputedDOMStyle::GetScrollFrameContentHeight(nscoord& aHeight)
{
  if (!mOuterFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  nsIScrollableFrame* scrollableFrame =
    nsLayoutUtils::GetNearestScrollableFrame(mOuterFrame->GetParent(),
      nsLayoutUtils::SCROLLABLE_SAME_DOC |
      nsLayoutUtils::SCROLLABLE_INCLUDE_HIDDEN);

  if (!scrollableFrame) {
    return false;
  }
  aHeight =
    scrollableFrame->GetScrolledFrame()->GetContentRectRelativeToSelf().height;
  return true;
}

bool
nsComputedDOMStyle::GetFrameBorderRectWidth(nscoord& aWidth)
{
  if (!mInnerFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  aWidth = mInnerFrame->GetSize().width;
  return true;
}

bool
nsComputedDOMStyle::GetFrameBorderRectHeight(nscoord& aHeight)
{
  if (!mInnerFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  aHeight = mInnerFrame->GetSize().height;
  return true;
}

bool
nsComputedDOMStyle::GetFrameBoundsWidthForTransform(nscoord& aWidth)
{
  // We need a frame to work with.
  if (!mInnerFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  aWidth = nsStyleTransformMatrix::TransformReferenceBox(mInnerFrame).Width();
  return true;
}

bool
nsComputedDOMStyle::GetFrameBoundsHeightForTransform(nscoord& aHeight)
{
  // We need a frame to work with.
  if (!mInnerFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  aHeight = nsStyleTransformMatrix::TransformReferenceBox(mInnerFrame).Height();
  return true;
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetSVGPaintFor(bool aFill)
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  const nsStyleSVG* svg = StyleSVG();
  const nsStyleSVGPaint* paint = nullptr;

  if (aFill)
    paint = &svg->mFill;
  else
    paint = &svg->mStroke;

  nsAutoString paintString;

  switch (paint->Type()) {
    case eStyleSVGPaintType_None:
      val->SetIdent(eCSSKeyword_none);
      break;
    case eStyleSVGPaintType_Color:
      SetToRGBAColor(val, paint->GetColor());
      break;
    case eStyleSVGPaintType_Server: {
      RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);
      RefPtr<nsROCSSPrimitiveValue> fallback = new nsROCSSPrimitiveValue;
      SetValueToURLValue(paint->GetPaintServer(), val);
      SetToRGBAColor(fallback, paint->GetFallbackColor());

      valueList->AppendCSSValue(val.forget());
      valueList->AppendCSSValue(fallback.forget());
      return valueList.forget();
    }
    case eStyleSVGPaintType_ContextFill:
      val->SetIdent(eCSSKeyword_context_fill);
      // XXXheycam context-fill and context-stroke can have fallback colors,
      // so they should be serialized here too
      break;
    case eStyleSVGPaintType_ContextStroke:
      val->SetIdent(eCSSKeyword_context_stroke);
      break;
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFill()
{
  return GetSVGPaintFor(true);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetStroke()
{
  return GetSVGPaintFor(false);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMarkerEnd()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueToURLValue(StyleSVG()->mMarkerEnd, val);

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMarkerMid()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueToURLValue(StyleSVG()->mMarkerMid, val);

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMarkerStart()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueToURLValue(StyleSVG()->mMarkerStart, val);

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetStrokeDasharray()
{
  const nsStyleSVG* svg = StyleSVG();

  if (svg->mStrokeDasharray.IsEmpty()) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_none);
    return val.forget();
  }

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  for (uint32_t i = 0; i < svg->mStrokeDasharray.Length(); i++) {
    RefPtr<nsROCSSPrimitiveValue> dash = new nsROCSSPrimitiveValue;
    SetValueToCoord(dash, svg->mStrokeDasharray[i], true);
    valueList->AppendCSSValue(dash.forget());
  }

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetStrokeDashoffset()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StyleSVG()->mStrokeDashoffset, false);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetStrokeWidth()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StyleSVG()->mStrokeWidth, true);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetVectorEffect()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleSVGReset()->mVectorEffect,
                                               nsCSSProps::kVectorEffectKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFillOpacity()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleSVG()->mFillOpacity);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFloodOpacity()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleSVGReset()->mFloodOpacity);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetStopOpacity()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleSVGReset()->mStopOpacity);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetStrokeMiterlimit()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleSVG()->mStrokeMiterlimit);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetStrokeOpacity()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleSVG()->mStrokeOpacity);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetClipRule()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(
                  StyleSVG()->mClipRule, nsCSSProps::kFillRuleKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFillRule()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(
                  StyleSVG()->mFillRule, nsCSSProps::kFillRuleKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetStrokeLinecap()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleSVG()->mStrokeLinecap,
                                   nsCSSProps::kStrokeLinecapKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetStrokeLinejoin()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleSVG()->mStrokeLinejoin,
                                   nsCSSProps::kStrokeLinejoinKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTextAnchor()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleSVG()->mTextAnchor,
                                   nsCSSProps::kTextAnchorKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetColorInterpolation()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleSVG()->mColorInterpolation,
                                   nsCSSProps::kColorInterpolationKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetColorInterpolationFilters()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleSVG()->mColorInterpolationFilters,
                                   nsCSSProps::kColorInterpolationKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetDominantBaseline()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleSVGReset()->mDominantBaseline,
                                   nsCSSProps::kDominantBaselineKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetImageRendering()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleVisibility()->mImageRendering,
                                   nsCSSProps::kImageRenderingKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetShapeRendering()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleSVG()->mShapeRendering,
                                   nsCSSProps::kShapeRenderingKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTextRendering()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleText()->mTextRendering,
                                   nsCSSProps::kTextRenderingKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFloodColor()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetToRGBAColor(val, StyleSVGReset()->mFloodColor);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetLightingColor()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetToRGBAColor(val, StyleSVGReset()->mLightingColor);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetStopColor()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetToRGBAColor(val, StyleSVGReset()->mStopColor);
  return val.forget();
}

void
nsComputedDOMStyle::BoxValuesToString(nsAString& aString,
                                      const nsTArray<nsStyleCoord>& aBoxValues)
{
  MOZ_ASSERT(aBoxValues.Length() == 4, "wrong number of box values");
  nsAutoString value1, value2, value3, value4;
  SetCssTextToCoord(value1, aBoxValues[0]);
  SetCssTextToCoord(value2, aBoxValues[1]);
  SetCssTextToCoord(value3, aBoxValues[2]);
  SetCssTextToCoord(value4, aBoxValues[3]);

  // nsROCSSPrimitiveValue do not have binary comparison operators.
  // Compare string results instead.
  aString.Append(value1);
  if (value1 != value2 || value1 != value3 || value1 != value4) {
    aString.Append(' ');
    aString.Append(value2);
    if (value1 != value3 || value2 != value4) {
      aString.Append(' ');
      aString.Append(value3);
      if (value2 != value4) {
        aString.Append(' ');
        aString.Append(value4);
      }
    }
  }
}

void
nsComputedDOMStyle::BasicShapeRadiiToString(nsAString& aCssText,
                                            const nsStyleCorners& aCorners)
{
  nsTArray<nsStyleCoord> horizontal, vertical;
  nsAutoString horizontalString, verticalString;
  NS_FOR_CSS_FULL_CORNERS(corner) {
    horizontal.AppendElement(
      aCorners.Get(FullToHalfCorner(corner, false)));
    vertical.AppendElement(
      aCorners.Get(FullToHalfCorner(corner, true)));
  }
  BoxValuesToString(horizontalString, horizontal);
  BoxValuesToString(verticalString, vertical);
  aCssText.Append(horizontalString);
  if (horizontalString == verticalString) {
    return;
  }
  aCssText.AppendLiteral(" / ");
  aCssText.Append(verticalString);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::CreatePrimitiveValueForBasicShape(
  const StyleBasicShape* aStyleBasicShape)
{
  MOZ_ASSERT(aStyleBasicShape, "Expect a valid basic shape pointer!");

  StyleBasicShapeType type = aStyleBasicShape->GetShapeType();
  // Shape function name and opening parenthesis.
  nsAutoString shapeFunctionString;
  AppendASCIItoUTF16(nsCSSKeywords::GetStringValue(
                       aStyleBasicShape->GetShapeTypeName()),
                     shapeFunctionString);
  shapeFunctionString.Append('(');
  switch (type) {
    case StyleBasicShapeType::Polygon: {
      bool hasEvenOdd = aStyleBasicShape->GetFillRule() ==
        StyleFillRule::Evenodd;
      if (hasEvenOdd) {
        shapeFunctionString.AppendLiteral("evenodd");
      }
      for (size_t i = 0;
           i < aStyleBasicShape->Coordinates().Length(); i += 2) {
        nsAutoString coordString;
        if (i > 0 || hasEvenOdd) {
          shapeFunctionString.AppendLiteral(", ");
        }
        SetCssTextToCoord(coordString,
                          aStyleBasicShape->Coordinates()[i]);
        shapeFunctionString.Append(coordString);
        shapeFunctionString.Append(' ');
        SetCssTextToCoord(coordString,
                          aStyleBasicShape->Coordinates()[i + 1]);
        shapeFunctionString.Append(coordString);
      }
      break;
    }
    case StyleBasicShapeType::Circle:
    case StyleBasicShapeType::Ellipse: {
      const nsTArray<nsStyleCoord>& radii = aStyleBasicShape->Coordinates();
      MOZ_ASSERT(radii.Length() ==
                 (type == StyleBasicShapeType::Circle ? 1 : 2),
                 "wrong number of radii");
      for (size_t i = 0; i < radii.Length(); ++i) {
        nsAutoString radius;
        RefPtr<nsROCSSPrimitiveValue> value = new nsROCSSPrimitiveValue;
        bool clampNegativeCalc = true;
        SetValueToCoord(value, radii[i], clampNegativeCalc, nullptr,
                        nsCSSProps::kShapeRadiusKTable);
        value->GetCssText(radius);
        shapeFunctionString.Append(radius);
        shapeFunctionString.Append(' ');
      }
      shapeFunctionString.AppendLiteral("at ");

      RefPtr<nsDOMCSSValueList> position = GetROCSSValueList(false);
      nsAutoString positionString;
      SetValueToPosition(aStyleBasicShape->GetPosition(), position);
      position->GetCssText(positionString);
      shapeFunctionString.Append(positionString);
      break;
    }
    case StyleBasicShapeType::Inset: {
      BoxValuesToString(shapeFunctionString, aStyleBasicShape->Coordinates());
      if (aStyleBasicShape->HasRadius()) {
        shapeFunctionString.AppendLiteral(" round ");
        nsAutoString radiiString;
        BasicShapeRadiiToString(radiiString, aStyleBasicShape->GetRadius());
        shapeFunctionString.Append(radiiString);
      }
      break;
    }
    default:
      NS_NOTREACHED("unexpected type");
  }
  shapeFunctionString.Append(')');
  RefPtr<nsROCSSPrimitiveValue> functionValue = new nsROCSSPrimitiveValue;
  functionValue->SetString(shapeFunctionString);
  return functionValue.forget();
}

template<typename ReferenceBox>
already_AddRefed<CSSValue>
nsComputedDOMStyle::CreatePrimitiveValueForShapeSource(
  const StyleBasicShape* aStyleBasicShape,
  ReferenceBox aReferenceBox,
  const KTableEntry aBoxKeywordTable[])
{
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);
  if (aStyleBasicShape) {
    valueList->AppendCSSValue(
      CreatePrimitiveValueForBasicShape(aStyleBasicShape));
  }

  if (aReferenceBox == ReferenceBox::NoBox) {
    return valueList.forget();
  }

  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(aReferenceBox, aBoxKeywordTable));
  valueList->AppendCSSValue(val.forget());

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetShapeSource(
  const StyleShapeSource& aShapeSource,
  const KTableEntry aBoxKeywordTable[])
{
  switch (aShapeSource.GetType()) {
    case StyleShapeSourceType::Shape:
      return CreatePrimitiveValueForShapeSource(aShapeSource.GetBasicShape(),
                                                aShapeSource.GetReferenceBox(),
                                                aBoxKeywordTable);
    case StyleShapeSourceType::Box:
      return CreatePrimitiveValueForShapeSource(nullptr,
                                                aShapeSource.GetReferenceBox(),
                                                aBoxKeywordTable);
    case StyleShapeSourceType::URL: {
      RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
      SetValueToURLValue(aShapeSource.GetURL(), val);
      return val.forget();
    }
    case StyleShapeSourceType::None: {
      RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
      val->SetIdent(eCSSKeyword_none);
      return val.forget();
    }
    default:
      NS_NOTREACHED("unexpected type");
  }
  return nullptr;
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetClipPath()
{
  return GetShapeSource(StyleSVGReset()->mClipPath,
                        nsCSSProps::kClipPathGeometryBoxKTable);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetShapeOutside()
{
  return GetShapeSource(StyleDisplay()->mShapeOutside,
                        nsCSSProps::kShapeOutsideShapeBoxKTable);
}

void
nsComputedDOMStyle::SetCssTextToCoord(nsAString& aCssText,
                                      const nsStyleCoord& aCoord)
{
  RefPtr<nsROCSSPrimitiveValue> value = new nsROCSSPrimitiveValue;
  bool clampNegativeCalc = true;
  SetValueToCoord(value, aCoord, clampNegativeCalc);
  value->GetCssText(aCssText);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::CreatePrimitiveValueForStyleFilter(
  const nsStyleFilter& aStyleFilter)
{
  RefPtr<nsROCSSPrimitiveValue> value = new nsROCSSPrimitiveValue;
  // Handle url().
  if (aStyleFilter.GetType() == NS_STYLE_FILTER_URL) {
    MOZ_ASSERT(aStyleFilter.GetURL() &&
               aStyleFilter.GetURL()->GetURI());
    SetValueToURLValue(aStyleFilter.GetURL(), value);
    return value.forget();
  }

  // Filter function name and opening parenthesis.
  nsAutoString filterFunctionString;
  AppendASCIItoUTF16(
    nsCSSProps::ValueToKeyword(aStyleFilter.GetType(),
                               nsCSSProps::kFilterFunctionKTable),
                               filterFunctionString);
  filterFunctionString.Append('(');

  nsAutoString argumentString;
  if (aStyleFilter.GetType() == NS_STYLE_FILTER_DROP_SHADOW) {
    // Handle drop-shadow()
    RefPtr<CSSValue> shadowValue =
      GetCSSShadowArray(aStyleFilter.GetDropShadow(),
                        StyleColor()->mColor,
                        false);
    ErrorResult dummy;
    shadowValue->GetCssText(argumentString, dummy);
  } else {
    // Filter function argument.
    SetCssTextToCoord(argumentString, aStyleFilter.GetFilterParameter());
  }
  filterFunctionString.Append(argumentString);

  // Filter function closing parenthesis.
  filterFunctionString.Append(')');

  value->SetString(filterFunctionString);
  return value.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetFilter()
{
  const nsTArray<nsStyleFilter>& filters = StyleEffects()->mFilters;

  if (filters.IsEmpty()) {
    RefPtr<nsROCSSPrimitiveValue> value = new nsROCSSPrimitiveValue;
    value->SetIdent(eCSSKeyword_none);
    return value.forget();
  }

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);
  for(uint32_t i = 0; i < filters.Length(); i++) {
    RefPtr<CSSValue> value = CreatePrimitiveValueForStyleFilter(filters[i]);
    valueList->AppendCSSValue(value.forget());
  }
  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMask()
{
  const nsStyleSVGReset* svg = StyleSVGReset();
  const nsStyleImageLayers::Layer& firstLayer = svg->mMask.mLayers[0];

  // Mask is now a shorthand, but it used to be a longhand, so that we
  // need to support computed style for the cases where it used to be
  // a longhand.
  if (svg->mMask.mImageCount > 1 ||
      firstLayer.mClip != StyleGeometryBox::BorderBox ||
      firstLayer.mOrigin != StyleGeometryBox::BorderBox ||
      firstLayer.mComposite != NS_STYLE_MASK_COMPOSITE_ADD ||
      firstLayer.mMaskMode != NS_STYLE_MASK_MODE_MATCH_SOURCE ||
      !nsStyleImageLayers::IsInitialPositionForLayerType(
        firstLayer.mPosition, nsStyleImageLayers::LayerType::Mask) ||
      !firstLayer.mRepeat.IsInitialValue() ||
      !firstLayer.mSize.IsInitialValue() ||
      !(firstLayer.mImage.GetType() == eStyleImageType_Null ||
        firstLayer.mImage.GetType() == eStyleImageType_Image ||
        firstLayer.mImage.GetType() == eStyleImageType_URL)) {
    return nullptr;
  }

  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  SetValueToURLValue(firstLayer.mImage.GetURLValue(), val);

  return val.forget();
}

#ifdef MOZ_ENABLE_MASK_AS_SHORTHAND
already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMaskClip()
{
  return GetBackgroundList(&nsStyleImageLayers::Layer::mClip,
                           &nsStyleImageLayers::mClipCount,
                           StyleSVGReset()->mMask,
                           nsCSSProps::kMaskClipKTable);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMaskComposite()
{
  return GetBackgroundList(&nsStyleImageLayers::Layer::mComposite,
                           &nsStyleImageLayers::mCompositeCount,
                           StyleSVGReset()->mMask,
                           nsCSSProps::kImageLayerCompositeKTable);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMaskImage()
{
  const nsStyleImageLayers& layers = StyleSVGReset()->mMask;
  return DoGetImageLayerImage(layers);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMaskMode()
{
  return GetBackgroundList(&nsStyleImageLayers::Layer::mMaskMode,
                           &nsStyleImageLayers::mMaskModeCount,
                           StyleSVGReset()->mMask,
                           nsCSSProps::kImageLayerModeKTable);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMaskOrigin()
{
  return GetBackgroundList(&nsStyleImageLayers::Layer::mOrigin,
                           &nsStyleImageLayers::mOriginCount,
                           StyleSVGReset()->mMask,
                           nsCSSProps::kMaskOriginKTable);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMaskPosition()
{
  const nsStyleImageLayers& layers = StyleSVGReset()->mMask;
  return DoGetImageLayerPosition(layers);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMaskPositionX()
{
  const nsStyleImageLayers& layers = StyleSVGReset()->mMask;
  return DoGetImageLayerPositionX(layers);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMaskPositionY()
{
  const nsStyleImageLayers& layers = StyleSVGReset()->mMask;
  return DoGetImageLayerPositionY(layers);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMaskRepeat()
{
  const nsStyleImageLayers& layers = StyleSVGReset()->mMask;
  return DoGetImageLayerRepeat(layers);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMaskSize()
{
  const nsStyleImageLayers& layers = StyleSVGReset()->mMask;
  return DoGetImageLayerSize(layers);
}
#endif

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMaskType()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleSVGReset()->mMaskType,
                                   nsCSSProps::kMaskTypeKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetPaintOrder()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  nsAutoString string;
  uint8_t paintOrder = StyleSVG()->mPaintOrder;
  nsStyleUtil::AppendPaintOrderValue(paintOrder, string);
  val->SetString(string);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTransitionDelay()
{
  const nsStyleDisplay* display = StyleDisplay();

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  MOZ_ASSERT(display->mTransitionDelayCount > 0,
             "first item must be explicit");
  uint32_t i = 0;
  do {
    const StyleTransition *transition = &display->mTransitions[i];
    RefPtr<nsROCSSPrimitiveValue> delay = new nsROCSSPrimitiveValue;
    delay->SetTime((float)transition->GetDelay() / (float)PR_MSEC_PER_SEC);
    valueList->AppendCSSValue(delay.forget());
  } while (++i < display->mTransitionDelayCount);

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTransitionDuration()
{
  const nsStyleDisplay* display = StyleDisplay();

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  MOZ_ASSERT(display->mTransitionDurationCount > 0,
             "first item must be explicit");
  uint32_t i = 0;
  do {
    const StyleTransition *transition = &display->mTransitions[i];
    RefPtr<nsROCSSPrimitiveValue> duration = new nsROCSSPrimitiveValue;

    duration->SetTime((float)transition->GetDuration() / (float)PR_MSEC_PER_SEC);
    valueList->AppendCSSValue(duration.forget());
  } while (++i < display->mTransitionDurationCount);

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTransitionProperty()
{
  const nsStyleDisplay* display = StyleDisplay();

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  MOZ_ASSERT(display->mTransitionPropertyCount > 0,
             "first item must be explicit");
  uint32_t i = 0;
  do {
    const StyleTransition *transition = &display->mTransitions[i];
    RefPtr<nsROCSSPrimitiveValue> property = new nsROCSSPrimitiveValue;
    nsCSSPropertyID cssprop = transition->GetProperty();
    if (cssprop == eCSSPropertyExtra_all_properties)
      property->SetIdent(eCSSKeyword_all);
    else if (cssprop == eCSSPropertyExtra_no_properties)
      property->SetIdent(eCSSKeyword_none);
    else if (cssprop == eCSSProperty_UNKNOWN ||
             cssprop == eCSSPropertyExtra_variable)
    {
      nsAutoString escaped;
      nsStyleUtil::AppendEscapedCSSIdent(
        nsDependentAtomString(transition->GetUnknownProperty()), escaped);
      property->SetString(escaped); // really want SetIdent
    }
    else
      property->SetString(nsCSSProps::GetStringValue(cssprop));

    valueList->AppendCSSValue(property.forget());
  } while (++i < display->mTransitionPropertyCount);

  return valueList.forget();
}

void
nsComputedDOMStyle::AppendTimingFunction(nsDOMCSSValueList *aValueList,
                                         const nsTimingFunction& aTimingFunction)
{
  RefPtr<nsROCSSPrimitiveValue> timingFunction = new nsROCSSPrimitiveValue;

  nsAutoString tmp;
  switch (aTimingFunction.mType) {
    case nsTimingFunction::Type::CubicBezier:
      nsStyleUtil::AppendCubicBezierTimingFunction(aTimingFunction.mFunc.mX1,
                                                   aTimingFunction.mFunc.mY1,
                                                   aTimingFunction.mFunc.mX2,
                                                   aTimingFunction.mFunc.mY2,
                                                   tmp);
      break;
    case nsTimingFunction::Type::StepStart:
    case nsTimingFunction::Type::StepEnd:
      nsStyleUtil::AppendStepsTimingFunction(aTimingFunction.mType,
                                             aTimingFunction.mStepsOrFrames,
                                             tmp);
      break;
    case nsTimingFunction::Type::Frames:
      nsStyleUtil::AppendFramesTimingFunction(aTimingFunction.mStepsOrFrames,
                                              tmp);
      break;
    default:
      nsStyleUtil::AppendCubicBezierKeywordTimingFunction(aTimingFunction.mType,
                                                          tmp);
      break;
  }
  timingFunction->SetString(tmp);
  aValueList->AppendCSSValue(timingFunction.forget());
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTransitionTimingFunction()
{
  const nsStyleDisplay* display = StyleDisplay();

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  MOZ_ASSERT(display->mTransitionTimingFunctionCount > 0,
             "first item must be explicit");
  uint32_t i = 0;
  do {
    AppendTimingFunction(valueList,
                         display->mTransitions[i].GetTimingFunction());
  } while (++i < display->mTransitionTimingFunctionCount);

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetAnimationName()
{
  const nsStyleDisplay* display = StyleDisplay();

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  MOZ_ASSERT(display->mAnimationNameCount > 0,
             "first item must be explicit");
  uint32_t i = 0;
  do {
    const StyleAnimation *animation = &display->mAnimations[i];
    RefPtr<nsROCSSPrimitiveValue> property = new nsROCSSPrimitiveValue;

    const nsString& name = animation->GetName();
    if (name.IsEmpty()) {
      property->SetIdent(eCSSKeyword_none);
    } else {
      nsAutoString escaped;
      nsStyleUtil::AppendEscapedCSSIdent(animation->GetName(), escaped);
      property->SetString(escaped); // really want SetIdent
    }
    valueList->AppendCSSValue(property.forget());
  } while (++i < display->mAnimationNameCount);

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetAnimationDelay()
{
  const nsStyleDisplay* display = StyleDisplay();

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  MOZ_ASSERT(display->mAnimationDelayCount > 0,
             "first item must be explicit");
  uint32_t i = 0;
  do {
    const StyleAnimation *animation = &display->mAnimations[i];
    RefPtr<nsROCSSPrimitiveValue> delay = new nsROCSSPrimitiveValue;
    delay->SetTime((float)animation->GetDelay() / (float)PR_MSEC_PER_SEC);
    valueList->AppendCSSValue(delay.forget());
  } while (++i < display->mAnimationDelayCount);

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetAnimationDuration()
{
  const nsStyleDisplay* display = StyleDisplay();

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  MOZ_ASSERT(display->mAnimationDurationCount > 0,
             "first item must be explicit");
  uint32_t i = 0;
  do {
    const StyleAnimation *animation = &display->mAnimations[i];
    RefPtr<nsROCSSPrimitiveValue> duration = new nsROCSSPrimitiveValue;

    duration->SetTime((float)animation->GetDuration() / (float)PR_MSEC_PER_SEC);
    valueList->AppendCSSValue(duration.forget());
  } while (++i < display->mAnimationDurationCount);

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetAnimationTimingFunction()
{
  const nsStyleDisplay* display = StyleDisplay();

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  MOZ_ASSERT(display->mAnimationTimingFunctionCount > 0,
             "first item must be explicit");
  uint32_t i = 0;
  do {
    AppendTimingFunction(valueList,
                         display->mAnimations[i].GetTimingFunction());
  } while (++i < display->mAnimationTimingFunctionCount);

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetAnimationDirection()
{
  const nsStyleDisplay* display = StyleDisplay();

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  MOZ_ASSERT(display->mAnimationDirectionCount > 0,
             "first item must be explicit");
  uint32_t i = 0;
  do {
    const StyleAnimation *animation = &display->mAnimations[i];
    RefPtr<nsROCSSPrimitiveValue> direction = new nsROCSSPrimitiveValue;
    direction->SetIdent(
      nsCSSProps::ValueToKeywordEnum(
        static_cast<int32_t>(animation->GetDirection()),
        nsCSSProps::kAnimationDirectionKTable));

    valueList->AppendCSSValue(direction.forget());
  } while (++i < display->mAnimationDirectionCount);

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetAnimationFillMode()
{
  const nsStyleDisplay* display = StyleDisplay();

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  MOZ_ASSERT(display->mAnimationFillModeCount > 0,
             "first item must be explicit");
  uint32_t i = 0;
  do {
    const StyleAnimation *animation = &display->mAnimations[i];
    RefPtr<nsROCSSPrimitiveValue> fillMode = new nsROCSSPrimitiveValue;
    fillMode->SetIdent(
      nsCSSProps::ValueToKeywordEnum(
        static_cast<int32_t>(animation->GetFillMode()),
        nsCSSProps::kAnimationFillModeKTable));

    valueList->AppendCSSValue(fillMode.forget());
  } while (++i < display->mAnimationFillModeCount);

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetAnimationIterationCount()
{
  const nsStyleDisplay* display = StyleDisplay();

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  MOZ_ASSERT(display->mAnimationIterationCountCount > 0,
             "first item must be explicit");
  uint32_t i = 0;
  do {
    const StyleAnimation *animation = &display->mAnimations[i];
    RefPtr<nsROCSSPrimitiveValue> iterationCount = new nsROCSSPrimitiveValue;

    float f = animation->GetIterationCount();
    /* Need a nasty hack here to work around an optimizer bug in gcc
       4.2 on Mac, which somehow gets confused when directly comparing
       a float to the return value of NS_IEEEPositiveInfinity when
       building 32-bit builds. */
#ifdef XP_MACOSX
    volatile
#endif
      float inf = NS_IEEEPositiveInfinity();
    if (f == inf) {
      iterationCount->SetIdent(eCSSKeyword_infinite);
    } else {
      iterationCount->SetNumber(f);
    }
    valueList->AppendCSSValue(iterationCount.forget());
  } while (++i < display->mAnimationIterationCountCount);

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetAnimationPlayState()
{
  const nsStyleDisplay* display = StyleDisplay();

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  MOZ_ASSERT(display->mAnimationPlayStateCount > 0,
             "first item must be explicit");
  uint32_t i = 0;
  do {
    const StyleAnimation *animation = &display->mAnimations[i];
    RefPtr<nsROCSSPrimitiveValue> playState = new nsROCSSPrimitiveValue;
    playState->SetIdent(
      nsCSSProps::ValueToKeywordEnum(animation->GetPlayState(),
                                     nsCSSProps::kAnimationPlayStateKTable));
    valueList->AppendCSSValue(playState.forget());
  } while (++i < display->mAnimationPlayStateCount);

  return valueList.forget();
}

static void
MarkComputedStyleMapDirty(const char* aPref, void* aData)
{
  static_cast<nsComputedStyleMap*>(aData)->MarkDirty();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetCustomProperty(const nsAString& aPropertyName)
{
  MOZ_ASSERT(nsCSSProps::IsCustomPropertyName(aPropertyName));

  const nsStyleVariables* variables = StyleVariables();

  nsString variableValue;
  const nsAString& name = Substring(aPropertyName,
                                    CSS_CUSTOM_NAME_PREFIX_LENGTH);
  if (!variables->mVariables.Get(name, variableValue)) {
    return nullptr;
  }

  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetString(variableValue);

  return val.forget();
}

void
nsComputedDOMStyle::ParentChainChanged(nsIContent* aContent)
{
  NS_ASSERTION(mContent == aContent, "didn't we register mContent?");
  NS_ASSERTION(mResolvedStyleContext,
               "should have only registered an observer when "
               "mResolvedStyleContext is true");

  ClearStyleContext();
}

/* static */ nsComputedStyleMap*
nsComputedDOMStyle::GetComputedStyleMap()
{
  static nsComputedStyleMap map = {
    {
#define COMPUTED_STYLE_PROP(prop_, method_) \
  { eCSSProperty_##prop_, &nsComputedDOMStyle::DoGet##method_ },
#include "nsComputedDOMStylePropertyList.h"
#undef COMPUTED_STYLE_PROP
    }
  };
  return &map;
}

/* static */ void
nsComputedDOMStyle::RegisterPrefChangeCallbacks()
{
  // Note that this will register callbacks for all properties with prefs, not
  // just those that are implemented on computed style objects, as it's not
  // easy to grab specific property data from nsCSSPropList.h based on the
  // entries iterated in nsComputedDOMStylePropertyList.h.
  nsComputedStyleMap* data = GetComputedStyleMap();
#define REGISTER_CALLBACK(pref_)                                             \
  if (pref_[0]) {                                                            \
    Preferences::RegisterCallback(MarkComputedStyleMapDirty, pref_, data);   \
  }
#define CSS_PROP(prop_, id_, method_, flags_, pref_, parsevariant_,          \
                 kwtable_, stylestruct_, stylestructoffset_, animtype_)      \
  REGISTER_CALLBACK(pref_)
#define CSS_PROP_LIST_INCLUDE_LOGICAL
#include "nsCSSPropList.h"
#undef CSS_PROP_LIST_INCLUDE_LOGICAL
#undef CSS_PROP
#undef REGISTER_CALLBACK
}

/* static */ void
nsComputedDOMStyle::UnregisterPrefChangeCallbacks()
{
  nsComputedStyleMap* data = GetComputedStyleMap();
#define UNREGISTER_CALLBACK(pref_)                                             \
  if (pref_[0]) {                                                              \
    Preferences::UnregisterCallback(MarkComputedStyleMapDirty, pref_, data);   \
  }
#define CSS_PROP(prop_, id_, method_, flags_, pref_, parsevariant_,            \
                 kwtable_, stylestruct_, stylestructoffset_, animtype_)        \
  UNREGISTER_CALLBACK(pref_)
#define CSS_PROP_LIST_INCLUDE_LOGICAL
#include "nsCSSPropList.h"
#undef CSS_PROP_LIST_INCLUDE_LOGICAL
#undef CSS_PROP
#undef UNREGISTER_CALLBACK
}

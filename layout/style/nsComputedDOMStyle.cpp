/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object returned from element.getComputedStyle() */

#include "nsComputedDOMStyle.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/Preferences.h"

#include "nsError.h"
#include "nsIFrame.h"
#include "nsIFrameInlines.h"
#include "mozilla/ComputedStyle.h"
#include "nsIScrollableFrame.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "nsThemeConstants.h"

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
#include "mozilla/EffectSet.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/RestyleManager.h"
#include "imgIRequest.h"
#include "nsLayoutUtils.h"
#include "nsCSSKeywords.h"
#include "nsStyleCoord.h"
#include "nsDisplayList.h"
#include "nsDOMCSSDeclaration.h"
#include "nsStyleTransformMatrix.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ElementInlines.h"
#include "prtime.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/AppUnits.h"
#include <algorithm>
#include "mozilla/ComputedStyleInlines.h"

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
NS_NewComputedDOMStyle(dom::Element* aElement,
                       const nsAString& aPseudoElt,
                       nsIDocument* aDocument,
                       nsComputedDOMStyle::StyleType aStyleType)
{
  RefPtr<nsComputedDOMStyle> computedStyle =
    new nsComputedDOMStyle(aElement, aPseudoElt, aDocument, aStyleType);
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
                  const nsCSSKTableEntry aTable[])
{
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);

  for (uint32_t i = 0, i_end = aLayers.*aCount; i < i_end; ++i) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    val->SetIdent(nsCSSProps::ValueToKeywordEnum(aLayers.mLayers[i].*aMember, aTable));
    valueList->AppendCSSValue(val.forget());
  }

  return valueList.forget();
}


// Whether aDocument needs to restyle for aElement
static bool
DocumentNeedsRestyle(
  const nsIDocument* aDocument,
  Element* aElement,
  nsAtom* aPseudo)
{
  nsIPresShell* shell = aDocument->GetShell();
  if (!shell) {
    return true;
  }

  nsPresContext* presContext = shell->GetPresContext();
  MOZ_ASSERT(presContext);

  // Unfortunately we don't know if the sheet change affects mElement or not, so
  // just assume it will and that we need to flush normally.
  ServoStyleSet* styleSet = shell->StyleSet();
  if (styleSet->StyleSheetsHaveChanged()) {
    return true;
  }

  // Pending media query updates can definitely change style on the element. For
  // example, if you change the zoom factor and then call getComputedStyle, you
  // should be able to observe the style with the new media queries.
  //
  // TODO(emilio): Does this need to also check the user font set? (it affects
  // ch / ex units).
  if (presContext->HasPendingMediaQueryUpdates()) {
    // So gotta flush.
    return true;
  }

  // If the pseudo-element is animating, make sure to flush.
  if (aElement->MayHaveAnimations() && aPseudo) {
    if (aPseudo == nsCSSPseudoElements::before) {
      if (EffectSet::GetEffectSet(aElement, CSSPseudoElementType::before)) {
        return true;
      }
    } else if (aPseudo == nsCSSPseudoElements::after) {
      if (EffectSet::GetEffectSet(aElement, CSSPseudoElementType::after)) {
        return true;
      }
    }
  }

  // For Servo, we need to process the restyle-hint-invalidations first, to
  // expand LaterSiblings hint, so that we can look whether ancestors need
  // restyling.
  RestyleManager* restyleManager = presContext->RestyleManager();
  restyleManager->ProcessAllPendingAttributeAndStateInvalidations();

  if (!presContext->EffectCompositor()->HasPendingStyleUpdates() &&
      !aDocument->GetServoRestyleRoot()) {
    return false;
  }

  // Then if there is a restyle root, we check if the root is an ancestor of
  // this content. If it is not, then we don't need to restyle immediately.
  // Note this is different from Gecko: we only check if any ancestor needs
  // to restyle _itself_, not descendants, since dirty descendants can be
  // another subtree.
  return restyleManager->HasPendingRestyleAncestor(aElement);
}

/**
 * An object that represents the ordered set of properties that are exposed on
 * an nsComputedDOMStyle object and how their computed values can be obtained.
 */
struct ComputedStyleMap
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
                                      CSSPropFlags::GetCSNeedsLayoutFlush);
    }

    bool IsEnabled() const
    {
      return nsCSSProps::IsEnabled(mProperty, CSSEnabledState::eForAllContent);
    }
  };

  // This generated file includes definition of kEntries which is typed
  // Entry[] and used below, so this #include has to be put here.
#include "nsComputedDOMStyleGenerated.cpp"

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
   * The number of properties that should be exposed on an nsComputedDOMStyle.
   * This will be less than eComputedStyleProperty_COUNT if some property
   * prefs are disabled.  A value of 0 indicates that it and mIndexMap are out
   * of date.
   */
  uint32_t mExposedPropertyCount;

  /**
   * A map of indexes on the nsComputedDOMStyle object to indexes into kEntries.
   */
  uint32_t mIndexMap[ArrayLength(kEntries)];

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

constexpr ComputedStyleMap::Entry
ComputedStyleMap::kEntries[ArrayLength(kEntries)];

void
ComputedStyleMap::Update()
{
  if (!IsDirty()) {
    return;
  }

  uint32_t index = 0;
  for (uint32_t i = 0; i < ArrayLength(kEntries); i++) {
    if (kEntries[i].IsEnabled()) {
      mIndexMap[index++] = i;
    }
  }
  mExposedPropertyCount = index;
}

nsComputedDOMStyle::nsComputedDOMStyle(dom::Element* aElement,
                                       const nsAString& aPseudoElt,
                                       nsIDocument* aDocument,
                                       StyleType aStyleType)
  : mDocumentWeak(nullptr)
  , mOuterFrame(nullptr)
  , mInnerFrame(nullptr)
  , mPresShell(nullptr)
  , mStyleType(aStyleType)
  , mComputedStyleGeneration(0)
  , mExposeVisitedStyle(false)
  , mResolvedComputedStyle(false)
#ifdef DEBUG
  , mFlushedPendingReflows(false)
#endif
{
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aDocument);
  // TODO(emilio, bug 548397, https://github.com/w3c/csswg-drafts/issues/2403):
  // Should use aElement->OwnerDoc() instead.
  mDocumentWeak = do_GetWeakReference(aDocument);
  mElement = aElement;
  mPseudo = nsCSSPseudoElements::GetPseudoAtom(aPseudoElt);
}

nsComputedDOMStyle::~nsComputedDOMStyle()
{
  MOZ_ASSERT(!mResolvedComputedStyle,
             "Should have called ClearComputedStyle() during last release.");
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsComputedDOMStyle)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsComputedDOMStyle)
  tmp->ClearComputedStyle();  // remove observer before clearing mElement
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsComputedDOMStyle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mElement)
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


NS_IMPL_MAIN_THREAD_ONLY_CYCLE_COLLECTING_ADDREF(nsComputedDOMStyle)
NS_IMPL_MAIN_THREAD_ONLY_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(
  nsComputedDOMStyle, ClearComputedStyle())

nsresult
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

nsresult
nsComputedDOMStyle::SetPropertyValue(const nsCSSPropertyID aPropID,
                                     const nsAString& aValue,
                                     nsIPrincipal* aSubjectPrincipal)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

void
nsComputedDOMStyle::GetCssText(nsAString& aCssText)
{
  aCssText.Truncate();
}

void
nsComputedDOMStyle::SetCssText(const nsAString& aCssText,
                               nsIPrincipal* aSubjectPrincipal,
                               ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
}

uint32_t
nsComputedDOMStyle::Length()
{
  // Make sure we have up to date style so that we can include custom
  // properties.
  UpdateCurrentStyleSources(false);
  if (!mComputedStyle) {
    return 0;
  }

  uint32_t length =
    GetComputedStyleMap()->Length() +
    Servo_GetCustomPropertiesCount(mComputedStyle);

  ClearCurrentStyleSources();

  return length;
}

css::Rule*
nsComputedDOMStyle::GetParentRule()
{
  return nullptr;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPropertyValue(const nsAString& aPropertyName,
                                     nsAString& aReturn)
{
  aReturn.Truncate();

  nsCSSPropertyID prop =
    nsCSSProps::LookupProperty(aPropertyName, CSSEnabledState::eForAllContent);

  const ComputedStyleMap::Entry* entry = nullptr;
  if (prop != eCSSPropertyExtra_variable) {
    entry = GetComputedStyleMap()->FindEntryForProperty(prop);
    if (!entry) {
      return NS_OK;
    }
  }

  const bool layoutFlushIsNeeded = entry && entry->IsLayoutFlushNeeded();
  UpdateCurrentStyleSources(layoutFlushIsNeeded);
  if (!mComputedStyle) {
    return NS_OK;
  }

  auto cleanup = mozilla::MakeScopeExit([&] {
    ClearCurrentStyleSources();
  });

  if (!entry) {
    MOZ_ASSERT(nsCSSProps::IsCustomPropertyName(aPropertyName));
    const nsAString& name =
      Substring(aPropertyName, CSS_CUSTOM_NAME_PREFIX_LENGTH);
    Servo_GetCustomPropertyValue(mComputedStyle, &name, &aReturn);
    return NS_OK;
  }

  if (nsCSSProps::PropHasFlags(prop, CSSPropFlags::IsLogical)) {
    MOZ_ASSERT(entry);
    MOZ_ASSERT(entry->mGetter == &nsComputedDOMStyle::DummyGetter);

    prop = Servo_ResolveLogicalProperty(prop, mComputedStyle);
    entry = GetComputedStyleMap()->FindEntryForProperty(prop);

    MOZ_ASSERT(layoutFlushIsNeeded == entry->IsLayoutFlushNeeded(),
               "Logical and physical property don't agree on whether layout is "
               "needed");
  }

  if (!nsCSSProps::PropHasFlags(prop, CSSPropFlags::SerializedByServo)) {
    if (RefPtr<CSSValue> value = (this->*entry->mGetter)()) {
      ErrorResult rv;
      nsString text;
      value->GetCssText(text, rv);
      aReturn.Assign(text);
      return rv.StealNSResult();
    }
    return NS_OK;
  }

  MOZ_ASSERT(entry->mGetter == &nsComputedDOMStyle::DummyGetter);
  Servo_GetPropertyValue(mComputedStyle, prop, &aReturn);
  return NS_OK;
}

/* static */
already_AddRefed<ComputedStyle>
nsComputedDOMStyle::GetComputedStyle(Element* aElement,
                                     nsAtom* aPseudo,
                                     StyleType aStyleType)
{
  if (nsIDocument* doc = aElement->GetComposedDoc()) {
    doc->FlushPendingNotifications(FlushType::Style);
  }
  return GetComputedStyleNoFlush(aElement, aPseudo, aStyleType);
}


/**
 * The following function checks whether we need to explicitly resolve the style
 * again, even though we have a style coming from the frame.
 *
 * This basically checks whether the style is or may be under a ::first-line or
 * ::first-letter frame, in which case we can't return the frame style, and we
 * need to resolve it. See bug 505515.
 */
static bool
MustReresolveStyle(const mozilla::ComputedStyle* aStyle)
{
  MOZ_ASSERT(aStyle);

  // TODO(emilio): We may want to avoid re-resolving pseudo-element styles
  // more often.
  return aStyle->HasPseudoElementData() && !aStyle->GetPseudo();
}

static inline CSSPseudoElementType
GetPseudoType(nsAtom* aPseudo)
{
  if (!aPseudo) {
    return CSSPseudoElementType::NotPseudo;
  }
  // FIXME(emilio, bug 1433439): The eIgnoreEnabledState thing is dubious.
  return nsCSSPseudoElements::GetPseudoType(
    aPseudo, CSSEnabledState::eIgnoreEnabledState);
}

already_AddRefed<ComputedStyle>
nsComputedDOMStyle::DoGetComputedStyleNoFlush(Element* aElement,
                                              nsAtom* aPseudo,
                                              nsIPresShell* aPresShell,
                                              StyleType aStyleType)
{
  MOZ_ASSERT(aElement, "NULL element");

  // If the content has a pres shell, we must use it.  Otherwise we'd
  // potentially mix rule trees by using the wrong pres shell's style
  // set.  Using the pres shell from the content also means that any
  // content that's actually *in* a document will get the style from the
  // correct document.
  nsIPresShell* presShell = nsContentUtils::GetPresShellForContent(aElement);
  bool inDocWithShell = true;
  if (!presShell) {
    inDocWithShell = false;
    presShell = aPresShell;
    if (!presShell) {
      return nullptr;
    }
  }

  CSSPseudoElementType pseudoType = GetPseudoType(aPseudo);
  if (aPseudo && pseudoType >= CSSPseudoElementType::Count) {
    return nullptr;
  }

  if (aElement->IsInNativeAnonymousSubtree() && !aElement->IsInComposedDoc()) {
    // Normal web content can't access NAC, but Accessibility, DevTools and
    // Editor use this same API and this may get called for anonymous content.
    // Computing the style of a pseudo-element that doesn't have a parent doesn't
    // really make sense.
    return nullptr;
  }

  // XXX the !aElement->IsHTMLElement(nsGkAtoms::area)
  // check is needed due to bug 135040 (to avoid using
  // mPrimaryFrame). Remove it once that's fixed.
  if (inDocWithShell &&
      aStyleType == eAll &&
      !aElement->IsHTMLElement(nsGkAtoms::area)) {
    nsIFrame* frame = nullptr;
    if (aPseudo == nsCSSPseudoElements::before) {
      frame = nsLayoutUtils::GetBeforeFrame(aElement);
    } else if (aPseudo == nsCSSPseudoElements::after) {
      frame = nsLayoutUtils::GetAfterFrame(aElement);
    } else if (!aPseudo) {
      frame = nsLayoutUtils::GetStyleFrame(aElement);
    }
    if (frame) {
      ComputedStyle* result = frame->Style();
      // Don't use the style if it was influenced by pseudo-elements, since then
      // it's not the primary style for this element / pseudo.
      if (!MustReresolveStyle(result)) {
        RefPtr<ComputedStyle> ret = result;
        return ret.forget();
      }
    }
  }

  // No frame has been created, or we have a pseudo, or we're looking
  // for the default style, so resolve the style ourselves.
  ServoStyleSet* styleSet = presShell->StyleSet();

  StyleRuleInclusion rules = aStyleType == eDefaultOnly
                             ? StyleRuleInclusion::DefaultOnly
                             : StyleRuleInclusion::All;
  RefPtr<ComputedStyle> result =
     styleSet->ResolveStyleLazily(aElement, pseudoType, rules);
  return result.forget();
}

already_AddRefed<ComputedStyle>
nsComputedDOMStyle::GetUnanimatedComputedStyleNoFlush(Element* aElement,
                                                      nsAtom* aPseudo)
{
  RefPtr<ComputedStyle> style = GetComputedStyleNoFlush(aElement, aPseudo);
  if (!style) {
    return nullptr;
  }

  CSSPseudoElementType pseudoType = GetPseudoType(aPseudo);
  nsIPresShell* shell = aElement->OwnerDoc()->GetShell();
  MOZ_ASSERT(shell, "How in the world did we get a style a few lines above?");

  Element* elementOrPseudoElement =
    EffectCompositor::GetElementToRestyle(aElement, pseudoType);
  if (!elementOrPseudoElement) {
    return nullptr;
  }

  return shell->StyleSet()->
    GetBaseContextForElement(elementOrPseudoElement, style);
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

static void
AddImageURL(nsIURI& aURI, nsTArray<nsString>& aURLs)
{
  nsAutoCString spec;
  nsresult rv = aURI.GetSpec(spec);
  if (NS_FAILED(rv)) {
    return;
  }

  aURLs.AppendElement(NS_ConvertUTF8toUTF16(spec));
}


static void
AddImageURL(const css::URLValueData& aURL, nsTArray<nsString>& aURLs)
{
  if (aURL.IsLocalRef()) {
    return;
  }

  if (nsIURI* uri = aURL.GetURI()) {
    AddImageURL(*uri, aURLs);
  }
}


static void
AddImageURL(const nsStyleImageRequest& aRequest, nsTArray<nsString>& aURLs)
{
  if (auto* value = aRequest.GetImageValue()) {
    AddImageURL(*value, aURLs);
  }
}

static void
AddImageURL(const nsStyleImage& aImage, nsTArray<nsString>& aURLs)
{
  if (auto* urlValue = aImage.GetURLValue()) {
    AddImageURL(*urlValue, aURLs);
  }
}

static void
AddImageURL(const StyleShapeSource& aShapeSource, nsTArray<nsString>& aURLs)
{
  switch (aShapeSource.GetType()) {
    case StyleShapeSourceType::URL:
      AddImageURL(*aShapeSource.GetURL(), aURLs);
      break;
    case StyleShapeSourceType::Image:
      AddImageURL(*aShapeSource.GetShapeImage(), aURLs);
      break;
    default:
      break;
  }
}

static void
AddImageURLs(const nsStyleImageLayers& aLayers, nsTArray<nsString>& aURLs)
{
  for (auto i : IntegerRange(aLayers.mLayers.Length())) {
    AddImageURL(aLayers.mLayers[i].mImage, aURLs);
  }
}

// FIXME(stylo-everywhere): This should be `const ComputedStyle&`.
static void
CollectImageURLsForProperty(nsCSSPropertyID aProp,
                            ComputedStyle& aStyle,
                            nsTArray<nsString>& aURLs)
{
  if (nsCSSProps::IsShorthand(aProp)) {
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aProp, CSSEnabledState::eInChrome) {
      CollectImageURLsForProperty(*p, aStyle, aURLs);
    }
    return;
  }

  switch (aProp) {
    case eCSSProperty_cursor:
      for (auto& image : aStyle.StyleUserInterface()->mCursorImages) {
        AddImageURL(*image.mImage, aURLs);
      }
      break;
    case eCSSProperty_background_image:
      AddImageURLs(aStyle.StyleBackground()->mImage, aURLs);
      break;
    case eCSSProperty_mask_clip:
      AddImageURLs(aStyle.StyleSVGReset()->mMask, aURLs);
      break;
    case eCSSProperty_list_style_image:
      if (nsStyleImageRequest* image = aStyle.StyleList()->mListStyleImage) {
        AddImageURL(*image, aURLs);
      }
      break;
    case eCSSProperty_border_image_source:
      AddImageURL(aStyle.StyleBorder()->mBorderImageSource, aURLs);
      break;
    case eCSSProperty_clip_path:
      AddImageURL(aStyle.StyleSVGReset()->mClipPath, aURLs);
      break;
    case eCSSProperty_shape_outside:
      AddImageURL(aStyle.StyleDisplay()->mShapeOutside, aURLs);
      break;
    default:
      break;
  }
}

void
nsComputedDOMStyle::GetCSSImageURLs(const nsAString& aPropertyName,
                                    nsTArray<nsString>& aImageURLs,
                                    mozilla::ErrorResult& aRv)
{
  nsCSSPropertyID prop =
    nsCSSProps::LookupProperty(aPropertyName, CSSEnabledState::eInChrome);
  if (prop == eCSSProperty_UNKNOWN) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  UpdateCurrentStyleSources(false);

  if (!mComputedStyle) {
    return;
  }

  CollectImageURLsForProperty(prop, *mComputedStyle, aImageURLs);
  ClearCurrentStyleSources();
}

// nsDOMCSSDeclaration abstract methods which should never be called
// on a nsComputedDOMStyle object, but must be defined to avoid
// compile errors.
DeclarationBlock*
nsComputedDOMStyle::GetOrCreateCSSDeclaration(Operation aOperation,
                                              DeclarationBlock** aCreated)
{
  MOZ_CRASH("called nsComputedDOMStyle::GetCSSDeclaration");
}

nsresult
nsComputedDOMStyle::SetCSSDeclaration(DeclarationBlock*,
                                      MutationClosureData*)
{
  MOZ_CRASH("called nsComputedDOMStyle::SetCSSDeclaration");
}

nsIDocument*
nsComputedDOMStyle::DocToUpdate()
{
  MOZ_CRASH("called nsComputedDOMStyle::DocToUpdate");
}

nsDOMCSSDeclaration::ParsingEnvironment
nsComputedDOMStyle::GetParsingEnvironment(
  nsIPrincipal* aSubjectPrincipal) const
{
  MOZ_CRASH("called nsComputedDOMStyle::GetParsingEnvironment");
}

void
nsComputedDOMStyle::ClearComputedStyle()
{
  if (mResolvedComputedStyle) {
    mResolvedComputedStyle = false;
    mElement->RemoveMutationObserver(this);
  }
  mComputedStyle = nullptr;
}

void
nsComputedDOMStyle::SetResolvedComputedStyle(RefPtr<ComputedStyle>&& aContext,
                                            uint64_t aGeneration)
{
  if (!mResolvedComputedStyle) {
    mResolvedComputedStyle = true;
    mElement->AddMutationObserver(this);
  }
  mComputedStyle = aContext;
  mComputedStyleGeneration = aGeneration;
}

void
nsComputedDOMStyle::SetFrameComputedStyle(mozilla::ComputedStyle* aStyle,
                                         uint64_t aGeneration)
{
  ClearComputedStyle();
  mComputedStyle = aStyle;
  mComputedStyleGeneration = aGeneration;
}

bool
nsComputedDOMStyle::NeedsToFlush(nsIDocument* aDocument) const
{
  // If mElement is not in the same document, we could do some checks to know if
  // there are some pending restyles can be ignored across documents (since we
  // will use the caller document's style), but it can be complicated and should
  // be an edge case, so we just don't bother to do the optimization in this
  // case.
  //
  // FIXME(emilio): This is likely to want GetComposedDoc() instead of
  // OwnerDoc().
  if (aDocument != mElement->OwnerDoc()) {
    return true;
  }
  if (DocumentNeedsRestyle(aDocument, mElement, mPseudo)) {
    return true;
  }
  // If parent document is there, also needs to check if there is some change
  // that needs to flush this document (e.g. size change for iframe).
  while (nsIDocument* parentDocument = aDocument->GetParentDocument()) {
    Element* element = parentDocument->FindContentForSubDocument(aDocument);
    if (DocumentNeedsRestyle(parentDocument, element, nullptr)) {
      return true;
    }
    aDocument = parentDocument;
  }

  return false;
}

void
nsComputedDOMStyle::UpdateCurrentStyleSources(bool aNeedsLayoutFlush)
{
  nsCOMPtr<nsIDocument> document = do_QueryReferent(mDocumentWeak);
  if (!document) {
    ClearComputedStyle();
    return;
  }

  // TODO(emilio): We may want to handle a few special-cases here:
  //
  //  * https://github.com/w3c/csswg-drafts/issues/1964
  //  * https://github.com/w3c/csswg-drafts/issues/1548

  // If the property we are computing relies on layout, then we must flush.
  const bool needsToFlush = aNeedsLayoutFlush || NeedsToFlush(document);
  if (needsToFlush) {
    // Flush _before_ getting the presshell, since that could create a new
    // presshell.  Also note that we want to flush the style on the document
    // we're computing style in, not on the document mElement is in -- the two
    // may be different.
    document->FlushPendingNotifications(
      aNeedsLayoutFlush ? FlushType::Layout : FlushType::Style);
  }

#ifdef DEBUG
  mFlushedPendingReflows = aNeedsLayoutFlush;
#endif

  nsCOMPtr<nsIPresShell> presShellForContent =
    nsContentUtils::GetPresShellForContent(mElement);
  if (presShellForContent && presShellForContent->GetDocument() != document) {
    presShellForContent->GetDocument()->FlushPendingNotifications(FlushType::Style);
    if (presShellForContent->IsDestroying()) {
      presShellForContent = nullptr;
    }
  }

  mPresShell = document->GetShell();
  if (!mPresShell || !mPresShell->GetPresContext()) {
    ClearComputedStyle();
    return;
  }

  // We need to use GetUndisplayedRestyleGeneration instead of
  // GetRestyleGeneration, because the caching of mComputedStyle is an
  // optimization that is useful only for displayed elements.
  // For undisplayed elements we need to take into account any DOM changes that
  // might cause a restyle, because Servo will not increase the generation for
  // undisplayed elements.
  // As for Gecko, GetUndisplayedRestyleGeneration is effectively equal to
  // GetRestyleGeneration, since the generation is incremented whenever we
  // process restyles.
  uint64_t currentGeneration =
    mPresShell->GetPresContext()->GetUndisplayedRestyleGeneration();

  if (mComputedStyle) {
    // We can't rely on the undisplayed restyle generation if mElement is
    // out-of-document, since that generation is not incremented for DOM changes
    // on out-of-document elements.
    //
    // So we always need to update the style to ensure it it up-to-date.
    if (mComputedStyleGeneration == currentGeneration &&
        mElement->IsInComposedDoc()) {
      // Our cached style is still valid.
      return;
    }
    // We've processed some restyles, so the cached style might be out of date.
    mComputedStyle = nullptr;
  }

  // XXX the !mElement->IsHTMLElement(nsGkAtoms::area)
  // check is needed due to bug 135040 (to avoid using
  // mPrimaryFrame). Remove it once that's fixed.
  if (mStyleType == eAll && !mElement->IsHTMLElement(nsGkAtoms::area)) {
    mOuterFrame = nullptr;

    if (!mPseudo) {
      mOuterFrame = mElement->GetPrimaryFrame();
    } else if (mPseudo == nsCSSPseudoElements::before ||
               mPseudo == nsCSSPseudoElements::after) {
      nsAtom* property = mPseudo == nsCSSPseudoElements::before
                            ? nsGkAtoms::beforePseudoProperty
                            : nsGkAtoms::afterPseudoProperty;

      auto* pseudo = static_cast<Element*>(mElement->GetProperty(property));
      mOuterFrame = pseudo ? pseudo->GetPrimaryFrame() : nullptr;
    }

    mInnerFrame = mOuterFrame;
    if (mOuterFrame) {
      LayoutFrameType type = mOuterFrame->Type();
      if (type == LayoutFrameType::TableWrapper) {
        // If the frame is a table wrapper frame then we should get the style
        // from the inner table frame.
        mInnerFrame = mOuterFrame->PrincipalChildList().FirstChild();
        NS_ASSERTION(mInnerFrame, "table wrapper must have an inner");
        NS_ASSERTION(!mInnerFrame->GetNextSibling(),
                     "table wrapper frames should have just one child, "
                     "the inner table");
      }

      SetFrameComputedStyle(mInnerFrame->Style(), currentGeneration);
      NS_ASSERTION(mComputedStyle, "Frame without style?");
    }
  }

  if (!mComputedStyle || MustReresolveStyle(mComputedStyle)) {
    // Need to resolve a style.
    RefPtr<ComputedStyle> resolvedComputedStyle =
      DoGetComputedStyleNoFlush(
          mElement,
          mPseudo,
          presShellForContent ? presShellForContent.get() : mPresShell,
          mStyleType);
    if (!resolvedComputedStyle) {
      ClearComputedStyle();
      return;
    }

    // No need to re-get the generation, even though GetComputedStyle
    // will flush, since we flushed style at the top of this function.
    // We don't need to check this if we only flushed the parent.
    NS_ASSERTION(!needsToFlush ||
                 currentGeneration ==
                     mPresShell->GetPresContext()->GetUndisplayedRestyleGeneration(),
                   "why should we have flushed style again?");

    SetResolvedComputedStyle(std::move(resolvedComputedStyle), currentGeneration);
    NS_ASSERTION(mPseudo || !mComputedStyle->HasPseudoElementData(),
                 "should not have pseudo-element data");
  }

  // mExposeVisitedStyle is set to true only by testing APIs that
  // require chrome privilege.
  MOZ_ASSERT(!mExposeVisitedStyle || nsContentUtils::IsCallerChrome(),
             "mExposeVisitedStyle set incorrectly");
  if (mExposeVisitedStyle && mComputedStyle->RelevantLinkVisited()) {
    if (ComputedStyle* styleIfVisited = mComputedStyle->GetStyleIfVisited()) {
      mComputedStyle = styleIfVisited;
    }
  }
}

void
nsComputedDOMStyle::ClearCurrentStyleSources()
{
  // Release the current style if we got it off the frame.
  //
  // For a style we resolved, keep it around so that we can re-use it next time
  // this object is queried, but not if it-s a re-resolved style because we were
  // inside a pseudo-element.
  if (!mResolvedComputedStyle || mOuterFrame) {
    ClearComputedStyle();
  }

  mOuterFrame = nullptr;
  mInnerFrame = nullptr;
  mPresShell = nullptr;
}

NS_IMETHODIMP
nsComputedDOMStyle::RemoveProperty(const nsAString& aPropertyName,
                                   nsAString& aReturn)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


void
nsComputedDOMStyle::GetPropertyPriority(const nsAString& aPropertyName,
                                        nsAString& aReturn)
{
  aReturn.Truncate();
}

NS_IMETHODIMP
nsComputedDOMStyle::SetProperty(const nsAString& aPropertyName,
                                const nsAString& aValue,
                                const nsAString& aPriority,
                                nsIPrincipal* aSubjectPrincipal)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

void
nsComputedDOMStyle::IndexedGetter(uint32_t   aIndex,
                                  bool&      aFound,
                                  nsAString& aPropName)
{
  ComputedStyleMap* map = GetComputedStyleMap();
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
  if (!mComputedStyle) {
    aFound = false;
    return;
  }

  uint32_t count =
    Servo_GetCustomPropertiesCount(mComputedStyle);

  const uint32_t index = aIndex - length;
  if (index < count) {
    aFound = true;
    nsString varName;
    Servo_GetCustomPropertyNameAt(mComputedStyle, index, &varName);
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

  if (display->mBinding && display->mBinding->GetURI()) {
    val->SetURI(display->mBinding->GetURI());
  } else {
    val->SetIdent(eCSSKeyword_none);
  }

  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBottom()
{
  return GetOffsetWidthFor(eSideBottom);
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
  SetToRGBAColor(aValue, aColor.CalcColor(mComputedStyle));
}

void
nsComputedDOMStyle::SetValueForWidgetColor(nsROCSSPrimitiveValue* aValue,
                                           const StyleComplexColor& aColor,
                                           uint8_t aWidgetType)
{
  if (!aColor.IsAuto()) {
    SetToRGBAColor(aValue, aColor.CalcColor(mComputedStyle));
    return;
  }
  nsPresContext* presContext = mPresShell->GetPresContext();
  MOZ_ASSERT(presContext);
  if (nsContentUtils::ShouldResistFingerprinting(presContext->GetDocShell())) {
    // Return transparent when resisting fingerprinting.
    SetToRGBAColor(aValue, NS_RGBA(0, 0, 0, 0));
    return;
  }
  if (nsITheme* theme = presContext->GetTheme()) {
    nscolor color = theme->GetWidgetAutoColor(mComputedStyle, aWidgetType);
    SetToRGBAColor(aValue, color);
  } else {
    // If we don't have theme, we don't know what value it should be,
    // just give it a transparent fallback.
    SetToRGBAColor(aValue, NS_RGBA(0, 0, 0, 0));
  }
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetColor()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetToRGBAColor(val, StyleColor()->mColor);
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
nsComputedDOMStyle::DoGetColumnRuleWidth()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetAppUnits(StyleColumn()->GetComputedColumnRuleWidth());
  return val.forget();
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
nsComputedDOMStyle::DoGetTransformStyle()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
      nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mTransformStyle,
                                     nsCSSProps::kTransformStyleKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTransform()
{
  const nsStyleDisplay* display = StyleDisplay();
  return GetTransformValue(display->mSpecifiedTransform);
}

static already_AddRefed<CSSValue>
ReadIndividualTransformValue(nsCSSValueSharedList* aList,
                             const std::function<void(const nsCSSValue::Array*,
                                                      nsString&)>& aCallback)
{
  if (!aList) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_none);
    return val.forget();
  }

  nsAutoString result;
  const nsCSSValue::Array* data = aList->mHead->mValue.GetArrayValue();
  aCallback(data, result);

  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetString(result);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetTranslate()
{
  typedef nsStyleTransformMatrix::TransformReferenceBox TransformReferenceBox;

  RefPtr<nsComputedDOMStyle> self(this);
  return ReadIndividualTransformValue(StyleDisplay()->mSpecifiedTranslate,
    [self](const nsCSSValue::Array* aData, nsString& aResult) {
      TransformReferenceBox refBox(self->mInnerFrame, nsSize(0, 0));

      // Even though the spec doesn't say to resolve percentage values, Blink
      // and Edge do and so until that is clarified we do as well:
      //
      // https://github.com/w3c/csswg-drafts/issues/2124
      switch (nsStyleTransformMatrix::TransformFunctionOf(aData)) {
        /* translate : <length-percentage> */
        case eCSSKeyword_translatex: {
          MOZ_ASSERT(aData->Count() == 2, "Invalid array!");
          float tx = ProcessTranslatePart(aData->Item(1),
                                          &refBox,
                                          &TransformReferenceBox::Width);
          aResult.AppendFloat(tx);
          aResult.AppendLiteral("px");
          break;
        }
        /* translate : <length-percentage> <length-percentage> */
        case eCSSKeyword_translate: {
          MOZ_ASSERT(aData->Count() == 3, "Invalid array!");
          float tx = ProcessTranslatePart(aData->Item(1),
                                          &refBox,
                                          &TransformReferenceBox::Width);
          aResult.AppendFloat(tx);
          aResult.AppendLiteral("px");

          float ty = ProcessTranslatePart(aData->Item(2),
                                          &refBox,
                                          &TransformReferenceBox::Height);
          if (ty != 0) {
            aResult.AppendLiteral(" ");
            aResult.AppendFloat(ty);
            aResult.AppendLiteral("px");
          }
          break;
        }
        /* translate : <length-percentage> <length-percentage> <length>*/
        case eCSSKeyword_translate3d: {
          MOZ_ASSERT(aData->Count() == 4, "Invalid array!");
          float tx = ProcessTranslatePart(aData->Item(1),
                                          &refBox,
                                          &TransformReferenceBox::Width);
          aResult.AppendFloat(tx);
          aResult.AppendLiteral("px");

          float ty = ProcessTranslatePart(aData->Item(2),
                                          &refBox,
                                          &TransformReferenceBox::Height);

          float tz = ProcessTranslatePart(aData->Item(3),
                                          &refBox,
                                          nullptr);
          if (ty != 0. || tz != 0.) {
            aResult.AppendLiteral(" ");
            aResult.AppendFloat(ty);
            aResult.AppendLiteral("px");
          }
          if (tz != 0.) {
            aResult.AppendLiteral(" ");
            aResult.AppendFloat(tz);
            aResult.AppendLiteral("px");
          }

          break;
        }
        default:
          MOZ_ASSERT_UNREACHABLE("Unexpected CSS keyword.");
      }
    });
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetScale()
{
  return ReadIndividualTransformValue(StyleDisplay()->mSpecifiedScale,
    [](const nsCSSValue::Array* aData, nsString& aResult) {
      switch (nsStyleTransformMatrix::TransformFunctionOf(aData)) {
        /* scale : <number> */
        case eCSSKeyword_scalex:
          MOZ_ASSERT(aData->Count() == 2, "Invalid array!");
          aResult.AppendFloat(aData->Item(1).GetFloatValue());
          break;
        /* scale : <number> <number>*/
        case eCSSKeyword_scale: {
          MOZ_ASSERT(aData->Count() == 3, "Invalid array!");
          aResult.AppendFloat(aData->Item(1).GetFloatValue());

          float sy = aData->Item(2).GetFloatValue();
          if (sy != 1.) {
            aResult.AppendLiteral(" ");
            aResult.AppendFloat(sy);
          }
          break;
        }
        /* scale : <number> <number> <number> */
        case eCSSKeyword_scale3d: {
          MOZ_ASSERT(aData->Count() == 4, "Invalid array!");
          aResult.AppendFloat(aData->Item(1).GetFloatValue());

          float sy = aData->Item(2).GetFloatValue();
          float sz = aData->Item(3).GetFloatValue();

          if (sy != 1. || sz != 1.) {
            aResult.AppendLiteral(" ");
            aResult.AppendFloat(sy);
          }
          if (sz != 1.) {
            aResult.AppendLiteral(" ");
            aResult.AppendFloat(sz);
          }
          break;
        }
        default:
          MOZ_ASSERT_UNREACHABLE("Unexpected CSS keyword.");
      }
    });
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetRotate()
{
  return ReadIndividualTransformValue(StyleDisplay()->mSpecifiedRotate,
    [](const nsCSSValue::Array* aData, nsString& aResult) {

      switch (nsStyleTransformMatrix::TransformFunctionOf(aData)) {
        /* rotate : <angle> */
        case eCSSKeyword_rotate: {
          MOZ_ASSERT(aData->Count() == 2, "Invalid array!");
          float theta = aData->Item(1).GetAngleValueInDegrees();
          aResult.AppendFloat(theta);
          aResult.AppendLiteral("deg");
          break;
        }
        /* rotate : <number> <number> <number> <angle> */
        case eCSSKeyword_rotate3d: {
          MOZ_ASSERT(aData->Count() == 5, "Invalid array!");
          float rx = aData->Item(1).GetFloatValue();
          float ry = aData->Item(2).GetFloatValue();
          float rz = aData->Item(3).GetFloatValue();
          if (rx != 0. || ry != 0. || rz != 1.) {
            aResult.AppendFloat(rx);
            aResult.AppendLiteral(" ");
            aResult.AppendFloat(ry);
            aResult.AppendLiteral(" ");
            aResult.AppendFloat(rz);
            aResult.AppendLiteral(" ");
          }
          float theta = aData->Item(4).GetAngleValueInDegrees();
          aResult.AppendFloat(theta);
          aResult.AppendLiteral("deg");
          break;
        }
        default:
          MOZ_ASSERT_UNREACHABLE("Unexpected CSS keyword.");
      }
    });
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

  if (xValue == 0.5f &&
      yValue == (aGradient->mLegacySyntax ? 0.0f : 1.0f)) {
    // omit "to bottom" in modern syntax, "top" in legacy syntax
    return;
  }
  NS_ASSERTION(yValue != 0.5f || xValue != 0.5f, "invalid box position");

  if (!aGradient->mLegacySyntax) {
    // Modern syntax explicitly includes the word "to". Old syntax does not
    // (and is implicitly "from" the given position instead).
    aString.AppendLiteral("to ");
  }

  if (xValue == 0.0f) {
    aString.AppendLiteral("left");
  } else if (xValue == 1.0f) {
    aString.AppendLiteral("right");
  } else if (xValue != 0.5f) { // do not write "center" keyword
    MOZ_ASSERT_UNREACHABLE("invalid box position");
  }

  if (xValue != 0.5f && yValue != 0.5f) {
    // We're appending both an x-keyword and a y-keyword.
    // Add a space between them here.
    aString.AppendLiteral(" ");
  }

  if (yValue == 0.0f) {
    aString.AppendLiteral("top");
  } else if (yValue == 1.0f) {
    aString.AppendLiteral("bottom");
  } else if (yValue != 0.5f) { // do not write "center" keyword
    MOZ_ASSERT_UNREACHABLE("invalid box position");
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
               aGradient->mBgPosY.GetPercentValue() != (isRadial ? 0.5f : 0.0f)) {
      // [-vendor-]radial-gradient or -moz-linear-gradient, with
      // non-default box position, which we output here.
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
      SetValueFromComplexColor(tmpVal, stop.mColor);
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
      MOZ_ASSERT_UNREACHABLE("unexpected image type");
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

    const StyleImageLayerRepeat xRepeat = aLayers.mLayers[i].mRepeat.mXRepeat;
    const StyleImageLayerRepeat yRepeat = aLayers.mLayers[i].mRepeat.mYRepeat;

    bool hasContraction = true;
    unsigned contraction;
    if (xRepeat == yRepeat) {
      contraction = uint8_t(xRepeat);
    } else if (xRepeat == StyleImageLayerRepeat::Repeat &&
               yRepeat == StyleImageLayerRepeat::NoRepeat) {
      contraction = uint8_t(StyleImageLayerRepeat::RepeatX);
    } else if (xRepeat == StyleImageLayerRepeat::NoRepeat &&
               yRepeat == StyleImageLayerRepeat::Repeat) {
      contraction = uint8_t(StyleImageLayerRepeat::RepeatY);
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
  nsStyleUtil::AppendBitmaskCSSValue(nsCSSProps::kGridAutoFlowKTable,
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
    nsGridContainerFrame::GetGridFrameWithComputedInfo(mInnerFrame);

  if (gridFrame) {
    info = gridFrame->GetComputedTemplateColumns();
  }

  return GetGridTemplateColumnsRows(
    StylePosition()->GridTemplateColumns(), info);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetGridTemplateRows()
{
  const ComputedGridTrackInfo* info = nullptr;

  nsGridContainerFrame* gridFrame =
    nsGridContainerFrame::GetGridFrameWithComputedInfo(mInnerFrame);

  if (gridFrame) {
    info = gridFrame->GetComputedTemplateRows();
  }

  return GetGridTemplateColumnsRows(
    StylePosition()->GridTemplateRows(), info);
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
nsComputedDOMStyle::DoGetColumnGap()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  const auto& columnGap = StylePosition()->mColumnGap;
  if (columnGap.GetUnit() == eStyleUnit_Normal) {
    val->SetIdent(eCSSKeyword_normal);
  } else {
    SetValueToCoord(val, columnGap, true);
  }
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetRowGap()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  const auto& rowGap = StylePosition()->mRowGap;
  if (rowGap.GetUnit() == eStyleUnit_Normal) {
    val->SetIdent(eCSSKeyword_normal);
  } else {
    SetValueToCoord(val, rowGap, true);
  }
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
nsComputedDOMStyle::DoGetOverscrollBehaviorX()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mOverscrollBehaviorX,
                                   nsCSSProps::kOverscrollBehaviorKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetOverscrollBehaviorY()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mOverscrollBehaviorY,
                                   nsCSSProps::kOverscrollBehaviorKTable));
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
    SetCssTextToCoord(argumentString, aCoord, true);
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
nsComputedDOMStyle::DoGetScrollbarFaceColor()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueForWidgetColor(val, StyleUserInterface()->mScrollbarFaceColor,
                         NS_THEME_SCROLLBARTHUMB_VERTICAL);
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetScrollbarTrackColor()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueForWidgetColor(val, StyleUserInterface()->mScrollbarTrackColor,
                         NS_THEME_SCROLLBAR_VERTICAL);
  return val.forget();
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
    SetValueFromComplexColor(val, item->mColor);
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
nsComputedDOMStyle::DoGetBoxShadow()
{
  return GetCSSShadowArray(StyleEffects()->mBoxShadow, true);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetZIndex()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StylePosition()->mZIndex, false);
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
    nsStyleUtil::AppendBitmaskCSSValue(nsCSSProps::kTextDecorationLineKTable,
                                       intValue,
                                       NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE,
                                       NS_STYLE_TEXT_DECORATION_LINE_BLINK,
                                       decorationLineString);
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
  return GetCSSShadowArray(StyleText()->mTextShadow, false);
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
nsComputedDOMStyle::DoGetWebkitTextStrokeWidth()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetAppUnits(StyleText()->mWebkitTextStrokeWidth);
  return val.forget();
}

static_assert(NS_STYLE_UNICODE_BIDI_NORMAL == 0,
              "unicode-bidi style constants not as expected");

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
nsComputedDOMStyle::DoGetBoxFlex()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleXUL()->mBoxFlex);
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

void
nsComputedDOMStyle::AppendFourSideCoordValues(nsDOMCSSValueList* aList,
                                              const nsStyleSides& aValues)
{
  const nsStyleCoord& top = aValues.Get(eSideTop);
  const nsStyleCoord& right = aValues.Get(eSideRight);
  const nsStyleCoord& bottom = aValues.Get(eSideBottom);
  const nsStyleCoord& left = aValues.Get(eSideLeft);

  auto appendValue = [this, aList](const nsStyleCoord& value) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    SetValueToCoord(val, value, true);
    aList->AppendCSSValue(val.forget());
  };
  appendValue(top);
  if (top != right || top != bottom || top != left) {
    appendValue(right);
    if (top != bottom || right != left) {
      appendValue(bottom);
      if (right != left) {
        appendValue(left);
      }
    }
  }
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderImageSlice()
{
  const nsStyleBorder* border = StyleBorder();
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);
  AppendFourSideCoordValues(valueList, border->mBorderImageSlice);

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
  AppendFourSideCoordValues(valueList, border->mBorderImageWidth);
  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetBorderImageOutset()
{
  const nsStyleBorder* border = StyleBorder();
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);
  AppendFourSideCoordValues(valueList, border->mBorderImageOutset);
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
                  nullptr, nsCSSProps::kFlexBasisKTable);
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
  auto justify = StylePosition()->mJustifyItems;
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
nsComputedDOMStyle::DoGetForceBrokenImageIcon()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleUIReset()->mForceBrokenImageIcon);
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
                 "contain: strict should imply contain: size layout style paint");
    val->SetIdent(eCSSKeyword_strict);
  } else if (mask & NS_STYLE_CONTAIN_CONTENT) {
    NS_ASSERTION(mask == (NS_STYLE_CONTAIN_CONTENT | NS_STYLE_CONTAIN_CONTENT_BITS),
                 "contain: content should imply contain: layout style paint");
    val->SetIdent(eCSSKeyword_content);
  }  else {
    nsAutoString valueStr;
    nsStyleUtil::AppendBitmaskCSSValue(nsCSSProps::kContainKTable,
                                       mask,
                                       NS_STYLE_CONTAIN_SIZE, NS_STYLE_CONTAIN_PAINT,
                                       valueStr);
    val->SetString(valueStr);
  }

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
  const nsTArray<RefPtr<nsAtom>>& willChange = StyleDisplay()->mWillChange;

  if (willChange.IsEmpty()) {
    RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_auto);
    return val.forget();
  }

  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(true);
  for (const nsAtom* ident : willChange) {
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

  RefPtr<nsROCSSPrimitiveValue> overflowX = new nsROCSSPrimitiveValue;
  overflowX->SetIdent(
    nsCSSProps::ValueToKeywordEnum(display->mOverflowX,
                                   nsCSSProps::kOverflowKTable));
  if (display->mOverflowX == display->mOverflowY) {
    return overflowX.forget();
  }
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);
  valueList->AppendCSSValue(overflowX.forget());

  RefPtr<nsROCSSPrimitiveValue> overflowY= new nsROCSSPrimitiveValue;
  overflowY->SetIdent(
    nsCSSProps::ValueToKeywordEnum(display->mOverflowY,
                                   nsCSSProps::kOverflowKTable));
  valueList->AppendCSSValue(overflowY.forget());
  return valueList.forget();
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
nsComputedDOMStyle::DoGetOverflowClipBoxBlock()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mOverflowClipBoxBlock,
                                   nsCSSProps::kOverflowClipBoxKTable));
  return val.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetOverflowClipBoxInline()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mOverflowClipBoxInline,
                                   nsCSSProps::kOverflowClipBoxKTable));
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
  nsStyleUtil::AppendBitmaskCSSValue(nsCSSProps::kTouchActionKTable,
                                     intValue,
                                     NS_STYLE_TOUCH_ACTION_NONE,
                                     NS_STYLE_TOUCH_ACTION_MANIPULATION,
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
        !mInnerFrame->IsSVGOuterSVGFrame()) {

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
        !mInnerFrame->IsSVGOuterSVGFrame()) {

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

/**
 * This function indicates whether we should return "auto" as the
 * getComputedStyle() result for the (default) "min-width: auto" and
 * "min-height: auto" CSS values.
 *
 * As of this writing, the CSS Sizing draft spec says this "auto" value
 * *always* computes to itself.  However, for now, we only make it compute to
 * itself for grid and flex items (the containers where "auto" has special
 * significance), because those are the only areas where the CSSWG has actually
 * resolved on this "computes-to-itself" behavior. For elements in other sorts
 * of containers, this function returns false, which will make us resolve
 * "auto" to 0.
 */
bool
nsComputedDOMStyle::ShouldHonorMinSizeAutoInAxis(PhysicalAxis aAxis)
{
  return mOuterFrame && mOuterFrame->IsFlexOrGridItem();
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

  if (container->IsViewportFrame()) {
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

  nsPresContext* presContext = mPresShell->GetPresContext();

  // lie about font size inflation since we lie about font size (since
  // the inflation only applies to text)
  aCoord = ReflowInput::CalcLineHeight(mElement,
                                       mComputedStyle,
                                       presContext,
                                       blockHeight, 1.0f);

  // CalcLineHeight uses font->mFont.size, but we want to use
  // font->mSize as the font size.  Adjust for that.  Also adjust for
  // the text zoom, if any.
  const nsStyleFont* font = StyleFont();
  float fCoord = float(aCoord);
  if (font->mAllowZoom) {
    fCoord /= presContext->EffectiveTextZoom();
  }
  if (font->mFont.size != font->mSize) {
    fCoord = fCoord * (float(font->mSize) / float(font->mFont.size));
  }
  aCoord = NSToCoordRound(fCoord);

  return true;
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
  SetValueFromComplexColor(val, StyleBorder()->BorderColorFor(aSide));
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
  MOZ_ASSERT(aValue, "Must have a value to work with");

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
        nscoord val = aCoord.ComputeCoordPercentCalc(0);
        if (aClampNegativeCalc && val < 0) {
          MOZ_ASSERT(aCoord.IsCalcUnit(),
                     "parser should have rejected value");
          val = 0;
        }
        aValue->SetAppUnits(std::max(aMinAppUnits, std::min(val, aMaxAppUnits)));
      } else if (aPercentageBaseGetter &&
                 (this->*aPercentageBaseGetter)(percentageBase)) {
        nscoord val = aCoord.ComputeCoordPercentCalc(percentageBase);
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
  MOZ_ASSERT(aPercentageBaseGetter, "Must have a percentage base getter");
  if (aCoord.GetUnit() == eStyleUnit_Coord) {
    return aCoord.GetCoordValue();
  }
  if (aCoord.GetUnit() == eStyleUnit_Percent || aCoord.IsCalcUnit()) {
    nscoord percentageBase;
    if ((this->*aPercentageBaseGetter)(percentageBase)) {
      nscoord result = aCoord.ComputeCoordPercentCalc(percentageBase);
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
nsComputedDOMStyle::GetFallbackValue(const nsStyleSVGPaint* aPaint)
{
  RefPtr<nsROCSSPrimitiveValue> fallback = new nsROCSSPrimitiveValue;
  if (aPaint->GetFallbackType() == eStyleSVGFallbackType_Color) {
    SetToRGBAColor(fallback, aPaint->GetFallbackColor());
  } else {
    fallback->SetIdent(eCSSKeyword_none);
  }
  return fallback.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetSVGPaintFor(bool aFill)
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;

  const nsStyleSVG* svg = StyleSVG();
  const nsStyleSVGPaint* paint = aFill ? &svg->mFill : &svg->mStroke;

  nsAutoString paintString;

  switch (paint->Type()) {
    case eStyleSVGPaintType_None:
      val->SetIdent(eCSSKeyword_none);
      break;
    case eStyleSVGPaintType_Color:
      SetToRGBAColor(val, paint->GetColor());
      break;
    case eStyleSVGPaintType_Server: {
      SetValueToURLValue(paint->GetPaintServer(), val);
      if (paint->GetFallbackType() != eStyleSVGFallbackType_NotSet) {
        RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);
        RefPtr<CSSValue> fallback = GetFallbackValue(paint);
        valueList->AppendCSSValue(val.forget());
        valueList->AppendCSSValue(fallback.forget());
        return valueList.forget();
      }
      break;
    }
    case eStyleSVGPaintType_ContextFill:
    case eStyleSVGPaintType_ContextStroke: {
      val->SetIdent(paint->Type() == eStyleSVGPaintType_ContextFill ?
                    eCSSKeyword_context_fill : eCSSKeyword_context_stroke);
      if (paint->GetFallbackType() != eStyleSVGFallbackType_NotSet) {
        RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);
        RefPtr<CSSValue> fallback = GetFallbackValue(paint);
        valueList->AppendCSSValue(val.forget());
        valueList->AppendCSSValue(fallback.forget());
        return valueList.forget();
      }
      break;
    }
  }

  return val.forget();
}

/* If the property is "none", hand back "none" wrapped in a value.
 * Otherwise, compute the aggregate transform matrix and hands it back in a
 * "matrix" wrapper.
 */
already_AddRefed<CSSValue>
nsComputedDOMStyle::GetTransformValue(nsCSSValueSharedList* aSpecifiedTransform)
{
  /* If there are no transforms, then we should construct a single-element
   * entry and hand it back.
   */
  if (!aSpecifiedTransform) {
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

   bool dummyBool;
   gfx::Matrix4x4 matrix =
     nsStyleTransformMatrix::ReadTransforms(aSpecifiedTransform->mHead,
                                            refBox,
                                            float(mozilla::AppUnitsPerCSSPixel()),
                                            &dummyBool);

  return MatrixToCSSValue(matrix);
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
nsComputedDOMStyle::DoGetFillOpacity()
{
  RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleSVG()->mFillOpacity);
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

void
nsComputedDOMStyle::BoxValuesToString(nsAString& aString,
                                      const nsTArray<nsStyleCoord>& aBoxValues,
                                      bool aClampNegativeCalc)
{
  MOZ_ASSERT(aBoxValues.Length() == 4, "wrong number of box values");
  nsAutoString value1, value2, value3, value4;
  SetCssTextToCoord(value1, aBoxValues[0], aClampNegativeCalc);
  SetCssTextToCoord(value2, aBoxValues[1], aClampNegativeCalc);
  SetCssTextToCoord(value3, aBoxValues[2], aClampNegativeCalc);
  SetCssTextToCoord(value4, aBoxValues[3], aClampNegativeCalc);

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
  BoxValuesToString(horizontalString, horizontal, true);
  BoxValuesToString(verticalString, vertical, true);
  aCssText.Append(horizontalString);
  if (horizontalString == verticalString) {
    return;
  }
  aCssText.AppendLiteral(" / ");
  aCssText.Append(verticalString);
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::CreatePrimitiveValueForBasicShape(
  const UniquePtr<StyleBasicShape>& aStyleBasicShape)
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
                          aStyleBasicShape->Coordinates()[i],
                          false);
        shapeFunctionString.Append(coordString);
        shapeFunctionString.Append(' ');
        SetCssTextToCoord(coordString,
                          aStyleBasicShape->Coordinates()[i + 1],
                          false);
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
      BoxValuesToString(shapeFunctionString, aStyleBasicShape->Coordinates(), false);
      if (aStyleBasicShape->HasRadius()) {
        shapeFunctionString.AppendLiteral(" round ");
        nsAutoString radiiString;
        BasicShapeRadiiToString(radiiString, aStyleBasicShape->GetRadius());
        shapeFunctionString.Append(radiiString);
      }
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("unexpected type");
  }
  shapeFunctionString.Append(')');
  RefPtr<nsROCSSPrimitiveValue> functionValue = new nsROCSSPrimitiveValue;
  functionValue->SetString(shapeFunctionString);
  return functionValue.forget();
}

template<typename ReferenceBox>
already_AddRefed<CSSValue>
nsComputedDOMStyle::CreatePrimitiveValueForShapeSource(
  const UniquePtr<StyleBasicShape>& aStyleBasicShape,
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
    case StyleShapeSourceType::Image: {
      RefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
      SetValueToStyleImage(*aShapeSource.GetShapeImage(), val);
      return val.forget();
    }
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
                                      const nsStyleCoord& aCoord,
                                      bool aClampNegativeCalc)
{
  RefPtr<nsROCSSPrimitiveValue> value = new nsROCSSPrimitiveValue;
  SetValueToCoord(value, aCoord, aClampNegativeCalc);
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
      GetCSSShadowArray(aStyleFilter.GetDropShadow(), false);
    ErrorResult dummy;
    shadowValue->GetCssText(argumentString, dummy);
  } else {
    // Filter function argument.
    SetCssTextToCoord(argumentString, aStyleFilter.GetFilterParameter(), true);
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

already_AddRefed<CSSValue>
nsComputedDOMStyle::DoGetMaskImage()
{
  const nsStyleImageLayers& layers = StyleSVGReset()->mMask;
  return DoGetImageLayerImage(layers);
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

    nsAtom* name = animation->GetName();
    if (name == nsGkAtoms::_empty) {
      property->SetIdent(eCSSKeyword_none);
    } else {
      nsDependentAtomString nameStr(name);
      nsAutoString escaped;
      nsStyleUtil::AppendEscapedCSSIdent(nameStr, escaped);
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
    if (f == PositiveInfinity<float>()) {
      iterationCount->SetIdent(eCSSKeyword_infinite);
    } else {
      iterationCount->SetNumber(f);
    }
    valueList->AppendCSSValue(iterationCount.forget());
  } while (++i < display->mAnimationIterationCountCount);

  return valueList.forget();
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::DummyGetter()
{
  MOZ_CRASH("DummyGetter is not supposed to be invoked");
}

static void
MarkComputedStyleMapDirty(const char* aPref, void* aData)
{
  static_cast<ComputedStyleMap*>(aData)->MarkDirty();
}

void
nsComputedDOMStyle::ParentChainChanged(nsIContent* aContent)
{
  NS_ASSERTION(mElement == aContent, "didn't we register mElement?");
  NS_ASSERTION(mResolvedComputedStyle,
               "should have only registered an observer when "
               "mResolvedComputedStyle is true");

  ClearComputedStyle();
}

/* static */ ComputedStyleMap*
nsComputedDOMStyle::GetComputedStyleMap()
{
  static ComputedStyleMap map{};
  return &map;
}

/* static */ void
nsComputedDOMStyle::RegisterPrefChangeCallbacks()
{
  // Note that this will register callbacks for all properties with prefs, not
  // just those that are implemented on computed style objects, as it's not
  // easy to grab specific property data from ServoCSSPropList.h based on the
  // entries iterated in nsComputedDOMStylePropertyList.h.
  ComputedStyleMap* data = GetComputedStyleMap();
  for (const auto* p = nsCSSProps::kPropertyPrefTable;
       p->mPropID != eCSSProperty_UNKNOWN; p++) {
    nsCString name;
    name.AssignLiteral(p->mPref, strlen(p->mPref));
    Preferences::RegisterCallback(MarkComputedStyleMapDirty, name, data);
  }
}

/* static */ void
nsComputedDOMStyle::UnregisterPrefChangeCallbacks()
{
  ComputedStyleMap* data = GetComputedStyleMap();
  for (const auto* p = nsCSSProps::kPropertyPrefTable;
       p->mPropID != eCSSProperty_UNKNOWN; p++) {
    Preferences::UnregisterCallback(MarkComputedStyleMapDirty,
                                    nsDependentCString(p->mPref), data);
  }
}

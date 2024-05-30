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
#include "mozilla/PresShell.h"
#include "mozilla/PresShellInlines.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/ScrollContainerFrame.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/StaticPrefs_layout.h"

#include "nsError.h"
#include "nsIFrame.h"
#include "nsIFrameInlines.h"
#include "mozilla/ComputedStyle.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsIContent.h"
#include "nsStyleConsts.h"

#include "nsDOMCSSValueList.h"
#include "nsFlexContainerFrame.h"
#include "nsGridContainerFrame.h"
#include "nsGkAtoms.h"
#include "mozilla/ReflowInput.h"
#include "nsStyleUtil.h"
#include "nsStyleStructInlines.h"
#include "nsROCSSPrimitiveValue.h"

#include "nsPresContext.h"
#include "mozilla/dom/Document.h"

#include "nsCSSProps.h"
#include "nsCSSPseudoElements.h"
#include "mozilla/EffectSet.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/ViewportFrame.h"
#include "nsLayoutUtils.h"
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
#include "nsPrintfCString.h"

using namespace mozilla;
using namespace mozilla::dom;

/*
 * This is the implementation of the readonly CSSStyleDeclaration that is
 * returned by the getComputedStyle() function.
 */

already_AddRefed<nsComputedDOMStyle> NS_NewComputedDOMStyle(
    dom::Element* aElement, const nsAString& aPseudoElt, Document* aDocument,
    nsComputedDOMStyle::StyleType aStyleType, mozilla::ErrorResult&) {
  auto [pseudo, functionalPseudoParameter] =
      nsCSSPseudoElements::ParsePseudoElement(aPseudoElt,
                                              CSSEnabledState::ForAllContent);
  auto returnEmpty = nsComputedDOMStyle::AlwaysReturnEmptyStyle::No;
  if (!pseudo) {
    if (!aPseudoElt.IsEmpty() && aPseudoElt.First() == u':') {
      returnEmpty = nsComputedDOMStyle::AlwaysReturnEmptyStyle::Yes;
    }
    pseudo.emplace(PseudoStyleType::NotPseudo);
  }
  return MakeAndAddRef<nsComputedDOMStyle>(aElement, *pseudo,
                                           functionalPseudoParameter, aDocument,
                                           aStyleType, returnEmpty);
}

static nsDOMCSSValueList* GetROCSSValueList(bool aCommaDelimited) {
  return new nsDOMCSSValueList(aCommaDelimited);
}

static const Element* GetRenderedElement(const Element* aElement,
                                         PseudoStyleType aPseudo) {
  if (aPseudo == PseudoStyleType::NotPseudo) {
    return aElement;
  }
  if (aPseudo == PseudoStyleType::before) {
    return nsLayoutUtils::GetBeforePseudo(aElement);
  }
  if (aPseudo == PseudoStyleType::after) {
    return nsLayoutUtils::GetAfterPseudo(aElement);
  }
  if (aPseudo == PseudoStyleType::marker) {
    return nsLayoutUtils::GetMarkerPseudo(aElement);
  }
  return nullptr;
}

// Whether aDocument needs to restyle for aElement
static bool ElementNeedsRestyle(Element* aElement, PseudoStyleType aPseudo,
                                bool aMayNeedToFlushLayout) {
  const Document* doc = aElement->GetComposedDoc();
  if (!doc) {
    // If the element is out of the document we don't return styles for it, so
    // nothing to do.
    return false;
  }

  PresShell* presShell = doc->GetPresShell();
  if (!presShell) {
    // If there's no pres-shell we'll just either mint a new style from our
    // caller document, or return no styles, so nothing to do (unless our owner
    // element needs to get restyled, which could cause us to gain a pres shell,
    // but the caller checks that).
    return false;
  }

  // Unfortunately we don't know if the sheet change affects mElement or not, so
  // just assume it will and that we need to flush normally.
  ServoStyleSet* styleSet = presShell->StyleSet();
  if (styleSet->StyleSheetsHaveChanged()) {
    return true;
  }

  nsPresContext* presContext = presShell->GetPresContext();
  MOZ_ASSERT(presContext);

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
  if (aElement->MayHaveAnimations() && aPseudo != PseudoStyleType::NotPseudo &&
      AnimationUtils::IsSupportedPseudoForAnimations(aPseudo)) {
    if (EffectSet::Get(aElement, aPseudo)) {
      return true;
    }
  }

  // For Servo, we need to process the restyle-hint-invalidations first, to
  // expand LaterSiblings hint, so that we can look whether ancestors need
  // restyling.
  RestyleManager* restyleManager = presContext->RestyleManager();
  restyleManager->ProcessAllPendingAttributeAndStateInvalidations();

  if (!presContext->EffectCompositor()->HasPendingStyleUpdates() &&
      !doc->GetServoRestyleRoot()) {
    return false;
  }

  // If there's a pseudo, we need to prefer that element, as the pseudo itself
  // may have explicit restyles.
  const Element* styledElement = GetRenderedElement(aElement, aPseudo);
  // Try to skip the restyle otherwise.
  return Servo_HasPendingRestyleAncestor(
      styledElement ? styledElement : aElement, aMayNeedToFlushLayout);
}

/**
 * An object that represents the ordered set of properties that are exposed on
 * an nsComputedDOMStyle object and how their computed values can be obtained.
 */
struct ComputedStyleMap {
  friend class nsComputedDOMStyle;

  struct Entry {
    // Create a pointer-to-member-function type.
    using ComputeMethod = already_AddRefed<CSSValue> (nsComputedDOMStyle::*)();

    nsCSSPropertyID mProperty;

    // Whether the property can ever be exposed in getComputedStyle(). For
    // example, @page descriptors implemented as CSS properties or other
    // internal properties, would have this flag set to `false`.
    bool mCanBeExposed = false;

    ComputeMethod mGetter = nullptr;

    bool IsEnumerable() const {
      return IsEnabled() && !nsCSSProps::IsShorthand(mProperty);
    }

    bool IsEnabled() const {
      return mCanBeExposed &&
             nsCSSProps::IsEnabled(mProperty, CSSEnabledState::ForAllContent);
    }
  };

  // This generated file includes definition of kEntries which is typed
  // Entry[] and used below, so this #include has to be put here.
#include "nsComputedDOMStyleGenerated.inc"

  /**
   * Returns the number of properties that should be exposed on an
   * nsComputedDOMStyle, ecxluding any disabled properties.
   */
  uint32_t Length() {
    Update();
    return mEnumerablePropertyCount;
  }

  /**
   * Returns the property at the given index in the list of properties
   * that should be exposed on an nsComputedDOMStyle, excluding any
   * disabled properties.
   */
  nsCSSPropertyID PropertyAt(uint32_t aIndex) {
    Update();
    return kEntries[EntryIndex(aIndex)].mProperty;
  }

  /**
   * Searches for and returns the computed style map entry for the given
   * property, or nullptr if the property is not exposed on nsComputedDOMStyle
   * or is currently disabled.
   */
  const Entry* FindEntryForProperty(nsCSSPropertyID aPropID) {
    if (size_t(aPropID) >= ArrayLength(kEntryIndices)) {
      MOZ_ASSERT(aPropID == eCSSProperty_UNKNOWN);
      return nullptr;
    }
    MOZ_ASSERT(kEntryIndices[aPropID] < ArrayLength(kEntries));
    const auto& entry = kEntries[kEntryIndices[aPropID]];
    if (!entry.IsEnabled()) {
      return nullptr;
    }
    return &entry;
  }

  /**
   * Records that mIndexMap needs updating, due to prefs changing that could
   * affect the set of properties exposed on an nsComputedDOMStyle.
   */
  void MarkDirty() { mEnumerablePropertyCount = 0; }

  // The member variables are public so that we can use an initializer in
  // nsComputedDOMStyle::GetComputedStyleMap.  Use the member functions
  // above to get information from this object.

  /**
   * The number of properties that should be exposed on an nsComputedDOMStyle.
   * This will be less than eComputedStyleProperty_COUNT if some property
   * prefs are disabled.  A value of 0 indicates that it and mIndexMap are out
   * of date.
   */
  uint32_t mEnumerablePropertyCount = 0;

  /**
   * A map of indexes on the nsComputedDOMStyle object to indexes into kEntries.
   */
  uint32_t mIndexMap[ArrayLength(kEntries)];

 private:
  /**
   * Returns whether mEnumerablePropertyCount and mIndexMap are out of date.
   */
  bool IsDirty() { return mEnumerablePropertyCount == 0; }

  /**
   * Updates mEnumerablePropertyCount and mIndexMap to take into account
   * properties whose prefs are currently disabled.
   */
  void Update();

  /**
   * Maps an nsComputedDOMStyle indexed getter index to an index into kEntries.
   */
  uint32_t EntryIndex(uint32_t aIndex) const {
    MOZ_ASSERT(aIndex < mEnumerablePropertyCount);
    return mIndexMap[aIndex];
  }
};

constexpr ComputedStyleMap::Entry
    ComputedStyleMap::kEntries[ArrayLength(kEntries)];

constexpr size_t ComputedStyleMap::kEntryIndices[ArrayLength(kEntries)];

void ComputedStyleMap::Update() {
  if (!IsDirty()) {
    return;
  }

  uint32_t index = 0;
  for (uint32_t i = 0; i < ArrayLength(kEntries); i++) {
    if (kEntries[i].IsEnumerable()) {
      mIndexMap[index++] = i;
    }
  }
  mEnumerablePropertyCount = index;
}

nsComputedDOMStyle::nsComputedDOMStyle(dom::Element* aElement,
                                       PseudoStyleType aPseudo,
                                       nsAtom* aFunctionalPseudoParameter,
                                       Document* aDocument,
                                       StyleType aStyleType,
                                       AlwaysReturnEmptyStyle aAlwaysEmpty)
    : mDocumentWeak(nullptr),
      mOuterFrame(nullptr),
      mInnerFrame(nullptr),
      mPresShell(nullptr),
      mPseudo(aPseudo),
      mFunctionalPseudoParameter(aFunctionalPseudoParameter),
      mStyleType(aStyleType),
      mAlwaysReturnEmpty(aAlwaysEmpty) {
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aDocument);
  // TODO(emilio, bug 548397, https://github.com/w3c/csswg-drafts/issues/2403):
  // Should use aElement->OwnerDoc() instead.
  mDocumentWeak = aDocument;
  mElement = aElement;
  SetEnabledCallbacks(nsIMutationObserver::kParentChainChanged);
}

nsComputedDOMStyle::~nsComputedDOMStyle() {
  MOZ_ASSERT(!mResolvedComputedStyle,
             "Should have called ClearComputedStyle() during last release.");
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(nsComputedDOMStyle)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsComputedDOMStyle)
  tmp->ClearComputedStyle();  // remove observer before clearing mElement
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsComputedDOMStyle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// We can skip the nsComputedDOMStyle if it has no wrapper and its
// element is skippable, because it will have no outgoing edges, so
// it can't be part of a cycle.

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsComputedDOMStyle)
  if (!tmp->GetWrapperPreserveColor()) {
    return !tmp->mElement ||
           mozilla::dom::FragmentOrElement::CanSkip(tmp->mElement, true);
  }
  return tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsComputedDOMStyle)
  if (!tmp->GetWrapperPreserveColor()) {
    return !tmp->mElement ||
           mozilla::dom::FragmentOrElement::CanSkipInCC(tmp->mElement);
  }
  return tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsComputedDOMStyle)
  if (!tmp->GetWrapperPreserveColor()) {
    return !tmp->mElement ||
           mozilla::dom::FragmentOrElement::CanSkipThis(tmp->mElement);
  }
  return tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

// QueryInterface implementation for nsComputedDOMStyle
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsComputedDOMStyle)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
NS_INTERFACE_MAP_END_INHERITING(nsDOMCSSDeclaration)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsComputedDOMStyle)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(nsComputedDOMStyle,
                                                   ClearComputedStyle())

void nsComputedDOMStyle::GetPropertyValue(const nsCSSPropertyID aPropID,
                                          nsACString& aValue) {
  return GetPropertyValue(aPropID, EmptyCString(), aValue);
}

void nsComputedDOMStyle::SetPropertyValue(const nsCSSPropertyID aPropID,
                                          const nsACString& aValue,
                                          nsIPrincipal* aSubjectPrincipal,
                                          ErrorResult& aRv) {
  aRv.ThrowNoModificationAllowedError(nsPrintfCString(
      "Can't set value for property '%s' in computed style",
      PromiseFlatCString(nsCSSProps::GetStringValue(aPropID)).get()));
}

void nsComputedDOMStyle::GetCssText(nsACString& aCssText) {
  aCssText.Truncate();
}

void nsComputedDOMStyle::SetCssText(const nsACString& aCssText,
                                    nsIPrincipal* aSubjectPrincipal,
                                    ErrorResult& aRv) {
  aRv.ThrowNoModificationAllowedError("Can't set cssText on computed style");
}

uint32_t nsComputedDOMStyle::Length() {
  // Make sure we have up to date style so that we can include custom
  // properties.
  UpdateCurrentStyleSources(eCSSPropertyExtra_variable);
  if (!mComputedStyle) {
    return 0;
  }

  uint32_t length = GetComputedStyleMap()->Length() +
                    Servo_GetCustomPropertiesCount(mComputedStyle);

  ClearCurrentStyleSources();

  return length;
}

css::Rule* nsComputedDOMStyle::GetParentRule() { return nullptr; }

void nsComputedDOMStyle::GetPropertyValue(const nsACString& aPropertyName,
                                          nsACString& aReturn) {
  nsCSSPropertyID prop = nsCSSProps::LookupProperty(aPropertyName);
  GetPropertyValue(prop, aPropertyName, aReturn);
}

void nsComputedDOMStyle::GetPropertyValue(
    nsCSSPropertyID aPropID, const nsACString& aMaybeCustomPropertyName,
    nsACString& aReturn) {
  MOZ_ASSERT(aReturn.IsEmpty());

  const ComputedStyleMap::Entry* entry = nullptr;
  if (aPropID != eCSSPropertyExtra_variable) {
    entry = GetComputedStyleMap()->FindEntryForProperty(aPropID);
    if (!entry) {
      return;
    }
  }

  UpdateCurrentStyleSources(aPropID);
  if (!mComputedStyle) {
    return;
  }

  auto cleanup = mozilla::MakeScopeExit([&] { ClearCurrentStyleSources(); });

  if (!entry) {
    MOZ_ASSERT(nsCSSProps::IsCustomPropertyName(aMaybeCustomPropertyName));
    const nsACString& name =
        Substring(aMaybeCustomPropertyName, CSS_CUSTOM_NAME_PREFIX_LENGTH);
    Servo_GetCustomPropertyValue(mComputedStyle, &name,
                                 mPresShell->StyleSet()->RawData(), &aReturn);
    return;
  }

  if (nsCSSProps::PropHasFlags(aPropID, CSSPropFlags::IsLogical)) {
    MOZ_ASSERT(entry);
    MOZ_ASSERT(entry->mGetter == &nsComputedDOMStyle::DummyGetter);

    DebugOnly<nsCSSPropertyID> logicalProp = aPropID;

    aPropID = Servo_ResolveLogicalProperty(aPropID, mComputedStyle);
    entry = GetComputedStyleMap()->FindEntryForProperty(aPropID);

    MOZ_ASSERT(NeedsToFlushLayout(logicalProp) == NeedsToFlushLayout(aPropID),
               "Logical and physical property don't agree on whether layout is "
               "needed");
  }

  if (!nsCSSProps::PropHasFlags(aPropID, CSSPropFlags::SerializedByServo)) {
    if (RefPtr<CSSValue> value = (this->*entry->mGetter)()) {
      nsAutoString text;
      value->GetCssText(text);
      CopyUTF16toUTF8(text, aReturn);
    }
    return;
  }

  MOZ_ASSERT(entry->mGetter == &nsComputedDOMStyle::DummyGetter);
  Servo_GetResolvedValue(mComputedStyle, aPropID,
                         mPresShell->StyleSet()->RawData(), mElement, &aReturn);
}

/* static */
already_AddRefed<const ComputedStyle> nsComputedDOMStyle::GetComputedStyle(
    Element* aElement, PseudoStyleType aPseudo,
    nsAtom* aFunctionalPseudoParameter, StyleType aStyleType) {
  if (Document* doc = aElement->GetComposedDoc()) {
    doc->FlushPendingNotifications(FlushType::Style);
  }
  return GetComputedStyleNoFlush(aElement, aPseudo, aFunctionalPseudoParameter,
                                 aStyleType);
}

/**
 * The following function checks whether we need to explicitly resolve the style
 * again, even though we have a style coming from the frame.
 *
 * This basically checks whether the style is or may be under a ::first-line or
 * ::first-letter frame, in which case we can't return the frame style, and we
 * need to resolve it. See bug 505515.
 */
static bool MustReresolveStyle(const ComputedStyle* aStyle) {
  MOZ_ASSERT(aStyle);

  // TODO(emilio): We may want to avoid re-resolving pseudo-element styles
  // more often.
  return aStyle->HasPseudoElementData() && !aStyle->IsPseudoElement();
}

static bool IsInFlatTree(const Element& aElement) {
  const auto* topmost = &aElement;
  while (true) {
    if (topmost->HasServoData()) {
      // If we have styled this element then we know it's in the flat tree.
      return true;
    }
    const Element* parent = topmost->GetFlattenedTreeParentElement();
    if (!parent) {
      break;
    }
    topmost = parent;
  }
  auto* root = topmost->GetFlattenedTreeParentNode();
  return root && root->IsDocument();
}

already_AddRefed<const ComputedStyle>
nsComputedDOMStyle::DoGetComputedStyleNoFlush(
    const Element* aElement, PseudoStyleType aPseudo,
    nsAtom* aFunctionalPseudoParameter, PresShell* aPresShell,
    StyleType aStyleType) {
  MOZ_ASSERT(aElement, "NULL element");

  // If the content has a pres shell, we must use it.  Otherwise we'd
  // potentially mix rule trees by using the wrong pres shell's style
  // set.  Using the pres shell from the content also means that any
  // content that's actually *in* a document will get the style from the
  // correct document.
  PresShell* presShell = nsContentUtils::GetPresShellForContent(aElement);
  bool inDocWithShell = true;
  if (!presShell) {
    inDocWithShell = false;
    presShell = aPresShell;
    if (!presShell) {
      return nullptr;
    }
  }

  MOZ_ASSERT(aPseudo == PseudoStyleType::NotPseudo ||
             PseudoStyle::IsPseudoElement(aPseudo));
  if (!aElement->IsInComposedDoc()) {
    // Don't return styles for disconnected elements, that makes no sense. This
    // can only happen with a non-null presShell for cross-document calls.
    return nullptr;
  }

  if (!IsInFlatTree(*aElement)) {
    return nullptr;
  }

  // XXX the !aElement->IsHTMLElement(nsGkAtoms::area)
  // check is needed due to bug 135040 (to avoid using
  // mPrimaryFrame). Remove it once that's fixed.
  if (inDocWithShell && aStyleType == StyleType::All &&
      !aElement->IsHTMLElement(nsGkAtoms::area)) {
    if (const Element* element = GetRenderedElement(aElement, aPseudo)) {
      if (element->HasServoData()) {
        const ComputedStyle* result =
            Servo_Element_GetMaybeOutOfDateStyle(element);
        return do_AddRef(result);
      }
    }
  }

  // No frame has been created, or we have a pseudo, or we're looking
  // for the default style, so resolve the style ourselves.
  ServoStyleSet* styleSet = presShell->StyleSet();

  StyleRuleInclusion rules = aStyleType == StyleType::DefaultOnly
                                 ? StyleRuleInclusion::DefaultOnly
                                 : StyleRuleInclusion::All;
  RefPtr<ComputedStyle> result = styleSet->ResolveStyleLazily(
      *aElement, aPseudo, aFunctionalPseudoParameter, rules);
  return result.forget();
}

already_AddRefed<const ComputedStyle>
nsComputedDOMStyle::GetUnanimatedComputedStyleNoFlush(
    Element* aElement, PseudoStyleType aPseudo,
    nsAtom* aFunctionalPseudoParameter) {
  RefPtr<const ComputedStyle> style =
      GetComputedStyleNoFlush(aElement, aPseudo, aFunctionalPseudoParameter);
  if (!style) {
    return nullptr;
  }

  PresShell* presShell = aElement->OwnerDoc()->GetPresShell();
  MOZ_ASSERT(presShell,
             "How in the world did we get a style a few lines above?");

  Element* elementOrPseudoElement =
      AnimationUtils::GetElementForRestyle(aElement, aPseudo);
  if (!elementOrPseudoElement) {
    return nullptr;
  }

  return presShell->StyleSet()->GetBaseContextForElement(elementOrPseudoElement,
                                                         style);
}

nsMargin nsComputedDOMStyle::GetAdjustedValuesForBoxSizing() {
  // We want the width/height of whatever parts 'width' or 'height' controls,
  // which can be different depending on the value of the 'box-sizing' property.
  const nsStylePosition* stylePos = StylePosition();

  nsMargin adjustment;
  if (stylePos->mBoxSizing == StyleBoxSizing::Border) {
    adjustment = mInnerFrame->GetUsedBorderAndPadding();
  }

  return adjustment;
}

static void AddImageURL(nsIURI& aURI, nsTArray<nsCString>& aURLs) {
  nsCString spec;
  nsresult rv = aURI.GetSpec(spec);
  if (NS_FAILED(rv)) {
    return;
  }

  aURLs.AppendElement(std::move(spec));
}

static void AddImageURL(const StyleComputedUrl& aURL,
                        nsTArray<nsCString>& aURLs) {
  if (aURL.IsLocalRef()) {
    return;
  }

  if (nsIURI* uri = aURL.GetURI()) {
    AddImageURL(*uri, aURLs);
  }
}

static void AddImageURL(const StyleImage& aImage, nsTArray<nsCString>& aURLs) {
  if (auto* urlValue = aImage.GetImageRequestURLValue()) {
    AddImageURL(*urlValue, aURLs);
  }
}

static void AddImageURL(const StyleShapeOutside& aShapeOutside,
                        nsTArray<nsCString>& aURLs) {
  if (aShapeOutside.IsImage()) {
    AddImageURL(aShapeOutside.AsImage(), aURLs);
  }
}

static void AddImageURL(const StyleClipPath& aClipPath,
                        nsTArray<nsCString>& aURLs) {
  if (aClipPath.IsUrl()) {
    AddImageURL(aClipPath.AsUrl(), aURLs);
  }
}

static void AddImageURLs(const nsStyleImageLayers& aLayers,
                         nsTArray<nsCString>& aURLs) {
  for (auto i : IntegerRange(aLayers.mLayers.Length())) {
    AddImageURL(aLayers.mLayers[i].mImage, aURLs);
  }
}

static void CollectImageURLsForProperty(nsCSSPropertyID aProp,
                                        const ComputedStyle& aStyle,
                                        nsTArray<nsCString>& aURLs) {
  if (nsCSSProps::IsShorthand(aProp)) {
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(p, aProp,
                                         CSSEnabledState::ForAllContent) {
      CollectImageURLsForProperty(*p, aStyle, aURLs);
    }
    return;
  }

  switch (aProp) {
    case eCSSProperty_cursor:
      for (auto& image : aStyle.StyleUI()->Cursor().images.AsSpan()) {
        AddImageURL(image.image, aURLs);
      }
      break;
    case eCSSProperty_background_image:
      AddImageURLs(aStyle.StyleBackground()->mImage, aURLs);
      break;
    case eCSSProperty_mask_clip:
      AddImageURLs(aStyle.StyleSVGReset()->mMask, aURLs);
      break;
    case eCSSProperty_list_style_image: {
      const auto& image = aStyle.StyleList()->mListStyleImage;
      if (image.IsUrl()) {
        AddImageURL(image.AsUrl(), aURLs);
      }
      break;
    }
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

float nsComputedDOMStyle::UsedFontSize() {
  UpdateCurrentStyleSources(eCSSProperty_font_size);

  if (!mComputedStyle) {
    return -1.0;
  }

  return mComputedStyle->StyleFont()->mFont.size.ToCSSPixels();
}

void nsComputedDOMStyle::GetCSSImageURLs(const nsACString& aPropertyName,
                                         nsTArray<nsCString>& aImageURLs,
                                         mozilla::ErrorResult& aRv) {
  nsCSSPropertyID prop = nsCSSProps::LookupProperty(aPropertyName);
  if (prop == eCSSProperty_UNKNOWN) {
    // Note: not using nsPrintfCString here in case aPropertyName contains
    // nulls.
    aRv.ThrowSyntaxError("Invalid property name '"_ns + aPropertyName + "'"_ns);
    return;
  }

  UpdateCurrentStyleSources(prop);

  if (!mComputedStyle) {
    return;
  }

  CollectImageURLsForProperty(prop, *mComputedStyle, aImageURLs);
  ClearCurrentStyleSources();
}

// nsDOMCSSDeclaration abstract methods which should never be called
// on a nsComputedDOMStyle object, but must be defined to avoid
// compile errors.
DeclarationBlock* nsComputedDOMStyle::GetOrCreateCSSDeclaration(
    Operation aOperation, DeclarationBlock** aCreated) {
  MOZ_CRASH("called nsComputedDOMStyle::GetCSSDeclaration");
}

nsresult nsComputedDOMStyle::SetCSSDeclaration(DeclarationBlock*,
                                               MutationClosureData*) {
  MOZ_CRASH("called nsComputedDOMStyle::SetCSSDeclaration");
}

Document* nsComputedDOMStyle::DocToUpdate() {
  MOZ_CRASH("called nsComputedDOMStyle::DocToUpdate");
}

nsDOMCSSDeclaration::ParsingEnvironment
nsComputedDOMStyle::GetParsingEnvironment(
    nsIPrincipal* aSubjectPrincipal) const {
  MOZ_CRASH("called nsComputedDOMStyle::GetParsingEnvironment");
}

void nsComputedDOMStyle::ClearComputedStyle() {
  if (mResolvedComputedStyle) {
    mResolvedComputedStyle = false;
    mElement->RemoveMutationObserver(this);
  }
  mComputedStyle = nullptr;
}

void nsComputedDOMStyle::SetResolvedComputedStyle(
    RefPtr<const ComputedStyle>&& aContext, uint64_t aGeneration) {
  if (!mResolvedComputedStyle) {
    mResolvedComputedStyle = true;
    mElement->AddMutationObserver(this);
  }
  mComputedStyle = aContext;
  mComputedStyleGeneration = aGeneration;
  mPresShellId = mPresShell->GetPresShellId();
}

void nsComputedDOMStyle::SetFrameComputedStyle(mozilla::ComputedStyle* aStyle,
                                               uint64_t aGeneration) {
  ClearComputedStyle();
  mComputedStyle = aStyle;
  mComputedStyleGeneration = aGeneration;
  mPresShellId = mPresShell->GetPresShellId();
}

static bool MayNeedToFlushLayout(nsCSSPropertyID aPropID) {
  switch (aPropID) {
    case eCSSProperty_width:
    case eCSSProperty_height:
    case eCSSProperty_block_size:
    case eCSSProperty_inline_size:
    case eCSSProperty_line_height:
    case eCSSProperty_grid_template_rows:
    case eCSSProperty_grid_template_columns:
    case eCSSProperty_perspective_origin:
    case eCSSProperty_transform_origin:
    case eCSSProperty_transform:
    case eCSSProperty_border_top_width:
    case eCSSProperty_border_bottom_width:
    case eCSSProperty_border_left_width:
    case eCSSProperty_border_right_width:
    case eCSSProperty_border_block_start_width:
    case eCSSProperty_border_block_end_width:
    case eCSSProperty_border_inline_start_width:
    case eCSSProperty_border_inline_end_width:
    case eCSSProperty_top:
    case eCSSProperty_right:
    case eCSSProperty_bottom:
    case eCSSProperty_left:
    case eCSSProperty_inset_block_start:
    case eCSSProperty_inset_block_end:
    case eCSSProperty_inset_inline_start:
    case eCSSProperty_inset_inline_end:
    case eCSSProperty_padding_top:
    case eCSSProperty_padding_right:
    case eCSSProperty_padding_bottom:
    case eCSSProperty_padding_left:
    case eCSSProperty_padding_block_start:
    case eCSSProperty_padding_block_end:
    case eCSSProperty_padding_inline_start:
    case eCSSProperty_padding_inline_end:
    case eCSSProperty_margin_top:
    case eCSSProperty_margin_right:
    case eCSSProperty_margin_bottom:
    case eCSSProperty_margin_left:
    case eCSSProperty_margin_block_start:
    case eCSSProperty_margin_block_end:
    case eCSSProperty_margin_inline_start:
    case eCSSProperty_margin_inline_end:
      return true;
    default:
      return false;
  }
}

bool nsComputedDOMStyle::NeedsToFlushStyle(nsCSSPropertyID aPropID) const {
  bool mayNeedToFlushLayout = MayNeedToFlushLayout(aPropID);

  // We always compute styles from the element's owner document.
  if (ElementNeedsRestyle(mElement, mPseudo, mayNeedToFlushLayout)) {
    return true;
  }

  Document* doc = mElement->OwnerDoc();
  // If parent document is there, also needs to check if there is some change
  // that needs to flush this document (e.g. size change for iframe).
  while (doc->StyleOrLayoutObservablyDependsOnParentDocumentLayout()) {
    if (Element* element = doc->GetEmbedderElement()) {
      if (ElementNeedsRestyle(element, PseudoStyleType::NotPseudo,
                              mayNeedToFlushLayout)) {
        return true;
      }
    }

    doc = doc->GetInProcessParentDocument();
  }

  return false;
}

static bool IsNonReplacedInline(nsIFrame* aFrame) {
  // FIXME: this should be IsInlineInsideStyle() since width/height
  // doesn't apply to ruby boxes.
  return aFrame->StyleDisplay()->IsInlineFlow() && !aFrame->IsReplaced() &&
         !aFrame->IsFieldSetFrame() && !aFrame->IsBlockFrame() &&
         !aFrame->IsScrollContainerFrame() &&
         !aFrame->IsColumnSetWrapperFrame();
}

static Side SideForPaddingOrMarginOrInsetProperty(nsCSSPropertyID aPropID) {
  switch (aPropID) {
    case eCSSProperty_top:
    case eCSSProperty_margin_top:
    case eCSSProperty_padding_top:
      return eSideTop;
    case eCSSProperty_right:
    case eCSSProperty_margin_right:
    case eCSSProperty_padding_right:
      return eSideRight;
    case eCSSProperty_bottom:
    case eCSSProperty_margin_bottom:
    case eCSSProperty_padding_bottom:
      return eSideBottom;
    case eCSSProperty_left:
    case eCSSProperty_margin_left:
    case eCSSProperty_padding_left:
      return eSideLeft;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected property");
      return eSideTop;
  }
}

static bool PaddingNeedsUsedValue(const LengthPercentage& aValue,
                                  const ComputedStyle& aStyle) {
  return !aValue.ConvertsToLength() || aStyle.StyleDisplay()->HasAppearance();
}

bool nsComputedDOMStyle::NeedsToFlushLayout(nsCSSPropertyID aPropID) const {
  MOZ_ASSERT(aPropID != eCSSProperty_UNKNOWN);
  if (aPropID == eCSSPropertyExtra_variable) {
    return false;
  }
  nsIFrame* outerFrame = GetOuterFrame();
  if (!outerFrame) {
    return false;
  }
  nsIFrame* frame = nsLayoutUtils::GetStyleFrame(outerFrame);
  auto* style = frame->Style();
  if (nsCSSProps::PropHasFlags(aPropID, CSSPropFlags::IsLogical)) {
    aPropID = Servo_ResolveLogicalProperty(aPropID, style);
  }

  switch (aPropID) {
    case eCSSProperty_width:
    case eCSSProperty_height:
      return !IsNonReplacedInline(frame);
    case eCSSProperty_line_height:
      return frame->StyleFont()->mLineHeight.IsMozBlockHeight();
    case eCSSProperty_grid_template_rows:
    case eCSSProperty_grid_template_columns:
      return !!nsGridContainerFrame::GetGridContainerFrame(frame);
    case eCSSProperty_perspective_origin:
      return style->StyleDisplay()->mPerspectiveOrigin.HasPercent();
    case eCSSProperty_transform_origin:
      return style->StyleDisplay()->mTransformOrigin.HasPercent();
    case eCSSProperty_transform:
      return style->StyleDisplay()->mTransform.HasPercent();
    case eCSSProperty_border_top_width:
    case eCSSProperty_border_bottom_width:
    case eCSSProperty_border_left_width:
    case eCSSProperty_border_right_width:
      // FIXME(emilio): This should return false per spec (bug 1551000), but
      // themed borders don't make that easy, so for now flush for that case.
      //
      // TODO(emilio): If we make GetUsedBorder() stop returning 0 for an
      // unreflowed frame, or something of that sort, then we can stop flushing
      // layout for themed frames.
      return style->StyleDisplay()->HasAppearance();
    case eCSSProperty_top:
    case eCSSProperty_right:
    case eCSSProperty_bottom:
    case eCSSProperty_left:
      // Doing better than this is actually hard.
      return style->StyleDisplay()->mPosition != StylePositionProperty::Static;
    case eCSSProperty_padding_top:
    case eCSSProperty_padding_right:
    case eCSSProperty_padding_bottom:
    case eCSSProperty_padding_left: {
      Side side = SideForPaddingOrMarginOrInsetProperty(aPropID);
      // Theming can override used padding, sigh.
      //
      // TODO(emilio): If we make GetUsedPadding() stop returning 0 for an
      // unreflowed frame, or something of that sort, then we can stop flushing
      // layout for themed frames.
      return PaddingNeedsUsedValue(style->StylePadding()->mPadding.Get(side),
                                   *style);
    }
    case eCSSProperty_margin_top:
    case eCSSProperty_margin_right:
    case eCSSProperty_margin_bottom:
    case eCSSProperty_margin_left: {
      // NOTE(emilio): This is dubious, but matches other browsers.
      // See https://github.com/w3c/csswg-drafts/issues/2328
      Side side = SideForPaddingOrMarginOrInsetProperty(aPropID);
      return !style->StyleMargin()->mMargin.Get(side).ConvertsToLength();
    }
    default:
      return false;
  }
}

bool nsComputedDOMStyle::NeedsToFlushLayoutForContainerQuery() const {
  const auto* outerFrame = GetOuterFrame();
  if (!outerFrame) {
    return false;
  }
  const auto* innerFrame = nsLayoutUtils::GetStyleFrame(outerFrame);
  MOZ_ASSERT(innerFrame, "No valid inner frame?");
  // It's possible that potential containers are styled but not yet reflowed,
  // i.e. They don't have a correct size, which makes any container query
  // evaluation against them invalid.
  return innerFrame->HasUnreflowedContainerQueryAncestor();
}

void nsComputedDOMStyle::Flush(Document& aDocument, FlushType aFlushType) {
  MOZ_ASSERT(mElement->IsInComposedDoc());
  MOZ_ASSERT(mDocumentWeak == &aDocument);

  if (MOZ_UNLIKELY(&aDocument != mElement->OwnerDoc())) {
    aDocument.FlushPendingNotifications(aFlushType);
  }
  // This performs the flush, and also guarantees that content-visibility:
  // hidden elements get laid out, if needed.
  mElement->GetPrimaryFrame(aFlushType);
}

nsIFrame* nsComputedDOMStyle::GetOuterFrame() const {
  if (mPseudo == PseudoStyleType::NotPseudo) {
    return mElement->GetPrimaryFrame();
  }
  nsAtom* property = nullptr;
  if (mPseudo == PseudoStyleType::before) {
    property = nsGkAtoms::beforePseudoProperty;
  } else if (mPseudo == PseudoStyleType::after) {
    property = nsGkAtoms::afterPseudoProperty;
  } else if (mPseudo == PseudoStyleType::marker) {
    property = nsGkAtoms::markerPseudoProperty;
  }
  if (!property) {
    return nullptr;
  }
  auto* pseudo = static_cast<Element*>(mElement->GetProperty(property));
  return pseudo ? pseudo->GetPrimaryFrame() : nullptr;
}

void nsComputedDOMStyle::UpdateCurrentStyleSources(nsCSSPropertyID aPropID) {
  nsCOMPtr<Document> document(mDocumentWeak);
  if (!document) {
    ClearComputedStyle();
    return;
  }

  // We don't return styles for disconnected elements anymore, so don't go
  // through the trouble of flushing or what not.
  //
  // TODO(emilio): We may want to return earlier for elements outside of the
  // flat tree too: https://github.com/w3c/csswg-drafts/issues/1964
  if (!mElement->IsInComposedDoc()) {
    ClearComputedStyle();
    return;
  }

  if (mAlwaysReturnEmpty == AlwaysReturnEmptyStyle::Yes) {
    ClearComputedStyle();
    return;
  }

  DebugOnly<bool> didFlush = false;
  if (NeedsToFlushStyle(aPropID)) {
    didFlush = true;
    // We look at the frame in NeedsToFlushLayout, so flush frames, not only
    // styles.
    Flush(*document, FlushType::Frames);
  }

  const bool needsToFlushLayoutForProp = NeedsToFlushLayout(aPropID);
  if (needsToFlushLayoutForProp || NeedsToFlushLayoutForContainerQuery()) {
    MOZ_ASSERT_IF(needsToFlushLayoutForProp, MayNeedToFlushLayout(aPropID));
    didFlush = true;
    Flush(*document, FlushType::Layout);
#ifdef DEBUG
    mFlushedPendingReflows = true;
#endif
  } else {
#ifdef DEBUG
    mFlushedPendingReflows = false;
#endif
  }

  mPresShell = document->GetPresShell();
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
  uint64_t currentGeneration =
      mPresShell->GetPresContext()->GetUndisplayedRestyleGeneration();

  if (mComputedStyle && mComputedStyleGeneration == currentGeneration &&
      mPresShellId == mPresShell->GetPresShellId()) {
    // Our cached style is still valid.
    return;
  }

  mComputedStyle = nullptr;

  // XXX the !mElement->IsHTMLElement(nsGkAtoms::area) check is needed due to
  // bug 135040 (to avoid using mPrimaryFrame). Remove it once that's fixed.
  if (mStyleType == StyleType::All &&
      !mElement->IsHTMLElement(nsGkAtoms::area)) {
    mOuterFrame = GetOuterFrame();
    mInnerFrame = mOuterFrame;
    if (mOuterFrame) {
      mInnerFrame = nsLayoutUtils::GetStyleFrame(mOuterFrame);
      SetFrameComputedStyle(mInnerFrame->Style(), currentGeneration);
      NS_ASSERTION(mComputedStyle, "Frame without style?");
    }
  }

  if (!mComputedStyle || MustReresolveStyle(mComputedStyle)) {
    PresShell* presShellForContent = mElement->OwnerDoc()->GetPresShell();
    // Need to resolve a style.
    RefPtr<const ComputedStyle> resolvedComputedStyle =
        DoGetComputedStyleNoFlush(
            mElement, mPseudo, mFunctionalPseudoParameter,
            presShellForContent ? presShellForContent : mPresShell, mStyleType);
    if (!resolvedComputedStyle) {
      ClearComputedStyle();
      return;
    }

    // No need to re-get the generation, even though GetComputedStyle
    // will flush, since we flushed style at the top of this function.
    // We don't need to check this if we only flushed the parent.
    NS_ASSERTION(
        !didFlush ||
            currentGeneration ==
                mPresShell->GetPresContext()->GetUndisplayedRestyleGeneration(),
        "why should we have flushed style again?");

    SetResolvedComputedStyle(std::move(resolvedComputedStyle),
                             currentGeneration);
    NS_ASSERTION(mPseudo != PseudoStyleType::NotPseudo ||
                     !mComputedStyle->HasPseudoElementData(),
                 "should not have pseudo-element data");
  }

  // mExposeVisitedStyle is set to true only by testing APIs that
  // require chrome privilege.
  MOZ_ASSERT(!mExposeVisitedStyle || nsContentUtils::IsCallerChrome(),
             "mExposeVisitedStyle set incorrectly");
  if (mExposeVisitedStyle && mComputedStyle->RelevantLinkVisited()) {
    if (const auto* styleIfVisited = mComputedStyle->GetStyleIfVisited()) {
      mComputedStyle = styleIfVisited;
    }
  }
}

void nsComputedDOMStyle::ClearCurrentStyleSources() {
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

void nsComputedDOMStyle::RemoveProperty(const nsACString& aPropertyName,
                                        nsACString& aReturn, ErrorResult& aRv) {
  // Note: not using nsPrintfCString here in case aPropertyName contains
  // nulls.
  aRv.ThrowNoModificationAllowedError("Can't remove property '"_ns +
                                      aPropertyName +
                                      "' from computed style"_ns);
}

void nsComputedDOMStyle::GetPropertyPriority(const nsACString& aPropertyName,
                                             nsACString& aReturn) {
  aReturn.Truncate();
}

void nsComputedDOMStyle::SetProperty(const nsACString& aPropertyName,
                                     const nsACString& aValue,
                                     const nsACString& aPriority,
                                     nsIPrincipal* aSubjectPrincipal,
                                     ErrorResult& aRv) {
  // Note: not using nsPrintfCString here in case aPropertyName contains
  // nulls.
  aRv.ThrowNoModificationAllowedError("Can't set value for property '"_ns +
                                      aPropertyName + "' in computed style"_ns);
}

void nsComputedDOMStyle::IndexedGetter(uint32_t aIndex, bool& aFound,
                                       nsACString& aPropName) {
  ComputedStyleMap* map = GetComputedStyleMap();
  uint32_t length = map->Length();

  if (aIndex < length) {
    aFound = true;
    aPropName.Assign(nsCSSProps::GetStringValue(map->PropertyAt(aIndex)));
    return;
  }

  // Custom properties are exposed with indexed properties just after all
  // of the built-in properties.
  UpdateCurrentStyleSources(eCSSPropertyExtra_variable);
  if (!mComputedStyle) {
    aFound = false;
    return;
  }

  uint32_t count = Servo_GetCustomPropertiesCount(mComputedStyle);

  const uint32_t index = aIndex - length;
  if (index < count) {
    aFound = true;
    aPropName.AssignLiteral("--");
    if (nsAtom* atom = Servo_GetCustomPropertyNameAt(mComputedStyle, index)) {
      aPropName.Append(nsAtomCString(atom));
    }
  } else {
    aFound = false;
  }

  ClearCurrentStyleSources();
}

// Property getters...

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetBottom() {
  return GetOffsetWidthFor(eSideBottom);
}

static Position MaybeResolvePositionForTransform(const LengthPercentage& aX,
                                                 const LengthPercentage& aY,
                                                 nsIFrame* aInnerFrame) {
  if (!aInnerFrame) {
    return {aX, aY};
  }
  nsStyleTransformMatrix::TransformReferenceBox refBox(aInnerFrame);
  CSSPoint p = nsStyleTransformMatrix::Convert2DPosition(aX, aY, refBox);
  return {LengthPercentage::FromPixels(p.x), LengthPercentage::FromPixels(p.y)};
}

/* Convert the stored representation into a list of two values and then hand
 * it back.
 */
already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetTransformOrigin() {
  /* Store things as a value list */
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);

  /* Now, get the values. */
  const auto& origin = StyleDisplay()->mTransformOrigin;

  auto position = MaybeResolvePositionForTransform(
      origin.horizontal, origin.vertical, mInnerFrame);
  SetValueToPosition(position, valueList);
  if (!origin.depth.IsZero()) {
    auto depth = MakeRefPtr<nsROCSSPrimitiveValue>();
    depth->SetPixels(origin.depth.ToCSSPixels());
    valueList->AppendCSSValue(depth.forget());
  }
  return valueList.forget();
}

/* Convert the stored representation into a list of two values and then hand
 * it back.
 */
already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetPerspectiveOrigin() {
  /* Store things as a value list */
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);

  /* Now, get the values. */
  const auto& origin = StyleDisplay()->mPerspectiveOrigin;

  auto position = MaybeResolvePositionForTransform(
      origin.horizontal, origin.vertical, mInnerFrame);
  SetValueToPosition(position, valueList);
  return valueList.forget();
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetTransform() {
  const nsStyleDisplay* display = StyleDisplay();
  return GetTransformValue(display->mTransform);
}

/* static */
already_AddRefed<nsROCSSPrimitiveValue> nsComputedDOMStyle::MatrixToCSSValue(
    const mozilla::gfx::Matrix4x4& matrix) {
  bool is3D = !matrix.Is2D();

  nsAutoString resultString(u"matrix"_ns);
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
  auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
  val->SetString(resultString);
  return val.forget();
}

already_AddRefed<nsROCSSPrimitiveValue> nsComputedDOMStyle::AppUnitsToCSSValue(
    nscoord aAppUnits) {
  return PixelsToCSSValue(CSSPixel::FromAppUnits(aAppUnits));
}

already_AddRefed<nsROCSSPrimitiveValue> nsComputedDOMStyle::PixelsToCSSValue(
    float aPixels) {
  auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
  SetValueToPixels(val, aPixels);
  return val.forget();
}

void nsComputedDOMStyle::SetValueToPixels(nsROCSSPrimitiveValue* aValue,
                                          float aPixels) {
  MOZ_ASSERT(mComputedStyle);
  aValue->SetPixels(mComputedStyle->EffectiveZoom().Unzoom(aPixels));
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetMozOsxFontSmoothing() {
  if (nsContentUtils::ShouldResistFingerprinting(
          mPresShell->GetPresContext()->GetDocShell(),
          RFPTarget::DOMStyleOsxFontSmoothing)) {
    return nullptr;
  }

  nsAutoCString result;
  mComputedStyle->GetComputedPropertyValue(eCSSProperty__moz_osx_font_smoothing,
                                           result);
  auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
  val->SetString(result);
  return val.forget();
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetImageLayerPosition(
    const nsStyleImageLayers& aLayers) {
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

void nsComputedDOMStyle::SetValueToPosition(const Position& aPosition,
                                            nsDOMCSSValueList* aValueList) {
  auto valX = MakeRefPtr<nsROCSSPrimitiveValue>();
  SetValueToLengthPercentage(valX, aPosition.horizontal, false);
  aValueList->AppendCSSValue(valX.forget());

  auto valY = MakeRefPtr<nsROCSSPrimitiveValue>();
  SetValueToLengthPercentage(valY, aPosition.vertical, false);
  aValueList->AppendCSSValue(valY.forget());
}

enum class Brackets { No, Yes };

static void AppendGridLineNames(nsACString& aResult,
                                Span<const StyleCustomIdent> aLineNames,
                                Brackets aBrackets) {
  if (aLineNames.IsEmpty()) {
    if (aBrackets == Brackets::Yes) {
      aResult.AppendLiteral("[]");
    }
    return;
  }
  uint32_t numLines = aLineNames.Length();
  if (aBrackets == Brackets::Yes) {
    aResult.Append('[');
  }
  for (uint32_t i = 0;;) {
    // TODO: Maybe use servo to do this and avoid the silly utf16->utf8 dance?
    nsAutoString name;
    nsStyleUtil::AppendEscapedCSSIdent(
        nsDependentAtomString(aLineNames[i].AsAtom()), name);
    AppendUTF16toUTF8(name, aResult);

    if (++i == numLines) {
      break;
    }
    aResult.Append(' ');
  }
  if (aBrackets == Brackets::Yes) {
    aResult.Append(']');
  }
}

static void AppendGridLineNames(nsDOMCSSValueList* aValueList,
                                Span<const StyleCustomIdent> aLineNames,
                                bool aSuppressEmptyList = true) {
  if (aLineNames.IsEmpty() && aSuppressEmptyList) {
    return;
  }
  auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
  nsAutoCString lineNamesString;
  AppendGridLineNames(lineNamesString, aLineNames, Brackets::Yes);
  val->SetString(lineNamesString);
  aValueList->AppendCSSValue(val.forget());
}

static void AppendGridLineNames(nsDOMCSSValueList* aValueList,
                                Span<const StyleCustomIdent> aLineNames1,
                                Span<const StyleCustomIdent> aLineNames2) {
  if (aLineNames1.IsEmpty() && aLineNames2.IsEmpty()) {
    return;
  }
  auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
  nsAutoCString lineNamesString;
  lineNamesString.Assign('[');
  if (!aLineNames1.IsEmpty()) {
    AppendGridLineNames(lineNamesString, aLineNames1, Brackets::No);
  }
  if (!aLineNames2.IsEmpty()) {
    if (!aLineNames1.IsEmpty()) {
      lineNamesString.Append(' ');
    }
    AppendGridLineNames(lineNamesString, aLineNames2, Brackets::No);
  }
  lineNamesString.Append(']');
  val->SetString(lineNamesString);
  aValueList->AppendCSSValue(val.forget());
}

void nsComputedDOMStyle::SetValueToTrackBreadth(
    nsROCSSPrimitiveValue* aValue, const StyleTrackBreadth& aBreadth) {
  using Tag = StyleTrackBreadth::Tag;
  switch (aBreadth.tag) {
    case Tag::MinContent:
      return aValue->SetString("min-content");
    case Tag::MaxContent:
      return aValue->SetString("max-content");
    case Tag::Auto:
      return aValue->SetString("auto");
    case Tag::Breadth:
      return SetValueToLengthPercentage(aValue, aBreadth.AsBreadth(), true);
    case Tag::Fr: {
      nsAutoString tmpStr;
      nsStyleUtil::AppendCSSNumber(aBreadth.AsFr(), tmpStr);
      tmpStr.AppendLiteral("fr");
      return aValue->SetString(tmpStr);
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown breadth value");
      return;
  }
}

already_AddRefed<nsROCSSPrimitiveValue> nsComputedDOMStyle::GetGridTrackBreadth(
    const StyleTrackBreadth& aBreadth) {
  auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
  SetValueToTrackBreadth(val, aBreadth);
  return val.forget();
}

already_AddRefed<nsROCSSPrimitiveValue> nsComputedDOMStyle::GetGridTrackSize(
    const StyleTrackSize& aTrackSize) {
  if (aTrackSize.IsFitContent()) {
    // A fit-content() function.
    auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
    MOZ_ASSERT(aTrackSize.AsFitContent().IsBreadth(),
               "unexpected unit for fit-content() argument value");
    SetValueFromFitContentFunction(val, aTrackSize.AsFitContent().AsBreadth());
    return val.forget();
  }

  if (aTrackSize.IsBreadth()) {
    return GetGridTrackBreadth(aTrackSize.AsBreadth());
  }

  MOZ_ASSERT(aTrackSize.IsMinmax());
  const auto& min = aTrackSize.AsMinmax()._0;
  const auto& max = aTrackSize.AsMinmax()._1;
  if (min == max) {
    return GetGridTrackBreadth(min);
  }

  // minmax(auto, <flex>) is equivalent to (and is our internal representation
  // of) <flex>, and both compute to <flex>
  if (min.IsAuto() && max.IsFr()) {
    return GetGridTrackBreadth(max);
  }

  nsAutoString argumentStr, minmaxStr;
  minmaxStr.AppendLiteral("minmax(");

  {
    RefPtr<nsROCSSPrimitiveValue> argValue = GetGridTrackBreadth(min);
    argValue->GetCssText(argumentStr);
    minmaxStr.Append(argumentStr);
    argumentStr.Truncate();
  }

  minmaxStr.AppendLiteral(", ");

  {
    RefPtr<nsROCSSPrimitiveValue> argValue = GetGridTrackBreadth(max);
    argValue->GetCssText(argumentStr);
    minmaxStr.Append(argumentStr);
  }

  minmaxStr.Append(char16_t(')'));
  auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
  val->SetString(minmaxStr);
  return val.forget();
}

already_AddRefed<CSSValue> nsComputedDOMStyle::GetGridTemplateColumnsRows(
    const StyleGridTemplateComponent& aTrackList,
    const ComputedGridTrackInfo& aTrackInfo) {
  if (aTrackInfo.mIsMasonry) {
    auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
    val->SetString("masonry");
    return val.forget();
  }

  if (aTrackInfo.mIsSubgrid) {
    RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);
    auto subgridKeyword = MakeRefPtr<nsROCSSPrimitiveValue>();
    subgridKeyword->SetString("subgrid");
    valueList->AppendCSSValue(subgridKeyword.forget());
    for (const auto& lineNames : aTrackInfo.mResolvedLineNames) {
      AppendGridLineNames(valueList, lineNames, /*aSuppressEmptyList*/ false);
    }
    uint32_t line = aTrackInfo.mResolvedLineNames.Length();
    uint32_t lastLine = aTrackInfo.mNumExplicitTracks + 1;
    const Span<const StyleCustomIdent> empty;
    for (; line < lastLine; ++line) {
      AppendGridLineNames(valueList, empty, /*aSuppressEmptyList*/ false);
    }
    return valueList.forget();
  }

  const bool serializeImplicit =
      StaticPrefs::layout_css_serialize_grid_implicit_tracks();

  const nsTArray<nscoord>& trackSizes = aTrackInfo.mSizes;
  const uint32_t numExplicitTracks = aTrackInfo.mNumExplicitTracks;
  const uint32_t numLeadingImplicitTracks =
      aTrackInfo.mNumLeadingImplicitTracks;
  uint32_t numSizes = trackSizes.Length();
  MOZ_ASSERT(numSizes >= numLeadingImplicitTracks + numExplicitTracks);

  const bool hasTracksToSerialize =
      serializeImplicit ? !!numSizes : !!numExplicitTracks;
  const bool hasRepeatAuto = aTrackList.HasRepeatAuto();
  if (!hasTracksToSerialize && !hasRepeatAuto) {
    auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
    val->SetString("none");
    return val.forget();
  }

  // We've done layout on the grid and have resolved the sizes of its tracks,
  // so we'll return those sizes here.  The grid spec says we MAY use
  // repeat(<positive-integer>, Npx) here for consecutive tracks with the same
  // size, but that doesn't seem worth doing since even for repeat(auto-*)
  // the resolved size might differ for the repeated tracks.
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);

  // Add any leading implicit tracks.
  if (serializeImplicit) {
    for (uint32_t i = 0; i < numLeadingImplicitTracks; ++i) {
      valueList->AppendCSSValue(AppUnitsToCSSValue(trackSizes[i]));
    }
  }

  if (hasRepeatAuto) {
    const auto* const autoRepeatValue = aTrackList.GetRepeatAutoValue();
    const auto repeatLineNames = autoRepeatValue->line_names.AsSpan();
    MOZ_ASSERT(repeatLineNames.Length() >= 2);
    // Number of tracks inside the repeat, not including any repetitions.
    // Check that if we have truncated the number of tracks due to overflowing
    // the maximum track limit then we also truncate this repeat count.
    MOZ_ASSERT(repeatLineNames.Length() ==
               autoRepeatValue->track_sizes.len + 1);
    // If we have truncated the first repetition of repeat tracks, then we
    // can't index using autoRepeatValue->track_sizes.len, and
    // aTrackInfo.mRemovedRepeatTracks.Length() will account for all repeat
    // tracks that haven't been truncated.
    const uint32_t numRepeatTracks =
        std::min(aTrackInfo.mRemovedRepeatTracks.Length(),
                 autoRepeatValue->track_sizes.len);
    MOZ_ASSERT(repeatLineNames.Length() >= numRepeatTracks + 1);
    // The total of all tracks in all repetitions of the repeat.
    const uint32_t totalNumRepeatTracks =
        aTrackInfo.mRemovedRepeatTracks.Length();
    const uint32_t repeatStart = aTrackInfo.mRepeatFirstTrack;
    // We need to skip over any track sizes which were resolved to 0 by
    // collapsed tracks. Keep track of the iteration separately.
    const auto explicitTrackSizeBegin =
        trackSizes.cbegin() + numLeadingImplicitTracks;
    const auto explicitTrackSizeEnd =
        explicitTrackSizeBegin + numExplicitTracks;
    auto trackSizeIter = explicitTrackSizeBegin;
    // Write any leading explicit tracks before the repeat.
    for (uint32_t i = 0; i < repeatStart; i++) {
      AppendGridLineNames(valueList, aTrackInfo.mResolvedLineNames[i]);
      valueList->AppendCSSValue(AppUnitsToCSSValue(*trackSizeIter++));
    }
    auto lineNameIter = aTrackInfo.mResolvedLineNames.cbegin() + repeatStart;
    // Write the track names at the start of the repeat, including the names
    // at the end of the last non-repeat track. Unlike all later repeat line
    // name lists, this one needs the resolved line name which includes both
    // the last non-repeat line names and the leading repeat line names.
    AppendGridLineNames(valueList, *lineNameIter++);
    {
      // Write out the first repeat value, checking for size zero (removed
      // track).
      const nscoord firstRepeatTrackSize =
          (!aTrackInfo.mRemovedRepeatTracks[0]) ? *trackSizeIter++ : 0;
      valueList->AppendCSSValue(AppUnitsToCSSValue(firstRepeatTrackSize));
    }
    // Write the line names and track sizes inside the repeat, checking for
    // removed tracks (size 0).
    for (uint32_t i = 1; i < totalNumRepeatTracks; i++) {
      const uint32_t repeatIndex = i % numRepeatTracks;
      // If we are rolling over from one repetition to the next, include track
      // names from both the end of the previous repeat and the start of the
      // next.
      if (repeatIndex == 0) {
        AppendGridLineNames(valueList,
                            repeatLineNames[numRepeatTracks].AsSpan(),
                            repeatLineNames[0].AsSpan());
      } else {
        AppendGridLineNames(valueList, repeatLineNames[repeatIndex].AsSpan());
      }
      MOZ_ASSERT(aTrackInfo.mRemovedRepeatTracks[i] ||
                 trackSizeIter != explicitTrackSizeEnd);
      const nscoord repeatTrackSize =
          (!aTrackInfo.mRemovedRepeatTracks[i]) ? *trackSizeIter++ : 0;
      valueList->AppendCSSValue(AppUnitsToCSSValue(repeatTrackSize));
    }
    // The resolved line names include a single repetition of the auto-repeat
    // line names. Skip over those.
    lineNameIter += numRepeatTracks - 1;
    // Write out any more tracks after the repeat.
    while (trackSizeIter != explicitTrackSizeEnd) {
      AppendGridLineNames(valueList, *lineNameIter++);
      valueList->AppendCSSValue(AppUnitsToCSSValue(*trackSizeIter++));
    }
    // Write the final trailing line name.
    AppendGridLineNames(valueList, *lineNameIter++);
  } else if (numExplicitTracks > 0) {
    // If there are explicit tracks but no repeat tracks, just serialize those.
    for (uint32_t i = 0;; i++) {
      AppendGridLineNames(valueList, aTrackInfo.mResolvedLineNames[i]);
      if (i == numExplicitTracks) {
        break;
      }
      valueList->AppendCSSValue(
          AppUnitsToCSSValue(trackSizes[i + numLeadingImplicitTracks]));
    }
  }
  // Add any trailing implicit tracks.
  if (serializeImplicit) {
    for (uint32_t i = numLeadingImplicitTracks + numExplicitTracks;
         i < numSizes; ++i) {
      valueList->AppendCSSValue(AppUnitsToCSSValue(trackSizes[i]));
    }
  }

  return valueList.forget();
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetGridTemplateColumns() {
  nsGridContainerFrame* gridFrame =
      nsGridContainerFrame::GetGridFrameWithComputedInfo(mInnerFrame);
  if (!gridFrame) {
    // The element doesn't have a box - return the computed value.
    // https://drafts.csswg.org/css-grid/#resolved-track-list
    nsAutoCString string;
    mComputedStyle->GetComputedPropertyValue(eCSSProperty_grid_template_columns,
                                             string);
    auto value = MakeRefPtr<nsROCSSPrimitiveValue>();
    value->SetString(string);
    return value.forget();
  }

  // GetGridFrameWithComputedInfo() above ensures that this returns non-null:
  const ComputedGridTrackInfo* info = gridFrame->GetComputedTemplateColumns();
  return GetGridTemplateColumnsRows(StylePosition()->mGridTemplateColumns,
                                    *info);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetGridTemplateRows() {
  nsGridContainerFrame* gridFrame =
      nsGridContainerFrame::GetGridFrameWithComputedInfo(mInnerFrame);
  if (!gridFrame) {
    // The element doesn't have a box - return the computed value.
    // https://drafts.csswg.org/css-grid/#resolved-track-list
    nsAutoCString string;
    mComputedStyle->GetComputedPropertyValue(eCSSProperty_grid_template_rows,
                                             string);
    auto value = MakeRefPtr<nsROCSSPrimitiveValue>();
    value->SetString(string);
    return value.forget();
  }

  // GetGridFrameWithComputedInfo() above ensures that this returns non-null:
  const ComputedGridTrackInfo* info = gridFrame->GetComputedTemplateRows();
  return GetGridTemplateColumnsRows(StylePosition()->mGridTemplateRows, *info);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetPaddingTop() {
  return GetPaddingWidthFor(eSideTop);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetPaddingBottom() {
  return GetPaddingWidthFor(eSideBottom);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetPaddingLeft() {
  return GetPaddingWidthFor(eSideLeft);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetPaddingRight() {
  return GetPaddingWidthFor(eSideRight);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetBorderSpacing() {
  RefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(false);

  const nsStyleTableBorder* border = StyleTableBorder();
  valueList->AppendCSSValue(
      PixelsToCSSValue(border->mBorderSpacing.width.ToCSSPixels()));
  valueList->AppendCSSValue(
      PixelsToCSSValue(border->mBorderSpacing.height.ToCSSPixels()));

  return valueList.forget();
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetBorderTopWidth() {
  return GetBorderWidthFor(eSideTop);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetBorderBottomWidth() {
  return GetBorderWidthFor(eSideBottom);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetBorderLeftWidth() {
  return GetBorderWidthFor(eSideLeft);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetBorderRightWidth() {
  return GetBorderWidthFor(eSideRight);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetMarginTop() {
  return GetMarginFor(eSideTop);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetMarginBottom() {
  return GetMarginFor(eSideBottom);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetMarginLeft() {
  return GetMarginFor(eSideLeft);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetMarginRight() {
  return GetMarginFor(eSideRight);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetHeight() {
  if (mInnerFrame && !IsNonReplacedInline(mInnerFrame)) {
    AssertFlushedPendingReflows();
    const nsMargin adjustedValues = GetAdjustedValuesForBoxSizing();
    return AppUnitsToCSSValue(mInnerFrame->GetContentRect().height +
                              adjustedValues.TopBottom());
  }
  auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
  SetValueToSize(val, StylePosition()->mHeight);
  return val.forget();
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetWidth() {
  if (mInnerFrame && !IsNonReplacedInline(mInnerFrame)) {
    AssertFlushedPendingReflows();
    nsMargin adjustedValues = GetAdjustedValuesForBoxSizing();
    return AppUnitsToCSSValue(mInnerFrame->GetContentRect().width +
                              adjustedValues.LeftRight());
  }
  auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
  SetValueToSize(val, StylePosition()->mWidth);
  return val.forget();
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetMaxHeight() {
  auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
  SetValueToMaxSize(val, StylePosition()->mMaxHeight);
  return val.forget();
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetMaxWidth() {
  auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
  SetValueToMaxSize(val, StylePosition()->mMaxWidth);
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
bool nsComputedDOMStyle::ShouldHonorMinSizeAutoInAxis(PhysicalAxis aAxis) {
  return mOuterFrame && mOuterFrame->IsFlexOrGridItem();
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetMinHeight() {
  auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
  StyleSize minHeight = StylePosition()->mMinHeight;

  if (minHeight.IsAuto() &&
      !ShouldHonorMinSizeAutoInAxis(PhysicalAxis::Vertical)) {
    minHeight = StyleSize::LengthPercentage(LengthPercentage::Zero());
  }

  SetValueToSize(val, minHeight);
  return val.forget();
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetMinWidth() {
  auto val = MakeRefPtr<nsROCSSPrimitiveValue>();

  StyleSize minWidth = StylePosition()->mMinWidth;

  if (minWidth.IsAuto() &&
      !ShouldHonorMinSizeAutoInAxis(PhysicalAxis::Horizontal)) {
    minWidth = StyleSize::LengthPercentage(LengthPercentage::Zero());
  }

  SetValueToSize(val, minWidth);
  return val.forget();
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetLeft() {
  return GetOffsetWidthFor(eSideLeft);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetRight() {
  return GetOffsetWidthFor(eSideRight);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DoGetTop() {
  return GetOffsetWidthFor(eSideTop);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::GetOffsetWidthFor(
    mozilla::Side aSide) {
  const nsStyleDisplay* display = StyleDisplay();

  mozilla::StylePositionProperty position = display->mPosition;
  if (!mOuterFrame) {
    // GetNonStaticPositionOffset or GetAbsoluteOffset don't handle elements
    // without frames in any sensible way. GetStaticOffset, however, is perfect
    // for that case.
    position = StylePositionProperty::Static;
  }

  switch (position) {
    case StylePositionProperty::Static:
      return GetStaticOffset(aSide);
    case StylePositionProperty::Sticky:
      return GetNonStaticPositionOffset(
          aSide, false, &nsComputedDOMStyle::GetScrollFrameContentWidth,
          &nsComputedDOMStyle::GetScrollFrameContentHeight);
    case StylePositionProperty::Absolute:
    case StylePositionProperty::Fixed:
      return GetAbsoluteOffset(aSide);
    case StylePositionProperty::Relative:
      return GetNonStaticPositionOffset(
          aSide, true, &nsComputedDOMStyle::GetCBContentWidth,
          &nsComputedDOMStyle::GetCBContentHeight);
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid position");
      return nullptr;
  }
}

static_assert(eSideTop == 0 && eSideRight == 1 && eSideBottom == 2 &&
                  eSideLeft == 3,
              "box side constants not as expected for NS_OPPOSITE_SIDE");
#define NS_OPPOSITE_SIDE(s_) mozilla::Side(((s_) + 2) & 3)

already_AddRefed<CSSValue> nsComputedDOMStyle::GetNonStaticPositionOffset(
    mozilla::Side aSide, bool aResolveAuto, PercentageBaseGetter aWidthGetter,
    PercentageBaseGetter aHeightGetter) {
  const nsStylePosition* positionData = StylePosition();
  int32_t sign = 1;
  LengthPercentageOrAuto coord = positionData->mOffset.Get(aSide);

  if (coord.IsAuto()) {
    if (!aResolveAuto) {
      auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
      val->SetString("auto");
      return val.forget();
    }
    coord = positionData->mOffset.Get(NS_OPPOSITE_SIDE(aSide));
    sign = -1;
  }
  if (!coord.IsLengthPercentage()) {
    return PixelsToCSSValue(0.0f);
  }

  const auto& lp = coord.AsLengthPercentage();
  if (lp.ConvertsToLength()) {
    return PixelsToCSSValue(sign * lp.ToLengthInCSSPixels());
  }

  PercentageBaseGetter baseGetter = (aSide == eSideLeft || aSide == eSideRight)
                                        ? aWidthGetter
                                        : aHeightGetter;
  nscoord percentageBase;
  if (!(this->*baseGetter)(percentageBase)) {
    return PixelsToCSSValue(0.0f);
  }
  nscoord result = lp.Resolve(percentageBase);
  return AppUnitsToCSSValue(sign * result);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::GetAbsoluteOffset(
    mozilla::Side aSide) {
  const auto& offset = StylePosition()->mOffset;
  const auto& coord = offset.Get(aSide);
  const auto& oppositeCoord = offset.Get(NS_OPPOSITE_SIDE(aSide));

  if (coord.IsAuto() || oppositeCoord.IsAuto()) {
    return AppUnitsToCSSValue(GetUsedAbsoluteOffset(aSide));
  }

  return GetNonStaticPositionOffset(
      aSide, false, &nsComputedDOMStyle::GetCBPaddingRectWidth,
      &nsComputedDOMStyle::GetCBPaddingRectHeight);
}

nscoord nsComputedDOMStyle::GetUsedAbsoluteOffset(mozilla::Side aSide) {
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
    ScrollContainerFrame* scrollContainerFrame = do_QueryFrame(scrollingChild);
    if (scrollContainerFrame) {
      scrollbarSizes = scrollContainerFrame->GetActualScrollbarSizes();
    }

    // The viewport size might have been expanded by the visual viewport or
    // the minimum-scale size.
    const ViewportFrame* viewportFrame = do_QueryFrame(container);
    MOZ_ASSERT(viewportFrame);
    containerRect.SizeTo(
        viewportFrame->AdjustViewportSizeForFixedPosition(containerRect));
  } else if (container->IsGridContainerFrame() &&
             mOuterFrame->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW)) {
    containerRect = nsGridContainerFrame::GridItemCB(mOuterFrame);
    rect.MoveBy(-containerRect.x, -containerRect.y);
  }

  nscoord offset = 0;
  switch (aSide) {
    case eSideTop:
      offset = rect.y - margin.top - border.top - scrollbarSizes.top;

      break;
    case eSideRight:
      offset = containerRect.width - rect.width - rect.x - margin.right -
               border.right - scrollbarSizes.right;

      break;
    case eSideBottom:
      offset = containerRect.height - rect.height - rect.y - margin.bottom -
               border.bottom - scrollbarSizes.bottom;

      break;
    case eSideLeft:
      offset = rect.x - margin.left - border.left - scrollbarSizes.left;

      break;
    default:
      NS_ERROR("Invalid side");
      break;
  }

  return offset;
}

already_AddRefed<CSSValue> nsComputedDOMStyle::GetStaticOffset(
    mozilla::Side aSide) {
  auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
  SetValueToLengthPercentageOrAuto(val, StylePosition()->mOffset.Get(aSide),
                                   false);
  return val.forget();
}

already_AddRefed<CSSValue> nsComputedDOMStyle::GetPaddingWidthFor(
    mozilla::Side aSide) {
  const auto& padding = StylePadding()->mPadding.Get(aSide);
  if (!mInnerFrame || !PaddingNeedsUsedValue(padding, *mComputedStyle)) {
    auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
    SetValueToLengthPercentage(val, padding, true);
    return val.forget();
  }
  AssertFlushedPendingReflows();
  return AppUnitsToCSSValue(mInnerFrame->GetUsedPadding().Side(aSide));
}

already_AddRefed<CSSValue> nsComputedDOMStyle::GetBorderWidthFor(
    mozilla::Side aSide) {
  nscoord width;
  if (mInnerFrame && mComputedStyle->StyleDisplay()->HasAppearance()) {
    AssertFlushedPendingReflows();
    width = mInnerFrame->GetUsedBorder().Side(aSide);
  } else {
    width = StyleBorder()->GetComputedBorderWidth(aSide);
  }
  return AppUnitsToCSSValue(width);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::GetMarginFor(Side aSide) {
  const auto& margin = StyleMargin()->mMargin.Get(aSide);
  if (!mInnerFrame || margin.ConvertsToLength()) {
    auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
    SetValueToLengthPercentageOrAuto(val, margin, false);
    return val.forget();
  }
  AssertFlushedPendingReflows();
  // For tables, GetUsedMargin always returns an empty margin, so we
  // should read the margin from the table wrapper frame instead.
  NS_ASSERTION(
      mOuterFrame == mInnerFrame || mInnerFrame->GetUsedMargin() == nsMargin(),
      "Inner tables must have zero margins");
  return AppUnitsToCSSValue(mOuterFrame->GetUsedMargin().Side(aSide));
}

static void SetValueToExtremumLength(nsROCSSPrimitiveValue* aValue,
                                     nsIFrame::ExtremumLength aSize) {
  switch (aSize) {
    case nsIFrame::ExtremumLength::MaxContent:
      return aValue->SetString("max-content");
    case nsIFrame::ExtremumLength::MinContent:
      return aValue->SetString("min-content");
    case nsIFrame::ExtremumLength::MozAvailable:
      return aValue->SetString("-moz-available");
    case nsIFrame::ExtremumLength::FitContent:
      return aValue->SetString("fit-content");
    case nsIFrame::ExtremumLength::FitContentFunction:
      MOZ_ASSERT_UNREACHABLE("fit-content() should be handled separately");
  }
  MOZ_ASSERT_UNREACHABLE("Unknown extremum length?");
}

void nsComputedDOMStyle::SetValueFromFitContentFunction(
    nsROCSSPrimitiveValue* aValue, const LengthPercentage& aLength) {
  nsAutoString argumentStr;
  SetValueToLengthPercentage(aValue, aLength, true);
  aValue->GetCssText(argumentStr);

  nsAutoString fitContentStr;
  fitContentStr.AppendLiteral("fit-content(");
  fitContentStr.Append(argumentStr);
  fitContentStr.Append(u')');
  aValue->SetString(fitContentStr);
}

void nsComputedDOMStyle::SetValueToSize(nsROCSSPrimitiveValue* aValue,
                                        const StyleSize& aSize) {
  if (aSize.IsAuto()) {
    return aValue->SetString("auto");
  }
  if (aSize.IsFitContentFunction()) {
    return SetValueFromFitContentFunction(aValue, aSize.AsFitContentFunction());
  }
  if (auto length = nsIFrame::ToExtremumLength(aSize)) {
    return SetValueToExtremumLength(aValue, *length);
  }
  MOZ_ASSERT(aSize.IsLengthPercentage());
  SetValueToLengthPercentage(aValue, aSize.AsLengthPercentage(), true);
}

void nsComputedDOMStyle::SetValueToMaxSize(nsROCSSPrimitiveValue* aValue,
                                           const StyleMaxSize& aSize) {
  if (aSize.IsNone()) {
    return aValue->SetString("none");
  }
  if (aSize.IsFitContentFunction()) {
    return SetValueFromFitContentFunction(aValue, aSize.AsFitContentFunction());
  }
  if (auto length = nsIFrame::ToExtremumLength(aSize)) {
    return SetValueToExtremumLength(aValue, *length);
  }
  MOZ_ASSERT(aSize.IsLengthPercentage());
  SetValueToLengthPercentage(aValue, aSize.AsLengthPercentage(), true);
}

void nsComputedDOMStyle::SetValueToLengthPercentageOrAuto(
    nsROCSSPrimitiveValue* aValue, const LengthPercentageOrAuto& aSize,
    bool aClampNegativeCalc) {
  if (aSize.IsAuto()) {
    return aValue->SetString("auto");
  }
  SetValueToLengthPercentage(aValue, aSize.AsLengthPercentage(),
                             aClampNegativeCalc);
}

void nsComputedDOMStyle::SetValueToLengthPercentage(
    nsROCSSPrimitiveValue* aValue, const mozilla::LengthPercentage& aLength,
    bool aClampNegativeCalc) {
  if (aLength.ConvertsToLength()) {
    CSSCoord length = aLength.ToLengthInCSSPixels();
    if (aClampNegativeCalc) {
      length = std::max(float(length), 0.0f);
    }
    return SetValueToPixels(aValue, length);
  }
  if (aLength.ConvertsToPercentage()) {
    float result = aLength.ToPercentage();
    if (aClampNegativeCalc) {
      result = std::max(result, 0.0f);
    }
    return aValue->SetPercent(result);
  }

  nsAutoCString result;
  Servo_LengthPercentage_ToCss(&aLength, &result);
  aValue->SetString(result);
}

bool nsComputedDOMStyle::GetCBContentWidth(nscoord& aWidth) {
  if (!mOuterFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  aWidth = mOuterFrame->GetContainingBlock()->GetContentRect().width;
  return true;
}

bool nsComputedDOMStyle::GetCBContentHeight(nscoord& aHeight) {
  if (!mOuterFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  aHeight = mOuterFrame->GetContainingBlock()->GetContentRect().height;
  return true;
}

bool nsComputedDOMStyle::GetCBPaddingRectWidth(nscoord& aWidth) {
  if (!mOuterFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  aWidth = mOuterFrame->GetContainingBlock()->GetPaddingRect().width;
  return true;
}

bool nsComputedDOMStyle::GetCBPaddingRectHeight(nscoord& aHeight) {
  if (!mOuterFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  aHeight = mOuterFrame->GetContainingBlock()->GetPaddingRect().height;
  return true;
}

bool nsComputedDOMStyle::GetScrollFrameContentWidth(nscoord& aWidth) {
  if (!mOuterFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  ScrollContainerFrame* scrollContainerFrame =
      nsLayoutUtils::GetNearestScrollContainerFrame(
          mOuterFrame->GetParent(),
          nsLayoutUtils::SCROLLABLE_SAME_DOC |
              nsLayoutUtils::SCROLLABLE_INCLUDE_HIDDEN);

  if (!scrollContainerFrame) {
    return false;
  }
  aWidth = scrollContainerFrame->GetScrolledFrame()
               ->GetContentRectRelativeToSelf()
               .width;
  return true;
}

bool nsComputedDOMStyle::GetScrollFrameContentHeight(nscoord& aHeight) {
  if (!mOuterFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  ScrollContainerFrame* scrollContainerFrame =
      nsLayoutUtils::GetNearestScrollContainerFrame(
          mOuterFrame->GetParent(),
          nsLayoutUtils::SCROLLABLE_SAME_DOC |
              nsLayoutUtils::SCROLLABLE_INCLUDE_HIDDEN);

  if (!scrollContainerFrame) {
    return false;
  }
  aHeight = scrollContainerFrame->GetScrolledFrame()
                ->GetContentRectRelativeToSelf()
                .height;
  return true;
}

bool nsComputedDOMStyle::GetFrameBorderRectWidth(nscoord& aWidth) {
  if (!mInnerFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  aWidth = mInnerFrame->GetSize().width;
  return true;
}

bool nsComputedDOMStyle::GetFrameBorderRectHeight(nscoord& aHeight) {
  if (!mInnerFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  aHeight = mInnerFrame->GetSize().height;
  return true;
}

/* If the property is "none", hand back "none" wrapped in a value.
 * Otherwise, compute the aggregate transform matrix and hands it back in a
 * "matrix" wrapper.
 */
already_AddRefed<CSSValue> nsComputedDOMStyle::GetTransformValue(
    const StyleTransform& aTransform) {
  /* If there are no transforms, then we should construct a single-element
   * entry and hand it back.
   */
  if (aTransform.IsNone()) {
    auto val = MakeRefPtr<nsROCSSPrimitiveValue>();
    val->SetString("none");
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
  nsStyleTransformMatrix::TransformReferenceBox refBox(mInnerFrame, nsRect());
  gfx::Matrix4x4 matrix = nsStyleTransformMatrix::ReadTransforms(
      aTransform, refBox, float(mozilla::AppUnitsPerCSSPixel()));

  return MatrixToCSSValue(matrix);
}

already_AddRefed<CSSValue> nsComputedDOMStyle::DummyGetter() {
  MOZ_CRASH("DummyGetter is not supposed to be invoked");
}

static void MarkComputedStyleMapDirty(const char* aPref, void* aMap) {
  static_cast<ComputedStyleMap*>(aMap)->MarkDirty();
}

void nsComputedDOMStyle::ParentChainChanged(nsIContent* aContent) {
  NS_ASSERTION(mElement == aContent, "didn't we register mElement?");
  NS_ASSERTION(mResolvedComputedStyle,
               "should have only registered an observer when "
               "mResolvedComputedStyle is true");

  ClearComputedStyle();
}

/* static */
ComputedStyleMap* nsComputedDOMStyle::GetComputedStyleMap() {
  static ComputedStyleMap map{};
  return &map;
}

static StaticAutoPtr<nsTArray<const char*>> gCallbackPrefs;

/* static */
void nsComputedDOMStyle::RegisterPrefChangeCallbacks() {
  // Note that this will register callbacks for all properties with prefs, not
  // just those that are implemented on computed style objects, as it's not
  // easy to grab specific property data from ServoCSSPropList.h based on the
  // entries iterated in nsComputedDOMStylePropertyList.h.

  AutoTArray<const char*, 64> prefs;
  for (const auto* p = nsCSSProps::kPropertyPrefTable;
       p->mPropID != eCSSProperty_UNKNOWN; p++) {
    // Many properties are controlled by the same preference, so de-duplicate
    // them before adding observers.
    //
    // Note: This is done by pointer comparison, which works because the mPref
    // members are string literals from the same same translation unit, and are
    // therefore de-duplicated by the compiler. On the off chance that we wind
    // up with some duplicates with different pointers, though, it's not a bit
    // deal.
    if (!prefs.ContainsSorted(p->mPref)) {
      prefs.InsertElementSorted(p->mPref);
    }
  }

  prefs.AppendElement(nullptr);

  MOZ_ASSERT(!gCallbackPrefs);
  gCallbackPrefs = new nsTArray<const char*>(std::move(prefs));

  Preferences::RegisterCallbacks(MarkComputedStyleMapDirty,
                                 gCallbackPrefs->Elements(),
                                 GetComputedStyleMap());
}

/* static */
void nsComputedDOMStyle::UnregisterPrefChangeCallbacks() {
  if (!gCallbackPrefs) {
    return;
  }

  Preferences::UnregisterCallbacks(MarkComputedStyleMapDirty,
                                   gCallbackPrefs->Elements(),
                                   GetComputedStyleMap());
  gCallbackPrefs = nullptr;
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* FFI functions for Servo to call into Gecko */

#include "mozilla/GeckoBindings.h"

#include "ChildIterator.h"
#include "ErrorReporter.h"
#include "GeckoProfiler.h"
#include "gfxFontFamilyList.h"
#include "gfxFontFeatures.h"
#include "gfxTextRun.h"
#include "imgLoader.h"
#include "nsAnimationManager.h"
#include "nsAttrValueInlines.h"
#include "nsCSSFrameConstructor.h"
#include "nsCSSProps.h"
#include "nsCSSPseudoElements.h"
#include "nsContentUtils.h"
#include "nsDOMTokenList.h"
#include "nsDeviceContext.h"
#include "nsLayoutUtils.h"
#include "nsIContentInlines.h"
#include "mozilla/dom/DocumentInlines.h"
#include "nsILoadContext.h"
#include "nsIFrame.h"
#include "nsIMozBrowserFrame.h"
#include "nsINode.h"
#include "nsIURI.h"
#include "nsFontMetrics.h"
#include "nsHTMLStyleSheet.h"
#include "nsMappedAttributes.h"
#include "nsNameSpaceManager.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsString.h"
#include "nsStyleStruct.h"
#include "nsStyleUtil.h"
#include "nsTArray.h"
#include "nsTransitionManager.h"
#include "nsWindowSizes.h"

#include "mozilla/css/ImageLoader.h"
#include "mozilla/DeclarationBlock.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EffectSet.h"
#include "mozilla/EventStates.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/Keyframe.h"
#include "mozilla/Mutex.h"
#include "mozilla/Preferences.h"
#include "mozilla/ServoElementSnapshot.h"
#include "mozilla/ShadowParts.h"
#include "mozilla/StaticPresData.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/SizeOfState.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoTraversalStatistics.h"
#include "mozilla/Telemetry.h"
#include "mozilla/RWLock.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/dom/HTMLTableCellElement.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "mozilla/dom/MediaList.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/URLExtraData.h"
#include "mozilla/dom/CSSMozDocumentRule.h"

#if defined(MOZ_MEMORY)
#  include "mozmemory.h"
#endif

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;

// Definitions of the global traversal stats.
bool ServoTraversalStatistics::sActive = false;
ServoTraversalStatistics ServoTraversalStatistics::sSingleton;

static RWLock* sServoFFILock = nullptr;

static const nsFont* ThreadSafeGetDefaultFontHelper(
    const Document& aDocument, nsAtom* aLanguage,
    StyleGenericFontFamily aGenericId) {
  bool needsCache = false;
  const nsFont* retval;

  auto GetDefaultFont = [&](bool* aNeedsToCache) {
    auto* prefs = aDocument.GetFontPrefsForLang(aLanguage, aNeedsToCache);
    return prefs ? prefs->GetDefaultFont(aGenericId) : nullptr;
  };

  {
    AutoReadLock guard(*sServoFFILock);
    retval = GetDefaultFont(&needsCache);
  }
  if (!needsCache) {
    return retval;
  }
  {
    AutoWriteLock guard(*sServoFFILock);
    retval = GetDefaultFont(nullptr);
  }
  return retval;
}

/*
 * Does this child count as significant for selector matching?
 *
 * See nsStyleUtil::IsSignificantChild for details.
 */
bool Gecko_IsSignificantChild(const nsINode* aNode,
                              bool aWhitespaceIsSignificant) {
  return nsStyleUtil::ThreadSafeIsSignificantChild(aNode->AsContent(),
                                                   aWhitespaceIsSignificant);
}

const nsINode* Gecko_GetLastChild(const nsINode* aNode) {
  return aNode->GetLastChild();
}

const nsINode* Gecko_GetPreviousSibling(const nsINode* aNode) {
  return aNode->GetPreviousSibling();
}

const nsINode* Gecko_GetFlattenedTreeParentNode(const nsINode* aNode) {
  return aNode->GetFlattenedTreeParentNodeForStyle();
}

const Element* Gecko_GetBeforeOrAfterPseudo(const Element* aElement,
                                            bool aIsBefore) {
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aElement->HasProperties());

  return aIsBefore ? nsLayoutUtils::GetBeforePseudo(aElement)
                   : nsLayoutUtils::GetAfterPseudo(aElement);
}

const Element* Gecko_GetMarkerPseudo(const Element* aElement) {
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aElement->HasProperties());

  return nsLayoutUtils::GetMarkerPseudo(aElement);
}

nsTArray<nsIContent*>* Gecko_GetAnonymousContentForElement(
    const Element* aElement) {
  nsIAnonymousContentCreator* ac = do_QueryFrame(aElement->GetPrimaryFrame());
  if (!ac) {
    return nullptr;
  }

  auto* array = new nsTArray<nsIContent*>();
  nsContentUtils::AppendNativeAnonymousChildren(aElement, *array, 0);
  return array;
}

void Gecko_DestroyAnonymousContentList(nsTArray<nsIContent*>* aAnonContent) {
  MOZ_ASSERT(aAnonContent);
  delete aAnonContent;
}

const nsTArray<RefPtr<nsINode>>* Gecko_GetAssignedNodes(
    const Element* aElement) {
  MOZ_ASSERT(HTMLSlotElement::FromNode(aElement));
  return &static_cast<const HTMLSlotElement*>(aElement)->AssignedNodes();
}

void Gecko_ComputedStyle_Init(ComputedStyle* aStyle,
                              const ServoComputedData* aValues,
                              PseudoStyleType aPseudoType) {
  new (KnownNotNull, aStyle)
      ComputedStyle(aPseudoType, ServoComputedDataForgotten(aValues));
}

ServoComputedData::ServoComputedData(const ServoComputedDataForgotten aValue) {
  PodAssign(this, aValue.mPtr);
}

MOZ_DEFINE_MALLOC_ENCLOSING_SIZE_OF(ServoStyleStructsMallocEnclosingSizeOf)

void ServoComputedData::AddSizeOfExcludingThis(nsWindowSizes& aSizes) const {
  // Note: GetStyleFoo() returns a pointer to an nsStyleFoo that sits within a
  // servo_arc::Arc, i.e. it is preceded by a word-sized refcount. So we need
  // to measure it with a function that can handle an interior pointer. We use
  // ServoStyleStructsEnclosingMallocSizeOf to clearly identify in DMD's
  // output the memory measured here.
#define STYLE_STRUCT(name_)                                       \
  static_assert(alignof(nsStyle##name_) <= sizeof(size_t),        \
                "alignment will break AddSizeOfExcludingThis()"); \
  const void* p##name_ = GetStyle##name_();                       \
  if (!aSizes.mState.HaveSeenPtr(p##name_)) {                     \
    aSizes.mStyleSizes.NS_STYLE_SIZES_FIELD(name_) +=             \
        ServoStyleStructsMallocEnclosingSizeOf(p##name_);         \
  }
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

  if (visited_style.mPtr && !aSizes.mState.HaveSeenPtr(visited_style.mPtr)) {
    visited_style.mPtr->AddSizeOfIncludingThis(
        aSizes, &aSizes.mLayoutComputedValuesVisited);
  }

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - custom_properties
  // - writing_mode
  // - rules
  // - font_computation_data
}

void Gecko_ComputedStyle_Destroy(ComputedStyle* aStyle) {
  aStyle->~ComputedStyle();
}

void Gecko_ConstructStyleChildrenIterator(const Element* aElement,
                                          StyleChildrenIterator* aIterator) {
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aIterator);
  new (aIterator) StyleChildrenIterator(aElement);
}

void Gecko_DestroyStyleChildrenIterator(StyleChildrenIterator* aIterator) {
  MOZ_ASSERT(aIterator);

  aIterator->~StyleChildrenIterator();
}

const nsINode* Gecko_GetNextStyleChild(StyleChildrenIterator* aIterator) {
  MOZ_ASSERT(aIterator);
  return aIterator->GetNextChild();
}

bool Gecko_VisitedStylesEnabled(const Document* aDoc) {
  MOZ_ASSERT(aDoc);
  MOZ_ASSERT(NS_IsMainThread());

  if (!StaticPrefs::layout_css_visited_links_enabled()) {
    return false;
  }

  if (aDoc->IsBeingUsedAsImage()) {
    return false;
  }

  nsILoadContext* loadContext = aDoc->GetLoadContext();
  if (loadContext && loadContext->UsePrivateBrowsing()) {
    return false;
  }

  return true;
}

EventStates::ServoType Gecko_ElementState(const Element* aElement) {
  return aElement->StyleState().ServoValue();
}

bool Gecko_IsRootElement(const Element* aElement) {
  return aElement->OwnerDoc()->GetRootElement() == aElement;
}

// Dirtiness tracking.
void Gecko_SetNodeFlags(const nsINode* aNode, uint32_t aFlags) {
  const_cast<nsINode*>(aNode)->SetFlags(aFlags);
}

void Gecko_UnsetNodeFlags(const nsINode* aNode, uint32_t aFlags) {
  const_cast<nsINode*>(aNode)->UnsetFlags(aFlags);
}

void Gecko_NoteDirtyElement(const Element* aElement) {
  MOZ_ASSERT(NS_IsMainThread());
  const_cast<Element*>(aElement)->NoteDirtyForServo();
}

void Gecko_NoteDirtySubtreeForInvalidation(const Element* aElement) {
  MOZ_ASSERT(NS_IsMainThread());
  const_cast<Element*>(aElement)->NoteDirtySubtreeForServo();
}

void Gecko_NoteAnimationOnlyDirtyElement(const Element* aElement) {
  MOZ_ASSERT(NS_IsMainThread());
  const_cast<Element*>(aElement)->NoteAnimationOnlyDirtyForServo();
}

bool Gecko_AnimationNameMayBeReferencedFromStyle(
    const nsPresContext* aPresContext, nsAtom* aName) {
  MOZ_ASSERT(aPresContext);
  return aPresContext->AnimationManager()->AnimationMayBeReferenced(aName);
}

PseudoStyleType Gecko_GetImplementedPseudo(const Element* aElement) {
  return aElement->GetPseudoElementType();
}

uint32_t Gecko_CalcStyleDifference(const ComputedStyle* aOldStyle,
                                   const ComputedStyle* aNewStyle,
                                   bool* aAnyStyleStructChanged,
                                   bool* aOnlyResetStructsChanged) {
  MOZ_ASSERT(aOldStyle);
  MOZ_ASSERT(aNewStyle);

  uint32_t equalStructs;
  nsChangeHint result =
      aOldStyle->CalcStyleDifference(*aNewStyle, &equalStructs);

  *aAnyStyleStructChanged =
      equalStructs != StyleStructConstants::kAllStructsMask;

  const auto kInheritedStructsMask =
      StyleStructConstants::kInheritedStructsMask;
  *aOnlyResetStructsChanged =
      (equalStructs & kInheritedStructsMask) == kInheritedStructsMask;

  return result;
}

const ServoElementSnapshot* Gecko_GetElementSnapshot(
    const ServoElementSnapshotTable* aTable, const Element* aElement) {
  MOZ_ASSERT(aTable);
  MOZ_ASSERT(aElement);

  return aTable->Get(const_cast<Element*>(aElement));
}

bool Gecko_HaveSeenPtr(SeenPtrs* aTable, const void* aPtr) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aTable);
  // Empty Rust allocations are indicated by small values up to the alignment
  // of the relevant type. We shouldn't see anything like that here.
  MOZ_ASSERT(uintptr_t(aPtr) > 16);

  return aTable->HaveSeenPtr(aPtr);
}

const StyleStrong<RawServoDeclarationBlock>* Gecko_GetStyleAttrDeclarationBlock(
    const Element* aElement) {
  DeclarationBlock* decl = aElement->GetInlineStyleDeclaration();
  if (!decl) {
    return nullptr;
  }
  return decl->RefRawStrong();
}

void Gecko_UnsetDirtyStyleAttr(const Element* aElement) {
  DeclarationBlock* decl = aElement->GetInlineStyleDeclaration();
  if (!decl) {
    return;
  }
  decl->UnsetDirty();
}

static const StyleStrong<RawServoDeclarationBlock>* AsRefRawStrong(
    const RefPtr<RawServoDeclarationBlock>& aDecl) {
  static_assert(sizeof(RefPtr<RawServoDeclarationBlock>) ==
                    sizeof(StyleStrong<RawServoDeclarationBlock>),
                "RefPtr should just be a pointer");
  return reinterpret_cast<const StyleStrong<RawServoDeclarationBlock>*>(&aDecl);
}

const StyleStrong<RawServoDeclarationBlock>*
Gecko_GetHTMLPresentationAttrDeclarationBlock(const Element* aElement) {
  const nsMappedAttributes* attrs = aElement->GetMappedAttributes();
  if (!attrs) {
    auto* svg = SVGElement::FromNodeOrNull(aElement);
    if (svg) {
      if (auto decl = svg->GetContentDeclarationBlock()) {
        return decl->RefRawStrong();
      }
    }
    return nullptr;
  }

  return AsRefRawStrong(attrs->GetServoStyle());
}

const StyleStrong<RawServoDeclarationBlock>*
Gecko_GetExtraContentStyleDeclarations(const Element* aElement) {
  if (!aElement->IsAnyOfHTMLElements(nsGkAtoms::td, nsGkAtoms::th)) {
    return nullptr;
  }
  const HTMLTableCellElement* cell =
      static_cast<const HTMLTableCellElement*>(aElement);
  if (nsMappedAttributes* attrs =
          cell->GetMappedAttributesInheritedFromTable()) {
    return AsRefRawStrong(attrs->GetServoStyle());
  }
  return nullptr;
}

const StyleStrong<RawServoDeclarationBlock>*
Gecko_GetUnvisitedLinkAttrDeclarationBlock(const Element* aElement) {
  nsHTMLStyleSheet* sheet = aElement->OwnerDoc()->GetAttributeStyleSheet();
  if (!sheet) {
    return nullptr;
  }

  return AsRefRawStrong(sheet->GetServoUnvisitedLinkDecl());
}

StyleSheet* Gecko_StyleSheet_Clone(const StyleSheet* aSheet,
                                   const StyleSheet* aNewParentSheet) {
  MOZ_ASSERT(aSheet);
  MOZ_ASSERT(aSheet->GetParentSheet(), "Should only be used for @import");
  MOZ_ASSERT(aNewParentSheet, "Wat");

  RefPtr<StyleSheet> newSheet =
      aSheet->Clone(nullptr, nullptr, nullptr, nullptr);

  // NOTE(emilio): This code runs in the StylesheetInner constructor, which
  // means that the inner pointer of `aNewParentSheet` still points to the old
  // one.
  //
  // So we _don't_ update neither the parent pointer of the stylesheet, nor the
  // child list (yet). This is fixed up in that same constructor.
  return static_cast<StyleSheet*>(newSheet.forget().take());
}

void Gecko_StyleSheet_AddRef(const StyleSheet* aSheet) {
  MOZ_ASSERT(NS_IsMainThread());
  const_cast<StyleSheet*>(aSheet)->AddRef();
}

void Gecko_StyleSheet_Release(const StyleSheet* aSheet) {
  MOZ_ASSERT(NS_IsMainThread());
  const_cast<StyleSheet*>(aSheet)->Release();
}

const StyleStrong<RawServoDeclarationBlock>*
Gecko_GetVisitedLinkAttrDeclarationBlock(const Element* aElement) {
  nsHTMLStyleSheet* sheet = aElement->OwnerDoc()->GetAttributeStyleSheet();
  if (!sheet) {
    return nullptr;
  }

  return AsRefRawStrong(sheet->GetServoVisitedLinkDecl());
}

const StyleStrong<RawServoDeclarationBlock>*
Gecko_GetActiveLinkAttrDeclarationBlock(const Element* aElement) {
  nsHTMLStyleSheet* sheet = aElement->OwnerDoc()->GetAttributeStyleSheet();
  if (!sheet) {
    return nullptr;
  }

  return AsRefRawStrong(sheet->GetServoActiveLinkDecl());
}

static PseudoStyleType GetPseudoTypeFromElementForAnimation(
    const Element*& aElementOrPseudo) {
  if (aElementOrPseudo->IsGeneratedContentContainerForBefore()) {
    aElementOrPseudo = aElementOrPseudo->GetParent()->AsElement();
    return PseudoStyleType::before;
  }

  if (aElementOrPseudo->IsGeneratedContentContainerForAfter()) {
    aElementOrPseudo = aElementOrPseudo->GetParent()->AsElement();
    return PseudoStyleType::after;
  }

  if (aElementOrPseudo->IsGeneratedContentContainerForMarker()) {
    aElementOrPseudo = aElementOrPseudo->GetParent()->AsElement();
    return PseudoStyleType::marker;
  }

  return PseudoStyleType::NotPseudo;
}

bool Gecko_GetAnimationRule(const Element* aElement,
                            EffectCompositor::CascadeLevel aCascadeLevel,
                            RawServoAnimationValueMap* aAnimationValues) {
  MOZ_ASSERT(aElement);

  Document* doc = aElement->GetComposedDoc();
  if (!doc) {
    return false;
  }
  nsPresContext* presContext = doc->GetPresContext();
  if (!presContext) {
    return false;
  }

  PseudoStyleType pseudoType = GetPseudoTypeFromElementForAnimation(aElement);

  return presContext->EffectCompositor()->GetServoAnimationRule(
      aElement, pseudoType, aCascadeLevel, aAnimationValues);
}

bool Gecko_StyleAnimationsEquals(const nsStyleAutoArray<StyleAnimation>* aA,
                                 const nsStyleAutoArray<StyleAnimation>* aB) {
  return *aA == *aB;
}

void Gecko_CopyAnimationNames(nsStyleAutoArray<StyleAnimation>* aDest,
                              const nsStyleAutoArray<StyleAnimation>* aSrc) {
  size_t srcLength = aSrc->Length();
  aDest->EnsureLengthAtLeast(srcLength);

  for (size_t index = 0; index < srcLength; index++) {
    (*aDest)[index].SetName((*aSrc)[index].GetName());
  }
}

void Gecko_SetAnimationName(StyleAnimation* aStyleAnimation, nsAtom* aAtom) {
  MOZ_ASSERT(aStyleAnimation);
  aStyleAnimation->SetName(already_AddRefed<nsAtom>(aAtom));
}

void Gecko_UpdateAnimations(const Element* aElement,
                            const ComputedStyle* aOldComputedData,
                            const ComputedStyle* aComputedData,
                            UpdateAnimationsTasks aTasks) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aElement);

  if (!aElement->IsInComposedDoc()) {
    return;
  }

  nsPresContext* presContext = nsContentUtils::GetContextForContent(aElement);
  if (!presContext || !presContext->IsDynamic()) {
    return;
  }

  nsAutoAnimationMutationBatch mb(aElement->OwnerDoc());

  PseudoStyleType pseudoType = GetPseudoTypeFromElementForAnimation(aElement);

  if (aTasks & UpdateAnimationsTasks::CSSAnimations) {
    presContext->AnimationManager()->UpdateAnimations(
        const_cast<Element*>(aElement), pseudoType, aComputedData);
  }

  // aComputedData might be nullptr if the target element is now in a
  // display:none subtree. We still call Gecko_UpdateAnimations in this case
  // because we need to stop CSS animations in the display:none subtree.
  // However, we don't need to update transitions since they are stopped by
  // RestyleManager::AnimationsWithDestroyedFrame so we just return early
  // here.
  if (!aComputedData) {
    return;
  }

  if (aTasks & UpdateAnimationsTasks::CSSTransitions) {
    MOZ_ASSERT(aOldComputedData);
    presContext->TransitionManager()->UpdateTransitions(
        const_cast<Element*>(aElement), pseudoType, *aOldComputedData,
        *aComputedData);
  }

  if (aTasks & UpdateAnimationsTasks::EffectProperties) {
    presContext->EffectCompositor()->UpdateEffectProperties(
        aComputedData, const_cast<Element*>(aElement), pseudoType);
  }

  if (aTasks & UpdateAnimationsTasks::CascadeResults) {
    EffectSet* effectSet = EffectSet::GetEffectSet(aElement, pseudoType);
    // CSS animations/transitions might have been destroyed as part of the above
    // steps so before updating cascade results, we check if there are still any
    // animations to update.
    if (effectSet) {
      // We call UpdateCascadeResults directly (intead of
      // MaybeUpdateCascadeResults) since we know for sure that the cascade has
      // changed, but we were unable to call MarkCascadeUpdated when we noticed
      // it since we avoid mutating state as part of the Servo parallel
      // traversal.
      presContext->EffectCompositor()->UpdateCascadeResults(
          *effectSet, const_cast<Element*>(aElement), pseudoType);
    }
  }

  if (aTasks & UpdateAnimationsTasks::DisplayChangedFromNone) {
    presContext->EffectCompositor()->RequestRestyle(
        const_cast<Element*>(aElement), pseudoType,
        EffectCompositor::RestyleType::Standard,
        EffectCompositor::CascadeLevel::Animations);
  }
}

size_t Gecko_GetAnimationEffectCount(const Element* aElementOrPseudo) {
  PseudoStyleType pseudoType =
      GetPseudoTypeFromElementForAnimation(aElementOrPseudo);

  EffectSet* effectSet = EffectSet::GetEffectSet(aElementOrPseudo, pseudoType);
  return effectSet ? effectSet->Count() : 0;
}

bool Gecko_ElementHasAnimations(const Element* aElement) {
  PseudoStyleType pseudoType = GetPseudoTypeFromElementForAnimation(aElement);

  return !!EffectSet::GetEffectSet(aElement, pseudoType);
}

bool Gecko_ElementHasCSSAnimations(const Element* aElement) {
  PseudoStyleType pseudoType = GetPseudoTypeFromElementForAnimation(aElement);
  nsAnimationManager::CSSAnimationCollection* collection =
      nsAnimationManager::CSSAnimationCollection ::GetAnimationCollection(
          aElement, pseudoType);

  return collection && !collection->mAnimations.IsEmpty();
}

bool Gecko_ElementHasCSSTransitions(const Element* aElement) {
  PseudoStyleType pseudoType = GetPseudoTypeFromElementForAnimation(aElement);
  nsTransitionManager::CSSTransitionCollection* collection =
      nsTransitionManager::CSSTransitionCollection ::GetAnimationCollection(
          aElement, pseudoType);

  return collection && !collection->mAnimations.IsEmpty();
}

size_t Gecko_ElementTransitions_Length(const Element* aElement) {
  PseudoStyleType pseudoType = GetPseudoTypeFromElementForAnimation(aElement);
  nsTransitionManager::CSSTransitionCollection* collection =
      nsTransitionManager::CSSTransitionCollection ::GetAnimationCollection(
          aElement, pseudoType);

  return collection ? collection->mAnimations.Length() : 0;
}

static CSSTransition* GetCurrentTransitionAt(const Element* aElement,
                                             size_t aIndex) {
  PseudoStyleType pseudoType = GetPseudoTypeFromElementForAnimation(aElement);
  nsTransitionManager::CSSTransitionCollection* collection =
      nsTransitionManager::CSSTransitionCollection ::GetAnimationCollection(
          aElement, pseudoType);
  if (!collection) {
    return nullptr;
  }
  nsTArray<RefPtr<CSSTransition>>& transitions = collection->mAnimations;
  return aIndex < transitions.Length() ? transitions[aIndex].get() : nullptr;
}

nsCSSPropertyID Gecko_ElementTransitions_PropertyAt(const Element* aElement,
                                                    size_t aIndex) {
  CSSTransition* transition = GetCurrentTransitionAt(aElement, aIndex);
  return transition ? transition->TransitionProperty()
                    : nsCSSPropertyID::eCSSProperty_UNKNOWN;
}

const RawServoAnimationValue* Gecko_ElementTransitions_EndValueAt(
    const Element* aElement, size_t aIndex) {
  CSSTransition* transition = GetCurrentTransitionAt(aElement, aIndex);
  return transition ? transition->ToValue().mServo.get() : nullptr;
}

double Gecko_GetProgressFromComputedTiming(const ComputedTiming* aTiming) {
  return aTiming->mProgress.Value();
}

double Gecko_GetPositionInSegment(
    const AnimationPropertySegment* aSegment, double aProgress,
    ComputedTimingFunction::BeforeFlag aBeforeFlag) {
  MOZ_ASSERT(aSegment->mFromKey < aSegment->mToKey,
             "The segment from key should be less than to key");

  double positionInSegment = (aProgress - aSegment->mFromKey) /
                             // To avoid floating precision inaccuracies, make
                             // sure we calculate both the numerator and
                             // denominator using double precision.
                             (double(aSegment->mToKey) - aSegment->mFromKey);

  return ComputedTimingFunction::GetPortion(aSegment->mTimingFunction,
                                            positionInSegment, aBeforeFlag);
}

const RawServoAnimationValue* Gecko_AnimationGetBaseStyle(
    const RawServoAnimationValueTable* aBaseStyles, nsCSSPropertyID aProperty) {
  auto base = reinterpret_cast<
      const nsRefPtrHashtable<nsUint32HashKey, RawServoAnimationValue>*>(
      aBaseStyles);
  return base->GetWeak(aProperty);
}

void Gecko_FillAllImageLayers(nsStyleImageLayers* aLayers, uint32_t aMaxLen) {
  aLayers->FillAllLayers(aMaxLen);
}

bool Gecko_IsDocumentBody(const Element* aElement) {
  Document* doc = aElement->GetUncomposedDoc();
  return doc && doc->GetBodyElement() == aElement;
}

nscolor Gecko_GetLookAndFeelSystemColor(int32_t aId, const Document* aDoc) {
  auto colorId = static_cast<LookAndFeel::ColorID>(aId);
  AutoWriteLock guard(*sServoFFILock);
  return LookAndFeel::Color(colorId, *aDoc);
}

int32_t Gecko_GetLookAndFeelInt(int32_t aId) {
  auto intId = static_cast<LookAndFeel::IntID>(aId);
  AutoWriteLock guard(*sServoFFILock);
  return LookAndFeel::GetInt(intId);
}

bool Gecko_MatchLang(const Element* aElement, nsAtom* aOverrideLang,
                     bool aHasOverrideLang, const char16_t* aValue) {
  MOZ_ASSERT(!(aOverrideLang && !aHasOverrideLang),
             "aHasOverrideLang should only be set when aOverrideLang is null");
  MOZ_ASSERT(aValue, "null lang parameter");
  if (!aValue || !*aValue) {
    return false;
  }

  // We have to determine the language of the current element.  Since
  // this is currently no property and since the language is inherited
  // from the parent we have to be prepared to look at all parent
  // nodes.  The language itself is encoded in the LANG attribute.
  if (auto* language = aHasOverrideLang ? aOverrideLang : aElement->GetLang()) {
    return nsStyleUtil::DashMatchCompare(
        nsDependentAtomString(language), nsDependentString(aValue),
        nsASCIICaseInsensitiveStringComparator);
  }

  // Try to get the language from the HTTP header or if this
  // is missing as well from the preferences.
  // The content language can be a comma-separated list of
  // language codes.
  nsAutoString language;
  aElement->OwnerDoc()->GetContentLanguage(language);

  nsDependentString langString(aValue);
  language.StripWhitespace();
  for (auto const& lang : language.Split(char16_t(','))) {
    if (nsStyleUtil::DashMatchCompare(lang, langString,
                                      nsASCIICaseInsensitiveStringComparator)) {
      return true;
    }
  }
  return false;
}

nsAtom* Gecko_GetXMLLangValue(const Element* aElement) {
  const nsAttrValue* attr =
      aElement->GetParsedAttr(nsGkAtoms::lang, kNameSpaceID_XML);

  if (!attr) {
    return nullptr;
  }

  MOZ_ASSERT(attr->Type() == nsAttrValue::eAtom);

  RefPtr<nsAtom> atom = attr->GetAtomValue();
  return atom.forget().take();
}

Document::DocumentTheme Gecko_GetDocumentLWTheme(const Document* aDocument) {
  return aDocument->ThreadSafeGetDocumentLWTheme();
}

const PreferenceSheet::Prefs* Gecko_GetPrefSheetPrefs(const Document* aDoc) {
  return &PreferenceSheet::PrefsFor(*aDoc);
}

bool Gecko_IsTableBorderNonzero(const Element* aElement) {
  if (!aElement->IsHTMLElement(nsGkAtoms::table)) {
    return false;
  }
  const nsAttrValue* val = aElement->GetParsedAttr(nsGkAtoms::border);
  return val &&
         (val->Type() != nsAttrValue::eInteger || val->GetIntegerValue() != 0);
}

bool Gecko_IsBrowserFrame(const Element* aElement) {
  nsIMozBrowserFrame* browserFrame =
      const_cast<Element*>(aElement)->GetAsMozBrowserFrame();
  return browserFrame && browserFrame->GetReallyIsBrowser();
}

bool Gecko_IsSelectListBox(const Element* aElement) {
  const auto* select = HTMLSelectElement::FromNode(aElement);
  return select && !select->IsCombobox();
}

template <typename Implementor>
static nsAtom* LangValue(Implementor* aElement) {
  // TODO(emilio): Deduplicate a bit with nsIContent::GetLang().
  const nsAttrValue* attr =
      aElement->GetParsedAttr(nsGkAtoms::lang, kNameSpaceID_XML);
  if (!attr && aElement->SupportsLangAttr()) {
    attr = aElement->GetParsedAttr(nsGkAtoms::lang);
  }

  if (!attr) {
    return nullptr;
  }

  MOZ_ASSERT(attr->Type() == nsAttrValue::eAtom);
  RefPtr<nsAtom> atom = attr->GetAtomValue();
  return atom.forget().take();
}

template <typename Implementor, typename MatchFn>
static bool DoMatch(Implementor* aElement, nsAtom* aNS, nsAtom* aName,
                    MatchFn aMatch) {
  if (MOZ_LIKELY(aNS)) {
    int32_t ns = aNS == nsGkAtoms::_empty
                     ? kNameSpaceID_None
                     : nsContentUtils::NameSpaceManager()->GetNameSpaceID(
                           aNS, aElement->IsInChromeDocument());

    MOZ_ASSERT(ns == nsContentUtils::NameSpaceManager()->GetNameSpaceID(
                         aNS, aElement->IsInChromeDocument()));
    NS_ENSURE_TRUE(ns != kNameSpaceID_Unknown, false);
    const nsAttrValue* value = aElement->GetParsedAttr(aName, ns);
    return value && aMatch(value);
  }

  // No namespace means any namespace - we have to check them all. :-(
  BorrowedAttrInfo attrInfo;
  for (uint32_t i = 0; (attrInfo = aElement->GetAttrInfoAt(i)); ++i) {
    if (attrInfo.mName->LocalName() != aName) {
      continue;
    }
    if (aMatch(attrInfo.mValue)) {
      return true;
    }
  }
  return false;
}

template <typename Implementor>
static bool HasAttr(Implementor* aElement, nsAtom* aNS, nsAtom* aName) {
  auto match = [](const nsAttrValue* aValue) { return true; };
  return DoMatch(aElement, aNS, aName, match);
}

template <typename Implementor>
static bool AttrEquals(Implementor* aElement, nsAtom* aNS, nsAtom* aName,
                       nsAtom* aStr, bool aIgnoreCase) {
  auto match = [aStr, aIgnoreCase](const nsAttrValue* aValue) {
    return aValue->Equals(aStr, aIgnoreCase ? eIgnoreCase : eCaseMatters);
  };
  return DoMatch(aElement, aNS, aName, match);
}

#define WITH_COMPARATOR(ignore_case_, c_, expr_)                  \
  auto c_ = ignore_case_ ? nsASCIICaseInsensitiveStringComparator \
                         : nsTDefaultStringComparator<char16_t>;  \
  return expr_;

template <typename Implementor>
static bool AttrDashEquals(Implementor* aElement, nsAtom* aNS, nsAtom* aName,
                           nsAtom* aStr, bool aIgnoreCase) {
  auto match = [aStr, aIgnoreCase](const nsAttrValue* aValue) {
    nsAutoString str;
    aValue->ToString(str);
    WITH_COMPARATOR(
        aIgnoreCase, c,
        nsStyleUtil::DashMatchCompare(str, nsDependentAtomString(aStr), c))
  };
  return DoMatch(aElement, aNS, aName, match);
}

template <typename Implementor>
static bool AttrIncludes(Implementor* aElement, nsAtom* aNS, nsAtom* aName,
                         nsAtom* aStr, bool aIgnoreCase) {
  auto match = [aStr, aIgnoreCase](const nsAttrValue* aValue) {
    nsAutoString str;
    aValue->ToString(str);
    WITH_COMPARATOR(
        aIgnoreCase, c,
        nsStyleUtil::ValueIncludes(str, nsDependentAtomString(aStr), c))
  };
  return DoMatch(aElement, aNS, aName, match);
}

template <typename Implementor>
static bool AttrHasSubstring(Implementor* aElement, nsAtom* aNS, nsAtom* aName,
                             nsAtom* aStr, bool aIgnoreCase) {
  auto match = [aStr, aIgnoreCase](const nsAttrValue* aValue) {
    return aValue->HasSubstring(nsDependentAtomString(aStr),
                                aIgnoreCase ? eIgnoreCase : eCaseMatters);
  };
  return DoMatch(aElement, aNS, aName, match);
}

template <typename Implementor>
static bool AttrHasPrefix(Implementor* aElement, nsAtom* aNS, nsAtom* aName,
                          nsAtom* aStr, bool aIgnoreCase) {
  auto match = [aStr, aIgnoreCase](const nsAttrValue* aValue) {
    return aValue->HasPrefix(nsDependentAtomString(aStr),
                             aIgnoreCase ? eIgnoreCase : eCaseMatters);
  };
  return DoMatch(aElement, aNS, aName, match);
}

template <typename Implementor>
static bool AttrHasSuffix(Implementor* aElement, nsAtom* aNS, nsAtom* aName,
                          nsAtom* aStr, bool aIgnoreCase) {
  auto match = [aStr, aIgnoreCase](const nsAttrValue* aValue) {
    return aValue->HasSuffix(nsDependentAtomString(aStr),
                             aIgnoreCase ? eIgnoreCase : eCaseMatters);
  };
  return DoMatch(aElement, aNS, aName, match);
}

#define SERVO_IMPL_ELEMENT_ATTR_MATCHING_FUNCTIONS(prefix_, implementor_)      \
  nsAtom* prefix_##LangValue(implementor_ aElement) {                          \
    return LangValue(aElement);                                                \
  }                                                                            \
  bool prefix_##HasAttr(implementor_ aElement, nsAtom* aNS, nsAtom* aName) {   \
    return HasAttr(aElement, aNS, aName);                                      \
  }                                                                            \
  bool prefix_##AttrEquals(implementor_ aElement, nsAtom* aNS, nsAtom* aName,  \
                           nsAtom* aStr, bool aIgnoreCase) {                   \
    return AttrEquals(aElement, aNS, aName, aStr, aIgnoreCase);                \
  }                                                                            \
  bool prefix_##AttrDashEquals(implementor_ aElement, nsAtom* aNS,             \
                               nsAtom* aName, nsAtom* aStr,                    \
                               bool aIgnoreCase) {                             \
    return AttrDashEquals(aElement, aNS, aName, aStr, aIgnoreCase);            \
  }                                                                            \
  bool prefix_##AttrIncludes(implementor_ aElement, nsAtom* aNS,               \
                             nsAtom* aName, nsAtom* aStr, bool aIgnoreCase) {  \
    return AttrIncludes(aElement, aNS, aName, aStr, aIgnoreCase);              \
  }                                                                            \
  bool prefix_##AttrHasSubstring(implementor_ aElement, nsAtom* aNS,           \
                                 nsAtom* aName, nsAtom* aStr,                  \
                                 bool aIgnoreCase) {                           \
    return AttrHasSubstring(aElement, aNS, aName, aStr, aIgnoreCase);          \
  }                                                                            \
  bool prefix_##AttrHasPrefix(implementor_ aElement, nsAtom* aNS,              \
                              nsAtom* aName, nsAtom* aStr, bool aIgnoreCase) { \
    return AttrHasPrefix(aElement, aNS, aName, aStr, aIgnoreCase);             \
  }                                                                            \
  bool prefix_##AttrHasSuffix(implementor_ aElement, nsAtom* aNS,              \
                              nsAtom* aName, nsAtom* aStr, bool aIgnoreCase) { \
    return AttrHasSuffix(aElement, aNS, aName, aStr, aIgnoreCase);             \
  }

SERVO_IMPL_ELEMENT_ATTR_MATCHING_FUNCTIONS(Gecko_, const Element*)
SERVO_IMPL_ELEMENT_ATTR_MATCHING_FUNCTIONS(Gecko_Snapshot,
                                           const ServoElementSnapshot*)

#undef SERVO_IMPL_ELEMENT_ATTR_MATCHING_FUNCTIONS

nsAtom* Gecko_Atomize(const char* aString, uint32_t aLength) {
  return NS_Atomize(nsDependentCSubstring(aString, aLength)).take();
}

nsAtom* Gecko_Atomize16(const nsAString* aString) {
  return NS_Atomize(*aString).take();
}

void Gecko_AddRefAtom(nsAtom* aAtom) { NS_ADDREF(aAtom); }

void Gecko_ReleaseAtom(nsAtom* aAtom) { NS_RELEASE(aAtom); }

void Gecko_nsTArray_FontFamilyName_AppendNamed(
    nsTArray<FontFamilyName>* aNames, nsAtom* aName,
    StyleFontFamilyNameSyntax aSyntax) {
  aNames->AppendElement(FontFamilyName(aName, aSyntax));
}

void Gecko_nsTArray_FontFamilyName_AppendGeneric(
    nsTArray<FontFamilyName>* aNames, StyleGenericFontFamily aType) {
  aNames->AppendElement(FontFamilyName(aType));
}

SharedFontList* Gecko_SharedFontList_Create() {
  RefPtr<SharedFontList> fontlist = new SharedFontList();
  return fontlist.forget().take();
}

MOZ_DEFINE_MALLOC_SIZE_OF(GeckoSharedFontListMallocSizeOf)

size_t Gecko_SharedFontList_SizeOfIncludingThisIfUnshared(
    SharedFontList* aFontlist) {
  MOZ_ASSERT(NS_IsMainThread());
  return aFontlist->SizeOfIncludingThisIfUnshared(
      GeckoSharedFontListMallocSizeOf);
}

size_t Gecko_SharedFontList_SizeOfIncludingThis(SharedFontList* aFontlist) {
  MOZ_ASSERT(NS_IsMainThread());
  return aFontlist->SizeOfIncludingThis(GeckoSharedFontListMallocSizeOf);
}

NS_IMPL_THREADSAFE_FFI_REFCOUNTING(SharedFontList, SharedFontList);

void Gecko_CopyFontFamilyFrom(nsFont* dst, const nsFont* src) {
  dst->fontlist = src->fontlist;
}

void Gecko_nsFont_InitSystem(nsFont* aDest, StyleSystemFont aFontId,
                             const nsStyleFont* aFont,
                             const Document* aDocument) {
  const nsFont* defaultVariableFont = ThreadSafeGetDefaultFontHelper(
      *aDocument, aFont->mLanguage, StyleGenericFontFamily::None);

  // We have passed uninitialized memory to this function,
  // initialize it. We can't simply return an nsFont because then
  // we need to know its size beforehand. Servo cannot initialize nsFont
  // itself, so this will do.
  new (aDest) nsFont(*defaultVariableFont);

  AutoWriteLock guard(*sServoFFILock);
  nsLayoutUtils::ComputeSystemFont(aDest, aFontId, defaultVariableFont,
                                   aDocument);
}

void Gecko_nsFont_Destroy(nsFont* aDest) { aDest->~nsFont(); }

StyleGenericFontFamily Gecko_nsStyleFont_ComputeDefaultFontType(
    const Document* aDoc, StyleGenericFontFamily aGenericId,
    nsAtom* aLanguage) {
  const nsFont* defaultFont =
      ThreadSafeGetDefaultFontHelper(*aDoc, aLanguage, aGenericId);
  return defaultFont->fontlist.GetDefaultFontType();
}

gfxFontFeatureValueSet* Gecko_ConstructFontFeatureValueSet() {
  return new gfxFontFeatureValueSet();
}

nsTArray<uint32_t>* Gecko_AppendFeatureValueHashEntry(
    gfxFontFeatureValueSet* aFontFeatureValues, nsAtom* aFamily,
    uint32_t aAlternate, nsAtom* aName) {
  MOZ_ASSERT(NS_IsMainThread());
  return aFontFeatureValues->AppendFeatureValueHashEntry(nsAtomCString(aFamily),
                                                         aName, aAlternate);
}

float Gecko_FontStretch_ToFloat(FontStretch aStretch) {
  // Servo represents percentages with 1. being 100%.
  return aStretch.Percentage() / 100.0f;
}

void Gecko_FontStretch_SetFloat(FontStretch* aStretch, float aFloat) {
  // Servo represents percentages with 1. being 100%.
  //
  // Also, the font code assumes a given maximum that style doesn't really need
  // to know about. So clamp here at the boundary.
  *aStretch = FontStretch(std::min(aFloat * 100.0f, float(FontStretch::kMax)));
}

void Gecko_FontSlantStyle_SetNormal(FontSlantStyle* aStyle) {
  *aStyle = FontSlantStyle::Normal();
}

void Gecko_FontSlantStyle_SetItalic(FontSlantStyle* aStyle) {
  *aStyle = FontSlantStyle::Italic();
}

void Gecko_FontSlantStyle_SetOblique(FontSlantStyle* aStyle,
                                     float aAngleInDegrees) {
  *aStyle = FontSlantStyle::Oblique(aAngleInDegrees);
}

void Gecko_FontSlantStyle_Get(FontSlantStyle aStyle, bool* aNormal,
                              bool* aItalic, float* aObliqueAngle) {
  *aNormal = aStyle.IsNormal();
  *aItalic = aStyle.IsItalic();
  if (aStyle.IsOblique()) {
    *aObliqueAngle = aStyle.ObliqueAngle();
  }
}

float Gecko_FontWeight_ToFloat(FontWeight aWeight) { return aWeight.ToFloat(); }

void Gecko_FontWeight_SetFloat(FontWeight* aWeight, float aFloat) {
  *aWeight = FontWeight(aFloat);
}

void Gecko_CounterStyle_ToPtr(const StyleCounterStyle* aStyle,
                              CounterStylePtr* aPtr) {
  *aPtr = CounterStylePtr::FromStyle(*aStyle);
}

void Gecko_SetCounterStyleToNone(CounterStylePtr* aPtr) {
  *aPtr = nsGkAtoms::none;
}

void Gecko_SetCounterStyleToString(CounterStylePtr* aPtr,
                                   const nsACString* aSymbol) {
  *aPtr = new AnonymousCounterStyle(NS_ConvertUTF8toUTF16(*aSymbol));
}

void Gecko_CopyCounterStyle(CounterStylePtr* aDst,
                            const CounterStylePtr* aSrc) {
  *aDst = *aSrc;
}

nsAtom* Gecko_CounterStyle_GetName(const CounterStylePtr* aPtr) {
  return aPtr->IsAtom() ? aPtr->AsAtom() : nullptr;
}

const AnonymousCounterStyle* Gecko_CounterStyle_GetAnonymous(
    const CounterStylePtr* aPtr) {
  return aPtr->AsAnonymous();
}

void Gecko_EnsureTArrayCapacity(void* aArray, size_t aCapacity,
                                size_t aElemSize) {
  auto base =
      reinterpret_cast<nsTArray_base<nsTArrayInfallibleAllocator,
                                     nsTArray_RelocateUsingMemutils>*>(aArray);

  base->EnsureCapacity<nsTArrayInfallibleAllocator>(aCapacity, aElemSize);
}

void Gecko_ClearPODTArray(void* aArray, size_t aElementSize,
                          size_t aElementAlign) {
  auto base =
      reinterpret_cast<nsTArray_base<nsTArrayInfallibleAllocator,
                                     nsTArray_RelocateUsingMemutils>*>(aArray);

  base->template ShiftData<nsTArrayInfallibleAllocator>(
      0, base->Length(), 0, aElementSize, aElementAlign);
}

void Gecko_ResizeTArrayForStrings(nsTArray<nsString>* aArray,
                                  uint32_t aLength) {
  aArray->SetLength(aLength);
}

void Gecko_ResizeAtomArray(nsTArray<RefPtr<nsAtom>>* aArray, uint32_t aLength) {
  aArray->SetLength(aLength);
}

void Gecko_EnsureImageLayersLength(nsStyleImageLayers* aLayers, size_t aLen,
                                   nsStyleImageLayers::LayerType aLayerType) {
  size_t oldLength = aLayers->mLayers.Length();

  aLayers->mLayers.EnsureLengthAtLeast(aLen);

  for (size_t i = oldLength; i < aLen; ++i) {
    aLayers->mLayers[i].Initialize(aLayerType);
  }
}

template <typename StyleType>
static void EnsureStyleAutoArrayLength(StyleType* aArray, size_t aLen) {
  size_t oldLength = aArray->Length();

  aArray->EnsureLengthAtLeast(aLen);

  for (size_t i = oldLength; i < aLen; ++i) {
    (*aArray)[i].SetInitialValues();
  }
}

void Gecko_EnsureStyleAnimationArrayLength(void* aArray, size_t aLen) {
  auto base = static_cast<nsStyleAutoArray<StyleAnimation>*>(aArray);
  EnsureStyleAutoArrayLength(base, aLen);
}

void Gecko_EnsureStyleTransitionArrayLength(void* aArray, size_t aLen) {
  auto base = reinterpret_cast<nsStyleAutoArray<StyleTransition>*>(aArray);
  EnsureStyleAutoArrayLength(base, aLen);
}

enum class KeyframeSearchDirection {
  Forwards,
  Backwards,
};

enum class KeyframeInsertPosition {
  Prepend,
  LastForOffset,
};

static Keyframe* GetOrCreateKeyframe(nsTArray<Keyframe>* aKeyframes,
                                     float aOffset,
                                     const nsTimingFunction* aTimingFunction,
                                     KeyframeSearchDirection aSearchDirection,
                                     KeyframeInsertPosition aInsertPosition) {
  MOZ_ASSERT(aKeyframes, "The keyframe array should be valid");
  MOZ_ASSERT(aTimingFunction, "The timing function should be valid");
  MOZ_ASSERT(aOffset >= 0. && aOffset <= 1.,
             "The offset should be in the range of [0.0, 1.0]");

  size_t keyframeIndex;
  switch (aSearchDirection) {
    case KeyframeSearchDirection::Forwards:
      if (nsAnimationManager::FindMatchingKeyframe(
              *aKeyframes, aOffset, *aTimingFunction, keyframeIndex)) {
        return &(*aKeyframes)[keyframeIndex];
      }
      break;
    case KeyframeSearchDirection::Backwards:
      if (nsAnimationManager::FindMatchingKeyframe(Reversed(*aKeyframes),
                                                   aOffset, *aTimingFunction,
                                                   keyframeIndex)) {
        return &(*aKeyframes)[aKeyframes->Length() - 1 - keyframeIndex];
      }
      keyframeIndex = aKeyframes->Length() - 1;
      break;
  }

  Keyframe* keyframe = aKeyframes->InsertElementAt(
      aInsertPosition == KeyframeInsertPosition::Prepend ? 0 : keyframeIndex);
  keyframe->mOffset.emplace(aOffset);
  if (!aTimingFunction->IsLinear()) {
    keyframe->mTimingFunction.emplace();
    keyframe->mTimingFunction->Init(*aTimingFunction);
  }

  return keyframe;
}

Keyframe* Gecko_GetOrCreateKeyframeAtStart(
    nsTArray<Keyframe>* aKeyframes, float aOffset,
    const nsTimingFunction* aTimingFunction) {
  MOZ_ASSERT(aKeyframes->IsEmpty() ||
                 aKeyframes->ElementAt(0).mOffset.value() >= aOffset,
             "The offset should be less than or equal to the first keyframe's "
             "offset if there are exisiting keyframes");

  return GetOrCreateKeyframe(aKeyframes, aOffset, aTimingFunction,
                             KeyframeSearchDirection::Forwards,
                             KeyframeInsertPosition::Prepend);
}

Keyframe* Gecko_GetOrCreateInitialKeyframe(
    nsTArray<Keyframe>* aKeyframes, const nsTimingFunction* aTimingFunction) {
  return GetOrCreateKeyframe(aKeyframes, 0., aTimingFunction,
                             KeyframeSearchDirection::Forwards,
                             KeyframeInsertPosition::LastForOffset);
}

Keyframe* Gecko_GetOrCreateFinalKeyframe(
    nsTArray<Keyframe>* aKeyframes, const nsTimingFunction* aTimingFunction) {
  return GetOrCreateKeyframe(aKeyframes, 1., aTimingFunction,
                             KeyframeSearchDirection::Backwards,
                             KeyframeInsertPosition::LastForOffset);
}

PropertyValuePair* Gecko_AppendPropertyValuePair(
    nsTArray<PropertyValuePair>* aProperties, nsCSSPropertyID aProperty) {
  MOZ_ASSERT(aProperties);
  MOZ_ASSERT(aProperty == eCSSPropertyExtra_variable ||
             !nsCSSProps::PropHasFlags(aProperty, CSSPropFlags::IsLogical));
  return aProperties->AppendElement(PropertyValuePair{aProperty});
}

void Gecko_GetComputedURLSpec(const StyleComputedUrl* aURL, nsCString* aOut) {
  MOZ_ASSERT(aURL);
  MOZ_ASSERT(aOut);
  if (aURL->IsLocalRef()) {
    aOut->Assign(aURL->SpecifiedSerialization());
    return;
  }
  Gecko_GetComputedImageURLSpec(aURL, aOut);
}

void Gecko_GetComputedImageURLSpec(const StyleComputedUrl* aURL,
                                   nsCString* aOut) {
  // Image URIs don't serialize local refs as local.
  if (nsIURI* uri = aURL->GetURI()) {
    nsresult rv = uri->GetSpec(*aOut);
    if (NS_SUCCEEDED(rv)) {
      return;
    }
  }

  aOut->AssignLiteral("about:invalid");
}

bool Gecko_IsSupportedImageMimeType(const uint8_t* aMimeType,
                                    const uint32_t aLen) {
  nsDependentCSubstring mime(reinterpret_cast<const char*>(aMimeType), aLen);
  return imgLoader::SupportImageWithMimeType(
      mime, AcceptedMimeTypes::IMAGES_AND_DOCUMENTS);
}

void Gecko_nsIURI_Debug(nsIURI* aURI, nsCString* aOut) {
  // TODO(emilio): Do we have more useful stuff to put here, maybe?
  if (aURI) {
    *aOut = aURI->GetSpecOrDefault();
  }
}

// XXX Implemented by hand because even though it's thread-safe, only the
// subclasses have the HasThreadSafeRefCnt bits.
void Gecko_AddRefnsIURIArbitraryThread(nsIURI* aPtr) { NS_ADDREF(aPtr); }
void Gecko_ReleasensIURIArbitraryThread(nsIURI* aPtr) { NS_RELEASE(aPtr); }

void Gecko_nsIReferrerInfo_Debug(nsIReferrerInfo* aReferrerInfo,
                                 nsCString* aOut) {
  if (aReferrerInfo) {
    if (nsCOMPtr<nsIURI> referrer = aReferrerInfo->GetComputedReferrer()) {
      *aOut = referrer->GetSpecOrDefault();
    }
  }
}

template <typename ElementLike>
void DebugListAttributes(const ElementLike& aElement, nsCString& aOut) {
  const uint32_t kMaxAttributeLength = 40;

  uint32_t i = 0;
  while (BorrowedAttrInfo info = aElement.GetAttrInfoAt(i++)) {
    aOut.AppendLiteral(" ");
    if (nsAtom* prefix = info.mName->GetPrefix()) {
      aOut.Append(NS_ConvertUTF16toUTF8(nsDependentAtomString(prefix)));
      aOut.AppendLiteral(":");
    }
    aOut.Append(
        NS_ConvertUTF16toUTF8(nsDependentAtomString(info.mName->LocalName())));
    if (!info.mValue) {
      continue;
    }
    aOut.AppendLiteral("=\"");
    nsAutoString value;
    info.mValue->ToString(value);
    if (value.Length() > kMaxAttributeLength) {
      value.Truncate(kMaxAttributeLength - 3);
      value.AppendLiteral("...");
    }
    aOut.Append(NS_ConvertUTF16toUTF8(value));
    aOut.AppendLiteral("\"");
  }
}

void Gecko_Element_DebugListAttributes(const Element* aElement,
                                       nsCString* aOut) {
  DebugListAttributes(*aElement, *aOut);
}

void Gecko_Snapshot_DebugListAttributes(const ServoElementSnapshot* aSnapshot,
                                        nsCString* aOut) {
  DebugListAttributes(*aSnapshot, *aOut);
}

NS_IMPL_THREADSAFE_FFI_REFCOUNTING(URLExtraData, URLExtraData);

void Gecko_nsStyleFont_SetLang(nsStyleFont* aFont, nsAtom* aAtom) {
  aFont->mLanguage = dont_AddRef(aAtom);
  aFont->mExplicitLanguage = true;
}

void Gecko_nsStyleFont_CopyLangFrom(nsStyleFont* aFont,
                                    const nsStyleFont* aSource) {
  aFont->mLanguage = aSource->mLanguage;
}

void Gecko_nsStyleFont_PrioritizeUserFonts(
    nsStyleFont* aFont, StyleGenericFontFamily aDefaultGeneric) {
  MOZ_ASSERT(!StaticPrefs::browser_display_use_document_fonts());
  MOZ_ASSERT(aDefaultGeneric != StyleGenericFontFamily::None);
  if (!aFont->mFont.fontlist.PrioritizeFirstGeneric()) {
    aFont->mFont.fontlist.PrependGeneric(aDefaultGeneric);
  }
}

Length Gecko_nsStyleFont_ComputeMinSize(const nsStyleFont* aFont,
                                        const Document* aDocument) {
  // Don't change font-size:0, since that would un-hide hidden text,
  // or SVG text, or chrome docs, we assume those know what they do.
  if (aFont->mSize.IsZero() || !aFont->mAllowZoomAndMinSize ||
      nsContentUtils::IsChromeDoc(aDocument)) {
    return {0};
  }

  Length minFontSize;
  bool needsCache = false;

  auto MinFontSize = [&](bool* aNeedsToCache) {
    auto* prefs =
        aDocument->GetFontPrefsForLang(aFont->mLanguage, aNeedsToCache);
    return prefs ? prefs->mMinimumFontSize : Length{0};
  };

  {
    AutoReadLock guard(*sServoFFILock);
    minFontSize = MinFontSize(&needsCache);
  }

  if (needsCache) {
    AutoWriteLock guard(*sServoFFILock);
    minFontSize = MinFontSize(nullptr);
  }

  if (minFontSize.ToCSSPixels() <= 0.0f) {
    return {0};
  }

  minFontSize.ScaleBy(aFont->mMinFontSizeRatio);
  minFontSize.ScaleBy(1.0f / 100.0f);
  return minFontSize;
}

StyleDefaultFontSizes Gecko_GetBaseSize(nsAtom* aLanguage) {
  LangGroupFontPrefs prefs;
  nsStaticAtom* langGroupAtom =
      StaticPresData::Get()->GetUncachedLangGroup(aLanguage);
  prefs.Initialize(langGroupAtom);
  return {prefs.mDefaultVariableFont.size,  prefs.mDefaultSerifFont.size,
          prefs.mDefaultSansSerifFont.size, prefs.mDefaultMonospaceFont.size,
          prefs.mDefaultCursiveFont.size,   prefs.mDefaultFantasyFont.size};
}

static StaticRefPtr<UACacheReporter> gUACacheReporter;

namespace mozilla {

void InitializeServo() {
  URLExtraData::Init();
  Servo_Initialize(URLExtraData::Dummy(), URLExtraData::DummyChrome());

  gUACacheReporter = new UACacheReporter();
  RegisterWeakMemoryReporter(gUACacheReporter);

  sServoFFILock = new RWLock("Servo::FFILock");
}

void ShutdownServo() {
  MOZ_ASSERT(sServoFFILock);

  UnregisterWeakMemoryReporter(gUACacheReporter);
  gUACacheReporter = nullptr;

  delete sServoFFILock;
  sServoFFILock = nullptr;
  Servo_Shutdown();

  URLExtraData::Shutdown();
}

void AssertIsMainThreadOrServoFontMetricsLocked() {
  if (!NS_IsMainThread()) {
    MOZ_ASSERT(sServoFFILock &&
               sServoFFILock->LockedForWritingByCurrentThread());
  }
}

}  // namespace mozilla

GeckoFontMetrics Gecko_GetFontMetrics(const nsPresContext* aPresContext,
                                      bool aIsVertical,
                                      const nsStyleFont* aFont,
                                      Length aFontSize, bool aUseUserFontSet) {
  AutoWriteLock guard(*sServoFFILock);

  // Getting font metrics can require some main thread only work to be
  // done, such as work that needs to touch non-threadsafe refcounted
  // objects (like the DOM FontFace/FontFaceSet objects), network loads, etc.
  //
  // To handle this work, font code checks whether we are in a Servo traversal
  // and if so, appends PostTraversalTasks to the current ServoStyleSet
  // to be performed immediately after the traversal is finished.  This
  // works well for starting downloadable font loads, since we don't have
  // those fonts available to get metrics for anyway.  Platform fonts and
  // ArrayBuffer-backed FontFace objects are handled synchronously.

  nsPresContext* presContext = const_cast<nsPresContext*>(aPresContext);
  presContext->SetUsesExChUnits(true);

  RefPtr<nsFontMetrics> fm = nsLayoutUtils::GetMetricsFor(
      presContext, aIsVertical, aFont, aFontSize, aUseUserFontSet);
  const auto& metrics =
      fm->GetThebesFontGroup()->GetFirstValidFont()->GetMetrics(
          fm->Orientation());

  int32_t d2a = aPresContext->AppUnitsPerDevPixel();
  auto ToLength = [](nscoord aLen) {
    return Length::FromPixels(CSSPixel::FromAppUnits(aLen));
  };
  return {ToLength(NS_round(metrics.xHeight * d2a)),
          ToLength(NS_round(metrics.zeroWidth * d2a))};
}

NS_IMPL_THREADSAFE_FFI_REFCOUNTING(SheetLoadDataHolder, SheetLoadDataHolder);

void Gecko_StyleSheet_FinishAsyncParse(
    SheetLoadDataHolder* aData,
    StyleStrong<RawServoStyleSheetContents> aSheetContents,
    StyleOwnedOrNull<StyleUseCounters> aUseCounters) {
  UniquePtr<StyleUseCounters> useCounters = aUseCounters.Consume();
  RefPtr<SheetLoadDataHolder> loadData = aData;
  RefPtr<RawServoStyleSheetContents> sheetContents = aSheetContents.Consume();
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__, [d = std::move(loadData), contents = std::move(sheetContents),
                 counters = std::move(useCounters)]() mutable {
        MOZ_ASSERT(NS_IsMainThread());
        SheetLoadData* data = d->get();
        data->mUseCounters = std::move(counters);
        data->mSheet->FinishAsyncParse(contents.forget());
      }));
}

static already_AddRefed<StyleSheet> LoadImportSheet(
    Loader* aLoader, StyleSheet* aParent, SheetLoadData* aParentLoadData,
    LoaderReusableStyleSheets* aReusableSheets, const StyleCssUrl& aURL,
    already_AddRefed<RawServoMediaList> aMediaList) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aLoader, "Should've catched this before");
  MOZ_ASSERT(aParent, "Only used for @import, so parent should exist!");

  RefPtr<MediaList> media = new MediaList(std::move(aMediaList));
  nsCOMPtr<nsIURI> uri = aURL.GetURI();
  nsresult rv = uri ? NS_OK : NS_ERROR_FAILURE;

  size_t previousSheetCount = aParent->ChildSheets().Length();
  if (NS_SUCCEEDED(rv)) {
    // TODO(emilio): We should probably make LoadChildSheet return the
    // stylesheet rather than the return code.
    rv = aLoader->LoadChildSheet(*aParent, aParentLoadData, uri, media,
                                 aReusableSheets);
  }

  if (NS_FAILED(rv) || previousSheetCount == aParent->ChildSheets().Length()) {
    // Servo and Gecko have different ideas of what a valid URL is, so we might
    // get in here with a URL string that NS_NewURI can't handle.  We may also
    // reach here via an import cycle.  For the import cycle case, we need some
    // sheet object per spec, even if its empty.  DevTools uses the URI to
    // realize it has hit an import cycle, so we mark it complete to make the
    // sheet readable from JS.
    RefPtr<StyleSheet> emptySheet =
        aParent->CreateEmptyChildSheet(media.forget());
    // Make a dummy URI if we don't have one because some methods assume
    // non-null URIs.
    if (!uri) {
      NS_NewURI(getter_AddRefs(uri), "about:invalid"_ns);
    }
    emptySheet->SetURIs(uri, uri, uri);
    emptySheet->SetPrincipal(aURL.ExtraData().Principal());
    nsCOMPtr<nsIReferrerInfo> referrerInfo =
        ReferrerInfo::CreateForExternalCSSResources(emptySheet);
    emptySheet->SetReferrerInfo(referrerInfo);
    emptySheet->SetComplete();
    aParent->AppendStyleSheet(*emptySheet);
    return emptySheet.forget();
  }

  RefPtr<StyleSheet> sheet = aParent->ChildSheets().LastElement();
  return sheet.forget();
}

StyleSheet* Gecko_LoadStyleSheet(Loader* aLoader, StyleSheet* aParent,
                                 SheetLoadData* aParentLoadData,
                                 LoaderReusableStyleSheets* aReusableSheets,
                                 const StyleCssUrl* aUrl,
                                 StyleStrong<RawServoMediaList> aMediaList) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aUrl);

  return LoadImportSheet(aLoader, aParent, aParentLoadData, aReusableSheets,
                         *aUrl, aMediaList.Consume())
      .take();
}

void Gecko_LoadStyleSheetAsync(SheetLoadDataHolder* aParentData,
                               const StyleCssUrl* aUrl,
                               StyleStrong<RawServoMediaList> aMediaList,
                               StyleStrong<RawServoImportRule> aImportRule) {
  MOZ_ASSERT(aUrl);
  RefPtr<SheetLoadDataHolder> loadData = aParentData;
  RefPtr<RawServoMediaList> mediaList = aMediaList.Consume();
  RefPtr<RawServoImportRule> importRule = aImportRule.Consume();
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__,
      [data = std::move(loadData), url = StyleCssUrl(*aUrl),
       media = std::move(mediaList), import = std::move(importRule)]() mutable {
        MOZ_ASSERT(NS_IsMainThread());
        SheetLoadData* d = data->get();
        RefPtr<StyleSheet> sheet = LoadImportSheet(
            d->mLoader, d->mSheet, d, nullptr, url, media.forget());
        Servo_ImportRule_SetSheet(import, sheet);
      }));
}

void Gecko_AddPropertyToSet(nsCSSPropertyIDSet* aPropertySet,
                            nsCSSPropertyID aProperty) {
  aPropertySet->AddProperty(aProperty);
}

#define STYLE_STRUCT(name)                                             \
                                                                       \
  void Gecko_Construct_Default_nsStyle##name(nsStyle##name* ptr,       \
                                             const Document* doc) {    \
    new (ptr) nsStyle##name(*doc);                                     \
  }                                                                    \
                                                                       \
  void Gecko_CopyConstruct_nsStyle##name(nsStyle##name* ptr,           \
                                         const nsStyle##name* other) { \
    new (ptr) nsStyle##name(*other);                                   \
  }                                                                    \
                                                                       \
  void Gecko_Destroy_nsStyle##name(nsStyle##name* ptr) {               \
    ptr->~nsStyle##name();                                             \
  }

void Gecko_RegisterProfilerThread(const char* name) {
  PROFILER_REGISTER_THREAD(name);
}

void Gecko_UnregisterProfilerThread() { PROFILER_UNREGISTER_THREAD(); }

#ifdef MOZ_GECKO_PROFILER
void Gecko_Construct_AutoProfilerLabel(AutoProfilerLabel* aAutoLabel,
                                       JS::ProfilingCategoryPair aCatPair) {
  new (aAutoLabel) AutoProfilerLabel(
      "", nullptr, aCatPair,
      uint32_t(
          js::ProfilingStackFrame::Flags::LABEL_DETERMINED_BY_CATEGORY_PAIR));
}

void Gecko_Destroy_AutoProfilerLabel(AutoProfilerLabel* aAutoLabel) {
  aAutoLabel->~AutoProfilerLabel();
}
#endif

bool Gecko_DocumentRule_UseForPresentation(
    const Document* aDocument, const nsACString* aPattern,
    DocumentMatchingFunction aMatchingFunction) {
  MOZ_ASSERT(NS_IsMainThread());

  nsIURI* docURI = aDocument->GetDocumentURI();
  nsAutoCString docURISpec;
  if (docURI) {
    // If GetSpec fails (due to OOM) just skip these URI-specific CSS rules.
    nsresult rv = docURI->GetSpec(docURISpec);
    NS_ENSURE_SUCCESS(rv, false);
  }

  return CSSMozDocumentRule::Match(aDocument, docURI, docURISpec, *aPattern,
                                   aMatchingFunction);
}

void Gecko_SetJemallocThreadLocalArena(bool enabled) {
#if defined(MOZ_MEMORY)
  jemalloc_thread_local_arena(enabled);
#endif
}

#include "nsStyleStructList.h"

#undef STYLE_STRUCT

bool Gecko_ErrorReportingEnabled(const StyleSheet* aSheet,
                                 const Loader* aLoader,
                                 uint64_t* aOutWindowId) {
  if (!ErrorReporter::ShouldReportErrors(aSheet, aLoader)) {
    return false;
  }
  *aOutWindowId = ErrorReporter::FindInnerWindowId(aSheet, aLoader);
  return true;
}

void Gecko_ReportUnexpectedCSSError(
    const uint64_t aWindowId, nsIURI* aURI, const char* message,
    const char* param, uint32_t paramLen, const char* prefix,
    const char* prefixParam, uint32_t prefixParamLen, const char* suffix,
    const char* source, uint32_t sourceLen, const char* selectors,
    uint32_t selectorsLen, uint32_t lineNumber, uint32_t colNumber) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  ErrorReporter reporter(aWindowId);

  if (prefix) {
    if (prefixParam) {
      nsDependentCSubstring paramValue(prefixParam, prefixParamLen);
      AutoTArray<nsString, 1> wideParam;
      CopyUTF8toUTF16(paramValue, *wideParam.AppendElement());
      reporter.ReportUnexpectedUnescaped(prefix, wideParam);
    } else {
      reporter.ReportUnexpected(prefix);
    }
  }

  if (param) {
    nsDependentCSubstring paramValue(param, paramLen);
    AutoTArray<nsString, 1> wideParam;
    CopyUTF8toUTF16(paramValue, *wideParam.AppendElement());
    reporter.ReportUnexpectedUnescaped(message, wideParam);
  } else {
    reporter.ReportUnexpected(message);
  }

  if (suffix) {
    reporter.ReportUnexpected(suffix);
  }
  nsDependentCSubstring sourceValue(source, sourceLen);
  nsDependentCSubstring selectorsValue(selectors, selectorsLen);
  reporter.OutputError(sourceValue, selectorsValue, lineNumber, colNumber,
                       aURI);
}

void Gecko_ContentList_AppendAll(nsSimpleContentList* aList,
                                 const Element** aElements, size_t aLength) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aElements);
  MOZ_ASSERT(aLength);
  MOZ_ASSERT(aList);

  aList->SetCapacity(aLength);

  for (size_t i = 0; i < aLength; ++i) {
    aList->AppendElement(const_cast<Element*>(aElements[i]));
  }
}

const nsTArray<Element*>* Gecko_Document_GetElementsWithId(const Document* aDoc,
                                                           nsAtom* aId) {
  MOZ_ASSERT(aDoc);
  MOZ_ASSERT(aId);

  return aDoc->GetAllElementsForId(nsDependentAtomString(aId));
}

const nsTArray<Element*>* Gecko_ShadowRoot_GetElementsWithId(
    const ShadowRoot* aShadowRoot, nsAtom* aId) {
  MOZ_ASSERT(aShadowRoot);
  MOZ_ASSERT(aId);

  return aShadowRoot->GetAllElementsForId(nsDependentAtomString(aId));
}

bool Gecko_GetBoolPrefValue(const char* aPrefName) {
  MOZ_ASSERT(NS_IsMainThread());
  return Preferences::GetBool(aPrefName);
}

bool Gecko_IsInServoTraversal() { return ServoStyleSet::IsInServoTraversal(); }

bool Gecko_IsMainThread() { return NS_IsMainThread(); }

const nsAttrValue* Gecko_GetSVGAnimatedClass(const Element* aElement) {
  MOZ_ASSERT(aElement->IsSVGElement());
  return static_cast<const SVGElement*>(aElement)->GetAnimatedClassName();
}

bool Gecko_AssertClassAttrValueIsSane(const nsAttrValue* aValue) {
  MOZ_ASSERT(aValue->Type() == nsAttrValue::eAtom ||
             aValue->Type() == nsAttrValue::eString ||
             aValue->Type() == nsAttrValue::eAtomArray);
  MOZ_ASSERT_IF(
      aValue->Type() == nsAttrValue::eString,
      nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(
          aValue->GetStringValue())
          .IsEmpty());
  return true;
}

void Gecko_GetSafeAreaInsets(const nsPresContext* aPresContext, float* aTop,
                             float* aRight, float* aBottom, float* aLeft) {
  MOZ_ASSERT(aPresContext);
  ScreenIntMargin safeAreaInsets = aPresContext->GetSafeAreaInsets();
  *aTop = aPresContext->DevPixelsToFloatCSSPixels(safeAreaInsets.top);
  *aRight = aPresContext->DevPixelsToFloatCSSPixels(safeAreaInsets.right);
  *aBottom = aPresContext->DevPixelsToFloatCSSPixels(safeAreaInsets.bottom);
  *aLeft = aPresContext->DevPixelsToFloatCSSPixels(safeAreaInsets.left);
}

void Gecko_PrintfStderr(const nsCString* aStr) {
  printf_stderr("%s", aStr->get());
}

nsAtom* Gecko_Element_ImportedPart(const nsAttrValue* aValue,
                                   nsAtom* aPartName) {
  if (aValue->Type() != nsAttrValue::eShadowParts) {
    return nullptr;
  }
  return aValue->GetShadowPartsValue().GetReverse(aPartName);
}

nsAtom** Gecko_Element_ExportedParts(const nsAttrValue* aValue,
                                     nsAtom* aPartName, size_t* aOutLength) {
  if (aValue->Type() != nsAttrValue::eShadowParts) {
    return nullptr;
  }
  auto* parts = aValue->GetShadowPartsValue().Get(aPartName);
  if (!parts) {
    return nullptr;
  }
  *aOutLength = parts->Length();
  static_assert(sizeof(RefPtr<nsAtom>) == sizeof(nsAtom*));
  static_assert(alignof(RefPtr<nsAtom>) == alignof(nsAtom*));
  return reinterpret_cast<nsAtom**>(parts->Elements());
}

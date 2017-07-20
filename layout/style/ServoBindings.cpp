/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoBindings.h"

#include "ChildIterator.h"
#include "ErrorReporter.h"
#include "GeckoProfiler.h"
#include "gfxFontFamilyList.h"
#include "nsAnimationManager.h"
#include "nsAttrValueInlines.h"
#include "nsCSSCounterStyleRule.h"
#include "nsCSSFontFaceRule.h"
#include "nsCSSFrameConstructor.h"
#include "nsCSSProps.h"
#include "nsCSSParser.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSRuleProcessor.h"
#include "nsCSSRules.h"
#include "nsContentUtils.h"
#include "nsDOMTokenList.h"
#include "nsDeviceContext.h"
#include "nsIContentInlines.h"
#include "nsIDOMNode.h"
#include "nsIDocumentInlines.h"
#include "nsIFrame.h"
#include "nsINode.h"
#include "nsIPresShell.h"
#include "nsIPresShellInlines.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsFontMetrics.h"
#include "nsHTMLStyleSheet.h"
#include "nsMappedAttributes.h"
#include "nsMediaFeatures.h"
#include "nsNameSpaceManager.h"
#include "nsNetUtil.h"
#include "nsRuleNode.h"
#include "nsString.h"
#include "nsStyleStruct.h"
#include "nsStyleUtil.h"
#include "nsSVGElement.h"
#include "nsTArray.h"
#include "nsTransitionManager.h"

#include "mozilla/DeclarationBlockInlines.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EffectSet.h"
#include "mozilla/EventStates.h"
#include "mozilla/GeckoStyleContext.h"
#include "mozilla/Keyframe.h"
#include "mozilla/Mutex.h"
#include "mozilla/Preferences.h"
#include "mozilla/ServoElementSnapshot.h"
#include "mozilla/ServoRestyleManager.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/ServoMediaList.h"
#include "mozilla/RWLock.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/dom/HTMLTableCellElement.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/URLExtraData.h"

#if defined(MOZ_MEMORY)
# include "mozmemory.h"
#endif

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;

#define SERVO_ARC_TYPE(name_, type_) \
  already_AddRefed<type_>            \
  type_##Strong::Consume() {         \
    RefPtr<type_> result;            \
    result.swap(mPtr);               \
    return result.forget();          \
  }
#include "mozilla/ServoArcTypeList.h"
SERVO_ARC_TYPE(StyleContext, ServoStyleContext)
#undef SERVO_ARC_TYPE

static Mutex* sServoFontMetricsLock = nullptr;
static RWLock* sServoLangFontPrefsLock = nullptr;

static bool sFramesTimingFunctionEnabled;

static
const nsFont*
ThreadSafeGetDefaultFontHelper(const nsPresContext* aPresContext,
                               nsIAtom* aLanguage, uint8_t aGenericId)
{
  bool needsCache = false;
  const nsFont* retval;

  {
    AutoReadLock guard(*sServoLangFontPrefsLock);
    retval = aPresContext->GetDefaultFont(aGenericId, aLanguage, &needsCache);
  }
  if (!needsCache) {
    return retval;
  }
  {
    AutoWriteLock guard(*sServoLangFontPrefsLock);
  retval = aPresContext->GetDefaultFont(aGenericId, aLanguage, nullptr);
  }
  return retval;
}

void
AssertIsMainThreadOrServoLangFontPrefsCacheLocked()
{
  MOZ_ASSERT(NS_IsMainThread() || sServoLangFontPrefsLock->LockedForWritingByCurrentThread());
}

uint32_t
Gecko_ChildrenCount(RawGeckoNodeBorrowed aNode)
{
  return aNode->GetChildCount();
}

bool
Gecko_NodeIsElement(RawGeckoNodeBorrowed aNode)
{
  return aNode->IsElement();
}

bool
Gecko_IsInDocument(RawGeckoNodeBorrowed aNode)
{
  return aNode->IsInComposedDoc();
}

#ifdef MOZ_DEBUG_RUST
bool
Gecko_FlattenedTreeParentIsParent(RawGeckoNodeBorrowed aNode)
{
  // Servo calls this in debug builds to verify the result of its own
  // flattened_tree_parent_is_parent() function.
  return FlattenedTreeParentIsParent<nsIContent::eForStyle>(aNode);
}
#endif

/*
 * Does this child count as significant for selector matching?
 *
 * See nsStyleUtil::IsSignificantChild for details.
 */
bool
Gecko_IsSignificantChild(RawGeckoNodeBorrowed aNode, bool aTextIsSignificant,
                         bool aWhitespaceIsSignificant)
{
  return nsStyleUtil::ThreadSafeIsSignificantChild(aNode->AsContent(),
                                                   aTextIsSignificant,
                                                   aWhitespaceIsSignificant);
}

RawGeckoNodeBorrowedOrNull
Gecko_GetLastChild(RawGeckoNodeBorrowed aNode)
{
  return aNode->GetLastChild();
}

RawGeckoNodeBorrowedOrNull
Gecko_GetFlattenedTreeParentNode(RawGeckoNodeBorrowed aNode)
{
  MOZ_ASSERT(!FlattenedTreeParentIsParent<nsIContent::eForStyle>(aNode),
             "Should have taken the inline path");
  MOZ_ASSERT(aNode->IsContent(), "Slow path only applies to content");
  const nsIContent* c = aNode->AsContent();
  return c->GetFlattenedTreeParentNodeInternal(nsIContent::eForStyle);
}

RawGeckoElementBorrowedOrNull
Gecko_GetBeforeOrAfterPseudo(RawGeckoElementBorrowed aElement, bool aIsBefore)
{
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aElement->HasProperties());

  return aIsBefore
    ? nsLayoutUtils::GetBeforePseudo(aElement)
    : nsLayoutUtils::GetAfterPseudo(aElement);
}

nsTArray<nsIContent*>*
Gecko_GetAnonymousContentForElement(RawGeckoElementBorrowed aElement)
{
  nsIAnonymousContentCreator* ac = do_QueryFrame(aElement->GetPrimaryFrame());
  if (!ac) {
    return nullptr;
  }

  auto* array = new nsTArray<nsIContent*>();
  nsContentUtils::AppendNativeAnonymousChildren(aElement, *array, 0);
  return array;
}

void
Gecko_DestroyAnonymousContentList(nsTArray<nsIContent*>* aAnonContent)
{
  MOZ_ASSERT(aAnonContent);
  delete aAnonContent;
}

void
Gecko_ServoStyleContext_Init(ServoStyleContext* aContext,
                             const ServoStyleContext* aParentContext,
                             RawGeckoPresContextBorrowed aPresContext, const ServoComputedValues* aValues,
                             mozilla::CSSPseudoElementType aPseudoType, nsIAtom* aPseudoTag)
{
  // because it is within an Arc it is unsafe for the Rust side to ever
  // carry around a mutable non opaque reference to the context, so we
  // cast it here.
  ServoStyleContext* parent = const_cast<ServoStyleContext*>(aParentContext);
  nsPresContext* pres = const_cast<nsPresContext*>(aPresContext);
  new (KnownNotNull, aContext) ServoStyleContext(parent, pres, aPseudoTag,
                                                 aPseudoType, ServoComputedValuesForgotten(aValues));
}

ServoComputedValues::ServoComputedValues(const ServoComputedValuesForgotten aValue) {
  PodAssign(this, aValue.mPtr);
}

const nsStyleVariables* ServoComputedValues::GetStyleVariables() const
{
  return Servo_GetEmptyVariables();
}

void
Gecko_ServoStyleContext_Destroy(ServoStyleContext* aContext)
{
  aContext->~ServoStyleContext();
}

void
Gecko_ConstructStyleChildrenIterator(
  RawGeckoElementBorrowed aElement,
  RawGeckoStyleChildrenIteratorBorrowedMut aIterator)
{
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aIterator);
  new (aIterator) StyleChildrenIterator(aElement);
}

void
Gecko_DestroyStyleChildrenIterator(
  RawGeckoStyleChildrenIteratorBorrowedMut aIterator)
{
  MOZ_ASSERT(aIterator);

  aIterator->~StyleChildrenIterator();
}

nsIContent*
Gecko_ElementBindingAnonymousContent(RawGeckoElementBorrowed aElement)
{
  MOZ_ASSERT(aElement->HasFlag(NODE_MAY_BE_IN_BINDING_MNGR));
  nsBindingManager* manager = aElement->OwnerDoc()->BindingManager();
  if (nsXBLBinding* binding = manager->GetBindingWithContent(aElement)) {
    return binding->GetAnonymousContent();
  }
  return nullptr;
}

RawGeckoNodeBorrowed
Gecko_GetNextStyleChild(RawGeckoStyleChildrenIteratorBorrowedMut aIterator)
{
  MOZ_ASSERT(aIterator);
  return aIterator->GetNextChild();
}

EventStates::ServoType
Gecko_ElementState(RawGeckoElementBorrowed aElement)
{
  return aElement->StyleState().ServoValue();
}

EventStates::ServoType
Gecko_DocumentState(const nsIDocument* aDocument)
{
  return aDocument->ThreadSafeGetDocumentState().ServoValue();
}

bool
Gecko_IsTextNode(RawGeckoNodeBorrowed aNode)
{
  return aNode->NodeInfo()->NodeType() == nsIDOMNode::TEXT_NODE;
}

bool
Gecko_IsRootElement(RawGeckoElementBorrowed aElement)
{
  return aElement->OwnerDoc()->GetRootElement() == aElement;
}

bool
Gecko_MatchesElement(CSSPseudoClassType aType,
                     RawGeckoElementBorrowed aElement)
{
  return nsCSSPseudoClasses::MatchesElement(aType, aElement).value();
}

nsIAtom*
Gecko_LocalName(RawGeckoElementBorrowed aElement)
{
  return aElement->NodeInfo()->NameAtom();
}

nsIAtom*
Gecko_Namespace(RawGeckoElementBorrowed aElement)
{
  int32_t id = aElement->NodeInfo()->NamespaceID();
  return nsContentUtils::NameSpaceManager()->NameSpaceURIAtomForServo(id);
}

nsIAtom*
Gecko_GetElementId(RawGeckoElementBorrowed aElement)
{
  const nsAttrValue* attr = aElement->GetParsedAttr(nsGkAtoms::id);
  if (attr && attr->Type() == nsAttrValue::eAtom) {
    return attr->GetAtomValue();
  }
  // The only case in which we should have a non-atom value here is if it's the
  // empty string value.
  MOZ_ASSERT(!attr || attr->IsEmptyString());
  return nullptr;
}

// Dirtiness tracking.
uint32_t
Gecko_GetNodeFlags(RawGeckoNodeBorrowed aNode)
{
  return aNode->GetFlags();
}

void
Gecko_SetNodeFlags(RawGeckoNodeBorrowed aNode, uint32_t aFlags)
{
  const_cast<nsINode*>(aNode)->SetFlags(aFlags);
}

void
Gecko_UnsetNodeFlags(RawGeckoNodeBorrowed aNode, uint32_t aFlags)
{
  const_cast<nsINode*>(aNode)->UnsetFlags(aFlags);
}

void
Gecko_SetOwnerDocumentNeedsStyleFlush(RawGeckoElementBorrowed aElement)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (nsIPresShell* shell = aElement->OwnerDoc()->GetShell()) {
    shell->EnsureStyleFlush();
  }
}

nsStyleContext*
Gecko_GetStyleContext(RawGeckoElementBorrowed aElement,
                      nsIAtom* aPseudoTagOrNull)
{
  nsIFrame* relevantFrame =
    ServoRestyleManager::FrameForPseudoElement(aElement, aPseudoTagOrNull);
  if (relevantFrame) {
    return relevantFrame->StyleContext();
  }

  if (aPseudoTagOrNull) {
    return nullptr;
  }

  // FIXME(emilio): Is there a shorter path?
  nsIPresShell* shell = aElement->OwnerDoc()->GetShell();
  NS_ENSURE_TRUE(shell, nullptr);
  nsCSSFrameConstructor* fc = shell->GetPresContext()->FrameConstructor();

  // NB: This is only called for CalcStyleDifference, and we handle correctly
  // the display: none case since Servo still has the older style.
  return fc->GetDisplayContentsStyleFor(aElement);
}

CSSPseudoElementType
Gecko_GetImplementedPseudo(RawGeckoElementBorrowed aElement)
{
  return aElement->GetPseudoElementType();
}

nsChangeHint
Gecko_CalcStyleDifference(nsStyleContext* aOldStyleContext,
                          ServoComputedValuesBorrowed aComputedValues,
                          bool* aAnyStyleChanged)
{
  MOZ_ASSERT(aOldStyleContext);
  MOZ_ASSERT(aComputedValues);

  uint32_t equalStructs, samePointerStructs;
  nsChangeHint result =
    aOldStyleContext->CalcStyleDifference(aComputedValues,
                                          &equalStructs,
                                          &samePointerStructs);
  *aAnyStyleChanged = equalStructs != NS_STYLE_INHERIT_MASK;
  return result;
}

nsChangeHint
Gecko_HintsHandledForDescendants(nsChangeHint aHint)
{
  return aHint & ~NS_HintsNotHandledForDescendantsIn(aHint);
}

const ServoElementSnapshot*
Gecko_GetElementSnapshot(const ServoElementSnapshotTable* aTable,
                         const Element* aElement)
{
  MOZ_ASSERT(aTable);
  MOZ_ASSERT(aElement);

  return aTable->Get(const_cast<Element*>(aElement));
}

RawServoDeclarationBlockStrongBorrowedOrNull
Gecko_GetStyleAttrDeclarationBlock(RawGeckoElementBorrowed aElement)
{
  DeclarationBlock* decl = aElement->GetInlineStyleDeclaration();
  if (!decl) {
    return nullptr;
  }
  if (decl->IsGecko()) {
    // XXX This can happen when nodes are adopted from a Gecko-style-backend
    //     document into a Servo-style-backend document.  See bug 1330051.
    NS_WARNING("stylo: requesting a Gecko declaration block?");
    return nullptr;
  }
  return decl->AsServo()->RefRawStrong();
}

void
Gecko_UnsetDirtyStyleAttr(RawGeckoElementBorrowed aElement)
{
  DeclarationBlock* decl = aElement->GetInlineStyleDeclaration();
  if (!decl) {
    return;
  }
  if (decl->IsGecko()) {
    // XXX This can happen when nodes are adopted from a Gecko-style-backend
    //     document into a Servo-style-backend document.  See bug 1330051.
    NS_WARNING("stylo: requesting a Gecko declaration block?");
    return;
  }
  decl->UnsetDirty();
}

RawServoDeclarationBlockStrongBorrowedOrNull
Gecko_GetSMILOverrideDeclarationBlock(RawGeckoElementBorrowed aElement)
{
  // This function duplicates a lot of the code in
  // Gecko_GetStyleAttrDeclarationBlock above because I haven't worked out a way
  // to persuade hazard analysis that a pointer-to-lambda is ok yet.
  MOZ_ASSERT(aElement, "Invalid GeckoElement");

  DeclarationBlock* decl =
    const_cast<dom::Element*>(aElement)->GetSMILOverrideStyleDeclaration();
  if (!decl) {
    return nullptr;
  }
  if (decl->IsGecko()) {
    // XXX This can happen when nodes are adopted from a Gecko-style-backend
    //     document into a Servo-style-backend document.  See bug 1330051.
    NS_WARNING("stylo: requesting a Gecko declaration block?");
    return nullptr;
  }
  return decl->AsServo()->RefRawStrong();
}

const RawServoDeclarationBlockStrong*
AsRefRawStrong(const RefPtr<RawServoDeclarationBlock>& aDecl)
{
  static_assert(sizeof(RefPtr<RawServoDeclarationBlock>) ==
                sizeof(RawServoDeclarationBlockStrong),
                "RefPtr should just be a pointer");
  return reinterpret_cast<const RawServoDeclarationBlockStrong*>(&aDecl);
}

RawServoDeclarationBlockStrongBorrowedOrNull
Gecko_GetHTMLPresentationAttrDeclarationBlock(RawGeckoElementBorrowed aElement)
{
  const nsMappedAttributes* attrs = aElement->GetMappedAttributes();
  if (!attrs) {
    auto* svg = nsSVGElement::FromContentOrNull(aElement);
    if (svg) {
      if (auto decl = svg->GetContentDeclarationBlock()) {
        return decl->AsServo()->RefRawStrong();
      }
    }
    return nullptr;
  }

  return AsRefRawStrong(attrs->GetServoStyle());
}

RawServoDeclarationBlockStrongBorrowedOrNull
Gecko_GetExtraContentStyleDeclarations(RawGeckoElementBorrowed aElement)
{
  if (!aElement->IsAnyOfHTMLElements(nsGkAtoms::td, nsGkAtoms::th)) {
    return nullptr;
  }
  const HTMLTableCellElement* cell = static_cast<const HTMLTableCellElement*>(aElement);
  if (nsMappedAttributes* attrs = cell->GetMappedAttributesInheritedFromTable()) {
    return AsRefRawStrong(attrs->GetServoStyle());
  }
  return nullptr;
}

RawServoDeclarationBlockStrongBorrowedOrNull
Gecko_GetUnvisitedLinkAttrDeclarationBlock(RawGeckoElementBorrowed aElement)
{
  nsHTMLStyleSheet* sheet = aElement->OwnerDoc()->GetAttributeStyleSheet();
  if (!sheet) {
    return nullptr;
  }

  return AsRefRawStrong(sheet->GetServoUnvisitedLinkDecl());
}

ServoStyleSheet* Gecko_StyleSheet_Clone(
    const ServoStyleSheet* aSheet,
    const ServoStyleSheet* aNewParentSheet)
{
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
  return static_cast<ServoStyleSheet*>(newSheet.forget().take());
}

void
Gecko_StyleSheet_AddRef(const ServoStyleSheet* aSheet)
{
  MOZ_ASSERT(NS_IsMainThread());
  const_cast<ServoStyleSheet*>(aSheet)->AddRef();
}

void
Gecko_StyleSheet_Release(const ServoStyleSheet* aSheet)
{
  MOZ_ASSERT(NS_IsMainThread());
  const_cast<ServoStyleSheet*>(aSheet)->Release();
}

RawServoDeclarationBlockStrongBorrowedOrNull
Gecko_GetVisitedLinkAttrDeclarationBlock(RawGeckoElementBorrowed aElement)
{
  nsHTMLStyleSheet* sheet = aElement->OwnerDoc()->GetAttributeStyleSheet();
  if (!sheet) {
    return nullptr;
  }

  return AsRefRawStrong(sheet->GetServoVisitedLinkDecl());
}

RawServoDeclarationBlockStrongBorrowedOrNull
Gecko_GetActiveLinkAttrDeclarationBlock(RawGeckoElementBorrowed aElement)
{
  nsHTMLStyleSheet* sheet = aElement->OwnerDoc()->GetAttributeStyleSheet();
  if (!sheet) {
    return nullptr;
  }

  return AsRefRawStrong(sheet->GetServoActiveLinkDecl());
}

static CSSPseudoElementType
GetPseudoTypeFromElementForAnimation(const Element*& aElementOrPseudo) {
  if (aElementOrPseudo->IsGeneratedContentContainerForBefore()) {
    aElementOrPseudo = aElementOrPseudo->GetParent()->AsElement();
    return CSSPseudoElementType::before;
  }

  if (aElementOrPseudo->IsGeneratedContentContainerForAfter()) {
    aElementOrPseudo = aElementOrPseudo->GetParent()->AsElement();
    return CSSPseudoElementType::after;
  }

  return CSSPseudoElementType::NotPseudo;
}

bool
Gecko_GetAnimationRule(RawGeckoElementBorrowed aElement,
                       EffectCompositor::CascadeLevel aCascadeLevel,
                       RawServoAnimationValueMapBorrowedMut aAnimationValues)
{
  MOZ_ASSERT(aElement);

  nsIDocument* doc = aElement->GetComposedDoc();
  if (!doc || !doc->GetShell()) {
    return false;
  }
  nsPresContext* presContext = doc->GetShell()->GetPresContext();
  if (!presContext || !presContext->IsDynamic()) {
    // For print or print preview, ignore animations.
    return false;
  }

  CSSPseudoElementType pseudoType =
    GetPseudoTypeFromElementForAnimation(aElement);

  return presContext->EffectCompositor()
    ->GetServoAnimationRule(aElement,
                            pseudoType,
                            aCascadeLevel,
                            aAnimationValues);
}

bool
Gecko_StyleAnimationsEquals(RawGeckoStyleAnimationListBorrowed aA,
                            RawGeckoStyleAnimationListBorrowed aB)
{
  return *aA == *aB;
}

void
Gecko_UpdateAnimations(RawGeckoElementBorrowed aElement,
                       ServoStyleContextBorrowedOrNull aOldComputedValues,
                       ServoStyleContextBorrowedOrNull aComputedValues,
                       UpdateAnimationsTasks aTasks)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aElement);

  if (!aElement->IsInComposedDoc()) {
    return;
  }

  nsPresContext* presContext = nsContentUtils::GetContextForContent(aElement);
  if (!presContext || !presContext->IsDynamic()) {
    return;
  }

  CSSPseudoElementType pseudoType =
    GetPseudoTypeFromElementForAnimation(aElement);

  if (aTasks & UpdateAnimationsTasks::CSSAnimations) {
    presContext->AnimationManager()->
      UpdateAnimations(const_cast<dom::Element*>(aElement), pseudoType,
                       aComputedValues);
  }

  // aComputedValues might be nullptr if the target element is now in a
  // display:none subtree. We still call Gecko_UpdateAnimations in this case
  // because we need to stop CSS animations in the display:none subtree.
  // However, we don't need to update transitions since they are stopped by
  // RestyleManager::AnimationsWithDestroyedFrame so we just return early
  // here.
  if (!aComputedValues) {
    return;
  }

  if (aTasks & UpdateAnimationsTasks::CSSTransitions) {
    MOZ_ASSERT(aOldComputedValues);
    presContext->TransitionManager()->
      UpdateTransitions(const_cast<dom::Element*>(aElement), pseudoType,
                        aOldComputedValues,
                        aComputedValues);
  }

  if (aTasks & UpdateAnimationsTasks::EffectProperties) {
    presContext->EffectCompositor()->UpdateEffectProperties(
      aComputedValues, const_cast<dom::Element*>(aElement), pseudoType);
  }

  if (aTasks & UpdateAnimationsTasks::CascadeResults) {
    // This task will be scheduled if we detected any changes to !important
    // rules. We post a restyle here so that we can update the cascade
    // results in the pre-traversal of the next restyle.
    presContext->EffectCompositor()
               ->RequestRestyle(const_cast<Element*>(aElement),
                                pseudoType,
                                EffectCompositor::RestyleType::Standard,
                                EffectCompositor::CascadeLevel::Animations);
  }
}

bool
Gecko_ElementHasAnimations(RawGeckoElementBorrowed aElement)
{
  CSSPseudoElementType pseudoType =
    GetPseudoTypeFromElementForAnimation(aElement);

  return !!EffectSet::GetEffectSet(aElement, pseudoType);
}

bool
Gecko_ElementHasCSSAnimations(RawGeckoElementBorrowed aElement)
{
  CSSPseudoElementType pseudoType =
    GetPseudoTypeFromElementForAnimation(aElement);
  nsAnimationManager::CSSAnimationCollection* collection =
    nsAnimationManager::CSSAnimationCollection
                      ::GetAnimationCollection(aElement, pseudoType);

  return collection && !collection->mAnimations.IsEmpty();
}

bool
Gecko_ElementHasCSSTransitions(RawGeckoElementBorrowed aElement)
{
  CSSPseudoElementType pseudoType =
    GetPseudoTypeFromElementForAnimation(aElement);
  nsTransitionManager::CSSTransitionCollection* collection =
    nsTransitionManager::CSSTransitionCollection
                       ::GetAnimationCollection(aElement, pseudoType);

  return collection && !collection->mAnimations.IsEmpty();
}

size_t
Gecko_ElementTransitions_Length(RawGeckoElementBorrowed aElement)
{
  CSSPseudoElementType pseudoType =
    GetPseudoTypeFromElementForAnimation(aElement);
  nsTransitionManager::CSSTransitionCollection* collection =
    nsTransitionManager::CSSTransitionCollection
                       ::GetAnimationCollection(aElement, pseudoType);

  return collection ? collection->mAnimations.Length() : 0;
}

static CSSTransition*
GetCurrentTransitionAt(RawGeckoElementBorrowed aElement, size_t aIndex)
{
  CSSPseudoElementType pseudoType =
    GetPseudoTypeFromElementForAnimation(aElement);
  nsTransitionManager::CSSTransitionCollection* collection =
    nsTransitionManager::CSSTransitionCollection
                       ::GetAnimationCollection(aElement, pseudoType);
  if (!collection) {
    return nullptr;
  }
  nsTArray<RefPtr<CSSTransition>>& transitions = collection->mAnimations;
  return aIndex < transitions.Length()
         ? transitions[aIndex].get()
         : nullptr;
}

nsCSSPropertyID
Gecko_ElementTransitions_PropertyAt(RawGeckoElementBorrowed aElement,
                                    size_t aIndex)
{
  CSSTransition* transition = GetCurrentTransitionAt(aElement, aIndex);
  return transition ? transition->TransitionProperty()
                    : nsCSSPropertyID::eCSSProperty_UNKNOWN;
}

RawServoAnimationValueBorrowedOrNull
Gecko_ElementTransitions_EndValueAt(RawGeckoElementBorrowed aElement,
                                    size_t aIndex)
{
  CSSTransition* transition = GetCurrentTransitionAt(aElement,
                                                     aIndex);
  return transition ? transition->ToValue().mServo.get() : nullptr;
}

double
Gecko_GetProgressFromComputedTiming(RawGeckoComputedTimingBorrowed aComputedTiming)
{
  return aComputedTiming->mProgress.Value();
}

double
Gecko_GetPositionInSegment(RawGeckoAnimationPropertySegmentBorrowed aSegment,
                          double aProgress,
                          ComputedTimingFunction::BeforeFlag aBeforeFlag)
{
  MOZ_ASSERT(aSegment->mFromKey < aSegment->mToKey,
             "The segment from key should be less than to key");

  double positionInSegment =
    (aProgress - aSegment->mFromKey) / (aSegment->mToKey - aSegment->mFromKey);

  return ComputedTimingFunction::GetPortion(aSegment->mTimingFunction,
                                            positionInSegment,
                                            aBeforeFlag);
}

bool
Gecko_IsFramesTimingEnabled() {
  return sFramesTimingFunctionEnabled;
}

RawServoAnimationValueBorrowedOrNull
Gecko_AnimationGetBaseStyle(void* aBaseStyles, nsCSSPropertyID aProperty)
{
  auto base =
    static_cast<nsRefPtrHashtable<nsUint32HashKey, RawServoAnimationValue>*>
      (aBaseStyles);
  return base->GetWeak(aProperty);
}

void
Gecko_StyleTransition_SetUnsupportedProperty(StyleTransition* aTransition,
                                             nsIAtom* aAtom)
{
  nsCSSPropertyID id =
    nsCSSProps::LookupProperty(nsDependentAtomString(aAtom),
                               CSSEnabledState::eForAllContent);
  if (id == eCSSProperty_UNKNOWN || id == eCSSPropertyExtra_variable) {
    aTransition->SetUnknownProperty(id, aAtom);
  } else {
    aTransition->SetProperty(id);
  }
}

void
Gecko_FillAllBackgroundLists(nsStyleImageLayers* aLayers, uint32_t aMaxLen)
{
  nsRuleNode::FillAllBackgroundLists(*aLayers, aMaxLen);
}

void
Gecko_FillAllMaskLists(nsStyleImageLayers* aLayers, uint32_t aMaxLen)
{
  nsRuleNode::FillAllMaskLists(*aLayers, aMaxLen);
}

RawGeckoElementBorrowedOrNull
Gecko_GetBody(RawGeckoPresContextBorrowed aPresContext)
{
  return aPresContext->Document()->GetBodyElement();
}

nscolor Gecko_GetLookAndFeelSystemColor(int32_t aId,
                                        RawGeckoPresContextBorrowed aPresContext)
{
  bool useStandinsForNativeColors = aPresContext && !aPresContext->IsChrome();
  nscolor result;
  LookAndFeel::ColorID colorId = static_cast<LookAndFeel::ColorID>(aId);
  LookAndFeel::GetColor(colorId, useStandinsForNativeColors, &result);
  return result;
}

bool
Gecko_MatchStringArgPseudo(RawGeckoElementBorrowed aElement,
                           CSSPseudoClassType aType,
                           const char16_t* aIdent,
                           bool* aSetSlowSelectorFlag)
{
  EventStates dummyMask; // mask is never read because we pass aDependence=nullptr
  return nsCSSRuleProcessor::StringPseudoMatches(aElement, aType, aIdent,
                                                 aElement->OwnerDoc(), true,
                                                 dummyMask, aSetSlowSelectorFlag, nullptr);
}

bool
Gecko_MatchLang(RawGeckoElementBorrowed aElement,
                nsIAtom* aOverrideLang,
                bool aHasOverrideLang,
                const char16_t* aValue)
{
  MOZ_ASSERT(!(aOverrideLang && !aHasOverrideLang),
             "aHasOverrideLang should only be set when aOverrideLang is null");

  if (!aHasOverrideLang) {
    return nsCSSRuleProcessor::LangPseudoMatches(aElement, nullptr, false,
                                                 aValue, aElement->OwnerDoc());
  }

  return nsCSSRuleProcessor::LangPseudoMatches(aElement, aOverrideLang, true,
                                               aValue, aElement->OwnerDoc());
}

nsIAtom*
Gecko_GetXMLLangValue(RawGeckoElementBorrowed aElement)
{
  const nsAttrValue* attr =
    aElement->GetParsedAttr(nsGkAtoms::lang, kNameSpaceID_XML);

  if (!attr) {
    return nullptr;
  }

  MOZ_ASSERT(attr->Type() == nsAttrValue::eAtom);

  nsCOMPtr<nsIAtom> atom = attr->GetAtomValue();
  return atom.forget().take();
}

nsIDocument::DocumentTheme
Gecko_GetDocumentLWTheme(const nsIDocument* aDocument)
{
  return aDocument->ThreadSafeGetDocumentLWTheme();
}

template <typename Implementor>
static nsIAtom*
AtomAttrValue(Implementor* aElement, nsIAtom* aName)
{
  const nsAttrValue* attr = aElement->GetParsedAttr(aName);
  return attr ? attr->GetAtomValue() : nullptr;
}

template <typename Implementor>
static nsIAtom*
LangValue(Implementor* aElement)
{
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
  nsCOMPtr<nsIAtom> atom = attr->GetAtomValue();
  return atom.forget().take();
}

template <typename Implementor, typename MatchFn>
static bool
DoMatch(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName, MatchFn aMatch)
{
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
static bool
HasAttr(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName)
{
  auto match = [](const nsAttrValue* aValue) { return true; };
  return DoMatch(aElement, aNS, aName, match);
}

template <typename Implementor>
static bool
AttrEquals(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName, nsIAtom* aStr,
           bool aIgnoreCase)
{
  auto match = [aStr, aIgnoreCase](const nsAttrValue* aValue) {
    return aValue->Equals(aStr, aIgnoreCase ? eIgnoreCase : eCaseMatters);
  };
  return DoMatch(aElement, aNS, aName, match);
}

#define WITH_COMPARATOR(ignore_case_, c_, expr_)    \
    if (ignore_case_) {                             \
      const nsCaseInsensitiveStringComparator c_    \
          = nsCaseInsensitiveStringComparator();    \
      return expr_;                                 \
    } else {                                        \
      const nsDefaultStringComparator c_;           \
      return expr_;                                 \
    }


template <typename Implementor>
static bool
AttrDashEquals(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName,
               nsIAtom* aStr, bool aIgnoreCase)
{
  auto match = [aStr, aIgnoreCase](const nsAttrValue* aValue) {
    nsAutoString str;
    aValue->ToString(str);
    WITH_COMPARATOR(aIgnoreCase, c,
                    nsStyleUtil::DashMatchCompare(str, nsDependentAtomString(aStr), c))
  };
  return DoMatch(aElement, aNS, aName, match);
}

template <typename Implementor>
static bool
AttrIncludes(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName,
             nsIAtom* aStr, bool aIgnoreCase)
{
  auto match = [aStr, aIgnoreCase](const nsAttrValue* aValue) {
    nsAutoString str;
    aValue->ToString(str);
    WITH_COMPARATOR(aIgnoreCase, c,
                    nsStyleUtil::ValueIncludes(str, nsDependentAtomString(aStr), c))
  };
  return DoMatch(aElement, aNS, aName, match);
}

template <typename Implementor>
static bool
AttrHasSubstring(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName,
                 nsIAtom* aStr, bool aIgnoreCase)
{
  auto match = [aStr, aIgnoreCase](const nsAttrValue* aValue) {
    nsAutoString str;
    aValue->ToString(str);
    WITH_COMPARATOR(aIgnoreCase, c,
                    FindInReadable(nsDependentAtomString(aStr), str, c))
  };
  return DoMatch(aElement, aNS, aName, match);
}

template <typename Implementor>
static bool
AttrHasPrefix(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName,
              nsIAtom* aStr, bool aIgnoreCase)
{
  auto match = [aStr, aIgnoreCase](const nsAttrValue* aValue) {
    nsAutoString str;
    aValue->ToString(str);
    WITH_COMPARATOR(aIgnoreCase, c,
                    StringBeginsWith(str, nsDependentAtomString(aStr), c))
  };
  return DoMatch(aElement, aNS, aName, match);
}

template <typename Implementor>
static bool
AttrHasSuffix(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName,
              nsIAtom* aStr, bool aIgnoreCase)
{
  auto match = [aStr, aIgnoreCase](const nsAttrValue* aValue) {
    nsAutoString str;
    aValue->ToString(str);
    WITH_COMPARATOR(aIgnoreCase, c,
                    StringEndsWith(str, nsDependentAtomString(aStr), c))
  };
  return DoMatch(aElement, aNS, aName, match);
}

/**
 * Gets the class or class list (if any) of the implementor. The calling
 * convention here is rather hairy, and is optimized for getting Servo the
 * information it needs for hot calls.
 *
 * The return value indicates the number of classes. If zero, neither outparam
 * is valid. If one, the class_ outparam is filled with the atom of the class.
 * If two or more, the classList outparam is set to point to an array of atoms
 * representing the class list.
 *
 * The array is borrowed and the atoms are not addrefed. These values can be
 * invalidated by any DOM mutation. Use them in a tight scope.
 */
template <typename Implementor>
static uint32_t
ClassOrClassList(Implementor* aElement, nsIAtom** aClass, nsIAtom*** aClassList)
{
  const nsAttrValue* attr = aElement->GetClasses();
  if (!attr) {
    return 0;
  }

  // For class values with only whitespace, Gecko just stores a string. For the
  // purposes of the style system, there is no class in this case.
  if (attr->Type() == nsAttrValue::eString) {
    MOZ_ASSERT(nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(
                 attr->GetStringValue()).IsEmpty());
    return 0;
  }

  // Single tokens are generally stored as an atom. Check that case.
  if (attr->Type() == nsAttrValue::eAtom) {
    *aClass = attr->GetAtomValue();
    return 1;
  }

  // At this point we should have an atom array. It is likely, but not
  // guaranteed, that we have two or more elements in the array.
  MOZ_ASSERT(attr->Type() == nsAttrValue::eAtomArray);
  nsTArray<nsCOMPtr<nsIAtom>>* atomArray = attr->GetAtomArrayValue();
  uint32_t length = atomArray->Length();

  // Special case: zero elements.
  if (length == 0) {
    return 0;
  }

  // Special case: one element.
  if (length == 1) {
    *aClass = atomArray->ElementAt(0);
    return 1;
  }

  // General case: Two or more elements.
  //
  // Note: We could also expose this array as an array of nsCOMPtrs, since
  // bindgen knows what those look like, and eliminate the reinterpret_cast.
  // But it's not obvious that that would be preferable.
  static_assert(sizeof(nsCOMPtr<nsIAtom>) == sizeof(nsIAtom*), "Bad simplification");
  static_assert(alignof(nsCOMPtr<nsIAtom>) == alignof(nsIAtom*), "Bad simplification");

  nsCOMPtr<nsIAtom>* elements = atomArray->Elements();
  nsIAtom** rawElements = reinterpret_cast<nsIAtom**>(elements);
  *aClassList = rawElements;
  return atomArray->Length();
}

#define SERVO_IMPL_ELEMENT_ATTR_MATCHING_FUNCTIONS(prefix_, implementor_)        \
  nsIAtom* prefix_##AtomAttrValue(implementor_ aElement, nsIAtom* aName)         \
  {                                                                              \
    return AtomAttrValue(aElement, aName);                                       \
  }                                                                              \
  nsIAtom* prefix_##LangValue(implementor_ aElement)                             \
  {                                                                              \
    return LangValue(aElement);                                                  \
  }                                                                              \
  bool prefix_##HasAttr(implementor_ aElement, nsIAtom* aNS, nsIAtom* aName)     \
  {                                                                              \
    return HasAttr(aElement, aNS, aName);                                        \
  }                                                                              \
  bool prefix_##AttrEquals(implementor_ aElement, nsIAtom* aNS,                  \
                           nsIAtom* aName, nsIAtom* aStr, bool aIgnoreCase)      \
  {                                                                              \
    return AttrEquals(aElement, aNS, aName, aStr, aIgnoreCase);                  \
  }                                                                              \
  bool prefix_##AttrDashEquals(implementor_ aElement, nsIAtom* aNS,              \
                               nsIAtom* aName, nsIAtom* aStr, bool aIgnoreCase)  \
  {                                                                              \
    return AttrDashEquals(aElement, aNS, aName, aStr, aIgnoreCase);              \
  }                                                                              \
  bool prefix_##AttrIncludes(implementor_ aElement, nsIAtom* aNS,                \
                             nsIAtom* aName, nsIAtom* aStr, bool aIgnoreCase)    \
  {                                                                              \
    return AttrIncludes(aElement, aNS, aName, aStr, aIgnoreCase);                \
  }                                                                              \
  bool prefix_##AttrHasSubstring(implementor_ aElement, nsIAtom* aNS,            \
                                 nsIAtom* aName, nsIAtom* aStr, bool aIgnoreCase)\
  {                                                                              \
    return AttrHasSubstring(aElement, aNS, aName, aStr, aIgnoreCase);            \
  }                                                                              \
  bool prefix_##AttrHasPrefix(implementor_ aElement, nsIAtom* aNS,               \
                              nsIAtom* aName, nsIAtom* aStr, bool aIgnoreCase)   \
  {                                                                              \
    return AttrHasPrefix(aElement, aNS, aName, aStr, aIgnoreCase);               \
  }                                                                              \
  bool prefix_##AttrHasSuffix(implementor_ aElement, nsIAtom* aNS,               \
                              nsIAtom* aName, nsIAtom* aStr, bool aIgnoreCase)   \
  {                                                                              \
    return AttrHasSuffix(aElement, aNS, aName, aStr, aIgnoreCase);               \
  }                                                                              \
  uint32_t prefix_##ClassOrClassList(implementor_ aElement, nsIAtom** aClass,    \
                                     nsIAtom*** aClassList)                      \
  {                                                                              \
    return ClassOrClassList(aElement, aClass, aClassList);                       \
  }

SERVO_IMPL_ELEMENT_ATTR_MATCHING_FUNCTIONS(Gecko_, RawGeckoElementBorrowed)
SERVO_IMPL_ELEMENT_ATTR_MATCHING_FUNCTIONS(Gecko_Snapshot, const ServoElementSnapshot*)

#undef SERVO_IMPL_ELEMENT_ATTR_MATCHING_FUNCTIONS

nsIAtom*
Gecko_Atomize(const char* aString, uint32_t aLength)
{
  return NS_Atomize(nsDependentCSubstring(aString, aLength)).take();
}

nsIAtom*
Gecko_Atomize16(const nsAString* aString)
{
  return NS_Atomize(*aString).take();
}

void
Gecko_AddRefAtom(nsIAtom* aAtom)
{
  NS_ADDREF(aAtom);
}

void
Gecko_ReleaseAtom(nsIAtom* aAtom)
{
  NS_RELEASE(aAtom);
}

const uint16_t*
Gecko_GetAtomAsUTF16(nsIAtom* aAtom, uint32_t* aLength)
{
  static_assert(sizeof(char16_t) == sizeof(uint16_t), "Servo doesn't know what a char16_t is");
  MOZ_ASSERT(aAtom);
  *aLength = aAtom->GetLength();

  // We need to manually cast from char16ptr_t to const char16_t* to handle the
  // MOZ_USE_CHAR16_WRAPPER we use on WIndows.
  return reinterpret_cast<const uint16_t*>(static_cast<const char16_t*>(aAtom->GetUTF16String()));
}

bool
Gecko_AtomEqualsUTF8(nsIAtom* aAtom, const char* aString, uint32_t aLength)
{
  // XXXbholley: We should be able to do this without converting, I just can't
  // find the right thing to call.
  nsDependentAtomString atomStr(aAtom);
  NS_ConvertUTF8toUTF16 inStr(nsDependentCSubstring(aString, aLength));
  return atomStr.Equals(inStr);
}

bool
Gecko_AtomEqualsUTF8IgnoreCase(nsIAtom* aAtom, const char* aString, uint32_t aLength)
{
  // XXXbholley: We should be able to do this without converting, I just can't
  // find the right thing to call.
  nsDependentAtomString atomStr(aAtom);
  NS_ConvertUTF8toUTF16 inStr(nsDependentCSubstring(aString, aLength));
  return nsContentUtils::EqualsIgnoreASCIICase(atomStr, inStr);
}

void
Gecko_EnsureMozBorderColors(nsStyleBorder* aBorder)
{
  aBorder->EnsureBorderColors();
}

void Gecko_ClearMozBorderColors(nsStyleBorder* aBorder, mozilla::Side aSide)
{
  aBorder->ClearBorderColors(aSide);
}

void
Gecko_AppendMozBorderColors(nsStyleBorder* aBorder, mozilla::Side aSide,
                            nscolor aColor)
{
  aBorder->AppendBorderColor(aSide, aColor);
}

void
Gecko_CopyMozBorderColors(nsStyleBorder* aDest, const nsStyleBorder* aSrc,
                          mozilla::Side aSide)
{
  if (aSrc->mBorderColors) {
    aDest->CopyBorderColorsFrom(aSrc->mBorderColors[aSide], aSide);
  }
}

const nsBorderColors*
Gecko_GetMozBorderColors(const nsStyleBorder* aBorder, mozilla::Side aSide)
{
  MOZ_ASSERT(aBorder);
  return aBorder->mBorderColors ? aBorder->mBorderColors[aSide] : nullptr;
}

void
Gecko_FontFamilyList_Clear(FontFamilyList* aList) {
  aList->Clear();
}

void
Gecko_FontFamilyList_AppendNamed(FontFamilyList* aList, nsIAtom* aName, bool aQuoted)
{
  FontFamilyName family;
  aName->ToString(family.mName);
  if (aQuoted) {
    family.mType = eFamily_named_quoted;
  }

  aList->Append(family);
}

void
Gecko_FontFamilyList_AppendGeneric(FontFamilyList* aList, FontFamilyType aType)
{
  aList->Append(FontFamilyName(aType));
}

void
Gecko_CopyFontFamilyFrom(nsFont* dst, const nsFont* src)
{
  dst->fontlist = src->fontlist;
}

void
Gecko_nsFont_InitSystem(nsFont* aDest, int32_t aFontId,
                        const nsStyleFont* aFont, RawGeckoPresContextBorrowed aPresContext)
{
  const nsFont* defaultVariableFont = ThreadSafeGetDefaultFontHelper(aPresContext, aFont->mLanguage,
                                                                     kPresContext_DefaultVariableFont_ID);

  // We have passed uninitialized memory to this function,
  // initialize it. We can't simply return an nsFont because then
  // we need to know its size beforehand. Servo cannot initialize nsFont
  // itself, so this will do.
  nsFont* system = new (aDest) nsFont(*defaultVariableFont);

  MOZ_RELEASE_ASSERT(system);

  *aDest = *defaultVariableFont;
  LookAndFeel::FontID fontID = static_cast<LookAndFeel::FontID>(aFontId);

  MutexAutoLock lock(*sServoFontMetricsLock);
  nsRuleNode::ComputeSystemFont(aDest, fontID, aPresContext, defaultVariableFont);
}

void
Gecko_nsFont_Destroy(nsFont* aDest)
{
  aDest->~nsFont();
}


void
Gecko_ClearAlternateValues(nsFont* aFont, size_t aLength)
{
  aFont->alternateValues.Clear();
  aFont->alternateValues.SetCapacity(aLength);
}

void
Gecko_AppendAlternateValues(nsFont* aFont, uint32_t aAlternateName, nsIAtom* aAtom)
{
  aFont->alternateValues.AppendElement(gfxAlternateValue {
    aAlternateName,
    nsDependentAtomString(aAtom)
  });
}

void
Gecko_CopyAlternateValuesFrom(nsFont* aDest, const nsFont* aSrc)
{
  aDest->alternateValues.Clear();
  aDest->alternateValues.AppendElements(aSrc->alternateValues);
}

void
Gecko_SetImageOrientation(nsStyleVisibility* aVisibility,
                          uint8_t aOrientation, bool aFlip)
{
  aVisibility->mImageOrientation =
    nsStyleImageOrientation::CreateAsOrientationAndFlip(aOrientation, aFlip);
}

void
Gecko_SetImageOrientationAsFromImage(nsStyleVisibility* aVisibility)
{
  aVisibility->mImageOrientation = nsStyleImageOrientation::CreateAsFromImage();
}

void
Gecko_CopyImageOrientationFrom(nsStyleVisibility* aDst,
                               const nsStyleVisibility* aSrc)
{
  aDst->mImageOrientation = aSrc->mImageOrientation;
}

void
Gecko_SetCounterStyleToName(CounterStylePtr* aPtr, nsIAtom* aName,
                            RawGeckoPresContextBorrowed aPresContext)
{
  // Try resolving the counter style if possible, and keep it unresolved
  // otherwise.
  CounterStyleManager* manager = aPresContext->CounterStyleManager();
  nsCOMPtr<nsIAtom> name = already_AddRefed<nsIAtom>(aName);
  if (CounterStyle* style = manager->GetCounterStyle(name)) {
    *aPtr = style;
  } else {
    *aPtr = name.forget();
  }
}

void
Gecko_SetCounterStyleToSymbols(CounterStylePtr* aPtr, uint8_t aSymbolsType,
                               nsACString const* const* aSymbols,
                               uint32_t aSymbolsCount)
{
  nsTArray<nsString> symbols(aSymbolsCount);
  for (uint32_t i = 0; i < aSymbolsCount; i++) {
    symbols.AppendElement(NS_ConvertUTF8toUTF16(*aSymbols[i]));
  }
  *aPtr = new AnonymousCounterStyle(aSymbolsType, Move(symbols));
}

void
Gecko_SetCounterStyleToString(CounterStylePtr* aPtr, const nsACString* aSymbol)
{
  *aPtr = new AnonymousCounterStyle(NS_ConvertUTF8toUTF16(*aSymbol));
}

void
Gecko_CopyCounterStyle(CounterStylePtr* aDst, const CounterStylePtr* aSrc)
{
  *aDst = *aSrc;
}

already_AddRefed<css::URLValue>
ServoBundledURI::IntoCssUrl()
{
  if (!mURLString) {
    return nullptr;
  }

  MOZ_ASSERT(mExtraData->GetReferrer());
  MOZ_ASSERT(mExtraData->GetPrincipal());

  NS_ConvertUTF8toUTF16 url(reinterpret_cast<const char*>(mURLString),
                            mURLStringLength);

  RefPtr<css::URLValue> urlValue =
    new css::URLValue(url, do_AddRef(mExtraData));
  return urlValue.forget();
}

void
Gecko_SetNullImageValue(nsStyleImage* aImage)
{
  MOZ_ASSERT(aImage);
  aImage->SetNull();
}

void
Gecko_SetGradientImageValue(nsStyleImage* aImage, nsStyleGradient* aGradient)
{
  MOZ_ASSERT(aImage);
  aImage->SetGradientData(aGradient);
}

NS_IMPL_THREADSAFE_FFI_REFCOUNTING(mozilla::css::ImageValue, ImageValue);

static already_AddRefed<nsStyleImageRequest>
CreateStyleImageRequest(nsStyleImageRequest::Mode aModeFlags,
                        mozilla::css::ImageValue* aImageValue)
{
  RefPtr<nsStyleImageRequest> req =
    new nsStyleImageRequest(aModeFlags, aImageValue);
  return req.forget();
}

mozilla::css::ImageValue*
Gecko_ImageValue_Create(ServoBundledURI aURI)
{
  NS_ConvertUTF8toUTF16 url(reinterpret_cast<const char*>(aURI.mURLString),
                            aURI.mURLStringLength);

  RefPtr<css::ImageValue> value(
    new css::ImageValue(url, do_AddRef(aURI.mExtraData)));
  return value.forget().take();
}

void
Gecko_SetLayerImageImageValue(nsStyleImage* aImage,
                              mozilla::css::ImageValue* aImageValue)
{
  MOZ_ASSERT(aImage && aImageValue);

  RefPtr<nsStyleImageRequest> req =
    CreateStyleImageRequest(nsStyleImageRequest::Mode::Track, aImageValue);
  aImage->SetImageRequest(req.forget());
}

void
Gecko_SetImageElement(nsStyleImage* aImage, nsIAtom* aAtom) {
  MOZ_ASSERT(aImage);
  aImage->SetElementId(do_AddRef(aAtom));
}

void
Gecko_CopyImageValueFrom(nsStyleImage* aImage, const nsStyleImage* aOther)
{
  MOZ_ASSERT(aImage);
  MOZ_ASSERT(aOther);

  *aImage = *aOther;
}

void
Gecko_InitializeImageCropRect(nsStyleImage* aImage)
{
  MOZ_ASSERT(aImage);
  aImage->SetCropRect(MakeUnique<nsStyleSides>());
}

void
Gecko_SetCursorArrayLength(nsStyleUserInterface* aStyleUI, size_t aLen)
{
  aStyleUI->mCursorImages.Clear();
  aStyleUI->mCursorImages.SetLength(aLen);
}

void
Gecko_SetCursorImageValue(nsCursorImage* aCursor,
                          mozilla::css::ImageValue* aImageValue)
{
  MOZ_ASSERT(aCursor && aImageValue);

  aCursor->mImage =
    CreateStyleImageRequest(nsStyleImageRequest::Mode::Discard, aImageValue);
}

void
Gecko_CopyCursorArrayFrom(nsStyleUserInterface* aDest,
                          const nsStyleUserInterface* aSrc)
{
  aDest->mCursorImages = aSrc->mCursorImages;
}

void
Gecko_SetContentDataImageValue(nsStyleContentData* aContent,
                               mozilla::css::ImageValue* aImageValue)
{
  MOZ_ASSERT(aContent && aImageValue);

  RefPtr<nsStyleImageRequest> req =
    CreateStyleImageRequest(nsStyleImageRequest::Mode::Track, aImageValue);
  aContent->SetImageRequest(req.forget());
}

nsStyleContentData::CounterFunction*
Gecko_SetCounterFunction(nsStyleContentData* aContent, nsStyleContentType aType)
{
  RefPtr<nsStyleContentData::CounterFunction>
    counterFunc = new nsStyleContentData::CounterFunction();
  nsStyleContentData::CounterFunction* ptr = counterFunc;
  aContent->SetCounters(aType, counterFunc.forget());
  return ptr;
}

nsStyleGradient*
Gecko_CreateGradient(uint8_t aShape,
                     uint8_t aSize,
                     bool aRepeating,
                     bool aLegacySyntax,
                     bool aMozLegacySyntax,
                     uint32_t aStopCount)
{
  nsStyleGradient* result = new nsStyleGradient();

  result->mShape = aShape;
  result->mSize = aSize;
  result->mRepeating = aRepeating;
  result->mLegacySyntax = aLegacySyntax;
  result->mMozLegacySyntax = aMozLegacySyntax;

  result->mAngle.SetNoneValue();
  result->mBgPosX.SetNoneValue();
  result->mBgPosY.SetNoneValue();
  result->mRadiusX.SetNoneValue();
  result->mRadiusY.SetNoneValue();

  nsStyleGradientStop dummyStop;
  dummyStop.mLocation.SetNoneValue();
  dummyStop.mColor = NS_RGB(0, 0, 0);
  dummyStop.mIsInterpolationHint = 0;

  for (uint32_t i = 0; i < aStopCount; i++) {
    result->mStops.AppendElement(dummyStop);
  }

  return result;
}

const mozilla::css::URLValueData*
Gecko_GetURLValue(const nsStyleImage* aImage)
{
  MOZ_ASSERT(aImage && aImage->GetType() == eStyleImageType_Image);
  return aImage->GetURLValue();
}

nsIAtom*
Gecko_GetImageElement(const nsStyleImage* aImage)
{
  MOZ_ASSERT(aImage && aImage->GetType() == eStyleImageType_Element);
  return const_cast<nsIAtom*>(aImage->GetElementId());
}

const nsStyleGradient*
Gecko_GetGradientImageValue(const nsStyleImage* aImage)
{
  MOZ_ASSERT(aImage && aImage->GetType() == eStyleImageType_Gradient);
  return aImage->GetGradientData();
}

void
Gecko_SetListStyleImageNone(nsStyleList* aList)
{
  aList->mListStyleImage = nullptr;
}

void
Gecko_SetListStyleImageImageValue(nsStyleList* aList,
                             mozilla::css::ImageValue* aImageValue)
{
  MOZ_ASSERT(aList && aImageValue);

  aList->mListStyleImage =
    CreateStyleImageRequest(nsStyleImageRequest::Mode(0), aImageValue);
}

void
Gecko_CopyListStyleImageFrom(nsStyleList* aList, const nsStyleList* aSource)
{
  aList->mListStyleImage = aSource->mListStyleImage;
}

void
Gecko_EnsureTArrayCapacity(void* aArray, size_t aCapacity, size_t aElemSize)
{
  auto base =
    reinterpret_cast<nsTArray_base<nsTArrayInfallibleAllocator,
                                   nsTArray_CopyWithMemutils>*>(aArray);

  base->EnsureCapacity<nsTArrayInfallibleAllocator>(aCapacity, aElemSize);
}

void
Gecko_ClearPODTArray(void* aArray, size_t aElementSize, size_t aElementAlign)
{
  auto base =
    reinterpret_cast<nsTArray_base<nsTArrayInfallibleAllocator,
                                   nsTArray_CopyWithMemutils>*>(aArray);

  base->template ShiftData<nsTArrayInfallibleAllocator>(0, base->Length(), 0,
                                                        aElementSize, aElementAlign);
}

void Gecko_SetStyleGridTemplateArrayLengths(nsStyleGridTemplate* aValue,
                                            uint32_t aTrackSizes)
{
  aValue->mMinTrackSizingFunctions.SetLength(aTrackSizes);
  aValue->mMaxTrackSizingFunctions.SetLength(aTrackSizes);
  aValue->mLineNameLists.SetLength(aTrackSizes + 1);
}

void Gecko_SetGridTemplateLineNamesLength(nsStyleGridTemplate* aValue,
                                          uint32_t aNames)
{
  aValue->mLineNameLists.SetLength(aNames);
}

void Gecko_ResizeTArrayForStrings(nsTArray<nsString>* aArray, uint32_t aLength)
{
  aArray->SetLength(aLength);
}

void
Gecko_CopyStyleGridTemplateValues(nsStyleGridTemplate* aGridTemplate,
                                  const nsStyleGridTemplate* aOther)
{
  *aGridTemplate = *aOther;
}

mozilla::css::GridTemplateAreasValue*
Gecko_NewGridTemplateAreasValue(uint32_t aAreas, uint32_t aTemplates, uint32_t aColumns)
{
  RefPtr<mozilla::css::GridTemplateAreasValue> value = new mozilla::css::GridTemplateAreasValue;
  value->mNamedAreas.SetLength(aAreas);
  value->mTemplates.SetLength(aTemplates);
  value->mNColumns = aColumns;
  return value.forget().take();
}

NS_IMPL_THREADSAFE_FFI_REFCOUNTING(mozilla::css::GridTemplateAreasValue, GridTemplateAreasValue);

void
Gecko_ClearAndResizeStyleContents(nsStyleContent* aContent, uint32_t aHowMany)
{
  aContent->AllocateContents(aHowMany);
}

void
Gecko_CopyStyleContentsFrom(nsStyleContent* aContent, const nsStyleContent* aOther)
{
  uint32_t count = aOther->ContentCount();

  aContent->AllocateContents(count);

  for (uint32_t i = 0; i < count; ++i) {
    aContent->ContentAt(i) = aOther->ContentAt(i);
  }
}

void
Gecko_ClearAndResizeCounterIncrements(nsStyleContent* aContent, uint32_t aHowMany)
{
  aContent->AllocateCounterIncrements(aHowMany);
}

void
Gecko_CopyCounterIncrementsFrom(nsStyleContent* aContent, const nsStyleContent* aOther)
{
  uint32_t count = aOther->CounterIncrementCount();

  aContent->AllocateCounterIncrements(count);

  for (uint32_t i = 0; i < count; ++i) {
    const nsStyleCounterData& data = aOther->CounterIncrementAt(i);
    aContent->SetCounterIncrementAt(i, data.mCounter, data.mValue);
  }
}

void
Gecko_ClearAndResizeCounterResets(nsStyleContent* aContent, uint32_t aHowMany)
{
  aContent->AllocateCounterResets(aHowMany);
}

void
Gecko_CopyCounterResetsFrom(nsStyleContent* aContent, const nsStyleContent* aOther)
{
  uint32_t count = aOther->CounterResetCount();

  aContent->AllocateCounterResets(count);

  for (uint32_t i = 0; i < count; ++i) {
    const nsStyleCounterData& data = aOther->CounterResetAt(i);
    aContent->SetCounterResetAt(i, data.mCounter, data.mValue);
  }
}

void
Gecko_EnsureImageLayersLength(nsStyleImageLayers* aLayers, size_t aLen,
                              nsStyleImageLayers::LayerType aLayerType)
{
  size_t oldLength = aLayers->mLayers.Length();

  aLayers->mLayers.EnsureLengthAtLeast(aLen);

  for (size_t i = oldLength; i < aLen; ++i) {
    aLayers->mLayers[i].Initialize(aLayerType);
  }
}

void
Gecko_EnsureStyleAnimationArrayLength(void* aArray, size_t aLen)
{
  auto base =
    static_cast<nsStyleAutoArray<StyleAnimation>*>(aArray);

  size_t oldLength = base->Length();

  base->EnsureLengthAtLeast(aLen);

  for (size_t i = oldLength; i < aLen; ++i) {
    (*base)[i].SetInitialValues();
  }
}

void
Gecko_EnsureStyleTransitionArrayLength(void* aArray, size_t aLen)
{
  auto base =
    reinterpret_cast<nsStyleAutoArray<StyleTransition>*>(aArray);

  size_t oldLength = base->Length();

  base->EnsureLengthAtLeast(aLen);

  for (size_t i = oldLength; i < aLen; ++i) {
    (*base)[i].SetInitialValues();
  }
}

void
Gecko_ClearWillChange(nsStyleDisplay* aDisplay, size_t aLength)
{
  aDisplay->mWillChange.Clear();
  aDisplay->mWillChange.SetCapacity(aLength);
}

void
Gecko_AppendWillChange(nsStyleDisplay* aDisplay, nsIAtom* aAtom)
{
  aDisplay->mWillChange.AppendElement(aAtom);
}

void
Gecko_CopyWillChangeFrom(nsStyleDisplay* aDest, nsStyleDisplay* aSrc)
{
  aDest->mWillChange.Clear();
  aDest->mWillChange.AppendElements(aSrc->mWillChange);
}

enum class KeyframeSearchDirection {
  Forwards,
  Backwards,
};

enum class KeyframeInsertPosition {
  Prepend,
  LastForOffset,
};

static Keyframe*
GetOrCreateKeyframe(nsTArray<Keyframe>* aKeyframes,
                    float aOffset,
                    const nsTimingFunction* aTimingFunction,
                    KeyframeSearchDirection aSearchDirection,
                    KeyframeInsertPosition aInsertPosition)
{
  MOZ_ASSERT(aKeyframes, "The keyframe array should be valid");
  MOZ_ASSERT(aTimingFunction, "The timing function should be valid");
  MOZ_ASSERT(aOffset >= 0. && aOffset <= 1.,
             "The offset should be in the range of [0.0, 1.0]");

  size_t keyframeIndex;
  switch (aSearchDirection) {
    case KeyframeSearchDirection::Forwards:
      if (nsAnimationManager::FindMatchingKeyframe(*aKeyframes,
                                                   aOffset,
                                                   *aTimingFunction,
                                                   keyframeIndex)) {
        return &(*aKeyframes)[keyframeIndex];
      }
      break;
    case KeyframeSearchDirection::Backwards:
      if (nsAnimationManager::FindMatchingKeyframe(Reversed(*aKeyframes),
                                                   aOffset,
                                                   *aTimingFunction,
                                                   keyframeIndex)) {
        return &(*aKeyframes)[aKeyframes->Length() - 1 - keyframeIndex];
      }
      keyframeIndex = aKeyframes->Length() - 1;
      break;
  }

  Keyframe* keyframe =
    aKeyframes->InsertElementAt(
      aInsertPosition == KeyframeInsertPosition::Prepend
                         ? 0
                         : keyframeIndex);
  keyframe->mOffset.emplace(aOffset);
  if (aTimingFunction->mType != nsTimingFunction::Type::Linear) {
    keyframe->mTimingFunction.emplace();
    keyframe->mTimingFunction->Init(*aTimingFunction);
  }

  return keyframe;
}

Keyframe*
Gecko_GetOrCreateKeyframeAtStart(nsTArray<Keyframe>* aKeyframes,
                                 float aOffset,
                                 const nsTimingFunction* aTimingFunction)
{
  MOZ_ASSERT(aKeyframes->IsEmpty() ||
             aKeyframes->ElementAt(0).mOffset.value() >= aOffset,
             "The offset should be less than or equal to the first keyframe's "
             "offset if there are exisiting keyframes");

  return GetOrCreateKeyframe(aKeyframes,
                             aOffset,
                             aTimingFunction,
                             KeyframeSearchDirection::Forwards,
                             KeyframeInsertPosition::Prepend);
}

Keyframe*
Gecko_GetOrCreateInitialKeyframe(nsTArray<Keyframe>* aKeyframes,
                                 const nsTimingFunction* aTimingFunction)
{
  return GetOrCreateKeyframe(aKeyframes,
                             0.,
                             aTimingFunction,
                             KeyframeSearchDirection::Forwards,
                             KeyframeInsertPosition::LastForOffset);
}

Keyframe*
Gecko_GetOrCreateFinalKeyframe(nsTArray<Keyframe>* aKeyframes,
                               const nsTimingFunction* aTimingFunction)
{
  return GetOrCreateKeyframe(aKeyframes,
                             1.,
                             aTimingFunction,
                             KeyframeSearchDirection::Backwards,
                             KeyframeInsertPosition::LastForOffset);
}

PropertyValuePair*
Gecko_AppendPropertyValuePair(nsTArray<PropertyValuePair>* aProperties,
                              nsCSSPropertyID aProperty)
{
  MOZ_ASSERT(aProperties);
  return aProperties->AppendElement(PropertyValuePair {aProperty});
}

void
Gecko_ResetStyleCoord(nsStyleUnit* aUnit, nsStyleUnion* aValue)
{
  nsStyleCoord::Reset(*aUnit, *aValue);
}

void
Gecko_SetStyleCoordCalcValue(nsStyleUnit* aUnit, nsStyleUnion* aValue, nsStyleCoord::CalcValue aCalc)
{
  // Calc units should be cleaned up first
  MOZ_ASSERT(*aUnit != nsStyleUnit::eStyleUnit_Calc);
  nsStyleCoord::Calc* calcRef = new nsStyleCoord::Calc();
  calcRef->mLength = aCalc.mLength;
  calcRef->mPercent = aCalc.mPercent;
  calcRef->mHasPercent = aCalc.mHasPercent;
  *aUnit = nsStyleUnit::eStyleUnit_Calc;
  aValue->mPointer = calcRef;
  calcRef->AddRef();
}

void
Gecko_CopyShapeSourceFrom(mozilla::StyleShapeSource* aDst, const mozilla::StyleShapeSource* aSrc)
{
  MOZ_ASSERT(aDst);
  MOZ_ASSERT(aSrc);

  *aDst = *aSrc;
}

void
Gecko_DestroyShapeSource(mozilla::StyleShapeSource* aShape)
{
  aShape->~StyleShapeSource();
}

void
Gecko_StyleShapeSource_SetURLValue(mozilla::StyleShapeSource* aShape, ServoBundledURI aURI)
{
  RefPtr<css::URLValue> url = aURI.IntoCssUrl();
  aShape->SetURL(url.get());
}

mozilla::StyleBasicShape*
Gecko_NewBasicShape(mozilla::StyleBasicShapeType aType)
{
  RefPtr<StyleBasicShape> ptr = new mozilla::StyleBasicShape(aType);
  return ptr.forget().take();
}

void
Gecko_ResetFilters(nsStyleEffects* effects, size_t new_len)
{
  effects->mFilters.Clear();
  effects->mFilters.SetLength(new_len);
}

void
Gecko_CopyFiltersFrom(nsStyleEffects* aSrc, nsStyleEffects* aDest)
{
  aDest->mFilters = aSrc->mFilters;
}

void
Gecko_nsStyleFilter_SetURLValue(nsStyleFilter* aEffects, ServoBundledURI aURI)
{
  RefPtr<css::URLValue> url = aURI.IntoCssUrl();
  aEffects->SetURL(url.get());
}

void
Gecko_nsStyleSVGPaint_CopyFrom(nsStyleSVGPaint* aDest, const nsStyleSVGPaint* aSrc)
{
  *aDest = *aSrc;
}

void
Gecko_nsStyleSVGPaint_SetURLValue(nsStyleSVGPaint* aPaint, ServoBundledURI aURI)
{
  RefPtr<css::URLValue> url = aURI.IntoCssUrl();
  aPaint->SetPaintServer(url.get());
}

void Gecko_nsStyleSVGPaint_Reset(nsStyleSVGPaint* aPaint)
{
  aPaint->SetNone();
}

void
Gecko_nsStyleSVG_SetDashArrayLength(nsStyleSVG* aSvg, uint32_t aLen)
{
  aSvg->mStrokeDasharray.Clear();
  aSvg->mStrokeDasharray.SetLength(aLen);
}

void
Gecko_nsStyleSVG_CopyDashArray(nsStyleSVG* aDst, const nsStyleSVG* aSrc)
{
  aDst->mStrokeDasharray = aSrc->mStrokeDasharray;
}

void
Gecko_nsStyleSVG_SetContextPropertiesLength(nsStyleSVG* aSvg, uint32_t aLen)
{
  aSvg->mContextProps.Clear();
  aSvg->mContextProps.SetLength(aLen);
}

void
Gecko_nsStyleSVG_CopyContextProperties(nsStyleSVG* aDst, const nsStyleSVG* aSrc)
{
  aDst->mContextProps = aSrc->mContextProps;
  aDst->mContextPropsBits = aSrc->mContextPropsBits;
}


css::URLValue*
Gecko_NewURLValue(ServoBundledURI aURI)
{
  RefPtr<css::URLValue> url = aURI.IntoCssUrl();
  return url.forget().take();
}

NS_IMPL_THREADSAFE_FFI_REFCOUNTING(css::URLValue, CSSURLValue);

NS_IMPL_THREADSAFE_FFI_REFCOUNTING(URLExtraData, URLExtraData);

NS_IMPL_THREADSAFE_FFI_REFCOUNTING(nsStyleCoord::Calc, Calc);

nsCSSShadowArray*
Gecko_NewCSSShadowArray(uint32_t aLen)
{
  RefPtr<nsCSSShadowArray> arr = new(aLen) nsCSSShadowArray(aLen);
  return arr.forget().take();
}

NS_IMPL_THREADSAFE_FFI_REFCOUNTING(nsCSSShadowArray, CSSShadowArray);

nsStyleQuoteValues*
Gecko_NewStyleQuoteValues(uint32_t aLen)
{
  RefPtr<nsStyleQuoteValues> values = new nsStyleQuoteValues;
  values->mQuotePairs.SetLength(aLen);
  return values.forget().take();
}

NS_IMPL_THREADSAFE_FFI_REFCOUNTING(nsStyleQuoteValues, QuoteValues);

nsCSSValueSharedList*
Gecko_NewCSSValueSharedList(uint32_t aLen)
{
  RefPtr<nsCSSValueSharedList> list = new nsCSSValueSharedList;
  if (aLen == 0) {
    return list.forget().take();
  }

  list->mHead = new nsCSSValueList;
  nsCSSValueList* cur = list->mHead;
  for (uint32_t i = 0; i < aLen - 1; i++) {
    cur->mNext = new nsCSSValueList;
    cur = cur->mNext;
  }

  return list.forget().take();
}

nsCSSValueSharedList*
Gecko_NewNoneTransform()
{
  RefPtr<nsCSSValueSharedList> list = new nsCSSValueSharedList;
  list->mHead = new nsCSSValueList;
  list->mHead->mValue.SetNoneValue();
  return list.forget().take();
}

void
Gecko_CSSValue_SetAbsoluteLength(nsCSSValueBorrowedMut aCSSValue, nscoord aLen)
{
  MOZ_ASSERT(aCSSValue->GetUnit() == eCSSUnit_Null || aCSSValue->IsLengthUnit());
  // The call below could trigger refcounting if aCSSValue were a
  // FontFamilyList, but we just asserted that it's not. So we can
  // whitelist this for static analysis.
  aCSSValue->SetIntegerCoordValue(aLen);
}

nscoord
Gecko_CSSValue_GetAbsoluteLength(nsCSSValueBorrowed aCSSValue)
{
  // SetIntegerCoordValue() which is used in Gecko_CSSValue_SetAbsoluteLength()
  // converts values by nsPresContext::AppUnitsToFloatCSSPixels() and stores
  // values in eCSSUnit_Pixel unit. We need to convert the values back to app
  // units by GetPixelLength().
  MOZ_ASSERT(aCSSValue->GetUnit() == eCSSUnit_Pixel,
             "The unit should be eCSSUnit_Pixel");
  return aCSSValue->GetPixelLength();
}

void
Gecko_CSSValue_SetNumber(nsCSSValueBorrowedMut aCSSValue, float aNumber)
{
  aCSSValue->SetFloatValue(aNumber, eCSSUnit_Number);
}

float
Gecko_CSSValue_GetNumber(nsCSSValueBorrowed aCSSValue)
{
  return aCSSValue->GetFloatValue();
}

void
Gecko_CSSValue_SetKeyword(nsCSSValueBorrowedMut aCSSValue, nsCSSKeyword aKeyword)
{
  aCSSValue->SetEnumValue(aKeyword);
}

nsCSSKeyword
Gecko_CSSValue_GetKeyword(nsCSSValueBorrowed aCSSValue)
{
  return aCSSValue->GetKeywordValue();
}

void
Gecko_CSSValue_SetPercentage(nsCSSValueBorrowedMut aCSSValue, float aPercent)
{
  aCSSValue->SetPercentValue(aPercent);
}

float
Gecko_CSSValue_GetPercentage(nsCSSValueBorrowed aCSSValue)
{
  return aCSSValue->GetPercentValue();
}

void
Gecko_CSSValue_SetCalc(nsCSSValueBorrowedMut aCSSValue, nsStyleCoord::CalcValue aCalc)
{
  aCSSValue->SetCalcValue(&aCalc);
}

nsStyleCoord::CalcValue
Gecko_CSSValue_GetCalc(nsCSSValueBorrowed aCSSValue)
{
  return aCSSValue->GetCalcValue();
}

void
Gecko_CSSValue_SetFunction(nsCSSValueBorrowedMut aCSSValue, int32_t aLen)
{
  nsCSSValue::Array* arr = nsCSSValue::Array::Create(aLen);
  aCSSValue->SetArrayValue(arr, eCSSUnit_Function);
}

void
Gecko_CSSValue_SetString(nsCSSValueBorrowedMut aCSSValue,
                         const uint8_t* aString, uint32_t aLength,
                         nsCSSUnit aUnit)
{
  MOZ_ASSERT(aCSSValue->GetUnit() == eCSSUnit_Null);
  nsString string;
  nsDependentCSubstring slice(reinterpret_cast<const char*>(aString),
                                  aLength);
  AppendUTF8toUTF16(slice, string);
  aCSSValue->SetStringValue(string, aUnit);
}

void
Gecko_CSSValue_SetStringFromAtom(nsCSSValueBorrowedMut aCSSValue,
                                 nsIAtom* aAtom, nsCSSUnit aUnit)
{
  aCSSValue->SetStringValue(nsDependentAtomString(aAtom), aUnit);
}

void
Gecko_CSSValue_SetAtomIdent(nsCSSValueBorrowedMut aCSSValue, nsIAtom* aAtom)
{
  aCSSValue->SetAtomIdentValue(already_AddRefed<nsIAtom>(aAtom));
}

void
Gecko_CSSValue_SetArray(nsCSSValueBorrowedMut aCSSValue, int32_t aLength)
{
  MOZ_ASSERT(aCSSValue->GetUnit() == eCSSUnit_Null);
  RefPtr<nsCSSValue::Array> array
    = nsCSSValue::Array::Create(aLength);
  aCSSValue->SetArrayValue(array, eCSSUnit_Array);
}

void
Gecko_CSSValue_SetURL(nsCSSValueBorrowedMut aCSSValue,
                      ServoBundledURI aURI)
{
  MOZ_ASSERT(aCSSValue->GetUnit() == eCSSUnit_Null);
  RefPtr<css::URLValue> url = aURI.IntoCssUrl();
  aCSSValue->SetURLValue(url.get());
}

void
Gecko_CSSValue_SetInt(nsCSSValueBorrowedMut aCSSValue,
                      int32_t aInteger, nsCSSUnit aUnit)
{
  aCSSValue->SetIntValue(aInteger, aUnit);
}

nsCSSValueBorrowedMut
Gecko_CSSValue_GetArrayItem(nsCSSValueBorrowedMut aCSSValue, int32_t aIndex)
{
  return &aCSSValue->GetArrayValue()->Item(aIndex);
}

nsCSSValueBorrowed
Gecko_CSSValue_GetArrayItemConst(nsCSSValueBorrowed aCSSValue, int32_t aIndex)
{
  return &aCSSValue->GetArrayValue()->Item(aIndex);
}

void
Gecko_CSSValue_SetPair(nsCSSValueBorrowedMut aCSSValue,
                       nsCSSValueBorrowed aXValue, nsCSSValueBorrowed aYValue)
{
  MOZ_ASSERT(NS_IsMainThread());
  aCSSValue->SetPairValue(*aXValue, *aYValue);
}

void
Gecko_CSSValue_SetList(nsCSSValueBorrowedMut aCSSValue, uint32_t aLen)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCSSValueList* item = aCSSValue->SetListValue();
  for (uint32_t i = 1; i < aLen; ++i) {
    item->mNext = new nsCSSValueList;
    item = item->mNext;
  }
}

void
Gecko_CSSValue_SetPairList(nsCSSValueBorrowedMut aCSSValue, uint32_t aLen)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCSSValuePairList* item = aCSSValue->SetPairListValue();
  for (uint32_t i = 1; i < aLen; ++i) {
    item->mNext = new nsCSSValuePairList;
    item = item->mNext;
  }
}

void
Gecko_CSSValue_InitSharedList(nsCSSValueBorrowedMut aCSSValue,
                              uint32_t aLen)
{
  MOZ_ASSERT(aLen > 0, "Must create at least one nsCSSValueList (mHead)");

  nsCSSValueSharedList* list = new nsCSSValueSharedList;
  aCSSValue->SetSharedListValue(list);
  list->mHead = new nsCSSValueList;
  nsCSSValueList* cur = list->mHead;
  for (uint32_t i = 1; i < aLen; ++i) {
    cur->mNext = new nsCSSValueList;
    cur = cur->mNext;
  }
}

bool
Gecko_PropertyId_IsPrefEnabled(nsCSSPropertyID id)
{
  return nsCSSProps::IsEnabled(id);
}

void
Gecko_CSSValue_Drop(nsCSSValueBorrowedMut aCSSValue)
{
  aCSSValue->~nsCSSValue();
}

void
Gecko_nsStyleFont_SetLang(nsStyleFont* aFont, nsIAtom* aAtom)
{
  already_AddRefed<nsIAtom> atom = already_AddRefed<nsIAtom>(aAtom);
  aFont->mLanguage = atom;
  aFont->mExplicitLanguage = true;
}

void
Gecko_nsStyleFont_CopyLangFrom(nsStyleFont* aFont, const nsStyleFont* aSource)
{
  aFont->mLanguage = aSource->mLanguage;
}

void
Gecko_nsStyleFont_FixupNoneGeneric(nsStyleFont* aFont,
                                   RawGeckoPresContextBorrowed aPresContext)
{
  const nsFont* defaultVariableFont = ThreadSafeGetDefaultFontHelper(aPresContext, aFont->mLanguage,
                                                                     kPresContext_DefaultVariableFont_ID);
  nsRuleNode::FixupNoneGeneric(&aFont->mFont, aPresContext,
                               aFont->mGenericID, defaultVariableFont);
}

void
Gecko_nsStyleFont_PrefillDefaultForGeneric(nsStyleFont* aFont,
                                           RawGeckoPresContextBorrowed aPresContext,
                                           uint8_t aGenericId)
{
  const nsFont* defaultFont = ThreadSafeGetDefaultFontHelper(aPresContext, aFont->mLanguage,
                                                             aGenericId);
   aFont->mFont.fontlist = defaultFont->fontlist;
}

void
Gecko_nsStyleFont_FixupMinFontSize(nsStyleFont* aFont,
                                   RawGeckoPresContextBorrowed aPresContext)
{
  nscoord minFontSize;
  bool needsCache = false;

  {
    AutoReadLock guard(*sServoLangFontPrefsLock);
    minFontSize = aPresContext->MinFontSize(aFont->mLanguage, &needsCache);
  }

  if (needsCache) {
    AutoWriteLock guard(*sServoLangFontPrefsLock);
    minFontSize = aPresContext->MinFontSize(aFont->mLanguage, nullptr);
  }

  nsRuleNode::ApplyMinFontSize(aFont, aPresContext, minFontSize);
}

void
FontSizePrefs::CopyFrom(const LangGroupFontPrefs& prefs)
{
  mDefaultVariableSize = prefs.mDefaultVariableFont.size;
  mDefaultFixedSize = prefs.mDefaultFixedFont.size;
  mDefaultSerifSize = prefs.mDefaultSerifFont.size;
  mDefaultSansSerifSize = prefs.mDefaultSansSerifFont.size;
  mDefaultMonospaceSize = prefs.mDefaultMonospaceFont.size;
  mDefaultCursiveSize = prefs.mDefaultCursiveFont.size;
  mDefaultFantasySize = prefs.mDefaultFantasyFont.size;
}

FontSizePrefs
Gecko_GetBaseSize(nsIAtom* aLanguage)
{
  LangGroupFontPrefs prefs;
  nsCOMPtr<nsIAtom> langGroupAtom = StaticPresData::Get()->GetUncachedLangGroup(aLanguage);

  prefs.Initialize(langGroupAtom);
  FontSizePrefs sizes;
  sizes.CopyFrom(prefs);

  return sizes;
}

RawGeckoElementBorrowedOrNull
Gecko_GetBindingParent(RawGeckoElementBorrowed aElement)
{
  nsIContent* parent = aElement->GetBindingParent();
  return parent ? parent->AsElement() : nullptr;
}

RawGeckoXBLBindingBorrowedOrNull
Gecko_GetXBLBinding(RawGeckoElementBorrowed aElement)
{
  return aElement->GetXBLBinding();
}

RawServoStyleSetBorrowedOrNull
Gecko_XBLBinding_GetRawServoStyleSet(RawGeckoXBLBindingBorrowed aXBLBinding)
{
  const ServoStyleSet* set = aXBLBinding->GetServoStyleSet();
  return set ? set->RawSet() : nullptr;
}

bool
Gecko_XBLBinding_InheritsStyle(RawGeckoXBLBindingBorrowed aXBLBinding)
{
  return aXBLBinding->InheritsStyle();
}

void
InitializeServo()
{
  URLExtraData::InitDummy();
  Servo_Initialize(URLExtraData::Dummy());

  sServoFontMetricsLock = new Mutex("Gecko_GetFontMetrics");
  sServoLangFontPrefsLock = new RWLock("nsPresContext::GetDefaultFont");

  Preferences::AddBoolVarCache(&sFramesTimingFunctionEnabled,
                               "layout.css.frames-timing.enabled");
}

void
ShutdownServo()
{
  delete sServoFontMetricsLock;
  delete sServoLangFontPrefsLock;
  Servo_Shutdown();
}

namespace mozilla {

void
AssertIsMainThreadOrServoFontMetricsLocked()
{
  if (!NS_IsMainThread()) {
    MOZ_ASSERT(sServoFontMetricsLock);
    sServoFontMetricsLock->AssertCurrentThreadOwns();
  }
}

} // namespace mozilla

GeckoFontMetrics
Gecko_GetFontMetrics(RawGeckoPresContextBorrowed aPresContext,
                     bool aIsVertical,
                     const nsStyleFont* aFont,
                     nscoord aFontSize,
                     bool aUseUserFontSet)
{
  MutexAutoLock lock(*sServoFontMetricsLock);
  GeckoFontMetrics ret;

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
  RefPtr<nsFontMetrics> fm = nsRuleNode::GetMetricsFor(presContext, aIsVertical,
                                                       aFont, aFontSize,
                                                       aUseUserFontSet);
  ret.mXSize = fm->XHeight();
  gfxFloat zeroWidth = fm->GetThebesFontGroup()->GetFirstValidFont()->
                           GetMetrics(fm->Orientation()).zeroOrAveCharWidth;
  ret.mChSize = ceil(aPresContext->AppUnitsPerDevPixel() * zeroWidth);
  return ret;
}

int32_t
Gecko_GetAppUnitsPerPhysicalInch(RawGeckoPresContextBorrowed aPresContext)
{
  nsPresContext* presContext = const_cast<nsPresContext*>(aPresContext);
  return presContext->DeviceContext()->AppUnitsPerPhysicalInch();
}

ServoStyleSheet*
Gecko_LoadStyleSheet(css::Loader* aLoader,
                     ServoStyleSheet* aParent,
                     css::LoaderReusableStyleSheets* aReusableSheets,
                     RawGeckoURLExtraData* aBaseURLData,
                     const uint8_t* aURLString,
                     uint32_t aURLStringLength,
                     RawServoMediaListStrong aMediaList)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aLoader, "Should've catched this before");
  MOZ_ASSERT(aParent, "Only used for @import, so parent should exist!");
  MOZ_ASSERT(aURLString, "Invalid URLs shouldn't be loaded!");
  MOZ_ASSERT(aBaseURLData, "Need base URL data");

  RefPtr<dom::MediaList> media = new ServoMediaList(aMediaList.Consume());
  nsDependentCSubstring urlSpec(reinterpret_cast<const char*>(aURLString),
                                aURLStringLength);
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), urlSpec, nullptr,
                          aBaseURLData->BaseURI());

  StyleSheet* previousFirstChild = aParent->GetFirstChild();
  if (NS_SUCCEEDED(rv)) {
    rv = aLoader->LoadChildSheet(aParent, uri, media, nullptr, aReusableSheets);
  }

  if (NS_FAILED(rv) ||
      !aParent->GetFirstChild() ||
      aParent->GetFirstChild() == previousFirstChild) {
    // Servo and Gecko have different ideas of what a valid URL is, so we might
    // get in here with a URL string that NS_NewURI can't handle.  If so,
    // silently do nothing.  Eventually we should be able to assert that the
    // NS_NewURI succeeds, here.
    RefPtr<ServoStyleSheet> emptySheet =
      aParent->CreateEmptyChildSheet(media.forget());
    aParent->PrependStyleSheet(emptySheet);
    return emptySheet.forget().take();
  }

  RefPtr<ServoStyleSheet> sheet =
    static_cast<ServoStyleSheet*>(aParent->GetFirstChild());
  return sheet.forget().take();
}

const nsMediaFeature*
Gecko_GetMediaFeatures()
{
  return nsMediaFeatures::features;
}

nsCSSKeyword
Gecko_LookupCSSKeyword(const uint8_t* aString, uint32_t aLength)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsDependentCSubstring keyword(reinterpret_cast<const char*>(aString), aLength);
  return nsCSSKeywords::LookupKeyword(keyword);
}

const char*
Gecko_CSSKeywordString(nsCSSKeyword aKeyword, uint32_t* aLength)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aLength);
  const nsCString& value = nsCSSKeywords::GetStringValue(aKeyword);
  *aLength = value.Length();
  return value.get();
}

nsCSSFontFaceRule*
Gecko_CSSFontFaceRule_Create(uint32_t aLine, uint32_t aColumn)
{
  RefPtr<nsCSSFontFaceRule> rule = new nsCSSFontFaceRule(aLine, aColumn);
  return rule.forget().take();
}

nsCSSFontFaceRule*
Gecko_CSSFontFaceRule_Clone(const nsCSSFontFaceRule* aRule)
{
  RefPtr<css::Rule> rule = aRule->Clone();
  return static_cast<nsCSSFontFaceRule*>(rule.forget().take());
}

void
Gecko_CSSFontFaceRule_GetCssText(const nsCSSFontFaceRule* aRule,
                                 nsAString* aResult)
{
  // GetCSSText serializes nsCSSValues, which have a heap write
  // hazard when dealing with color values (nsCSSKeywords::AddRefTable)
  // We only serialize on the main thread; assert to convince the analysis
  // and prevent accidentally calling this elsewhere
  MOZ_ASSERT(NS_IsMainThread());

  aRule->GetCssText(*aResult);
}

void
Gecko_AddPropertyToSet(nsCSSPropertyIDSetBorrowedMut aPropertySet,
                       nsCSSPropertyID aProperty)
{
  aPropertySet->AddProperty(aProperty);
}

int32_t
Gecko_RegisterNamespace(nsIAtom* aNamespace)
{
  int32_t id;

  MOZ_ASSERT(NS_IsMainThread());

  nsAutoString str;
  aNamespace->ToString(str);
  nsresult rv = nsContentUtils::NameSpaceManager()->RegisterNameSpace(str, id);

  if (NS_FAILED(rv)) {
    return -1;
  }
  return id;
}

NS_IMPL_FFI_REFCOUNTING(nsCSSFontFaceRule, CSSFontFaceRule);

nsCSSCounterStyleRule*
Gecko_CSSCounterStyle_Create(nsIAtom* aName)
{
  RefPtr<nsCSSCounterStyleRule> rule = new nsCSSCounterStyleRule(aName, 0, 0);
  return rule.forget().take();
}

nsCSSCounterStyleRule*
Gecko_CSSCounterStyle_Clone(const nsCSSCounterStyleRule* aRule)
{
  RefPtr<css::Rule> rule = aRule->Clone();
  return static_cast<nsCSSCounterStyleRule*>(rule.forget().take());
}

void
Gecko_CSSCounterStyle_GetCssText(const nsCSSCounterStyleRule* aRule,
                                 nsAString* aResult)
{
  MOZ_ASSERT(NS_IsMainThread());
  aRule->GetCssText(*aResult);
}

NS_IMPL_FFI_REFCOUNTING(nsCSSCounterStyleRule, CSSCounterStyleRule);

NS_IMPL_THREADSAFE_FFI_REFCOUNTING(nsCSSValueSharedList, CSSValueSharedList);

#define STYLE_STRUCT(name, checkdata_cb)                                      \
                                                                              \
void                                                                          \
Gecko_Construct_Default_nsStyle##name(nsStyle##name* ptr,                     \
                                      const nsPresContext* pres_context)      \
{                                                                             \
  new (ptr) nsStyle##name(pres_context);                                      \
}                                                                             \
                                                                              \
void                                                                          \
Gecko_CopyConstruct_nsStyle##name(nsStyle##name* ptr,                         \
                                  const nsStyle##name* other)                 \
{                                                                             \
  new (ptr) nsStyle##name(*other);                                            \
}                                                                             \
                                                                              \
void                                                                          \
Gecko_Destroy_nsStyle##name(nsStyle##name* ptr)                               \
{                                                                             \
  ptr->~nsStyle##name();                                                      \
}

void
Gecko_Construct_nsStyleVariables(nsStyleVariables* ptr)
{
  new (ptr) nsStyleVariables();
}

void
Gecko_RegisterProfilerThread(const char* name)
{
  char stackTop;
  profiler_register_thread(name, &stackTop);
}

void
Gecko_UnregisterProfilerThread()
{
  profiler_unregister_thread();
}

bool
Gecko_DocumentRule_UseForPresentation(RawGeckoPresContextBorrowed aPresContext,
                                      const nsACString* aPattern,
                                      css::URLMatchingFunction aURLMatchingFunction)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsIDocument *doc = aPresContext->Document();
  nsIURI *docURI = doc->GetDocumentURI();
  nsAutoCString docURISpec;
  if (docURI) {
    // If GetSpec fails (due to OOM) just skip these URI-specific CSS rules.
    nsresult rv = docURI->GetSpec(docURISpec);
    NS_ENSURE_SUCCESS(rv, false);
  }

  return css::DocumentRule::UseForPresentation(doc, docURI, docURISpec,
                                               *aPattern, aURLMatchingFunction);
}

void
Gecko_SetJemallocThreadLocalArena(bool enabled)
{
#if defined(MOZ_MEMORY)
  // At this point we convert |enabled| from a plain C++ bool to a
  // |jemalloc_bool|, so be on the safe side.
  jemalloc_thread_local_arena(!!enabled);
#endif
}

#include "nsStyleStructList.h"

#undef STYLE_STRUCT

#ifndef MOZ_STYLO
#define SERVO_BINDING_FUNC(name_, return_, ...)                               \
  return_ name_(__VA_ARGS__) {                                                \
    MOZ_CRASH("stylo: shouldn't be calling " #name_ "in a non-stylo build");  \
  }
#include "ServoBindingList.h"
#undef SERVO_BINDING_FUNC
#endif

ErrorReporter*
Gecko_CreateCSSErrorReporter(ServoStyleSheet* sheet,
                             Loader* loader,
                             nsIURI* uri)
{
  MOZ_ASSERT(NS_IsMainThread());
  return new ErrorReporter(sheet, loader, uri);
}

void
Gecko_DestroyCSSErrorReporter(ErrorReporter* reporter)
{
  delete reporter;
}

void
Gecko_ReportUnexpectedCSSError(ErrorReporter* reporter,
                               const char* message,
                               const char* param,
                               uint32_t paramLen,
                               const char* source,
                               uint32_t sourceLen,
                               uint32_t lineNumber,
                               uint32_t colNumber,
                               nsIURI* uri)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsDependentCSubstring paramValue(param, paramLen);
  nsAutoString wideParam = NS_ConvertUTF8toUTF16(paramValue);
  reporter->ReportUnexpectedUnescaped(message, wideParam);
  nsDependentCSubstring sourceValue(source, sourceLen);
  reporter->OutputError(lineNumber, colNumber, sourceValue);
}

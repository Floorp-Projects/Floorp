/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoBindings.h"

#include "ChildIterator.h"
#include "GeckoProfiler.h"
#include "gfxFontFamilyList.h"
#include "nsAnimationManager.h"
#include "nsAttrValueInlines.h"
#include "nsCSSFrameConstructor.h"
#include "nsCSSProps.h"
#include "nsCSSParser.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSRuleProcessor.h"
#include "nsContentUtils.h"
#include "nsDOMTokenList.h"
#include "nsIContentInlines.h"
#include "nsIDOMNode.h"
#include "nsIDocument.h"
#include "nsIDocumentInlines.h"
#include "nsIFrame.h"
#include "nsINode.h"
#include "nsIPresShell.h"
#include "nsIPresShellInlines.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsFontMetrics.h"
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
#include "mozilla/Keyframe.h"
#include "mozilla/Mutex.h"
#include "mozilla/ServoElementSnapshot.h"
#include "mozilla/ServoRestyleManager.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/ServoMediaList.h"
#include "mozilla/ServoComputedValuesWithParent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/dom/HTMLTableCellElement.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/URLExtraData.h"

using namespace mozilla;
using namespace mozilla::dom;

#define SERVO_ARC_TYPE(name_, type_) \
  already_AddRefed<type_>            \
  type_##Strong::Consume() {         \
    RefPtr<type_> result;            \
    result.swap(mPtr);               \
    return result.forget();          \
  }
#include "mozilla/ServoArcTypeList.h"
#undef SERVO_ARC_TYPE


static Mutex* sServoFontMetricsLock = nullptr;

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
Gecko_GetParentNode(RawGeckoNodeBorrowed aNode)
{
  MOZ_ASSERT(!FlattenedTreeParentIsParent<nsIContent::eForStyle>(aNode),
             "Should have taken the inline path");
  MOZ_ASSERT(aNode->IsContent(), "Slow path only applies to content");
  const nsIContent* c = aNode->AsContent();
  return c->GetFlattenedTreeParentNodeInternal(nsIContent::eForStyle);
}

RawGeckoNodeBorrowedOrNull
Gecko_GetFirstChild(RawGeckoNodeBorrowed aNode)
{
  return aNode->GetFirstChild();
}

RawGeckoNodeBorrowedOrNull
Gecko_GetLastChild(RawGeckoNodeBorrowed aNode)
{
  return aNode->GetLastChild();
}

RawGeckoNodeBorrowedOrNull
Gecko_GetPrevSibling(RawGeckoNodeBorrowed aNode)
{
  return aNode->GetPreviousSibling();
}

RawGeckoNodeBorrowedOrNull
Gecko_GetNextSibling(RawGeckoNodeBorrowed aNode)
{
  return aNode->GetNextSibling();
}

RawGeckoElementBorrowedOrNull
Gecko_GetParentElement(RawGeckoElementBorrowed aElement)
{
  return aElement->GetFlattenedTreeParentElementForStyle();
}

RawGeckoElementBorrowedOrNull
Gecko_GetFirstChildElement(RawGeckoElementBorrowed aElement)
{
  return aElement->GetFirstElementChild();
}

RawGeckoElementBorrowedOrNull Gecko_GetLastChildElement(RawGeckoElementBorrowed aElement)
{
  return aElement->GetLastElementChild();
}

RawGeckoElementBorrowedOrNull
Gecko_GetPrevSiblingElement(RawGeckoElementBorrowed aElement)
{
  return aElement->GetPreviousElementSibling();
}

RawGeckoElementBorrowedOrNull
Gecko_GetNextSiblingElement(RawGeckoElementBorrowed aElement)
{
  return aElement->GetNextElementSibling();
}

RawGeckoElementBorrowedOrNull
Gecko_GetDocumentElement(RawGeckoDocumentBorrowed aDoc)
{
  return aDoc->GetDocumentElement();
}

StyleChildrenIteratorOwnedOrNull
Gecko_MaybeCreateStyleChildrenIterator(RawGeckoNodeBorrowed aNode)
{
  if (!aNode->IsElement()) {
    return nullptr;
  }

  const Element* el = aNode->AsElement();
  return StyleChildrenIterator::IsNeeded(el) ? new StyleChildrenIterator(el)
                                             : nullptr;
}

void
Gecko_DropStyleChildrenIterator(StyleChildrenIteratorOwned aIterator)
{
  MOZ_ASSERT(aIterator);
  delete aIterator;
}

RawGeckoNodeBorrowed
Gecko_GetNextStyleChild(StyleChildrenIteratorBorrowedMut aIterator)
{
  MOZ_ASSERT(aIterator);
  return aIterator->GetNextChild();
}

EventStates::ServoType
Gecko_ElementState(RawGeckoElementBorrowed aElement)
{
  return aElement->StyleState().ServoValue();
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
  return attr ? attr->GetAtomValue() : nullptr;
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
  nsCSSFrameConstructor* fc =
    aElement->OwnerDoc()->GetShell()->GetPresContext()->FrameConstructor();

  // NB: This is only called for CalcStyleDifference, and we handle correctly
  // the display: none case since Servo still has the older style.
  return fc->GetDisplayContentsStyleFor(aElement);
}

nsIAtom*
Gecko_GetImplementedPseudo(RawGeckoElementBorrowed aElement)
{
  CSSPseudoElementType pseudo = aElement->GetPseudoElementType();
  if (pseudo == CSSPseudoElementType::NotPseudo)
    return nullptr;
  return nsCSSPseudoElements::GetPseudoAtom(pseudo);
}

nsChangeHint
Gecko_CalcStyleDifference(nsStyleContext* aOldStyleContext,
                          ServoComputedValuesBorrowed aComputedValues)
{
  MOZ_ASSERT(aOldStyleContext);
  MOZ_ASSERT(aComputedValues);

  // Eventually, we should compute things out of these flags like
  // ElementRestyler::RestyleSelf does and pass the result to the caller to
  // potentially halt traversal. See bug 1289868.
  uint32_t equalStructs, samePointerStructs;
  nsChangeHint result =
    aOldStyleContext->CalcStyleDifference(aComputedValues,
                                          &equalStructs,
                                          &samePointerStructs);

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

RawServoDeclarationBlockStrongBorrowedOrNull
Gecko_GetHTMLPresentationAttrDeclarationBlock(RawGeckoElementBorrowed aElement)
{
  static_assert(sizeof(RefPtr<RawServoDeclarationBlock>) ==
                sizeof(RawServoDeclarationBlockStrong),
                "RefPtr should just be a pointer");
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

  const RefPtr<RawServoDeclarationBlock>& servo = attrs->GetServoStyle();
  return reinterpret_cast<const RawServoDeclarationBlockStrong*>(&servo);
}

RawServoDeclarationBlockStrongBorrowedOrNull
Gecko_GetExtraContentStyleDeclarations(RawGeckoElementBorrowed aElement)
{
  static_assert(sizeof(RefPtr<RawServoDeclarationBlock>) ==
                sizeof(RawServoDeclarationBlockStrong),
                "RefPtr should just be a pointer");
  if (!aElement->IsAnyOfHTMLElements(nsGkAtoms::td, nsGkAtoms::th)) {
    return nullptr;
  }
  const HTMLTableCellElement* cell = static_cast<const HTMLTableCellElement*>(aElement);
  if (nsMappedAttributes* attrs = cell->GetMappedAttributesInheritedFromTable()) {
    const RefPtr<RawServoDeclarationBlock>& servo = attrs->GetServoStyle();
    return reinterpret_cast<const RawServoDeclarationBlockStrong*>(&servo);
  }
  return nullptr;
}

static nsIAtom*
PseudoTagAndCorrectElementForAnimation(const Element*& aElementOrPseudo) {
  if (aElementOrPseudo->IsGeneratedContentContainerForBefore()) {
    aElementOrPseudo = aElementOrPseudo->GetParent()->AsElement();
    return nsCSSPseudoElements::before;
  }

  if (aElementOrPseudo->IsGeneratedContentContainerForAfter()) {
    aElementOrPseudo = aElementOrPseudo->GetParent()->AsElement();
    return nsCSSPseudoElements::after;
  }

  return nullptr;
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

  nsIAtom* pseudoTag = PseudoTagAndCorrectElementForAnimation(aElement);

  CSSPseudoElementType pseudoType =
    nsCSSPseudoElements::GetPseudoType(
      pseudoTag,
      nsCSSProps::EnabledState::eIgnoreEnabledState);

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
                       ServoComputedValuesBorrowedOrNull aOldComputedValues,
                       ServoComputedValuesBorrowedOrNull aComputedValues,
                       ServoComputedValuesBorrowedOrNull aParentComputedValues,
                       UpdateAnimationsTasks aTasks)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aElement);

  nsPresContext* presContext = nsContentUtils::GetContextForContent(aElement);
  if (!presContext) {
    return;
  }

  nsIAtom* pseudoTag = PseudoTagAndCorrectElementForAnimation(aElement);
  if (presContext->IsDynamic() && aElement->IsInComposedDoc()) {
    const ServoComputedValuesWithParent servoValues =
      { aComputedValues, aParentComputedValues };
    CSSPseudoElementType pseudoType =
      nsCSSPseudoElements::GetPseudoType(pseudoTag,
                                         CSSEnabledState::eForAllContent);

    if (aTasks & UpdateAnimationsTasks::CSSAnimations) {
      presContext->AnimationManager()->
        UpdateAnimations(const_cast<dom::Element*>(aElement), pseudoType,
                         servoValues);
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
      const ServoComputedValuesWithParent oldServoValues =
        { aOldComputedValues, nullptr };
      presContext->TransitionManager()->
        UpdateTransitions(const_cast<dom::Element*>(aElement), pseudoType,
                          oldServoValues, servoValues);
    }
    if (aTasks & UpdateAnimationsTasks::EffectProperties) {
      presContext->EffectCompositor()->UpdateEffectProperties(
        servoValues, const_cast<dom::Element*>(aElement), pseudoType);
    }
  }
}

bool
Gecko_ElementHasAnimations(RawGeckoElementBorrowed aElement)
{
  nsIAtom* pseudoTag = PseudoTagAndCorrectElementForAnimation(aElement);
  CSSPseudoElementType pseudoType =
    nsCSSPseudoElements::GetPseudoType(pseudoTag,
                                       CSSEnabledState::eForAllContent);

  return !!EffectSet::GetEffectSet(aElement, pseudoType);
}

bool
Gecko_ElementHasCSSAnimations(RawGeckoElementBorrowed aElement)
{
  nsIAtom* pseudoTag = PseudoTagAndCorrectElementForAnimation(aElement);
  nsAnimationManager::CSSAnimationCollection* collection =
    nsAnimationManager::CSSAnimationCollection
                      ::GetAnimationCollection(aElement, pseudoTag);

  return collection && !collection->mAnimations.IsEmpty();
}

bool
Gecko_ElementHasCSSTransitions(RawGeckoElementBorrowed aElement)
{
  nsIAtom* pseudoTag = PseudoTagAndCorrectElementForAnimation(aElement);
  nsTransitionManager::CSSTransitionCollection* collection =
    nsTransitionManager::CSSTransitionCollection
                       ::GetAnimationCollection(aElement, pseudoTag);

  return collection && !collection->mAnimations.IsEmpty();
}

size_t
Gecko_ElementTransitions_Length(RawGeckoElementBorrowed aElement)
{
  nsIAtom* pseudoTag = PseudoTagAndCorrectElementForAnimation(aElement);
  nsTransitionManager::CSSTransitionCollection* collection =
    nsTransitionManager::CSSTransitionCollection
                       ::GetAnimationCollection(aElement, pseudoTag);

  return collection ? collection->mAnimations.Length() : 0;
}

static CSSTransition*
GetCurrentTransitionAt(RawGeckoElementBorrowed aElement, size_t aIndex)
{
  nsIAtom* pseudoTag = PseudoTagAndCorrectElementForAnimation(aElement);
  nsTransitionManager::CSSTransitionCollection* collection =
    nsTransitionManager::CSSTransitionCollection
                       ::GetAnimationCollection(aElement, pseudoTag);
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

nsIAtom*
Gecko_GetXMLLangValue(RawGeckoElementBorrowed aElement)
{
  nsString string;
  if (aElement->GetAttr(kNameSpaceID_XML, nsGkAtoms::lang, string)) {
    return NS_Atomize(string).take();
  }
  return nullptr;
}

template <typename Implementor>
static nsIAtom*
AtomAttrValue(Implementor* aElement, nsIAtom* aName)
{
  const nsAttrValue* attr = aElement->GetParsedAttr(aName);
  return attr ? attr->GetAtomValue() : nullptr;
}

template <typename Implementor, typename MatchFn>
static bool
DoMatch(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName, MatchFn aMatch)
{
  if (aNS) {
    int32_t ns = nsContentUtils::NameSpaceManager()->GetNameSpaceID(aNS,
                                                                    aElement->IsInChromeDocument());
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

template <typename Implementor>
static bool
AttrDashEquals(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName,
               nsIAtom* aStr)
{
  auto match = [aStr](const nsAttrValue* aValue) {
    nsAutoString str;
    aValue->ToString(str);
    const nsDefaultStringComparator c;
    return nsStyleUtil::DashMatchCompare(str, nsDependentAtomString(aStr), c);
  };
  return DoMatch(aElement, aNS, aName, match);
}

template <typename Implementor>
static bool
AttrIncludes(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName,
             nsIAtom* aStr)
{
  auto match = [aStr](const nsAttrValue* aValue) {
    nsAutoString str;
    aValue->ToString(str);
    const nsDefaultStringComparator c;
    return nsStyleUtil::ValueIncludes(str, nsDependentAtomString(aStr), c);
  };
  return DoMatch(aElement, aNS, aName, match);
}

template <typename Implementor>
static bool
AttrHasSubstring(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName,
                 nsIAtom* aStr)
{
  auto match = [aStr](const nsAttrValue* aValue) {
    nsAutoString str;
    aValue->ToString(str);
    return FindInReadable(str, nsDependentAtomString(aStr));
  };
  return DoMatch(aElement, aNS, aName, match);
}

template <typename Implementor>
static bool
AttrHasPrefix(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName,
              nsIAtom* aStr)
{
  auto match = [aStr](const nsAttrValue* aValue) {
    nsAutoString str;
    aValue->ToString(str);
    return StringBeginsWith(str, nsDependentAtomString(aStr));
  };
  return DoMatch(aElement, aNS, aName, match);
}

template <typename Implementor>
static bool
AttrHasSuffix(Implementor* aElement, nsIAtom* aNS, nsIAtom* aName,
              nsIAtom* aStr)
{
  auto match = [aStr](const nsAttrValue* aValue) {
    nsAutoString str;
    aValue->ToString(str);
    return StringEndsWith(str, nsDependentAtomString(aStr));
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
  const nsAttrValue* attr = aElement->GetParsedAttr(nsGkAtoms::_class);
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

#define SERVO_IMPL_ELEMENT_ATTR_MATCHING_FUNCTIONS(prefix_, implementor_)      \
  nsIAtom* prefix_##AtomAttrValue(implementor_ aElement, nsIAtom* aName)       \
  {                                                                            \
    return AtomAttrValue(aElement, aName);                                     \
  }                                                                            \
  bool prefix_##HasAttr(implementor_ aElement, nsIAtom* aNS, nsIAtom* aName)   \
  {                                                                            \
    return HasAttr(aElement, aNS, aName);                                      \
  }                                                                            \
  bool prefix_##AttrEquals(implementor_ aElement, nsIAtom* aNS,                \
                           nsIAtom* aName, nsIAtom* aStr, bool aIgnoreCase)    \
  {                                                                            \
    return AttrEquals(aElement, aNS, aName, aStr, aIgnoreCase);                \
  }                                                                            \
  bool prefix_##AttrDashEquals(implementor_ aElement, nsIAtom* aNS,            \
                               nsIAtom* aName, nsIAtom* aStr)                  \
  {                                                                            \
    return AttrDashEquals(aElement, aNS, aName, aStr);                         \
  }                                                                            \
  bool prefix_##AttrIncludes(implementor_ aElement, nsIAtom* aNS,              \
                             nsIAtom* aName, nsIAtom* aStr)                    \
  {                                                                            \
    return AttrIncludes(aElement, aNS, aName, aStr);                           \
  }                                                                            \
  bool prefix_##AttrHasSubstring(implementor_ aElement, nsIAtom* aNS,          \
                                 nsIAtom* aName, nsIAtom* aStr)                \
  {                                                                            \
    return AttrHasSubstring(aElement, aNS, aName, aStr);                       \
  }                                                                            \
  bool prefix_##AttrHasPrefix(implementor_ aElement, nsIAtom* aNS,             \
                              nsIAtom* aName, nsIAtom* aStr)                   \
  {                                                                            \
    return AttrHasPrefix(aElement, aNS, aName, aStr);                          \
  }                                                                            \
  bool prefix_##AttrHasSuffix(implementor_ aElement, nsIAtom* aNS,             \
                              nsIAtom* aName, nsIAtom* aStr)                   \
  {                                                                            \
    return AttrHasSuffix(aElement, aNS, aName, aStr);                          \
  }                                                                            \
  uint32_t prefix_##ClassOrClassList(implementor_ aElement, nsIAtom** aClass,  \
                                     nsIAtom*** aClassList)                    \
  {                                                                            \
    return ClassOrClassList(aElement, aClass, aClassList);                     \
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
  MutexAutoLock lock(*sServoFontMetricsLock);
  const nsFont* defaultVariableFont =
    aPresContext->GetDefaultFont(kPresContext_DefaultVariableFont_ID,
                                 aFont->mLanguage);

  // We have passed uninitialized memory to this function,
  // initialize it. We can't simply return an nsFont because then
  // we need to know its size beforehand. Servo cannot initialize nsFont
  // itself, so this will do.
  nsFont* system = new (aDest) nsFont(*defaultVariableFont);

  MOZ_RELEASE_ASSERT(system);

  *aDest = *defaultVariableFont;
  LookAndFeel::FontID fontID = static_cast<LookAndFeel::FontID>(aFontId);
  nsRuleNode::ComputeSystemFont(aDest, fontID, aPresContext, defaultVariableFont);
}

void
Gecko_nsFont_Destroy(nsFont* aDest)
{
  aDest->~nsFont();
}


void
Gecko_SetImageOrientation(nsStyleVisibility* aVisibility,
                          double aRadians, bool aFlip)
{
  aVisibility->mImageOrientation =
    nsStyleImageOrientation::CreateAsAngleAndFlip(aRadians, aFlip);
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
Gecko_SetListStyleType(nsStyleList* style_struct, uint32_t type)
{
  // Builtin counter styles are static and use no-op refcounting, and thus are
  // safe to use off-main-thread.
  style_struct->SetCounterStyle(CounterStyleManager::GetBuiltinStyle(type));
}

void
Gecko_CopyListStyleTypeFrom(nsStyleList* dst, const nsStyleList* src)
{
  dst->SetCounterStyle(src->GetCounterStyle());
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

static already_AddRefed<nsStyleImageRequest>
CreateStyleImageRequest(nsStyleImageRequest::Mode aModeFlags,
                        ServoBundledURI aURI)
{
  MOZ_ASSERT(aURI.mURLString);
  MOZ_ASSERT(aURI.mExtraData->GetReferrer());
  MOZ_ASSERT(aURI.mExtraData->GetPrincipal());

  NS_ConvertUTF8toUTF16 url(reinterpret_cast<const char*>(aURI.mURLString),
                            aURI.mURLStringLength);

  RefPtr<nsStyleImageRequest> req =
    new nsStyleImageRequest(aModeFlags, url, do_AddRef(aURI.mExtraData));
  return req.forget();
}

NS_IMPL_THREADSAFE_FFI_REFCOUNTING(mozilla::css::ImageValue, ImageValue);

void
Gecko_SetUrlImageValue(nsStyleImage* aImage, ServoBundledURI aURI)
{
  RefPtr<nsStyleImageRequest> req =
    CreateStyleImageRequest(nsStyleImageRequest::Mode::Track, aURI);
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
Gecko_SetCursorImage(nsCursorImage* aCursor, ServoBundledURI aURI)
{
  aCursor->mImage =
    CreateStyleImageRequest(nsStyleImageRequest::Mode::Discard, aURI);
}

void
Gecko_CopyCursorArrayFrom(nsStyleUserInterface* aDest,
                          const nsStyleUserInterface* aSrc)
{
  aDest->mCursorImages = aSrc->mCursorImages;
}

void
Gecko_SetContentDataImage(nsStyleContentData* aContent, ServoBundledURI aURI)
{
  RefPtr<nsStyleImageRequest> req = CreateStyleImageRequest(nsStyleImageRequest::Mode::Track, aURI);
  aContent->SetImageRequest(req.forget());
}

void
Gecko_SetContentDataArray(nsStyleContentData* aContent,
                          nsStyleContentType aType, uint32_t aLen)
{
  nsCSSValue::Array* arr = nsCSSValue::Array::Create(aLen);
  aContent->SetCounters(aType, arr);
}

nsStyleGradient*
Gecko_CreateGradient(uint8_t aShape,
                     uint8_t aSize,
                     bool aRepeating,
                     bool aLegacySyntax,
                     uint32_t aStopCount)
{
  nsStyleGradient* result = new nsStyleGradient();

  result->mShape = aShape;
  result->mSize = aSize;
  result->mRepeating = aRepeating;
  result->mLegacySyntax = aLegacySyntax;

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

void
Gecko_SetListStyleImageNone(nsStyleList* aList)
{
  aList->mListStyleImage = nullptr;
}

void
Gecko_SetListStyleImage(nsStyleList* aList,
                        ServoBundledURI aURI)
{
  aList->mListStyleImage =
    CreateStyleImageRequest(nsStyleImageRequest::Mode(0), aURI);
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

Keyframe*
Gecko_AnimationAppendKeyframe(RawGeckoKeyframeListBorrowedMut aKeyframes,
                              float aOffset,
                              const nsTimingFunction* aTimingFunction)
{
  Keyframe* keyframe = aKeyframes->AppendElement();
  keyframe->mOffset.emplace(aOffset);
  if (aTimingFunction &&
      aTimingFunction->mType != nsTimingFunction::Type::Linear) {
    keyframe->mTimingFunction.emplace();
    keyframe->mTimingFunction->Init(*aTimingFunction);
  }

  return keyframe;
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
Gecko_CSSValue_SetNormal(nsCSSValueBorrowedMut aCSSValue)
{
  aCSSValue->SetNormalValue();
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
  const nsFont* defaultVariableFont =
    aPresContext->GetDefaultFont(kPresContext_DefaultVariableFont_ID,
                                 aFont->mLanguage);
  nsRuleNode::FixupNoneGeneric(&aFont->mFont, aPresContext,
                               aFont->mGenericID, defaultVariableFont);
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

void
InitializeServo()
{
  URLExtraData::InitDummy();
  Servo_Initialize(URLExtraData::Dummy());

  sServoFontMetricsLock = new Mutex("Gecko_GetFontMetrics");
}

void
ShutdownServo()
{
  delete sServoFontMetricsLock;
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

void
Gecko_LoadStyleSheet(css::Loader* aLoader,
                     ServoStyleSheet* aParent,
                     RawServoStyleSheetBorrowed aChildSheet,
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

  if (NS_FAILED(rv)) {
    // Servo and Gecko have different ideas of what a valid URL is, so we might
    // get in here with a URL string that NS_NewURI can't handle.  If so,
    // silently do nothing.  Eventually we should be able to assert that the
    // NS_NewURI succeeds, here.
    return;
  }

  aLoader->LoadChildSheet(aParent, uri, media, nullptr, aChildSheet, nullptr);
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
  const nsAFlatCString& value = nsCSSKeywords::GetStringValue(aKeyword);
  *aLength = value.Length();
  return value.get();
}

nsCSSFontFaceRule*
Gecko_CSSFontFaceRule_Create()
{
  RefPtr<nsCSSFontFaceRule> rule = new nsCSSFontFaceRule(0, 0);
  return rule.forget().take();
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

NS_IMPL_FFI_REFCOUNTING(nsCSSFontFaceRule, CSSFontFaceRule);

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

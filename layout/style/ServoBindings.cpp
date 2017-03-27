/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoBindings.h"

#include "ChildIterator.h"
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
#include "nsIFrame.h"
#include "nsINode.h"
#include "nsIPresShell.h"
#include "nsIPresShellInlines.h"
#include "nsIPrincipal.h"
#include "nsMappedAttributes.h"
#include "nsMediaFeatures.h"
#include "nsNameSpaceManager.h"
#include "nsNetUtil.h"
#include "nsRuleNode.h"
#include "nsString.h"
#include "nsStyleStruct.h"
#include "nsStyleUtil.h"
#include "nsTArray.h"

#include "mozilla/EffectCompositor.h"
#include "mozilla/EventStates.h"
#include "mozilla/Keyframe.h"
#include "mozilla/ServoElementSnapshot.h"
#include "mozilla/ServoRestyleManager.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/DeclarationBlockInlines.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/LookAndFeel.h"

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

#ifdef DEBUG
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
Gecko_IsLink(RawGeckoElementBorrowed aElement)
{
  return nsCSSRuleProcessor::IsLink(aElement);
}

bool
Gecko_IsTextNode(RawGeckoNodeBorrowed aNode)
{
  return aNode->NodeInfo()->NodeType() == nsIDOMNode::TEXT_NODE;
}

bool
Gecko_IsVisitedLink(RawGeckoElementBorrowed aElement)
{
  return aElement->StyleState().HasState(NS_EVENT_STATE_VISITED);
}

bool
Gecko_IsUnvisitedLink(RawGeckoElementBorrowed aElement)
{
  return aElement->StyleState().HasState(NS_EVENT_STATE_UNVISITED);
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
    shell->SetNeedStyleFlush();
  }
}

nsStyleContext*
Gecko_GetStyleContext(RawGeckoNodeBorrowed aNode, nsIAtom* aPseudoTagOrNull)
{
  MOZ_ASSERT(aNode->IsContent());
  nsIFrame* relevantFrame =
    ServoRestyleManager::FrameForPseudoElement(aNode->AsContent(),
                                               aPseudoTagOrNull);
  if (relevantFrame) {
    return relevantFrame->StyleContext();
  }

  if (aPseudoTagOrNull) {
    return nullptr;
  }

  // FIXME(emilio): Is there a shorter path?
  nsCSSFrameConstructor* fc =
    aNode->OwnerDoc()->GetShell()->GetPresContext()->FrameConstructor();

  // NB: This is only called for CalcStyleDifference, and we handle correctly
  // the display: none case since Servo still has the older style.
  return fc->GetDisplayContentsStyleFor(aNode->AsContent());
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

ServoElementSnapshotOwned
Gecko_CreateElementSnapshot(RawGeckoElementBorrowed aElement)
{
  MOZ_ASSERT(NS_IsMainThread());
  return new ServoElementSnapshot(aElement);
}

void
Gecko_DropElementSnapshot(ServoElementSnapshotOwned aSnapshot)
{
  // Proxy deletes have a lot of overhead, so Servo tries hard to only drop
  // snapshots on the main thread. However, there are certain cases where
  // it's unavoidable (i.e. synchronously dropping the style data for the
  // descendants of a new display:none root).
  if (MOZ_UNLIKELY(!NS_IsMainThread())) {
    nsCOMPtr<nsIRunnable> task = NS_NewRunnableFunction([=]() { delete aSnapshot; });
    SystemGroup::Dispatch("Gecko_DropElementSnapshot", TaskCategory::Other,
                          task.forget());
  } else {
    delete aSnapshot;
  }
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

bool
Gecko_GetAnimationRule(RawGeckoElementBorrowed aElement,
                       nsIAtom* aPseudoTag,
                       EffectCompositor::CascadeLevel aCascadeLevel,
                       RawServoAnimationValueMapBorrowed aAnimationValues)
{
  MOZ_ASSERT(aElement, "Invalid GeckoElement");
  MOZ_ASSERT(!aPseudoTag ||
             aPseudoTag == nsCSSPseudoElements::before ||
             aPseudoTag == nsCSSPseudoElements::after);

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
    nsCSSPseudoElements::GetPseudoType(
      aPseudoTag,
      nsCSSProps::EnabledState::eIgnoreEnabledState);

  return presContext->EffectCompositor()
    ->GetServoAnimationRule(aElement, pseudoType,
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
                       nsIAtom* aPseudoTagOrNull,
                       ServoComputedValuesBorrowedOrNull aComputedValues,
                       ServoComputedValuesBorrowedOrNull aParentComputedValues)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aElement);

  nsPresContext* presContext = nsContentUtils::GetContextForContent(aElement);
  if (!presContext) {
    return;
  }

  if (presContext->IsDynamic() && aElement->IsInComposedDoc()) {
    presContext->AnimationManager()->
      UpdateAnimations(const_cast<dom::Element*>(aElement), aPseudoTagOrNull,
                       aComputedValues, aParentComputedValues);
  }
}

bool
Gecko_ElementHasCSSAnimations(RawGeckoElementBorrowed aElement,
                              nsIAtom* aPseudoTagOrNull)
{
  if (aPseudoTagOrNull &&
      aPseudoTagOrNull != nsCSSPseudoElements::before &&
      aPseudoTagOrNull != nsCSSPseudoElements::after) {
    return false;
  }

  CSSPseudoElementType pseudoType =
    nsCSSPseudoElements::GetPseudoType(aPseudoTagOrNull,
                                       CSSEnabledState::eForAllContent);
  nsAnimationManager::CSSAnimationCollection* collection =
    nsAnimationManager::CSSAnimationCollection
                      ::GetAnimationCollection(aElement, pseudoType);

  return collection && !collection->mAnimations.IsEmpty();
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
                                                 dummyMask, false, aSetSlowSelectorFlag, nullptr);
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
Gecko_FontFamilyList_Clear(FontFamilyList* aList) {
  aList->Clear();
}

void
Gecko_FontFamilyList_AppendNamed(FontFamilyList* aList, nsIAtom* aName)
{
  // Servo doesn't record whether the name was quoted or unquoted, so just
  // assume unquoted for now.
  FontFamilyName family;
  aName->ToString(family.mName);
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

NS_IMPL_HOLDER_FFI_REFCOUNTING(nsIPrincipal, Principal)
NS_IMPL_HOLDER_FFI_REFCOUNTING(nsIURI, URI)

already_AddRefed<css::URLValue>
ServoBundledURI::IntoCssUrl()
{
  if (!mURLString) {
    return nullptr;
  }

  MOZ_ASSERT(mBaseURI);
  MOZ_ASSERT(mReferrer);
  MOZ_ASSERT(mPrincipal);

  nsString url;
  nsDependentCSubstring urlString(reinterpret_cast<const char*>(mURLString),
                                  mURLStringLength);
  AppendUTF8toUTF16(urlString, url);
  RefPtr<nsStringBuffer> urlBuffer = nsCSSValue::BufferFromString(url);

  RefPtr<css::URLValue> urlValue = new css::URLValue(urlBuffer,
                                                     do_AddRef(mBaseURI),
                                                     do_AddRef(mReferrer),
                                                     do_AddRef(mPrincipal));
  return urlValue.forget();
}

GeckoParserExtraData::GeckoParserExtraData(nsIURI* aBaseURI,
                                           nsIURI* aReferrer,
                                           nsIPrincipal* aPrincipal)
    : mBaseURI(new ThreadSafeURIHolder(aBaseURI)),
      mReferrer(new ThreadSafeURIHolder(aReferrer)),
      mPrincipal(new ThreadSafePrincipalHolder(aPrincipal))
{
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
  MOZ_ASSERT(aURI.mBaseURI);
  MOZ_ASSERT(aURI.mReferrer);
  MOZ_ASSERT(aURI.mPrincipal);

  nsString url;
  nsDependentCSubstring urlString(reinterpret_cast<const char*>(aURI.mURLString),
                                  aURI.mURLStringLength);
  AppendUTF8toUTF16(urlString, url);
  RefPtr<nsStringBuffer> urlBuffer = nsCSSValue::BufferFromString(url);

  RefPtr<nsStyleImageRequest> req =
    new nsStyleImageRequest(aModeFlags, urlBuffer, do_AddRef(aURI.mBaseURI),
                            do_AddRef(aURI.mReferrer), do_AddRef(aURI.mPrincipal));
  return req.forget();
}

void
Gecko_SetUrlImageValue(nsStyleImage* aImage, ServoBundledURI aURI)
{
  RefPtr<nsStyleImageRequest> req =
    CreateStyleImageRequest(nsStyleImageRequest::Mode::Track, aURI);
  aImage->SetImageRequest(req.forget());
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
  nsStyleSides cropRect;
  aImage->SetCropRect(MakeUnique<nsStyleSides>(cropRect));
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
  nsCSSValue::ThreadSafeArray* arr = nsCSSValue::ThreadSafeArray::Create(aLen);
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
    reinterpret_cast<nsStyleAutoArray<StyleAnimation>*>(aArray);

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
Gecko_CopyClipPathValueFrom(mozilla::StyleShapeSource* aDst, const mozilla::StyleShapeSource* aSrc)
{
  MOZ_ASSERT(aDst);
  MOZ_ASSERT(aSrc);

  *aDst = *aSrc;
}

void
Gecko_DestroyClipPath(mozilla::StyleShapeSource* aClip)
{
  aClip->~StyleShapeSource();
}

void
Gecko_StyleClipPath_SetURLValue(mozilla::StyleShapeSource* aClip, ServoBundledURI aURI)
{
  RefPtr<css::URLValue> url = aURI.IntoCssUrl();
  aClip->SetURL(url.get());
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
  aPaint->SetPaintServer(url.get(), NS_RGB(0, 0, 0));
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
Gecko_CSSValue_SetAngle(nsCSSValueBorrowedMut aCSSValue, float aRadians)
{
  aCSSValue->SetFloatValue(aRadians, eCSSUnit_Radian);
}

float
Gecko_CSSValue_GetAngle(nsCSSValueBorrowed aCSSValue)
{
  // Unfortunately nsCSSValue.GetAngleValueInRadians() returns double,
  // so we use GetAngleValue() instead.
  MOZ_ASSERT(aCSSValue->GetUnit() == eCSSUnit_Radian,
             "The unit should be eCSSUnit_Radian");
  return aCSSValue->GetAngleValue();
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
Gecko_CSSValue_SetString(nsCSSValueBorrowedMut aCSSValue, const uint8_t* aString, uint32_t aLength)
{
  MOZ_ASSERT(aCSSValue->GetUnit() == eCSSUnit_Null);
  nsString string;
  nsDependentCSubstring slice(reinterpret_cast<const char*>(aString),
                                  aLength);
  AppendUTF8toUTF16(slice, string);
  aCSSValue->SetStringValue(string, eCSSUnit_String);
}

void
Gecko_CSSValue_SetIdent(nsCSSValueBorrowedMut aCSSValue, const uint8_t* aString, uint32_t aLength)
{
  MOZ_ASSERT(aCSSValue->GetUnit() == eCSSUnit_Null);
  nsString string;
  nsDependentCSubstring slice(reinterpret_cast<const char*>(aString),
                                  aLength);
  AppendUTF8toUTF16(slice, string);
  aCSSValue->SetStringValue(string, eCSSUnit_Ident);
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
Gecko_CSSValue_SetLocal(nsCSSValueBorrowedMut aCSSValue, const nsString aFamily)
{
  MOZ_ASSERT(aCSSValue->GetUnit() == eCSSUnit_Null);
  aCSSValue->SetStringValue(aFamily, eCSSUnit_Local_Font);
}

void
Gecko_CSSValue_SetInteger(nsCSSValueBorrowedMut aCSSValue, int32_t aInteger)
{
  MOZ_ASSERT(aCSSValue->GetUnit() == eCSSUnit_Null ||
    aCSSValue->GetUnit() == eCSSUnit_Integer);
  aCSSValue->SetIntValue(aInteger, eCSSUnit_Integer);
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

nscoord
Gecko_nsStyleFont_GetBaseSize(const nsStyleFont* aFont, RawGeckoPresContextBorrowed aPresContext)
{
  return aPresContext->GetDefaultFont(aFont->mGenericID, aFont->mLanguage)->size;
}

void
Gecko_LoadStyleSheet(css::Loader* aLoader,
                     ServoStyleSheet* aParent,
                     RawServoImportRuleBorrowed aImportRule,
                     nsIURI* aBaseURI,
                     const uint8_t* aURLString,
                     uint32_t aURLStringLength,
                     const uint8_t* aMediaString,
                     uint32_t aMediaStringLength)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aLoader, "Should've catched this before");
  MOZ_ASSERT(aParent, "Only used for @import, so parent should exist!");
  MOZ_ASSERT(aURLString, "Invalid URLs shouldn't be loaded!");
  MOZ_ASSERT(aBaseURI, "Need a base URI");
  RefPtr<nsMediaList> media = new nsMediaList();
  if (aMediaStringLength) {
    MOZ_ASSERT(aMediaString);
    // TODO(emilio, bug 1325878): This is not great, though this is going away
    // soon anyway, when we can have a Servo-backed nsMediaList.
    nsDependentCSubstring medium(reinterpret_cast<const char*>(aMediaString),
                                 aMediaStringLength);
    nsCSSParser mediumParser(aLoader);
    mediumParser.ParseMediaList(
        NS_ConvertUTF8toUTF16(medium), nullptr, 0, media);
  }

  nsDependentCSubstring urlSpec(reinterpret_cast<const char*>(aURLString),
                                aURLStringLength);
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), urlSpec, nullptr, aBaseURI);

  if (NS_FAILED(rv)) {
    // Servo and Gecko have different ideas of what a valid URL is, so we might
    // get in here with a URL string that NS_NewURI can't handle.  If so,
    // silently do nothing.  Eventually we should be able to assert that the
    // NS_NewURI succeeds, here.
    return;
  }

  aLoader->LoadChildSheet(aParent, uri, media, nullptr, aImportRule, nullptr);
}

const nsMediaFeature*
Gecko_GetMediaFeatures()
{
  return nsMediaFeatures::features;
}

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

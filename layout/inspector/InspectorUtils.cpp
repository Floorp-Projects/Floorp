/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/EventStates.h"

#include "inLayoutUtils.h"

#include "nsArray.h"
#include "nsAutoPtr.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsIContentInlines.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIDOMWindow.h"
#include "nsXBLBinding.h"
#include "nsXBLPrototypeBinding.h"
#include "nsIMutableArray.h"
#include "nsBindingManager.h"
#include "ChildIterator.h"
#include "nsComputedDOMStyle.h"
#include "mozilla/EventStateManager.h"
#include "nsAtom.h"
#include "nsRange.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/dom/CharacterData.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/CSSLexer.h"
#include "mozilla/dom/InspectorUtilsBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsCSSProps.h"
#include "nsCSSValue.h"
#include "nsColor.h"
#include "mozilla/ServoStyleSet.h"
#include "nsStyleUtil.h"
#include "nsQueryObject.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoStyleRule.h"
#include "mozilla/ServoStyleRuleMap.h"
#include "mozilla/ServoCSSParser.h"
#include "mozilla/dom/InspectorUtils.h"
#include "mozilla/dom/InspectorFontFace.h"

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;

extern const char* const kCSSRawProperties[];

namespace mozilla {
namespace dom {

/* static */ void
InspectorUtils::GetAllStyleSheets(GlobalObject& aGlobalObject,
                                  nsIDocument& aDocument,
                                  bool aDocumentOnly,
                                  nsTArray<RefPtr<StyleSheet>>& aResult)
{
  // Get the agent, then user and finally xbl sheets in the style set.
  nsIPresShell* presShell = aDocument.GetShell();

  if (presShell) {
    ServoStyleSet* styleSet = presShell->StyleSet();

    if (!aDocumentOnly) {
      SheetType sheetType = SheetType::Agent;
      for (int32_t i = 0; i < styleSet->SheetCount(sheetType); i++) {
        aResult.AppendElement(styleSet->StyleSheetAt(sheetType, i));
      }
      sheetType = SheetType::User;
      for (int32_t i = 0; i < styleSet->SheetCount(sheetType); i++) {
        aResult.AppendElement(styleSet->StyleSheetAt(sheetType, i));
      }
    }

    AutoTArray<StyleSheet*, 32> xblSheetArray;
    styleSet->AppendAllNonDocumentAuthorSheets(xblSheetArray);

    // The XBL stylesheet array will quite often be full of duplicates. Cope:
    //
    // FIXME(emilio, bug 1454467): I think this is not true since bug 1452525.
    nsTHashtable<nsPtrHashKey<StyleSheet>> sheetSet;
    for (StyleSheet* sheet : xblSheetArray) {
      if (!sheetSet.Contains(sheet)) {
        sheetSet.PutEntry(sheet);
        aResult.AppendElement(sheet);
      }
    }
  }

  // Get the document sheets.
  for (size_t i = 0; i < aDocument.SheetCount(); i++) {
    aResult.AppendElement(aDocument.SheetAt(i));
  }
}

bool
InspectorUtils::IsIgnorableWhitespace(CharacterData& aDataNode)
{
  if (!aDataNode.TextIsOnlyWhitespace()) {
    return false;
  }

  // Okay.  We have only white space.  Let's check the white-space
  // property now and make sure that this isn't preformatted text...
  if (nsIFrame* frame = aDataNode.GetPrimaryFrame()) {
    return !frame->StyleText()->WhiteSpaceIsSignificant();
  }

  // empty inter-tag text node without frame, e.g., in between <table>\n<tr>
  return true;
}

/* static */ nsINode*
InspectorUtils::GetParentForNode(nsINode& aNode,
                                 bool aShowingAnonymousContent)
{
  // First do the special cases -- document nodes and anonymous content
  nsINode* parent = nullptr;

  if (aNode.IsDocument()) {
    parent = inLayoutUtils::GetContainerFor(*aNode.AsDocument());
  } else if (aShowingAnonymousContent) {
    if (aNode.IsContent()) {
      parent = aNode.AsContent()->GetFlattenedTreeParent();
    }
  }

  if (!parent) {
    // Ok, just get the normal DOM parent node
    return aNode.GetParentNode();
  }

  return parent;
}

/* static */ already_AddRefed<nsINodeList>
InspectorUtils::GetChildrenForNode(nsINode& aNode,
                                   bool aShowingAnonymousContent)
{
  nsCOMPtr<nsINodeList> kids;

  if (aShowingAnonymousContent) {
    if (aNode.IsContent()) {
      kids = aNode.AsContent()->GetChildren(nsIContent::eAllChildren);
    }
  }

  if (!kids) {
    kids = aNode.ChildNodes();
  }

  return kids.forget();
}

/* static */ void
InspectorUtils::GetCSSStyleRules(GlobalObject& aGlobalObject,
                                 Element& aElement,
                                 const nsAString& aPseudo,
                                 nsTArray<RefPtr<css::Rule>>& aResult)
{
  RefPtr<nsAtom> pseudoElt;
  if (!aPseudo.IsEmpty()) {
    pseudoElt = NS_Atomize(aPseudo);
  }

  RefPtr<ComputedStyle> computedStyle =
    GetCleanComputedStyleForElement(&aElement, pseudoElt);
  if (!computedStyle) {
    // This can fail for elements that are not in the document or
    // if the document they're in doesn't have a presshell.  Bail out.
    return;
  }


  nsIDocument* doc = aElement.OwnerDoc();
  nsIPresShell* shell = doc->GetShell();
  if (!shell) {
    return;
  }

  nsTArray<const RawServoStyleRule*> rawRuleList;
  Servo_ComputedValues_GetStyleRuleList(computedStyle, &rawRuleList);

  AutoTArray<ServoStyleRuleMap*, 1> maps;
  {
    ServoStyleSet* styleSet = shell->StyleSet();
    ServoStyleRuleMap* map = styleSet->StyleRuleMap();
    maps.AppendElement(map);
  }

  // Collect style rule maps for bindings.
  for (nsIContent* bindingContent = &aElement; bindingContent;
       bindingContent = bindingContent->GetBindingParent()) {
    for (nsXBLBinding* binding = bindingContent->GetXBLBinding();
         binding; binding = binding->GetBaseBinding()) {
      if (auto* map = binding->PrototypeBinding()->GetServoStyleRuleMap()) {
        maps.AppendElement(map);
      }
    }
    // Note that we intentionally don't cut off here, unlike when we
    // do styling, because even if style rules from parent binding
    // do not apply to the element directly in those cases, their
    // rules may still show up in the list we get above due to the
    // inheritance in cascading.
  }

  // Now shadow DOM stuff...
  if (auto* shadow = aElement.GetShadowRoot()) {
    maps.AppendElement(&shadow->ServoStyleRuleMap());
  }

  for (auto* shadow = aElement.GetContainingShadow();
       shadow;
       shadow = shadow->Host()->GetContainingShadow()) {
    maps.AppendElement(&shadow->ServoStyleRuleMap());
  }

  // Find matching rules in the table.
  for (const RawServoStyleRule* rawRule : Reversed(rawRuleList)) {
    ServoStyleRule* rule = nullptr;
    for (ServoStyleRuleMap* map : maps) {
      rule = map->Lookup(rawRule);
      if (rule) {
        break;
      }
    }
    if (rule) {
      aResult.AppendElement(rule);
    } else {
      MOZ_ASSERT_UNREACHABLE("We should be able to map a raw rule to a rule");
    }
  }
}

/* static */ uint32_t
InspectorUtils::GetRuleLine(GlobalObject& aGlobal, css::Rule& aRule)
{
  return aRule.GetLineNumber();
}

/* static */ uint32_t
InspectorUtils::GetRuleColumn(GlobalObject& aGlobal, css::Rule& aRule)
{
  return aRule.GetColumnNumber();
}

/* static */ uint32_t
InspectorUtils::GetRelativeRuleLine(GlobalObject& aGlobal, css::Rule& aRule)
{
  uint32_t lineNumber = aRule.GetLineNumber();

  // If aRule was parsed along with its stylesheet, then it will
  // have an absolute lineNumber that we need to remap to its
  // containing node. But if aRule was added via CSSOM after parsing,
  // then it has a sort-of relative line number already:
  // Gecko gives all rules a 0 lineNumber.
  // Servo gives the first line of a rule a 0 lineNumber, and then
  //   counts up from there.

  // The Servo behavior is arguably more correct, but harder to
  // interpret for purposes of deciding whether a lineNumber is
  // relative or absolute.

  // Since most of the time, inserted rules are single line and
  // therefore have 0 lineNumbers in both Gecko and Servo, we use
  // that to detect that a lineNumber is already relative.

  // There is one ugly edge case that we avoid: if an inserted rule
  // is multi-line, then Servo will give it 0+ lineNumbers. If we
  // do relative number mapping on those line numbers, we could get
  // negative underflow. So we check for underflow and instead report
  // a 0 lineNumber.
  StyleSheet* sheet = aRule.GetStyleSheet();
  if (sheet && lineNumber != 0) {
    nsINode* owningNode = sheet->GetOwnerNode();
    if (owningNode) {
      nsCOMPtr<nsIStyleSheetLinkingElement> link =
        do_QueryInterface(owningNode);
      if (link) {
        // Check for underflow, which is one indication that we're
        // trying to remap an already relative lineNumber.
        uint32_t linkLineIndex0 = link->GetLineNumber() - 1;
        if (linkLineIndex0 > lineNumber ) {
          lineNumber = 0;
        } else {
          lineNumber -= linkLineIndex0;
        }
      }
    }
  }

  return lineNumber;
}

/* static */ bool
InspectorUtils::HasRulesModifiedByCSSOM(GlobalObject& aGlobal, StyleSheet& aSheet)
{
  return aSheet.HasModifiedRules();
}

/* static */ CSSLexer*
InspectorUtils::GetCSSLexer(GlobalObject& aGlobal, const nsAString& aText)
{
  return new CSSLexer(aText);
}

/* static */ uint32_t
InspectorUtils::GetSelectorCount(GlobalObject& aGlobal,
                                 BindingStyleRule& aRule)
{
  return aRule.GetSelectorCount();
}

/* static */ void
InspectorUtils::GetSelectorText(GlobalObject& aGlobal,
                                BindingStyleRule& aRule,
                                uint32_t aSelectorIndex,
                                nsString& aText,
                                ErrorResult& aRv)
{
  aRv = aRule.GetSelectorText(aSelectorIndex, aText);
}

/* static */ uint64_t
InspectorUtils::GetSpecificity(GlobalObject& aGlobal,
                               BindingStyleRule& aRule,
                               uint32_t aSelectorIndex,
                               ErrorResult& aRv)
{
  uint64_t s;
  aRv = aRule.GetSpecificity(aSelectorIndex, &s);
  return s;
}

/* static */ bool
InspectorUtils::SelectorMatchesElement(GlobalObject& aGlobalObject,
                                       Element& aElement,
                                       BindingStyleRule& aRule,
                                       uint32_t aSelectorIndex,
                                       const nsAString& aPseudo,
                                       ErrorResult& aRv)
{
  bool result = false;
  aRv = aRule.SelectorMatchesElement(&aElement, aSelectorIndex, aPseudo,
                                     &result);
  return result;
}

/* static */ bool
InspectorUtils::IsInheritedProperty(GlobalObject& aGlobalObject,
                                    const nsAString& aPropertyName)
{
  NS_ConvertUTF16toUTF8 propName(aPropertyName);
  return Servo_Property_IsInherited(&propName);
}

/* static */ void
InspectorUtils::GetCSSPropertyNames(GlobalObject& aGlobalObject,
                                    const PropertyNamesOptions& aOptions,
                                    nsTArray<nsString>& aResult)
{
  CSSEnabledState enabledState = aOptions.mIncludeExperimentals
    ? CSSEnabledState::eIgnoreEnabledState
    : CSSEnabledState::eForAllContent;

  auto appendProperty = [enabledState, &aResult](uint32_t prop) {
    nsCSSPropertyID cssProp = nsCSSPropertyID(prop);
    if (nsCSSProps::IsEnabled(cssProp, enabledState)) {
      nsDependentCString name(kCSSRawProperties[prop]);
      aResult.AppendElement(NS_ConvertASCIItoUTF16(name));
    }
  };

  uint32_t prop = 0;
  for ( ; prop < eCSSProperty_COUNT_no_shorthands; ++prop) {
    if (!nsCSSProps::PropHasFlags(nsCSSPropertyID(prop),
                                  CSSPropFlags::Inaccessible)) {
      appendProperty(prop);
    }
  }

  if (aOptions.mIncludeShorthands) {
    for ( ; prop < eCSSProperty_COUNT; ++prop) {
      appendProperty(prop);
    }
  }

  if (aOptions.mIncludeAliases) {
    for (prop = eCSSProperty_COUNT; prop < eCSSProperty_COUNT_with_aliases; ++prop) {
      appendProperty(prop);
    }
  }
}

/* static */ void
InspectorUtils::GetSubpropertiesForCSSProperty(GlobalObject& aGlobal,
                                               const nsAString& aProperty,
                                               nsTArray<nsString>& aResult,
                                               ErrorResult& aRv)
{
  nsCSSPropertyID propertyID =
    nsCSSProps::LookupProperty(aProperty, CSSEnabledState::eForAllContent);

  if (propertyID == eCSSProperty_UNKNOWN) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (propertyID == eCSSPropertyExtra_variable) {
    aResult.AppendElement(aProperty);
    return;
  }

  if (!nsCSSProps::IsShorthand(propertyID)) {
    nsString* name = aResult.AppendElement();
    CopyASCIItoUTF16(nsCSSProps::GetStringValue(propertyID), *name);
    return;
  }

  for (const nsCSSPropertyID* props = nsCSSProps::SubpropertyEntryFor(propertyID);
       *props != eCSSProperty_UNKNOWN; ++props) {
    nsString* name = aResult.AppendElement();
    CopyASCIItoUTF16(nsCSSProps::GetStringValue(*props), *name);
  }
}

/* static */ bool
InspectorUtils::CssPropertyIsShorthand(GlobalObject& aGlobalObject,
                                       const nsAString& aProperty,
                                       ErrorResult& aRv)
{
  NS_ConvertUTF16toUTF8 prop(aProperty);
  bool found;
  bool isShorthand = Servo_Property_IsShorthand(&prop, &found);
  if (!found) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
  return isShorthand;
}

bool
InspectorUtils::CssPropertySupportsType(GlobalObject& aGlobalObject,
                                        const nsAString& aProperty,
                                        uint32_t aType,
                                        ErrorResult& aRv)
{
  NS_ConvertUTF16toUTF8 property(aProperty);
  bool found;
  bool result = Servo_Property_SupportsType(&property, aType, &found);
  if (!found) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }
  return result;
}

/* static */ void
InspectorUtils::GetCSSValuesForProperty(GlobalObject& aGlobalObject,
                                        const nsAString& aProperty,
                                        nsTArray<nsString>& aResult,
                                        ErrorResult& aRv)
{
  NS_ConvertUTF16toUTF8 property(aProperty);
  bool found;
  Servo_Property_GetCSSValuesForProperty(&property, &found, &aResult);
  if (!found) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
  return;
}

/* static */ void
InspectorUtils::RgbToColorName(GlobalObject& aGlobalObject,
                               uint8_t aR, uint8_t aG, uint8_t aB,
                               nsAString& aColorName,
                               ErrorResult& aRv)
{
  const char* color = NS_RGBToColorName(NS_RGB(aR, aG, aB));
  if (!color) {
    aColorName.Truncate();
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  aColorName.AssignASCII(color);
}

/* static */ void
InspectorUtils::ColorToRGBA(GlobalObject& aGlobalObject,
                            const nsAString& aColorString,
                            Nullable<InspectorRGBATuple>& aResult)
{
  nscolor color = NS_RGB(0, 0, 0);

  if (!ServoCSSParser::ComputeColor(nullptr, NS_RGB(0, 0, 0), aColorString,
                                    &color)) {
    aResult.SetNull();
    return;
  }

  InspectorRGBATuple& tuple = aResult.SetValue();
  tuple.mR = NS_GET_R(color);
  tuple.mG = NS_GET_G(color);
  tuple.mB = NS_GET_B(color);
  tuple.mA = nsStyleUtil::ColorComponentToFloat(NS_GET_A(color));
}

/* static */ bool
InspectorUtils::IsValidCSSColor(GlobalObject& aGlobalObject,
                                const nsAString& aColorString)
{
  return ServoCSSParser::IsValidCSSColor(aColorString);
}

void
InspectorUtils::GetBindingURLs(GlobalObject& aGlobalObject,
                               Element& aElement,
                               nsTArray<nsString>& aResult)
{
  nsXBLBinding* binding = aElement.GetXBLBinding();

  while (binding) {
    nsCString spec;
    nsCOMPtr<nsIURI> bindingURI = binding->PrototypeBinding()->BindingURI();
    bindingURI->GetSpec(spec);
    nsString* resultURI = aResult.AppendElement();
    CopyASCIItoUTF16(spec, *resultURI);
    binding = binding->GetBaseBinding();
  }
}

/* static */ bool
InspectorUtils::SetContentState(GlobalObject& aGlobalObject,
                                Element& aElement,
                                uint64_t aState,
                                ErrorResult& aRv)
{
  RefPtr<EventStateManager> esm =
    inLayoutUtils::GetEventStateManagerFor(aElement);
  if (!esm) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  return esm->SetContentState(&aElement, EventStates(aState));
}

/* static */ bool
InspectorUtils::RemoveContentState(GlobalObject& aGlobalObject,
                                   Element& aElement,
                                   uint64_t aState,
                                   bool aClearActiveDocument,
                                   ErrorResult& aRv)
{
  RefPtr<EventStateManager> esm =
    inLayoutUtils::GetEventStateManagerFor(aElement);
  if (!esm) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  bool result = esm->SetContentState(nullptr, EventStates(aState));

  if (aClearActiveDocument && EventStates(aState) == NS_EVENT_STATE_ACTIVE) {
    EventStateManager* activeESM = static_cast<EventStateManager*>(
      EventStateManager::GetActiveEventStateManager());
    if (activeESM == esm) {
      EventStateManager::ClearGlobalActiveContent(nullptr);
    }
  }

  return result;
}

/* static */ uint64_t
InspectorUtils::GetContentState(GlobalObject& aGlobalObject,
                                Element& aElement)
{
  // NOTE: if this method is removed,
  // please remove GetInternalValue from EventStates
  return aElement.State().GetInternalValue();
}

/* static */ already_AddRefed<ComputedStyle>
InspectorUtils::GetCleanComputedStyleForElement(dom::Element* aElement,
                                               nsAtom* aPseudo)
{
  MOZ_ASSERT(aElement);

  nsIDocument* doc = aElement->GetComposedDoc();
  if (!doc) {
    return nullptr;
  }

  nsIPresShell *presShell = doc->GetShell();
  if (!presShell) {
    return nullptr;
  }

  nsPresContext *presContext = presShell->GetPresContext();
  if (!presContext) {
    return nullptr;
  }

  presContext->EnsureSafeToHandOutCSSRules();

  return nsComputedDOMStyle::GetComputedStyle(aElement, aPseudo);
}

/* static */ void
InspectorUtils::GetUsedFontFaces(GlobalObject& aGlobalObject,
                                 nsRange& aRange,
                                 uint32_t aMaxRanges,
                                 nsTArray<nsAutoPtr<InspectorFontFace>>& aResult,
                                 ErrorResult& aRv)
{
  nsresult rv = aRange.GetUsedFontFaces(aResult, aMaxRanges);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

static EventStates
GetStatesForPseudoClass(const nsAString& aStatePseudo)
{
  if (aStatePseudo.IsEmpty() || aStatePseudo[0] != u':') {
    return EventStates();
  }
  NS_ConvertUTF16toUTF8 statePseudo(Substring(aStatePseudo, 1));
  return EventStates(Servo_PseudoClass_GetStates(&statePseudo));
}

/* static */ void
InspectorUtils::GetCSSPseudoElementNames(GlobalObject& aGlobalObject,
                                         nsTArray<nsString>& aResult)
{
  const CSSPseudoElementTypeBase pseudoCount =
    static_cast<CSSPseudoElementTypeBase>(CSSPseudoElementType::Count);
  for (CSSPseudoElementTypeBase i = 0; i < pseudoCount; ++i) {
    CSSPseudoElementType type = static_cast<CSSPseudoElementType>(i);
    if (nsCSSPseudoElements::IsEnabled(type, CSSEnabledState::eForAllContent)) {
      nsAtom* atom = nsCSSPseudoElements::GetPseudoAtom(type);
      aResult.AppendElement(nsDependentAtomString(atom));
    }
  }
}

/* static */ void
InspectorUtils::AddPseudoClassLock(GlobalObject& aGlobalObject,
                                   Element& aElement,
                                   const nsAString& aPseudoClass,
                                   bool aEnabled)
{
  EventStates state = GetStatesForPseudoClass(aPseudoClass);
  if (state.IsEmpty()) {
    return;
  }

  aElement.LockStyleStates(state, aEnabled);
}

/* static */ void
InspectorUtils::RemovePseudoClassLock(GlobalObject& aGlobal,
                                      Element& aElement,
                                      const nsAString& aPseudoClass)
{
  EventStates state = GetStatesForPseudoClass(aPseudoClass);
  if (state.IsEmpty()) {
    return;
  }

  aElement.UnlockStyleStates(state);
}

/* static */ bool
InspectorUtils::HasPseudoClassLock(GlobalObject& aGlobalObject,
                                   Element& aElement,
                                   const nsAString& aPseudoClass)
{
  EventStates state = GetStatesForPseudoClass(aPseudoClass);
  if (state.IsEmpty()) {
    return false;
  }

  EventStates locks = aElement.LockedStyleStates().mLocks;
  return locks.HasAllStates(state);
}

/* static */ void
InspectorUtils::ClearPseudoClassLocks(GlobalObject& aGlobalObject,
                                      Element& aElement)
{
  aElement.ClearStyleStateLocks();
}

/* static */ void
InspectorUtils::ParseStyleSheet(GlobalObject& aGlobalObject,
                                StyleSheet& aSheet,
                                const nsAString& aInput,
                                ErrorResult& aRv)
{

  aRv = aSheet.ReparseSheet(aInput);
}

void
InspectorUtils::ScrollElementIntoView(GlobalObject& aGlobalObject,
                                      Element& aElement)
{
  nsIPresShell* presShell = aElement.OwnerDoc()->GetShell();
  if (!presShell) {
    return;
  }

  presShell->ScrollContentIntoView(&aElement,
                                   nsIPresShell::ScrollAxis(),
                                   nsIPresShell::ScrollAxis(),
                                   nsIPresShell::SCROLL_OVERFLOW_HIDDEN);
}

} // namespace dom
} // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleSetHandleInlines_h
#define mozilla_StyleSetHandleInlines_h

#include "mozilla/StyleSheetInlines.h"
#include "mozilla/ServoStyleSet.h"
#include "nsStyleSet.h"

#define FORWARD_CONCRETE(method_, geckoargs_, servoargs_) \
  if (IsGecko()) { \
    return AsGecko()->method_ geckoargs_; \
  } else { \
    return AsServo()->method_ servoargs_; \
  }

#define FORWARD_WITH_PARENT(method_, parent_, args_) \
  if (IsGecko()) { \
    auto* parent = parent_ ? parent_->AsGecko() : nullptr; \
    return AsGecko()->method_ args_; \
  } else { \
    auto* parent = parent_ ? parent_->AsServo() : nullptr; \
    return AsServo()->method_ args_; \
  }

#define FORWARD(method_, args_) FORWARD_CONCRETE(method_, args_, args_)

namespace mozilla {

void
StyleSetHandle::Ptr::Delete()
{
  if (mValue) {
    if (IsGecko()) {
      delete AsGecko();
    } else {
      delete AsServo();
    }
  }
}

void
StyleSetHandle::Ptr::Init(nsPresContext* aPresContext,
                          nsBindingManager* aBindingManager)
{
  FORWARD(Init, (aPresContext, aBindingManager));
}

void
StyleSetHandle::Ptr::BeginShutdown()
{
  FORWARD(BeginShutdown, ());
}

void
StyleSetHandle::Ptr::Shutdown()
{
  FORWARD(Shutdown, ());
}

bool
StyleSetHandle::Ptr::GetAuthorStyleDisabled() const
{
  FORWARD(GetAuthorStyleDisabled, ());
}

nsresult
StyleSetHandle::Ptr::SetAuthorStyleDisabled(bool aStyleDisabled)
{
  FORWARD(SetAuthorStyleDisabled, (aStyleDisabled));
}

void
StyleSetHandle::Ptr::BeginUpdate()
{
  FORWARD(BeginUpdate, ());
}

nsresult
StyleSetHandle::Ptr::EndUpdate()
{
  FORWARD(EndUpdate, ());
}

// resolve a style context
already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolveStyleFor(dom::Element* aElement,
                                     nsStyleContext* aParentContext,
                                     LazyComputeBehavior aMayCompute)
{
  FORWARD_WITH_PARENT(ResolveStyleFor, aParentContext, (aElement, parent, aMayCompute));
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolveStyleFor(dom::Element* aElement,
                                     nsStyleContext* aParentContext,
                                     LazyComputeBehavior aMayCompute,
                                     TreeMatchContext* aTreeMatchContext)
{
  if (IsGecko()) {
    MOZ_ASSERT(aTreeMatchContext);
    auto* parent = aParentContext ? aParentContext->AsGecko() : nullptr;
    return AsGecko()->ResolveStyleFor(aElement, parent, aMayCompute, *aTreeMatchContext);
  } else {
    auto* parent = aParentContext ? aParentContext->AsServo() : nullptr;
    return AsServo()->ResolveStyleFor(aElement, parent, aMayCompute);
  }
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolveStyleForText(nsIContent* aTextNode,
                                         nsStyleContext* aParentContext)
{
  FORWARD_WITH_PARENT(ResolveStyleForText, aParentContext, (aTextNode, parent));
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolveStyleForPlaceholder()
{
  FORWARD(ResolveStyleForPlaceholder, ());
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolveStyleForFirstLetterContinuation(nsStyleContext* aParentContext)
{
  FORWARD_WITH_PARENT(ResolveStyleForFirstLetterContinuation, aParentContext, (parent));
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolvePseudoElementStyle(dom::Element* aParentElement,
                                               CSSPseudoElementType aType,
                                               nsStyleContext* aParentContext,
                                               dom::Element* aPseudoElement)
{
  FORWARD_WITH_PARENT(ResolvePseudoElementStyle, aParentContext, (aParentElement, aType, parent, aPseudoElement));
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolveInheritingAnonymousBoxStyle(nsIAtom* aPseudoTag,
                                                        nsStyleContext* aParentContext)
{
  FORWARD_WITH_PARENT(ResolveInheritingAnonymousBoxStyle, aParentContext, (aPseudoTag, parent));
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolveNonInheritingAnonymousBoxStyle(nsIAtom* aPseudoTag)
{
  FORWARD(ResolveNonInheritingAnonymousBoxStyle, (aPseudoTag));
}

// manage the set of style sheets in the style set
nsresult
StyleSetHandle::Ptr::AppendStyleSheet(SheetType aType, StyleSheet* aSheet)
{
  FORWARD_CONCRETE(AppendStyleSheet, (aType, aSheet->AsGecko()),
                                     (aType, aSheet->AsServo()));
}

nsresult
StyleSetHandle::Ptr::PrependStyleSheet(SheetType aType, StyleSheet* aSheet)
{
  FORWARD_CONCRETE(PrependStyleSheet, (aType, aSheet->AsGecko()),
                                      (aType, aSheet->AsServo()));
}

nsresult
StyleSetHandle::Ptr::RemoveStyleSheet(SheetType aType, StyleSheet* aSheet)
{
  FORWARD_CONCRETE(RemoveStyleSheet, (aType, aSheet->AsGecko()),
                                     (aType, aSheet->AsServo()));
}

nsresult
StyleSetHandle::Ptr::ReplaceSheets(SheetType aType,
                       const nsTArray<RefPtr<StyleSheet>>& aNewSheets)
{
  if (IsGecko()) {
    nsTArray<RefPtr<CSSStyleSheet>> newSheets(aNewSheets.Length());
    for (auto& sheet : aNewSheets) {
      newSheets.AppendElement(sheet->AsGecko());
    }
    return AsGecko()->ReplaceSheets(aType, newSheets);
  } else {
    nsTArray<RefPtr<ServoStyleSheet>> newSheets(aNewSheets.Length());
    for (auto& sheet : aNewSheets) {
      newSheets.AppendElement(sheet->AsServo());
    }
    return AsServo()->ReplaceSheets(aType, newSheets);
  }
}

nsresult
StyleSetHandle::Ptr::InsertStyleSheetBefore(SheetType aType,
                                StyleSheet* aNewSheet,
                                StyleSheet* aReferenceSheet)
{
  FORWARD_CONCRETE(
    InsertStyleSheetBefore,
    (aType, aNewSheet->AsGecko(), aReferenceSheet->AsGecko()),
    (aType, aNewSheet->AsServo(), aReferenceSheet->AsServo()));
}

int32_t
StyleSetHandle::Ptr::SheetCount(SheetType aType) const
{
  FORWARD(SheetCount, (aType));
}

StyleSheet*
StyleSetHandle::Ptr::StyleSheetAt(SheetType aType, int32_t aIndex) const
{
  FORWARD(StyleSheetAt, (aType, aIndex));
}

void
StyleSetHandle::Ptr::AppendAllXBLStyleSheets(nsTArray<StyleSheet*>& aArray) const
{
  FORWARD(AppendAllXBLStyleSheets, (aArray));
}

nsresult
StyleSetHandle::Ptr::RemoveDocStyleSheet(StyleSheet* aSheet)
{
  FORWARD_CONCRETE(RemoveDocStyleSheet, (aSheet->AsGecko()),
                                        (aSheet->AsServo()));
}

nsresult
StyleSetHandle::Ptr::AddDocStyleSheet(StyleSheet* aSheet,
                                      nsIDocument* aDocument)
{
  FORWARD_CONCRETE(AddDocStyleSheet, (aSheet->AsGecko(), aDocument),
                                     (aSheet->AsServo(), aDocument));
}

void
StyleSetHandle::Ptr::RecordStyleSheetChange(StyleSheet* aSheet,
                                            StyleSheet::ChangeType aChangeType)
{
  FORWARD_CONCRETE(RecordStyleSheetChange, (aSheet->AsGecko(), aChangeType),
                                           (aSheet->AsServo(), aChangeType));
}

void
StyleSetHandle::Ptr::RecordShadowStyleChange(mozilla::dom::ShadowRoot* aShadowRoot)
{
  FORWARD(RecordShadowStyleChange, (aShadowRoot));
}

bool
StyleSetHandle::Ptr::StyleSheetsHaveChanged() const
{
  FORWARD(StyleSheetsHaveChanged, ());
}
nsRestyleHint
StyleSetHandle::Ptr::MediumFeaturesChanged(bool aViewportChanged)
{
  FORWARD(MediumFeaturesChanged, (aViewportChanged));
}
void
StyleSetHandle::Ptr::InvalidateStyleForCSSRuleChanges()
{
  FORWARD(InvalidateStyleForCSSRuleChanges, ());
}

// check whether there is ::before/::after style for an element
already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ProbePseudoElementStyle(dom::Element* aParentElement,
                                             CSSPseudoElementType aType,
                                             nsStyleContext* aParentContext)
{
  FORWARD_WITH_PARENT(ProbePseudoElementStyle, aParentContext, (aParentElement, aType, parent));
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ProbePseudoElementStyle(dom::Element* aParentElement,
                                             CSSPseudoElementType aType,
                                             nsStyleContext* aParentContext,
                                             TreeMatchContext* aTreeMatchContext)
{
  if (IsGecko()) {
    MOZ_ASSERT(aTreeMatchContext);
    auto* parent = aParentContext ? aParentContext->AsGecko() : nullptr;
    return AsGecko()->ProbePseudoElementStyle(aParentElement, aType, parent,
                                              *aTreeMatchContext);
  }
  auto* parent = aParentContext ? aParentContext->AsServo() : nullptr;
  return AsServo()->ProbePseudoElementStyle(aParentElement, aType, parent);
}

nsRestyleHint
StyleSetHandle::Ptr::HasStateDependentStyle(dom::Element* aElement,
                                            EventStates aStateMask)
{
  FORWARD(HasStateDependentStyle, (aElement, aStateMask));
}

nsRestyleHint
StyleSetHandle::Ptr::HasStateDependentStyle(dom::Element* aElement,
                                            CSSPseudoElementType aPseudoType,
                                            dom::Element* aPseudoElement,
                                            EventStates aStateMask)
{
  FORWARD(HasStateDependentStyle, (aElement, aPseudoType, aPseudoElement,
                                   aStateMask));
}

void
StyleSetHandle::Ptr::RootStyleContextAdded()
{
  if (IsGecko()) {
    AsGecko()->RootStyleContextAdded();
  } else {
    // Not needed.
  }
}

void
StyleSetHandle::Ptr::RootStyleContextRemoved()
{
  if (IsGecko()) {
    RootStyleContextAdded();
  } else {
    // Not needed.
  }
}

bool
StyleSetHandle::Ptr::
AppendFontFaceRules(nsTArray<nsFontFaceRuleContainer>& aArray)
{
  FORWARD(AppendFontFaceRules, (aArray));
}

nsCSSCounterStyleRule*
StyleSetHandle::Ptr::CounterStyleRuleForName(nsIAtom* aName)
{
  FORWARD(CounterStyleRuleForName, (aName));
}

bool
StyleSetHandle::Ptr::EnsureUniqueInnerOnCSSSheets()
{
  FORWARD(EnsureUniqueInnerOnCSSSheets, ());
}

void
StyleSetHandle::Ptr::SetNeedsRestyleAfterEnsureUniqueInner()
{
  FORWARD(SetNeedsRestyleAfterEnsureUniqueInner, ());
}

} // namespace mozilla

#undef FORWARD

#endif // mozilla_StyleSetHandleInlines_h

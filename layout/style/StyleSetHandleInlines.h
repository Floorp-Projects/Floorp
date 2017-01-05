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
StyleSetHandle::Ptr::Init(nsPresContext* aPresContext)
{
  FORWARD(Init, (aPresContext));
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
  FORWARD(ResolveStyleFor, (aElement, aParentContext, aMayCompute));
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolveStyleFor(dom::Element* aElement,
                                     nsStyleContext* aParentContext,
                                     LazyComputeBehavior aMayCompute,
                                     TreeMatchContext& aTreeMatchContext)
{
  FORWARD(ResolveStyleFor, (aElement, aParentContext, aMayCompute, aTreeMatchContext));
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolveStyleForText(nsIContent* aTextNode,
                                         nsStyleContext* aParentContext)
{
  FORWARD(ResolveStyleForText, (aTextNode, aParentContext));
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolveStyleForOtherNonElement(nsStyleContext* aParentContext)
{
  FORWARD(ResolveStyleForOtherNonElement, (aParentContext));
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolvePseudoElementStyle(dom::Element* aParentElement,
                                               CSSPseudoElementType aType,
                                               nsStyleContext* aParentContext,
                                               dom::Element* aPseudoElement)
{
  FORWARD(ResolvePseudoElementStyle, (aParentElement, aType, aParentContext,
                                      aPseudoElement));
}

// aFlags is an nsStyleSet flags bitfield
already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolveAnonymousBoxStyle(nsIAtom* aPseudoTag,
                                              nsStyleContext* aParentContext,
                                              uint32_t aFlags)
{
  FORWARD(ResolveAnonymousBoxStyle, (aPseudoTag, aParentContext, aFlags));
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
    (aType, aReferenceSheet->AsServo(), aReferenceSheet->AsServo()));
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

// check whether there is ::before/::after style for an element
already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ProbePseudoElementStyle(dom::Element* aParentElement,
                                             CSSPseudoElementType aType,
                                             nsStyleContext* aParentContext)
{
  FORWARD(ProbePseudoElementStyle, (aParentElement, aType, aParentContext));
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ProbePseudoElementStyle(dom::Element* aParentElement,
                                             CSSPseudoElementType aType,
                                             nsStyleContext* aParentContext,
                                             TreeMatchContext& aTreeMatchContext,
                                             dom::Element* aPseudoElement)
{
  FORWARD(ProbePseudoElementStyle, (aParentElement, aType, aParentContext,
                                    aTreeMatchContext, aPseudoElement));
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

} // namespace mozilla

#undef FORWARD

#endif // mozilla_StyleSetHandleInlines_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleSetHandleInlines_h
#define mozilla_StyleSetHandleInlines_h

#include "mozilla/ServoStyleSet.h"
#include "nsStyleSet.h"

#define FORWARD(method_, args_) \
  return IsGecko() ? AsGecko()->method_ args_ : AsServo()->method_ args_;

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
                                     nsStyleContext* aParentContext)
{
  FORWARD(ResolveStyleFor, (aElement, aParentContext));
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolveStyleFor(dom::Element* aElement,
                                     nsStyleContext* aParentContext,
                                     TreeMatchContext& aTreeMatchContext)
{
  FORWARD(ResolveStyleFor, (aElement, aParentContext, aTreeMatchContext));
}

already_AddRefed<nsStyleContext>
StyleSetHandle::Ptr::ResolveStyleForNonElement(nsStyleContext* aParentContext)
{
  FORWARD(ResolveStyleForNonElement, (aParentContext));
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
StyleSetHandle::Ptr::AppendStyleSheet(SheetType aType, CSSStyleSheet* aSheet)
{
  FORWARD(AppendStyleSheet, (aType, aSheet));
}

nsresult
StyleSetHandle::Ptr::PrependStyleSheet(SheetType aType, CSSStyleSheet* aSheet)
{
  FORWARD(PrependStyleSheet, (aType, aSheet));
}

nsresult
StyleSetHandle::Ptr::RemoveStyleSheet(SheetType aType, CSSStyleSheet* aSheet)
{
  FORWARD(RemoveStyleSheet, (aType, aSheet));
}

nsresult
StyleSetHandle::Ptr::ReplaceSheets(SheetType aType,
                       const nsTArray<RefPtr<CSSStyleSheet>>& aNewSheets)
{
  FORWARD(ReplaceSheets, (aType, aNewSheets));
}

nsresult
StyleSetHandle::Ptr::InsertStyleSheetBefore(SheetType aType,
                                CSSStyleSheet* aNewSheet,
                                CSSStyleSheet* aReferenceSheet)
{
  FORWARD(InsertStyleSheetBefore, (aType, aNewSheet, aReferenceSheet));
}

int32_t
StyleSetHandle::Ptr::SheetCount(SheetType aType) const
{
  FORWARD(SheetCount, (aType));
}

CSSStyleSheet*
StyleSetHandle::Ptr::StyleSheetAt(SheetType aType, int32_t aIndex) const
{
  FORWARD(StyleSheetAt, (aType, aIndex));
}

nsresult
StyleSetHandle::Ptr::RemoveDocStyleSheet(CSSStyleSheet* aSheet)
{
  FORWARD(RemoveDocStyleSheet, (aSheet));
}

nsresult
StyleSetHandle::Ptr::AddDocStyleSheet(CSSStyleSheet* aSheet,
                                      nsIDocument* aDocument)
{
  FORWARD(AddDocStyleSheet, (aSheet, aDocument));
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

} // namespace mozilla

#undef FORWARD

#endif // mozilla_StyleSetHandleInlines_h

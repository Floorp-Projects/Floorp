/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoStyleSet.h"

#include "nsCSSAnonBoxes.h"

using namespace mozilla;
using namespace mozilla::dom;

void
ServoStyleSet::Init(nsPresContext* aPresContext)
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoStyleSet::BeginShutdown()
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoStyleSet::Shutdown()
{
  MOZ_CRASH("stylo: not implemented");
}

bool
ServoStyleSet::GetAuthorStyleDisabled() const
{
  MOZ_CRASH("stylo: not implemented");
}

nsresult
ServoStyleSet::SetAuthorStyleDisabled(bool aStyleDisabled)
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoStyleSet::BeginUpdate()
{
  MOZ_CRASH("stylo: not implemented");
}

nsresult
ServoStyleSet::EndUpdate()
{
  MOZ_CRASH("stylo: not implemented");
}

// resolve a style context
already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveStyleFor(Element* aElement,
                               nsStyleContext* aParentContext)
{
  MOZ_CRASH("stylo: not implemented");
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveStyleFor(Element* aElement,
                               nsStyleContext* aParentContext,
                               TreeMatchContext& aTreeMatchContext)
{
  MOZ_CRASH("stylo: not implemented");
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveStyleForNonElement(nsStyleContext* aParentContext)
{
  MOZ_CRASH("stylo: not implemented");
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolvePseudoElementStyle(Element* aParentElement,
                                         CSSPseudoElementType aType,
                                         nsStyleContext* aParentContext,
                                         Element* aPseudoElement)
{
  MOZ_CRASH("stylo: not implemented");
}

// aFlags is an nsStyleSet flags bitfield
already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveAnonymousBoxStyle(nsIAtom* aPseudoTag,
                                        nsStyleContext* aParentContext,
                                        uint32_t aFlags)
{
  MOZ_CRASH("stylo: not implemented");
}

// manage the set of style sheets in the style set
nsresult
ServoStyleSet::AppendStyleSheet(SheetType aType,
                                CSSStyleSheet* aSheet)
{
  MOZ_CRASH("stylo: not implemented");
}

nsresult
ServoStyleSet::PrependStyleSheet(SheetType aType,
                                 CSSStyleSheet* aSheet)
{
  MOZ_CRASH("stylo: not implemented");
}

nsresult
ServoStyleSet::RemoveStyleSheet(SheetType aType,
                                CSSStyleSheet* aSheet)
{
  MOZ_CRASH("stylo: not implemented");
}

nsresult
ServoStyleSet::ReplaceSheets(SheetType aType,
                             const nsTArray<RefPtr<CSSStyleSheet>>& aNewSheets)
{
  MOZ_CRASH("stylo: not implemented");
}

nsresult
ServoStyleSet::InsertStyleSheetBefore(SheetType aType,
                                      CSSStyleSheet* aNewSheet,
                                      CSSStyleSheet* aReferenceSheet)
{
  MOZ_CRASH("stylo: not implemented");
}

int32_t
ServoStyleSet::SheetCount(SheetType aType) const
{
  MOZ_CRASH("stylo: not implemented");
}

CSSStyleSheet*
ServoStyleSet::StyleSheetAt(SheetType aType,
                            int32_t aIndex) const
{
  MOZ_CRASH("stylo: not implemented");
}

nsresult
ServoStyleSet::RemoveDocStyleSheet(CSSStyleSheet* aSheet)
{
  MOZ_CRASH("stylo: not implemented");
}

nsresult
ServoStyleSet::AddDocStyleSheet(CSSStyleSheet* aSheet,
                                nsIDocument* aDocument)
{
  MOZ_CRASH("stylo: not implemented");
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ProbePseudoElementStyle(Element* aParentElement,
                                       CSSPseudoElementType aType,
                                       nsStyleContext* aParentContext)
{
  MOZ_CRASH("stylo: not implemented");
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ProbePseudoElementStyle(Element* aParentElement,
                                       CSSPseudoElementType aType,
                                       nsStyleContext* aParentContext,
                                       TreeMatchContext& aTreeMatchContext,
                                       Element* aPseudoElement)
{
  MOZ_CRASH("stylo: not implemented");
}

nsRestyleHint
ServoStyleSet::HasStateDependentStyle(dom::Element* aElement,
                                      EventStates aStateMask)
{
  MOZ_CRASH("stylo: not implemented");
}

nsRestyleHint
ServoStyleSet::HasStateDependentStyle(dom::Element* aElement,
                                      CSSPseudoElementType aPseudoType,
                                     dom::Element* aPseudoElement,
                                     EventStates aStateMask)
{
  MOZ_CRASH("stylo: not implemented");
}

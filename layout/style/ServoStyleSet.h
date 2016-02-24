/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoStyleSet_h
#define mozilla_ServoStyleSet_h

#include "mozilla/EventStates.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SheetType.h"
#include "nsChangeHint.h"
#include "nsCSSPseudoElements.h"
#include "nsIAtom.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
class CSSStyleSheet;
} // namespace mozilla
class nsStyleContext;
class nsPresContext;
struct TreeMatchContext;

namespace mozilla {

/**
 * The set of style sheets that apply to a document, backed by a Servo
 * Stylist.  A ServoStyleSet contains ServoStyleSheets.
 */
class ServoStyleSet
{
public:
  void Init(nsPresContext* aPresContext);
  void BeginShutdown();
  void Shutdown();

  bool GetAuthorStyleDisabled() const;
  nsresult SetAuthorStyleDisabled(bool aStyleDisabled);

  void BeginUpdate();
  nsresult EndUpdate();

  // resolve a style context
  already_AddRefed<nsStyleContext>
  ResolveStyleFor(dom::Element* aElement,
                  nsStyleContext* aParentContext);

  already_AddRefed<nsStyleContext>
  ResolveStyleFor(dom::Element* aElement,
                  nsStyleContext* aParentContext,
                  TreeMatchContext& aTreeMatchContext);

  already_AddRefed<nsStyleContext>
  ResolveStyleForNonElement(nsStyleContext* aParentContext);

  already_AddRefed<nsStyleContext>
  ResolvePseudoElementStyle(dom::Element* aParentElement,
                            mozilla::CSSPseudoElementType aType,
                            nsStyleContext* aParentContext,
                            dom::Element* aPseudoElement);

  // aFlags is an nsStyleSet flags bitfield
  already_AddRefed<nsStyleContext>
  ResolveAnonymousBoxStyle(nsIAtom* aPseudoTag, nsStyleContext* aParentContext,
                           uint32_t aFlags = 0);

  // manage the set of style sheets in the style set
  nsresult AppendStyleSheet(SheetType aType, CSSStyleSheet* aSheet);
  nsresult PrependStyleSheet(SheetType aType, CSSStyleSheet* aSheet);
  nsresult RemoveStyleSheet(SheetType aType, CSSStyleSheet* aSheet);
  nsresult ReplaceSheets(SheetType aType,
                         const nsTArray<RefPtr<CSSStyleSheet>>& aNewSheets);
  nsresult InsertStyleSheetBefore(SheetType aType,
                                  CSSStyleSheet* aNewSheet,
                                  CSSStyleSheet* aReferenceSheet);

  int32_t SheetCount(SheetType aType) const;
  CSSStyleSheet* StyleSheetAt(SheetType aType, int32_t aIndex) const;

  nsresult RemoveDocStyleSheet(CSSStyleSheet* aSheet);
  nsresult AddDocStyleSheet(CSSStyleSheet* aSheet, nsIDocument* aDocument);

  // check whether there is ::before/::after style for an element
  already_AddRefed<nsStyleContext>
  ProbePseudoElementStyle(dom::Element* aParentElement,
                          mozilla::CSSPseudoElementType aType,
                          nsStyleContext* aParentContext);

  already_AddRefed<nsStyleContext>
  ProbePseudoElementStyle(dom::Element* aParentElement,
                          mozilla::CSSPseudoElementType aType,
                          nsStyleContext* aParentContext,
                          TreeMatchContext& aTreeMatchContext,
                          dom::Element* aPseudoElement = nullptr);

  // Test if style is dependent on content state
  nsRestyleHint HasStateDependentStyle(dom::Element* aElement,
                                       EventStates aStateMask);
  nsRestyleHint HasStateDependentStyle(dom::Element* aElement,
                                       mozilla::CSSPseudoElementType aPseudoType,
                                       dom::Element* aPseudoElement,
                                       EventStates aStateMask);
};

} // namespace mozilla

#endif // mozilla_ServoStyleSet_h

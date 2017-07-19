/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoStyleContext.h"

#include "nsStyleConsts.h"
#include "nsStyleStruct.h"
#include "nsPresContext.h"
#include "nsCSSRuleProcessor.h"
#include "mozilla/dom/HTMLBodyElement.h"

#include "mozilla/ServoBindings.h"

namespace mozilla {

ServoStyleContext::ServoStyleContext(
    nsStyleContext* aParent,
    nsPresContext* aPresContext,
    nsIAtom* aPseudoTag,
    CSSPseudoElementType aPseudoType,
    ServoComputedValuesForgotten aComputedValues)
  : nsStyleContext(aParent, aPseudoTag, aPseudoType)
  , mSource(aComputedValues)
{
  mPresContext = aPresContext;
  AddStyleBit(Servo_ComputedValues_GetStyleBits(this));

  FinishConstruction();

  // No need to call ApplyStyleFixups here, since fixups are handled by Servo when
  // producing the ServoComputedValues.
}

void
ServoStyleContext::UpdateWithElementState(Element* aElementForAnimation)
{
  bool isLink = false;
  bool isVisitedLink = false;
  // If we need visited styles for callers where `aElementForAnimation` is null,
  // we can precompute these and pass them as flags, similar to nsStyleSet.cpp.
  if (aElementForAnimation) {
    isLink = nsCSSRuleProcessor::IsLink(aElementForAnimation);
    isVisitedLink = nsCSSRuleProcessor::GetContentState(aElementForAnimation)
                                       .HasState(NS_EVENT_STATE_VISITED);
  }


  auto parent = GetParentAllowServo();

  // The true visited state of the relevant link is used to decided whether
  // visited styles should be consulted for all visited dependent properties.
  bool relevantLinkVisited = isLink ? isVisitedLink :
    (parent && parent->RelevantLinkVisited());

  if (relevantLinkVisited && GetStyleIfVisited()) {
    AddStyleBit(NS_STYLE_RELEVANT_LINK_VISITED);
  }

  // Set the body color on the pres context. See nsStyleSet::GetContext
  if (aElementForAnimation &&
      aElementForAnimation->IsHTMLElement(nsGkAtoms::body) &&
      GetPseudoType() == CSSPseudoElementType::NotPseudo &&
      mPresContext->CompatibilityMode() == eCompatibility_NavQuirks) {
    nsIDocument* doc = aElementForAnimation->GetUncomposedDoc();
    if (doc && doc->GetBodyElement() == aElementForAnimation) {
      // Update the prescontext's body color
      mPresContext->SetBodyTextColor(StyleColor()->mColor);
    }
  }
}

} // namespace mozilla

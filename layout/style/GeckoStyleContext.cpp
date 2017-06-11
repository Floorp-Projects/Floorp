/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/GeckoStyleContext.h"

#include "nsStyleConsts.h"
#include "nsStyleStruct.h"
#include "nsPresContext.h"
#include "nsRuleNode.h"

using namespace mozilla;

GeckoStyleContext::GeckoStyleContext(nsStyleContext* aParent,
                                     nsIAtom* aPseudoTag,
                                     CSSPseudoElementType aPseudoType,
                                     already_AddRefed<nsRuleNode> aRuleNode,
                                     bool aSkipParentDisplayBasedStyleFixup)
  : nsStyleContext(aParent, OwningStyleContextSource(Move(aRuleNode)),
                   aPseudoTag, aPseudoType)
{
#ifdef MOZ_STYLO
  mPresContext = mSource.AsGeckoRuleNode()->PresContext();
#endif

  if (aParent) {
#ifdef DEBUG
    nsRuleNode *r1 = mParent->RuleNode(), *r2 = mSource.AsGeckoRuleNode();
    while (r1->GetParent())
      r1 = r1->GetParent();
    while (r2->GetParent())
      r2 = r2->GetParent();
    NS_ASSERTION(r1 == r2, "must be in the same rule tree as parent");
#endif
  } else {
    PresContext()->PresShell()->StyleSet()->RootStyleContextAdded();
  }

  mSource.AsGeckoRuleNode()->SetUsedDirectly(); // before ApplyStyleFixups()!
  FinishConstruction();
  ApplyStyleFixups(aSkipParentDisplayBasedStyleFixup);
}

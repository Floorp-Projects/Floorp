/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * style rule processor for rules from SMIL Animation of SVG mapped
 * attributes (attributes whose values are mapped into style)
 */

#include "SVGAttrAnimationRuleProcessor.h"
#include "nsRuleProcessorData.h"
#include "nsSVGElement.h"

using namespace mozilla;
using namespace mozilla::dom;

SVGAttrAnimationRuleProcessor::SVGAttrAnimationRuleProcessor()
{
}

SVGAttrAnimationRuleProcessor::~SVGAttrAnimationRuleProcessor()
{
}

NS_IMPL_ISUPPORTS(SVGAttrAnimationRuleProcessor, nsIStyleRuleProcessor)

/* virtual */ void
SVGAttrAnimationRuleProcessor::RulesMatching(ElementRuleProcessorData* aData)
{
  ElementRulesMatching(aData->mElement, aData->mRuleWalker);
}

void
SVGAttrAnimationRuleProcessor::ElementRulesMatching(Element* aElement,
                                                    nsRuleWalker* aRuleWalker)
{
  if (aElement->IsSVGElement()) {
    static_cast<nsSVGElement*>(aElement)->
      WalkAnimatedContentStyleRules(aRuleWalker);
  }
}

/* virtual */ nsRestyleHint
SVGAttrAnimationRuleProcessor::HasStateDependentStyle(StateRuleProcessorData* aData)
{
  return nsRestyleHint(0);
}

/* virtual */ nsRestyleHint
SVGAttrAnimationRuleProcessor::HasStateDependentStyle(PseudoElementStateRuleProcessorData* aData)
{
  return nsRestyleHint(0);
}

/* virtual */ bool
SVGAttrAnimationRuleProcessor::HasDocumentStateDependentStyle(StateRuleProcessorData* aData)
{
  return false;
}

/* virtual */ nsRestyleHint
SVGAttrAnimationRuleProcessor::HasAttributeDependentStyle(
    AttributeRuleProcessorData* aData,
    RestyleHintData& aRestyleHintDataResult)
{
  return nsRestyleHint(0);
}

/* virtual */ bool
SVGAttrAnimationRuleProcessor::MediumFeaturesChanged(nsPresContext* aPresContext)
{
  return false;
}

/* virtual */ void
SVGAttrAnimationRuleProcessor::RulesMatching(PseudoElementRuleProcessorData* aData)
{
  // If SMIL Animation of SVG attributes can ever target
  // pseudo-elements, we need to adjust either
  // nsStyleSet::RuleNodeWithReplacement or the test in
  // ElementRestyler::RestyleSelf (added in bug 977991 patch 4) to
  // handle such styles.
}

/* virtual */ void
SVGAttrAnimationRuleProcessor::RulesMatching(AnonBoxRuleProcessorData* aData)
{
  // If SMIL Animation of SVG attributes can ever target anonymous boxes,
  // see comment in RulesMatching(PseudoElementRuleProcessorData*).
}

#ifdef MOZ_XUL
/* virtual */ void
SVGAttrAnimationRuleProcessor::RulesMatching(XULTreeRuleProcessorData* aData)
{
  // If SMIL Animation of SVG attributes can ever target XUL tree pseudos,
  // see comment in RulesMatching(PseudoElementRuleProcessorData*).
}
#endif

/* virtual */ size_t
SVGAttrAnimationRuleProcessor::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return 0; // SVGAttrAnimationRuleProcessors are charged to the DOM, not layout
}

/* virtual */ size_t
SVGAttrAnimationRuleProcessor::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return 0; // SVGAttrAnimationRuleProcessors are charged to the DOM, not layout
}

size_t
SVGAttrAnimationRuleProcessor::DOMSizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  return n;
}

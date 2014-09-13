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

using namespace mozilla;

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
SVGAttrAnimationRuleProcessor::HasAttributeDependentStyle(AttributeRuleProcessorData* aData)
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
}

/* virtual */ void
SVGAttrAnimationRuleProcessor::RulesMatching(AnonBoxRuleProcessorData* aData)
{
}

#ifdef MOZ_XUL
/* virtual */ void
SVGAttrAnimationRuleProcessor::RulesMatching(XULTreeRuleProcessorData* aData)
{
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

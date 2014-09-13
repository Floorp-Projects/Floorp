/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * style rule processor for rules from SMIL Animation of SVG mapped
 * attributes (attributes whose values are mapped into style)
 */

#ifndef mozilla_SVGAttrAnimationRuleProcessor_h_
#define mozilla_SVGAttrAnimationRuleProcessor_h_

#include "nsIStyleRuleProcessor.h"

class nsIDocument;
class nsRuleWalker;

namespace mozilla {

namespace dom {
class Element;
}

class SVGAttrAnimationRuleProcessor MOZ_FINAL : public nsIStyleRuleProcessor
{
public:
  SVGAttrAnimationRuleProcessor();

private:
  ~SVGAttrAnimationRuleProcessor();

public:
  NS_DECL_ISUPPORTS

  // nsIStyleRuleProcessor API
  virtual void RulesMatching(ElementRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual void RulesMatching(PseudoElementRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual void RulesMatching(AnonBoxRuleProcessorData* aData) MOZ_OVERRIDE;
#ifdef MOZ_XUL
  virtual void RulesMatching(XULTreeRuleProcessorData* aData) MOZ_OVERRIDE;
#endif
  virtual nsRestyleHint HasStateDependentStyle(StateRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual nsRestyleHint HasStateDependentStyle(PseudoElementStateRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual bool HasDocumentStateDependentStyle(StateRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual nsRestyleHint
    HasAttributeDependentStyle(AttributeRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual bool MediumFeaturesChanged(nsPresContext* aPresContext) MOZ_OVERRIDE;
  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE MOZ_OVERRIDE;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE MOZ_OVERRIDE;

  size_t DOMSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  // A shortcut for nsStyleSet to call RulesMatching with less setup.
  void ElementRulesMatching(mozilla::dom::Element* aElement,
                            nsRuleWalker* aRuleWalker);

private:
  SVGAttrAnimationRuleProcessor(const SVGAttrAnimationRuleProcessor& aCopy) MOZ_DELETE;
  SVGAttrAnimationRuleProcessor& operator=(const SVGAttrAnimationRuleProcessor& aCopy) MOZ_DELETE;
};

} // namespace mozilla

#endif /* !defined(mozilla_SVGAttrAnimationRuleProcessor_h_) */

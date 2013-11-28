/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * internal abstract interface for containers (roughly origins within
 * the CSS cascade) that provide style rules matching an element or
 * pseudo-element
 */

#ifndef nsIStyleRuleProcessor_h___
#define nsIStyleRuleProcessor_h___

#include "mozilla/MemoryReporting.h"
#include "nsISupports.h"
#include "nsChangeHint.h"

struct RuleProcessorData;
struct ElementDependentRuleProcessorData;
struct ElementRuleProcessorData;
struct PseudoElementRuleProcessorData;
struct AnonBoxRuleProcessorData;
#ifdef MOZ_XUL
struct XULTreeRuleProcessorData;
#endif
struct StateRuleProcessorData;
struct PseudoElementStateRuleProcessorData;
struct AttributeRuleProcessorData;
class nsPresContext;

// IID for the nsIStyleRuleProcessor interface
// {c1d6001e-4fcb-4c40-bce1-5eba80bfd8f3}
#define NS_ISTYLE_RULE_PROCESSOR_IID     \
{ 0xc1d6001e, 0x4fcb, 0x4c40, \
  {0xbc, 0xe1, 0x5e, 0xba, 0x80, 0xbf, 0xd8, 0xf3} }


/* The style rule processor interface is a mechanism to separate the matching
 * of style rules from style sheet instances.
 * Simple style sheets can and will act as their own processor. 
 * Sheets where rule ordering interlaces between multiple sheets, will need to 
 * share a single rule processor between them (CSS sheets do this for cascading order)
 *
 * @see nsIStyleRule (for significantly more detailed comments)
 */
class nsIStyleRuleProcessor : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISTYLE_RULE_PROCESSOR_IID)

  // Shorthand for:
  //  nsCOMArray<nsIStyleRuleProcessor>::nsCOMArrayEnumFunc
  typedef bool (* EnumFunc)(nsIStyleRuleProcessor*, void*);

  /**
   * Find the |nsIStyleRule|s matching the given content node and
   * position the given |nsRuleWalker| at the |nsRuleNode| in the rule
   * tree representing that ordered list of rules (with higher
   * precedence being farther from the root of the lexicographic tree).
   */
  virtual void RulesMatching(ElementRuleProcessorData* aData) = 0;

  /**
   * Just like the previous |RulesMatching|, except for a given content
   * node <em>and pseudo-element</em>.
   */
  virtual void RulesMatching(PseudoElementRuleProcessorData* aData) = 0;

  /**
   * Just like the previous |RulesMatching|, except for a given anonymous box.
   */
  virtual void RulesMatching(AnonBoxRuleProcessorData* aData) = 0;

#ifdef MOZ_XUL
  /**
   * Just like the previous |RulesMatching|, except for a given content
   * node <em>and tree pseudo</em>.
   */
  virtual void RulesMatching(XULTreeRuleProcessorData* aData) = 0;
#endif

  /**
   * Return whether style can depend on a change of the given document state.
   *
   * Document states are defined in nsIDocument.h.
   */
  virtual bool
    HasDocumentStateDependentStyle(StateRuleProcessorData* aData) = 0;

  /**
   * Return how (as described by nsRestyleHint) style can depend on a
   * change of the given content state on the given content node.  This
   * test is used for optimization only, and may err on the side of
   * reporting more dependencies than really exist.
   *
   * Event states are defined in nsEventStates.h.
   */
  virtual nsRestyleHint
    HasStateDependentStyle(StateRuleProcessorData* aData) = 0;
  virtual nsRestyleHint
    HasStateDependentStyle(PseudoElementStateRuleProcessorData* aData) = 0;

  /**
   * This method will be called twice for every attribute change.
   * During the first call, aData->mAttrHasChanged will be false and
   * the attribute change will not have happened yet.  During the
   * second call, aData->mAttrHasChanged will be true and the
   * change will have already happened.  The bitwise OR of the two
   * return values must describe the style changes that are needed due
   * to the attribute change.  It's up to the rule processor
   * implementation to decide how to split the bits up amongst the two
   * return values.  For example, it could return the bits needed by
   * rules that might stop matching the node from the first call and
   * the bits needed by rules that might have started matching the
   * node from the second call.  This test is used for optimization
   * only, and may err on the side of reporting more dependencies than
   * really exist.
   */
  virtual nsRestyleHint
    HasAttributeDependentStyle(AttributeRuleProcessorData* aData) = 0;

  /**
   * Do any processing that needs to happen as a result of a change in
   * the characteristics of the medium, and return whether this rule
   * processor's rules have changed (e.g., because of media queries).
   */
  virtual bool MediumFeaturesChanged(nsPresContext* aPresContext) = 0;

  /**
   * Report the size of this style rule processor to about:memory.  A
   * processor may return 0.
   */
  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const = 0;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIStyleRuleProcessor,
                              NS_ISTYLE_RULE_PROCESSOR_IID)

#endif /* nsIStyleRuleProcessor_h___ */

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * internal abstract interface for containers (roughly origins within
 * the CSS cascade) that provide style rules matching an element or
 * pseudo-element
 */

#ifndef nsIStyleRuleProcessor_h___
#define nsIStyleRuleProcessor_h___

#include "nsISupports.h"
#include "nsChangeHint.h"
#include "nsIContent.h"

struct RuleProcessorData;
struct ElementRuleProcessorData;
struct PseudoElementRuleProcessorData;
struct AnonBoxRuleProcessorData;
#ifdef MOZ_XUL
struct XULTreeRuleProcessorData;
#endif
struct StateRuleProcessorData;
struct AttributeRuleProcessorData;
class nsPresContext;

// IID for the nsIStyleRuleProcessor interface
// {32612c0e-3d34-4a6f-89d9-464f6811ac13}
#define NS_ISTYLE_RULE_PROCESSOR_IID     \
{ 0x32612c0e, 0x3d34, 0x4a6f, \
  {0x89, 0xd9, 0x46, 0x4f, 0x68, 0x11, 0xac, 0x13} }


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
  virtual PRInt64 SizeOf() const = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIStyleRuleProcessor,
                              NS_ISTYLE_RULE_PROCESSOR_IID)

#endif /* nsIStyleRuleProcessor_h___ */

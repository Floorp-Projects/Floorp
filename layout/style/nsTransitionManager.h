/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
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
 * The Original Code is nsTransitionManager.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* Code to start and animate CSS transitions. */

#ifndef nsTransitionManager_h_
#define nsTransitionManager_h_

#include "AnimationCommon.h"
#include "nsCSSPseudoElements.h"

class nsStyleContext;
class nsPresContext;
class nsCSSPropertySet;
struct nsTransition;
struct ElementTransitions;

class nsTransitionManager : public mozilla::css::CommonAnimationManager
{
public:
  nsTransitionManager(nsPresContext *aPresContext)
    : mozilla::css::CommonAnimationManager(aPresContext)
  {
  }

  /**
   * StyleContextChanged 
   *
   * To be called from nsFrameManager::ReResolveStyleContext when the
   * style of an element has changed, to initiate transitions from
   * that style change.  For style contexts with :before and :after
   * pseudos, aElement is expected to be the generated before/after
   * element.
   *
   * It may return a "cover rule" (see CoverTransitionStartStyleRule) to
   * cover up some of the changes for the duration of the restyling of
   * descendants.  If it does, this function will take care of causing
   * the necessary restyle afterwards, but the caller must restyle the
   * element *again* with the original sequence of rules plus the
   * returned cover rule as the most specific rule.
   */
  already_AddRefed<nsIStyleRule>
    StyleContextChanged(mozilla::dom::Element *aElement,
                        nsStyleContext *aOldStyleContext,
                        nsStyleContext *aNewStyleContext);

  // nsIStyleRuleProcessor (parts)
  virtual void RulesMatching(ElementRuleProcessorData* aData);
  virtual void RulesMatching(PseudoElementRuleProcessorData* aData);
  virtual void RulesMatching(AnonBoxRuleProcessorData* aData);
#ifdef MOZ_XUL
  virtual void RulesMatching(XULTreeRuleProcessorData* aData);
#endif

  // nsARefreshObserver
  virtual void WillRefresh(mozilla::TimeStamp aTime);

private:
  void ConsiderStartingTransition(nsCSSProperty aProperty,
                                  const nsTransition& aTransition,
                                  mozilla::dom::Element *aElement,
                                  ElementTransitions *&aElementTransitions,
                                  nsStyleContext *aOldStyleContext,
                                  nsStyleContext *aNewStyleContext,
                                  bool *aStartedAny,
                                  nsCSSPropertySet *aWhichStarted);
  ElementTransitions* GetElementTransitions(mozilla::dom::Element *aElement,
                                            nsCSSPseudoElements::Type aPseudoType,
                                            bool aCreateIfNeeded);
  void WalkTransitionRule(RuleProcessorData* aData,
                          nsCSSPseudoElements::Type aPseudoType);
};

#endif /* !defined(nsTransitionManager_h_) */

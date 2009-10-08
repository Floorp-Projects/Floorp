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

#include "prclist.h"
#include "nsCSSProperty.h"
#include "nsIStyleRuleProcessor.h"
#include "nsRefreshDriver.h"

class nsStyleContext;
class nsPresContext;
class nsCSSPropertySet;
struct nsTransition;
struct ElementTransitions;

/**
 * Must be created only as a sub-object of an nsPresContext (since its
 * reference counting methods assume that).
 */
class nsTransitionManager : public nsIStyleRuleProcessor,
                            public nsARefreshObserver {
public:
  nsTransitionManager(nsPresContext *aPresContext);
  ~nsTransitionManager();

  /**
   * StyleContextChanged 
   *
   * To be called from nsFrameManager::ReResolveStyleContext when the
   * style of an element has changed, to initiate transitions from that
   * style change.
   *
   * It may return a "cover rule" (see CoverTransitionStartStyleRule) to
   * cover up some of the changes for the duration of the restyling of
   * descendants.  If it does, this function will take care of causing
   * the necessary restyle afterwards, but the caller must restyle the
   * element *again* with the original sequence of rules plus the
   * returned cover rule as the most specific rule.
   */
  already_AddRefed<nsIStyleRule>
    StyleContextChanged(nsIContent *aElement,
                        nsStyleContext *aOldStyleContext,
                        nsStyleContext *aNewStyleContext);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIStyleRuleProcessor
  NS_IMETHOD RulesMatching(ElementRuleProcessorData* aData);
  NS_IMETHOD RulesMatching(PseudoRuleProcessorData* aData);
  NS_IMETHOD HasStateDependentStyle(StateRuleProcessorData* aData,
                                    nsReStyleHint* aResult);
  NS_IMETHOD HasAttributeDependentStyle(AttributeRuleProcessorData* aData,
                                        nsReStyleHint* aResult);
  NS_IMETHOD MediumFeaturesChanged(nsPresContext* aPresContext,
                                   PRBool* aRulesChanged);

  // nsARefreshObserver
  virtual void WillRefresh(mozilla::TimeStamp aTime);

private:
  friend class ElementTransitions; // for TransitionsRemoved

  void ConsiderStartingTransition(nsCSSProperty aProperty,
                                  const nsTransition& aTransition,
                                  nsIContent *aElement,
                                  ElementTransitions *&aElementTransitions,
                                  nsStyleContext *aOldStyleContext,
                                  nsStyleContext *aNewStyleContext,
                                  PRBool *aStartedAny,
                                  nsCSSPropertySet *aWhichStarted);
  ElementTransitions* GetElementTransitions(nsIContent *aElement,
                                            nsIAtom *aPseudo,
                                            PRBool aCreateIfNeeded);
  void AddElementTransitions(ElementTransitions* aElementTransitions);
  void TransitionsRemoved();
  nsresult WalkTransitionRule(RuleProcessorData* aData, nsIAtom *aPseudo);

  PRCList mElementTransitions;
  nsPresContext *mPresContext;
};

#endif /* !defined(nsTransitionManager_h_) */

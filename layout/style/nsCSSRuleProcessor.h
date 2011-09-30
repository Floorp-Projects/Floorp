/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=2:expandtab:shiftwidth=2:
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
 *   L. David Baron <dbaron@dbaron.org>
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
 * style rule processor for CSS style sheets, responsible for selector
 * matching and cascading
 */

#ifndef nsCSSRuleProcessor_h_
#define nsCSSRuleProcessor_h_

#include "nsIStyleRuleProcessor.h"
#include "nsCSSStyleSheet.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsCSSRules.h"
#include "nsRuleWalker.h"

struct RuleCascadeData;
struct nsCSSSelectorList;
struct CascadeEnumData;
struct TreeMatchContext;
class nsCSSKeyframesRule;

/**
 * The CSS style rule processor provides a mechanism for sibling style
 * sheets to combine their rule processing in order to allow proper
 * cascading to happen.
 *
 * CSS style rule processors keep a live reference on all style sheets
 * bound to them.  The CSS style sheets keep a weak reference to all the
 * processors that they are bound to (many to many).  The CSS style sheet
 * is told when the rule processor is going away (via DropRuleProcessor).
 */

class nsCSSRuleProcessor: public nsIStyleRuleProcessor {
public:
  typedef nsTArray<nsRefPtr<nsCSSStyleSheet> > sheet_array_type;

  nsCSSRuleProcessor(const sheet_array_type& aSheets, PRUint8 aSheetType);
  virtual ~nsCSSRuleProcessor();

  NS_DECL_ISUPPORTS

public:
  nsresult ClearRuleCascades();

  static nsresult Startup();
  static void Shutdown();
  static void FreeSystemMetrics();
  static bool HasSystemMetric(nsIAtom* aMetric);

  /*
   * Returns true if the given aElement matches one of the
   * selectors in aSelectorList.  Note that this method will assume
   * the given aElement is not a relevant link.  aSelectorList must not
   * include any pseudo-element selectors.  aSelectorList is allowed
   * to be null; in this case PR_FALSE will be returned.
   */
  static bool SelectorListMatches(mozilla::dom::Element* aElement,
                                    TreeMatchContext& aTreeMatchContext,
                                    nsCSSSelectorList* aSelectorList);

  /*
   * Helper to get the content state for a content node.  This may be
   * slightly adjusted from IntrinsicState().
   */
  static nsEventStates GetContentState(mozilla::dom::Element* aElement);

  /*
   * Helper to get the content state for :visited handling for an element
   */
  static nsEventStates GetContentStateForVisitedHandling(
             mozilla::dom::Element* aElement,
             nsRuleWalker::VisitedHandlingType aVisitedHandling,
             bool aIsRelevantLink);

  /*
   * Helper to test whether a node is a link
   */
  static bool IsLink(mozilla::dom::Element* aElement);

  // nsIStyleRuleProcessor
  virtual void RulesMatching(ElementRuleProcessorData* aData);

  virtual void RulesMatching(PseudoElementRuleProcessorData* aData);

  virtual void RulesMatching(AnonBoxRuleProcessorData* aData);

#ifdef MOZ_XUL
  virtual void RulesMatching(XULTreeRuleProcessorData* aData);
#endif

  virtual nsRestyleHint HasStateDependentStyle(StateRuleProcessorData* aData);

  virtual bool HasDocumentStateDependentStyle(StateRuleProcessorData* aData);

  virtual nsRestyleHint
    HasAttributeDependentStyle(AttributeRuleProcessorData* aData);

  virtual bool MediumFeaturesChanged(nsPresContext* aPresContext);

  virtual PRInt64 SizeOf() const;

  // Append all the currently-active font face rules to aArray.  Return
  // true for success and false for failure.
  bool AppendFontFaceRules(nsPresContext* aPresContext,
                             nsTArray<nsFontFaceRuleContainer>& aArray);

  bool AppendKeyframesRules(nsPresContext* aPresContext,
                              nsTArray<nsCSSKeyframesRule*>& aArray);

#ifdef DEBUG
  void AssertQuirksChangeOK() {
    NS_ASSERTION(!mRuleCascades, "can't toggle quirks style sheet without "
                                 "clearing rule cascades");
  }
#endif

#ifdef XP_WIN
  // Cached theme identifier for the moz-windows-theme media query.
  static PRUint8 GetWindowsThemeIdentifier();
  static void SetWindowsThemeIdentifier(PRUint8 aId) { 
    sWinThemeId = aId;
  }
#endif

private:
  static bool CascadeSheet(nsCSSStyleSheet* aSheet, CascadeEnumData* aData);

  RuleCascadeData* GetRuleCascade(nsPresContext* aPresContext);
  void RefreshRuleCascade(nsPresContext* aPresContext);

  // The sheet order here is the same as in nsStyleSet::mSheets
  sheet_array_type mSheets;

  // active first, then cached (most recent first)
  RuleCascadeData* mRuleCascades;

  // The last pres context for which GetRuleCascades was called.
  nsPresContext *mLastPresContext;
  
  // type of stylesheet using this processor
  PRUint8 mSheetType;  // == nsStyleSet::sheetType

#ifdef XP_WIN
  static PRUint8 sWinThemeId;
#endif
};

#endif /* nsCSSRuleProcessor_h_ */

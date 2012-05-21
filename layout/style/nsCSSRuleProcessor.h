/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=2:expandtab:shiftwidth=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsEventStates.h"

struct CascadeEnumData;
struct nsCSSSelector;
struct nsCSSSelectorList;
struct RuleCascadeData;
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
   * to be null; in this case false will be returned.
   */
  static bool SelectorListMatches(mozilla::dom::Element* aElement,
                                    TreeMatchContext& aTreeMatchContext,
                                    nsCSSSelectorList* aSelectorList);

  /*
   * Helper to get the content state for a content node.  This may be
   * slightly adjusted from IntrinsicState().
   */
  static nsEventStates GetContentState(mozilla::dom::Element* aElement,
                                       const TreeMatchContext& aTreeMatchContext);

  /*
   * Helper to get the content state for :visited handling for an element
   */
  static nsEventStates GetContentStateForVisitedHandling(
             mozilla::dom::Element* aElement,
             const TreeMatchContext& aTreeMatchContext,
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

  virtual NS_MUST_OVERRIDE size_t
    SizeOfExcludingThis(nsMallocSizeOfFun mallocSizeOf) const MOZ_OVERRIDE;
  virtual NS_MUST_OVERRIDE size_t
    SizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf) const MOZ_OVERRIDE;

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

  struct StateSelector {
    StateSelector(nsEventStates aStates, nsCSSSelector* aSelector)
      : mStates(aStates),
        mSelector(aSelector)
    {}

    nsEventStates mStates;
    nsCSSSelector* mSelector;
  };

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

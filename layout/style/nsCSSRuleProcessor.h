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

#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "nsIStyleRuleProcessor.h"
#include "nsCSSStyleSheet.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsRuleWalker.h"
#include "nsEventStates.h"

struct CascadeEnumData;
struct nsCSSSelector;
struct nsCSSSelectorList;
struct RuleCascadeData;
struct TreeMatchContext;
class nsCSSKeyframesRule;
class nsCSSPageRule;
class nsCSSFontFeatureValuesRule;

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

  // aScopeElement must be non-null iff aSheetType is
  // nsStyleSet::eScopedDocSheet.
  nsCSSRuleProcessor(const sheet_array_type& aSheets,
                     uint8_t aSheetType,
                     mozilla::dom::Element* aScopeElement);
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
  virtual void RulesMatching(ElementRuleProcessorData* aData) MOZ_OVERRIDE;

  virtual void RulesMatching(PseudoElementRuleProcessorData* aData) MOZ_OVERRIDE;

  virtual void RulesMatching(AnonBoxRuleProcessorData* aData) MOZ_OVERRIDE;

#ifdef MOZ_XUL
  virtual void RulesMatching(XULTreeRuleProcessorData* aData) MOZ_OVERRIDE;
#endif

  virtual nsRestyleHint HasStateDependentStyle(StateRuleProcessorData* aData) MOZ_OVERRIDE;

  virtual bool HasDocumentStateDependentStyle(StateRuleProcessorData* aData) MOZ_OVERRIDE;

  virtual nsRestyleHint
    HasAttributeDependentStyle(AttributeRuleProcessorData* aData) MOZ_OVERRIDE;

  virtual bool MediumFeaturesChanged(nsPresContext* aPresContext) MOZ_OVERRIDE;

  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf)
    const MOZ_MUST_OVERRIDE MOZ_OVERRIDE;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf)
    const MOZ_MUST_OVERRIDE MOZ_OVERRIDE;

  // Append all the currently-active font face rules to aArray.  Return
  // true for success and false for failure.
  bool AppendFontFaceRules(nsPresContext* aPresContext,
                           nsTArray<nsFontFaceRuleContainer>& aArray);

  nsCSSKeyframesRule* KeyframesRuleForName(nsPresContext* aPresContext,
                                           const nsString& aName);

  bool AppendPageRules(nsPresContext* aPresContext,
                       nsTArray<nsCSSPageRule*>& aArray);

  bool AppendFontFeatureValuesRules(nsPresContext* aPresContext,
                              nsTArray<nsCSSFontFeatureValuesRule*>& aArray);

  /**
   * Returns the scope element for the scoped style sheets this rule
   * processor is for.  If this is not a rule processor for scoped style
   * sheets, it returns null.
   */
  mozilla::dom::Element* GetScopeElement() const { return mScopeElement; }

#ifdef DEBUG
  void AssertQuirksChangeOK() {
    NS_ASSERTION(!mRuleCascades, "can't toggle quirks style sheet without "
                                 "clearing rule cascades");
  }
#endif

#ifdef XP_WIN
  // Cached theme identifier for the moz-windows-theme media query.
  static uint8_t GetWindowsThemeIdentifier();
  static void SetWindowsThemeIdentifier(uint8_t aId) { 
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

  // The scope element for this rule processor's scoped style sheets.
  // Only used if mSheetType == nsStyleSet::eScopedDocSheet.
  nsRefPtr<mozilla::dom::Element> mScopeElement;

  // type of stylesheet using this processor
  uint8_t mSheetType;  // == nsStyleSet::sheetType

#ifdef XP_WIN
  static uint8_t sWinThemeId;
#endif
};

#endif /* nsCSSRuleProcessor_h_ */

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
#include "mozilla/EventStates.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/RefCountType.h"
#include "mozilla/SheetType.h"
#include "mozilla/UniquePtr.h"
#include "nsExpirationTracker.h"
#include "nsMediaList.h"
#include "nsIStyleRuleProcessor.h"
#include "nsRuleWalker.h"
#include "nsTArray.h"

struct CascadeEnumData;
struct ElementDependentRuleProcessorData;
struct nsCSSSelector;
struct nsCSSSelectorList;
struct nsFontFaceRuleContainer;
struct RuleCascadeData;
struct TreeMatchContext;
class nsCSSKeyframesRule;
class nsCSSPageRule;
class nsCSSFontFeatureValuesRule;
class nsCSSCounterStyleRule;

namespace mozilla {
class CSSStyleSheet;
enum class CSSPseudoElementType : uint8_t;
enum class CSSPseudoClassType : uint8_t;
namespace css {
class DocumentRule;
} // namespace css
} // namespace mozilla

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
  typedef nsTArray<RefPtr<mozilla::CSSStyleSheet>> sheet_array_type;

  // aScopeElement must be non-null iff aSheetType is
  // SheetType::ScopedDoc.
  // aPreviousCSSRuleProcessor is the rule processor (if any) that this
  // one is replacing.
  nsCSSRuleProcessor(const sheet_array_type& aSheets,
                     mozilla::SheetType aSheetType,
                     mozilla::dom::Element* aScopeElement,
                     nsCSSRuleProcessor* aPreviousCSSRuleProcessor,
                     bool aIsShared = false);
  nsCSSRuleProcessor(sheet_array_type&& aSheets,
                     mozilla::SheetType aSheetType,
                     mozilla::dom::Element* aScopeElement,
                     nsCSSRuleProcessor* aPreviousCSSRuleProcessor,
                     bool aIsShared = false);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsCSSRuleProcessor)

public:
  nsresult ClearRuleCascades();

  static void Startup();
  static void InitSystemMetrics();
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
  static mozilla::EventStates GetContentState(
                                mozilla::dom::Element* aElement,
                                bool aUsingPrivateBrowsing);
  static mozilla::EventStates GetContentState(
                                mozilla::dom::Element* aElement,
                                const TreeMatchContext& aTreeMatchContext);
  static mozilla::EventStates GetContentState(
                                mozilla::dom::Element* aElement);

  /*
   * Helper to get the content state for :visited handling for an element
   */
  static mozilla::EventStates GetContentStateForVisitedHandling(
             mozilla::dom::Element* aElement,
             nsRuleWalker::VisitedHandlingType aVisitedHandling,
             bool aIsRelevantLink);

  /*
   * Helper to test whether a node is a link
   */
  static bool IsLink(const mozilla::dom::Element* aElement);

  /**
   * Returns true if the given aElement matches aSelector.
   * Like nsCSSRuleProcessor.cpp's SelectorMatches (and unlike
   * SelectorMatchesTree), this does not check an entire selector list
   * separated by combinators.
   *
   * :visited and :link will match both visited and non-visited links,
   * as if aTreeMatchContext->mVisitedHandling were eLinksVisitedOrUnvisited.
   *
   * aSelector is restricted to not containing pseudo-elements.
   */
  static bool RestrictedSelectorMatches(mozilla::dom::Element* aElement,
                                        nsCSSSelector* aSelector,
                                        TreeMatchContext& aTreeMatchContext);
  /**
   * Checks if a function-like ident-containing pseudo (:pseudo(ident))
   * matches a given element.
   *
   * Returns true if it parses and matches, Some(false) if it
   * parses but does not match. Asserts if it fails to parse; only
   * call this when you're sure it's a string-like pseudo.
   *
   * In Servo mode, please ensure that UpdatePossiblyStaleDocumentState()
   * has been called first.
   *
   * @param aElement The element we are trying to match
   * @param aPseudo The name of the pseudoselector
   * @param aString The identifier inside the pseudoselector (cannot be null)
   * @param aDocument The document
   * @param aForStyling Is this matching operation for the creation of a style context?
   *                    (For setting the slow selector flag)
   * @param aStateMask Mask containing states which we should exclude.
   *                   Ignored if aDependence is null
   * @param aSetSlowSelectorFlag Outparam, set if the caller is
   *                             supposed to set the slow selector flag.
   * @param aDependence Pointer to be set to true if we ignored a state due to
   *                    aStateMask. Can be null.
   */
  static bool StringPseudoMatches(const mozilla::dom::Element* aElement,
                                  mozilla::CSSPseudoClassType aPseudo,
                                  const char16_t* aString,
                                  const nsIDocument* aDocument,
                                  bool aForStyling,
                                  mozilla::EventStates aStateMask,
                                  bool* aSetSlowSelectorFlag,
                                  bool* const aDependence = nullptr);

  static bool LangPseudoMatches(const mozilla::dom::Element* aElement,
                                const nsIAtom* aOverrideLang,
                                bool aHasOverrideLang,
                                const char16_t* aString,
                                const nsIDocument* aDocument);

  // nsIStyleRuleProcessor
  virtual void RulesMatching(ElementRuleProcessorData* aData) override;

  virtual void RulesMatching(PseudoElementRuleProcessorData* aData) override;

  virtual void RulesMatching(AnonBoxRuleProcessorData* aData) override;

#ifdef MOZ_XUL
  virtual void RulesMatching(XULTreeRuleProcessorData* aData) override;
#endif

  virtual nsRestyleHint HasStateDependentStyle(StateRuleProcessorData* aData) override;
  virtual nsRestyleHint HasStateDependentStyle(PseudoElementStateRuleProcessorData* aData) override;

  virtual bool HasDocumentStateDependentStyle(StateRuleProcessorData* aData) override;

  virtual nsRestyleHint
    HasAttributeDependentStyle(AttributeRuleProcessorData* aData,
                               mozilla::RestyleHintData& aRestyleHintDataResult)
                                 override;

  virtual bool MediumFeaturesChanged(nsPresContext* aPresContext) override;

  /**
   * If this rule processor currently has a substantive media query
   * result cache key, return a copy of it.
   */
  mozilla::UniquePtr<nsMediaQueryResultCacheKey> CloneMQCacheKey();

  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf)
    const MOZ_MUST_OVERRIDE override;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf)
    const MOZ_MUST_OVERRIDE override;

  // Append all the currently-active font face rules to aArray.  Return
  // true for success and false for failure.
  bool AppendFontFaceRules(nsPresContext* aPresContext,
                           nsTArray<nsFontFaceRuleContainer>& aArray);

  nsCSSKeyframesRule* KeyframesRuleForName(nsPresContext* aPresContext,
                                           const nsString& aName);

  nsCSSCounterStyleRule* CounterStyleRuleForName(nsPresContext* aPresContext,
                                                 nsIAtom* aName);

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

  void TakeDocumentRulesAndCacheKey(
      nsPresContext* aPresContext,
      nsTArray<mozilla::css::DocumentRule*>& aDocumentRules,
      nsDocumentRuleResultCacheKey& aDocumentRuleResultCacheKey);

  bool IsShared() const { return mIsShared; }

  nsExpirationState* GetExpirationState() { return &mExpirationState; }
  void AddStyleSetRef();
  void ReleaseStyleSetRef();
  void SetInRuleProcessorCache(bool aVal) {
    MOZ_ASSERT(mIsShared);
    mInRuleProcessorCache = aVal;
  }
  bool IsInRuleProcessorCache() const { return mInRuleProcessorCache; }
  bool IsUsedByMultipleStyleSets() const { return mStyleSetRefCnt > 1; }

#ifdef XP_WIN
  // Cached theme identifier for the moz-windows-theme media query.
  static uint8_t GetWindowsThemeIdentifier();
  static void SetWindowsThemeIdentifier(uint8_t aId) { 
    sWinThemeId = aId;
  }
#endif

  struct StateSelector {
    StateSelector(mozilla::EventStates aStates, nsCSSSelector* aSelector)
      : mStates(aStates),
        mSelector(aSelector)
    {}

    mozilla::EventStates mStates;
    nsCSSSelector* mSelector;
  };

protected:
  virtual ~nsCSSRuleProcessor();

private:
  static bool CascadeSheet(mozilla::CSSStyleSheet* aSheet,
                           CascadeEnumData* aData);

  RuleCascadeData* GetRuleCascade(nsPresContext* aPresContext);
  void RefreshRuleCascade(nsPresContext* aPresContext);

  nsRestyleHint HasStateDependentStyle(ElementDependentRuleProcessorData* aData,
                                       mozilla::dom::Element* aStatefulElement,
                                       mozilla::CSSPseudoElementType aPseudoType,
                                       mozilla::EventStates aStateMask);

  void ClearSheets();

  // The sheet order here is the same as in nsStyleSet::mSheets
  sheet_array_type mSheets;

  // active first, then cached (most recent first)
  RuleCascadeData* mRuleCascades;

  // If we cleared our mRuleCascades or replaced a previous rule
  // processor, this is the media query result cache key that was used
  // before we lost the old rule cascades.
  mozilla::UniquePtr<nsMediaQueryResultCacheKey> mPreviousCacheKey;

  // The last pres context for which GetRuleCascades was called.
  nsPresContext *mLastPresContext;

  // The scope element for this rule processor's scoped style sheets.
  // Only used if mSheetType == nsStyleSet::eScopedDocSheet.
  RefPtr<mozilla::dom::Element> mScopeElement;

  nsTArray<mozilla::css::DocumentRule*> mDocumentRules;
  nsDocumentRuleResultCacheKey mDocumentCacheKey;

  nsExpirationState mExpirationState;
  MozRefCountType mStyleSetRefCnt;

  // type of stylesheet using this processor
  mozilla::SheetType mSheetType;

  const bool mIsShared;

  // Whether we need to build up mDocumentCacheKey and mDocumentRules as
  // we build a RuleCascadeData.  Is true only for shared rule processors
  // and only before we build the first RuleCascadeData.  See comment in
  // RefreshRuleCascade for why.
  bool mMustGatherDocumentRules;

  bool mInRuleProcessorCache;

#ifdef DEBUG
  bool mDocumentRulesAndCacheKeyValid;
#endif

#ifdef XP_WIN
  static uint8_t sWinThemeId;
#endif
};

#endif /* nsCSSRuleProcessor_h_ */

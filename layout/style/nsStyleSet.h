/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * the container for the style sheets that apply to a presentation, and
 * the internal API that the style system exposes for creating (and
 * potentially re-creating) style contexts
 */

#ifndef nsStyleSet_h_
#define nsStyleSet_h_

#include "mozilla/Attributes.h"

#include "nsIStyleRuleProcessor.h"
#include "nsCSSStyleSheet.h"
#include "nsBindingManager.h"
#include "nsRuleNode.h"
#include "nsTArray.h"
#include "nsCOMArray.h"
#include "nsAutoPtr.h"
#include "nsIStyleRule.h"
#include "nsCSSPseudoElements.h"
#include "gfxFontFeatures.h"

class nsIURI;
class nsCSSFontFaceRule;
class nsCSSKeyframesRule;
class nsCSSFontFeatureValuesRule;
class nsCSSPageRule;
class nsRuleWalker;
struct ElementDependentRuleProcessorData;
struct TreeMatchContext;

class nsEmptyStyleRule MOZ_FINAL : public nsIStyleRule
{
  NS_DECL_ISUPPORTS
  virtual void MapRuleInfoInto(nsRuleData* aRuleData) MOZ_OVERRIDE;
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const MOZ_OVERRIDE;
#endif
};

class nsInitialStyleRule MOZ_FINAL : public nsIStyleRule
{
  NS_DECL_ISUPPORTS
  virtual void MapRuleInfoInto(nsRuleData* aRuleData) MOZ_OVERRIDE;
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const MOZ_OVERRIDE;
#endif
};

// The style set object is created by the document viewer and ownership is
// then handed off to the PresShell.  Only the PresShell should delete a
// style set.

class nsStyleSet
{
 public:
  nsStyleSet();

  size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

  void Init(nsPresContext *aPresContext);

  nsRuleNode* GetRuleTree() { return mRuleTree; }

  // enable / disable the Quirk style sheet
  void EnableQuirkStyleSheet(bool aEnable);

  // get a style context for a non-pseudo frame.
  already_AddRefed<nsStyleContext>
  ResolveStyleFor(mozilla::dom::Element* aElement,
                  nsStyleContext* aParentContext);

  already_AddRefed<nsStyleContext>
  ResolveStyleFor(mozilla::dom::Element* aElement,
                  nsStyleContext* aParentContext,
                  TreeMatchContext& aTreeMatchContext);

  // Get a style context (with the given parent) for the
  // sequence of style rules in the |aRules| array.
  already_AddRefed<nsStyleContext>
  ResolveStyleForRules(nsStyleContext* aParentContext,
                       const nsTArray< nsCOMPtr<nsIStyleRule> > &aRules);

  // used in ResolveStyleForRules below
  struct RuleAndLevel
  {
    nsIStyleRule* mRule;
    uint8_t mLevel;
  };

  // Get a new style context for aElement for the rules in aRules
  // aRules is an array of rules and their levels in reverse order,
  // that is from the leaf-most to the root-most rule in the rule tree.
  already_AddRefed<nsStyleContext>
  ResolveStyleForRules(nsStyleContext* aParentContext,
                       nsStyleContext* aOldStyle,
                       const nsTArray<RuleAndLevel>& aRules);

  // Get a style context that represents aBaseContext, but as though
  // it additionally matched the rules in the aRules array (in that
  // order, as more specific than any other rules).
  already_AddRefed<nsStyleContext>
  ResolveStyleByAddingRules(nsStyleContext* aBaseContext,
                            const nsCOMArray<nsIStyleRule> &aRules);

  // Get a style context for a non-element (which no rules will match),
  // such as text nodes, placeholder frames, and the nsFirstLetterFrame
  // for everything after the first letter.
  //
  // Perhaps this should go away and we shouldn't even create style
  // contexts for such content nodes.  However, not doing any rule
  // matching for them is a first step.
  already_AddRefed<nsStyleContext>
  ResolveStyleForNonElement(nsStyleContext* aParentContext);

  // Get a style context for a pseudo-element.  aParentElement must be
  // non-null.  aPseudoID is the nsCSSPseudoElements::Type for the
  // pseudo-element.
  already_AddRefed<nsStyleContext>
  ResolvePseudoElementStyle(mozilla::dom::Element* aParentElement,
                            nsCSSPseudoElements::Type aType,
                            nsStyleContext* aParentContext);

  // This functions just like ResolvePseudoElementStyle except that it will
  // return nullptr if there are no explicit style rules for that
  // pseudo element.
  already_AddRefed<nsStyleContext>
  ProbePseudoElementStyle(mozilla::dom::Element* aParentElement,
                          nsCSSPseudoElements::Type aType,
                          nsStyleContext* aParentContext);
  already_AddRefed<nsStyleContext>
  ProbePseudoElementStyle(mozilla::dom::Element* aParentElement,
                          nsCSSPseudoElements::Type aType,
                          nsStyleContext* aParentContext,
                          TreeMatchContext& aTreeMatchContext);

  // Get a style context for an anonymous box.  aPseudoTag is the
  // pseudo-tag to use and must be non-null.
  already_AddRefed<nsStyleContext>
  ResolveAnonymousBoxStyle(nsIAtom* aPseudoTag, nsStyleContext* aParentContext);

#ifdef MOZ_XUL
  // Get a style context for a XUL tree pseudo.  aPseudoTag is the
  // pseudo-tag to use and must be non-null.  aParentContent must be
  // non-null.  aComparator must be non-null.
  already_AddRefed<nsStyleContext>
  ResolveXULTreePseudoStyle(mozilla::dom::Element* aParentElement,
                            nsIAtom* aPseudoTag,
                            nsStyleContext* aParentContext,
                            nsICSSPseudoComparator* aComparator);
#endif

  // Append all the currently-active font face rules to aArray.  Return
  // true for success and false for failure.
  bool AppendFontFaceRules(nsPresContext* aPresContext,
                             nsTArray<nsFontFaceRuleContainer>& aArray);

  // Append all the currently-active keyframes rules to aArray.  Return
  // true for success and false for failure.
  bool AppendKeyframesRules(nsPresContext* aPresContext,
                              nsTArray<nsCSSKeyframesRule*>& aArray);

  // Fetch object for looking up font feature values
  already_AddRefed<gfxFontFeatureValueSet> GetFontFeatureValuesLookup();

  // Append all the currently-active font feature values rules to aArray.
  // Return true for success and false for failure.
  bool AppendFontFeatureValuesRules(nsPresContext* aPresContext,
                              nsTArray<nsCSSFontFeatureValuesRule*>& aArray);

  // Append all the currently-active page rules to aArray.  Return
  // true for success and false for failure.
  bool AppendPageRules(nsPresContext* aPresContext,
                       nsTArray<nsCSSPageRule*>& aArray);

  // Begin ignoring style context destruction, to avoid lots of unnecessary
  // work on document teardown.
  void BeginShutdown(nsPresContext* aPresContext);

  // Free all of the data associated with this style set.
  void Shutdown(nsPresContext* aPresContext);

  // Notification that a style context is being destroyed.
  void NotifyStyleContextDestroyed(nsPresContext* aPresContext,
                                   nsStyleContext* aStyleContext);

  // Get a new style context that lives in a different parent
  // The new context will be the same as the old if the new parent is the
  // same as the old parent.
  // aElement should be non-null if this is a style context for an
  // element or pseudo-element; in the latter case it should be the
  // real element the pseudo-element is for.
  already_AddRefed<nsStyleContext>
  ReparentStyleContext(nsStyleContext* aStyleContext,
                       nsStyleContext* aNewParentContext,
                       mozilla::dom::Element* aElement);

  // Test if style is dependent on a document state.
  bool HasDocumentStateDependentStyle(nsPresContext* aPresContext,
                                        nsIContent*    aContent,
                                        nsEventStates  aStateMask);

  // Test if style is dependent on content state
  nsRestyleHint HasStateDependentStyle(nsPresContext* aPresContext,
                                       mozilla::dom::Element* aElement,
                                       nsEventStates aStateMask);

  // Test if style is dependent on the presence of an attribute.
  nsRestyleHint HasAttributeDependentStyle(nsPresContext* aPresContext,
                                           mozilla::dom::Element* aElement,
                                           nsIAtom*       aAttribute,
                                           int32_t        aModType,
                                           bool           aAttrHasChanged);

  /*
   * Do any processing that needs to happen as a result of a change in
   * the characteristics of the medium, and return whether style rules
   * may have changed as a result.
   */
  bool MediumFeaturesChanged(nsPresContext* aPresContext);

  // APIs for registering objects that can supply additional
  // rules during processing.
  void SetBindingManager(nsBindingManager* aBindingManager)
  {
    mBindingManager = aBindingManager;
  }

  // The "origins" of the CSS cascade, from lowest precedence to
  // highest (for non-!important rules).
  enum sheetType {
    eAgentSheet, // CSS
    eUserSheet, // CSS
    ePresHintSheet,
    eDocSheet, // CSS
    eScopedDocSheet,
    eStyleAttrSheet,
    eOverrideSheet, // CSS
    eAnimationSheet,
    eTransitionSheet,
    eSheetTypeCount
    // be sure to keep the number of bits in |mDirty| below and in
    // NS_RULE_NODE_LEVEL_MASK updated when changing the number of sheet
    // types
  };

  // APIs to manipulate the style sheet lists.  The sheets in each
  // list are stored with the most significant sheet last.
  nsresult AppendStyleSheet(sheetType aType, nsIStyleSheet *aSheet);
  nsresult PrependStyleSheet(sheetType aType, nsIStyleSheet *aSheet);
  nsresult RemoveStyleSheet(sheetType aType, nsIStyleSheet *aSheet);
  nsresult ReplaceSheets(sheetType aType,
                         const nsCOMArray<nsIStyleSheet> &aNewSheets);
  nsresult InsertStyleSheetBefore(sheetType aType, nsIStyleSheet *aNewSheet,
                                  nsIStyleSheet *aReferenceSheet);

  nsresult DirtyRuleProcessors(sheetType aType);

  // Enable/Disable entire author style level (Doc, ScopedDoc & PresHint levels)
  bool GetAuthorStyleDisabled();
  nsresult SetAuthorStyleDisabled(bool aStyleDisabled);

  int32_t SheetCount(sheetType aType) const {
    return mSheets[aType].Count();
  }

  nsIStyleSheet* StyleSheetAt(sheetType aType, int32_t aIndex) const {
    return mSheets[aType].ObjectAt(aIndex);
  }

  nsresult RemoveDocStyleSheet(nsIStyleSheet* aSheet);
  nsresult AddDocStyleSheet(nsIStyleSheet* aSheet, nsIDocument* aDocument);

  void     BeginUpdate();
  nsresult EndUpdate();

  // Methods for reconstructing the tree; BeginReconstruct basically moves the
  // old rule tree root and style context roots out of the way,
  // and EndReconstruct destroys the old rule tree when we're done
  nsresult BeginReconstruct();
  // Note: EndReconstruct should not be called if BeginReconstruct fails
  void EndReconstruct();

  // Let the style set know that a particular sheet is the quirks sheet.  This
  // sheet must already have been added to the UA sheets.  The pointer must not
  // be null.  This should only be called once for a given style set.
  void SetQuirkStyleSheet(nsIStyleSheet* aQuirkStyleSheet);

  // Return whether the rule tree has cached data such that we need to
  // do dynamic change handling for changes that change the results of
  // media queries or require rebuilding all style data.
  // We don't care whether we have cached rule processors or whether
  // they have cached rule cascades; getting the rule cascades again in
  // order to do rule matching will get the correct rule cascade.
  bool HasCachedStyleData() const {
    return (mRuleTree && mRuleTree->TreeHasCachedData()) || !mRoots.IsEmpty();
  }

  // Notify the style set that a rulenode is no longer in use, or was
  // just created and is not in use yet.
  void RuleNodeUnused() {
    ++mUnusedRuleNodeCount;
  }

  // Notify the style set that a rulenode that wasn't in use now is
  void RuleNodeInUse() {
    --mUnusedRuleNodeCount;
  }

  nsCSSStyleSheet::EnsureUniqueInnerResult EnsureUniqueInnerOnCSSSheets();

  nsIStyleRule* InitialStyleRule();

 private:
  nsStyleSet(const nsStyleSet& aCopy) MOZ_DELETE;
  nsStyleSet& operator=(const nsStyleSet& aCopy) MOZ_DELETE;

  // Run mark-and-sweep GC on mRuleTree and mOldRuleTrees, based on mRoots.
  void GCRuleTrees();

  // Update the rule processor list after a change to the style sheet list.
  nsresult GatherRuleProcessors(sheetType aType);

  void AddImportantRules(nsRuleNode* aCurrLevelNode,
                         nsRuleNode* aLastPrevLevelNode,
                         nsRuleWalker* aRuleWalker);

  // Move aRuleWalker forward by the appropriate rule if we need to add
  // a rule due to property restrictions on pseudo-elements.
  void WalkRestrictionRule(nsCSSPseudoElements::Type aPseudoType,
                           nsRuleWalker* aRuleWalker);

#ifdef DEBUG
  // Just like AddImportantRules except it doesn't actually add anything; it
  // just asserts that there are no important rules between aCurrLevelNode and
  // aLastPrevLevelNode.
  void AssertNoImportantRules(nsRuleNode* aCurrLevelNode,
                              nsRuleNode* aLastPrevLevelNode);
  
  // Just like AddImportantRules except it doesn't actually add anything; it
  // just asserts that there are no CSS rules between aCurrLevelNode and
  // aLastPrevLevelNode.  Mostly useful for the preshint level.
  void AssertNoCSSRules(nsRuleNode* aCurrLevelNode,
                        nsRuleNode* aLastPrevLevelNode);
#endif
  
  // Enumerate the rules in a way that cares about the order of the
  // rules.
  // aElement is the element the rules are for.  It might be null.  aData
  // is the closure to pass to aCollectorFunc.  If aContent is not null,
  // aData must be a RuleProcessorData*
  void FileRules(nsIStyleRuleProcessor::EnumFunc aCollectorFunc,
                 RuleProcessorData* aData, mozilla::dom::Element* aElement,
                 nsRuleWalker* aRuleWalker);

  // Enumerate all the rules in a way that doesn't care about the order
  // of the rules and break out if the enumeration is halted.
  void WalkRuleProcessors(nsIStyleRuleProcessor::EnumFunc aFunc,
                          ElementDependentRuleProcessorData* aData,
                          bool aWalkAllXBLStylesheets);

  /**
   * Bit-flags that can be passed to GetContext() in its parameter 'aFlags'.
   */
  enum {
    eNoFlags =          0,
    eIsLink =           1 << 0,
    eIsVisitedLink =    1 << 1,
    eDoAnimation =      1 << 2,

    // Indicates that we should skip the flex-item-specific chunk of
    // ApplyStyleFixups().  This is useful if our parent has "display: flex"
    // but we can tell it's not going to actually be a flex container (e.g. if
    // it's the outer frame of a button widget, and we're the inline frame for
    // the button's label).
    eSkipFlexItemStyleFixup = 1 << 3
  };

  already_AddRefed<nsStyleContext>
  GetContext(nsStyleContext* aParentContext,
             nsRuleNode* aRuleNode,
             nsRuleNode* aVisitedRuleNode,
             nsIAtom* aPseudoTag,
             nsCSSPseudoElements::Type aPseudoType,
             mozilla::dom::Element* aElementForAnimation,
             uint32_t aFlags);

  nsPresContext* PresContext() { return mRuleTree->PresContext(); }

  // The sheets in each array in mSheets are stored with the most significant
  // sheet last.
  // The arrays for ePresHintSheet, eStyleAttrSheet, eTransitionSheet,
  // and eAnimationSheet are always empty.  (FIXME:  We should reduce
  // the storage needed for them.)
  nsCOMArray<nsIStyleSheet> mSheets[eSheetTypeCount];

  // mRuleProcessors[eScopedDocSheet] is always null; rule processors
  // for scoped style sheets are stored in mScopedDocSheetRuleProcessors.
  nsCOMPtr<nsIStyleRuleProcessor> mRuleProcessors[eSheetTypeCount];

  // Rule processors for HTML5 scoped style sheets, one per scope.
  nsTArray<nsCOMPtr<nsIStyleRuleProcessor> > mScopedDocSheetRuleProcessors;

  // cached instance for enabling/disabling
  nsCOMPtr<nsIStyleSheet> mQuirkStyleSheet;

  nsRefPtr<nsBindingManager> mBindingManager;

  nsRuleNode* mRuleTree; // This is the root of our rule tree.  It is a
                         // lexicographic tree of matched rules that style
                         // contexts use to look up properties.

  uint16_t mBatching;

  unsigned mInShutdown : 1;
  unsigned mAuthorStyleDisabled: 1;
  unsigned mInReconstruct : 1;
  unsigned mInitFontFeatureValuesLookup : 1;
  unsigned mDirty : 9;  // one dirty bit is used per sheet type

  uint32_t mUnusedRuleNodeCount; // used to batch rule node GC
  nsTArray<nsStyleContext*> mRoots; // style contexts with no parent

  // Empty style rules to force things that restrict which properties
  // apply into different branches of the rule tree.
  nsRefPtr<nsEmptyStyleRule> mFirstLineRule, mFirstLetterRule, mPlaceholderRule;

  // Style rule which sets all properties to their initial values for
  // determining when context-sensitive values are in use.
  nsRefPtr<nsInitialStyleRule> mInitialStyleRule;

  // Old rule trees, which should only be non-empty between
  // BeginReconstruct and EndReconstruct, but in case of bugs that cause
  // style contexts to exist too long, may last longer.
  nsTArray<nsRuleNode*> mOldRuleTrees;

  // whether font feature values lookup object needs initialization
  nsRefPtr<gfxFontFeatureValueSet> mFontFeatureValuesLookup;
};

#ifdef _IMPL_NS_LAYOUT
inline
void nsRuleNode::AddRef()
{
  if (mRefCnt++ == 0 && !IsRoot()) {
    mPresContext->StyleSet()->RuleNodeInUse();
  }
}

inline
void nsRuleNode::Release()
{
  if (--mRefCnt == 0 && !IsRoot()) {
    mPresContext->StyleSet()->RuleNodeUnused();
  }
}
#endif

#endif

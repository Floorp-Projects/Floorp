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
#include "mozilla/CSSStyleSheet.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/SheetType.h"

#include "nsIStyleRuleProcessor.h"
#include "nsBindingManager.h"
#include "nsRuleNode.h"
#include "nsTArray.h"
#include "nsCOMArray.h"
#include "nsAutoPtr.h"
#include "nsIStyleRule.h"
#include "nsCSSPseudoElements.h"

class gfxFontFeatureValueSet;
class nsCSSKeyframesRule;
class nsCSSFontFeatureValuesRule;
class nsCSSPageRule;
class nsCSSCounterStyleRule;
class nsICSSPseudoComparator;
class nsRuleWalker;
struct ElementDependentRuleProcessorData;
struct nsFontFaceRuleContainer;
struct TreeMatchContext;

namespace mozilla {
class CSSStyleSheet;
class EventStates;
} // namespace mozilla

class nsEmptyStyleRule final : public nsIStyleRule
{
private:
  ~nsEmptyStyleRule() {}

public:
  NS_DECL_ISUPPORTS
  virtual void MapRuleInfoInto(nsRuleData* aRuleData) override;
  virtual bool MightMapInheritedStyleData() override;
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
};

class nsInitialStyleRule final : public nsIStyleRule
{
private:
  ~nsInitialStyleRule() {}

public:
  NS_DECL_ISUPPORTS
  virtual void MapRuleInfoInto(nsRuleData* aRuleData) override;
  virtual bool MightMapInheritedStyleData() override;
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
};

class nsDisableTextZoomStyleRule final : public nsIStyleRule
{
private:
  ~nsDisableTextZoomStyleRule() {}

public:
  NS_DECL_ISUPPORTS
  virtual void MapRuleInfoInto(nsRuleData* aRuleData) override;
  virtual bool MightMapInheritedStyleData() override;
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif
};

// The style set object is created by the document viewer and ownership is
// then handed off to the PresShell.  Only the PresShell should delete a
// style set.

class nsStyleSet final
{
 public:
  nsStyleSet();
  ~nsStyleSet();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  void Init(nsPresContext *aPresContext);

  nsRuleNode* GetRuleTree() { return mRuleTree; }

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

  // Get a style context that represents aBaseContext, but as though
  // it additionally matched the rules in the aRules array (in that
  // order, as more specific than any other rules).
  //
  // One of the following must hold:
  // 1. The resulting style context must be used only on a temporary
  //    basis, and it must never be put into the style context tree
  //    (and, in particular, we must never call
  //    ResolveStyleWithReplacement with it as the old context, which
  //    might happen if it is put in the style context tree), or
  // 2. The additional rules must be appropriate for the transitions
  //    level of the cascade, which is the highest level of the cascade.
  //    (This is the case for one current caller, the cover rule used
  //    for CSS transitions.)
  already_AddRefed<nsStyleContext>
  ResolveStyleByAddingRules(nsStyleContext* aBaseContext,
                            const nsCOMArray<nsIStyleRule> &aRules);

  // Resolve style by making replacements in the list of style rules as
  // described by aReplacements, but otherwise maintaining the status
  // quo.
  // aPseudoElement must follow the same rules as for
  // ResolvePseudoElementStyle, and be null for non-pseudo-element cases
  enum { // flags for aFlags
    // Skip starting CSS animations that result from the style.
    eSkipStartingAnimations = (1<<0),
  };
  already_AddRefed<nsStyleContext>
  ResolveStyleWithReplacement(mozilla::dom::Element* aElement,
                              mozilla::dom::Element* aPseudoElement,
                              nsStyleContext* aNewParentContext,
                              nsStyleContext* aOldStyleContext,
                              nsRestyleHint aReplacements,
                              uint32_t aFlags = 0);

  // Resolve style by returning a style context with the specified
  // animation data removed.  It is allowable to remove all animation
  // data with eRestyle_AllHintsWithAnimations, or by using any other
  // hints that are allowed by ResolveStyleWithReplacement.
  already_AddRefed<nsStyleContext>
    ResolveStyleWithoutAnimation(mozilla::dom::Element* aElement,
                                 nsStyleContext* aStyleContext,
                                 nsRestyleHint aWhichToRemove);

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
  // pseudo-element.  aPseudoElement must be non-null if the pseudo-element
  // type is one that allows user action pseudo-classes after it or allows
  // style attributes; otherwise, it is ignored.
  already_AddRefed<nsStyleContext>
  ResolvePseudoElementStyle(mozilla::dom::Element* aParentElement,
                            nsCSSPseudoElements::Type aType,
                            nsStyleContext* aParentContext,
                            mozilla::dom::Element* aPseudoElement);

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
                          TreeMatchContext& aTreeMatchContext,
                          mozilla::dom::Element* aPseudoElement = nullptr);

  /**
   * Bit-flags that can be passed to ResolveAnonymousBoxStyle and GetContext
   * in their parameter 'aFlags'.
   */
  enum {
    eNoFlags =          0,
    eIsLink =           1 << 0,
    eIsVisitedLink =    1 << 1,
    eDoAnimation =      1 << 2,

    // Indicates that we should skip the flex/grid item specific chunk of
    // ApplyStyleFixups().  This is useful if our parent has "display: flex"
    // or "display: grid" but we can tell we're not going to honor that (e.g. if
    // it's the outer frame of a button widget, and we're the inline frame for
    // the button's label).
    eSkipParentDisplayBasedStyleFixup = 1 << 3
  };

  // Get a style context for an anonymous box.  aPseudoTag is the
  // pseudo-tag to use and must be non-null.  aFlags will be forwarded
  // to a GetContext call internally.
  already_AddRefed<nsStyleContext>
  ResolveAnonymousBoxStyle(nsIAtom* aPseudoTag, nsStyleContext* aParentContext,
                           uint32_t aFlags = eNoFlags);

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
  bool AppendFontFaceRules(nsTArray<nsFontFaceRuleContainer>& aArray);

  // Return the winning (in the cascade) @keyframes rule for the given name.
  nsCSSKeyframesRule* KeyframesRuleForName(const nsString& aName);

  // Return the winning (in the cascade) @counter-style rule for the given name.
  nsCSSCounterStyleRule* CounterStyleRuleForName(const nsAString& aName);

  // Fetch object for looking up font feature values
  already_AddRefed<gfxFontFeatureValueSet> GetFontFeatureValuesLookup();

  // Append all the currently-active font feature values rules to aArray.
  // Return true for success and false for failure.
  bool AppendFontFeatureValuesRules(
                              nsTArray<nsCSSFontFeatureValuesRule*>& aArray);

  // Append all the currently-active page rules to aArray.  Return
  // true for success and false for failure.
  bool AppendPageRules(nsTArray<nsCSSPageRule*>& aArray);

  // Begin ignoring style context destruction, to avoid lots of unnecessary
  // work on document teardown.
  void BeginShutdown();

  // Free all of the data associated with this style set.
  void Shutdown();

  // Notification that a style context is being destroyed.
  void NotifyStyleContextDestroyed(nsStyleContext* aStyleContext);

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
  bool HasDocumentStateDependentStyle(nsIContent*    aContent,
                                      mozilla::EventStates aStateMask);

  // Test if style is dependent on content state
  nsRestyleHint HasStateDependentStyle(mozilla::dom::Element* aElement,
                                       mozilla::EventStates aStateMask);
  nsRestyleHint HasStateDependentStyle(mozilla::dom::Element* aElement,
                                       nsCSSPseudoElements::Type aPseudoType,
                                       mozilla::dom::Element* aPseudoElement,
                                       mozilla::EventStates aStateMask);

  // Test if style is dependent on the presence of an attribute.
  nsRestyleHint HasAttributeDependentStyle(mozilla::dom::Element* aElement,
                                           nsIAtom*       aAttribute,
                                           int32_t        aModType,
                                           bool           aAttrHasChanged,
                                           const nsAttrValue* aOtherValue,
                                           mozilla::RestyleHintData&
                                             aRestyleHintDataResult);

  /*
   * Do any processing that needs to happen as a result of a change in
   * the characteristics of the medium, and return whether style rules
   * may have changed as a result.
   */
  bool MediumFeaturesChanged();

  // APIs for registering objects that can supply additional
  // rules during processing.
  void SetBindingManager(nsBindingManager* aBindingManager)
  {
    mBindingManager = aBindingManager;
  }

  // APIs to manipulate the style sheet lists.  The sheets in each
  // list are stored with the most significant sheet last.
  nsresult AppendStyleSheet(mozilla::SheetType aType,
                            mozilla::CSSStyleSheet* aSheet);
  nsresult PrependStyleSheet(mozilla::SheetType aType,
                             mozilla::CSSStyleSheet* aSheet);
  nsresult RemoveStyleSheet(mozilla::SheetType aType,
                            mozilla::CSSStyleSheet* aSheet);
  nsresult ReplaceSheets(mozilla::SheetType aType,
                         const nsTArray<RefPtr<mozilla::CSSStyleSheet>>& aNewSheets);
  nsresult InsertStyleSheetBefore(mozilla::SheetType aType,
                                  mozilla::CSSStyleSheet* aNewSheet,
                                  mozilla::CSSStyleSheet* aReferenceSheet);

  // Enable/Disable entire author style level (Doc, ScopedDoc & PresHint levels)
  bool GetAuthorStyleDisabled();
  nsresult SetAuthorStyleDisabled(bool aStyleDisabled);

  int32_t SheetCount(mozilla::SheetType aType) const {
    return mSheets[aType].Length();
  }

  mozilla::CSSStyleSheet* StyleSheetAt(mozilla::SheetType aType,
                                       int32_t aIndex) const {
    return mSheets[aType][aIndex];
  }

  void AppendAllXBLStyleSheets(nsTArray<mozilla::CSSStyleSheet*>& aArray) const {
    if (mBindingManager) {
      mBindingManager->AppendAllSheets(aArray);
    }
  }

  nsresult RemoveDocStyleSheet(mozilla::CSSStyleSheet* aSheet);
  nsresult AddDocStyleSheet(mozilla::CSSStyleSheet* aSheet,
                            nsIDocument* aDocument);

  void     BeginUpdate();
  nsresult EndUpdate();

  // Methods for reconstructing the tree; BeginReconstruct basically moves the
  // old rule tree root and style context roots out of the way,
  // and EndReconstruct destroys the old rule tree when we're done
  nsresult BeginReconstruct();
  // Note: EndReconstruct should not be called if BeginReconstruct fails
  void EndReconstruct();

  bool IsInRuleTreeReconstruct() const {
    return mInReconstruct;
  }

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

  // Returns true if a restyle of the document is needed due to cloning
  // sheet inners.
  bool EnsureUniqueInnerOnCSSSheets();

  // Called by CSSStyleSheet::EnsureUniqueInner to let us know it cloned
  // its inner.
  void SetNeedsRestyleAfterEnsureUniqueInner() {
    mNeedsRestyleAfterEnsureUniqueInner = true;
  }

  nsIStyleRule* InitialStyleRule();

  bool HasRuleProcessorUsedByMultipleStyleSets(mozilla::SheetType aSheetType);

  // Tells the RestyleManager for the document using this style set
  // to drop any nsCSSSelector pointers it has.
  void ClearSelectors();

private:
  nsStyleSet(const nsStyleSet& aCopy) = delete;
  nsStyleSet& operator=(const nsStyleSet& aCopy) = delete;

  // Run mark-and-sweep GC on mRuleTree and mOldRuleTrees, based on mRoots.
  void GCRuleTrees();

  nsresult DirtyRuleProcessors(mozilla::SheetType aType);

  // Update the rule processor list after a change to the style sheet list.
  nsresult GatherRuleProcessors(mozilla::SheetType aType);

  void AddImportantRules(nsRuleNode* aCurrLevelNode,
                         nsRuleNode* aLastPrevLevelNode,
                         nsRuleWalker* aRuleWalker);

  // Move aRuleWalker forward by the appropriate rule if we need to add
  // a rule due to property restrictions on pseudo-elements.
  void WalkRestrictionRule(nsCSSPseudoElements::Type aPseudoType,
                           nsRuleWalker* aRuleWalker);

  void WalkDisableTextZoomRule(mozilla::dom::Element* aElement,
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

  // Helper for ResolveStyleWithReplacement
  // aPseudoElement must follow the same rules as for
  // ResolvePseudoElementStyle, and be null for non-pseudo-element cases
  nsRuleNode* RuleNodeWithReplacement(mozilla::dom::Element* aElement,
                                      mozilla::dom::Element* aPseudoElement,
                                      nsRuleNode* aOldRuleNode,
                                      nsCSSPseudoElements::Type aPseudoType,
                                      nsRestyleHint aReplacements);

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
  // eAnimationSheet and eSVGAttrAnimationSheet are always empty.
  // (FIXME:  We should reduce the storage needed for them.)
  mozilla::EnumeratedArray<mozilla::SheetType, mozilla::SheetType::Count,
                           nsTArray<RefPtr<mozilla::CSSStyleSheet>>> mSheets;

  // mRuleProcessors[eScopedDocSheet] is always null; rule processors
  // for scoped style sheets are stored in mScopedDocSheetRuleProcessors.
  mozilla::EnumeratedArray<mozilla::SheetType, mozilla::SheetType::Count,
                           nsCOMPtr<nsIStyleRuleProcessor>> mRuleProcessors;

  // Rule processors for HTML5 scoped style sheets, one per scope.
  nsTArray<nsCOMPtr<nsIStyleRuleProcessor> > mScopedDocSheetRuleProcessors;

  RefPtr<nsBindingManager> mBindingManager;

  nsRuleNode* mRuleTree; // This is the root of our rule tree.  It is a
                         // lexicographic tree of matched rules that style
                         // contexts use to look up properties.

  uint16_t mBatching;

  unsigned mInShutdown : 1;
  unsigned mAuthorStyleDisabled: 1;
  unsigned mInReconstruct : 1;
  unsigned mInitFontFeatureValuesLookup : 1;
  unsigned mNeedsRestyleAfterEnsureUniqueInner : 1;
  unsigned mDirty : int(mozilla::SheetType::Count);  // one bit per sheet type

  uint32_t mUnusedRuleNodeCount; // used to batch rule node GC
  nsTArray<nsStyleContext*> mRoots; // style contexts with no parent

  // Empty style rules to force things that restrict which properties
  // apply into different branches of the rule tree.
  RefPtr<nsEmptyStyleRule> mFirstLineRule, mFirstLetterRule, mPlaceholderRule;

  // Style rule which sets all properties to their initial values for
  // determining when context-sensitive values are in use.
  RefPtr<nsInitialStyleRule> mInitialStyleRule;

  // Style rule that sets the internal -x-text-zoom property on
  // <svg:text> elements to disable the effect of text zooming.
  RefPtr<nsDisableTextZoomStyleRule> mDisableTextZoomStyleRule;

  // Old rule trees, which should only be non-empty between
  // BeginReconstruct and EndReconstruct, but in case of bugs that cause
  // style contexts to exist too long, may last longer.
  nsTArray<nsRuleNode*> mOldRuleTrees;

  // whether font feature values lookup object needs initialization
  RefPtr<gfxFontFeatureValueSet> mFontFeatureValuesLookup;
};

#ifdef MOZILLA_INTERNAL_API
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

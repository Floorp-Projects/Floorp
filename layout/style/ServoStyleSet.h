/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoStyleSet_h
#define mozilla_ServoStyleSet_h

#include "mozilla/EffectCompositor.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/EventStates.h"
#include "mozilla/PostTraversalTask.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/ServoElementSnapshot.h"
#include "mozilla/ServoUtils.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/SheetType.h"
#include "mozilla/UniquePtr.h"
#include "MainThreadUtils.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSAnonBoxes.h"
#include "nsChangeHint.h"
#include "nsIAtom.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
class CSSStyleSheet;
class ServoRestyleManager;
class ServoStyleSheet;
struct Keyframe;
struct ServoComputedValuesWithParent;
class ServoElementSnapshotTable;
} // namespace mozilla
class nsCSSCounterStyleRule;
class nsIContent;
class nsIDocument;
class nsStyleContext;
class nsPresContext;
struct nsTimingFunction;
struct RawServoRuleNode;
struct TreeMatchContext;

namespace mozilla {

/**
 * The set of style sheets that apply to a document, backed by a Servo
 * Stylist.  A ServoStyleSet contains ServoStyleSheets.
 */
class ServoStyleSet
{
  friend class ServoRestyleManager;
  typedef ServoElementSnapshotTable SnapshotTable;

public:
  class AutoAllowStaleStyles
  {
  public:
    explicit AutoAllowStaleStyles(ServoStyleSet* aStyleSet)
      : mStyleSet(aStyleSet)
    {
      if (mStyleSet) {
        MOZ_ASSERT(!mStyleSet->mAllowResolveStaleStyles);
        mStyleSet->mAllowResolveStaleStyles = true;
      }
    }

    ~AutoAllowStaleStyles()
    {
      if (mStyleSet) {
        mStyleSet->mAllowResolveStaleStyles = false;
      }
    }

  private:
    ServoStyleSet* mStyleSet;
  };

  static bool IsInServoTraversal()
  {
    // The callers of this function are generally main-thread-only _except_
    // for potentially running during the Servo traversal, in which case they may
    // take special paths that avoid writing to caches and the like. In order
    // to allow those callers to branch efficiently without checking TLS, we
    // maintain this static boolean. However, the danger is that those callers
    // are generally unprepared to deal with non-Servo-but-also-non-main-thread
    // callers, and are likely to take the main-thread codepath if this function
    // returns false. So we assert against other non-main-thread callers here.
    MOZ_ASSERT(sInServoTraversal || NS_IsMainThread());
    return sInServoTraversal;
  }

  static ServoStyleSet* Current()
  {
    return sInServoTraversal;
  }

  ServoStyleSet();
  ~ServoStyleSet();

  void Init(nsPresContext* aPresContext);
  void BeginShutdown();
  void Shutdown();

  void RecordStyleSheetChange(mozilla::ServoStyleSheet*, StyleSheet::ChangeType)
  {
    // TODO(emilio): Record which kind of changes have we handled, and act
    // properly in InvalidateStyleForCSSRuleChanges instead of invalidating the
    // whole document.
    NoteStyleSheetsChanged();
  }

  void RecordShadowStyleChange(mozilla::dom::ShadowRoot* aShadowRoot) {
    // FIXME(emilio): When we properly support shadow dom we'll need to do
    // better.
    NoteStyleSheetsChanged();
  }

  bool StyleSheetsHaveChanged() const
  {
    return StylistNeedsUpdate();
  }

  void InvalidateStyleForCSSRuleChanges();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  const RawServoStyleSet& RawSet() const { return *mRawSet; }

  bool GetAuthorStyleDisabled() const;
  nsresult SetAuthorStyleDisabled(bool aStyleDisabled);

  void BeginUpdate();
  nsresult EndUpdate();

  already_AddRefed<nsStyleContext>
  ResolveStyleFor(dom::Element* aElement,
                  nsStyleContext* aParentContext,
                  LazyComputeBehavior aMayCompute);

  // Get a style context for a text node (which no rules will match).
  //
  // The returned style context will have nsCSSAnonBoxes::mozText as its pseudo.
  //
  // (Perhaps mozText should go away and we shouldn't even create style
  // contexts for such content nodes, when text-combine-upright is not
  // present.  However, not doing any rule matching for them is a first step.)
  already_AddRefed<nsStyleContext>
  ResolveStyleForText(nsIContent* aTextNode,
                      nsStyleContext* aParentContext);

  // Get a style context for a first-letter continuation (which no rules will
  // match).
  //
  // The returned style context will have
  // nsCSSAnonBoxes::firstLetterContinuation as its pseudo.
  //
  // (Perhaps nsCSSAnonBoxes::firstLetterContinuation should go away and we
  // shouldn't even create style contexts for such frames.  However, not doing
  // any rule matching for them is a first step.  And right now we do use this
  // style context for some things)
  already_AddRefed<nsStyleContext>
  ResolveStyleForFirstLetterContinuation(nsStyleContext* aParentContext);

  // Get a style context for a placeholder frame (which no rules will match).
  //
  // The returned style context will have nsCSSAnonBoxes::oofPlaceholder as
  // its pseudo.
  //
  // (Perhaps nsCSSAnonBoxes::oofPaceholder should go away and we shouldn't even
  // create style contexts for placeholders.  However, not doing any rule
  // matching for them is a first step.)
  already_AddRefed<nsStyleContext>
  ResolveStyleForPlaceholder();

  // Get a style context for a pseudo-element.  aParentElement must be
  // non-null.  aPseudoID is the CSSPseudoElementType for the
  // pseudo-element.  aPseudoElement must be non-null if the pseudo-element
  // type is one that allows user action pseudo-classes after it or allows
  // style attributes; otherwise, it is ignored.
  already_AddRefed<nsStyleContext>
  ResolvePseudoElementStyle(dom::Element* aOriginatingElement,
                            CSSPseudoElementType aType,
                            nsStyleContext* aParentContext,
                            dom::Element* aPseudoElement);

  // Resolves style for a (possibly-pseudo) Element without assuming that the
  // style has been resolved, and without worrying about setting the style
  // context up to live in the style context tree (a null parent is used).
  // |aPeudoTag| and |aPseudoType| must match.
  already_AddRefed<nsStyleContext>
  ResolveTransientStyle(dom::Element* aElement,
                        nsIAtom* aPseudoTag,
                        CSSPseudoElementType aPseudoType,
                        StyleRuleInclusion aRules =
                          StyleRuleInclusion::All);

  // Similar to ResolveTransientStyle() but returns ServoComputedValues.
  // Unlike ResolveServoStyle() this function calls PreTraverseSync().
  already_AddRefed<ServoComputedValues>
  ResolveTransientServoStyle(dom::Element* aElement,
                             CSSPseudoElementType aPseudoTag,
                             StyleRuleInclusion aRules =
                               StyleRuleInclusion::All);

  // Get a style context for an anonymous box.  aPseudoTag is the pseudo-tag to
  // use and must be non-null.  It must be an anon box, and must be one that
  // inherits style from the given aParentContext.
  already_AddRefed<nsStyleContext>
  ResolveInheritingAnonymousBoxStyle(nsIAtom* aPseudoTag,
                                     nsStyleContext* aParentContext);

  // Get a style context for an anonymous box that does not inherit style from
  // anything.  aPseudoTag is the pseudo-tag to use and must be non-null.  It
  // must be an anon box, and must be a non-inheriting one.
  already_AddRefed<nsStyleContext>
  ResolveNonInheritingAnonymousBoxStyle(nsIAtom* aPseudoTag);

  // manage the set of style sheets in the style set
  nsresult AppendStyleSheet(SheetType aType, ServoStyleSheet* aSheet);
  nsresult PrependStyleSheet(SheetType aType, ServoStyleSheet* aSheet);
  nsresult RemoveStyleSheet(SheetType aType, ServoStyleSheet* aSheet);
  nsresult ReplaceSheets(SheetType aType,
                         const nsTArray<RefPtr<ServoStyleSheet>>& aNewSheets);
  nsresult InsertStyleSheetBefore(SheetType aType,
                                  ServoStyleSheet* aNewSheet,
                                  ServoStyleSheet* aReferenceSheet);

  int32_t SheetCount(SheetType aType) const;
  ServoStyleSheet* StyleSheetAt(SheetType aType, int32_t aIndex) const;

  nsresult RemoveDocStyleSheet(ServoStyleSheet* aSheet);
  nsresult AddDocStyleSheet(ServoStyleSheet* aSheet, nsIDocument* aDocument);

  // check whether there is ::before/::after style for an element
  already_AddRefed<nsStyleContext>
  ProbePseudoElementStyle(dom::Element* aOriginatingElement,
                          mozilla::CSSPseudoElementType aType,
                          nsStyleContext* aParentContext,
                          dom::Element* aPseudoElement = nullptr);

  // Test if style is dependent on content state
  nsRestyleHint HasStateDependentStyle(dom::Element* aElement,
                                       EventStates aStateMask);
  nsRestyleHint HasStateDependentStyle(
    dom::Element* aElement, mozilla::CSSPseudoElementType aPseudoType,
    dom::Element* aPseudoElement, EventStates aStateMask);

  /**
   * Performs a Servo traversal to compute style for all dirty nodes in the
   * document.
   *
   * This will traverse all of the document's style roots (that is, its document
   * element, and the roots of the document-level native anonymous content).
   *
   * |aRestyleBehavior| should be `Normal` or `ForCSSRuleChanges`.
   * We need to specify |ForCSSRuleChanges| to try to update all CSS animations
   * when we call this function due to CSS rule changes since @keyframes rules
   * may have changed.
   *
   * Returns true if a post-traversal is required.
   */
  bool StyleDocument(TraversalRestyleBehavior aRestyleBehavior);

  /**
   * Performs a Servo animation-only traversal to compute style for all nodes
   * with the animation-only dirty bit in the document.
   *
   * This will traverse all of the document's style roots (that is, its document
   * element, and the roots of the document-level native anonymous content).
   */
  bool StyleDocumentForAnimationOnly();

  /**
   * Eagerly styles a subtree of unstyled nodes that was just appended to the
   * tree. This is used in situations where we need the style immediately and
   * cannot wait for a future batch restyle.
   */
  void StyleNewSubtree(dom::Element* aRoot);

  /**
   * Like the above, but skips the root node, and only styles unstyled children.
   * When potentially appending multiple children, it's preferable to call
   * StyleNewChildren on the node rather than making multiple calls to
   * StyleNewSubtree on each child, since it allows for more parallelism.
   */
  void StyleNewChildren(dom::Element* aParent);

  /**
   * Like StyleNewSubtree, but in response to a request to reconstruct frames
   * for the given subtree, and so works on elements that already have
   * styles.  This will leave the subtree in a state just like after an initial
   * styling, i.e. with new styles, no change hints, and with the dirty
   * descendants bits cleared.  No comparison of old and new styles is done,
   * so no change hints will be processed.
   */
  void StyleSubtreeForReconstruct(dom::Element* aRoot);

  /**
   * Records that the contents of style sheets have changed since the last
   * restyle.  Calling this will ensure that the Stylist rebuilds its
   * selector maps.
   */
  void NoteStyleSheetsChanged();

  /**
   * Helper for correctly calling RebuildStylist without paying the cost of an
   * extra function call in the common no-rebuild-needed case.
   */
  void UpdateStylistIfNeeded()
  {
    if (StylistNeedsUpdate()) {
      UpdateStylist();
    }
  }

#ifdef DEBUG
  void AssertTreeIsClean();
#else
  void AssertTreeIsClean() {}
#endif

  /**
   * Clears the style data, both style sheet data and cached non-inheriting
   * style contexts, and marks the stylist as needing an unconditional full
   * rebuild, including a device reset.
   */
  void ClearDataAndMarkDeviceDirty();

  /**
   * Resolve style for the given element, and return it as a
   * ServoComputedValues, not an nsStyleContext.
   */
  already_AddRefed<ServoComputedValues> ResolveServoStyle(dom::Element* aElement);

  bool GetKeyframesForName(const nsString& aName,
                           const nsTimingFunction& aTimingFunction,
                           const ServoComputedValues* aComputedValues,
                           nsTArray<Keyframe>& aKeyframes);

  nsTArray<ComputedKeyframeValues>
  GetComputedKeyframeValuesFor(const nsTArray<Keyframe>& aKeyframes,
                               dom::Element* aElement,
                               const ServoComputedValuesWithParent&
                                 aServoValues);

  bool AppendFontFaceRules(nsTArray<nsFontFaceRuleContainer>& aArray);

  nsCSSCounterStyleRule* CounterStyleRuleForName(nsIAtom* aName);

  already_AddRefed<ServoComputedValues>
  GetBaseComputedValuesForElement(dom::Element* aElement,
                                  CSSPseudoElementType aPseudoType);

  /**
   * Resolve style for a given declaration block with/without the parent style.
   * If the parent style is not specified, the document default computed values
   * is used.
   */
  already_AddRefed<ServoComputedValues>
  ResolveForDeclarations(ServoComputedValuesBorrowedOrNull aParentOrNull,
                         RawServoDeclarationBlockBorrowed aDeclarations);

  already_AddRefed<RawServoAnimationValue>
  ComputeAnimationValue(RawServoDeclarationBlock* aDeclaration,
                        const ServoComputedValuesWithParent& aComputedValues);

  void AppendTask(PostTraversalTask aTask)
  {
    MOZ_ASSERT(IsInServoTraversal());

    // We currently only use PostTraversalTasks while the Servo font metrics
    // mutex is locked.  If we need to use them in other situations during
    // a traversal, we should assert that we've taken appropriate
    // synchronization measures.
    AssertIsMainThreadOrServoFontMetricsLocked();

    mPostTraversalTasks.AppendElement(aTask);
  }

  // Returns true if a restyle of the document is needed due to cloning
  // sheet inners.
  bool EnsureUniqueInnerOnCSSSheets();

  // Called by StyleSheet::EnsureUniqueInner to let us know it cloned
  // its inner.
  void SetNeedsRestyleAfterEnsureUniqueInner() {
    mNeedsRestyleAfterEnsureUniqueInner = true;
  }

private:
  // On construction, sets sInServoTraversal to the given ServoStyleSet.
  // On destruction, clears sInServoTraversal and calls RunPostTraversalTasks.
  class MOZ_STACK_CLASS AutoSetInServoTraversal
  {
  public:
    explicit AutoSetInServoTraversal(ServoStyleSet* aSet)
      : mSet(aSet)
    {
      MOZ_ASSERT(!sInServoTraversal);
      MOZ_ASSERT(aSet);
      sInServoTraversal = aSet;
    }

    ~AutoSetInServoTraversal()
    {
      MOZ_ASSERT(sInServoTraversal);
      sInServoTraversal = nullptr;
      mSet->RunPostTraversalTasks();
    }

  private:
    ServoStyleSet* mSet;
  };

  already_AddRefed<nsStyleContext> GetContext(already_AddRefed<ServoComputedValues>,
                                              nsStyleContext* aParentContext,
                                              nsIAtom* aPseudoTag,
                                              CSSPseudoElementType aPseudoType,
                                              dom::Element* aElementForAnimation);

  already_AddRefed<nsStyleContext> GetContext(nsIContent* aContent,
                                              nsStyleContext* aParentContext,
                                              nsIAtom* aPseudoTag,
                                              CSSPseudoElementType aPseudoType,
                                              LazyComputeBehavior aMayCompute);

  /**
   * Rebuild the style data. This will force a stylesheet flush, and also
   * recompute the default computed styles.
   */
  void RebuildData();

  /**
   * Gets the pending snapshots to handle from the restyle manager.
   */
  const SnapshotTable& Snapshots();

  /**
   * Resolve all ServoDeclarationBlocks attached to mapped
   * presentation attributes cached on the document.
   *
   * Call this before jumping into Servo's style system.
   */
  void ResolveMappedAttrDeclarationBlocks();

  /**
   * Perform all lazy operations required before traversing
   * a subtree.
   *
   * Returns whether a post-traversal is required.
   */
  bool PrepareAndTraverseSubtree(RawGeckoElementBorrowed aRoot,
                                 TraversalRootBehavior aRootBehavior,
                                 TraversalRestyleBehavior aRestyleBehavior);

  /**
   * Clear our cached mNonInheritingStyleContexts.
   *
   * We do this when we want to make sure those style contexts won't live too
   * long (e.g. when rebuilding all style data or when shutting down the style
   * set).
   */
  void ClearNonInheritingStyleContexts();

  /**
   * Perform processes that we should do before traversing.
   *
   * When aRoot is null, the entire document is pre-traversed.  Otherwise,
   * only the subtree rooted at aRoot is pre-traversed.
   */
  void PreTraverse(dom::Element* aRoot = nullptr,
                   EffectCompositor::AnimationRestyleType =
                     EffectCompositor::AnimationRestyleType::Throttled);
  // Subset of the pre-traverse steps that involve syncing up data
  void PreTraverseSync();

  /**
   * A tri-state used to track which kind of stylist state we may need to
   * update.
   */
  enum class StylistState : uint8_t {
    /** The stylist is not dirty, we should do nothing */
    NotDirty,
    /** The style sheets have changed, so we need to update the style data. */
    StyleSheetsDirty,
    /**
     * All style data is dirty and both style sheet data and default computed
     * values need to be recomputed.
     */
    FullyDirty,
  };

  /**
   * Note that the stylist needs a style flush due to style sheet changes.
   */
  void SetStylistStyleSheetsDirty()
  {
    if (mStylistState == StylistState::NotDirty) {
      mStylistState = StylistState::StyleSheetsDirty;
    }
  }

  bool StylistNeedsUpdate() const
  {
    return mStylistState != StylistState::NotDirty;
  }

  /**
   * Update the stylist as needed to ensure style data is up-to-date.
   *
   * This should only be called if StylistNeedsUpdate returns true.
   */
  void UpdateStylist();

  already_AddRefed<ServoComputedValues>
    ResolveStyleLazily(dom::Element* aElement,
                       CSSPseudoElementType aPseudoType,
                       StyleRuleInclusion aRules =
                         StyleRuleInclusion::All);

  void RunPostTraversalTasks();

  void PrependSheetOfType(SheetType aType,
                          ServoStyleSheet* aSheet);

  void AppendSheetOfType(SheetType aType,
                         ServoStyleSheet* aSheet);

  void InsertSheetOfType(SheetType aType,
                         ServoStyleSheet* aSheet,
                         ServoStyleSheet* aBeforeSheet);

  void RemoveSheetOfType(SheetType aType,
                         ServoStyleSheet* aSheet);

  nsPresContext* mPresContext;
  UniquePtr<RawServoStyleSet> mRawSet;
  EnumeratedArray<SheetType, SheetType::Count,
                  nsTArray<RefPtr<ServoStyleSheet>>> mSheets;
  bool mAllowResolveStaleStyles;
  bool mAuthorStyleDisabled;
  StylistState mStylistState;

  bool mNeedsRestyleAfterEnsureUniqueInner;

  // Stores pointers to our cached style contexts for non-inheriting anonymous
  // boxes.
  EnumeratedArray<nsCSSAnonBoxes::NonInheriting,
                  nsCSSAnonBoxes::NonInheriting::_Count,
                  RefPtr<nsStyleContext>> mNonInheritingStyleContexts;

  // Tasks to perform after a traversal, back on the main thread.
  //
  // These are similar to Servo's SequentialTasks, except that they are
  // posted by C++ code running on style worker threads.
  nsTArray<PostTraversalTask> mPostTraversalTasks;

  static ServoStyleSet* sInServoTraversal;
};

} // namespace mozilla

#endif // mozilla_ServoStyleSet_h

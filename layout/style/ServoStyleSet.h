/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoStyleSet_h
#define mozilla_ServoStyleSet_h

#include "mozilla/AtomArray.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/EventStates.h"
#include "mozilla/PostTraversalTask.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/ServoElementSnapshot.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoUtils.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/SheetType.h"
#include "mozilla/UniquePtr.h"
#include "MainThreadUtils.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSAnonBoxes.h"
#include "nsChangeHint.h"
#include "nsAtom.h"
#include "nsIMemoryReporter.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
class CSSStyleSheet;
class ServoRestyleManager;
class ServoStyleSheet;
struct Keyframe;
class ServoElementSnapshotTable;
class ServoStyleContext;
class ServoStyleRuleMap;
} // namespace mozilla
class nsCSSCounterStyleRule;
class nsIContent;
class nsIDocument;
class nsPresContext;
struct nsTimingFunction;
struct RawServoRuleNode;
struct TreeMatchContext;

namespace mozilla {

// A few flags used to track which kind of stylist state we may need to
// update.
enum class StylistState : uint8_t {
  // The stylist is not dirty, we should do nothing.
  NotDirty = 0,

  // The style sheets have changed, so we need to update the style data.
  StyleSheetsDirty = 1 << 0,

  // Some of the style sheets of the bound elements in binding manager have
  // changed, so we need to tell the binding manager to update style data.
  XBLStyleSheetsDirty = 1 << 1,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(StylistState)

// Bitfield type to represent Servo stylesheet origins.
enum class OriginFlags : uint8_t {
  UserAgent = 0x01,
  User      = 0x02,
  Author    = 0x04,
  All       = 0x07,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(OriginFlags)

/**
 * The set of style sheets that apply to a document, backed by a Servo
 * Stylist.  A ServoStyleSet contains ServoStyleSheets.
 */
class ServoStyleSet
{
  friend class ServoRestyleManager;
  typedef ServoElementSnapshotTable SnapshotTable;

public:
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

#ifdef DEBUG
  // Used for debug assertions. We make this debug-only to prevent callers from
  // accidentally using it instead of IsInServoTraversal, which is cheaper. We
  // can change this if a use-case arises.
  static bool IsCurrentThreadInServoTraversal();
#endif

  static ServoStyleSet* Current()
  {
    return sInServoTraversal;
  }

  // The kind of styleset we have.
  //
  // We use ServoStyleSet also from XBL bindings, and some stuff needs to be
  // different between them.
  enum class Kind : uint8_t {
    // A "master" StyleSet.
    //
    // This one is owned by a pres shell for a given document.
    Master,

    // A StyleSet for XBL, which is owned by a given XBL binding.
    ForXBL,
  };

  explicit ServoStyleSet(Kind aKind);
  ~ServoStyleSet();

  static UniquePtr<ServoStyleSet>
  CreateXBLServoStyleSet(nsPresContext* aPresContext,
                         const nsTArray<RefPtr<ServoStyleSheet>>& aNewSheets);

  void Init(nsPresContext* aPresContext, nsBindingManager* aBindingManager);
  void BeginShutdown() {}
  void Shutdown();

  // Called when a rules in a stylesheet in this set, or a child sheet of that,
  // are mutated from CSSOM.
  void RuleAdded(ServoStyleSheet&, css::Rule&);
  void RuleRemoved(ServoStyleSheet&, css::Rule&);
  void RuleChanged(ServoStyleSheet& aSheet, css::Rule* aRule);

  // All the relevant changes are handled in RuleAdded / RuleRemoved / etc, and
  // the relevant AppendSheet / RemoveSheet...
  void RecordStyleSheetChange(ServoStyleSheet*, StyleSheet::ChangeType) {}

  void RecordShadowStyleChange(dom::ShadowRoot* aShadowRoot) {
    // FIXME(emilio): When we properly support shadow dom we'll need to do
    // better.
    MarkOriginsDirty(OriginFlags::All);
  }

  bool StyleSheetsHaveChanged() const
  {
    return StylistNeedsUpdate();
  }

  nsRestyleHint MediumFeaturesChanged(bool aViewportChanged);

  // Evaluates a given SourceSizeList, returning the optimal viewport width in
  // app units.
  //
  // The SourceSizeList parameter can be null, in which case it will return
  // 100vw.
  nscoord EvaluateSourceSizeList(const RawServoSourceSizeList* aSourceSizeList) const {
    return Servo_SourceSizeList_Evaluate(mRawSet.get(), aSourceSizeList);
  }

  // aViewportChanged outputs whether any viewport units is used.
  bool MediumFeaturesChangedRules(bool* aViewportUnitsUsed);

  void InvalidateStyleForCSSRuleChanges();

  void AddSizeOfIncludingThis(nsWindowSizes& aSizes) const;
  const RawServoStyleSet* RawSet() const {
    return mRawSet.get();
  }

  bool GetAuthorStyleDisabled() const;
  nsresult SetAuthorStyleDisabled(bool aStyleDisabled);

  void BeginUpdate();
  nsresult EndUpdate();

  already_AddRefed<ServoStyleContext>
  ResolveStyleFor(dom::Element* aElement,
                  ServoStyleContext* aParentContext,
                  LazyComputeBehavior aMayCompute);

  // Get a style context for a text node (which no rules will match).
  //
  // The returned style context will have nsCSSAnonBoxes::mozText as its pseudo.
  //
  // (Perhaps mozText should go away and we shouldn't even create style
  // contexts for such content nodes, when text-combine-upright is not
  // present.  However, not doing any rule matching for them is a first step.)
  already_AddRefed<ServoStyleContext>
  ResolveStyleForText(nsIContent* aTextNode,
                      ServoStyleContext* aParentContext);

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
  already_AddRefed<ServoStyleContext>
  ResolveStyleForFirstLetterContinuation(ServoStyleContext* aParentContext);

  // Get a style context for a placeholder frame (which no rules will match).
  //
  // The returned style context will have nsCSSAnonBoxes::oofPlaceholder as
  // its pseudo.
  //
  // (Perhaps nsCSSAnonBoxes::oofPaceholder should go away and we shouldn't even
  // create style contexts for placeholders.  However, not doing any rule
  // matching for them is a first step.)
  already_AddRefed<ServoStyleContext>
  ResolveStyleForPlaceholder();

  // Get a style context for a pseudo-element.  aParentElement must be
  // non-null.  aPseudoID is the CSSPseudoElementType for the
  // pseudo-element.  aPseudoElement must be non-null if the pseudo-element
  // type is one that allows user action pseudo-classes after it or allows
  // style attributes; otherwise, it is ignored.
  already_AddRefed<ServoStyleContext>
  ResolvePseudoElementStyle(dom::Element* aOriginatingElement,
                            CSSPseudoElementType aType,
                            ServoStyleContext* aParentContext,
                            dom::Element* aPseudoElement);

  // Resolves style for a (possibly-pseudo) Element without assuming that the
  // style has been resolved. If the element was unstyled and a new style
  // context was resolved, it is not stored in the DOM. (That is, the element
  // remains unstyled.)
  already_AddRefed<ServoStyleContext>
  ResolveStyleLazily(dom::Element* aElement,
                     CSSPseudoElementType aPseudoType,
                     StyleRuleInclusion aRules =
                       StyleRuleInclusion::All);

  // Get a style context for an anonymous box.  aPseudoTag is the pseudo-tag to
  // use and must be non-null.  It must be an anon box, and must be one that
  // inherits style from the given aParentContext.
  already_AddRefed<ServoStyleContext>
  ResolveInheritingAnonymousBoxStyle(nsAtom* aPseudoTag,
                                     ServoStyleContext* aParentContext);

  // Get a style context for an anonymous box that does not inherit style from
  // anything.  aPseudoTag is the pseudo-tag to use and must be non-null.  It
  // must be an anon box, and must be a non-inheriting one.
  already_AddRefed<ServoStyleContext>
  ResolveNonInheritingAnonymousBoxStyle(nsAtom* aPseudoTag);

#ifdef MOZ_XUL
  already_AddRefed<ServoStyleContext>
  ResolveXULTreePseudoStyle(dom::Element* aParentElement,
                            nsICSSAnonBoxPseudo* aPseudoTag,
                            ServoStyleContext* aParentContext,
                            const AtomArray& aInputWord);
#endif

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

  void AppendAllXBLStyleSheets(nsTArray<StyleSheet*>& aArray) const;

  template<typename Func>
  void EnumerateStyleSheetArrays(Func aCallback) const {
    for (const auto& sheetArray : mSheets) {
      aCallback(sheetArray);
    }
  }

  nsresult RemoveDocStyleSheet(ServoStyleSheet* aSheet);
  nsresult AddDocStyleSheet(ServoStyleSheet* aSheet, nsIDocument* aDocument);

  // check whether there is ::before/::after style for an element
  already_AddRefed<ServoStyleContext>
  ProbePseudoElementStyle(dom::Element* aOriginatingElement,
                          CSSPseudoElementType aType,
                          ServoStyleContext* aParentContext);

  /**
   * Performs a Servo traversal to compute style for all dirty nodes in the
   * document.
   *
   * This will traverse all of the document's style roots (that is, its document
   * element, and the roots of the document-level native anonymous content).
   *
   * We specify |ForCSSRuleChanges| to try to update all CSS animations
   * when we call this function due to CSS rule changes since @keyframes rules
   * may have changed.
   *
   * Returns true if a post-traversal is required.
   */
  bool StyleDocument(ServoTraversalFlags aFlags);

  /**
   * Eagerly styles a subtree of unstyled nodes that was just appended to the
   * tree. This is used in situations where we need the style immediately and
   * cannot wait for a future batch restyle.
   */
  void StyleNewSubtree(dom::Element* aRoot);

  /**
   * Helper for correctly calling UpdateStylist without paying the cost of an
   * extra function call in the common no-rebuild-needed case.
   */
  void UpdateStylistIfNeeded()
  {
    if (StylistNeedsUpdate()) {
      UpdateStylist();
    }
  }

  /**
   * Checks whether the rule tree has crossed its threshold for unused nodes,
   * and if so, frees them.
   */
  void MaybeGCRuleTree();

  /**
   * Returns true if the given element may be used as the root of a style
   * traversal. Reasons for false include having an unstyled parent, or having
   * a parent that is display:none.
   *
   * Most traversal callsites don't need to check this, but some do.
   */
  static bool MayTraverseFrom(const dom::Element* aElement);

#ifdef DEBUG
  void AssertTreeIsClean();
#else
  void AssertTreeIsClean() {}
#endif

  /**
   * Clears any cached style data that may depend on all sorts of computed
   * values.
   *
   * Right now this clears the non-inheriting style context cache, and resets
   * the default computed values.
   *
   * This does _not_, however, clear the stylist.
   */
  void ClearCachedStyleData();

  /**
   * Notifies the Servo stylesheet that the document's compatibility mode has changed.
   */
  void CompatibilityModeChanged();

  /**
   * Resolve style for the given element, and return it as a
   * ServoStyleContext.
   *
   * FIXME(emilio): Is there a point in this after bug 1367904?
   */
  already_AddRefed<ServoStyleContext> ResolveServoStyle(dom::Element* aElement);

  bool GetKeyframesForName(nsAtom* aName,
                           const nsTimingFunction& aTimingFunction,
                           nsTArray<Keyframe>& aKeyframes);

  nsTArray<ComputedKeyframeValues>
  GetComputedKeyframeValuesFor(const nsTArray<Keyframe>& aKeyframes,
                               dom::Element* aElement,
                               const ServoStyleContext* aContext);

  void
  GetAnimationValues(RawServoDeclarationBlock* aDeclarations,
                     dom::Element* aElement,
                     const ServoStyleContext* aContext,
                     nsTArray<RefPtr<RawServoAnimationValue>>& aAnimationValues);

  bool AppendFontFaceRules(nsTArray<nsFontFaceRuleContainer>& aArray);

  nsCSSCounterStyleRule* CounterStyleRuleForName(nsAtom* aName);

  // Get all the currently-active font feature values set.
  already_AddRefed<gfxFontFeatureValueSet> BuildFontFeatureValueSet();

  already_AddRefed<ServoStyleContext>
  GetBaseContextForElement(dom::Element* aElement,
                           nsPresContext* aPresContext,
                           const ServoStyleContext* aStyle);

  // Get a style context that represents |aStyle|, but as though
  // it additionally matched the rules of the newly added |aAnimaitonaValue|.
  // We use this function to temporarily generate a ServoStyleContext for
  // calculating the cumulative change hints.
  // This must hold:
  //   The additional rules must be appropriate for the transition
  //   level of the cascade, which is the highest level of the cascade.
  //   (This is the case for one current caller, the cover rule used
  //   for CSS transitions.)
  // Note: |aElement| should be the generated element if it is pseudo.
  already_AddRefed<ServoStyleContext>
  ResolveServoStyleByAddingAnimation(dom::Element* aElement,
                                     const ServoStyleContext* aStyle,
                                     RawServoAnimationValue* aAnimationValue);
  /**
   * Resolve style for a given declaration block with/without the parent style.
   * If the parent style is not specified, the document default computed values
   * is used.
   */
  already_AddRefed<ServoStyleContext>
  ResolveForDeclarations(const ServoStyleContext* aParentOrNull,
                         RawServoDeclarationBlockBorrowed aDeclarations);

  already_AddRefed<RawServoAnimationValue>
  ComputeAnimationValue(dom::Element* aElement,
                        RawServoDeclarationBlock* aDeclaration,
                        const ServoStyleContext* aContext);

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

  // Returns the style rule map.
  ServoStyleRuleMap* StyleRuleMap();

  // Return whether this is the last PresContext which uses this XBL styleset.
  bool IsPresContextChanged(nsPresContext* aPresContext) const {
    return aPresContext != mLastPresContextUsesXBLStyleSet;
  }

  // Set PresContext (i.e. Device) for mRawSet. This should be called only
  // by XBL stylesets. Returns true if there is any rule changing.
  bool SetPresContext(nsPresContext* aPresContext);

  /**
   * Returns true if a modification to an an attribute with the specified
   * local name might require us to restyle the element.
   *
   * This function allows us to skip taking a an attribute snapshot when
   * the modified attribute doesn't appear in an attribute selector in
   * a style sheet.
   */
  bool MightHaveAttributeDependency(const dom::Element& aElement,
                                    nsAtom* aAttribute) const;

  /**
   * Returns true if a change in event state on an element might require
   * us to restyle the element.
   *
   * This function allows us to skip taking a state snapshot when
   * the changed state isn't depended upon by any pseudo-class selectors
   * in a style sheet.
   */
  bool HasStateDependency(const dom::Element& aElement,
                          EventStates aState) const;

  /**
   * Returns true if a change in document state might require us to restyle the
   * document.
   */
  bool HasDocumentStateDependency(EventStates aState) const;

  /**
   * Get a new style context that uses the same rules as the given style context
   * but has a different parent.
   *
   * aElement is non-null if this is a style context for a frame whose mContent
   * is an element and which has no pseudo on its style context (so it's the
   * actual style for the element being passed).
   */
  already_AddRefed<ServoStyleContext>
  ReparentStyleContext(ServoStyleContext* aStyleContext,
                       ServoStyleContext* aNewParent,
                       ServoStyleContext* aNewParentIgnoringFirstLine,
                       ServoStyleContext* aNewLayoutParent,
                       Element* aElement);

  bool IsMaster() const { return mKind == Kind::Master; }
  bool IsForXBL() const { return mKind == Kind::ForXBL; }

private:
  friend class AutoSetInServoTraversal;
  friend class AutoPrepareTraversal;

  bool ShouldTraverseInParallel() const;

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
  void PreTraverse(ServoTraversalFlags aFlags,
                   dom::Element* aRoot = nullptr);

  // Subset of the pre-traverse steps that involve syncing up data
  void PreTraverseSync();

  /**
   * Records that the contents of style sheets at the specified origin have
   * changed since the last.  Calling this will ensure that the Stylist
   * rebuilds its selector maps.
   */
  void MarkOriginsDirty(OriginFlags aChangedOrigins);

  /**
   * Note that the stylist needs a style flush due to style sheet changes.
   */
  void SetStylistStyleSheetsDirty()
  {
    mStylistState |= StylistState::StyleSheetsDirty;
  }

  void SetStylistXBLStyleSheetsDirty()
  {
    mStylistState |= StylistState::XBLStyleSheetsDirty;
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

  already_AddRefed<ServoStyleContext>
    ResolveStyleLazilyInternal(dom::Element* aElement,
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

  const Kind mKind;

  // Nullptr if this is an XBL style set.
  nsPresContext* MOZ_NON_OWNING_REF mPresContext = nullptr;

  // Because XBL style set could be used by multiple PresContext, we need to
  // store the last PresContext pointer which uses this XBL styleset for
  // computing medium rule changes.
  void* MOZ_NON_OWNING_REF mLastPresContextUsesXBLStyleSet = nullptr;

  UniquePtr<RawServoStyleSet> mRawSet;
  EnumeratedArray<SheetType, SheetType::Count,
                  nsTArray<RefPtr<ServoStyleSheet>>> mSheets;
  bool mAuthorStyleDisabled;
  StylistState mStylistState;
  uint64_t mUserFontSetUpdateGeneration;
  uint32_t mUserFontCacheUpdateGeneration;

  bool mNeedsRestyleAfterEnsureUniqueInner;

  // Stores pointers to our cached style contexts for non-inheriting anonymous
  // boxes.
  EnumeratedArray<nsCSSAnonBoxes::NonInheriting,
                  nsCSSAnonBoxes::NonInheriting::_Count,
                  RefPtr<ServoStyleContext>> mNonInheritingStyleContexts;

  // Tasks to perform after a traversal, back on the main thread.
  //
  // These are similar to Servo's SequentialTasks, except that they are
  // posted by C++ code running on style worker threads.
  nsTArray<PostTraversalTask> mPostTraversalTasks;

  // Map from raw Servo style rule to Gecko's wrapper object.
  // Constructed lazily when requested by devtools.
  UniquePtr<ServoStyleRuleMap> mStyleRuleMap;

  // This can be null if we are used to hold XBL style sheets.
  RefPtr<nsBindingManager> mBindingManager;

  static ServoStyleSet* sInServoTraversal;
};

class UACacheReporter final : public nsIMemoryReporter
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

private:
  ~UACacheReporter() {}
};

} // namespace mozilla

#endif // mozilla_ServoStyleSet_h

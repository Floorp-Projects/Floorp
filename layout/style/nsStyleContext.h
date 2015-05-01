/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the interface (to internal code) for retrieving computed style data */

#ifndef _nsStyleContext_h_
#define _nsStyleContext_h_

#include "mozilla/RestyleLogging.h"
#include "mozilla/Assertions.h"
#include "nsRuleNode.h"
#include "nsCSSPseudoElements.h"

class nsIAtom;
class nsPresContext;

/**
 * An nsStyleContext represents the computed style data for an element.
 * The computed style data are stored in a set of structs (see
 * nsStyleStruct.h) that are cached either on the style context or in
 * the rule tree (see nsRuleNode.h for a description of this caching and
 * how the cached structs are shared).
 *
 * Since the data in |nsIStyleRule|s and |nsRuleNode|s are immutable
 * (with a few exceptions, like system color changes), the data in an
 * nsStyleContext are also immutable (with the additional exception of
 * GetUniqueStyleData).  When style data change,
 * nsFrameManager::ReResolveStyleContext creates a new style context.
 *
 * Style contexts are reference counted.  References are generally held
 * by:
 *  1. the |nsIFrame|s that are using the style context and
 *  2. any *child* style contexts (this might be the reverse of
 *     expectation, but it makes sense in this case)
 * Style contexts participate in the mark phase of rule node garbage
 * collection.
 */

class nsStyleContext final
{
public:
  /**
   * Create a new style context.
   * @param aParent  The parent of a style context is used for CSS
   *                 inheritance.  When the element or pseudo-element
   *                 this style context represents the style data of
   *                 inherits a CSS property, the value comes from the
   *                 parent style context.  This means style context
   *                 parentage must match the definitions of inheritance
   *                 in the CSS specification.
   * @param aPseudoTag  The pseudo-element or anonymous box for which
   *                    this style context represents style.  Null if
   *                    this style context is for a normal DOM element.
   * @param aPseudoType  Must match aPseudoTag.
   * @param aRuleNode  A rule node representing the ordered sequence of
   *                   rules that any element, pseudo-element, or
   *                   anonymous box that this style context is for
   *                   matches.  See |nsRuleNode| and |nsIStyleRule|.
   * @param aSkipParentDisplayBasedStyleFixup
   *                 If set, this flag indicates that we should skip
   *                 the chunk of ApplyStyleFixups() that applies to 
   *                 special cases where a child element's style may 
   *                 need to be modified based on its parent's display 
   *                 value.
   */
  nsStyleContext(nsStyleContext* aParent, nsIAtom* aPseudoTag,
                 nsCSSPseudoElements::Type aPseudoType,
                 nsRuleNode* aRuleNode,
                 bool aSkipParentDisplayBasedStyleFixup);

  void* operator new(size_t sz, nsPresContext* aPresContext) CPP_THROW_NEW;
  void Destroy();

#ifdef DEBUG
  /**
   * Initializes a cached pref, which is only used in DEBUG code.
   */
  static void Initialize();
#endif

  nsrefcnt AddRef() {
    if (mRefCnt == UINT32_MAX) {
      NS_WARNING("refcount overflow, leaking object");
      return mRefCnt;
    }
    ++mRefCnt;
    NS_LOG_ADDREF(this, mRefCnt, "nsStyleContext", sizeof(nsStyleContext));
    return mRefCnt;
  }

  nsrefcnt Release() {
    if (mRefCnt == UINT32_MAX) {
      NS_WARNING("refcount overflow, leaking object");
      return mRefCnt;
    }
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "nsStyleContext");
    if (mRefCnt == 0) {
      Destroy();
      return 0;
    }
    return mRefCnt;
  }

#ifdef DEBUG
  void FrameAddRef() {
    ++mFrameRefCnt;
  }

  void FrameRelease() {
    --mFrameRefCnt;
  }

  uint32_t FrameRefCnt() const {
    return mFrameRefCnt;
  }
#endif

  bool HasSingleReference() const {
    NS_ASSERTION(mRefCnt != 0,
                 "do not call HasSingleReference on a newly created "
                 "nsStyleContext with no references yet");
    return mRefCnt == 1;
  }

  nsPresContext* PresContext() const { return mRuleNode->PresContext(); }

  nsStyleContext* GetParent() const { return mParent; }

  nsIAtom* GetPseudo() const { return mPseudoTag; }
  nsCSSPseudoElements::Type GetPseudoType() const {
    return static_cast<nsCSSPseudoElements::Type>(mBits >>
                                                  NS_STYLE_CONTEXT_TYPE_SHIFT);
  }

  enum {
    eRelevantLinkVisited = 1 << 0,
    eSuppressLineBreak = 1 << 1
  };

  // Find, if it already exists *and is easily findable* (i.e., near the
  // start of the child list), a style context whose:
  //  * GetPseudo() matches aPseudoTag
  //  * RuleNode() matches aRules
  //  * !GetStyleIfVisited() == !aRulesIfVisited, and, if they're
  //    non-null, GetStyleIfVisited()->RuleNode() == aRulesIfVisited
  //  * RelevantLinkVisited() == (aFlags & eRelevantLinkVisited)
  //  * ShouldSuppressLineBreak() == (aFlags & eSuppressLineBreak)
  already_AddRefed<nsStyleContext>
  FindChildWithRules(const nsIAtom* aPseudoTag, nsRuleNode* aRules,
                     nsRuleNode* aRulesIfVisited, uint32_t aFlags);

  // Does this style context or any of its ancestors have text
  // decoration lines?
  bool HasTextDecorationLines() const
    { return !!(mBits & NS_STYLE_HAS_TEXT_DECORATION_LINES); }

  // Whether any line break inside should be suppressed? If this returns
  // true, the line should not be broken inside, which means inlines act
  // as if nowrap is set, <br> is suppressed, and blocks are inlinized.
  // This bit is propogated to all children of line partitipants. It is
  // currently used by ruby to make its content frames unbreakable.
  bool ShouldSuppressLineBreak() const
    { return !!(mBits & NS_STYLE_SUPPRESS_LINEBREAK); }

  // Does this style context or any of its ancestors have display:none set?
  bool IsInDisplayNoneSubtree() const
    { return !!(mBits & NS_STYLE_IN_DISPLAY_NONE_SUBTREE); }

  // Does this style context represent the style for a pseudo-element or
  // inherit data from such a style context?  Whether this returns true
  // is equivalent to whether it or any of its ancestors returns
  // non-null for GetPseudo.
  bool HasPseudoElementData() const
    { return !!(mBits & NS_STYLE_HAS_PSEUDO_ELEMENT_DATA); }

  // Is the only link whose visitedness is allowed to influence the
  // style of the node this style context is for (which is that element
  // or its nearest ancestor that is a link) visited?
  bool RelevantLinkVisited() const
    { return !!(mBits & NS_STYLE_RELEVANT_LINK_VISITED); }

  // Is this a style context for a link?
  bool IsLinkContext() const {
    return
      GetStyleIfVisited() && GetStyleIfVisited()->GetParent() == GetParent();
  }

  // Is this style context the GetStyleIfVisited() for some other style
  // context?
  bool IsStyleIfVisited() const
    { return !!(mBits & NS_STYLE_IS_STYLE_IF_VISITED); }

  // Tells this style context that it should return true from
  // IsStyleIfVisited.
  void SetIsStyleIfVisited()
    { mBits |= NS_STYLE_IS_STYLE_IF_VISITED; }

  // Return the style context whose style data should be used for the R,
  // G, and B components of color, background-color, and border-*-color
  // if RelevantLinkIsVisited().
  //
  // GetPseudo() and GetPseudoType() on this style context return the
  // same as on |this|, and its depth in the tree (number of GetParent()
  // calls until null is returned) is the same as |this|, since its
  // parent is either |this|'s parent or |this|'s parent's
  // style-if-visited.
  //
  // Structs on this context should never be examined without also
  // examining the corresponding struct on |this|.  Doing so will likely
  // both (1) lead to a privacy leak and (2) lead to dynamic change bugs
  // related to the Peek code in nsStyleContext::CalcStyleDifference.
  nsStyleContext* GetStyleIfVisited() const
    { return mStyleIfVisited; }

  // To be called only from nsStyleSet.
  void SetStyleIfVisited(already_AddRefed<nsStyleContext> aStyleIfVisited)
  {
    MOZ_ASSERT(!IsStyleIfVisited(), "this context is not visited data");
    NS_ASSERTION(!mStyleIfVisited, "should only be set once");

    mStyleIfVisited = aStyleIfVisited;

    MOZ_ASSERT(mStyleIfVisited->IsStyleIfVisited(),
               "other context is visited data");
    MOZ_ASSERT(!mStyleIfVisited->GetStyleIfVisited(),
               "other context does not have visited data");
    NS_ASSERTION(GetStyleIfVisited()->GetPseudo() == GetPseudo(),
                 "pseudo tag mismatch");
    if (GetParent() && GetParent()->GetStyleIfVisited()) {
      NS_ASSERTION(GetStyleIfVisited()->GetParent() ==
                     GetParent()->GetStyleIfVisited() ||
                   GetStyleIfVisited()->GetParent() == GetParent(),
                   "parent mismatch");
    } else {
      NS_ASSERTION(GetStyleIfVisited()->GetParent() == GetParent(),
                   "parent mismatch");
    }
  }

  // Does this style context, or any of its descendants, have any style values
  // that were computed based on this style context's grandparent style context
  // or any of the grandparent's ancestors?
  bool UsesGrandancestorStyle() const
    { return !!(mBits & NS_STYLE_USES_GRANDANCESTOR_STYLE); }

  // Is this style context shared with a sibling or cousin?
  // (See nsStyleSet::GetContext.)
  bool IsShared() const
    { return !!(mBits & NS_STYLE_IS_SHARED); }

  // Does this style context have any children that return true for
  // UsesGrandancestorStyle()?
  bool HasChildThatUsesGrandancestorStyle() const;

  // Tell this style context to cache aStruct as the struct for aSID
  void SetStyle(nsStyleStructID aSID, void* aStruct);

  // Setters for inherit structs only, since rulenode only sets those eagerly.
  #define STYLE_STRUCT_INHERITED(name_, checkdata_cb_)                      \
    void SetStyle##name_ (nsStyle##name_ * aStruct) {                       \
      void *& slot =                                                        \
        mCachedInheritedData.mStyleStructs[eStyleStruct_##name_];           \
      NS_ASSERTION(!slot ||                                                 \
                   (mBits &                                                 \
                    nsCachedStyleData::GetBitForSID(eStyleStruct_##name_)), \
                   "Going to leak styledata");                              \
      slot = aStruct;                                                       \
    }
#define STYLE_STRUCT_RESET(name_, checkdata_cb_) /* nothing */
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT_RESET
  #undef STYLE_STRUCT_INHERITED

  /**
   * Returns whether this style context and aOther both have the same
   * cached style struct pointer for a given style struct.
   */
  bool HasSameCachedStyleData(nsStyleContext* aOther, nsStyleStructID aSID);

  /**
   * Returns whether this style context has cached, inherited style data for a
   * given style struct.
   */
  bool HasCachedInheritedStyleData(nsStyleStructID aSID)
    { return mBits & nsCachedStyleData::GetBitForSID(aSID); }

  nsRuleNode* RuleNode() { return mRuleNode; }
  void AddStyleBit(const uint64_t& aBit) { mBits |= aBit; }

  /*
   * Mark this style context's rule node (and its ancestors) to prevent
   * it from being garbage collected.
   */
  void Mark();

  /*
   * Get the style data for a style struct.  This is the most important
   * member function of nsIStyleContext.  It fills in a const pointer
   * to a style data struct that is appropriate for the style context's
   * frame.  This struct may be shared with other contexts (either in
   * the rule tree or the style context tree), so it should not be
   * modified.
   *
   * This function will NOT return null (even when out of memory) when
   * given a valid style struct ID, so the result does not need to be
   * null-checked.
   *
   * The typesafe functions below are preferred to the use of this
   * function, both because they're easier to read and because they're
   * faster.
   */
  const void* NS_FASTCALL StyleData(nsStyleStructID aSID);

  /**
   * Define typesafe getter functions for each style struct by
   * preprocessing the list of style structs.  These functions are the
   * preferred way to get style data.  The macro creates functions like:
   *   const nsStyleBorder* StyleBorder();
   *   const nsStyleColor* StyleColor();
   */
  #define STYLE_STRUCT(name_, checkdata_cb_)              \
    const nsStyle##name_ * Style##name_() {               \
      return DoGetStyle##name_<true>();                   \
    }
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT

  /**
   * PeekStyle* is like GetStyle* but doesn't trigger style
   * computation if the data is not cached on either the style context
   * or the rule node.
   *
   * Perhaps this shouldn't be a public nsStyleContext API.
   */
  #define STYLE_STRUCT(name_, checkdata_cb_)              \
    const nsStyle##name_ * PeekStyle##name_() {           \
      return DoGetStyle##name_<false>();                  \
    }
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT

  /**
   * Compute the style changes needed during restyling when this style
   * context is being replaced by aOther.  (This is nonsymmetric since
   * we optimize by skipping comparison for styles that have never been
   * requested.)
   *
   * This method returns a change hint (see nsChangeHint.h).  All change
   * hints apply to the frame and its later continuations or ib-split
   * siblings.  Most (all of those except the "NotHandledForDescendants"
   * hints) also apply to all descendants.  The caller must pass in any
   * non-inherited hints that resulted from the parent style context's
   * style change.  The caller *may* pass more hints than needed, but
   * must not pass less than needed; therefore if the caller doesn't
   * know, the caller should pass
   * nsChangeHint_Hints_NotHandledForDescendants.
   *
   * aEqualStructs must not be null.  Into it will be stored a bitfield
   * representing which structs were compared to be non-equal.
   */
  nsChangeHint CalcStyleDifference(nsStyleContext* aOther,
                                   nsChangeHint aParentHintsNotHandledForDescendants,
                                   uint32_t* aEqualStructs);

  /**
   * Get a color that depends on link-visitedness using this and
   * this->GetStyleIfVisited().
   *
   * aProperty must be a color-valued property that StyleAnimationValue
   * knows how to extract.  It must also be a property that we know to
   * do change handling for in nsStyleContext::CalcDifference.
   *
   * Note that if aProperty is eCSSProperty_border_*_color, this
   * function handles -moz-use-text-color.
   */
  nscolor GetVisitedDependentColor(nsCSSProperty aProperty);

  /**
   * aColors should be a two element array of nscolor in which the first
   * color is the unvisited color and the second is the visited color.
   *
   * Combine the R, G, and B components of whichever of aColors should
   * be used based on aLinkIsVisited with the A component of aColors[0].
   */
  static nscolor CombineVisitedColors(nscolor *aColors,
                                      bool aLinkIsVisited);

  /**
   * Start the background image loads for this style context.
   */
  void StartBackgroundImageLoads() {
    // Just get our background struct; that should do the trick
    StyleBackground();
  }

  /**
   * Moves this style context to a new parent.
   *
   * This function violates style context tree immutability, and
   * is a very low-level function and should only be used after verifying
   * many conditions that make it safe to call.
   */
  void MoveTo(nsStyleContext* aNewParent);

  /**
   * Swaps owned style struct pointers between this and aNewContext, on
   * the assumption that aNewContext is the new style context for a frame
   * and this is the old one.  aStructs indicates which structs to consider
   * swapping; only those which are owned in both this and aNewContext
   * will be swapped.
   *
   * Additionally, if there are identical struct pointers for one of the
   * structs indicated by aStructs, and it is not an owned struct on this,
   * then the cached struct slot on this will be set to null.  If the struct
   * has been swapped on an ancestor, this style context (being the old one)
   * will be left caching the struct pointer on the new ancestor, despite
   * inheriting from the old ancestor.  This is not normally a problem, as
   * this style context will usually be destroyed by being released at the
   * end of ElementRestyler::Restyle; but for style contexts held on to outside
   * of the frame, we need to clear out the cached pointer so that if we need
   * it again we'll re-fetch it from the new ancestor.
   */
  void SwapStyleData(nsStyleContext* aNewContext, uint32_t aStructs);

  /**
   * On each descendant of this style context, clears out any cached inherited
   * structs indicated in aStructs.
   */
  void ClearCachedInheritedStyleDataOnDescendants(uint32_t aStructs);

  /**
   * Sets the NS_STYLE_INELIGIBLE_FOR_SHARING bit on this style context
   * and its descendants.  If it finds a descendant that has the bit
   * already set, assumes that it can skip that subtree.
   */
  void SetIneligibleForSharing();

#ifdef DEBUG
  void List(FILE* out, int32_t aIndent, bool aListDescendants = true);
  static void AssertStyleStructMaxDifferenceValid();
  static const char* StructName(nsStyleStructID aSID);
  static bool LookupStruct(const nsACString& aName, nsStyleStructID& aResult);
#endif

#ifdef RESTYLE_LOGGING
  nsCString GetCachedStyleDataAsString(uint32_t aStructs);
  void LogStyleContextTree(int32_t aLoggingDepth, uint32_t aStructs);
  int32_t& LoggingDepth();
#endif

private:
  // Private destructor, to discourage deletion outside of Release():
  ~nsStyleContext();

  void AddChild(nsStyleContext* aChild);
  void RemoveChild(nsStyleContext* aChild);

  void* GetUniqueStyleData(const nsStyleStructID& aSID);
  void* CreateEmptyStyleData(const nsStyleStructID& aSID);

  void ApplyStyleFixups(bool aSkipParentDisplayBasedStyleFixup);

  // Helper function for HasChildThatUsesGrandancestorStyle.
  static bool ListContainsStyleContextThatUsesGrandancestorStyle(
                                                   const nsStyleContext* aHead);

  // Helper function that GetStyleData and GetUniqueStyleData use.  Only
  // returns the structs we cache ourselves; never consults the ruletree.
  inline const void* GetCachedStyleData(nsStyleStructID aSID);

#ifdef DEBUG
  struct AutoCheckDependency {

    nsStyleContext* mStyleContext;
    nsStyleStructID mOuterSID;

    AutoCheckDependency(nsStyleContext* aContext, nsStyleStructID aInnerSID)
      : mStyleContext(aContext)
    {
      mOuterSID = aContext->mComputingStruct;
      MOZ_ASSERT(mOuterSID == nsStyleStructID_None ||
                 DependencyAllowed(mOuterSID, aInnerSID),
                 "Undeclared dependency, see generate-stylestructlist.py");
      aContext->mComputingStruct = aInnerSID;
    }

    ~AutoCheckDependency()
    {
      mStyleContext->mComputingStruct = mOuterSID;
    }

  };

#define AUTO_CHECK_DEPENDENCY(sid_) \
  AutoCheckDependency checkNesting_(this, sid_)
#else
#define AUTO_CHECK_DEPENDENCY(sid_)
#endif

  // Helper functions for GetStyle* and PeekStyle*
  #define STYLE_STRUCT_INHERITED(name_, checkdata_cb_)                  \
    template<bool aComputeData>                                         \
    const nsStyle##name_ * DoGetStyle##name_() {                        \
      const nsStyle##name_ * cachedData =                               \
        static_cast<nsStyle##name_*>(                                   \
          mCachedInheritedData.mStyleStructs[eStyleStruct_##name_]);    \
      if (cachedData) /* Have it cached already, yay */                 \
        return cachedData;                                              \
      /* Have the rulenode deal */                                      \
      AUTO_CHECK_DEPENDENCY(eStyleStruct_##name_);                      \
      return mRuleNode->GetStyle##name_<aComputeData>(this);            \
    }
  #define STYLE_STRUCT_RESET(name_, checkdata_cb_)                      \
    template<bool aComputeData>                                         \
    const nsStyle##name_ * DoGetStyle##name_() {                        \
      if (mCachedResetData) {                                           \
        const nsStyle##name_ * cachedData =                             \
          static_cast<nsStyle##name_*>(                                 \
            mCachedResetData->mStyleStructs[eStyleStruct_##name_]);     \
        if (cachedData) /* Have it cached already, yay */               \
          return cachedData;                                            \
      }                                                                 \
      /* Have the rulenode deal */                                      \
      AUTO_CHECK_DEPENDENCY(eStyleStruct_##name_);                      \
      return mRuleNode->GetStyle##name_<aComputeData>(this);            \
    }
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT_RESET
  #undef STYLE_STRUCT_INHERITED

  // Helper for ClearCachedInheritedStyleDataOnDescendants.
  void DoClearCachedInheritedStyleDataOnDescendants(uint32_t aStructs);

#ifdef DEBUG
  void AssertStructsNotUsedElsewhere(nsStyleContext* aDestroyingContext,
                                     int32_t aLevels) const;
#endif

#ifdef RESTYLE_LOGGING
  void LogStyleContextTree(bool aFirst, uint32_t aStructs);

  // This only gets called under call trees where we've already checked
  // that PresContext()->RestyleManager()->ShouldLogRestyle() returned true.
  // It exists here just to satisfy LOG_RESTYLE's expectations.
  bool ShouldLogRestyle() { return true; }
#endif

  nsStyleContext* mParent; // STRONG

  // Children are kept in two circularly-linked lists.  The list anchor
  // is not part of the list (null for empty), and we point to the first
  // child.
  // mEmptyChild for children whose rule node is the root rule node, and
  // mChild for other children.  The order of children is not
  // meaningful.
  nsStyleContext* mChild;
  nsStyleContext* mEmptyChild;
  nsStyleContext* mPrevSibling;
  nsStyleContext* mNextSibling;

  // Style to be used instead for the R, G, and B components of color,
  // background-color, and border-*-color if the nearest ancestor link
  // element is visited (see RelevantLinkVisited()).
  nsRefPtr<nsStyleContext> mStyleIfVisited;

  // If this style context is for a pseudo-element or anonymous box,
  // the relevant atom.
  nsCOMPtr<nsIAtom> mPseudoTag;

  // The rule node is the node in the lexicographic tree of rule nodes
  // (the "rule tree") that indicates which style rules are used to
  // compute the style data, and in what cascading order.  The least
  // specific rule matched is the one whose rule node is a child of the
  // root of the rule tree, and the most specific rule matched is the
  // |mRule| member of |mRuleNode|.
  nsRuleNode* const       mRuleNode;

  // mCachedInheritedData and mCachedResetData point to both structs that
  // are owned by this style context and structs that are owned by one of
  // this style context's ancestors (which are indirectly owned since this
  // style context owns a reference to its parent).  If the bit in |mBits|
  // is set for a struct, that means that the pointer for that struct is
  // owned by an ancestor or by mRuleNode rather than by this style context.
  // Since style contexts typically have some inherited data but only sometimes
  // have reset data, we always allocate the mCachedInheritedData, but only
  // sometimes allocate the mCachedResetData.
  nsResetStyleData*       mCachedResetData; // Cached reset style data.
  nsInheritedStyleData    mCachedInheritedData; // Cached inherited style data
  uint64_t                mBits; // Which structs are inherited from the
                                 // parent context or owned by mRuleNode.
  uint32_t                mRefCnt;

#ifdef DEBUG
  uint32_t                mFrameRefCnt; // number of frames that use this
                                        // as their style context

  nsStyleStructID         mComputingStruct;

  static bool DependencyAllowed(nsStyleStructID aOuterSID,
                                nsStyleStructID aInnerSID)
  {
    return !!(sDependencyTable[aOuterSID] &
              nsCachedStyleData::GetBitForSID(aInnerSID));
  }

  static const uint32_t sDependencyTable[];
#endif
};

already_AddRefed<nsStyleContext>
NS_NewStyleContext(nsStyleContext* aParentContext,
                   nsIAtom* aPseudoTag,
                   nsCSSPseudoElements::Type aPseudoType,
                   nsRuleNode* aRuleNode,
                   bool aSkipParentDisplayBasedStyleFixup);
#endif

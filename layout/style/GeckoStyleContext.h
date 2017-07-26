/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_GeckoStyleContext_h
#define mozilla_GeckoStyleContext_h

#include "nsStyleContext.h"

namespace mozilla {

class GeckoStyleContext final : public nsStyleContext {
public:
  static already_AddRefed<GeckoStyleContext>
  TakeRef(already_AddRefed<nsStyleContext> aStyleContext)
  {
    auto* context = aStyleContext.take();
    MOZ_ASSERT(context);

    return already_AddRefed<GeckoStyleContext>(context->AsGecko());
  }

  GeckoStyleContext(GeckoStyleContext* aParent,
                    nsIAtom* aPseudoTag,
                    CSSPseudoElementType aPseudoType,
                    already_AddRefed<nsRuleNode> aRuleNode,
                    bool aSkipParentDisplayBasedStyleFixup);

  void* operator new(size_t sz, nsPresContext* aPresContext);

  nsPresContext* PresContext() const {
    return RuleNode()->PresContext();
  }

  void AddChild(GeckoStyleContext* aChild);
  void RemoveChild(GeckoStyleContext* aChild);

  GeckoStyleContext* GetParent() const {
    return mParent ? mParent->AsGecko() : nullptr;
  }

  bool IsLinkContext() const {
    return GetStyleIfVisited() &&
           GetStyleIfVisited()->GetParent() == GetParent();
  }

  /**
   * Moves this style context to a new parent.
   *
   * This function violates style context tree immutability, and
   * is a very low-level function and should only be used after verifying
   * many conditions that make it safe to call.
   */
  void MoveTo(GeckoStyleContext* aNewParent);

  void* GetUniqueStyleData(const nsStyleStructID& aSID);
  void* CreateEmptyStyleData(const nsStyleStructID& aSID);

  // To be called only from nsStyleSet / ServoStyleSet.
  void SetStyleIfVisited(already_AddRefed<GeckoStyleContext> aStyleIfVisited);
  GeckoStyleContext* GetStyleIfVisited() const { return mStyleIfVisited; };
#ifdef DEBUG
  /**
   * Initializes a cached pref, which is only used in DEBUG code.
   */
  static void Initialize();
#endif

  /**
   * Ensures the same structs are cached on this style context as would be
   * done if we called aOther->CalcDifference(this).
   */
  void EnsureSameStructsCached(nsStyleContext* aOldContext);

  /**
   * Sets the NS_STYLE_INELIGIBLE_FOR_SHARING bit on this style context
   * and its descendants.  If it finds a descendant that has the bit
   * already set, assumes that it can skip that subtree.
   */
  void SetIneligibleForSharing();
  /**
   * On each descendant of this style context, clears out any cached inherited
   * structs indicated in aStructs.
   */
  void ClearCachedInheritedStyleDataOnDescendants(uint32_t aStructs);
  // Find, if it already exists *and is easily findable* (i.e., near the
  // start of the child list), a style context whose:
  //  * GetPseudo() matches aPseudoTag
  //  * mRuleNode matches aSource
  //  * !!GetStyleIfVisited() == !!aSourceIfVisited, and, if they're
  //    non-null, GetStyleIfVisited()->mRuleNode == aSourceIfVisited
  //  * RelevantLinkVisited() == aRelevantLinkVisited
  already_AddRefed<GeckoStyleContext>
  FindChildWithRules(const nsIAtom* aPseudoTag,
                     nsRuleNode* aSource,
                     nsRuleNode* aSourceIfVisited,
                     bool aRelevantLinkVisited);

  // Tell this style context to cache aStruct as the struct for aSID
  void SetStyle(nsStyleStructID aSID, void* aStruct);


  /*
   * Get the style data for a style struct.  This is the most important
   * member function of nsStyleContext.  It fills in a const pointer
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
  const void* NS_FASTCALL StyleData(nsStyleStructID aSID) MOZ_NONNULL_RETURN;

#ifdef DEBUG
  void ListDescendants(FILE* out, int32_t aIndent);

#endif

#ifdef RESTYLE_LOGGING

  // This only gets called under call trees where we've already checked
  // that PresContext()->RestyleManager()->ShouldLogRestyle() returned true.
  // It exists here just to satisfy LOG_RESTYLE's expectations.
  bool ShouldLogRestyle() { return true; }
  void LogStyleContextTree(int32_t aLoggingDepth, uint32_t aStructs);
  void LogStyleContextTree(bool aFirst, uint32_t aStructs);
  int32_t& LoggingDepth();
  nsCString GetCachedStyleDataAsString(uint32_t aStructs);
#endif

  // Only called for Gecko-backed nsStyleContexts.
  void ApplyStyleFixups(bool aSkipParentDisplayBasedStyleFixup);

  bool HasNoChildren() const;

  nsRuleNode* RuleNode() const {
    MOZ_ASSERT(mRuleNode);
    return mRuleNode;
  }

  bool HasSingleReference() const {
    NS_ASSERTION(mRefCnt != 0,
                 "do not call HasSingleReference on a newly created "
                 "nsStyleContext with no references yet");
    return mRefCnt == 1;
  }

  void AddRef() {
    if (mRefCnt == UINT32_MAX) {
      NS_WARNING("refcount overflow, leaking object");
      return;
    }
    ++mRefCnt;
    NS_LOG_ADDREF(this, mRefCnt, "nsStyleContext", sizeof(nsStyleContext));
    return;
  }

  void Release() {
    if (mRefCnt == UINT32_MAX) {
      NS_WARNING("refcount overflow, leaking object");
      return;
    }
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "nsStyleContext");
    if (mRefCnt == 0) {
      Destroy();
      return;
    }
    return;
  }

  ~GeckoStyleContext();

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
  void SwapStyleData(GeckoStyleContext* aNewContext, uint32_t aStructs);

  void DestroyCachedStructs(nsPresContext* aPresContext);

  /**
   * Return style data that is currently cached on the style context.
   * Only returns the structs we cache ourselves; never consults the
   * rule tree.
   *
   * For "internal" use only in nsStyleContext and nsRuleNode.
   */
  const void* GetCachedStyleData(nsStyleStructID aSID)
  {
    const void* cachedData;
    if (nsCachedStyleData::IsReset(aSID)) {
      if (mCachedResetData) {
        cachedData = mCachedResetData->mStyleStructs[aSID];
      } else {
        cachedData = nullptr;
      }
    } else {
      cachedData = mCachedInheritedData.mStyleStructs[aSID];
    }
    return cachedData;
  }

  // mCachedInheritedData and mCachedResetData point to both structs that
  // are owned by this style context and structs that are owned by one of
  // this style context's ancestors (which are indirectly owned since this
  // style context owns a reference to its parent).  If the bit in |mBits|
  // is set for a struct, that means that the pointer for that struct is
  // owned by an ancestor or by the rule node rather than by this style context.
  // Since style contexts typically have some inherited data but only sometimes
  // have reset data, we always allocate the mCachedInheritedData, but only
  // sometimes allocate the mCachedResetData.
  nsResetStyleData*       mCachedResetData; // Cached reset style data.
  nsInheritedStyleData    mCachedInheritedData; // Cached inherited style data

  uint32_t mRefCnt;

#ifdef DEBUG
  void AssertStructsNotUsedElsewhere(GeckoStyleContext* aDestroyingContext,
                                     int32_t aLevels) const;
#endif

private:
  // Helper for ClearCachedInheritedStyleDataOnDescendants.
  void DoClearCachedInheritedStyleDataOnDescendants(uint32_t aStructs);
  void Destroy();

  // Children are kept in two circularly-linked lists.  The list anchor
  // is not part of the list (null for empty), and we point to the first
  // child.
  // mEmptyChild for children whose rule node is the root rule node, and
  // mChild for other children.  The order of children is not
  // meaningful.
  GeckoStyleContext* mChild;
  GeckoStyleContext* mEmptyChild;
  GeckoStyleContext* mPrevSibling;
  GeckoStyleContext* mNextSibling;
  RefPtr<nsRuleNode> mRuleNode;

  // Style to be used instead for the R, G, and B components of color,
  // background-color, and border-*-color if the nearest ancestor link
  // element is visited (see RelevantLinkVisited()).
  RefPtr<GeckoStyleContext> mStyleIfVisited;

#ifdef DEBUG
public:
  struct AutoCheckDependency {

    GeckoStyleContext* mStyleContext;
    nsStyleStructID mOuterSID;

    AutoCheckDependency(GeckoStyleContext* aContext, nsStyleStructID aInnerSID)
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

private:
  // Used to check for undeclared dependencies.
  // See AUTO_CHECK_DEPENDENCY in nsStyleContextInlines.h.
  nsStyleStructID         mComputingStruct;

#define AUTO_CHECK_DEPENDENCY(gecko_, sid_) \
  mozilla::GeckoStyleContext::AutoCheckDependency checkNesting_(gecko_, sid_)
#else
#define AUTO_CHECK_DEPENDENCY(gecko_, sid_)
#endif
};
}

#endif // mozilla_GeckoStyleContext_h

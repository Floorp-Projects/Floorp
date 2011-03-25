/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   David Hyatt <hyatt@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

/* the interface (to internal code) for retrieving computed style data */

#ifndef _nsStyleContext_h_
#define _nsStyleContext_h_

#include "nsRuleNode.h"
#include "nsIAtom.h"
#include "nsCSSPseudoElements.h"

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

class nsStyleContext
{
public:
  nsStyleContext(nsStyleContext* aParent, nsIAtom* aPseudoTag,
                 nsCSSPseudoElements::Type aPseudoType,
                 nsRuleNode* aRuleNode, nsPresContext* aPresContext);
  ~nsStyleContext();

  void* operator new(size_t sz, nsPresContext* aPresContext) CPP_THROW_NEW;
  void Destroy();

  nsrefcnt AddRef() {
    if (mRefCnt == PR_UINT32_MAX) {
      NS_WARNING("refcount overflow, leaking object");
      return mRefCnt;
    }
    ++mRefCnt;
    NS_LOG_ADDREF(this, mRefCnt, "nsStyleContext", sizeof(nsStyleContext));
    return mRefCnt;
  }

  nsrefcnt Release() {
    if (mRefCnt == PR_UINT32_MAX) {
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

  nsPresContext* PresContext() const { return mRuleNode->GetPresContext(); }

  nsStyleContext* GetParent() const { return mParent; }

  nsIAtom* GetPseudo() const { return mPseudoTag; }
  nsCSSPseudoElements::Type GetPseudoType() const {
    return static_cast<nsCSSPseudoElements::Type>(mBits >>
                                                  NS_STYLE_CONTEXT_TYPE_SHIFT);
  }

  // Find, if it already exists *and is easily findable* (i.e., near the
  // start of the child list), a style context whose:
  //  * GetPseudo() matches aPseudoTag
  //  * GetRuleNode() matches aRules
  //  * !GetStyleIfVisited() == !aRulesIfVisited, and, if they're
  //    non-null, GetStyleIfVisited()->GetRuleNode() == aRulesIfVisited
  //  * RelevantLinkVisited() == aRelevantLinkVisited
  already_AddRefed<nsStyleContext>
  FindChildWithRules(const nsIAtom* aPseudoTag, nsRuleNode* aRules,
                     nsRuleNode* aRulesIfVisited,
                     PRBool aRelevantLinkVisited);

  // Does this style context or any of its ancestors have text
  // decorations?
  PRBool HasTextDecorations() const
    { return !!(mBits & NS_STYLE_HAS_TEXT_DECORATIONS); }

  // Does this style context represent the style for a pseudo-element or
  // inherit data from such a style context?  Whether this returns true
  // is equivalent to whether it or any of its ancestors returns
  // non-null for GetPseudo.
  PRBool HasPseudoElementData() const
    { return !!(mBits & NS_STYLE_HAS_PSEUDO_ELEMENT_DATA); }

  // Is the only link whose visitedness is allowed to influence the
  // style of the node this style context is for (which is that element
  // or its nearest ancestor that is a link) visited?
  PRBool RelevantLinkVisited() const
    { return !!(mBits & NS_STYLE_RELEVANT_LINK_VISITED); }

  // Is this a style context for a link?
  PRBool IsLinkContext() const {
    return
      GetStyleIfVisited() && GetStyleIfVisited()->GetParent() == GetParent();
  }

  // Is this style context the GetStyleIfVisited() for some other style
  // context?
  PRBool IsStyleIfVisited() const
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
    NS_ABORT_IF_FALSE(!IsStyleIfVisited(), "this context is not visited data");
    NS_ABORT_IF_FALSE(aStyleIfVisited.get()->IsStyleIfVisited(),
                      "other context is visited data");
    NS_ABORT_IF_FALSE(!aStyleIfVisited.get()->GetStyleIfVisited(),
                      "other context does not have visited data");
    NS_ASSERTION(!mStyleIfVisited, "should only be set once");
    mStyleIfVisited = aStyleIfVisited;

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

  // Tell this style context to cache aStruct as the struct for aSID
  void SetStyle(nsStyleStructID aSID, void* aStruct);

  // Setters for inherit structs only, since rulenode only sets those eagerly.
  #define STYLE_STRUCT_INHERITED(name_, checkdata_cb_, ctor_args_)          \
    void SetStyle##name_ (nsStyle##name_ * aStruct) {                       \
      void *& slot =                                                        \
        mCachedInheritedData.mStyleStructs[eStyleStruct_##name_];           \
      NS_ASSERTION(!slot ||                                                 \
                   (mBits &                                                 \
                    nsCachedStyleData::GetBitForSID(eStyleStruct_##name_)), \
                   "Going to leak styledata");                              \
      slot = aStruct;                                                       \
    }
#define STYLE_STRUCT_RESET(name_, checkdata_cb_, ctor_args_) /* nothing */
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT_RESET
  #undef STYLE_STRUCT_INHERITED

  nsRuleNode* GetRuleNode() { return mRuleNode; }
  void AddStyleBit(const PRUint32& aBit) { mBits |= aBit; }

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
   * function, bothe because they're easier to read and  because they're
   * faster.
   */
  const void* NS_FASTCALL GetStyleData(nsStyleStructID aSID);

  /**
   * Define typesafe getter functions for each style struct by
   * preprocessing the list of style structs.  These functions are the
   * preferred way to get style data.  The macro creates functions like:
   *   const nsStyleBorder* GetStyleBorder();
   *   const nsStyleColor* GetStyleColor();
   */
  #define STYLE_STRUCT(name_, checkdata_cb_, ctor_args_)  \
    const nsStyle##name_ * GetStyle##name_() {            \
      return DoGetStyle##name_(PR_TRUE);                  \
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
  #define STYLE_STRUCT(name_, checkdata_cb_, ctor_args_)  \
    const nsStyle##name_ * PeekStyle##name_() {           \
      return DoGetStyle##name_(PR_FALSE);                 \
    }
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT

  void* GetUniqueStyleData(const nsStyleStructID& aSID);

  nsChangeHint CalcStyleDifference(nsStyleContext* aOther);

  /**
   * Get a color that depends on link-visitedness using this and
   * this->GetStyleIfVisited().
   *
   * aProperty must be a color-valued property that nsStyleAnimation
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
                                      PRBool aLinkIsVisited);

  /**
   * Allocate a chunk of memory that is scoped to the lifetime of this
   * style context, i.e., memory that will automatically be freed when
   * this style context is destroyed.  This is intended for allocations
   * that are stored on this style context or its style structs.  (Use
   * on style structs is fine since any style context to which this
   * context's style structs are shared will be a descendant of this
   * style context and thus keep it alive.)
   *
   * This currently allocates the memory out of the pres shell arena.
   *
   * It would be relatively straightforward to write a Free method
   * for the underlying implementation, but we don't need it (or the
   * overhead of making a doubly-linked list or other structure to
   * support it).
   *
   * WARNING: Memory allocated using this method cannot be stored in the
   * rule tree, since rule nodes may outlive the style context.
   */
  void* Alloc(size_t aSize);

  /**
   * Start the background image loads for this style context.
   */
  void StartBackgroundImageLoads() {
    // Just get our background struct; that should do the trick
    GetStyleBackground();
  }

#ifdef DEBUG
  void List(FILE* out, PRInt32 aIndent);
#endif

protected:
  void AddChild(nsStyleContext* aChild);
  void RemoveChild(nsStyleContext* aChild);

  void ApplyStyleFixups(nsPresContext* aPresContext);

  void FreeAllocations(nsPresContext* aPresContext);

  // Helper function that GetStyleData and GetUniqueStyleData use.  Only
  // returns the structs we cache ourselves; never consults the ruletree.
  inline const void* GetCachedStyleData(nsStyleStructID aSID);

  // Helper functions for GetStyle* and PeekStyle*
  #define STYLE_STRUCT_INHERITED(name_, checkdata_cb_, ctor_args_)      \
    const nsStyle##name_ * DoGetStyle##name_(PRBool aComputeData) {     \
      const nsStyle##name_ * cachedData =                               \
        static_cast<nsStyle##name_*>(                                   \
          mCachedInheritedData.mStyleStructs[eStyleStruct_##name_]);    \
      if (cachedData) /* Have it cached already, yay */                 \
        return cachedData;                                              \
      /* Have the rulenode deal */                                      \
      return mRuleNode->GetStyle##name_(this, aComputeData);            \
    }
  #define STYLE_STRUCT_RESET(name_, checkdata_cb_, ctor_args_)          \
    const nsStyle##name_ * DoGetStyle##name_(PRBool aComputeData) {     \
      const nsStyle##name_ * cachedData = mCachedResetData              \
        ? static_cast<nsStyle##name_*>(                                 \
            mCachedResetData->mStyleStructs[eStyleStruct_##name_])      \
        : nsnull;                                                       \
      if (cachedData) /* Have it cached already, yay */                 \
        return cachedData;                                              \
      /* Have the rulenode deal */                                      \
      return mRuleNode->GetStyle##name_(this, aComputeData);            \
    }
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT_RESET
  #undef STYLE_STRUCT_INHERITED

  nsStyleContext* const mParent; // STRONG

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

  // Private to nsStyleContext::Alloc and FreeAllocations.
  struct AllocationHeader {
    AllocationHeader* mNext;
    size_t mSize;

    void* mStorageStart; // ensure the storage is at least pointer-aligned
  };
  AllocationHeader*       mAllocations;

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
  PRUint32                mBits; // Which structs are inherited from the
                                 // parent context or owned by mRuleNode.
  PRUint32                mRefCnt;
};

already_AddRefed<nsStyleContext>
NS_NewStyleContext(nsStyleContext* aParentContext,
                   nsIAtom* aPseudoTag,
                   nsCSSPseudoElements::Type aPseudoType,
                   nsRuleNode* aRuleNode,
                   nsPresContext* aPresContext);
#endif

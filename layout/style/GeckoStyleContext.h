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
  GeckoStyleContext(nsStyleContext* aParent,
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

  void* GetUniqueStyleData(const nsStyleStructID& aSID);
  void* CreateEmptyStyleData(const nsStyleStructID& aSID);


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
  void AssertChildStructsNotUsedElsewhere(nsStyleContext* aDestroyingContext,
                                          int32_t aLevels) const;
  void ListDescendants(FILE* out, int32_t aIndent);

#endif

#ifdef RESTYLE_LOGGING
  void LogChildStyleContextTree(uint32_t aStructs) const;
#endif

  // Only called for Gecko-backed nsStyleContexts.
  void ApplyStyleFixups(bool aSkipParentDisplayBasedStyleFixup);

  bool HasNoChildren() const;

  NonOwningStyleContextSource StyleSource() const {
    return NonOwningStyleContextSource(mRuleNode);
  }

  nsRuleNode* RuleNode() const {
    MOZ_ASSERT(mRuleNode);
    return mRuleNode;
  }

  ~GeckoStyleContext() {
    Destructor();
  }

private:
  // Helper for ClearCachedInheritedStyleDataOnDescendants.
  void DoClearCachedInheritedStyleDataOnDescendants(uint32_t aStructs);

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
};

}

#endif // mozilla_GeckoStyleContext_h

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
    return mSource.AsGeckoRuleNode()->PresContext();
  }

  void AddChild(GeckoStyleContext* aChild);
  void RemoveChild(GeckoStyleContext* aChild);
  /**
   * On each descendant of this style context, clears out any cached inherited
   * structs indicated in aStructs.
   */
  void ClearCachedInheritedStyleDataOnDescendants(uint32_t aStructs);
  // Find, if it already exists *and is easily findable* (i.e., near the
  // start of the child list), a style context whose:
  //  * GetPseudo() matches aPseudoTag
  //  * mSource matches aSource
  //  * !!GetStyleIfVisited() == !!aSourceIfVisited, and, if they're
  //    non-null, GetStyleIfVisited()->mSource == aSourceIfVisited
  //  * RelevantLinkVisited() == aRelevantLinkVisited
  already_AddRefed<GeckoStyleContext>
  FindChildWithRules(const nsIAtom* aPseudoTag,
                     mozilla::NonOwningStyleContextSource aSource,
                     mozilla::NonOwningStyleContextSource aSourceIfVisited,
                     bool aRelevantLinkVisited);

#ifdef DEBUG
  void ListDescendants(FILE* out, int32_t aIndent);
#endif

private:
  // Helper for ClearCachedInheritedStyleDataOnDescendants.
  void DoClearCachedInheritedStyleDataOnDescendants(uint32_t aStructs);


public:
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
};

}

#endif // mozilla_GeckoStyleContext_h

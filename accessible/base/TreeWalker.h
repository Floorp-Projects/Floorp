/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_TreeWalker_h_
#define mozilla_a11y_TreeWalker_h_

#include "mozilla/Attributes.h"
#include <stdint.h>
#include "mozilla/dom/ChildIterator.h"
#include "nsCOMPtr.h"

class nsIContent;

namespace mozilla {
namespace a11y {

class Accessible;
class DocAccessible;

/**
 * This class is used to walk the DOM tree to create accessible tree.
 */
class TreeWalker final
{
public:
  enum {
    // used to walk the existing tree of the given node
    eWalkCache = 1,
    // used to walk the context tree starting from given node
    eWalkContextTree = 2 | eWalkCache
  };

  /**
   * Constructor
   *
   * @param aContext [in] container accessible for the given node, used to
   *                   define accessible context
   * @param aNode    [in] the node the search will be prepared relative to
   * @param aFlags   [in] flags (see enum above)
   */
  TreeWalker(Accessible* aContext, nsIContent* aNode, uint32_t aFlags = 0);
  ~TreeWalker();

  /**
   * Return the next child accessible.
   *
   * @note Returned accessible is bound to the document, if the accessible is
   *       rejected during tree creation then the caller should be unbind it
   *       from the document.
   */
  Accessible* NextChild();

private:
  TreeWalker();
  TreeWalker(const TreeWalker&);
  TreeWalker& operator =(const TreeWalker&);

  struct ChildrenIterator {
    ChildrenIterator(nsIContent* aNode, uint32_t aFilter) :
      mDOMIter(aNode, aFilter), mARIAOwnsIdx(0) { }

    dom::AllChildrenIterator mDOMIter;
    uint32_t mARIAOwnsIdx;
  };

  nsIContent* Next(ChildrenIterator* aIter, Accessible** aAccessible = nullptr,
                   bool* aSkipSubtree = nullptr);

  /**
   * Create new state for the given node and push it on top of stack.
   *
   * @note State stack is used to navigate up/down the DOM subtree during
   *        accessible children search.
   */
  ChildrenIterator* PushState(nsIContent* aContent)
  {
    return mStateStack.AppendElement(ChildrenIterator(aContent, mChildFilter));
  }

  /**
   * Pop state from stack.
   */
  ChildrenIterator* PopState();

  DocAccessible* mDoc;
  Accessible* mContext;
  nsIContent* mAnchorNode;
  nsAutoTArray<ChildrenIterator, 20> mStateStack;
  int32_t mChildFilter;
  uint32_t mFlags;
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_TreeWalker_h_

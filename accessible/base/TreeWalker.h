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
   * Used to navigate and create if needed the accessible children.
   */
  explicit TreeWalker(Accessible* aContext);

  /**
   * Used to navigate the accessible children relative to the anchor.
   *
   * @param aContext [in] container accessible for the given node, used to
   *                   define accessible context
   * @param aAnchorNode [in] the node the search will be prepared relative to
   * @param aFlags   [in] flags (see enum above)
   */
  TreeWalker(Accessible* aContext, nsIContent* aAnchorNode, uint32_t aFlags = eWalkCache);

  /**
   * Navigates the accessible children within the anchor node subtree.
   */
  TreeWalker(DocAccessible* aDocument, nsIContent* aAnchorNode);

  ~TreeWalker();

  /**
   * Resets the walker state, and sets the given node as an anchor. Returns a
   * first accessible element within the node including the node itself.
   */
  Accessible* Scope(nsIContent* aAnchorNode);

  /**
   * Resets the walker state.
   */
  void Reset()
  {
    mPhase = eAtStart;
    mStateStack.Clear();
    mARIAOwnsIdx = 0;
  }

  /**
   * Sets the walker state to the given child node if it's within the anchor.
   */
  bool Seek(nsIContent* aChildNode);

  /**
   * Return the next/prev accessible.
   *
   * @note Returned accessible is bound to the document, if the accessible is
   *       rejected during tree creation then the caller should be unbind it
   *       from the document.
   */
  Accessible* Next();
  Accessible* Prev();

  Accessible* Context() const { return mContext; }
  DocAccessible* Document() const { return mDoc; }

private:
  TreeWalker();
  TreeWalker(const TreeWalker&);
  TreeWalker& operator =(const TreeWalker&);

  /**
   * Return an accessible for the given node if any.
   */
  Accessible* AccessibleFor(nsIContent* aNode, uint32_t aFlags,
                            bool* aSkipSubtree);

  /**
   * Create new state for the given node and push it on top of stack / at bottom
   * of stack.
   *
   * @note State stack is used to navigate up/down the DOM subtree during
   *        accessible children search.
   */
  dom::AllChildrenIterator* PushState(nsIContent* aContent,
                                      bool aStartAtBeginning)
  {
    return mStateStack.AppendElement(
      dom::AllChildrenIterator(aContent, mChildFilter, aStartAtBeginning));
  }
  dom::AllChildrenIterator* PrependState(nsIContent* aContent,
                                         bool aStartAtBeginning)
  {
    return mStateStack.InsertElementAt(0,
      dom::AllChildrenIterator(aContent, mChildFilter, aStartAtBeginning));
  }

  /**
   * Pop state from stack.
   */
  dom::AllChildrenIterator* PopState();

  DocAccessible* mDoc;
  Accessible* mContext;
  nsIContent* mAnchorNode;

  AutoTArray<dom::AllChildrenIterator, 20> mStateStack;
  uint32_t mARIAOwnsIdx;

  int32_t mChildFilter;
  uint32_t mFlags;

  enum Phase {
    eAtStart,
    eAtDOM,
    eAtARIAOwns,
    eAtEnd
  };
  Phase mPhase;
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_TreeWalker_h_

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_TreeWalker_h_
#define mozilla_a11y_TreeWalker_h_

#include <stdint.h>

class nsIContent;

namespace mozilla {
namespace a11y {

class Accessible;
class DocAccessible;

struct WalkState;

/**
 * This class is used to walk the DOM tree to create accessible tree.
 */
class TreeWalker
{
public:
  TreeWalker(Accessible* aContext, nsIContent* aNode, bool aWalkCache = false);
  virtual ~TreeWalker();

  /**
   * Return the next child accessible.
   *
   * @note Returned accessible is bound to the document, if the accessible is
   *       rejected during tree creation then the caller should be unbind it
   *       from the document.
   */
  Accessible* NextChild()
  {
    return NextChildInternal(false);
  }

private:
  TreeWalker();
  TreeWalker(const TreeWalker&);
  TreeWalker& operator =(const TreeWalker&);

  /**
   * Return the next child accessible.
   *
   * @param  aNoWalkUp  [in] specifies the walk direction, true means we
   *                     shouldn't go up through the tree if we failed find
   *                     accessible children.
   */
  Accessible* NextChildInternal(bool aNoWalkUp);

  /**
   * Create new state for the given node and push it on top of stack.
   *
   * @note State stack is used to navigate up/down the DOM subtree during
   *        accessible children search.
   */
  bool PushState(nsIContent *aNode);

  /**
   * Pop state from stack.
   */
  void PopState();

  DocAccessible* mDoc;
  Accessible* mContext;
  int32_t mChildFilter;
  bool mWalkCache;
  WalkState* mState;
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_TreeWalker_h_

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsAccTreeWalker_H_
#define _nsAccTreeWalker_H_

#include "nsAutoPtr.h"
#include "nsIContent.h"

class nsAccessible;
class nsDocAccessible;
struct WalkState;

/**
 * This class is used to walk the DOM tree to create accessible tree.
 */
class nsAccTreeWalker
{
public:
  nsAccTreeWalker(nsDocAccessible* aDoc, nsIContent* aNode, 
                  bool aWalkAnonymousContent, bool aWalkCache = false);
  virtual ~nsAccTreeWalker();

  /**
   * Return the next child accessible.
   *
   * @note Returned accessible is bound to the document, if the accessible is
   *       rejected during tree creation then the caller should be unbind it
   *       from the document.
   */
  inline nsAccessible* NextChild()
  {
    return NextChildInternal(false);
  }

private:

  /**
   * Return the next child accessible.
   *
   * @param  aNoWalkUp  [in] specifies the walk direction, true means we
   *                     shouldn't go up through the tree if we failed find
   *                     accessible children.
   */
  nsAccessible* NextChildInternal(bool aNoWalkUp);

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

  nsDocAccessible* mDoc;
  PRInt32 mChildFilter;
  bool mWalkCache;
  WalkState* mState;
};

#endif 

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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Aaron Leventhal <aaronleventhal@moonset.net> (original author)
 *  Alexander Surkov <surkov.alexander@gmail.com>
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

#ifndef _nsAccTreeWalker_H_
#define _nsAccTreeWalker_H_

#include "nsAutoPtr.h"
#include "nsIContent.h"
#include "nsIWeakReference.h"

class nsAccessible;
struct WalkState;

/**
 * This class is used to walk the DOM tree to create accessible tree.
 */
class nsAccTreeWalker
{
public:
  nsAccTreeWalker(nsIWeakReference *aShell, nsIContent *aNode, 
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

  nsCOMPtr<nsIWeakReference> mWeakShell;
  PRInt32 mChildFilter;
  bool mWalkCache;
  WalkState* mState;
};

#endif 

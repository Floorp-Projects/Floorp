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
 *   Original Author: Aaron Leventhal (aaronl@netscape.com)
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

#ifndef _nsAccessibleTreeWalker_H_
#define _nsAccessibleTreeWalker_H_

/* For documentation of the accessibility architecture,  * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIAccessible.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIAccessibilityService.h"
#include "nsIWeakReference.h"

enum { eSiblingsUninitialized = -1, eSiblingsWalkFrames = -2 };

struct WalkState {
  nsCOMPtr<nsIAccessible> accessible;
  nsCOMPtr<nsIDOMNode> domNode;
  nsCOMPtr<nsIDOMNodeList> siblingList;
  nsIContent *parentContent; // For walking normal DOM
  WalkState *prevState;
  nsIFrame *frame;     // Helps avoid GetPrimaryFrameFor() calls
  PRInt32 siblingIndex;    // Holds a state flag or an index into the siblingList
  PRBool isHidden;         // Don't enter subtree if hidden
};
 
/** This class is used to walk the DOM tree. It skips
  * everything but nodes that either implement nsIAccessibleProvider
  * or have primary frames that implement "GetAccessible"
  */

class nsAccessibleTreeWalker {
public:
  nsAccessibleTreeWalker(nsIWeakReference* aShell, nsIDOMNode* aContent, 
    PRBool mWalkAnonymousContent);
  virtual ~nsAccessibleTreeWalker();

  /**
   * Moves current state to point to the next child accessible.
   */
  NS_IMETHOD GetNextSibling();

  /**
   * Moves current state to point to the first child accessible.
   */
  NS_IMETHOD GetFirstChild();

  /**
   * Current state. Used to initialize a11y tree walker and to get an accessible
   * current state points to.
   */
  WalkState mState;

protected:

  /**
   * Return true if currently navigated node/frame is accessible.
   */
  PRBool GetAccessible();

  /**
   * Prepares current state to navigate through children of node/frame.
   */
  void GetKids(nsIDOMNode *aParent);

  /**
   * Clears the current state.
   */
  void ClearState();

  /**
   * Push current state on top of stack. State stack is used to navigate down to
   * DOM/frame subtree during searching of accessible children.
   */
  NS_IMETHOD PushState();

  /**
   * Pop state from stack and make it current.
   */
  NS_IMETHOD PopState();

  /**
   * Change current state so that its frame is changed to next frame.
   *
   * @param  aTryFirstChild  [in] points whether we should move to child or
   *                         sibling frame
   */
  void UpdateFrame(PRBool aTryFirstChild);

  /**
   * Change current state so that its node is changed to next node.
   */
  void GetNextDOMNode();

  nsCOMPtr<nsIWeakReference> mWeakShell;
  nsCOMPtr<nsIAccessibilityService> mAccService;
  PRBool mWalkAnonContent;
};

#endif 

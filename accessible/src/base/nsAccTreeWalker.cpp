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

#include "nsAccTreeWalker.h"

#include "nsAccessible.h"
#include "nsAccessibilityService.h"

#include "nsINodeList.h"
#include "nsIPresShell.h"

////////////////////////////////////////////////////////////////////////////////
// WalkState
////////////////////////////////////////////////////////////////////////////////

struct WalkState
{
  WalkState(nsIContent *aContent) :
    content(aContent), childIdx(0), prevState(nsnull) {}

  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsINodeList> childList;
  PRUint32 childIdx;
  WalkState *prevState;
};

////////////////////////////////////////////////////////////////////////////////
// nsAccTreeWalker
////////////////////////////////////////////////////////////////////////////////

nsAccTreeWalker::
  nsAccTreeWalker(nsIWeakReference* aShell, nsIContent* aContent,
                  PRBool aWalkAnonContent) :
  mWeakShell(aShell), mState(nsnull)
{
  NS_ASSERTION(aContent, "No node for the accessible tree walker!");

  if (aContent)
    mState = new WalkState(aContent);

  mChildFilter = aWalkAnonContent ? nsIContent::eAllChildren :
                                  nsIContent::eAllButXBL;

  mChildFilter |= nsIContent::eSkipPlaceholderContent;

  MOZ_COUNT_CTOR(nsAccTreeWalker);
}

nsAccTreeWalker::~nsAccTreeWalker()
{
  // Clear state stack from memory
  while (mState)
    PopState();

  MOZ_COUNT_DTOR(nsAccTreeWalker);
}

////////////////////////////////////////////////////////////////////////////////
// nsAccTreeWalker: private

already_AddRefed<nsAccessible>
nsAccTreeWalker::GetNextChildInternal(PRBool aNoWalkUp)
{
  if (!mState || !mState->content)
    return nsnull;

  if (!mState->childList)
    mState->childList = mState->content->GetChildren(mChildFilter);

  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));

  PRUint32 length = 0;
  if (mState->childList)
    mState->childList->GetLength(&length);

  while (mState->childIdx < length) {
    nsIContent* childNode = mState->childList->GetNodeAt(mState->childIdx);
    mState->childIdx++;

    bool isSubtreeHidden = false;
    nsRefPtr<nsAccessible> accessible =
      GetAccService()->GetOrCreateAccessible(childNode, presShell, mWeakShell,
                                             &isSubtreeHidden);

    if (accessible)
      return accessible.forget();

    // Walk down into subtree to find accessibles.
    if (!isSubtreeHidden) {
      if (!PushState(childNode))
        break;

      accessible = GetNextChildInternal(PR_TRUE);
      if (accessible)
        return accessible.forget();
    }
  }

  // No more children, get back to the parent.
  PopState();

  return aNoWalkUp ? nsnull : GetNextChildInternal(PR_FALSE);
}

void
nsAccTreeWalker::PopState()
{
  WalkState* prevToLastState = mState->prevState;
  delete mState;
  mState = prevToLastState;
}

PRBool
nsAccTreeWalker::PushState(nsIContent* aContent)
{
  WalkState* nextToLastState = new WalkState(aContent);
  if (!nextToLastState)
    return PR_FALSE;

  nextToLastState->prevState = mState;
  mState = nextToLastState;

  return PR_TRUE;
}

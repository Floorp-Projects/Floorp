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

#include "nsAccessibleTreeWalker.h"
#include "nsAccessibilityAtoms.h"
#include "nsAccessNode.h"
#include "nsIServiceManager.h"
#include "nsIContent.h"
#include "nsIDOMXULElement.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsWeakReference.h"

nsAccessibleTreeWalker::nsAccessibleTreeWalker(nsIWeakReference* aPresShell, nsIDOMNode* aNode, PRBool aWalkAnonContent): 
  mWeakShell(aPresShell), 
  mAccService(do_GetService("@mozilla.org/accessibilityService;1"))
{
  mState.domNode = aNode;
  mState.prevState = nsnull;
  mState.siblingIndex = eSiblingsUninitialized;
  mState.siblingList = nsnull;
  mState.isHidden = false;
  mState.frame = nsnull;

  if (aWalkAnonContent) {
    nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
    if (presShell)
      mBindingManager = presShell->GetDocument()->GetBindingManager();
  }
  MOZ_COUNT_CTOR(nsAccessibleTreeWalker);
}

nsAccessibleTreeWalker::~nsAccessibleTreeWalker()
{
  // Clear state stack from memory
  while (NS_SUCCEEDED(PopState()))
    /* do nothing */ ;
   MOZ_COUNT_DTOR(nsAccessibleTreeWalker);
}

// GetFullParentNode gets the parent node in the deep tree
// This might not be the DOM parent in cases where <children/> was used in an XBL binding.
// In that case, this returns the parent in the XBL'ized tree.

NS_IMETHODIMP nsAccessibleTreeWalker::GetFullTreeParentNode(nsIDOMNode *aChildNode, nsIDOMNode **aParentNodeOut)
{
  nsCOMPtr<nsIContent> childContent(do_QueryInterface(aChildNode));
  nsCOMPtr<nsIContent> bindingParentContent;
  nsCOMPtr<nsIDOMNode> parentNode;

  if (mState.prevState)
    parentNode = mState.prevState->domNode;
  else {
    if (mBindingManager) {
      mBindingManager->GetInsertionParent(childContent, getter_AddRefs(bindingParentContent));
      if (bindingParentContent) 
        parentNode = do_QueryInterface(bindingParentContent);
    }

    if (!parentNode) 
      aChildNode->GetParentNode(getter_AddRefs(parentNode));
  }

  if (parentNode) {
    *aParentNodeOut = parentNode;
    NS_ADDREF(*aParentNodeOut);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

void nsAccessibleTreeWalker::GetKids(nsIDOMNode *aParentNode)
{
  nsCOMPtr<nsIContent> parentContent(do_QueryInterface(aParentNode));
  if (!parentContent || !parentContent->IsContentOfType(nsIContent::eHTML)) {
    mState.frame = nsnull;  // Don't walk frames in non-HTML content, just walk the DOM.
  }

  PushState();
  UpdateFrame(PR_TRUE);

  // Walk frames? UpdateFrame() sets this when it sees anonymous frames
  if (mState.siblingIndex == eSiblingsWalkFrames) {
    return;
  }

  // Walk anonymous content? Not currently used for HTML -- anonymous content there uses frame walking
  if (parentContent && !parentContent->IsContentOfType(nsIContent::eHTML) && mBindingManager) {
    // Walk anonymous content
    mBindingManager->GetXBLChildNodesFor(parentContent, getter_AddRefs(mState.siblingList)); // returns null if no anon nodes
    if (mState.siblingList) {
      mState.siblingIndex = 0;   // Indicates our index into the sibling list
      mState.siblingList->Item(0, getter_AddRefs(mState.domNode));
      return;
    }
  }

  // Walk normal DOM
  mState.siblingIndex = eSiblingsWalkNormalDOM;  // Default value - indicates no sibling list
  if (aParentNode) {
    aParentNode->GetFirstChild(getter_AddRefs(mState.domNode));
  }
}

NS_IMETHODIMP nsAccessibleTreeWalker::GetParent()
{
  nsCOMPtr<nsIDOMNode> parent;

  while (NS_SUCCEEDED(GetFullTreeParentNode(mState.domNode, getter_AddRefs(parent)))) {
    if (NS_FAILED(PopState())) {
      mState.domNode = parent;
      GetAccessible();
    }
    if (mState.accessible)
      return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAccessibleTreeWalker::PopState()
{
  nsIFrame *frameParent = mState.frame? mState.frame->GetParent(): nsnull;
  if (mState.prevState) {
    WalkState *toBeDeleted = mState.prevState;
    mState = *mState.prevState; // deep copy
    mState.isHidden = PR_FALSE; // If we were in a child, the parent wasn't hidden
    if (!mState.frame) {
      mState.frame = frameParent;
    }
    delete toBeDeleted;
    return NS_OK;
  }
  ClearState();
  mState.frame = frameParent;
  mState.isHidden = PR_FALSE;
  return NS_ERROR_FAILURE;
}

void nsAccessibleTreeWalker::ClearState()
{
  mState.siblingList = nsnull;
  mState.accessible = nsnull;
  mState.domNode = nsnull;
  mState.siblingIndex = eSiblingsUninitialized;
}

NS_IMETHODIMP nsAccessibleTreeWalker::PushState()
{
  // Duplicate mState and put right before end; reset mState; make mState the new end of the stack
  WalkState* nextToLastState= new WalkState();
  if (!nextToLastState)
    return NS_ERROR_OUT_OF_MEMORY;
  *nextToLastState = mState;  // Deep copy - copy contents of struct to new state that will be added to end of our stack
  ClearState();
  mState.prevState = nextToLastState;   // Link to previous state
  return NS_OK;
}

void nsAccessibleTreeWalker::GetNextDOMNode()
{
  // Get next DOM node
  if (mState.siblingIndex == eSiblingsWalkNormalDOM) {
    nsCOMPtr<nsIDOMNode> currentNode = mState.domNode;
    currentNode->GetNextSibling(getter_AddRefs(mState.domNode));
  }
  else if (mState.siblingIndex == eSiblingsWalkFrames) {
    if (mState.frame) {
      mState.domNode = do_QueryInterface(mState.frame->GetContent());
    } else {
      mState = nsnull;
    }
  }
  else { 
    mState.siblingList->Item(++mState.siblingIndex, getter_AddRefs(mState.domNode));
  }
}

NS_IMETHODIMP nsAccessibleTreeWalker::GetNextSibling()
{
  // Make sure mState.prevState and mState.siblingIndex are initialized so we can walk forward
  NS_ASSERTION(mState.prevState && mState.siblingIndex != eSiblingsUninitialized,
               "Error - GetNextSibling() only works after a GetFirstChild(), so we must have a prevState.");
  mState.accessible = nsnull;

  while (PR_TRUE) {
    // Get next frame
    UpdateFrame(PR_FALSE);
    GetNextDOMNode();

    if (!mState.domNode) {  // Done with current siblings
      PopState();   // Use parent - go up in stack. Can always pop state because we have to start with a GetFirstChild().
      if (!mState.prevState) {
        mState.accessible = nsnull;
        break; // Back to original accessible that we did GetFirstChild() from
      }
    }
    else if ((mState.domNode != mState.prevState->domNode && GetAccessible()) || 
             NS_SUCCEEDED(GetFirstChild())) {
      return NS_OK; // if next is accessible, use it 
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAccessibleTreeWalker::GetFirstChild()
{
  mState.accessible = nsnull;
  if (mState.isHidden || !mState.domNode) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMNode> parent(mState.domNode);
  GetKids(parent); // Side effects change our state (mState)

  // Recursive loop: depth first search for first accessible child
  while (mState.domNode) {
    if ((mState.domNode != parent && GetAccessible()) || NS_SUCCEEDED(GetFirstChild()))
      return NS_OK;
    UpdateFrame(PR_FALSE);
    GetNextDOMNode();
  }

  PopState();  // Return to previous state
  return NS_ERROR_FAILURE;
}

void nsAccessibleTreeWalker::UpdateFrame(PRBool aTryFirstChild)
{
  if (mState.frame) {
    mState.frame = aTryFirstChild? mState.frame->GetFirstChild(nsnull) : 
                                   mState.frame->GetNextSibling();
#ifndef MOZ_ACCESSIBILITY_ATK
    if (mState.frame && mState.siblingIndex <0 && 
        mState.frame->IsGeneratedContentFrame()) {
      mState.domNode = do_QueryInterface(mState.frame->GetContent());
      mState.siblingIndex = eSiblingsWalkFrames;
    }
#endif
  }
}

/**
 * If the DOM node's frame has an accessible or the DOMNode
 * itself implements nsIAccessible return it.
 */
PRBool nsAccessibleTreeWalker::GetAccessible()
{
  if (!mAccService) {
    return false;
  }
  mState.accessible = nsnull;
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));

  if (NS_SUCCEEDED(mAccService->GetAccessible(mState.domNode, presShell, mWeakShell, 
                                              &mState.frame, &mState.isHidden,
                                              getter_AddRefs(mState.accessible)))) {
    NS_ASSERTION(mState.accessible, "No accessible but no failure return code");
    return true;
  }
  return false;
}


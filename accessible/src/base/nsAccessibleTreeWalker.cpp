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
#include "nsWeakReference.h"
#include "nsAccessNode.h"
#include "nsIServiceManager.h"
#include "nsIContent.h"
#include "nsIDOMXULElement.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"

nsAccessibleTreeWalker::nsAccessibleTreeWalker(nsIWeakReference* aPresShell, nsIDOMNode* aNode, PRBool aWalkAnonContent): 
  mWeakShell(aPresShell), 
  mAccService(do_GetService("@mozilla.org/accessibilityService;1"))
{
  mState.domNode = aNode;
  mState.prevState = nsnull;
  mState.siblingIndex = eSiblingsUninitialized;
  mState.siblingList = nsnull;
  mState.isHidden = false;
  mState.frameHint = nsnull;

  if (aWalkAnonContent) {
    nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
    if (presShell) {
      nsCOMPtr<nsIDocument> doc;
      presShell->GetDocument(getter_AddRefs(doc)); 
      mBindingManager = doc->GetBindingManager();
    }
  }
  MOZ_COUNT_CTOR(nsAccessibleTreeWalker);
  mInitialState = mState;  // deep copy
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
  nsCOMPtr<nsIContent> content(do_QueryInterface(aParentNode));

  mState.siblingIndex = eSiblingsWalkNormalDOM;  // Default value - indicates no sibling list

  if (content && mBindingManager) {
    mBindingManager->GetXBLChildNodesFor(content, getter_AddRefs(mState.siblingList)); // returns null if no anon nodes
    if (mState.siblingList) 
      mState.siblingIndex = 0;   // Indicates our index into the sibling list
  }
}

void nsAccessibleTreeWalker::GetSiblings(nsIDOMNode *aOneOfTheSiblings)
{
  nsCOMPtr<nsIDOMNode> node;

  mState.siblingIndex = eSiblingsWalkNormalDOM; // Default value

  if (NS_SUCCEEDED(GetFullTreeParentNode(aOneOfTheSiblings, getter_AddRefs(node)))) {
    GetKids(node);
    if (mState.siblingList) {      // Init index by seeing how far we are into list
      if (mState.domNode == mInitialState.domNode)
        mInitialState = mState; // deep copy, we'll use sibling info for caching
      while (NS_SUCCEEDED(mState.siblingList->Item(mState.siblingIndex, getter_AddRefs(node))) && node != mState.domNode) {
        NS_ASSERTION(node, "Something is terribly wrong - the child is not in it's parent's children!");
        ++mState.siblingIndex;
      }
    }
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
  nsIFrame *frameHintParent = mState.frameHint? mState.frameHint->GetParent(): nsnull;
  if (mState.prevState) {
    WalkState *toBeDeleted = mState.prevState;
    mState = *mState.prevState; // deep copy
    mState.isHidden = PR_FALSE; // If we were in a child, the parent wasn't hidden
    if (!mState.frameHint) {
      mState.frameHint = frameHintParent;
    }
    delete toBeDeleted;
    return NS_OK;
  }
  ClearState();
  mState.frameHint = frameHintParent;
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

NS_IMETHODIMP nsAccessibleTreeWalker::GetNextSibling()
{
  mState.accessible = nsnull;

  // Make sure mState.siblingIndex and mState.siblingList are initialized
  if (mState.siblingIndex == eSiblingsUninitialized) 
    GetSiblings(mState.domNode);

  // get next sibling
  nsCOMPtr<nsIDOMNode> next;

  while (PR_TRUE) {
    if (mState.siblingIndex == eSiblingsWalkNormalDOM)
      mState.domNode->GetNextSibling(getter_AddRefs(next));
    else 
      mState.siblingList->Item(++mState.siblingIndex, getter_AddRefs(next));

    if (!next) {  // Done with siblings
      // if no DOM parent or DOM parent is accessible fail
      nsCOMPtr<nsIDOMNode> parent;
      if (NS_FAILED(GetFullTreeParentNode(mState.domNode, getter_AddRefs(parent))))
        break; // Failed - can't get parent node, we're at the top 

      if (NS_FAILED(PopState())) {   // Use parent - go up in stack
        mState.domNode = parent;
      }
      if (mState.siblingIndex == eSiblingsUninitialized) 
        GetSiblings(mState.domNode);

      if (GetAccessible()) {
        mState.accessible = nsnull;
        break; // Failed - anything after this in the tree is in a new group of siblings
      }
    }
    else {
      UpdateFrameHint(next, false);
      // if next is accessible, use it 
      mState.domNode = next;
      if (GetAccessible())
        return NS_OK;

      // otherwise call first on next
      mState.domNode = next;
      if (NS_SUCCEEDED(GetFirstChild()))
        return NS_OK;

      // If no results, keep recursiom going - call next on next
      mState.domNode = next;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAccessibleTreeWalker::GetFirstChild()
{
  if (mState.isHidden) {
    return NS_ERROR_FAILURE;
  }
  mState.accessible = nsnull;

  if (!mState.domNode)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> next, parent(mState.domNode);

  PushState(); // Save old state

  GetKids(parent); // Side effects change our state

  if (mState.siblingIndex == eSiblingsWalkNormalDOM)  // Indicates we must use normal DOM calls to traverse here
    parent->GetFirstChild(getter_AddRefs(next));
  else  // Use the sibling list - there are anonymous content nodes in here
    mState.siblingList->Item(0, getter_AddRefs(next));
  UpdateFrameHint(next, true);

  // Recursive loop: depth first search for first accessible child
  while (next) {
    mState.domNode = next;
    if (GetAccessible() || NS_SUCCEEDED(GetFirstChild()))
      return NS_OK;
    if (mState.siblingIndex == eSiblingsWalkNormalDOM)  // Indicates we must use normal DOM calls to traverse here
      mState.domNode->GetNextSibling(getter_AddRefs(next));
    else 
      mState.siblingList->Item(++mState.siblingIndex, getter_AddRefs(next));
    UpdateFrameHint(next, false);
  }

  PopState();  // Return to previous state
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAccessibleTreeWalker::GetChildBefore(nsIDOMNode* aParent, nsIDOMNode* aChild)
{
  mState.accessible = nsnull;
  mState.domNode = aParent;

  if (!mState.domNode || NS_FAILED(GetFirstChild()) || mState.domNode == aChild) 
    return NS_ERROR_FAILURE;   // if the first child is us, then we fail, because there is no child before the first

  nsCOMPtr<nsIDOMNode> prevDOMNode(mState.domNode);
  nsCOMPtr<nsIAccessible> prevAccessible(mState.accessible);

  while (mState.domNode && NS_SUCCEEDED(GetNextSibling()) && mState.domNode != aChild) {
    prevDOMNode = mState.domNode;
    prevAccessible = mState.accessible;
  }

  mState.accessible = prevAccessible;
  mState.domNode = prevDOMNode;

  return NS_OK;
}

NS_IMETHODIMP nsAccessibleTreeWalker::GetPreviousSibling()
{
  nsCOMPtr<nsIDOMNode> child(mState.domNode);
  nsresult rv = GetParent();
  if (NS_SUCCEEDED(rv))
    rv = GetChildBefore(mState.domNode, child);
  return rv;
}

NS_IMETHODIMP nsAccessibleTreeWalker::GetLastChild()
{
  return GetChildBefore(mState.domNode, nsnull);
}

void nsAccessibleTreeWalker::UpdateFrameHint(nsIDOMNode *aStartNode, PRBool aTryFirstChild)
{
  if (mState.frameHint) {
    nsIFrame *testFrame = aTryFirstChild? mState.frameHint->GetFirstChild(nsnull) :
                                          mState.frameHint->GetNextSibling();
    nsCOMPtr<nsIContent> content(do_QueryInterface(aStartNode));
    if (testFrame && content && content == testFrame->GetContent()) {
      mState.frameHint = testFrame;
    }
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
                                              &mState.frameHint, &mState.isHidden,
                                              getter_AddRefs(mState.accessible)))) {
    NS_ASSERTION(mState.accessible, "No accessible but no failure return code");
    return true;
  }
  return false;
}


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
  mAccService(do_GetService("@mozilla.org/accessibilityService;1")),
  mWalkAnonContent(aWalkAnonContent)
{
  mState.domNode = aNode;
  mState.prevState = nsnull;
  mState.siblingIndex = eSiblingsUninitialized;
  mState.siblingList = nsnull;
  mState.isHidden = false;
  mState.frame = nsnull;

  MOZ_COUNT_CTOR(nsAccessibleTreeWalker);
}

nsAccessibleTreeWalker::~nsAccessibleTreeWalker()
{
  // Clear state stack from memory
  while (NS_SUCCEEDED(PopState()))
    /* do nothing */ ;
   MOZ_COUNT_DTOR(nsAccessibleTreeWalker);
}

void nsAccessibleTreeWalker::GetKids(nsIDOMNode *aParentNode)
{
  nsCOMPtr<nsIContent> parentContent(do_QueryInterface(aParentNode));
  if (!parentContent || !parentContent->IsNodeOfType(nsINode::eHTML)) {
    mState.frame = nsnull;  // Don't walk frames in non-HTML content, just walk the DOM.
  }

  PushState();
  UpdateFrame(PR_TRUE);

  // Walk frames? UpdateFrame() sets this when it sees anonymous frames
  if (mState.siblingIndex == eSiblingsWalkFrames) {
    return;
  }

  // Walk anonymous content? Not currently used for HTML -- anonymous content there uses frame walking
  mState.siblingIndex = 0;   // Indicates our index into the sibling list
  if (parentContent) {
    if (mWalkAnonContent) {
      // Walk anonymous content
      nsIDocument* doc = parentContent->GetOwnerDoc();
      if (doc) {
        // returns null if no anon nodes
        doc->GetXBLChildNodesFor(parentContent,
                                 getter_AddRefs(mState.siblingList));
      }
    }
    if (!mState.siblingList) {
      // Walk normal DOM. Just use nsIContent -- it doesn't require 
      // the mallocs that GetChildNodes() needs
      //aParentNode->GetChildNodes(getter_AddRefs(mState.siblingList));
      mState.parentContent = parentContent;
      mState.domNode = do_QueryInterface(parentContent->GetChildAt(0 /* 0 == mState.siblingIndex */));
      return;
    }
  }
  else {
    // We're on document node, that's why we could not QI to nsIContent.
    // So, use nsIDOMNodeList method to walk content.
    aParentNode->GetChildNodes(getter_AddRefs(mState.siblingList));
    if (!mState.siblingList) {
      return;
    }
  }

  mState.siblingList->Item(0 /* 0 == mState.siblingIndex */, getter_AddRefs(mState.domNode));
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
  mState.parentContent = nsnull;
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
  if (mState.parentContent) {
    mState.domNode = do_QueryInterface(mState.parentContent->GetChildAt(++mState.siblingIndex));
  }
  else if (mState.siblingIndex == eSiblingsWalkFrames) {
    if (mState.frame) {
      mState.domNode = do_QueryInterface(mState.frame->GetContent());
    } else {
      mState.domNode = nsnull;
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
  if (!mState.frame) {
    return;
  }
  if (aTryFirstChild) {
    mState.frame = mState.frame->GetFirstChild(nsnull);
// temporary workaround for Bug 359210. We never want to walk frames.
// Aaron Leventhal will refix :before and :after content later without walking frames.
#if 0
    if (mState.frame && mState.siblingIndex < 0) {
      // Container frames can contain generated content frames from
      // :before and :after style rules, so we walk their frame trees
      // instead of content trees
      // XXX Walking the frame tree doesn't get us Aural CSS nodes, e.g. 
      // @media screen { display: none; }
      // Asking the style system might be better (with ProbePseudoStyleFor(),
      // except that we need to ask only for those display types that support 
      // :before and :after (which roughly means non-replaced elements)
      // Here's some code to see if there is an :after rule for an element
      // nsRefPtr<nsStyleContext> pseudoContext;
      // nsStyleContext *styleContext = primaryFrame->GetStyleContext();
      // if (aContent) {
      //   pseudoContext = presContext->StyleSet()->
      //     ProbePseudoStyleFor(content, nsAccessibilityAtoms::after, aStyleContext);
      mState.domNode = do_QueryInterface(mState.frame->GetContent());
      mState.siblingIndex = eSiblingsWalkFrames;
    }
#endif
  }
  else {
    mState.frame = mState.frame->GetNextSibling();
  }
}

/**
 * If the DOM node's frame has an accessible or the DOMNode
 * itself implements nsIAccessible return it.
 */
PRBool nsAccessibleTreeWalker::GetAccessible()
{
  if (!mAccService) {
    return PR_FALSE;
  }

  mState.accessible = nsnull;
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));

  mAccService->GetAccessible(mState.domNode, presShell, mWeakShell,
                             &mState.frame, &mState.isHidden,
                             getter_AddRefs(mState.accessible));
  return mState.accessible ? PR_TRUE : PR_FALSE;
}


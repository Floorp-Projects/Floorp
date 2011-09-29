/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=79: */
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "inDeepTreeWalker.h"
#include "inLayoutUtils.h"

#include "nsString.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeFilter.h"
#include "nsIDOMNodeList.h"
#include "nsServiceManagerUtils.h"
#include "inIDOMUtils.h"
#include "nsIContent.h"
#include "nsBindingManager.h"

/*****************************************************************************
 * This implementation does not currently operaate according to the W3C spec.
 * In particular it does NOT handle DOM mutations during the walk.  It also
 * ignores whatToShow and the filter.
 *****************************************************************************/

////////////////////////////////////////////////////

inDeepTreeWalker::inDeepTreeWalker() 
  : mShowAnonymousContent(PR_FALSE),
    mShowSubDocuments(PR_FALSE),
    mWhatToShow(nsIDOMNodeFilter::SHOW_ALL)
{
}

inDeepTreeWalker::~inDeepTreeWalker() 
{ 
}

NS_IMPL_ISUPPORTS2(inDeepTreeWalker,
                   inIDeepTreeWalker,
                   nsIDOMTreeWalker)

////////////////////////////////////////////////////
// inIDeepTreeWalker

NS_IMETHODIMP
inDeepTreeWalker::GetShowAnonymousContent(bool *aShowAnonymousContent)
{
  *aShowAnonymousContent = mShowAnonymousContent;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::SetShowAnonymousContent(bool aShowAnonymousContent)
{
  mShowAnonymousContent = aShowAnonymousContent;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::GetShowSubDocuments(bool *aShowSubDocuments)
{
  *aShowSubDocuments = mShowSubDocuments;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::SetShowSubDocuments(bool aShowSubDocuments)
{
  mShowSubDocuments = aShowSubDocuments;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::Init(nsIDOMNode* aRoot, PRUint32 aWhatToShow)
{
  mRoot = aRoot;
  mWhatToShow = aWhatToShow;
  
  PushNode(aRoot);

  return NS_OK;
}

////////////////////////////////////////////////////
// nsIDOMTreeWalker

NS_IMETHODIMP
inDeepTreeWalker::GetRoot(nsIDOMNode** aRoot)
{
  *aRoot = mRoot;
  NS_IF_ADDREF(*aRoot);
  
  return NS_OK;
}

NS_IMETHODIMP 
inDeepTreeWalker::GetWhatToShow(PRUint32* aWhatToShow)
{
  *aWhatToShow = mWhatToShow;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::GetFilter(nsIDOMNodeFilter** aFilter)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
inDeepTreeWalker::GetExpandEntityReferences(bool* aExpandEntityReferences)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
inDeepTreeWalker::GetCurrentNode(nsIDOMNode** aCurrentNode)
{
  *aCurrentNode = mCurrentNode;
  NS_IF_ADDREF(*aCurrentNode);
  
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::SetCurrentNode(nsIDOMNode* aCurrentNode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
inDeepTreeWalker::ParentNode(nsIDOMNode** _retval)
{
  *_retval = nsnull;
  if (!mCurrentNode) return NS_OK;

  if (mStack.Length() == 1) {
    // No parent
    return NS_OK;
  }

  // Pop off the current node, and push the new one
  mStack.RemoveElementAt(mStack.Length()-1);
  DeepTreeStackItem& top = mStack.ElementAt(mStack.Length() - 1);
  mCurrentNode = top.node;
  top.lastIndex = 0;
  NS_ADDREF(*_retval = mCurrentNode);
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::FirstChild(nsIDOMNode **_retval)
{
  *_retval = nsnull;
  if (!mCurrentNode) {
    return NS_OK;
  }

  DeepTreeStackItem& top = mStack.ElementAt(mStack.Length() - 1);
  nsCOMPtr<nsIDOMNode> kid;
  top.kids->Item(0, getter_AddRefs(kid));
  if (!kid) {
    return NS_OK;
  }
  top.lastIndex = 1;
  PushNode(kid);
  kid.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::LastChild(nsIDOMNode **_retval)
{
  *_retval = nsnull;
  if (!mCurrentNode) {
    return NS_OK;
  }

  DeepTreeStackItem& top = mStack.ElementAt(mStack.Length() - 1);
  nsCOMPtr<nsIDOMNode> kid;
  PRUint32 length;
  top.kids->GetLength(&length);
  top.kids->Item(length - 1, getter_AddRefs(kid));
  if (!kid) {
    return NS_OK;
  }
  top.lastIndex = length;
  PushNode(kid);
  kid.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::PreviousSibling(nsIDOMNode **_retval)
{
  *_retval = nsnull;
  if (!mCurrentNode) {
    return NS_OK;
  }

  NS_ASSERTION(mStack.Length() > 0, "Should have things in mStack");

  if (mStack.Length() == 1) {
    // No previous sibling
    return NS_OK;
  }

  DeepTreeStackItem& parent = mStack.ElementAt(mStack.Length()-2);
  nsCOMPtr<nsIDOMNode> previousSibling;
  parent.kids->Item(parent.lastIndex-2, getter_AddRefs(previousSibling));
  if (!previousSibling) {
    return NS_OK;
  }

  // Our mStack's topmost element is our current node. Since we're trying to
  // change that to the previous sibling, pop off the current node, and push
  // the new one.
  mStack.RemoveElementAt(mStack.Length() - 1);
  parent.lastIndex--;
  PushNode(previousSibling);
  previousSibling.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::NextSibling(nsIDOMNode **_retval)
{
  *_retval = nsnull;
  if (!mCurrentNode) {
    return NS_OK;
  }

  NS_ASSERTION(mStack.Length() > 0, "Should have things in mStack");

  if (mStack.Length() == 1) {
    // No next sibling
    return NS_OK;
  }

  DeepTreeStackItem& parent = mStack.ElementAt(mStack.Length()-2);
  nsCOMPtr<nsIDOMNode> nextSibling;
  parent.kids->Item(parent.lastIndex, getter_AddRefs(nextSibling));
  if (!nextSibling) {
    return NS_OK;
  }

  // Our mStack's topmost element is our current node. Since we're trying to
  // change that to the next sibling, pop off the current node, and push
  // the new one.
  mStack.RemoveElementAt(mStack.Length() - 1);
  parent.lastIndex++;
  PushNode(nextSibling);
  nextSibling.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::PreviousNode(nsIDOMNode **_retval)
{
  if (!mCurrentNode || mStack.Length() == 1) {
    // Nowhere to go from here
    *_retval = nsnull;
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNode> node;
  PreviousSibling(getter_AddRefs(node));

  if (!node) {
    return ParentNode(_retval);
  }

  // Now we're positioned at our previous sibling.  But since the DOM tree
  // traversal is depth-first, the previous node is its most deeply nested last
  // child.  Just loop until LastChild() returns null; since the LastChild()
  // call that returns null won't affect our position, we will then be
  // positioned at the correct node.
  while (node) {
    LastChild(getter_AddRefs(node));
  }

  NS_ADDREF(*_retval = mCurrentNode);
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::NextNode(nsIDOMNode **_retval)
{
  // First try our kids
  FirstChild(_retval);

  if (*_retval) {
    return NS_OK;
  }

  // Now keep trying next siblings up the parent chain, but if we
  // discover there's nothing else restore our state.
#ifdef DEBUG
  nsIDOMNode* origCurrentNode = mCurrentNode;
#endif
  PRUint32 lastChildCallsToMake = 0;
  while (1) {
    NextSibling(_retval);

    if (*_retval) {
      return NS_OK;
    }

    nsCOMPtr<nsIDOMNode> parent;
    ParentNode(getter_AddRefs(parent));
    if (!parent) {
      // Nowhere else to go; we're done.  Restore our state.
      while (lastChildCallsToMake--) {
        nsCOMPtr<nsIDOMNode> dummy;
        LastChild(getter_AddRefs(dummy));
      }
      NS_ASSERTION(mCurrentNode == origCurrentNode,
                   "Didn't go back to the right node?");
      *_retval = nsnull;
      return NS_OK;
    }
    ++lastChildCallsToMake;
  }

  NS_NOTREACHED("how did we get here?");
  return NS_OK;
}

void
inDeepTreeWalker::PushNode(nsIDOMNode* aNode)
{
  mCurrentNode = aNode;
  if (!aNode) return;

  DeepTreeStackItem item;
  item.node = aNode;

  nsCOMPtr<nsIDOMNodeList> kids;
  if (mShowSubDocuments) {
    nsCOMPtr<nsIDOMDocument> domdoc = inLayoutUtils::GetSubDocumentFor(aNode);
    if (domdoc) {
      domdoc->GetChildNodes(getter_AddRefs(kids));
    }
  }
  
  if (!kids) {
    if (mShowAnonymousContent) {
      nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
      nsRefPtr<nsBindingManager> bindingManager;
      if (content &&
          (bindingManager = inLayoutUtils::GetBindingManagerFor(aNode))) {
        bindingManager->GetAnonymousNodesFor(content, getter_AddRefs(kids));
        if (!kids)
          bindingManager->GetContentListFor(content, getter_AddRefs(kids));
      } else {
        aNode->GetChildNodes(getter_AddRefs(kids));
      }
    } else
      aNode->GetChildNodes(getter_AddRefs(kids));
  }
  
  item.kids = kids;
  item.lastIndex = 0;
  mStack.AppendElement(item);
}

/*
// This NextNode implementation does not require the use of stacks, 
// as does the one above. However, it does not handle anonymous 
// content and sub-documents.
NS_IMETHODIMP
inDeepTreeWalker::NextNode(nsIDOMNode **_retval)
{
  if (!mCurrentNode) return NS_OK;
  
  // walk down the tree first
  nsCOMPtr<nsIDOMNode> next;
  mCurrentNode->GetFirstChild(getter_AddRefs(next));
  if (!next) {
    mCurrentNode->GetNextSibling(getter_AddRefs(next));
    if (!next) { 
      // we've hit the end, so walk back up the tree until another
      // downward opening is found, or the top of the tree
      nsCOMPtr<nsIDOMNode> subject = mCurrentNode;
      nsCOMPtr<nsIDOMNode> parent;
      while (1) {
        subject->GetParentNode(getter_AddRefs(parent));
        if (!parent) // hit the top of the tree
          break;
        parent->GetNextSibling(getter_AddRefs(subject));
        if (subject) { // found a downward opening
          next = subject;
          break;
        } else // walk up another level
          subject = parent;
      } 
    }
  }
  
  mCurrentNode = next;
  
  *_retval = next;
  NS_IF_ADDREF(*_retval);
  
  return NS_OK;
}


char* getURL(nsIDOMDocument* aDoc)
{
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);
  nsIURI *uri = doc->GetDocumentURI();
  char* s;
  uri->GetSpec(&s);
  return s;
}
*/

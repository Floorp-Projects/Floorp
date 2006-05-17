/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "inDeepTreeWalker.h"
#include "inLayoutUtils.h"

#include "nsString.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeFilter.h"
#include "nsIDOMNodeList.h"
#include "nsIContent.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebNavigation.h"
#include "nsIScriptGlobalObject.h"

/*****************************************************************************
 * This implementation does not currently operaate according to the W3C spec.
 * So far, only parentNode() and nextNode() are implemented, and are not
 * interoperable.  
 *****************************************************************************/

////////////////////////////////////////////////////

inDeepTreeWalker::inDeepTreeWalker() 
{
  NS_INIT_REFCNT();

  mShowAnonymousContent = PR_FALSE;
  mShowSubDocuments = PR_FALSE;
  mWhatToShow = nsIDOMNodeFilter::SHOW_ALL;
}

inDeepTreeWalker::~inDeepTreeWalker() 
{ 
}

NS_IMPL_ISUPPORTS1(inDeepTreeWalker, inIDeepTreeWalker);

typedef struct _DeepTreeStackItem 
{
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIDOMNodeList> kids;
  PRUint32 lastIndex;
} DeepTreeStackItem;

////////////////////////////////////////////////////
// inIDeepTreeWalker

NS_IMETHODIMP
inDeepTreeWalker::GetShowAnonymousContent(PRBool *aShowAnonymousContent)
{
  *aShowAnonymousContent = mShowAnonymousContent;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::SetShowAnonymousContent(PRBool aShowAnonymousContent)
{
  mShowAnonymousContent = aShowAnonymousContent;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::GetShowSubDocuments(PRBool *aShowSubDocuments)
{
  *aShowSubDocuments = mShowSubDocuments;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::SetShowSubDocuments(PRBool aShowSubDocuments)
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
inDeepTreeWalker::GetExpandEntityReferences(PRBool* aExpandEntityReferences)
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
  if (!mCurrentNode) return NS_OK;

  if (mShowSubDocuments && inLayoutUtils::IsDocumentElement(mCurrentNode)) {
    nsCOMPtr<nsIDOMDocument> doc;
    mCurrentNode->GetOwnerDocument(getter_AddRefs(doc));
    nsCOMPtr<nsIDOMNode> node = inLayoutUtils::GetContainerFor(doc);
    if (node)
      *_retval = node;
  }

  if (mShowAnonymousContent && !*_retval) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(mCurrentNode);
    nsCOMPtr<nsIContent> bparent;
    nsCOMPtr<nsIBindingManager> bindingManager = inLayoutUtils::GetBindingManagerFor(mCurrentNode);
    if (bindingManager)
      bindingManager->GetInsertionParent(content, getter_AddRefs(bparent));
      
    if (bparent) {
      nsCOMPtr<nsIDOMNode> parent = do_QueryInterface(bparent);
      *_retval = parent;
    }
  }
  
  if (!*_retval) {
    nsCOMPtr<nsIDOMNode> node;
    mCurrentNode->GetParentNode(getter_AddRefs(node));
    *_retval = node;
  }

  mCurrentNode = *_retval;
  NS_IF_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::FirstChild(nsIDOMNode **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
inDeepTreeWalker::LastChild(nsIDOMNode **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
inDeepTreeWalker::PreviousSibling(nsIDOMNode **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
inDeepTreeWalker::NextSibling(nsIDOMNode **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
inDeepTreeWalker::PreviousNode(nsIDOMNode **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
inDeepTreeWalker::NextNode(nsIDOMNode **_retval)
{
  if (!mCurrentNode) return NS_OK;
  
  nsCOMPtr<nsIDOMNode> next;
  
  while (1) {
    DeepTreeStackItem* top = (DeepTreeStackItem*)mStack.ElementAt(mStack.Count()-1);
    nsCOMPtr<nsIDOMNodeList> kids = top->kids;
    PRUint32 childCount;
    kids->GetLength(&childCount);

    if (top->lastIndex == childCount) {
      mStack.RemoveElementAt(mStack.Count()-1);
      delete top;
      if (mStack.Count() == 0) {
        mCurrentNode = nsnull;
        break;
      }
    } else {
      kids->Item(top->lastIndex++, getter_AddRefs(next));
      PushNode(next);
      break;      
    }
  } 
  
  *_retval = next;
  NS_IF_ADDREF(*_retval);
  
  return NS_OK;
}

void
inDeepTreeWalker::PushNode(nsIDOMNode* aNode)
{
  mCurrentNode = aNode;
  if (!aNode) return;

  DeepTreeStackItem* item = new DeepTreeStackItem();
  item->node = aNode;

  nsCOMPtr<nsIDOMNodeList> kids;
  if (mShowSubDocuments) {
    nsCOMPtr<nsIDOMDocument> domdoc = inLayoutUtils::GetSubDocumentFor(aNode);
    if (domdoc) {
      domdoc->GetChildNodes(getter_AddRefs(kids));
    }
  }
  
  if (!kids) {
    if (mShowAnonymousContent) {
      nsCOMPtr<nsIBindingManager> bindingManager = inLayoutUtils::GetBindingManagerFor(aNode);
      nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
      if (bindingManager) {
        bindingManager->GetAnonymousNodesFor(content, getter_AddRefs(kids));
        if (!kids)
          bindingManager->GetContentListFor(content, getter_AddRefs(kids));
      } else {
        aNode->GetChildNodes(getter_AddRefs(kids));
      }
    } else
      aNode->GetChildNodes(getter_AddRefs(kids));
  }
  
  item->kids = kids;
  item->lastIndex = 0;
  mStack.AppendElement((void*)item);
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
  nsCOMPtr<nsIURI> uri;
  doc->GetDocumentURL(getter_AddRefs(uri));
  char* s;
  uri->GetSpec(&s);
  return s;
}
*/

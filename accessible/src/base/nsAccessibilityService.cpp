/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsIAccessibilityService.h"
#include "nsAccessibilityService.h"
#include "nsAccessible.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsRootAccessible.h"
#include "nsINameSpaceManager.h"
#include "nsLayoutAtoms.h"
#include "nsIDOMNode.h"
#include "nsHTMLTextAccessible.h"
#include "nsHTMLTableAccessible.h"
#include "nsHTMLImageAccessible.h"
#include "nsHTMLAreaAccessible.h"
#include "nsHTMLLinkAccessible.h"
#include "nsHTMLSelectListAccessible.h"
#include "nsHTMLComboboxAccessible.h"
#include "nsHTMLListboxAccessible.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsHTMLFormControlAccessible.h"
#include "nsIAccessibleProvider.h"
#include "nsILink.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMXULCheckboxElement.h"

// IFrame
#include "nsIDocShell.h"
#include "nsHTMLIFrameRootAccessible.h"

//--------------------


nsAccessibilityService::nsAccessibilityService()
{
  NS_INIT_REFCNT();
}

nsAccessibilityService::~nsAccessibilityService()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsAccessibilityService, nsIAccessibilityService);




////////////////////////////////////////////////////////////////////////////////
// nsIAccessibilityService methods:

NS_IMETHODIMP 
nsAccessibilityService::CreateRootAccessible(nsISupports* aPresContext, nsISupports* aFrame, nsIAccessible **_retval)
{
  // XXX - jgaunt - looks like we aren't using this
  //nsIFrame* f = NS_STATIC_CAST(nsIFrame*, aFrame);

  nsCOMPtr<nsIPresContext> presContext(do_QueryInterface(aPresContext));
  NS_ASSERTION(presContext,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIPresShell> presShell;
  presContext->GetShell(getter_AddRefs(presShell));

  NS_ASSERTION(presShell,"Error not presshell!!");

  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(presShell));

  *_retval = new nsRootAccessible(weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);

  return NS_OK;
}


NS_IMETHODIMP 
nsAccessibilityService::CreateHTMLComboboxAccessible(nsIDOMNode* aDOMNode, nsISupports* aPresContext, nsIAccessible **_retval)
{
  nsCOMPtr<nsIPresContext> presContext(do_QueryInterface(aPresContext));
  NS_ASSERTION(presContext,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIPresShell> presShell;
  presContext->GetShell(getter_AddRefs(presShell)); 

  nsCOMPtr<nsIWeakReference> weakShell = do_GetWeakReference(presShell);

  *_retval = new nsHTMLComboboxAccessible(aDOMNode, weakShell);
  if (! *_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateHTMLListboxAccessible(nsIDOMNode* aDOMNode, nsISupports* aPresContext, nsIAccessible **_retval)
{
  nsCOMPtr<nsIPresContext> presContext(do_QueryInterface(aPresContext));
  NS_ASSERTION(presContext,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIPresShell> presShell;
  presContext->GetShell(getter_AddRefs(presShell)); 

  nsCOMPtr<nsIWeakReference> weakShell = do_GetWeakReference(presShell);

  *_retval = new nsHTMLListboxAccessible(aDOMNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateHTMLSelectOptionAccessible(nsIDOMNode* aDOMNode, nsIAccessible *aAccParent, nsISupports* aPresContext, nsIAccessible **_retval)
{
  nsCOMPtr<nsIPresContext> presContext(do_QueryInterface(aPresContext));
  NS_ASSERTION(presContext,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIPresShell> presShell;
  presContext->GetShell(getter_AddRefs(presShell)); 

  nsCOMPtr<nsIWeakReference> weakShell = do_GetWeakReference(presShell);

  *_retval = new nsHTMLSelectOptionAccessible(aAccParent, aDOMNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}


/* nsIAccessible createHTMLCheckboxAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTMLCheckboxAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLCheckboxAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP nsAccessibilityService::CreateXULCheckboxAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsHTMLCheckboxAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIAccessible createHTMRadioButtonAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTMLRadioButtonAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLRadioButtonAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIAccessible createHTMLButtonAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTMLButtonAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLButtonAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIAccessible createHTML4ButtonAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTML4ButtonAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTML4ButtonAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIAccessible createHTMLTextAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTMLTextAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLTextAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}


/* nsIAccessible createHTMLTableAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTMLTableAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLTableAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIAccessible createHTMLTableCellAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTMLTableCellAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLTableCellAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIAccessible createHTMLImageAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTMLImageAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;
  nsIImageFrame* imageFrame = nsnull;
  
  // not using a nsCOMPtr frames don't support them.
  aFrame->QueryInterface(NS_GET_IID(nsIImageFrame), (void**)&imageFrame);

  if (!imageFrame)
    return NS_ERROR_FAILURE;

  *_retval = new nsHTMLImageAccessible(node, imageFrame, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIAccessible createHTMLAreaAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTMLAreaAccessible(nsIWeakReference *aShell, nsIDOMNode *aDOMNode, nsIAccessible *aAccParent, 
                                                               nsIAccessible **_retval)
{
  *_retval = new nsHTMLAreaAccessible(aDOMNode, aAccParent, aShell);

  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIAccessible createHTMLTextFieldAccessible (in nsISupports aPresShell, in nsISupports aFrame); */
NS_IMETHODIMP nsAccessibilityService::CreateHTMLTextFieldAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLTextFieldAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP nsAccessibilityService::GetShellFromNode(nsIDOMNode *aNode, nsIWeakReference **aWeakShell)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  aNode->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  if (!doc)
    return NS_ERROR_INVALID_ARG;

  // ---- Get the pres shell ----
  nsCOMPtr<nsIPresShell> shell;
  doc->GetShellAt(0, getter_AddRefs(shell));
  if (!shell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIWeakReference> weakRef(do_GetWeakReference(shell));
  
  *aWeakShell = weakRef;
  NS_IF_ADDREF(*aWeakShell);

  return NS_OK;
}

NS_IMETHODIMP nsAccessibilityService::GetInfo(nsISupports* aFrame, nsIFrame** aRealFrame, nsIWeakReference** aShell, nsIDOMNode** aNode)
{
  NS_ASSERTION(aFrame,"Error -- 1st argument (aFrame) is null!!");
  *aRealFrame = NS_STATIC_CAST(nsIFrame*, aFrame);
  nsCOMPtr<nsIContent> content;
  (*aRealFrame)->GetContent(getter_AddRefs(content));
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));
  *aNode = node;
  NS_IF_ADDREF(*aNode);

  nsCOMPtr<nsIDocument> document;
  content->GetDocument(*getter_AddRefs(document));
  if (!document)
    return NS_ERROR_FAILURE;

#ifdef DEBUG
  PRInt32 shells = document->GetNumberOfShells();
  NS_ASSERTION(shells > 0,"Error no shells!");
#endif

  // do_GetWR only works into a |nsCOMPtr| :-(
  nsCOMPtr<nsIPresShell> tempShell;
  nsCOMPtr<nsIWeakReference> weakShell;
  document->GetShellAt(0, getter_AddRefs(tempShell));
  weakShell = do_GetWeakReference(tempShell);
  NS_IF_ADDREF(*aShell = weakShell);

  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateAccessible(nsIDOMNode* aDOMNode, nsISupports* aDocument, nsIAccessible **_retval)
{
  nsCOMPtr<nsIDocument> document (do_QueryInterface(aDocument));
  if (!document)
    return NS_ERROR_FAILURE;

#ifdef DEBUG
  PRInt32 shells = document->GetNumberOfShells();
  NS_ASSERTION(shells > 0,"Error no shells!");
#endif

  nsCOMPtr<nsIPresShell> tempShell;
  document->GetShellAt(0, getter_AddRefs(tempShell));
  nsCOMPtr<nsIWeakReference> weakShell = do_GetWeakReference(tempShell);

  *_retval = new nsAccessible(aDOMNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateHTMLBlockAccessible(nsIDOMNode* aDOMNode, nsISupports* aDocument, nsIAccessible **_retval)
{
  nsCOMPtr<nsIDocument> document (do_QueryInterface(aDocument));
  if (!document)
    return NS_ERROR_FAILURE;

#ifdef DEBUG
  PRInt32 shells = document->GetNumberOfShells();
  NS_ASSERTION(shells > 0,"Error no shells!");
#endif

  nsCOMPtr<nsIPresShell> tempShell;
  document->GetShellAt(0, getter_AddRefs(tempShell));
  nsCOMPtr<nsIWeakReference> weakShell = do_GetWeakReference(tempShell);

  *_retval = new nsAccessible(aDOMNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateHTMLIFrameAccessible(nsIDOMNode* aDOMNode, nsISupports* aPresContext, nsIAccessible **_retval)
{
  *_retval = nsnull;

  nsCOMPtr<nsIContent> content(do_QueryInterface(aDOMNode));
  NS_ASSERTION(content,"Error non nsIContent passed to accessible factory!!!");

  nsCOMPtr<nsIPresContext> presContext(do_QueryInterface(aPresContext));
  NS_ASSERTION(presContext,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIPresShell> presShell;
  presContext->GetShell(getter_AddRefs(presShell)); 
  NS_ASSERTION(presShell,"Error non PresShell passed to accessible factory!!!");

  nsCOMPtr<nsIWeakReference> weakRef = do_GetWeakReference(presShell);

  nsCOMPtr<nsIDocument> doc;
  if (NS_SUCCEEDED(content->GetDocument(*getter_AddRefs(doc))) && doc) {
    nsCOMPtr<nsIPresShell> presShell;
    doc->GetShellAt(0, getter_AddRefs(presShell));
    if (presShell) {
      nsCOMPtr<nsISupports> supps;
      presShell->GetSubShellFor(content, getter_AddRefs(supps));
      if (supps) {
        nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(supps));
        if (docShell) {
          nsCOMPtr<nsIPresShell> ps;
          docShell->GetPresShell(getter_AddRefs(ps));
          if (ps) {
            nsCOMPtr<nsIWeakReference> wr = do_GetWeakReference(ps);
            nsCOMPtr<nsIDocument> innerDoc;
            ps->GetDocument(getter_AddRefs(innerDoc)); 
            if (innerDoc) {
              nsCOMPtr<nsIAccessible> root = new nsHTMLIFrameRootAccessible(aDOMNode, wr);
              if ( root ) {
                nsHTMLIFrameAccessible* frameAcc = new nsHTMLIFrameAccessible(aDOMNode, root, weakRef, innerDoc);
                if ( frameAcc != nsnull ) {
                  *_retval = NS_STATIC_CAST(nsIAccessible*, frameAcc);
                  if ( *_retval ) {
                    NS_ADDREF(*_retval);
                    return NS_OK;
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  return NS_ERROR_FAILURE;
}

//-----------------------------------------------------------------------
 // This method finds the content node in the parent document
 // corresponds to the docshell
 // This code is copied and pasted from nsEventStateManager.cpp
 // Is also inefficient - better solution should come along as part of 
 // Bug 85602: "FindContentForDocShell walks entire content tree"
 // Hopefully there will be a better method soon, with a public interface

 nsIContent* 
 nsAccessibilityService::FindContentForDocShell(nsIPresShell* aPresShell,
                                             nsIContent*   aContent,
                                             nsIDocShell*  aDocShell)
 {
   NS_ASSERTION(aPresShell, "Pointer is null!");
   NS_ASSERTION(aDocShell,  "Pointer is null!");
   NS_ASSERTION(aContent,   "Pointer is null!");
 
   nsCOMPtr<nsISupports> supps;
   aPresShell->GetSubShellFor(aContent, getter_AddRefs(supps));
   if (supps) {
     nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(supps));
     if (docShell.get() == aDocShell) 
       return aContent;
   }
 
   // walk children content
   PRInt32 count;
   aContent->ChildCount(count);
   for (PRInt32 i=0;i<count;i++) {
     nsCOMPtr<nsIContent> child;
     aContent->ChildAt(i, *getter_AddRefs(child));
     nsIContent* foundContent = FindContentForDocShell(aPresShell, child, aDocShell);
     if (foundContent != nsnull) {
       return foundContent;
     }
   }
   return nsnull;
 }

void nsAccessibilityService::GetOwnerFor(nsIPresShell *aPresShell, nsIPresShell **aOwnerShell, nsIContent **aOwnerContent)
{
  nsCOMPtr<nsIPresContext> presContext;
  aPresShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext) 
    return;
  nsCOMPtr<nsISupports> pcContainer;
  presContext->GetContainer(getter_AddRefs(pcContainer));
  if (!pcContainer) 
    return;
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(pcContainer));

  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(docShell));
  if (!treeItem) 
    return;

  // Get Parent Doc
  nsCOMPtr<nsIDocShellTreeItem> treeItemParent;
  treeItem->GetParent(getter_AddRefs(treeItemParent));
  if (!treeItemParent) 
    return;

  nsCOMPtr<nsIDocShell> parentDS(do_QueryInterface(treeItemParent));
  if (!parentDS) 
    return;

  nsCOMPtr<nsIPresShell> parentPresShell;
  parentDS->GetPresShell(getter_AddRefs(parentPresShell));
  if (!parentPresShell) 
    return;

  nsCOMPtr<nsIDocument> parentDoc;
  parentPresShell->GetDocument(getter_AddRefs(parentDoc));
  if (!parentDoc)
    return;

  nsCOMPtr<nsIContent> rootContent;
  parentDoc->GetRootContent(getter_AddRefs(rootContent));
  
  nsIContent *tempContent;
  tempContent = FindContentForDocShell(parentPresShell, rootContent, docShell);
  if (tempContent) {
    *aOwnerContent = tempContent;
    *aOwnerShell = parentPresShell;
    NS_ADDREF(*aOwnerShell);
    NS_ADDREF(*aOwnerContent);
  }
}

/* -------------------------------------------------------
 * GetAccessibleFor - get an nsIAccessible from a DOM node
 * ------------------------------------------------------- */

NS_IMETHODIMP nsAccessibilityService::GetAccessibleFor(nsIDOMNode *aNode, 
                                                       nsIAccessible **_retval) 
{
  *_retval = nsnull;

  if (!aNode)
    return NS_ERROR_NULL_POINTER;

  // ---- Get the document for this node  ----
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIDocument> nodeIsDoc(do_QueryInterface(aNode));
  if (nodeIsDoc)
    doc = nodeIsDoc;
  else {
    nsCOMPtr<nsIDOMDocument> domDoc;
    aNode->GetOwnerDocument(getter_AddRefs(domDoc));
    if (!domDoc)
      return NS_ERROR_INVALID_ARG;
    doc = do_QueryInterface(domDoc);
  }
  if (!doc)
    return NS_ERROR_INVALID_ARG;

  // ---- Get the pres shell ----
  nsCOMPtr<nsIPresShell> shell;
  doc->GetShellAt(0, getter_AddRefs(shell));
  if (!shell)
    return NS_ERROR_FAILURE;

  // ---- Check if area node ----
  nsCOMPtr<nsIDOMHTMLAreaElement> areaContent(do_QueryInterface(aNode));
  if (areaContent)   // Area elements are implemented in nsHTMLImageAccessible as children of the image
    return PR_FALSE; // Return, otherwise the image frame looks like an accessible object in the wrong place

  // ---- Check if we need outer owning doc ----
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  if (!content && nodeIsDoc) {
    // This happens when we're on the document node, which will not QI to an nsIContent, 
    // When that happens, we try to get the outer, parent document node that contains the document
    // For example, a <browser> or <iframe> element
    nsCOMPtr<nsIPresShell> ownerShell;
    nsCOMPtr<nsIContent> ownerContent;
    GetOwnerFor(shell, getter_AddRefs(ownerShell), getter_AddRefs(ownerContent));
    shell = ownerShell;
    content = ownerContent;
  }

  // ---- If still no nsIContent, return ----
  if (!content)
    return PR_FALSE;

  // ---- Try using frame to get IAccessible ----
  nsIFrame* frame = nsnull;
  shell->GetPrimaryFrameFor(content, &frame);
  if (!frame)
    return PR_FALSE;

  nsCOMPtr<nsIAccessible> newAcc;
  frame->GetAccessible(getter_AddRefs(newAcc));

  // ---- Is it a XUL element? -- they impl nsIAccessibleProvider via XBL
  if (!newAcc) {
    nsCOMPtr<nsIAccessibleProvider> accProv(do_QueryInterface(aNode));
    if (accProv)  
      accProv->GetAccessible(getter_AddRefs(newAcc));
  }

  // ---- If link, create link accessible ----
  if (!newAcc) {
    // is it a link?
    nsCOMPtr<nsILink> link(do_QueryInterface(aNode));
    if (link) {
      nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(shell));
      newAcc = new nsHTMLLinkAccessible(aNode, weakShell);
    }
  }

  // ---- If <select> option, create select option accessible
  if (!newAcc) {
    nsCOMPtr<nsIDOMHTMLOptionElement> optionElement(do_QueryInterface(aNode));
    if (optionElement) {
      // nsHTMLSelectionOptionAccessible's must be created via the parent
      nsCOMPtr<nsIDOMNode> parentNode;
      aNode->GetParentNode(getter_AddRefs(parentNode));
      if (parentNode) {
        nsCOMPtr<nsIAccessible> parentAccessible;
        GetAccessibleFor(parentNode, getter_AddRefs(parentAccessible));
        if (parentAccessible) {
          nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(shell));
          newAcc = new nsHTMLSelectOptionAccessible(parentAccessible, aNode, weakShell);
        }
      }
    }
  }

  if (!newAcc)
    return NS_ERROR_FAILURE;

  *_retval = newAcc;
  NS_ADDREF(*_retval);

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

nsresult
NS_NewAccessibilityService(nsIAccessibilityService** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
      return NS_ERROR_NULL_POINTER;

  nsAccessibilityService* accService = new nsAccessibilityService();
  if (!accService)
      return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(accService);
  *aResult = accService;
  return NS_OK;
}


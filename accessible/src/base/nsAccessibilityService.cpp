/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 * Contributors:    John Gaunt (jgaunt@netscape.com)
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

// NOTE: alphabetically ordered
#include "nsAccessibilityService.h"
#include "nsAccessible.h"
#include "nsCOMPtr.h"
#include "nsHTMLAreaAccessible.h"
#include "nsHTMLFormControlAccessible.h"
#include "nsHTMLImageAccessible.h"
#include "nsHTMLLinkAccessible.h"
//#include "nsHTMLObjectAccessible.h" -- this comes with a later checkin DOH! - jgaunt
#include "nsHTMLSelectAccessible.h"
#include "nsHTMLTableAccessible.h"
#include "nsHTMLTextAccessible.h"
#include "nsIAccessibilityService.h"
#include "nsIAccessibleProvider.h"
#include "nsIContent.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLLegendElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMXULCheckboxElement.h"
#include "nsIFrame.h"
#include "nsILink.h"
#include "nsINameSpaceManager.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsITextContent.h"
#include "nsLayoutAtoms.h"
#include "nsRootAccessible.h"
#include "nsString.h"
#include "nsTextFragment.h"
#include "nsXULColorPickerAccessible.h"
#include "nsXULFormControlAccessible.h"
#include "nsXULMenuAccessible.h"
#include "nsXULSelectAccessible.h"
#include "nsXULTabAccessible.h"
#include "nsXULTextAccessible.h"
#include "nsIAccessible.h"

// IFrame
#include "nsIDocShell.h"
#include "nsHTMLIFrameRootAccessible.h"

/**
  * nsAccessibility Service
  */

nsAccessibilityService::nsAccessibilityService()
{
  NS_INIT_REFCNT();
}

nsAccessibilityService::~nsAccessibilityService()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsAccessibilityService, nsIAccessibilityService);

nsresult
nsAccessibilityService::GetInfo(nsISupports* aFrame, nsIFrame** aRealFrame, nsIWeakReference** aShell, nsIDOMNode** aNode)
{
  NS_ASSERTION(aFrame,"Error -- 1st argument (aFrame) is null!!");
  *aRealFrame = NS_STATIC_CAST(nsIFrame*, aFrame);
  nsCOMPtr<nsIContent> content;
  (*aRealFrame)->GetContent(getter_AddRefs(content));
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));
  if (!content || !node)
    return NS_ERROR_FAILURE;
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
  nsCOMPtr<nsISupports> docShellSupports(do_QueryInterface(pcContainer));

  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(pcContainer));
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

  nsCOMPtr<nsIContent> tempContent;
  parentPresShell->FindContentForShell(docShellSupports, getter_AddRefs(tempContent));

  if (tempContent) {
    *aOwnerContent = tempContent;
    *aOwnerShell = parentPresShell;
    NS_ADDREF(*aOwnerShell);
    NS_ADDREF(*aOwnerContent);
  }
}

nsresult
nsAccessibilityService::GetShellFromNode(nsIDOMNode *aNode, nsIWeakReference **aWeakShell)
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

/**
  * nsIAccessibilityService methods:
  */

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
nsAccessibilityService::CreateIFrameAccessible(nsIDOMNode* aDOMNode, nsIAccessible **_retval)
{
  NS_ENSURE_ARG_POINTER(aDOMNode);
  
  *_retval = nsnull;

  nsCOMPtr<nsIContent> content(do_QueryInterface(aDOMNode));
  if (!content)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIWeakReference> outerWeakShell;
  GetShellFromNode(aDOMNode, getter_AddRefs(outerWeakShell));

  nsCOMPtr<nsIPresShell> outerPresShell(do_QueryReferent(outerWeakShell));
  if (!outerPresShell) {
    NS_WARNING("No outer pres shell in CreateHTMLIFrameAccessible!");
    return NS_ERROR_FAILURE;
  }
  
  nsCOMPtr<nsIPresContext> outerPresContext;
  outerPresShell->GetPresContext(getter_AddRefs(outerPresContext));
  if (!outerPresContext) {
    NS_WARNING("No outer pres context in CreateHTMLIFrameAccessible!");
    return NS_ERROR_FAILURE;
  }
  
  nsCOMPtr<nsIDocument> doc;
  if (NS_SUCCEEDED(content->GetDocument(*getter_AddRefs(doc))) && doc) {
    if (outerPresShell) {
      nsCOMPtr<nsISupports> supps;
      outerPresShell->GetSubShellFor(content, getter_AddRefs(supps));
      if (supps) {
        nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(supps));
        if (docShell) {
          nsCOMPtr<nsIPresShell> innerPresShell;
          docShell->GetPresShell(getter_AddRefs(innerPresShell));
          if (innerPresShell) {
            nsCOMPtr<nsIWeakReference> innerWeakShell(do_GetWeakReference(innerPresShell));
            nsCOMPtr<nsIDocument> innerDoc;
            innerPresShell->GetDocument(getter_AddRefs(innerDoc)); 
            if (innerDoc) {
              nsCOMPtr<nsIAccessible> innerRootAccessible(new nsHTMLIFrameRootAccessible(aDOMNode, innerWeakShell));
              if (innerRootAccessible) {
                nsHTMLIFrameAccessible* outerRootAccessible = new nsHTMLIFrameAccessible(aDOMNode, innerRootAccessible, outerWeakShell, innerDoc);
                if (outerRootAccessible) {
                  *_retval = NS_STATIC_CAST(nsIAccessible*, outerRootAccessible);
                  if (*_retval) {
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

NS_IMETHODIMP 
nsAccessibilityService::CreateRootAccessible(nsISupports* aPresContext, nsISupports* aFrame, nsIAccessible **_retval)
{
  *_retval = nsnull;

  nsCOMPtr<nsIPresContext> presContext(do_QueryInterface(aPresContext));
  NS_ASSERTION(presContext,"Error non prescontext passed to accessible factory!!!");

  nsCOMPtr<nsIPresShell> presShell;
  presContext->GetShell(getter_AddRefs(presShell));
  NS_ASSERTION(presShell,"Error no presshell!!");

  nsCOMPtr<nsISupports> pcContainer;
  presContext->GetContainer(getter_AddRefs(pcContainer));
  NS_ASSERTION(pcContainer,"Error no container for pres context!!!");

  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(pcContainer));
  NS_ASSERTION(treeItem,"Error no tree item for pres context container!!!");

  nsCOMPtr<nsIDocShellTreeItem> treeItemParent;
  treeItem->GetParent(getter_AddRefs(treeItemParent));
  if (treeItemParent) {
    // We only create root accessibles for the true root, othewise create an iframe/iframeroot accessible
    nsCOMPtr<nsIDocument> document;
    nsCOMPtr<nsIContent> rootContent;
    presShell->GetDocument(getter_AddRefs(document));
    nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(document));
    if (rootNode)
      GetAccessibleFor(rootNode, _retval);
  }
  else {
    nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(presShell));
    *_retval = new nsRootAccessible(weakShell);
    NS_IF_ADDREF(*_retval);
  }

  if (! *_retval) 
    return NS_ERROR_FAILURE;

  return NS_OK;
}

 /**
   * HTML widget creation
   */
NS_IMETHODIMP
nsAccessibilityService::CreateHTML4ButtonAccessible(nsISupports *aFrame, nsIAccessible **_retval)
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

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLAreaAccessible(nsIWeakReference *aShell, nsIDOMNode *aDOMNode, nsIAccessible *aAccParent, 
                                                               nsIAccessible **_retval)
{
  *_retval = new nsHTMLAreaAccessible(aDOMNode, aAccParent, aShell);

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
nsAccessibilityService::CreateHTMLButtonAccessible(nsISupports *aFrame, nsIAccessible **_retval)
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

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLCheckboxAccessible(nsISupports *aFrame, nsIAccessible **_retval)
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
nsAccessibilityService::CreateHTMLImageAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLImageAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLGroupboxAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLGroupboxAccessible(node, weakShell);
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

/* -- coming in a later patch
NS_IMETHODIMP
nsAccessibilityService::CreateHTMLObjectAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLObjectAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}
*/

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLRadioButtonAccessible(nsISupports *aFrame, nsIAccessible **_retval)
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

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLTableAccessible(nsISupports *aFrame, nsIAccessible **_retval)
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

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLTableCellAccessible(nsISupports *aFrame, nsIAccessible **_retval)
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

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLTextAccessible(nsISupports *aFrame, nsIAccessible **_retval)
{
  nsIFrame* frame;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, &frame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = nsnull;

  nsCOMPtr<nsITextContent> textContent(do_QueryInterface(node));
  if (textContent) {
    // If empty text string, don't include in accessible tree
    // Items with length 0 are already gone, we just need to check for single NBSP's
    PRInt32 textLength = 0;
    textContent->GetTextLength(&textLength);
    if (textLength == 1) {
      const PRUnichar NBSP = 160;
      const nsTextFragment *textFrag;
      nsresult rv = textContent->GetText(&textFrag);
      PRUnichar theChar = textFrag->CharAt(0);
      if (theChar == NBSP)
        return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIDOMNode> parentNode;
    node->GetParentNode(getter_AddRefs(parentNode));
    nsCOMPtr<nsIDOMHTMLLegendElement> legend(do_QueryInterface(parentNode));
    if (legend)  // Expose <legend> as the name in a groupbox, not as a ROLE_TEXT accessible
      return NS_ERROR_FAILURE; 
  }
    
  *_retval = new nsHTMLTextAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateHTMLTextFieldAccessible(nsISupports *aFrame, nsIAccessible **_retval)
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

 /**
   * XUL widget creation
   *  we can't ifdef this whole block because there is no way to exclude
   *  these methods from the idl definition conditionally (MOZ_XUL).
   *  XXXjgaunt what needs to happen is all of the CreateFooAcc() methods get re-written
   *  into a single method.
   */

NS_IMETHODIMP nsAccessibilityService::CreateXULButtonAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  // reusing the HTML accessible widget and enhancing for XUL
  *_retval = new nsXULButtonAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULCheckboxAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  // reusing the HTML accessible widget and enhancing for XUL
  *_retval = new nsXULCheckboxAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULColorPickerAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULColorPickerAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULColorPickerTileAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULColorPickerTileAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateXULComboboxAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULComboboxAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateXULDropmarkerAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULDropmarkerAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULGroupboxAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULGroupboxAccessible(aNode, weakShell);
  if (! *_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULImageAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  // Don't include nameless images in accessible tree
  *_retval = nsnull;

  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(aNode));
  if (!elt)
    return NS_ERROR_FAILURE;
  PRBool hasTextEquivalent;
  elt->HasAttribute(NS_LITERAL_STRING("tooltiptext"), &hasTextEquivalent); // Prefer value over tooltiptext
  if (!hasTextEquivalent) {
    return NS_OK;
  }

  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsHTMLImageAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);

#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateXULListboxAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULListboxAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateXULListitemAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULListitemAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULMenubarAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULMenubarAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULMenuitemAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULMenuitemAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULMenupopupAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULMenupopupAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULMenuSeparatorAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULMenuSeparatorAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULProgressMeterAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULProgressMeterAccessible(aNode, weakShell);
  if (! *_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateXULRadioButtonAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULRadioButtonAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateXULRadioGroupAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULRadioGroupAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateXULSelectListAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULSelectListAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateXULSelectOptionAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULSelectOptionAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP 
nsAccessibilityService::CreateXULStatusBarAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULStatusBarAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateXULTextAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  // reusing the HTML accessible widget and enhancing for XUL
  *_retval = new nsXULTextAccessible(aNode, weakShell);
  if (! *_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

/** The single tab in a dialog or tabbrowser/editor interface */
NS_IMETHODIMP nsAccessibilityService::CreateXULTabAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULTabAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

/** A combination of a tabs object and a tabpanels object */
NS_IMETHODIMP nsAccessibilityService::CreateXULTabBoxAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULTabBoxAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

/** The display area for a dialog or tabbrowser interface */
NS_IMETHODIMP nsAccessibilityService::CreateXULTabPanelsAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULTabPanelsAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}

/** The collection of tab objects, useable in the TabBox and independant of as well */
NS_IMETHODIMP nsAccessibilityService::CreateXULTabsAccessible(nsIDOMNode *aNode, nsIAccessible **_retval)
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIWeakReference> weakShell;
  GetShellFromNode(aNode, getter_AddRefs(weakShell));

  *_retval = new nsXULTabsAccessible(aNode, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
#else
  *_retval = nsnull;
#endif // MOZ_XUL
  return NS_OK;
}


/**
  * GetAccessibleFor - get an nsIAccessible from a DOM node
  */
NS_IMETHODIMP nsAccessibilityService::GetAccessibleFor(nsIDOMNode *aNode, 
                                                       nsIAccessible **_retval) 
{
  *_retval = nsnull;

  NS_ASSERTION(aNode, "GetAccessibleFor() called with no node.");

  nsCOMPtr<nsIAccessible> newAcc;

#ifdef MOZ_XUL
  nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(aNode)); 
  if (xulElement) {
#ifdef DEBUG_aaronl
    // Please leave this in for now, it's a convenient debugging method
    nsAutoString name;
    aNode->GetLocalName(name);
    if (name.Equals(NS_LITERAL_STRING("dropmarker"))) 
      printf("## aaronl debugging tag name\n");

    nsAutoString className;
    xulElement->GetAttribute(NS_LITERAL_STRING("class"), className);
    if (className.Equals(NS_LITERAL_STRING("toolbarbutton-menubutton-dropmarker")))
      printf("## aaronl debugging attribute\n");
#endif
    nsCOMPtr<nsIAccessibleProvider> accProv(do_QueryInterface(aNode));
    if (accProv) {
      accProv->GetAccessible(_retval); 
      if (*_retval)
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }
#endif // MOZ_XUL

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
    return NS_ERROR_FAILURE; // Return, otherwise the image frame looks like an accessible object in the wrong place

  // ---- Check if we need outer owning doc ----
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  if (!content && nodeIsDoc) {
    // This happens when we're on the document node, which will not QI to an nsIContent, 
    // When that happens, we try to get the outer, parent document node that contains the document
    // For example, a <browser> or <iframe> element
    nsCOMPtr<nsIPresShell> ownerShell;
    nsCOMPtr<nsIContent> ownerContent;
    GetOwnerFor(shell, getter_AddRefs(ownerShell), getter_AddRefs(ownerContent));
    if (ownerContent) {
      shell = ownerShell;
      content = ownerContent;
    }
    else {
      // Nothing above us, return the root accessible
      doc->GetRootContent(getter_AddRefs(content));
      nsIFrame* frame = nsnull;
      shell->GetPrimaryFrameFor(content, &frame);
      if (!frame)
        return NS_ERROR_FAILURE;
      nsCOMPtr<nsIPresContext> presContext;
      shell->GetPresContext(getter_AddRefs(presContext));
      return CreateRootAccessible(presContext, frame, _retval);
    }
  }

  NS_ASSERTION(content, "GetAccessibleFor() called with no content.");

  // ---- Try using frame to get nsIAccessible ----
  nsIFrame* frame = nsnull;
  shell->GetPrimaryFrameFor(content, &frame);
  if (!frame)
    return NS_ERROR_FAILURE;

  frame->GetAccessible(getter_AddRefs(newAcc));

  // ---- If link, create link accessible ----
  if (!newAcc) {
    // is it a link?
    nsCOMPtr<nsILink> link(do_QueryInterface(aNode));
    if (link) {
      nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(shell));
      newAcc = new nsHTMLLinkAccessible(aNode, weakShell);
    }
  }

  // ---- If <select> <option>, create select option accessible
    
  if (!newAcc) {
    nsCOMPtr<nsIDOMHTMLOptionElement> optionElement(do_QueryInterface(aNode));
    if (optionElement) {
      nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(shell));
      newAcc = new nsHTMLSelectOptionAccessible(nsnull, aNode, weakShell);
    }
  }
  // See if this is an <optgroup>, 
  // create the accessible for the optgroup.
  if (!newAcc) {
    nsCOMPtr<nsIDOMHTMLOptGroupElement> optGroupElement(do_QueryInterface(aNode));
    if (optGroupElement) {
      nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(shell));
      newAcc = new nsHTMLSelectOptGroupAccessible(nsnull, aNode, weakShell);
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



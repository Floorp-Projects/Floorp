/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

// NOTE: alphabetically ordered
#include "nsAccessibilityService.h"
#include "nsCaretAccessible.h"
#include "nsIAccessibleEvent.h"
#include "nsICaret.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsRootAccessible.h"
#include "nsISelectionController.h"
#include "nsISelectionPrivate.h"
#include "nsServiceManagerUtils.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"

NS_IMPL_ISUPPORTS_INHERITED2(nsCaretAccessible, nsLeafAccessible, nsIAccessibleCaret, nsISelectionListener)

nsCaretAccessible::nsCaretAccessible(nsIDOMNode* aDocumentNode, nsIWeakReference* aShell, nsRootAccessible *aRootAccessible):
nsLeafAccessible(aDocumentNode, aShell), mVisible(PR_TRUE), mCurrentDOMNode(nsnull), mRootAccessible(aRootAccessible)
{
}

NS_IMETHODIMP nsCaretAccessible::Shutdown()
{
  mDomSelectionWeak = nsnull;
  mCurrentDOMNode = nsnull;
  RemoveSelectionListener();
  return NS_OK;
}

NS_IMETHODIMP nsCaretAccessible::RemoveSelectionListener()
{
  nsCOMPtr<nsISelection> prevDomSel(do_QueryReferent(mDomSelectionWeak));
  nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(prevDomSel));
  if (selPrivate) {
    mDomSelectionWeak = nsnull;
    return selPrivate->RemoveSelectionListener(this);
  }
  return NS_OK;
}

NS_IMETHODIMP nsCaretAccessible::AttachNewSelectionListener(nsIDOMNode *aCurrentNode)
{
  mCurrentDOMNode = aCurrentNode;

  // When focus moves such that the caret is part of a new frame selection
  // this removes the old selection listener and attaches a new one for the current focus
  nsCOMPtr<nsIPresShell> presShell = 
    nsRootAccessible::GetPresShellFor(aCurrentNode);
  if (!presShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc = presShell->GetDocument();
  if (!doc)  // we also should try to QI to document instead (necessary to do when node is a document)
    doc = do_QueryInterface(aCurrentNode);
  nsCOMPtr<nsIContent> content(do_QueryInterface(aCurrentNode));
  if (!content)
    content = doc->GetRootContent();  // If node is not content, use root content

  nsIFrame *frame = presShell->GetPrimaryFrameFor(content);
  nsPresContext *presContext = presShell->GetPresContext();
  if (!frame || !presContext)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISelectionController> selCon;
  frame->GetSelectionController(presContext, getter_AddRefs(selCon));
  if (!selCon)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsISelection> domSel, prevDomSel(do_QueryReferent(mDomSelectionWeak));
  selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel));
  if (domSel == prevDomSel)
    return NS_OK; // This is already the selection we're listening to
  RemoveSelectionListener();
  nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(domSel));

  if (!selPrivate)
    return NS_ERROR_FAILURE;

  mDomSelectionWeak = do_GetWeakReference(domSel);
  return selPrivate->AddSelectionListener(this);
}

NS_IMETHODIMP nsCaretAccessible::NotifySelectionChanged(nsIDOMDocument *aDoc, nsISelection *aSel, PRInt16 aReason)
{
  nsCOMPtr<nsIPresShell> presShell = GetPresShellFor(mCurrentDOMNode);
  nsCOMPtr<nsISelection> domSel(do_QueryReferent(mDomSelectionWeak));
  if (!presShell || domSel != aSel)
    return NS_OK;  // Only listening to selection changes in currently focused frame

  nsCOMPtr<nsICaret> caret;
  presShell->GetCaret(getter_AddRefs(caret));
  if (!caret)
    return NS_OK;

  nsRect caretRect;
  PRBool isCollapsed;
  caret->GetCaretCoordinates(nsICaret::eTopLevelWindowCoordinates, domSel,
                             &caretRect, &isCollapsed, nsnull);
  PRBool visible = !caretRect.IsEmpty();
  if (visible)  // Make sure it's visible both by looking at coordinates and visible flag
    caret->GetCaretVisible(&visible);
  if (visible != mVisible) {
    mVisible = visible;
#ifdef XP_WIN
    mRootAccessible->FireToolkitEvent(mVisible? nsIAccessibleEvent::EVENT_SHOW: 
                                      nsIAccessibleEvent::EVENT_HIDE, this, nsnull);
#endif
  }

#ifdef XP_WIN
  // Support old style MSAA caret move events, which utilize screen coodinates
  // rather than position within the text
  nsPresContext *presContext = presShell->GetPresContext();
  nsIViewManager* viewManager = presShell->GetViewManager();
  if (!presContext || !viewManager)
    return NS_OK;
  nsIView *view = nsnull;
  viewManager->GetRootView(view);
  if (!view)
    return NS_OK;
  nsIWidget* widget = view->GetWidget();
  if (!widget)
    return NS_OK;

  float t2p = presContext->TwipsToPixels();
  
  // Convert to pixels using that scale
  caretRect.x      = NSTwipsToIntPixels(caretRect.x, t2p);
  caretRect.y      = NSTwipsToIntPixels(caretRect.y, t2p);

  caretRect.width  = NSTwipsToIntPixels(caretRect.width, t2p);
  caretRect.height = NSTwipsToIntPixels(caretRect.height, t2p);

  widget->WidgetToScreen(caretRect, mCaretRect);

  mRootAccessible->FireToolkitEvent(nsIAccessibleEvent::EVENT_LOCATION_CHANGE, this, nsnull);
#endif

  // Get first nnsIAccessibleText in parent chain and fire caret-move, selection-change event for it
  nsCOMPtr<nsIAccessible> accessible;
  nsIAccessibilityService *accService = GetAccService();
  NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);
  // Get accessible from selection's focus node or its parent
  nsCOMPtr<nsIDOMNode> focusNode;
  domSel->GetFocusNode(getter_AddRefs(focusNode));
  if (!focusNode) {
    return NS_OK; // No selection
  }
  nsCOMPtr<nsIAccessibleText> textAcc;
  while (focusNode) {
    // Make sure to get the correct starting node for selection events inside XBL content trees
    nsCOMPtr<nsIDOMNode> relevantNode;
    if (NS_SUCCEEDED(accService->GetRelevantContentNodeFor(focusNode, getter_AddRefs(relevantNode))) && relevantNode) {
      focusNode  = relevantNode;
    }

    nsCOMPtr<nsIContent> content = do_QueryInterface(focusNode);
    if (!content || !content->IsNodeOfType(nsINode::eTEXT)) {
      accService->GetAccessibleInShell(focusNode, presShell,  getter_AddRefs(accessible));
      textAcc = do_QueryInterface(accessible);
      if (textAcc) {
        break;
      }
    }
    nsCOMPtr<nsIDOMNode> parentNode;
    focusNode->GetParentNode(getter_AddRefs(parentNode));
    focusNode.swap(parentNode);
  }
  NS_ASSERTION(textAcc, "No nsIAccessibleText for caret move event!"); // No nsIAccessibleText for caret move event!
  NS_ENSURE_TRUE(textAcc, NS_ERROR_FAILURE);

  return mRootAccessible->FireDelayedToolkitEvent(nsIAccessibleEvent::EVENT_ATK_TEXT_CARET_MOVE, focusNode, nsnull, PR_FALSE);
}

/** Return the caret's bounds */
NS_IMETHODIMP nsCaretAccessible::GetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  if (mCaretRect.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }
  *x = mCaretRect.x;
  *y = mCaretRect.y;
  *width = mCaretRect.width;
  *height = mCaretRect.height;
  return NS_OK;
}

NS_IMETHODIMP nsCaretAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_CARET;
  return NS_OK;
}

NS_IMETHODIMP nsCaretAccessible::GetState(PRUint32 *_retval)
{
  *_retval = mVisible? 0: STATE_INVISIBLE;
  return NS_OK;
}

NS_IMETHODIMP nsCaretAccessible::GetParent(nsIAccessible **aParent)
{   
  NS_ADDREF(*aParent = mRootAccessible);
  return NS_OK;
}
NS_IMETHODIMP nsCaretAccessible::GetPreviousSibling(nsIAccessible **_retval)
{ 
  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsCaretAccessible::GetNextSibling(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}


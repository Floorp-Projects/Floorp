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
#include "nsISelectionController.h"
#include "nsISelectionPrivate.h"
#include "nsIServiceManagerUtils.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsRootAccessible.h"
#include "nsTextAccessible.h"

#ifdef MOZ_ACCESSIBILITY_ATK
#include "nsAccessibleText.h"
#endif

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
  nsCOMPtr<nsIPresShell> presShell;
  nsRootAccessible::GetEventShell(aCurrentNode, getter_AddRefs(presShell));
  if (!presShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc = presShell->GetDocument();
  if (!doc)  // we also should try to QI to document instead (necessary to do when node is a document)
    doc = do_QueryInterface(aCurrentNode);
  nsCOMPtr<nsIContent> content(do_QueryInterface(aCurrentNode));
  if (!content)
    content = doc->GetRootContent();  // If node is not content, use root content

  nsIFrame *frame = nsnull;
  presShell->GetPrimaryFrameFor(content, &frame);
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

NS_IMETHODIMP nsCaretAccessible::NotifySelectionChanged(nsIDOMDocument *aDoc, nsISelection *aSel, short aReason)
{
#ifdef MOZ_ACCESSIBILITY_ATK
  if (nsAccessibleText::gSuppressedNotifySelectionChanged)
    return NS_OK;
#endif    

  nsCOMPtr<nsIPresShell> presShell;
  nsRootAccessible::GetEventShell(mCurrentDOMNode, getter_AddRefs(presShell));
  nsCOMPtr<nsISelection> domSel(do_QueryReferent(mDomSelectionWeak));
  if (!presShell || domSel != aSel)
    return NS_OK;  // Only listening to selection changes in currently focused frame

  nsCOMPtr<nsICaret> caret;
  presShell->GetCaret(getter_AddRefs(caret));
  if (!caret)
    return NS_OK;

  nsRect caretRect;
  PRBool isCollapsed;
  caret->GetCaretCoordinates(nsICaret::eTopLevelWindowCoordinates, domSel, &caretRect, &isCollapsed, nsnull);
#ifndef MOZ_ACCESSIBILITY_ATK
  PRBool visible = (caretRect.x >= 0 && caretRect.y >= 0 && caretRect.width >= 0 && caretRect.height >= 0);
  if (visible)  // Make sure it's visible both by looking at coordinates and visible flag
    caret->GetCaretVisible(&visible);
  if (visible != mVisible) {
    mVisible = visible;
    mRootAccessible->FireToolkitEvent(mVisible? nsIAccessibleEvent::EVENT_SHOW: 
                                      nsIAccessibleEvent::EVENT_HIDE, this, nsnull);
  }

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

  float t2p;
  t2p = presContext->TwipsToPixels();
    // Convert to pixels using that scale
  caretRect.x      = NSTwipsToIntPixels(caretRect.x, t2p);
  caretRect.y      = NSTwipsToIntPixels(caretRect.y, t2p);

  caretRect.width  = NSTwipsToIntPixels(caretRect.width, t2p);
  caretRect.height = NSTwipsToIntPixels(caretRect.height, t2p);

  nsRect caretScreenRect;
  widget->WidgetToScreen(caretRect, mCaretRect);
#endif

#ifndef MOZ_ACCESSIBILITY_ATK
  if (visible) {
    mRootAccessible->FireToolkitEvent(nsIAccessibleEvent::EVENT_LOCATION_CHANGE, this, nsnull);
  }
#else
  nsCOMPtr<nsIAccessible> accessible;
  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
  accService->GetAccessibleInShell(mCurrentDOMNode, presShell, getter_AddRefs(accessible));
  nsCOMPtr<nsIAccessibleDocument> docAcc(do_QueryInterface(accessible));
  if (docAcc) {
    PRBool isEditable;
    docAcc->GetIsEditable(&isEditable);
    if (!isEditable) { // this is not a composer window, find out the text accessible object
      nsCOMPtr<nsIDOMNode> focusNode;
      domSel->GetFocusNode(getter_AddRefs(focusNode));
      if (!focusNode) {
        return NS_OK;
      }
      nsCOMPtr<nsIDOMHTMLAnchorElement> anchorElement(do_QueryInterface(focusNode));
      if (anchorElement) {
        // do not report caret-move event for link
        return NS_OK;
      }
      nsCOMPtr<nsIDOMNode> blockNode;
      if (NS_FAILED(nsAccessible::GetParentBlockNode(presShell, focusNode, getter_AddRefs(blockNode)))) {
        return NS_OK;
      }
      accService->GetAccessibleInShell(blockNode, presShell, getter_AddRefs(accessible));
      if (!accessible) {
        return NS_OK;
      }
    }
  }

  if (!accessible) {
    return NS_OK;
  }

  if (isCollapsed) {
    nsCOMPtr<nsIAccessibleText> textAcc(do_QueryInterface(accessible));
    if (textAcc) {
      PRInt32 caretOffset;
      textAcc->GetCaretOffset(&caretOffset);
      mRootAccessible->FireToolkitEvent(nsIAccessibleEvent::EVENT_ATK_TEXT_CARET_MOVE, accessible, &caretOffset);
    }
  }
  else {
    //Current text interface doesn't support this event yet
    //mListener->FireToolkitEvent(nsIAccessibleEventReceiver::EVENT_ATK_TEXT_SELECTION_CHANGE, accessible, nsnull);
  }
#endif

  return NS_OK;
}

/** Return the caret's bounds */
NS_IMETHODIMP nsCaretAccessible::GetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  if (!mVisible)
    return NS_ERROR_FAILURE;  // When root accessible hasn't yet called SetCaretBounds()
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

NS_IMETHODIMP nsCaretAccessible::GetParent(nsIAccessible **_retval)
{   
  *_retval = nsnull;
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


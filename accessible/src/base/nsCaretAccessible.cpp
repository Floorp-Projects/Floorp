/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Original Author: Aaron Leventhal (aaronl@netscape.com)
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

#include "nsAccessibilityService.h"
#include "nsCaretAccessible.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsICaret.h"
#include "nsISelectionController.h"
#include "nsIFrame.h"
#include "nsISelectionPrivate.h"
#include "nsIAccessibleEventListener.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsIPresShell.h"
#include "nsTextAccessible.h"

NS_IMPL_ISUPPORTS_INHERITED2(nsCaretAccessible, nsLeafAccessible, nsIAccessibleCaret, nsISelectionListener)

nsCaretAccessible::nsCaretAccessible(nsIDOMNode* aDocumentNode, nsIWeakReference* aShell, nsIAccessibleEventListener *aListener):
nsLeafAccessible(aDocumentNode, aShell), mVisible(PR_TRUE), mCurrentDOMNode(nsnull), mListener(aListener)
{
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
  nsCOMPtr<nsIDOMDocument> domDoc;
  aCurrentNode->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  if (!doc)  // we also should try to QI to document instead (necessary to do when node is a document)
    doc = do_QueryInterface(aCurrentNode);
  if (!doc)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresShell> presShell;
  doc->GetShellAt(0, getter_AddRefs(presShell));
  nsCOMPtr<nsIContent> content(do_QueryInterface(aCurrentNode));
  if (!content)
    doc->GetRootContent(getter_AddRefs(content));  // If node is not content, use root content
  if (!presShell || !content)
    return NS_ERROR_FAILURE;

  nsIFrame *frame = nsnull;
  presShell->GetPrimaryFrameFor(content, &frame);
  nsCOMPtr<nsIPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));
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
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDoc));
  if (!doc)
    return NS_OK;
  
  nsCOMPtr<nsIPresShell> presShell;
  doc->GetShellAt(0, getter_AddRefs(presShell));
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
  PRBool visible = (caretRect.x >= 0 && caretRect.y >= 0 && caretRect.width >= 0 && caretRect.height >= 0);
  if (visible)  // Make sure it's visible both by looking at coordinates and visible flag
    caret->GetCaretVisible(&visible);
  if (visible != mVisible) {
    mVisible = visible;
    mListener->HandleEvent(mVisible? nsIAccessibleEventListener::EVENT_SHOW: nsIAccessibleEventListener::EVENT_HIDE, this, nsnull);
  }

  nsCOMPtr<nsIPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));
  nsCOMPtr<nsIViewManager> viewManager;
  presShell->GetViewManager(getter_AddRefs(viewManager));
  if (!presContext || !viewManager)
    return NS_OK;
  nsIView *view = nsnull;
  viewManager->GetRootView(view);
  if (!view)
    return NS_OK;
  nsCOMPtr<nsIWidget> widget;
  view->GetWidget(*getter_AddRefs(widget));
  if (!widget)
    return NS_OK;

  float t2p;
  presContext->GetTwipsToPixels(&t2p);
    // Convert to pixels using that scale
  caretRect.x      = NSTwipsToIntPixels(caretRect.x, t2p);
  caretRect.y      = NSTwipsToIntPixels(caretRect.y, t2p);

  caretRect.width  = NSTwipsToIntPixels(caretRect.width, t2p);
  caretRect.height = NSTwipsToIntPixels(caretRect.height, t2p);

  nsRect caretScreenRect;
  widget->WidgetToScreen(caretRect, mCaretRect);

#ifndef MOZ_ACCESSIBILITY_ATK
  if (visible)
    mListener->HandleEvent(nsIAccessibleEventListener::EVENT_LOCATION_CHANGE, this, nsnull);
#else
  nsCOMPtr<nsIDOMNode> focusNode;
  nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(mCurrentDOMNode));
  nsCOMPtr<nsIDOMHTMLTextAreaElement> textArea(do_QueryInterface(mCurrentDOMNode));
  if (inputElement || textArea)
    focusNode = mCurrentDOMNode;
  else
    domSel->GetFocusNode(getter_AddRefs(focusNode));
  if (!focusNode)
    return NS_OK;
  
  nsCOMPtr<nsIAccessible> accessible;
  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
  accService->GetAccessibleFor(focusNode, getter_AddRefs(accessible));
  if (accessible) {
    if (isCollapsed) {
      PRInt32 caretOffset;
      domSel->GetFocusOffset(&caretOffset);
      mListener->HandleEvent(nsIAccessibleEventListener::EVENT_ATK_TEXT_CARET_MOVE, accessible, (AccessibleEventData*)&caretOffset);
    }
    else
      mListener->HandleEvent(nsIAccessibleEventListener::EVENT_ATK_TEXT_SELECTION_CHANGE, accessible, nsnull);
  }
#endif

  return NS_OK;
}

/** Return the caret's bounds */
NS_IMETHODIMP nsCaretAccessible::AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  if (!mVisible)
    return NS_ERROR_FAILURE;  // When root accessible hasn't yet called SetCaretBounds()
  *x = mCaretRect.x;
  *y = mCaretRect.y;
  *width = mCaretRect.width;
  *height = mCaretRect.height;
  return NS_OK;
}

NS_IMETHODIMP nsCaretAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_CARET;
  return NS_OK;
}

NS_IMETHODIMP nsCaretAccessible::GetAccState(PRUint32 *_retval)
{
  *_retval = mVisible? 0: STATE_INVISIBLE;
  return NS_OK;
}

NS_IMETHODIMP nsCaretAccessible::GetAccParent(nsIAccessible **_retval)
{   
  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsCaretAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{ 
  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsCaretAccessible::GetAccNextSibling(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}


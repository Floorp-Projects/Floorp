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

NS_IMPL_ISUPPORTS1(nsCaretAccessible, nsISelectionListener)
  
nsCaretAccessible::nsCaretAccessible( nsRootAccessible *aRootAccessible):
mLastCaretOffset(-1), mRootAccessible(aRootAccessible)
{
}

nsCaretAccessible::~nsCaretAccessible()
{
}

void nsCaretAccessible::Shutdown()
{
  // The caret accessible isn't shut down until the nsRootAccessible owning it is shut down
  // Each nsDocAccessible, including the nsRootAccessible, is responsible for clearing the
  // doc selection listeners they registered in this nsCaretAccessible

  ClearControlSelectionListener(); // Clear the selection listener for the currently focused control
  mLastTextAccessible = nsnull;
  mLastUsedSelection = nsnull;
}

nsresult nsCaretAccessible::ClearControlSelectionListener()
{
  nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryReferent(mCurrentControlSelection));
  NS_ENSURE_TRUE(selPrivate, NS_ERROR_FAILURE);

  mCurrentControlSelection = nsnull;
  mCurrentControl = nsnull;
  return selPrivate->RemoveSelectionListener(this);
}

nsresult nsCaretAccessible::SetControlSelectionListener(nsIDOMNode *aCurrentNode)
{
  mCurrentControl = aCurrentNode;
  mLastTextAccessible = nsnull;

  // When focus moves such that the caret is part of a new frame selection
  // this removes the old selection listener and attaches a new one for the current focus
  nsCOMPtr<nsIPresShell> presShell = 
    mRootAccessible->GetPresShellFor(aCurrentNode);
  if (!presShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc = presShell->GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIContent> content(do_QueryInterface(aCurrentNode));
  // The control selection listener is only for form controls, not for the document
  // When there is no document, the content will be null
  if (!content) {
    return NS_OK;
  }

  nsIFrame *frame = presShell->GetPrimaryFrameFor(content);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  nsPresContext *presContext = presShell->GetPresContext();
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

  nsCOMPtr<nsISelectionController> selCon;
  frame->GetSelectionController(presContext, getter_AddRefs(selCon));
  NS_ENSURE_TRUE(selCon, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsISelection> domSel;
  selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel));

  ClearControlSelectionListener();
  nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(domSel));
  NS_ENSURE_TRUE(selPrivate, NS_ERROR_FAILURE);

  mCurrentControlSelection = do_GetWeakReference(domSel);
  return selPrivate->AddSelectionListener(this);
}

nsresult nsCaretAccessible::AddDocSelectionListener(nsIDOMDocument *aDoc)
{
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
  nsCOMPtr<nsISelectionController> selCon = do_QueryInterface(doc->GetPrimaryShell());
  NS_ENSURE_TRUE(selCon, NS_ERROR_FAILURE);

  nsCOMPtr<nsISelection> domSel;
  selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel));
  nsCOMPtr<nsISelectionPrivate> selPrivate = do_QueryInterface(domSel);
  NS_ENSURE_TRUE(selPrivate, NS_ERROR_FAILURE);

  return selPrivate->AddSelectionListener(this);
}

nsresult nsCaretAccessible::RemoveDocSelectionListener(nsIDOMDocument *aDoc)
{
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsCOMPtr<nsISelectionController> selCon = do_QueryInterface(doc->GetPrimaryShell());
  NS_ENSURE_TRUE(selCon, NS_ERROR_FAILURE);

  nsCOMPtr<nsISelection> domSel;
  selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel));
  nsCOMPtr<nsISelectionPrivate> selPrivate = do_QueryInterface(domSel);
  NS_ENSURE_TRUE(selPrivate, NS_ERROR_FAILURE);

  return selPrivate->RemoveSelectionListener(this);
}

NS_IMETHODIMP nsCaretAccessible::NotifySelectionChanged(nsIDOMDocument *aDoc, nsISelection *aSel, PRInt16 aReason)
{
  mLastUsedSelection = do_GetWeakReference(aSel);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);
  nsIPresShell *presShell = doc->GetPrimaryShell();
  NS_ENSURE_TRUE(presShell, NS_OK);

  // Get first nnsIAccessibleText in parent chain and fire caret-move, selection-change event for it
  nsCOMPtr<nsIAccessible> accessible;
  nsIAccessibilityService *accService = mRootAccessible->GetAccService();
  NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);
  // Get accessible from selection's focus node or its parent
  nsCOMPtr<nsIDOMNode> focusNode;
  aSel->GetFocusNode(getter_AddRefs(focusNode));
  if (!focusNode) {
    mLastTextAccessible = nsnull;
    return NS_OK; // No selection
  }

  nsCOMPtr<nsIAccessibleDocument> docAccessible =
    nsAccessNode::GetDocAccessibleFor(focusNode);
  nsCOMPtr<nsIAccessible> accessibleForDoc =
    do_QueryInterface(docAccessible);
  if (!accessibleForDoc) {
    return NS_OK;
  }
  PRUint32 docState;
  accessibleForDoc->GetFinalState(&docState, nsnull);
  if (docState & nsIAccessibleStates::STATE_BUSY) {
    return NS_OK;  // Don't fire caret moves until doc loaded
  }

  nsCOMPtr<nsIDOMNode> nodeWithCaret = focusNode;

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

  PRInt32 caretOffset;
  nsresult rv = textAcc->GetCaretOffset(&caretOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  if (textAcc == mLastTextAccessible && caretOffset == mLastCaretOffset) {
    PRInt32 selectionCount;
    textAcc->GetSelectionCount(&selectionCount);   // Don't swallow similar events when selecting text
    if (!selectionCount) {
      return NS_OK;  // Swallow duplicate caret event
    }
  }
  mLastCaretOffset = caretOffset;
  mLastTextAccessible = textAcc;

  nsCOMPtr<nsIAccessibleCaretMoveEvent> event =
    new nsAccCaretMoveEvent(focusNode);
  NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);

  return mRootAccessible->FireDelayedAccessibleEvent(event, nsDocAccessible::eRemoveDupes);
}

nsRect
nsCaretAccessible::GetCaretRect(nsIWidget **aOutWidget)
{
  nsRect caretRect;
  NS_ENSURE_TRUE(aOutWidget, caretRect);
  *aOutWidget = nsnull;

  if (!mLastTextAccessible) {
    return caretRect;    // Return empty rect
  }

  nsCOMPtr<nsIAccessNode> lastAccessNode(do_QueryInterface(mLastTextAccessible));
  NS_ENSURE_TRUE(lastAccessNode, caretRect);

  nsCOMPtr<nsIDOMNode> lastNodeWithCaret;
  lastAccessNode->GetDOMNode(getter_AddRefs(lastNodeWithCaret));
  NS_ENSURE_TRUE(lastNodeWithCaret, caretRect);

  nsCOMPtr<nsIPresShell> presShell = mRootAccessible->GetPresShellFor(lastNodeWithCaret);
  NS_ENSURE_TRUE(presShell, caretRect);

  nsCOMPtr<nsICaret> caret;
  presShell->GetCaret(getter_AddRefs(caret));
  NS_ENSURE_TRUE(caret, caretRect);

  PRBool isCollapsed;
  nsIView *view;
  nsCOMPtr<nsISelection> caretSelection(do_QueryReferent(mLastUsedSelection));
  NS_ENSURE_TRUE(caretSelection, caretRect);
  
  caret->GetCaretCoordinates(nsICaret::eRenderingViewCoordinates, caretSelection,
                             &caretRect, &isCollapsed, &view);
  if (!view || caretRect.IsEmpty()) {
    return nsRect(); // Return empty rect
  }

  PRBool isVisible;
  caret->GetCaretVisible(&isVisible);
  if (!isVisible) {
    return nsRect();  // Return empty rect
  }
  nsPoint offsetFromWidget;
  *aOutWidget = view->GetNearestWidget(&offsetFromWidget);
  NS_ENSURE_TRUE(*aOutWidget, nsRect());

  nsPresContext *presContext = presShell->GetPresContext();
  NS_ENSURE_TRUE(presContext, nsRect());

  caretRect.x = presContext->AppUnitsToDevPixels(caretRect.x + offsetFromWidget.x);
  caretRect.y = presContext->AppUnitsToDevPixels(caretRect.y + offsetFromWidget.y);
  caretRect.width = presContext->AppUnitsToDevPixels(caretRect.width);
  caretRect.height = presContext->AppUnitsToDevPixels(caretRect.height);

  (*aOutWidget)->WidgetToScreen(caretRect, caretRect);

  // Correct for character size, so that caret always matches the size of the character
  // This is important for font size transitions, and is necessary because the Gecko caret uses the
  // previous character's size as the user moves forward in the text by character.
  PRInt32 charX, charY, charWidth, charHeight;
  if (NS_SUCCEEDED(mLastTextAccessible->GetCharacterExtents(mLastCaretOffset, &charX, &charY,
                                                            &charWidth, &charHeight,
                                                            nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE))) {
    caretRect.height -= charY - caretRect.y;
    caretRect.y = charY;
  }

  return caretRect;
}


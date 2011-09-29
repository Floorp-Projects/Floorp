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

#include "nsCaretAccessible.h"

#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "nsIAccessibleEvent.h"

#include "nsCaret.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsRootAccessible.h"
#include "nsISelectionPrivate.h"
#include "nsServiceManagerUtils.h"

class nsIWidget;

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
  mRootAccessible = nsnull;
}

nsresult nsCaretAccessible::ClearControlSelectionListener()
{
  nsCOMPtr<nsISelectionController> controller =
    GetSelectionControllerForNode(mCurrentControl);

  mCurrentControl = nsnull;

  if (!controller)
    return NS_OK;

  // Remove 'this' registered as selection listener for the normal selection.
  nsCOMPtr<nsISelection> normalSel;
  controller->GetSelection(nsISelectionController::SELECTION_NORMAL,
                           getter_AddRefs(normalSel));
  nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(normalSel));
  NS_ENSURE_TRUE(selPrivate, NS_ERROR_FAILURE);

  nsresult rv = selPrivate->RemoveSelectionListener(this);
  NS_ENSURE_SUCCESS(rv, rv);

  // Remove 'this' registered as selection listener for the spellcheck
  // selection.
  nsCOMPtr<nsISelection> spellcheckSel;
  controller->GetSelection(nsISelectionController::SELECTION_SPELLCHECK,
                           getter_AddRefs(spellcheckSel));
  selPrivate = do_QueryInterface(spellcheckSel);
  NS_ENSURE_TRUE(selPrivate, NS_ERROR_FAILURE);

  return selPrivate->RemoveSelectionListener(this);
}

nsresult
nsCaretAccessible::SetControlSelectionListener(nsIContent *aCurrentNode)
{
  NS_ENSURE_TRUE(mRootAccessible, NS_ERROR_FAILURE);

  ClearControlSelectionListener();

  mCurrentControl = aCurrentNode;
  mLastTextAccessible = nsnull;

  // When focus moves such that the caret is part of a new frame selection
  // this removes the old selection listener and attaches a new one for
  // the current focus.

  nsCOMPtr<nsISelectionController> controller =
    GetSelectionControllerForNode(mCurrentControl);
#ifdef DEBUG
  NS_ASSERTION(controller || aCurrentNode->IsNodeOfType(nsINode::eDOCUMENT),
               "No selection controller for non document node!");
#endif
  if (!controller)
    return NS_OK;

  // Register 'this' as selection listener for the normal selection.
  nsCOMPtr<nsISelection> normalSel;
  controller->GetSelection(nsISelectionController::SELECTION_NORMAL,
                           getter_AddRefs(normalSel));
  nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(normalSel));
  NS_ENSURE_TRUE(selPrivate, NS_ERROR_FAILURE);

  nsresult rv = selPrivate->AddSelectionListener(this);
  NS_ENSURE_SUCCESS(rv, rv);

  // Register 'this' as selection listener for the spellcheck selection.
  nsCOMPtr<nsISelection> spellcheckSel;
  controller->GetSelection(nsISelectionController::SELECTION_SPELLCHECK,
                           getter_AddRefs(spellcheckSel));
  selPrivate = do_QueryInterface(spellcheckSel);
  NS_ENSURE_TRUE(selPrivate, NS_ERROR_FAILURE);
  
  return selPrivate->AddSelectionListener(this);
}

nsresult
nsCaretAccessible::AddDocSelectionListener(nsIPresShell *aShell)
{
  NS_ENSURE_TRUE(mRootAccessible, NS_ERROR_FAILURE);

  nsCOMPtr<nsISelectionController> selCon = do_QueryInterface(aShell);
  NS_ENSURE_TRUE(selCon, NS_ERROR_FAILURE);

  nsCOMPtr<nsISelection> domSel;
  selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel));
  nsCOMPtr<nsISelectionPrivate> selPrivate = do_QueryInterface(domSel);
  NS_ENSURE_TRUE(selPrivate, NS_ERROR_FAILURE);

  nsresult rv = selPrivate->AddSelectionListener(this);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISelection> spellcheckSel;
  selCon->GetSelection(nsISelectionController::SELECTION_SPELLCHECK,
                       getter_AddRefs(spellcheckSel));
  selPrivate = do_QueryInterface(spellcheckSel);
  NS_ENSURE_TRUE(selPrivate, NS_ERROR_FAILURE);
  
  return selPrivate->AddSelectionListener(this);
}

nsresult
nsCaretAccessible::RemoveDocSelectionListener(nsIPresShell *aShell)
{
  nsCOMPtr<nsISelectionController> selCon = do_QueryInterface(aShell);
  NS_ENSURE_TRUE(selCon, NS_ERROR_FAILURE);

  nsCOMPtr<nsISelection> domSel;
  selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel));
  nsCOMPtr<nsISelectionPrivate> selPrivate = do_QueryInterface(domSel);
  NS_ENSURE_TRUE(selPrivate, NS_ERROR_FAILURE);

  selPrivate->RemoveSelectionListener(this);

  nsCOMPtr<nsISelection> spellcheckSel;
  selCon->GetSelection(nsISelectionController::SELECTION_SPELLCHECK,
                       getter_AddRefs(spellcheckSel));
  selPrivate = do_QueryInterface(spellcheckSel);
  NS_ENSURE_TRUE(selPrivate, NS_ERROR_FAILURE);

  return selPrivate->RemoveSelectionListener(this);
}

NS_IMETHODIMP
nsCaretAccessible::NotifySelectionChanged(nsIDOMDocument* aDOMDocument,
                                          nsISelection* aSelection,
                                          PRInt16 aReason)
{
  NS_ENSURE_ARG(aDOMDocument);
  NS_ENSURE_STATE(mRootAccessible);

  nsCOMPtr<nsIDocument> documentNode(do_QueryInterface(aDOMDocument));
  nsDocAccessible* document = GetAccService()->GetDocAccessible(documentNode);

#ifdef DEBUG_NOTIFICATIONS
  nsCOMPtr<nsISelectionPrivate> privSel(do_QueryInterface(aSelection));

  PRInt16 type = 0;
  privSel->GetType(&type);

  if (type == nsISelectionController::SELECTION_NORMAL ||
      type == nsISelectionController::SELECTION_SPELLCHECK) {

    bool isNormalSelection =
      (type == nsISelectionController::SELECTION_NORMAL);

    bool isIgnored = !document || !document->IsContentLoaded();
    printf("\nSelection changed, selection type: %s, notification %s\n",
           (isNormalSelection ? "normal" : "spellcheck"),
           (isIgnored ? "ignored" : "pending"));
  }
#endif

  // Don't fire events until document is loaded.
  if (document && document->IsContentLoaded()) {
    // The caret accessible has the same lifetime as the root accessible, and
    // this outlives all its descendant document accessibles, so that we are
    // guaranteed that the notification is processed before the caret accessible
    // is destroyed.
    document->HandleNotification<nsCaretAccessible, nsISelection>
      (this, &nsCaretAccessible::ProcessSelectionChanged, aSelection);
  }

  return NS_OK;
}

void
nsCaretAccessible::ProcessSelectionChanged(nsISelection* aSelection)
{
  nsCOMPtr<nsISelectionPrivate> privSel(do_QueryInterface(aSelection));

  PRInt16 type = 0;
  privSel->GetType(&type);

  if (type == nsISelectionController::SELECTION_NORMAL)
    NormalSelectionChanged(aSelection);

  else if (type == nsISelectionController::SELECTION_SPELLCHECK)
    SpellcheckSelectionChanged(aSelection);
}

void
nsCaretAccessible::NormalSelectionChanged(nsISelection* aSelection)
{
  mLastUsedSelection = do_GetWeakReference(aSelection);

  PRInt32 rangeCount = 0;
  aSelection->GetRangeCount(&rangeCount);
  if (rangeCount == 0) {
    mLastTextAccessible = nsnull;
    return; // No selection
  }

  nsHyperTextAccessible* textAcc =
    nsAccUtils::GetTextAccessibleFromSelection(aSelection);
  if (!textAcc)
    return;

  PRInt32 caretOffset = -1;
  nsresult rv = textAcc->GetCaretOffset(&caretOffset);
  if (NS_FAILED(rv))
    return;

  if (textAcc == mLastTextAccessible && caretOffset == mLastCaretOffset) {
    PRInt32 selectionCount = 0;
    textAcc->GetSelectionCount(&selectionCount);   // Don't swallow similar events when selecting text
    if (!selectionCount)
      return;  // Swallow duplicate caret event
  }

  mLastCaretOffset = caretOffset;
  mLastTextAccessible = textAcc;

  nsRefPtr<AccEvent> event =
    new AccCaretMoveEvent(mLastTextAccessible->GetNode());
  if (event)
    mLastTextAccessible->GetDocAccessible()->FireDelayedAccessibleEvent(event);
}

void
nsCaretAccessible::SpellcheckSelectionChanged(nsISelection* aSelection)
{
  // XXX: fire an event for accessible of focus node of the selection. If
  // spellchecking is enabled then we will fire the number of events for
  // the same accessible for newly appended range of the selection (for every
  // misspelled word). If spellchecking is disabled (for example,
  // @spellcheck="false" on html:body) then we won't fire any event.

  nsHyperTextAccessible* textAcc =
    nsAccUtils::GetTextAccessibleFromSelection(aSelection);
  if (!textAcc)
    return;

  nsRefPtr<AccEvent> event =
    new AccEvent(nsIAccessibleEvent::EVENT_TEXT_ATTRIBUTE_CHANGED, textAcc);
  if (event)
    textAcc->GetDocAccessible()->FireDelayedAccessibleEvent(event);
}

nsIntRect
nsCaretAccessible::GetCaretRect(nsIWidget **aOutWidget)
{
  nsIntRect caretRect;
  NS_ENSURE_TRUE(aOutWidget, caretRect);
  *aOutWidget = nsnull;
  NS_ENSURE_TRUE(mRootAccessible, caretRect);

  if (!mLastTextAccessible) {
    return caretRect;    // Return empty rect
  }

  nsINode *lastNodeWithCaret = mLastTextAccessible->GetNode();
  NS_ENSURE_TRUE(lastNodeWithCaret, caretRect);

  nsIPresShell *presShell = nsCoreUtils::GetPresShellFor(lastNodeWithCaret);
  NS_ENSURE_TRUE(presShell, caretRect);

  nsRefPtr<nsCaret> caret = presShell->GetCaret();
  NS_ENSURE_TRUE(caret, caretRect);

  nsCOMPtr<nsISelection> caretSelection(do_QueryReferent(mLastUsedSelection));
  NS_ENSURE_TRUE(caretSelection, caretRect);
  
  bool isVisible;
  caret->GetCaretVisible(&isVisible);
  if (!isVisible) {
    return nsIntRect();  // Return empty rect
  }

  nsRect rect;
  nsIFrame* frame = caret->GetGeometry(caretSelection, &rect);
  if (!frame || rect.IsEmpty()) {
    return nsIntRect(); // Return empty rect
  }

  nsPoint offset;
  // Offset from widget origin to the frame origin, which includes chrome
  // on the widget.
  *aOutWidget = frame->GetNearestWidget(offset);
  NS_ENSURE_TRUE(*aOutWidget, nsIntRect());
  rect.MoveBy(offset);

  caretRect = rect.ToOutsidePixels(frame->PresContext()->AppUnitsPerDevPixel());
  // ((content screen origin) - (content offset in the widget)) = widget origin on the screen
  caretRect.MoveBy((*aOutWidget)->WidgetToScreenOffset() - (*aOutWidget)->GetClientOffset());

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

already_AddRefed<nsISelectionController>
nsCaretAccessible::GetSelectionControllerForNode(nsIContent *aContent)
{
  if (!aContent)
    return nsnull;

  nsIDocument *document = aContent->GetOwnerDoc();
  if (!document)
    return nsnull;

  nsIPresShell *presShell = document->GetShell();
  if (!presShell)
    return nsnull;

  nsIFrame *frame = aContent->GetPrimaryFrame();
  if (!frame)
    return nsnull;

  nsPresContext *presContext = presShell->GetPresContext();
  if (!presContext)
    return nsnull;

  nsISelectionController *controller = nsnull;
  frame->GetSelectionController(presContext, &controller);
  return controller;
}


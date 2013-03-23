/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/a11y/SelectionManager.h"

#include "DocAccessible-inl.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "nsIAccessibleEvent.h"
#include "RootAccessible.h"

#include "nsCaret.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsISelectionPrivate.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Selection.h"

using namespace mozilla;
using namespace mozilla::a11y;

void
SelectionManager::Shutdown()
{
  ClearControlSelectionListener();
  mLastTextAccessible = nullptr;
  mLastUsedSelection = nullptr;
}

void
SelectionManager::ClearControlSelectionListener()
{
  if (!mCurrCtrlFrame)
    return;

  const nsFrameSelection* frameSel = mCurrCtrlFrame->GetConstFrameSelection();
  NS_ASSERTION(frameSel, "No frame selection for the element!");

  mCurrCtrlFrame = nullptr;
  if (!frameSel)
    return;

  // Remove 'this' registered as selection listener for the normal selection.
  Selection* normalSel =
    frameSel->GetSelection(nsISelectionController::SELECTION_NORMAL);
  normalSel->RemoveSelectionListener(this);

  // Remove 'this' registered as selection listener for the spellcheck
  // selection.
  Selection* spellSel =
    frameSel->GetSelection(nsISelectionController::SELECTION_SPELLCHECK);
  spellSel->RemoveSelectionListener(this);
}

void
SelectionManager::SetControlSelectionListener(dom::Element* aFocusedElm)
{
  // When focus moves such that the caret is part of a new frame selection
  // this removes the old selection listener and attaches a new one for
  // the current focus.
  ClearControlSelectionListener();

  mLastTextAccessible = nullptr;

  mCurrCtrlFrame = aFocusedElm->GetPrimaryFrame();
  if (!mCurrCtrlFrame)
    return;

  const nsFrameSelection* frameSel = mCurrCtrlFrame->GetConstFrameSelection();
  NS_ASSERTION(frameSel, "No frame selection for focused element!");
  if (!frameSel)
    return;

  // Register 'this' as selection listener for the normal selection.
  Selection* normalSel =
    frameSel->GetSelection(nsISelectionController::SELECTION_NORMAL);
  normalSel->AddSelectionListener(this);

  // Register 'this' as selection listener for the spell check selection.
  Selection* spellSel =
    frameSel->GetSelection(nsISelectionController::SELECTION_SPELLCHECK);
  spellSel->AddSelectionListener(this);
}

void
SelectionManager::AddDocSelectionListener(nsIPresShell* aPresShell)
{
  const nsFrameSelection* frameSel = aPresShell->ConstFrameSelection();

  // Register 'this' as selection listener for the normal selection.
  Selection* normalSel =
    frameSel->GetSelection(nsISelectionController::SELECTION_NORMAL);
  normalSel->AddSelectionListener(this);

  // Register 'this' as selection listener for the spell check selection.
  Selection* spellSel =
    frameSel->GetSelection(nsISelectionController::SELECTION_SPELLCHECK);
  spellSel->AddSelectionListener(this);
}

void
SelectionManager::RemoveDocSelectionListener(nsIPresShell* aPresShell)
{
  const nsFrameSelection* frameSel = aPresShell->ConstFrameSelection();

  // Remove 'this' registered as selection listener for the normal selection.
  Selection* normalSel =
    frameSel->GetSelection(nsISelectionController::SELECTION_NORMAL);
  normalSel->RemoveSelectionListener(this);

  // Remove 'this' registered as selection listener for the spellcheck
  // selection.
  Selection* spellSel =
    frameSel->GetSelection(nsISelectionController::SELECTION_SPELLCHECK);
  spellSel->RemoveSelectionListener(this);
}

NS_IMETHODIMP
SelectionManager::NotifySelectionChanged(nsIDOMDocument* aDOMDocument,
                                         nsISelection* aSelection,
                                         int16_t aReason)
{
  NS_ENSURE_ARG(aDOMDocument);

  nsCOMPtr<nsIDocument> documentNode(do_QueryInterface(aDOMDocument));
  DocAccessible* document = GetAccService()->GetDocAccessible(documentNode);

#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eSelection))
    logging::SelChange(aSelection, document);
#endif

  // Don't fire events until document is loaded.
  if (document && document->IsContentLoaded()) {
    // Selection manager has longer lifetime than any document accessible,
    // so that we are guaranteed that the notification is processed before
    // the selection manager is destroyed.
    document->HandleNotification<SelectionManager, nsISelection>
      (this, &SelectionManager::ProcessSelectionChanged, aSelection);
  }

  return NS_OK;
}

void
SelectionManager::ProcessSelectionChanged(nsISelection* aSelection)
{
  nsCOMPtr<nsISelectionPrivate> privSel(do_QueryInterface(aSelection));

  int16_t type = 0;
  privSel->GetType(&type);

  if (type == nsISelectionController::SELECTION_NORMAL)
    NormalSelectionChanged(aSelection);

  else if (type == nsISelectionController::SELECTION_SPELLCHECK)
    SpellcheckSelectionChanged(aSelection);
}

void
SelectionManager::NormalSelectionChanged(nsISelection* aSelection)
{
  mLastUsedSelection = do_GetWeakReference(aSelection);

  int32_t rangeCount = 0;
  aSelection->GetRangeCount(&rangeCount);
  if (rangeCount == 0) {
    mLastTextAccessible = nullptr;
    return; // No selection
  }

  HyperTextAccessible* textAcc =
    nsAccUtils::GetTextAccessibleFromSelection(aSelection);
  if (!textAcc)
    return;

  int32_t caretOffset = -1;
  nsresult rv = textAcc->GetCaretOffset(&caretOffset);
  if (NS_FAILED(rv))
    return;

  if (textAcc == mLastTextAccessible && caretOffset == mLastCaretOffset) {
    int32_t selectionCount = 0;
    textAcc->GetSelectionCount(&selectionCount);   // Don't swallow similar events when selecting text
    if (!selectionCount)
      return;  // Swallow duplicate caret event
  }

  mLastCaretOffset = caretOffset;
  mLastTextAccessible = textAcc;

  nsRefPtr<AccEvent> event = new AccCaretMoveEvent(mLastTextAccessible);
  mLastTextAccessible->Document()->FireDelayedEvent(event);
}

void
SelectionManager::SpellcheckSelectionChanged(nsISelection* aSelection)
{
  // XXX: fire an event for accessible of focus node of the selection. If
  // spellchecking is enabled then we will fire the number of events for
  // the same accessible for newly appended range of the selection (for every
  // misspelled word). If spellchecking is disabled (for example,
  // @spellcheck="false" on html:body) then we won't fire any event.

  HyperTextAccessible* hyperText =
    nsAccUtils::GetTextAccessibleFromSelection(aSelection);
  if (hyperText) {
    hyperText->Document()->
      FireDelayedEvent(nsIAccessibleEvent::EVENT_TEXT_ATTRIBUTE_CHANGED,
                       hyperText);
  }
}

nsIntRect
SelectionManager::GetCaretRect(nsIWidget** aWidget)
{
  nsIntRect caretRect;
  NS_ENSURE_TRUE(aWidget, caretRect);
  *aWidget = nullptr;

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
  *aWidget = frame->GetNearestWidget(offset);
  NS_ENSURE_TRUE(*aWidget, nsIntRect());
  rect.MoveBy(offset);

  caretRect = rect.ToOutsidePixels(frame->PresContext()->AppUnitsPerDevPixel());
  // ((content screen origin) - (content offset in the widget)) = widget origin on the screen
  caretRect.MoveBy((*aWidget)->WidgetToScreenOffset() - (*aWidget)->GetClientOffset());

  // Correct for character size, so that caret always matches the size of the character
  // This is important for font size transitions, and is necessary because the Gecko caret uses the
  // previous character's size as the user moves forward in the text by character.
  int32_t charX, charY, charWidth, charHeight;
  if (NS_SUCCEEDED(mLastTextAccessible->GetCharacterExtents(mLastCaretOffset, &charX, &charY,
                                                            &charWidth, &charHeight,
                                                            nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE))) {
    caretRect.height -= charY - caretRect.y;
    caretRect.y = charY;
  }

  return caretRect;
}

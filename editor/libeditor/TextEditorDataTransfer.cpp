/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TextEditor.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/SelectionState.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DragEvent.h"
#include "mozilla/dom/Selection.h"
#include "nsAString.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIClipboard.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsIEditor.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIPrincipal.h"
#include "nsIFormControl.h"
#include "nsIPlaintextEditor.h"
#include "nsISupportsPrimitives.h"
#include "nsITransferable.h"
#include "nsIVariant.h"
#include "nsLiteralString.h"
#include "nsRange.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsXPCOM.h"
#include "nscore.h"

class nsILoadContext;
class nsISupports;

namespace mozilla {

using namespace dom;

nsresult
TextEditor::PrepareTransferable(nsITransferable** transferable)
{
  // Create generic Transferable for getting the data
  nsresult rv = CallCreateInstance("@mozilla.org/widget/transferable;1", transferable);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the nsITransferable interface for getting the data from the clipboard
  if (transferable) {
    nsCOMPtr<nsIDocument> destdoc = GetDocument();
    nsILoadContext* loadContext = destdoc ? destdoc->GetLoadContext() : nullptr;
    (*transferable)->Init(loadContext);

    (*transferable)->AddDataFlavor(kUnicodeMime);
    (*transferable)->AddDataFlavor(kMozTextInternal);
  };
  return NS_OK;
}

nsresult
TextEditor::InsertTextAt(const nsAString& aStringToInsert,
                         nsINode* aDestinationNode,
                         int32_t aDestOffset,
                         bool aDoDeleteSelection)
{
  if (aDestinationNode) {
    RefPtr<Selection> selection = GetSelection();
    NS_ENSURE_STATE(selection);

    nsCOMPtr<nsINode> targetNode = aDestinationNode;
    int32_t targetOffset = aDestOffset;

    if (aDoDeleteSelection) {
      // Use an auto tracker so that our drop point is correctly
      // positioned after the delete.
      AutoTrackDOMPoint tracker(mRangeUpdater, &targetNode, &targetOffset);
      nsresult rv = DeleteSelectionAsAction(eNone, eStrip);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    ErrorResult error;
    selection->Collapse(RawRangeBoundary(targetNode, targetOffset), error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }

  nsresult rv = InsertTextAsAction(aStringToInsert);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult
TextEditor::InsertTextFromTransferable(nsITransferable* aTransferable)
{
  nsresult rv = NS_OK;
  nsAutoCString bestFlavor;
  nsCOMPtr<nsISupports> genericDataObj;
  uint32_t len = 0;
  if (NS_SUCCEEDED(
        aTransferable->GetAnyTransferData(bestFlavor,
                                          getter_AddRefs(genericDataObj),
                                          &len)) &&
      (bestFlavor.EqualsLiteral(kUnicodeMime) ||
       bestFlavor.EqualsLiteral(kMozTextInternal))) {
    AutoTransactionsConserveSelection dontChangeMySelection(this);
    nsCOMPtr<nsISupportsString> textDataObj ( do_QueryInterface(genericDataObj) );
    if (textDataObj && len > 0) {
      nsAutoString stuffToPaste;
      textDataObj->GetData(stuffToPaste);
      NS_ASSERTION(stuffToPaste.Length() <= (len/2), "Invalid length!");

      // Sanitize possible carriage returns in the string to be inserted
      nsContentUtils::PlatformToDOMLineBreaks(stuffToPaste);

      AutoPlaceholderBatch beginBatching(this);
      rv = InsertTextAt(stuffToPaste, nullptr, 0, true);
    }
  }

  // Try to scroll the selection into view if the paste/drop succeeded

  if (NS_SUCCEEDED(rv)) {
    ScrollSelectionIntoView(false);
  }

  return rv;
}

nsresult
TextEditor::InsertFromDataTransfer(DataTransfer* aDataTransfer,
                                   int32_t aIndex,
                                   nsIDocument* aSourceDoc,
                                   nsINode* aDestinationNode,
                                   int32_t aDestOffset,
                                   bool aDoDeleteSelection)
{
  nsCOMPtr<nsIVariant> data;
  aDataTransfer->GetDataAtNoSecurityCheck(NS_LITERAL_STRING("text/plain"), aIndex,
                                          getter_AddRefs(data));
  if (data) {
    nsAutoString insertText;
    data->GetAsAString(insertText);
    nsContentUtils::PlatformToDOMLineBreaks(insertText);

    AutoPlaceholderBatch beginBatching(this);
    return InsertTextAt(insertText, aDestinationNode, aDestOffset, aDoDeleteSelection);
  }

  return NS_OK;
}

nsresult
TextEditor::OnDrop(DragEvent* aDropEvent)
{
  CommitComposition();

  NS_ENSURE_TRUE(aDropEvent, NS_ERROR_FAILURE);

  RefPtr<DataTransfer> dataTransfer = aDropEvent->GetDataTransfer();
  NS_ENSURE_TRUE(dataTransfer, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  NS_ASSERTION(dragSession, "No drag session");

  nsCOMPtr<nsINode> sourceNode = dataTransfer->GetMozSourceNode();

  nsCOMPtr<nsIDocument> srcdoc;
  if (sourceNode) {
    srcdoc = sourceNode->OwnerDoc();
  }

  if (nsContentUtils::CheckForSubFrameDrop(dragSession,
        aDropEvent->WidgetEventPtr()->AsDragEvent())) {
    // Don't allow drags from subframe documents with different origins than
    // the drop destination.
    if (srcdoc && !IsSafeToInsertData(srcdoc)) {
      return NS_OK;
    }
  }

  // Current doc is destination
  nsIDocument* destdoc = GetDocument();
  if (NS_WARN_IF(!destdoc)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  uint32_t numItems = dataTransfer->MozItemCount();
  if (numItems < 1) {
    return NS_ERROR_FAILURE;  // Nothing to drop?
  }

  // Combine any deletion and drop insertion into one transaction
  AutoPlaceholderBatch beginBatching(this);

  bool deleteSelection = false;

  // We have to figure out whether to delete and relocate caret only once
  // Parent and offset are under the mouse cursor
  nsCOMPtr<nsINode> newSelectionParent = aDropEvent->GetRangeParent();
  NS_ENSURE_TRUE(newSelectionParent, NS_ERROR_FAILURE);

  int32_t newSelectionOffset = aDropEvent->RangeOffset();

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  bool isCollapsed = selection->IsCollapsed();

  // Check if mouse is in the selection
  // if so, jump through some hoops to determine if mouse is over selection (bail)
  // and whether user wants to copy selection or delete it
  if (!isCollapsed) {
    // We never have to delete if selection is already collapsed
    bool cursorIsInSelection = false;

    uint32_t rangeCount = selection->RangeCount();

    for (uint32_t j = 0; j < rangeCount; j++) {
      RefPtr<nsRange> range = selection->GetRangeAt(j);
      if (!range) {
        // don't bail yet, iterate through them all
        continue;
      }

      IgnoredErrorResult rv;
      cursorIsInSelection =
        range->IsPointInRange(*newSelectionParent, newSelectionOffset, rv);
      if (rv.Failed()) {
        // Probably don't want to consider this as "in selection!"
        cursorIsInSelection = false;
      }
      if (cursorIsInSelection) {
        break;
      }
    }

    if (cursorIsInSelection) {
      // Dragging within same doc can't drop on itself -- leave!
      if (srcdoc == destdoc) {
        return NS_OK;
      }

      // Dragging from another window onto a selection
      // XXX Decision made to NOT do this,
      //     note that 4.x does replace if dropped on
      //deleteSelection = true;
    } else {
      // We are NOT over the selection
      if (srcdoc == destdoc) {
        // Within the same doc: delete if user doesn't want to copy
        uint32_t dropEffect = dataTransfer->DropEffectInt();
        deleteSelection = !(dropEffect & nsIDragService::DRAGDROP_ACTION_COPY);
      } else {
        // Different source doc: Don't delete
        deleteSelection = false;
      }
    }
  }

  if (IsPlaintextEditor()) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(newSelectionParent);
    while (content) {
      nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(content));
      if (formControl && !formControl->AllowDrop()) {
        // Don't allow dropping into a form control that doesn't allow being
        // dropped into.
        return NS_OK;
      }
      content = content->GetParent();
    }
  }

  for (uint32_t i = 0; i < numItems; ++i) {
    InsertFromDataTransfer(dataTransfer, i, srcdoc,
                           newSelectionParent,
                           newSelectionOffset, deleteSelection);
  }

  ScrollSelectionIntoView(false);

  return NS_OK;
}

NS_IMETHODIMP
TextEditor::Paste(int32_t aSelectionType)
{
  if (!FireClipboardEvent(ePaste, aSelectionType)) {
    return NS_OK;
  }

  // Get Clipboard Service
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Get the nsITransferable interface for getting the data from the clipboard
  nsCOMPtr<nsITransferable> trans;
  rv = PrepareTransferable(getter_AddRefs(trans));
  if (NS_SUCCEEDED(rv) && trans) {
    // Get the Data from the clipboard
    if (NS_SUCCEEDED(clipboard->GetData(trans, aSelectionType)) &&
        IsModifiable()) {
      rv = InsertTextFromTransferable(trans);
    }
  }

  return rv;
}

NS_IMETHODIMP
TextEditor::PasteTransferable(nsITransferable* aTransferable)
{
  // Use an invalid value for the clipboard type as data comes from aTransferable
  // and we don't currently implement a way to put that in the data transfer yet.
  if (!FireClipboardEvent(ePaste, -1)) {
    return NS_OK;
  }

  if (!IsModifiable()) {
    return NS_OK;
  }

  return InsertTextFromTransferable(aTransferable);
}

NS_IMETHODIMP
TextEditor::CanPaste(int32_t aSelectionType,
                     bool* aCanPaste)
{
  NS_ENSURE_ARG_POINTER(aCanPaste);
  *aCanPaste = false;

  // Always enable the paste command when inside of a HTML or XHTML document.
  nsCOMPtr<nsIDocument> doc = GetDocument();
  if (doc && doc->IsHTMLOrXHTML()) {
    *aCanPaste = true;
    return NS_OK;
  }

  // can't paste if readonly
  if (!IsModifiable()) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // the flavors that we can deal with
  const char* textEditorFlavors[] = { kUnicodeMime };

  bool haveFlavors;
  rv = clipboard->HasDataMatchingFlavors(textEditorFlavors,
                                         ArrayLength(textEditorFlavors),
                                         aSelectionType, &haveFlavors);
  NS_ENSURE_SUCCESS(rv, rv);

  *aCanPaste = haveFlavors;
  return NS_OK;
}

bool
TextEditor::CanPasteTransferable(nsITransferable* aTransferable)
{
  // can't paste if readonly
  if (!IsModifiable()) {
    return false;
  }

  // If |aTransferable| is null, assume that a paste will succeed.
  if (!aTransferable) {
    return true;
  }

  nsCOMPtr<nsISupports> data;
  uint32_t dataLen;
  nsresult rv = aTransferable->GetTransferData(kUnicodeMime,
                                               getter_AddRefs(data),
                                               &dataLen);
  if (NS_SUCCEEDED(rv) && data) {
    return true;
  }

  return false;
}

bool
TextEditor::IsSafeToInsertData(nsIDocument* aSourceDoc)
{
  // Try to determine whether we should use a sanitizing fragment sink
  bool isSafe = false;

  nsCOMPtr<nsIDocument> destdoc = GetDocument();
  NS_ASSERTION(destdoc, "Where is our destination doc?");
  nsCOMPtr<nsIDocShellTreeItem> dsti = destdoc->GetDocShell();
  nsCOMPtr<nsIDocShellTreeItem> root;
  if (dsti) {
    dsti->GetRootTreeItem(getter_AddRefs(root));
  }
  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(root);
  uint32_t appType;
  if (docShell && NS_SUCCEEDED(docShell->GetAppType(&appType))) {
    isSafe = appType == nsIDocShell::APP_TYPE_EDITOR;
  }
  if (!isSafe && aSourceDoc) {
    nsIPrincipal* srcPrincipal = aSourceDoc->NodePrincipal();
    nsIPrincipal* destPrincipal = destdoc->NodePrincipal();
    NS_ASSERTION(srcPrincipal && destPrincipal, "How come we don't have a principal?");
    srcPrincipal->Subsumes(destPrincipal, &isSafe);
  }

  return isSafe;
}

} // namespace mozilla

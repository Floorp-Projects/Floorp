/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsPlaintextEditor.h"

#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIFormControl.h"
#include "nsIDOMEventTarget.h" 
#include "nsIDOMNSEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMDragEvent.h"
#include "nsISelection.h"
#include "nsCRT.h"
#include "nsServiceManagerUtils.h"

#include "nsIDOMRange.h"
#include "nsIDOMDOMStringList.h"
#include "nsIDocumentEncoder.h"
#include "nsISupportsPrimitives.h"

// Drag & Drop, Clipboard
#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsIDragService.h"
#include "nsIDOMUIEvent.h"
#include "nsCopySupport.h"

// Misc
#include "nsEditorUtils.h"
#include "nsContentCID.h"
#include "nsISelectionPrivate.h"
#include "nsFrameSelection.h"
#include "nsEventDispatcher.h"
#include "nsContentUtils.h"

using namespace mozilla;

NS_IMETHODIMP nsPlaintextEditor::PrepareTransferable(nsITransferable **transferable)
{
  // Create generic Transferable for getting the data
  nsresult rv = CallCreateInstance("@mozilla.org/widget/transferable;1", transferable);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the nsITransferable interface for getting the data from the clipboard
  if (transferable) {
    nsCOMPtr<nsIDocument> destdoc = GetDocument();
    nsILoadContext* loadContext = destdoc ? destdoc->GetLoadContext() : nsnull;
    (*transferable)->Init(loadContext);

    (*transferable)->AddDataFlavor(kUnicodeMime);
    (*transferable)->AddDataFlavor(kMozTextInternal);
  };
  return NS_OK;
}

nsresult nsPlaintextEditor::InsertTextAt(const nsAString &aStringToInsert,
                                         nsIDOMNode *aDestinationNode,
                                         PRInt32 aDestOffset,
                                         bool aDoDeleteSelection)
{
  if (aDestinationNode)
  {
    nsresult res;
    nsCOMPtr<nsISelection>selection;
    res = GetSelection(getter_AddRefs(selection));
    NS_ENSURE_SUCCESS(res, res);

    nsCOMPtr<nsIDOMNode> targetNode = aDestinationNode;
    PRInt32 targetOffset = aDestOffset;

    if (aDoDeleteSelection)
    {
      // Use an auto tracker so that our drop point is correctly
      // positioned after the delete.
      nsAutoTrackDOMPoint tracker(mRangeUpdater, &targetNode, &targetOffset);
      res = DeleteSelection(eNone, eStrip);
      NS_ENSURE_SUCCESS(res, res);
    }

    res = selection->Collapse(targetNode, targetOffset);
    NS_ENSURE_SUCCESS(res, res);
  }

  return InsertText(aStringToInsert);
}

NS_IMETHODIMP nsPlaintextEditor::InsertTextFromTransferable(nsITransferable *aTransferable,
                                                            nsIDOMNode *aDestinationNode,
                                                            PRInt32 aDestOffset,
                                                            bool aDoDeleteSelection)
{
  HandlingTrustedAction trusted(this);

  nsresult rv = NS_OK;
  char* bestFlavor = nsnull;
  nsCOMPtr<nsISupports> genericDataObj;
  PRUint32 len = 0;
  if (NS_SUCCEEDED(aTransferable->GetAnyTransferData(&bestFlavor, getter_AddRefs(genericDataObj), &len))
      && bestFlavor && (0 == nsCRT::strcmp(bestFlavor, kUnicodeMime) ||
                        0 == nsCRT::strcmp(bestFlavor, kMozTextInternal)))
  {
    nsAutoTxnsConserveSelection dontSpazMySelection(this);
    nsCOMPtr<nsISupportsString> textDataObj ( do_QueryInterface(genericDataObj) );
    if (textDataObj && len > 0)
    {
      nsAutoString stuffToPaste;
      textDataObj->GetData(stuffToPaste);
      NS_ASSERTION(stuffToPaste.Length() <= (len/2), "Invalid length!");

      // Sanitize possible carriage returns in the string to be inserted
      nsContentUtils::PlatformToDOMLineBreaks(stuffToPaste);

      nsAutoEditBatch beginBatching(this);
      rv = InsertTextAt(stuffToPaste, aDestinationNode, aDestOffset, aDoDeleteSelection);
    }
  }
  NS_Free(bestFlavor);
      
  // Try to scroll the selection into view if the paste/drop succeeded

  if (NS_SUCCEEDED(rv))
    ScrollSelectionIntoView(false);

  return rv;
}

nsresult nsPlaintextEditor::InsertFromDataTransfer(nsIDOMDataTransfer *aDataTransfer,
                                                   PRInt32 aIndex,
                                                   nsIDOMDocument *aSourceDoc,
                                                   nsIDOMNode *aDestinationNode,
                                                   PRInt32 aDestOffset,
                                                   bool aDoDeleteSelection)
{
  nsCOMPtr<nsIVariant> data;
  aDataTransfer->MozGetDataAt(NS_LITERAL_STRING("text/plain"), aIndex,
                              getter_AddRefs(data));
  if (data) {
    nsAutoString insertText;
    data->GetAsAString(insertText);
    nsContentUtils::PlatformToDOMLineBreaks(insertText);

    nsAutoEditBatch beginBatching(this);
    return InsertTextAt(insertText, aDestinationNode, aDestOffset, aDoDeleteSelection);
  }

  return NS_OK;
}

nsresult nsPlaintextEditor::InsertFromDrop(nsIDOMEvent* aDropEvent)
{
  ForceCompositionEnd();

  nsCOMPtr<nsIDOMDragEvent> dragEvent(do_QueryInterface(aDropEvent));
  NS_ENSURE_TRUE(dragEvent, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMDataTransfer> dataTransfer;
  nsresult rv = dragEvent->GetDataTransfer(getter_AddRefs(dataTransfer));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  NS_ASSERTION(dragSession, "No drag session");

  nsDragEvent* dragEventInternal = static_cast<nsDragEvent *>(aDropEvent->GetInternalNSEvent());
  if (nsContentUtils::CheckForSubFrameDrop(dragSession, dragEventInternal)) {
    return NS_OK;
  }

  // Current doc is destination
  nsCOMPtr<nsIDOMDocument> destdomdoc = GetDOMDocument();
  NS_ENSURE_TRUE(destdomdoc, NS_ERROR_NOT_INITIALIZED);

  PRUint32 numItems = 0;
  rv = dataTransfer->GetMozItemCount(&numItems);
  NS_ENSURE_SUCCESS(rv, rv);
  if (numItems < 1) return NS_ERROR_FAILURE;  // nothing to drop?

  // Combine any deletion and drop insertion into one transaction
  nsAutoEditBatch beginBatching(this);

  bool deleteSelection = false;

  // We have to figure out whether to delete and relocate caret only once
  // Parent and offset are under the mouse cursor
  nsCOMPtr<nsIDOMUIEvent> uiEvent = do_QueryInterface(aDropEvent);
  NS_ENSURE_TRUE(uiEvent, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMNode> newSelectionParent;
  rv = uiEvent->GetRangeParent(getter_AddRefs(newSelectionParent));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(newSelectionParent, NS_ERROR_FAILURE);

  PRInt32 newSelectionOffset;
  rv = uiEvent->GetRangeOffset(&newSelectionOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISelection> selection;
  rv = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  bool isCollapsed = selection->Collapsed();

  nsCOMPtr<nsIDOMNode> sourceNode;
  dataTransfer->GetMozSourceNode(getter_AddRefs(sourceNode));

  nsCOMPtr<nsIDOMDocument> srcdomdoc;
  if (sourceNode) {
    sourceNode->GetOwnerDocument(getter_AddRefs(srcdomdoc));
    NS_ENSURE_TRUE(sourceNode, NS_ERROR_FAILURE);
  }

  // Only the nsHTMLEditor::FindUserSelectAllNode returns a node.
  nsCOMPtr<nsIDOMNode> userSelectNode = FindUserSelectAllNode(newSelectionParent);
  if (userSelectNode)
  {
    // The drop is happening over a "-moz-user-select: all"
    // subtree so make sure the content we insert goes before
    // the root of the subtree.
    //
    // XXX: Note that inserting before the subtree matches the
    //      current behavior when dropping on top of an image.
    //      The decision for dropping before or after the
    //      subtree should really be done based on coordinates.

    GetNodeLocation(userSelectNode, address_of(newSelectionParent),
                    &newSelectionOffset);

    NS_ENSURE_TRUE(newSelectionParent, NS_ERROR_FAILURE);
  }

  // Check if mouse is in the selection
  // if so, jump through some hoops to determine if mouse is over selection (bail)
  // and whether user wants to copy selection or delete it
  if (!isCollapsed)
  {
    // We never have to delete if selection is already collapsed
    bool cursorIsInSelection = false;

    PRInt32 rangeCount;
    rv = selection->GetRangeCount(&rangeCount);
    NS_ENSURE_SUCCESS(rv, rv);

    for (PRInt32 j = 0; j < rangeCount; j++)
    {
      nsCOMPtr<nsIDOMRange> range;
      rv = selection->GetRangeAt(j, getter_AddRefs(range));
      if (NS_FAILED(rv) || !range) 
        continue;  // don't bail yet, iterate through them all

      rv = range->IsPointInRange(newSelectionParent, newSelectionOffset, &cursorIsInSelection);
      if (cursorIsInSelection)
        break;
    }

    if (cursorIsInSelection)
    {
      // Dragging within same doc can't drop on itself -- leave!
      if (srcdomdoc == destdomdoc)
        return NS_OK;

      // Dragging from another window onto a selection
      // XXX Decision made to NOT do this,
      //     note that 4.x does replace if dropped on
      //deleteSelection = true;
    }
    else 
    {
      // We are NOT over the selection
      if (srcdomdoc == destdomdoc)
      {
        // Within the same doc: delete if user doesn't want to copy
        PRUint32 dropEffect;
        dataTransfer->GetDropEffectInt(&dropEffect);
        deleteSelection = !(dropEffect & nsIDragService::DRAGDROP_ACTION_COPY);
      }
      else
      {
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

  for (PRUint32 i = 0; i < numItems; ++i) {
    InsertFromDataTransfer(dataTransfer, i, srcdomdoc, newSelectionParent,
                           newSelectionOffset, deleteSelection);
  }

  if (NS_SUCCEEDED(rv))
    ScrollSelectionIntoView(false);

  return rv;
}

NS_IMETHODIMP nsPlaintextEditor::Paste(PRInt32 aSelectionType)
{
  if (!FireClipboardEvent(NS_PASTE))
    return NS_OK;

  // Get Clipboard Service
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  if ( NS_FAILED(rv) )
    return rv;

  // Get the nsITransferable interface for getting the data from the clipboard
  nsCOMPtr<nsITransferable> trans;
  rv = PrepareTransferable(getter_AddRefs(trans));
  if (NS_SUCCEEDED(rv) && trans)
  {
    // Get the Data from the clipboard  
    if (NS_SUCCEEDED(clipboard->GetData(trans, aSelectionType)) && IsModifiable())
    {
      // handle transferable hooks
      nsCOMPtr<nsIDOMDocument> domdoc = GetDOMDocument();
      if (!nsEditorHookUtils::DoInsertionHook(domdoc, nsnull, trans))
        return NS_OK;

      rv = InsertTextFromTransferable(trans, nsnull, nsnull, true);
    }
  }

  return rv;
}

NS_IMETHODIMP nsPlaintextEditor::PasteTransferable(nsITransferable *aTransferable)
{
  if (!FireClipboardEvent(NS_PASTE))
    return NS_OK;

  if (!IsModifiable())
    return NS_OK;

  // handle transferable hooks
  nsCOMPtr<nsIDOMDocument> domdoc = GetDOMDocument();
  if (!nsEditorHookUtils::DoInsertionHook(domdoc, nsnull, aTransferable))
    return NS_OK;

  return InsertTextFromTransferable(aTransferable, nsnull, nsnull, true);
}

NS_IMETHODIMP nsPlaintextEditor::CanPaste(PRInt32 aSelectionType, bool *aCanPaste)
{
  NS_ENSURE_ARG_POINTER(aCanPaste);
  *aCanPaste = false;

  // can't paste if readonly
  if (!IsModifiable())
    return NS_OK;

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


NS_IMETHODIMP nsPlaintextEditor::CanPasteTransferable(nsITransferable *aTransferable, bool *aCanPaste)
{
  NS_ENSURE_ARG_POINTER(aCanPaste);

  // can't paste if readonly
  if (!IsModifiable()) {
    *aCanPaste = false;
    return NS_OK;
  }

  // If |aTransferable| is null, assume that a paste will succeed.
  if (!aTransferable) {
    *aCanPaste = true;
    return NS_OK;
  }

  nsCOMPtr<nsISupports> data;
  PRUint32 dataLen;
  nsresult rv = aTransferable->GetTransferData(kUnicodeMime,
                                               getter_AddRefs(data),
                                               &dataLen);
  if (NS_SUCCEEDED(rv) && data)
    *aCanPaste = true;
  else
    *aCanPaste = false;
  
  return NS_OK;
}

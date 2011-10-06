/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsIDOMNSRange.h"
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

NS_IMETHODIMP nsPlaintextEditor::PrepareTransferable(nsITransferable **transferable)
{
  // Create generic Transferable for getting the data
  nsresult rv = CallCreateInstance("@mozilla.org/widget/transferable;1", transferable);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the nsITransferable interface for getting the data from the clipboard
  if (transferable) {
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
      res = DeleteSelection(eNone);
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
  FireTrustedInputEvent trusted(this);

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

  // After ScrollSelectionIntoView(), the pending notifications might be flushed
  // and PresShell/PresContext/Frames may be dead. See bug 418470.
  if (NS_SUCCEEDED(rv))
    ScrollSelectionIntoView(PR_FALSE);

  return rv;
}

NS_IMETHODIMP nsPlaintextEditor::InsertFromDrop(nsIDOMEvent* aDropEvent)
{
  ForceCompositionEnd();
  
  nsresult rv;
  nsCOMPtr<nsIDragService> dragService = 
           do_GetService("@mozilla.org/widget/dragservice;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDragSession> dragSession;
  dragService->GetCurrentSession(getter_AddRefs(dragSession));
  NS_ENSURE_TRUE(dragSession, NS_OK);

  // Current doc is destination
  nsCOMPtr<nsIDOMDocument> destdomdoc; 
  rv = GetDocument(getter_AddRefs(destdomdoc)); 
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the nsITransferable interface for getting the data from the drop
  nsCOMPtr<nsITransferable> trans;
  rv = PrepareTransferable(getter_AddRefs(trans));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(trans, NS_OK);  // NS_ERROR_FAILURE; SHOULD WE FAIL?

  PRUint32 numItems = 0; 
  rv = dragSession->GetNumDropItems(&numItems);
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

  bool isCollapsed;
  rv = selection->GetIsCollapsed(&isCollapsed);
  NS_ENSURE_SUCCESS(rv, rv);

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
      nsCOMPtr<nsIDOMNSRange> nsrange(do_QueryInterface(range));
      if (NS_FAILED(rv) || !nsrange) 
        continue;  // don't bail yet, iterate through them all

      rv = nsrange->IsPointInRange(newSelectionParent, newSelectionOffset, &cursorIsInSelection);
      if (cursorIsInSelection)
        break;
    }

    // Source doc is null if source is *not* the current editor document
    // Current doc is destination (set earlier)
    nsCOMPtr<nsIDOMDocument> srcdomdoc;
    rv = dragSession->GetSourceDocument(getter_AddRefs(srcdomdoc));
    NS_ENSURE_SUCCESS(rv, rv);

    if (cursorIsInSelection)
    {
      // Dragging within same doc can't drop on itself -- leave!
      if (srcdomdoc == destdomdoc)
        return NS_OK;

      // Dragging from another window onto a selection
      // XXX Decision made to NOT do this,
      //     note that 4.x does replace if dropped on
      //deleteSelection = PR_TRUE;
    }
    else 
    {
      // We are NOT over the selection
      if (srcdomdoc == destdomdoc)
      {
        // Within the same doc: delete if user doesn't want to copy
        PRUint32 action;
        dragSession->GetDragAction(&action);
        deleteSelection = !(action & nsIDragService::DRAGDROP_ACTION_COPY);
      }
      else
      {
        // Different source doc: Don't delete
        deleteSelection = PR_FALSE;
      }
    }
  }

  nsCOMPtr<nsIContent> newSelectionContent =
    do_QueryInterface(newSelectionParent);
  nsIContent *content = newSelectionContent;

  while (content) {
    nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(content));

    if (formControl && !formControl->AllowDrop()) {
      // Don't allow dropping into a form control that doesn't allow being
      // dropped into.

      return NS_OK;
    }

    content = content->GetParent();
  }

  PRUint32 i; 
  for (i = 0; i < numItems; ++i)
  {
    rv = dragSession->GetData(trans, i);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(trans, NS_OK); // NS_ERROR_FAILURE; Should we fail?

    // Beware! This may flush notifications via synchronous
    // ScrollSelectionIntoView.
    rv = InsertTextFromTransferable(trans, newSelectionParent, newSelectionOffset, deleteSelection);
  }

  return rv;
}

NS_IMETHODIMP nsPlaintextEditor::CanDrag(nsIDOMEvent *aDragEvent, bool *aCanDrag)
{
  NS_ENSURE_TRUE(aCanDrag, NS_ERROR_NULL_POINTER);
  /* we really should be checking the XY coordinates of the mouseevent and ensure that
   * that particular point is actually within the selection (not just that there is a selection)
   */
  *aCanDrag = PR_FALSE;
 
  // KLUDGE to work around bug 50703
  // After double click and object property editing, 
  //  we get a spurious drag event
  if (mIgnoreSpuriousDragEvent)
  {
    mIgnoreSpuriousDragEvent = PR_FALSE;
    return NS_OK;
  }
   
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
    
  bool isCollapsed;
  res = selection->GetIsCollapsed(&isCollapsed);
  NS_ENSURE_SUCCESS(res, res);
  
  // if we are collapsed, we have no selection so nothing to drag
  if ( isCollapsed )
    return NS_OK;

  nsCOMPtr<nsIDOMEventTarget> eventTarget;

  nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aDragEvent));
  if (nsevent) {
    res = nsevent->GetTmpRealOriginalTarget(getter_AddRefs(eventTarget));
    NS_ENSURE_SUCCESS(res, res);
  }

  if (eventTarget)
  {
    nsCOMPtr<nsIDOMNode> eventTargetDomNode = do_QueryInterface(eventTarget);
    if ( eventTargetDomNode )
    {
      bool isTargetedCorrectly = false;
      res = selection->ContainsNode(eventTargetDomNode, PR_FALSE, &isTargetedCorrectly);
      NS_ENSURE_SUCCESS(res, res);

      *aCanDrag = isTargetedCorrectly;
    }
  }

  return res;
}

NS_IMETHODIMP nsPlaintextEditor::DoDrag(nsIDOMEvent *aDragEvent)
{
  nsresult rv;

  nsCOMPtr<nsITransferable> trans;
  rv = PutDragDataInTransferable(getter_AddRefs(trans));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(trans, NS_OK); // maybe there was nothing to copy?

 /* get the drag service */
  nsCOMPtr<nsIDragService> dragService = 
           do_GetService("@mozilla.org/widget/dragservice;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  /* create an array of transferables */
  nsCOMPtr<nsISupportsArray> transferableArray;
  NS_NewISupportsArray(getter_AddRefs(transferableArray));
  NS_ENSURE_TRUE(transferableArray, NS_ERROR_OUT_OF_MEMORY);

  /* add the transferable to the array */
  rv = transferableArray->AppendElement(trans);
  NS_ENSURE_SUCCESS(rv, rv);

  // check our transferable hooks (if any)
  nsCOMPtr<nsIDOMDocument> domdoc;
  GetDocument(getter_AddRefs(domdoc));

  /* invoke drag */
  nsCOMPtr<nsIDOMEventTarget> eventTarget;
  rv = aDragEvent->GetTarget(getter_AddRefs(eventTarget));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDOMNode> domnode = do_QueryInterface(eventTarget);

  nsCOMPtr<nsIScriptableRegion> selRegion;
  nsCOMPtr<nsISelection> selection;
  rv = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  unsigned int flags;
  // in some cases we'll want to cut rather than copy... hmmmmm...
  flags = nsIDragService::DRAGDROP_ACTION_COPY + nsIDragService::DRAGDROP_ACTION_MOVE;

  nsCOMPtr<nsIDOMDragEvent> dragEvent(do_QueryInterface(aDragEvent));
  rv = dragService->InvokeDragSessionWithSelection(selection, transferableArray,
                                                   flags, dragEvent, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  aDragEvent->StopPropagation();
  aDragEvent->PreventDefault();

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
      nsCOMPtr<nsIDOMDocument> domdoc;
      GetDocument(getter_AddRefs(domdoc));
      if (!nsEditorHookUtils::DoInsertionHook(domdoc, nsnull, trans))
        return NS_OK;

      // Beware! This may flush notifications via synchronous
      // ScrollSelectionIntoView.
      rv = InsertTextFromTransferable(trans, nsnull, nsnull, PR_TRUE);
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
  nsCOMPtr<nsIDOMDocument> domdoc;
  GetDocument(getter_AddRefs(domdoc));
  if (!nsEditorHookUtils::DoInsertionHook(domdoc, nsnull, aTransferable))
    return NS_OK;

  // Beware! This may flush notifications via synchronous
  // ScrollSelectionIntoView.
  return InsertTextFromTransferable(aTransferable, nsnull, nsnull, PR_TRUE);
}

NS_IMETHODIMP nsPlaintextEditor::CanPaste(PRInt32 aSelectionType, bool *aCanPaste)
{
  NS_ENSURE_ARG_POINTER(aCanPaste);
  *aCanPaste = PR_FALSE;

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
                                         NS_ARRAY_LENGTH(textEditorFlavors),
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
    *aCanPaste = PR_FALSE;
    return NS_OK;
  }

  // If |aTransferable| is null, assume that a paste will succeed.
  if (!aTransferable) {
    *aCanPaste = PR_TRUE;
    return NS_OK;
  }

  nsCOMPtr<nsISupports> data;
  PRUint32 dataLen;
  nsresult rv = aTransferable->GetTransferData(kUnicodeMime,
                                               getter_AddRefs(data),
                                               &dataLen);
  if (NS_SUCCEEDED(rv) && data)
    *aCanPaste = PR_TRUE;
  else
    *aCanPaste = PR_FALSE;
  
  return NS_OK;
}


nsresult
nsPlaintextEditor::SetupDocEncoder(nsIDocumentEncoder **aDocEncoder)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  nsresult rv = GetDocument(getter_AddRefs(domDoc));
  NS_ENSURE_SUCCESS(rv, rv);

  // find out if we're a plaintext control or not
  // get correct mimeType and document encoder flags set
  nsAutoString mimeType;
  PRUint32 docEncoderFlags = 0;
  if (IsPlaintextEditor())
  {
    docEncoderFlags |= nsIDocumentEncoder::OutputBodyOnly | nsIDocumentEncoder::OutputPreformatted;
    mimeType.AssignLiteral(kUnicodeMime);
  }
  else
    mimeType.AssignLiteral(kHTMLMime);

  // set up docEncoder
  nsCOMPtr<nsIDocumentEncoder> encoder = do_CreateInstance(NS_HTMLCOPY_ENCODER_CONTRACTID);
  NS_ENSURE_TRUE(encoder, NS_ERROR_OUT_OF_MEMORY);

  rv = encoder->Init(domDoc, mimeType, docEncoderFlags);
  NS_ENSURE_SUCCESS(rv, rv);
    
  /* get the selection to be dragged */
  nsCOMPtr<nsISelection> selection;
  rv = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = encoder->SetSelection(selection);
  NS_ENSURE_SUCCESS(rv, rv);

  *aDocEncoder = encoder;
  NS_ADDREF(*aDocEncoder);
  return NS_OK;
}

nsresult
nsPlaintextEditor::PutDragDataInTransferable(nsITransferable **aTransferable)
{
  *aTransferable = nsnull;
  nsCOMPtr<nsIDocumentEncoder> docEncoder;
  nsresult rv = SetupDocEncoder(getter_AddRefs(docEncoder));
  NS_ENSURE_SUCCESS(rv, rv);

  // grab a string
  nsAutoString buffer;
  rv = docEncoder->EncodeToString(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  // if we have an empty string, we're done; otherwise continue
  if (buffer.IsEmpty())
    return NS_OK;

  nsCOMPtr<nsISupportsString> dataWrapper =
                        do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dataWrapper->SetData(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  /* create html flavor transferable */
  nsCOMPtr<nsITransferable> trans = do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // find out if we're a plaintext control or not
  if (IsPlaintextEditor())
  {
    // Add the unicode flavor to the transferable
    rv = trans->AddDataFlavor(kUnicodeMime);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else
  {
    rv = trans->AddDataFlavor(kHTMLMime);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFormatConverter> htmlConverter = do_CreateInstance("@mozilla.org/widget/htmlformatconverter;1");
    NS_ENSURE_TRUE(htmlConverter, NS_ERROR_FAILURE);

    rv = trans->SetConverter(htmlConverter);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // QI the data object an |nsISupports| so that when the transferable holds
  // onto it, it will addref the correct interface.
  nsCOMPtr<nsISupports> nsisupportsDataWrapper = do_QueryInterface(dataWrapper);
  rv = trans->SetTransferData(IsPlaintextEditor() ? kUnicodeMime : kHTMLMime,
                   nsisupportsDataWrapper, buffer.Length() * sizeof(PRUnichar));
  NS_ENSURE_SUCCESS(rv, rv);

  *aTransferable = trans;
  NS_ADDREF(*aTransferable);
  return NS_OK;
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */
#include "nsICaret.h"


#include "nsPlaintextEditor.h"
#include "nsHTMLEditUtils.h"

#include "nsEditorEventListeners.h"

#include "nsIDOMText.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDocument.h"
#include "nsIDOMEventReceiver.h" 
#include "nsIDOMKeyEvent.h"
#include "nsIDOMKeyListener.h" 
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseEvent.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsISelectionController.h"

#include "nsIFrameSelection.h"  // For TABLESELECTION_ defines
#include "nsIIndependentSelection.h" //domselections answer to frameselection


#include "nsICSSLoader.h"
#include "nsICSSStyleSheet.h"
#include "nsIHTMLContentContainer.h"
#include "nsIStyleSet.h"
#include "nsIDocumentObserver.h"
#include "nsIDocumentStateListener.h"

#include "nsIStyleContext.h"

#include "nsIEnumerator.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsEditorCID.h"
#include "nsLayoutCID.h"
#include "nsIDOMRange.h"
#include "nsIDOMNSRange.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsFileSpec.h"
#include "nsIFile.h"
#include "nsIURL.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsIDocumentEncoder.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsIImage.h"
#include "nsAOLCiter.h"
#include "nsInternetCiter.h"
#include "nsISupportsPrimitives.h"
#include "InsertTextTxn.h"

// netwerk
#include "nsIURI.h"
#include "nsNetUtil.h"

// Drag & Drop, Clipboard
#include "nsWidgetsCID.h"
#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsIDragService.h"
#include "nsIDOMNSUIEvent.h"

// Transactionas
#include "PlaceholderTxn.h"
#include "nsStyleSheetTxns.h"

// Misc
#include "TextEditorTest.h"
#include "nsEditorUtils.h"
#include "nsIPref.h"
const PRUnichar nbsp = 160;

// HACK - CID for NS_CTRANSITIONAL_DTD_CID so that we can get at transitional dtd
#define NS_CTRANSITIONAL_DTD_CID \
{ 0x4611d482, 0x960a, 0x11d4, { 0x8e, 0xb0, 0xb6, 0x17, 0x66, 0x1b, 0x6f, 0x7c } }


static NS_DEFINE_CID(kHTMLEditorCID,  NS_HTMLEDITOR_CID);
static NS_DEFINE_CID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_IID(kSubtreeIteratorCID, NS_SUBTREEITERATOR_CID);
static NS_DEFINE_CID(kCRangeCID,      NS_RANGE_CID);
static NS_DEFINE_CID(kCDOMSelectionCID,      NS_DOMSELECTION_CID);
static NS_DEFINE_IID(kFileWidgetCID,  NS_FILEWIDGET_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID); 
static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID); 
static NS_DEFINE_CID(kCTransitionalDTDCID,  NS_CTRANSITIONAL_DTD_CID);

// Drag & Drop, Clipboard Support
static NS_DEFINE_CID(kCClipboardCID,    NS_CLIPBOARD_CID);
static NS_DEFINE_CID(kCTransferableCID, NS_TRANSFERABLE_CID);
static NS_DEFINE_CID(kCDragServiceCID, NS_DRAGSERVICE_CID);
static NS_DEFINE_CID(kCHTMLFormatConverterCID, NS_HTMLFORMATCONVERTER_CID);
// private clipboard data flavors for html copy/paste
#define kHTMLContext   "text/_moz_htmlcontext"
#define kHTMLInfo      "text/_moz_htmlinfo"


#if defined(NS_DEBUG) && defined(DEBUG_buster)
static PRBool gNoisy = PR_FALSE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif

NS_IMETHODIMP nsPlaintextEditor::PrepareTransferable(nsITransferable **transferable)
{
  // Create generic Transferable for getting the data
  nsresult rv = nsComponentManager::CreateInstance(kCTransferableCID, nsnull, 
                                          NS_GET_IID(nsITransferable), 
                                          (void**)transferable);
  if (NS_FAILED(rv))
    return rv;

  // Get the nsITransferable interface for getting the data from the clipboard
  if (transferable) (*transferable)->AddDataFlavor(kUnicodeMime);
  return NS_OK;
}

NS_IMETHODIMP nsPlaintextEditor::InsertTextFromTransferable(nsITransferable *transferable)
{
  nsresult rv = NS_OK;
  char* bestFlavor = nsnull;
  nsCOMPtr<nsISupports> genericDataObj;
  PRUint32 len = 0;
  if ( NS_SUCCEEDED(transferable->GetAnyTransferData(&bestFlavor, getter_AddRefs(genericDataObj), &len)) )
  {
    nsAutoTxnsConserveSelection dontSpazMySelection(this);
    nsAutoString flavor, stuffToPaste;
    flavor.AssignWithConversion( bestFlavor );   // just so we can use flavor.Equals()
    if (flavor.EqualsWithConversion(kUnicodeMime))
    {
      nsCOMPtr<nsISupportsWString> textDataObj ( do_QueryInterface(genericDataObj) );
      if (textDataObj && len > 0)
      {
        PRUnichar* text = nsnull;
        textDataObj->ToString ( &text );
        stuffToPaste.Assign ( text, len / 2 );
        nsAutoEditBatch beginBatching(this);
        rv = InsertText(stuffToPaste.GetUnicode());
        if (text)
          nsMemory::Free(text);
      }
    }
  }
  nsCRT::free(bestFlavor);
      
  // Try to scroll the selection into view if the paste/drop succeeded
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsISelectionController> selCon;
    if (NS_SUCCEEDED(GetSelectionController(getter_AddRefs(selCon))) && selCon)
      selCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL, nsISelectionController::SELECTION_FOCUS_REGION);
  }

  return rv;
}

NS_IMETHODIMP nsPlaintextEditor::InsertFromDrop(nsIDOMEvent* aDropEvent)
{
  ForceCompositionEnd();
  
  nsresult rv;
  NS_WITH_SERVICE(nsIDragService, dragService, "@mozilla.org/widget/dragservice;1", &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDragSession> dragSession(do_QueryInterface(dragService));
  
  if (!dragSession) return NS_OK;

  // Get the nsITransferable interface for getting the data from the drop
  nsCOMPtr<nsITransferable> trans;
  rv = PrepareTransferable(getter_AddRefs(trans));
  if (NS_FAILED(rv)) return rv;
  if (!trans) return NS_OK;  // NS_ERROR_FAILURE; SHOULD WE FAIL?

  PRUint32 numItems = 0; 
  rv = dragSession->GetNumDropItems(&numItems);
  if (NS_FAILED(rv)) return rv;

  // Combine any deletion and drop insertion into one transaction
  nsAutoEditBatch beginBatching(this);

  PRUint32 i; 
  PRBool doPlaceCaret = PR_TRUE;
  for (i = 0; i < numItems; ++i)
  {
    rv = dragSession->GetData(trans, i);
    if (NS_FAILED(rv)) return rv;
    if (!trans) return NS_OK; // NS_ERROR_FAILURE; Should we fail?

    if ( doPlaceCaret )
    {
      // check if the user pressed the key to force a copy rather than a move
      // if we run into problems here, we'll just assume the user doesn't want a copy
      PRBool userWantsCopy = PR_FALSE;

      nsCOMPtr<nsIDOMNSUIEvent> nsuiEvent (do_QueryInterface(aDropEvent));
      if (!nsuiEvent) return NS_ERROR_FAILURE;

      nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aDropEvent) );
      if (mouseEvent)

#ifdef XP_MAC
        mouseEvent->GetAltKey(&userWantsCopy);
#else
        mouseEvent->GetCtrlKey(&userWantsCopy);
#endif
      // Source doc is null if source is *not* the current editor document
      nsCOMPtr<nsIDOMDocument> srcdomdoc;
      rv = dragSession->GetSourceDocument(getter_AddRefs(srcdomdoc));
      if (NS_FAILED(rv)) return rv;

      // Current doc is destination
      nsCOMPtr<nsIDOMDocument>destdomdoc; 
      rv = GetDocument(getter_AddRefs(destdomdoc)); 
      if (NS_FAILED(rv)) return rv;

      nsCOMPtr<nsISelection> selection;
      rv = GetSelection(getter_AddRefs(selection));
      if (NS_FAILED(rv)) return rv;
      if (!selection) return NS_ERROR_FAILURE;

      PRBool isCollapsed;
      rv = selection->GetIsCollapsed(&isCollapsed);
      if (NS_FAILED(rv)) return rv;
      
      // Parent and offset under the mouse cursor
      nsCOMPtr<nsIDOMNode> newSelectionParent;
      PRInt32 newSelectionOffset = 0;
      rv = nsuiEvent->GetRangeParent(getter_AddRefs(newSelectionParent));
      if (NS_FAILED(rv)) return rv;
      if (!newSelectionParent) return NS_ERROR_FAILURE;

      rv = nsuiEvent->GetRangeOffset(&newSelectionOffset);
      if (NS_FAILED(rv)) return rv;
      /* Creating a range to store insert position because when
         we delete the selection, range gravity will make sure the insertion
         point is in the correct place */
      nsCOMPtr<nsIDOMRange> destinationRange;
      rv = CreateRange(newSelectionParent, newSelectionOffset,newSelectionParent, newSelectionOffset, getter_AddRefs(destinationRange));
      if (NS_FAILED(rv))
        return rv;
      if(!destinationRange)
        return NS_ERROR_FAILURE;

      // We never have to delete if selection is already collapsed
      PRBool deleteSelection = PR_FALSE;
      PRBool cursorIsInSelection = PR_FALSE;

      // Check if mouse is in the selection
      if (!isCollapsed)
      {
        PRInt32 rangeCount;
        rv = selection->GetRangeCount(&rangeCount);
        if (NS_FAILED(rv)) 
          return rv?rv:NS_ERROR_FAILURE;

        for (PRInt32 j = 0; j < rangeCount; j++)
        {
          nsCOMPtr<nsIDOMRange> range;

          rv = selection->GetRangeAt(j, getter_AddRefs(range));
          if (NS_FAILED(rv) || !range) 
            continue;//dont bail yet, iterate through them all

          nsCOMPtr<nsIDOMNSRange> nsrange(do_QueryInterface(range));
          if (NS_FAILED(rv) || !nsrange) 
            continue;//dont bail yet, iterate through them all

          rv = nsrange->IsPointInRange(newSelectionParent, newSelectionOffset, &cursorIsInSelection);
          if(cursorIsInSelection)
            break;
        }
        if (cursorIsInSelection)
        {
          // Dragging within same doc can't drop on itself -- leave!
          // (We shouldn't get here - drag event shouldn't have started if over selection)
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
            deleteSelection = !userWantsCopy;
          }
          else
          {
            // Different source doc: Don't delete
            deleteSelection = PR_FALSE;
          }
        }
      }

      if (deleteSelection)
      {
        rv = DeleteSelection(eNone);
        if (NS_FAILED(rv)) return rv;
      }

      // If we deleted the selection because we dropped from another doc,
      //  then we don't have to relocate the caret (insert at the deletion point)
      if (!(deleteSelection && srcdomdoc != destdomdoc))
      {
        // Move the selection to the point under the mouse cursor
        rv = destinationRange->GetStartContainer(getter_AddRefs(newSelectionParent));
        if (NS_FAILED(rv))
          return rv;
        if(!newSelectionParent)
          return NS_ERROR_FAILURE;
       
        rv = destinationRange->GetStartOffset(&newSelectionOffset);
        if (NS_FAILED(rv))
          return rv;
        selection->Collapse(newSelectionParent, newSelectionOffset);
      }      
      // We have to figure out whether to delete and relocate caret only once
      doPlaceCaret = PR_FALSE;
    }
    
    rv = InsertTextFromTransferable(trans);
  }

  return rv;
}

NS_IMETHODIMP nsPlaintextEditor::CanDrag(nsIDOMEvent *aDragEvent, PRBool &aCanDrag)
{
  /* we really should be checking the XY coordinates of the mouseevent and ensure that
   * that particular point is actually within the selection (not just that there is a selection)
   */
  aCanDrag = PR_FALSE;
 
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
  if (NS_FAILED(res)) return res;
    
  PRBool isCollapsed;
  res = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;
  
  // if we are collapsed, we have no selection so nothing to drag
  if ( isCollapsed )
    return NS_OK;

  nsCOMPtr<nsIDOMEventTarget> eventTarget;
  res = aDragEvent->GetOriginalTarget(getter_AddRefs(eventTarget));
  if (NS_FAILED(res)) return res;
  if ( eventTarget )
  {
    nsCOMPtr<nsIDOMNode> eventTargetDomNode = do_QueryInterface(eventTarget);
    if ( eventTargetDomNode )
    {
      PRBool amTargettedCorrectly = PR_FALSE;
      res = selection->ContainsNode(eventTargetDomNode, PR_FALSE, &amTargettedCorrectly);
      if (NS_FAILED(res)) return res;

    	aCanDrag = amTargettedCorrectly;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsPlaintextEditor::DoDrag(nsIDOMEvent *aDragEvent)
{
  nsresult rv;

  nsCOMPtr<nsIDOMEventTarget> eventTarget;
  rv = aDragEvent->GetTarget(getter_AddRefs(eventTarget));
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIDOMNode> domnode = do_QueryInterface(eventTarget);

  /* get the selection to be dragged */
  nsCOMPtr<nsISelection> selection;
  rv = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(rv)) return rv;

  /* create an array of transferables */
  nsCOMPtr<nsISupportsArray> transferableArray;
  NS_NewISupportsArray(getter_AddRefs(transferableArray));
  if (transferableArray == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  /* get the drag service */
  NS_WITH_SERVICE(nsIDragService, dragService, "@mozilla.org/widget/dragservice;1", &rv);
  if (NS_FAILED(rv)) return rv;

  /* create html flavor transferable */
  nsCOMPtr<nsITransferable> trans;
  rv = nsComponentManager::CreateInstance(kCTransferableCID, nsnull, 
                                        NS_GET_IID(nsITransferable), 
                                        getter_AddRefs(trans));
  if (NS_FAILED(rv)) return rv;
  if ( !trans ) return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIDOMDocument> domdoc;
  rv = GetDocument(getter_AddRefs(domdoc));
  if (NS_FAILED(rv)) return rv;
	
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
  if (doc)
  {
    nsCOMPtr<nsIDocumentEncoder> docEncoder;

    docEncoder = do_CreateInstance(NS_DOC_ENCODER_CONTRACTID_BASE "text/html");
    NS_ENSURE_TRUE(docEncoder, NS_ERROR_FAILURE);

    docEncoder->Init(doc, NS_LITERAL_STRING("text/html"), 0);
    docEncoder->SetSelection(selection);

    nsAutoString buffer;

    rv = docEncoder->EncodeToString(buffer);
  
    if (NS_FAILED(rv))
      return rv;

    if ( !buffer.IsEmpty() )
    {
      nsCOMPtr<nsIFormatConverter> htmlConverter;
      rv = nsComponentManager::CreateInstance(kCHTMLFormatConverterCID, nsnull, NS_GET_IID(nsIFormatConverter),
                                              getter_AddRefs(htmlConverter));
      if (NS_FAILED(rv)) return rv;
      if (!htmlConverter) return NS_ERROR_OUT_OF_MEMORY;

      nsCOMPtr<nsISupportsWString> dataWrapper;
      rv = nsComponentManager::CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID, nsnull,
                                              NS_GET_IID(nsISupportsWString), getter_AddRefs(dataWrapper));
      if (NS_FAILED(rv)) return rv;
      if ( !dataWrapper ) return NS_ERROR_OUT_OF_MEMORY;

      rv = trans->AddDataFlavor(kHTMLMime);
      if (NS_FAILED(rv)) return rv;
      rv = trans->SetConverter(htmlConverter);
      if (NS_FAILED(rv)) return rv;

      rv = dataWrapper->SetData( NS_CONST_CAST(PRUnichar*, buffer.GetUnicode()) );
      if (NS_FAILED(rv)) return rv;

      // QI the data object an |nsISupports| so that when the transferable holds
      // onto it, it will addref the correct interface.
      nsCOMPtr<nsISupports> nsisupportsDataWrapper ( do_QueryInterface(dataWrapper) );
      rv = trans->SetTransferData(kHTMLMime, nsisupportsDataWrapper, buffer.Length() * 2);
      if (NS_FAILED(rv)) return rv;

      /* add the transferable to the array */
      rv = transferableArray->AppendElement(trans);
      if (NS_FAILED(rv)) return rv;

      /* invoke drag */
      unsigned int flags;
      // in some cases we'll want to cut rather than copy... hmmmmm...
      // if ( wantToCut )
      //   flags = nsIDragService.DRAGDROP_ACTION_COPY + nsIDragService.DRAGDROP_ACTION_MOVE;
      // else
        flags = nsIDragService::DRAGDROP_ACTION_COPY + nsIDragService::DRAGDROP_ACTION_MOVE;
      
      rv = dragService->InvokeDragSession( domnode, transferableArray, nsnull, flags);
      if (NS_FAILED(rv)) return rv;

      aDragEvent->PreventBubble();
    }
  }

  return rv;
}

NS_IMETHODIMP nsPlaintextEditor::Paste(PRInt32 aSelectionType)
{
  ForceCompositionEnd();

  // Get Clipboard Service
  nsresult rv;
  NS_WITH_SERVICE ( nsIClipboard, clipboard, kCClipboardCID, &rv );
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
      rv = InsertTextFromTransferable(trans);
    }
  }

  return rv;
}


NS_IMETHODIMP nsPlaintextEditor::CanPaste(PRInt32 aSelectionType, PRBool &aCanPaste)
{
  aCanPaste = PR_FALSE;
  
  // can't paste if readonly
  if (!IsModifiable())
    return NS_OK;

  nsresult rv;
  NS_WITH_SERVICE(nsIClipboard, clipboard, kCClipboardCID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  // the flavors that we can deal with
  char* textEditorFlavors[] = { kUnicodeMime, nsnull };

  nsCOMPtr<nsISupportsArray> flavorsList;
  rv = nsComponentManager::CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, nsnull, 
         NS_GET_IID(nsISupportsArray), getter_AddRefs(flavorsList));
  if (NS_FAILED(rv)) return rv;
  
  PRUint32 editorFlags;
  GetFlags(&editorFlags);
  
  // add the flavors for text editors
  for (char** flavor = textEditorFlavors; *flavor; flavor++)
  {
    nsCOMPtr<nsISupportsString> flavorString;            
    nsComponentManager::CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, nsnull, 
         NS_GET_IID(nsISupportsString), getter_AddRefs(flavorString));
    if (flavorString)
    {
      flavorString->SetData(*flavor);
      flavorsList->AppendElement(flavorString);
    }
  }
  
  PRBool haveFlavors;
  rv = clipboard->HasDataMatchingFlavors(flavorsList, aSelectionType, &haveFlavors);
  if (NS_FAILED(rv)) return rv;
  
  aCanPaste = haveFlavors;
  return NS_OK;
}

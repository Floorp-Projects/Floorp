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


#include "nsHTMLEditor.h"
#include "nsHTMLEditRules.h"
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
static nsCOMPtr<nsIDOMNode> GetListParent(nsIDOMNode* aNode)
{
  if (!aNode) return nsnull;
  nsCOMPtr<nsIDOMNode> parent, tmp;
  aNode->GetParentNode(getter_AddRefs(parent));
  while (parent)
  {
    if (nsHTMLEditUtils::IsList(parent)) return parent;
    parent->GetParentNode(getter_AddRefs(tmp));
    parent = tmp;
  }
  return nsnull;
}

static nsCOMPtr<nsIDOMNode> GetTableParent(nsIDOMNode* aNode)
{
  if (!aNode) return nsnull;
  nsCOMPtr<nsIDOMNode> parent, tmp;
  aNode->GetParentNode(getter_AddRefs(parent));
  while (parent)
  {
    if (nsHTMLEditUtils::IsTable(parent)) return parent;
    parent->GetParentNode(getter_AddRefs(tmp));
    parent = tmp;
  }
  return nsnull;
}


NS_IMETHODIMP nsHTMLEditor::InsertHTML(const nsString& aInputString)
{
  nsAutoString charset;
  return InsertHTMLWithCharset(aInputString, charset);
}

nsresult nsHTMLEditor::InsertHTMLWithContext(const nsString& aInputString, const nsString& aContextStr, const nsString& aInfoStr)
{
  nsAutoString charset;
  return InsertHTMLWithCharsetAndContext(aInputString, charset, aContextStr, aInfoStr);
}


NS_IMETHODIMP nsHTMLEditor::InsertHTMLWithCharset(const nsString& aInputString, const nsString& aCharset)
{
  return InsertHTMLWithCharsetAndContext(aInputString, aCharset, nsAutoString(), nsAutoString());
}


nsresult nsHTMLEditor::InsertHTMLWithCharsetAndContext(const nsString& aInputString,
                                                       const nsString& aCharset,
                                                       const nsString& aContextStr,
                                                       const nsString& aInfoStr)
{
  if (!mRules) return NS_ERROR_NOT_INITIALIZED;

  // First, make sure there are no return chars in the document.
  // Bad things happen if you insert returns (instead of dom newlines, \n)
  // into an editor document.
  nsAutoString inputString (aInputString);  // hope this does copy-on-write

  // Windows linebreaks: Map CRLF to LF:
  inputString.ReplaceSubstring(NS_ConvertASCIItoUCS2("\r\n"),
                               NS_ConvertASCIItoUCS2("\n"));
 
  // Mac linebreaks: Map any remaining CR to LF:
  inputString.ReplaceSubstring(NS_ConvertASCIItoUCS2("\r"),
                               NS_ConvertASCIItoUCS2("\n"));

  // force IME commit; set up rules sniffing and batching
  ForceCompositionEnd();
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpHTMLPaste, nsIEditor::eNext);
  
  // Get selection
  nsresult res;
  nsCOMPtr<nsISelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  
  // Get the first range in the selection, for context:
  nsCOMPtr<nsIDOMRange> range;
  res = selection->GetRangeAt(0, getter_AddRefs(range));
  if (NS_FAILED(res))
    return res;

  nsCOMPtr<nsIDOMNSRange> nsrange (do_QueryInterface(range));
  if (!nsrange)
    return NS_ERROR_NO_INTERFACE;

  // create a dom document fragment that represents the structure to paste
  nsCOMPtr<nsIDOMNode> fragmentAsNode;
  PRInt32 rangeStartHint, rangeEndHint;
  res = CreateDOMFragmentFromPaste(nsrange, inputString, aContextStr, aInfoStr, 
                                            address_of(fragmentAsNode),
                                            &rangeStartHint, &rangeEndHint);
  NS_ENSURE_SUCCESS(res, res);


  // make a list of what nodes in docFrag we need to move
  nsCOMPtr<nsISupportsArray> nodeList;
  res = CreateListOfNodesToPaste(fragmentAsNode, address_of(nodeList), rangeStartHint, rangeEndHint);
  NS_ENSURE_SUCCESS(res, res);
  
  // are there any table elements in the list?  
  // node and offset for insertion
  nsCOMPtr<nsIDOMNode> parentNode;
  PRInt32 offsetOfNewNode;
  
  // check for table cell selection mode
  PRBool cellSelectionMode = PR_FALSE;
  nsCOMPtr<nsIDOMElement> cell;
  res = GetFirstSelectedCell(getter_AddRefs(cell), nsnull);
  if (NS_SUCCEEDED(res) && cell)
  {
    cellSelectionMode = PR_TRUE;
  }
  
  if (cellSelectionMode)
  {
    // do we have table content to paste?  If so, we want to delete
    // the selected table cells and replace with new table elements;
    // but if not we want to delete _contents_ of cells and replace
    // with non-table elements.  Use cellSelectionMode bool to 
    // indicate results.
    nsCOMPtr<nsISupports> isupports = nodeList->ElementAt(0);
    nsCOMPtr<nsIDOMNode> firstNode( do_QueryInterface(isupports) );
    if (!nsHTMLEditUtils::IsTableElement(firstNode))
      cellSelectionMode = PR_FALSE;
  }

  if (!cellSelectionMode)
  {
    res = DeleteSelectionAndPrepareToCreateNode(parentNode, offsetOfNewNode);
    NS_ENSURE_SUCCESS(res, res);

    // pasting does not inherit local inline styles
    res = RemoveAllInlineProperties();
    NS_ENSURE_SUCCESS(res, res);
  }
  else
  {
    // delete whole cells: we will replace with new table content
    if (1)
    {
      // Save current selection since DeleteTableCell perturbs it
      nsAutoSelectionReset selectionResetter(selection, this);
      res = DeleteTableCell(1);
      NS_ENSURE_SUCCESS(res, res);
    }
    // colapse selection to beginning of deleted table content
    selection->CollapseToStart();
  }
  
  // give rules a chance to handle or cancel
  nsTextRulesInfo ruleInfo(nsTextEditRules::kInsertElement);
  PRBool cancel, handled;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(res)) return res;
  if (cancel) return NS_OK; // rules canceled the operation
  if (!handled)
  {
    // The rules code (WillDoAction above) might have changed the selection.  
    // refresh our memory...
    res = GetStartNodeAndOffset(selection, address_of(parentNode), &offsetOfNewNode);
    if (!parentNode) res = NS_ERROR_FAILURE;
    if (NS_FAILED(res)) return res;
    
    // are we in a text node?  If so, split it.
    if (IsTextNode(parentNode))
    {
      nsCOMPtr<nsIDOMNode> temp;
      res = SplitNodeDeep(parentNode, parentNode, offsetOfNewNode, &offsetOfNewNode);
      if (NS_FAILED(res)) return res;
      res = parentNode->GetParentNode(getter_AddRefs(temp));
      if (NS_FAILED(res)) return res;
      parentNode = temp;
    }

    // build up list of parents of first node in lst that are either:
    // lists, or tables.  
    nsCOMPtr<nsISupports> isup = nodeList->ElementAt(0);
    nsCOMPtr<nsIDOMNode>  pNode( do_QueryInterface(isup) );
    nsCOMPtr<nsISupportsArray> listAndTableArray;
    res = NS_NewISupportsArray(getter_AddRefs(listAndTableArray));
    NS_ENSURE_SUCCESS(res, res);
    while (pNode)
    {
      if (nsHTMLEditUtils::IsList(pNode) || nsHTMLEditUtils::IsTable(pNode))
      {
        isup = do_QueryInterface(pNode);
        listAndTableArray->AppendElement(isup);
      }
      nsCOMPtr<nsIDOMNode> parent;
      pNode->GetParentNode(getter_AddRefs(parent));
      pNode = parent;
    }
    
    // remember number of lists and tables above us
    PRUint32 listAndTableParents;
    PRInt32 highWaterMark = -1;
    listAndTableArray->Count(&listAndTableParents);
    
    PRUint32 listCount, j;
    if (listAndTableParents)
    {
      // scan insertion list for table elements (other than table).  
      nodeList->Count(&listCount);
      for (j=0; j<listCount; j++)
      {
        nsCOMPtr<nsISupports> isupports = nodeList->ElementAt(j);
        nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );

        NS_ENSURE_TRUE(curNode, NS_ERROR_FAILURE);
        if (nsHTMLEditUtils::IsTableElement(curNode) && !nsHTMLEditUtils::IsTable(curNode))
        {
          nsCOMPtr<nsIDOMNode> theTable = GetTableParent(curNode);
          if (theTable)
          {
            nsCOMPtr<nsISupports> isupTable(do_QueryInterface(theTable));
            PRInt32 indexT = listAndTableArray->IndexOf(isupTable);
            if (indexT >= 0)
            {
              highWaterMark = indexT;
              if ((PRUint32)highWaterMark == listAndTableParents-1) break;
            }
            else
            {
              break;
            }
          }
        }
        if (nsHTMLEditUtils::IsListItem(curNode))
        {
          nsCOMPtr<nsIDOMNode> theList = GetListParent(curNode);
          if (theList)
          {
            nsCOMPtr<nsISupports> isupList(do_QueryInterface(theList));
            PRInt32 indexL = listAndTableArray->IndexOf(isupList);
            if (indexL >= 0)
            {
              highWaterMark = indexL;
              if ((PRUint32)highWaterMark == listAndTableParents-1) break;
            }
            else
            {
              break;
            }
          }
        }
      }
    }
    // if we have pieces of tables or lists to be inserted, let's force the paste 
    // to deal with table elements right away, so that it doesn't orphan some 
    // table or list contents outside the table or list.
    if (highWaterMark >= 0)
    {
      nsCOMPtr<nsISupports> isupports = listAndTableArray->ElementAt(highWaterMark);
      nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );
      nsCOMPtr<nsIDOMNode> replaceNode, tmp;
      if (nsHTMLEditUtils::IsTable(curNode))
      {
        // look upward from curNode for a piece of this table
        isup  = nodeList->ElementAt(0);
        pNode = do_QueryInterface(isup);
        while (pNode)
        {
          if (nsHTMLEditUtils::IsTableElement(pNode) && !nsHTMLEditUtils::IsTable(pNode))
          {
            nsCOMPtr<nsIDOMNode> tableP = GetTableParent(pNode);
            if (tableP == curNode)
            {
              replaceNode = pNode;
              break;
            }
          }
          nsCOMPtr<nsIDOMNode> parent;
          pNode->GetParentNode(getter_AddRefs(parent));
          pNode = parent;
        }
      }
      else // list case
      {
        // look upward from curNode for a piece of this list
        isup  = nodeList->ElementAt(0);
        pNode = do_QueryInterface(isup);
        while (pNode)
        {
          if (nsHTMLEditUtils::IsListItem(pNode))
          {
            nsCOMPtr<nsIDOMNode> listP = GetListParent(pNode);
            if (listP == curNode)
            {
              replaceNode = pNode;
              break;
            }
          }
          nsCOMPtr<nsIDOMNode> parent;
          pNode->GetParentNode(getter_AddRefs(parent));
          pNode = parent;
        }
      }
      
      if (replaceNode)
      {
        isupports = do_QueryInterface(replaceNode);
        nodeList->ReplaceElementAt(isupports, 0);
        // postprocess list to remove any descendants of this node
        // so that we dont insert them twice.
        do
        {
          isupports = nodeList->ElementAt(1);
          tmp = do_QueryInterface(isupports);
          if (tmp && nsHTMLEditUtils::IsDescendantOf(tmp, replaceNode))
            nodeList->RemoveElementAt(1);
          else
            break;
        } while(tmp);
      }
    }
    
    // Loop over the node list and paste the nodes:
    PRBool bDidInsert = PR_FALSE;
    nsCOMPtr<nsIDOMNode> lastInsertNode, insertedContextParent;
    nodeList->Count(&listCount);
    for (j=0; j<listCount; j++)
    {
      nsCOMPtr<nsISupports> isupports = nodeList->ElementAt(j);
      nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );

      NS_ENSURE_TRUE(curNode, NS_ERROR_FAILURE);
      NS_ENSURE_TRUE(curNode != fragmentAsNode, NS_ERROR_FAILURE);
      NS_ENSURE_TRUE(!nsHTMLEditUtils::IsBody(curNode), NS_ERROR_FAILURE);
      
      if (insertedContextParent)
      {
        // if we had to insert something higher up in the paste heirarchy, we want to 
        // skip any further paste nodes that descend from that.  Else we will paste twice.
        if (nsHTMLEditUtils::IsDescendantOf(curNode, insertedContextParent))
          continue;
      }
      
      // give the user a hand on table element insertion.  if they have
      // a table or table row on the clipboard, and are trying to insert
      // into a table or table row, insert the appropriate children instead.
      if (  (nsHTMLEditUtils::IsTableRow(curNode) && nsHTMLEditUtils::IsTableRow(parentNode))
         && (nsHTMLEditUtils::IsTable(curNode)    || nsHTMLEditUtils::IsTable(parentNode)) )
      {
        nsCOMPtr<nsIDOMNode> child;
        curNode->GetFirstChild(getter_AddRefs(child));
        while (child)
        {
          InsertNodeAtPoint(child, parentNode, offsetOfNewNode, PR_TRUE);
          if (NS_SUCCEEDED(res)) 
          {
            bDidInsert = PR_TRUE;
            lastInsertNode = curNode;
            offsetOfNewNode++;
          }
          curNode->GetFirstChild(getter_AddRefs(child));
        }
      }
      else
      {
        // try to insert
        res = InsertNodeAtPoint(curNode, parentNode, offsetOfNewNode, PR_TRUE);
        if (NS_SUCCEEDED(res)) 
        {
          bDidInsert = PR_TRUE;
          lastInsertNode = curNode;
        }
          
        // assume failure means no legal parent in the document heirarchy.
        // try again with the parent of curNode in the paste heirarchy.
        nsCOMPtr<nsIDOMNode> parent;
        while (NS_FAILED(res) && curNode)
        {
          curNode->GetParentNode(getter_AddRefs(parent));
          if (parent && !nsHTMLEditUtils::IsBody(parent))
          {
            res = InsertNodeAtPoint(parent, parentNode, offsetOfNewNode, PR_TRUE);
            if (NS_SUCCEEDED(res)) 
            {
              bDidInsert = PR_TRUE;
              insertedContextParent = parent;
              lastInsertNode = parent;
            }
          }
          curNode = parent;
        }
      }
      if (bDidInsert)
      {
        res = GetNodeLocation(lastInsertNode, address_of(parentNode), &offsetOfNewNode);
        NS_ENSURE_SUCCESS(res, res);
        offsetOfNewNode++;
      }
    }

    // Now collapse the selection to the end of what we just inserted:
    if (lastInsertNode) 
    {
      res = GetNodeLocation(lastInsertNode, address_of(parentNode), &offsetOfNewNode);
      NS_ENSURE_SUCCESS(res, res);
      selection->Collapse(parentNode, offsetOfNewNode+1);
    }
  }
  
  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

nsresult
nsHTMLEditor::StripFormattingNodes(nsIDOMNode *aNode)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  nsresult res = NS_OK;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (IsEmptyTextContent(content))
  {
    nsCOMPtr<nsIDOMNode> parent, ignored;
    aNode->GetParentNode(getter_AddRefs(parent));
    if (parent)
    {
      res = parent->RemoveChild(aNode, getter_AddRefs(ignored));
      return res;
    }
  }
  
  if (!nsHTMLEditUtils::IsPre(aNode))
  {
    nsCOMPtr<nsIDOMNode> child;
    aNode->GetLastChild(getter_AddRefs(child));
  
    while (child)
    {
      nsCOMPtr<nsIDOMNode> tmp;
      child->GetPreviousSibling(getter_AddRefs(tmp));
      res = StripFormattingNodes(child);
      NS_ENSURE_SUCCESS(res, res);
      child = tmp;
    }
  }
  return res;
}

NS_IMETHODIMP nsHTMLEditor::PrepareTransferable(nsITransferable **transferable)
{
  // Create generic Transferable for getting the data
  nsresult rv = nsComponentManager::CreateInstance(kCTransferableCID, nsnull, 
                                          NS_GET_IID(nsITransferable), 
                                          (void**)transferable);
  if (NS_FAILED(rv))
    return rv;

  // Get the nsITransferable interface for getting the data from the clipboard
  if (transferable)
  {
    // Create the desired DataFlavor for the type of data
    // we want to get out of the transferable
    if ((mFlags & eEditorPlaintextMask) == 0)  // This should only happen in html editors, not plaintext
    {
      (*transferable)->AddDataFlavor(kJPEGImageMime);
      (*transferable)->AddDataFlavor(kHTMLMime);
      (*transferable)->AddDataFlavor(kFileMime);
    }
    (*transferable)->AddDataFlavor(kUnicodeMime);
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsHTMLEditor::InsertFromTransferable(nsITransferable *transferable, 
                                                   const nsString& aContextStr,
                                                   const nsString& aInfoStr)
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
#ifdef DEBUG_akkana
    printf("Got flavor [%s]\n", bestFlavor);
#endif
    if (flavor.EqualsWithConversion(kHTMLMime))
    {
      nsCOMPtr<nsISupportsWString> textDataObj ( do_QueryInterface(genericDataObj) );
      if (textDataObj && len > 0)
      {
        PRUnichar* text = nsnull;

        textDataObj->ToString ( &text );
        stuffToPaste.Assign ( text, len / 2 );
        nsAutoEditBatch beginBatching(this);
        rv = InsertHTMLWithContext(stuffToPaste, aContextStr, aInfoStr);
        if (text)
          nsMemory::Free(text);
      }
    }
    else if (flavor.EqualsWithConversion(kUnicodeMime))
    {
      nsCOMPtr<nsISupportsWString> textDataObj ( do_QueryInterface(genericDataObj) );
      if (textDataObj && len > 0)
      {
        PRUnichar* text = nsnull;
        textDataObj->ToString ( &text );
        stuffToPaste.Assign ( text, len / 2 );
        nsAutoEditBatch beginBatching(this);
        // pasting does not inherit local inline styles
        RemoveAllInlineProperties();
        rv = InsertText(stuffToPaste.GetUnicode());
        if (text)
          nsMemory::Free(text);
      }
    }
    else if (flavor.EqualsWithConversion(kFileMime))
    {
      nsCOMPtr<nsIFile> fileObj ( do_QueryInterface(genericDataObj) );
      if (fileObj && len > 0)
      {
        nsCOMPtr<nsIFileURL> fileURL;
        rv = nsComponentManager::CreateInstance("@mozilla.org/network/standard-url;1", nsnull, 
                                     NS_GET_IID(nsIURL), getter_AddRefs(fileURL));
        if (NS_FAILED(rv))
          return rv;
        
        if ( fileURL )
        {
          rv = fileURL->SetFile( fileObj );
          if (NS_FAILED(rv))
            return rv;
          
          PRBool insertAsImage = PR_FALSE;
          char *fileextension = nsnull;
          rv = fileURL->GetFileExtension( &fileextension );
          if ( NS_SUCCEEDED(rv) && fileextension )
          {
            if ( (nsCRT::strcasecmp( fileextension, "jpg" ) == 0 )
              || (nsCRT::strcasecmp( fileextension, "jpeg" ) == 0 )
              || (nsCRT::strcasecmp( fileextension, "gif" ) == 0 )
              || (nsCRT::strcasecmp( fileextension, "png" ) == 0 ) )
            {
              insertAsImage = PR_TRUE;
            }
          }
          if (fileextension) nsCRT::free(fileextension);
          
          char *urltext = nsnull;
          rv = fileURL->GetSpec( &urltext );
          if ( NS_SUCCEEDED(rv) && urltext && urltext[0] != 0)
          {
            len = strlen(urltext);
            if ( insertAsImage )
            {
              stuffToPaste.AssignWithConversion ( "<IMG src=\"", 10);
              stuffToPaste.AppendWithConversion ( urltext, len );
              stuffToPaste.AppendWithConversion ( "\">" );
            }
            else /* insert as link */
            {
              stuffToPaste.AssignWithConversion ( "<A href=\"" );
              stuffToPaste.AppendWithConversion ( urltext, len );
              stuffToPaste.AppendWithConversion ( "\">" );
              stuffToPaste.AppendWithConversion ( urltext, len );
              stuffToPaste.AppendWithConversion ( "</A>" );
            }
            nsAutoEditBatch beginBatching(this);
            rv = InsertHTML(stuffToPaste);
          }
          if (urltext) nsCRT::free(urltext);
        }
      }
    }
    else if (flavor.EqualsWithConversion(kJPEGImageMime))
    {
      // Insert Image code here
      printf("Don't know how to insert an image yet!\n");
      //nsIImage* image = (nsIImage *)data;
      //NS_RELEASE(image);
      rv = NS_ERROR_NOT_IMPLEMENTED; // for now give error code
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

NS_IMETHODIMP nsHTMLEditor::InsertFromDrop(nsIDOMEvent* aDropEvent)
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
    
    rv = InsertFromTransferable(trans, nsAutoString(), nsAutoString());
  }

  return rv;
}

NS_IMETHODIMP nsHTMLEditor::CanDrag(nsIDOMEvent *aDragEvent, PRBool &aCanDrag)
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
#ifdef DEBUG_cmanske
    printf(" *** IGNORING SPURIOUS DRAG EVENT!\n");
#endif
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

NS_IMETHODIMP nsHTMLEditor::DoDrag(nsIDOMEvent *aDragEvent)
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

NS_IMETHODIMP nsHTMLEditor::Paste(PRInt32 aSelectionType)
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
      // also get additional html copy hints, if present
      nsAutoString contextStr, infoStr;
      nsCOMPtr<nsISupports> contextDataObj, infoDataObj;
      PRUint32 contextLen, infoLen;
      nsCOMPtr<nsISupportsWString> textDataObj;
      
      nsCOMPtr<nsITransferable> contextTrans = do_CreateInstance(kCTransferableCID);
      NS_ENSURE_TRUE(contextTrans, NS_ERROR_NULL_POINTER);
      contextTrans->AddDataFlavor(kHTMLContext);
      clipboard->GetData(contextTrans, aSelectionType);
      contextTrans->GetTransferData(kHTMLContext, getter_AddRefs(contextDataObj), &contextLen);

      nsCOMPtr<nsITransferable> infoTrans = do_CreateInstance(kCTransferableCID);
      NS_ENSURE_TRUE(infoTrans, NS_ERROR_NULL_POINTER);
      infoTrans->AddDataFlavor(kHTMLInfo);
      clipboard->GetData(infoTrans, aSelectionType);
      infoTrans->GetTransferData(kHTMLInfo, getter_AddRefs(infoDataObj), &infoLen);
      
      if (contextDataObj)
      {
        PRUnichar* text = nsnull;
        textDataObj = do_QueryInterface(contextDataObj);
        textDataObj->ToString ( &text );
        contextStr.Assign ( text, contextLen / 2 );
        if (text)
          nsMemory::Free(text);
      }
      
      if (infoDataObj)
      {
        PRUnichar* text = nsnull;
        textDataObj = do_QueryInterface(infoDataObj);
        textDataObj->ToString ( &text );
        infoStr.Assign ( text, infoLen / 2 );
        if (text)
          nsMemory::Free(text);
      }
      rv = InsertFromTransferable(trans, contextStr, infoStr);
    }
  }

  return rv;
}


NS_IMETHODIMP nsHTMLEditor::CanPaste(PRInt32 aSelectionType, PRBool &aCanPaste)
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
  char* htmlEditorFlavors[] = { kJPEGImageMime, kHTMLMime, nsnull };

  nsCOMPtr<nsISupportsArray> flavorsList;
  rv = nsComponentManager::CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, nsnull, 
         NS_GET_IID(nsISupportsArray), getter_AddRefs(flavorsList));
  if (NS_FAILED(rv)) return rv;
  
  PRUint32 editorFlags;
  GetFlags(&editorFlags);
  
  // add the flavors for all editors
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
  
  // add the HTML-editor only flavors
  if ((editorFlags & eEditorPlaintextMask) == 0)
  {
    for (char** htmlFlavor = htmlEditorFlavors; *htmlFlavor; htmlFlavor++)
    {
      nsCOMPtr<nsISupportsString> flavorString;            
      nsComponentManager::CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, nsnull, 
           NS_GET_IID(nsISupportsString), getter_AddRefs(flavorString));
      if (flavorString)
      {
        flavorString->SetData(*htmlFlavor);
        flavorsList->AppendElement(flavorString);
      }
    }
  }
  
  PRBool haveFlavors;
  rv = clipboard->HasDataMatchingFlavors(flavorsList, aSelectionType, &haveFlavors);
  if (NS_FAILED(rv)) return rv;
  
  aCanPaste = haveFlavors;
  return NS_OK;
}


// 
// HTML PasteAsQuotation: Paste in a blockquote type=cite
//
NS_IMETHODIMP nsHTMLEditor::PasteAsQuotation(PRInt32 aSelectionType)
{
  if (mFlags & eEditorPlaintextMask)
    return PasteAsPlaintextQuotation(aSelectionType);

  nsAutoString citation;
  return PasteAsCitedQuotation(citation, aSelectionType);
}

NS_IMETHODIMP nsHTMLEditor::PasteAsCitedQuotation(const nsString& aCitation,
                                                  PRInt32 aSelectionType)
{
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpInsertQuotation, nsIEditor::eNext);

  // get selection
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  // give rules a chance to handle or cancel
  nsTextRulesInfo ruleInfo(nsTextEditRules::kInsertElement);
  PRBool cancel, handled;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(res)) return res;
  if (cancel) return NS_OK; // rules canceled the operation
  if (!handled)
  {
    nsCOMPtr<nsIDOMNode> newNode;
    nsAutoString tag; tag.AssignWithConversion("blockquote");
    res = DeleteSelectionAndCreateNode(tag, getter_AddRefs(newNode));
    if (NS_FAILED(res)) return res;
    if (!newNode) return NS_ERROR_NULL_POINTER;

    // Try to set type=cite.  Ignore it if this fails.
    nsCOMPtr<nsIDOMElement> newElement (do_QueryInterface(newNode));
    if (newElement)
    {
      nsAutoString type; type.AssignWithConversion("type");
      nsAutoString cite; cite.AssignWithConversion("cite");
      newElement->SetAttribute(type, cite);
    }

    // Set the selection to the underneath the node we just inserted:
    res = selection->Collapse(newNode, 0);
    if (NS_FAILED(res))
    {
#ifdef DEBUG_akkana
      printf("Couldn't collapse");
#endif
      // XXX: error result:  should res be returned here?
    }

    res = Paste(aSelectionType);
  }
  return res;
}

//
// Paste a plaintext quotation
//
NS_IMETHODIMP nsHTMLEditor::PasteAsPlaintextQuotation(PRInt32 aSelectionType)
{
  // Get Clipboard Service
  nsresult rv;
  NS_WITH_SERVICE(nsIClipboard, clipboard, kCClipboardCID, &rv);
  if (NS_FAILED(rv)) return rv;

  // Create generic Transferable for getting the data
  nsCOMPtr<nsITransferable> trans;
  rv = nsComponentManager::CreateInstance(kCTransferableCID, nsnull, 
                                          NS_GET_IID(nsITransferable), 
                                          (void**) getter_AddRefs(trans));
  if (NS_SUCCEEDED(rv) && trans)
  {
    // We only handle plaintext pastes here
    trans->AddDataFlavor(kUnicodeMime);

    // Get the Data from the clipboard
    clipboard->GetData(trans, aSelectionType);

    // Now we ask the transferable for the data
    // it still owns the data, we just have a pointer to it.
    // If it can't support a "text" output of the data the call will fail
    nsCOMPtr<nsISupports> genericDataObj;
    PRUint32 len = 0;
    char* flav = 0;
    rv = trans->GetAnyTransferData(&flav, getter_AddRefs(genericDataObj),
                                   &len);
    if (NS_FAILED(rv))
    {
#ifdef DEBUG_akkana
      printf("PasteAsPlaintextQuotation: GetAnyTransferData failed, %d\n", rv);
#endif
      return rv;
    }
#ifdef DEBUG_akkana
    printf("Got flavor [%s]\n", flav);
#endif
    nsAutoString flavor; flavor.AssignWithConversion(flav);
    nsAutoString stuffToPaste;
    if (flavor.EqualsWithConversion(kUnicodeMime))
    {
      nsCOMPtr<nsISupportsWString> textDataObj ( do_QueryInterface(genericDataObj) );
      if (textDataObj && len > 0)
      {
        PRUnichar* text = nsnull;
        textDataObj->ToString ( &text );
        stuffToPaste.Assign ( text, len / 2 );
        nsAutoEditBatch beginBatching(this);
        rv = InsertAsPlaintextQuotation(stuffToPaste, 0);
        if (text)
          nsMemory::Free(text);
      }
    }
    nsCRT::free(flav);
  }

  return rv;
}

NS_IMETHODIMP nsHTMLEditor::InsertAsQuotation(const nsString& aQuotedText,
                                              nsIDOMNode **aNodeInserted)
{
  if (mFlags & eEditorPlaintextMask)
    return InsertAsPlaintextQuotation(aQuotedText, aNodeInserted);

  nsAutoString citation;
  nsAutoString charset;
  return InsertAsCitedQuotation(aQuotedText, citation, PR_FALSE,
                                charset, aNodeInserted);
}

// text insert.
NS_IMETHODIMP
nsHTMLEditor::InsertAsPlaintextQuotation(const nsString& aQuotedText,
                                         nsIDOMNode **aNodeInserted)
{
  // We have the text.  Cite it appropriately:
  nsCOMPtr<nsICiter> citer;
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  char *citationType = 0;
  rv = prefs->CopyCharPref("mail.compose.citationType", &citationType);
                          
  if (NS_SUCCEEDED(rv) && citationType[0])
  {
    if (!strncmp(citationType, "aol", 3))
      citer = new nsAOLCiter;
    else
      citer = new nsInternetCiter;
    PL_strfree(citationType);
  }
  else
    citer = new nsInternetCiter;
  
  // Let the citer quote it for us:
  nsString quotedStuff;
  rv = citer->GetCiteString(aQuotedText, quotedStuff);
  if (!NS_SUCCEEDED(rv))
    return rv;

  // It's best to put a blank line after the quoted text so that mails
  // written without thinking won't be so ugly.
  quotedStuff.Append(PRUnichar('\n'));

  nsCOMPtr<nsIDOMNode> preNode;
  // get selection
  nsCOMPtr<nsISelection> selection;
  rv = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(rv)) return rv;
  if (!selection) return NS_ERROR_NULL_POINTER;
  else
  {
    nsAutoEditBatch beginBatching(this);
    nsAutoRules beginRulesSniffing(this, kOpInsertQuotation, nsIEditor::eNext);

    // give rules a chance to handle or cancel
    nsTextRulesInfo ruleInfo(nsTextEditRules::kInsertElement);
    PRBool cancel, handled;
    rv = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
    if (NS_FAILED(rv)) return rv;
    if (cancel) return NS_OK; // rules canceled the operation
    if (!handled)
    {
      // Wrap the inserted quote in a <pre> so it won't be wrapped:
      nsAutoString tag; tag.AssignWithConversion("pre");
      rv = DeleteSelectionAndCreateNode(tag, getter_AddRefs(preNode));
      
      // If this succeeded, then set selection inside the pre
      // so the inserted text will end up there.
      // If it failed, we don't care what the return value was,
      // but we'll fall through and try to insert the text anyway.
      if (NS_SUCCEEDED(rv) && preNode)
      {
        // Add an attribute on the pre node so we'll know it's a quotation.
        // Do this after the insertion, so that 
        nsCOMPtr<nsIDOMElement> preElement (do_QueryInterface(preNode));
        if (preElement)
        {
          preElement->SetAttribute(NS_LITERAL_STRING("_moz_quote"),
                                   NS_LITERAL_STRING("true"));
          // set style to not have unwanted vertical margins
          preElement->SetAttribute(NS_LITERAL_STRING("style"),
                                   NS_LITERAL_STRING("margin: 0 0 0 0px;"));
        }

        // and set the selection inside it:
        selection->Collapse(preNode, 0);
      }

      rv = InsertText(quotedStuff.GetUnicode());

      if (aNodeInserted && NS_SUCCEEDED(rv))
      {
        *aNodeInserted = preNode;
        NS_IF_ADDREF(*aNodeInserted);
      }
    }
  }
    
  // Set the selection to just after the inserted node:
  if (NS_SUCCEEDED(rv) && preNode)
  {
    nsCOMPtr<nsIDOMNode> parent;
    PRInt32 offset;
    if (NS_SUCCEEDED(GetNodeLocation(preNode, address_of(parent), &offset)) && parent)
      selection->Collapse(parent, offset+1);
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLEditor::InsertAsCitedQuotation(const nsString& aQuotedText,
                                     const nsString& aCitation,
                                     PRBool aInsertHTML,
                                     const nsString& aCharset,
                                     nsIDOMNode **aNodeInserted)
{
  nsCOMPtr<nsIDOMNode> newNode;
  nsresult res = NS_OK;

  // get selection
  nsCOMPtr<nsISelection> selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  else
  {
    nsAutoEditBatch beginBatching(this);
    nsAutoRules beginRulesSniffing(this, kOpInsertQuotation, nsIEditor::eNext);

    // give rules a chance to handle or cancel
    nsTextRulesInfo ruleInfo(nsTextEditRules::kInsertElement);
    PRBool cancel, handled;
    res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
    if (NS_FAILED(res)) return res;
    if (cancel) return NS_OK; // rules canceled the operation
    if (!handled)
    {
      nsAutoString tag; tag.AssignWithConversion("blockquote");
      res = DeleteSelectionAndCreateNode(tag, getter_AddRefs(newNode));
      if (NS_FAILED(res)) return res;
      if (!newNode) return NS_ERROR_NULL_POINTER;

      // Try to set type=cite.  Ignore it if this fails.
      nsCOMPtr<nsIDOMElement> newElement (do_QueryInterface(newNode));
      if (newElement)
      {
        nsAutoString type; type.AssignWithConversion("type");
        nsAutoString cite; cite.AssignWithConversion("cite");
        newElement->SetAttribute(type, cite);

        if (aCitation.Length() > 0)
          newElement->SetAttribute(cite, aCitation);

        // Set the selection inside the blockquote so aQuotedText will go there:
        selection->Collapse(newNode, 0);
      }

      if (aInsertHTML)
        res = InsertHTMLWithCharset(aQuotedText, aCharset);

      else
        res = InsertText(aQuotedText.GetUnicode());  // XXX ignore charset

      if (aNodeInserted)
      {
        if (NS_SUCCEEDED(res))
        {
          *aNodeInserted = newNode;
          NS_IF_ADDREF(*aNodeInserted);
        }
      }
    }
  }

  // Set the selection to just after the inserted node:
  if (NS_SUCCEEDED(res) && newNode)
  {
    nsCOMPtr<nsIDOMNode> parent;
    PRInt32 offset;
    if (NS_SUCCEEDED(GetNodeLocation(newNode, address_of(parent), &offset)) && parent)
      selection->Collapse(parent, offset+1);
  }
  return res;
}

nsresult nsHTMLEditor::CreateDOMFragmentFromPaste(nsIDOMNSRange *nsrange,
                                                  const nsString& aInputString,
                                                  const nsString& aContextStr,
                                                  const nsString& aInfoStr,
                                                  nsCOMPtr<nsIDOMNode> *outFragNode,
                                                  PRInt32 *outRangeStartHint,
                                                  PRInt32 *outRangeEndHint)
{
  if (!outFragNode || !outRangeStartHint || !outRangeEndHint) 
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMDocumentFragment> docfrag;
  nsresult res = nsrange->CreateContextualFragment(aInputString, getter_AddRefs(docfrag));
  NS_ENSURE_SUCCESS(res, res);
  *outFragNode = do_QueryInterface(docfrag);

  res = StripFormattingNodes(*outFragNode);
  NS_ENSURE_SUCCESS(res, res);

  // if we have context info, create a fragment for that too
  nsCOMPtr<nsIDOMDocumentFragment> contextfrag;
  nsCOMPtr<nsIDOMNode> contextLeaf;
  PRInt32 contextDepth = 0;
  if (aContextStr.Length())
  {
    res = nsrange->CreateContextualFragment(aContextStr, getter_AddRefs(contextfrag));
    NS_ENSURE_SUCCESS(res, res);
    nsCOMPtr<nsIDOMNode> contextAsNode (do_QueryInterface(contextfrag));
    res = StripFormattingNodes(contextAsNode);
    NS_ENSURE_SUCCESS(res, res);
    // cache the deepest leaf in the context
    nsCOMPtr<nsIDOMNode> junk, child, tmp = contextAsNode;
    while (tmp)
    {
      contextDepth++;
      contextLeaf = tmp;
      contextLeaf->GetFirstChild(getter_AddRefs(tmp));
    }
    // unite the two trees
    contextLeaf->AppendChild(*outFragNode, getter_AddRefs(junk));
    *outFragNode = contextAsNode;
    // no longer have fragmentAsNode in tree
    contextDepth--;
  }
  
  // get the infoString contents
  nsAutoString numstr1, numstr2;
  if (aInfoStr.Length())
  {
    PRInt32 err, sep;
    sep = aInfoStr.FindChar((PRUnichar)',');
    aInfoStr.Left(numstr1, sep);
    aInfoStr.Mid(numstr2, sep+1, -1);
    *outRangeStartHint = numstr1.ToInteger(&err) + contextDepth;
    *outRangeEndHint   = numstr2.ToInteger(&err) + contextDepth;
  }
  else
  {
    *outRangeStartHint = contextDepth;
    *outRangeEndHint = contextDepth;
  }
  return res;
}

nsresult nsHTMLEditor::CreateListOfNodesToPaste(nsIDOMNode  *aFragmentAsNode,
                                                nsCOMPtr<nsISupportsArray> *outNodeList,
                                                PRInt32 aRangeStartHint,
                                                PRInt32 aRangeEndHint)
{
  if (!outNodeList || !aFragmentAsNode) 
    return NS_ERROR_NULL_POINTER;

  // First off create a range over the portion of docFrag indicated by
  // the range hints.
  nsCOMPtr<nsIDOMRange> docFragRange;
  docFragRange = do_CreateInstance(kCRangeCID);
  nsCOMPtr<nsIDOMNode> startParent, endParent, tmp;
  PRInt32 endOffset;
  startParent = aFragmentAsNode;
  while (aRangeStartHint > 0)
  {
    startParent->GetFirstChild(getter_AddRefs(tmp));
    startParent = tmp;
    aRangeStartHint--;
    NS_ENSURE_TRUE(startParent, NS_ERROR_FAILURE);
  }
  endParent = aFragmentAsNode;
  while (aRangeEndHint > 0)
  {
    endParent->GetLastChild(getter_AddRefs(tmp));
    endParent = tmp;
    aRangeEndHint--;
    NS_ENSURE_TRUE(endParent, NS_ERROR_FAILURE);
  }
  nsresult res = GetLengthOfDOMNode(endParent, (PRUint32&)endOffset);
  NS_ENSURE_SUCCESS(res, res);

  res = docFragRange->SetStart(startParent, 0);
  NS_ENSURE_SUCCESS(res, res);
  res = docFragRange->SetEnd(endParent, endOffset);
  NS_ENSURE_SUCCESS(res, res);

  // now use a subtree iterator over the range to create a list of nodes
  nsTrivialFunctor functor;
  nsDOMSubtreeIterator iter;
  res = NS_NewISupportsArray(getter_AddRefs(*outNodeList));
  NS_ENSURE_SUCCESS(res, res);
  res = iter.Init(docFragRange);
  NS_ENSURE_SUCCESS(res, res);
  res = iter.AppendList(functor, *outNodeList);

  return res;
}



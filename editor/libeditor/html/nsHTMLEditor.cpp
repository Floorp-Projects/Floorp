/* -*- Mode: C++ tab-width: 2 indent-tabs-mode: nil c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL") you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsTextEditor.h"
#include "nsHTMLEditor.h"
#include "nsHTMLEditRules.h"
#include "nsEditorEventListeners.h"
#include "nsInsertHTMLTxn.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNSRange.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMEventReceiver.h" 
#include "nsIDOMKeyListener.h" 
#include "nsIDOMMouseListener.h"
#include "nsIDOMSelection.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIEnumerator.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsEditorCID.h"
#include "nsLayoutCID.h"
#include "nsIDOMRange.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsFileSpec.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIFileWidget.h" // for GetLocalFileURL stuff
#include "nsWidgetsCID.h"
#include "nsIDocumentEncoder.h"


static NS_DEFINE_IID(kInsertHTMLTxnIID, NS_INSERT_HTML_TXN_IID);

static NS_DEFINE_CID(kEditorCID,      NS_EDITOR_CID);
static NS_DEFINE_CID(kTextEditorCID,  NS_TEXTEDITOR_CID);
static NS_DEFINE_CID(kHTMLEditorCID,  NS_HTMLEDITOR_CID);
static NS_DEFINE_CID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_CID(kCRangeCID,      NS_RANGE_CID);
static NS_DEFINE_IID(kFileWidgetCID,  NS_FILEWIDGET_CID);
static NS_DEFINE_CID(kHTMLEncoderCID, NS_HTML_ENCODER_CID);

#ifdef NS_DEBUG
static PRBool gNoisy = PR_FALSE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif

nsHTMLEditor::nsHTMLEditor()
{
// Done in nsEditor
// NS_INIT_REFCNT();
}

nsHTMLEditor::~nsHTMLEditor()
{
  //the autopointers will clear themselves up. 
}

// Adds appropriate AddRef, Release, and QueryInterface methods for derived class
//NS_IMPL_ISUPPORTS_INHERITED(nsHTMLEditor, nsTextEditor, nsIHTMLEditor)

//NS_IMPL_ADDREF_INHERITED(Class, Super)
NS_IMETHODIMP_(nsrefcnt) nsHTMLEditor::AddRef(void)
{
  return nsTextEditor::AddRef();
}

//NS_IMPL_RELEASE_INHERITED(Class, Super)
NS_IMETHODIMP_(nsrefcnt) nsHTMLEditor::Release(void)
{
  return nsTextEditor::Release();
}

//NS_IMPL_QUERY_INTERFACE_INHERITED(Class, Super, AdditionalInterface)
NS_IMETHODIMP nsHTMLEditor::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
 
  if (aIID.Equals(nsIHTMLEditor::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsIHTMLEditor*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return nsTextEditor::QueryInterface(aIID, aInstancePtr);
}


NS_IMETHODIMP nsHTMLEditor::Init(nsIDOMDocument *aDoc, 
                                 nsIPresShell   *aPresShell)
{
  NS_PRECONDITION(nsnull!=aDoc && nsnull!=aPresShell, "bad arg");
  nsresult res=NS_ERROR_NULL_POINTER;
  if ((nsnull!=aDoc) && (nsnull!=aPresShell))
  {
    return nsTextEditor::Init(aDoc, aPresShell);
  }
  return res;
}

void  nsHTMLEditor::InitRules()
{
// instantiate the rules for this text editor
// XXX: we should be told which set of rules to instantiate
  mRules =  new nsHTMLEditRules();
  mRules->Init(this);
}

NS_IMETHODIMP nsHTMLEditor::SetTextProperty(nsIAtom *aProperty, 
                                            const nsString *aAttribute,
                                            const nsString *aValue)
{
  return nsTextEditor::SetTextProperty(aProperty, aAttribute, aValue);
}

NS_IMETHODIMP nsHTMLEditor::GetTextProperty(nsIAtom *aProperty, 
                                            const nsString *aAttribute, 
                                            const nsString *aValue,
                                            PRBool &aFirst, PRBool &aAny, PRBool &aAll)
{
  return nsTextEditor::GetTextProperty(aProperty, aAttribute, aValue, aFirst, aAny, aAll);
}

NS_IMETHODIMP nsHTMLEditor::RemoveTextProperty(nsIAtom *aProperty, const nsString *aAttribute)
{
  return nsTextEditor::RemoveTextProperty(aProperty, aAttribute);
}

NS_IMETHODIMP nsHTMLEditor::DeleteSelection(nsIEditor::ECollapsedSelectionAction aAction)
{
  return nsTextEditor::DeleteSelection(aAction);
}

NS_IMETHODIMP nsHTMLEditor::InsertText(const nsString& aStringToInsert)
{
  return nsTextEditor::InsertText(aStringToInsert);
}

NS_IMETHODIMP nsHTMLEditor::SetBackgroundColor(const nsString& aColor)
{
  nsresult res;
  NS_ASSERTION(mDoc, "Missing Editor DOM Document");
  
  // TODO: Check selection for Cell, Row, Column or table and do color on appropriate level
  // For initial testing, just set the background on the BODY tag (the document's background)

  // Set the background color attribute on the body tag
  nsCOMPtr<nsIDOMElement> bodyElement;
  res = nsEditor::GetBodyElement(getter_AddRefs(bodyElement));
  if (NS_SUCCEEDED(res) && bodyElement)
  {
    nsAutoEditBatch beginBatching(this);
    bodyElement->SetAttribute("bgcolor", aColor);
  }
  return res;
}

NS_IMETHODIMP nsHTMLEditor::SetBodyAttribute(const nsString& aAttribute, const nsString& aValue)
{
  nsresult res;
  // TODO: Check selection for Cell, Row, Column or table and do color on appropriate level

  NS_ASSERTION(mDoc, "Missing Editor DOM Document");
  
  // Set the background color attribute on the body tag
  nsCOMPtr<nsIDOMElement> bodyElement;

  res = nsEditor::GetBodyElement(getter_AddRefs(bodyElement));
  if (NS_SUCCEEDED(res) && bodyElement)
  {
    nsAutoEditBatch beginBatching(this);
    bodyElement->SetAttribute(aAttribute, aValue);
  }
  return res;
}

NS_IMETHODIMP nsHTMLEditor::InsertBreak()
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel= PR_FALSE;

  nsAutoEditBatch beginBatching(this);

  // pre-process
  nsEditor::GetSelection(getter_AddRefs(selection));
  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kInsertBreak);
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel);
  if ((PR_FALSE==cancel) && (NS_SUCCEEDED(res)))
  {
    // create the new BR node
    nsCOMPtr<nsIDOMNode> newNode;
    nsAutoString tag("BR");
    res = nsEditor::DeleteSelectionAndCreateNode(tag, getter_AddRefs(newNode));
    if (NS_SUCCEEDED(res) && newNode)
    {
      // set the selection to the new node
      nsCOMPtr<nsIDOMNode>parent;
      res = newNode->GetParentNode(getter_AddRefs(parent));
      if (NS_SUCCEEDED(res) && parent)
      {
        PRInt32 offsetInParent=-1;  // we use the -1 as a marker to see if we need to compute this or not
        nsCOMPtr<nsIDOMNode>nextNode;
        newNode->GetNextSibling(getter_AddRefs(nextNode));
        if (nextNode)
        {
          nsCOMPtr<nsIDOMCharacterData>nextTextNode;
          nextTextNode = do_QueryInterface(nextNode);
          if (!nextTextNode) {
            nextNode = do_QueryInterface(newNode);
          }
          else { 
            offsetInParent=0; 
          }
        }
        else {
          nextNode = do_QueryInterface(newNode);
        }
        res = nsEditor::GetSelection(getter_AddRefs(selection));
        if (NS_SUCCEEDED(res))
        {
          if (-1==offsetInParent) 
          {
            nextNode->GetParentNode(getter_AddRefs(parent));
            res = GetChildOffset(nextNode, parent, offsetInParent);
            if (NS_SUCCEEDED(res)) {
              selection->Collapse(parent, offsetInParent+1);  // +1 to insert just after the break
            }
          }
          else
          {
            selection->Collapse(nextNode, offsetInParent);
          }
        }
      }
    }
    // post-process, always called if WillInsertBreak didn't return cancel==PR_TRUE
    res = mRules->DidDoAction(selection, &ruleInfo, res);
  }
// XXXX: Horrible hack! We are doing this because
// of an error in Gecko which is not rendering the
// document after a change via the DOM - gpk 2/13/99
  // BEGIN HACK!!!
  // HACKForceRedraw();
  // END HACK

  return res;
}

NS_IMETHODIMP nsHTMLEditor::GetParagraphFormat(nsString& aParagraphFormat)
{
  nsresult res = NS_ERROR_NOT_INITIALIZED;

  return res;
}

NS_IMETHODIMP nsHTMLEditor::SetParagraphFormat(const nsString& aParagraphFormat)
{
  nsresult res = NS_ERROR_NOT_INITIALIZED;
  //Kinda sad to waste memory just to force lower case
  nsAutoString tag = aParagraphFormat;
  tag.ToLowerCase();
  if (tag == "normal" || tag == "p") {
    res = RemoveParagraphStyle();
  } else {
    res = ReplaceBlockParent(tag);
  }
  return res;
}


// Methods shared with the base editor.
// Note: We could call each of these via nsTextEditor -- is that better?
NS_IMETHODIMP nsHTMLEditor::EnableUndo(PRBool aEnable)
{
  return nsTextEditor::EnableUndo(aEnable);
}

NS_IMETHODIMP nsHTMLEditor::Undo(PRUint32 aCount)
{
  return nsTextEditor::Undo(aCount);
}

NS_IMETHODIMP nsHTMLEditor::CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo)
{
  return nsTextEditor::CanUndo(aIsEnabled, aCanUndo);
}

NS_IMETHODIMP nsHTMLEditor::Redo(PRUint32 aCount)
{
  return nsTextEditor::Redo(aCount);
}

NS_IMETHODIMP nsHTMLEditor::CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo)
{
  return nsTextEditor::CanRedo(aIsEnabled, aCanRedo);
}

NS_IMETHODIMP nsHTMLEditor::BeginTransaction()
{
  return nsTextEditor::BeginTransaction();
}

NS_IMETHODIMP nsHTMLEditor::EndTransaction()
{
  return nsTextEditor::EndTransaction();
}

NS_IMETHODIMP nsHTMLEditor::MoveSelectionUp(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return nsTextEditor::MoveSelectionUp(aIncrement, aExtendSelection);
}

NS_IMETHODIMP nsHTMLEditor::MoveSelectionDown(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return nsTextEditor::MoveSelectionDown(aIncrement, aExtendSelection);
}

NS_IMETHODIMP nsHTMLEditor::MoveSelectionNext(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return nsTextEditor::MoveSelectionNext(aIncrement, aExtendSelection);
}

NS_IMETHODIMP nsHTMLEditor::MoveSelectionPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return nsTextEditor::MoveSelectionPrevious(aIncrement, aExtendSelection);
}

NS_IMETHODIMP nsHTMLEditor::SelectNext(nsIAtom *aIncrement, PRBool aExtendSelection) 
{
  return nsTextEditor::SelectNext(aIncrement, aExtendSelection);
}

NS_IMETHODIMP nsHTMLEditor::SelectPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return nsTextEditor::SelectPrevious(aIncrement, aExtendSelection);
}

NS_IMETHODIMP nsHTMLEditor::SelectAll()
{
  return nsTextEditor::SelectAll();
}

NS_IMETHODIMP nsHTMLEditor::ScrollUp(nsIAtom *aIncrement)
{
  return nsTextEditor::ScrollUp(aIncrement);
}

NS_IMETHODIMP nsHTMLEditor::ScrollDown(nsIAtom *aIncrement)
{
  return nsTextEditor::ScrollDown(aIncrement);
}

NS_IMETHODIMP nsHTMLEditor::ScrollIntoView(PRBool aScrollToBegin)
{
  return nsTextEditor::ScrollIntoView(aScrollToBegin);
}

NS_IMETHODIMP nsHTMLEditor::Save()
{
	return nsTextEditor::Save();
}

NS_IMETHODIMP nsHTMLEditor::SaveAs(PRBool aSavingCopy)
{
	return nsTextEditor::SaveAs(aSavingCopy);
}

NS_IMETHODIMP nsHTMLEditor::Cut()
{
  return nsTextEditor::Cut();
}

NS_IMETHODIMP nsHTMLEditor::Copy()
{
  return nsTextEditor::Copy();
}

NS_IMETHODIMP nsHTMLEditor::Paste()
{
  return nsTextEditor::Paste();
}

// 
// HTML PasteAsQuotation: Paste in a blockquote type=cite
//
NS_IMETHODIMP nsHTMLEditor::PasteAsQuotation()
{
  nsAutoString citation("");
  return PasteAsCitedQuotation(citation);
}

NS_IMETHODIMP nsHTMLEditor::PasteAsCitedQuotation(const nsString& aCitation)
{
  printf("nsHTMLEditor::PasteAsQuotation\n");

  nsAutoEditBatch beginBatching(this);
  nsCOMPtr<nsIDOMNode> newNode;
  nsAutoString tag("blockquote");
  nsresult res = nsEditor::DeleteSelectionAndCreateNode(tag, getter_AddRefs(newNode));
  if (NS_FAILED(res) || !newNode)
    return res;

  // Try to set type=cite.  Ignore it if this fails.
  nsCOMPtr<nsIDOMElement> newElement (do_QueryInterface(newNode));
  if (newElement)
  {
    nsAutoString type ("type");
    nsAutoString cite ("cite");
    newElement->SetAttribute(type, cite);
  }

  // Set the selection to the underneath the node we just inserted:
  nsCOMPtr<nsIDOMSelection> selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res) || !selection)
  {
#ifdef DEBUG_akkana
    printf("Can't get selection!");
#endif
  }

  res = selection->Collapse(newNode, 0);
  if (NS_FAILED(res))
  {
#ifdef DEBUG_akkana
    printf("Couldn't collapse");
#endif
  }

  res = Paste();
  return res;
}

NS_IMETHODIMP nsHTMLEditor::InsertAsQuotation(const nsString& aQuotedText)
{
  nsAutoString citation ("");
  return InsertAsCitedQuotation(aQuotedText, citation);
}

NS_IMETHODIMP nsHTMLEditor::InsertAsCitedQuotation(const nsString& aQuotedText,
                                                   const nsString& aCitation)
{
  printf("nsHTMLEditor::InsertAsQuotation\n");

  nsAutoEditBatch beginBatching(this);
  nsCOMPtr<nsIDOMNode> newNode;
  nsAutoString tag("blockquote");
  nsresult res = nsEditor::DeleteSelectionAndCreateNode(tag, getter_AddRefs(newNode));
  if (NS_FAILED(res) || !newNode)
    return res;

  // Try to set type=cite.  Ignore it if this fails.
  nsCOMPtr<nsIDOMElement> newElement (do_QueryInterface(newNode));
  if (newElement)
  {
    nsAutoString type ("type");
    nsAutoString cite ("cite");
    newElement->SetAttribute(type, cite);

    if (aCitation.Length() > 0)
      newElement->SetAttribute(cite, aCitation);
  }

  res = InsertHTML(aQuotedText);
  return res;
}

NS_IMETHODIMP nsHTMLEditor::InsertHTML(const nsString& aInputString)
{
  nsresult res;
  nsAutoEditBatch beginBatching(this);

  nsEditor::DeleteSelection(nsIEditor::eDoNothing);

  // Make the transaction for insert html:
  nsInsertHTMLTxn* insertHTMLTxn = 0;
  res = TransactionFactory::GetNewTransaction(kInsertHTMLTxnIID,
                                              (EditTxn **)&insertHTMLTxn);
  if (NS_SUCCEEDED(res))
  {
    res = insertHTMLTxn->Init(aInputString, this);
    if (NS_SUCCEEDED(res))
      res = Do(insertHTMLTxn);

    // XXX How is it that we don't have to release the transaction?
  }

  if (NS_FAILED(res))
    printf("Couldn't insert html: error was %d\n", res);

  return res;
}

NS_IMETHODIMP nsHTMLEditor::OutputTextToString(nsString& aOutputString)
{
  return nsTextEditor::OutputTextToString(aOutputString);
}

NS_IMETHODIMP nsHTMLEditor::OutputHTMLToString(nsString& aOutputString)
{
#if defined(DEBUG_kostello) || defined(DEBUG_akkana)
  nsCOMPtr<nsIHTMLEncoder> encoder;
  nsresult rv = nsComponentManager::CreateInstance(kHTMLEncoderCID,
                                                   nsnull,
                                                   nsIDocumentEncoder::GetIID(),
                                                   getter_AddRefs(encoder));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDOMDocument> domdoc;
  rv = GetDocument(getter_AddRefs(domdoc));
  if (NS_FAILED(rv))
    return rv;
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
  nsString mimetype ("text/html");

  rv = encoder->Init(doc, mimetype);
  if (NS_FAILED(rv))
    return rv;

  return encoder->EncodeToString(aOutputString);
#else
  return nsTextEditor::OutputHTMLToString(aOutputString);
#endif
}

NS_IMETHODIMP nsHTMLEditor::OutputTextToStream(nsIOutputStream* aOutputStream, nsString* aCharsetOverride)
{
  return nsTextEditor::OutputTextToStream(aOutputStream,aCharsetOverride);
}

NS_IMETHODIMP nsHTMLEditor::OutputHTMLToStream(nsIOutputStream* aOutputStream,nsString* aCharsetOverride)
{
  return nsTextEditor::OutputHTMLToStream(aOutputStream,aCharsetOverride);
}


NS_IMETHODIMP
nsHTMLEditor::CopyAttributes(nsIDOMNode *aDestNode, nsIDOMNode *aSourceNode)
{
  return nsTextEditor::CopyAttributes(aDestNode, aSourceNode);
}

//================================================================
// HTML Editor methods
//
// Note: Table Editing methods are implemented in EditTable.cpp
//

// get the paragraph style(s) for the selection
NS_IMETHODIMP 
nsHTMLEditor::GetParagraphStyle(nsStringArray *aTagList)
{
  if (gNoisy) { printf("---------- nsHTMLEditor::GetParagraphStyle ----------\n"); }
  if (!aTagList) { return NS_ERROR_NULL_POINTER; }

  nsresult res;
  nsCOMPtr<nsIDOMSelection>selection;
  res = nsEditor::GetSelection(getter_AddRefs(selection));
  if ((NS_SUCCEEDED(res)) && selection)
  {
    nsCOMPtr<nsIEnumerator> enumerator;
    enumerator = do_QueryInterface(selection);
    if (enumerator)
    {
      enumerator->First(); 
      nsISupports *currentItem;
      res = enumerator->CurrentItem(&currentItem);
      if ((NS_SUCCEEDED(res)) && (nsnull!=currentItem))
      {
        nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
        // scan the range for all the independent block content blockSections
        // and get the block parent of each
        nsISupportsArray *blockSections;
        res = NS_NewISupportsArray(&blockSections);
        if ((NS_SUCCEEDED(res)) && blockSections)
        {
          res = GetBlockSectionsForRange(range, blockSections);
          if (NS_SUCCEEDED(res))
          {
            nsIDOMRange *subRange;
            subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
            while (subRange && (NS_SUCCEEDED(res)))
            {
              nsCOMPtr<nsIDOMNode>startParent;
              res = subRange->GetStartParent(getter_AddRefs(startParent));
              if (NS_SUCCEEDED(res) && startParent) 
              {
                nsCOMPtr<nsIDOMElement> blockParent;
                res = GetBlockParent(startParent, getter_AddRefs(blockParent));
                if (NS_SUCCEEDED(res) && blockParent)
                {
                  nsAutoString blockParentTag;
                  blockParent->GetTagName(blockParentTag);
                  PRBool isRoot;
                  IsRootTag(blockParentTag, isRoot);
                  if ((PR_FALSE==isRoot) && (-1==aTagList->IndexOf(blockParentTag))) {
                    aTagList->AppendString(blockParentTag);
                  }
                }
              }
              NS_RELEASE(subRange);
              blockSections->RemoveElementAt(0);
              subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
            }
          }
          NS_RELEASE(blockSections);
        }
      }
    }
  }

  return res;
}

// use this when block parents are to be added regardless of current state
NS_IMETHODIMP 
nsHTMLEditor::AddBlockParent(nsString& aParentTag)
{
  if (gNoisy) 
  { 
    char *tag = aParentTag.ToNewCString();
    printf("---------- nsHTMLEditor::AddBlockParent %s ----------\n", tag); 
    delete [] tag;
  }
  
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMSelection>selection;
  res = nsEditor::GetSelection(getter_AddRefs(selection));
  if ((NS_SUCCEEDED(res)) && selection)
  {
    // set the block parent for all selected ranges
    nsAutoSelectionReset selectionResetter(selection);
    nsAutoEditBatch beginBatching(this);
    nsCOMPtr<nsIEnumerator> enumerator;
    enumerator = do_QueryInterface(selection);
    if (enumerator)
    {
      enumerator->First(); 
      nsISupports *currentItem;
      res = enumerator->CurrentItem(&currentItem);
      if ((NS_SUCCEEDED(res)) && (nsnull!=currentItem))
      {
        nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
        // scan the range for all the independent block content blockSections
        // and apply the transformation to them
        res = ReParentContentOfRange(range, aParentTag, eInsertParent);
      }
    }
    if (NS_SUCCEEDED(res))
    { // set the selection
      // XXX: can't do anything until I can create ranges
    }
  }
  if (gNoisy) {printf("Finished nsHTMLEditor::AddBlockParent with this content:\n"); DebugDumpContent(); } // DEBUG

  return res;
}

// use this when a paragraph type is being transformed from one type to another
NS_IMETHODIMP 
nsHTMLEditor::ReplaceBlockParent(nsString& aParentTag)
{

  if (gNoisy) 
  { 
    char *tag = aParentTag.ToNewCString();
    printf("---------- nsHTMLEditor::ReplaceBlockParent %s ----------\n", tag); 
    delete [] tag;
  }
  
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMSelection>selection;
  res = nsEditor::GetSelection(getter_AddRefs(selection));
  if ((NS_SUCCEEDED(res)) && selection)
  {
    nsAutoSelectionReset selectionResetter(selection);
    // set the block parent for all selected ranges
    nsAutoEditBatch beginBatching(this);
    nsCOMPtr<nsIEnumerator> enumerator;
    enumerator = do_QueryInterface(selection);
    if (enumerator)
    {
      enumerator->First(); 
      nsISupports *currentItem;
      res = enumerator->CurrentItem(&currentItem);
      if ((NS_SUCCEEDED(res)) && (nsnull!=currentItem))
      {
        nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
        // scan the range for all the independent block content blockSections
        // and apply the transformation to them
        res = ReParentContentOfRange(range, aParentTag, eReplaceParent);
      }
    }
  }
  if (gNoisy) {printf("Finished nsHTMLEditor::AddBlockParent with this content:\n"); DebugDumpContent(); } // DEBUG

  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::ReParentContentOfNode(nsIDOMNode *aNode, 
                                    nsString &aParentTag,
                                    BlockTransformationType aTransformation)
{
  if (!aNode) { return NS_ERROR_NULL_POINTER; }
  if (gNoisy) 
  { 
    char *tag = aParentTag.ToNewCString();
    printf("---------- ReParentContentOfNode(%p,%s,%d) -----------\n", aNode, tag, aTransformation); 
    delete [] tag;
  }
  // find the current block parent, or just use aNode if it is a block node
  nsCOMPtr<nsIDOMElement>blockParentElement;
  nsCOMPtr<nsIDOMNode>nodeToReParent; // this is the node we'll operate on, by default it's aNode
  nsresult res = aNode->QueryInterface(nsIDOMNode::GetIID(), getter_AddRefs(nodeToReParent));
  PRBool nodeIsInline;
  PRBool nodeIsBlock=PR_FALSE;
  nsTextEditor::IsNodeInline(aNode, nodeIsInline);
  if (PR_FALSE==nodeIsInline)
  {
    nsresult QIResult;
    nsCOMPtr<nsIDOMCharacterData>nodeAsText;
    QIResult = aNode->QueryInterface(nsIDOMCharacterData::GetIID(), getter_AddRefs(nodeAsText));
    if (NS_FAILED(QIResult) || !nodeAsText) {
      nodeIsBlock=PR_TRUE;
    }
  }
  // if aNode is the block parent, then the node to reparent is one of its children
  if (PR_TRUE==nodeIsBlock) 
  {
    res = aNode->QueryInterface(nsIDOMNode::GetIID(), getter_AddRefs(blockParentElement));
    if (NS_SUCCEEDED(res) && blockParentElement) {
      res = aNode->GetFirstChild(getter_AddRefs(nodeToReParent));
    }
  }
  else { // we just need to get the block parent of aNode
    res = nsTextEditor::GetBlockParent(aNode, getter_AddRefs(blockParentElement));
  }
  // at this point, we must have a good res, a node to reparent, and a block parent
  if (!nodeToReParent) { return NS_ERROR_UNEXPECTED;}
  if (!blockParentElement) { return NS_ERROR_NULL_POINTER;}
  if (NS_SUCCEEDED(res))
  {
    nsCOMPtr<nsIDOMNode> newParentNode;
    nsCOMPtr<nsIDOMNode> blockParentNode = do_QueryInterface(blockParentElement);
    // we need to treat nodes directly inside the body differently
    nsAutoString parentTag;
    blockParentElement->GetTagName(parentTag);
    PRBool isRoot;
    IsRootTag(parentTag, isRoot);
    if (PR_TRUE==isRoot)    
    {
      // if nodeToReParent is a text node, we have <ROOT>Text.
      // re-parent Text into a new <aTag> at the offset of Text in <ROOT>
      // so we end up with <ROOT><aTag>Text
      // ignore aTransformation, replaces act like inserts
      nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(nodeToReParent);
      if (nodeAsText)
      {
        res = ReParentBlockContent(nodeToReParent, aParentTag, blockParentNode, parentTag, 
                                      aTransformation, getter_AddRefs(newParentNode));
      }
      else
      { // this is the case of an insertion point between 2 non-text objects
        // XXX: how to you know it's an insertion point???
        PRInt32 offsetInParent=0;
        res = GetChildOffset(nodeToReParent, blockParentNode, offsetInParent);
        NS_ASSERTION((NS_SUCCEEDED(res)), "bad res from GetChildOffset");
        // otherwise, just create the block parent at the selection
        res = nsTextEditor::CreateNode(aParentTag, blockParentNode, offsetInParent, 
                                          getter_AddRefs(newParentNode));
        // XXX: need to move some of the children of blockParentNode into the newParentNode?
        // XXX: need to create a bogus text node inside this new block?
        //      that means, I need to generalize bogus node handling
      }
    }
    else
    { // the block parent is not a ROOT, 
      // for the selected block content, transform blockParentNode 
      if (((eReplaceParent==aTransformation) && (PR_FALSE==parentTag.EqualsIgnoreCase(aParentTag))) ||
           (eInsertParent==aTransformation))
      {
		    if (gNoisy) { DebugDumpContent(); } // DEBUG
        res = ReParentBlockContent(nodeToReParent, aParentTag, blockParentNode, parentTag, 
                                      aTransformation, getter_AddRefs(newParentNode));
        if ((NS_SUCCEEDED(res)) && (newParentNode) && (eReplaceParent==aTransformation))
        {
          PRBool hasChildren;
          blockParentNode->HasChildNodes(&hasChildren);
          if (PR_FALSE==hasChildren)
          {
            res = nsEditor::DeleteNode(blockParentNode);
            if (gNoisy) 
            {
              printf("deleted old block parent node %p\n", blockParentNode.get());
              DebugDumpContent(); // DEBUG
            }
          }
        }
      }
      else { // otherwise, it's a no-op
        if (gNoisy) { printf("AddBlockParent is a no-op for this collapsed selection.\n"); }
      }
    }
  }
  return res;
}


NS_IMETHODIMP 
nsHTMLEditor::ReParentBlockContent(nsIDOMNode  *aNode, 
                                   nsString    &aParentTag,
                                   nsIDOMNode  *aBlockParentNode,
                                   nsString    &aBlockParentTag,
                                   BlockTransformationType aTransformation,
                                   nsIDOMNode **aNewParentNode)
{
  if (!aNode || !aBlockParentNode || !aNewParentNode) { return NS_ERROR_NULL_POINTER; }
  nsCOMPtr<nsIDOMNode> blockParentNode = do_QueryInterface(aBlockParentNode);
  PRBool removeBlockParent = PR_FALSE;
  PRBool removeBreakBefore = PR_FALSE;
  PRBool removeBreakAfter = PR_FALSE;
  nsCOMPtr<nsIDOMNode>ancestor;
  nsresult res = aNode->GetParentNode(getter_AddRefs(ancestor));
  nsCOMPtr<nsIDOMNode>previousAncestor = do_QueryInterface(aNode);
  while (NS_SUCCEEDED(res) && ancestor)
  {
    nsCOMPtr<nsIDOMElement>ancestorElement = do_QueryInterface(ancestor);
    nsAutoString ancestorTag;
    ancestorElement->GetTagName(ancestorTag);
    if (ancestorTag.EqualsIgnoreCase(aBlockParentTag))
    {
      break;  // previousAncestor will contain the node to operate on
    }
    previousAncestor = do_QueryInterface(ancestor);
    res = ancestorElement->GetParentNode(getter_AddRefs(ancestor));
  }
  // now, previousAncestor is the node we are operating on
  nsCOMPtr<nsIDOMNode>leftNode, rightNode;
  res = GetBlockSection(previousAncestor, 
                           getter_AddRefs(leftNode), 
                           getter_AddRefs(rightNode));
  if ((NS_SUCCEEDED(res)) && leftNode && rightNode)
  {
    // determine some state for managing <BR>s around the new block
    PRBool isSubordinateBlock = PR_FALSE;  // if true, the content is already in a subordinate block
    PRBool isRootBlock = PR_FALSE;  // if true, the content is in a root block
    nsCOMPtr<nsIDOMElement>blockParentElement = do_QueryInterface(blockParentNode);
    if (blockParentElement)
    {
      nsAutoString blockParentTag;
      blockParentElement->GetTagName(blockParentTag);
      IsSubordinateBlock(blockParentTag, isSubordinateBlock);
      IsRootTag(blockParentTag, isRootBlock);
    }

    if (PR_TRUE==isRootBlock)
    { // we're creating a block element where a block element did not previously exist 
      removeBreakBefore = PR_TRUE;
      removeBreakAfter = PR_TRUE;
    }

    // apply the transformation
    PRInt32 offsetInParent=0;
    if (eInsertParent==aTransformation || PR_TRUE==isRootBlock)
    {
      res = GetChildOffset(leftNode, blockParentNode, offsetInParent);
      NS_ASSERTION((NS_SUCCEEDED(res)), "bad res from GetChildOffset");
      res = nsTextEditor::CreateNode(aParentTag, blockParentNode, offsetInParent, aNewParentNode);
      if (gNoisy) { printf("created a node in blockParentNode at offset %d\n", offsetInParent); }
    }
    else
    {
      nsCOMPtr<nsIDOMNode> grandParent;
      res = blockParentNode->GetParentNode(getter_AddRefs(grandParent));
      if ((NS_SUCCEEDED(res)) && grandParent)
      {
        nsCOMPtr<nsIDOMNode>firstChildNode, lastChildNode;
        blockParentNode->GetFirstChild(getter_AddRefs(firstChildNode));
        blockParentNode->GetLastChild(getter_AddRefs(lastChildNode));
        if (firstChildNode==leftNode && lastChildNode==rightNode)
        {
          res = GetChildOffset(blockParentNode, grandParent, offsetInParent);
          NS_ASSERTION((NS_SUCCEEDED(res)), "bad res from GetChildOffset");
          res = nsTextEditor::CreateNode(aParentTag, grandParent, offsetInParent, aNewParentNode);
          if (gNoisy) { printf("created a node in grandParent at offset %d\n", offsetInParent); }
        }
        else
        {
          // We're in the case where the content of blockParentNode is separated by <BR>'s, 
          // creating multiple block content ranges.
          // Split blockParentNode around the blockContent
          if (gNoisy) { printf("splitting a node because of <BR>s\n"); }
          nsCOMPtr<nsIDOMNode> newLeftNode;
          if (firstChildNode!=leftNode)
          {
            res = GetChildOffset(leftNode, blockParentNode, offsetInParent);
            if (gNoisy) { printf("splitting left at %d\n", offsetInParent); }
            res = SplitNode(blockParentNode, offsetInParent, getter_AddRefs(newLeftNode));
            // after this split, blockParentNode still contains leftNode and rightNode
          }
          if (lastChildNode!=rightNode)
          {
            res = GetChildOffset(rightNode, blockParentNode, offsetInParent);
            offsetInParent++;
            if (gNoisy) { printf("splitting right at %d\n", offsetInParent); }
            res = SplitNode(blockParentNode, offsetInParent, getter_AddRefs(newLeftNode));
            blockParentNode = do_QueryInterface(newLeftNode);
          }
          res = GetChildOffset(leftNode, blockParentNode, offsetInParent);
          NS_ASSERTION((NS_SUCCEEDED(res)), "bad res from GetChildOffset");
          res = nsTextEditor::CreateNode(aParentTag, blockParentNode, offsetInParent, aNewParentNode);
          if (gNoisy) { printf("created a node in blockParentNode at offset %d\n", offsetInParent); }
          // what we need to do here is remove the existing block parent when we're all done.
          removeBlockParent = PR_TRUE;
        }
      }
    }
    if ((NS_SUCCEEDED(res)) && *aNewParentNode)
    { // move all the children/contents of blockParentNode to aNewParentNode
      nsCOMPtr<nsIDOMNode>childNode = do_QueryInterface(rightNode);
      nsCOMPtr<nsIDOMNode>previousSiblingNode;
      while (NS_SUCCEEDED(res) && childNode)
      {
        childNode->GetPreviousSibling(getter_AddRefs(previousSiblingNode));
        // explicitly delete of childNode from it's current parent
        // can't just rely on DOM semantics of InsertNode doing the delete implicitly, doesn't undo! 
        res = nsEditor::DeleteNode(childNode); 
        if (NS_SUCCEEDED(res))
        {
          res = nsEditor::InsertNode(childNode, *aNewParentNode, 0);
          if (gNoisy) 
          {
            printf("re-parented sibling node %p\n", childNode.get());
          }
        }
        if (childNode==leftNode || rightNode==leftNode) {
          break;
        }
        childNode = do_QueryInterface(previousSiblingNode);
      } // end while loop 
    }
    // clean up the surrounding content to maintain vertical whitespace
    if (NS_SUCCEEDED(res))
    {
      // if the prior node is a <BR> and we did something to change vertical whitespacing, delete the <BR>
      nsCOMPtr<nsIDOMNode> brNode;
      res = GetPriorNode(leftNode, PR_TRUE, getter_AddRefs(brNode));
      if (NS_SUCCEEDED(res) && brNode)
      {
        nsCOMPtr<nsIContent> brContent = do_QueryInterface(brNode);
        if (brContent)
        {
          nsCOMPtr<nsIAtom> brContentTag;
          brContent->GetTag(*getter_AddRefs(brContentTag));
          if (nsIEditProperty::br==brContentTag.get()) {
            res = DeleteNode(brNode);
          }
        }
      }

      // if the next node is a <BR> and we did something to change vertical whitespacing, delete the <BR>
      if (NS_SUCCEEDED(res))
      {
        res = GetNextNode(rightNode, PR_TRUE, getter_AddRefs(brNode));
        if (NS_SUCCEEDED(res) && brNode)
        {
          nsCOMPtr<nsIContent> brContent = do_QueryInterface(brNode);
          if (brContent)
          {
            nsCOMPtr<nsIAtom> brContentTag;
            brContent->GetTag(*getter_AddRefs(brContentTag));
            if (nsIEditProperty::br==brContentTag.get()) {
              res = DeleteNode(brNode);
            }
          }
        }
      }
    }
    if ((NS_SUCCEEDED(res)) && (PR_TRUE==removeBlockParent))
    { // we determined we need to remove the previous block parent.  Do it!
      // go through list backwards so deletes don't interfere with the iteration
      nsCOMPtr<nsIDOMNodeList> childNodes;
      res = blockParentNode->GetChildNodes(getter_AddRefs(childNodes));
      if ((NS_SUCCEEDED(res)) && (childNodes))
      {
        nsCOMPtr<nsIDOMNode>grandParent;
        blockParentNode->GetParentNode(getter_AddRefs(grandParent));
        //PRInt32 offsetInParent;
        res = GetChildOffset(blockParentNode, grandParent, offsetInParent);
        PRUint32 childCount;
        childNodes->GetLength(&childCount);
        PRInt32 i=childCount-1;
        for ( ; ((NS_SUCCEEDED(res)) && (0<=i)); i--)
        {
          nsCOMPtr<nsIDOMNode> childNode;
          res = childNodes->Item(i, getter_AddRefs(childNode));
          if ((NS_SUCCEEDED(res)) && (childNode))
          {
            res = nsTextEditor::DeleteNode(childNode);
            if (NS_SUCCEEDED(res)) {
              res = nsTextEditor::InsertNode(childNode, grandParent, offsetInParent);
            }
          }
        }
        if (gNoisy) { printf("removing the old block parent %p\n", blockParentNode.get()); }
        res = nsTextEditor::DeleteNode(blockParentNode);
      }
    }
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::ReParentContentOfRange(nsIDOMRange *aRange, 
                                     nsString &aParentTag,
                                     BlockTransformationType aTranformation)
{
  if (!aRange) { return NS_ERROR_NULL_POINTER; }
  nsresult res;
  nsISupportsArray *blockSections;
  res = NS_NewISupportsArray(&blockSections);
  if ((NS_SUCCEEDED(res)) && blockSections)
  {
    res = GetBlockSectionsForRange(aRange, blockSections);
    if (NS_SUCCEEDED(res)) 
    {
      nsIDOMRange *subRange;
      subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
      while (subRange && (NS_SUCCEEDED(res)))
      {
        nsCOMPtr<nsIDOMNode>startParent;
        res = subRange->GetStartParent(getter_AddRefs(startParent));
        if (NS_SUCCEEDED(res) && startParent) 
        {
          if (gNoisy) { printf("ReParentContentOfRange calling ReParentContentOfNode\n"); }
          res = ReParentContentOfNode(startParent, aParentTag, aTranformation);
        }
        NS_RELEASE(subRange);
        blockSections->RemoveElementAt(0);
        subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
      }
    }
    NS_RELEASE(blockSections);
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::RemoveParagraphStyle()
{
  if (gNoisy) { 
    printf("---------- nsHTMLEditor::RemoveParagraphStyle ----------\n"); 
  }
  
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMSelection>selection;
  res = nsEditor::GetSelection(getter_AddRefs(selection));
  if ((NS_SUCCEEDED(res)) && selection)
  {
    nsAutoSelectionReset selectionResetter(selection);
    nsAutoEditBatch beginBatching(this);
    nsCOMPtr<nsIEnumerator> enumerator;
    enumerator = do_QueryInterface(selection);
    if (enumerator)
    {
      enumerator->First(); 
      nsISupports *currentItem;
      res = enumerator->CurrentItem(&currentItem);
      if ((NS_SUCCEEDED(res)) && (nsnull!=currentItem))
      {
        nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
        res = RemoveParagraphStyleFromRange(range);
      }
    }
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::RemoveParagraphStyleFromRange(nsIDOMRange *aRange)
{
  if (!aRange) { return NS_ERROR_NULL_POINTER; }
  nsresult res;
  nsISupportsArray *blockSections;
  res = NS_NewISupportsArray(&blockSections);
  if ((NS_SUCCEEDED(res)) && blockSections)
  {
    res = GetBlockSectionsForRange(aRange, blockSections);
    if (NS_SUCCEEDED(res)) 
    {
      nsIDOMRange *subRange;
      subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
      while (subRange && (NS_SUCCEEDED(res)))
      {
        res = RemoveParagraphStyleFromBlockContent(subRange);
        NS_RELEASE(subRange);
        blockSections->RemoveElementAt(0);
        subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
      }
    }
    NS_RELEASE(blockSections);
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::RemoveParagraphStyleFromBlockContent(nsIDOMRange *aRange)
{
  if (!aRange) { return NS_ERROR_NULL_POINTER; }
  nsresult res;
  nsCOMPtr<nsIDOMNode>startParent;
  aRange->GetStartParent(getter_AddRefs(startParent));
  nsCOMPtr<nsIDOMElement>blockParentElement;
  res = nsTextEditor::GetBlockParent(startParent, getter_AddRefs(blockParentElement));
  while ((NS_SUCCEEDED(res)) && blockParentElement)
  {
    nsAutoString blockParentTag;
    blockParentElement->GetTagName(blockParentTag);
    PRBool isSubordinateBlock;
    IsSubordinateBlock(blockParentTag, isSubordinateBlock);
    if (PR_FALSE==isSubordinateBlock) {
      break;
    }
    else 
    {
      // go through list backwards so deletes don't interfere with the iteration
      nsCOMPtr<nsIDOMNodeList> childNodes;
      res = blockParentElement->GetChildNodes(getter_AddRefs(childNodes));
      if ((NS_SUCCEEDED(res)) && (childNodes))
      {
        nsCOMPtr<nsIDOMNode>grandParent;
        blockParentElement->GetParentNode(getter_AddRefs(grandParent));
        PRInt32 offsetInParent;
        res = GetChildOffset(blockParentElement, grandParent, offsetInParent);
        PRUint32 childCount;
        childNodes->GetLength(&childCount);
        PRInt32 i=childCount-1;
        for ( ; ((NS_SUCCEEDED(res)) && (0<=i)); i--)
        {
          nsCOMPtr<nsIDOMNode> childNode;
          res = childNodes->Item(i, getter_AddRefs(childNode));
          if ((NS_SUCCEEDED(res)) && (childNode))
          {
            res = nsTextEditor::DeleteNode(childNode);
            if (NS_SUCCEEDED(res)) {
              res = nsTextEditor::InsertNode(childNode, grandParent, offsetInParent);
            }
          }
        }
        if (NS_SUCCEEDED(res)) {
          res = nsTextEditor::DeleteNode(blockParentElement);
        }
      }
    }
    res = nsTextEditor::GetBlockParent(startParent, getter_AddRefs(blockParentElement));
  }
  return res;
}


NS_IMETHODIMP 
nsHTMLEditor::RemoveParent(const nsString &aParentTag)
{
  if (gNoisy) { 
    printf("---------- nsHTMLEditor::RemoveParent ----------\n"); 
  }
  
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMSelection>selection;
  res = nsEditor::GetSelection(getter_AddRefs(selection));
  if ((NS_SUCCEEDED(res)) && selection)
  {
    nsAutoSelectionReset selectionResetter(selection);
    nsAutoEditBatch beginBatching(this);
    nsCOMPtr<nsIEnumerator> enumerator;
    enumerator = do_QueryInterface(selection);
    if (enumerator)
    {
      enumerator->First(); 
      nsISupports *currentItem;
      res = enumerator->CurrentItem(&currentItem);
      if ((NS_SUCCEEDED(res)) && (nsnull!=currentItem))
      {
        nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
        res = RemoveParentFromRange(aParentTag, range);
      }
    }
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::RemoveParentFromRange(const nsString &aParentTag, nsIDOMRange *aRange)
{
  if (!aRange) { return NS_ERROR_NULL_POINTER; }
  nsresult res;
  nsISupportsArray *blockSections;
  res = NS_NewISupportsArray(&blockSections);
  if ((NS_SUCCEEDED(res)) && blockSections)
  {
    res = GetBlockSectionsForRange(aRange, blockSections);
    if (NS_SUCCEEDED(res)) 
    {
      nsIDOMRange *subRange;
      subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
      while (subRange && (NS_SUCCEEDED(res)))
      {
        res = RemoveParentFromBlockContent(aParentTag, subRange);
        NS_RELEASE(subRange);
        blockSections->RemoveElementAt(0);
        subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
      }
    }
    NS_RELEASE(blockSections);
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::RemoveParentFromBlockContent(const nsString &aParentTag, nsIDOMRange *aRange)
{
  if (!aRange) { return NS_ERROR_NULL_POINTER; }
  nsresult res;
  nsCOMPtr<nsIDOMNode>startParent;
  res = aRange->GetStartParent(getter_AddRefs(startParent));
  if ((NS_SUCCEEDED(res)) && startParent)
  { 
    nsCOMPtr<nsIDOMNode>parentNode;
    nsCOMPtr<nsIDOMElement>parentElement;
    res = startParent->GetParentNode(getter_AddRefs(parentNode));
    while ((NS_SUCCEEDED(res)) && parentNode)
    {
      parentElement = do_QueryInterface(parentNode);
      nsAutoString parentTag;
      parentElement->GetTagName(parentTag);
      PRBool isRoot;
      IsRootTag(parentTag, isRoot);
      if (aParentTag.EqualsIgnoreCase(parentTag))
      {
        // go through list backwards so deletes don't interfere with the iteration
        nsCOMPtr<nsIDOMNodeList> childNodes;
        res = parentElement->GetChildNodes(getter_AddRefs(childNodes));
        if ((NS_SUCCEEDED(res)) && (childNodes))
        {
          nsCOMPtr<nsIDOMNode>grandParent;
          parentElement->GetParentNode(getter_AddRefs(grandParent));
          PRInt32 offsetInParent;
          res = GetChildOffset(parentElement, grandParent, offsetInParent);
          PRUint32 childCount;
          childNodes->GetLength(&childCount);
          PRInt32 i=childCount-1;
          for ( ; ((NS_SUCCEEDED(res)) && (0<=i)); i--)
          {
            nsCOMPtr<nsIDOMNode> childNode;
            res = childNodes->Item(i, getter_AddRefs(childNode));
            if ((NS_SUCCEEDED(res)) && (childNode))
            {
              res = nsTextEditor::DeleteNode(childNode);
              if (NS_SUCCEEDED(res)) {
                res = nsTextEditor::InsertNode(childNode, grandParent, offsetInParent);
              }
            }
          }
          if (NS_SUCCEEDED(res)) {
            res = nsTextEditor::DeleteNode(parentElement);
          }
        }
        break;
      }
      else if (PR_TRUE==isRoot) {  // hit a local root node, terminate loop
        break;
      }
      res = parentElement->GetParentNode(getter_AddRefs(parentNode));
    }
  }
  return res;
}


// TODO: Implement "outdent"
NS_IMETHODIMP
nsHTMLEditor::Indent(const nsString& aIndent)
{
  nsAutoEditBatch beginBatching(this);
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  
  // Find out if the selection is collapsed:
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res) || !selection) return res;

  nsAutoSelectionReset selectionResetter(selection);

  PRBool isCollapsed;
  res = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;

  res = GetStartNodeAndOffset(selection, &node, &offset);
  if (!node) res = NS_ERROR_FAILURE;
  if (NS_FAILED(res)) return res;
  
  nsAutoString inward("indent");
  if (aIndent == inward)
  {
  
    if (isCollapsed)
    {
      // have to find a place to put the blockquote
      nsCOMPtr<nsIDOMNode> parent = node;
      nsCOMPtr<nsIDOMNode> topChild = node;
      nsCOMPtr<nsIDOMNode> tmp;
      nsAutoString bq("blockquote");
      while ( !CanContainTag(parent, bq))
      {
        parent->GetParentNode(getter_AddRefs(tmp));
        if (!tmp) return NS_ERROR_FAILURE;
        topChild = parent;
        parent = tmp;
      }
    
      if (parent != node)
      {
        // we need to split up to the child of parent
        res = SplitNodeDeep(topChild, node, offset);
        if (NS_FAILED(res)) return res;
        // topChild already went to the right on the split
        // so we don't need to add one to offset when figuring
        // out where to plop list
        offset = GetIndexOf(parent,topChild);  
      }

      // make a blockquote
      nsCOMPtr<nsIDOMNode> newBQ;
      res = CreateNode(bq, parent, offset, getter_AddRefs(newBQ));
      if (NS_FAILED(res)) return res;
      // put a space in it so layout will draw the list item
      res = selection->Collapse(newBQ,0);
      if (NS_FAILED(res)) return res;
      nsAutoString theText(" ");
      res = InsertText(theText);
      if (NS_FAILED(res)) return res;
      // reposition selection to before the space character
      res = GetStartNodeAndOffset(selection, &node, &offset);
      if (NS_FAILED(res)) return res;
      res = selection->Collapse(node,0);
      if (NS_FAILED(res)) return res;
    }
  }
  
  return NS_OK;
}

//TODO: IMPLEMENT ALIGNMENT!

NS_IMETHODIMP
nsHTMLEditor::Align(const nsString& aAlignType)
{
  nsAutoEditBatch beginBatching(this);
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  
  // Find out if the selection is collapsed:
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res) || !selection) return res;

  nsAutoSelectionReset selectionResetter(selection);

  PRBool isCollapsed;
  res = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;

  res = GetStartNodeAndOffset(selection, &node, &offset);
  if (!node) res = NS_ERROR_FAILURE;
  if (NS_FAILED(res)) return res;
  
  nsAutoString leftStr("left");
  if (aAlignType == leftStr)
  {
  
    if (isCollapsed)
    {
    
    }
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLEditor::InsertList(const nsString& aListType)
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel= PR_FALSE;

  nsAutoEditBatch beginBatching(this);
  
  // pre-process
  nsEditor::GetSelection(getter_AddRefs(selection));
  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kMakeList);
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel);
  if (cancel || (NS_FAILED(res))) return res;

  // Find out if the selection is collapsed:
  if (NS_FAILED(res) || !selection) return res;

  nsAutoSelectionReset selectionResetter(selection);

  PRBool isCollapsed;
  res = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  
  res = GetStartNodeAndOffset(selection, &node, &offset);
  if (!node) res = NS_ERROR_FAILURE;
  if (NS_FAILED(res)) return res;
  
  if (isCollapsed)
  {
    // have to find a place to put the list
    nsCOMPtr<nsIDOMNode> parent = node;
    nsCOMPtr<nsIDOMNode> topChild = node;
    nsCOMPtr<nsIDOMNode> tmp;
    
    while ( !CanContainTag(parent, aListType))
    {
      parent->GetParentNode(getter_AddRefs(tmp));
      if (!tmp) return NS_ERROR_FAILURE;
      topChild = parent;
      parent = tmp;
    }
    
    if (parent != node)
    {
      // we need to split up to the child of parent
      res = SplitNodeDeep(topChild, node, offset);
      if (NS_FAILED(res)) return res;
      // topChild already went to the right on the split
      // so we don't need to add one to offset when figuring
      // out where to plop list
      offset = GetIndexOf(parent,topChild);  
    }

    // make a list
    nsCOMPtr<nsIDOMNode> newList;
    res = CreateNode(aListType, parent, offset, getter_AddRefs(newList));
    if (NS_FAILED(res)) return res;
    // make a list item
    nsAutoString tag("li");
    nsCOMPtr<nsIDOMNode> newItem;
    res = CreateNode(tag, newList, 0, getter_AddRefs(newItem));
    if (NS_FAILED(res)) return res;
    // put a space in it so layout will draw the list item
    res = selection->Collapse(newItem,0);
    if (NS_FAILED(res)) return res;
    nsAutoString theText(" ");
    res = InsertText(theText);
    if (NS_FAILED(res)) return res;
    // reposition selection to before the space character
    res = GetStartNodeAndOffset(selection, &node, &offset);
    if (NS_FAILED(res)) return res;
    res = selection->Collapse(node,0);
    if (NS_FAILED(res)) return res;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::InsertLink(nsString& aURL)
{
  nsAutoEditBatch beginBatching(this);

  // Find out if the selection is collapsed:
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res) || !selection)
  {
#ifdef DEBUG_akkana
    printf("Can't get selection!");
#endif
    return res;
  }
  PRBool isCollapsed;
  res = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res))
    isCollapsed = PR_TRUE;

  // Temporary: we need to save the contents of the selection,
  // then insert them back in as the child of the newly created
  // anchor node in order to put the link around the selection.
  // This will require copying the selection into a document fragment,
  // then splicing the document fragment back into the tree after the
  // new anchor node has been put in place.  As a temporary solution,
  // Copy/Paste does this for us in the text case
  // (and eventually in all cases).
  if (!isCollapsed)
    (void)Copy();

  nsCOMPtr<nsIDOMNode> newNode;
  nsAutoString tag("A");
  res = nsEditor::DeleteSelectionAndCreateNode(tag, getter_AddRefs(newNode));
  if (NS_FAILED(res) || !newNode)
    return res;

  nsCOMPtr<nsIDOMHTMLAnchorElement> anchor (do_QueryInterface(newNode));
  if (!anchor)
  {
#ifdef DEBUG_akkana
    printf("Not an anchor element\n");
#endif
    return NS_NOINTERFACE;
  }

  res = anchor->SetHref(aURL);
  if (NS_FAILED(res))
  {
#ifdef DEBUG_akkana
    printf("SetHref failed");
#endif
    return res;
  }

  // Set the selection to the node we just inserted:
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res) || !selection)
  {
#ifdef DEBUG_akkana
    printf("Can't get selection!");
#endif
    return res;
  }
  res = selection->Collapse(newNode, 0);
  if (NS_FAILED(res))
  {
#ifdef DEBUG_akkana
    printf("Couldn't collapse");
#endif
    return res;
  }

  // If we weren't collapsed, paste the old selection back in under the link:
  if (!isCollapsed)
    (void)Paste();
  // Otherwise (we were collapsed) insert some bogus text in
  // so the link will be visible
  else
  {
    nsString link("[***]");
    (void) InsertText(link);   // ignore return value -- we don't care
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::InsertImage(nsString& aURL,
                          nsString& aWidth, nsString& aHeight,
                          nsString& aHspace, nsString& aVspace,
                          nsString& aBorder,
                          nsString& aAlt,
                          nsString& aAlignment)
{
  nsresult res;
  nsCOMPtr<nsIDOMNode> newNode;

  nsCOMPtr<nsIDOMDocument>doc;
  res = GetDocument(getter_AddRefs(doc));
  if (NS_SUCCEEDED(res))
  {
    nsAutoString tag("IMG");
    nsCOMPtr<nsIDOMElement>newElement;
    res = doc->CreateElement(tag, getter_AddRefs(newElement));
    if (NS_SUCCEEDED(res) && newElement)
    {
      newNode = do_QueryInterface(newElement);
      nsCOMPtr<nsIDOMHTMLImageElement> image (do_QueryInterface(newNode));
      // Set all the attributes now, before we insert into the tree:
      if (image)
      {
        if (NS_SUCCEEDED(res = image->SetSrc(aURL)))
          if (NS_SUCCEEDED(res = image->SetWidth(aWidth)))
            if (NS_SUCCEEDED(res = image->SetHeight(aHeight)))
              if (NS_SUCCEEDED(res = image->SetAlt(aAlt)))
                if (NS_SUCCEEDED(res = image->SetBorder(aBorder)))
                  if (NS_SUCCEEDED(res = image->SetAlign(aAlignment)))
                    if (NS_SUCCEEDED(res = image->SetHspace(aHspace)))
                      if (NS_SUCCEEDED(res = image->SetVspace(aVspace)))
                        ;
      }
    }
  }

  // If any of these failed, then don't insert the new node into the tree
  if (NS_FAILED(res))
  {
#ifdef DEBUG_akkana
    printf("Some failure creating the new image node\n");
#endif
    return res;
  }

  //
  // Now we're ready to insert the new image node:
  // Starting now, don't return without ending the transaction!
  //
  nsAutoEditBatch beginBatching(this);

  nsCOMPtr<nsIDOMNode> parentNode;
  PRInt32 offsetOfNewNode;
  res = nsEditor::DeleteSelectionAndPrepareToCreateNode(parentNode,
                                                        offsetOfNewNode);
  if (NS_SUCCEEDED(res))
  {
    // and insert it into the right place in the tree:
    res = InsertNode(newNode, parentNode, offsetOfNewNode);
  }

  return res;
}

  // This should replace InsertLink and InsertImage once it is working
NS_IMETHODIMP
nsHTMLEditor::GetSelectedElement(const nsString& aTagName, nsIDOMElement** aReturn)
{
  if (!aReturn )
    return NS_ERROR_NULL_POINTER;
  
  *aReturn = nsnull;
  
  nsAutoString TagName = aTagName;
  TagName.ToLowerCase();
  
  //Note that this doesn't need to go through the transaction system

  nsresult res=NS_ERROR_NOT_INITIALIZED;
  //PRBool first=PR_TRUE;
  nsCOMPtr<nsIDOMSelection>selection;
  res = nsEditor::GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res) || !selection)
    return res;

  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);
  nsCOMPtr<nsIDOMElement> selectedElement;
  PRBool bNodeFound = PR_FALSE;

  // Don't bother to examine selection if it is collapsed
  if (!isCollapsed)
  {
    nsCOMPtr<nsIEnumerator> enumerator;
    enumerator = do_QueryInterface(selection);
    if (enumerator)
    {
      enumerator->First(); 
      nsISupports *currentItem;
      res = enumerator->CurrentItem(&currentItem);
      if ((NS_SUCCEEDED(res)) && currentItem)
      {
        nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
        nsCOMPtr<nsIContentIterator> iter;
        res = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                                    nsIContentIterator::GetIID(), 
                                                    getter_AddRefs(iter));
        if ((NS_SUCCEEDED(res)) && iter)
        {
          iter->Init(range);
          // loop through the content iterator for each content node
          nsCOMPtr<nsIContent> content;
          res = iter->CurrentNode(getter_AddRefs(content));
          //PRBool bOtherNodeTypeFound = PR_FALSE;

          while (NS_COMFALSE == iter->IsDone())
          {
             // Query interface to cast nsIContent to nsIDOMNode
             //  then get tagType to compare to  aTagName
             // Clone node of each desired type and append it to the aDomFrag
            selectedElement = do_QueryInterface(content);
            if (selectedElement)
            {
              // If we already found a node, then we have another element,
              //   so don't return an element
              if (bNodeFound)
              {
                //bNodeFound;
                break;
              }

              nsAutoString domTagName;
              selectedElement->GetNodeName(domTagName);
              domTagName.ToLowerCase();

              // The "A" tag is a pain,
              //  used for both link(href is set) and "Named Anchor"
              if (TagName == "href" || (TagName == "anchor"))
              {
                // We could use GetAttribute, but might as well use anchor element directly
                nsCOMPtr<nsIDOMHTMLAnchorElement> anchor = do_QueryInterface(selectedElement);
                if (anchor)
                {
                  nsString tmpText;
                  if( TagName == "href")
                  {
                    if (NS_SUCCEEDED(anchor->GetHref(tmpText)) && tmpText.GetUnicode() && tmpText.Length() != 0)
                      bNodeFound = PR_TRUE;
                  } else if (TagName == "anchor")
                  {
                    if (NS_SUCCEEDED(anchor->GetName(tmpText)) && tmpText.GetUnicode() && tmpText.Length() != 0)
                      bNodeFound = PR_TRUE;
                  }
                } else if (TagName == "href")
                {
                  // Check for a single image is inside a link
                  // It is usually the immediate parent, but lets be sure
                  //  by walking up the parents until we find an "A" tag
                  nsCOMPtr<nsIDOMHTMLImageElement> image = do_QueryInterface(selectedElement);
                  if (image)
                  {
                    nsCOMPtr<nsIDOMNode> parent;
                    nsCOMPtr<nsIDOMNode> current = do_QueryInterface(selectedElement); 
                    PRBool notDone = PR_TRUE;
                    do {
                      res = current->GetParentNode(getter_AddRefs(parent));
                      notDone = NS_SUCCEEDED(res) && parent != nsnull;
                      if(notDone)
                      {
                        nsString tmpText;
                        /*nsCOMPtr<nsIDOMHTMLAnchorElement>*/
                        anchor = do_QueryInterface(parent);
                        if (anchor && NS_SUCCEEDED(anchor->GetHref(tmpText)) && tmpText.GetUnicode() && tmpText.Length() != 0)
                        {
                          
                          nsCOMPtr<nsIDOMElement> link = do_QueryInterface(parent);
                          if (link)
                          {
                            *aReturn =link;
                          }
                          return NS_OK;
                        }
                      }
                      current = parent;
                    } while (notDone);
                  }
                }
              } else if (TagName == domTagName) { // All other tag names are handled here
                bNodeFound = PR_TRUE;
              }
              if (!bNodeFound)
              {
                // Check if node we have is really part of the selection???
                break;
              }
            }
            iter->Next();
          }
        }
      } else {
        // Should never get here?
        isCollapsed = PR_TRUE;
        printf("isCollapsed was FALSE, but no elements found in selection\n");
      }
    } else {
      printf("Could not create enumerator for GetSelectionProperties\n");
    }
  }
  if (bNodeFound)
  {
    
    *aReturn = selectedElement;  
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::CreateElementWithDefaults(const nsString& aTagName, nsIDOMElement** aReturn)
{
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  if (aReturn)
    *aReturn = nsnull;

  if (aTagName == "" || !aReturn)
    return NS_ERROR_NULL_POINTER;

  nsAutoString TagName = aTagName;
  TagName.ToLowerCase();
  nsAutoString realTagName;

  PRBool isHREF = (TagName.Equals("href"));
  PRBool isAnchor = (TagName.Equals("anchor"));
  if (isHREF || isAnchor)
  {
    realTagName = "a";
  } else {
    realTagName = TagName;
  }
  //We don't use editor's CreateElement because we don't want to 
  //  go through the transaction system

  nsCOMPtr<nsIDOMElement>newElement;
  res = mDoc->CreateElement(realTagName, getter_AddRefs(newElement));
  if (NS_FAILED(res) || !newElement)
    return NS_ERROR_FAILURE;


  // Set default values for new elements
  // SHOULD THIS BE PUT IN "RULES" SYSTEM OR
  //  ATTRIBUTES SAVED IN PREFS?
  if (isAnchor)
  {

    // TODO: Get the text of the selection and build a suggested Name
    //  Replace spaces with "_" 
  } else if (TagName.Equals("hr"))
  {
    
  }
  
  // ADD OTHER DEFAULT ATTRIBUTES HERE

  if (NS_SUCCEEDED(res))
  {
    *aReturn = newElement;
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::InsertElement(nsIDOMElement* aElement, PRBool aDeleteSelection, nsIDOMElement** aReturn)
{
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  if (aReturn)
    *aReturn = nsnull;
  
  if (!aElement || !aReturn)
    return NS_ERROR_NULL_POINTER;
  
  nsAutoEditBatch beginBatching(this);

  nsCOMPtr<nsIDOMNode> parentSelectedNode;
  PRInt32 offsetOfNewNode;

  // Clear current selection.
  // Should put caret at anchor point?
  if (!aDeleteSelection)
  {
    nsCOMPtr<nsIDOMSelection>selection;
    res = nsEditor::GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(res) && selection)
    {
      selection->ClearSelection();    
    }
  }

  res = DeleteSelectionAndPrepareToCreateNode(parentSelectedNode, offsetOfNewNode);
  if (NS_SUCCEEDED(res))
  {
    nsCOMPtr<nsIDOMNode> newNode = do_QueryInterface(aElement);
    res = InsertNode(aElement, parentSelectedNode, offsetOfNewNode);
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::InsertLinkAroundSelection(nsIDOMElement* aAnchorElement)
{
  nsresult res=NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMSelection> selection;

  // DON'T RETURN EXCEPT AT THE END -- WE NEED TO RELEASE THE aAnchorElement
  if (!aAnchorElement)
    goto DELETE_ANCHOR;

  // We must have a real selection
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res) || !selection)
    goto DELETE_ANCHOR;

  PRBool isCollapsed;
  res = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res))
    isCollapsed = PR_TRUE;
  
  if (isCollapsed)
  {
    printf("InsertLinkAroundSelection called but there is no selection!!!\n");     
    res = NS_OK;
  } else {
  
    // Be sure we were given an anchor element
    nsCOMPtr<nsIDOMHTMLAnchorElement> anchor = do_QueryInterface(aAnchorElement);
    if (anchor)
    {
      nsAutoString href;
      if (NS_SUCCEEDED(anchor->GetHref(href)) && href.GetUnicode() && href.Length() > 0)      
      {
        nsAutoEditBatch beginBatching(this);
        const nsString attribute("href");
        SetTextProperty(nsIEditProperty::a, &attribute, &href);
        //TODO: Enumerate through other properties of the anchor tag
        // and set those as well. 
        // Optimization: Modify SetTextProperty to set all attributes at once?
      }
    }
  }
DELETE_ANCHOR:
  // We don't insert the element created in CreateElementWithDefaults 
  //   into the document like we do in InsertElement,
  //   so shouldn't we have to do this here?
  //  It crashes in JavaScript if we do this!
  //NS_RELEASE(aAnchorElement);
  return res;
}

PRBool nsHTMLEditor::IsElementInBody(nsIDOMElement* aElement)
{
  if ( aElement )
  {
    nsIDOMElement* bodyElement = nsnull;
    nsresult res = nsEditor::GetBodyElement(&bodyElement);
    if (NS_SUCCEEDED(res) && bodyElement)
    {
      nsCOMPtr<nsIDOMNode> parent;
      nsCOMPtr<nsIDOMNode> currentElement = do_QueryInterface(aElement);
      if (currentElement)
      {
        do {
          currentElement->GetParentNode(getter_AddRefs(parent));
          if (parent)
          {
            if (parent == bodyElement)
              return PR_TRUE;

            currentElement = parent;
          }
        } while(parent);
      }
    }  
  }
  return PR_FALSE;
}

NS_IMETHODIMP
nsHTMLEditor::SelectElement(nsIDOMElement* aElement)
{
  nsresult res = NS_ERROR_NULL_POINTER;

  // Must be sure that element is contained in the document body
  if (IsElementInBody(aElement))
  {
    nsCOMPtr<nsIDOMSelection> selection;
    res = nsEditor::GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(res) && selection)
    {
      nsCOMPtr<nsIDOMNode>parent;
      res = aElement->GetParentNode(getter_AddRefs(parent));
      if (NS_SUCCEEDED(res) && parent)
      {
        PRInt32 offsetInParent;
        res = GetChildOffset(aElement, parent, offsetInParent);

        if (NS_SUCCEEDED(res))
        {
          // Collapse selection to just before desired element,
          selection->Collapse(parent, offsetInParent);
          //  then extend it to just after
          selection->Extend(parent, offsetInParent+1);
        }
      }
    }
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::SetCaretAfterElement(nsIDOMElement* aElement)
{
  nsresult res = NS_ERROR_NULL_POINTER;

  // Must be sure that element is contained in the document body
  if (IsElementInBody(aElement))
  {
    nsCOMPtr<nsIDOMSelection> selection;
    res = nsEditor::GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(res) && selection)
    {
      nsCOMPtr<nsIDOMNode>parent;
      res = aElement->GetParentNode(getter_AddRefs(parent));
      if (NS_SUCCEEDED(res) && parent)
      {
        PRInt32 offsetInParent;
        res = GetChildOffset(aElement, parent, offsetInParent);

        if (NS_SUCCEEDED(res))
        {
          // Collapse selection to just after desired element,
          selection->Collapse(parent, offsetInParent+1);
        }
      }
    }
  }
  return res;
}


PRBool 
nsHTMLEditor::CanContainTag(nsIDOMNode* aParent, const nsString &aTag)
{
  if (!aParent) return PR_FALSE;
  
  static nsAutoString ulTag = "ul";
  static nsAutoString olTag = "ol";
  static nsAutoString liTag = "li";
  static nsAutoString bodyTag = "body";
  static nsAutoString tdTag = "td";
  static nsAutoString thTag = "th";
  static nsAutoString bqTag = "blockquote";

  nsCOMPtr<nsIAtom> pTagAtom = GetTag(aParent);
  nsAutoString pTag;
  pTagAtom->ToString(pTag);

  // flesh this out...
  // for now, only lists and blockquotes are using this funct
  
  if (aTag.EqualsIgnoreCase(ulTag) ||
      aTag.EqualsIgnoreCase(olTag) )
  {
    if (pTag.EqualsIgnoreCase(bodyTag) ||
        pTag.EqualsIgnoreCase(tdTag) ||
        pTag.EqualsIgnoreCase(thTag) ||
        pTag.EqualsIgnoreCase(ulTag) ||
        pTag.EqualsIgnoreCase(olTag) ||
        pTag.EqualsIgnoreCase(liTag) ||
        pTag.EqualsIgnoreCase(bqTag) )
    {
      return PR_TRUE;
    }
  }
  else if (aTag.EqualsIgnoreCase(bqTag) )
  {
    if (pTag.EqualsIgnoreCase(bodyTag) ||
        pTag.EqualsIgnoreCase(tdTag) ||
        pTag.EqualsIgnoreCase(thTag) ||
        pTag.EqualsIgnoreCase(liTag) ||
        pTag.EqualsIgnoreCase(bqTag) )
    {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}


NS_IMETHODIMP
nsHTMLEditor::IsRootTag(nsString &aTag, PRBool &aIsTag)
{
  static nsAutoString bodyTag = "body";
  static nsAutoString tdTag = "td";
  static nsAutoString thTag = "th";
  static nsAutoString captionTag = "caption";
  if (PR_TRUE==aTag.EqualsIgnoreCase(bodyTag) ||
      PR_TRUE==aTag.EqualsIgnoreCase(tdTag) ||
      PR_TRUE==aTag.EqualsIgnoreCase(thTag) ||
      PR_TRUE==aTag.EqualsIgnoreCase(captionTag) )
  {
    aIsTag = PR_TRUE;
  }
  else {
    aIsTag = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::IsSubordinateBlock(nsString &aTag, PRBool &aIsTag)
{
  static nsAutoString p = "p";
  static nsAutoString h1 = "h1";
  static nsAutoString h2 = "h2";
  static nsAutoString h3 = "h3";
  static nsAutoString h4 = "h4";
  static nsAutoString h5 = "h5";
  static nsAutoString h6 = "h6";
  static nsAutoString address = "address";
  static nsAutoString pre = "pre";
  static nsAutoString li = "li";
  static nsAutoString dt = "dt";
  static nsAutoString dd = "dd";
  if (PR_TRUE==aTag.EqualsIgnoreCase(p)  ||
      PR_TRUE==aTag.EqualsIgnoreCase(h1) ||
      PR_TRUE==aTag.EqualsIgnoreCase(h2) ||
      PR_TRUE==aTag.EqualsIgnoreCase(h3) ||
      PR_TRUE==aTag.EqualsIgnoreCase(h4) ||
      PR_TRUE==aTag.EqualsIgnoreCase(h5) ||
      PR_TRUE==aTag.EqualsIgnoreCase(h6) ||
      PR_TRUE==aTag.EqualsIgnoreCase(address) ||
      PR_TRUE==aTag.EqualsIgnoreCase(pre) ||
      PR_TRUE==aTag.EqualsIgnoreCase(li) ||
      PR_TRUE==aTag.EqualsIgnoreCase(dt) ||
      PR_TRUE==aTag.EqualsIgnoreCase(dd) )
  {
    aIsTag = PR_TRUE;
  }
  else {
    aIsTag = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP nsHTMLEditor::BeginComposition(void)
{
	return nsTextEditor::BeginComposition();
}

NS_IMETHODIMP nsHTMLEditor::EndComposition(void)
{
	return nsTextEditor::EndComposition();
}

NS_IMETHODIMP nsHTMLEditor::SetCompositionString(const nsString& aCompositionString)
{
	return nsTextEditor::SetCompositionString(aCompositionString);
}

// Call the platform-specific file picker and convert it to URL format
NS_IMETHODIMP nsHTMLEditor::GetLocalFileURL(nsIDOMWindow* aParent, const nsString& aFilterType, nsString& aReturn)
{
  PRBool htmlFilter = aFilterType.EqualsIgnoreCase("html");
  PRBool imgFilter = aFilterType.EqualsIgnoreCase("img");

  aReturn = "";

  // TODO: DON'T ACCEPT NULL PARENT AFTER WIDGET IS FIXED
  if (/*!aParent||*/ !(htmlFilter || imgFilter))
    return NS_ERROR_NOT_INITIALIZED;


  nsCOMPtr<nsIFileWidget>  fileWidget;
  nsAutoString title("");

  // Get strings from editor resource bundle
  nsString name;
  if (htmlFilter)
  {
    name = "OpenHTMLFile";
  } else if (imgFilter)
  {
    name = "SelectImageFile";
  }
  GetString(name, title);
    
  nsFileSpec fileSpec;
  // TODO: GET THE DEFAULT DIRECTORY FOR DIFFERENT TYPES FROM PREFERENCES
  nsFileSpec aDisplayDirectory;

  nsresult res = nsComponentManager::CreateInstance(kFileWidgetCID,
					     nsnull,
					     nsIFileWidget::GetIID(),
					     (void**)&fileWidget);

  if (NS_SUCCEEDED(res))
  {
    nsFileDlgResults dialogResult;
    if (htmlFilter)
    {
      nsAutoString titles[] = {"HTML Files"};
      nsAutoString filters[] = {"*.htm; *.html; *.shtml"};
      fileWidget->SetFilterList(1, titles, filters);
      dialogResult = fileWidget->GetFile(nsnull, title, fileSpec);
    } else {
      nsAutoString titles[] = {"Image Files"};
      nsAutoString filters[] = {"*.gif; *.jpg; *.jpeg; *.png"};
      fileWidget->SetFilterList(1, titles, filters);
      dialogResult = fileWidget->GetFile(nsnull, title, fileSpec);
    }
    // Do this after we get this from preferences
    //fileWidget->SetDisplayDirectory(aDisplayDirectory);
    // First param should be Parent window, but type is nsIWidget*
    // Bug is filed to change this to a more suitable window type
    if (dialogResult != nsFileDlgResults_Cancel) 
    {
      // Get the platform-specific format
      // Convert it to the string version of the URL format
      // NOTE: THIS CRASHES IF fileSpec is empty
      nsFileURL url(fileSpec);
      aReturn = url.GetURLString();
    }
    // TODO: SAVE THIS TO THE PREFS?
    fileWidget->GetDisplayDirectory(aDisplayDirectory);
  }

  return res;
}

NS_IMETHODIMP
nsHTMLEditor::DebugUnitTests(PRInt32 *outNumTests, PRInt32 *outNumTestsFailed)
{
#ifdef DEBUG
  if (!outNumTests || !outNumTestsFailed)
    return NS_ERROR_NULL_POINTER;
  
  // first, run the text editor tests (is this appropriate?)
  nsresult rv = nsTextEditor::DebugUnitTests(outNumTests, outNumTestsFailed);
  if (NS_FAILED(rv))
    return rv;
  
  // now run our tests
  
  
  
  *outNumTests += 0;
  *outNumTestsFailed += 0;
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

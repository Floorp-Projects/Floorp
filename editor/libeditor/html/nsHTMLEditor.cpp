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
#include "nsIDOMText.h"
#include "nsIDOMNodeList.h"
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
#include "nsIDOMNSRange.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsFileSpec.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsIDocumentEncoder.h"
#include "nsIPref.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIPresShell.h"
#include "nsIImage.h"

// Drag & Drop, Clipboard
#include "nsWidgetsCID.h"
#include "nsIClipboard.h"
#include "nsITransferable.h"

#include "prprf.h"

const unsigned char nbsp = 160;

#ifdef ENABLE_JS_EDITOR_LOG
#include "nsJSEditorLog.h"
#endif // ENABLE_JS_EDITOR_LOG

// HACK - CID for NavDTD until we can get at dtd via the document
// {a6cf9107-15b3-11d2-932e-00805f8add32}
#define NS_CNAVDTD_CID \
{ 0xa6cf9107, 0x15b3, 0x11d2, { 0x93, 0x2e, 0x0, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }
static NS_DEFINE_CID(kCNavDTDCID,	  NS_CNAVDTD_CID);

static NS_DEFINE_CID(kEditorCID,      NS_EDITOR_CID);
static NS_DEFINE_CID(kTextEditorCID,  NS_TEXTEDITOR_CID);
static NS_DEFINE_CID(kHTMLEditorCID,  NS_HTMLEDITOR_CID);
static NS_DEFINE_CID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_CID(kCRangeCID,      NS_RANGE_CID);
static NS_DEFINE_IID(kFileWidgetCID,  NS_FILEWIDGET_CID);

// Drag & Drop, Clipboard Support
static NS_DEFINE_CID(kCClipboardCID,    NS_CLIPBOARD_CID);
static NS_DEFINE_CID(kCTransferableCID, NS_TRANSFERABLE_CID);

#ifdef NS_DEBUG
static PRBool gNoisy = PR_FALSE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif

// Some utilities to handle stupid overloading of "A" tag for link and named anchor
static char hrefText[] = "href";
static char linkText[] = "link";
static char anchorText[] = "anchor";
static char namedanchorText[] = "namedanchor";

#define IsLink(s) (s.EqualsIgnoreCase(hrefText) || s.EqualsIgnoreCase(linkText))
#define IsNamedAnchor(s) (s.EqualsIgnoreCase(anchorText) || s.EqualsIgnoreCase(namedanchorText))

static PRBool IsLinkNode(nsIDOMNode *aNode)
{
  if (aNode)
  {
    nsCOMPtr<nsIDOMHTMLAnchorElement> anchor = do_QueryInterface(aNode);
    if (anchor)
    {
      nsString tmpText;
      if (NS_SUCCEEDED(anchor->GetHref(tmpText)) && tmpText.GetUnicode() && tmpText.Length() != 0)
        return PR_TRUE;
    }
  }
  return PR_FALSE;
}

static PRBool IsNamedAnchorNode(nsIDOMNode *aNode)
{
  if (aNode)
  {
    nsCOMPtr<nsIDOMHTMLAnchorElement> anchor = do_QueryInterface(aNode);
    if (anchor)
    {
      nsString tmpText;
      if (NS_SUCCEEDED(anchor->GetName(tmpText)) && tmpText.GetUnicode() && tmpText.Length() != 0)
        return PR_TRUE;
    }
  }
  return PR_FALSE;
}

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
    res = nsTextEditor::Init(aDoc, aPresShell);
    if (NS_SUCCEEDED(res))
    {
      // Set up a DTD    XXX XXX 
      // HACK: This should have happened in a document specific way
      //       in nsEditor::Init(), but we dont' have a way to do that yet
      res = nsComponentManager::CreateInstance(kCNavDTDCID, nsnull,
                                              nsIDTD::GetIID(), getter_AddRefs(mDTD));
      if (!mDTD) res = NS_ERROR_FAILURE;
    }
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
//  nsresult result;
  NS_ASSERTION(mDoc, "Missing Editor DOM Document");
  
  // TODO: Check selection for Cell, Row, Column or table and do color on appropriate level
  // For initial testing, just set the background on the BODY tag (the document's background)

// Do this only if setting a table or cell background
// It will be called in nsTextEditor::SetBackgroundColor for the page background
#if 0 //def ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->SetBackgroundColor(aColor);
#endif // ENABLE_JS_EDITOR_LOG

  NS_ASSERTION(mDoc, "Missing Editor DOM Document");
  
  // TODO: Check selection for Cell, Row, Column or table and do color on appropriate level
  // For initial testing, just set the background on the BODY tag (the document's background)

  return nsTextEditor::SetBackgroundColor(aColor);
}

NS_IMETHODIMP nsHTMLEditor::SetBodyAttribute(const nsString& aAttribute, const nsString& aValue)
{

#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->SetBodyAttribute(aAttribute, aValue);
#endif // ENABLE_JS_EDITOR_LOG

  nsresult res;
  // TODO: Check selection for Cell, Row, Column or table and do color on appropriate level

  NS_ASSERTION(mDoc, "Missing Editor DOM Document");
  
  // Set the background color attribute on the body tag
  nsCOMPtr<nsIDOMElement> bodyElement;

  res = nsEditor::GetBodyElement(getter_AddRefs(bodyElement));
  if (NS_SUCCEEDED(res) && bodyElement)
  {
    // Use the editor's method which goes through the transaction system
    SetAttribute(bodyElement, aAttribute, aValue);
  }
  return res;
}

NS_IMETHODIMP nsHTMLEditor::InsertBreak()
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->InsertBreak();
#endif // ENABLE_JS_EDITOR_LOG

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

  return res;
}

NS_IMETHODIMP nsHTMLEditor::GetParagraphFormat(nsString& aParagraphFormat)
{
  nsresult res = NS_ERROR_NOT_INITIALIZED;

  return res;
}

NS_IMETHODIMP nsHTMLEditor::SetParagraphFormat(const nsString& aParagraphFormat)
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->SetParagraphFormat(aParagraphFormat);
#endif // ENABLE_JS_EDITOR_LOG

  nsresult res = NS_ERROR_NOT_INITIALIZED;
  //Kinda sad to waste memory just to force lower case
  nsAutoString tag = aParagraphFormat;
  tag.ToLowerCase();
  if (tag == "normal" || tag == "p") {
    res = RemoveParagraphStyle();
  } else if (tag == "li") {
    res = InsertList("ul");
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

NS_IMETHODIMP nsHTMLEditor::BeginningOfDocument()
{
  return nsEditor::BeginningOfDocument();
}

NS_IMETHODIMP nsHTMLEditor::EndOfDocument()
{
  return nsEditor::EndOfDocument();
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
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->Paste();
#endif // ENABLE_JS_EDITOR_LOG

  nsString stuffToPaste;

  // Get Clipboard Service
  nsIClipboard* clipboard;
  nsresult rv = nsServiceManager::GetService(kCClipboardCID,
                                             nsIClipboard::GetIID(),
                                             (nsISupports **)&clipboard);

  // Create generic Transferable for getting the data
  nsCOMPtr<nsITransferable> trans;
  rv = nsComponentManager::CreateInstance(kCTransferableCID, nsnull, 
                                          nsITransferable::GetIID(), 
                                          (void**) getter_AddRefs(trans));
  if (NS_SUCCEEDED(rv))
  {
    // Get the nsITransferable interface for getting the data from the clipboard
    if (trans)
    {
      // Create the desired DataFlavor for the type of data
      // we want to get out of the transferable
      nsAutoString htmlFlavor(kHTMLMime);
      nsAutoString textFlavor(kTextMime);
      nsAutoString imageFlavor(kJPEGImageMime);

      trans->AddDataFlavor(&htmlFlavor);
      trans->AddDataFlavor(&textFlavor);
      trans->AddDataFlavor(&imageFlavor);

      // Get the Data from the clipboard
      if (NS_SUCCEEDED(clipboard->GetData(trans)))
      {
        nsAutoString flavor;
        char *       data;
        PRUint32     len;
        if (NS_SUCCEEDED(trans->GetAnyTransferData(&flavor, (void **)&data, &len)))
        {
#ifdef DEBUG_akkana
          printf("Got flavor [%s]\n", flavor.ToNewCString());
#endif
          if (flavor.Equals(htmlFlavor))
          {
            if (data && len > 0) // stuffToPaste is ready for insertion into the content
            {
              stuffToPaste.SetString(data, len);
              rv = InsertHTML(stuffToPaste);
            }
          }
          else if (flavor.Equals(textFlavor))
          {
            if (data && len > 0) // stuffToPaste is ready for insertion into the content
            {
              stuffToPaste.SetString(data, len);
              rv = InsertText(stuffToPaste);
            }
          }
          else if (flavor.Equals(imageFlavor))
          {
            // Insert Image code here
            printf("Don't know how to insert an image yet!\n");
            //nsIImage* image = (nsIImage *)data;
            //NS_RELEASE(image);
            rv = NS_ERROR_FAILURE; // for now give error code
          }
        }

      }
    }
  }
  nsServiceManager::ReleaseService(kCClipboardCID, clipboard);

  return rv;
}

// 
// HTML PasteAsQuotation: Paste in a blockquote type=cite
//
NS_IMETHODIMP nsHTMLEditor::PasteAsQuotation()
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->PasteAsQuotation();
#endif // ENABLE_JS_EDITOR_LOG

  nsAutoString citation("");
  return PasteAsCitedQuotation(citation);
}

NS_IMETHODIMP nsHTMLEditor::PasteAsCitedQuotation(const nsString& aCitation)
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->PasteAsCitedQuotation(aCitation);
#endif // ENABLE_JS_EDITOR_LOG

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
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->InsertAsQuotation(aQuotedText);
#endif // ENABLE_JS_EDITOR_LOG

  nsAutoString citation ("");
  return InsertAsCitedQuotation(aQuotedText, citation);
}

NS_IMETHODIMP nsHTMLEditor::InsertAsCitedQuotation(const nsString& aQuotedText,
                                                   const nsString& aCitation)
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->InsertAsCitedQuotation(aQuotedText, aCitation);
#endif // ENABLE_JS_EDITOR_LOG

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
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->InsertHTML(aInputString);
#endif // ENABLE_JS_EDITOR_LOG

  nsAutoEditBatch beginBatching(this);

  nsresult res;
  nsCOMPtr<nsIDOMNode> parentNode;
  PRInt32 offsetOfNewNode;
  res = DeleteSelectionAndPrepareToCreateNode(parentNode, offsetOfNewNode);

  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res))
    return res;

    // Get the first range in the selection, for context:
  nsCOMPtr<nsIDOMRange> range;
  res = selection->GetRangeAt(0, getter_AddRefs(range));
  if (NS_FAILED(res))
    return res;

  nsCOMPtr<nsIDOMNSRange> nsrange (do_QueryInterface(range));
  if (!nsrange)
    return NS_ERROR_NO_INTERFACE;

  nsCOMPtr<nsIDOMDocumentFragment> docfrag;
  res = nsrange->CreateContextualFragment(aInputString,
                                          getter_AddRefs(docfrag));
  if (NS_FAILED(res))
  {
#ifdef DEBUG
    printf("Couldn't create contextual fragment: error was %d\n", res);
#endif
    return res;
  }

#if defined(DEBUG_akkana)
  printf("============ Fragment dump :===========\n");

  nsCOMPtr<nsIContent> fragc (do_QueryInterface(docfrag));
  if (!fragc)
    printf("Couldn't get fragment is nsIContent\n");
  else
    fragc->List(stdout);
#endif

  // Insert the contents of the document fragment:
  nsCOMPtr<nsIDOMNode> fragmentAsNode (do_QueryInterface(docfrag));
#define INSERT_FRAGMENT_DIRECTLY 1
#ifdef INSERT_FRAGMENT_DIRECTLY
  // Make a collapsed range pointing to right after the current selection,
  // and let range gravity keep track of where it is so that we can
  // set the selection back there after the insert.
  // Unfortunately this doesn't work right yet.
  nsCOMPtr<nsIDOMRange> saverange;
  res = selection->GetRangeAt(0, getter_AddRefs(saverange));

  // Insert the node:
  res = InsertNode(fragmentAsNode, parentNode, offsetOfNewNode);

  // Now collapse the selection to the beginning of what we just inserted;
  // would be better to set it to the end.
  if (saverange)
  {
    nsCOMPtr<nsIDOMNode> parent;
    PRInt32 offset;
    if (NS_SUCCEEDED(saverange->GetEndParent(getter_AddRefs(parent))))
      if (NS_SUCCEEDED(saverange->GetEndOffset(&offset)))
        selection->Collapse(parent, offset);
  }
  else
    selection->Collapse(parentNode, 0/*offsetOfNewNode*/);
#else /* INSERT_FRAGMENT_DIRECTLY */
  // Loop over the contents of the fragment:
  nsCOMPtr<nsIDOMNode> child;
  res = fragmentAsNode->GetFirstChild(getter_AddRefS(child));
  if (NS_FAILED(res))
  {
    printf("GetFirstChild failed!\n");
    return res;
  }
  while (child)
  {
    res = InsertNode(child, parentNode, offsetOfNewNode++);
    if (NS_FAILED(res))
      break;
    nsCOMPtr<nsIDOMNode> nextSib;
    if (NS_FAILED(child->GetNextSibling(getter_AddRefs(nextSib))))
      /*break*/;
    child = nextSib;
  }
  if (NS_FAILED(res))
    return res;

  // Now collapse the selection to the end of what we just inserted:
  selection->Collapse(parentNode, offsetOfNewNode);
#endif /* INSERT_FRAGMENT_DIRECTLY */
  return res;
}


NS_IMETHODIMP nsHTMLEditor::OutputToString(nsString& aOutputString,
                                           const nsString& aFormatType,
                                           PRUint32 aFlags)
{
  return nsTextEditor::OutputToString(aOutputString, aFormatType, aFlags);
}

NS_IMETHODIMP nsHTMLEditor::OutputToStream(nsIOutputStream* aOutputStream,
                                           const nsString& aFormatType,
                                           const nsString* aCharset,
                                           PRUint32 aFlags)
{
  return nsTextEditor::OutputToStream(aOutputStream, aFormatType,
                                      aCharset, aFlags);
}

NS_IMETHODIMP
nsHTMLEditor::CopyAttributes(nsIDOMNode *aDestNode, nsIDOMNode *aSourceNode)
{
  return nsTextEditor::CopyAttributes(aDestNode, aSourceNode);
}

NS_IMETHODIMP
nsHTMLEditor::ApplyStyleSheet(const nsString& aURL)
{
  return nsTextEditor::ApplyStyleSheet(aURL);
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
    res = selection->GetEnumerator(getter_AddRefs(enumerator));
    if (NS_SUCCEEDED(res) && enumerator)
    {
      enumerator->First(); 
      nsCOMPtr<nsISupports> currentItem;
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if ((NS_SUCCEEDED(res)) && (currentItem))
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
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->AddBlockParent(aParentTag);
#endif // ENABLE_JS_EDITOR_LOG

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
      nsCOMPtr<nsISupports> currentItem;
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if ((NS_SUCCEEDED(res)) && (currentItem))
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
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->ReplaceBlockParent(aParentTag);
#endif // ENABLE_JS_EDITOR_LOG

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
    res = selection->GetEnumerator(getter_AddRefs(enumerator));
    if (NS_SUCCEEDED(res) && enumerator)
    {
      enumerator->First(); 
      nsCOMPtr<nsISupports> currentItem;
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if ((NS_SUCCEEDED(res)) && (currentItem))
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
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->RemoveParagraphStyle();
#endif // ENABLE_JS_EDITOR_LOG

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
    res = selection->GetEnumerator(getter_AddRefs(enumerator));
    if (NS_SUCCEEDED(res) && enumerator)
    {
      enumerator->First(); 
      nsCOMPtr<nsISupports> currentItem;
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if ((NS_SUCCEEDED(res)) && (currentItem))
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
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->RemoveParent(aParentTag);
#endif // ENABLE_JS_EDITOR_LOG

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
    res = selection->GetEnumerator(getter_AddRefs(enumerator));
    if (NS_SUCCEEDED(res) && enumerator)
    {
      enumerator->First(); 
      nsCOMPtr<nsISupports> currentItem;
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if ((NS_SUCCEEDED(res)) && (currentItem))
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
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->Indent(aIndent);
#endif // ENABLE_JS_EDITOR_LOG
  
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  PRBool cancel= PR_FALSE;

  nsAutoEditBatch beginBatching(this);
  
  // pre-process
  nsCOMPtr<nsIDOMSelection> selection;
  nsEditor::GetSelection(getter_AddRefs(selection));
  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kIndent);
  if (aIndent == "outdent")
    ruleInfo.action = nsHTMLEditRules::kOutdent;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel);
  if (cancel || (NS_FAILED(res))) return res;
  
  // Do default - insert a blockquote node if selection collapsed
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
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
  
  return res;
}

//TODO: IMPLEMENT ALIGNMENT!

NS_IMETHODIMP
nsHTMLEditor::Align(const nsString& aAlignType)
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->Align(aAlignType);
#endif // ENABLE_JS_EDITOR_LOG

  nsAutoEditBatch beginBatching(this);
  nsCOMPtr<nsIDOMNode> node;
  PRBool cancel= PR_FALSE;
  
  // Find out if the selection is collapsed:
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res) || !selection) return res;
  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kAlign);
  ruleInfo.alignType = &aAlignType;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel);

  return res;
}


NS_IMETHODIMP
nsHTMLEditor::InsertList(const nsString& aListType)
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->InsertList(aListType);
#endif // ENABLE_JS_EDITOR_LOG

  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel= PR_FALSE;

  nsAutoEditBatch beginBatching(this);
  
  // pre-process
  nsEditor::GetSelection(getter_AddRefs(selection));
  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kMakeList);
  if (aListType == "ol") ruleInfo.bOrdered = PR_TRUE;
  else  ruleInfo.bOrdered = PR_FALSE;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel);
  if (cancel || (NS_FAILED(res))) return res;

  // Find out if the selection is collapsed:
  if (NS_FAILED(res) || !selection) return res;

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
    // XXX - revisit when layout is fixed
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

  return res;
}

NS_IMETHODIMP
nsHTMLEditor::GetElementOrParentByTagName(const nsString &aTagName, nsIDOMNode *aNode, nsIDOMElement** aReturn)
{
  if (aTagName.Length() == 0 || !aReturn )
    return NS_ERROR_NULL_POINTER;
  
  nsresult res = NS_OK;
  // If no node supplied, use anchor node of current selection
  if (!aNode)
  {
    nsCOMPtr<nsIDOMSelection>selection;
    res = nsEditor::GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res) || !selection)
      return res;

    //TODO: Do I need to release the node?
    if(NS_FAILED(selection->GetAnchorNode(&aNode)))
      return NS_ERROR_FAILURE;
  }
   
  nsAutoString TagName = aTagName;
  TagName.ToLowerCase();
  PRBool getLink = IsLink(TagName);
  PRBool getNamedAnchor = IsNamedAnchor(TagName);
  if ( getLink || getNamedAnchor)
  {
    TagName = "a";  
  }
  PRBool findTableCell = aTagName.EqualsIgnoreCase("td");

  // default is null - no element found
  *aReturn = nsnull;
  
  nsCOMPtr<nsIDOMNode> currentNode = aNode;
  nsCOMPtr<nsIDOMNode> parent;
  PRBool bNodeFound = PR_FALSE;

  while (PR_TRUE)
  {
    // Test if we have a link (an anchor with href set)
    if ( (getLink && IsLinkNode(currentNode)) ||
         (getNamedAnchor && IsNamedAnchorNode(currentNode)) )
    {
      bNodeFound = PR_TRUE;
      break;
    } else {
      nsAutoString currentTagName; 
      currentNode->GetNodeName(currentTagName);
      // Table cells are a special case:
      // Match either "td" or "th" for them
      if (currentTagName.EqualsIgnoreCase(TagName) ||
          (findTableCell && currentTagName.EqualsIgnoreCase("th")))
      {
        bNodeFound = PR_TRUE;
        break;
      } 
    }
    // Search up the parent chain
    // We should never fail because of root test below, but lets be safe
    if (!NS_SUCCEEDED(currentNode->GetParentNode(getter_AddRefs(parent))) || !parent)
      break;

    // Stop searching if parent is a body tag
    nsAutoString parentTagName;
    parent->GetNodeName(parentTagName);
    // Note: Originally used IsRoot to stop at table cells,
    //  but that's too messy when you are trying to find the parent table
    //PRBool isRoot;
    //if (!NS_SUCCEEDED(IsRootTag(parentTagName, isRoot)) || isRoot)
    if(parentTagName.EqualsIgnoreCase("body"))
      break;

    currentNode = parent;
  }
  if (bNodeFound)
  {
    nsCOMPtr<nsIDOMElement> currentElement = do_QueryInterface(currentNode);
    if (currentElement)
    {
      *aReturn = currentElement;
      // Getters must addref
      NS_ADDREF(*aReturn);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLEditor::GetSelectedElement(const nsString& aTagName, nsIDOMElement** aReturn)
{
  if (!aReturn )
    return NS_ERROR_NULL_POINTER;
  
  // default is null - no element found
  *aReturn = nsnull;
  
  nsAutoString TagName = aTagName;
  TagName.ToLowerCase();
  // Empty string indicates we should match any element tag
  PRBool anyTag = (TagName == "");
  
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

  if (IsLink(TagName))
  {
    // Link tag is a special case - we return the anchor node
    //  found for any selection that is totally within a link,
    //  included a collapsed selection (just a caret in a link)
    nsCOMPtr<nsIDOMNode> anchorNode;
    res = selection->GetAnchorNode(getter_AddRefs(anchorNode));
    PRInt32 anchorOffset = -1;
    if (anchorNode)
      selection->GetAnchorOffset(&anchorOffset);
    
    nsCOMPtr<nsIDOMNode> focusNode;
    selection->GetFocusNode(getter_AddRefs(focusNode));
    PRInt32 focusOffset = -1;
    if (focusNode)
      selection->GetFocusOffset(&focusOffset);

    // Link node must be the same for both ends of selection
    if (NS_SUCCEEDED(res) && anchorNode)
    {
#ifdef DEBUG_cmanske
      {
      nsAutoString name;
      anchorNode->GetNodeName(name);
      printf("GetSelectedElement: Anchor node of selection: ");
      wprintf(name.GetUnicode());
      printf(" Offset: %d\n", anchorOffset);
      focusNode->GetNodeName(name);
      printf("Focus node of selection: ");
      wprintf(name.GetUnicode());
      printf(" Offset: %d\n", focusOffset);
      }
#endif
      nsCOMPtr<nsIDOMElement> parentLinkOfAnchor;
      res = GetElementOrParentByTagName("href", anchorNode, getter_AddRefs(parentLinkOfAnchor));
      if (NS_SUCCEEDED(res) && parentLinkOfAnchor)
      {
        if (isCollapsed)
        {
          // We have just a caret in the link
          bNodeFound = PR_TRUE;
        } else if(focusNode) 
        {  // Link node must be the same for both ends of selection
          nsCOMPtr<nsIDOMElement> parentLinkOfFocus;
          res = GetElementOrParentByTagName("href", focusNode, getter_AddRefs(parentLinkOfFocus));
          if (NS_SUCCEEDED(res) && parentLinkOfFocus == parentLinkOfAnchor)
            bNodeFound = PR_TRUE;
        }
      
        // We found a link node parent
        if (bNodeFound) {
          // GetElementOrParentByTagName addref'd this, so we don't need to do it here
          *aReturn = parentLinkOfAnchor;
          return NS_OK;
        }
      }
      else if (anchorOffset >= 0)  // Check if link node is the only thing selected
      {
        nsCOMPtr<nsIDOMNode> anchorChild;
        anchorChild = GetChildAt(anchorNode,anchorOffset);
        if (anchorChild && IsLinkNode(anchorChild) && 
            (anchorNode == focusNode) && focusOffset == (anchorOffset+1))
        {
          selectedElement = do_QueryInterface(anchorChild);
          bNodeFound = PR_TRUE;
        }
      }
    }
  } 

  if (!bNodeFound && !isCollapsed)   // Don't bother to examine selection if it is collapsed
  {
    nsCOMPtr<nsIEnumerator> enumerator;
    res = selection->GetEnumerator(getter_AddRefs(enumerator));
    if (NS_SUCCEEDED(res) && enumerator)
    {
      enumerator->First(); 
      nsCOMPtr<nsISupports> currentItem;
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
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

              if (anyTag)
              {
                // Get name of first selected element
                selectedElement->GetTagName(TagName);
                TagName.ToLowerCase();
                anyTag = PR_FALSE;
              }

              // The "A" tag is a pain,
              //  used for both link(href is set) and "Named Anchor"
              nsCOMPtr<nsIDOMNode> selectedNode = do_QueryInterface(selectedElement);
              if ( (IsLink(TagName) && IsLinkNode(selectedNode)) ||
                   (IsNamedAnchor(TagName) && IsNamedAnchorNode(selectedNode)) )
              {
                bNodeFound = PR_TRUE;
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
    // Getters must addref
    NS_ADDREF(*aReturn);
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

  if (IsLink(TagName) || IsNamedAnchor(TagName))
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
  if (TagName.Equals("hr"))
  {
    // Hard coded defaults in case there's no prefs
    nsAutoString align("center");
    nsAutoString width("100%");
    nsAutoString height("2");
    PRBool bNoShade = PR_FALSE;

    if (mPrefs)
    {
      char buf[16];
      PRInt32 iAlign;
      // Currently using 0=left, 1=center, and 2=right
      if( NS_SUCCEEDED(mPrefs->GetIntPref("editor.hrule.align", &iAlign)))
      {
        switch (iAlign) {
          case 0:
            align = "left";
            break;
          case 2:
            align = "right";
            break;
        }
      }
      PRInt32 iHeight;
      PRUint32 count;
      if( NS_SUCCEEDED(mPrefs->GetIntPref("editor.hrule.height", &iHeight)))
      {
        count = PR_snprintf(buf, 16, "%d", iHeight);
        if (count > 0)
        {
          height = buf;
        }
      }
      PRInt32 iWidth;
      PRBool  bPercent;
      if( NS_SUCCEEDED(mPrefs->GetIntPref("editor.hrule.width", &iWidth)) &&
          NS_SUCCEEDED(mPrefs->GetBoolPref("editor.hrule.width_percent", &bPercent)))
      {
        count = PR_snprintf(buf, 16, "%d", iWidth);
        if (count > 0)
        {
          width = buf;
          if (bPercent)
            width.Append("%");
        }
      }
      PRBool  bShading;
      if (NS_SUCCEEDED(mPrefs->GetBoolPref("editor.hrule.shading", &bShading)))
      {
        bNoShade = !bShading;
      }
    }
    newElement->SetAttribute("align", align);
    newElement->SetAttribute("height", height);
    newElement->SetAttribute("width", width);
    if (bNoShade)    
      newElement->SetAttribute("noshade", "");

  } else if (TagName.Equals("table"))
  {
    newElement->SetAttribute("cellpadding","2");
    newElement->SetAttribute("cellspacing","2");
    newElement->SetAttribute("width","50%");
    newElement->SetAttribute("border","1");
  } else if (TagName.Equals("tr"))
  {
    newElement->SetAttribute("valign","top");
  } else if (TagName.Equals("td"))
  {
    newElement->SetAttribute("valign","top");

    // Insert the default space in a cell so border displays
    nsCOMPtr<nsIDOMNode> newCellNode = do_QueryInterface(newElement);
    if (newCellNode)
    {
      // TODO: This should probably be in the RULES code or 
      //       preference based for "should we add the nbsp"
      nsCOMPtr<nsIDOMText>newTextNode;
      nsString space;
      // Set contents to the &nbsp character by concatanating the char code
      space += nbsp;
      // If we fail here, we return NS_OK anyway, since we have an OK cell node
      nsresult result = mDoc->CreateTextNode(space, getter_AddRefs(newTextNode));
      if (NS_SUCCEEDED(result) && newTextNode)
      {
        nsCOMPtr<nsIDOMNode>resultNode;
        result = newCellNode->AppendChild(newTextNode, getter_AddRefs(resultNode));
      }
    }
  }
  // ADD OTHER DEFAULT ATTRIBUTES HERE

  if (NS_SUCCEEDED(res))
  {
    *aReturn = newElement;
  }

  return res;
}

NS_IMETHODIMP
nsHTMLEditor::InsertElement(nsIDOMElement* aElement, PRBool aDeleteSelection)
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->InsertElement(aElement, aDeleteSelection);
#endif // ENABLE_JS_EDITOR_LOG

  nsresult res = NS_ERROR_NOT_INITIALIZED;
  
  if (!aElement)
    return NS_ERROR_NULL_POINTER;
  
  nsAutoEditBatch beginBatching(this);
  // For most elements, set caret after inserting
  //PRBool setCaretAfterElement = PR_TRUE;

  nsCOMPtr<nsIDOMSelection>selection;
  res = nsEditor::GetSelection(getter_AddRefs(selection));
  if (!NS_SUCCEEDED(res) || !selection)
    return NS_ERROR_FAILURE;

  // Clear current selection.
  // Should put caret at anchor point?
  if (!aDeleteSelection)
  {
    PRBool collapseAfter = PR_TRUE;
    // Named Anchor is a special case,
    // We collapse to insert element BEFORE the selection
    // For all other tags, we insert AFTER the selection
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aElement);
    if (IsNamedAnchorNode(node))
      collapseAfter = PR_FALSE;

    if (collapseAfter)
    {
      // Default behavior is to collapse to the end of the selection
      selection->ClearSelection();
    } else {
      // Collapse to the start of the selection,
      // We must explore the first range and find
      //   its parent and starting offset of selection
      // TODO: Move this logic to a new method nsIDOMSelection::CollapseToStart()???
      nsCOMPtr<nsIDOMRange> firstRange;
      res = selection->GetRangeAt(0, getter_AddRefs(firstRange));
      if (NS_SUCCEEDED(res) && firstRange)
      {
        nsCOMPtr<nsIDOMNode> parent;
        res = firstRange->GetCommonParent(getter_AddRefs(parent));
        if (NS_SUCCEEDED(res) && parent)
        {
          PRInt32 startOffset;
          firstRange->GetStartOffset(&startOffset);
          selection->Collapse(parent, startOffset);
        } else {
          // Very unlikely, but collapse to the end if we failed above
          selection->ClearSelection();
        }
      }
    }
  }
  
  nsCOMPtr<nsIDOMNode> parentSelectedNode;
  PRInt32 offsetForInsert;
  res = selection->GetAnchorNode(getter_AddRefs(parentSelectedNode));
  if (NS_SUCCEEDED(res) && NS_SUCCEEDED(selection->GetAnchorOffset(&offsetForInsert)) && parentSelectedNode)
  {
#ifdef DEBUG_cmanske
    {
    nsAutoString name;
    parentSelectedNode->GetNodeName(name);
    printf("InsertElement: Anchor node of selection: ");
    wprintf(name.GetUnicode());
    printf(" Offset: %d\n", offsetForInsert);
    }
#endif
    nsAutoString tagName;
    aElement->GetNodeName(tagName);
    tagName.ToLowerCase();
    nsCOMPtr<nsIDOMNode> parent = parentSelectedNode;
    nsCOMPtr<nsIDOMNode> topChild = parentSelectedNode;
    nsCOMPtr<nsIDOMNode> tmp;
    nsAutoString parentTagName;
    PRBool isRoot;

    // Search up the parent chain to find a suitable container    
    while (!CanContainTag(parent, tagName))
    {
      // If the current parent is a root (body or table cell)
      // then go no further - we can't insert
      parent->GetNodeName(parentTagName);
      res = IsRootTag(parentTagName, isRoot);
      if (!NS_SUCCEEDED(res) || isRoot)
        return NS_ERROR_FAILURE;
      // Get the next parent
      parent->GetParentNode(getter_AddRefs(tmp));
      if (!tmp)
        return NS_ERROR_FAILURE;
      topChild = parent;
      parent = tmp;
    }
#ifdef DEBUG_cmanske
    {
    nsAutoString name;
    parent->GetNodeName(name);
    printf("Parent node to insert under: ");
    wprintf(name.GetUnicode());
    printf("\n");
    topChild->GetNodeName(name);
    printf("TopChild to split: ");
    wprintf(name.GetUnicode());
    printf("\n");
    }
#endif
    if (parent != topChild)
    {
      // we need to split some levels above the original selection parent
      res = SplitNodeDeep(topChild, parentSelectedNode, offsetForInsert);
      if (NS_FAILED(res))
        return res;
      // topChild went to the right on the split
      // so this is the offset to insert at
      offsetForInsert = GetIndexOf(parent,topChild);  
    }
    // Now we can insert the new node
    res = InsertNode(aElement, parent, offsetForInsert);
    
    // Set caret after element, but check for special case 
    //  of inserting table-related elements: set in first cell instead
    if (!SetCaretInTableCell(aElement))
      res = SetCaretAfterElement(aElement);

  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::SaveHLineSettings(nsIDOMElement* aElement)
{
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  if (!aElement || !mPrefs)
    return res;

  nsAutoString align, width, height, noshade;
  res = NS_ERROR_UNEXPECTED;
  if (NS_SUCCEEDED(aElement->GetAttribute("align", align)) &&
      NS_SUCCEEDED(aElement->GetAttribute("height", height)) &&
      NS_SUCCEEDED(aElement->GetAttribute("width", width)) &&
      NS_SUCCEEDED(aElement->GetAttribute("noshade", noshade)))
  {
    PRInt32 iAlign = 0;
    if (align == "center")
      iAlign = 1;
    else if (align == "right")
      iAlign = 2;
    mPrefs->SetIntPref("editor.hrule.align", iAlign);

    PRInt32 errorCode;
    PRInt32 iHeight = height.ToInteger(&errorCode);
    
    if (errorCode == NS_OK && iHeight > 0)
      mPrefs->SetIntPref("editor.hrule.height", iHeight);

    PRInt32 iWidth = width.ToInteger(&errorCode);
    if (errorCode == NS_OK && iWidth > 0) {
      mPrefs->SetIntPref("editor.hrule.width", iWidth);
      mPrefs->SetBoolPref("editor.hrule.width_percent", (width.Find("%") > 0));
    }

    mPrefs->SetBoolPref("editor.hrule.shading", (noshade == ""));
    res = NS_OK;
  }
        
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::InsertLinkAroundSelection(nsIDOMElement* aAnchorElement)
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->InsertLinkAroundSelection(aAnchorElement);
#endif // ENABLE_JS_EDITOR_LOG

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
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->SelectElement(aElement);
#endif // ENABLE_JS_EDITOR_LOG

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

PRBool
nsHTMLEditor::SetCaretInTableCell(nsIDOMElement* aElement)
{
  PRBool caretIsSet = PR_FALSE;

  if (aElement && IsElementInBody(aElement))
  {
    nsresult res = NS_OK;
    nsAutoString tagName;
    aElement->GetNodeName(tagName);
    tagName.ToLowerCase();
    if (tagName == "table" || tagName == "tr" || 
        tagName == "td"    || tagName == "th" ||
        tagName == "thead" || tagName == "tfoot" ||
        tagName == "tbody" || tagName == "caption")
    {
      nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aElement);
      nsCOMPtr<nsIDOMNode> parent;
      // This MUST succeed if IsElementInBody was TRUE
      node->GetParentNode(getter_AddRefs(parent));
      nsCOMPtr<nsIDOMNode>firstChild;
      // Find deepest child
      PRBool hasChild;
      while (NS_SUCCEEDED(node->HasChildNodes(&hasChild)) && hasChild)
      {
        if (NS_SUCCEEDED(node->GetFirstChild(getter_AddRefs(firstChild))))
        {
          parent = node;
          node = firstChild;
        }
      }
      PRInt32 offset = 0;
      nsCOMPtr<nsIDOMNode>lastChild;
      res = parent->GetLastChild(getter_AddRefs(lastChild));
      if (NS_SUCCEEDED(res) && lastChild && node != lastChild)
      {
        if (node == lastChild)
        {
          // Check if node is text and has more than just a &nbsp
          nsCOMPtr<nsIDOMCharacterData>textNode = do_QueryInterface(node);
          nsString text;
          char nbspStr[2] = {nbsp, 0};
          if (textNode && textNode->GetData(text))
          {
            // Set selection relative to the text node
            parent = node;
            PRInt32 len = text.Length();
            if (len > 1 || text != nbspStr)
            {
              offset = len;
            }
          }
        } else {
          // We have > 1 node, so set to end of content
        }
      }
      // Set selection at beginning of deepest node
      // Should we set 
      nsCOMPtr<nsIDOMSelection> selection;
      res = nsEditor::GetSelection(getter_AddRefs(selection));
      if (NS_SUCCEEDED(res) && selection)
      {
        res = selection->Collapse(parent, offset);
        if (NS_SUCCEEDED(res))
          caretIsSet = PR_TRUE;
      }
    }
  }
  return caretIsSet;
}            

NS_IMETHODIMP
nsHTMLEditor::SetCaretAfterElement(nsIDOMElement* aElement)
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->SetCaretAfterElement(aElement);
#endif // ENABLE_JS_EDITOR_LOG

  nsresult res = NS_ERROR_NULL_POINTER;

  // Be sure the element is contained in the document body
  if (aElement && IsElementInBody(aElement))
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
#ifdef DEBUG_cmanske
          {
          nsAutoString name;
          parent->GetNodeName(name);
          printf("SetCaretAfterElement: Parent node: ");
          wprintf(name.GetUnicode());
          printf(" Offset: %d\n\nHTML:\n", offsetInParent+1);
          nsString Format("text/html");
          nsString ContentsAs;
          OutputToString(ContentsAs, Format, 2);
          wprintf(ContentsAs.GetUnicode());
          }
#endif
        }
      }
    }
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::GetEmbeddedObjects(nsISupportsArray** aNodeList)
{
  if (!aNodeList)
    return NS_ERROR_NULL_POINTER;

  nsresult res;

  res = NS_NewISupportsArray(aNodeList);
  if (NS_FAILED(res) || !*aNodeList)
    return res;
  //NS_ADDREF(*aNodeList);

  nsCOMPtr<nsIContentIterator> iter;
  res = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                           nsIContentIterator::GetIID(), 
                                           getter_AddRefs(iter));
  if ((NS_SUCCEEDED(res)) && iter)
  {
    // get the root content
    nsCOMPtr<nsIContent> rootContent;

    nsCOMPtr<nsIDOMDocument> domdoc;
    nsEditor::GetDocument(getter_AddRefs(domdoc));
    if (!domdoc)
      return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIDocument> doc (do_QueryInterface(domdoc));
    if (!doc)
      return NS_ERROR_UNEXPECTED;

    rootContent = doc->GetRootContent();

    iter->Init(rootContent);

    // loop through the content iterator for each content node
    while (NS_COMFALSE == iter->IsDone())
    {
      nsCOMPtr<nsIContent> content;
      res = iter->CurrentNode(getter_AddRefs(content));
      if (NS_FAILED(res))
        break;
      nsCOMPtr<nsIDOMNode> node (do_QueryInterface(content));
      if (node)
      {
        nsAutoString tagName;
        node->GetNodeName(tagName);
        tagName.ToLowerCase();

        // See if it's an image or an embed
        if (tagName == "img" || tagName == "embed")
          (*aNodeList)->AppendElement(node);
        else if (tagName == "a")
        {
          // XXX Only include links if they're links to file: URLs
          nsCOMPtr<nsIDOMHTMLAnchorElement> anchor (do_QueryInterface(content));
          if (anchor)
          {
            nsAutoString href;
            if (NS_SUCCEEDED(anchor->GetHref(href)))
              if (href.Compare("file:", PR_TRUE, 5) == 0)
                (*aNodeList)->AppendElement(node);
          }
        }
      }
      iter->Next();
    }
  }

  return res;
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

NS_IMETHODIMP nsHTMLEditor::SetCompositionString(const nsString& aCompositionString, nsIDOMTextRangeList* aTextRangeList)
{
	return nsTextEditor::SetCompositionString(aCompositionString,aTextRangeList);
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

NS_IMETHODIMP
nsHTMLEditor::StartLogging(nsIFileSpec *aLogFile)
{
  return nsTextEditor::StartLogging(aLogFile);
}

NS_IMETHODIMP
nsHTMLEditor::StopLogging()
{
  return nsTextEditor::StopLogging();
}


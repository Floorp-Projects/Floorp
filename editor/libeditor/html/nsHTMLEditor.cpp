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
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsIDOMSelection.h"
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
#include "nsIXIFConverter.h"
#include "nsIDOMNSUIEvent.h"

// Transactionas
#include "PlaceholderTxn.h"
#include "nsStyleSheetTxns.h"

// Misc
#include "TextEditorTest.h"
#include "nsEditorUtils.h"
#include "nsIPref.h"

const PRUnichar nbsp = 160;

// HACK - CID for NavDTD until we can get at dtd via the document
// {a6cf9107-15b3-11d2-932e-00805f8add32}
#define NS_CNAVDTD_CID \
{ 0xa6cf9107, 0x15b3, 0x11d2, { 0x93, 0x2e, 0x0, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }
static NS_DEFINE_CID(kCNavDTDCID,    NS_CNAVDTD_CID);


static NS_DEFINE_CID(kHTMLEditorCID,  NS_HTMLEDITOR_CID);
static NS_DEFINE_CID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_IID(kSubtreeIteratorCID, NS_SUBTREEITERATOR_CID);
static NS_DEFINE_CID(kCRangeCID,      NS_RANGE_CID);
static NS_DEFINE_CID(kCDOMSelectionCID,      NS_DOMSELECTION_CID);
static NS_DEFINE_IID(kFileWidgetCID,  NS_FILEWIDGET_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID); 
static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID); 

// Drag & Drop, Clipboard Support
static NS_DEFINE_CID(kCClipboardCID,    NS_CLIPBOARD_CID);
static NS_DEFINE_CID(kCTransferableCID, NS_TRANSFERABLE_CID);
static NS_DEFINE_CID(kCDragServiceCID, NS_DRAGSERVICE_CID);
static NS_DEFINE_CID(kCXIFFormatConverterCID, NS_XIFFORMATCONVERTER_CID);

#if defined(NS_DEBUG) && defined(DEBUG_buster)
static PRBool gNoisy = PR_FALSE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif

// Some utilities to handle annoying overloading of "A" tag for link and named anchor
static char hrefText[] = "href";
static char anchorTxt[] = "anchor";
static char namedanchorText[] = "namedanchor";
nsIAtom *nsHTMLEditor::gTypingTxnName;
nsIAtom *nsHTMLEditor::gIMETxnName;
nsIAtom *nsHTMLEditor::gDeleteTxnName;

// some prototypes for rules creation shortcuts
nsresult NS_NewTextEditRules(nsIEditRules** aInstancePtrResult);
nsresult NS_NewHTMLEditRules(nsIEditRules** aInstancePtrResult);

#define IsLink(s) (s.EqualsIgnoreCase(hrefText))
#define IsNamedAnchor(s) (s.EqualsIgnoreCase(anchorTxt) || s.EqualsIgnoreCase(namedanchorText))

static PRBool IsLinkNode(nsIDOMNode *aNode)
{
  if (aNode)
  {
    nsCOMPtr<nsIDOMHTMLAnchorElement> anchor = do_QueryInterface(aNode);
    if (anchor)
    {
      nsAutoString tmpText;
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
      nsAutoString tmpText;
      if (NS_SUCCEEDED(anchor->GetName(tmpText)) && tmpText.GetUnicode() && tmpText.Length() != 0)
        return PR_TRUE;
    }
  }
  return PR_FALSE;
}

static PRBool IsListNode(nsIDOMNode *aNode)
{
  if (aNode)
  {
    nsCOMPtr<nsIDOMElement>element;
    element = do_QueryInterface(aNode);
    if (element)
    {
      nsAutoString tagName;
      if (NS_SUCCEEDED(element->GetTagName(tagName)))
      {
        tagName.ToLowerCase();
        // With only 3 tests, it doesn't 
        //  seem worth using nsAtoms
        if (tagName.EqualsWithConversion("ol") || 
            tagName.EqualsWithConversion("ul") ||
            tagName.EqualsWithConversion("dl"))
        {
          return PR_TRUE;
        }
      }
    }
  }
  return PR_FALSE;
}

static PRBool IsCellNode(nsIDOMNode *aNode)
{
  if (aNode)
  {
    nsCOMPtr<nsIDOMElement>element;
    element = do_QueryInterface(aNode);
    if (element)
    {
      nsAutoString tagName;
      if (NS_SUCCEEDED(element->GetTagName(tagName)))
      {
        // With only 2 tests, it doesn't 
        //  seem worth using nsAtoms
        if (tagName.EqualsIgnoreCase("td") || 
            tagName.EqualsIgnoreCase("th"))
        {
          return PR_TRUE;
        }
      }
    }
  }
  return PR_FALSE;
}


nsHTMLEditor::nsHTMLEditor()
: nsEditor()
, mTypeInState(nsnull)
, mRules(nsnull)
, mIsComposing(PR_FALSE)
, mMaxTextLength(-1)
, mSelectedCellIndex(0)
{
// Done in nsEditor
// NS_INIT_REFCNT();
  mBoldAtom = getter_AddRefs(NS_NewAtom("b"));
  mItalicAtom = getter_AddRefs(NS_NewAtom("i"));
  mUnderlineAtom = getter_AddRefs(NS_NewAtom("u"));
  mFontAtom = getter_AddRefs(NS_NewAtom("font"));
  mLinkAtom = getter_AddRefs(NS_NewAtom("a"));

  if (!gTypingTxnName)
    gTypingTxnName = NS_NewAtom("Typing");
  else
    NS_ADDREF(gTypingTxnName);
  if (!gIMETxnName)
    gIMETxnName = NS_NewAtom("IME");
  else
    NS_ADDREF(gIMETxnName);
  if (!gDeleteTxnName)
    gDeleteTxnName = NS_NewAtom("Deleting");
  else
    NS_ADDREF(gDeleteTxnName);
} 

nsHTMLEditor::~nsHTMLEditor()
{
  /* first, delete the transaction manager if there is one.
     this will release any remaining transactions.
     this is important because transactions can hold onto the atoms (gTypingTxnName, ...)
     and to make the optimization (holding refcounted statics) work correctly, 
     the editor instance needs to hold the last refcount.
     If you get this wrong, expect to deref a garbage gTypingTxnName pointer if you bring up a second editor.
  */
  if (mTxnMgr) { 
    mTxnMgr = 0;
  }
  nsrefcnt refCount=0;
  if (gTypingTxnName)  // we addref'd in the constructor
  { // want to release it without nulling out the pointer.
    refCount = gTypingTxnName->Release();
    if (0==refCount) {
      gTypingTxnName = nsnull; 
    }
  }

  if (gIMETxnName)  // we addref'd in the constructor
  { // want to release it without nulling out the pointer.
    refCount = gIMETxnName->Release();
    if (0==refCount) {
      gIMETxnName = nsnull;
    }
  }

  if (gDeleteTxnName)  // we addref'd in the constructor
  { // want to release it without nulling out the pointer.
    refCount = gDeleteTxnName->Release();
    if (0==refCount) {
      gDeleteTxnName = nsnull;
    }
  }
  
  // remove the rules as an action listener.  Else we get a bad ownership loop later on.
  // it's ok if the rules aren't a listener; we ignore the error.
  nsCOMPtr<nsIEditActionListener> mListener = do_QueryInterface(mRules);
  RemoveEditActionListener(mListener);
  
  //the autopointers will clear themselves up. 
  //but we need to also remove the listeners or we have a leak
  nsCOMPtr<nsIDOMSelection>selection;
  nsresult result = GetSelection(getter_AddRefs(selection));
  // if we don't get the selection, just skip this
  if (NS_SUCCEEDED(result) && selection) 
  {
    nsCOMPtr<nsIDOMSelectionListener>listener;
    listener = do_QueryInterface(mTypeInState);
    if (listener) {
      selection->RemoveSelectionListener(listener); 
    }
  }
  nsCOMPtr<nsIDOMEventReceiver> erP;
  result = GetDOMEventReceiver(getter_AddRefs(erP));
  if (NS_SUCCEEDED(result) && erP) 
  {
    if (mKeyListenerP) {
      erP->RemoveEventListenerByIID(mKeyListenerP, NS_GET_IID(nsIDOMKeyListener));
    }
    if (mMouseListenerP) {
      erP->RemoveEventListenerByIID(mMouseListenerP, NS_GET_IID(nsIDOMMouseListener));
    }
    if (mTextListenerP) {
      erP->RemoveEventListenerByIID(mTextListenerP, NS_GET_IID(nsIDOMTextListener));
    }
     if (mCompositionListenerP) {
      erP->RemoveEventListenerByIID(mCompositionListenerP, NS_GET_IID(nsIDOMCompositionListener));
    }
    if (mFocusListenerP) {
      erP->RemoveEventListenerByIID(mFocusListenerP, NS_GET_IID(nsIDOMFocusListener));
    }
    if (mDragListenerP) {
        erP->RemoveEventListenerByIID(mDragListenerP, NS_GET_IID(nsIDOMDragListener));
    }
  }
  else
    NS_NOTREACHED("~nsTextEditor");

  NS_IF_RELEASE(mTypeInState);
}

NS_IMPL_ADDREF_INHERITED(nsHTMLEditor, nsEditor)
NS_IMPL_RELEASE_INHERITED(nsHTMLEditor, nsEditor)


NS_IMETHODIMP nsHTMLEditor::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (!aInstancePtr)
    return NS_ERROR_NULL_POINTER;
 
  *aInstancePtr = nsnull;
  
  if (aIID.Equals(NS_GET_IID(nsIHTMLEditor))) {
    *aInstancePtr = NS_STATIC_CAST(nsIHTMLEditor*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIEditorMailSupport))) {
    *aInstancePtr = NS_STATIC_CAST(nsIEditorMailSupport*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsITableEditor))) {
    *aInstancePtr = NS_STATIC_CAST(nsITableEditor*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIEditorStyleSheets))) {
    *aInstancePtr = NS_STATIC_CAST(nsIEditorStyleSheets*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsICSSLoaderObserver))) {
    *aInstancePtr = NS_STATIC_CAST(nsICSSLoaderObserver*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return nsEditor::QueryInterface(aIID, aInstancePtr);
}


NS_IMETHODIMP nsHTMLEditor::Init(nsIDOMDocument *aDoc, 
                                 nsIPresShell   *aPresShell, nsIContent *aRoot, nsISelectionController *aSelCon, PRUint32 aFlags)
{
  NS_PRECONDITION(aDoc && aPresShell, "bad arg");
  if (!aDoc || !aPresShell)
    return NS_ERROR_NULL_POINTER;

  nsresult result = NS_ERROR_NULL_POINTER;
  // Init the base editor
  result = nsEditor::Init(aDoc, aPresShell, aRoot, aSelCon, aFlags);
  if (NS_FAILED(result)) { return result; }

  // disable links
  nsCOMPtr<nsIPresContext> context;
  aPresShell->GetPresContext(getter_AddRefs(context));
  if (!context) return NS_ERROR_NULL_POINTER;
  if (!(mFlags & eEditorPlaintextMask))
    context->SetLinkHandler(0);  

  nsCOMPtr<nsIDOMElement> bodyElement;
  result = nsEditor::GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(result)) { return result; }
  if (!bodyElement) { return NS_ERROR_NULL_POINTER; }

  // init the type-in state
  mTypeInState = new TypeInState();
  if (!mTypeInState) {return NS_ERROR_NULL_POINTER;}
  NS_ADDREF(mTypeInState);

  nsCOMPtr<nsIDOMSelection>selection;
  result = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) { return result; }
  if (selection) 
  {
    nsCOMPtr<nsIDOMSelectionListener>listener;
    listener = do_QueryInterface(mTypeInState);
    if (listener) {
      selection->AddSelectionListener(listener); 
    }
  }

  // Set up a DTD    XXX XXX 
  // HACK: This should have happened in a document specific way
  //       in nsEditor::Init(), but we dont' have a way to do that yet
  result = nsComponentManager::CreateInstance(kCNavDTDCID, nsnull,
                                          NS_GET_IID(nsIDTD), getter_AddRefs(mDTD));
  if (!mDTD) result = NS_ERROR_FAILURE;
  if (NS_FAILED(result)) return result;
  
  // Init the rules system
  result = InitRules();
  if (NS_FAILED(result)) return result;

  EnableUndo(PR_TRUE);
 
  return result;
}

NS_IMETHODIMP 
nsHTMLEditor::SetDocumentCharacterSet(const PRUnichar* characterSet) 
{ 
  nsresult result; 

  result = nsEditor::SetDocumentCharacterSet(characterSet); 

  // update META charset tag 
  if (NS_SUCCEEDED(result)) { 
    nsCOMPtr<nsIDOMDocument>domdoc; 
    result = GetDocument(getter_AddRefs(domdoc)); 
    if (NS_SUCCEEDED(result) && domdoc) { 
      nsAutoString newMetaString; 
      nsCOMPtr<nsIDOMNodeList>metaList; 
      nsCOMPtr<nsIDOMNode>metaNode; 
      nsCOMPtr<nsIDOMElement>metaElement; 
      PRBool newMetaCharset = PR_TRUE; 

      // get a list of META tags 
        // can't use |NS_LITERAL_STRING| here until |GetElementsByTagName| is fixed to accept readables
      result = domdoc->GetElementsByTagName(NS_ConvertASCIItoUCS2("meta"), getter_AddRefs(metaList)); 
      if (NS_SUCCEEDED(result) && metaList) { 
        PRUint32 listLength = 0; 
        (void) metaList->GetLength(&listLength); 

        for (PRUint32 i = 0; i < listLength; i++) { 
          metaList->Item(i, getter_AddRefs(metaNode)); 
          if (!metaNode) continue; 
          metaElement = do_QueryInterface(metaNode); 
          if (!metaElement) continue; 

          const NS_ConvertASCIItoUCS2 content("charset="); 
          nsString currentValue; 

            // can't use |NS_LITERAL_STRING| here until |GetAttribute| is fixed to accept readables
          if (NS_FAILED(metaElement->GetAttribute(NS_ConvertASCIItoUCS2("http-equiv"), currentValue))) continue; 

          if (kNotFound != currentValue.Find("content-type", PR_TRUE)) { 
            if (NS_FAILED(metaElement->GetAttribute(NS_ConvertASCIItoUCS2("content"), currentValue))) continue; 
              // can't use |NS_LITERAL_STRING| here until |GetAttribute| is fixed to accept readables

            PRInt32 offset = currentValue.Find(content.GetUnicode(), PR_TRUE); 
            if (kNotFound != offset) {
              currentValue.Left(newMetaString, offset); // copy current value before "charset=" (e.g. text/html) 
              newMetaString.Append(content); 
              newMetaString.Append(characterSet); 
              // can't use |NS_LITERAL_STRING| here until |SetAttributeSetAttribute| is fixed to accept readables
             result = nsEditor::SetAttribute(metaElement, NS_ConvertASCIItoUCS2("content"), newMetaString); 
              if (NS_SUCCEEDED(result)) 
                newMetaCharset = PR_FALSE; 
              break; 
            } 
          } 
        } 
      } 

      if (newMetaCharset) { 
        nsCOMPtr<nsIDOMNodeList>headList; 
        nsCOMPtr<nsIDOMNode>headNode; 
        nsCOMPtr<nsIDOMNode>resultNode; 

          // can't use |NS_LITERAL_STRING| here until |GetElementsByTagName| is fixed to accept readables
        result = domdoc->GetElementsByTagName(NS_ConvertASCIItoUCS2("head"),getter_AddRefs(headList)); 
        if (NS_SUCCEEDED(result) && headList) { 
          headList->Item(0, getter_AddRefs(headNode)); 
          if (headNode) { 
            // Create a new meta charset tag 
              // can't use |NS_LITERAL_STRING| here until |CreateNode| is fixed to accept readables
            result = CreateNode(NS_ConvertASCIItoUCS2("meta"), headNode, 0, getter_AddRefs(resultNode)); 
            if (NS_FAILED(result)) 
              return NS_ERROR_FAILURE; 

            // Set attributes to the created element 
            if (resultNode && nsCRT::strlen(characterSet) > 0) { 
              metaElement = do_QueryInterface(resultNode); 
              if (metaElement) { 
                // not undoable, undo should undo CreateNode 
                  // can't use |NS_LITERAL_STRING| here until |SetAttribute| is fixed to accept readables
                result = metaElement->SetAttribute(NS_ConvertASCIItoUCS2("http-equiv"), NS_ConvertASCIItoUCS2("Content-Type")); 
                if (NS_SUCCEEDED(result)) { 
                  newMetaString.AssignWithConversion("text/html;charset="); 
                  newMetaString.Append(characterSet); 
                  // not undoable, undo should undo CreateNode 
                  // can't use |NS_LITERAL_STRING| here until |SetAttribute| is fixed to accept readables
                  result = metaElement->SetAttribute(NS_ConvertASCIItoUCS2("content"), newMetaString); 
                } 
              } 
            } 
          } 
        } 
      } 
    } 
  } 

  return result; 
} 

NS_IMETHODIMP 
nsHTMLEditor::PostCreate()
{
  nsresult result = InstallEventListeners();
  if (NS_FAILED(result)) return result;

  result = nsEditor::PostCreate();
  return result;
}

NS_IMETHODIMP 
nsHTMLEditor::InstallEventListeners()
{
  NS_ASSERTION(mDocWeak, "no document set on this editor");
  if (!mDocWeak) return NS_ERROR_NOT_INITIALIZED;

  nsresult result;
  // get a key listener
  result = NS_NewEditorKeyListener(getter_AddRefs(mKeyListenerP), this);
  if (NS_FAILED(result)) {
    HandleEventListenerError();
    return result;
  }
  
  // get a mouse listener
  result = NS_NewEditorMouseListener(getter_AddRefs(mMouseListenerP), this);
  if (NS_FAILED(result)) {
    HandleEventListenerError();
    return result;
  }

  // get a text listener
  result = NS_NewEditorTextListener(getter_AddRefs(mTextListenerP),this);
  if (NS_FAILED(result)) { 
#ifdef DEBUG_TAGUE
printf("nsTextEditor.cpp: failed to get TextEvent Listener\n");
#endif
    HandleEventListenerError();
    return result;
  }

  // get a composition listener
  result = NS_NewEditorCompositionListener(getter_AddRefs(mCompositionListenerP),this);
  if (NS_FAILED(result)) { 
#ifdef DEBUG_TAGUE
printf("nsTextEditor.cpp: failed to get TextEvent Listener\n");
#endif
    HandleEventListenerError();
    return result;
  }

  // get a drag listener
  result = NS_NewEditorDragListener(getter_AddRefs(mDragListenerP), this);
  if (NS_FAILED(result)) {
    HandleEventListenerError();
    return result;
  }

  // get a focus listener
  result = NS_NewEditorFocusListener(getter_AddRefs(mFocusListenerP), this);
  if (NS_FAILED(result)) {
    HandleEventListenerError();
    return result;
  }

  nsCOMPtr<nsIDOMEventReceiver> erP;
  result = GetDOMEventReceiver(getter_AddRefs(erP));

  //end hack
  if (NS_FAILED(result)) {
    HandleEventListenerError();
    return result;
  }

  // register the event listeners with the DOM event reveiver
  result = erP->AddEventListenerByIID(mKeyListenerP, NS_GET_IID(nsIDOMKeyListener));
  NS_ASSERTION(NS_SUCCEEDED(result), "failed to register key listener");
  if (NS_SUCCEEDED(result))
  {
    result = erP->AddEventListenerByIID(mMouseListenerP, NS_GET_IID(nsIDOMMouseListener));
    NS_ASSERTION(NS_SUCCEEDED(result), "failed to register mouse listener");
    if (NS_SUCCEEDED(result))
    {
      result = erP->AddEventListenerByIID(mFocusListenerP, NS_GET_IID(nsIDOMFocusListener));
      NS_ASSERTION(NS_SUCCEEDED(result), "failed to register focus listener");
      if (NS_SUCCEEDED(result))
      {
        result = erP->AddEventListenerByIID(mTextListenerP, NS_GET_IID(nsIDOMTextListener));
        NS_ASSERTION(NS_SUCCEEDED(result), "failed to register text listener");
        if (NS_SUCCEEDED(result))
        {
          result = erP->AddEventListenerByIID(mCompositionListenerP, NS_GET_IID(nsIDOMCompositionListener));
          NS_ASSERTION(NS_SUCCEEDED(result), "failed to register composition listener");
          if (NS_SUCCEEDED(result))
          {
            result = erP->AddEventListenerByIID(mDragListenerP, NS_GET_IID(nsIDOMDragListener));
            NS_ASSERTION(NS_SUCCEEDED(result), "failed to register drag listener");
          }
        }
      }
    }
  }
  if (NS_FAILED(result)) {
    HandleEventListenerError();
  }
  return result;
}

NS_IMETHODIMP 
nsHTMLEditor::GetFlags(PRUint32 *aFlags)
{
  if (!mRules || !aFlags) { return NS_ERROR_NULL_POINTER; }
  return mRules->GetFlags(aFlags);
}


NS_IMETHODIMP 
nsHTMLEditor::SetFlags(PRUint32 aFlags)
{
  if (!mRules) { return NS_ERROR_NULL_POINTER; }
  return mRules->SetFlags(aFlags);
}


NS_IMETHODIMP nsHTMLEditor::InitRules()
{
// instantiate the rules for this text editor
// XXX: we should be told which set of rules to instantiate
  nsresult res = NS_ERROR_FAILURE;
  if (mFlags & eEditorPlaintextMask)
    res = NS_NewTextEditRules(getter_AddRefs(mRules));
  else
    res = NS_NewHTMLEditRules(getter_AddRefs(mRules));

  if (NS_FAILED(res)) return res;
  if (!mRules) return NS_ERROR_UNEXPECTED;
  res = mRules->Init(this, mFlags);
  
  return res;
}


PRBool nsHTMLEditor::IsModifiable()
{
  PRUint32 flags;
  if (NS_SUCCEEDED(GetFlags(&flags)))
    return ((flags & eEditorReadonlyMask) == 0);
  else
    return PR_FALSE;
}


#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIHTMLEditor methods 
#pragma mark -
#endif

NS_IMETHODIMP nsHTMLEditor::EditorKeyPress(nsIDOMKeyEvent* aKeyEvent)
{
  PRUint32 keyCode, character;
  PRBool   isShift, ctrlKey, altKey, metaKey;
  nsresult res;

  if (!aKeyEvent) return NS_ERROR_NULL_POINTER;

  if (NS_SUCCEEDED(aKeyEvent->GetKeyCode(&keyCode)) && 
      NS_SUCCEEDED(aKeyEvent->GetShiftKey(&isShift)) &&
      NS_SUCCEEDED(aKeyEvent->GetCtrlKey(&ctrlKey)) &&
      NS_SUCCEEDED(aKeyEvent->GetAltKey(&altKey)) &&
      NS_SUCCEEDED(aKeyEvent->GetMetaKey(&metaKey)))
  {
    // this royally blows: because tabs come in from keyDowns instead
    // of keyPress, and because GetCharCode refuses to work for keyDown
    // i have to play games.
    if (keyCode == nsIDOMKeyEvent::DOM_VK_TAB) character = '\t';
    else aKeyEvent->GetCharCode(&character);
    
    if (keyCode == nsIDOMKeyEvent::DOM_VK_TAB && !(mFlags&eEditorPlaintextBit))
    {
      nsCOMPtr<nsIDOMSelection>selection;
      res = GetSelection(getter_AddRefs(selection));
      if (NS_FAILED(res)) return res;
      PRInt32 offset;
      nsCOMPtr<nsIDOMNode> node, blockParent;
      res = GetStartNodeAndOffset(selection, &node, &offset);
      if (NS_FAILED(res)) return res;
      if (!node) return NS_ERROR_FAILURE;
  
      if (IsBlockNode(node)) blockParent = node;
      else blockParent = GetBlockNodeParent(node);
      
      if (blockParent)
      {
        PRBool bHandled = PR_FALSE;
        
        if (nsHTMLEditUtils::IsTableElement(blockParent))
          res = TabInTable(isShift, &bHandled);
        else if (nsHTMLEditUtils::IsListItem(blockParent))
        {
          nsAutoString indentstr;
          if (isShift) indentstr.AssignWithConversion("outdent");
          else         indentstr.AssignWithConversion("indent");
          res = Indent(indentstr);
          bHandled = PR_TRUE;
        }
        if (NS_FAILED(res)) return res;
        if (bHandled) return res;
      }
    }
    else if (keyCode == nsIDOMKeyEvent::DOM_VK_RETURN
             || keyCode == nsIDOMKeyEvent::DOM_VK_ENTER)
    {
      nsAutoString empty;
      if (isShift && !(mFlags&eEditorPlaintextBit))
      {
        return TypedText(empty, eTypedBR);  // only inserts a br node
      }
      else 
      {
        return TypedText(empty, eTypedBreak);  // uses rules to figure out what to insert
      }
    }
    else if (keyCode == nsIDOMKeyEvent::DOM_VK_ESCAPE)
    {
      // pass escape keypresses through as empty strings: needed forime support
      nsAutoString empty;
      return TypedText(empty, eTypedText);
    }
    
    // if we got here we either fell out of the tab case or have a normal character.
    // Either way, treat as normal character.
    if (character && !altKey && !ctrlKey && !isShift && !metaKey)
    {
      nsAutoString key(character);
      return TypedText(key, eTypedText);
    }
  }
  return NS_ERROR_FAILURE;
}

/* This routine is needed to provide a bottleneck for typing for logging
   purposes.  Can't use EditorKeyPress() (above) for that since it takes
   a nsIDOMUIEvent* parameter.  So instead we pass enough info through
   to TypedText() to determine what action to take, but without passing
   an event.
   */
NS_IMETHODIMP nsHTMLEditor::TypedText(const nsString& aString, PRInt32 aAction)
{
  nsAutoPlaceHolderBatch batch(this, gTypingTxnName);

  switch (aAction)
  {
    case eTypedText:
      {
        return InsertText(aString);
      }
    case eTypedBR:
      {
        nsCOMPtr<nsIDOMNode> brNode;
        return InsertBR(&brNode);  // only inserts a br node
      }
    case eTypedBreak:
      {
        return InsertBreak();  // uses rules to figure out what to insert
      } 
  } 
  return NS_ERROR_FAILURE; 
}

NS_IMETHODIMP nsHTMLEditor::TabInTable(PRBool inIsShift, PRBool *outHandled)
{
  if (!outHandled) return NS_ERROR_NULL_POINTER;
  *outHandled = PR_FALSE;

  // Find enclosing table cell from the selection (cell may be the selected element)
  nsCOMPtr<nsIDOMElement> cellElement;
    // can't use |NS_LITERAL_STRING| here until |GetElementOrParentByTagName| is fixed to accept readables
  nsresult res = GetElementOrParentByTagName(NS_ConvertASCIItoUCS2("td"), nsnull, getter_AddRefs(cellElement));
  if (NS_FAILED(res)) return res;
  // Do nothing -- we didn't find a table cell
  if (!cellElement) return NS_OK;

  // find enclosing table
  nsCOMPtr<nsIDOMNode> tbl = GetEnclosingTable(cellElement);
  if (!tbl) return res;

  // advance to next cell
  // first create an iterator over the table
  nsCOMPtr<nsIContentIterator> iter;
  res = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                           NS_GET_IID(nsIContentIterator), 
                                           getter_AddRefs(iter));
  if (NS_FAILED(res)) return res;
  if (!iter) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIContent> cTbl = do_QueryInterface(tbl);
  nsCOMPtr<nsIContent> cBlock = do_QueryInterface(cellElement);
  res = iter->Init(cTbl);
  if (NS_FAILED(res)) return res;
  // position iter at block
  res = iter->PositionAt(cBlock);
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIContent> cNode;
  do
  {
    if (inIsShift) res = iter->Prev();
    else res = iter->Next();
    if (NS_FAILED(res)) break;
    res = iter->CurrentNode(getter_AddRefs(cNode));
    if (NS_FAILED(res)) break;
    node = do_QueryInterface(cNode);
    if (nsHTMLEditUtils::IsTableCell(node) && (GetEnclosingTable(node) == tbl))
    {
      res = CollapseSelectionToDeepestNonTableFirstChild(nsnull, node);
      if (NS_FAILED(res)) return res;
      *outHandled = PR_TRUE;
      return NS_OK;
    }
  } while (iter->IsDone() == NS_ENUMERATOR_FALSE);
  
  if (!(*outHandled) && !inIsShift)
  {
    // if we havent handled it yet then we must have run off the end of
    // the table.  Insert a new row.
    res = InsertTableRow(1, PR_TRUE);
    if (NS_FAILED(res)) return res;
    *outHandled = PR_TRUE;
    // put selection in right place
    // Use table code to get selection and index to new row...
    nsCOMPtr<nsIDOMSelection>selection;
    nsCOMPtr<nsIDOMElement> tblElement;
    nsCOMPtr<nsIDOMElement> cell;
    PRInt32 row;
    res = GetCellContext(getter_AddRefs(selection), 
                         getter_AddRefs(tblElement),
                         getter_AddRefs(cell), 
                         nsnull, nsnull,
                         &row, nsnull);
    if (NS_FAILED(res)) return res;
    // ...so that we can ask for first cell in that row...
    res = GetCellAt(tblElement, row, 0, *(getter_AddRefs(cell)));
    if (NS_FAILED(res)) return res;
    // ...and then set selection there.
    // (Note that normally you should use CollapseSelectionToDeepestNonTableFirstChild(),
    //  but we know cell is an empty new cell, so this works fine)
    node = do_QueryInterface(cell);
    if (node) selection->Collapse(node,0);
    return NS_OK;
  }
  
  return res;
}

NS_IMETHODIMP nsHTMLEditor::CreateBRImpl(nsCOMPtr<nsIDOMNode> *aInOutParent, PRInt32 *aInOutOffset, nsCOMPtr<nsIDOMNode> *outBRNode, EDirection aSelect)
{
  if (!aInOutParent || !*aInOutParent || !aInOutOffset || !outBRNode) return NS_ERROR_NULL_POINTER;
  *outBRNode = nsnull;
  nsresult res;
  
  // we need to insert a br.  unfortunately, we may have to split a text node to do it.
  nsCOMPtr<nsIDOMNode> node = *aInOutParent;
  PRInt32 theOffset = *aInOutOffset;
  nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(node);
  nsAutoString brType; brType.AssignWithConversion("br");
  nsCOMPtr<nsIDOMNode> brNode;
  if (nodeAsText)  
  {
    nsCOMPtr<nsIDOMNode> tmp;
    PRInt32 offset;
    PRUint32 len;
    nodeAsText->GetLength(&len);
    GetNodeLocation(node, &tmp, &offset);
    if (!tmp) return NS_ERROR_FAILURE;
    if (!theOffset)
    {
      // we are already set to go
    }
    else if (theOffset == (PRInt32)len)
    {
      // update offset to point AFTER the text node
      offset++;
    }
    else
    {
      // split the text node
      res = SplitNode(node, theOffset, getter_AddRefs(tmp));
      if (NS_FAILED(res)) return res;
      res = GetNodeLocation(node, &tmp, &offset);
      if (NS_FAILED(res)) return res;
    }
    // create br
    res = CreateNode(brType, tmp, offset, getter_AddRefs(brNode));
    if (NS_FAILED(res)) return res;
    *aInOutParent = tmp;
    *aInOutOffset = offset+1;
  }
  else
  {
    res = CreateNode(brType, node, theOffset, getter_AddRefs(brNode));
    if (NS_FAILED(res)) return res;
    (*aInOutOffset)++;
  }

  *outBRNode = brNode;
  if (*outBRNode && (aSelect != eNone))
  {
    nsCOMPtr<nsIDOMSelection> selection;
    nsCOMPtr<nsIDOMNode> parent;
    PRInt32 offset;
    res = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    res = GetNodeLocation(*outBRNode, &parent, &offset);
    if (NS_FAILED(res)) return res;
    if (aSelect == eNext)
    {
      // position selection after br
      selection->SetHint(PR_TRUE);
      res = selection->Collapse(parent, offset+1);
    }
    else if (aSelect == ePrevious)
    {
      // position selection before br
      selection->SetHint(PR_TRUE);
      res = selection->Collapse(parent, offset);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP nsHTMLEditor::CreateBR(nsIDOMNode *aNode, PRInt32 aOffset, nsCOMPtr<nsIDOMNode> *outBRNode, EDirection aSelect)
{
  nsCOMPtr<nsIDOMNode> parent = aNode;
  PRInt32 offset = aOffset;
  return CreateBRImpl(&parent, &offset, outBRNode, aSelect);
}

NS_IMETHODIMP nsHTMLEditor::InsertBR(nsCOMPtr<nsIDOMNode> *outBRNode)
{
  PRBool bCollapsed;
  nsCOMPtr<nsIDOMSelection> selection;

  if (!outBRNode) return NS_ERROR_NULL_POINTER;
  *outBRNode = nsnull;

  // calling it text insertion to trigger moz br treatment by rules
  nsAutoRules beginRulesSniffing(this, kOpInsertText, nsIEditor::eNext);

  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  res = selection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed)
  {
    res = DeleteSelection(nsIEditor::eNone);
    if (NS_FAILED(res)) return res;
  }
  nsCOMPtr<nsIDOMNode> selNode;
  PRInt32 selOffset;
  res = GetStartNodeAndOffset(selection, &selNode, &selOffset);
  if (NS_FAILED(res)) return res;
  
  res = CreateBR(selNode, selOffset, outBRNode);
  if (NS_FAILED(res)) return res;
    
  // position selection after br
  res = GetNodeLocation(*outBRNode, &selNode, &selOffset);
  if (NS_FAILED(res)) return res;
  selection->SetHint(PR_TRUE);
  res = selection->Collapse(selNode, selOffset+1);
  
  return res;
}

NS_IMETHODIMP nsHTMLEditor::SetInlineProperty(nsIAtom *aProperty, 
                                            const nsString *aAttribute,
                                            const nsString *aValue)
{
  if (!aProperty) { return NS_ERROR_NULL_POINTER; }
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }
  ForceCompositionEnd();

  nsresult res;
  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);
  if (isCollapsed)
  {
    // manipulating text attributes on a collapsed selection only sets state for the next text insertion
    return mTypeInState->SetProp(aProperty, *aAttribute, *aValue);
  }
  
  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, kOpInsertElement, nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);
  
  PRBool cancel, handled;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kSetTextProperty);
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(res)) return res;
  if (!cancel && !handled)
  {
    // get selection range enumerator
    nsCOMPtr<nsIEnumerator> enumerator;
    res = selection->GetEnumerator(getter_AddRefs(enumerator));
    if (NS_FAILED(res)) return res;
    if (!enumerator)    return NS_ERROR_FAILURE;

    // loop thru the ranges in the selection
    enumerator->First(); 
    nsCOMPtr<nsISupports> currentItem;
    while ((NS_ENUMERATOR_FALSE == enumerator->IsDone()))
    {
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if (NS_FAILED(res)) return res;
      if (!currentItem)   return NS_ERROR_FAILURE;
      
      nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );

      // adjust range to include any ancestors who's children are entirely selected
      res = PromoteInlineRange(range);
      if (NS_FAILED(res)) return res;
      
      // check for easy case: both range endpoints in same text node
      nsCOMPtr<nsIDOMNode> startNode, endNode;
      res = range->GetStartParent(getter_AddRefs(startNode));
      if (NS_FAILED(res)) return res;
      res = range->GetEndParent(getter_AddRefs(endNode));
      if (NS_FAILED(res)) return res;
      if ((startNode == endNode) && IsTextNode(startNode))
      {
        // MOOSE: workaround for selection bug:
        //selection->ClearSelection();

        PRInt32 startOffset, endOffset;
        range->GetStartOffset(&startOffset);
        range->GetEndOffset(&endOffset);
        nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
        res = SetInlinePropertyOnTextNode(nodeAsText, startOffset, endOffset, aProperty, aAttribute, aValue);
        if (NS_FAILED(res)) return res;
      }
      else
      {
        // not the easy case.  range not contained in single text node. 
        // there are up to three phases here.  There are all the nodes
        // reported by the subtree iterator to be processed.  And there
        // are potentially a starting textnode and an ending textnode
        // which are only partially contained by the range.
        
        // lets handle the nodes reported by the iterator.  These nodes
        // are entirely contained in the selection range.  We build up
        // a list of them (since doing operations on the document during
        // iteration would perturb the iterator).

        nsCOMPtr<nsIContentIterator> iter;
        res = nsComponentManager::CreateInstance(kSubtreeIteratorCID, nsnull,
                                                  NS_GET_IID(nsIContentIterator), 
                                                  getter_AddRefs(iter));
        if (NS_FAILED(res)) return res;
        if (!iter)          return NS_ERROR_FAILURE;

        nsCOMPtr<nsISupportsArray> arrayOfNodes;
        nsCOMPtr<nsIContent> content;
        nsCOMPtr<nsIDOMNode> node;
        nsCOMPtr<nsISupports> isupports;
        
        // make a array
        res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
        if (NS_FAILED(res)) return res;
        
        // iterate range and build up array
        res = iter->Init(range);
        // init returns an error if no nodes in range.
        // this can easily happen with the subtree 
        // iterator if the selection doesn't contain
        // any *whole* nodes.
        if (NS_SUCCEEDED(res))
        {
          while (NS_ENUMERATOR_FALSE == iter->IsDone())
          {
            res = iter->CurrentNode(getter_AddRefs(content));
            if (NS_FAILED(res)) return res;
            node = do_QueryInterface(content);
            if (!node) return NS_ERROR_FAILURE;
            if (IsEditable(node))
            { 
              isupports = do_QueryInterface(node);
              arrayOfNodes->AppendElement(isupports);
            }
            res = iter->Next();
            if (NS_FAILED(res)) return res;
          }
        }
        // MOOSE: workaround for selection bug:
        //selection->ClearSelection();
        
        // first check the start parent of the range to see if it needs to 
        // be seperately handled (it does if it's a text node, due to how the
        // subtree iterator works - it will not have reported it).
        if (IsTextNode(startNode) && IsEditable(startNode))
        {
          nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
          PRInt32 startOffset;
          PRUint32 textLen;
          range->GetStartOffset(&startOffset);
          nodeAsText->GetLength(&textLen);
          res = SetInlinePropertyOnTextNode(nodeAsText, startOffset, textLen, aProperty, aAttribute, aValue);
          if (NS_FAILED(res)) return res;
        }
        
        // then loop through the list, set the property on each node
        PRUint32 listCount;
        PRUint32 j;
        arrayOfNodes->Count(&listCount);
        for (j = 0; j < listCount; j++)
        {
          isupports = (dont_AddRef)(arrayOfNodes->ElementAt(0));
          node = do_QueryInterface(isupports);
          res = SetInlinePropertyOnNode(node, aProperty, aAttribute, aValue);
          if (NS_FAILED(res)) return res;
          arrayOfNodes->RemoveElementAt(0);
        }
        
        // last check the end parent of the range to see if it needs to 
        // be seperately handled (it does if it's a text node, due to how the
        // subtree iterator works - it will not have reported it).
        if (IsTextNode(endNode) && IsEditable(endNode))
        {
          nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(endNode);
          PRInt32 endOffset;
          range->GetEndOffset(&endOffset);
          res = SetInlinePropertyOnTextNode(nodeAsText, 0, endOffset, aProperty, aAttribute, aValue);
          if (NS_FAILED(res)) return res;
        }
      }
      enumerator->Next();
    }
  }
  if (!cancel)
  {
    // post-process
    res = mRules->DidDoAction(selection, &ruleInfo, res);
  }
  return res;
}



nsresult
nsHTMLEditor::SetInlinePropertyOnTextNode( nsIDOMCharacterData *aTextNode, 
                                            PRInt32 aStartOffset,
                                            PRInt32 aEndOffset,
                                            nsIAtom *aProperty, 
                                            const nsString *aAttribute,
                                            const nsString *aValue)
{
  if (!aTextNode) return NS_ERROR_NULL_POINTER;
  
  // dont need to do anything if no characters actually selected
  if (aStartOffset == aEndOffset) return NS_OK;
  
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> tmp, node = do_QueryInterface(aTextNode);
  
  // dont need to do anything if property already set on node
  PRBool bHasProp;
  nsCOMPtr<nsIDOMNode> styleNode;
  IsTextPropertySetByContent(node, aProperty, aAttribute, aValue, bHasProp, getter_AddRefs(styleNode));
  if (bHasProp) return NS_OK;
  
  // do we need to split the text node?
  PRUint32 textLen;
  aTextNode->GetLength(&textLen);
  
  if ( (PRUint32)aEndOffset != textLen )
  {
    // we need to split off back of text node
    res = SplitNode(node, aEndOffset, getter_AddRefs(tmp));
    if (NS_FAILED(res)) return res;
    node = tmp;  // remember left node
  }
  if ( aStartOffset )
  {
    // we need to split off front of text node
    res = SplitNode(node, aStartOffset, getter_AddRefs(tmp));
    if (NS_FAILED(res)) return res;
  }
  
  // reparent the node inside inline node with appropriate {attribute,value}
  res = SetInlinePropertyOnNode(node, aProperty, aAttribute, aValue);
  return res;
}


nsresult
nsHTMLEditor::SetInlinePropertyOnNode( nsIDOMNode *aNode,
                                       nsIAtom *aProperty, 
                                       const nsString *aAttribute,
                                       const nsString *aValue)
{
  if (!aNode || !aProperty) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> tmp;
  nsAutoString tag;
  aProperty->ToString(tag);
  tag.ToLowerCase();
  
  // dont need to do anything if property already set on node
  PRBool bHasProp;
  nsCOMPtr<nsIDOMNode> styleNode;
  IsTextPropertySetByContent(aNode, aProperty, aAttribute, aValue, bHasProp, getter_AddRefs(styleNode));
  if (bHasProp) return NS_OK;

  // is it already the right kind of node, but with wrong attribute?
  if (NodeIsType(aNode, aProperty))
  {
    // just set the attribute on it.
    // but first remove any contrary style in it's children.
    res = RemoveStyleInside(aNode, aProperty, aAttribute, PR_TRUE);
    if (NS_FAILED(res)) return res;
    nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);
    return SetAttribute(elem, *aAttribute, *aValue);
  }
  
  // can it be put inside inline node?
  if (TagCanContain(tag, aNode))
  {
    nsCOMPtr<nsIDOMNode> priorNode, nextNode;
    // is either of it's neighbors the right kind of node?
    GetPriorHTMLSibling(aNode, &priorNode);
    GetNextHTMLSibling(aNode, &nextNode);
    if (priorNode && NodeIsType(priorNode, aProperty) && 
        HasAttrVal(priorNode, aAttribute, aValue)     &&
        IsOnlyAttribute(priorNode, aAttribute) )
    {
      // previous sib is already right kind of inline node; slide this over into it
      res = MoveNode(aNode, priorNode, -1);
    }
    else if (nextNode && NodeIsType(nextNode, aProperty) && 
             HasAttrVal(nextNode, aAttribute, aValue)    &&
             IsOnlyAttribute(priorNode, aAttribute) )
    {
      // following sib is already right kind of inline node; slide this over into it
      res = MoveNode(aNode, nextNode, 0);
    }
    else
    {
      // ok, chuck it in it's very own container
      res = InsertContainerAbove(aNode, &tmp, tag, aAttribute, aValue);
    }
    if (NS_FAILED(res)) return res;
    return RemoveStyleInside(aNode, aProperty, aAttribute);
  }
  // none of the above?  then cycle through the children.
  nsCOMPtr<nsIDOMNodeList> childNodes;
  res = aNode->GetChildNodes(getter_AddRefs(childNodes));
  if (NS_FAILED(res)) return res;
  if (childNodes)
  {
    PRInt32 j;
    PRUint32 childCount;
    childNodes->GetLength(&childCount);
    if (childCount)
    {
      nsCOMPtr<nsISupportsArray> arrayOfNodes;
      nsCOMPtr<nsIDOMNode> node;
      nsCOMPtr<nsISupports> isupports;
      
      // make a array
      res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
      if (NS_FAILED(res)) return res;
      
      // populate the list
      for (j=0 ; j < (PRInt32)childCount; j++)
      {
        nsCOMPtr<nsIDOMNode> childNode;
        res = childNodes->Item(j, getter_AddRefs(childNode));
        if ((NS_SUCCEEDED(res)) && (childNode) && IsEditable(childNode))
        {
          isupports = do_QueryInterface(childNode);
          arrayOfNodes->AppendElement(isupports);
        }
      }
      
      // then loop through the list, set the property on each node
      PRUint32 listCount;
      arrayOfNodes->Count(&listCount);
      for (j = 0; j < (PRInt32)listCount; j++)
      {
        isupports = (dont_AddRef)(arrayOfNodes->ElementAt(0));
        node = do_QueryInterface(isupports);
        res = SetInlinePropertyOnNode(node, aProperty, aAttribute, aValue);
        if (NS_FAILED(res)) return res;
        arrayOfNodes->RemoveElementAt(0);
      }
    }
  }
  return res;
}


nsresult nsHTMLEditor::SplitStyleAboveRange(nsIDOMRange *inRange, 
                                            nsIAtom *aProperty, 
                                            const nsString *aAttribute)
{
  if (!inRange) return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr<nsIDOMNode> startNode, endNode, origStartNode;
  PRInt32 startOffset, endOffset, origStartOffset;
  
  res = inRange->GetStartParent(getter_AddRefs(startNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetStartOffset(&startOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndParent(getter_AddRefs(endNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndOffset(&endOffset);
  if (NS_FAILED(res)) return res;
  
  origStartNode = startNode;
  origStartOffset = startOffset;
  PRBool sameNode = (startNode==endNode);
  
  // split any matching style nodes above the start of range
  res = SplitStyleAbovePoint(&startNode, &startOffset, aProperty, aAttribute);
  if (NS_FAILED(res)) return res;
  
  if (sameNode && (startNode != origStartNode))
  {
    // our startNode got split.  This changes the offset of the end of our range.
    endOffset -= origStartOffset;
  }
  
  // second verse, same as the first...
  res = SplitStyleAbovePoint(&endNode, &endOffset, aProperty, aAttribute);
  if (NS_FAILED(res)) return res;
  
  // reset the range
  res = inRange->SetStart(startNode, startOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->SetEnd(endNode, endOffset);
  return res;
}

nsresult nsHTMLEditor::SplitStyleAbovePoint(nsCOMPtr<nsIDOMNode> *aNode,
                                           PRInt32 *aOffset,
                                           nsIAtom *aProperty,          // null here means we split all properties
                                           const nsString *aAttribute)
{
  if (!aNode || !*aNode || !aOffset) return NS_ERROR_NULL_POINTER;
  // split any matching style nodes above the node/offset
  nsCOMPtr<nsIDOMNode> parent, tmp = *aNode;
  PRInt32 offset;
  while (tmp && !nsHTMLEditUtils::IsBody(tmp))
  {
    if ( (aProperty && NodeIsType(tmp, aProperty)) ||   // node is the correct inline prop
         (aProperty == nsIEditProperty::href && IsLinkNode(tmp)) || // node is href - test if really <a href=...
         (!aProperty && NodeIsProperty(tmp)) )         // or node is any prop, and we asked to split them all
    {
      // found a style node we need to split
      SplitNodeDeep(tmp, *aNode, *aOffset, &offset);
      // reset startNode/startOffset
      tmp->GetParentNode(getter_AddRefs(*aNode));
      *aOffset = offset;
    }
    tmp->GetParentNode(getter_AddRefs(parent));
    tmp = parent;
  }
  return NS_OK;
}

PRBool nsHTMLEditor::NodeIsProperty(nsIDOMNode *aNode)
{
  if (!aNode)               return PR_FALSE;
  if (!IsContainer(aNode))  return PR_FALSE;
  if (!IsEditable(aNode))   return PR_FALSE;
  if (!IsInlineNode(aNode)) return PR_FALSE;
  if (NodeIsType(aNode, nsIEditProperty::a)) return PR_FALSE;
  return PR_TRUE;
}

nsresult nsHTMLEditor::RemoveStyleInside(nsIDOMNode *aNode, 
                                   nsIAtom *aProperty,   // null here means remove all properties
                                   const nsString *aAttribute, 
                                   PRBool aChildrenOnly)
{
  if (!aNode) return NS_ERROR_NULL_POINTER;
  if (IsTextNode(aNode)) return NS_OK;
  nsresult res = NS_OK;

  // first process the children
  nsCOMPtr<nsIDOMNode> child, tmp;
  aNode->GetFirstChild(getter_AddRefs(child));
  while (child)
  {
    // cache next sibling since we might remove child
    child->GetNextSibling(getter_AddRefs(tmp));
    res = RemoveStyleInside(child, aProperty, aAttribute);
    if (NS_FAILED(res)) return res;
    child = tmp;
  }

  // then process the node itself
  if ( !aChildrenOnly && 
       ((aProperty && NodeIsType(aNode, aProperty)) || // node is prop we asked for
        (aProperty == nsIEditProperty::href && IsLinkNode(aNode))) || // but check for link (<a href=...)
       (!aProperty && NodeIsProperty(aNode)) )        // or node is any prop and we asked for that
  {
    // if we weren't passed an attribute, then we want to 
    // remove any matching inlinestyles entirely
    if (!aAttribute || aAttribute->IsEmpty())
    {
      res = RemoveContainer(aNode);
    }
    // otherwise we just want to eliminate the attribute
    else
    {
      if (HasAttr(aNode, aAttribute))
      {
        // if this matching attribute is the ONLY one on the node,
        // then remove the whole node.  Otherwise just nix the attribute.
        if (IsOnlyAttribute(aNode, aAttribute))
        {
          res = RemoveContainer(aNode);
        }
        else
        {
          nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);
          if (!elem) return NS_ERROR_NULL_POINTER;
          res = RemoveAttribute(elem, *aAttribute);
        }
      }
    }
  }
  return res;
}

PRBool nsHTMLEditor::IsOnlyAttribute(nsIDOMNode *aNode, 
                                     const nsString *aAttribute)
{
  if (!aNode || !aAttribute) return PR_FALSE;  // ooops
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (!content) return PR_FALSE;  // ooops
  
  PRInt32 attrCount, i, nameSpaceID;
  nsCOMPtr<nsIAtom> attrName, prefix;
  content->GetAttributeCount(attrCount);
  
  for (i=0; i<attrCount; i++)
  {
    content->GetAttributeNameAt(i, nameSpaceID, *getter_AddRefs(attrName),
                                *getter_AddRefs(prefix));
    nsAutoString attrString, tmp;
    if (!attrName) continue;  // ooops
    attrName->ToString(attrString);
    // if it's the attribute we know about, keep looking
    if (attrString.EqualsIgnoreCase(*aAttribute)) continue;
    // if it's a special _moz... attribute, keep looking
    attrString.Left(tmp,4);
    if (tmp.EqualsWithConversion("_moz")) continue;
    // otherwise, it's another attribute, so return false
    return PR_FALSE;
  }
  // if we made it through all of them without finding a real attribute
  // other than aAttribute, then return PR_TRUE
  return PR_TRUE;
}

PRBool 
nsHTMLEditor::HasMatchingAttributes(nsIDOMNode *aNode1, 
                                    nsIDOMNode *aNode2)
{
  if (!aNode1  || !aNode2) return PR_FALSE;  // ooops
  nsCOMPtr<nsIContent> content1 = do_QueryInterface(aNode1);
  if (!content1) return PR_FALSE;  // ooops
  nsCOMPtr<nsIContent> content2 = do_QueryInterface(aNode2);
  if (!content2) return PR_FALSE;  // ooops
  
  PRInt32 attrCount, i, nameSpaceID, realCount1=0, realCount2=0;
  nsCOMPtr<nsIAtom> attrName, prefix;
  nsresult res, res2;
  content1->GetAttributeCount(attrCount);
  nsAutoString attrString, tmp, attrVal1, attrVal2;
  
  for (i=0; i<attrCount; i++)
  {
    content1->GetAttributeNameAt(i, nameSpaceID, *getter_AddRefs(attrName),
                                 *getter_AddRefs(prefix));
    if (!attrName) continue;  // ooops
    attrName->ToString(attrString);
    // if it's a special _moz... attribute, keep going
    attrString.Left(tmp,4);
    if (tmp.EqualsWithConversion("_moz")) continue;
    // otherwise, it's another attribute, so count it
    realCount1++;
    // and compare it to element2's attributes
    res = content1->GetAttribute(nameSpaceID, attrName, attrVal1);
    res2 = content2->GetAttribute(nameSpaceID, attrName, attrVal2);
    if (res != res2) return PR_FALSE;
    if (!attrVal1.EqualsIgnoreCase(attrVal2)) return PR_FALSE;
  }

  content2->GetAttributeCount(attrCount);
  for (i=0; i<attrCount; i++)
  {
    content2->GetAttributeNameAt(i, nameSpaceID, *getter_AddRefs(attrName),
                                 *getter_AddRefs(prefix));
    if (!attrName) continue;  // ooops
    attrName->ToString(attrString);
    // if it's a special _moz... attribute, keep going
    attrString.Left(tmp,4);
    if (tmp.EqualsWithConversion("_moz")) continue;
    // otherwise, it's another attribute, so count it
    realCount2++;
  }

  if (realCount1 != realCount2) return PR_FALSE; 
  // otherwise, attribute counts match, and we already compared them 
  // when going through the first list, so we're done.
  return PR_TRUE;
}

PRBool nsHTMLEditor::HasAttr(nsIDOMNode *aNode, 
                             const nsString *aAttribute)
{
  if (!aNode) return PR_FALSE;
  if (!aAttribute || aAttribute->IsEmpty()) return PR_TRUE;  // everybody has the 'null' attribute
  
  // get element
  nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);
  if (!elem) return PR_FALSE;
  
  // get attribute node
  nsCOMPtr<nsIDOMAttr> attNode;
  nsresult res = elem->GetAttributeNode(*aAttribute, getter_AddRefs(attNode));
  if ((NS_FAILED(res)) || !attNode) return PR_FALSE;
  return PR_TRUE;
}


PRBool nsHTMLEditor::HasAttrVal(nsIDOMNode *aNode, 
                                const nsString *aAttribute, 
                                const nsString *aValue)
{
  if (!aNode) return PR_FALSE;
  if (!aAttribute || aAttribute->IsEmpty()) return PR_TRUE;  // everybody has the 'null' attribute
  
  // get element
  nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);
  if (!elem) return PR_FALSE;
  
  // get attribute node
  nsCOMPtr<nsIDOMAttr> attNode;
  nsresult res = elem->GetAttributeNode(*aAttribute, getter_AddRefs(attNode));
  if ((NS_FAILED(res)) || !attNode) return PR_FALSE;
  
  // check if attribute has a value
  PRBool isSet;
  attNode->GetSpecified(&isSet);
  // if no value, and that's what we wanted, then return true
  if (!isSet && (!aValue || aValue->IsEmpty())) return PR_TRUE; 
  
  // get attribute value
  nsAutoString attrVal;
  attNode->GetValue(attrVal);
  
  // do values match?
  if (attrVal.EqualsIgnoreCase(*aValue)) return PR_TRUE;
  return PR_FALSE;
}


nsresult nsHTMLEditor::PromoteInlineRange(nsIDOMRange *inRange)
{
  if (!inRange) return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr<nsIDOMNode> startNode, endNode, parent;
  PRInt32 startOffset, endOffset;
  
  res = inRange->GetStartParent(getter_AddRefs(startNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetStartOffset(&startOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndParent(getter_AddRefs(endNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndOffset(&endOffset);
  if (NS_FAILED(res)) return res;
  
  while ( startNode && 
          !nsHTMLEditUtils::IsBody(startNode) && 
          IsAtFrontOfNode(startNode, startOffset) )
  {
    res = GetNodeLocation(startNode, &parent, &startOffset);
    if (NS_FAILED(res)) return res;
    startNode = parent;
  }
  if (!startNode) return NS_ERROR_NULL_POINTER;
  
  while ( endNode && 
          !nsHTMLEditUtils::IsBody(endNode) && 
          IsAtEndOfNode(endNode, endOffset) )
  {
    res = GetNodeLocation(endNode, &parent, &endOffset);
    if (NS_FAILED(res)) return res;
    endNode = parent;
    endOffset++;  // we are AFTER this node
  }
  if (!endNode) return NS_ERROR_NULL_POINTER;
  
  res = inRange->SetStart(startNode, startOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->SetEnd(endNode, endOffset);
  return res;
}

PRBool nsHTMLEditor::IsAtFrontOfNode(nsIDOMNode *aNode, PRInt32 aOffset)
{
  if (!aNode) return PR_FALSE;  // oops
  if (!aOffset) return PR_TRUE;
  
  if (IsTextNode(aNode))
  {
    return PR_FALSE;
  }
  else
  {
    nsCOMPtr<nsIDOMNode> firstNode;
    GetFirstEditableChild(aNode, &firstNode);
    if (!firstNode) return PR_TRUE; 
    PRInt32 offset;
    nsEditor::GetChildOffset(firstNode, aNode, offset);
    if (offset < aOffset) return PR_FALSE;
    return PR_TRUE;
  }
}

PRBool nsHTMLEditor::IsAtEndOfNode(nsIDOMNode *aNode, PRInt32 aOffset)
{
  if (!aNode) return PR_FALSE;  // oops
  PRUint32 len;
  GetLengthOfDOMNode(aNode, len);
  if (aOffset == (PRInt32)len) return PR_TRUE;
  
  if (IsTextNode(aNode))
  {
    return PR_FALSE;
  }
  else
  {
    nsCOMPtr<nsIDOMNode> lastNode;
    GetLastEditableChild(aNode, &lastNode);
    if (!lastNode) return PR_TRUE; 
    PRInt32 offset;
    nsEditor::GetChildOffset(lastNode, aNode, offset);
    if (offset < aOffset) return PR_TRUE;
    return PR_FALSE;
  }
}

NS_IMETHODIMP nsHTMLEditor::GetInlineProperty(nsIAtom *aProperty, 
                                              const nsString *aAttribute, 
                                              const nsString *aValue,
                                              PRBool &aFirst, 
                                              PRBool &aAny, 
                                              PRBool &aAll)
{
  return GetInlinePropertyWithAttrValue( aProperty, aAttribute, aValue, aFirst, aAny, aAll, nsnull);
}

NS_IMETHODIMP nsHTMLEditor::GetInlinePropertyWithAttrValue(nsIAtom *aProperty, 
                                              const nsString *aAttribute, 
                                              const nsString *aValue,
                                              PRBool &aFirst, 
                                              PRBool &aAny, 
                                              PRBool &aAll,
                                              nsString *outValue)
{
  if (!aProperty)
    return NS_ERROR_NULL_POINTER;
/*
  if (gNoisy) 
  { 
    nsAutoString propString;
    aProperty->ToString(propString);
    char *propCString = propString.ToNewCString();
    if (gNoisy) { printf("nsTextEditor::GetTextProperty %s\n", propCString); }
    nsCRT::free(propCString);
  }
*/
  nsresult result;
  aAny=PR_FALSE;
  aAll=PR_TRUE;
  aFirst=PR_FALSE;
  PRBool first=PR_TRUE;
  nsCOMPtr<nsIDOMSelection>selection;
  result = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) return result;
  if (!selection) return NS_ERROR_NULL_POINTER;

  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);
  nsCOMPtr<nsIDOMNode> collapsedNode;
  nsCOMPtr<nsIEnumerator> enumerator;
  result = selection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(result)) return result;
  if (!enumerator) return NS_ERROR_NULL_POINTER;

  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  result = enumerator->CurrentItem(getter_AddRefs(currentItem));
  // XXX: should be a while loop, to get each separate range
  // XXX: ERROR_HANDLING can currentItem be null?
  if ((NS_SUCCEEDED(result)) && currentItem)
  {
    PRBool firstNodeInRange = PR_TRUE; // for each range, set a flag 
    nsCOMPtr<nsIDOMRange> range(do_QueryInterface(currentItem));

    if (isCollapsed)
    {
      // efficiency hack.  we cache prior results for being collapsed in a given text node.
      // this speeds up typing.  Note that other parts of the editor code have to clear out 
      // this cache after certain actions.
      range->GetStartParent(getter_AddRefs(collapsedNode));
      if (!collapsedNode) return NS_ERROR_FAILURE;
      // refresh the cache if we need to
      if (collapsedNode != mCachedNode) CacheInlineStyles(collapsedNode);
      // cache now current, use it!  But override it with typeInState results if any...
      PRBool isSet, theSetting;
      if (aProperty == mBoldAtom.get())
      {
        mTypeInState->GetTypingState(isSet, theSetting, aProperty);
        if (isSet) 
        {
          aFirst = aAny = aAll = theSetting;
        }
        else
        {
          aFirst = aAny = aAll = mCachedBoldStyle;
        }
        return NS_OK;
      }
      else if (aProperty == mItalicAtom.get())
      {
        mTypeInState->GetTypingState(isSet, theSetting, aProperty);
        if (isSet) 
        {
          aFirst = aAny = aAll = theSetting;
        }
        else
        {
          aFirst = aAny = aAll = mCachedItalicStyle;
        }
        return NS_OK;
      }
      else if (aProperty == mUnderlineAtom.get())
      {
        mTypeInState->GetTypingState(isSet, theSetting, aProperty);
        if (isSet) 
        {
          aFirst = aAny = aAll = theSetting;
        }
        else
        {
          aFirst = aAny = aAll = mCachedUnderlineStyle;
        }
        return NS_OK;
      }
    }

    // either non-collapsed selection or no cached value: do it the hard way
    nsCOMPtr<nsIContentIterator> iter;
    result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                                NS_GET_IID(nsIContentIterator), 
                                                getter_AddRefs(iter));
    if (NS_FAILED(result)) return result;
    if (!iter) return NS_ERROR_NULL_POINTER;

    iter->Init(range);
    nsCOMPtr<nsIContent> content;
    nsAutoString firstValue, theValue;
    iter->CurrentNode(getter_AddRefs(content));
    while (NS_ENUMERATOR_FALSE == iter->IsDone())
    {
      //if (gNoisy) { printf("  checking node %p\n", content.get()); }
      nsCOMPtr<nsIDOMCharacterData>text;
      text = do_QueryInterface(content);
      PRBool skipNode = PR_FALSE;
      if (text)
      {
        if (!isCollapsed && first && firstNodeInRange)
        {
          firstNodeInRange = PR_FALSE;
          PRInt32 startOffset;
          range->GetStartOffset(&startOffset);
          PRUint32 count;
          text->GetLength(&count);
          if (startOffset==(PRInt32)count) 
          {
            //if (gNoisy) { printf("  skipping node %p\n", content.get()); }
            skipNode = PR_TRUE;
          }
        }
      }
      else
      { // handle non-text leaf nodes here
        PRBool canContainChildren;
        content->CanContainChildren(canContainChildren);
        if (canContainChildren)
        {
          //if (gNoisy) { printf("  skipping non-leaf node %p\n", content.get()); }
          skipNode = PR_TRUE;
        }
        else {
          //if (gNoisy) { printf("  testing non-text leaf node %p\n", content.get()); }
        }
      }
      if (!skipNode)
      {
        nsCOMPtr<nsIDOMNode>node;
        node = do_QueryInterface(content);
        if (node)
        {
          PRBool isSet;
          nsCOMPtr<nsIDOMNode>resultNode;
          if (first)
          {
            IsTextPropertySetByContent(node, aProperty, aAttribute, aValue, isSet, getter_AddRefs(resultNode), &firstValue);
            aFirst = isSet;
            first = PR_FALSE;
            if (outValue) *outValue = firstValue;
          }
          else
          {
            IsTextPropertySetByContent(node, aProperty, aAttribute, aValue, isSet, getter_AddRefs(resultNode), &theValue);
            if (firstValue != theValue)
              aAll = PR_FALSE;
          }
          
          if (isSet) {
            aAny = PR_TRUE;
          }
          else {
            aAll = PR_FALSE;
          }
        }
      }
      result = iter->Next();
      if (NS_FAILED(result))  
        break;
      iter->CurrentNode(getter_AddRefs(content));
    }
  }
  if (!aAny) 
  { // make sure that if none of the selection is set, we don't report all is set
    aAll = PR_FALSE;
  }
  //if (gNoisy) { printf("  returning first=%d any=%d all=%d\n", aFirst, aAny, aAll); }
  return result;
}


NS_IMETHODIMP nsHTMLEditor::RemoveAllInlineProperties()
{
  return RemoveInlinePropertyImpl(nsnull, nsnull);
}

NS_IMETHODIMP nsHTMLEditor::RemoveInlineProperty(nsIAtom *aProperty, const nsString *aAttribute)
{
  return RemoveInlinePropertyImpl(aProperty, aAttribute);
}

nsresult nsHTMLEditor::RemoveInlinePropertyImpl(nsIAtom *aProperty, const nsString *aAttribute)
{
  if (!mRules)    return NS_ERROR_NOT_INITIALIZED;
  ForceCompositionEnd();

  nsresult res;
  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);
  if (isCollapsed)
  {
    // manipulating text attributes on a collapsed selection only sets state for the next text insertion

    // For links, aProperty uses "href", use "a" instead
    if (aProperty == nsIEditProperty::href)
      aProperty = nsIEditProperty::a;

    if (aProperty) return mTypeInState->ClearProp(aProperty, *aAttribute);
    else return mTypeInState->ClearAllProps();
  }
  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, kOpRemoveTextProperty, nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);
  
  PRBool cancel, handled;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kRemoveTextProperty);
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(res)) return res;
  if (!cancel && !handled)
  {
    // get selection range enumerator
    nsCOMPtr<nsIEnumerator> enumerator;
    res = selection->GetEnumerator(getter_AddRefs(enumerator));
    if (NS_FAILED(res)) return res;
    if (!enumerator)    return NS_ERROR_FAILURE;

    // loop thru the ranges in the selection
    enumerator->First(); 
    nsCOMPtr<nsISupports> currentItem;
    while ((NS_ENUMERATOR_FALSE == enumerator->IsDone()))
    {
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if (NS_FAILED(res)) return res;
      if (!currentItem)   return NS_ERROR_FAILURE;
      
      nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );

      // adjust range to include any ancestors who's children are entirely selected
      res = PromoteInlineRange(range);
      if (NS_FAILED(res)) return res;
      
      // remove this style from ancestors of our range endpoints, 
      // splitting them as appropriate
      res = SplitStyleAboveRange(range, aProperty, aAttribute);
      if (NS_FAILED(res)) return res;
      
      // check for easy case: both range endpoints in same text node
      nsCOMPtr<nsIDOMNode> startNode, endNode;
      res = range->GetStartParent(getter_AddRefs(startNode));
      if (NS_FAILED(res)) return res;
      res = range->GetEndParent(getter_AddRefs(endNode));
      if (NS_FAILED(res)) return res;
      if ((startNode == endNode) && IsTextNode(startNode))
      {
        // we're done with this range!
      }
      else
      {
        // not the easy case.  range not contained in single text node. 
        nsCOMPtr<nsIContentIterator> iter;
        res = nsComponentManager::CreateInstance(kSubtreeIteratorCID, nsnull,
                                                  NS_GET_IID(nsIContentIterator), 
                                                  getter_AddRefs(iter));
        if (NS_FAILED(res)) return res;
        if (!iter)          return NS_ERROR_FAILURE;

        nsCOMPtr<nsISupportsArray> arrayOfNodes;
        nsCOMPtr<nsIContent> content;
        nsCOMPtr<nsIDOMNode> node;
        nsCOMPtr<nsISupports> isupports;
        
        // make a array
        res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
        if (NS_FAILED(res)) return res;
        
        // iterate range and build up array
        iter->Init(range);
        while (NS_ENUMERATOR_FALSE == iter->IsDone())
        {
          res = iter->CurrentNode(getter_AddRefs(content));
          if (NS_FAILED(res)) return res;
          node = do_QueryInterface(content);
          if (!node) return NS_ERROR_FAILURE;
          if (IsEditable(node))
          { 
            isupports = do_QueryInterface(node);
            arrayOfNodes->AppendElement(isupports);
          }
          res = iter->Next();
          if (NS_FAILED(res)) return res;
        }
        
        // loop through the list, remove the property on each node
        PRUint32 listCount;
        PRUint32 j;
        arrayOfNodes->Count(&listCount);
        for (j = 0; j < listCount; j++)
        {
          isupports = (dont_AddRef)(arrayOfNodes->ElementAt(0));
          node = do_QueryInterface(isupports);
          res = RemoveStyleInside(node, aProperty, aAttribute);
          if (NS_FAILED(res)) return res;
          arrayOfNodes->RemoveElementAt(0);
        }
      }
      enumerator->Next();
    }
  }
  if (!cancel)
  {
    // post-process 
    res = mRules->DidDoAction(selection, &ruleInfo, res);
  }
  return res;
}

NS_IMETHODIMP nsHTMLEditor::IncreaseFontSize()
{
  return RelativeFontChange(1);
}

NS_IMETHODIMP nsHTMLEditor::DecreaseFontSize()
{
  return RelativeFontChange(-1);
}

nsresult nsHTMLEditor::GetTextSelectionOffsets(nsIDOMSelection *aSelection,
                                               PRInt32 &aOutStartOffset, 
                                               PRInt32 &aOutEndOffset)
{
  if(!aSelection) { return NS_ERROR_NULL_POINTER; }
  nsresult result;
  // initialize out params
  aOutStartOffset = 0; // default to first char in selection
  aOutEndOffset = -1;  // default to total length of text in selection

  nsCOMPtr<nsIDOMNode> startNode, endNode, parentNode;
  PRInt32 startOffset, endOffset;
  aSelection->GetAnchorNode(getter_AddRefs(startNode));
  aSelection->GetAnchorOffset(&startOffset);
  aSelection->GetFocusNode(getter_AddRefs(endNode));
  aSelection->GetFocusOffset(&endOffset);

  nsCOMPtr<nsIEnumerator> enumerator;
  result = aSelection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(result)) return result;
  if (!enumerator) return NS_ERROR_NULL_POINTER;

  // don't use "result" in this block
  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  nsresult findParentResult = enumerator->CurrentItem(getter_AddRefs(currentItem));
  if ((NS_SUCCEEDED(findParentResult)) && (currentItem))
  {
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    range->GetCommonParent(getter_AddRefs(parentNode));
  }
  else 
  {
    parentNode = do_QueryInterface(startNode);
  }


  return GetAbsoluteOffsetsForPoints(startNode, startOffset, 
                                     endNode, endOffset, 
                                     parentNode,
                                     aOutStartOffset, aOutEndOffset);
}

// this is a complete ripoff from nsTextEditor::GetTextSelectionOffsetsForRange
// the two should use common code, or even just be one method
nsresult nsHTMLEditor::GetAbsoluteOffsetsForPoints(nsIDOMNode *aInStartNode,
                                                   PRInt32 aInStartOffset,
                                                   nsIDOMNode *aInEndNode,
                                                   PRInt32 aInEndOffset,
                                                   nsIDOMNode *aInCommonParentNode,
                                                   PRInt32 &aOutStartOffset, 
                                                   PRInt32 &aOutEndOffset)
{
  if(!aInStartNode || !aInEndNode || !aInCommonParentNode) { return NS_ERROR_NULL_POINTER; }

  nsresult result;
  // initialize out params
  aOutStartOffset = 0; // default to first char in selection
  aOutEndOffset = -1;  // default to total length of text in selection

  nsCOMPtr<nsIContentIterator> iter;
  result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                              NS_GET_IID(nsIContentIterator), 
                                              getter_AddRefs(iter));
  if (NS_FAILED(result)) return result;
  if (!iter) return NS_ERROR_NULL_POINTER;
    
  PRUint32 totalLength=0;
  nsCOMPtr<nsIDOMCharacterData>textNode;
  nsCOMPtr<nsIContent>blockParentContent = do_QueryInterface(aInCommonParentNode);
  iter->Init(blockParentContent);
  // loop through the content iterator for each content node
  nsCOMPtr<nsIContent> content;
  result = iter->CurrentNode(getter_AddRefs(content));
  while (NS_ENUMERATOR_FALSE == iter->IsDone())
  {
    textNode = do_QueryInterface(content);
    if (textNode)
    {
      nsCOMPtr<nsIDOMNode>currentNode = do_QueryInterface(textNode);
      if (!currentNode) {return NS_ERROR_NO_INTERFACE;}
      if (IsEditable(currentNode))
      {
        if (currentNode.get() == aInStartNode)
        {
          aOutStartOffset = totalLength + aInStartOffset;
        }
        if (currentNode.get() == aInEndNode)
        {
          aOutEndOffset = totalLength + aInEndOffset;
          break;
        }
        PRUint32 length;
        textNode->GetLength(&length);
        totalLength += length;
      }
    }
    iter->Next();
    iter->CurrentNode(getter_AddRefs(content));
  }
  if (-1==aOutEndOffset) {
    aOutEndOffset = totalLength;
  }

  // guarantee that aOutStartOffset <= aOutEndOffset
  if (aOutEndOffset<aOutStartOffset) 
  {
    PRInt32 temp;
    temp = aOutStartOffset;
    aOutStartOffset= aOutEndOffset;
    aOutEndOffset = temp;
  }
  NS_POSTCONDITION(aOutStartOffset <= aOutEndOffset, "start > end");
  return result;
}

nsresult 
nsHTMLEditor::GetDOMEventReceiver(nsIDOMEventReceiver **aEventReceiver) 
{ 
  if (!aEventReceiver) 
    return NS_ERROR_NULL_POINTER; 

  *aEventReceiver = 0; 

  nsCOMPtr<nsIDOMElement> rootElement; 

  nsresult result = GetRootElement(getter_AddRefs(rootElement)); 

  if (NS_FAILED(result)) 
    return result; 

  if (!rootElement) 
    return NS_ERROR_FAILURE; 

  // Now hack to make sure we are not anonymous content. 
  // If we are grab the parent of root element for our observer. 

  nsCOMPtr<nsIContent> content = do_QueryInterface(rootElement); 

  if (content) 
  { 
    nsCOMPtr<nsIContent> parent; 
    if (NS_SUCCEEDED(content->GetParent(*getter_AddRefs(parent))) && parent) 
    { 
      PRInt32 index; 
      if (NS_FAILED(parent->IndexOf(content, index)) || index < 0 ) 
      { 
        rootElement = do_QueryInterface(parent); //this will put listener on the form element basically 
        result = rootElement->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), (void **)aEventReceiver); 
      } 
      else 
        rootElement = 0; // Let the event receiver work on the document instead of the root element 
    } 
  } 
  else 
    rootElement = 0; 

  if (!rootElement && mDocWeak) 
  { 
    // Don't use getDocument here, because we have no way of knowing if 
    // Init() was ever called.  So we need to get the document ourselves, 
    // if it exists. 

    nsCOMPtr<nsIDOMDocument> domdoc = do_QueryReferent(mDocWeak); 

    if (!domdoc) 
      return NS_ERROR_FAILURE; 

    result = domdoc->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), (void **)aEventReceiver); 
  } 

  return result; 
} 
  
NS_IMETHODIMP 
nsHTMLEditor::SetCaretToDocumentStart()
{
  nsCOMPtr<nsIDOMElement> bodyElement;
  nsresult res = nsEditor::GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;
  if (!bodyElement)   return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMNode> bodyNode = do_QueryInterface(bodyElement);
  return CollapseSelectionToDeepestNonTableFirstChild(nsnull, bodyNode);  
}

nsresult 
nsHTMLEditor::CollapseSelectionToDeepestNonTableFirstChild(nsIDOMSelection *aSelection, nsIDOMNode *aNode)
{
  if (!aNode) return NS_ERROR_NULL_POINTER;
  nsresult res;

  nsCOMPtr<nsIDOMSelection> selection;
  if (aSelection)
  {
    selection = aSelection;
  } else {
    res = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    if (!selection) return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIDOMNode> node = aNode;
  nsCOMPtr<nsIDOMNode> child;
  
  do {
    node->GetFirstChild(getter_AddRefs(child));
    
    if (child)
    {
      // Stop if we find a table
      // don't want to go into nested tables
      if (nsHTMLEditUtils::IsTable(child)) break;
      // hey, it'g gotta be a container too!
      if (!IsContainer(child)) break;
      node = child;
    }
  }
  while (child);

  selection->Collapse(node,0);
  return NS_OK;
}

NS_IMETHODIMP nsHTMLEditor::DeleteSelection(nsIEditor::EDirection aAction)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel, handled;
  nsresult result;

  // delete placeholder txns merge.
  nsAutoPlaceHolderBatch batch(this, gDeleteTxnName);
  nsAutoRules beginRulesSniffing(this, kOpDeleteSelection, aAction);

  // If it's one of these modes,
  // we have to extend the selection first.
  // This needs to happen inside selection batching,
  // otherwise the deleted text is autocopied to the clipboard.
  if (aAction == eNextWord || aAction == ePreviousWord
      || aAction == eToBeginningOfLine || aAction == eToEndOfLine)
  {
    if (!mSelConWeak) return NS_ERROR_NOT_INITIALIZED;
    nsCOMPtr<nsISelectionController> selCont (do_QueryReferent(mSelConWeak));
    if (!selCont)
      return NS_ERROR_NO_INTERFACE;

    switch (aAction)
    {
        case eNextWord:
          result = selCont->WordMove(PR_TRUE, PR_TRUE);
          // DeleteSelectionImpl doesn't handle these actions
          // because it's inside batching, so don't confuse it:
          aAction = eNone;
          break;
        case ePreviousWord:
          result = selCont->WordMove(PR_FALSE, PR_TRUE);
          aAction = eNone;
          break;
        case eToBeginningOfLine:
          selCont->IntraLineMove(PR_TRUE, PR_FALSE);          // try to move to end
          result = selCont->IntraLineMove(PR_FALSE, PR_TRUE); // select to beginning
          aAction = eNone;
          break;
        case eToEndOfLine:
          result = selCont->IntraLineMove(PR_TRUE, PR_TRUE);
          aAction = eNext;
          break;
        default:       // avoid several compiler warnings
          result = NS_OK;
          break;
    }
    if (NS_FAILED(result))
    {
#ifdef DEBUG
      printf("Selection controller interface didn't work!\n");
#endif
      return result;
    }
  }

  // pre-process
  result = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) return result;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsTextRulesInfo ruleInfo(nsTextEditRules::kDeleteSelection);
  ruleInfo.collapsedAction = aAction;
  result = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(result)) return result;
  if (!cancel && !handled)
  {
    result = DeleteSelectionImpl(aAction);
  }
  if (!cancel)
  {
    // post-process 
    result = mRules->DidDoAction(selection, &ruleInfo, result);
  }

  return result;
}

NS_IMETHODIMP nsHTMLEditor::InsertText(const nsString& aStringToInsert)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel, handled;
  PRInt32 theAction = nsTextEditRules::kInsertText;
  PRInt32 opID = kOpInsertText;
  if (mInIMEMode) 
  {
    theAction = nsTextEditRules::kInsertTextIME;
    opID = kOpInsertIMEText;
  }
  nsAutoPlaceHolderBatch batch(this, nsnull); 
  nsAutoRules beginRulesSniffing(this, opID, nsIEditor::eNext);

  // pre-process
  nsresult result = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) return result;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsAutoString resultString;
  nsTextRulesInfo ruleInfo(theAction);
  ruleInfo.inString = &aStringToInsert;
  ruleInfo.outString = &resultString;
  ruleInfo.maxLength = mMaxTextLength;

  result = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(result)) return result;
  if (!cancel && !handled)
  {
    // we rely on rules code for now - no default implementation
  }
  if (!cancel)
  {
    // post-process 
    result = mRules->DidDoAction(selection, &ruleInfo, result);
  }
  return result;
}

NS_IMETHODIMP nsHTMLEditor::InsertHTML(const nsString& aInputString)
{
  nsAutoString charset;
  return InsertHTMLWithCharset(aInputString, charset);
}

NS_IMETHODIMP nsHTMLEditor::InsertHTMLWithCharset(const nsString& aInputString,
                                                  const nsString& aCharset)
{
  // First, make sure there are no return chars in the document.
  // Bad things happen if you insert returns (instead of dom newlines, \n)
  // into an editor document.
  nsAutoString inputString (aInputString);  // hope this does copy-on-write
 
  // Windows linebreaks: Map CRLF to LF:
  inputString.ReplaceSubstring(NS_LITERAL_STRING("\r\n"),
                               NS_LITERAL_STRING("\n"));
 
  // Mac linebreaks: Map any remaining CR to LF:
  inputString.ReplaceSubstring(NS_LITERAL_STRING("\r"),
                               NS_LITERAL_STRING("\n"));

  ForceCompositionEnd();
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpInsertElement, nsIEditor::eNext);
  
  nsresult res;

  nsCOMPtr<nsIDOMSelection>selection;

  if (!mRules) return NS_ERROR_NOT_INITIALIZED;

  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> parentNode;
  PRInt32 offsetOfNewNode;
  res = DeleteSelectionAndPrepareToCreateNode(parentNode, offsetOfNewNode);
  if (NS_FAILED(res)) return res;

  // pasting does not inherit local inline styles
  RemoveAllInlineProperties();

  res = GetSelection(getter_AddRefs(selection));
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
    // The rules code (WillDoAction above) might have changed the selection.  
    // refresh our memory...
    res = GetStartNodeAndOffset(selection, &parentNode, &offsetOfNewNode);
    if (!parentNode) res = NS_ERROR_FAILURE;
    if (NS_FAILED(res)) return res;
    
    // are we in a text node?  If so, split it.
    if (IsTextNode(parentNode))
    {
      nsCOMPtr<nsIDOMNode> temp;
      res = SplitNode(parentNode, offsetOfNewNode, getter_AddRefs(temp));
      if (NS_FAILED(res)) return res;
      res = GetNodeLocation(parentNode, &temp, &offsetOfNewNode);
      if (NS_FAILED(res)) return res;
      parentNode = temp;
    }
    // Get the first range in the selection, for context:
    nsCOMPtr<nsIDOMRange> range;
    res = selection->GetRangeAt(0, getter_AddRefs(range));
    if (NS_FAILED(res))
      return res;

    nsCOMPtr<nsIDOMNSRange> nsrange (do_QueryInterface(range));
    if (!nsrange)
      return NS_ERROR_NO_INTERFACE;

    nsCOMPtr<nsIDOMDocumentFragment> docfrag;
    res = nsrange->CreateContextualFragment(inputString,
                                            getter_AddRefs(docfrag));
    if (NS_FAILED(res))
    {
#ifdef DEBUG
      printf("Couldn't create contextual fragment: error was %d\n", res);
#endif
      return res;
    }

#if defined(DEBUG_akkana_verbose)
    printf("============ Fragment dump :===========\n");

    nsCOMPtr<nsIContent> fragc (do_QueryInterface(docfrag));
    if (!fragc)
      printf("Couldn't get fragment is nsIContent\n");
    else
      fragc->List(stdout);
#endif

    // Insert the contents of the document fragment:
    nsCOMPtr<nsIDOMNode> fragmentAsNode (do_QueryInterface(docfrag));

    // Loop over the contents of the fragment:
    nsCOMPtr<nsIDOMNode> child;
    res = fragmentAsNode->GetFirstChild(getter_AddRefs(child));
    if (NS_FAILED(res))
    {
      printf("GetFirstChild failed!\n");
      return res;
    }
    while (child)
    {
#if defined(DEBUG_akkana_verbose)
      printf("About to try to insert this node:\n");
      nsCOMPtr<nsIContent> nodec (do_QueryInterface(child));
      if (nodec) nodec->List(stdout);
      printf("-----\n");
#endif
      // Get the next sibling before inserting the node;
      // when we insert the node, it moves into the main doc tree
      // so we'll no longer be able to get the siblings in the doc frag.
      nsCOMPtr<nsIDOMNode> nextSib;
      child->GetNextSibling(getter_AddRefs(nextSib));
      // Ignore the return value, we'll check child when we loop around again.

      // Now we can insert the node.
      res = InsertNode(child, parentNode, offsetOfNewNode++);
      if (NS_FAILED(res))
        break;
      child = nextSib;
    }
    if (NS_FAILED(res))
      return res;

    // Now collapse the selection to the end of what we just inserted:
    selection->Collapse(parentNode, offsetOfNewNode);
  }
  
  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::RebuildDocumentFromSource(const nsString& aSourceString)
{
  // First, make sure there are no return chars in the document.
  // Bad things happen if you insert returns (instead of dom newlines, \n)
  // into an editor document.
  nsAutoString sourceString (aSourceString);  // hope this does copy-on-write
 
  // Windows linebreaks: Map CRLF to LF:
  sourceString.ReplaceSubstring(NS_LITERAL_STRING("\r\n"),
                                NS_LITERAL_STRING("\n"));
 
  // Mac linebreaks: Map any remaining CR to LF:
  sourceString.ReplaceSubstring(NS_LITERAL_STRING("\r"),
                                NS_LITERAL_STRING("\n"));

  ForceCompositionEnd();

  // Get the document we want to replace
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDocument> document;
  nsresult res = ps->GetDocument(getter_AddRefs(document));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIParser>  theParser; 
  res = nsComponentManager::CreateInstance(kCParserCID,  nsnull,  kCParserIID, getter_AddRefs(theParser)); 

  if (NS_FAILED(res)) return res;
  
  // Parse the string to rebuild the document
  // Last 2 params: enableVerify and "this is the last chunk to parse"
  return theParser->Parse(sourceString, (void*)document.get(), NS_ConvertASCIItoUCS2("text/html"), PR_TRUE, PR_TRUE); 
}

NS_IMETHODIMP nsHTMLEditor::InsertBreak()
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsAutoRules beginRulesSniffing(this, kOpInsertBreak, nsIEditor::eNext);
  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel, handled;

  // pre-process
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsTextRulesInfo ruleInfo(nsTextEditRules::kInsertBreak);
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(res)) return res;
  if (!cancel && !handled)
  {
    // create the new BR node
    nsCOMPtr<nsIDOMNode> newNode;
    nsAutoString tag; tag.AssignWithConversion("BR");
    res = DeleteSelectionAndCreateNode(tag, getter_AddRefs(newNode));
    if (!newNode) res = NS_ERROR_NULL_POINTER; // don't return here, so DidDoAction is called
    if (NS_SUCCEEDED(res))
    {
      // set the selection to the new node
      nsCOMPtr<nsIDOMNode>parent;
      res = newNode->GetParentNode(getter_AddRefs(parent));
      if (!parent) res = NS_ERROR_NULL_POINTER; // don't return here, so DidDoAction is called
      if (NS_SUCCEEDED(res))
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
        res = GetSelection(getter_AddRefs(selection));
        if (!selection) res = NS_ERROR_NULL_POINTER; // don't return here, so DidDoAction is called
        if (NS_SUCCEEDED(res))
        {
          if (-1==offsetInParent) 
          {
            nextNode->GetParentNode(getter_AddRefs(parent));
            res = GetChildOffset(nextNode, parent, offsetInParent);
            if (NS_SUCCEEDED(res)) {
              // SetHint(PR_TRUE) means we want the caret to stick to the content on the "right".
              // We want the caret to stick to whatever is past the break.  This is
              // because the break is on the same line we were on, but the next content
              // will be on the following line.
              selection->SetHint(PR_TRUE);
              res = selection->Collapse(parent, offsetInParent+1);  // +1 to insert just after the break
            }
          }
          else
          {
            res = selection->Collapse(nextNode, offsetInParent);
          }
        }
      }
    }
  }
  if (!cancel)
  {
    // post-process, always called if WillInsertBreak didn't return cancel==PR_TRUE
    res = mRules->DidDoAction(selection, &ruleInfo, res);
  }

  return res;
}


NS_IMETHODIMP
nsHTMLEditor::InsertElementAtSelection(nsIDOMElement* aElement, PRBool aDeleteSelection)
{
  nsresult res = NS_ERROR_NOT_INITIALIZED;
  
  if (!aElement)
    return NS_ERROR_NULL_POINTER;
  
  ForceCompositionEnd();
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpInsertElement, nsIEditor::eNext);

  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (!NS_SUCCEEDED(res) || !selection)
    return NS_ERROR_FAILURE;

  // hand off to the rules system, see if it has anything to say about this
  PRBool cancel, handled;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kInsertElement);
  ruleInfo.insertElement = aElement;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;

  if (!handled)
  {
    if (aDeleteSelection)
    {
      nsCOMPtr<nsIDOMNode> tempNode;
      PRInt32 tempOffset;
      nsresult result = DeleteSelectionAndPrepareToCreateNode(tempNode,tempOffset);
      if (!NS_SUCCEEDED(result))
        return result;
    }

    // If deleting, selection will be collapsed.
    // so if not, we collapse it
    if (!aDeleteSelection)
    {
      // Named Anchor is a special case,
      // We collapse to insert element BEFORE the selection
      // For all other tags, we insert AFTER the selection
      nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aElement);
      if (IsNamedAnchorNode(node))
      {
        selection->CollapseToStart();
      } else {
        selection->CollapseToEnd();
      }
    }

    nsCOMPtr<nsIDOMNode> parentSelectedNode;
    PRInt32 offsetForInsert;
    res = selection->GetAnchorNode(getter_AddRefs(parentSelectedNode));
    // XXX: ERROR_HANDLING bad XPCOM usage
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
        res = SplitNodeDeep(topChild, parentSelectedNode, offsetForInsert, &offsetForInsert);
        if (NS_FAILED(res))
          return res;
      }
      // Now we can insert the new node
      res = InsertNode(aElement, parent, offsetForInsert);

      // Set caret after element, but check for special case 
      //  of inserting table-related elements: set in first cell instead
      if (!SetCaretInTableCell(aElement))
        res = SetCaretAfterElement(aElement);

    }
  }
  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

// XXX: error handling in this routine needs to be cleaned up!
NS_IMETHODIMP
nsHTMLEditor::DeleteSelectionAndCreateNode(const nsString& aTag,
                                           nsIDOMNode ** aNewNode)
{
  nsCOMPtr<nsIDOMNode> parentSelectedNode;
  PRInt32 offsetOfNewNode;
  nsresult result = DeleteSelectionAndPrepareToCreateNode(parentSelectedNode,
                                                          offsetOfNewNode);
  if (!NS_SUCCEEDED(result))
    return result;

  nsCOMPtr<nsIDOMNode> newNode;
  result = CreateNode(aTag, parentSelectedNode, offsetOfNewNode,
                      getter_AddRefs(newNode));
  // XXX: ERROR_HANDLING  check result, and make sure aNewNode is set correctly in success/failure cases
  *aNewNode = newNode;
  NS_IF_ADDREF(*aNewNode);

  // we want the selection to be just after the new node
  nsCOMPtr<nsIDOMSelection> selection;
  result = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) return result;
  if (!selection) return NS_ERROR_NULL_POINTER;
  result = selection->Collapse(parentSelectedNode, offsetOfNewNode+1);

  return result;
}

NS_IMETHODIMP
nsHTMLEditor::SelectElement(nsIDOMElement* aElement)
{
  nsresult res = NS_ERROR_NULL_POINTER;

  // Must be sure that element is contained in the document body
  if (IsElementInBody(aElement))
  {
    nsCOMPtr<nsIDOMSelection> selection;
    res = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    if (!selection) return NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIDOMNode>parent;
    res = aElement->GetParentNode(getter_AddRefs(parent));
    if (NS_SUCCEEDED(res) && parent)
    {
      PRInt32 offsetInParent;
      res = GetChildOffset(aElement, parent, offsetInParent);

      if (NS_SUCCEEDED(res))
      {
        // Collapse selection to just before desired element,
        res = selection->Collapse(parent, offsetInParent);
        if (NS_SUCCEEDED(res)) {
          //  then extend it to just after
          res = selection->Extend(parent, offsetInParent+1);
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

  // Be sure the element is contained in the document body
  if (aElement && IsElementInBody(aElement))
  {
    nsCOMPtr<nsIDOMSelection> selection;
    res = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    if (!selection) return NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIDOMNode>parent;
    res = aElement->GetParentNode(getter_AddRefs(parent));
    if (NS_FAILED(res)) return res;
    if (!parent) return NS_ERROR_NULL_POINTER;
    PRInt32 offsetInParent;
    res = GetChildOffset(aElement, parent, offsetInParent);
    if (NS_SUCCEEDED(res))
    {
      // Collapse selection to just after desired element,
      res = selection->Collapse(parent, offsetInParent+1);
#if 0 //def DEBUG_cmanske
      {
      nsAutoString name;
      parent->GetNodeName(name);
      printf("SetCaretAfterElement: Parent node: ");
      wprintf(name.GetUnicode());
      printf(" Offset: %d\n\nHTML:\n", offsetInParent+1);
      nsAutoString Format("text/html");
      nsAutoString ContentsAs;
      OutputToString(ContentsAs, Format, 2);
      wprintf(ContentsAs.GetUnicode());
      }
#endif
    }
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::SetParagraphFormat(const nsString& aParagraphFormat)
{
  nsAutoString tag; tag.Assign(aParagraphFormat);
  tag.ToLowerCase();
  if (tag.EqualsWithConversion("dd") || tag.EqualsWithConversion("dt"))
    return MakeDefinitionItem(tag);
  else
    return InsertBasicBlock(tag);
}

// XXX: ERROR_HANDLING -- this method needs a little work to ensure all error codes are 
//                        checked properly, all null pointers are checked, and no memory leaks occur
NS_IMETHODIMP 
nsHTMLEditor::GetParentBlockTags(nsStringArray *aTagList, PRBool aGetLists)
{
  if (!aTagList) { return NS_ERROR_NULL_POINTER; }

  nsresult res;
  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  // Find out if the selection is collapsed:
  PRBool isCollapsed;
  res = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;
  if (isCollapsed)
  {
    nsCOMPtr<nsIDOMNode> node, blockParent;
    PRInt32 offset;
  
    res = GetStartNodeAndOffset(selection, &node, &offset);
    if (!node) res = NS_ERROR_FAILURE;
    if (NS_FAILED(res)) return res;
  
    nsCOMPtr<nsIDOMElement> blockParentElem;
    if (aGetLists)
    {
      // Get the "ol", "ul", or "dl" parent element
      res = GetElementOrParentByTagName(NS_ConvertASCIItoUCS2("list"), node, getter_AddRefs(blockParentElem));
      if (NS_FAILED(res)) return res;
    } 
    else 
    {
      if (IsBlockNode(node)) blockParent = node;
      else blockParent = GetBlockNodeParent(node);
      blockParentElem = do_QueryInterface(blockParent);
    }
    if (blockParentElem)
    {
      nsAutoString blockParentTag;
      blockParentElem->GetTagName(blockParentTag);
      aTagList->AppendString(blockParentTag);
    }
    
    return res;
  }

  // else non-collapsed selection
  nsCOMPtr<nsIEnumerator> enumerator;
  res = selection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(res)) return res;
  if (!enumerator) return NS_ERROR_NULL_POINTER;

  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  res = enumerator->CurrentItem(getter_AddRefs(currentItem));
  if (NS_FAILED(res)) return res;
  //XXX: should be while loop?
  if (currentItem)
  {
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    // scan the range for all the independent block content blockSections
    // and get the block parent of each
    nsISupportsArray *blockSections;
    res = NS_NewISupportsArray(&blockSections);
    if (NS_FAILED(res)) return res;
    if (!blockSections) return NS_ERROR_NULL_POINTER;
    res = GetBlockSectionsForRange(range, blockSections);
    if (NS_SUCCEEDED(res))
    {
      nsIDOMRange *subRange;
      subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
      while (subRange)
      {
        nsCOMPtr<nsIDOMNode>startParent;
        res = subRange->GetStartParent(getter_AddRefs(startParent));
        if (NS_SUCCEEDED(res) && startParent) 
        {
          nsCOMPtr<nsIDOMElement> blockParent;
          if (aGetLists)
          {
            // Get the "ol", "ul", or "dl" parent element
            res = GetElementOrParentByTagName(NS_ConvertASCIItoUCS2("list"), startParent, getter_AddRefs(blockParent));
          } 
          else 
          {
            blockParent = do_QueryInterface(GetBlockNodeParent(startParent));
          }
          if (NS_SUCCEEDED(res) && blockParent)
          {
            nsAutoString blockParentTag;
            blockParent->GetTagName(blockParentTag);
            PRBool isRoot;
            IsRootTag(blockParentTag, isRoot);
            if ((!isRoot) && (-1==aTagList->IndexOf(blockParentTag))) {
              aTagList->AppendString(blockParentTag);
            }
          }
        }
        NS_RELEASE(subRange);
        if (NS_FAILED(res))
          break;  // don't return here, need to release blockSections
        blockSections->RemoveElementAt(0);
        subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
      }
    }
    NS_RELEASE(blockSections);
  }
  return res;
}


NS_IMETHODIMP 
nsHTMLEditor::GetParagraphState(PRBool &aMixed, nsString &outFormat)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIHTMLEditRules> htmlRules = do_QueryInterface(mRules);
  if (!htmlRules) return NS_ERROR_FAILURE;
  
  return htmlRules->GetParagraphState(aMixed, outFormat);
}

NS_IMETHODIMP 
nsHTMLEditor::GetFontFaceState(PRBool &aMixed, nsString &outFace)
{
  aMixed = PR_TRUE;
  outFace.AssignWithConversion("");
  
  nsresult res;
  nsAutoString faceStr; faceStr.AssignWithConversion("face");
  PRBool first, any, all;
  
  res = GetInlinePropertyWithAttrValue(nsIEditProperty::font, &faceStr, nsnull, first, any, all, &outFace);
  if (NS_FAILED(res)) return res;
  if (any && !all) return res; // mixed
  if (all)
  {
    aMixed = PR_FALSE;
    return res;
  }
  
  res = GetInlineProperty(nsIEditProperty::tt, nsnull, nsnull, first, any, all);
  if (NS_FAILED(res)) return res;
  if (any && !all) return res; // mixed
  if (all)
  {
    aMixed = PR_FALSE;
    nsIEditProperty::tt->ToString(outFace);
  }
  
  if (!any)
  {
    // there was no font face attrs of any kind.  We are in normal font.
    outFace.AssignWithConversion("");
    aMixed = PR_FALSE;
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::GetFontColorState(PRBool &aMixed, nsString &outColor)
{
  aMixed = PR_TRUE;
  outColor.AssignWithConversion("");
  
  nsresult res;
  nsAutoString colorStr; colorStr.AssignWithConversion("color");
  PRBool first, any, all;
  
  res = GetInlinePropertyWithAttrValue(nsIEditProperty::font, &colorStr, nsnull, first, any, all, &outColor);
  if (NS_FAILED(res)) return res;
  if (any && !all) return res; // mixed
  if (all)
  {
    aMixed = PR_FALSE;
    return res;
  }
  
  if (!any)
  {
    // there was no font color attrs of any kind..
    outColor.AssignWithConversion("");
    aMixed = PR_FALSE;
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::GetBackgroundColorState(PRBool &aMixed, nsString &outColor)
{
  //TODO: We don't handle "mixed" correctly!
  aMixed = PR_FALSE;
  outColor.AssignWithConversion("");
  
  nsCOMPtr<nsIDOMElement> element;
  PRInt32 selectedCount;
  nsAutoString tagName;
  nsresult res = GetSelectedOrParentTableElement(*getter_AddRefs(element), tagName, selectedCount);
  if (NS_FAILED(res)) return res;

  // If not table or cell found, get page body
  if (!element)
  {
    res = nsEditor::GetRootElement(getter_AddRefs(element));
    if (NS_FAILED(res)) return res;
  }

  if (!element) return NS_ERROR_NULL_POINTER;

  nsAutoString styleName; styleName.AssignWithConversion("bgcolor");
  return element->GetAttribute(styleName, outColor);
}

NS_IMETHODIMP 
nsHTMLEditor::GetListState(PRBool &aMixed, PRBool &aOL, PRBool &aUL, PRBool &aDL)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIHTMLEditRules> htmlRules = do_QueryInterface(mRules);
  if (!htmlRules) return NS_ERROR_FAILURE;
  
  return htmlRules->GetListState(aMixed, aOL, aUL, aDL);
}

NS_IMETHODIMP 
nsHTMLEditor::GetListItemState(PRBool &aMixed, PRBool &aLI, PRBool &aDT, PRBool &aDD)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIHTMLEditRules> htmlRules = do_QueryInterface(mRules);
  if (!htmlRules) return NS_ERROR_FAILURE;
  
  return htmlRules->GetListItemState(aMixed, aLI, aDT, aDD);
}

NS_IMETHODIMP
nsHTMLEditor::GetAlignment(PRBool &aMixed, nsIHTMLEditor::EAlignment &aAlign)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIHTMLEditRules> htmlRules = do_QueryInterface(mRules);
  if (!htmlRules) return NS_ERROR_FAILURE;
  
  return htmlRules->GetAlignment(aMixed, aAlign);
}


NS_IMETHODIMP 
nsHTMLEditor::GetIndentState(PRBool &aCanIndent, PRBool &aCanOutdent)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIHTMLEditRules> htmlRules = do_QueryInterface(mRules);
  if (!htmlRules) return NS_ERROR_FAILURE;
  
  return htmlRules->GetIndentState(aCanIndent, aCanOutdent);
}

NS_IMETHODIMP
nsHTMLEditor::MakeOrChangeList(const nsString& aListType)
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel, handled;

  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpMakeList, nsIEditor::eNext);
  
  // pre-process
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsTextRulesInfo ruleInfo(nsTextEditRules::kMakeList);
  ruleInfo.blockType = &aListType;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;

  if (!handled)
  {
    // Find out if the selection is collapsed:
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
        res = SplitNodeDeep(topChild, node, offset, &offset);
        if (NS_FAILED(res)) return res;
      }

      // make a list
      nsCOMPtr<nsIDOMNode> newList;
      res = CreateNode(aListType, parent, offset, getter_AddRefs(newList));
      if (NS_FAILED(res)) return res;
      // make a list item
      nsAutoString tag; tag.AssignWithConversion("li");
      nsCOMPtr<nsIDOMNode> newItem;
      res = CreateNode(tag, newList, 0, getter_AddRefs(newItem));
      if (NS_FAILED(res)) return res;
      res = selection->Collapse(newItem,0);
      if (NS_FAILED(res)) return res;
    }
  }
  
  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}


NS_IMETHODIMP
nsHTMLEditor::RemoveList(const nsString& aListType)
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel, handled;

  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpRemoveList, nsIEditor::eNext);
  
  // pre-process
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsTextRulesInfo ruleInfo(nsTextEditRules::kRemoveList);
  if (aListType.EqualsWithConversion("ol")) ruleInfo.bOrdered = PR_TRUE;
  else  ruleInfo.bOrdered = PR_FALSE;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;

  // no default behavior for this yet.  what would it mean?

  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

nsresult
nsHTMLEditor::MakeDefinitionItem(const nsString& aItemType)
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel, handled;

  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpMakeDefListItem, nsIEditor::eNext);
  
  // pre-process
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kMakeDefListItem);
  ruleInfo.blockType = &aItemType;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;

  if (!handled)
  {
    // todo: no default for now.  we count on rules to handle it.
  }

  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

nsresult
nsHTMLEditor::InsertBasicBlock(const nsString& aBlockType)
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel, handled;

  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpMakeBasicBlock, nsIEditor::eNext);
  
  // pre-process
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kMakeBasicBlock);
  ruleInfo.blockType = &aBlockType;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;

  if (!handled)
  {
    // Find out if the selection is collapsed:
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
      // have to find a place to put the block
      nsCOMPtr<nsIDOMNode> parent = node;
      nsCOMPtr<nsIDOMNode> topChild = node;
      nsCOMPtr<nsIDOMNode> tmp;
    
      while ( !CanContainTag(parent, aBlockType))
      {
        parent->GetParentNode(getter_AddRefs(tmp));
        if (!tmp) return NS_ERROR_FAILURE;
        topChild = parent;
        parent = tmp;
      }
    
      if (parent != node)
      {
        // we need to split up to the child of parent
        res = SplitNodeDeep(topChild, node, offset, &offset);
        if (NS_FAILED(res)) return res;
      }

      // make a block
      nsCOMPtr<nsIDOMNode> newBlock;
      res = CreateNode(aBlockType, parent, offset, getter_AddRefs(newBlock));
      if (NS_FAILED(res)) return res;
    
      // reposition selection to inside the block
      res = selection->Collapse(newBlock,0);
      if (NS_FAILED(res)) return res;  
    }
  }

  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::Indent(const nsString& aIndent)
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  PRBool cancel, handled;
  PRInt32 theAction = nsTextEditRules::kIndent;
  PRInt32 opID = kOpIndent;
  if (aIndent.EqualsWithConversion("outdent"))
  {
    theAction = nsTextEditRules::kOutdent;
    opID = kOpOutdent;
  }
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, opID, nsIEditor::eNext);
  
  // pre-process
  nsCOMPtr<nsIDOMSelection> selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsTextRulesInfo ruleInfo(theAction);
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;
  
  if (!handled)
  {
    // Do default - insert a blockquote node if selection collapsed
    nsCOMPtr<nsIDOMNode> node;
    PRInt32 offset;
    PRBool isCollapsed;
    res = selection->GetIsCollapsed(&isCollapsed);
    if (NS_FAILED(res)) return res;

    res = GetStartNodeAndOffset(selection, &node, &offset);
    if (!node) res = NS_ERROR_FAILURE;
    if (NS_FAILED(res)) return res;
  
    nsAutoString inward; inward.AssignWithConversion("indent");
    if (aIndent == inward)
    {
      if (isCollapsed)
      {
        // have to find a place to put the blockquote
        nsCOMPtr<nsIDOMNode> parent = node;
        nsCOMPtr<nsIDOMNode> topChild = node;
        nsCOMPtr<nsIDOMNode> tmp;
        nsAutoString bq; bq.AssignWithConversion("blockquote");
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
          res = SplitNodeDeep(topChild, node, offset, &offset);
          if (NS_FAILED(res)) return res;
        }

        // make a blockquote
        nsCOMPtr<nsIDOMNode> newBQ;
        res = CreateNode(bq, parent, offset, getter_AddRefs(newBQ));
        if (NS_FAILED(res)) return res;
        // put a space in it so layout will draw the list item
        res = selection->Collapse(newBQ,0);
        if (NS_FAILED(res)) return res;
        nsAutoString theText; theText.AssignWithConversion(" ");
        res = InsertText(theText);
        if (NS_FAILED(res)) return res;
        // reposition selection to before the space character
        res = GetStartNodeAndOffset(selection, &node, &offset);
        if (NS_FAILED(res)) return res;
        res = selection->Collapse(node,0);
        if (NS_FAILED(res)) return res;
      }
    }
  }
  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

//TODO: IMPLEMENT ALIGNMENT!

NS_IMETHODIMP
nsHTMLEditor::Align(const nsString& aAlignType)
{
  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpAlign, nsIEditor::eNext);

  nsCOMPtr<nsIDOMNode> node;
  PRBool cancel, handled;
  
  // Find out if the selection is collapsed:
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kAlign);
  ruleInfo.alignType = &aAlignType;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(res))
    return res;
  
  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::GetElementOrParentByTagName(const nsString &aTagName, nsIDOMNode *aNode, nsIDOMElement** aReturn)
{
  if (aTagName.Length() == 0 || !aReturn )
    return NS_ERROR_NULL_POINTER;
  
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> currentNode;

  if (aNode)
    currentNode = aNode;
  else
  {
    // If no node supplied, get it from anchor node of current selection
    nsCOMPtr<nsIDOMSelection>selection;
    res = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    if (!selection) return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIDOMNode> anchorNode;
    res = selection->GetAnchorNode(getter_AddRefs(anchorNode));
    if(NS_FAILED(res)) return res;
    if (!anchorNode)  return NS_ERROR_FAILURE;

    // Try to get the actual selected node
    PRBool hasChildren = PR_FALSE;
    anchorNode->HasChildNodes(&hasChildren);
    if (hasChildren)
    {
      PRInt32 offset;
      res = selection->GetAnchorOffset(&offset);
      if(NS_FAILED(res)) return res;
      currentNode = nsEditor::GetChildAt(anchorNode, offset);
    }
    // anchor node is probably a text node - just use that
    if (!currentNode)
      currentNode = anchorNode;
  }
   
  nsAutoString TagName(aTagName);
  TagName.ToLowerCase();
  PRBool getLink = IsLink(TagName);
  PRBool getNamedAnchor = IsNamedAnchor(TagName);
  if ( getLink || getNamedAnchor)
  {
    TagName.AssignWithConversion("a");  
  }
  PRBool findTableCell = aTagName.EqualsIgnoreCase("td");
  PRBool findList = aTagName.EqualsIgnoreCase("list");

  // default is null - no element found
  *aReturn = nsnull;
  
  nsCOMPtr<nsIDOMNode> parent;
  PRBool bNodeFound = PR_FALSE;

  while (PR_TRUE)
  {
    nsAutoString currentTagName; 
    // Test if we have a link (an anchor with href set)
    if ( (getLink && IsLinkNode(currentNode)) ||
         (getNamedAnchor && IsNamedAnchorNode(currentNode)) )
    {
      bNodeFound = PR_TRUE;
      break;
    } else {
      if (findList)
      {
        // Match "ol", "ul", or "dl" for lists
        if (IsListNode(currentNode))
          goto NODE_FOUND;

      } else if (findTableCell)
      {
        // Table cells are another special case:
        // Match either "td" or "th" for them
        if (IsCellNode(currentNode))
          goto NODE_FOUND;

      } else {
        currentNode->GetNodeName(currentTagName);
        if (currentTagName.EqualsIgnoreCase(TagName))
        {
NODE_FOUND:
          bNodeFound = PR_TRUE;
          break;
        } 
      }
    }
    // Search up the parent chain
    // We should never fail because of root test below, but lets be safe
    // XXX: ERROR_HANDLING error return code lost
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
  else res = NS_EDITOR_ELEMENT_NOT_FOUND;

  return res;
}

NS_IMETHODIMP
nsHTMLEditor::GetSelectedElement(const nsString& aTagName, nsIDOMElement** aReturn)
{
  if (!aReturn )
    return NS_ERROR_NULL_POINTER;
  
  // default is null - no element found
  *aReturn = nsnull;
  
  // First look for a single element in selection
  nsCOMPtr<nsIDOMSelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  PRBool bNodeFound = PR_FALSE;
  res=NS_ERROR_NOT_INITIALIZED;
  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);

  nsAutoString domTagName;
  nsAutoString TagName(aTagName);
  TagName.ToLowerCase();
  // Empty string indicates we should match any element tag
  PRBool anyTag = (TagName.IsEmpty());
  PRBool isLinkTag = IsLink(TagName);
  PRBool isNamedAnchorTag = IsNamedAnchor(TagName);
  
  nsCOMPtr<nsIDOMElement> selectedElement;
  nsCOMPtr<nsIDOMRange> range;
  res = selection->GetRangeAt(0, getter_AddRefs(range));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> startParent;
  PRInt32 startOffset, endOffset;
  res = range->GetStartParent(getter_AddRefs(startParent));
  if (NS_FAILED(res)) return res;
  res = range->GetStartOffset(&startOffset);
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> endParent;
  res = range->GetEndParent(getter_AddRefs(endParent));
  if (NS_FAILED(res)) return res;
  res = range->GetEndOffset(&endOffset);
  if (NS_FAILED(res)) return res;

  // Optimization for a single selected element
  if (startParent && startParent == endParent && (endOffset-startOffset) == 1)
  {
    nsCOMPtr<nsIDOMNode> selectedNode = GetChildAt(startParent, startOffset);
    if (NS_FAILED(res)) return NS_OK;
    if (selectedNode)
    {
      selectedNode->GetNodeName(domTagName);
      domTagName.ToLowerCase();

      // Test for appropriate node type requested
      if (anyTag || (TagName == domTagName) ||
          (isLinkTag && IsLinkNode(selectedNode)) ||
          (isNamedAnchorTag && IsNamedAnchorNode(selectedNode)))
      {
        bNodeFound = PR_TRUE;
        selectedElement = do_QueryInterface(selectedNode);
      }
    }
  }

  if (!bNodeFound)
  {
    if (isLinkTag)
    {
      // Link tag is a special case - we return the anchor node
      //  found for any selection that is totally within a link,
      //  included a collapsed selection (just a caret in a link)
      nsCOMPtr<nsIDOMNode> anchorNode;
      res = selection->GetAnchorNode(getter_AddRefs(anchorNode));
      if (NS_FAILED(res)) return res;
      PRInt32 anchorOffset = -1;
      if (anchorNode)
        selection->GetAnchorOffset(&anchorOffset);
    
      nsCOMPtr<nsIDOMNode> focusNode;
      res = selection->GetFocusNode(getter_AddRefs(focusNode));
      if (NS_FAILED(res)) return res;
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
        res = GetElementOrParentByTagName(NS_ConvertASCIItoUCS2("href"), anchorNode, getter_AddRefs(parentLinkOfAnchor));
        // XXX: ERROR_HANDLING  can parentLinkOfAnchor be null?
        if (NS_SUCCEEDED(res) && parentLinkOfAnchor)
        {
          if (isCollapsed)
          {
            // We have just a caret in the link
            bNodeFound = PR_TRUE;
          } else if(focusNode) 
          {  // Link node must be the same for both ends of selection
            nsCOMPtr<nsIDOMElement> parentLinkOfFocus;
            res = GetElementOrParentByTagName(NS_ConvertASCIItoUCS2("href"), focusNode, getter_AddRefs(parentLinkOfFocus));
            if (NS_SUCCEEDED(res) && parentLinkOfFocus == parentLinkOfAnchor)
              bNodeFound = PR_TRUE;
          }
      
          // We found a link node parent
          if (bNodeFound) {
            // GetElementOrParentByTagName addref'd this, so we don't need to do it here
            *aReturn = parentLinkOfAnchor;
            NS_IF_ADDREF(*aReturn);
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

    if (!isCollapsed)   // Don't bother to examine selection if it is collapsed
    {
      nsCOMPtr<nsIEnumerator> enumerator;
      res = selection->GetEnumerator(getter_AddRefs(enumerator));
      if (NS_SUCCEEDED(res))
      {
        if(!enumerator)
          return NS_ERROR_NULL_POINTER;

        enumerator->First(); 
        nsCOMPtr<nsISupports> currentItem;
        res = enumerator->CurrentItem(getter_AddRefs(currentItem));
        if ((NS_SUCCEEDED(res)) && currentItem)
        {
          nsCOMPtr<nsIDOMRange> currange( do_QueryInterface(currentItem) );
          nsCOMPtr<nsIContentIterator> iter;
          res = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                                      NS_GET_IID(nsIContentIterator), 
                                                      getter_AddRefs(iter));
          if (NS_FAILED(res)) return res;
          if (iter)
          {
            iter->Init(currange);
            // loop through the content iterator for each content node
            nsCOMPtr<nsIContent> content;
            while (NS_ENUMERATOR_FALSE == iter->IsDone())
            {
              res = iter->CurrentNode(getter_AddRefs(content));
              // Note likely!
              if (NS_FAILED(res))
                return NS_ERROR_FAILURE;

               // Query interface to cast nsIContent to nsIDOMNode
               //  then get tagType to compare to  aTagName
               // Clone node of each desired type and append it to the aDomFrag
              selectedElement = do_QueryInterface(content);
              if (selectedElement)
              {
                // If we already found a node, then we have another element,
                //  thus there's not just one element selected
                if (bNodeFound)
                {
                  bNodeFound = PR_FALSE;
                  break;
                }

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
                if ( (isLinkTag && IsLinkNode(selectedNode)) ||
                     (isNamedAnchorTag && IsNamedAnchorNode(selectedNode)) )
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
  }
  if (bNodeFound)
  {
    
    *aReturn = selectedElement;
    if (selectedElement)
    {  
      // Getters must addref
      NS_ADDREF(*aReturn);
    }
  } 
  else res = NS_EDITOR_ELEMENT_NOT_FOUND;

  return res;
}

NS_IMETHODIMP
nsHTMLEditor::CreateElementWithDefaults(const nsString& aTagName, nsIDOMElement** aReturn)
{
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  if (aReturn)
    *aReturn = nsnull;

  if (aTagName.IsEmpty() || !aReturn)
    return NS_ERROR_NULL_POINTER;
    
  nsAutoString TagName(aTagName);
  TagName.ToLowerCase();
  nsAutoString realTagName;

  if (IsLink(TagName) || IsNamedAnchor(TagName))
  {
    realTagName.AssignWithConversion("a");
  } else {
    realTagName = TagName;
  }
  //We don't use editor's CreateElement because we don't want to 
  //  go through the transaction system

  nsCOMPtr<nsIDOMElement>newElement;
  nsCOMPtr<nsIContent> newContent;
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;

  //new call to use instead to get proper HTML element, bug# 39919
  res = CreateHTMLContent(realTagName, getter_AddRefs(newContent));
  newElement = do_QueryInterface(newContent);
  if (NS_FAILED(res) || !newElement)
    return NS_ERROR_FAILURE;

  // Mark the new element dirty, so it will be formatted
  newElement->SetAttribute(NS_ConvertASCIItoUCS2("_moz_dirty"), nsAutoString());

  // Set default values for new elements
  if (TagName.EqualsWithConversion("hr"))
  {
    // Note that we read the user's attributes for these from prefs (in InsertHLine JS)
    newElement->SetAttribute(NS_ConvertASCIItoUCS2("align"),NS_ConvertASCIItoUCS2("center"));
    newElement->SetAttribute(NS_ConvertASCIItoUCS2("width"),NS_ConvertASCIItoUCS2("100%"));
    newElement->SetAttribute(NS_ConvertASCIItoUCS2("size"),NS_ConvertASCIItoUCS2("2"));
  } else if (TagName.EqualsWithConversion("table"))
  {
    newElement->SetAttribute(NS_ConvertASCIItoUCS2("cellpadding"),NS_ConvertASCIItoUCS2("2"));
    newElement->SetAttribute(NS_ConvertASCIItoUCS2("cellspacing"),NS_ConvertASCIItoUCS2("2"));
    newElement->SetAttribute(NS_ConvertASCIItoUCS2("border"),NS_ConvertASCIItoUCS2("1"));
  } else if (TagName.EqualsWithConversion("tr"))
  {
    newElement->SetAttribute(NS_ConvertASCIItoUCS2("valign"),NS_ConvertASCIItoUCS2("top"));
  } else if (TagName.EqualsWithConversion("td"))
  {
    newElement->SetAttribute(NS_ConvertASCIItoUCS2("valign"),NS_ConvertASCIItoUCS2("top"));
  }
  // ADD OTHER TAGS HERE

  if (NS_SUCCEEDED(res))
  {
    *aReturn = newElement;
    // Getters must addref
    NS_ADDREF(*aReturn);
  }

  return res;
}

NS_IMETHODIMP
nsHTMLEditor::InsertLinkAroundSelection(nsIDOMElement* aAnchorElement)
{
  nsresult res=NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMSelection> selection;

  if (!aAnchorElement) return NS_ERROR_NULL_POINTER; 


  // We must have a real selection
  res = GetSelection(getter_AddRefs(selection));
  if (!selection)
  {
    res = NS_ERROR_NULL_POINTER;
  }
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

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
      res = anchor->GetHref(href);
      if (NS_FAILED(res)) return res;
      if (href.GetUnicode() && href.Length() > 0)      
      {
        nsAutoEditBatch beginBatching(this);
        nsString attribute; attribute.AssignWithConversion("href");
        SetInlineProperty(nsIEditProperty::a, &attribute, &href);
        //TODO: Enumerate through other properties of the anchor tag
        // and set those as well. 
        // Optimization: Modify SetTextProperty to set all attributes at once?
      }
    }
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::SetBackgroundColor(const nsString& aColor)
{
  NS_PRECONDITION(mDocWeak, "Missing Editor DOM Document");
  
  // Find a selected or enclosing table element to set background on
  nsCOMPtr<nsIDOMElement> element;
  PRInt32 selectedCount;
  nsAutoString tagName;
  nsresult res = GetSelectedOrParentTableElement(*getter_AddRefs(element), tagName, selectedCount);
  if (NS_FAILED(res)) return res;

  PRBool setColor = (aColor.Length() > 0);

  if (element)
  {
    if (selectedCount > 0)
    {
      // Traverse all selected cells
      nsCOMPtr<nsIDOMElement> cell;
      res = GetFirstSelectedCell(getter_AddRefs(cell), nsnull);
      if (NS_SUCCEEDED(res) && cell)
      {
        while(cell)
        {
          if (setColor)
            res = SetAttribute(cell, NS_ConvertASCIItoUCS2("bgcolor"), aColor);
          else
            res = RemoveAttribute(cell, NS_ConvertASCIItoUCS2("bgcolor"));
          if (NS_FAILED(res)) break;

          GetNextSelectedCell(getter_AddRefs(cell), nsnull);
        };
        return res;
      }
    }
    // If we failed to find a cell, fall through to use originally-found element
  } else {
    // No table element -- set the background color on the body tag
    res = nsEditor::GetRootElement(getter_AddRefs(element));
    if (NS_FAILED(res)) return res;
    if (!element)       return NS_ERROR_NULL_POINTER;
  }
  // Use the editor method that goes through the transaction system
  if (setColor)
    res = SetAttribute(element, NS_ConvertASCIItoUCS2("bgcolor"), aColor);
  else
    res = RemoveAttribute(element, NS_ConvertASCIItoUCS2("bgcolor"));

  return res;
}

NS_IMETHODIMP nsHTMLEditor::SetBodyAttribute(const nsString& aAttribute, const nsString& aValue)
{
  nsresult res;
  // TODO: Check selection for Cell, Row, Column or table and do color on appropriate level

  NS_ASSERTION(mDocWeak, "Missing Editor DOM Document");
  
  // Set the background color attribute on the body tag
  nsCOMPtr<nsIDOMElement> bodyElement;

  res = nsEditor::GetRootElement(getter_AddRefs(bodyElement));
  if (!bodyElement) res = NS_ERROR_NULL_POINTER;
  if (NS_SUCCEEDED(res))
  {
    // Use the editor method that goes through the transaction system
    res = SetAttribute(bodyElement, aAttribute, aValue);
  }
  return res;
}

/*
NS_IMETHODIMP nsHTMLEditor::MoveSelectionUp(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::MoveSelectionDown(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::MoveSelectionNext(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::MoveSelectionPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::SelectNext(nsIAtom *aIncrement, PRBool aExtendSelection) 
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::SelectPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::ScrollUp(nsIAtom *aIncrement)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::ScrollDown(nsIAtom *aIncrement)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::ScrollIntoView(PRBool aScrollToBegin)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
*/


NS_IMETHODIMP
nsHTMLEditor::GetDocumentIsEmpty(PRBool *aDocumentIsEmpty)
{
  if (!aDocumentIsEmpty)
    return NS_ERROR_NULL_POINTER;
  
  if (!mRules)
    return NS_ERROR_NOT_INITIALIZED;
  
  return mRules->DocumentIsEmpty(aDocumentIsEmpty);
}


NS_IMETHODIMP
nsHTMLEditor::GetDocumentLength(PRInt32 *aCount)                                              
{
  if (!aCount) { return NS_ERROR_NULL_POINTER; }
  nsresult result;
  // initialize out params
  *aCount = 0;
  
  // special-case for empty document, to account for the bogus text node
  PRBool docEmpty;
  result = GetDocumentIsEmpty(&docEmpty);
  if (NS_FAILED(result)) return result;
  if (docEmpty)
  {
    *aCount = 0;
    return NS_OK;
  }
  
  // get the body node
  nsCOMPtr<nsIDOMElement> bodyElement;
  result = nsEditor::GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(result)) { return result; }
  if (!bodyElement) { return NS_ERROR_NULL_POINTER; }

  // get the offsets of the first and last children of the body node
  nsCOMPtr<nsIDOMNode>bodyNode = do_QueryInterface(bodyElement);
  if (!bodyNode) { return NS_ERROR_NULL_POINTER; }
  PRInt32 numBodyChildren=0;
  nsCOMPtr<nsIDOMNode>lastChild;
  result = bodyNode->GetLastChild(getter_AddRefs(lastChild));
  if (NS_FAILED(result)) { return result; }
  if (!lastChild) { return NS_ERROR_NULL_POINTER; }
  result = GetChildOffset(lastChild, bodyNode, numBodyChildren);
  if (NS_FAILED(result)) { return result; }

  // count
  PRInt32 start, end;
  result = GetAbsoluteOffsetsForPoints(bodyNode, 0, 
                                       bodyNode, numBodyChildren, 
                                       bodyNode, start, end);
  if (NS_SUCCEEDED(result))
  {
    NS_ASSERTION(0==start, "GetAbsoluteOffsetsForPoints failed to set start correctly.");
    NS_ASSERTION(0<=end, "GetAbsoluteOffsetsForPoints failed to set end correctly.");
    if (0<=end) {
      *aCount = end;
      printf ("count = %d\n", *aCount);
    }
  }
  return result;
}

NS_IMETHODIMP nsHTMLEditor::SetMaxTextLength(PRInt32 aMaxTextLength)
{
  mMaxTextLength = aMaxTextLength;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLEditor::GetMaxTextLength(PRInt32& aMaxTextLength)
{
  aMaxTextLength = mMaxTextLength;
  return NS_OK;
}


#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIEditorStyleSheets methods 
#pragma mark -
#endif


NS_IMETHODIMP
nsHTMLEditor::AddStyleSheet(nsICSSStyleSheet* aSheet)
{
  AddStyleSheetTxn* txn;
  nsresult rv = CreateTxnForAddStyleSheet(aSheet, &txn);
  if (!txn) rv = NS_ERROR_NULL_POINTER;
  if (NS_SUCCEEDED(rv))
  {
    rv = Do(txn);
    if (NS_SUCCEEDED(rv))
    {
      mLastStyleSheet = do_QueryInterface(aSheet);    // save it so we can remove before applying the next one
    }
  }
  // The transaction system (if any) has taken ownwership of txns
  NS_IF_RELEASE(txn);
  
  return rv;
}


NS_IMETHODIMP
nsHTMLEditor::RemoveStyleSheet(nsICSSStyleSheet* aSheet)
{
  RemoveStyleSheetTxn* txn;
  nsresult rv = CreateTxnForRemoveStyleSheet(aSheet, &txn);
  if (!txn) rv = NS_ERROR_NULL_POINTER;
  if (NS_SUCCEEDED(rv))
  {
    rv = Do(txn);
    if (NS_SUCCEEDED(rv))
    {
      mLastStyleSheet = nsnull;        // forget it
    }
  }
  // The transaction system (if any) has taken ownwership of txns
  NS_IF_RELEASE(txn);
  
  return rv;
}

// Do NOT use transaction system for override style sheets
NS_IMETHODIMP
nsHTMLEditor::RemoveOverrideStyleSheet(nsICSSStyleSheet* aSheet)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDocument> document;
  nsresult rv = ps->GetDocument(getter_AddRefs(document));
  if (NS_FAILED(rv)) return rv;
  if (!document)     return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIStyleSet> styleSet;
  rv = ps->GetStyleSet(getter_AddRefs(styleSet));

  if (NS_FAILED(rv)) return rv;
  if (!styleSet) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIStyleSheet> styleSheet = do_QueryInterface(aSheet);
  if (!styleSheet) return NS_ERROR_NULL_POINTER;
  styleSet->RemoveOverrideStyleSheet(styleSheet);

  // This notifies document observers to rebuild all frames
  // (this doesn't affect style sheet because it is not a doc sheet)
  document->SetStyleSheetDisabledState(styleSheet, PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLEditor::ApplyOverrideStyleSheet(const nsString& aURL, nsICSSStyleSheet **aStyleSheet)
{
  return ApplyDocumentOrOverrideStyleSheet(aURL, PR_TRUE, aStyleSheet);
}

NS_IMETHODIMP 
nsHTMLEditor::ApplyStyleSheet(const nsString& aURL, nsICSSStyleSheet **aStyleSheet)
{
  return ApplyDocumentOrOverrideStyleSheet(aURL, PR_FALSE, aStyleSheet);
}

//Note: Loading a document style sheet is undoable, loading an override sheet is not
nsresult 
nsHTMLEditor::ApplyDocumentOrOverrideStyleSheet(const nsString& aURL, PRBool aOverride, nsICSSStyleSheet **aStyleSheet)
{
  nsresult rv   = NS_OK;
  nsCOMPtr<nsIURI> uaURL;

  rv = NS_NewURI(getter_AddRefs(uaURL), aURL);

  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDocument> document;

    if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
    nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
    if (!ps) return NS_ERROR_NOT_INITIALIZED;
    rv = ps->GetDocument(getter_AddRefs(document));
    if (NS_FAILED(rv)) return rv;
    if (!document)     return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIHTMLContentContainer> container = do_QueryInterface(document);
    if (!container) return NS_ERROR_NULL_POINTER;
      
    nsCOMPtr<nsICSSLoader> cssLoader;
    nsCOMPtr<nsICSSStyleSheet> cssStyleSheet;

    rv = container->GetCSSLoader(*getter_AddRefs(cssLoader));
    if (NS_FAILED(rv)) return rv;
    if (!cssLoader)    return NS_ERROR_NULL_POINTER;

    PRBool complete;

    if (aOverride)
    {
      // We use null for the callback and data pointer because
      //  we MUST ONLY load synchronous local files (no @import)
      rv = cssLoader->LoadAgentSheet(uaURL, *getter_AddRefs(cssStyleSheet), complete,
                                     nsnull);

      // Synchronous loads should ALWAYS return completed
      if (!complete || !cssStyleSheet)
        return NS_ERROR_NULL_POINTER;

      nsCOMPtr<nsIStyleSheet> styleSheet;
      styleSheet = do_QueryInterface(cssStyleSheet);
      nsCOMPtr<nsIStyleSet> styleSet;
      rv = ps->GetStyleSet(getter_AddRefs(styleSet));
      if (NS_FAILED(rv)) return rv;
      if (!styleSet)     return NS_ERROR_NULL_POINTER;

      // Add the override style sheet
      // (This checks if already exists)
      styleSet->AppendOverrideStyleSheet(styleSheet);
      // Save doc pointer to be able to use nsIStyleSheet::SetEnabled()
      styleSheet->SetOwningDocument(document);

      // This notifies document observers to rebuild all frames
      // (this doesn't affect style sheet because it is not a doc sheet)
      document->SetStyleSheetDisabledState(styleSheet, PR_FALSE);
    }
    else 
    {
      rv = cssLoader->LoadAgentSheet(uaURL, *getter_AddRefs(cssStyleSheet), complete,
                                     this);
      if (NS_FAILED(rv)) return rv;
      if (complete)
        ApplyStyleSheetToPresShellDocument(cssStyleSheet,this);

      //
      // If not complete, we will be notified later
      // with a call to ApplyStyleSheetToPresShellDocument().
      //
    }
    if (aStyleSheet)
    {
      *aStyleSheet = cssStyleSheet;
      NS_ADDREF(*aStyleSheet);
    }
  }
  return rv;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIEditorMailSupport methods 
#pragma mark -
#endif

NS_IMETHODIMP
nsHTMLEditor::GetBodyStyleContext(nsIStyleContext** aStyleContext)
{
  nsCOMPtr<nsIDOMElement> body;
  nsresult res = GetRootElement(getter_AddRefs(body));
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsIContent> content = do_QueryInterface(body);

  nsIFrame *frame;
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  res = ps->GetPrimaryFrameFor(content, &frame);
  if (NS_FAILED(res)) return res;
  
  return ps->GetStyleContextFor(frame, aStyleContext);
}

//
// Get the wrap width for the first PRE tag in the document.
// If no PRE tag, throw an error.
//
NS_IMETHODIMP nsHTMLEditor::GetBodyWrapWidth(PRInt32 *aWrapColumn)
{
  nsresult res;

  if (! aWrapColumn)
    return NS_ERROR_NULL_POINTER;

  *aWrapColumn = -1;        // default: no wrap

  nsCOMPtr<nsIStyleContext> styleContext;
  res = GetBodyStyleContext(getter_AddRefs(styleContext));
  if (NS_FAILED(res)) return res;

  const nsStyleText* styleText =
    (const nsStyleText*)styleContext->GetStyleData(eStyleStruct_Text);

  if (NS_STYLE_WHITESPACE_PRE == styleText->mWhiteSpace)
    *aWrapColumn = 0;   // wrap to window width
  else if (NS_STYLE_WHITESPACE_MOZ_PRE_WRAP == styleText->mWhiteSpace)
  {
    const nsStylePosition* stylePosition =
      (const nsStylePosition*)styleContext->GetStyleData(eStyleStruct_Position); 
    if (stylePosition->mWidth.GetUnit() == eStyleUnit_Chars)
      *aWrapColumn = stylePosition->mWidth.GetIntValue(); 
    else {
#ifdef DEBUG_akkana
      printf("Can't get wrap column: style unit is %d\n",
             stylePosition->mWidth.GetUnit());
#endif
      *aWrapColumn = -1;
      return NS_ERROR_UNEXPECTED;
    }
  }
  else
    *aWrapColumn = -1;
  return NS_OK;
}

//
// See if the style value includes this attribute, and if it does,
// cut out everything from the attribute to the next semicolon.
//
static void CutStyle(const char* stylename, nsString& styleValue)
{
  // Find the current wrapping type:
  PRInt32 styleStart = styleValue.Find(stylename, PR_TRUE);
  if (styleStart >= 0)
  {
    PRInt32 styleEnd = styleValue.Find(";", PR_FALSE, styleStart);
    if (styleEnd > styleStart)
      styleValue.Cut(styleStart, styleEnd - styleStart + 1);
    else
      styleValue.Cut(styleStart, styleValue.Length() - styleStart);
  }
}

//
// Change the wrap width on the first <PRE> tag in this document.
// (Eventually want to search for more than one in case there are
// interspersed quoted text blocks.)
// Alternately: Change the wrap width on the editor style sheet.
// 
NS_IMETHODIMP nsHTMLEditor::SetBodyWrapWidth(PRInt32 aWrapColumn)
{
  nsresult res;

  // Ought to set a style sheet here ...
  // Probably should keep around an mPlaintextStyleSheet for this purpose.
  nsCOMPtr<nsIDOMElement> bodyElement;
  res = GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;
  if (!bodyElement) return NS_ERROR_NULL_POINTER;

  // Get the current style for this body element:
  nsAutoString styleName; styleName.AssignWithConversion("style");
  nsAutoString styleValue;
  res = bodyElement->GetAttribute(styleName, styleValue);
  if (NS_FAILED(res)) return res;

  // We'll replace styles for these values:
  CutStyle("white-space", styleValue);
  CutStyle("width", styleValue);
  CutStyle("font-family", styleValue);

  // If we have other style left, trim off any existing semicolons
  // or whitespace, then add a known semicolon-space:
  if (styleValue.Length() > 0)
  {
    styleValue.Trim("; \t", PR_FALSE, PR_TRUE);
    styleValue.AppendWithConversion("; ");
  }

  // Make sure we have fixed-width font.  This should be done for us,
  // but it isn't, see bug 22502, so we have to add "font: monospace;".
  // Only do this if we're wrapping.
  if (aWrapColumn >= 0)
    styleValue.AppendWithConversion("font-family: monospace; ");

  // and now we're ready to set the new whitespace/wrapping style.
  if (aWrapColumn > 0)        // Wrap to a fixed column
  {
    styleValue.AppendWithConversion("white-space: -moz-pre-wrap; width: ");
    styleValue.AppendInt(aWrapColumn);
    styleValue.AppendWithConversion("ch;");
  }
  else if (aWrapColumn == 0)
    styleValue.AppendWithConversion("white-space: -moz-pre-wrap;");
  else
    styleValue.AppendWithConversion("white-space: pre;");

  res = bodyElement->SetAttribute(styleName, styleValue);

#ifdef DEBUG_wrapstyle
  char* curstyle = styleValue.ToNewCString();
  printf("Setting style: [%s]\nNow body looks like:\n", curstyle);
  Recycle(curstyle);
  //nsCOMPtr<nsIContent> nodec (do_QueryInterface(bodyElement));
  //if (nodec) nodec->List(stdout);
  //printf("-----\n");
#endif /* DEBUG_akkana */

  return res;
}

NS_IMETHODIMP
nsHTMLEditor::GetEmbeddedObjects(nsISupportsArray** aNodeList)
{
  if (!aNodeList)
    return NS_ERROR_NULL_POINTER;

  nsresult res;

  res = NS_NewISupportsArray(aNodeList);
  if (NS_FAILED(res)) return res;
  if (!*aNodeList) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIContentIterator> iter;
  res = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                           NS_GET_IID(nsIContentIterator), 
                                           getter_AddRefs(iter));
  if (!iter) return NS_ERROR_NULL_POINTER;
  if ((NS_SUCCEEDED(res)))
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
    while (NS_ENUMERATOR_FALSE == iter->IsDone())
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
        if (tagName.EqualsWithConversion("img") || tagName.EqualsWithConversion("embed"))
          (*aNodeList)->AppendElement(node);
        else if (tagName.EqualsWithConversion("a"))
        {
          // XXX Only include links if they're links to file: URLs
          nsCOMPtr<nsIDOMHTMLAnchorElement> anchor (do_QueryInterface(content));
          if (anchor)
          {
            nsAutoString href;
            if (NS_SUCCEEDED(anchor->GetHref(href)))
              if (href.CompareWithConversion("file:", PR_TRUE, 5) == 0)
                (*aNodeList)->AppendElement(node);
          }
        }
      }
      iter->Next();
    }
  }

  return res;
}


#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIEditor overrides 
#pragma mark -
#endif


NS_IMETHODIMP 
nsHTMLEditor::Undo(PRUint32 aCount)
{
  ForceCompositionEnd();
  nsresult result = NS_OK;

  nsAutoRules beginRulesSniffing(this, kOpUndo, nsIEditor::eNone);

  nsTextRulesInfo ruleInfo(nsTextEditRules::kUndo);
  nsCOMPtr<nsIDOMSelection> selection;
  GetSelection(getter_AddRefs(selection));
  PRBool cancel, handled;
  result = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  
  if (!cancel && NS_SUCCEEDED(result))
  {
    result = nsEditor::Undo(aCount);
    result = mRules->DidDoAction(selection, &ruleInfo, result);
  } 
   
  return result;
}


NS_IMETHODIMP 
nsHTMLEditor::Redo(PRUint32 aCount)
{
  nsresult result = NS_OK;

  nsAutoRules beginRulesSniffing(this, kOpRedo, nsIEditor::eNone);

  nsTextRulesInfo ruleInfo(nsTextEditRules::kRedo);
  nsCOMPtr<nsIDOMSelection> selection;
  GetSelection(getter_AddRefs(selection));
  PRBool cancel, handled;
  result = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  
  if (!cancel && NS_SUCCEEDED(result))
  {
    result = nsEditor::Redo(aCount);
    result = mRules->DidDoAction(selection, &ruleInfo, result);
  } 
   
  return result;
}

NS_IMETHODIMP nsHTMLEditor::Cut()
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (!NS_SUCCEEDED(res))
    return res;

  PRBool isCollapsed;
  if (NS_SUCCEEDED(selection->GetIsCollapsed(&isCollapsed)) && isCollapsed)
    return NS_ERROR_NOT_AVAILABLE;

  res = Copy();
  if (NS_SUCCEEDED(res))
    res = DeleteSelection(eNone);
  return res;
}

NS_IMETHODIMP nsHTMLEditor::CanCut(PRBool &aCanCut)
{
  aCanCut = PR_FALSE;
  
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
    
  PRBool isCollapsed;
  res = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;

  aCanCut = !isCollapsed && IsModifiable();
  return NS_OK;
}

NS_IMETHODIMP nsHTMLEditor::Copy()
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  return ps->DoCopy();
}

NS_IMETHODIMP nsHTMLEditor::CanCopy(PRBool &aCanCopy)
{
  aCanCopy = PR_FALSE;
  
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
    
  PRBool isCollapsed;
  res = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;

  aCanCopy = !isCollapsed;
  return NS_OK;
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

NS_IMETHODIMP nsHTMLEditor::InsertFromTransferable(nsITransferable *transferable)
{
  nsresult rv = NS_OK;
  char* bestFlavor = nsnull;
  nsCOMPtr<nsISupports> genericDataObj;
  PRUint32 len = 0;
  if ( NS_SUCCEEDED(transferable->GetAnyTransferData(&bestFlavor, getter_AddRefs(genericDataObj), &len)) )
  {
    nsAutoTxnsConserveSelection dontSpazMySelection(this);
    nsAutoString stuffToPaste;
    nsAutoString flavor;
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
        rv = InsertHTML(stuffToPaste);
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
        rv = InsertText(stuffToPaste);
      }
    }
    else if (flavor.EqualsWithConversion(kFileMime))
    {
      nsCOMPtr<nsIFile> fileObj ( do_QueryInterface(genericDataObj) );
      if (fileObj && len > 0)
      {
        nsCOMPtr<nsIFileURL> fileURL;
        rv = nsComponentManager::CreateInstance("component://netscape/network/standard-url", nsnull, 
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
  NS_WITH_SERVICE(nsIDragService, dragService, "component://netscape/widget/dragservice", &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDragSession> dragSession(do_QueryInterface(dragService));
  
  if (dragSession)
  {
    // Get the nsITransferable interface for getting the data from the drop
    nsCOMPtr<nsITransferable> trans;
    rv = PrepareTransferable(getter_AddRefs(trans));
    if (NS_SUCCEEDED(rv) && trans)
    {
      PRUint32 numItems = 0; 
      if (NS_SUCCEEDED(dragSession->GetNumDropItems(&numItems)))
      {
        PRUint32 i; 
        PRBool doPlaceCaret = PR_TRUE;
        for (i = 0; i < numItems; ++i)
        {
          if (NS_SUCCEEDED(dragSession->GetData(trans, i)))
          {
            if ( doPlaceCaret )
            {
              // check if the user pressed the key to force a copy rather than a move
              // if we run into problems here, we'll just assume the user doesn't want a copy
              PRBool userWantsCopy = PR_FALSE;
              nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aDropEvent) );
              if (mouseEvent)
#ifdef XP_MAC
                mouseEvent->GetAltKey(&userWantsCopy); // check modifiers here
#else
                mouseEvent->GetCtrlKey(&userWantsCopy); // check modifiers here
#endif
              
              nsCOMPtr<nsIDOMDocument> srcdomdoc;
              dragSession->GetSourceDocument(getter_AddRefs(srcdomdoc));
              if (srcdomdoc)
              {
                // do deletion of selection if sourcedocument is current document && appropriate modifier isn't pressed
                nsCOMPtr<nsIDOMDocument>destdomdoc; 
                rv = GetDocument(getter_AddRefs(destdomdoc)); 
                if ( NS_SUCCEEDED(rv) && !userWantsCopy && (srcdomdoc == destdomdoc) )
                {
                  rv = DeleteSelection(eNone);
                  if (NS_FAILED(rv))
                    return rv;
                }
              }

              // Set the selection to the point under the mouse cursor:
              nsCOMPtr<nsIDOMNSUIEvent> nsuiEvent (do_QueryInterface(aDropEvent));

              if (!nsuiEvent)
                return NS_ERROR_NULL_POINTER;
              nsCOMPtr<nsIDOMNode> parent;
              if (!NS_SUCCEEDED(nsuiEvent->GetRangeParent(getter_AddRefs(parent))))
                return NS_ERROR_NULL_POINTER;
              PRInt32 offset = 0;
              if (!NS_SUCCEEDED(nsuiEvent->GetRangeOffset(&offset)))
                return NS_ERROR_NULL_POINTER;

              nsCOMPtr<nsIDOMSelection> selection;
              if (NS_SUCCEEDED(GetSelection(getter_AddRefs(selection))))
                (void)selection->Collapse(parent, offset);

              doPlaceCaret = PR_FALSE;
            }
            
            rv = InsertFromTransferable(trans);
          }
        }
      }
    }
  }

  return rv;
}

NS_IMETHODIMP nsHTMLEditor::CanDrag(nsIDOMEvent *aDragEvent, PRBool &aCanDrag)
{
  /* we really should be checking the XY coordinates of the mouseevent and ensure that
   * that particular point is actually within the selection (not just that there is a selection)
   */
  aCanDrag = PR_FALSE;
  
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
    
  PRBool isCollapsed;
  res = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;
  
  // if we are collapsed, we have no selection so nothing to drag
  if ( isCollapsed )
    return NS_OK;

  nsCOMPtr<nsIDOMEventTarget> eventTarget;
  res = aDragEvent->GetTarget(getter_AddRefs(eventTarget));
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
  nsCOMPtr<nsIDOMSelection> selection;
  rv = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(rv)) return rv;

  /* create an array of transferables */
  nsCOMPtr<nsISupportsArray> transferableArray;
  NS_NewISupportsArray(getter_AddRefs(transferableArray));
  if (transferableArray == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  /* get the drag service */
  NS_WITH_SERVICE(nsIDragService, dragService, "component://netscape/widget/dragservice", &rv);
  if (NS_FAILED(rv)) return rv;

  /* create xif flavor transferable */
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
    nsAutoString buffer;
    rv = doc->CreateXIF(buffer, selection);
    if (NS_FAILED(rv)) return rv;
    if ( !buffer.IsEmpty() )
    {
      nsCOMPtr<nsIFormatConverter> xifConverter;
      rv = nsComponentManager::CreateInstance(kCXIFFormatConverterCID, nsnull, NS_GET_IID(nsIFormatConverter),
                                              getter_AddRefs(xifConverter));
      if (NS_FAILED(rv)) return rv;
      if (!xifConverter) return NS_ERROR_OUT_OF_MEMORY;

      nsCOMPtr<nsISupportsWString> dataWrapper;
      rv = nsComponentManager::CreateInstance(NS_SUPPORTS_WSTRING_PROGID, nsnull,
                                              NS_GET_IID(nsISupportsWString), getter_AddRefs(dataWrapper));
      if (NS_FAILED(rv)) return rv;
      if ( !dataWrapper ) return NS_ERROR_OUT_OF_MEMORY;

      rv = trans->AddDataFlavor(kXIFMime);
      if (NS_FAILED(rv)) return rv;
      rv = trans->SetConverter(xifConverter);
      if (NS_FAILED(rv)) return rv;

      rv = dataWrapper->SetData( NS_CONST_CAST(PRUnichar*, buffer.GetUnicode()) );
      if (NS_FAILED(rv)) return rv;

      // QI the data object an |nsISupports| so that when the transferable holds
      // onto it, it will addref the correct interface.
      nsCOMPtr<nsISupports> nsisupportsDataWrapper ( do_QueryInterface(dataWrapper) );
      rv = trans->SetTransferData(kXIFMime, nsisupportsDataWrapper, buffer.Length() * 2);
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
      rv = InsertFromTransferable(trans);
    }
  }

  return rv;
}


NS_IMETHODIMP nsHTMLEditor::CanPaste(PRInt32 aSelectionType, PRBool &aCanPaste)
{
  aCanPaste = PR_FALSE;
  
  nsresult rv;
  NS_WITH_SERVICE(nsIClipboard, clipboard, kCClipboardCID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  // the flavors that we can deal with
  char* textEditorFlavors[] = { kUnicodeMime, nsnull };
  char* htmlEditorFlavors[] = { kJPEGImageMime, kHTMLMime, nsnull };

  nsCOMPtr<nsISupportsArray> flavorsList;
  rv = nsComponentManager::CreateInstance(NS_SUPPORTSARRAY_PROGID, nsnull, 
         NS_GET_IID(nsISupportsArray), getter_AddRefs(flavorsList));
  if (NS_FAILED(rv)) return rv;
  
  PRUint32 editorFlags;
  GetFlags(&editorFlags);
  
  // add the flavors for all editors
  for (char** flavor = textEditorFlavors; *flavor; flavor++)
  {
    nsCOMPtr<nsISupportsString> flavorString;            
    nsComponentManager::CreateInstance(NS_SUPPORTS_STRING_PROGID, nsnull, 
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
      nsComponentManager::CreateInstance(NS_SUPPORTS_STRING_PROGID, nsnull, 
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
  nsCOMPtr<nsIDOMSelection> selection;
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
  nsCOMPtr<nsIDOMSelection> selection;
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
          preElement->SetAttribute(NS_ConvertASCIItoUCS2("_moz_quote"),
                                   NS_ConvertASCIItoUCS2("true"));
          // set style to not have unwanted vertical margins
          preElement->SetAttribute(NS_ConvertASCIItoUCS2("style"),
                                   NS_ConvertASCIItoUCS2("margin: 0 0 0 0px;"));
        }

        // and set the selection inside it:
        selection->Collapse(preNode, 0);
      }

      rv = InsertText(quotedStuff);

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
    if (NS_SUCCEEDED(GetNodeLocation(preNode, &parent, &offset)) && parent)
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
  nsCOMPtr<nsIDOMSelection> selection;
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
        res = InsertText(aQuotedText);  // XXX ignore charset

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
    if (NS_SUCCEEDED(GetNodeLocation(newNode, &parent, &offset)) && parent)
      selection->Collapse(parent, offset+1);
  }
  return res;
}

NS_IMETHODIMP nsHTMLEditor::OutputToString(nsString& aOutputString,
                                           const nsString& aFormatType,
                                           PRUint32 aFlags)
{
  PRBool cancel, handled;
  nsString resultString;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kOutputText);
  ruleInfo.outString = &resultString;
  ruleInfo.outputFormat = &aFormatType;
  nsresult rv = mRules->WillDoAction(nsnull, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) { return rv; }
  if (handled)
  { // this case will get triggered by password fields
    aOutputString = *(ruleInfo.outString);
  }
  else
  {
    nsCOMPtr<nsIDOMSelection> selection;

    // Set the wrap column.  If our wrap column is 0,
    // i.e. wrap to body width, then don't set it, let the
    // document encoder use its own default.
    PRInt32 wrapColumn;
    PRUint32 wc =0;
    if (NS_SUCCEEDED(GetBodyWrapWidth(&wrapColumn)))
    {
      if (wrapColumn != 0)
      {
        if (wrapColumn < 0)
          wc = 0;
        else
          wc = (PRUint32)wrapColumn;
      }
    }

    nsCOMPtr<nsIDOMElement> rootElement;
    GetRootElement(getter_AddRefs(rootElement));
    NS_ENSURE_TRUE(rootElement, NS_ERROR_FAILURE);
    //is this a body?? do we want to output the whole doc?
    // Set the selection, if appropriate:
    if (aFlags & nsIDocumentEncoder::OutputSelectionOnly)
    {
      rv = GetSelection(getter_AddRefs(selection));
      if (NS_FAILED(rv))
        return rv;
    }
    else if (nsHTMLEditUtils::IsBody(rootElement))
    {
      nsresult rv = NS_OK;

      nsCOMPtr<nsIDocumentEncoder> encoder;
      nsCAutoString formatType(NS_DOC_ENCODER_PROGID_BASE);
      formatType.AppendWithConversion(aFormatType);
      rv = nsComponentManager::CreateInstance(formatType,
                                              nsnull,
                                              NS_GET_IID(nsIDocumentEncoder),
                                              getter_AddRefs(encoder));
      NS_ENSURE_SUCCESS(rv, rv);
      if (!mPresShellWeak) 
        return NS_ERROR_NOT_INITIALIZED;
      nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);

      if (!ps)
        return NS_ERROR_FAILURE;

      nsCOMPtr<nsIDocument> doc;
      rv = ps->GetDocument(getter_AddRefs(doc));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = encoder->Init(doc, aFormatType, aFlags);
      NS_ENSURE_SUCCESS(rv, rv);

      if (wc != 0)
        encoder->SetWrapColumn(wc);

      return encoder->EncodeToString(aOutputString);

    }
    if (!selection)
    {
      nsCOMPtr<nsIDOMRange> range;
      rv = nsComponentManager::CreateInstance(kCRangeCID,
                                 nsnull,
                                 NS_GET_IID(nsIDOMRange),
                                 getter_AddRefs(range));
      if (!range)
        return NS_ERROR_FAILURE;
      rv = nsComponentManager::CreateInstance(kCDOMSelectionCID,
                                 nsnull,
                                 NS_GET_IID(nsIDOMSelection),
                                 getter_AddRefs(selection));
      if (selection)
      {
  //get the independent selection interface
        nsCOMPtr<nsIIndependentSelection> indSel = do_QueryInterface(selection);
        if (indSel)
        {
          nsCOMPtr<nsIPresShell> presShell;
          if (NS_SUCCEEDED(GetPresShell(getter_AddRefs(presShell))) && presShell)
            indSel->SetPresShell(presShell);
        }
        nsCOMPtr<nsIContent> content(do_QueryInterface(rootElement));
        if (content)
        {
          range->SetStart(rootElement,0);
          PRInt32 children;
          if (NS_SUCCEEDED(content->ChildCount(children)))
          {
            range->SetEnd(rootElement,children);
          }
          if (NS_FAILED(selection->AddRange(range)))
            return NS_ERROR_FAILURE;
        }
      }
    }
    return selection->ToString(aFormatType, aFlags, wc, aOutputString);
  }
#if 0

  PRBool cancel, handled;
  nsString resultString;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kOutputText);
  ruleInfo.outString = &resultString;
  ruleInfo.outputFormat = &aFormatType;
  nsresult rv = mRules->WillDoAction(nsnull, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) { return rv; }
  if (handled)
  { // this case will get triggered by password fields
    aOutputString = *(ruleInfo.outString);
  }
  else
  { // default processing
    rv = NS_OK;
    
    // special-case for empty document when requesting plain text,
    // to account for the bogus text node
    if (aFormatType.EqualsWithConversion("text/plain"))
    {
      PRBool docEmpty;
      rv = GetDocumentIsEmpty(&docEmpty);
      if (NS_FAILED(rv)) return rv;
      
      if (docEmpty) {
        aOutputString.SetLength(0);
        return NS_OK;
      }
      else if (mFlags & eEditorPlaintextMask)
        aFlags |= nsIDocumentEncoder::OutputPreformatted;
    }


    nsCOMPtr<nsIDocumentEncoder> encoder;
    char* progid = (char *)nsMemory::Alloc(strlen(NS_DOC_ENCODER_PROGID_BASE) + aFormatType.Length() + 1);
    if (! progid)
      return NS_ERROR_OUT_OF_MEMORY;
    strcpy(progid, NS_DOC_ENCODER_PROGID_BASE);
    char* type = aFormatType.ToNewCString();
    strcat(progid, type);
    nsCRT::free(type);
    rv = nsComponentManager::CreateInstance(progid,
                                            nsnull,
                                            NS_GET_IID(nsIDocumentEncoder),
                                            getter_AddRefs(encoder));

    nsCRT::free(progid);
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIDOMDocument> domdoc;
    rv = GetDocument(getter_AddRefs(domdoc));
    if (NS_FAILED(rv))
      return rv;
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);

    rv = encoder->Init(doc, aFormatType, aFlags);
    if (NS_FAILED(rv))
      return rv;

    // Set the selection, if appropriate:
    if (aFlags & nsIDocumentEncoder::OutputSelectionOnly)
    {
      nsCOMPtr<nsIDOMSelection> selection;
      rv = GetSelection(getter_AddRefs(selection));
      if (NS_SUCCEEDED(rv) && selection)
        encoder->SetSelection(selection);
    }
    
    // Set the wrap column.  If our wrap column is 0,
    // i.e. wrap to body width, then don't set it, let the
    // document encoder use its own default.
    PRInt32 wrapColumn;
    if (NS_SUCCEEDED(GetBodyWrapWidth(&wrapColumn)))
    {
      if (wrapColumn != 0)
      {
        PRUint32 wc;
        if (wrapColumn < 0)
          wc = 0;
        else
          wc = (PRUint32)wrapColumn;
        if (wrapColumn > 0)
          (void)encoder->SetWrapColumn(wc);
      }
    }

    rv = encoder->EncodeToString(aOutputString);
  }
#endif
  return rv;
}

NS_IMETHODIMP nsHTMLEditor::OutputToStream(nsIOutputStream* aOutputStream,
                                           const nsString& aFormatType,
                                           const nsString* aCharset,
                                           PRUint32 aFlags)
{

  nsresult rv;

  // special-case for empty document when requesting plain text,
  // to account for the bogus text node
  if (aFormatType.EqualsWithConversion("text/plain"))
  {
    PRBool docEmpty;
    rv = GetDocumentIsEmpty(&docEmpty);
    if (NS_FAILED(rv)) return rv;
    
    if (docEmpty)
       return NS_OK;    // output nothing
  }

  nsCOMPtr<nsIDocumentEncoder> encoder;
  char* progid = (char *)nsMemory::Alloc(strlen(NS_DOC_ENCODER_PROGID_BASE) + aFormatType.Length() + 1);
  if (! progid)
      return NS_ERROR_OUT_OF_MEMORY;

  strcpy(progid, NS_DOC_ENCODER_PROGID_BASE);
  char* type = aFormatType.ToNewCString();
  strcat(progid, type);
  nsCRT::free(type);
  rv = nsComponentManager::CreateInstance(progid,
                                          nsnull,
                                          NS_GET_IID(nsIDocumentEncoder),
                                          getter_AddRefs(encoder));

  nsCRT::free(progid);
  if (NS_FAILED(rv))
  {
    printf("Couldn't get progid %s\n", progid);
    return rv;
  }

  nsCOMPtr<nsIDOMDocument> domdoc;
  rv = GetDocument(getter_AddRefs(domdoc));
  if (NS_FAILED(rv))
    return rv;
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);

  if (aCharset && aCharset->Length() != 0 && aCharset->EqualsWithConversion("null")==PR_FALSE)
    encoder->SetCharset(*aCharset);

    rv = encoder->Init(doc, aFormatType, aFlags);
    if (NS_FAILED(rv))
      return rv;

  // Set the selection, if appropriate:
  if (aFlags & nsIDocumentEncoder::OutputSelectionOnly)
  {
    nsCOMPtr<nsIDOMSelection> selection;
    rv = GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(rv) && selection)
      encoder->SetSelection(selection);
  }
    
  // Set the wrap column.  If our wrap column is 0,
  // i.e. wrap to body width, then don't set it, let the
  // document encoder use its own default.
  PRInt32 wrapColumn;
  if (NS_SUCCEEDED(GetBodyWrapWidth(&wrapColumn)))
  {
    if (wrapColumn != 0)
    {
      PRUint32 wc;
      if (wrapColumn < 0)
        wc = 0;
      else
        wc = (PRUint32)wrapColumn;
      if (wrapColumn > 0)
        (void)encoder->SetWrapColumn(wc);
    }
  }

  return encoder->EncodeToStream(aOutputStream);
}

static nsresult SetSelectionAroundHeadChildren(nsCOMPtr<nsIDOMSelection> aSelection, nsWeakPtr aDocWeak)
{
  nsresult res = NS_OK;
  // Set selection around <head> node
  nsCOMPtr<nsIDOMNodeList>nodeList; 
  nsAutoString headTag; headTag.AssignWithConversion("head"); 

  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(aDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;
  res = doc->GetElementsByTagName(headTag, getter_AddRefs(nodeList));
  if (NS_FAILED(res)) return res;
  if (!nodeList) return NS_ERROR_NULL_POINTER;

  PRUint32 count; 
  nodeList->GetLength(&count);
  if (count < 1) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> headNode;
  res = nodeList->Item(0, getter_AddRefs(headNode)); 
  if (NS_FAILED(res)) return res;
  if (!headNode) return NS_ERROR_NULL_POINTER;

  // Collapse selection to before first child of the head,
  res = aSelection->Collapse(headNode, 0);
  if (NS_FAILED(res)) return res;

  //  then extend it to just after
  nsCOMPtr<nsIDOMNodeList> childNodes;
  res = headNode->GetChildNodes(getter_AddRefs(childNodes));
  if (NS_FAILED(res)) return res;
  if (!childNodes) return NS_ERROR_NULL_POINTER;
  PRUint32 childCount;
  childNodes->GetLength(&childCount);

  return aSelection->Extend(headNode, childCount+1);
}

NS_IMETHODIMP
nsHTMLEditor::GetHeadContentsAsHTML(nsString& aOutputString)
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  // Save current selection
  nsAutoSelectionReset selectionResetter(selection, this);

  res = SetSelectionAroundHeadChildren(selection, mDocWeak);
  if (NS_FAILED(res)) return res;

  res = OutputToString(aOutputString, NS_ConvertASCIItoUCS2("text/html"),
                       nsIDocumentEncoder::OutputSelectionOnly);
  if (NS_SUCCEEDED(res))
  {
    // Selection always includes <body></body>,
    //  so terminate there
    PRInt32 offset = aOutputString.Find(NS_LITERAL_STRING("<body"), PR_TRUE);
    if (offset > 0)
    {
      // Ensure the string ends in a newline
      PRUnichar newline ('\n');
      if (aOutputString.CharAt(offset-1) != newline)
        aOutputString.SetCharAt(newline, offset++);

      aOutputString.Truncate(offset);
    }
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::ReplaceHeadContentsWithHTML(const nsString &aSourceToInsert)
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  // Save current selection
  nsAutoSelectionReset selectionResetter(selection, this);
  
  res = SetSelectionAroundHeadChildren(selection, mDocWeak);
  if (NS_FAILED(res)) return res;

  return InsertHTML(aSourceToInsert);
}


NS_IMETHODIMP
nsHTMLEditor::DebugUnitTests(PRInt32 *outNumTests, PRInt32 *outNumTestsFailed)
{
#ifdef DEBUG
  if (!outNumTests || !outNumTestsFailed)
    return NS_ERROR_NULL_POINTER;

  TextEditorTest *tester = new TextEditorTest();
  if (!tester)
    return NS_ERROR_OUT_OF_MEMORY;
   
  tester->Run(this, outNumTests, outNumTestsFailed);
  delete tester;
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIEditorIMESupport overrides 
#pragma mark -
#endif

NS_IMETHODIMP
nsHTMLEditor::SetCompositionString(const nsString& aCompositionString, nsIPrivateTextRangeList* aTextRangeList,nsTextEventReply* aReply)
{
  NS_ASSERTION(aTextRangeList, "null ptr");
  if(nsnull == aTextRangeList)
        return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsICaret>  caretP;
  
  // workaround for windows ime bug 23558: we get every ime event twice. 
  // for escape keypress, this causes an empty string to be passed
  // twice, which freaks out the editor.  This is to detect and aviod that
  // situation:
  if (aCompositionString.IsEmpty() && !mIMETextNode) 
  {
    return NS_OK;
  }
  
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult result = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) return result;

  mIMETextRangeList = aTextRangeList;
  nsAutoPlaceHolderBatch batch(this, gIMETxnName);

  result = InsertText(aCompositionString);

  mIMEBufferLength = aCompositionString.Length();

  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  ps->GetCaret(getter_AddRefs(caretP));
  caretP->GetWindowRelativeCoordinates(aReply->mCursorPosition,aReply->mCursorIsCollapsed,selection);

  // second part of 23558 fix:
  if (aCompositionString.IsEmpty()) 
  {
    mIMETextNode = nsnull;
  }
  
  return result;
}

NS_IMETHODIMP 
nsHTMLEditor::GetReconversionString(nsReconversionEventReply* aReply)
{
  nsresult res;

  nsCOMPtr<nsIDOMSelection> selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res) || !selection)
    return (res == NS_OK) ? NS_ERROR_FAILURE : res;

  // get the first range in the selection.  Since it is
  // unclear what to do if reconversion happens with a 
  // multirange selection, we will ignore any additional ranges.
  
  nsCOMPtr<nsIDOMRange> range;
  res = selection->GetRangeAt(0, getter_AddRefs(range));
  if (NS_FAILED(res) || !range)
    return (res == NS_OK) ? NS_ERROR_FAILURE : res;
  
  nsAutoString textValue;
  res = range->ToString(textValue);
  if (NS_FAILED(res))
    return res;
  
  aReply->mReconversionString = (PRUnichar*) nsMemory::Clone(textValue.GetUnicode(),
                                                                (textValue.Length() + 1) * sizeof(PRUnichar));
  if (!aReply->mReconversionString)
    return NS_ERROR_OUT_OF_MEMORY;

  // delete the selection
  res = DeleteSelection(eNone);
  
  return res;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  StyleSheet utils 
#pragma mark -
#endif


NS_IMETHODIMP
nsHTMLEditor::ReplaceStyleSheet(nsICSSStyleSheet *aNewSheet)
{
  nsresult  rv = NS_OK;
  
  nsAutoEditBatch batchIt(this);

  if (mLastStyleSheet)
  {
    rv = RemoveStyleSheet(mLastStyleSheet);
    //XXX: rv is ignored here, why?
  }

  rv = AddStyleSheet(aNewSheet);
  
  return rv;
}

NS_IMETHODIMP 
nsHTMLEditor::StyleSheetLoaded(nsICSSStyleSheet*aSheet, PRBool aNotify)
{
  ApplyStyleSheetToPresShellDocument(aSheet, this);
  return NS_OK;
}

/* static callback */
void nsHTMLEditor::ApplyStyleSheetToPresShellDocument(nsICSSStyleSheet* aSheet, void *aData)
{
  nsresult rv = NS_OK;

  nsHTMLEditor *editor = NS_STATIC_CAST(nsHTMLEditor*, aData);
  if (editor)
  {
    rv = editor->ReplaceStyleSheet(aSheet);
  }
  // XXX: we lose the return value here. Set a flag in the editor?
}


#ifdef XP_MAC
#pragma mark -
#pragma mark  nsEditor overrides 
#pragma mark -
#endif


/** All editor operations which alter the doc should be prefaced
 *  with a call to StartOperation, naming the action and direction */
NS_IMETHODIMP
nsHTMLEditor::StartOperation(PRInt32 opID, nsIEditor::EDirection aDirection)
{
  nsEditor::StartOperation(opID, aDirection);  // will set mAction, mDirection
  if (! ((mAction==kOpInsertText) || (mAction==kOpInsertIMEText)) )
    ClearInlineStylesCache();
  if (mRules) return mRules->BeforeEdit(mAction, mDirection);
  return NS_OK;
}


/** All editor operations which alter the doc should be followed
 *  with a call to EndOperation */
NS_IMETHODIMP
nsHTMLEditor::EndOperation()
{
  // post processing
  if (! ((mAction==kOpInsertText) || (mAction==kOpInsertIMEText) || (mAction==kOpIgnore)) )
    ClearInlineStylesCache();
  nsresult res = NS_OK;
  if (mRules) res = mRules->AfterEdit(mAction, mDirection);
  nsEditor::EndOperation();  // will clear mAction, mDirection
  return res;
}  


PRBool 
nsHTMLEditor::TagCanContainTag(const nsString &aParentTag, const nsString &aChildTag)
{
  // CNavDTD gives some unwanted results.  We override them here.

  if ( aParentTag.EqualsWithConversion("ol") ||
       aParentTag.EqualsWithConversion("ul") )
  {
    // if parent is a list and tag is text, say "no". 
    if (aChildTag.EqualsWithConversion("__moz_text"))
      return PR_FALSE;
    // if both are lists, return true
    if (aChildTag.EqualsWithConversion("ol") ||
        aChildTag.EqualsWithConversion("ul") ) 
      return PR_TRUE;
  }
  
  // if parent is a pre, and child is not inline, say "no"
  if ( aParentTag.EqualsWithConversion("pre") )
  {
    if (aChildTag.EqualsWithConversion("__moz_text"))
      return PR_TRUE;
    PRInt32 childTagEnum, parentTagEnum;
    nsAutoString non_const_childTag(aChildTag);
    nsAutoString non_const_parentTag(aParentTag);
    nsresult res = mDTD->StringTagToIntTag(non_const_childTag,&childTagEnum);
    if (NS_FAILED(res)) return PR_FALSE;
    res = mDTD->StringTagToIntTag(non_const_parentTag,&parentTagEnum);
    if (NS_FAILED(res)) return PR_FALSE;
    if (!mDTD->IsInlineElement(childTagEnum,parentTagEnum))
      return PR_FALSE;
  }
  // else fall thru
  return nsEditor::TagCanContainTag(aParentTag, aChildTag);
}


NS_IMETHODIMP 
nsHTMLEditor::SelectEntireDocument(nsIDOMSelection *aSelection)
{
  nsresult res;
  if (!aSelection || !mRules) { return NS_ERROR_NULL_POINTER; }
  
  // get body node
  nsCOMPtr<nsIDOMElement>bodyElement;
  res = GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsIDOMNode>bodyNode = do_QueryInterface(bodyElement);
  if (!bodyNode) return NS_ERROR_FAILURE;
  
  // is doc empty?
  PRBool bDocIsEmpty;
  res = mRules->DocumentIsEmpty(&bDocIsEmpty);
  if (NS_FAILED(res)) return res;
    
  if (bDocIsEmpty)
  {
    // if its empty dont select entire doc - that would select the bogus node
    return aSelection->Collapse(bodyNode, 0);
  }
  else
  {
    return nsEditor::SelectEntireDocument(aSelection);
  }
  return res;
}



#ifdef XP_MAC
#pragma mark -
#pragma mark  Random methods 
#pragma mark -
#endif


NS_IMETHODIMP nsHTMLEditor::GetLayoutObject(nsIDOMNode *aNode, nsISupports **aLayoutObject)
{
  nsresult result = NS_ERROR_FAILURE;  // we return an error unless we get the index
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;

  if ((nsnull!=aNode))
  { // get the content interface
    nsCOMPtr<nsIContent> nodeAsContent( do_QueryInterface(aNode) );
    if (nodeAsContent)
    { // get the frame from the content interface
      //Note: frames are not ref counted, so don't use an nsCOMPtr
      *aLayoutObject = nsnull;
      result = ps->GetLayoutObjectFor(nodeAsContent, aLayoutObject);
    }
  }
  else {
    result = NS_ERROR_NULL_POINTER;
  }

  return result;
}


// this will NOT find aAttribute unless aAttribute has a non-null value
// so singleton attributes like <Table border> will not be matched!
void nsHTMLEditor::IsTextPropertySetByContent(nsIDOMNode     *aNode,
                                              nsIAtom        *aProperty, 
                                              const nsString *aAttribute, 
                                              const nsString *aValue, 
                                              PRBool         &aIsSet,
                                              nsIDOMNode    **aStyleNode,
                                              nsString       *outValue) const
{
  nsresult result;
  aIsSet = PR_FALSE;  // must be initialized to false for code below to work
  nsAutoString propName;
  aProperty->ToString(propName);
  nsCOMPtr<nsIDOMNode>node = aNode;

  while (node)
  {
    nsCOMPtr<nsIDOMElement>element;
    element = do_QueryInterface(node);
    if (element)
    {
      nsAutoString tag, value;
      element->GetTagName(tag);
      if (propName.EqualsIgnoreCase(tag))
      {
        PRBool found = PR_FALSE;
        if (aAttribute && 0!=aAttribute->Length())
        {
          element->GetAttribute(*aAttribute, value);
          if (outValue) *outValue = value;
          if (value.Length())
          {
            if (!aValue) {
              found = PR_TRUE;
            }
            else if (aValue->EqualsIgnoreCase(value)) {
              found = PR_TRUE;
            }
            else {  // we found the prop with the attribute, but the value doesn't match
              break;
            }
          }
        }
        else { 
          found = PR_TRUE;
        }
        if (found)
        {
          aIsSet = PR_TRUE;
          break;
        }
      }
    }
    nsCOMPtr<nsIDOMNode>temp;
    result = node->GetParentNode(getter_AddRefs(temp));
    if (NS_SUCCEEDED(result) && temp) {
      node = do_QueryInterface(temp);
    }
    else {
      node = do_QueryInterface(nsnull);
    }
  }
}

void nsHTMLEditor::IsTextStyleSet(nsIStyleContext *aSC, 
                                  nsIAtom *aProperty, 
                                  const nsString *aAttribute,  
                                  PRBool &aIsSet) const
{
  aIsSet = PR_FALSE;
  if (aSC && aProperty)
  {
    nsStyleFont* font = (nsStyleFont*)aSC->GetStyleData(eStyleStruct_Font);
    if (nsIEditProperty::i==aProperty)
    {
      aIsSet = PRBool(font->mFont.style & NS_FONT_STYLE_ITALIC);
    }
    else if (nsIEditProperty::b==aProperty)
    { // XXX: check this logic with Peter
      aIsSet = PRBool(font->mFont.weight > NS_FONT_WEIGHT_NORMAL);
    }
  }
}


#ifdef XP_MAC
#pragma mark -
#endif

//================================================================
// HTML Editor methods
//
// Note: Table Editing methods are implemented in nsTableEditor.cpp
//


PRBool nsHTMLEditor::IsElementInBody(nsIDOMElement* aElement)
{
  return nsHTMLEditUtils::InBody(aElement, this);
}

PRBool
nsHTMLEditor::SetCaretInTableCell(nsIDOMElement* aElement)
{
  PRBool caretIsSet = PR_FALSE;

  if (aElement && IsElementInBody(aElement))
  {
    nsresult res = NS_OK;
    nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
    if (content)
    {
      nsCOMPtr<nsIAtom> atom;
      content->GetTag(*getter_AddRefs(atom));
      if (atom.get() == nsIEditProperty::table ||
          atom.get() == nsIEditProperty::tbody ||
          atom.get() == nsIEditProperty::thead ||
          atom.get() == nsIEditProperty::tfoot ||
          atom.get() == nsIEditProperty::caption ||
          atom.get() == nsIEditProperty::tr ||
          atom.get() == nsIEditProperty::td )
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
        // Set selection at beginning of deepest node
        nsCOMPtr<nsIDOMSelection> selection;
        res = GetSelection(getter_AddRefs(selection));
        if (NS_SUCCEEDED(res) && selection && firstChild)
        {
          res = selection->Collapse(firstChild, 0);
          if (NS_SUCCEEDED(res))
            caretIsSet = PR_TRUE;
        }
      }
    }
  }
  return caretIsSet;
}            



NS_IMETHODIMP
nsHTMLEditor::IsRootTag(nsString &aTag, PRBool &aIsTag)
{
  static char bodyTag[] = "body";
  static char tdTag[] = "td";
  static char thTag[] = "th";
  static char captionTag[] = "caption";
  if (aTag.EqualsIgnoreCase(bodyTag) ||
      aTag.EqualsIgnoreCase(tdTag) ||
      aTag.EqualsIgnoreCase(thTag) ||
      aTag.EqualsIgnoreCase(captionTag) )
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
  static char p[] = "p";
  static char h1[] = "h1";
  static char h2[] = "h2";
  static char h3[] = "h3";
  static char h4[] = "h4";
  static char h5[] = "h5";
  static char h6[] = "h6";
  static char address[] = "address";
  static char pre[] = "pre";
  static char li[] = "li";
  static char dt[] = "dt";
  static char dd[] = "dd";
  if (aTag.EqualsIgnoreCase(p)  ||
      aTag.EqualsIgnoreCase(h1) ||
      aTag.EqualsIgnoreCase(h2) ||
      aTag.EqualsIgnoreCase(h3) ||
      aTag.EqualsIgnoreCase(h4) ||
      aTag.EqualsIgnoreCase(h5) ||
      aTag.EqualsIgnoreCase(h6) ||
      aTag.EqualsIgnoreCase(address) ||
      aTag.EqualsIgnoreCase(pre) ||
      aTag.EqualsIgnoreCase(li) ||
      aTag.EqualsIgnoreCase(dt) ||
      aTag.EqualsIgnoreCase(dd) )
  {
    aIsTag = PR_TRUE;
  }
  else {
    aIsTag = PR_FALSE;
  }
  return NS_OK;
}



///////////////////////////////////////////////////////////////////////////
// GetEnclosingTable: find ancestor who is a table, if any
//                  
nsCOMPtr<nsIDOMNode> 
nsHTMLEditor::GetEnclosingTable(nsIDOMNode *aNode)
{
  NS_PRECONDITION(aNode, "null node passed to nsHTMLEditor::GetEnclosingTable");
  nsCOMPtr<nsIDOMNode> tbl, tmp, node = aNode;

  while (!tbl)
  {
    tmp = GetBlockNodeParent(node);
    if (!tmp) break;
    if (nsHTMLEditUtils::IsTable(tmp)) tbl = tmp;
    node = tmp;
  }
  return tbl;
}

NS_IMETHODIMP
nsHTMLEditor::DeleteSelectionAndPrepareToCreateNode(nsCOMPtr<nsIDOMNode> &parentSelectedNode, PRInt32& offsetOfNewNode)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMSelection> selection;
  result = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) return result;
  if (!selection) return NS_ERROR_NULL_POINTER;

  PRBool collapsed;
  result = selection->GetIsCollapsed(&collapsed);
  if (NS_SUCCEEDED(result) && !collapsed) 
  {
    result = DeleteSelection(nsIEditor::eNone);
    if (NS_FAILED(result)) {
      return result;
    }
    // get the new selection
    result = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(result)) {
      return result;
    }
#ifdef NS_DEBUG
    nsCOMPtr<nsIDOMNode>testSelectedNode;
    nsresult debugResult = selection->GetAnchorNode(getter_AddRefs(testSelectedNode));
    // no selection is ok.
    // if there is a selection, it must be collapsed
    if (testSelectedNode)
    {
      PRBool testCollapsed;
      debugResult = selection->GetIsCollapsed(&testCollapsed);
      NS_ASSERTION((NS_SUCCEEDED(result)), "couldn't get a selection after deletion");
      NS_ASSERTION(testCollapsed, "selection not reset after deletion");
    }
#endif
  }
  // split the selected node
  PRInt32 offsetOfSelectedNode;
  result = selection->GetAnchorNode(getter_AddRefs(parentSelectedNode));
  if (NS_SUCCEEDED(result) && NS_SUCCEEDED(selection->GetAnchorOffset(&offsetOfSelectedNode)) && parentSelectedNode)
  {
    nsCOMPtr<nsIDOMNode> selectedNode;
    PRUint32 selectedNodeContentCount=0;
    nsCOMPtr<nsIDOMCharacterData>selectedParentNodeAsText;
    selectedParentNodeAsText = do_QueryInterface(parentSelectedNode);

    offsetOfNewNode = offsetOfSelectedNode;
    
    /* if the selection is a text node, split the text node if necesary
       and compute where to put the new node
    */
    if (selectedParentNodeAsText) 
    { 
      PRInt32 indexOfTextNodeInParent;
      selectedNode = do_QueryInterface(parentSelectedNode);
      selectedNode->GetParentNode(getter_AddRefs(parentSelectedNode));
      selectedParentNodeAsText->GetLength(&selectedNodeContentCount);
      GetChildOffset(selectedNode, parentSelectedNode, indexOfTextNodeInParent);

      if ((offsetOfSelectedNode!=0) && (((PRUint32)offsetOfSelectedNode)!=selectedNodeContentCount))
      {
        nsCOMPtr<nsIDOMNode> newSiblingNode;
        result = SplitNode(selectedNode, offsetOfSelectedNode, getter_AddRefs(newSiblingNode));
        // now get the node's offset in it's parent, and insert the new tag there
        if (NS_SUCCEEDED(result)) {
          result = GetChildOffset(selectedNode, parentSelectedNode, offsetOfNewNode);
        }
      }
      else 
      { // determine where to insert the new node
        if (0==offsetOfSelectedNode) {
          offsetOfNewNode = indexOfTextNodeInParent; // insert new node as previous sibling to selection parent
        }
        else {                 // insert new node as last child
          GetChildOffset(selectedNode, parentSelectedNode, offsetOfNewNode);
          offsetOfNewNode++;    // offsets are 0-based, and we need the index of the new node
        }
      }
    }
    
  // I dont know what is up with this, but there is no reason to split 
  // any node we happen to be inserting into.  The code below (ifdef'd out) 
  // breaks InsertBreak().
  
  
#if 0  

    /* if the selection is not a text node, split the parent node if necesary
       and compute where to put the new node
    */
    else
    { // it's an interior node
      nsCOMPtr<nsIDOMNodeList>parentChildList;
      parentSelectedNode->GetChildNodes(getter_AddRefs(parentChildList));
      if ((NS_SUCCEEDED(result)) && parentChildList)
      {
        result = parentChildList->Item(offsetOfSelectedNode, getter_AddRefs(selectedNode));
        if ((NS_SUCCEEDED(result)) && selectedNode)
        {
          nsCOMPtr<nsIDOMCharacterData>selectedNodeAsText;
          selectedNodeAsText = do_QueryInterface(selectedNode);
          nsCOMPtr<nsIDOMNodeList>childList;
          //CM: I added "result ="
          result = selectedNode->GetChildNodes(getter_AddRefs(childList));
          if (NS_SUCCEEDED(result)) 
          {
            if (childList)
            {
              childList->GetLength(&selectedNodeContentCount);
            } 
            else 
            {
              // This is the case where the collapsed selection offset
              //   points to an inline node with no children
              //   This must also be where the new node should be inserted
              //   and there is no splitting necessary
              offsetOfNewNode = offsetOfSelectedNode;
              return NS_OK;
            }
          }
          else 
          {
            return NS_ERROR_FAILURE;
          }
          if ((offsetOfSelectedNode!=0) && (((PRUint32)offsetOfSelectedNode)!=selectedNodeContentCount))
          {
            nsCOMPtr<nsIDOMNode> newSiblingNode;
            result = SplitNode(selectedNode, offsetOfSelectedNode, getter_AddRefs(newSiblingNode));
            // now get the node's offset in it's parent, and insert the new tag there
            if (NS_SUCCEEDED(result)) {
              result = GetChildOffset(selectedNode, parentSelectedNode, offsetOfNewNode);
            }
          }
          else 
          { // determine where to insert the new node
            if (0==offsetOfSelectedNode) {
              offsetOfNewNode = 0; // insert new node as first child
            }
            else {                 // insert new node as last child
              GetChildOffset(selectedNode, parentSelectedNode, offsetOfNewNode);
              offsetOfNewNode++;    // offsets are 0-based, and we need the index of the new node
            }
          }
        }
      }
    }
#endif

    // Here's where the new node was inserted
  }
  else {
    printf("InsertBreak into an empty document is not yet supported\n");
  }
  return result;
}


#ifdef XP_MAC
#pragma mark -
#endif

void nsHTMLEditor::CacheInlineStyles(nsIDOMNode *aNode)
{
  if (!aNode) return;
  nsCOMPtr<nsIDOMNode> resultNode;
  mCachedNode = do_QueryInterface(aNode);
  IsTextPropertySetByContent(aNode, mBoldAtom, 0, 0, mCachedBoldStyle, getter_AddRefs(resultNode));
  IsTextPropertySetByContent(aNode, mItalicAtom, 0, 0, mCachedItalicStyle, getter_AddRefs(resultNode));
  IsTextPropertySetByContent(aNode, mUnderlineAtom, 0, 0, mCachedUnderlineStyle, getter_AddRefs(resultNode));
  
}

void nsHTMLEditor::ClearInlineStylesCache()
{
  mCachedNode = nsnull;
}

#ifdef PRE_NODE_IN_BODY
nsCOMPtr<nsIDOMElement> nsHTMLEditor::FindPreElement()
{
  nsCOMPtr<nsIDOMDocument> domdoc;
  nsEditor::GetDocument(getter_AddRefs(domdoc));
  if (!domdoc)
    return 0;

  nsCOMPtr<nsIDocument> doc (do_QueryInterface(domdoc));
  if (!doc)
    return 0;

  nsIContent* rootContent = doc->GetRootContent();
  if (!rootContent)
    return 0;

  nsCOMPtr<nsIDOMNode> rootNode (do_QueryInterface(rootContent));
  if (!rootNode)
    return 0;

  nsString prestr ("PRE");  // GetFirstNodeOfType requires capitals
  nsCOMPtr<nsIDOMNode> preNode;
  if (!NS_SUCCEEDED(nsEditor::GetFirstNodeOfType(rootNode, prestr,
                                                 getter_AddRefs(preNode))))
    return 0;

  return do_QueryInterface(preNode);
}
#endif /* PRE_NODE_IN_BODY */

void nsHTMLEditor::HandleEventListenerError()
{
  if (gNoisy) { printf("failed to add event listener\n"); }
  // null out the nsCOMPtrs
  mKeyListenerP = nsnull;
  mMouseListenerP = nsnull;
  mTextListenerP = nsnull;
  mDragListenerP = nsnull;
  mCompositionListenerP = nsnull;
  mFocusListenerP = nsnull;
}

/* this method scans the selection for adjacent text nodes
 * and collapses them into a single text node.
 * "adjacent" means literally adjacent siblings of the same parent.
 * Uses nsEditor::JoinNodes so action is undoable. 
 * Should be called within the context of a batch transaction.
 */
NS_IMETHODIMP
nsHTMLEditor::CollapseAdjacentTextNodes(nsIDOMRange *aInRange)
{
  if (!aInRange) return NS_ERROR_NULL_POINTER;
  nsAutoTxnsConserveSelection dontSpazMySelection(this);
  nsVoidArray textNodes;  // we can't actually do anything during iteration, so store the text nodes in an array
                          // don't bother ref counting them because we know we can hold them for the 
                          // lifetime of this method


  // build a list of editable text nodes
  nsCOMPtr<nsIContentIterator> iter;
  nsresult result = nsComponentManager::CreateInstance(kSubtreeIteratorCID, nsnull,
                                              NS_GET_IID(nsIContentIterator), 
                                              getter_AddRefs(iter));
  if (NS_FAILED(result)) return result;
  if (!iter) return NS_ERROR_NULL_POINTER;

  iter->Init(aInRange);
  nsCOMPtr<nsIContent> content;
  result = iter->CurrentNode(getter_AddRefs(content));  
  if (!content) return NS_OK;
  
  while (NS_ENUMERATOR_FALSE == iter->IsDone())
  {
    nsCOMPtr<nsIDOMCharacterData> text = do_QueryInterface(content);
    nsCOMPtr<nsIDOMNode>          node = do_QueryInterface(content);
    if (text && node && IsEditable(node))
    {
      textNodes.AppendElement((void*)(node.get()));
    }
    iter->Next();
    iter->CurrentNode(getter_AddRefs(content));
  }

  // now that I have a list of text nodes, collapse adjacent text nodes
  // NOTE: assumption that JoinNodes keeps the righthand node
  nsIDOMNode *leftTextNode = (nsIDOMNode *)(textNodes.ElementAt(0));
  nsIDOMNode *rightTextNode = (nsIDOMNode *)(textNodes.ElementAt(1));

  while (leftTextNode && rightTextNode)
  {
    // get the prev sibling of the right node, and see if it's leftTextNode
    nsCOMPtr<nsIDOMNode> prevSibOfRightNode;
    result = GetPriorHTMLSibling(rightTextNode, &prevSibOfRightNode);
    if (NS_FAILED(result)) return result;
    if (prevSibOfRightNode && (prevSibOfRightNode.get() == leftTextNode))
    {
      nsCOMPtr<nsIDOMNode> parent;
      result = rightTextNode->GetParentNode(getter_AddRefs(parent));
      if (NS_FAILED(result)) return result;
      if (!parent) return NS_ERROR_NULL_POINTER;
      result = JoinNodes(leftTextNode, rightTextNode, parent);
      if (NS_FAILED(result)) return result;
    }

    textNodes.RemoveElementAt(0); // remove the leftmost text node from the list
    leftTextNode = (nsIDOMNode *)(textNodes.ElementAt(0));
    rightTextNode = (nsIDOMNode *)(textNodes.ElementAt(1));
  }

  return result;
}

NS_IMETHODIMP
nsHTMLEditor::GetNextElementByTagName(nsIDOMElement    *aCurrentElement,
                                      const nsString   *aTagName,
                                      nsIDOMElement   **aReturn)
{
  nsresult res = NS_OK;
  if (!aCurrentElement || !aTagName || !aReturn)
    return NS_ERROR_NULL_POINTER;

  nsIAtom *tagAtom = NS_NewAtom(*aTagName);
  if (!tagAtom) { return NS_ERROR_NULL_POINTER; }
  if (tagAtom==nsIEditProperty::th)
    tagAtom=nsIEditProperty::td;

  nsCOMPtr<nsIDOMNode> currentNode = do_QueryInterface(aCurrentElement);
  if (!currentNode)
    return NS_ERROR_FAILURE;

  *aReturn = nsnull;

  nsCOMPtr<nsIDOMNode> nextNode;
  PRBool done = PR_FALSE;

  do {
    res = GetNextNode(currentNode, PR_TRUE, getter_AddRefs(nextNode));
    if (NS_FAILED(res)) return res;
    if (!nextNode) break;

    nsCOMPtr<nsIAtom> atom = GetTag(currentNode);

    if (tagAtom==atom.get())
    {
      nsCOMPtr<nsIDOMElement> element = do_QueryInterface(currentNode);
      if (!element) return NS_ERROR_NULL_POINTER;

      *aReturn = element;
      NS_ADDREF(*aReturn);
      done = PR_TRUE;
      return NS_OK;
    }
    currentNode = nextNode;
  } while (!done);

  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::SetSelectionAtDocumentStart(nsIDOMSelection *aSelection)
{
  nsCOMPtr<nsIDOMElement> bodyElement;
  nsresult res = GetRootElement(getter_AddRefs(bodyElement));  
  if (NS_SUCCEEDED(res))
  {
  	if (!bodyElement) return NS_ERROR_NULL_POINTER;
    res = aSelection->Collapse(bodyElement,0);
  }
  return res;
}

#ifdef XP_MAC
#pragma mark -
#endif

nsresult
nsHTMLEditor::RelativeFontChange( PRInt32 aSizeChange)
{
  // Can only change font size by + or - 1
  if ( !( (aSizeChange==1) || (aSizeChange==-1) ) )
    return NS_ERROR_ILLEGAL_VALUE;
  
  ForceCompositionEnd();

  // Get the selection 
  nsCOMPtr<nsIDOMSelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_FAILURE;
  
  // Is the selection collapsed?
  PRBool bCollapsed;
  res = selection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  
  // if it's collapsed set typing state
  if (bCollapsed)
  {
    nsCOMPtr<nsIAtom> atom;
    if (aSizeChange==1) atom = nsIEditProperty::big;
    else                atom = nsIEditProperty::small;
    // manipulating text attributes on a collapsed selection only sets state for the next text insertion
    return mTypeInState->SetProp(atom, nsAutoString(), nsAutoString());
  }
  
  // wrap with txn batching, rules sniffing, and selection preservation code
  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, kOpSetTextProperty, nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);

  // get selection range enumerator
  nsCOMPtr<nsIEnumerator> enumerator;
  res = selection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(res)) return res;
  if (!enumerator)    return NS_ERROR_FAILURE;

  // loop thru the ranges in the selection
  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  while ((NS_ENUMERATOR_FALSE == enumerator->IsDone()))
  {
    res = enumerator->CurrentItem(getter_AddRefs(currentItem));
    if (NS_FAILED(res)) return res;
    if (!currentItem)   return NS_ERROR_FAILURE;
    
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );

    // adjust range to include any ancestors who's children are entirely selected
    res = PromoteInlineRange(range);
    if (NS_FAILED(res)) return res;
    
    // check for easy case: both range endpoints in same text node
    nsCOMPtr<nsIDOMNode> startNode, endNode;
    res = range->GetStartParent(getter_AddRefs(startNode));
    if (NS_FAILED(res)) return res;
    res = range->GetEndParent(getter_AddRefs(endNode));
    if (NS_FAILED(res)) return res;
    if ((startNode == endNode) && IsTextNode(startNode))
    {
      // MOOSE: workaround for selection bug:
      //selection->ClearSelection();

      PRInt32 startOffset, endOffset;
      range->GetStartOffset(&startOffset);
      range->GetEndOffset(&endOffset);
      nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
      res = RelativeFontChangeOnTextNode(aSizeChange, nodeAsText, startOffset, endOffset);
      if (NS_FAILED(res)) return res;
    }
    else
    {
      // not the easy case.  range not contained in single text node. 
      // there are up to three phases here.  There are all the nodes
      // reported by the subtree iterator to be processed.  And there
      // are potentially a starting textnode and an ending textnode
      // which are only partially contained by the range.
      
      // lets handle the nodes reported by the iterator.  These nodes
      // are entirely contained in the selection range.  We build up
      // a list of them (since doing operations on the document during
      // iteration would perturb the iterator).

      nsCOMPtr<nsIContentIterator> iter;
      res = nsComponentManager::CreateInstance(kSubtreeIteratorCID, nsnull,
                                                NS_GET_IID(nsIContentIterator), 
                                                getter_AddRefs(iter));
      if (NS_FAILED(res)) return res;
      if (!iter)          return NS_ERROR_FAILURE;

      nsCOMPtr<nsISupportsArray> arrayOfNodes;
      nsCOMPtr<nsIContent> content;
      nsCOMPtr<nsIDOMNode> node;
      nsCOMPtr<nsISupports> isupports;
      
      // make a array
      res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
      if (NS_FAILED(res)) return res;
      
      // iterate range and build up array
      res = iter->Init(range);
      if (NS_SUCCEEDED(res))
      {
        while (NS_ENUMERATOR_FALSE == iter->IsDone())
        {
          res = iter->CurrentNode(getter_AddRefs(content));
          if (NS_FAILED(res)) return res;
          node = do_QueryInterface(content);
          if (!node) return NS_ERROR_FAILURE;
          if (IsEditable(node))
          { 
            isupports = do_QueryInterface(node);
            arrayOfNodes->AppendElement(isupports);
          }
          iter->Next();
        }
        
        // MOOSE: workaround for selection bug:
        //selection->ClearSelection();

        // now that we have the list, do the font size change on each node
        PRUint32 listCount;
        PRUint32 j;
        arrayOfNodes->Count(&listCount);
        for (j = 0; j < listCount; j++)
        {
          isupports = (dont_AddRef)(arrayOfNodes->ElementAt(0));
          node = do_QueryInterface(isupports);
          res = RelativeFontChangeOnNode(aSizeChange, node);
          if (NS_FAILED(res)) return res;
          arrayOfNodes->RemoveElementAt(0);
        }
      }
      // now check the start and end parents of the range to see if they need to 
      // be seperately handled (they do if they are text nodes, due to how the
      // subtree iterator works - it will not have reported them).
      if (IsTextNode(startNode) && IsEditable(startNode))
      {
        nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
        PRInt32 startOffset;
        PRUint32 textLen;
        range->GetStartOffset(&startOffset);
        nodeAsText->GetLength(&textLen);
        res = RelativeFontChangeOnTextNode(aSizeChange, nodeAsText, startOffset, textLen);
        if (NS_FAILED(res)) return res;
      }
      if (IsTextNode(endNode) && IsEditable(endNode))
      {
        nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(endNode);
        PRInt32 endOffset;
        range->GetEndOffset(&endOffset);
        res = RelativeFontChangeOnTextNode(aSizeChange, nodeAsText, 0, endOffset);
        if (NS_FAILED(res)) return res;
      }
    }
    enumerator->Next();
  }
  
  return res;  
}

nsresult
nsHTMLEditor::RelativeFontChangeOnTextNode( PRInt32 aSizeChange, 
                                            nsIDOMCharacterData *aTextNode, 
                                            PRInt32 aStartOffset,
                                            PRInt32 aEndOffset)
{
  // Can only change font size by + or - 1
  if ( !( (aSizeChange==1) || (aSizeChange==-1) ) )
    return NS_ERROR_ILLEGAL_VALUE;
  if (!aTextNode) return NS_ERROR_NULL_POINTER;
  
  // dont need to do anything if no characters actually selected
  if (aStartOffset == aEndOffset) return NS_OK;
  
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> tmp, node = do_QueryInterface(aTextNode);
  
  // do we need to split the text node?
  PRUint32 textLen;
  aTextNode->GetLength(&textLen);
  
  // -1 is a magic value meaning to the end of node
  if (aEndOffset == -1) aEndOffset = textLen;
  
  if ( (PRUint32)aEndOffset != textLen )
  {
    // we need to split off back of text node
    res = SplitNode(node, aEndOffset, getter_AddRefs(tmp));
    if (NS_FAILED(res)) return res;
    node = tmp;  // remember left node
  }
  if ( aStartOffset )
  {
    // we need to split off front of text node
    res = SplitNode(node, aStartOffset, getter_AddRefs(tmp));
    if (NS_FAILED(res)) return res;
  }
  
  // reparent the node inside font node with appropriate relative size
  res = InsertContainerAbove(node, &tmp, NS_ConvertASCIItoUCS2(aSizeChange==1 ? "big" : "small"));
  return res;
}


nsresult
nsHTMLEditor::RelativeFontChangeOnNode( PRInt32 aSizeChange, 
                                        nsIDOMNode *aNode)
{
  // Can only change font size by + or - 1
  if ( !( (aSizeChange==1) || (aSizeChange==-1) ) )
    return NS_ERROR_ILLEGAL_VALUE;
  if (!aNode) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> tmp;
  nsAutoString tag;
  if (aSizeChange == 1) tag.AssignWithConversion("big");
  else tag.AssignWithConversion("small");
  
  // is this node a text node?
  if (IsTextNode(aNode))
  {
    res = InsertContainerAbove(aNode, &tmp, tag);
    return res;
  }
  // is it the opposite of what we want?  
  if ( ((aSizeChange == 1) && nsHTMLEditUtils::IsSmall(aNode)) || 
       ((aSizeChange == -1) &&  nsHTMLEditUtils::IsBig(aNode)) )
  {
    // in that case, just remove this node and pull up the children
    res = RemoveContainer(aNode);
    return res;
  }
  // can it be put inside a "big" or "small"?
  if (TagCanContain(tag, aNode))
  {
    // ok, chuck it in.
    res = InsertContainerAbove(aNode, &tmp, tag);
    return res;
  }
  // none of the above?  then cycle through the children.
  // MOOSE: we should group the children together if possible
  // into a single "big" or "small".  For the moment they are
  // each getting their own.  
  nsCOMPtr<nsIDOMNodeList> childNodes;
  res = aNode->GetChildNodes(getter_AddRefs(childNodes));
  if (NS_FAILED(res)) return res;
  if (childNodes)
  {
    PRInt32 j;
    PRUint32 childCount;
    childNodes->GetLength(&childCount);
    for (j=0 ; j < (PRInt32)childCount; j++)
    {
      nsCOMPtr<nsIDOMNode> childNode;
      res = childNodes->Item(j, getter_AddRefs(childNode));
      if ((NS_SUCCEEDED(res)) && (childNode))
      {
        res = RelativeFontChangeOnNode(aSizeChange, childNode);
        if (NS_FAILED(res)) return res;
      }
    }
  }
  return res;
}

///////////////////////////////////////////////////////////////////////////
// GetPriorHTMLSibling: returns the previous editable sibling, if there is
//                   one within the parent
//                       
nsresult
nsHTMLEditor::GetPriorHTMLSibling(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode)
{
  if (!outNode || !inNode) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  *outNode = nsnull;
  nsCOMPtr<nsIDOMNode> temp, node = do_QueryInterface(inNode);
  
  while (1)
  {
    res = node->GetPreviousSibling(getter_AddRefs(temp));
    if (NS_FAILED(res)) return res;
    if (!temp) return NS_OK;  // return null sibling
    // if it's editable, we're done
    if (IsEditable(temp)) break;
    // otherwise try again
    node = temp;
  }
  *outNode = temp;
  return res;
}



///////////////////////////////////////////////////////////////////////////
// GetPriorHTMLSibling: returns the previous editable sibling, if there is
//                   one within the parent.  just like above routine but
//                   takes a parent/offset instead of a node.
//                       
nsresult
nsHTMLEditor::GetPriorHTMLSibling(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode)
{
  if (!outNode || !inParent) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  *outNode = nsnull;
  if (!inOffset) return NS_OK;  // return null sibling if at offset zero
  nsCOMPtr<nsIDOMNode> node = nsEditor::GetChildAt(inParent,inOffset-1);
  if (IsEditable(node)) 
  {
    *outNode = node;
    return res;
  }
  // else
  return GetPriorHTMLSibling(node, outNode);
}



///////////////////////////////////////////////////////////////////////////
// GetNextHTMLSibling: returns the next editable sibling, if there is
//                   one within the parent
//                       
nsresult
nsHTMLEditor::GetNextHTMLSibling(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode)
{
  if (!outNode) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  *outNode = nsnull;
  nsCOMPtr<nsIDOMNode> temp, node = do_QueryInterface(inNode);
  
  while (1)
  {
    res = node->GetNextSibling(getter_AddRefs(temp));
    if (NS_FAILED(res)) return res;
    if (!temp) return NS_ERROR_FAILURE;
    // if it's editable, we're done
    if (IsEditable(temp)) break;
    // otherwise try again
    node = temp;
  }
  *outNode = temp;
  return res;
}



///////////////////////////////////////////////////////////////////////////
// GetNextHTMLSibling: returns the next editable sibling, if there is
//                   one within the parent.  just like above routine but
//                   takes a parent/offset instead of a node.
//                       
nsresult
nsHTMLEditor::GetNextHTMLSibling(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode)
{
  if (!outNode || !inParent) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  *outNode = nsnull;
  nsCOMPtr<nsIDOMNode> node = nsEditor::GetChildAt(inParent,inOffset);
  if (!node) return NS_OK; // return null sibling if no sibling
  if (IsEditable(node)) 
  {
    *outNode = node;
    return res;
  }
  // else
  return GetPriorHTMLSibling(node, outNode);
}



///////////////////////////////////////////////////////////////////////////
// GetPriorHTMLNode: returns the previous editable leaf node, if there is
//                   one within the <body>
//                       
nsresult
nsHTMLEditor::GetPriorHTMLNode(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode)
{
  if (!outNode) return NS_ERROR_NULL_POINTER;
  nsresult res = GetPriorNode(inNode, PR_TRUE, getter_AddRefs(*outNode));
  if (NS_FAILED(res)) return res;
  
  // if it's not in the body, then zero it out
  if (*outNode && !nsHTMLEditUtils::InBody(*outNode, this))
  {
    *outNode = nsnull;
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// GetPriorHTMLNode: same as above but takes {parent,offset} instead of node
//                       
nsresult
nsHTMLEditor::GetPriorHTMLNode(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode)
{
  if (!outNode) return NS_ERROR_NULL_POINTER;
  nsresult res = GetPriorNode(inParent, inOffset, PR_TRUE, getter_AddRefs(*outNode));
  if (NS_FAILED(res)) return res;
  
  // if it's not in the body, then zero it out
  if (*outNode && !nsHTMLEditUtils::InBody(*outNode, this))
  {
    *outNode = nsnull;
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// GetNextHTMLNode: returns the previous editable leaf node, if there is
//                   one within the <body>
//                       
nsresult
nsHTMLEditor::GetNextHTMLNode(nsIDOMNode *inNode, nsCOMPtr<nsIDOMNode> *outNode)
{
  if (!outNode) return NS_ERROR_NULL_POINTER;
  nsresult res = GetNextNode(inNode, PR_TRUE, getter_AddRefs(*outNode));
  if (NS_FAILED(res)) return res;
  
  // if it's not in the body, then zero it out
  if (*outNode && !nsHTMLEditUtils::InBody(*outNode, this))
  {
    *outNode = nsnull;
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// GetNHTMLextNode: same as above but takes {parent,offset} instead of node
//                       
nsresult
nsHTMLEditor::GetNextHTMLNode(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outNode)
{
  if (!outNode) return NS_ERROR_NULL_POINTER;
  nsresult res = GetNextNode(inParent, inOffset, PR_TRUE, getter_AddRefs(*outNode));
  if (NS_FAILED(res)) return res;
  
  // if it's not in the body, then zero it out
  if (*outNode && !nsHTMLEditUtils::InBody(*outNode, this))
  {
    *outNode = nsnull;
  }
  return res;
}


nsresult 
nsHTMLEditor::IsFirstEditableChild( nsIDOMNode *aNode, PRBool *aOutIsFirst)
{
  // check parms
  if (!aOutIsFirst || !aNode) return NS_ERROR_NULL_POINTER;
  
  // init out parms
  *aOutIsFirst = PR_FALSE;
  
  // find first editable child and compare it to aNode
  nsCOMPtr<nsIDOMNode> parent, firstChild;
  nsresult res = aNode->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(res)) return res;
  if (!parent) return NS_ERROR_FAILURE;
  res = GetFirstEditableChild(parent, &firstChild);
  if (NS_FAILED(res)) return res;
  
  *aOutIsFirst = (firstChild.get() == aNode);
  return res;
}


nsresult 
nsHTMLEditor::IsLastEditableChild( nsIDOMNode *aNode, PRBool *aOutIsLast)
{
  // check parms
  if (!aOutIsLast || !aNode) return NS_ERROR_NULL_POINTER;
  
  // init out parms
  *aOutIsLast = PR_FALSE;
  
  // find last editable child and compare it to aNode
  nsCOMPtr<nsIDOMNode> parent, lastChild;
  nsresult res = aNode->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(res)) return res;
  if (!parent) return NS_ERROR_FAILURE;
  res = GetLastEditableChild(parent, &lastChild);
  if (NS_FAILED(res)) return res;
  
  *aOutIsLast = (lastChild.get() == aNode);
  return res;
}


nsresult 
nsHTMLEditor::GetFirstEditableChild( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutFirstChild)
{
  // check parms
  if (!aOutFirstChild || !aNode) return NS_ERROR_NULL_POINTER;
  
  // init out parms
  *aOutFirstChild = nsnull;
  
  // find first editable child
  nsCOMPtr<nsIDOMNode> child;
  nsresult res = aNode->GetFirstChild(getter_AddRefs(child));
  if (NS_FAILED(res)) return res;
  
  while (child && !IsEditable(child))
  {
    nsCOMPtr<nsIDOMNode> tmp;
    res = child->GetNextSibling(getter_AddRefs(tmp));
    if (NS_FAILED(res)) return res;
    if (!tmp) return NS_ERROR_FAILURE;
    child = tmp;
  }
  
  *aOutFirstChild = child;
  return res;
}


nsresult 
nsHTMLEditor::GetLastEditableChild( nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutLastChild)
{
  // check parms
  if (!aOutLastChild || !aNode) return NS_ERROR_NULL_POINTER;
  
  // init out parms
  *aOutLastChild = nsnull;
  
  // find last editable child
  nsCOMPtr<nsIDOMNode> child;
  nsresult res = aNode->GetLastChild(getter_AddRefs(child));
  if (NS_FAILED(res)) return res;
  
  while (child && !IsEditable(child))
  {
    nsCOMPtr<nsIDOMNode> tmp;
    res = child->GetPreviousSibling(getter_AddRefs(tmp));
    if (NS_FAILED(res)) return res;
    if (!tmp) return NS_ERROR_FAILURE;
    child = tmp;
  }
  
  *aOutLastChild = child;
  return res;
}

///////////////////////////////////////////////////////////////////////////
// IsEmptyNode: figure out if aNode is an empty node.
//               A block can have children and still be considered empty,
//               if the children are empty or non-editable.
//                  
nsresult
nsHTMLEditor::IsEmptyNode( nsIDOMNode *aNode, 
                           PRBool *outIsEmptyNode, 
                           PRBool aMozBRDoesntCount,
                           PRBool aListOrCellNotEmpty,
                           PRBool aSafeToAskFrames)
{
  if (!aNode || !outIsEmptyNode) return NS_ERROR_NULL_POINTER;
  *outIsEmptyNode = PR_TRUE;
  
  // effeciency hack - special case if it's a text node
  if (nsEditor::IsTextNode(aNode))
  {
    PRUint32 length = 0;
    nsCOMPtr<nsIDOMCharacterData>nodeAsText;
    nodeAsText = do_QueryInterface(aNode);
    nodeAsText->GetLength(&length);
    if (length) *outIsEmptyNode = PR_FALSE;
    return NS_OK;
  }

  // if it's not a text node (handled above) and it's not a container,
  // then we dont call it empty (it's an <hr>, or <br>, etc).
  // Also, if it's an anchor then dont treat it as empty - even though
  // anchors are containers, named anchors are "empty" but we don't
  // want to treat them as such.  Also, don't call ListItems or table
  // cells empty if caller desires.
  if (!IsContainer(aNode) || nsHTMLEditUtils::IsAnchor(aNode) || 
       (aListOrCellNotEmpty && nsHTMLEditUtils::IsListItem(aNode)) ||
       (aListOrCellNotEmpty && nsHTMLEditUtils::IsTableCell(aNode)) ) 
  {
    *outIsEmptyNode = PR_FALSE;
    return NS_OK;
  }
  
  // iterate over node. if no children, or all children are either 
  // empty text nodes or non-editable, then node qualifies as empty
  nsCOMPtr<nsIContentIterator> iter;
  nsCOMPtr<nsIContent> nodeAsContent = do_QueryInterface(aNode);
  if (!nodeAsContent) return NS_ERROR_FAILURE;
  nsresult res = nsComponentManager::CreateInstance(kCContentIteratorCID,
                                        nsnull,
                                        NS_GET_IID(nsIContentIterator), 
                                        getter_AddRefs(iter));
  if (NS_FAILED(res)) return res;
  res = iter->Init(nodeAsContent);
  if (NS_FAILED(res)) return res;
    
  while (NS_ENUMERATOR_FALSE == iter->IsDone())
  {
    nsCOMPtr<nsIDOMNode> node;
    nsCOMPtr<nsIContent> content;
    res = iter->CurrentNode(getter_AddRefs(content));
    if (NS_FAILED(res)) return res;
    node = do_QueryInterface(content);
    if (!node) return NS_ERROR_FAILURE;
 
    // is the node editable and non-empty?  if so, return false
    if (nsEditor::IsEditable(node))
    {
      if (nsEditor::IsTextNode(node))
      {
        PRUint32 length = 0;
        nsCOMPtr<nsIDOMCharacterData>nodeAsText;
        nodeAsText = do_QueryInterface(node);
        nodeAsText->GetLength(&length);
        if (aSafeToAskFrames)
        {
          nsCOMPtr<nsISelectionController> selCon;
          res = GetSelectionController(getter_AddRefs(selCon));
          if (NS_FAILED(res)) return res;
          if (!selCon) return NS_ERROR_FAILURE;
          PRBool isVisible = PR_FALSE;
        // ask the selection controller for information about whether any
        // of the data in the node is really rendered.  This is really
        // something that frames know about, but we aren't supposed to talk to frames.
        // So we put a call in the selection controller interface, since it's already
        // in bed with frames anyway.  (this is a fix for bug 22227, and a
        // partial fix for bug 46209)
          res = selCon->CheckVisibility(node, 0, length, &isVisible);
          if (NS_FAILED(res)) return res;
          if (isVisible) 
          {
            *outIsEmptyNode = PR_FALSE;
            break;
          }
        }
        else if (length)
        {
          *outIsEmptyNode = PR_FALSE;
          break;
        }
      }
      else  // an editable, non-text node. we aren't an empty block 
      {
        // is it the node we are iterating over?
        if (node.get() == aNode) break;
        // is it a moz-BR and did the caller ask us not to consider those relevant?
        if (!(aMozBRDoesntCount && nsHTMLEditUtils::IsMozBR(node))) 
        {
          // is it an empty node of some sort?
          PRBool isEmptyNode;
          res = IsEmptyNode(node, &isEmptyNode, aMozBRDoesntCount, aListOrCellNotEmpty);
          if (NS_FAILED(res)) return res;
          if (!isEmptyNode) 
          {
            // otherwise it ain't empty
            *outIsEmptyNode = PR_FALSE;
            break;
          }
        }
      }
    }
    res = iter->Next();
    if (NS_FAILED(res)) return res;
  }
  
  return NS_OK;
}


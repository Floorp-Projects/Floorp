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
#include "nsIDOMSelection.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsISelectionController.h"
#include "nsIFrameSelection.h"  // For TABLESELECTION_ defines

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
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsIDocumentEncoder.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIPresShell.h"
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
static NS_DEFINE_IID(kFileWidgetCID,  NS_FILEWIDGET_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

// Drag & Drop, Clipboard Support
static NS_DEFINE_CID(kCClipboardCID,    NS_CLIPBOARD_CID);
static NS_DEFINE_CID(kCTransferableCID, NS_TRANSFERABLE_CID);

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
        if (tagName.Equals("ol") || 
            tagName.Equals("ul") ||
            tagName.Equals("dl"))
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
  // Don't use getDocument here, because we have no way of knowing if
  // Init() was ever called.  So we need to get the document ourselves,
  // if it exists.
  if (mDocWeak)
  {
    nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
    if (doc)
    {
      nsCOMPtr<nsIDOMEventReceiver> erP = do_QueryInterface(doc, &result);
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
    }
  }

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
                                 nsIPresShell   *aPresShell, PRUint32 aFlags)
{
  NS_PRECONDITION(aDoc && aPresShell, "bad arg");
  if (!aDoc || !aPresShell)
    return NS_ERROR_NULL_POINTER;

  nsresult result = NS_ERROR_NULL_POINTER;
  // Init the base editor
  result = nsEditor::Init(aDoc, aPresShell, aFlags);
  if (NS_FAILED(result)) { return result; }

  nsCOMPtr<nsIDOMElement> bodyElement;
  result = nsEditor::GetBodyElement(getter_AddRefs(bodyElement));
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
      result = domdoc->GetElementsByTagName("meta", getter_AddRefs(metaList)); 
      if (NS_SUCCEEDED(result) && metaList) { 
        PRUint32 listLength = 0; 
        (void) metaList->GetLength(&listLength); 

        for (PRUint32 i = 0; i < listLength; i++) { 
          metaList->Item(i, getter_AddRefs(metaNode)); 
          if (!metaNode) continue; 
          metaElement = do_QueryInterface(metaNode); 
          if (!metaElement) continue; 

          const nsString content("charset="); 
          nsString currentValue; 

          if (NS_FAILED(metaElement->GetAttribute("http-equiv", currentValue))) continue; 

          if (kNotFound != currentValue.Find("content-type", PR_TRUE)) { 
            if (NS_FAILED(metaElement->GetAttribute("content", currentValue))) continue; 

            PRInt32 offset = currentValue.Find(content.GetUnicode(), PR_TRUE); 
            if (kNotFound != offset) {
              currentValue.Left(newMetaString, offset); // copy current value before "charset=" (e.g. text/html) 
              newMetaString.Append(content); 
              newMetaString.Append(characterSet); 
              result = nsEditor::SetAttribute(metaElement, "content", newMetaString); 
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

        result = domdoc->GetElementsByTagName("head",getter_AddRefs(headList)); 
        if (NS_SUCCEEDED(result) && headList) { 
          headList->Item(0, getter_AddRefs(headNode)); 
          if (headNode) { 
            // Create a new meta charset tag 
            result = CreateNode("meta", headNode, 0, getter_AddRefs(resultNode)); 
            if (NS_FAILED(result)) 
              return NS_ERROR_FAILURE; 

            // Set attributes to the created element 
            if (resultNode && nsCRT::strlen(characterSet) > 0) { 
              metaElement = do_QueryInterface(resultNode); 
              if (metaElement) { 
                // not undoable, undo should undo CreateNode 
                result = metaElement->SetAttribute("http-equiv", "Content-Type"); 
                if (NS_SUCCEEDED(result)) { 
                  newMetaString.Assign("text/html;charset="); 
                  newMetaString.Append(characterSet); 
                  // not undoable, undo should undo CreateNode 
                  result = metaElement->SetAttribute("content", newMetaString); 
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

  // get the DOM event receiver
  nsCOMPtr<nsIDOMEventReceiver> erP;
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;
  result = doc->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), getter_AddRefs(erP));
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
      PRBool bHandled = PR_FALSE;
      res = TabInTable(isShift, &bHandled);
      if (NS_FAILED(res)) return res;
      if (bHandled) return res;
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
  // find selection start
  nsCOMPtr<nsIDOMSelection> selection;

  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 selOffset;
  res = GetStartNodeAndOffset(selection, &parent, &selOffset);
  if (NS_FAILED(res)) return res;
  if (!parent) return res;
  nsCOMPtr<nsIDOMNode> selNode = GetChildAt(parent, selOffset);
  if (!selNode) selNode = parent;
  
  // is it in a table block of some kind?
  nsCOMPtr<nsIDOMNode> block;
  if (IsBlockNode(selNode)) block = selNode;
  else block = GetBlockNodeParent(selNode);
  if (!block) return res;
  
  if (IsTableElement(block))
  {
    // find enclosing table
    nsCOMPtr<nsIDOMNode> tbl;
    if (IsTable(block)) tbl = block;
    else tbl = GetEnclosingTable(block);
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
    nsCOMPtr<nsIContent> cBlock = do_QueryInterface(block);
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
      if (NS_FAILED(res)) return res;
      res = iter->CurrentNode(getter_AddRefs(cNode));
      if (NS_FAILED(res)) return res;
      node = do_QueryInterface(cNode);
      if (IsTableCell(node) && (GetEnclosingTable(node) == tbl))
      {
        selection->Collapse(node, 0);
        *outHandled = PR_TRUE;
        return NS_OK;
      }
    } while (iter->IsDone() == NS_ENUMERATOR_FALSE);
  }
  return NS_OK;
}

NS_IMETHODIMP nsHTMLEditor::JoeCreateBR(nsCOMPtr<nsIDOMNode> *aInOutParent, PRInt32 *aInOutOffset, nsCOMPtr<nsIDOMNode> *outBRNode, EDirection aSelect)
{
  if (!aInOutParent || !*aInOutParent || !aInOutOffset || !outBRNode) return NS_ERROR_NULL_POINTER;
  *outBRNode = nsnull;
  nsresult res;
  
  // we need to insert a br.  unfortunately, we may have to split a text node to do it.
  nsCOMPtr<nsIDOMNode> node = *aInOutParent;
  PRInt32 theOffset = *aInOutOffset;
  nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(node);
  nsAutoString brType("br");
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
  return JoeCreateBR(&parent, &offset, outBRNode, aSelect);
}

NS_IMETHODIMP nsHTMLEditor::InsertBR(nsCOMPtr<nsIDOMNode> *outBRNode)
{
  PRBool bCollapsed;
  nsCOMPtr<nsIDOMSelection> selection;

  if (!outBRNode) return NS_ERROR_NULL_POINTER;
  *outBRNode = nsnull;
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
    return SetTypeInStateForProperty(*mTypeInState, aProperty, aAttribute, aValue);
  }
  
  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, kOpInsertElement, nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  
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
        selection->ClearSelection();

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
        
        // MOOSE: workaround for selection bug:
        selection->ClearSelection();
        
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
  if (!inRange || !aProperty) return NS_ERROR_NULL_POINTER;
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
                                           nsIAtom *aProperty, 
                                           const nsString *aAttribute)
{
  if (!aNode || !*aNode || !aOffset) return NS_ERROR_NULL_POINTER;
  // split any matching style nodes above the node/offset
  nsCOMPtr<nsIDOMNode> parent, tmp = *aNode;
  PRInt32 offset;
  while (tmp && !nsHTMLEditUtils::IsBody(tmp))
  {
    if (NodeIsType(tmp, aProperty))
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
                                             
nsresult nsHTMLEditor::RemoveStyleInside(nsIDOMNode *aNode, 
                                   nsIAtom *aProperty, 
                                   const nsString *aAttribute, 
                                   PRBool aChildrenOnly)
{
  if (!aNode || !aProperty) return NS_ERROR_NULL_POINTER;
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
  if (!aChildrenOnly && NodeIsType(aNode, aProperty))
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
  nsIAtom* attrName;
  content->GetAttributeCount(attrCount);
  
  for (i=0; i<attrCount; i++)
  {
    content->GetAttributeNameAt(i, nameSpaceID, attrName);
    nsAutoString attrString, tmp;
    if (!attrName) continue;  // ooops
    attrName->ToString(attrString);
    // if it's the attribute we know about, keep looking
    if (attrString.EqualsIgnoreCase(*aAttribute)) continue;
    // if it's a special _moz... attribute, keep looking
    attrString.Left(tmp,4);
    if (tmp.Equals("_moz")) continue;
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
  nsIAtom* attrName;
  nsresult res, res2;
  content1->GetAttributeCount(attrCount);
  nsAutoString attrString, tmp, attrVal1, attrVal2;
  
  for (i=0; i<attrCount; i++)
  {
    content1->GetAttributeNameAt(i, nameSpaceID, attrName);
    if (!attrName) continue;  // ooops
    attrName->ToString(attrString);
    // if it's a special _moz... attribute, keep going
    attrString.Left(tmp,4);
    if (tmp.Equals("_moz")) continue;
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
    content2->GetAttributeNameAt(i, nameSpaceID, attrName);
    if (!attrName) continue;  // ooops
    attrName->ToString(attrString);
    // if it's a special _moz... attribute, keep going
    attrString.Left(tmp,4);
    if (tmp.Equals("_moz")) continue;
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
    GetFirstEditableNode(aNode, &firstNode);
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
    GetLastEditableNode(aNode, &lastNode);
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
                                              PRBool &aFirst, PRBool &aAny, PRBool &aAll)
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
        GetTypingState(aProperty, isSet, theSetting);
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
        GetTypingState(aProperty, isSet, theSetting);
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
        GetTypingState(aProperty, isSet, theSetting);
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
      else if (aProperty == mFontAtom.get())
      {
// MOOSE!          
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
    result = iter->CurrentNode(getter_AddRefs(content));
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
          IsTextPropertySetByContent(node, aProperty, aAttribute, aValue, isSet, getter_AddRefs(resultNode));
          if (first)
          {
            aFirst = isSet;
            first = PR_FALSE;
          }
          if (isSet) {
            aAny = PR_TRUE;
          }
          else {
            aAll = PR_FALSE;
          }
        }
      }
      iter->Next();
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

NS_IMETHODIMP nsHTMLEditor::RemoveInlineProperty(nsIAtom *aProperty, const nsString *aAttribute)
{
  if (!aProperty) return NS_ERROR_NULL_POINTER;
  if (!mRules)    return NS_ERROR_NOT_INITIALIZED;
  ForceCompositionEnd();

  nsresult res;
  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, kOpRemoveTextProperty, nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  
  PRBool cancel, handled;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kRemoveTextProperty);
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(res)) return res;
  if (!cancel && !handled)
  {
    PRBool isCollapsed;
    selection->GetIsCollapsed(&isCollapsed);
    if (isCollapsed)
    {
      // manipulating text attributes on a collapsed selection only sets state for the next text insertion
      // But only if it's a property for which we have TypeInState!
      if ( (aProperty == mBoldAtom.get()) || 
           (aProperty == mItalicAtom.get()) || 
           (aProperty == mUnderlineAtom.get()) )
      {
        SetTypeInStateForProperty(*mTypeInState, aProperty, aAttribute, nsnull);
      }
      else if (aProperty == mLinkAtom.get())
      {
        // a hack for links.  The user is trying to kill the link "style" where
        // they are typing.  We need to split up through any link elements above the
        // collapsed insertion point.
        nsCOMPtr<nsIDOMNode> selNode, tmpNode, parent;
        PRInt32 selOffset, outOffset;
        res = GetStartNodeAndOffset(selection, &selNode, &selOffset);
        if (NS_FAILED(res)) return res;
        tmpNode = selNode;
        while (tmpNode)
        {
          if (nsHTMLEditUtils::IsBody(tmpNode)) break;
          if (IsLinkNode(tmpNode))
          {
            res = SplitNodeDeep(tmpNode, selNode, selOffset, &outOffset);
            if (NS_FAILED(res)) return res;
            tmpNode->GetParentNode(getter_AddRefs(parent));
            selection->Collapse(parent, outOffset);
            break;
          }
          tmpNode->GetParentNode(getter_AddRefs(parent));
          tmpNode = parent;
        }
      }
    }
    else
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
        
        // remove this style from ancestors of our range empoints, 
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

NS_IMETHODIMP nsHTMLEditor::GetTypingState(nsIAtom *aProperty, PRBool &aPropIsSet, PRBool &aSetting)
{
  if (!aProperty)
    return NS_ERROR_NULL_POINTER;
  
  if (!mTypeInState)
    return NS_ERROR_NOT_INITIALIZED;
  
  PRUint32  styleEnum;
  mTypeInState->GetEnumForName(aProperty, styleEnum);
  if (styleEnum == NS_TYPEINSTATE_UNKNOWN)
    return NS_ERROR_UNEXPECTED;
  
  aPropIsSet = mTypeInState->IsSet(styleEnum);
  if (aPropIsSet) mTypeInState->GetProp(styleEnum, aSetting);
  
  return NS_OK;
}


NS_IMETHODIMP nsHTMLEditor::GetTypingStateValue(nsIAtom *aProperty, PRBool &aPropIsSet, nsString &aValue)
{
  if (!aProperty)
    return NS_ERROR_NULL_POINTER;
  
  if (!mTypeInState)
    return NS_ERROR_NOT_INITIALIZED;
  
  PRUint32  styleEnum;
  mTypeInState->GetEnumForName(aProperty, styleEnum);
  if (styleEnum == NS_TYPEINSTATE_UNKNOWN)
    return NS_ERROR_UNEXPECTED;
  
  aPropIsSet = mTypeInState->IsSet(styleEnum);
  if (aPropIsSet) mTypeInState->GetPropValue(styleEnum, aValue);
  
  return NS_OK;
}

NS_IMETHODIMP nsHTMLEditor::DeleteSelection(nsIEditor::EDirection aAction)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel, handled;
  nsresult result;

  // If it's one of these modes,
  // we have to extend the selection first.
  // This can't happen inside selection batching --
  // selection refuses to move if batching is on.
  if (aAction == eNextWord || aAction == ePreviousWord
      || aAction == eToBeginningOfLine || aAction == eToEndOfLine)
  {
    if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
    nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
    nsCOMPtr<nsISelectionController> selCont (do_QueryInterface(ps));
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

  // delete placeholder txns merge.
  nsAutoPlaceHolderBatch batch(this, gDeleteTxnName);
  nsAutoRules beginRulesSniffing(this, kOpDeleteSelection, aAction);

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
  ruleInfo.typeInState = *mTypeInState;
  ruleInfo.maxLength = mMaxTextLength;

  result = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(result)) return result;
  if (!cancel && !handled)
  {
    result = InsertTextImpl(resultString);
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

  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  // give rules a chance to handle or cancel
  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kInsertElement);
  PRBool cancel, handled;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(res)) return res;
  if (cancel) return NS_OK; // rules canceled the operation
  if (!handled)
  {
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

  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kInsertBreak);
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(res)) return res;
  if (!cancel && !handled)
  {
    // create the new BR node
    nsCOMPtr<nsIDOMNode> newNode;
    nsAutoString tag("BR");
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
  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kInsertElement);
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

NS_IMETHODIMP nsHTMLEditor::SetParagraphFormat(const nsString& aParagraphFormat)
{
  //Kinda sad to waste memory just to force lower case
  nsAutoString tag = aParagraphFormat;
  tag.ToLowerCase();
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
      res = GetElementOrParentByTagName("list", node, getter_AddRefs(blockParentElem));
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
            res = GetElementOrParentByTagName("list", startParent, getter_AddRefs(blockParent));
          } else {
            res = GetBlockParent(startParent, getter_AddRefs(blockParent));
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


// get the paragraph style(s) for the selection
NS_IMETHODIMP 
nsHTMLEditor::GetParagraphTags(nsStringArray *aTagList)
{
#if 0
  if (gNoisy) { printf("---------- nsHTMLEditor::GetPargraphTags ----------\n"); }
#endif
  return GetParentBlockTags(aTagList, PR_FALSE);
}

NS_IMETHODIMP 
nsHTMLEditor::GetListTags(nsStringArray *aTagList)
{
  return GetParentBlockTags(aTagList, PR_TRUE);
}

/*
// use this when block parents are to be added regardless of current state
NS_IMETHODIMP 
nsHTMLEditor::AddBlockParent(nsString& aParentTag)
{
  if (gNoisy) 
  { 
    char *tag = aParentTag.ToNewCString();
    printf("---------- nsHTMLEditor::AddBlockParent %s ----------\n", tag); 
    nsCRT::free(tag);
  }
  
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

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
    // XXX: ERROR_HANDLING  can currentItem be null?
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
    printf("- nsHTMLEditor::ReplaceBlockParent %s -\n", tag); 
    nsCRT::free(tag);
  }
  
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsAutoSelectionReset selectionResetter(selection, this);
  // set the block parent for all selected ranges
  nsAutoEditBatch beginBatching(this);
  nsCOMPtr<nsIEnumerator> enumerator;
  res = selection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(res)) return res;
  if (!enumerator) return NS_ERROR_NULL_POINTER;

  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  res = enumerator->CurrentItem(getter_AddRefs(currentItem));
  // XXX: ERROR_HANDLING  can currentItem be null?
  if ((NS_SUCCEEDED(res)) && (currentItem))
  {
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    // scan the range for all the independent block content blockSections
    // and apply the transformation to them
    res = ReParentContentOfRange(range, aParentTag, eReplaceParent);
  }
  if (gNoisy) {printf("Finished nsHTMLEditor::AddBlockParent with this content:\n"); DebugDumpContent(); } // DEBUG

  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::RemoveParagraphStyle()
{
  if (gNoisy) { 
    printf("- nsHTMLEditor::RemoveParagraphStyle -\n"); 
  }
  
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoEditBatch beginBatching(this);

  nsCOMPtr<nsIEnumerator> enumerator;
  res = selection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(res)) return res;
  if (!enumerator) return NS_ERROR_NULL_POINTER;

  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  res = enumerator->CurrentItem(getter_AddRefs(currentItem));
  // XXX: ERROR_HANDLING  can currentItem be null?
  if ((NS_SUCCEEDED(res)) && (currentItem))
  {
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    res = RemoveParagraphStyleFromRange(range);
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::RemoveParent(const nsString &aParentTag)
{
  if (gNoisy) { 
    printf("- nsHTMLEditor::RemoveParent -\n"); 
  }
  
  nsresult res;
  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoEditBatch beginBatching(this);
  nsCOMPtr<nsIEnumerator> enumerator;
  res = selection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(res)) return res;
  if (!enumerator) return NS_ERROR_NULL_POINTER;

  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  res = enumerator->CurrentItem(getter_AddRefs(currentItem));
  // XXX: ERROR_HANDLING can currentItem be null?
  if ((NS_SUCCEEDED(res)) && (currentItem))
  {
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    res = RemoveParentFromRange(aParentTag, range);
  }
  return res;
}
*/

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

  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kMakeList);
  if (aListType.Equals("ol")) ruleInfo.bOrdered = PR_TRUE;
  else  ruleInfo.bOrdered = PR_FALSE;
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
      nsAutoString tag("li");
      nsCOMPtr<nsIDOMNode> newItem;
      res = CreateNode(tag, newList, 0, getter_AddRefs(newItem));
      if (NS_FAILED(res)) return res;
      // put a space in it so layout will draw the list item
      // XXX - revisit when layout is fixed
      res = selection->Collapse(newItem,0);
      if (NS_FAILED(res)) return res;
#if 0    
      nsAutoString theText(" ");
      res = InsertText(theText);
      if (NS_FAILED(res)) return res;
      // reposition selection to before the space character
      res = GetStartNodeAndOffset(selection, &node, &offset);
      if (NS_FAILED(res)) return res;
      res = selection->Collapse(node,0);
      if (NS_FAILED(res)) return res;
#endif
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

  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kRemoveList);
  if (aListType.Equals("ol")) ruleInfo.bOrdered = PR_TRUE;
  else  ruleInfo.bOrdered = PR_FALSE;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || (NS_FAILED(res))) return res;

  // no default behavior for this yet.  what would it mean?

  res = mRules->DidDoAction(selection, &ruleInfo, res);
  return res;
}


NS_IMETHODIMP
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
  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kMakeBasicBlock);
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
  PRInt32 theAction = nsHTMLEditRules::kIndent;
  PRInt32 opID = kOpIndent;
  if (aIndent.Equals("outdent"))
  {
    theAction = nsHTMLEditRules::kOutdent;
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
  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kAlign);
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
   
  nsAutoString TagName = aTagName;
  TagName.ToLowerCase();
  PRBool getLink = IsLink(TagName);
  PRBool getNamedAnchor = IsNamedAnchor(TagName);
  if ( getLink || getNamedAnchor)
  {
    TagName = "a";  
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
  
  nsAutoString TagName = aTagName;
  TagName.ToLowerCase();
  // Empty string indicates we should match any element tag
  PRBool anyTag = (TagName.IsEmpty());
  
  //Note that this doesn't need to go through the transaction system

  nsresult res=NS_ERROR_NOT_INITIALIZED;
  //PRBool first=PR_TRUE;
  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  
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
      res = GetElementOrParentByTagName("href", anchorNode, getter_AddRefs(parentLinkOfAnchor));
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
    if (NS_SUCCEEDED(res))
    {
      if(!enumerator)
        return NS_ERROR_NULL_POINTER;

      enumerator->First(); 
      nsCOMPtr<nsISupports> currentItem;
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if ((NS_SUCCEEDED(res)) && currentItem)
      {
        nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
        nsCOMPtr<nsIContentIterator> iter;
        res = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                                    NS_GET_IID(nsIContentIterator), 
                                                    getter_AddRefs(iter));
        // XXX: ERROR_HANDLING  XPCOM usage
        if ((NS_SUCCEEDED(res)) && iter)
        {
          iter->Init(range);
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
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  if (!doc) return NS_ERROR_NOT_INITIALIZED;
  res = doc->CreateElement(realTagName, getter_AddRefs(newElement));
  if (NS_FAILED(res) || !newElement)
    return NS_ERROR_FAILURE;

  // Mark the new element dirty, so it will be formatted
  newElement->SetAttribute("_moz_dirty", "");

  // Set default values for new elements
  if (TagName.Equals("hr"))
  {
    // Note that we read the user's attributes for these from prefs (in InsertHLine JS)
    newElement->SetAttribute("align","center");
    newElement->SetAttribute("width","100%");
    newElement->SetAttribute("size","2");
  } else if (TagName.Equals("table"))
  {
    newElement->SetAttribute("cellpadding","2");
    newElement->SetAttribute("cellspacing","2");
    newElement->SetAttribute("border","1");
  } else if (TagName.Equals("tr"))
  {
    newElement->SetAttribute("valign","top");
  } else if (TagName.Equals("td"))
  {
    newElement->SetAttribute("valign","top");

// I'm def'ing this out to see if auto br insertion is working here
#if 0
    // Insert the default space in a cell so border displays
    nsCOMPtr<nsIDOMNode> newCellNode = do_QueryInterface(newElement);
    if (newCellNode)
    {
      // TODO: This should probably be in the RULES code or 
      //       preference based for "should we add the nbsp"
      nsCOMPtr<nsIDOMText>newTextNode;
      nsAutoString space;
      // Set contents to the &nbsp character by concatanating the char code
      space += nbsp;
      // If we fail here, we return NS_OK anyway, since we have an OK cell node
      nsresult result = doc->CreateTextNode(space, getter_AddRefs(newTextNode));
      if (NS_FAILED(result)) return result;
      if (!newTextNode) return NS_ERROR_NULL_POINTER;

      nsCOMPtr<nsIDOMNode>resultNode;
      result = newCellNode->AppendChild(newTextNode, getter_AddRefs(resultNode));
    }
#endif
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
        const nsString attribute("href");
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
  if (element)
  {
    if (selectedCount > 0)
    {
      // Traverse all selected cells
      nsCOMPtr<nsIDOMElement> cell;
      res = GetFirstSelectedCell(getter_AddRefs(cell));
      if (NS_SUCCEEDED(res) && cell)
      {
        while(cell)
        {
          SetAttribute(cell, "bgcolor", aColor);
          GetNextSelectedCell(getter_AddRefs(cell));
        };
        return NS_OK;
      }
    }
    // If we failed to find a cell, fall through to use originally-found element
  } else {
    // No table element -- set the background color on the body tag
    res = nsEditor::GetBodyElement(getter_AddRefs(element));
    if (NS_FAILED(res)) return res;
    if (!element)       return NS_ERROR_NULL_POINTER;
  }
  // Use the editor method that goes through the transaction system
  return SetAttribute(element, "bgcolor", aColor);
}

NS_IMETHODIMP nsHTMLEditor::SetBodyAttribute(const nsString& aAttribute, const nsString& aValue)
{
  nsresult res;
  // TODO: Check selection for Cell, Row, Column or table and do color on appropriate level

  NS_ASSERTION(mDocWeak, "Missing Editor DOM Document");
  
  // Set the background color attribute on the body tag
  nsCOMPtr<nsIDOMElement> bodyElement;

  res = nsEditor::GetBodyElement(getter_AddRefs(bodyElement));
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
  result = nsEditor::GetBodyElement(getter_AddRefs(bodyElement));
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

NS_IMETHODIMP 
nsHTMLEditor::ApplyOverrideStyleSheet(const nsString& aURL)
{
  return ApplyDocumentOrOverrideStyleSheet(aURL, PR_TRUE);
}

NS_IMETHODIMP 
nsHTMLEditor::ApplyStyleSheet(const nsString& aURL)
{
  return ApplyDocumentOrOverrideStyleSheet(aURL, PR_FALSE);
}

//Note: Loading a document style sheet is undoable, loading an override sheet is not
nsresult 
nsHTMLEditor::ApplyDocumentOrOverrideStyleSheet(const nsString& aURL, PRBool aOverride)
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

    if (NS_SUCCEEDED(rv)) {
      if (!document)
        return NS_ERROR_NULL_POINTER;

      nsCOMPtr<nsIHTMLContentContainer> container = do_QueryInterface(document);
      if (!container)
        return NS_ERROR_NULL_POINTER;
        
      nsCOMPtr<nsICSSLoader> cssLoader;
      nsCOMPtr<nsICSSStyleSheet> cssStyleSheet;

      rv = container->GetCSSLoader(*getter_AddRefs(cssLoader));

      if (NS_SUCCEEDED(rv)) {
        PRBool complete;
        
        if (!cssLoader)
          return NS_ERROR_NULL_POINTER;

        if (aOverride) {
          // We use null for the callback and data pointer because
          //  we MUST ONLY load synchronous local files (no @import)
          rv = cssLoader->LoadAgentSheet(uaURL, *getter_AddRefs(cssStyleSheet), complete,
                                         nsnull);

          // Synchronous loads should ALWAYS return completed
          if (!complete || !cssStyleSheet)
            return NS_ERROR_NULL_POINTER;

          nsCOMPtr<nsIStyleSheet> styleSheet = do_QueryInterface(cssStyleSheet);
          nsCOMPtr<nsIStyleSet> styleSet;
          rv = ps->GetStyleSet(getter_AddRefs(styleSet));
          if (NS_SUCCEEDED(rv)) {
            if (!styleSet)
              return NS_ERROR_NULL_POINTER;

            // Add the override style sheet
            // (This checks if already exists
            //  If yes, it and reads it does)
            styleSet->AppendOverrideStyleSheet(styleSheet);

            // This notifies document observers to rebuild all frames
            // (this doesn't affect style sheet because it is not a doc sheet)
            document->SetStyleSheetDisabledState(styleSheet, PR_FALSE);
          }
        }
        else {
          rv = cssLoader->LoadAgentSheet(uaURL, *getter_AddRefs(cssStyleSheet), complete,
                                         this);

          if (NS_SUCCEEDED(rv)) {
            if (complete) {
              if (cssStyleSheet) {
                ApplyStyleSheetToPresShellDocument(cssStyleSheet,this);
              }
              else
                rv = NS_ERROR_NULL_POINTER;
            }
            //
            // If not complete, we will be notified later
            // with a call to ApplyStyleSheetToPresShellDocument().
            //
          }
        }
      }
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
  nsresult res = GetBodyElement(getter_AddRefs(body));
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsIContent> content = do_QueryInterface(body);

  nsIFrame *frame;
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  res = ps->GetPrimaryFrameFor(content, &frame);
  if (NS_FAILED(res)) return res;
  
  nsCOMPtr<nsIStyleContext> styleContext;
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
  res = GetBodyElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;
  if (!bodyElement) return NS_ERROR_NULL_POINTER;

  // Get the current style for this body element:
  nsAutoString styleName ("style");
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
    styleValue.Append("; ");
  }

  // Make sure we have fixed-width font.  This should be done for us,
  // but it isn't, see bug 22502, so we have to add "font: monospace;".
  // Only do this if we're wrapping.
  if (aWrapColumn >= 0)
    styleValue.Append("font-family: monospace; ");

  // and now we're ready to set the new whitespace/wrapping style.
  if (aWrapColumn > 0)        // Wrap to a fixed column
  {
    styleValue.Append("white-space: -moz-pre-wrap; width: ");
    styleValue.Append(aWrapColumn);
    styleValue.Append("ch;");
  }
  else if (aWrapColumn == 0)
    styleValue.Append("white-space: -moz-pre-wrap;");
  else
    styleValue.Append("white-space: pre;");

  res = bodyElement->SetAttribute(styleName, styleValue);

#ifdef DEBUG_akkana
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
        if (tagName.Equals("img") || tagName.Equals("embed"))
          (*aNodeList)->AppendElement(node);
        else if (tagName.Equals("a"))
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

  BeginUpdateViewBatch();

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
   
  EndUpdateViewBatch();

  return result;
}


NS_IMETHODIMP 
nsHTMLEditor::Redo(PRUint32 aCount)
{
  nsresult result = NS_OK;

  BeginUpdateViewBatch();

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
   
  EndUpdateViewBatch();

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

NS_IMETHODIMP nsHTMLEditor::Paste()
{
  ForceCompositionEnd();
  nsAutoString stuffToPaste;

  // Get Clipboard Service
  nsresult rv;
  NS_WITH_SERVICE ( nsIClipboard, clipboard, kCClipboardCID, &rv );
  if ( NS_FAILED(rv) )
    return rv;
    
  // Create generic Transferable for getting the data
  nsCOMPtr<nsITransferable> trans;
  rv = nsComponentManager::CreateInstance(kCTransferableCID, nsnull, 
                                          NS_GET_IID(nsITransferable), 
                                          (void**) getter_AddRefs(trans));
  if (NS_SUCCEEDED(rv))
  {
    // Get the nsITransferable interface for getting the data from the clipboard
    if (trans)
    {
      // Create the desired DataFlavor for the type of data
      // we want to get out of the transferable
      if ((mFlags & eEditorPlaintextMask) == 0)  // This should only happen in html editors, not plaintext
      {
        trans->AddDataFlavor(kJPEGImageMime);
        trans->AddDataFlavor(kHTMLMime);
      }
      trans->AddDataFlavor(kUnicodeMime);

      // Get the Data from the clipboard
      if (NS_SUCCEEDED(clipboard->GetData(trans)))
      {
        char* bestFlavor = nsnull;
        nsCOMPtr<nsISupports> genericDataObj;
        PRUint32 len = 0;
        if ( NS_SUCCEEDED(trans->GetAnyTransferData(&bestFlavor, getter_AddRefs(genericDataObj), &len)) )
        {
          nsAutoString flavor ( bestFlavor );   // just so we can use flavor.Equals()
#ifdef DEBUG_akkana
          printf("Got flavor [%s]\n", bestFlavor);
#endif
          if (flavor.Equals(kHTMLMime))
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
          else if (flavor.Equals(kUnicodeMime))
          {
            nsCOMPtr<nsISupportsWString> textDataObj ( do_QueryInterface(genericDataObj) );
            if (textDataObj && len > 0)
            {
              PRUnichar* text = nsnull;
              textDataObj->ToString ( &text );
              stuffToPaste.Assign ( text, len / 2 );
              nsAutoEditBatch beginBatching(this);
              rv = InsertText(stuffToPaste);
            }
          }
          else if (flavor.Equals(kJPEGImageMime))
          {
            // Insert Image code here
            printf("Don't know how to insert an image yet!\n");
            //nsIImage* image = (nsIImage *)data;
            //NS_RELEASE(image);
            rv = NS_ERROR_NOT_IMPLEMENTED; // for now give error code
          }
        }
        nsCRT::free(bestFlavor);
      }
    }
  }

  // Try to scroll the selection into view if the paste succeeded:
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIPresShell> presShell;
    if (NS_SUCCEEDED(GetPresShell(getter_AddRefs(presShell))) && presShell)
      presShell->ScrollSelectionIntoView(SELECTION_NORMAL, SELECTION_FOCUS_REGION);
  }

  return rv;
}


NS_IMETHODIMP nsHTMLEditor::CanPaste(PRBool &aCanPaste)
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
  rv = clipboard->HasDataMatchingFlavors(flavorsList, &haveFlavors);
  if (NS_FAILED(rv)) return rv;
  
  aCanPaste = haveFlavors;
  return NS_OK;
}


// 
// HTML PasteAsQuotation: Paste in a blockquote type=cite
//
NS_IMETHODIMP nsHTMLEditor::PasteAsQuotation()
{
  if (mFlags & eEditorPlaintextMask)
    return PasteAsPlaintextQuotation();

  nsAutoString citation("");
  return PasteAsCitedQuotation(citation);
}

NS_IMETHODIMP nsHTMLEditor::PasteAsCitedQuotation(const nsString& aCitation)
{
  nsAutoEditBatch beginBatching(this);

  // get selection
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  // give rules a chance to handle or cancel
  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kInsertElement);
  PRBool cancel, handled;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(res)) return res;
  if (cancel) return NS_OK; // rules canceled the operation
  if (!handled)
  {
    nsCOMPtr<nsIDOMNode> newNode;
    nsAutoString tag("blockquote");
    res = DeleteSelectionAndCreateNode(tag, getter_AddRefs(newNode));
    if (NS_FAILED(res)) return res;
    if (!newNode) return NS_ERROR_NULL_POINTER;

    // Try to set type=cite.  Ignore it if this fails.
    nsCOMPtr<nsIDOMElement> newElement (do_QueryInterface(newNode));
    if (newElement)
    {
      nsAutoString type ("type");
      nsAutoString cite ("cite");
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

    res = Paste();
  }
  return res;
}

//
// Paste a plaintext quotation
//
NS_IMETHODIMP nsHTMLEditor::PasteAsPlaintextQuotation()
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
    clipboard->GetData(trans);

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
    nsAutoString flavor(flav);
    nsAutoString stuffToPaste;
    if (flavor.Equals(kUnicodeMime))
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

  nsAutoString citation ("");
  nsAutoString charset ("");
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

  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpInsertQuotation, nsIEditor::eNext);

  // get selection
  nsCOMPtr<nsIDOMSelection> selection;
  rv = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(rv)) return rv;
  if (!selection) return NS_ERROR_NULL_POINTER;

  // give rules a chance to handle or cancel
  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kInsertElement);
  PRBool cancel, handled;
  rv = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(rv)) return rv;
  if (cancel) return NS_OK; // rules canceled the operation
  nsCOMPtr<nsIDOMNode> preNode;
  if (!handled)
  {
    // Wrap the inserted quote in a <pre> so it won't be wrapped:
    nsAutoString tag("pre");
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
        preElement->SetAttribute("_moz_quote", "true");

      // and set the selection inside it:
      selection->Collapse(preNode, 0);
    }

    rv = InsertText(quotedStuff);

    if (aNodeInserted && NS_SUCCEEDED(rv))
    {
      *aNodeInserted = preNode;
      NS_IF_ADDREF(*aNodeInserted);
    }

    // Set the selection to just after the inserted node:
    if (NS_SUCCEEDED(rv) && preNode)
    {
      nsCOMPtr<nsIDOMNode> parent;
      PRInt32 offset;
      if (NS_SUCCEEDED(GetNodeLocation(preNode, &parent, &offset)) && parent)
        selection->Collapse(parent, offset+1);
    }
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
  nsAutoEditBatch beginBatching(this);
  nsCOMPtr<nsIDOMNode> newNode;
  nsAutoRules beginRulesSniffing(this, kOpInsertQuotation, nsIEditor::eNext);

  // get selection
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  // give rules a chance to handle or cancel
  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kInsertElement);
  PRBool cancel, handled;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(res)) return res;
  if (cancel) return NS_OK; // rules canceled the operation
  if (!handled)
  {
    nsAutoString tag("blockquote");
    res = DeleteSelectionAndCreateNode(tag, getter_AddRefs(newNode));
    if (NS_FAILED(res)) return res;
    if (!newNode) return NS_ERROR_NULL_POINTER;

    // Try to set type=cite.  Ignore it if this fails.
    nsCOMPtr<nsIDOMElement> newElement (do_QueryInterface(newNode));
    if (newElement)
    {
      nsAutoString type ("type");
      nsAutoString cite ("cite");
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

    // Set the selection to just after the inserted node:
    if (NS_SUCCEEDED(res) && newNode)
    {
      nsCOMPtr<nsIDOMNode> parent;
      PRInt32 offset;
      if (NS_SUCCEEDED(GetNodeLocation(newNode, &parent, &offset)) && parent)
        selection->Collapse(parent, offset+1);
    }
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
  { // default processing


    rv = NS_OK;
    
    // special-case for empty document when requesting plain text,
    // to account for the bogus text node
    if (aFormatType.Equals("text/plain"))
    {
      PRBool docEmpty;
      rv = GetDocumentIsEmpty(&docEmpty);
      if (NS_FAILED(rv)) return rv;
      
      if (docEmpty) {
        aOutputString = "";
        return NS_OK;
      }
      else if (mFlags & eEditorPlaintextMask)
        aFlags |= nsIDocumentEncoder::OutputPreformatted;
    }

    nsCOMPtr<nsIDocumentEncoder> encoder;
    char* progid = (char *)nsAllocator::Alloc(strlen(NS_DOC_ENCODER_PROGID_BASE) + aFormatType.Length() + 1);
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
  if (aFormatType.Equals("text/plain"))
  {
    PRBool docEmpty;
    rv = GetDocumentIsEmpty(&docEmpty);
    if (NS_FAILED(rv)) return rv;
    
    if (docEmpty)
       return NS_OK;    // output nothing
  }

  nsCOMPtr<nsIDocumentEncoder> encoder;
  char* progid = (char *)nsAllocator::Alloc(strlen(NS_DOC_ENCODER_PROGID_BASE) + aFormatType.Length() + 1);
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

  if (aCharset && aCharset->Length() != 0 && aCharset->Equals("null")==PR_FALSE)
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
#if 0
NS_IMETHODIMP
nsHTMLEditor::GetTextNearNode(nsIDOMNode *aNode, aNode, PRInt32 aMaxChars, nsString& aOutputString)
{
  if (!aNode)
    return NS_ERROR_NULL_POINTER;

  // Create a temporary selection object and 
  //  based on the suppled
  nsCOMPtr<nsIDOMSelection> selection;
  nsCOMPtr<nsIDOMRange> range;
  nsresult rv = NS_NewRange(getter_AddRsfs(range))
  if (NS_SUCCEEDED(rv))
  {
    if (!range)
      return NS_ERROR_NULL_POINTER;
    range.SetStart(aNode,0);
    range.SetEnd(aNode,      
  }

  if (NS_SUCCEEDED(rv) && selection)
    encoder->SetSelection(selection);

  return rv;
}
#endif

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
  caretP->GetWindowRelativeCoordinates(aReply->mCursorPosition,aReply->mCursorIsCollapsed);

  // second part of 23558 fix:
  if (aCompositionString.IsEmpty()) 
  {
    mIMETextNode = nsnull;
  }
  
  return result;
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
  if (! ((opID==kOpInsertText) || (opID==kOpInsertIMEText)) )
    ClearInlineStylesCache();
  if (mRules) return mRules->BeforeEdit(opID, aDirection);
  return NS_OK;
}


/** All editor operations which alter the doc should be followed
 *  with a call to EndOperation, naming the action and direction */
NS_IMETHODIMP
nsHTMLEditor::EndOperation(PRInt32 opID, nsIEditor::EDirection aDirection)
{
  if (! ((opID==kOpInsertText) || (opID==kOpInsertIMEText)) )
    ClearInlineStylesCache();
  if (mRules) return mRules->AfterEdit(opID, aDirection);
  return NS_OK;
}


PRBool 
nsHTMLEditor::CanContainTag(nsIDOMNode* aParent, const nsString &aTag)
{
  // CNavDTD gives some unwanted results.  We override them here.
  // if parent is a list and tag is text, say "no". 
  if (IsListNode(aParent) && (aTag.Equals("__moz_text")))
    return PR_FALSE;
  // else fall thru
  return nsEditor::CanContainTag(aParent, aTag);
}


NS_IMETHODIMP 
nsHTMLEditor::SelectEntireDocument(nsIDOMSelection *aSelection)
{
  nsresult res;
  if (!aSelection || !mRules) { return NS_ERROR_NULL_POINTER; }
  
  // get body node
  nsCOMPtr<nsIDOMElement>bodyElement;
  res = GetBodyElement(getter_AddRefs(bodyElement));
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


NS_IMETHODIMP
nsHTMLEditor::SetTypeInStateForProperty(TypeInState    &aTypeInState, 
                                        nsIAtom        *aPropName, 
                                        const nsString *aAttribute,
                                        const nsString *aValue)
{
  if (!aPropName) {
    return NS_ERROR_NULL_POINTER;
  }
  PRUint32 propEnum;
  aTypeInState.GetEnumForName(aPropName, propEnum);
  if (nsIEditProperty::b==aPropName || nsIEditProperty::i==aPropName || nsIEditProperty::u==aPropName) 
  {
    if (aTypeInState.IsSet(propEnum))
    { // toggle currently set boldness
      aTypeInState.UnSet(propEnum);
    }
    else
    { // get the current style and set boldness to the opposite of the current state
      PRBool any = PR_FALSE;
      PRBool all = PR_FALSE;
      PRBool first = PR_FALSE;
      GetInlineProperty(aPropName, aAttribute, nsnull, first, any, all); // operates on current selection
      aTypeInState.SetProp(propEnum, !any);
    }
  }
  else if (nsIEditProperty::font==aPropName)
  {
    if (!aAttribute) { return NS_ERROR_NULL_POINTER; }
    nsIAtom *attribute = NS_NewAtom(*aAttribute);
    if (!attribute) { return NS_ERROR_NULL_POINTER; }
    PRUint32 attrEnum;
    aTypeInState.GetEnumForName(attribute, attrEnum);
    if (nsIEditProperty::color==attribute || nsIEditProperty::face==attribute || nsIEditProperty::size==attribute)
    {
      if (aTypeInState.IsSet(attrEnum))
      { 
        if (nsnull==aValue) {
          aTypeInState.UnSet(attrEnum);
        }
        else { // we're just changing the value of color
          aTypeInState.SetPropValue(attrEnum, *aValue);
        }
      }
      else
      { // get the current style and set font color if it's needed
        if (!aValue) { return NS_ERROR_NULL_POINTER; }
        PRBool any = PR_FALSE;
        PRBool all = PR_FALSE;
        PRBool first = PR_FALSE;
        GetInlineProperty(aPropName, aAttribute, aValue, first, any, all); // operates on current selection
        if (!all) {
          aTypeInState.SetPropValue(attrEnum, *aValue);
        }
      }    
    }
    else { return NS_ERROR_FAILURE; }
  }
  else { return NS_ERROR_FAILURE; }
  return NS_OK;
}


// this will NOT find aAttribute unless aAttribute has a non-null value
// so singleton attributes like <Table border> will not be matched!
void nsHTMLEditor::IsTextPropertySetByContent(nsIDOMNode     *aNode,
                                              nsIAtom        *aProperty, 
                                              const nsString *aAttribute, 
                                              const nsString *aValue, 
                                              PRBool         &aIsSet,
                                              nsIDOMNode    **aStyleNode) const
{
  nsresult result;
  aIsSet = PR_FALSE;  // must be initialized to false for code below to work
  nsAutoString propName;
  aProperty->ToString(propName);
  nsCOMPtr<nsIDOMNode>parent;
  result = aNode->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(result)) return;
  while (parent)
  {
    nsCOMPtr<nsIDOMElement>element;
    element = do_QueryInterface(parent);
    if (element)
    {
      nsAutoString tag;
      element->GetTagName(tag);
      if (propName.EqualsIgnoreCase(tag))
      {
        PRBool found = PR_FALSE;
        if (aAttribute && 0!=aAttribute->Length())
        {
          nsAutoString value;
          element->GetAttribute(*aAttribute, value);
          if (0!=value.Length())
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
    result = parent->GetParentNode(getter_AddRefs(temp));
    if (NS_SUCCEEDED(result) && temp) {
      parent = do_QueryInterface(temp);
    }
    else {
      parent = do_QueryInterface(nsnull);
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


NS_IMETHODIMP
nsHTMLEditor::GetTextSelectionOffsetsForRange(nsIDOMSelection *aSelection,
                                              nsIDOMNode **aParent,
                                              PRInt32     &aOutStartOffset, 
                                              PRInt32     &aEndOffset)
{
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  aOutStartOffset = aEndOffset = 0;
  nsresult result;

  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset, endOffset;
  aSelection->GetAnchorNode(getter_AddRefs(startNode));
  aSelection->GetAnchorOffset(&startOffset);
  aSelection->GetFocusNode(getter_AddRefs(endNode));
  aSelection->GetFocusOffset(&endOffset);

  nsCOMPtr<nsIEnumerator> enumerator;
  result = aSelection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(result)) return result;
  if (!enumerator) return NS_ERROR_NULL_POINTER;
  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  result = enumerator->CurrentItem(getter_AddRefs(currentItem));
  if ((NS_SUCCEEDED(result)) && currentItem)
  {
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    range->GetCommonParent(aParent);
  }

  nsCOMPtr<nsIContentIterator> iter;
  result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                              NS_GET_IID(nsIContentIterator), 
                                              getter_AddRefs(iter));
  if (NS_FAILED(result)) return result;
  if (!iter) return NS_ERROR_NULL_POINTER;

  PRUint32 totalLength=0;
  nsCOMPtr<nsIDOMCharacterData>textNode;
  nsCOMPtr<nsIContent>blockParentContent = do_QueryInterface(*aParent);
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
      if (currentNode.get() == startNode.get())
      {
        aOutStartOffset = totalLength + startOffset;
      }
      if (currentNode.get() == endNode.get())
      {
        aEndOffset = totalLength + endOffset;
        break;
      }
      PRUint32 length;
      textNode->GetLength(&length);
      totalLength += length;
    }
    iter->Next();
    iter->CurrentNode(getter_AddRefs(content));
  }
  return result;
}

void nsHTMLEditor::ResetTextSelectionForRange(nsIDOMNode *aParent,
                                              PRInt32     aOutStartOffset,
                                              PRInt32     aEndOffset,
                                              nsIDOMSelection *aSelection)
{
  if (!aParent || !aSelection) { return; }  // XXX: should return an error
  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset=0, endOffset=0;

  nsresult result;
  nsCOMPtr<nsIContentIterator> iter;
  result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                              NS_GET_IID(nsIContentIterator), 
                                              getter_AddRefs(iter));
  if (NS_FAILED(result)) return;
  if (!iter) return;
  PRBool setStart = PR_FALSE;
  PRUint32 totalLength=0;
  nsCOMPtr<nsIDOMCharacterData>textNode;
  nsCOMPtr<nsIContent>blockParentContent = do_QueryInterface(aParent);
  iter->Init(blockParentContent);
  // loop through the content iterator for each content node
  nsCOMPtr<nsIContent> content;
  result = iter->CurrentNode(getter_AddRefs(content));
  while (NS_ENUMERATOR_FALSE == iter->IsDone())
  {
    textNode = do_QueryInterface(content);
    if (textNode)
    {
      PRUint32 length;
      textNode->GetLength(&length);
      if ((!setStart) && aOutStartOffset<=(PRInt32)(totalLength+length))
      {
        setStart = PR_TRUE;
        startNode = do_QueryInterface(textNode);
        startOffset = aOutStartOffset-totalLength;
      }
      if (aEndOffset<=(PRInt32)(totalLength+length))
      {
        endNode = do_QueryInterface(textNode);
        endOffset = aEndOffset-totalLength;
        break;
      }        
      totalLength += length;
    }
    iter->Next();
    iter->CurrentNode(getter_AddRefs(content));
  }
  aSelection->Collapse(startNode, startOffset);
  aSelection->Extend(endNode, endOffset);
}

#ifdef XP_MAC
#pragma mark -
#endif

//================================================================
// HTML Editor methods
//
// Note: Table Editing methods are implemented in nsTableEditor.cpp
//

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
    nsCRT::free(tag);
  }
  // find the current block parent, or just use aNode if it is a block node
  nsCOMPtr<nsIDOMElement>blockParentElement;
  nsCOMPtr<nsIDOMNode>nodeToReParent; // this is the node we'll operate on, by default it's aNode
  nsresult res = aNode->QueryInterface(NS_GET_IID(nsIDOMNode), getter_AddRefs(nodeToReParent));
  PRBool nodeIsInline;
  PRBool nodeIsBlock=PR_FALSE;
  IsNodeInline(aNode, nodeIsInline);
  if (!nodeIsInline)
  {
    nsresult QIResult;
    nsCOMPtr<nsIDOMCharacterData>nodeAsText;
    QIResult = aNode->QueryInterface(NS_GET_IID(nsIDOMCharacterData), getter_AddRefs(nodeAsText));
    if (NS_FAILED(QIResult) || !nodeAsText) {
      nodeIsBlock=PR_TRUE;
    }
  }
  // if aNode is the block parent, then the node to reparent is one of its children
  if (nodeIsBlock) 
  {
    res = aNode->QueryInterface(NS_GET_IID(nsIDOMNode), getter_AddRefs(blockParentElement));
    if (NS_SUCCEEDED(res) && blockParentElement) {
      res = aNode->GetFirstChild(getter_AddRefs(nodeToReParent));
    }
  }
  else { // we just need to get the block parent of aNode
    res = GetBlockParent(aNode, getter_AddRefs(blockParentElement));
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
    if (isRoot)    
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
        res = CreateNode(aParentTag, blockParentNode, offsetInParent, 
                                          getter_AddRefs(newParentNode));
        // XXX: need to move some of the children of blockParentNode into the newParentNode?
        // XXX: need to create a bogus text node inside this new block?
        //      that means, I need to generalize bogus node handling
      }
    }
    else
    { // the block parent is not a ROOT, 
      // for the selected block content, transform blockParentNode 
      if (((eReplaceParent==aTransformation) && (!parentTag.EqualsIgnoreCase(aParentTag))) ||
           (eInsertParent==aTransformation))
      {
        if (gNoisy) { DebugDumpContent(); } // DEBUG
        res = ReParentBlockContent(nodeToReParent, aParentTag, blockParentNode, parentTag, 
                                      aTransformation, getter_AddRefs(newParentNode));
        if ((NS_SUCCEEDED(res)) && (newParentNode) && (eReplaceParent==aTransformation))
        {
          PRBool hasChildren;
          blockParentNode->HasChildNodes(&hasChildren);
          if (!hasChildren)
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
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsIDOMNode>previousAncestor = do_QueryInterface(aNode);
  while (ancestor)
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
    if (NS_FAILED(res)) return res;
  }
  // now, previousAncestor is the node we are operating on
  nsCOMPtr<nsIDOMNode>leftNode, rightNode;
  res = GetBlockSection(previousAncestor, 
                        getter_AddRefs(leftNode), 
                        getter_AddRefs(rightNode));
  if (NS_FAILED(res)) return res;
  if (!leftNode || !rightNode) return NS_ERROR_NULL_POINTER;

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

  if (isRootBlock)
  { // we're creating a block element where a block element did not previously exist 
    removeBreakBefore = PR_TRUE;
    removeBreakAfter = PR_TRUE;
  }

  // apply the transformation
  PRInt32 offsetInParent=0;
  if (eInsertParent==aTransformation || isRootBlock)
  {
    res = GetChildOffset(leftNode, blockParentNode, offsetInParent);
    NS_ASSERTION((NS_SUCCEEDED(res)), "bad res from GetChildOffset");
    res = CreateNode(aParentTag, blockParentNode, offsetInParent, aNewParentNode);
    if (gNoisy) { printf("created a node in blockParentNode at offset %d\n", offsetInParent); }
  }
  else
  {
    nsCOMPtr<nsIDOMNode> grandParent;
    res = blockParentNode->GetParentNode(getter_AddRefs(grandParent));
    if (NS_FAILED(res)) return res;
    if (!grandParent) return NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIDOMNode>firstChildNode, lastChildNode;
    blockParentNode->GetFirstChild(getter_AddRefs(firstChildNode));
    blockParentNode->GetLastChild(getter_AddRefs(lastChildNode));
    if (firstChildNode==leftNode && lastChildNode==rightNode)
    {
      res = GetChildOffset(blockParentNode, grandParent, offsetInParent);
      NS_ASSERTION((NS_SUCCEEDED(res)), "bad res from GetChildOffset");
      res = CreateNode(aParentTag, grandParent, offsetInParent, aNewParentNode);
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
      res = CreateNode(aParentTag, blockParentNode, offsetInParent, aNewParentNode);
      if (gNoisy) { printf("created a node in blockParentNode at offset %d\n", offsetInParent); }
      // what we need to do here is remove the existing block parent when we're all done.
      removeBlockParent = PR_TRUE;
    }
  }
  if ((NS_SUCCEEDED(res)) && *aNewParentNode)
  { // move all the children/contents of blockParentNode to aNewParentNode
    nsCOMPtr<nsIDOMNode>childNode = do_QueryInterface(rightNode);
    nsCOMPtr<nsIDOMNode>previousSiblingNode;
    while (NS_SUCCEEDED(res) && childNode)
    {
      childNode->GetPreviousSibling(getter_AddRefs(previousSiblingNode));
      res = nsEditor::MoveNode(childNode, *aNewParentNode, 0);
      if (gNoisy) 
      {
        printf("re-parented sibling node %p\n", childNode.get());
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
  if ((NS_SUCCEEDED(res)) && (removeBlockParent))
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
          res = MoveNode(childNode, grandParent, offsetInParent);
        }
      }
      if (gNoisy) { printf("removing the old block parent %p\n", blockParentNode.get()); }
      res = DeleteNode(blockParentNode);
    }
  }
  return res;
}

/*
NS_IMETHODIMP 
nsHTMLEditor::ReParentContentOfRange(nsIDOMRange *aRange, 
                                     nsString &aParentTag,
                                     BlockTransformationType aTranformation)
{
  if (!aRange) { return NS_ERROR_NULL_POINTER; }
  nsresult res;
  nsISupportsArray *blockSections;
  res = NS_NewISupportsArray(&blockSections);
  if (NS_FAILED(res)) return res;
  if (!blockSections) return NS_ERROR_NULL_POINTER;

  res = GetBlockSectionsForRange(aRange, blockSections);
  if (NS_FAILED(res)) return res;

  // no embedded returns allowed below here in this method, or you'll get a space leak
  nsIDOMRange *subRange;
  subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
  while (subRange)
  {
    nsCOMPtr<nsIDOMNode>startParent;
    res = subRange->GetStartParent(getter_AddRefs(startParent));
    if (NS_SUCCEEDED(res) && startParent) 
    {
      if (gNoisy) { printf("ReParentContentOfRange calling ReParentContentOfNode\n"); }
      res = ReParentContentOfNode(startParent, aParentTag, aTranformation);
    }
    NS_RELEASE(subRange);
    if (NS_FAILED(res)) 
    { // be sure to break after free of subRange
      break; 
    }
    blockSections->RemoveElementAt(0);
    subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
  }
  NS_RELEASE(blockSections);

  return res;
}
*/

NS_IMETHODIMP 
nsHTMLEditor::RemoveParagraphStyleFromRange(nsIDOMRange *aRange)
{
  if (!aRange) { return NS_ERROR_NULL_POINTER; }
  nsresult res;
  nsISupportsArray *blockSections;
  res = NS_NewISupportsArray(&blockSections);
  if (NS_FAILED(res)) return res;
  if (!blockSections) return NS_ERROR_NULL_POINTER;

  res = GetBlockSectionsForRange(aRange, blockSections);
  if (NS_FAILED(res)) return res;

  nsIDOMRange *subRange;
  subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
  while (subRange)
  {
    res = RemoveParagraphStyleFromBlockContent(subRange);
    NS_RELEASE(subRange);
    if (NS_FAILED(res))
    { // be sure to break after subrange is released
      break;
    }
    blockSections->RemoveElementAt(0);
    subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
  }
  NS_RELEASE(blockSections);
  
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
  res = GetBlockParent(startParent, getter_AddRefs(blockParentElement));
  if (NS_FAILED(res)) return res;

  while (blockParentElement)
  {
    nsAutoString blockParentTag;
    blockParentElement->GetTagName(blockParentTag);
    PRBool isSubordinateBlock;
    IsSubordinateBlock(blockParentTag, isSubordinateBlock);
    if (!isSubordinateBlock) {
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
        if (NS_FAILED(res)) return res;
        if (!grandParent) return NS_ERROR_NULL_POINTER;

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
            res = MoveNode(childNode, grandParent, offsetInParent);
            if (NS_FAILED(res)) return res;
          }
        }
        if (NS_SUCCEEDED(res)) 
        {
          res = DeleteNode(blockParentElement);
          if (NS_FAILED(res)) return res;
        }
      }
    }
    res = GetBlockParent(startParent, getter_AddRefs(blockParentElement));
    if (NS_FAILED(res)) return res;
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
  if (NS_FAILED(res)) return res;
  if (!blockSections) return NS_ERROR_NULL_POINTER;
  res = GetBlockSectionsForRange(aRange, blockSections);
  if (NS_SUCCEEDED(res)) 
  {
    nsIDOMRange *subRange;
    subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
    while (subRange && (NS_SUCCEEDED(res)))
    {
      res = RemoveParentFromBlockContent(aParentTag, subRange);
      NS_RELEASE(subRange);
      if (NS_FAILED(res))
      {
        break;
      }
      blockSections->RemoveElementAt(0);
      subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
    }
  }
  NS_RELEASE(blockSections);

  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::RemoveParentFromBlockContent(const nsString &aParentTag, nsIDOMRange *aRange)
{
  if (!aRange) { return NS_ERROR_NULL_POINTER; }
  nsresult res;
  nsCOMPtr<nsIDOMNode>startParent;
  res = aRange->GetStartParent(getter_AddRefs(startParent));
  if (NS_FAILED(res)) return res;
  if (!startParent) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode>parentNode;
  nsCOMPtr<nsIDOMElement>parentElement;
  res = startParent->GetParentNode(getter_AddRefs(parentNode));
  if (NS_FAILED(res)) return res;

  while (parentNode)
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
      if (NS_FAILED(res)) return res;
      if (!childNodes) return NS_ERROR_NULL_POINTER;

      nsCOMPtr<nsIDOMNode>grandParent;
      parentElement->GetParentNode(getter_AddRefs(grandParent));
      PRInt32 offsetInParent;
      res = GetChildOffset(parentElement, grandParent, offsetInParent);
      if (NS_FAILED(res)) return res;

      PRUint32 childCount;
      childNodes->GetLength(&childCount);
      PRInt32 i=childCount-1;
      for ( ; ((NS_SUCCEEDED(res)) && (0<=i)); i--)
      {
        nsCOMPtr<nsIDOMNode> childNode;
        res = childNodes->Item(i, getter_AddRefs(childNode));
        if (NS_FAILED(res)) return res;
        if (!childNode) return NS_ERROR_NULL_POINTER;
        res = MoveNode(childNode, grandParent, offsetInParent);
        if (NS_FAILED(res)) return res;
      }
      res = DeleteNode(parentElement);
      if (NS_FAILED(res)) { return res; }

      break;
    }
    else if (isRoot) {  // hit a local root node, terminate loop
      break;
    }
    res = parentElement->GetParentNode(getter_AddRefs(parentNode));
    if (NS_FAILED(res)) return res;
  }
  return res;
}



PRBool nsHTMLEditor::IsElementInBody(nsIDOMElement* aElement)
{
  if ( aElement )
  {
    nsIDOMElement* bodyElement = nsnull;
    nsresult res = nsEditor::GetBodyElement(&bodyElement);
    if (NS_FAILED(res)) return res;
    if (!bodyElement) return NS_ERROR_NULL_POINTER;
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
  return PR_FALSE;
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
# if 0 
// I've ifdef'd this out because it isn't finished and I'm not sure what the intent is.
        PRInt32 offset = 0;
        nsCOMPtr<nsIDOMNode>lastChild;
        res = parent->GetLastChild(getter_AddRefs(lastChild));
        if (NS_SUCCEEDED(res) && lastChild && node != lastChild)
        {
          if (node == lastChild)
          {
            // Check if node is text and has more than just a &nbsp
            nsCOMPtr<nsIDOMCharacterData>textNode = do_QueryInterface(node);
            nsAutoString text;
            PRUnichar nbspStr[2] = {nbsp, 0};
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
#endif
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
// IsTable: true if node an html table
//                  
PRBool 
nsHTMLEditor::IsTable(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null node passed to nsHTMLEditor::IsTable");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  if (tag.Equals("table"))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsTableCell: true if node an html td
//                  
PRBool 
nsHTMLEditor::IsTableCell(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null node passed to nsHTMLEditor::IsTableCell");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  if (tag.Equals("td") || tag.Equals("th"))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsTableElement: true if node an html table, td, tr, ...
//                  
PRBool 
nsHTMLEditor::IsTableElement(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null node passed to nsHTMLEditor::IsTableElement");
  nsAutoString tagName;
  nsEditor::GetTagString(node,tagName);
  if (tagName.Equals("table") || tagName.Equals("tr") || 
      tagName.Equals("td")    || tagName.Equals("th") ||
      tagName.Equals("thead") || tagName.Equals("tfoot") ||
      tagName.Equals("tbody") || tagName.Equals("caption"))
  {
    return PR_TRUE;
  }
  return PR_FALSE;
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
    if (IsTable(tmp)) tbl = tmp;
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


TypeInState * nsHTMLEditor::GetTypeInState()
{
  if (mTypeInState) {
    NS_ADDREF(mTypeInState);
  }
  return mTypeInState;
}


NS_IMETHODIMP
nsHTMLEditor::SetTextPropertiesForNode(nsIDOMNode  *aNode, 
                                       nsIDOMNode  *aParent,
                                       PRInt32      aStartOffset,
                                       PRInt32      aEndOffset,
                                       nsIAtom     *aPropName, 
                                       const nsString *aAttribute,
                                       const nsString *aValue)
{
  if (gNoisy) { printf("nsTextEditor::SetTextPropertyForNode\n"); }
  nsresult result=NS_OK;
  PRBool textPropertySet;
  nsCOMPtr<nsIDOMNode>resultNode;
  IsTextPropertySetByContent(aNode, aPropName, aAttribute, aValue, textPropertySet, getter_AddRefs(resultNode));
  if (!textPropertySet)
  {
    if (aValue  &&  0!=aValue->Length())
    {
      result = RemoveTextPropertiesForNode(aNode, aParent, aStartOffset, aEndOffset, aPropName, aAttribute);
    }
    nsAutoString tag;
    aPropName->ToString(tag);
    if (NS_SUCCEEDED(result)) 
    {
      nsCOMPtr<nsIDOMNode>newStyleNode;
      result = nsEditor::CreateNode(tag, aParent, 0, getter_AddRefs(newStyleNode));
      if (NS_FAILED(result)) return result;
      if (!newStyleNode) return NS_ERROR_NULL_POINTER;

      nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
      nodeAsChar =  do_QueryInterface(aNode);
      if (nodeAsChar)
      {
        result = MoveContentOfNodeIntoNewParent(aNode, newStyleNode, aStartOffset, aEndOffset);
      }
      else
      { // handle non-text selection
        nsCOMPtr<nsIDOMNode> parent;  // used just to make the code easier to understand
        nsCOMPtr<nsIDOMNode> child;
        parent = do_QueryInterface(aNode);
        child = GetChildAt(parent, aStartOffset);
        // XXX: need to loop for aStartOffset!=aEndOffset-1?
        PRInt32 offsetInParent = aStartOffset; // remember where aNode was in aParent
        if (NS_SUCCEEDED(result) && child)
        { // move child
          result = nsEditor::MoveNode(child, newStyleNode, 0);
          if (NS_SUCCEEDED(result)) 
          { // put newStyleNode in parent where child was
            result = nsEditor::InsertNode(newStyleNode, parent, offsetInParent);
          }
        }
      }
      if (NS_SUCCEEDED(result)) 
      {
        if (aAttribute && 0!=aAttribute->Length())
        {
          nsCOMPtr<nsIDOMElement> newStyleElement;
          newStyleElement = do_QueryInterface(newStyleNode);
          nsAutoString value;
          if (aValue) {
            value = *aValue;
          }
          // XXX should be a call to editor to change attribute!
          result = newStyleElement->SetAttribute(*aAttribute, value);
        }
      }
    }
  }
  if (gNoisy) { printf("SetTextPropertyForNode returning %d, dumping content...\n", result);}
  if (gNoisy) {DebugDumpContent(); } // DEBUG
  return result;
}

NS_IMETHODIMP nsHTMLEditor::MoveContentOfNodeIntoNewParent(nsIDOMNode  *aNode, 
                                                           nsIDOMNode  *aNewParentNode, 
                                                           PRInt32      aStartOffset, 
                                                           PRInt32      aEndOffset)
{
  if (!aNode || !aNewParentNode) { return NS_ERROR_NULL_POINTER; }
  if (gNoisy) { printf("nsTextEditor::MoveContentOfNodeIntoNewParent\n"); }
  nsresult result=NS_OK;

  PRUint32 count;
  result = GetLengthOfDOMNode(aNode, count);

  if (NS_SUCCEEDED(result))
  {
    nsCOMPtr<nsIDOMNode>newChildNode;  // this will be the child node we move into the new style node
    // split the node at the start offset unless the split would create an empty node
    if (aStartOffset!=0)
    {
      result = nsEditor::SplitNode(aNode, aStartOffset, getter_AddRefs(newChildNode));
      if (gNoisy) { printf("* split created left node %p\n", newChildNode.get());}
      if (gNoisy) {DebugDumpContent(); } // DEBUG
    }
    if (NS_SUCCEEDED(result))
    {
      if (aEndOffset!=(PRInt32)count)
      {
        result = nsEditor::SplitNode(aNode, aEndOffset-aStartOffset, getter_AddRefs(newChildNode));
        if (gNoisy) { printf("* split created left node %p\n", newChildNode.get());}
        if (gNoisy) {DebugDumpContent(); } // DEBUG
      }
      else
      {
        newChildNode = do_QueryInterface(aNode);
        if (gNoisy) { printf("* second split not required, new text node set to aNode = %p\n", newChildNode.get());}
      }
      if (NS_SUCCEEDED(result))
      {
        // move aNewParentNode into the right location

        // optimization:  if all we're doing is changing a value for an existing attribute for the
        //                entire selection, then just twiddle the existing style node
        PRBool done = PR_FALSE; // set to true in optimized case if we can really do the optimization
        /*
        if (aAttribute && aValue && (0==aStartOffset) && (aEndOffset==(PRInt32)count))
        {
          // ??? can we really compute this?
        }
        */
        if (!done)
        {
          // if we've ended up with an empty text node, just delete it and we're done
          nsCOMPtr<nsIDOMCharacterData>newChildNodeAsChar;
          newChildNodeAsChar =  do_QueryInterface(newChildNode);
          PRUint32 newChildNodeLength;
          if (newChildNodeAsChar)
          {
            newChildNodeAsChar->GetLength(&newChildNodeLength);
            if (0==newChildNodeLength)
            {
              result = nsEditor::DeleteNode(newChildNode); 
              done = PR_TRUE;
              // XXX: need to set selection here
            }
          }
          // move the new child node into the new parent
          if (!done)
          {
            // first, move the new parent into the correct location
            PRInt32 offsetInParent;
            nsCOMPtr<nsIDOMNode>parentNode;
            result = aNode->GetParentNode(getter_AddRefs(parentNode));
            if (NS_SUCCEEDED(result))
            {
              result = nsEditor::DeleteNode(aNewParentNode);
              if (NS_SUCCEEDED(result))
              { // must get child offset AFTER delete of aNewParentNode!
                result = GetChildOffset(aNode, parentNode, offsetInParent);
                if (NS_SUCCEEDED(result))
                {
                  result = nsEditor::InsertNode(aNewParentNode, parentNode, offsetInParent);
                  if (NS_SUCCEEDED(result))
                  {
                    // then move the new child into the new parent node
                    result = nsEditor::MoveNode(newChildNode, aNewParentNode, 0);
                  }
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

/* this should only get called if the only intervening nodes are inline style nodes */
NS_IMETHODIMP 
nsHTMLEditor::SetTextPropertiesForNodesWithSameParent(nsIDOMNode  *aStartNode,
                                                      PRInt32      aStartOffset,
                                                      nsIDOMNode  *aEndNode,
                                                      PRInt32      aEndOffset,
                                                      nsIDOMNode  *aParent,
                                                      nsIAtom     *aPropName,
                                                      const nsString *aAttribute,
                                                      const nsString *aValue)
{
  if (gNoisy) { printf("- start nsTextEditor::SetTextPropertiesForNodesWithSameParent -\n"); }
  nsresult result=NS_OK;
  PRBool textPropertySet;
  nsCOMPtr<nsIDOMNode>resultNode;
  IsTextPropertySetByContent(aStartNode, aPropName, aAttribute, aValue, textPropertySet, getter_AddRefs(resultNode));
  if (!textPropertySet)
  {
    result = RemoveTextPropertiesForNodeWithDifferentParents(aStartNode, aStartOffset,
                                                             aEndNode,   aEndOffset,
                                                             aParent,    aPropName, aAttribute);
    if (NS_FAILED(result)) return result;

    nsAutoString tag;
    aPropName->ToString(tag);
    // create the new style node, which will be the new parent for the selected nodes
    nsCOMPtr<nsIDOMNode>newStyleNode;
    nsCOMPtr<nsIDOMDocument>doc;
    result = GetDocument(getter_AddRefs(doc));
    if (NS_FAILED(result)) return result;
    if (!doc) return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIDOMElement>newElement;
    result = doc->CreateElement(tag, getter_AddRefs(newElement));
    if (NS_FAILED(result)) return result;
    if (!newElement) return NS_ERROR_NULL_POINTER;

    newStyleNode = do_QueryInterface(newElement);
    if (!newStyleNode) return NS_ERROR_NULL_POINTER;
    result = MoveContiguousContentIntoNewParent(aStartNode, aStartOffset, aEndNode, aEndOffset, aParent, newStyleNode);
    if (NS_SUCCEEDED(result) && aAttribute && 0!=aAttribute->Length())
    {
      nsCOMPtr<nsIDOMElement> newStyleElement;
      newStyleElement = do_QueryInterface(newStyleNode);
      nsAutoString value;
      if (aValue) {
        value = *aValue;
      }
      result = newStyleElement->SetAttribute(*aAttribute, value);
    }
  }
  if (gNoisy) { printf("---------- end nsTextEditor::SetTextPropertiesForNodesWithSameParent ----------\n"); }
  return result;
}

NS_IMETHODIMP
nsHTMLEditor::MoveContiguousContentIntoNewParent(nsIDOMNode  *aStartNode, 
                                                 PRInt32      aStartOffset, 
                                                 nsIDOMNode  *aEndNode,
                                                 PRInt32      aEndOffset,
                                                 nsIDOMNode  *aGrandParentNode,
                                                 nsIDOMNode  *aNewParentNode)
{
  if (!aStartNode || !aEndNode || !aNewParentNode) { return NS_ERROR_NULL_POINTER; }
  if (gNoisy) { printf("--- start nsTextEditor::MoveContiguousContentIntoNewParent ---\n"); }
  if (gNoisy) {DebugDumpContent(); } // DEBUG

  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode>startNode, endNode;
  PRInt32 startOffset = aStartOffset; // this will be the left edge of what we change
  nsCOMPtr<nsIDOMNode>newLeftNode;  // this will be the middle text node
  if (IsTextNode(aStartNode))
  {
    startOffset = 0;
    if (gNoisy) { printf("aStartNode is a text node.\n"); }
    startNode = do_QueryInterface(aStartNode);
    if (0!=aStartOffset) 
    {
      result = nsEditor::SplitNode(aStartNode, aStartOffset, getter_AddRefs(newLeftNode));
      if (gNoisy) { printf("split aStartNode, newLeftNode = %p\n", newLeftNode.get()); }
      if (gNoisy) {DebugDumpContent(); } // DEBUG
    }
    else { 
      if (gNoisy) { printf("did not split aStartNode\n"); } 
    }
  }
  else {
    startNode = GetChildAt(aStartNode, aStartOffset);
    if (gNoisy) { printf("aStartNode is not a text node, got startNode = %p.\n", startNode.get()); }
  }
  if (NS_SUCCEEDED(result))
  {
    nsCOMPtr<nsIDOMNode>newRightNode;  // this will be the middle text node
    if (IsTextNode(aEndNode))
    {
      if (gNoisy) { printf("aEndNode is a text node.\n"); }
      endNode = do_QueryInterface(aEndNode);
      PRUint32 count;
      GetLengthOfDOMNode(aEndNode, count);
      if ((PRInt32)count!=aEndOffset) 
      {
        result = nsEditor::SplitNode(aEndNode, aEndOffset, getter_AddRefs(newRightNode));
        if (gNoisy) { printf("split aEndNode, newRightNode = %p\n", newRightNode.get()); }
        if (gNoisy) {DebugDumpContent(); } // DEBUG
      }
      else {
        newRightNode = do_QueryInterface(aEndNode);
        if (gNoisy) { printf("did not split aEndNode\n"); }
      }
    }
    else
    {
      endNode = GetChildAt(aEndNode, aEndOffset-1);
      newRightNode = do_QueryInterface(endNode);
      if (gNoisy) { printf("aEndNode is not a text node, got endNode = %p.\n", endNode.get()); }
    }
    if (NS_SUCCEEDED(result))
    {
      PRInt32 offsetInParent;
      result = GetChildOffset(startNode, aGrandParentNode, offsetInParent);
      /*
      if (newLeftNode) {
        result = GetChildOffset(newLeftNode, aGrandParentNode, offsetInParent);
      }
      else {
        offsetInParent = 0; // relies on +1 below in call to CreateNode
      }
      */
      if (NS_SUCCEEDED(result))
      {
        // insert aNewParentNode into aGrandParentNode
        result = nsEditor::InsertNode(aNewParentNode, aGrandParentNode, offsetInParent);
        if (gNoisy) printf("just after InsertNode 1\n");
        if (gNoisy) {DebugDumpContent(); } // DEBUG
        if (NS_SUCCEEDED(result))
        { // move the right half of the start node into the new parent node
          nsCOMPtr<nsIDOMNode>intermediateNode;
          result = startNode->GetNextSibling(getter_AddRefs(intermediateNode));
          if (NS_SUCCEEDED(result))
          {
            PRInt32 childIndex=0;
            result = nsEditor::MoveNode(startNode, aNewParentNode, childIndex);
            if (gNoisy) printf("just after MoveNode 2\n");
            if (gNoisy) {DebugDumpContent(); } // DEBUG
            childIndex++;
            if (NS_SUCCEEDED(result))
            { // move all the intermediate nodes into the new parent node
              nsCOMPtr<nsIDOMNode>nextSibling;
              while (intermediateNode.get() != endNode.get())
              {
                if (!intermediateNode)
                  result = NS_ERROR_NULL_POINTER;
                if (NS_FAILED(result)) 
                {
                  break;
                }
                // get the next sibling before moving the current child!!!
                intermediateNode->GetNextSibling(getter_AddRefs(nextSibling));
                result = nsEditor::MoveNode(intermediateNode, aNewParentNode, childIndex);
                if (gNoisy) printf("just after MoveNode 4\n");
                if (gNoisy) {DebugDumpContent(); } // DEBUG
                childIndex++;
              }
              intermediateNode = do_QueryInterface(nextSibling);
            }
            if (NS_SUCCEEDED(result))
            { // move the left half of the end node into the new parent node
              result = nsEditor::MoveNode(newRightNode, aNewParentNode, childIndex);
              if (gNoisy) printf("just after MoveNode 5\n");
              if (gNoisy) {DebugDumpContent(); } // DEBUG
            }
          }
        }
      }
    }
  }
    if (gNoisy) { printf(" end nsTextEditor::MoveContiguousContentIntoNewParent \n"); }
    if (gNoisy) {DebugDumpContent(); } // DEBUG
  return result;
}

NS_IMETHODIMP nsHTMLEditor::IsLeafThatTakesInlineStyle(const nsString *aTag,
                                                       PRBool &aResult)
{
  if (!aTag) { return NS_ERROR_NULL_POINTER; }

  aResult = (PRBool)((!aTag->EqualsIgnoreCase("br")) &&
                     (!aTag->EqualsIgnoreCase("hr")) );
  return NS_OK;
}

/* this wraps every selected text node in a new inline style node if needed
   the text nodes are treated as being unique -- each needs it's own style node 
   if the style is not already present.
   each action has immediate effect on the content tree and resolved style, so
   doing outermost text nodes first removes the need for interior style nodes in some cases.
   XXX: need to code test to see if new style node is needed
*/
NS_IMETHODIMP
nsHTMLEditor::SetTextPropertiesForNodeWithDifferentParents(nsIDOMRange *aRange,
                                                           nsIDOMNode  *aStartNode,
                                                           PRInt32      aStartOffset,
                                                           nsIDOMNode  *aEndNode,
                                                           PRInt32      aEndOffset,
                                                           nsIDOMNode  *aParent,
                                                           nsIAtom     *aPropName,
                                                           const nsString *aAttribute,
                                                           const nsString *aValue)
{
  if (gNoisy) { printf("start nsTextEditor::SetTextPropertiesForNodeWithDifferentParents\n"); }
  nsresult result=NS_OK;
  PRUint32 count;
  if (!aRange || !aStartNode || !aEndNode || !aParent || !aPropName)
    return NS_ERROR_NULL_POINTER;

  PRInt32 startOffset, endOffset;
  // create a style node for the text in the start parent
  nsCOMPtr<nsIDOMNode>parent;

  result = RemoveTextPropertiesForNodeWithDifferentParents(aStartNode,aStartOffset, 
                                                           aEndNode,  aEndOffset, 
                                                           aParent,   aPropName, aAttribute);
  if (NS_FAILED(result)) { return result; }

  // RemoveTextProperties... might have changed selection endpoints, get new ones
  nsCOMPtr<nsIDOMSelection>selection;
  result = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) { return result; }
  if (!selection) { return NS_ERROR_NULL_POINTER; } 
  selection->GetAnchorOffset(&aStartOffset);
  selection->GetFocusOffset(&aEndOffset);

  // create new parent nodes for all the content between the start and end nodes
  nsCOMPtr<nsIContentIterator>iter;
  result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                              NS_GET_IID(nsIContentIterator), getter_AddRefs(iter));
  if (NS_FAILED(result)) return result;
  if (!iter) return NS_ERROR_NULL_POINTER;

  // find our starting point
  PRBool startIsText = IsTextNode(aStartNode);
  nsCOMPtr<nsIContent>startContent;
  if (startIsText) {
    startContent = do_QueryInterface(aStartNode);
  }
  else {
    nsCOMPtr<nsIDOMNode>node = GetChildAt(aStartNode, aStartOffset);
    startContent = do_QueryInterface(node);
  }

  // find our ending point
  PRBool endIsText = IsTextNode(aEndNode);
  nsCOMPtr<nsIContent>endContent;
  if (endIsText) {
    endContent = do_QueryInterface(aEndNode);
  }
  else 
  {
    nsCOMPtr<nsIDOMNode>theEndNode;
    if (aEndOffset>0)
    {
      theEndNode = GetChildAt(aEndNode, aEndOffset-1);
    }
    else {
      // XXX: we need to find the previous node and set the selection correctly
      NS_ASSERTION(0, "unexpected selection");
      return NS_ERROR_NOT_IMPLEMENTED;
    }
    endContent = do_QueryInterface(theEndNode);
  }

  if (!startContent || !endContent) { return NS_ERROR_NULL_POINTER; }
  // iterate over the nodes between the starting and ending points
  iter->Init(aRange);
  nsCOMPtr<nsIContent> content;
  iter->CurrentNode(getter_AddRefs(content));
  nsAutoString tag;
  aPropName->ToString(tag);
  while (NS_ENUMERATOR_FALSE == iter->IsDone())
  {
    if ((content.get() != startContent.get()) &&
        (content.get() != endContent.get()))
    {
      nsCOMPtr<nsIDOMNode>node;
      node = do_QueryInterface(content);
      if (IsEditable(node))
      {
        PRBool canContainChildren;
        content->CanContainChildren(canContainChildren);
        if (!canContainChildren)
        { 
          nsEditor::GetTagString(node,tag);
          PRBool processLeaf; 
          IsLeafThatTakesInlineStyle(&tag, processLeaf);
          if (processLeaf)  // skip leaf tags that don't take inline style
          { 
            // only want to wrap the node in a new style node if it doesn't already have that style
            if (gNoisy) { printf("node %p is an editable leaf.\n", node.get()); }
            PRBool textPropertySet;
            nsCOMPtr<nsIDOMNode>resultNode;
            IsTextPropertySetByContent(node, aPropName, aAttribute, aValue, textPropertySet, getter_AddRefs(resultNode));
            if (!textPropertySet)
            {
              if (gNoisy) { printf("property not set\n"); }
              node->GetParentNode(getter_AddRefs(parent));
              if (!parent) { return NS_ERROR_NULL_POINTER; }
              nsCOMPtr<nsIContent>parentContent;
              parentContent = do_QueryInterface(parent);
              nsCOMPtr<nsIDOMNode>parentNode = do_QueryInterface(parent);
              if (IsTextNode(node))
              {
                startOffset = 0;
                result = GetLengthOfDOMNode(node, (PRUint32&)endOffset);
              }
              else
              {
                parentContent->IndexOf(content, startOffset);
                endOffset = startOffset+1;
              }
              if (gNoisy) { printf("start/end = %d %d\n", startOffset, endOffset); }
              if (NS_SUCCEEDED(result)) {
                result = SetTextPropertiesForNode(node, parentNode, startOffset, endOffset, aPropName, aAttribute, aValue);
              }
            }
          }
        }
      }
      // XXX: shouldn't there be an else here for non-text leaf nodes?
    }
    // note we don't check the result, we just rely on iter->IsDone
    iter->Next();
    iter->CurrentNode(getter_AddRefs(content));
  }
  // handle endpoints
  if (NS_SUCCEEDED(result))
  {
    // create a style node for the text in the start parent
    nsCOMPtr<nsIDOMNode>startNode = do_QueryInterface(startContent);
    result = startNode->GetParentNode(getter_AddRefs(parent));
    if (NS_FAILED(result)) return result;
    if (!parent) return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
    nodeAsChar = do_QueryInterface(startNode);
    if (nodeAsChar)
    {   
      nodeAsChar->GetLength(&count);
      if (gNoisy) { printf("processing start node %p.\n", nodeAsChar.get()); }
      if (aStartOffset!=(PRInt32)count)
      {
        result = SetTextPropertiesForNode(startNode, parent, aStartOffset, count, aPropName, aAttribute, aValue);
        startOffset = 0;
      }
    }
    else 
    {
      nsCOMPtr<nsIDOMNode>grandParent;
      result = parent->GetParentNode(getter_AddRefs(grandParent));
      if (NS_FAILED(result)) return result;
      if (!grandParent) return NS_ERROR_NULL_POINTER;
      if (gNoisy) { printf("processing start node %p.\n", parent.get()); }
      result = SetTextPropertiesForNode(parent, grandParent, aStartOffset, aStartOffset+1, aPropName, aAttribute, aValue);
      startNode = do_QueryInterface(parent);
      startOffset = aStartOffset;
    }


    if (NS_SUCCEEDED(result))
    {
      // create a style node for the text in the end parent
      nsCOMPtr<nsIDOMNode>endNode = do_QueryInterface(endContent);
      result = endNode->GetParentNode(getter_AddRefs(parent));
      if (NS_FAILED(result)) {
        return result;
      }
      nodeAsChar = do_QueryInterface(endNode);
      if (nodeAsChar)
      {
        nodeAsChar->GetLength(&count);
        if (gNoisy) { printf("processing end node %p.\n", nodeAsChar.get()); }
        if (0!=aEndOffset)
        {
          result = SetTextPropertiesForNode(endNode, parent, 0, aEndOffset, aPropName, aAttribute, aValue);
          // SetTextPropertiesForNode kindly computed the proper selection focus node and offset for us, 
          // remember them here
          selection->GetFocusOffset(&endOffset);
          selection->GetFocusNode(getter_AddRefs(endNode));
        }
      }
      else 
      {
        NS_ASSERTION(0!=aEndOffset, "unexpected selection end offset");
        if (0==aEndOffset) { return NS_ERROR_NOT_IMPLEMENTED; }
        nsCOMPtr<nsIDOMNode>grandParent;
        result = parent->GetParentNode(getter_AddRefs(grandParent));
        if (NS_FAILED(result)) return result;
        if (!grandParent) return NS_ERROR_NULL_POINTER;
        if (gNoisy) { printf("processing end node %p.\n", parent.get()); }
        result = SetTextPropertiesForNode(parent, grandParent, aEndOffset-1, aEndOffset, aPropName, aAttribute, aValue);
        endNode = do_QueryInterface(parent);
        endOffset = 0;
      }
      if (NS_SUCCEEDED(result))
      {

        selection->Collapse(startNode, startOffset);
        selection->Extend(endNode, aEndOffset);
      }
    }
  }
  if (gNoisy) { printf("end nsTextEditor::SetTextPropertiesForNodeWithDifferentParents\n"); }

  return result;
}

NS_IMETHODIMP
nsHTMLEditor::RemoveTextPropertiesForNode(nsIDOMNode *aNode, 
                                          nsIDOMNode *aParent,
                                          PRInt32     aStartOffset,
                                          PRInt32     aEndOffset,
                                          nsIAtom    *aPropName,
                                          const nsString *aAttribute)
{
  if (gNoisy) { printf("nsTextEditor::RemoveTextPropertyForNode\n"); }
  nsresult result=NS_OK;
  nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
  nodeAsChar =  do_QueryInterface(aNode);
  PRBool textPropertySet;
  nsCOMPtr<nsIDOMNode>resultNode;
  IsTextPropertySetByContent(aNode, aPropName, aAttribute, nsnull, textPropertySet, getter_AddRefs(resultNode));
  if (textPropertySet)
  {
    nsCOMPtr<nsIDOMNode>parent; // initially set to first interior parent node to process
    nsCOMPtr<nsIDOMNode>newMiddleNode; // this will be the middle node after any required splits
    nsCOMPtr<nsIDOMNode>newLeftNode;   // this will be the leftmost node, 
                                       // the node being split will be rightmost
    PRUint32 count;
    // if aNode is a text node, treat is specially
    if (nodeAsChar)
    {
      nodeAsChar->GetLength(&count);
      // split the node, and all parent nodes up to the style node
      // then promote the selected content to the parent of the style node
      if (0!=aStartOffset) {
        if (gNoisy) { printf("* splitting text node %p at %d\n", aNode, aStartOffset);}
        result = nsEditor::SplitNode(aNode, aStartOffset, getter_AddRefs(newLeftNode));
        if (gNoisy) { printf("* split created left node %p\n", newLeftNode.get());}
        if (gNoisy) {DebugDumpContent(); } // DEBUG
      }
      if (NS_SUCCEEDED(result))
      {
        if ((PRInt32)count!=aEndOffset) {
          if (gNoisy) { printf("* splitting text node (right node) %p at %d\n", aNode, aEndOffset-aStartOffset);}
          result = nsEditor::SplitNode(aNode, aEndOffset-aStartOffset, getter_AddRefs(newMiddleNode));
          if (gNoisy) { printf("* split created middle node %p\n", newMiddleNode.get());}
          if (gNoisy) {DebugDumpContent(); } // DEBUG
        }
        else {
          if (gNoisy) { printf("* no need to split text node, middle to aNode\n");}
          newMiddleNode = do_QueryInterface(aNode);
        }
        NS_ASSERTION(newMiddleNode, "no middle node created");
        // now that the text node is split, split parent nodes until we get to the style node
        parent = do_QueryInterface(aParent);  // we know this has to succeed, no need to check
      }
    }
    else {
      newMiddleNode = do_QueryInterface(aNode);
      parent = do_QueryInterface(aParent); 
    }
    if (NS_SUCCEEDED(result) && newMiddleNode)
    {
      // split every ancestor until we find the node that is giving us the style we want to remove
      // then split the style node and promote the selected content to the style node's parent
      while (NS_SUCCEEDED(result) && parent)
      {
        if (gNoisy) { printf("* looking at parent %p\n", parent.get());}
        // get the tag from parent and see if we're done
        nsCOMPtr<nsIDOMNode>temp;
        nsCOMPtr<nsIDOMElement>element;
        element = do_QueryInterface(parent);
        if (element)
        {
          nsAutoString tag;
          result = element->GetTagName(tag);
          if (gNoisy) { printf("* parent has tag %s\n", tag.ToNewCString()); } // XXX leak!
          if (NS_SUCCEEDED(result))
          {
            const PRUnichar *unicodeString;
            aPropName->GetUnicode(&unicodeString);
            if (!tag.EqualsIgnoreCase(unicodeString))
            {
              PRInt32 offsetInParent;
              result = GetChildOffset(newMiddleNode, parent, offsetInParent);
              if (NS_SUCCEEDED(result))
              {
                if (0!=offsetInParent) {
                  if (gNoisy) { printf("* splitting parent %p at offset %d\n", parent.get(), offsetInParent);}
                  result = nsEditor::SplitNode(parent, offsetInParent, getter_AddRefs(newLeftNode));
                  if (gNoisy) { printf("* split created left node %p sibling of parent\n", newLeftNode.get());}
                  if (gNoisy) {DebugDumpContent(); } // DEBUG
                }
                if (NS_SUCCEEDED(result))
                {
                  nsCOMPtr<nsIDOMNodeList>childNodes;
                  result = parent->GetChildNodes(getter_AddRefs(childNodes));
                  if (NS_SUCCEEDED(result) && childNodes)
                  {
                    childNodes->GetLength(&count);
                    NS_ASSERTION(count>0, "bad child count in newly split node");
                    if ((PRInt32)count!=1) 
                    {
                      if (gNoisy) { printf("* splitting parent %p at offset %d\n", parent.get(), 1);}
                      result = nsEditor::SplitNode(parent, 1, getter_AddRefs(newMiddleNode));
                      if (gNoisy) { printf("* split created middle node %p sibling of parent\n", newMiddleNode.get());}
                      if (gNoisy) {DebugDumpContent(); } // DEBUG
                    }
                    else {
                      if (gNoisy) { printf("* no need to split parent, newMiddleNode=parent\n");}
                      newMiddleNode = do_QueryInterface(parent);
                    }
                    NS_ASSERTION(newMiddleNode, "no middle node created");
                    parent->GetParentNode(getter_AddRefs(temp));
                    parent = do_QueryInterface(temp);
                  }
                }
              }
            }
            // else we've found the style tag (referred to by "parent")
            // newMiddleNode is the node that is an ancestor to the selection
            else
            {
              if (gNoisy) { printf("* this is the style node\n");}
              PRInt32 offsetInParent;
              result = GetChildOffset(newMiddleNode, parent, offsetInParent);
              if (NS_SUCCEEDED(result))
              {
                nsCOMPtr<nsIDOMNodeList>childNodes;
                result = parent->GetChildNodes(getter_AddRefs(childNodes));
                if (NS_SUCCEEDED(result) && childNodes)
                {
                  childNodes->GetLength(&count);
                  // if there are siblings to the right, split parent at offsetInParent+1
                  if ((PRInt32)count!=offsetInParent+1)
                  {
                    nsCOMPtr<nsIDOMNode>newRightNode;
                    //nsCOMPtr<nsIDOMNode>temp;
                    if (gNoisy) { printf("* splitting parent %p at offset %d for right side\n", parent.get(), offsetInParent+1);}
                    result = nsEditor::SplitNode(parent, offsetInParent+1, getter_AddRefs(temp));
                    if (NS_SUCCEEDED(result))
                    {
                      newRightNode = do_QueryInterface(parent);
                      parent = do_QueryInterface(temp);
                      if (gNoisy) { printf("* split created right node %p sibling of parent %p\n", newRightNode.get(), parent.get());}
                      if (gNoisy) {DebugDumpContent(); } // DEBUG
                    }
                  }
                  if (NS_SUCCEEDED(result) && 0!=offsetInParent) {
                    if (gNoisy) { printf("* splitting parent %p at offset %d for left side\n", parent.get(), offsetInParent);}
                    result = nsEditor::SplitNode(parent, offsetInParent, getter_AddRefs(newLeftNode));
                    if (gNoisy) { printf("* split created left node %p sibling of parent %p\n", newLeftNode.get(), parent.get());}
                    if (gNoisy) {DebugDumpContent(); } // DEBUG
                  }
                  if (NS_SUCCEEDED(result))
                  { // promote the selection to the grandparent
                    // first, determine the child's position in it's parent
                    PRInt32 childPositionInParent;
                    GetChildOffset(newMiddleNode, parent, childPositionInParent);
                    // compare childPositionInParent to the number of children in parent
                    //PRUint32 count=0;
                    //nsCOMPtr<nsIDOMNodeList>childNodes;
                    result = parent->GetChildNodes(getter_AddRefs(childNodes));
                    if (NS_SUCCEEDED(result) && childNodes) {
                      childNodes->GetLength(&count);
                    }
                    PRBool insertAfter = PR_FALSE;
                    // if they're equal, we'll insert newMiddleNode in grandParent after the parent
                    if ((PRInt32)count==childPositionInParent) {
                      insertAfter = PR_TRUE;
                    }
                    // now that we know where to put newMiddleNode, do it.
                    nsCOMPtr<nsIDOMNode>grandParent;
                    result = parent->GetParentNode(getter_AddRefs(grandParent));
                    if (NS_SUCCEEDED(result) && grandParent)
                    {
                      PRInt32 position;
                      result = GetChildOffset(parent, grandParent, position);
                      if (NS_SUCCEEDED(result)) 
                      {
                        if (insertAfter)
                        {
                          if (gNoisy) {printf("insertAfter=PR_TRUE, incr. position\n"); }
                          position++;
                        }
                        if (gNoisy) { 
                          printf("* inserting node %p in grandparent %p at offset %d\n", 
                                 newMiddleNode.get(), grandParent.get(), position);
                        }
                        result = nsEditor::MoveNode(newMiddleNode, grandParent, position);
                        if (gNoisy) {DebugDumpContent(); } // DEBUG
                        if (NS_SUCCEEDED(result)) 
                        {
                          PRBool hasChildren=PR_TRUE;
                          parent->HasChildNodes(&hasChildren);
                          if (!hasChildren) {
                            if (gNoisy) { printf("* deleting empty style node %p\n", parent.get());}
                            result = nsEditor::DeleteNode(parent);
                            if (gNoisy) {DebugDumpContent(); } // DEBUG
                          }
                        }
                      }
                    }
                  }
                }
              }
              break;
            }
          }
        }
      }
    }
  }
  return result;
}

/* this should only get called if the only intervening nodes are inline style nodes */
NS_IMETHODIMP 
nsHTMLEditor::RemoveTextPropertiesForNodesWithSameParent(nsIDOMNode *aStartNode,
                                                         PRInt32     aStartOffset,
                                                         nsIDOMNode *aEndNode,
                                                         PRInt32     aEndOffset,
                                                         nsIDOMNode *aParent,
                                                         nsIAtom    *aPropName,
                                                         const nsString *aAttribute)
{
  NS_ASSERTION(0, "obsolete");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLEditor::RemoveTextPropertiesForNodeWithDifferentParents(nsIDOMNode  *aStartNode,
                                                              PRInt32      aStartOffset,
                                                              nsIDOMNode  *aEndNode,
                                                              PRInt32      aEndOffset,
                                                              nsIDOMNode  *aParent,
                                                              nsIAtom     *aPropName,
                                                              const nsString *aAttribute)
{
  if (gNoisy) { printf("-- start nsTextEditor::RemoveTextPropertiesForNodeWithDifferentParents--\n"); }
  nsresult result=NS_OK;
  if (!aStartNode || !aEndNode || !aParent || !aPropName)
    return NS_ERROR_NULL_POINTER;

  PRInt32 rangeStartOffset = aStartOffset;  // used to construct a range for the nodes between
  PRInt32 rangeEndOffset = aEndOffset;      // aStartNode and aEndNode after we've processed those endpoints

  // temporary state variables
  PRBool textPropertySet;
  nsCOMPtr<nsIDOMNode>resultNode;

  // delete the style node for the text in the start parent
  nsCOMPtr<nsIDOMNode>startNode = do_QueryInterface(aStartNode);  // use computed endpoint based on end points passed in
  nsCOMPtr<nsIDOMNode>endNode = do_QueryInterface(aEndNode);      // use computed endpoint based on end points passed in
  nsCOMPtr<nsIDOMNode>parent;                                     // the parent of the node we're operating on
  PRBool skippedStartNode = PR_FALSE;                             // we skip the start node if aProp is not set on it
  PRBool skippedEndNode = PR_FALSE;                             // we skip the end node if aProp is not set on it
  PRUint32 count;
  nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
  nodeAsChar = do_QueryInterface(startNode);
  if (nodeAsChar) 
  {
    result = aStartNode->GetParentNode(getter_AddRefs(parent));
    if (NS_FAILED(result)) { return result; }
    nodeAsChar->GetLength(&count);
    if ((PRUint32)aStartOffset!=count) 
    {  // only do this if at least one child is selected
      IsTextPropertySetByContent(aStartNode, aPropName, aAttribute, nsnull, textPropertySet, getter_AddRefs(resultNode));
      if (textPropertySet)
      {
        result = RemoveTextPropertiesForNode(aStartNode, parent, aStartOffset, count, aPropName, aAttribute);
        if (0!=aStartOffset) {
          rangeStartOffset = 0; // we split aStartNode at aStartOffset and it is the right node now
        }
      }
      else 
      {
        skippedStartNode = PR_TRUE;
        if (gNoisy) { printf("skipping start node because property not set\n"); }
      }
    }
    else 
    { 
      skippedStartNode = PR_TRUE;
      if (gNoisy) { printf("skipping start node because aStartOffset==count\n"); } 
    }
  }
  else
  {
    startNode = GetChildAt(aStartNode, aStartOffset);
    parent = do_QueryInterface(aStartNode);
    if (!startNode || !parent) {return NS_ERROR_NULL_POINTER;}
    IsTextPropertySetByContent(startNode, aPropName, aAttribute, nsnull, textPropertySet, getter_AddRefs(resultNode));
    if (textPropertySet)
    {
      result = RemoveTextPropertiesForNode(startNode, parent, aStartOffset, aStartOffset+1, aPropName, aAttribute);
    }
    else
    {
      skippedStartNode = PR_TRUE;
      if (gNoisy) { printf("skipping start node because property not set\n"); }
    }
  }

  // delete the style node for the text in the end parent
  if (NS_SUCCEEDED(result))
  {
    nodeAsChar = do_QueryInterface(endNode);
    if (nodeAsChar)
    {
      result = endNode->GetParentNode(getter_AddRefs(parent));
      if (NS_FAILED(result)) return result;
      if (!parent) return NS_ERROR_NULL_POINTER;
      nodeAsChar->GetLength(&count);
      if (aEndOffset!=0) 
      {  // only do this if at least one child is selected
        IsTextPropertySetByContent(endNode, aPropName, aAttribute, nsnull, textPropertySet, getter_AddRefs(resultNode));
        if (textPropertySet)
        {
          result = RemoveTextPropertiesForNode(endNode, parent, 0, aEndOffset, aPropName, aAttribute);
          if (0!=aEndOffset && (((PRInt32)count)!=aEndOffset)) {
            rangeEndOffset = 0; // we split endNode at aEndOffset and it is the right node now
          }
        }
        else 
        {
          skippedEndNode = PR_TRUE;
          if (gNoisy) { printf("skipping end node because aProperty not set.\n"); } 
        }
      }
      else 
      { 
        skippedEndNode = PR_TRUE;
        if (gNoisy) { printf("skipping end node because aEndOffset==0\n"); } 
      }
    }
    else
    {
      // experimental hack:  need to rewrite all of this
      // MOOSE!
      if (aEndOffset<0) aEndOffset=1;
      endNode = GetChildAt(aEndNode, aEndOffset-1);
      parent = do_QueryInterface(aEndNode);
      if (!endNode || !parent) {return NS_ERROR_NULL_POINTER;}
      IsTextPropertySetByContent(endNode, aPropName, aAttribute, nsnull, textPropertySet, getter_AddRefs(resultNode));
      if (textPropertySet)
      {
        result = RemoveTextPropertiesForNode(endNode, parent, aEndOffset-1, aEndOffset, aPropName, aAttribute);
      }
      else
      {
        skippedEndNode = PR_TRUE;
        if (gNoisy) { printf("skipping end node because property not set\n"); }
      }
    }
  }

  // remove aPropName style nodes for all the content between the start and end nodes
  if (NS_SUCCEEDED(result))
  {
    // build our own range now, because the endpoints may have shifted during shipping
    nsCOMPtr<nsIDOMRange> range;
    result = nsComponentManager::CreateInstance(kCRangeCID, 
                                                nsnull, 
                                                NS_GET_IID(nsIDOMRange), 
                                                getter_AddRefs(range));
    if (NS_FAILED(result)) { return result; }
    if (!range) { return NS_ERROR_NULL_POINTER; }
    // compute the start node
    if (skippedStartNode) 
    {
      nsCOMPtr<nsIDOMNode>tempNode = do_QueryInterface(startNode);
      nsEditor::GetNextNode(startNode, PR_TRUE, getter_AddRefs(tempNode));
      startNode = do_QueryInterface(tempNode);
    }
    range->SetStart(startNode, rangeStartOffset);
    range->SetEnd(endNode, rangeEndOffset);
    if (gNoisy) 
    { 
      printf("created range [(%p,%d), (%p,%d)]\n", 
              startNode.get(), rangeStartOffset,
              endNode.get(), rangeEndOffset);
    }

    nsVoidArray nodeList;
    nsCOMPtr<nsIContentIterator>iter;
    result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                                NS_GET_IID(nsIContentIterator), getter_AddRefs(iter));
    if (NS_FAILED(result)) return result;
    if (!iter) return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIContent>startContent;
    startContent = do_QueryInterface(startNode);
    nsCOMPtr<nsIContent>endContent;
    endContent = do_QueryInterface(endNode);
    if (startContent && endContent)
    {
      iter->Init(range);
      nsCOMPtr<nsIContent> content;
      iter->CurrentNode(getter_AddRefs(content));
      nsAutoString propName;  // the property we are removing
      aPropName->ToString(propName);
      while (NS_ENUMERATOR_FALSE == iter->IsDone())
      {
        if ((content.get() != startContent.get()) &&
            (content.get() != endContent.get()))
        {
          nsCOMPtr<nsIDOMElement>element;
          element = do_QueryInterface(content);
          if (element)
          {
            nsString tag;
            element->GetTagName(tag);
            if (propName.EqualsIgnoreCase(tag))
            {
              if (-1==nodeList.IndexOf(content.get())) {
                nodeList.AppendElement((void *)(content.get()));
              }
            }
          }
        }
        // note we don't check the result, we just rely on iter->IsDone
        iter->Next();
        iter->CurrentNode(getter_AddRefs(content));
      }
    }

    // now delete all the style nodes we found
    if (NS_SUCCEEDED(result))
    {
      nsIContent *contentPtr;
      contentPtr = (nsIContent*)(nodeList.ElementAt(0));
      while (NS_SUCCEEDED(result) && contentPtr)
      {
        nsCOMPtr<nsIDOMNode>styleNode;
        styleNode = do_QueryInterface(contentPtr);
        // promote the children of styleNode
        nsCOMPtr<nsIDOMNode>parentNode;
        result = styleNode->GetParentNode(getter_AddRefs(parentNode));
        if (NS_SUCCEEDED(result) && parentNode)
        {
          PRInt32 position;
          result = GetChildOffset(styleNode, parentNode, position);
          if (NS_SUCCEEDED(result))
          {
            nsCOMPtr<nsIDOMNode>previousSiblingNode;
            nsCOMPtr<nsIDOMNode>childNode;
            result = styleNode->GetLastChild(getter_AddRefs(childNode));
            while (NS_SUCCEEDED(result) && childNode)
            {
              childNode->GetPreviousSibling(getter_AddRefs(previousSiblingNode));
              result = nsEditor::MoveNode(childNode, parentNode, position);
              if (gNoisy) 
              {
                printf("deleted next sibling node %p\n", childNode.get());
              }
              childNode = do_QueryInterface(previousSiblingNode);        
            } // end while loop 
            // delete styleNode
            result = nsEditor::DeleteNode(styleNode);
            if (gNoisy) 
            {
              printf("deleted style node %p\n", styleNode.get());
            }
          }
        }

        // get next content ptr
        nodeList.RemoveElementAt(0);
        contentPtr = (nsIContent*)(nodeList.ElementAt(0));
      }
    }
    nsCOMPtr<nsIDOMSelection>selection;
    result = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(result)) return result;
    if (!selection) return NS_ERROR_NULL_POINTER;

    // set the sel. start point.  if we skipped the start node, just use it
    if (skippedStartNode)
      startNode = do_QueryInterface(aStartNode);
    result = selection->Collapse(startNode, rangeStartOffset);
    if (NS_FAILED(result)) return result;

    // set the sel. end point.  if we skipped the end node, just use it
    if (skippedEndNode)
      endNode = do_QueryInterface(aEndNode);
    result = selection->Extend(endNode, rangeEndOffset);
    if (NS_FAILED(result)) return result;
    if (gNoisy) { printf("RTPFNWDP set selection.\n"); }
  }
  if (gNoisy) 
  { 
    printf("----- end nsTextEditor::RemoveTextPropertiesForNodeWithDifferentParents, dumping content...-----\n"); 
    DebugDumpContent();
  }

  return result;
}

/* this method scans the selection for adjacent text nodes
 * and collapses them into a single text node.
 * "adjacent" means literally adjacent siblings of the same parent.
 * In all cases, the content of the right text node is moved into 
 * the left node, and the right node is deleted.
 * Uses nsEditor::JoinNodes so action is undoable. 
 * Should be called within the context of a batch transaction.
 */
 /*
 XXX:  TODO: use a helper function of next/prev sibling that only deals with editable content
 */
NS_IMETHODIMP
nsHTMLEditor::CollapseAdjacentTextNodes(nsIDOMSelection *aInSelection)
{
  if (gNoisy) 
  { 
    printf("---------- start nsTextEditor::CollapseAdjacentTextNodes ----------\n"); 
    DebugDumpContent();
  }
  if (!aInSelection) return NS_ERROR_NULL_POINTER;

  nsVoidArray textNodes;  // we can't actually do anything during iteration, so store the text nodes in an array
                          // don't bother ref counting them because we know we can hold them for the 
                          // lifetime of this method

  PRBool isCollapsed;
  aInSelection->GetIsCollapsed(&isCollapsed);
  if (isCollapsed) { return NS_OK; } // no need to scan collapsed selection

  // store info about selection endpoints so we can re-establish selection after collapsing text nodes
  // XXX: won't work for multiple selections (this will create a single selection from anchor to focus)
  nsCOMPtr<nsIDOMNode> parentForSelection;  // selection's block parent
  PRInt32 rangeStartOffset, rangeEndOffset; // selection offsets
  nsresult result = GetTextSelectionOffsetsForRange(aInSelection, getter_AddRefs(parentForSelection), 
                                                    rangeStartOffset, rangeEndOffset);  
  if (NS_FAILED(result)) return result;
  if (!parentForSelection) return NS_ERROR_NULL_POINTER;

  // first, do the endpoints of the selection, so we don't rely on the iteration to include them
  nsCOMPtr<nsIDOMNode>anchor, focus;
  nsCOMPtr<nsIDOMCharacterData>anchorText, focusText;
  aInSelection->GetAnchorNode(getter_AddRefs(anchor));
  anchorText = do_QueryInterface(anchor);
  if (anchorText)
  {
    textNodes.AppendElement((void*)(anchorText.get()));
  }
  aInSelection->GetFocusNode(getter_AddRefs(focus));
  focusText = do_QueryInterface(focus);

  // for all the ranges in the selection, build a list of text nodes
  nsCOMPtr<nsIEnumerator> enumerator;
  result = aInSelection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(result)) return result;
  if (!enumerator) return NS_ERROR_NULL_POINTER;

  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  result = enumerator->CurrentItem(getter_AddRefs(currentItem));
  if (NS_FAILED(result)) return result;
  if (!currentItem) return NS_OK; // there are no ranges in the selection, so nothing to do

  while ((NS_ENUMERATOR_FALSE == enumerator->IsDone()))
  {
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    nsCOMPtr<nsIContentIterator> iter;
    result = nsComponentManager::CreateInstance(kSubtreeIteratorCID, nsnull,
                                                NS_GET_IID(nsIContentIterator), 
                                                getter_AddRefs(iter));
    if (NS_FAILED(result)) return result;
    if (!iter) return NS_ERROR_NULL_POINTER;

    iter->Init(range);
    nsCOMPtr<nsIContent> content;
    result = iter->CurrentNode(getter_AddRefs(content));
    while (NS_ENUMERATOR_FALSE == iter->IsDone())
    {
      nsCOMPtr<nsIDOMCharacterData> text = do_QueryInterface(content);
      if (text && anchorText.get() != text.get() && focusText.get() != text.get())
      {
        textNodes.AppendElement((void*)(text.get()));
      }
      iter->Next();
      iter->CurrentNode(getter_AddRefs(content));
    }
    enumerator->Next();
    enumerator->CurrentItem(getter_AddRefs(currentItem));
  }
  // now add the focus to the list, if it's a text node
  if (focusText)
  {
    textNodes.AppendElement((void*)(focusText.get()));
  }


  // now that I have a list of text nodes, collapse adjacent text nodes
  PRInt32 count = textNodes.Count();
  const PRInt32 initialCount = count;
  while (1<count)
  {
    // get the leftmost 2 elements
    nsIDOMCharacterData *leftTextNode = (nsIDOMCharacterData *)(textNodes.ElementAt(0));
    nsIDOMCharacterData *rightTextNode = (nsIDOMCharacterData *)(textNodes.ElementAt(1));

    // special case:  check prev sibling of the absolute leftmost text node
    if (initialCount==count)
    {
      nsCOMPtr<nsIDOMNode> prevSiblingOfLeftTextNode;
      result = leftTextNode->GetPreviousSibling(getter_AddRefs(prevSiblingOfLeftTextNode));
      if (NS_FAILED(result)) return result;
      if (prevSiblingOfLeftTextNode)
      {
        nsCOMPtr<nsIDOMCharacterData>prevSiblingAsText = do_QueryInterface(prevSiblingOfLeftTextNode);
        if (prevSiblingAsText)
        {
          nsCOMPtr<nsIDOMNode> parent;
          result = leftTextNode->GetParentNode(getter_AddRefs(parent));
          if (NS_FAILED(result)) return result;
          if (!parent) return NS_ERROR_NULL_POINTER;
          result = JoinNodes(prevSiblingOfLeftTextNode, leftTextNode, parent);
          if (NS_FAILED(result)) return result;
        }
      }
    }

    // get the prev sibling of the right node, and see if it's leftTextNode
    nsCOMPtr<nsIDOMNode> prevSiblingOfRightTextNode;
    result = rightTextNode->GetPreviousSibling(getter_AddRefs(prevSiblingOfRightTextNode));
    if (NS_FAILED(result)) return result;
    if (prevSiblingOfRightTextNode)
    {
      nsCOMPtr<nsIDOMCharacterData>prevSiblingAsText = do_QueryInterface(prevSiblingOfRightTextNode);
      if (prevSiblingAsText)
      {
        if (prevSiblingAsText.get()==leftTextNode)
        {
          nsCOMPtr<nsIDOMNode> parent;
          result = rightTextNode->GetParentNode(getter_AddRefs(parent));
          if (NS_FAILED(result)) return result;
          if (!parent) return NS_ERROR_NULL_POINTER;
          result = JoinNodes(leftTextNode, rightTextNode, parent);
          if (NS_FAILED(result)) return result;
        }
      }
    }

    // special case:  check next sibling of the rightmost text node
    if (2==count)
    {
      nsCOMPtr<nsIDOMNode> nextSiblingOfRightTextNode;
      result = rightTextNode->GetNextSibling(getter_AddRefs(nextSiblingOfRightTextNode));
      if (NS_FAILED(result)) return result;
      if (nextSiblingOfRightTextNode)
      {
        nsCOMPtr<nsIDOMCharacterData>nextSiblingAsText = do_QueryInterface(nextSiblingOfRightTextNode);
        if (nextSiblingAsText)
        {
          nsCOMPtr<nsIDOMNode> parent;
          result = rightTextNode->GetParentNode(getter_AddRefs(parent));
          if (NS_FAILED(result)) return result;
          if (!parent) return NS_ERROR_NULL_POINTER;
          result = JoinNodes(rightTextNode, nextSiblingOfRightTextNode, parent);
          if (NS_FAILED(result)) return result;
        }
      }
    }

    // remove the rightmost remaining text node from the array and work our way towards the beginning
    textNodes.RemoveElementAt(0); // remove the leftmost text node from the list
    count --;
  }

  ResetTextSelectionForRange(parentForSelection, rangeStartOffset, rangeEndOffset, aInSelection);

  if (gNoisy) 
  { 
    printf("---------- end nsTextEditor::CollapseAdjacentTextNodes ----------\n"); 
    DebugDumpContent();
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
  nsresult res = GetBodyElement(getter_AddRefs(bodyElement));  
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
  
  // if it's collapsed dont do anything.  
  // MOOSE: We should probably have typing state for this like
  // we do for other things.
  if (bCollapsed)
  {
    return NS_OK;
  }
  
  // wrap with txn batching, rules sniffing, and selection preservation code
  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, kOpSetTextProperty, nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);

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
      selection->ClearSelection();

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
      
      // MOOSE: workaround for selection bug:
      selection->ClearSelection();

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
  nsAutoString tag;
  if (aSizeChange == 1) tag = "big";
  else tag = "small";
  res = InsertContainerAbove(node, &tmp, tag);
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
  if (aSizeChange == 1) tag = "big";
  else tag = "small";
  
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
  if (*outNode && !nsHTMLEditUtils::InBody(*outNode))
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
  if (*outNode && !nsHTMLEditUtils::InBody(*outNode))
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
  if (*outNode && !nsHTMLEditUtils::InBody(*outNode))
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
  if (*outNode && !nsHTMLEditUtils::InBody(*outNode))
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


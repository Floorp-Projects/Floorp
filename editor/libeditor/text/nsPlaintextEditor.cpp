/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsPlaintextEditor.h"
#include "nsICaret.h"
#include "nsTextEditUtils.h"
#include "nsTextEditRules.h"
#include "nsEditorEventListeners.h"
#include "nsIEditActionListener.h"
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
#include "nsGUIEvent.h"

#include "nsIIndependentSelection.h" //domselections answer to frameselection

#include "nsIDocumentObserver.h"
#include "nsIDocumentStateListener.h"

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
#include "nsISupportsPrimitives.h"

// netwerk
#include "nsIURI.h"
#include "nsNetUtil.h"

// Misc
#include "nsEditorUtils.h"
#include "nsIPref.h"
#include "nsStyleConsts.h"
#include "nsIStyleContext.h"

#include "nsAOLCiter.h"
#include "nsInternetCiter.h"

// Drag & Drop, Clipboard
//#include "nsWidgetsCID.h"
#include "nsIClipboard.h"
#include "nsITransferable.h"
//#include "nsIDragService.h"
//#include "nsIDOMNSUIEvent.h"

const PRUnichar nbsp = 160;

// HACK - CID for NS_CTRANSITIONAL_DTD_CID so that we can get at transitional dtd
#define NS_CTRANSITIONAL_DTD_CID \
{ 0x4611d482, 0x960a, 0x11d4, { 0x8e, 0xb0, 0xb6, 0x17, 0x66, 0x1b, 0x6f, 0x7c } }


static NS_DEFINE_CID(kHTMLEditorCID,  NS_HTMLEDITOR_CID);
static NS_DEFINE_CID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_IID(kSubtreeIteratorCID, NS_SUBTREEITERATOR_CID);
static NS_DEFINE_CID(kCRangeCID,      NS_RANGE_CID);
static NS_DEFINE_CID(kCDOMSelectionCID,      NS_DOMSELECTION_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID); 
static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID); 
static NS_DEFINE_CID(kCTransitionalDTDCID,  NS_CTRANSITIONAL_DTD_CID);
// Drag & Drop, Clipboard Support
static NS_DEFINE_CID(kCClipboardCID,    NS_CLIPBOARD_CID);
static NS_DEFINE_CID(kCTransferableCID, NS_TRANSFERABLE_CID);
//static NS_DEFINE_CID(kCDragServiceCID, NS_DRAGSERVICE_CID);
//static NS_DEFINE_CID(kCHTMLFormatConverterCID, NS_HTMLFORMATCONVERTER_CID);

#if defined(NS_DEBUG) && defined(DEBUG_buster)
static PRBool gNoisy = PR_FALSE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif

// prototype for rules creation shortcut
nsresult NS_NewTextEditRules(nsIEditRules** aInstancePtrResult);

nsPlaintextEditor::nsPlaintextEditor()
: nsEditor()
, mIgnoreSpuriousDragEvent(PR_FALSE)
, mRules(nsnull)
, mIsComposing(PR_FALSE)
, mMaxTextLength(-1)
, mInitTriggerCounter(0)
{
// Done in nsEditor
// NS_INIT_REFCNT();
} 

nsPlaintextEditor::~nsPlaintextEditor()
{
  // remove the rules as an action listener.  Else we get a bad ownership loop later on.
  // it's ok if the rules aren't a listener; we ignore the error.
  nsCOMPtr<nsIEditActionListener> mListener = do_QueryInterface(mRules);
  RemoveEditActionListener(mListener);
  
  nsCOMPtr<nsIDOMEventReceiver> erP;
  nsresult result = GetDOMEventReceiver(getter_AddRefs(erP));
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

NS_IMPL_ADDREF_INHERITED(nsPlaintextEditor, nsEditor)
NS_IMPL_RELEASE_INHERITED(nsPlaintextEditor, nsEditor)


NS_IMETHODIMP nsPlaintextEditor::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (!aInstancePtr)
    return NS_ERROR_NULL_POINTER;
 
  *aInstancePtr = nsnull;
  
  if (aIID.Equals(NS_GET_IID(nsIPlaintextEditor))) {
    *aInstancePtr = NS_STATIC_CAST(nsIPlaintextEditor*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIEditor))) {
    *aInstancePtr = NS_STATIC_CAST(nsIEditor*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIEditorMailSupport))) {
    *aInstancePtr = NS_STATIC_CAST(nsIEditorMailSupport*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return nsEditor::QueryInterface(aIID, aInstancePtr);
}


NS_IMETHODIMP nsPlaintextEditor::Init(nsIDOMDocument *aDoc, 
                                 nsIPresShell   *aPresShell, nsIContent *aRoot, nsISelectionController *aSelCon, PRUint32 aFlags)
{
  NS_PRECONDITION(aDoc && aPresShell, "bad arg");
  if (!aDoc || !aPresShell)
    return NS_ERROR_NULL_POINTER;
  
  nsresult res = NS_OK, rulesRes = NS_OK;
  
  if (1)
  {
    // block to scope nsAutoEditInitRulesTrigger
    nsAutoEditInitRulesTrigger rulesTrigger(this, rulesRes);
  
    // Init the base editor
    res = nsEditor::Init(aDoc, aPresShell, aRoot, aSelCon, aFlags);
  }
  
  if (NS_FAILED(rulesRes)) return rulesRes;
  return res;
}

void 
nsPlaintextEditor::BeginEditorInit()
{
  mInitTriggerCounter++;
}

nsresult 
nsPlaintextEditor::EndEditorInit()
{
  nsresult res = NS_OK;
  NS_PRECONDITION(mInitTriggerCounter > 0, "ended editor init before we began?");
  mInitTriggerCounter--;
  if (mInitTriggerCounter == 0)
  {
    res = InitRules();
    if (NS_SUCCEEDED(res)) 
      EnableUndo(PR_TRUE);
  }
  return res;
}

NS_IMETHODIMP 
nsPlaintextEditor::SetDocumentCharacterSet(const nsAReadableString & characterSet) 
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
      result = domdoc->GetElementsByTagName(NS_LITERAL_STRING("meta"), getter_AddRefs(metaList)); 
      if (NS_SUCCEEDED(result) && metaList) { 
        PRUint32 listLength = 0; 
        (void) metaList->GetLength(&listLength); 

        for (PRUint32 i = 0; i < listLength; i++) { 
          metaList->Item(i, getter_AddRefs(metaNode)); 
          if (!metaNode) continue; 
          metaElement = do_QueryInterface(metaNode); 
          if (!metaElement) continue; 

          NS_NAMED_LITERAL_STRING(charset, "charset=");
          NS_NAMED_LITERAL_STRING(content, "content");
          nsString currentValue; 

          if (NS_FAILED(metaElement->GetAttribute(NS_LITERAL_STRING("http-equiv"), currentValue))) continue; 

          if (kNotFound != currentValue.Find("content-type", PR_TRUE)) { 
            if (NS_FAILED(metaElement->GetAttribute(content, currentValue))) continue; 

            PRInt32 offset = currentValue.Find(charset.get(), PR_TRUE); 
            if (kNotFound != offset) {
              currentValue.Left(newMetaString, offset); // copy current value before "charset=" (e.g. text/html) 
              newMetaString.Append(charset); 
              newMetaString.Append(characterSet); 
              result = nsEditor::SetAttribute(metaElement, content, newMetaString); 
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

        result = domdoc->GetElementsByTagName(NS_LITERAL_STRING("head"),getter_AddRefs(headList)); 
        if (NS_SUCCEEDED(result) && headList) { 
          headList->Item(0, getter_AddRefs(headNode)); 
          if (headNode) { 
            // Create a new meta charset tag 
              // can't use |NS_LITERAL_STRING| here until |CreateNode| is fixed to accept readables
            result = CreateNode(NS_LITERAL_STRING("meta"), headNode, 0, getter_AddRefs(resultNode)); 
            if (NS_FAILED(result)) 
              return NS_ERROR_FAILURE; 

            // Set attributes to the created element 
            if (resultNode && characterSet.Length() > 0) { 
              metaElement = do_QueryInterface(resultNode); 
              if (metaElement) { 
                // not undoable, undo should undo CreateNode 
                result = metaElement->SetAttribute(NS_LITERAL_STRING("http-equiv"), NS_LITERAL_STRING("Content-Type")); 
                if (NS_SUCCEEDED(result)) { 
                  newMetaString.AssignWithConversion("text/html;charset="); 
                  newMetaString.Append(characterSet); 
                  // not undoable, undo should undo CreateNode 
                  result = metaElement->SetAttribute(NS_LITERAL_STRING("content"), newMetaString); 
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
nsPlaintextEditor::PostCreate()
{
  nsresult result = InstallEventListeners();
  if (NS_FAILED(result)) return result;

  result = nsEditor::PostCreate();
  return result;
}

NS_IMETHODIMP 
nsPlaintextEditor::InstallEventListeners()
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
nsPlaintextEditor::GetFlags(PRUint32 *aFlags)
{
  if (!mRules || !aFlags) { return NS_ERROR_NULL_POINTER; }
  return mRules->GetFlags(aFlags);
}


NS_IMETHODIMP 
nsPlaintextEditor::SetFlags(PRUint32 aFlags)
{
  if (!mRules) { return NS_ERROR_NULL_POINTER; }
  return mRules->SetFlags(aFlags);
}


NS_IMETHODIMP nsPlaintextEditor::InitRules()
{
  // instantiate the rules for this text editor
  nsresult res = NS_ERROR_FAILURE;
  res = NS_NewTextEditRules(getter_AddRefs(mRules));
  if (NS_FAILED(res)) return res;
  if (!mRules) return NS_ERROR_UNEXPECTED;
  res = mRules->Init(this, mFlags);
  
  return res;
}


PRBool nsPlaintextEditor::IsModifiable()
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

NS_IMETHODIMP nsPlaintextEditor::HandleKeyPress(nsIDOMKeyEvent* aKeyEvent)
{
  PRUint32 keyCode, character;
  PRBool   isShift, ctrlKey, altKey, metaKey;

  if (!aKeyEvent) return NS_ERROR_NULL_POINTER;

  if (NS_SUCCEEDED(aKeyEvent->GetKeyCode(&keyCode)) && 
      NS_SUCCEEDED(aKeyEvent->GetShiftKey(&isShift)) &&
      NS_SUCCEEDED(aKeyEvent->GetCtrlKey(&ctrlKey)) &&
      NS_SUCCEEDED(aKeyEvent->GetAltKey(&altKey)) &&
      NS_SUCCEEDED(aKeyEvent->GetMetaKey(&metaKey)))
  {
    aKeyEvent->GetCharCode(&character);
    if (keyCode == nsIDOMKeyEvent::DOM_VK_RETURN
     || keyCode == nsIDOMKeyEvent::DOM_VK_ENTER)
    {
      nsString empty;
      return TypedText(empty, eTypedBreak);
    }
    else if (keyCode == nsIDOMKeyEvent::DOM_VK_ESCAPE)
    {
      // pass escape keypresses through as empty strings: needed for ime support
      nsString empty;
      return TypedText(empty, eTypedText);
    }
    
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
NS_IMETHODIMP nsPlaintextEditor::TypedText(const nsAReadableString& aString,
                                      PRInt32 aAction)
{
  nsAutoPlaceHolderBatch batch(this, gTypingTxnName);

  switch (aAction)
  {
    case eTypedText:
      {
        return InsertText(aString);
      }
    case eTypedBreak:
      {
        return InsertLineBreak();
      } 
  } 
  return NS_ERROR_FAILURE; 
}

NS_IMETHODIMP nsPlaintextEditor::CreateBRImpl(nsCOMPtr<nsIDOMNode> *aInOutParent, PRInt32 *aInOutOffset, nsCOMPtr<nsIDOMNode> *outBRNode, EDirection aSelect)
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
    GetNodeLocation(node, address_of(tmp), &offset);
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
      res = GetNodeLocation(node, address_of(tmp), &offset);
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
    nsCOMPtr<nsISelection> selection;
    nsCOMPtr<nsIDOMNode> parent;
    PRInt32 offset;
    res = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
    res = GetNodeLocation(*outBRNode, address_of(parent), &offset);
    if (NS_FAILED(res)) return res;
    if (aSelect == eNext)
    {
      // position selection after br
      selPriv->SetInterlinePosition(PR_TRUE);
      res = selection->Collapse(parent, offset+1);
    }
    else if (aSelect == ePrevious)
    {
      // position selection before br
      selPriv->SetInterlinePosition(PR_TRUE);
      res = selection->Collapse(parent, offset);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP nsPlaintextEditor::CreateBR(nsIDOMNode *aNode, PRInt32 aOffset, nsCOMPtr<nsIDOMNode> *outBRNode, EDirection aSelect)
{
  nsCOMPtr<nsIDOMNode> parent = aNode;
  PRInt32 offset = aOffset;
  return CreateBRImpl(address_of(parent), &offset, outBRNode, aSelect);
}

NS_IMETHODIMP nsPlaintextEditor::InsertBR(nsCOMPtr<nsIDOMNode> *outBRNode)
{
  PRBool bCollapsed;
  nsCOMPtr<nsISelection> selection;

  if (!outBRNode) return NS_ERROR_NULL_POINTER;
  *outBRNode = nsnull;

  // calling it text insertion to trigger moz br treatment by rules
  nsAutoRules beginRulesSniffing(this, kOpInsertText, nsIEditor::eNext);

  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
  res = selection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed)
  {
    res = DeleteSelection(nsIEditor::eNone);
    if (NS_FAILED(res)) return res;
  }
  nsCOMPtr<nsIDOMNode> selNode;
  PRInt32 selOffset;
  res = GetStartNodeAndOffset(selection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
  
  res = CreateBR(selNode, selOffset, outBRNode);
  if (NS_FAILED(res)) return res;
    
  // position selection after br
  res = GetNodeLocation(*outBRNode, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
  selPriv->SetInterlinePosition(PR_TRUE);
  res = selection->Collapse(selNode, selOffset+1);
  
  return res;
}
nsresult nsPlaintextEditor::GetTextSelectionOffsets(nsISelection *aSelection,
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
  nsCOMPtr<nsISelection> selection(aSelection);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
  result = selPriv->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(result)) return result;
  if (!enumerator) return NS_ERROR_NULL_POINTER;

  // don't use "result" in this block
  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  nsresult findParentResult = enumerator->CurrentItem(getter_AddRefs(currentItem));
  if ((NS_SUCCEEDED(findParentResult)) && (currentItem))
  {
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    range->GetCommonAncestorContainer(getter_AddRefs(parentNode));
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

nsresult
nsPlaintextEditor::GetAbsoluteOffsetsForPoints(nsIDOMNode *aInStartNode,
                                          PRInt32 aInStartOffset,
                                          nsIDOMNode *aInEndNode,
                                          PRInt32 aInEndOffset,
                                          nsIDOMNode *aInCommonParentNode,
                                          PRInt32 &aOutStartOffset, 
                                          PRInt32 &aOutEndOffset)
{
  if(!aInStartNode || !aInEndNode || !aInCommonParentNode)
    return NS_ERROR_NULL_POINTER;

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
nsPlaintextEditor::GetDOMEventReceiver(nsIDOMEventReceiver **aEventReceiver) 
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
nsPlaintextEditor::CollapseSelectionToStart()
{
  nsCOMPtr<nsIDOMElement> bodyElement;
  nsresult res = nsEditor::GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;
  if (!bodyElement)   return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMNode> bodyNode = do_QueryInterface(bodyElement);
  nsCOMPtr<nsISelection> selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  return selection->Collapse(bodyNode,0);
}

NS_IMETHODIMP nsPlaintextEditor::DeleteSelection(nsIEditor::EDirection aAction)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsISelection> selection;
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

NS_IMETHODIMP nsPlaintextEditor::InsertText(const nsAReadableString &aStringToInsert)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsISelection> selection;
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
  // XXX can we trust instring to outlive ruleInfo,
  // XXX and ruleInfo not to refer to instring in its dtor?
  //nsAutoString instring(aStringToInsert);
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


NS_IMETHODIMP nsPlaintextEditor::InsertLineBreak()
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpInsertBreak, nsIEditor::eNext);
  nsCOMPtr<nsISelection> selection;
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
          nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
          if (-1==offsetInParent) 
          {
            nextNode->GetParentNode(getter_AddRefs(parent));
            res = GetChildOffset(nextNode, parent, offsetInParent);
            if (NS_SUCCEEDED(res)) {
              // SetInterlinePosition(PR_TRUE) means we want the caret to stick to the content on the "right".
              // We want the caret to stick to whatever is past the break.  This is
              // because the break is on the same line we were on, but the next content
              // will be on the following line.
              selPriv->SetInterlinePosition(PR_TRUE);
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
nsPlaintextEditor::BeginComposition(nsTextEventReply* aReply)
{
  if(mFlags & nsIPlaintextEditor::eEditorPasswordMask)  {
    if (mRules) {
      nsIEditRules *p = mRules.get();
      nsTextEditRules *textEditRules = NS_STATIC_CAST(nsTextEditRules *, p);
      textEditRules->ResetIMETextPWBuf();
    }
    else  {
      return NS_ERROR_NULL_POINTER;
    }
  }

  return nsEditor::BeginComposition(aReply);
}

NS_IMETHODIMP
nsPlaintextEditor::GetDocumentIsEmpty(PRBool *aDocumentIsEmpty)
{
  if (!aDocumentIsEmpty)
    return NS_ERROR_NULL_POINTER;
  
  if (!mRules)
    return NS_ERROR_NOT_INITIALIZED;
  
  return mRules->DocumentIsEmpty(aDocumentIsEmpty);
}

NS_IMETHODIMP
nsPlaintextEditor::GetTextLength(PRInt32 *aCount)
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
    }
  }
  return result;
}

NS_IMETHODIMP
nsPlaintextEditor::SetMaxTextLength(PRInt32 aMaxTextLength)
{
  mMaxTextLength = aMaxTextLength;
  return NS_OK;
}

NS_IMETHODIMP
nsPlaintextEditor::GetMaxTextLength(PRInt32* aMaxTextLength)
{
  if (!aMaxTextLength)
    return NS_ERROR_INVALID_POINTER;
  *aMaxTextLength = mMaxTextLength;
  return NS_OK;
}

NS_IMETHODIMP
nsPlaintextEditor::GetBodyStyleContext(nsIStyleContext** aStyleContext)
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
// Get the wrap width for the root of the document.
//
NS_IMETHODIMP 
nsPlaintextEditor::GetWrapWidth(PRInt32 *aWrapColumn)
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
  {
    *aWrapColumn = 0;   // wrap to window width
  }
  else if (NS_STYLE_WHITESPACE_MOZ_PRE_WRAP == styleText->mWhiteSpace)
  {
    const nsStylePosition* stylePosition =
      (const nsStylePosition*)styleContext->GetStyleData(eStyleStruct_Position); 
    if (stylePosition->mWidth.GetUnit() == eStyleUnit_Chars)
      *aWrapColumn = stylePosition->mWidth.GetIntValue(); 
    else 
    {
      *aWrapColumn = -1;
      return NS_ERROR_UNEXPECTED;
    }
  }
  else
  {
    *aWrapColumn = -1;
  }
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
// Change the wrap width on the root of this document.
// 
NS_IMETHODIMP 
nsPlaintextEditor::SetWrapWidth(PRInt32 aWrapColumn)
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
  // but it isn't, see bug 22502, so we have to add "font: -moz-fixed;".
  // Only do this if we're wrapping.
  PRUint32 flags = 0;
  GetFlags(&flags);
  if ((flags & eEditorEnableWrapHackMask) && aWrapColumn >= 0)
    styleValue.AppendWithConversion("font-family: -moz-fixed; ");

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
  return res;
}



#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIEditor overrides 
#pragma mark -
#endif

NS_IMETHODIMP 
nsPlaintextEditor::Undo(PRUint32 aCount)
{
  nsAutoUpdateViewBatch beginViewBatching(this);

  ForceCompositionEnd();
  nsresult result = NS_OK;

  nsAutoRules beginRulesSniffing(this, kOpUndo, nsIEditor::eNone);

  nsTextRulesInfo ruleInfo(nsTextEditRules::kUndo);
  nsCOMPtr<nsISelection> selection;
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
nsPlaintextEditor::Redo(PRUint32 aCount)
{
  nsresult result = NS_OK;

  nsAutoUpdateViewBatch beginViewBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpRedo, nsIEditor::eNone);

  nsTextRulesInfo ruleInfo(nsTextEditRules::kRedo);
  nsCOMPtr<nsISelection> selection;
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

NS_IMETHODIMP nsPlaintextEditor::Cut()
{
  nsCOMPtr<nsISelection> selection;
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

NS_IMETHODIMP nsPlaintextEditor::CanCut(PRBool *aCanCut)
{
  if (!aCanCut)
    return NS_ERROR_NULL_POINTER;
  *aCanCut = PR_FALSE;
  
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
    
  PRBool isCollapsed;
  res = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;

  *aCanCut = !isCollapsed && IsModifiable();
  return NS_OK;
}

NS_IMETHODIMP nsPlaintextEditor::Copy()
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  return ps->DoCopy();
}

NS_IMETHODIMP nsPlaintextEditor::CanCopy(PRBool *aCanCopy)
{
  if (!aCanCopy)
    return NS_ERROR_NULL_POINTER;
  *aCanCopy = PR_FALSE;
  
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
    
  PRBool isCollapsed;
  res = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;

  *aCanCopy = !isCollapsed;
  return NS_OK;
}


// Shared between OutputToString and OutputToStream
NS_IMETHODIMP
nsPlaintextEditor::GetAndInitDocEncoder(const nsAReadableString& aFormatType,
                                        PRUint32 aFlags,
                                        const nsAReadableString& aCharset,
                                        nsIDocumentEncoder** encoder)
{
  nsCOMPtr<nsIPresShell> presShell;
  nsresult rv = GetPresShell(getter_AddRefs(presShell));
  if (NS_FAILED(rv)) return rv;
  if (!presShell) return NS_ERROR_FAILURE;

  nsCAutoString formatType(NS_DOC_ENCODER_CONTRACTID_BASE);
  formatType.AppendWithConversion(aFormatType);
  nsCOMPtr<nsIDocumentEncoder> docEncoder (do_CreateInstance(formatType.get(), &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocument> doc;
  rv = presShell->GetDocument(getter_AddRefs(doc));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = docEncoder->Init(doc, aFormatType, aFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aCharset.Length() != 0
    && !(aCharset.Equals(NS_LITERAL_STRING("null"))))
    docEncoder->SetCharset(aCharset);

  PRInt32 wc;
  (void) GetWrapWidth(&wc);
  if (wc >= 0)
    (void) docEncoder->SetWrapColumn(wc);

  // Set the selection, if appropriate.
  // We do this either if the OutputSelectionOnly flag is set,
  // in which case we use our existing selection ...
  if (aFlags & nsIDocumentEncoder::OutputSelectionOnly)
  {
    nsCOMPtr<nsISelection> selection;
    rv = GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(rv) && selection)
      rv = docEncoder->SetSelection(selection);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  // ... or if the root element is not a body,
  // in which case we set the selection to encompass the root.
  else
  {
    nsCOMPtr<nsIDOMElement> rootElement;
    GetRootElement(getter_AddRefs(rootElement));
    NS_ENSURE_TRUE(rootElement, NS_ERROR_FAILURE);
    if (!nsTextEditUtils::IsBody(rootElement))
    {
      // XXX Why does this use range rather than selection collapse/extend?
      nsCOMPtr<nsIDOMRange> range (do_CreateInstance(kCRangeCID, &rv));
      if (NS_FAILED(rv)) return rv;
      if (!range) return NS_ERROR_FAILURE;
      nsCOMPtr<nsISelection> selection (do_CreateInstance(kCDOMSelectionCID,
                                                          &rv));
      if (NS_FAILED(rv)) return rv;
      if (!selection) return NS_ERROR_FAILURE;

      // get the independent selection interface
      nsCOMPtr<nsIIndependentSelection> indSel = do_QueryInterface(selection);
      if (indSel)
        indSel->SetPresShell(presShell);

      nsCOMPtr<nsIContent> content(do_QueryInterface(rootElement));
      if (content)
      {
        range->SetStart(rootElement,0);
        PRInt32 children;
        if (NS_SUCCEEDED(content->ChildCount(children)))
          range->SetEnd(rootElement,children);
        // XXX else, should we return the error code?

        if (NS_FAILED(selection->AddRange(range)))
          return NS_ERROR_FAILURE;
      }
      rv = docEncoder->SetSelection(selection);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  NS_ADDREF(*encoder = docEncoder);
  return rv;
}


NS_IMETHODIMP 
nsPlaintextEditor::OutputToString(nsAWritableString& aOutputString,
                                  const nsAReadableString& aFormatType,
                                  PRUint32 aFlags)
{
  PRBool cancel, handled;
  nsString resultString;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kOutputText);
  ruleInfo.outString = &resultString;
  // XXX Struct should store a nsAReadable*
  nsAutoString str(aFormatType);
  ruleInfo.outputFormat = &str;
  nsresult rv = mRules->WillDoAction(nsnull, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) { return rv; }
  if (handled)
  { // this case will get triggered by password fields
    aOutputString.Assign(*(ruleInfo.outString));
    return rv;
  }

  nsAutoString charsetStr;
  rv = GetDocumentCharacterSet(charsetStr);
  if(NS_FAILED(rv) || charsetStr.IsEmpty())
    charsetStr = NS_LITERAL_STRING("ISO-8859-1");

  nsCOMPtr<nsIDocumentEncoder> encoder;
  rv = GetAndInitDocEncoder(aFormatType, aFlags, charsetStr, getter_AddRefs(encoder));

  if (NS_FAILED(rv))
    return rv;
  rv = encoder->EncodeToString(aOutputString);
  return rv;
}

NS_IMETHODIMP
nsPlaintextEditor::OutputToStream(nsIOutputStream* aOutputStream,
                             const nsAReadableString& aFormatType,
                             const nsAReadableString& aCharset,
                             PRUint32 aFlags)
{
  nsresult rv;

  // special-case for empty document when requesting plain text,
  // to account for the bogus text node.
  // XXX Should there be a similar test in OutputToString?
  if (aFormatType == NS_LITERAL_STRING("text/plain"))
  {
    PRBool docEmpty;
    rv = GetDocumentIsEmpty(&docEmpty);
    if (NS_FAILED(rv)) return rv;
    
    if (docEmpty)
       return NS_OK;    // output nothing
  }

  nsCOMPtr<nsIDocumentEncoder> encoder;
  rv = GetAndInitDocEncoder(aFormatType, aFlags, aCharset,
                            getter_AddRefs(encoder));

  if (NS_FAILED(rv))
    return rv;

  return encoder->EncodeToStream(aOutputStream);
}


#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIEditorMailSupport overrides 
#pragma mark -
#endif

NS_IMETHODIMP
nsPlaintextEditor::PasteAsQuotation(PRInt32 aSelectionType)
{
  // Get Clipboard Service
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
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
#ifdef DEBUG_clipboard
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
        rv = InsertAsQuotation(stuffToPaste, 0);
        if (text)
          nsMemory::Free(text);
      }
    }
    nsCRT::free(flav);
  }

  return rv;
}

// Utility routine to make a new citer.  This addrefs, of course.
static nsICiter* MakeACiter()
{
  // Make a citer of an appropriate type
  nsICiter* citer = 0;
  nsresult rv;
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID, &rv));
  if (NS_FAILED(rv)) return 0;

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

  if (citer)
    NS_ADDREF(citer);
  return citer;
}

NS_IMETHODIMP
nsPlaintextEditor::InsertAsQuotation(const nsAReadableString& aQuotedText,
                                     nsIDOMNode **aNodeInserted)
{
  // We have the text.  Cite it appropriately:
  nsCOMPtr<nsICiter> citer = dont_AddRef(MakeACiter());

  // Let the citer quote it for us:
  nsString quotedStuff;
  nsresult rv = citer->GetCiteString(aQuotedText, quotedStuff);
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

  nsAutoEditBatch beginBatching(this);
  nsAutoRules beginRulesSniffing(this, kOpInsertText, nsIEditor::eNext);

  // give rules a chance to handle or cancel
  nsTextRulesInfo ruleInfo(nsTextEditRules::kInsertElement);
  PRBool cancel, handled;
  rv = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(rv)) return rv;
  if (cancel) return NS_OK; // rules canceled the operation
  if (!handled)
  {
    rv = InsertText(quotedStuff);

    // XXX Should set *aNodeInserted to the first node inserted
    if (aNodeInserted && NS_SUCCEEDED(rv))
    {
      *aNodeInserted = 0;
      //NS_IF_ADDREF(*aNodeInserted);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsPlaintextEditor::PasteAsCitedQuotation(const nsAReadableString& aCitation,
                                         PRInt32 aSelectionType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPlaintextEditor::InsertAsCitedQuotation(const nsAReadableString& aQuotedText,
                                          const nsAReadableString& aCitation,
                                          PRBool aInsertHTML,
                                          const nsAReadableString& aCharset,
                                          nsIDOMNode **aNodeInserted)
{
  return InsertAsQuotation(aQuotedText, aNodeInserted);
}

NS_IMETHODIMP
nsPlaintextEditor::Rewrap(PRBool aRespectNewlines)
{
  PRInt32 wrapCol;
  nsresult rv = GetWrapWidth(&wrapCol);
  if (NS_FAILED(rv))
    return NS_OK;

  // Rewrap makes no sense if there's no wrap column; default to 72.
  if (wrapCol <= 0)
    wrapCol = 72;

#ifdef DEBUG_akkana
  printf("nsPlaintextEditor::Rewrap to %ld columns\n", (long)wrapCol);
#endif

  nsCOMPtr<nsISelection> selection;
  rv = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(rv)) return rv;

  if (!selection)
    return NS_ERROR_NOT_INITIALIZED;
  PRBool isCollapsed;
  rv = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(rv)) return rv;

  // Variables we'll need either way
  nsAutoString format; format.AssignWithConversion("text/plain");
  nsAutoString current;
  nsString wrapped;

  if (isCollapsed)    // rewrap the whole document
  {
    rv = OutputToString(current, format,
                        nsIDocumentEncoder::OutputFormatted
                        | nsIDocumentEncoder::OutputLFLineBreak);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsICiter> citer = dont_AddRef(MakeACiter());
    if (NS_FAILED(rv)) return rv;
    if (!citer) return NS_ERROR_UNEXPECTED;

    rv = citer->Rewrap(current, wrapCol, 0, aRespectNewlines, wrapped);
    if (NS_FAILED(rv)) return rv;

    rv = SelectAll();
    if (NS_FAILED(rv)) return rv;

    return InsertText(wrapped);
  }
  else                // rewrap only the selection
  {
    rv = OutputToString(current, format,
                        nsIDocumentEncoder::OutputFormatted
                        | nsIDocumentEncoder::OutputLFLineBreak
                        | nsIDocumentEncoder::OutputSelectionOnly);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsICiter> citer = dont_AddRef(MakeACiter());
    if (NS_FAILED(rv)) return rv;
    if (!citer) return NS_ERROR_UNEXPECTED;

    PRUint32 firstLineOffset = 0;   // XXX need to get this
    rv = citer->Rewrap(current, wrapCol, firstLineOffset, aRespectNewlines,
                       wrapped);
    if (NS_FAILED(rv)) return rv;

    return InsertText(wrapped);
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsPlaintextEditor::StripCites()
{
#ifdef DEBUG_akkana
  printf("nsPlaintextEditor::StripCites()\n");
#endif

  nsCOMPtr<nsISelection> selection;
  nsresult rv = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(rv)) return rv;

  if (!selection)
    return NS_ERROR_NOT_INITIALIZED;
  PRBool isCollapsed;
  rv = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(rv)) return rv;

  // Variables we'll need either way
  nsAutoString format; format.AssignWithConversion("text/plain");
  nsAutoString current;
  nsString stripped;

  if (isCollapsed)    // rewrap the whole document
  {
    rv = OutputToString(current, format,
                               nsIDocumentEncoder::OutputFormatted);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsICiter> citer = dont_AddRef(MakeACiter());
    if (NS_FAILED(rv)) return rv;
    if (!citer) return NS_ERROR_UNEXPECTED;

    rv = citer->StripCites(current, stripped);
    if (NS_FAILED(rv)) return rv;

    rv = SelectAll();
    if (NS_FAILED(rv)) return rv;

    return InsertText(stripped);
  }
  else                // rewrap only the selection
  {
    rv = OutputToString(current, format,
                               nsIDocumentEncoder::OutputFormatted
                                | nsIDocumentEncoder::OutputSelectionOnly);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsICiter> citer = dont_AddRef(MakeACiter());
    if (NS_FAILED(rv)) return rv;
    if (!citer) return NS_ERROR_UNEXPECTED;

    rv = citer->StripCites(current, stripped);
    if (NS_FAILED(rv)) return rv;

    return InsertText(stripped);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPlaintextEditor::GetEmbeddedObjects(nsISupportsArray** aNodeList)
{
  *aNodeList = 0;
  return NS_OK;
}


#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIEditorIMESupport overrides 
#pragma mark -
#endif

NS_IMETHODIMP
nsPlaintextEditor::SetCompositionString(const nsAReadableString& aCompositionString, nsIPrivateTextRangeList* aTextRangeList,nsTextEventReply* aReply)
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
  
  nsCOMPtr<nsISelection> selection;
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
  caretP->SetCaretDOMSelection(selection);

  caretP->GetCaretCoordinates(nsICaret::eIMECoordinates, selection,
            &(aReply->mCursorPosition), &(aReply->mCursorIsCollapsed));

  // second part of 23558 fix:
  if (aCompositionString.IsEmpty()) 
  {
    mIMETextNode = nsnull;
  }
  
  return result;
}

NS_IMETHODIMP 
nsPlaintextEditor::GetReconversionString(nsReconversionEventReply* aReply)
{
  nsresult res;

  nsCOMPtr<nsISelection> selection;
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
  
  aReply->mReconversionString = (PRUnichar*) nsMemory::Clone(textValue.get(),
                                                                (textValue.Length() + 1) * sizeof(PRUnichar));
  if (!aReply->mReconversionString)
    return NS_ERROR_OUT_OF_MEMORY;

  // delete the selection
  res = DeleteSelection(eNone);
  
  return res;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  nsEditor overrides 
#pragma mark -
#endif


/** All editor operations which alter the doc should be prefaced
 *  with a call to StartOperation, naming the action and direction */
NS_IMETHODIMP
nsPlaintextEditor::StartOperation(PRInt32 opID, nsIEditor::EDirection aDirection)
{
  nsEditor::StartOperation(opID, aDirection);  // will set mAction, mDirection
  if (mRules) return mRules->BeforeEdit(mAction, mDirection);
  return NS_OK;
}


/** All editor operations which alter the doc should be followed
 *  with a call to EndOperation */
NS_IMETHODIMP
nsPlaintextEditor::EndOperation()
{
  // post processing
  nsresult res = NS_OK;
  if (mRules) res = mRules->AfterEdit(mAction, mDirection);
  nsEditor::EndOperation();  // will clear mAction, mDirection
  return res;
}  


NS_IMETHODIMP 
nsPlaintextEditor::SelectEntireDocument(nsISelection *aSelection)
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


NS_IMETHODIMP nsPlaintextEditor::GetLayoutObject(nsIDOMNode *aNode, nsISupports **aLayoutObject)
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


#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsPlaintextEditor::IsRootTag(nsString &aTag, PRBool &aIsTag)
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


#ifdef XP_MAC
#pragma mark -
#endif

void nsPlaintextEditor::HandleEventListenerError()
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

#ifdef XP_MAC
#pragma mark -
#endif


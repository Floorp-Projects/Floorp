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



#include "nsCOMPtr.h"
#include "nsVoidArray.h"

#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDiskDocument.h"
#include "nsIDOMElement.h"
#include "nsISelection.h"
#include "nsIDOMAttr.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindowInternal.h"
#include "nsITimer.h"

#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsITransactionManager.h"

#include "nsInterfaceState.h"


/*
static PRBool StringHashDestroyFunc(nsHashKey *aKey, void *aData, void* closure)
{
  nsString* stringData = NS_REINTERPRET_CAST(nsString*, aData);
  delete stringData;
  return PR_FALSE;
}

static void* StringHashCloneElementFunc(nsHashKey *aKey, void *aData, void* closure)
{
  nsString* stringData = NS_REINTERPRET_CAST(nsString*, aData);
  if (stringData)
  {
    nsString* newString = new nsString(*stringData);
    return newString;
  }
  
  return nsnull;
}

*/

#ifdef XP_MAC
#pragma mark -
#endif

nsInterfaceState::nsInterfaceState()
:  mEditor(nsnull)
,  mChromeDoc(nsnull)
,  mDOMWindow(nsnull)
,  mUpdateParagraph(PR_FALSE)
,  mUpdateFont(PR_FALSE)
,  mUpdateList(PR_FALSE)
,  mUpdateBold(PR_FALSE)
,  mUpdateItalics(PR_FALSE)
,  mUpdateUnderline(PR_FALSE)
,  mBoldState(eStateUninitialized)
,  mItalicState(eStateUninitialized)
,  mUnderlineState(eStateUninitialized)
,  mDirtyState(eStateUninitialized)
,  mSelectionCollapsed(eStateUninitialized)
,  mFirstDoOfFirstUndo(PR_TRUE)
{
	NS_INIT_REFCNT();
}

nsInterfaceState::~nsInterfaceState()
{
}

NS_IMPL_ADDREF(nsInterfaceState);
NS_IMPL_RELEASE(nsInterfaceState);
NS_IMPL_QUERY_INTERFACE4(nsInterfaceState, nsISelectionListener, nsIDocumentStateListener, nsITransactionListener, nsITimerCallback);

NS_IMETHODIMP
nsInterfaceState::Init(nsIHTMLEditor* aEditor, nsIDOMDocument *aChromeDoc)
{
  if (!aEditor)
    return NS_ERROR_INVALID_ARG;

  if (!aChromeDoc)
    return NS_ERROR_INVALID_ARG;

  mEditor = aEditor;		// no addreffing here
  mChromeDoc = aChromeDoc;
  
  // it sucks explicitly naming XUL nodes here. Would be better to have
  // some way to register things that we want to observe from JS
  mUpdateParagraph = XULNodeExists("ParagraphSelect");
  mUpdateFont = XULNodeExists("FontFaceSelect");
  mUpdateList = XULNodeExists("ulButton") ||  XULNodeExists("olButton");
  
  mUpdateBold = XULNodeExists("boldButton");
  mUpdateItalics = XULNodeExists("italicButton");
  mUpdateUnderline = XULNodeExists("underlineButton");
  
  return NS_OK;
}

NS_IMETHODIMP
nsInterfaceState::NotifyDocumentCreated()
{
  return NS_OK;
}

NS_IMETHODIMP
nsInterfaceState::NotifyDocumentWillBeDestroyed()
{
  // cancel any outstanding udpate timer
  if (mUpdateTimer)
    mUpdateTimer->Cancel();
  
  return NS_OK;
}


NS_IMETHODIMP
nsInterfaceState::NotifyDocumentStateChanged(PRBool aNowDirty)
{
  // update document modified. We should have some other notifications for this too.
  return UpdateDirtyState(aNowDirty);
}

NS_IMETHODIMP
nsInterfaceState::NotifySelectionChanged(nsIDOMDocument *, nsISelection *, short)
{
  return PrimeUpdateTimer();
}

#ifdef XP_MAC
#pragma mark -
#endif


NS_IMETHODIMP nsInterfaceState::WillDo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsInterfaceState::DidDo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, nsresult aDoResult)
{
  // only need to update if the status of the Undo menu item changes.
  PRInt32 undoCount;
  aManager->GetNumberOfUndoItems(&undoCount);
  if (undoCount == 1)
  {
    if (mFirstDoOfFirstUndo)
      CallUpdateCommands(NS_ConvertASCIItoUCS2("undo"));
    mFirstDoOfFirstUndo = PR_FALSE;
  }
	
  return NS_OK;
}

NS_IMETHODIMP nsInterfaceState::WillUndo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsInterfaceState::DidUndo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, nsresult aUndoResult)
{
  PRInt32 undoCount;
  aManager->GetNumberOfUndoItems(&undoCount);
  if (undoCount == 0)
    mFirstDoOfFirstUndo = PR_TRUE;    // reset the state for the next do

  CallUpdateCommands(NS_ConvertASCIItoUCS2("undo"));
  return NS_OK;
}

NS_IMETHODIMP nsInterfaceState::WillRedo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsInterfaceState::DidRedo(nsITransactionManager *aManager,  
  nsITransaction *aTransaction, nsresult aRedoResult)
{
  CallUpdateCommands(NS_ConvertASCIItoUCS2("undo"));
  return NS_OK;
}

NS_IMETHODIMP nsInterfaceState::WillBeginBatch(nsITransactionManager *aManager, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsInterfaceState::DidBeginBatch(nsITransactionManager *aManager, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP nsInterfaceState::WillEndBatch(nsITransactionManager *aManager, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsInterfaceState::DidEndBatch(nsITransactionManager *aManager, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP nsInterfaceState::WillMerge(nsITransactionManager *aManager,
        nsITransaction *aTopTransaction, nsITransaction *aTransactionToMerge, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsInterfaceState::DidMerge(nsITransactionManager *aManager,
  nsITransaction *aTopTransaction, nsITransaction *aTransactionToMerge,
                    PRBool aDidMerge, nsresult aMergeResult)
{
  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#endif


nsresult nsInterfaceState::PrimeUpdateTimer()
{
  nsresult rv = NS_OK;
    
  if (mUpdateTimer)
  {
    // i'd love to be able to just call SetDelay on the existing timer, but
    // i think i have to tear it down and make a new one.
    mUpdateTimer->Cancel();
    mUpdateTimer = NULL;      // free it
  }
  
  mUpdateTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_FAILED(rv)) return rv;

  const PRUint32 kUpdateTimerDelay = 150;
  return mUpdateTimer->Init(NS_STATIC_CAST(nsITimerCallback*, this), kUpdateTimerDelay);
}


void nsInterfaceState::TimerCallback()
{
  // if the selection state has changed, update stuff
  PRBool isCollapsed = SelectionIsCollapsed();
  if (isCollapsed != mSelectionCollapsed)
  {
    CallUpdateCommands(NS_ConvertASCIItoUCS2("select"));
    mSelectionCollapsed = isCollapsed;
  }
  
  // (void)ForceUpdate();
  CallUpdateCommands(NS_ConvertASCIItoUCS2("style"));
}


nsresult nsInterfaceState::CallUpdateCommands(const nsString& aCommand)
{
  if (!mDOMWindow)
  {
    nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
    if (!editor) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMDocument> domDoc;
    editor->GetDocument(getter_AddRefs(domDoc));
    if (!domDoc) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDocument> theDoc = do_QueryInterface(domDoc);
    if (!theDoc) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject;
    theDoc->GetScriptGlobalObject(getter_AddRefs(scriptGlobalObject));

    nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(scriptGlobalObject);
    if (!domWindow) return NS_ERROR_FAILURE;
    mDOMWindow = domWindow;
  }
  
  return mDOMWindow->UpdateCommands(aCommand);
}

NS_IMETHODIMP
nsInterfaceState::ForceUpdate(const PRUnichar *tagToUpdate)
{
  PRBool updateEverything = !tagToUpdate || !*tagToUpdate;

#if 0
  // This code is obsolete now that commanders are doing the work
  nsresult  rv;
    
  // update bold
  if (mUpdateBold && (updateEverything || nsAutoString("b").Equals(tagToUpdate)))
  {
    rv = UpdateTextState("b", "cmd_bold", "bold", mBoldState);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to update state");
  }
  
  // update italic
  if (mUpdateItalics && (updateEverything || nsAutoString("i").Equals(tagToUpdate)))
  {
    rv = UpdateTextState("i", "cmd_italic", "italic", mItalicState);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to update state");
  }

  // update underline
  if (mUpdateUnderline && (updateEverything || nsAutoString("u").Equals(tagToUpdate)))
  {
    rv = UpdateTextState("u", "cmd_underline", "underline", mUnderlineState);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to update state");
  }
  
  // update the paragraph format popup
  if (mUpdateParagraph && (updateEverything || nsAutoString("format").Equals(tagToUpdate)))
  {
    rv = UpdateParagraphState("Editor:Paragraph:Format", "format");
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to update state");
  }

  // udpate the font face
  if (mUpdateFont && (updateEverything || nsAutoString("font").Equals(tagToUpdate)))
  {
    rv = UpdateFontFace("Editor:Font:Face", "font");
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to update state");
  }
  
  // TODO: FINISH FONT FACE AND ADD FONT SIZE ("Editor:Font:Size", "fontsize", mFontSize)

  // update the list buttons
  if (mUpdateList && (updateEverything ||
        nsAutoString("ol").Equals(tagToUpdate) ||
        nsAutoString("ul").Equals(tagToUpdate)))
  {
    rv = UpdateListState("Editor:Paragraph:ListType");
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to update state");
  }
#endif

  return NS_OK;
}

PRBool
nsInterfaceState::SelectionIsCollapsed()
{
  nsresult rv;
  // we don't care too much about failures here.
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor, &rv);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsISelection> domSelection;
    rv = editor->GetSelection(getter_AddRefs(domSelection));
    if (NS_SUCCEEDED(rv))
    {    
      PRBool selectionCollapsed = PR_FALSE;
      rv = domSelection->GetIsCollapsed(&selectionCollapsed);
      return selectionCollapsed;
    }
  }
  return PR_FALSE;
}

nsresult
nsInterfaceState::UpdateParagraphState(const char* observerName, const char* attributeName)
{
  nsCOMPtr<nsISelection> domSelection;
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  // Get the nsIEditor pointer (mEditor is nsIHTMLEditor)
  if (!editor) return NS_ERROR_NULL_POINTER;
  nsresult rv = editor->GetSelection(getter_AddRefs(domSelection));
  if (NS_FAILED(rv)) return rv;

  PRBool selectionCollapsed = PR_FALSE;
  rv = domSelection->GetIsCollapsed(&selectionCollapsed);
  if (NS_FAILED(rv)) return rv;

  // Get anchor and focus nodes:
  nsCOMPtr<nsIDOMNode> anchorNode;
  rv = domSelection->GetAnchorNode(getter_AddRefs(anchorNode));
  if (NS_FAILED(rv)) return rv;
  if (!anchorNode) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> focusNode;
  rv = domSelection->GetFocusNode(getter_AddRefs(focusNode));
  if (NS_FAILED(rv)) return rv;
  if (!focusNode) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> anchorNodeBlockParent;
  PRBool isBlock;
  rv = editor->NodeIsBlock(anchorNode, isBlock);
  if (NS_FAILED(rv)) return rv;

  if (isBlock)
  {
    anchorNodeBlockParent = anchorNode;
  }
  else
  {
    // Get block parent of anchorNode node
    // We could simply use this if it was in nsIEditor interface!
    //rv = editor->GetBlockParent(anchorNode, getter_AddRefs(anchorNodeBlockParent));
    // if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDOMNode>parent;
    nsCOMPtr<nsIDOMNode>temp;
    rv = anchorNode->GetParentNode(getter_AddRefs(parent));
    while (NS_SUCCEEDED(rv) && parent)
    {
      rv = editor->NodeIsBlock(parent, isBlock);
      if (NS_FAILED(rv)) return rv;
      if (isBlock)
      {
        anchorNodeBlockParent = parent;
        break;
      }
      rv = parent->GetParentNode(getter_AddRefs(temp));
      parent = do_QueryInterface(temp);
    }
  }
  if (!anchorNodeBlockParent)  // return NS_ERROR_NULL_POINTER;
    anchorNodeBlockParent = anchorNode;

  nsAutoString tagName;

  // Check if we have a selection that extends into multiple nodes,
  //  so we can check for "mixed" selection state
  if (selectionCollapsed || focusNode == anchorNode)
  {
    // Entire selection is within one block
    anchorNodeBlockParent->GetNodeName(tagName);
  }
  else
  {
    // We may have different block parent node types WITHIN the selection,
    //  even if the anchor and focus parents are the same type.
    // Getting the list of all block, e.g., by using GetParagraphTags(&tagList)
    //  is too inefficient for long documents.
    // TODO: Change to use selection iterator to detect each block node in the 
    //  selection and if different from the anchorNodeBlockParent, use "mixed" state
    // *** Not doing this now reduces risk for Beta1 -- simply assume mixed state
    // Note that "mixed" displays as "normal" in UI as of 3/6. 
    tagName.AssignWithConversion("mixed");
  }

  if (tagName != mParagraphFormat)
  {
    rv = SetNodeAttribute(observerName, attributeName, tagName);
    if (NS_FAILED(rv)) return rv;
    mParagraphFormat = tagName;
  }
  return NS_OK;
}

nsresult
nsInterfaceState::UpdateListState(const char* observerName)
{
  nsresult  rv = NS_ERROR_NO_INTERFACE;
  nsAutoString tagStr;  // empty by default.
  
  PRBool bMixed, bOL, bUL, bDL;
  rv = mEditor->GetListState(bMixed, bOL, bUL, bDL);
  if (NS_FAILED(rv)) return rv;  

  if (bMixed)
    {} // leave tagStr empty
  else if (bOL)
    tagStr.AssignWithConversion("ol");
  else if (bUL)
    tagStr.AssignWithConversion("ul");
  else if (bDL)
    tagStr.AssignWithConversion("dl");
  // else leave tagStr empty
  
  rv = SetNodeAttribute(observerName, "format", tagStr);
    
  mListTag = tagStr;

  return rv;
}

nsresult
nsInterfaceState::UpdateFontFace(const char* observerName, const char* attributeName)
{
  nsresult  rv;
  
  PRBool    firstOfSelectionHasProp = PR_FALSE;
  PRBool    anyOfSelectionHasProp = PR_FALSE;
  PRBool    allOfSelectionHasProp = PR_FALSE;

  nsCOMPtr<nsIAtom> styleAtom = getter_AddRefs(NS_NewAtom("font"));
  nsAutoString faceStr; faceStr.AssignWithConversion("face");
  nsAutoString thisFace;
  
  // Use to test for "Default Fixed Width"
  nsCOMPtr<nsIAtom> fixedStyleAtom = getter_AddRefs(NS_NewAtom("tt"));

  PRBool testBoolean;
  
  rv = mEditor->GetInlineProperty(styleAtom, &faceStr, &thisFace, firstOfSelectionHasProp, anyOfSelectionHasProp, allOfSelectionHasProp);
  if( !anyOfSelectionHasProp )
  {
    // No font face set -- check for "tt". This can return an error if the selection isn't in a node
    rv = mEditor->GetInlineProperty(fixedStyleAtom, nsnull, nsnull, firstOfSelectionHasProp, anyOfSelectionHasProp, allOfSelectionHasProp);
    if (NS_SUCCEEDED(rv))
    {
      testBoolean = anyOfSelectionHasProp;
      if (anyOfSelectionHasProp)
        thisFace.AssignWithConversion("tt");
    }
    else
      rv = NS_OK;   // we don't want to propagate this error
  }

  // TODO: HANDLE "MIXED" STATE
  if (thisFace != mFontString)
  {
    rv = SetNodeAttribute(observerName, "face", thisFace);
    if (NS_SUCCEEDED(rv))
      mFontString = thisFace;
  }
  return rv;
}


nsresult
nsInterfaceState::UpdateTextState(const char* tagName, const char* observerName, const char* attributeName, PRInt8& ioState)
{
  nsresult  rv;
    
  nsCOMPtr<nsIAtom> styleAtom = getter_AddRefs(NS_NewAtom(tagName));

  PRBool testBoolean;
  PRBool firstOfSelectionHasProp = PR_FALSE;
  PRBool anyOfSelectionHasProp = PR_FALSE;
  PRBool allOfSelectionHasProp = PR_FALSE;

  rv = mEditor->GetInlineProperty(styleAtom, nsnull, nsnull, firstOfSelectionHasProp, anyOfSelectionHasProp, allOfSelectionHasProp);
  testBoolean = allOfSelectionHasProp;			// change this to alter the behaviour

  if (NS_FAILED(rv)) return rv;

  if (testBoolean != ioState)
  {
    rv = SetNodeAttribute(observerName, attributeName, NS_ConvertASCIItoUCS2(testBoolean ? "true" : "false"));
	  if (NS_FAILED(rv))
	    return rv;
	  
	  ioState = testBoolean;
  }
  
  return NS_OK;
}

nsresult
nsInterfaceState::UpdateDirtyState(PRBool aNowDirty)
{
  if (mDirtyState != aNowDirty)
  {
    CallUpdateCommands(NS_ConvertASCIItoUCS2("save"));

    mDirtyState = aNowDirty;
  }
  
  return NS_OK;  
}


PRBool
nsInterfaceState::XULNodeExists(const char* nodeID)
{
  nsresult rv;

  if (!mChromeDoc)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDOMElement> elem;
  rv = mChromeDoc->GetElementById( NS_ConvertASCIItoUCS2(nodeID), getter_AddRefs(elem) );
  
  return NS_SUCCEEDED(rv) && elem;
}

nsresult
nsInterfaceState::SetNodeAttribute(const char* nodeID, const char* attributeName, const nsString& newValue)
{
    nsresult rv = NS_OK;

  if (!mChromeDoc)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDOMElement> elem;
  rv = mChromeDoc->GetElementById( NS_ConvertASCIItoUCS2(nodeID), getter_AddRefs(elem) );
  if (NS_FAILED(rv) || !elem) return rv;
  
  return elem->SetAttribute(NS_ConvertASCIItoUCS2(attributeName), newValue);
}


nsresult
nsInterfaceState::UnsetNodeAttribute(const char* nodeID, const char* attributeName)
{
    nsresult rv = NS_OK;

  if (!mChromeDoc)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDOMElement> elem;
  rv = mChromeDoc->GetElementById( NS_ConvertASCIItoUCS2(nodeID), getter_AddRefs(elem) );
  if (NS_FAILED(rv) || !elem) return rv;
  
  return elem->RemoveAttribute(NS_ConvertASCIItoUCS2(attributeName));
}


#ifdef XP_MAC
#pragma mark -
#endif


void
nsInterfaceState::Notify(nsITimer *timer)
{
  NS_ASSERTION(timer == mUpdateTimer.get(), "Hey, this ain't my timer!");
  mUpdateTimer = NULL;    // release my hold  
  TimerCallback();
}

#ifdef XP_MAC
#pragma mark -
#endif


nsresult NS_NewInterfaceState(nsIHTMLEditor* aEditor, nsIDOMDocument* aChromeDoc, nsISelectionListener** aInstancePtrResult)
{
  nsInterfaceState* newThang = new nsInterfaceState;
  if (!newThang)
    return NS_ERROR_OUT_OF_MEMORY;

  *aInstancePtrResult = nsnull;
  nsresult rv = newThang->Init(aEditor, aChromeDoc);
  if (NS_FAILED(rv))
  {
    delete newThang;
    return rv;
  }
      
  return newThang->QueryInterface(NS_GET_IID(nsISelectionListener), (void **)aInstancePtrResult);
}

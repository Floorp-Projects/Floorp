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
#include "nsEditorEventListeners.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventReceiver.h" 
#include "nsIDOMKeyListener.h" 
#include "nsIDOMMouseListener.h"
#include "nsEditorCID.h"

#include "nsRepository.h"
#include "nsIServiceManager.h"
#include "nsITableCellLayout.h"   //For GetColIndexForCell


static NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMKeyListenerIID,   NS_IDOMKEYLISTENER_IID);

static NS_DEFINE_CID(kEditorCID,      NS_EDITOR_CID);
static NS_DEFINE_IID(kIEditorIID,     NS_IEDITOR_IID);
static NS_DEFINE_IID(kISupportsIID,   NS_ISUPPORTS_IID);

static NS_DEFINE_IID(kITextEditorIID, NS_ITEXTEDITOR_IID);
static NS_DEFINE_CID(kTextEditorCID,  NS_TEXTEDITOR_CID);
static NS_DEFINE_IID(kIHTMLEditorIID, NS_IHTMLEDITOR_IID);
static NS_DEFINE_CID(kHTMLEditorCID,  NS_HTMLEDITOR_CID);



nsHTMLEditor::nsHTMLEditor()
{
  NS_INIT_REFCNT();
}

nsHTMLEditor::~nsHTMLEditor()
{
  //the autopointers will clear themselves up. 
#if 0
// NO EVENT LISTERNERS YET
  //but we need to also remove the listeners or we have a leak
  //(This is identical to nsTextEditor code
  if (mEditor)
  {
    nsCOMPtr<nsIDOMDocument> doc;
    mEditor->GetDocument(getter_AddRefs(doc));
    if (doc)
    {
      nsCOMPtr<nsIDOMEventReceiver> erP;
      nsresult result = doc->QueryInterface(kIDOMEventReceiverIID, getter_AddRefs(erP));
      if (NS_SUCCEEDED(result) && erP) 
      {
        if (mKeyListenerP) {
          erP->RemoveEventListener(mKeyListenerP, kIDOMKeyListenerIID);
        }
        if (mMouseListenerP) {
          erP->RemoveEventListener(mMouseListenerP, kIDOMMouseListenerIID);
        }
      }
      else
        NS_NOTREACHED("~nsHTMLEditor");
    }
  }
#endif
}

nsresult nsHTMLEditor::InitHTMLEditor(nsIDOMDocument *aDoc, 
                                      nsIPresShell   *aPresShell,
                                      nsIEditorCallback *aCallback)
{
  NS_PRECONDITION(nsnull!=aDoc && nsnull!=aPresShell, "bad arg");
  nsresult result=NS_ERROR_NULL_POINTER;
  if ((nsnull!=aDoc) && (nsnull!=aPresShell))
  {
    nsITextEditor *aTextEditor = nsnull;
    result = nsRepository::CreateInstance(kTextEditorCID, nsnull,
                                          kITextEditorIID, (void **)&aTextEditor);

    if (NS_FAILED(result) || !aTextEditor) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mTextEditor = aTextEditor; // CreateInstance did our addRef

    // Initialize nsTextEditor -- this will create and initialize the base nsEditor
    // Note: nsTextEditor adds its own key, mouse, and DOM listners -- is that OK?
    result = mTextEditor->InitTextEditor(aDoc, aPresShell);
    if (NS_OK != result) {
      return result;
    }
    mTextEditor->EnableUndo(PR_TRUE);

    // Get the pointer to the base editor for easier access
    result = mTextEditor->QueryInterface(kIEditorIID, getter_AddRefs(mEditor));
    if (NS_OK != result) {
      return result;
    }
    result = NS_OK;
  }
  return result;
}

nsresult nsHTMLEditor::SetTextProperties(nsISupportsArray *aPropList)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mTextEditor->SetTextProperties(aPropList);
  }
  return result;
}

nsresult nsHTMLEditor::GetTextProperties(nsISupportsArray *aPropList)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mTextEditor->GetTextProperties(aPropList);
  }
  return result;
}

nsresult nsHTMLEditor::RemoveTextProperties(nsISupportsArray *aPropList)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mTextEditor->RemoveTextProperties(aPropList);
  }
  return result;
}

nsresult nsHTMLEditor::DeleteSelection(nsIEditor::Direction aDir)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->DeleteSelection(aDir);
  }
  return result;
}

nsresult nsHTMLEditor::InsertText(const nsString& aStringToInsert)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->InsertText(aStringToInsert);
  }
  return result;
}

nsresult nsHTMLEditor::InsertBreak(PRBool aCtrlKey)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mTextEditor->InsertBreak(aCtrlKey);
  }
  return result;
}

// Methods shared with the base editor.
// Note: We could call each of these via nsTextEditor -- is that better?
nsresult nsHTMLEditor::EnableUndo(PRBool aEnable)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->EnableUndo(aEnable);
  }
  return result;
}

nsresult nsHTMLEditor::Undo(PRUint32 aCount)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->Undo(aCount);
  }
  return result;
}

nsresult nsHTMLEditor::CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->CanUndo(aIsEnabled, aCanUndo);
  }
  return result;
}

nsresult nsHTMLEditor::Redo(PRUint32 aCount)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->Redo(aCount);
  }
  return result;
}

nsresult nsHTMLEditor::CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->CanRedo(aIsEnabled, aCanRedo);
  }
  return result;
}

nsresult nsHTMLEditor::BeginTransaction()
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->BeginTransaction();
  }
  return result;
}

nsresult nsHTMLEditor::EndTransaction()
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->EndTransaction();
  }
  return result;
}

nsresult nsHTMLEditor::MoveSelectionUp(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mTextEditor->MoveSelectionUp(aIncrement, aExtendSelection);
  }
  return result;
}

nsresult nsHTMLEditor::MoveSelectionDown(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mTextEditor->MoveSelectionDown(aIncrement, aExtendSelection);
  }
  return result;
}

nsresult nsHTMLEditor::MoveSelectionNext(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mTextEditor->MoveSelectionNext(aIncrement, aExtendSelection);
  }
  return result;
}

nsresult nsHTMLEditor::MoveSelectionPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mTextEditor->MoveSelectionPrevious(aIncrement, aExtendSelection);
  }
  return result;
}

nsresult nsHTMLEditor::SelectNext(nsIAtom *aIncrement, PRBool aExtendSelection) 
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mTextEditor->SelectNext(aIncrement, aExtendSelection);
  }
  return result;
}

nsresult nsHTMLEditor::SelectPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mTextEditor->SelectPrevious(aIncrement, aExtendSelection);
  }
  return result;
}

nsresult nsHTMLEditor::ScrollUp(nsIAtom *aIncrement)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mTextEditor->ScrollUp(aIncrement);
  }
  return result;
}

nsresult nsHTMLEditor::ScrollDown(nsIAtom *aIncrement)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mTextEditor->ScrollDown(aIncrement);
  }
  return result;
}

nsresult nsHTMLEditor::ScrollIntoView(PRBool aScrollToBegin)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mTextEditor->ScrollIntoView(aScrollToBegin);
  }
  return result;
}

nsresult nsHTMLEditor::Insert(nsIInputStream *aInputStream)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mTextEditor->Insert(aInputStream);
  }
  return result;
}

nsresult nsHTMLEditor::OutputText(nsIOutputStream *aOutputStream)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mTextEditor->OutputText(aOutputStream);
  }
  return result;
}

nsresult nsHTMLEditor::OutputHTML(nsIOutputStream *aOutputStream)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mTextEditor->OutputHTML(aOutputStream);
  }
  return result;
}


NS_IMPL_ADDREF(nsHTMLEditor)

NS_IMPL_RELEASE(nsHTMLEditor)

nsresult
nsHTMLEditor::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIHTMLEditorIID)) {
    *aInstancePtr = (void*)(nsIHTMLEditor*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  // If our pointer to nsTextEditor is null, don't bother querying?
  if (aIID.Equals(kITextEditorIID) && (mTextEditor)) {
      nsCOMPtr<nsIEditor> editor;
      nsresult result = mTextEditor->QueryInterface(kITextEditorIID, getter_AddRefs(editor));
      if (NS_SUCCEEDED(result) && editor) {
        *aInstancePtr = (void*)editor;
        return NS_OK;
      }
  }
  if (aIID.Equals(kIEditorIID) && (mEditor)) {
      nsCOMPtr<nsIEditor> editor;
      nsresult result = mEditor->QueryInterface(kIEditorIID, getter_AddRefs(editor));
      if (NS_SUCCEEDED(result) && editor) {
        *aInstancePtr = (void*)editor;
        return NS_OK;
      }
  }
  return NS_NOINTERFACE;
}

//================================================================
// HTML Editor methods
//
// Note: Table Editing methods are implemented in EditTable.cpp
//

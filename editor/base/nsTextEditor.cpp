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
#include "nsEditorEventListeners.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventReceiver.h" 
#include "nsIDOMKeyListener.h" 
#include "nsIDOMMouseListener.h"
#include "nsEditorCID.h"

#include "nsRepository.h"
#include "nsIServiceManager.h"

static NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMKeyListenerIID,   NS_IDOMKEYLISTENER_IID);

static NS_DEFINE_CID(kEditorCID,      NS_EDITOR_CID);
static NS_DEFINE_IID(kIEditorIID,     NS_IEDITOR_IID);
static NS_DEFINE_IID(kISupportsIID,   NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITextEditorIID, NS_ITEXTEDITOR_IID);
static NS_DEFINE_CID(kTextEditorCID,  NS_TEXTEDITOR_CID);



nsTextEditor::nsTextEditor()
{
  NS_INIT_REFCNT();
}

nsTextEditor::~nsTextEditor()
{
  //the autopointers will clear themselves up. 
  //but we need to also remove the listeners or we have a leak
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
        NS_NOTREACHED("~nsTextEditor");
    }
  }
}

nsresult nsTextEditor::InitTextEditor(nsIDOMDocument *aDoc, 
                                      nsIPresShell   *aPresShell,
                                      nsIEditorCallback *aCallback)
{
  NS_PRECONDITION(nsnull!=aDoc && nsnull!=aPresShell, "bad arg");
  nsresult result=NS_ERROR_NULL_POINTER;
  if ((nsnull!=aDoc) && (nsnull!=aPresShell))
  {
    // get the editor
    nsIEditor *editor = nsnull;
    result = nsRepository::CreateInstance(kEditorCID, nsnull,
                                          kIEditorIID, (void **)&editor);
    if (NS_FAILED(result) || !editor) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mEditor = editor; // CreateInstance did our addRef

    mEditor->Init(aDoc, aPresShell);
    mEditor->EnableUndo(PR_TRUE);

    result = NS_NewEditorKeyListener(getter_AddRefs(mKeyListenerP), this);
    if (NS_OK != result) {
      return result;
    }
    result = NS_NewEditorMouseListener(getter_AddRefs(mMouseListenerP), this);
    if (NS_OK != result) {
      mKeyListenerP = 0; // drop the key listener if we couldn't get a mouse listener
      return result;
    }
    nsCOMPtr<nsIDOMEventReceiver> erP;
    result = aDoc->QueryInterface(kIDOMEventReceiverIID, getter_AddRefs(erP));
    if (NS_OK != result) 
    {
      mKeyListenerP = 0;
      mMouseListenerP = 0; //dont need these if we cant register them
      return result;
    }
    erP->AddEventListener(mKeyListenerP, kIDOMKeyListenerIID);
    //erP->AddEventListener(mMouseListenerP, kIDOMMouseListenerIID);

    result = NS_OK;
  }
  return result;
}

nsresult nsTextEditor::SetTextProperties(nsVoidArray *aPropList)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::GetTextProperties(nsVoidArray *aPropList)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::RemoveTextProperties(nsVoidArray *aPropList)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::DeleteSelection(nsIEditor::Direction aDir)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->DeleteSelection(aDir);
  }
  return result;
}

nsresult nsTextEditor::InsertText(const nsString& aStringToInsert)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->InsertText(aStringToInsert);
  }
  return result;
}

nsresult nsTextEditor::InsertBreak(PRBool aCtrlKey)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::EnableUndo(PRBool aEnable)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->EnableUndo(aEnable);
  }
  return result;
}

nsresult nsTextEditor::Undo(PRUint32 aCount)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->Undo(aCount);
  }
  return result;
}

nsresult nsTextEditor::CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->CanUndo(aIsEnabled, aCanUndo);
  }
  return result;
}

nsresult nsTextEditor::Redo(PRUint32 aCount)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->Redo(aCount);
  }
  return result;
}

nsresult nsTextEditor::CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->CanRedo(aIsEnabled, aCanRedo);
  }
  return result;
}

nsresult nsTextEditor::BeginTransaction()
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->BeginTransaction();
  }
  return result;
}

nsresult nsTextEditor::EndTransaction()
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->EndTransaction();
  }
  return result;
}

nsresult nsTextEditor::MoveSelectionUp(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::MoveSelectionDown(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::MoveSelectionNext(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::MoveSelectionPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::SelectNext(nsIAtom *aIncrement, PRBool aExtendSelection) 
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::SelectPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::ScrollUp(nsIAtom *aIncrement)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::ScrollDown(nsIAtom *aIncrement)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::ScrollIntoView(PRBool aScrollToBegin)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::Insert(nsIInputStream *aInputStream)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::OutputText(nsIOutputStream *aOutputStream)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::OutputHTML(nsIOutputStream *aOutputStream)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}


NS_IMPL_ADDREF(nsTextEditor)

NS_IMPL_RELEASE(nsTextEditor)

nsresult
nsTextEditor::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kITextEditorIID)) {
    *aInstancePtr = (void*)(nsITextEditor*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

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

#include "nsIComponentManager.h"
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
  return Inherited::AddRef();
}

//NS_IMPL_RELEASE_INHERITED(Class, Super)
NS_IMETHODIMP_(nsrefcnt) nsHTMLEditor::Release(void)
{
  return Inherited::Release();
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
  return Inherited::QueryInterface(aIID, aInstancePtr);
}


NS_IMETHODIMP nsHTMLEditor::Init(nsIDOMDocument *aDoc, nsIPresShell *aPresShell)
{
  NS_PRECONDITION(nsnull!=aDoc && nsnull!=aPresShell, "bad arg");
  nsresult result=NS_ERROR_NULL_POINTER;
  if ((nsnull!=aDoc) && (nsnull!=aPresShell))
  {
    return Inherited::Init(aDoc, aPresShell);
  }
  return result;
}

NS_IMETHODIMP nsHTMLEditor::SetTextProperty(nsIAtom *aProperty)
{
  return Inherited::SetTextProperty(aProperty);
}

NS_IMETHODIMP nsHTMLEditor::GetTextProperty(nsIAtom *aProperty, PRBool &aAny, PRBool &aAll)
{
  return Inherited::GetTextProperty(aProperty, aAny, aAll);
}

NS_IMETHODIMP nsHTMLEditor::RemoveTextProperty(nsIAtom *aProperty)
{
  return Inherited::RemoveTextProperty(aProperty);
}

NS_IMETHODIMP nsHTMLEditor::DeleteSelection(nsIEditor::Direction aDir)
{
  return Inherited::DeleteSelection(aDir);
}

NS_IMETHODIMP nsHTMLEditor::InsertText(const nsString& aStringToInsert)
{
  return Inherited::InsertText(aStringToInsert);
}

NS_IMETHODIMP nsHTMLEditor::InsertBreak()
{
  return Inherited::InsertBreak();
}

// Methods shared with the base editor.
// Note: We could call each of these via nsTextEditor -- is that better?
NS_IMETHODIMP nsHTMLEditor::EnableUndo(PRBool aEnable)
{
  return Inherited::EnableUndo(aEnable);
}

NS_IMETHODIMP nsHTMLEditor::Undo(PRUint32 aCount)
{
  return Inherited::Undo(aCount);
}

NS_IMETHODIMP nsHTMLEditor::CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo)
{
  return Inherited::CanUndo(aIsEnabled, aCanUndo);
}

NS_IMETHODIMP nsHTMLEditor::Redo(PRUint32 aCount)
{
  return Inherited::Redo(aCount);
}

NS_IMETHODIMP nsHTMLEditor::CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo)
{
  return Inherited::CanRedo(aIsEnabled, aCanRedo);
}

NS_IMETHODIMP nsHTMLEditor::BeginTransaction()
{
  return Inherited::BeginTransaction();
}

NS_IMETHODIMP nsHTMLEditor::EndTransaction()
{
  return Inherited::EndTransaction();
}

NS_IMETHODIMP nsHTMLEditor::MoveSelectionUp(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return Inherited::MoveSelectionUp(aIncrement, aExtendSelection);
}

NS_IMETHODIMP nsHTMLEditor::MoveSelectionDown(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return Inherited::MoveSelectionDown(aIncrement, aExtendSelection);
}

NS_IMETHODIMP nsHTMLEditor::MoveSelectionNext(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return Inherited::MoveSelectionNext(aIncrement, aExtendSelection);
}

NS_IMETHODIMP nsHTMLEditor::MoveSelectionPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return Inherited::MoveSelectionPrevious(aIncrement, aExtendSelection);
}

NS_IMETHODIMP nsHTMLEditor::SelectNext(nsIAtom *aIncrement, PRBool aExtendSelection) 
{
  return Inherited::SelectNext(aIncrement, aExtendSelection);
}

NS_IMETHODIMP nsHTMLEditor::SelectPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return Inherited::SelectPrevious(aIncrement, aExtendSelection);
}

NS_IMETHODIMP nsHTMLEditor::ScrollUp(nsIAtom *aIncrement)
{
  return Inherited::ScrollUp(aIncrement);
}

NS_IMETHODIMP nsHTMLEditor::ScrollDown(nsIAtom *aIncrement)
{
  return Inherited::ScrollDown(aIncrement);
}

NS_IMETHODIMP nsHTMLEditor::ScrollIntoView(PRBool aScrollToBegin)
{
  return Inherited::ScrollIntoView(aScrollToBegin);
}

NS_IMETHODIMP nsHTMLEditor::Insert(nsIInputStream *aInputStream)
{
  return Inherited::Insert(aInputStream);
}

NS_IMETHODIMP nsHTMLEditor::OutputText(nsString& aOutputString)
{
  return Inherited::OutputText(aOutputString);
}

NS_IMETHODIMP nsHTMLEditor::OutputHTML(nsString& aOutputString)
{
  return Inherited::OutputHTML(aOutputString);
}

//================================================================
// HTML Editor methods
//
// Note: Table Editing methods are implemented in EditTable.cpp
//

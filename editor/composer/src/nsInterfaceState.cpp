/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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



#include "nsCOMPtr.h"
#include "nsVoidArray.h"

#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDiskDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMSelection.h"

#include "nsIEditor.h"
#include "nsIHTMLEditor.h"

#include "nsInterfaceState.h"



nsInterfaceState::nsInterfaceState()
:  mEditor(nsnull)
,  mWebShell(nsnull)
,  mBoldState(eStateUninitialized)
,  mItalicState(eStateUninitialized)
,  mUnderlineState(eStateUninitialized)
,  mDirtyState(eStateUninitialized)
{
	NS_INIT_REFCNT();
}

nsInterfaceState::~nsInterfaceState()
{
}

NS_IMPL_ADDREF(nsInterfaceState);
NS_IMPL_RELEASE(nsInterfaceState);

NS_IMETHODIMP
nsInterfaceState::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  
  *aInstancePtr = nsnull;
  if (aIID.Equals(nsIDOMSelectionListener::GetIID()))
  {
    *aInstancePtr = (void*)(nsIDOMSelectionListener*)this;
    AddRef();
    return NS_OK;
  }
  
  if (aIID.Equals(nsIDocumentStateListener::GetIID()))
  {
    *aInstancePtr = (void*)(nsIDocumentStateListener*)this;
    AddRef();
    return NS_OK;
  }
  
  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
  {
    *aInstancePtr = (void*)(nsISupports *)(nsIDOMSelectionListener*)this;
    AddRef();
    return NS_OK;
  }
  
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsInterfaceState::Init(nsIHTMLEditor* aEditor, nsIWebShell *aChromeWebShell)
{
  if (!aEditor)
    return NS_ERROR_INVALID_ARG;

  if (!aChromeWebShell)
    return NS_ERROR_INVALID_ARG;

  mEditor = aEditor;		// no addreffing here
  mWebShell = aChromeWebShell;
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
  return NS_OK;
}


NS_IMETHODIMP
nsInterfaceState::NotifyDocumentStateChanged(PRBool aNowDirty)
{
  // update document modified. We should have some other notifications for this too.
  return UpdateDirtyState(aNowDirty);
}

NS_IMETHODIMP
nsInterfaceState::NotifySelectionChanged()
{
  nsresult  rv;
  
  // we don't really care if any of these fail.
  
  // update bold
  rv = UpdateTextState("b", "Editor:Style:IsBold", "bold", mBoldState);
  // update italic
  rv = UpdateTextState("i", "Editor:Style:IsItalic", "italic", mItalicState);
  // update underline
  rv = UpdateTextState("u", "Editor:Style:IsUnderline", "underline", mUnderlineState);
  
  // udpate the font face
  rv = UpdateFontFace("Editor:Font:Face", "font", mFontString);
  
  // update the paragraph format popup
  rv = UpdateParagraphState("Editor:Paragraph:Format", "format", mParagraphFormat);
  
  // update the list buttons
  rv = UpdateListState("Editor:Paragraph:List", "ol");

  return NS_OK;
}


nsresult
nsInterfaceState::UpdateParagraphState(const char* observerName, const char* attributeName, nsString& ioParaFormat)
{
  nsStringArray tagList;
  
  mEditor->GetParagraphStyle(&tagList);

  PRInt32 numTags = tagList.Count();
  if (numTags > 0)
  {
    nsAutoString  thisTag;
    tagList.StringAt(0, thisTag);
    if (thisTag != mParagraphFormat)
    {
      nsresult rv = SetNodeAttribute(observerName, attributeName, thisTag);
      if (NS_FAILED(rv)) return rv;
      mParagraphFormat = thisTag;
    }
  }
  
  return NS_OK;
}

nsresult
nsInterfaceState::UpdateListState(const char* observerName, const char* tagName)
{
  nsresult  rv = NS_ERROR_NO_INTERFACE;

  nsCOMPtr<nsIDOMSelection>  domSelection;
  {
    nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
    editor->GetSelection(getter_AddRefs(domSelection));
  }

  nsAutoString  tagStr(tagName);
  
  nsCOMPtr<nsIDOMNode>       domNode;
  if (domSelection)
    domSelection->GetAnchorNode(getter_AddRefs(domNode));
  
  nsCOMPtr<nsIDOMElement> parentElement;
  rv = mEditor->GetElementOrParentByTagName(tagStr, domNode, getter_AddRefs(parentElement));

  return rv;
}

nsresult
nsInterfaceState::UpdateFontFace(const char* observerName, const char* attributeName, nsString& ioFontString)
{
  nsresult  rv;
  
  PRBool    firstOfSelectionHasProp = PR_FALSE;
  PRBool    anyOfSelectionHasProp = PR_FALSE;
  PRBool    allOfSelectionHasProp = PR_FALSE;

  nsCOMPtr<nsIAtom> styleAtom = getter_AddRefs(NS_NewAtom("font"));
  nsAutoString faceStr("face");
  
  rv = mEditor->GetInlineProperty(styleAtom, &faceStr, nsnull, firstOfSelectionHasProp, anyOfSelectionHasProp, allOfSelectionHasProp);
  
  return rv;
}


nsresult
nsInterfaceState::UpdateTextState(const char* tagName, const char* observerName, const char* attributeName, PRInt8& ioState)
{
  nsresult  rv;
  
  PRBool    firstOfSelectionHasProp = PR_FALSE;
  PRBool    anyOfSelectionHasProp = PR_FALSE;
  PRBool    allOfSelectionHasProp = PR_FALSE;
  
  nsCOMPtr<nsIAtom> styleAtom = getter_AddRefs(NS_NewAtom(tagName));

  rv = mEditor->GetInlineProperty(styleAtom, nsnull, nsnull, firstOfSelectionHasProp, anyOfSelectionHasProp, allOfSelectionHasProp);
 
  PRBool    &behaviour = allOfSelectionHasProp;			// change this to alter the behaviour
  if (behaviour != ioState)
  {
    rv = SetNodeAttribute(observerName, attributeName, behaviour ? "true" : "false");
	  if (NS_FAILED(rv))
	    return rv;
	  
	  ioState = behaviour;
  }
  
  return rv;
}

nsresult
nsInterfaceState::UpdateDirtyState(PRBool aNowDirty)
{
  if (mDirtyState != aNowDirty)
  {
    nsresult rv = SetNodeAttribute("Editor:Document:Dirty", "dirty", aNowDirty ? "true" : "false");
	  if (NS_FAILED(rv)) return rv;
    mDirtyState = aNowDirty;
  }
  
  return NS_OK;  
}


nsresult
nsInterfaceState::SetNodeAttribute(const char* nodeID, const char* attributeName, const nsString& newValue)
{
    nsresult rv = NS_OK;

  if (!mWebShell)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIContentViewer> cv;
  rv = mWebShell->GetContentViewer(getter_AddRefs(cv));
  if (NS_FAILED(rv)) return rv;
    
  nsCOMPtr<nsIDocumentViewer> docViewer = do_QueryInterface(cv, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDocument> chromeDoc;
  rv = docViewer->GetDocument(*getter_AddRefs(chromeDoc));
  if (NS_FAILED(rv)) return rv;
 
  nsCOMPtr<nsIDOMXULDocument> xulDoc = do_QueryInterface(chromeDoc, &rv);
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIDOMElement> elem;
  rv = xulDoc->GetElementById( nodeID, getter_AddRefs(elem) );
  if (NS_FAILED(rv) || !elem) return rv;
  
  return elem->SetAttribute(attributeName, newValue);
}


nsresult NS_NewInterfaceState(nsIHTMLEditor* aEditor, nsIWebShell* aWebShell, nsIDOMSelectionListener** aInstancePtrResult)
{
  nsInterfaceState* newThang = new nsInterfaceState;
  if (!newThang)
    return NS_ERROR_OUT_OF_MEMORY;

  *aInstancePtrResult = nsnull;
  nsresult rv = newThang->Init(aEditor, aWebShell);
  if (NS_FAILED(rv))
  {
    delete newThang;
    return rv;
  }
      
  return newThang->QueryInterface(nsIDOMSelectionListener::GetIID(), (void **)aInstancePtrResult);
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsIComponentManager.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDOMWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMDocument.h"
#include "nsDocShellEditorData.h"

/*---------------------------------------------------------------------------

  nsDocShellEditorData

----------------------------------------------------------------------------*/

nsDocShellEditorData::nsDocShellEditorData(nsIDocShell* inOwningDocShell)
: mDocShell(inOwningDocShell)
, mMakeEditable(false)
, mIsDetached(false)
, mDetachedMakeEditable(false)
, mDetachedEditingState(nsIHTMLDocument::eOff)
{
  NS_ASSERTION(mDocShell, "Where is my docShell?");
}


/*---------------------------------------------------------------------------

  ~nsDocShellEditorData

----------------------------------------------------------------------------*/
nsDocShellEditorData::~nsDocShellEditorData()
{
  TearDownEditor();
}

void
nsDocShellEditorData::TearDownEditor()
{
  if (mEditor) {
    mEditor->PreDestroy(false);
    mEditor = nsnull;
  }
  mEditingSession = nsnull;
  mIsDetached = false;
}


/*---------------------------------------------------------------------------

  MakeEditable

----------------------------------------------------------------------------*/
nsresult
nsDocShellEditorData::MakeEditable(bool inWaitForUriLoad)
{
  if (mMakeEditable)
    return NS_OK;
  
  // if we are already editable, and are getting turned off,
  // nuke the editor.
  if (mEditor)
  {
    NS_WARNING("Destroying existing editor on frame");
    
    mEditor->PreDestroy(false);
    mEditor = nsnull;
  }
  
  if (inWaitForUriLoad)
    mMakeEditable = true;
  return NS_OK;
}


/*---------------------------------------------------------------------------

  GetEditable

----------------------------------------------------------------------------*/
bool
nsDocShellEditorData::GetEditable()
{
  return mMakeEditable || (mEditor != nsnull);
}

/*---------------------------------------------------------------------------

  CreateEditor

----------------------------------------------------------------------------*/
nsresult
nsDocShellEditorData::CreateEditor()
{
  nsCOMPtr<nsIEditingSession>   editingSession;    
  nsresult rv = GetEditingSession(getter_AddRefs(editingSession));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIDOMWindow>    domWindow = do_GetInterface(mDocShell);
  rv = editingSession->SetupEditorOnWindow(domWindow);
  if (NS_FAILED(rv)) return rv;
  
  return NS_OK;
}


/*---------------------------------------------------------------------------

  GetEditingSession

----------------------------------------------------------------------------*/
nsresult
nsDocShellEditorData::GetEditingSession(nsIEditingSession **outEditingSession)
{
  nsresult rv = EnsureEditingSession();
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*outEditingSession = mEditingSession);

  return NS_OK;
}


/*---------------------------------------------------------------------------

  GetEditor

----------------------------------------------------------------------------*/
nsresult
nsDocShellEditorData::GetEditor(nsIEditor **outEditor)
{
  NS_ENSURE_ARG_POINTER(outEditor);
  NS_IF_ADDREF(*outEditor = mEditor);
  return NS_OK;
}


/*---------------------------------------------------------------------------

  SetEditor

----------------------------------------------------------------------------*/
nsresult
nsDocShellEditorData::SetEditor(nsIEditor *inEditor)
{
  // destroy any editor that we have. Checks for equality are
  // necessary to ensure that assigment into the nsCOMPtr does
  // not temporarily reduce the refCount of the editor to zero
  if (mEditor.get() != inEditor)
  {
    if (mEditor)
    {
      mEditor->PreDestroy(false);
      mEditor = nsnull;
    }
      
    mEditor = inEditor;    // owning addref
    if (!mEditor)
      mMakeEditable = false;
  }   
  
  return NS_OK;
}


/*---------------------------------------------------------------------------

  EnsureEditingSession
  
  This creates the editing session on the content docShell that owns
  'this'.

----------------------------------------------------------------------------*/
nsresult
nsDocShellEditorData::EnsureEditingSession()
{
  NS_ASSERTION(mDocShell, "Should have docShell here");
  NS_ASSERTION(!mIsDetached, "This will stomp editing session!");
  
  nsresult rv = NS_OK;
  
  if (!mEditingSession)
  {
    mEditingSession =
      do_CreateInstance("@mozilla.org/editor/editingsession;1", &rv);
  }

  return rv;
}

nsresult
nsDocShellEditorData::DetachFromWindow()
{
  NS_ASSERTION(mEditingSession,
               "Can't detach when we don't have a session to detach!");
  
  nsCOMPtr<nsIDOMWindow> domWindow = do_GetInterface(mDocShell);
  nsresult rv = mEditingSession->DetachFromWindow(domWindow);
  NS_ENSURE_SUCCESS(rv, rv);

  mIsDetached = true;
  mDetachedMakeEditable = mMakeEditable;
  mMakeEditable = false;

  nsCOMPtr<nsIDOMDocument> domDoc;
  domWindow->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(domDoc);
  if (htmlDoc)
    mDetachedEditingState = htmlDoc->GetEditingState();

  mDocShell = nsnull;

  return NS_OK;
}

nsresult
nsDocShellEditorData::ReattachToWindow(nsIDocShell* aDocShell)
{
  mDocShell = aDocShell;

  nsCOMPtr<nsIDOMWindow> domWindow = do_GetInterface(mDocShell);
  nsresult rv = mEditingSession->ReattachToWindow(domWindow);
  NS_ENSURE_SUCCESS(rv, rv);

  mIsDetached = false;
  mMakeEditable = mDetachedMakeEditable;

  nsCOMPtr<nsIDOMDocument> domDoc;
  domWindow->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(domDoc);
  if (htmlDoc)
    htmlDoc->SetEditingState(mDetachedEditingState);
 
  return NS_OK;
}

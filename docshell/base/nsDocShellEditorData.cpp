/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Simon Fraser <sfraser@netscape.com>
 */


#include "nsIComponentManager.h"
#include "nsIInterfaceRequestorUtils.h"

#include "nsIDOMWindow.h"
#include "nsIDocShellTreeItem.h"

#include "nsDocShellEditorData.h"


/*---------------------------------------------------------------------------

  nsDocShellEditorData

----------------------------------------------------------------------------*/

nsDocShellEditorData::nsDocShellEditorData(nsIDocShell* inOwningDocShell)
: mDocShell(inOwningDocShell)
, mMakeEditable(PR_FALSE)
{
  NS_ASSERTION(mDocShell, "Where is my docShell?");
}


/*---------------------------------------------------------------------------

  ~nsDocShellEditorData

----------------------------------------------------------------------------*/
nsDocShellEditorData::~nsDocShellEditorData()
{
  if (mEditor)
  {
    mEditor->PreDestroy();
    mEditor = nsnull;     // explicit clear to make destruction order predictable
  }
  
  mEditingSession = nsnull;
}


/*---------------------------------------------------------------------------

  MakeEditable

----------------------------------------------------------------------------*/
nsresult
nsDocShellEditorData::MakeEditable(PRBool inWaitForUriLoad /*, PRBool inEditable */)
{
  if (mMakeEditable)
    return NS_OK;
  
  // if we are already editable, and are getting turned off,
  // nuke the editor.
  if (mEditor)
  {
    NS_WARNING("Destroying existing editor on frame");
    
    mEditor->PreDestroy();
    mEditor = nsnull;
  }
  
  mMakeEditable = PR_TRUE;
  return NS_OK;
}


/*---------------------------------------------------------------------------

  GetEditable

----------------------------------------------------------------------------*/
PRBool
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
  NS_ENSURE_ARG_POINTER(outEditingSession);
  return GetOrCreateEditingSession(outEditingSession);
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
      mEditor->PreDestroy();
      mEditor = nsnull;
    }
      
    mEditor = inEditor;    // owning addref
  }   
  
  return NS_OK;
}


/*---------------------------------------------------------------------------

  GetOrCreateEditingSession
  
  This creates the editing session on the content root docShell,
  irrespective of the shell owning 'this'.

----------------------------------------------------------------------------*/
nsresult
nsDocShellEditorData::GetOrCreateEditingSession(nsIEditingSession **outEditingSession)
{
  NS_ENSURE_ARG_POINTER(outEditingSession);
  *outEditingSession = nsnull;
  
  NS_ASSERTION(mDocShell, "Should have docShell here");
  
  nsresult rv = NS_OK;
  
  nsCOMPtr<nsIDocShellTreeItem> owningShell = do_QueryInterface(mDocShell);
  if (!owningShell) return NS_ERROR_FAILURE;
  
  // Get the root docshell
  nsCOMPtr<nsIDocShellTreeItem> contentRootShell;
  owningShell->GetSameTypeRootTreeItem(getter_AddRefs(contentRootShell));
  if (!contentRootShell) return NS_ERROR_FAILURE;
  
  if (contentRootShell.get() == owningShell.get())
  {
    // if we're on the root shell, go ahead and create the editing shell
    // if necessary.
    if (!mEditingSession)
    {
      mEditingSession = do_CreateInstance("@mozilla.org/editor/editingsession;1", &rv);
      if (NS_FAILED(rv)) return rv;

      nsCOMPtr<nsIDOMWindow> domWindow(do_GetInterface(mDocShell, &rv));
      if (NS_FAILED(rv)) return rv;

      rv = mEditingSession->Init(domWindow);
      if (NS_FAILED(rv)) return rv;
    }
    
    NS_ADDREF(*outEditingSession = mEditingSession.get());
  }
  else
  {
    // otherwise we're on a subshell. In this case, call GetInterface
    // on the root shell, which will come back into this routine,
    // to the block above, and give us back the editing session on
    // the content root.
    nsCOMPtr<nsIEditingSession> editingSession = do_GetInterface(contentRootShell);
    NS_ASSERTION(editingSession, "should have been able to get the editing session here");
    NS_IF_ADDREF(*outEditingSession = editingSession.get());
  }
  
  return (*outEditingSession) ? NS_OK : NS_ERROR_FAILURE;
}


/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser <sfraser@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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
, mMakeEditable(PR_FALSE)
, mIsDetached(PR_FALSE)
, mDetachedMakeEditable(PR_FALSE)
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
    mEditor->PreDestroy(PR_FALSE);
    mEditor = nsnull;
  }
  mEditingSession = nsnull;
  mIsDetached = PR_FALSE;
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
    
    mEditor->PreDestroy(PR_FALSE);
    mEditor = nsnull;
  }
  
  if (inWaitForUriLoad)
    mMakeEditable = PR_TRUE;
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
      mEditor->PreDestroy(PR_FALSE);
      mEditor = nsnull;
    }
      
    mEditor = inEditor;    // owning addref
    if (!mEditor)
      mMakeEditable = PR_FALSE;
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

  mIsDetached = PR_TRUE;
  mDetachedMakeEditable = mMakeEditable;
  mMakeEditable = PR_FALSE;

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

  mIsDetached = PR_FALSE;
  mMakeEditable = mDetachedMakeEditable;

  nsCOMPtr<nsIDOMDocument> domDoc;
  domWindow->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(domDoc);
  if (htmlDoc)
    htmlDoc->SetEditingState(mDetachedEditingState);
 
  return NS_OK;
}

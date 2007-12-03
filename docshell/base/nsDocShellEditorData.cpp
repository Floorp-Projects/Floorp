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
  TearDownEditor();
}

void
nsDocShellEditorData::TearDownEditor()
{
  if (mEditingSession)
  {
    nsCOMPtr<nsIDOMWindow> domWindow = do_GetInterface(mDocShell);
    // This will eventually call nsDocShellEditorData::SetEditor(nsnull)
    //   which will call mEditorPreDestroy() and delete the editor
    mEditingSession->TearDownEditorOnWindow(domWindow);
  }
  else if (mEditor) // Should never have this w/o nsEditingSession!
  {
    mEditor->PreDestroy();
    mEditor = nsnull;     // explicit clear to make destruction order predictable
  }
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
  
  if (inWaitForUriLoad)
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
      mEditor->PreDestroy();
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
  
  nsresult rv = NS_OK;
  
  if (!mEditingSession)
  {
    mEditingSession =
      do_CreateInstance("@mozilla.org/editor/editingsession;1", &rv);
  }

  return rv;
}


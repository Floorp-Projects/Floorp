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
 *      Simon Fraser <sfraser@netscape.com>
 *
 */

#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMDocument.h"
#include "nsIScriptGlobalObject.h"

#include "nsIControllers.h"
#include "nsIController.h"
#include "nsIEditorController.h"

#include "nsIPresShell.h"

#include "nsEditingSession.h"

#include "nsIComponentManager.h"

/*---------------------------------------------------------------------------

  nsEditingSession

----------------------------------------------------------------------------*/
nsEditingSession::nsEditingSession()
{
  NS_INIT_ISUPPORTS();
}

/*---------------------------------------------------------------------------

  ~nsEditingSession

----------------------------------------------------------------------------*/
nsEditingSession::~nsEditingSession()
{
}

NS_IMPL_ISUPPORTS2(nsEditingSession, nsIEditingSession, nsISupportsWeakReference)

/*---------------------------------------------------------------------------

  GetEditingShell

  void init (in nsIDOMWindow aWindow)
----------------------------------------------------------------------------*/
NS_IMETHODIMP 
nsEditingSession::Init(nsIDOMWindow *aWindow)
{
  nsCOMPtr<nsIDocShell> docShell;
  nsresult rv = GetDocShellFromWindow(aWindow, getter_AddRefs(docShell));
  if (NS_FAILED(rv)) return rv;

  mEditingShell = getter_AddRefs(NS_GetWeakReference(docShell));
  if (!mEditingShell) return NS_ERROR_NO_INTERFACE;

  rv = SetupFrameControllers(aWindow);
  if (NS_FAILED(rv)) return rv;
  
  return NS_OK;
}


/*---------------------------------------------------------------------------

  GetEditingShell

  [noscript] readonly attribute nsIEditingShell editingShell;
----------------------------------------------------------------------------*/
NS_IMETHODIMP 
nsEditingSession::GetEditingShell(nsIEditingShell * *outEditingShell)
{
  nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(mEditingShell);
  if (!docShell) return NS_ERROR_FAILURE;

  return docShell->QueryInterface(NS_GET_IID(nsIEditingShell), outEditingShell);
}


/*---------------------------------------------------------------------------

  SetupEditorOnFrame

  nsIEditor setupEditorOnFrame (in nsIDOMWindow aWindow);
----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::SetupEditorOnFrame(nsIDOMWindow *aWindow, nsIEditor **outEditor)
{
  nsresult rv;
  nsCOMPtr<nsIEditor> editor(do_CreateInstance("@mozilla.org/editor/htmleditor;1", &rv));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDocShell> docShell;
  rv = GetDocShellFromWindow(aWindow, getter_AddRefs(docShell));
  if (NS_FAILED(rv)) return rv;  
  
  nsCOMPtr<nsIPresShell> presShell;
  rv = docShell->GetPresShell(getter_AddRefs(presShell));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIContentViewer> contentViewer;
  rv = docShell->GetContentViewer(getter_AddRefs(contentViewer));
  if (NS_FAILED(rv)) return rv;
    
  nsCOMPtr<nsIDOMDocument> domDoc;  
  rv = contentViewer->GetDOMDocument(getter_AddRefs(domDoc));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsISelectionController> selCon = do_QueryInterface(presShell);

  rv = editor->Init(domDoc, presShell, nsnull /* root content */, selCon, 0);
  if (NS_FAILED(rv)) return rv;
  
  rv = editor->PostCreate();
  if (NS_FAILED(rv)) return rv;

  // set the editor on the controller
  SetEditorOnControllers(aWindow, editor);
  
  *outEditor = editor;
  NS_ADDREF(*outEditor);
  return NS_OK;
}


/*---------------------------------------------------------------------------

  TearDownEditorOnFrame

  void tearDownEditorOnFrame (in nsIDOMWindow aWindow);
----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::TearDownEditorOnFrame(nsIDOMWindow *aWindow)
{
  nsresult rv;
  
  // null out the editor on the controller
  rv = SetEditorOnControllers(aWindow, nsnull);
  if (NS_FAILED(rv)) return rv;  
  
  return NS_OK;
}

/*---------------------------------------------------------------------------

  GetEditorForFrame

  nsIEditor getEditorForFrame (in nsIDOMWindow aWindow);
----------------------------------------------------------------------------*/
NS_IMETHODIMP 
nsEditingSession::GetEditorForFrame(nsIDOMWindow *aWindow, nsIEditor **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

#ifdef XP_MAC
#pragma mark -
#endif

/*---------------------------------------------------------------------------

  GetDocShellFromWindow

  Utility method. This will always return an error if no docShell
  is returned.
----------------------------------------------------------------------------*/
nsresult
nsEditingSession::GetDocShellFromWindow(nsIDOMWindow *inWindow, nsIDocShell** outDocShell)
{
  nsCOMPtr<nsIScriptGlobalObject> scriptGO(do_QueryInterface(inWindow));
  if (!scriptGO) return NS_ERROR_FAILURE;

  nsresult rv = scriptGO->GetDocShell(outDocShell);
  if (NS_FAILED(rv)) return rv;
  if (!*outDocShell) return NS_ERROR_FAILURE;
  return NS_OK;
}



/*---------------------------------------------------------------------------

  SetupFrameControllers

  Set up the controller for this frame.
----------------------------------------------------------------------------*/
nsresult
nsEditingSession::SetupFrameControllers(nsIDOMWindow *inWindow)
{
  nsresult rv;
  
  nsCOMPtr<nsIDOMWindowInternal> domWindowInt(do_QueryInterface(inWindow, &rv));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIControllers> controllers;      
  rv = domWindowInt->GetControllers(getter_AddRefs(controllers));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIController> controller(do_CreateInstance("@mozilla.org/editor/editorcontroller;1", &rv));
  if (NS_FAILED(rv)) return rv;  

  nsCOMPtr<nsIEditorController> editorController(do_QueryInterface(controller));
  rv = editorController->Init(nsnull);    // we set the editor later when we have one
  if (NS_FAILED(rv)) return rv;

  rv = controllers->AppendController(controller);
  if (NS_FAILED(rv)) return rv;  

  return NS_OK;  
}

/*---------------------------------------------------------------------------

  SetEditorOnControllers

  Set the editor on the controller(s) for this window
----------------------------------------------------------------------------*/
nsresult
nsEditingSession::SetEditorOnControllers(nsIDOMWindow *inWindow, nsIEditor* inEditor)
{
  nsresult rv;
  
  // set the editor on the controller
  nsCOMPtr<nsIDOMWindowInternal> domWindowInt(do_QueryInterface(inWindow, &rv));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIControllers> controllers;      
  rv = domWindowInt->GetControllers(getter_AddRefs(controllers));
  if (NS_FAILED(rv)) return rv;

  // find the editor controllers by QIing each one. This sucks.
  // Controllers need to have IDs of some kind.
  PRUint32    numControllers;
  rv = controllers->GetControllerCount(&numControllers);
  if (NS_FAILED(rv)) return rv;

  for (PRUint32 i = 0; i < numControllers; i ++)
  {
    nsCOMPtr<nsIController> thisController;    
    controllers->GetControllerAt(i, getter_AddRefs(thisController));
    
    nsCOMPtr<nsIEditorController> editorController(do_QueryInterface(thisController));    // ok with nil controller
    if (editorController)
    {
      rv = editorController->SetCommandRefCon(inEditor);
      if (NS_FAILED(rv)) break;
    }  
  }
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}


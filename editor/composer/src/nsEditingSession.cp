/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Simon Fraser <sfraser@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMDocument.h"
#include "nsIScriptGlobalObject.h"
#include "nsISelectionPrivate.h"
#include "nsITransactionManager.h"

#include "nsIEditorDocShell.h"

#include "nsIChannel.h"
#include "nsIWebProgress.h"

#include "nsIControllers.h"
#include "nsIController.h"
#include "nsIEditorController.h"

#include "nsIPresShell.h"

#include "nsComposerCommandsUpdater.h"

#include "nsEditingSession.h"

#include "nsIComponentManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

#if DEBUG
#define NOISY_DOC_LOADING   1
#endif

/*---------------------------------------------------------------------------

  nsEditingSession

----------------------------------------------------------------------------*/
nsEditingSession::nsEditingSession()
: mDoneSetup(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
}

/*---------------------------------------------------------------------------

  ~nsEditingSession

----------------------------------------------------------------------------*/
nsEditingSession::~nsEditingSession()
{
  NS_IF_RELEASE(mStateMaintainer);
}

NS_IMPL_ISUPPORTS3(nsEditingSession, nsIEditingSession, nsIWebProgressListener, nsISupportsWeakReference)

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

  return NS_OK;
}


/*---------------------------------------------------------------------------

  MakeWindowEditable

  void makeWindowEditable (in nsIDOMWindow aWindow, in boolean inDoAfterUriLoad);
----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::MakeWindowEditable(nsIDOMWindow *aWindow, PRBool inDoAfterUriLoad)
{
  nsresult rv = PrepareForEditing();
  if (NS_FAILED(rv)) return rv;  
  
  nsCOMPtr<nsIEditorDocShell> editorDocShell;
  rv = GetEditorDocShellFromWindow(aWindow, getter_AddRefs(editorDocShell));
  if (NS_FAILED(rv)) return rv;  
  
  // set the flag on the docShell to say that it's editable
  rv = editorDocShell->MakeEditable(inDoAfterUriLoad);
  if (NS_FAILED(rv)) return rv;  
  
  rv = SetupFrameControllers(aWindow);
  if (NS_FAILED(rv)) return rv;

  // make an editor immediately
  if (!inDoAfterUriLoad)
  {
    rv = SetupEditorOnWindow(aWindow);
    if (NS_FAILED(rv)) return rv;  
  }
  
  return NS_OK;
}

/*---------------------------------------------------------------------------

  WindowIsEditable

  boolean windowIsEditable (in nsIDOMWindow aWindow);
----------------------------------------------------------------------------*/
NS_IMETHODIMP nsEditingSession::WindowIsEditable(nsIDOMWindow *aWindow, PRBool *outIsEditable)
{
  nsCOMPtr<nsIEditorDocShell> editorDocShell;
  nsresult rv = GetEditorDocShellFromWindow(aWindow, getter_AddRefs(editorDocShell));
  if (NS_FAILED(rv)) return rv;  

  return editorDocShell->GetEditable(outIsEditable);
}

/*---------------------------------------------------------------------------

  SetupEditorOnWindow

  nsIEditor setupEditorOnWindow (in nsIDOMWindow aWindow);
----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::SetupEditorOnWindow(nsIDOMWindow *aWindow)
{
  nsresult rv = PrepareForEditing();
  if (NS_FAILED(rv)) return rv;  

  nsCOMPtr<nsIDocShell> docShell;
  rv = GetDocShellFromWindow(aWindow, getter_AddRefs(docShell));
  if (NS_FAILED(rv)) return rv;  
  
  nsCOMPtr<nsIEditorDocShell>   editorDocShell(do_QueryInterface(docShell, &rv));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIEditor> editor(do_CreateInstance("@mozilla.org/editor/htmleditor;1", &rv));
  if (NS_FAILED(rv)) return rv;

  // set the editor on the docShell. The docShell now owns it.
  rv = editorDocShell->SetEditor(editor);
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIPresShell> presShell;
  rv = docShell->GetPresShell(getter_AddRefs(presShell));
  if (NS_FAILED(rv)) return rv;
  if (!presShell) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIContentViewer> contentViewer;
  rv = docShell->GetContentViewer(getter_AddRefs(contentViewer));
  if (NS_FAILED(rv)) return rv;
  if (!contentViewer) return NS_ERROR_FAILURE;
    
  nsCOMPtr<nsIDOMDocument> domDoc;  
  rv = contentViewer->GetDOMDocument(getter_AddRefs(domDoc));
  if (NS_FAILED(rv)) return rv;
  if (!domDoc) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsISelectionController> selCon = do_QueryInterface(presShell);

  rv = editor->Init(domDoc, presShell, nsnull /* root content */, selCon, 0);
  if (NS_FAILED(rv)) return rv;
  
  rv = editor->PostCreate();
  if (NS_FAILED(rv)) return rv;

  // set the editor on the controller
  rv = SetEditorOnControllers(aWindow, editor);
  if (NS_FAILED(rv)) return rv;

  // make the UI state maintainer
  NS_NEWXPCOM(mStateMaintainer, nsComposerCommandsUpdater);
  if (!mStateMaintainer) return NS_ERROR_OUT_OF_MEMORY;
  mStateMaintainer->AddRef();      // the owning reference

  // now init the state maintainer
  // XXX this needs to swap out editors
  rv = mStateMaintainer->Init(editor);
  if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsISelection>	selection;
	editor->GetSelection(getter_AddRefs(selection));	
  nsCOMPtr<nsISelectionPrivate> selPriv = do_QueryInterface(selection);
	if (!selPriv) return NS_ERROR_FAILURE;

  rv = selPriv->AddSelectionListener(mStateMaintainer);
  if (NS_FAILED(rv)) return rv;

  // and set it up as a doc state listener
  rv = editor->AddDocumentStateListener(NS_STATIC_CAST(nsIDocumentStateListener*, mStateMaintainer));
  if (NS_FAILED(rv)) return rv;
  
  // and as a transaction listener
  nsCOMPtr<nsITransactionManager> txnMgr;
  editor->GetTransactionManager(getter_AddRefs(txnMgr));
  if (txnMgr)
  {
    txnMgr->AddListener(NS_STATIC_CAST(nsITransactionListener*, mStateMaintainer));
  }
  
  return NS_OK;
}


/*---------------------------------------------------------------------------

  TearDownEditorOnWindow

  void tearDownEditorOnWindow (in nsIDOMWindow aWindow);
----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::TearDownEditorOnWindow(nsIDOMWindow *aWindow)
{
  nsresult rv;
  
  // null out the editor on the controller
  rv = SetEditorOnControllers(aWindow, nsnull);
  if (NS_FAILED(rv)) return rv;  
  
  nsCOMPtr<nsIEditorDocShell> editorDocShell;
  rv = GetEditorDocShellFromWindow(aWindow, getter_AddRefs(editorDocShell));
  if (NS_FAILED(rv)) return rv;  
  
  // null out the editor on the docShell
  rv = editorDocShell->SetEditor(nsnull);
  if (NS_FAILED(rv)) return rv;
  
  return NS_OK;
}

/*---------------------------------------------------------------------------

  GetEditorForFrame

  nsIEditor getEditorForFrame (in nsIDOMWindow aWindow);
----------------------------------------------------------------------------*/
NS_IMETHODIMP 
nsEditingSession::GetEditorForWindow(nsIDOMWindow *aWindow, nsIEditor **outEditor)
{
  nsCOMPtr<nsIEditorDocShell> editorDocShell;
  nsresult rv = GetEditorDocShellFromWindow(aWindow, getter_AddRefs(editorDocShell));
  if (NS_FAILED(rv)) return rv;  
  
  return editorDocShell->GetEditor(outEditor);
}

#ifdef XP_MAC
#pragma mark -
#endif

/*---------------------------------------------------------------------------

  OnStateChange

----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aStateFlags, PRUint32 aStatus)
{
  //
  // A Request has started...
  //
  if (aStateFlags & nsIWebProgressListener::STATE_START)
  {
    // Page level notification...
    if (aStateFlags & nsIWebProgressListener::STATE_IS_NETWORK)
    {
      StartPageLoad(aWebProgress);
    }

    // Document level notification...
    if (aStateFlags & nsIWebProgressListener::STATE_IS_DOCUMENT)
    {
      if (NotifyingCurrentDocument(aWebProgress))
        (void)StartDocumentLoad(aWebProgress);
    }
  }
  //
  // A Request is being processed
  //
  else if (aStateFlags & nsIWebProgressListener::STATE_TRANSFERRING)
  {
    if (aStateFlags * nsIWebProgressListener::STATE_IS_DOCUMENT)
    {
      // document transfer started
    }
  }
  //
  // Got a redirection
  //
  else if (aStateFlags & nsIWebProgressListener::STATE_REDIRECTING)
  {
    if (aStateFlags * nsIWebProgressListener::STATE_IS_DOCUMENT)
    {
      // got a redirect
    }
  }
  //
  // A network or document Request as finished...
  //
  else if (aStateFlags & nsIWebProgressListener::STATE_STOP)
  {

    // Document level notification...
    if (aStateFlags & nsIWebProgressListener::STATE_IS_DOCUMENT)
    {
      if (NotifyingCurrentDocument(aWebProgress))
      {
        nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
        (void)EndDocumentLoad(aWebProgress, channel, aStatus);
      }      
    }

    // Page level notification...
    if (aStateFlags & nsIWebProgressListener::STATE_IS_NETWORK)
    {
      nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
      (void)EndPageLoad(aWebProgress, channel, aStatus);
    }
  }

  return NS_OK;
}

/*---------------------------------------------------------------------------

  OnProgressChange

----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
    return NS_OK;
}

/*---------------------------------------------------------------------------

  OnLocationChange

----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    return NS_OK;
}

/*---------------------------------------------------------------------------

  OnStatusChange

----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
    return NS_OK;
}

/*---------------------------------------------------------------------------

  OnSecurityChange

----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 state)
{
    return NS_OK;
}


#ifdef XP_MAC
#pragma mark -
#endif

/*---------------------------------------------------------------------------

  NotifyingCurrentDocument

  Check that this notification is for our document. Necessary?
----------------------------------------------------------------------------*/

PRBool
nsEditingSession::NotifyingCurrentDocument(nsIWebProgress *aWebProgress)
{
  return PR_TRUE;
}


/*---------------------------------------------------------------------------

  StartDocumentLoad

  Called on start of load in a single frame
----------------------------------------------------------------------------*/
nsresult
nsEditingSession::StartDocumentLoad(nsIWebProgress *aWebProgress)
{
#ifdef NOISY_DOC_LOADING
  printf("Editing session StartDocumentLoad\n");
#endif

  NS_ENSURE_ARG(aWebProgress);
  
  // If we have an editor here, then we got a reload after making the editor.
  // We need to blow it away and make a new one at the end of the load.
  nsCOMPtr<nsIDOMWindow> domWindow;
  aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));

  if (domWindow)
  {
    nsresult rv = TearDownEditorOnWindow(domWindow);
  }
    
  return NS_OK;
}

/*---------------------------------------------------------------------------

  EndDocumentLoad

  Called on end of load in a single frame
----------------------------------------------------------------------------*/
nsresult
nsEditingSession::EndDocumentLoad(nsIWebProgress *aWebProgress, nsIChannel* aChannel, nsresult aStatus)
{
  NS_ENSURE_ARG(aWebProgress);
  
#ifdef NOISY_DOC_LOADING
  printf("Editing shell EndDocumentLoad\n");
#endif
  // we want to call the base class EndDocumentLoad, but avoid some of the stuff
  // that nsWebShell does (need to refactor).
  
  // OK, time to make an editor on this document
  nsCOMPtr<nsIDOMWindow> domWindow;
  aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));
  
  nsCOMPtr<nsIDocShell> docShell;
  nsresult rv = GetDocShellFromWindow(domWindow, getter_AddRefs(docShell));
  if (NS_FAILED(rv)) return rv;       // better error handling?
  
  nsCOMPtr<nsIEditorDocShell> editorDocShell(do_QueryInterface(docShell));
  // did someone set the flag to make this shell editable?
  if (editorDocShell)
  {
    PRBool  makeEditable;
    editorDocShell->GetEditable(&makeEditable);
  
    if (makeEditable)
    {
      nsresult rv = SetupEditorOnWindow(domWindow);
    }
  }

  return NS_OK;
}


/*---------------------------------------------------------------------------

  StartPageLoad

  Called on start load of the entire page (incl. subframes)
----------------------------------------------------------------------------*/
nsresult
nsEditingSession::StartPageLoad(nsIWebProgress *aWebProgress)
{
  return NS_OK;
}

/*---------------------------------------------------------------------------

  EndPageLoad

  Called on end load of the entire page (incl. subframes)
----------------------------------------------------------------------------*/
nsresult
nsEditingSession::EndPageLoad(nsIWebProgress *aWebProgress, nsIChannel* aChannel, nsresult aStatus)
{
  return NS_OK;
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

  GetEditorDocShellFromWindow

  Utility method. This will always return an error if no docShell
  is returned.
----------------------------------------------------------------------------*/
nsresult
nsEditingSession::GetEditorDocShellFromWindow(nsIDOMWindow *inWindow, nsIEditorDocShell** outDocShell)
{
  nsCOMPtr<nsIDocShell> docShell;
  nsresult rv = GetDocShellFromWindow(inWindow, getter_AddRefs(docShell));
  if (NS_FAILED(rv)) return rv;
  
  return docShell->QueryInterface(NS_GET_IID(nsIEditorDocShell), outDocShell);
}

/*---------------------------------------------------------------------------

  PrepareForEditing

  Set up this editing session for one or more editors
----------------------------------------------------------------------------*/
nsresult
nsEditingSession::PrepareForEditing()
{
  if (mDoneSetup)
    return NS_OK;
    
  mDoneSetup = PR_TRUE;

  nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(mEditingShell);
  if (!docShell) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDOMWindow>  domWindow = do_GetInterface(docShell);
  if (!domWindow) return NS_ERROR_FAILURE;
  
  // register callback
  nsCOMPtr<nsIWebProgress> webProgress = do_GetInterface(docShell);
  if (!webProgress) return NS_ERROR_FAILURE;

  nsresult rv = webProgress->AddProgressListener(this);
  if (NS_FAILED(rv)) return rv;

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

  // the first is an editor controller, and takes an nsIEditor as the refCon
  nsCOMPtr<nsIController> controller(do_CreateInstance("@mozilla.org/editor/editorcontroller;1", &rv));
  if (NS_FAILED(rv)) return rv;  

  nsCOMPtr<nsIEditorController> editorController(do_QueryInterface(controller));
  rv = editorController->Init(nsnull);    // we set the editor later when we have one
  if (NS_FAILED(rv)) return rv;

  rv = controllers->InsertControllerAt(0, controller);
  if (NS_FAILED(rv)) return rv;  

  // the second is an composer controller, and also takes an nsIEditor as the refCon
  controller = do_CreateInstance("@mozilla.org/editor/composercontroller;1", &rv);
  if (NS_FAILED(rv)) return rv;  

  nsCOMPtr<nsIEditorController> composerController(do_QueryInterface(controller));
  rv = composerController->Init(nsnull);    // we set the editor later when we have one
  if (NS_FAILED(rv)) return rv;

  rv = controllers->InsertControllerAt(1, controller);
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



/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser   <sfraser@netscape.com>
 *   Michael Judge  <mjudge@netscape.com>
 *   Charles Manske <cmanske@netscape.com>
 *   Kathleen Brade <brade@netscape.com>
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

#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIURI.h"
#include "nsIScriptGlobalObject.h"
#include "nsISelectionPrivate.h"
#include "nsITransactionManager.h"

#include "nsIEditorDocShell.h"
#include "nsIDocShell.h"

#include "nsIChannel.h"
#include "nsIWebProgress.h"
#include "nsIWebNavigation.h"
#include "nsIRefreshURI.h"

#include "nsIControllers.h"
#include "nsIController.h"
#include "nsIControllerContext.h"
#include "nsICommandManager.h"
#include "nsPICommandUpdater.h"

#include "nsIPresShell.h"

#include "nsComposerCommandsUpdater.h"
#include "nsEditingSession.h"

#include "nsComponentManagerUtils.h"
#include "nsIInterfaceRequestorUtils.h"

#include "nsIContentViewer.h"
#include "nsISelectionController.h"
#include "nsIPlaintextEditor.h"
#include "nsIEditor.h"

#include "nsIDOMNSDocument.h"
#include "nsIScriptContext.h"
#include "imgIContainer.h"
#include "nsIPresContext.h"

#if DEBUG
//#define NOISY_DOC_LOADING  1
#endif

/*---------------------------------------------------------------------------

  nsEditingSession

----------------------------------------------------------------------------*/
nsEditingSession::nsEditingSession()
: mDoneSetup(PR_FALSE)
, mCanCreateEditor(PR_FALSE)
, mEditorFlags(0)
, mEditorStatus(eEditorOK)
, mBaseCommandControllerId(0)
, mDocStateControllerId(0)
, mHTMLCommandControllerId(0)
{
}

/*---------------------------------------------------------------------------

  ~nsEditingSession

----------------------------------------------------------------------------*/
nsEditingSession::~nsEditingSession()
{
  // Must cancel previous timer?
  if (mLoadBlankDocTimer)
    mLoadBlankDocTimer->Cancel();
}

NS_IMPL_ISUPPORTS4(nsEditingSession, nsIEditingSession, nsIWebProgressListener, 
                   nsIURIContentListener, nsISupportsWeakReference)

/*---------------------------------------------------------------------------

  GetEditingShell

  void init (in nsIDOMWindow aWindow)
----------------------------------------------------------------------------*/
NS_IMETHODIMP 
nsEditingSession::Init(nsIDOMWindow *aWindow)
{
  nsIDocShell *docShell = GetDocShellFromWindow(aWindow);
  if (!docShell) return NS_ERROR_FAILURE;

  mEditingShell = do_GetWeakReference(docShell);
  if (!mEditingShell) return NS_ERROR_NO_INTERFACE;

  return NS_OK;
}


/*---------------------------------------------------------------------------

  MakeWindowEditable

  aEditorType string, "html" "htmlsimple" "text" "textsimple"
  void makeWindowEditable(in nsIDOMWindow aWindow, in string aEditorType, 
                          in boolean aDoAfterUriLoad);
----------------------------------------------------------------------------*/
#define DEFAULT_EDITOR_TYPE "html"

NS_IMETHODIMP
nsEditingSession::MakeWindowEditable(nsIDOMWindow *aWindow,
                                     const char *aEditorType, 
                                     PRBool aDoAfterUriLoad)
{
  mEditorType.Truncate();
  mEditorFlags = 0;
  mWindowToBeEdited = do_GetWeakReference(aWindow);

  // disable plugins
  nsIDocShell *docShell = GetDocShellFromWindow(aWindow);
  if (!docShell) return NS_ERROR_FAILURE;

  nsresult rv = docShell->SetAllowPlugins(PR_FALSE);
  if (NS_FAILED(rv)) return rv;

  // register as a content listener, so that we can fend off URL
  // loads from sidebar
  rv = docShell->SetParentURIContentListener(this);
  if (NS_FAILED(rv)) return rv;

  // Disable JavaScript in this document:
  nsCOMPtr<nsIScriptGlobalObject> sgo (do_QueryInterface(aWindow));
  if (sgo)
  {
    nsIScriptContext *scriptContext = sgo->GetContext();
    if (scriptContext)
    {
      scriptContext->SetScriptsEnabled(PR_FALSE, PR_TRUE);
    }
  }

  // Always remove existing editor
  TearDownEditorOnWindow(aWindow);
  
  // Tells embedder that startup is in progress
  mEditorStatus = eEditorCreationInProgress;

  //temporary to set editor type here. we will need different classes soon.
  if (!aEditorType)
    aEditorType = DEFAULT_EDITOR_TYPE;
  mEditorType = aEditorType;

  // if all this does is setup listeners and I don't need listeners, 
  // can't this step be ignored?? (based on aDoAfterURILoad)
  rv = PrepareForEditing();
  if (NS_FAILED(rv)) return rv;  
  
  nsCOMPtr<nsIEditorDocShell> editorDocShell;
  rv = GetEditorDocShellFromWindow(aWindow, getter_AddRefs(editorDocShell));
  if (NS_FAILED(rv)) return rv;  
  
  // set the flag on the docShell to say that it's editable
  rv = editorDocShell->MakeEditable(aDoAfterUriLoad);
  if (NS_FAILED(rv)) return rv;  
  
  // Setup commands common to plaintext and html editors,
  //  including the document creation observers
  // the first is an editor controller
  rv = SetupEditorCommandController("@mozilla.org/editor/editorcontroller;1",
                                    aWindow,
                                    NS_STATIC_CAST(nsIEditingSession*, this),
                                    &mBaseCommandControllerId);
  if (NS_FAILED(rv)) return rv;

  // The second is a controller to monitor doc state,
  // such as creation and "dirty flag"
  rv = SetupEditorCommandController("@mozilla.org/editor/editordocstatecontroller;1",
                                    aWindow,
                                    NS_STATIC_CAST(nsIEditingSession*, this),
                                    &mDocStateControllerId);
  if (NS_FAILED(rv)) return rv;

  // aDoAfterUriLoad can be false only when making an existing window editable
  if (!aDoAfterUriLoad)
  {
    rv = SetupEditorOnWindow(aWindow);

    // mEditorStatus is set to the error reason
    // Since this is used only when editing an existing page,
    //  it IS ok to destroy current editor
    if (NS_FAILED(rv))
      TearDownEditorOnWindow(aWindow);
  }
  return rv;
}

/*---------------------------------------------------------------------------

  WindowIsEditable

  boolean windowIsEditable (in nsIDOMWindow aWindow);
----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::WindowIsEditable(nsIDOMWindow *aWindow, PRBool *outIsEditable)
{
  nsCOMPtr<nsIEditorDocShell> editorDocShell;
  nsresult rv = GetEditorDocShellFromWindow(aWindow,
                                            getter_AddRefs(editorDocShell));
  if (NS_FAILED(rv)) return rv;  

  return editorDocShell->GetEditable(outIsEditable);
}


// These are MIME types that are automatically parsed as "text/plain"
//   and thus we can edit them as plaintext
// Note: in older versions, we attempted to convert the mimetype of
//   the network channel for these and "text/xml" to "text/plain", 
//   but further investigation reveals that strategy doesn't work
const char* const gSupportedTextTypes[] = {
  "text/plain",
  "text/css",
  "text/rdf",
  "text/xsl",
  "text/javascript",    // obsolete type
  "application/x-javascript",
  "text/xul",           // obsolete type
  "application/vnd.mozilla.xul+xml",
  NULL      // IMPORTANT! Null must be at end
};

PRBool
IsSupportedTextType(const char* aMIMEType)
{
  if (!aMIMEType)
    return PR_FALSE;

  PRInt32 i = 0;
  while (gSupportedTextTypes[i])
  {
    if (strcmp(gSupportedTextTypes[i], aMIMEType) == 0)
    {
      return PR_TRUE;
    }

    i ++;
  }
  
  return PR_FALSE;
}

/*---------------------------------------------------------------------------

  SetupEditorOnWindow

  nsIEditor setupEditorOnWindow (in nsIDOMWindow aWindow);
----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::SetupEditorOnWindow(nsIDOMWindow *aWindow)
{
  nsresult rv;

  //MIME CHECKING
  //must get the content type
  // Note: the doc gets this from the network channel during StartPageLoad,
  //    so we don't have to get it from there ourselves
  nsCOMPtr<nsIDOMDocument> doc;
  nsCAutoString mimeCType;

  //then lets check the mime type
  if (NS_SUCCEEDED(aWindow->GetDocument(getter_AddRefs(doc))) && doc)
  {
    nsCOMPtr<nsIDOMNSDocument> nsdoc = do_QueryInterface(doc);
    if (nsdoc)
    {
      nsAutoString mimeType;
      if (NS_SUCCEEDED(nsdoc->GetContentType(mimeType)))
        CopyUTF16toUTF8(mimeType, mimeCType);

      if (IsSupportedTextType(mimeCType.get()))
      {
        mEditorType.AssignLiteral("text");
        mimeCType = "text/plain";
      }
      else if (!mimeCType.EqualsLiteral("text/html"))
      {
        // Neither an acceptable text or html type.
        mEditorStatus = eEditorErrorCantEditMimeType;

        // Turn editor into HTML -- we will load blank page later
        mEditorType.AssignLiteral("html");
        mimeCType.AssignLiteral("text/html");
      }
    }
  }
  PRBool needHTMLController = PR_FALSE;

  const char *classString = "@mozilla.org/editor/htmleditor;1";
  if (mEditorType.EqualsLiteral("textmail"))
  {
    mEditorFlags = nsIPlaintextEditor::eEditorPlaintextMask | 
                   nsIPlaintextEditor::eEditorEnableWrapHackMask | 
                   nsIPlaintextEditor::eEditorMailMask;
  }
  else if (mEditorType.EqualsLiteral("text"))
  {
    mEditorFlags = nsIPlaintextEditor::eEditorPlaintextMask | 
                   nsIPlaintextEditor::eEditorEnableWrapHackMask;
  }
  else if (mEditorType.EqualsLiteral("htmlmail"))
  {
    if (mimeCType.EqualsLiteral("text/html"))
    {
      needHTMLController = PR_TRUE;
      mEditorFlags = nsIPlaintextEditor::eEditorMailMask;
    }
    else //set the flags back to textplain.
      mEditorFlags = nsIPlaintextEditor::eEditorPlaintextMask | 
                     nsIPlaintextEditor::eEditorEnableWrapHackMask;
  }
  else // Defaulted to html
  {
    needHTMLController = PR_TRUE;
  }

  // make the UI state maintainer
  nsComposerCommandsUpdater *stateMaintainer;
  NS_NEWXPCOM(stateMaintainer, nsComposerCommandsUpdater);
  mStateMaintainer = NS_STATIC_CAST(nsISelectionListener*, stateMaintainer);

  if (!mStateMaintainer) return NS_ERROR_OUT_OF_MEMORY;

  // now init the state maintainer
  // This allows notification of error state
  //  even if we don't create an editor
  rv = stateMaintainer->Init(aWindow);
  if (NS_FAILED(rv)) return rv;

  if (mEditorStatus != eEditorCreationInProgress)
  {
    // We had an earlier error -- force notification of document creation
    nsCOMPtr<nsIDocumentStateListener> docListener =
                                          do_QueryInterface(mStateMaintainer);
    if (docListener)
      docListener->NotifyDocumentCreated();

    return NS_ERROR_FAILURE;
  }

  // Create editor and do other things 
  //  only if we haven't found some error above,
  nsIDocShell *docShell = GetDocShellFromWindow(aWindow);
  if (!docShell) return NS_ERROR_FAILURE;  

  nsCOMPtr<nsIPresShell> presShell;
  rv = docShell->GetPresShell(getter_AddRefs(presShell));
  if (NS_FAILED(rv)) return rv;
  if (!presShell) return NS_ERROR_FAILURE;

  // Disable animation of images in this document:
  nsCOMPtr<nsIPresContext> presContext;
  rv = presShell->GetPresContext(getter_AddRefs(presContext));
  if (NS_FAILED(rv)) return rv;
  if (!presContext) return NS_ERROR_FAILURE;

  presContext->SetImageAnimationMode(imgIContainer::kDontAnimMode);

  // create and set editor
  nsCOMPtr<nsIEditorDocShell> editorDocShell = do_QueryInterface(docShell, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIEditor> editor = do_CreateInstance(classString, &rv);
  if (NS_FAILED(rv)) return rv;
  // set the editor on the docShell. The docShell now owns it.
  rv = editorDocShell->SetEditor(editor);
  if (NS_FAILED(rv)) return rv;

  // setup the HTML editor command controller
  if (needHTMLController)
  {
    // The third controller takes an nsIEditor as the context
    rv = SetupEditorCommandController("@mozilla.org/editor/htmleditorcontroller;1",
                                      aWindow, editor,
                                      &mHTMLCommandControllerId);
    if (NS_FAILED(rv)) return rv;
  }

  // Set mimetype on editor
  rv = editor->SetContentsMIMEType(mimeCType.get());
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIContentViewer> contentViewer;
  rv = docShell->GetContentViewer(getter_AddRefs(contentViewer));
  if (NS_FAILED(rv)) return rv;
  if (!contentViewer) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMDocument> domDoc;  
  rv = contentViewer->GetDOMDocument(getter_AddRefs(domDoc));
  if (NS_FAILED(rv)) return rv;
  if (!domDoc) return NS_ERROR_FAILURE;

  // Set up as a doc state listener
  // Important! We must have this to broadcast the "obs_documentCreated" message
  rv = editor->AddDocumentStateListener(
      NS_STATIC_CAST(nsIDocumentStateListener*, stateMaintainer));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsISelectionController> selCon = do_QueryInterface(presShell);
  rv = editor->Init(domDoc, presShell, nsnull /* root content */,
                    selCon, mEditorFlags);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsISelection>	selection;
  editor->GetSelection(getter_AddRefs(selection));	
  nsCOMPtr<nsISelectionPrivate> selPriv = do_QueryInterface(selection);
  if (!selPriv) return NS_ERROR_FAILURE;

  rv = selPriv->AddSelectionListener(stateMaintainer);
  if (NS_FAILED(rv)) return rv;

  // and as a transaction listener
  nsCOMPtr<nsITransactionManager> txnMgr;
  editor->GetTransactionManager(getter_AddRefs(txnMgr));
  if (txnMgr)
    txnMgr->AddListener(NS_STATIC_CAST(nsITransactionListener*,
                        stateMaintainer));

  // Set context on all controllers to be the editor
  rv = SetEditorOnControllers(aWindow, editor);
  if (NS_FAILED(rv)) return rv;

  // Everything went fine!
  mEditorStatus = eEditorOK;


  // This will trigger documentCreation notification
  return editor->PostCreate();
}

/*---------------------------------------------------------------------------

  TearDownEditorOnWindow

  void tearDownEditorOnWindow (in nsIDOMWindow aWindow);
----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::TearDownEditorOnWindow(nsIDOMWindow *aWindow)
{
  nsresult rv;
  
   // Kill any existing reload timer
   if (mLoadBlankDocTimer)
   {
     mLoadBlankDocTimer->Cancel();
     mLoadBlankDocTimer = nsnull;
   }

  nsCOMPtr<nsIEditorDocShell> editorDocShell;
  rv = GetEditorDocShellFromWindow(aWindow, getter_AddRefs(editorDocShell));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIEditor> editor;
  rv = editorDocShell->GetEditor(getter_AddRefs(editor));
  if (NS_FAILED(rv)) return rv;

  // null out the editor on the controllers first to prevent their weak 
  // references from pointing to a destroyed editor
  if (mStateMaintainer && editor)
  {
      // null out the editor on the controllers
      rv = SetEditorOnControllers(aWindow, nsnull);
      if (NS_FAILED(rv)) return rv;  
  }

  // null out the editor on the docShell to trigger PreDestroy which
  // needs to happen before document state listeners are removed below
  rv = editorDocShell->SetEditor(nsnull);
  if (NS_FAILED(rv)) return rv;

  if (mStateMaintainer)
  {
    if (editor)
    {
      // If we had an editor -- we are loading a new URL into existing window

      // Remove all the listeners
      nsCOMPtr<nsISelection>	selection;
      editor->GetSelection(getter_AddRefs(selection));	
      nsCOMPtr<nsISelectionPrivate> selPriv = do_QueryInterface(selection);
      if (selPriv)
      {
        nsCOMPtr<nsISelectionListener> listener = 
                                        do_QueryInterface(mStateMaintainer);
        rv = selPriv->RemoveSelectionListener(listener);
        if (NS_FAILED(rv)) return rv;
      }

      nsCOMPtr<nsIDocumentStateListener> docListener =
                                            do_QueryInterface(mStateMaintainer);
      rv = editor->RemoveDocumentStateListener(docListener);
      if (NS_FAILED(rv)) return rv;
  
      nsCOMPtr<nsITransactionManager> txnMgr;
      editor->GetTransactionManager(getter_AddRefs(txnMgr));
      if (txnMgr)
      {
        nsCOMPtr<nsITransactionListener> transactionListener =
              do_QueryInterface(mStateMaintainer);
        txnMgr->RemoveListener(transactionListener);
      }
    }
    else
    {
      // No editor: we have a new window and previous controllers
      //   were destroyed with the contentWindow.
      //   Clear IDs to trigger creation of new controllers
      mBaseCommandControllerId = 0;
      mDocStateControllerId = 0;
      mHTMLCommandControllerId = 0;
    }
  }

  return rv;
}

/*---------------------------------------------------------------------------

  GetEditorForFrame

  nsIEditor getEditorForFrame (in nsIDOMWindow aWindow);
----------------------------------------------------------------------------*/
NS_IMETHODIMP 
nsEditingSession::GetEditorForWindow(nsIDOMWindow *aWindow,
                                     nsIEditor **outEditor)
{
  nsCOMPtr<nsIEditorDocShell> editorDocShell;
  nsresult rv = GetEditorDocShellFromWindow(aWindow,
                                            getter_AddRefs(editorDocShell));
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
nsEditingSession::OnStateChange(nsIWebProgress *aWebProgress,
                                nsIRequest *aRequest,
                                PRUint32 aStateFlags, nsresult aStatus)
{

#ifdef NOISY_DOC_LOADING
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  if (channel)
  {
    nsCAutoString contentType;
    channel->GetContentType(contentType);
    if (!contentType.IsEmpty())
      printf(" ++++++ MIMETYPE = %s\n", contentType.get());
  }
#endif

  //
  // A Request has started...
  //
  if (aStateFlags & nsIWebProgressListener::STATE_START)
  {
#ifdef NOISY_DOC_LOADING
  {
    nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
    if (channel)
    {
      nsCOMPtr<nsIURI> uri;
      channel->GetURI(getter_AddRefs(uri));
      if (uri)
      {
        nsXPIDLCString spec;
        uri->GetSpec(spec);
        printf(" **** STATE_START: CHANNEL URI=%s, flags=%x\n",
               spec.get(), aStateFlags);
      }
    }
    else
      printf("    STATE_START: NO CHANNEL flags=%x\n", aStateFlags);
  }
#endif
    // Page level notification...
    if (aStateFlags & nsIWebProgressListener::STATE_IS_NETWORK)
    {
      nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
      StartPageLoad(channel);
#ifdef NOISY_DOC_LOADING
      printf("STATE_START & STATE_IS_NETWORK flags=%x\n", aStateFlags);
#endif
    }

    // Document level notification...
    if (aStateFlags & nsIWebProgressListener::STATE_IS_DOCUMENT)
    {
      mCanCreateEditor = PR_TRUE;
      (void)StartDocumentLoad(aWebProgress,
                              IsProgressForTargetDocument(aWebProgress));
#ifdef NOISY_DOC_LOADING
      printf("STATE_START & STATE_IS_DOCUMENT flags=%x\n", aStateFlags);
#endif
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

#ifdef NOISY_DOC_LOADING
  {
    nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
    if (channel)
    {
      nsCOMPtr<nsIURI> uri;
      channel->GetURI(getter_AddRefs(uri));
      if (uri)
      {
        nsXPIDLCString spec;
        uri->GetSpec(spec);
        printf(" **** STATE_STOP: CHANNEL URI=%s, flags=%x\n",
               spec.get(), aStateFlags);
      }
    }
    else
      printf("     STATE_STOP: NO CHANNEL  flags=%x\n", aStateFlags);
  }
#endif

    // Document level notification...
    if (aStateFlags & nsIWebProgressListener::STATE_IS_DOCUMENT)
    {
      nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
      nsresult rv = EndDocumentLoad(aWebProgress, channel, aStatus,
                                    IsProgressForTargetDocument(aWebProgress));
#ifdef NOISY_DOC_LOADING
      printf("STATE_STOP & STATE_IS_DOCUMENT flags=%x\n", aStateFlags);
#endif
    }

    // Page level notification...
    if (aStateFlags & nsIWebProgressListener::STATE_IS_NETWORK)
    {
      nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
      (void)EndPageLoad(aWebProgress, channel, aStatus);
#ifdef NOISY_DOC_LOADING
      printf("STATE_STOP & STATE_IS_NETWORK flags=%x\n", aStateFlags);
#endif
    }
  }

  return NS_OK;
}

/*---------------------------------------------------------------------------

  OnProgressChange

----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::OnProgressChange(nsIWebProgress *aWebProgress,
                                   nsIRequest *aRequest,
                                   PRInt32 aCurSelfProgress,
                                   PRInt32 aMaxSelfProgress,
                                   PRInt32 aCurTotalProgress,
                                   PRInt32 aMaxTotalProgress)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

/*---------------------------------------------------------------------------

  OnLocationChange

----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::OnLocationChange(nsIWebProgress *aWebProgress, 
                                   nsIRequest *aRequest, nsIURI *aURI)
{
  nsCOMPtr<nsIDOMWindow> domWindow;
  nsresult rv = aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMDocument> domDoc;
  rv = domWindow->GetDocument(getter_AddRefs(domDoc));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  if (!doc) return NS_ERROR_FAILURE;

  doc->SetDocumentURI(aURI);

  // Notify the location-changed observer that
  //  the document URL has changed
  nsIDocShell *docShell = GetDocShellFromWindow(domWindow);
  if (!docShell) return NS_ERROR_FAILURE;

  nsCOMPtr<nsICommandManager> commandManager = do_GetInterface(docShell);
  nsCOMPtr<nsPICommandUpdater> commandUpdater =
                                  do_QueryInterface(commandManager);
  if (!commandUpdater) return NS_ERROR_FAILURE;

  return commandUpdater->CommandStatusChanged("obs_documentLocationChanged");
}

/*---------------------------------------------------------------------------

  OnStatusChange

----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::OnStatusChange(nsIWebProgress *aWebProgress,
                                 nsIRequest *aRequest,
                                 nsresult aStatus,
                                 const PRUnichar *aMessage)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

/*---------------------------------------------------------------------------

  OnSecurityChange

----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::OnSecurityChange(nsIWebProgress *aWebProgress,
                                   nsIRequest *aRequest, PRUint32 state)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}


#ifdef XP_MAC
#pragma mark -
#endif

/* boolean onStartURIOpen (in nsIURI aURI); */
NS_IMETHODIMP nsEditingSession::OnStartURIOpen(nsIURI *aURI, PRBool *aAbortOpen)
{
  return NS_OK;
}

/* boolean doContent (in string aContentType, in boolean aIsContentPreferred, in nsIRequest aRequest, out nsIStreamListener aContentHandler); */
NS_IMETHODIMP nsEditingSession::DoContent(const char *aContentType, PRBool aIsContentPreferred, nsIRequest *aRequest, nsIStreamListener **aContentHandler, PRBool *aAbortProcess)
{
  NS_ENSURE_ARG_POINTER(aContentHandler);
  NS_ENSURE_ARG_POINTER(aAbortProcess);
  *aContentHandler = nsnull;
  *aAbortProcess = PR_FALSE;
  return NS_OK;
}

/* boolean isPreferred (in string aContentType, out string aDesiredContentType); */
NS_IMETHODIMP nsEditingSession::IsPreferred(const char *aContentType, char **aDesiredContentType, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(aDesiredContentType);
  NS_ENSURE_ARG_POINTER(_retval);
  *aDesiredContentType = nsnull;
  *_retval = PR_FALSE;
  return NS_OK;
}

/* boolean canHandleContent (in string aContentType, in boolean aIsContentPreferred, out string aDesiredContentType); */
NS_IMETHODIMP nsEditingSession::CanHandleContent(const char *aContentType, PRBool aIsContentPreferred, char **aDesiredContentType, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(aDesiredContentType);
  NS_ENSURE_ARG_POINTER(_retval);
  *aDesiredContentType = nsnull;
  *_retval = PR_FALSE;
  return NS_OK;
}

/* attribute nsISupports loadCookie; */
NS_IMETHODIMP nsEditingSession::GetLoadCookie(nsISupports * *aLoadCookie)
{
  NS_ENSURE_ARG_POINTER(aLoadCookie);
  *aLoadCookie = nsnull;
  return NS_OK;
}
NS_IMETHODIMP nsEditingSession::SetLoadCookie(nsISupports * aLoadCookie)
{
  return NS_OK;
}

/* attribute nsIURIContentListener parentContentListener; */
NS_IMETHODIMP nsEditingSession::GetParentContentListener(nsIURIContentListener * *aParentContentListener)
{
  NS_ENSURE_ARG_POINTER(aParentContentListener);
  *aParentContentListener = nsnull;
  return NS_OK;
}
NS_IMETHODIMP nsEditingSession::SetParentContentListener(nsIURIContentListener * aParentContentListener)
{
  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#endif


/*---------------------------------------------------------------------------

  IsProgressForTargetDocument

  Check that this notification is for our document.
----------------------------------------------------------------------------*/

PRBool
nsEditingSession::IsProgressForTargetDocument(nsIWebProgress *aWebProgress)
{
  nsCOMPtr<nsIDOMWindow> domWindow;
  if (aWebProgress)
    aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));
  nsCOMPtr<nsIDOMWindow> editedDOMWindow = do_QueryReferent(mWindowToBeEdited);

  return (domWindow && (domWindow == editedDOMWindow));
}


/*---------------------------------------------------------------------------

  GetEditorStatus

  Called during GetCommandStateParams("obs_documentCreated"...) 
  to determine if editor was created and document 
  was loaded successfully
----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::GetEditorStatus(PRUint32 *aStatus)
{
  NS_ENSURE_ARG_POINTER(aStatus);
  *aStatus = mEditorStatus;
  return NS_OK;
}

/*---------------------------------------------------------------------------

  StartDocumentLoad

  Called on start of load in a single frame
----------------------------------------------------------------------------*/
nsresult
nsEditingSession::StartDocumentLoad(nsIWebProgress *aWebProgress, 
                                    PRBool aIsToBeMadeEditable)
{
#ifdef NOISY_DOC_LOADING
  printf("======= StartDocumentLoad ========\n");
#endif

  NS_ENSURE_ARG_POINTER(aWebProgress);
  
  // If we have an editor here, then we got a reload after making the editor.
  // We need to blow it away and make a new one at the end of the load.
  nsCOMPtr<nsIDOMWindow> domWindow;
  aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));
  if (domWindow)
  {
    nsresult rv = TearDownEditorOnWindow(domWindow);
  }
    
  if (aIsToBeMadeEditable)
    mEditorStatus = eEditorCreationInProgress;

  return NS_OK;
}

/*---------------------------------------------------------------------------

  EndDocumentLoad

  Called on end of load in a single frame
----------------------------------------------------------------------------*/
nsresult
nsEditingSession::EndDocumentLoad(nsIWebProgress *aWebProgress,
                                  nsIChannel* aChannel, nsresult aStatus,
                                  PRBool aIsToBeMadeEditable)
{
  NS_ENSURE_ARG_POINTER(aWebProgress);
  
#ifdef NOISY_DOC_LOADING
  printf("======= EndDocumentLoad ========\n");
  printf("with status %d, ", aStatus);
  nsCOMPtr<nsIURI> uri;
  nsXPIDLCString spec;
  if (NS_SUCCEEDED(aChannel->GetURI(getter_AddRefs(uri)))) {
    uri->GetSpec(spec);
    printf(" uri %s\n", spec.get());
  }
#endif

  // We want to call the base class EndDocumentLoad,
  // but avoid some of the stuff
  // that nsWebShell does (need to refactor).
  
  // OK, time to make an editor on this document
  nsCOMPtr<nsIDOMWindow> domWindow;
  aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));
  
  // Set the error state -- we will create an editor 
  // anyway and load empty doc later
  if (aIsToBeMadeEditable) {
    if (aStatus == NS_ERROR_FILE_NOT_FOUND)
      mEditorStatus = eEditorErrorFileNotFound;
  }

  nsIDocShell *docShell = GetDocShellFromWindow(domWindow);
  if (!docShell) return NS_ERROR_FAILURE;       // better error handling?

  // cancel refresh from meta tags
  // we need to make sure that all pages in editor (whether editable or not)
  // can't refresh contents being edited
  nsCOMPtr<nsIRefreshURI> refreshURI = do_QueryInterface(docShell);
  if (refreshURI)
    refreshURI->CancelRefreshURITimers();

  nsCOMPtr<nsIEditorDocShell> editorDocShell = do_QueryInterface(docShell);

  nsresult rv = NS_OK;

  // did someone set the flag to make this shell editable?
  if (aIsToBeMadeEditable && mCanCreateEditor && editorDocShell)
  {
    PRBool  makeEditable;
    editorDocShell->GetEditable(&makeEditable);
  
    if (makeEditable)
    {
      mCanCreateEditor = PR_FALSE;
      rv = SetupEditorOnWindow(domWindow);
      if (NS_FAILED(rv))
      {
        // If we had an error, setup timer to load a blank page later
        if (mLoadBlankDocTimer)
        {
          // Must cancel previous timer?
          mLoadBlankDocTimer->Cancel();
          mLoadBlankDocTimer = NULL;
        }
  
        mLoadBlankDocTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
        if (NS_FAILED(rv)) return rv;

        mEditorStatus = eEditorCreationInProgress;
        mLoadBlankDocTimer->InitWithFuncCallback(
                                        nsEditingSession::TimerCallback,
                                        (void*)docShell,
                                        10, nsITimer::TYPE_ONE_SHOT);
      }
    }
  }
  return rv;
}


void
nsEditingSession::TimerCallback(nsITimer* aTimer, void* aClosure)
{
  nsCOMPtr<nsIDocShell> docShell = (nsIDocShell*)aClosure;
  if (docShell)
  {
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(docShell));
    if (webNav)
      webNav->LoadURI(NS_LITERAL_STRING("about:blank").get(),
                      0, nsnull, nsnull, nsnull);
  }
}

/*---------------------------------------------------------------------------

  StartPageLoad

  Called on start load of the entire page (incl. subframes)
----------------------------------------------------------------------------*/
nsresult
nsEditingSession::StartPageLoad(nsIChannel *aChannel)
{
#ifdef NOISY_DOC_LOADING
  printf("======= StartPageLoad ========\n");
#endif
  return NS_OK;
}

/*---------------------------------------------------------------------------

  EndPageLoad

  Called on end load of the entire page (incl. subframes)
----------------------------------------------------------------------------*/
nsresult
nsEditingSession::EndPageLoad(nsIWebProgress *aWebProgress,
                              nsIChannel* aChannel, nsresult aStatus)
{
#ifdef NOISY_DOC_LOADING
  printf("======= EndPageLoad ========\n");
  printf("  with status %d, ", aStatus);
  nsCOMPtr<nsIURI> uri;
  nsXPIDLCString spec;
  if (NS_SUCCEEDED(aChannel->GetURI(getter_AddRefs(uri)))) {
    uri->GetSpec(spec);
    printf("uri %s\n", spec.get());
  }
 
  nsCAutoString contentType;
  aChannel->GetContentType(contentType);
  if (!contentType.IsEmpty())
    printf("   flags = %d, status = %d, MIMETYPE = %s\n", 
               mEditorFlags, mEditorStatus, contentType.get());
#endif

  // Set the error state -- we will create an editor anyway 
  // and load empty doc later
  if (aStatus == NS_ERROR_FILE_NOT_FOUND)
    mEditorStatus = eEditorErrorFileNotFound;

  nsCOMPtr<nsIDOMWindow> domWindow;
  nsresult rv = aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));
  
  nsIDocShell *docShell = GetDocShellFromWindow(domWindow);
  if (!docShell) return NS_ERROR_FAILURE;

  // cancel refresh from meta tags
  // we need to make sure that all pages in editor (whether editable or not)
  // can't refresh contents being edited
  nsCOMPtr<nsIRefreshURI> refreshURI = do_QueryInterface(docShell);
  if (refreshURI)
    refreshURI->CancelRefreshURITimers();

#if 0
  // Shouldn't we do this when we want to edit sub-frames?
  return MakeWindowEditable(domWindow, "html", PR_FALSE);
#else
  return NS_OK;
#endif
}


#ifdef XP_MAC
#pragma mark -
#endif

/*---------------------------------------------------------------------------

  GetDocShellFromWindow

  Utility method. This will always return nsnull if no docShell is found.
----------------------------------------------------------------------------*/
nsIDocShell *
nsEditingSession::GetDocShellFromWindow(nsIDOMWindow *aWindow)
{
  nsCOMPtr<nsIScriptGlobalObject> scriptGO = do_QueryInterface(aWindow);
  if (!scriptGO)
    return nsnull;

  return scriptGO->GetDocShell();
}

/*---------------------------------------------------------------------------

  GetEditorDocShellFromWindow

  Utility method. This will always return an error if no docShell
  is returned.
----------------------------------------------------------------------------*/
nsresult
nsEditingSession::GetEditorDocShellFromWindow(nsIDOMWindow *aWindow,
                                              nsIEditorDocShell** outDocShell)
{
  nsIDocShell *docShell = GetDocShellFromWindow(aWindow);
  if (!docShell) return NS_ERROR_FAILURE;
  
  return docShell->QueryInterface(NS_GET_IID(nsIEditorDocShell), 
                                  (void **)outDocShell);
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

  return webProgress->AddProgressListener(this,
                            (nsIWebProgress::NOTIFY_STATE_NETWORK  | 
                             nsIWebProgress::NOTIFY_STATE_DOCUMENT |
                             nsIWebProgress::NOTIFY_LOCATION));
}

/*---------------------------------------------------------------------------

  SetupEditorCommandController

  Create a command controller, append to controllers,
  get and return the controller ID, and set the context
----------------------------------------------------------------------------*/
nsresult
nsEditingSession::SetupEditorCommandController(
                                  const char *aControllerClassName,
                                  nsIDOMWindow *aWindow,
                                  nsISupports *aContext,
                                  PRUint32 *aControllerId)
{
  NS_ENSURE_ARG_POINTER(aControllerClassName);
  NS_ENSURE_ARG_POINTER(aWindow);
  NS_ENSURE_ARG_POINTER(aContext);
  NS_ENSURE_ARG_POINTER(aControllerId);

  nsresult rv;
  nsCOMPtr<nsIDOMWindowInternal> domWindowInt =
                                    do_QueryInterface(aWindow, &rv);
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIControllers> controllers;      
  rv = domWindowInt->GetControllers(getter_AddRefs(controllers));
  if (NS_FAILED(rv)) return rv;

  // We only have to create each singleton controller once
  // We know this has happened once we have a controllerId value
  if (!*aControllerId)
  {
    nsresult rv;
    nsCOMPtr<nsIController> controller;
    controller = do_CreateInstance(aControllerClassName, &rv);
    if (NS_FAILED(rv)) return rv;  

    // We must insert at head of the list to be sure our
    //   controller is found before other implementations
    //   (e.g., not-implemented versions by browser)
    rv = controllers->InsertControllerAt(0,controller);
    if (NS_FAILED(rv)) return rv;  

    // Remember the ID for the controller
    rv = controllers->GetControllerId(controller, aControllerId);
    if (NS_FAILED(rv)) return rv;  
  }  

  // Set the context
  return SetContextOnControllerById(controllers, aContext, *aControllerId);
}

/*---------------------------------------------------------------------------

  SetEditorOnControllers

  Set the editor on the controller(s) for this window
----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::SetEditorOnControllers(nsIDOMWindow *aWindow,
                                         nsIEditor* aEditor)
{
  nsresult rv;
  
  // set the editor on the controller
  nsCOMPtr<nsIDOMWindowInternal> domWindowInt =
                                     do_QueryInterface(aWindow, &rv);
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIControllers> controllers;      
  rv = domWindowInt->GetControllers(getter_AddRefs(controllers));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsISupports> editorAsISupports = do_QueryInterface(aEditor);
  if (mBaseCommandControllerId)
  {
    rv = SetContextOnControllerById(controllers, editorAsISupports,
                                   mBaseCommandControllerId);
    if (NS_FAILED(rv)) return rv;
  }

  if (mDocStateControllerId)
  {
    rv = SetContextOnControllerById(controllers, editorAsISupports,
                                   mDocStateControllerId);
    if (NS_FAILED(rv)) return rv;
  }

  if (mHTMLCommandControllerId)
    rv = SetContextOnControllerById(controllers, editorAsISupports,
                                   mHTMLCommandControllerId);

  return rv;
}

nsresult
nsEditingSession::SetContextOnControllerById(nsIControllers* aControllers,
                                            nsISupports* aContext,
                                            PRUint32 aID)
{
  NS_ENSURE_ARG_POINTER(aControllers);

  // aContext can be null (when destroying editor)
  nsCOMPtr<nsIController> controller;    
  aControllers->GetControllerById(aID, getter_AddRefs(controller));
  
  // ok with nil controller
  nsCOMPtr<nsIControllerContext> editorController =
                                       do_QueryInterface(controller);
  if (!editorController) return NS_ERROR_FAILURE;

  return editorController->SetCommandContext(aContext);
}

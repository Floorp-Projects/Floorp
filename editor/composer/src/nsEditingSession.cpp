/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>                     // for NULL, strcmp

#include "imgIContainer.h"              // for imgIContainer, etc
#include "mozFlushType.h"               // for mozFlushType::Flush_Frames
#include "mozilla/mozalloc.h"           // for operator new
#include "nsAString.h"
#include "nsComponentManagerUtils.h"    // for do_CreateInstance
#include "nsComposerCommandsUpdater.h"  // for nsComposerCommandsUpdater
#include "nsDebug.h"                    // for NS_ENSURE_SUCCESS, etc
#include "nsEditingSession.h"
#include "nsError.h"                    // for NS_ERROR_FAILURE, NS_OK, etc
#include "nsIChannel.h"                 // for nsIChannel
#include "nsICommandManager.h"          // for nsICommandManager
#include "nsIContentViewer.h"           // for nsIContentViewer
#include "nsIController.h"              // for nsIController
#include "nsIControllerContext.h"       // for nsIControllerContext
#include "nsIControllers.h"             // for nsIControllers
#include "nsID.h"                       // for NS_GET_IID, etc
#include "nsIDOMDocument.h"             // for nsIDOMDocument
#include "nsIDOMHTMLDocument.h"         // for nsIDOMHTMLDocument
#include "nsIDOMWindow.h"               // for nsIDOMWindow
#include "nsIDOMWindowUtils.h"          // for nsIDOMWindowUtils
#include "nsIDocShell.h"                // for nsIDocShell
#include "nsIDocument.h"                // for nsIDocument
#include "nsIDocumentStateListener.h"
#include "nsIEditor.h"                  // for nsIEditor
#include "nsIEditorDocShell.h"          // for nsIEditorDocShell
#include "nsIHTMLDocument.h"            // for nsIHTMLDocument, etc
#include "nsIInterfaceRequestorUtils.h"  // for do_GetInterface
#include "nsIPlaintextEditor.h"         // for nsIPlaintextEditor, etc
#include "nsIRefreshURI.h"              // for nsIRefreshURI
#include "nsIRequest.h"                 // for nsIRequest
#include "nsISelection.h"               // for nsISelection
#include "nsISelectionPrivate.h"        // for nsISelectionPrivate
#include "nsITimer.h"                   // for nsITimer, etc
#include "nsITransactionManager.h"      // for nsITransactionManager
#include "nsIWeakReference.h"           // for nsISupportsWeakReference, etc
#include "nsIWebNavigation.h"           // for nsIWebNavigation
#include "nsIWebProgress.h"             // for nsIWebProgress, etc
#include "nsLiteralString.h"            // for NS_LITERAL_STRING
#include "nsPICommandUpdater.h"         // for nsPICommandUpdater
#include "nsPIDOMWindow.h"              // for nsPIDOMWindow
#include "nsReadableUtils.h"            // for AppendUTF16toUTF8
#include "nsStringFwd.h"                // for nsAFlatString

class nsISupports;
class nsIURI;

/*---------------------------------------------------------------------------

  nsEditingSession

----------------------------------------------------------------------------*/
nsEditingSession::nsEditingSession()
: mDoneSetup(false)
, mCanCreateEditor(false)
, mInteractive(false)
, mMakeWholeDocumentEditable(true)
, mDisabledJSAndPlugins(false)
, mScriptsEnabled(true)
, mPluginsEnabled(true)
, mProgressListenerRegistered(false)
, mImageAnimationMode(0)
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

NS_IMPL_ISUPPORTS3(nsEditingSession, nsIEditingSession, nsIWebProgressListener, 
                   nsISupportsWeakReference)

/*---------------------------------------------------------------------------

  MakeWindowEditable

  aEditorType string, "html" "htmlsimple" "text" "textsimple"
  void makeWindowEditable(in nsIDOMWindow aWindow, in string aEditorType, 
                          in boolean aDoAfterUriLoad,
                          in boolean aMakeWholeDocumentEditable,
                          in boolean aInteractive);
----------------------------------------------------------------------------*/
#define DEFAULT_EDITOR_TYPE "html"

NS_IMETHODIMP
nsEditingSession::MakeWindowEditable(nsIDOMWindow *aWindow,
                                     const char *aEditorType, 
                                     bool aDoAfterUriLoad,
                                     bool aMakeWholeDocumentEditable,
                                     bool aInteractive)
{
  mEditorType.Truncate();
  mEditorFlags = 0;

  // disable plugins
  nsIDocShell *docShell = GetDocShellFromWindow(aWindow);
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  mDocShell = do_GetWeakReference(docShell);
  mInteractive = aInteractive;
  mMakeWholeDocumentEditable = aMakeWholeDocumentEditable;

  nsresult rv;
  if (!mInteractive) {
    rv = DisableJSAndPlugins(aWindow);
    NS_ENSURE_SUCCESS(rv, rv);
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
  rv = PrepareForEditing(aWindow);
  NS_ENSURE_SUCCESS(rv, rv);  
  
  nsCOMPtr<nsIEditorDocShell> editorDocShell;
  rv = GetEditorDocShellFromWindow(aWindow, getter_AddRefs(editorDocShell));
  NS_ENSURE_SUCCESS(rv, rv);  
  
  // set the flag on the docShell to say that it's editable
  rv = editorDocShell->MakeEditable(aDoAfterUriLoad);
  NS_ENSURE_SUCCESS(rv, rv);  

  // Setup commands common to plaintext and html editors,
  //  including the document creation observers
  // the first is an editing controller
  rv = SetupEditorCommandController("@mozilla.org/editor/editingcontroller;1",
                                    aWindow,
                                    static_cast<nsIEditingSession*>(this),
                                    &mBaseCommandControllerId);
  NS_ENSURE_SUCCESS(rv, rv);

  // The second is a controller to monitor doc state,
  // such as creation and "dirty flag"
  rv = SetupEditorCommandController("@mozilla.org/editor/editordocstatecontroller;1",
                                    aWindow,
                                    static_cast<nsIEditingSession*>(this),
                                    &mDocStateControllerId);
  NS_ENSURE_SUCCESS(rv, rv);

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

NS_IMETHODIMP
nsEditingSession::DisableJSAndPlugins(nsIDOMWindow *aWindow)
{
  nsIDocShell *docShell = GetDocShellFromWindow(aWindow);
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  bool tmp;
  nsresult rv = docShell->GetAllowJavascript(&tmp);
  NS_ENSURE_SUCCESS(rv, rv);

  mScriptsEnabled = tmp;

  rv = docShell->SetAllowJavascript(false);
  NS_ENSURE_SUCCESS(rv, rv);

  // Disable plugins in this document:
  rv = docShell->GetAllowPlugins(&tmp);
  NS_ENSURE_SUCCESS(rv, rv);

  mPluginsEnabled = tmp;

  rv = docShell->SetAllowPlugins(false);
  NS_ENSURE_SUCCESS(rv, rv);

  mDisabledJSAndPlugins = true;

  return NS_OK;
}

NS_IMETHODIMP
nsEditingSession::RestoreJSAndPlugins(nsIDOMWindow *aWindow)
{
  NS_ENSURE_TRUE(mDisabledJSAndPlugins, NS_OK);

  mDisabledJSAndPlugins = false;

  nsIDocShell *docShell = GetDocShellFromWindow(aWindow);
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsresult rv = docShell->SetAllowJavascript(mScriptsEnabled);
  NS_ENSURE_SUCCESS(rv, rv);

  // Disable plugins in this document:
  return docShell->SetAllowPlugins(mPluginsEnabled);
}

NS_IMETHODIMP
nsEditingSession::GetJsAndPluginsDisabled(bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mDisabledJSAndPlugins;
  return NS_OK;
}

/*---------------------------------------------------------------------------

  WindowIsEditable

  boolean windowIsEditable (in nsIDOMWindow aWindow);
----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::WindowIsEditable(nsIDOMWindow *aWindow, bool *outIsEditable)
{
  nsCOMPtr<nsIEditorDocShell> editorDocShell;
  nsresult rv = GetEditorDocShellFromWindow(aWindow,
                                            getter_AddRefs(editorDocShell));
  NS_ENSURE_SUCCESS(rv, rv);  

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
  "text/javascript",           // obsolete type
  "text/ecmascript",           // obsolete type
  "application/javascript",
  "application/ecmascript",
  "application/x-javascript",  // obsolete type
  "text/xul",                  // obsolete type
  "application/vnd.mozilla.xul+xml",
  NULL      // IMPORTANT! Null must be at end
};

bool
IsSupportedTextType(const char* aMIMEType)
{
  NS_ENSURE_TRUE(aMIMEType, false);

  PRInt32 i = 0;
  while (gSupportedTextTypes[i])
  {
    if (strcmp(gSupportedTextTypes[i], aMIMEType) == 0)
    {
      return true;
    }

    i ++;
  }
  
  return false;
}

/*---------------------------------------------------------------------------

  SetupEditorOnWindow

  nsIEditor setupEditorOnWindow (in nsIDOMWindow aWindow);
----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::SetupEditorOnWindow(nsIDOMWindow *aWindow)
{
  mDoneSetup = true;

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
    nsAutoString mimeType;
    if (NS_SUCCEEDED(doc->GetContentType(mimeType)))
      AppendUTF16toUTF8(mimeType, mimeCType);

    if (IsSupportedTextType(mimeCType.get()))
    {
      mEditorType.AssignLiteral("text");
      mimeCType = "text/plain";
    }
    else if (!mimeCType.EqualsLiteral("text/html") &&
             !mimeCType.EqualsLiteral("application/xhtml+xml"))
    {
      // Neither an acceptable text or html type.
      mEditorStatus = eEditorErrorCantEditMimeType;

      // Turn editor into HTML -- we will load blank page later
      mEditorType.AssignLiteral("html");
      mimeCType.AssignLiteral("text/html");
    }

    // Flush out frame construction to make sure that the subframe's
    // presshell is set up if it needs to be.
    nsCOMPtr<nsIDocument> document = do_QueryInterface(doc);
    if (document) {
      document->FlushPendingNotifications(Flush_Frames);
      if (mMakeWholeDocumentEditable) {
        document->SetEditableFlag(true);
        nsCOMPtr<nsIHTMLDocument> htmlDocument = do_QueryInterface(document);
        if (htmlDocument) {
          // Enable usage of the execCommand API
          htmlDocument->SetEditingState(nsIHTMLDocument::eDesignMode);
        }
      }
    }
  }
  bool needHTMLController = false;

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
      needHTMLController = true;
      mEditorFlags = nsIPlaintextEditor::eEditorMailMask;
    }
    else //set the flags back to textplain.
      mEditorFlags = nsIPlaintextEditor::eEditorPlaintextMask | 
                     nsIPlaintextEditor::eEditorEnableWrapHackMask;
  }
  else // Defaulted to html
  {
    needHTMLController = true;
  }

  if (mInteractive) {
    mEditorFlags |= nsIPlaintextEditor::eEditorAllowInteraction;
  }

  // make the UI state maintainer
  mStateMaintainer = new nsComposerCommandsUpdater();

  // now init the state maintainer
  // This allows notification of error state
  //  even if we don't create an editor
  rv = mStateMaintainer->Init(aWindow);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mEditorStatus != eEditorCreationInProgress)
  {
    mStateMaintainer->NotifyDocumentCreated();
    return NS_ERROR_FAILURE;
  }

  // Create editor and do other things 
  //  only if we haven't found some error above,
  nsIDocShell *docShell = GetDocShellFromWindow(aWindow);
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);  

  if (!mInteractive) {
    // Disable animation of images in this document:
    nsCOMPtr<nsIDOMWindowUtils> utils(do_GetInterface(aWindow));
    NS_ENSURE_TRUE(utils, NS_ERROR_FAILURE);

    rv = utils->GetImageAnimationMode(&mImageAnimationMode);
    NS_ENSURE_SUCCESS(rv, rv);
    utils->SetImageAnimationMode(imgIContainer::kDontAnimMode);
  }

  // create and set editor
  nsCOMPtr<nsIEditorDocShell> editorDocShell = do_QueryInterface(docShell, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Try to reuse an existing editor
  nsCOMPtr<nsIEditor> editor = do_QueryReferent(mExistingEditor);
  if (editor) {
    editor->PreDestroy(false);
  } else {
    editor = do_CreateInstance(classString, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    mExistingEditor = do_GetWeakReference(editor);
  }
  // set the editor on the docShell. The docShell now owns it.
  rv = editorDocShell->SetEditor(editor);
  NS_ENSURE_SUCCESS(rv, rv);

  // setup the HTML editor command controller
  if (needHTMLController)
  {
    // The third controller takes an nsIEditor as the context
    rv = SetupEditorCommandController("@mozilla.org/editor/htmleditorcontroller;1",
                                      aWindow, editor,
                                      &mHTMLCommandControllerId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Set mimetype on editor
  rv = editor->SetContentsMIMEType(mimeCType.get());
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContentViewer> contentViewer;
  rv = docShell->GetContentViewer(getter_AddRefs(contentViewer));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(contentViewer, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMDocument> domDoc;  
  rv = contentViewer->GetDOMDocument(getter_AddRefs(domDoc));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(domDoc, NS_ERROR_FAILURE);

  // Set up as a doc state listener
  // Important! We must have this to broadcast the "obs_documentCreated" message
  rv = editor->AddDocumentStateListener(mStateMaintainer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = editor->Init(domDoc, nullptr /* root content */,
                    nullptr, mEditorFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISelection> selection;
  editor->GetSelection(getter_AddRefs(selection));
  nsCOMPtr<nsISelectionPrivate> selPriv = do_QueryInterface(selection);
  NS_ENSURE_TRUE(selPriv, NS_ERROR_FAILURE);

  rv = selPriv->AddSelectionListener(mStateMaintainer);
  NS_ENSURE_SUCCESS(rv, rv);

  // and as a transaction listener
  nsCOMPtr<nsITransactionManager> txnMgr;
  editor->GetTransactionManager(getter_AddRefs(txnMgr));
  if (txnMgr)
    txnMgr->AddListener(mStateMaintainer);

  // Set context on all controllers to be the editor
  rv = SetEditorOnControllers(aWindow, editor);
  NS_ENSURE_SUCCESS(rv, rv);

  // Everything went fine!
  mEditorStatus = eEditorOK;

  // This will trigger documentCreation notification
  return editor->PostCreate();
}

// Removes all listeners and controllers from aWindow and aEditor.
void
nsEditingSession::RemoveListenersAndControllers(nsIDOMWindow *aWindow,
                                                nsIEditor *aEditor)
{
  if (!mStateMaintainer || !aEditor)
    return;

  // Remove all the listeners
  nsCOMPtr<nsISelection> selection;
  aEditor->GetSelection(getter_AddRefs(selection));
  nsCOMPtr<nsISelectionPrivate> selPriv = do_QueryInterface(selection);
  if (selPriv)
    selPriv->RemoveSelectionListener(mStateMaintainer);

  aEditor->RemoveDocumentStateListener(mStateMaintainer);

  nsCOMPtr<nsITransactionManager> txnMgr;
  aEditor->GetTransactionManager(getter_AddRefs(txnMgr));
  if (txnMgr)
    txnMgr->RemoveListener(mStateMaintainer);

  // Remove editor controllers from the window now that we're not
  // editing in that window any more.
  RemoveEditorControllers(aWindow);
}

/*---------------------------------------------------------------------------

  TearDownEditorOnWindow

  void tearDownEditorOnWindow (in nsIDOMWindow aWindow);
----------------------------------------------------------------------------*/
NS_IMETHODIMP
nsEditingSession::TearDownEditorOnWindow(nsIDOMWindow *aWindow)
{
  if (!mDoneSetup) {
    return NS_OK;
  }

  NS_ENSURE_TRUE(aWindow, NS_ERROR_NULL_POINTER);

  nsresult rv;
  
  // Kill any existing reload timer
  if (mLoadBlankDocTimer)
  {
    mLoadBlankDocTimer->Cancel();
    mLoadBlankDocTimer = nullptr;
  }

  mDoneSetup = false;

  // Check if we're turning off editing (from contentEditable or designMode).
  nsCOMPtr<nsIDOMDocument> domDoc;
  aWindow->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(domDoc);
  bool stopEditing = htmlDoc && htmlDoc->IsEditingOn();
  if (stopEditing)
    RemoveWebProgressListener(aWindow);

  nsCOMPtr<nsIEditorDocShell> editorDocShell;
  rv = GetEditorDocShellFromWindow(aWindow, getter_AddRefs(editorDocShell));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIEditor> editor;
  rv = editorDocShell->GetEditor(getter_AddRefs(editor));
  NS_ENSURE_SUCCESS(rv, rv);

  if (stopEditing)
    htmlDoc->TearingDownEditor(editor);

  if (mStateMaintainer && editor)
  {
    // Null out the editor on the controllers first to prevent their weak 
    // references from pointing to a destroyed editor.
    SetEditorOnControllers(aWindow, nullptr);
  }

  // Null out the editor on the docShell to trigger PreDestroy which
  // needs to happen before document state listeners are removed below.
  editorDocShell->SetEditor(nullptr);

  RemoveListenersAndControllers(aWindow, editor);

  if (stopEditing)
  {
    // Make things the way they were before we started editing.
    RestoreJSAndPlugins(aWindow);
    RestoreAnimationMode(aWindow);

    if (mMakeWholeDocumentEditable)
    {
      nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      doc->SetEditableFlag(false);
      nsCOMPtr<nsIHTMLDocument> htmlDocument = do_QueryInterface(doc);
      if (htmlDocument) {
        htmlDocument->SetEditingState(nsIHTMLDocument::eOff);
      }
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
  NS_ENSURE_SUCCESS(rv, rv);  
  
  return editorDocShell->GetEditor(outEditor);
}

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
    if (aStateFlags & nsIWebProgressListener::STATE_IS_DOCUMENT &&
        !(aStateFlags & nsIWebProgressListener::STATE_RESTORING)) {
#ifdef NOISY_DOC_LOADING
      printf("STATE_START & STATE_IS_DOCUMENT flags=%x\n", aStateFlags);
#endif

      bool progressIsForTargetDocument =
        IsProgressForTargetDocument(aWebProgress);

      if (progressIsForTargetDocument)
      {
        nsCOMPtr<nsIDOMWindow> window;
        aWebProgress->GetDOMWindow(getter_AddRefs(window));

        nsCOMPtr<nsIDOMDocument> doc;
        window->GetDocument(getter_AddRefs(doc));

        nsCOMPtr<nsIHTMLDocument> htmlDoc(do_QueryInterface(doc));

        if (htmlDoc && htmlDoc->IsWriting())
        {
          nsCOMPtr<nsIDOMHTMLDocument> htmlDomDoc = do_QueryInterface(doc);
          nsAutoString designMode;
          htmlDomDoc->GetDesignMode(designMode);

          if (designMode.EqualsLiteral("on"))
          {
            // This notification is for data coming in through
            // document.open/write/close(), ignore it.

            return NS_OK;
          }
        }

        mCanCreateEditor = true;
        StartDocumentLoad(aWebProgress, progressIsForTargetDocument);
      }
    }
  }
  //
  // A Request is being processed
  //
  else if (aStateFlags & nsIWebProgressListener::STATE_TRANSFERRING)
  {
    if (aStateFlags & nsIWebProgressListener::STATE_IS_DOCUMENT)
    {
      // document transfer started
    }
  }
  //
  // Got a redirection
  //
  else if (aStateFlags & nsIWebProgressListener::STATE_REDIRECTING)
  {
    if (aStateFlags & nsIWebProgressListener::STATE_IS_DOCUMENT)
    {
      // got a redirect
    }
  }
  //
  // A network or document Request has finished...
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
      EndDocumentLoad(aWebProgress, channel, aStatus,
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
                                   nsIRequest *aRequest, nsIURI *aURI,
                                   PRUint32 aFlags)
{
  nsCOMPtr<nsIDOMWindow> domWindow;
  nsresult rv = aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMDocument> domDoc;
  rv = domWindow->GetDocument(getter_AddRefs(domDoc));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  doc->SetDocumentURI(aURI);

  // Notify the location-changed observer that
  //  the document URL has changed
  nsIDocShell *docShell = GetDocShellFromWindow(domWindow);
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsICommandManager> commandManager = do_GetInterface(docShell);
  nsCOMPtr<nsPICommandUpdater> commandUpdater =
                                  do_QueryInterface(commandManager);
  NS_ENSURE_TRUE(commandUpdater, NS_ERROR_FAILURE);

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


/*---------------------------------------------------------------------------

  IsProgressForTargetDocument

  Check that this notification is for our document.
----------------------------------------------------------------------------*/

bool
nsEditingSession::IsProgressForTargetDocument(nsIWebProgress *aWebProgress)
{
  nsCOMPtr<nsIWebProgress> editedWebProgress = do_QueryReferent(mDocShell);
  return editedWebProgress == aWebProgress;
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
                                    bool aIsToBeMadeEditable)
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
    nsIDocShell *docShell = GetDocShellFromWindow(domWindow);
    NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
    docShell->DetachEditorFromWindow();
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
                                  bool aIsToBeMadeEditable)
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
  // that nsDocShell does (need to refactor).
  
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
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);       // better error handling?

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
    bool    makeEditable;
    editorDocShell->GetEditable(&makeEditable);
  
    if (makeEditable)
    {
      // To keep pre Gecko 1.9 behavior, setup editor always when
      // mMakeWholeDocumentEditable.
      bool needsSetup = false;
      if (mMakeWholeDocumentEditable) {
        needsSetup = true;
      } else {
        // do we already have an editor here?
        nsCOMPtr<nsIEditor> editor;
        rv = editorDocShell->GetEditor(getter_AddRefs(editor));
        NS_ENSURE_SUCCESS(rv, rv);

        needsSetup = !editor;
      }

      if (needsSetup)
      {
        mCanCreateEditor = false;
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
          NS_ENSURE_SUCCESS(rv, rv);

          mEditorStatus = eEditorCreationInProgress;
          mLoadBlankDocTimer->InitWithFuncCallback(
                                          nsEditingSession::TimerCallback,
                                          static_cast<void*> (mDocShell.get()),
                                          10, nsITimer::TYPE_ONE_SHOT);
        }
      }
    }
  }
  return rv;
}


void
nsEditingSession::TimerCallback(nsITimer* aTimer, void* aClosure)
{
  nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(static_cast<nsIWeakReference*> (aClosure));
  if (docShell)
  {
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(docShell));
    if (webNav)
      webNav->LoadURI(NS_LITERAL_STRING("about:blank").get(),
                      0, nullptr, nullptr, nullptr);
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
  aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));

  nsIDocShell *docShell = GetDocShellFromWindow(domWindow);
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  // cancel refresh from meta tags
  // we need to make sure that all pages in editor (whether editable or not)
  // can't refresh contents being edited
  nsCOMPtr<nsIRefreshURI> refreshURI = do_QueryInterface(docShell);
  if (refreshURI)
    refreshURI->CancelRefreshURITimers();

#if 0
  // Shouldn't we do this when we want to edit sub-frames?
  return MakeWindowEditable(domWindow, "html", false, mInteractive);
#else
  return NS_OK;
#endif
}

/*---------------------------------------------------------------------------

  GetDocShellFromWindow

  Utility method. This will always return nullptr if no docShell is found.
----------------------------------------------------------------------------*/
nsIDocShell *
nsEditingSession::GetDocShellFromWindow(nsIDOMWindow *aWindow)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);
  NS_ENSURE_TRUE(window, nullptr);

  return window->GetDocShell();
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
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
  
  return docShell->QueryInterface(NS_GET_IID(nsIEditorDocShell), 
                                  (void **)outDocShell);
}

/*---------------------------------------------------------------------------

  PrepareForEditing

  Set up this editing session for one or more editors
----------------------------------------------------------------------------*/
nsresult
nsEditingSession::PrepareForEditing(nsIDOMWindow *aWindow)
{
  if (mProgressListenerRegistered)
    return NS_OK;
    
  nsIDocShell *docShell = GetDocShellFromWindow(aWindow);
  
  // register callback
  nsCOMPtr<nsIWebProgress> webProgress = do_GetInterface(docShell);
  NS_ENSURE_TRUE(webProgress, NS_ERROR_FAILURE);

  nsresult rv =
    webProgress->AddProgressListener(this,
                                     (nsIWebProgress::NOTIFY_STATE_NETWORK  | 
                                      nsIWebProgress::NOTIFY_STATE_DOCUMENT |
                                      nsIWebProgress::NOTIFY_LOCATION));

  mProgressListenerRegistered = NS_SUCCEEDED(rv);

  return rv;
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
  
  nsCOMPtr<nsIControllers> controllers;      
  nsresult rv = aWindow->GetControllers(getter_AddRefs(controllers));
  NS_ENSURE_SUCCESS(rv, rv);

  // We only have to create each singleton controller once
  // We know this has happened once we have a controllerId value
  if (!*aControllerId)
  {
    nsCOMPtr<nsIController> controller;
    controller = do_CreateInstance(aControllerClassName, &rv);
    NS_ENSURE_SUCCESS(rv, rv);  

    // We must insert at head of the list to be sure our
    //   controller is found before other implementations
    //   (e.g., not-implemented versions by browser)
    rv = controllers->InsertControllerAt(0, controller);
    NS_ENSURE_SUCCESS(rv, rv);  

    // Remember the ID for the controller
    rv = controllers->GetControllerId(controller, aControllerId);
    NS_ENSURE_SUCCESS(rv, rv);  
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
  NS_ENSURE_TRUE(aWindow, NS_ERROR_NULL_POINTER);
  
  nsCOMPtr<nsIControllers> controllers;      
  nsresult rv = aWindow->GetControllers(getter_AddRefs(controllers));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> editorAsISupports = do_QueryInterface(aEditor);
  if (mBaseCommandControllerId)
  {
    rv = SetContextOnControllerById(controllers, editorAsISupports,
                                    mBaseCommandControllerId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mDocStateControllerId)
  {
    rv = SetContextOnControllerById(controllers, editorAsISupports,
                                    mDocStateControllerId);
    NS_ENSURE_SUCCESS(rv, rv);
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
  NS_ENSURE_TRUE(editorController, NS_ERROR_FAILURE);

  return editorController->SetCommandContext(aContext);
}

void
nsEditingSession::RemoveEditorControllers(nsIDOMWindow *aWindow)
{
  // Remove editor controllers from the aWindow, call when we're 
  // tearing down/detaching editor.

  nsCOMPtr<nsIControllers> controllers;
  if (aWindow)
    aWindow->GetControllers(getter_AddRefs(controllers));

  if (controllers)
  {
    nsCOMPtr<nsIController> controller;
    if (mBaseCommandControllerId)
    {
      controllers->GetControllerById(mBaseCommandControllerId,
                                     getter_AddRefs(controller));
      if (controller)
        controllers->RemoveController(controller);
    }

    if (mDocStateControllerId)
    {
      controllers->GetControllerById(mDocStateControllerId,
                                     getter_AddRefs(controller));
      if (controller)
        controllers->RemoveController(controller);
    }

    if (mHTMLCommandControllerId)
    {
      controllers->GetControllerById(mHTMLCommandControllerId,
                                     getter_AddRefs(controller));
      if (controller)
        controllers->RemoveController(controller);
    }
  }

  // Clear IDs to trigger creation of new controllers.
  mBaseCommandControllerId = 0;
  mDocStateControllerId = 0;
  mHTMLCommandControllerId = 0;
}

void
nsEditingSession::RemoveWebProgressListener(nsIDOMWindow *aWindow)
{
  nsIDocShell *docShell = GetDocShellFromWindow(aWindow);
  nsCOMPtr<nsIWebProgress> webProgress = do_GetInterface(docShell);
  if (webProgress)
  {
    webProgress->RemoveProgressListener(this);
    mProgressListenerRegistered = false;
  }
}

void
nsEditingSession::RestoreAnimationMode(nsIDOMWindow *aWindow)
{
  if (!mInteractive)
  {
    nsCOMPtr<nsIDOMWindowUtils> utils(do_GetInterface(aWindow));
    if (utils)
      utils->SetImageAnimationMode(mImageAnimationMode);
  }
}

nsresult
nsEditingSession::DetachFromWindow(nsIDOMWindow* aWindow)
{
  NS_ENSURE_TRUE(mDoneSetup, NS_OK);

  NS_ASSERTION(mStateMaintainer, "mStateMaintainer should exist.");

  // Kill any existing reload timer
  if (mLoadBlankDocTimer)
  {
    mLoadBlankDocTimer->Cancel();
    mLoadBlankDocTimer = nullptr;
  }

  // Remove controllers, webprogress listener, and otherwise
  // make things the way they were before we started editing.
  RemoveEditorControllers(aWindow);
  RemoveWebProgressListener(aWindow);
  RestoreJSAndPlugins(aWindow);
  RestoreAnimationMode(aWindow);

  // Kill our weak reference to our original window, in case
  // it changes on restore, or otherwise dies.
  mDocShell = nullptr;

  return NS_OK;
}

nsresult
nsEditingSession::ReattachToWindow(nsIDOMWindow* aWindow)
{
  NS_ENSURE_TRUE(mDoneSetup, NS_OK);

  NS_ASSERTION(mStateMaintainer, "mStateMaintainer should exist.");

  // Imitate nsEditorDocShell::MakeEditable() to reattach the
  // old editor ot the window.
  nsresult rv;

  nsIDocShell *docShell = GetDocShellFromWindow(aWindow);
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
  mDocShell = do_GetWeakReference(docShell);

  // Disable plugins.
  if (!mInteractive)
  {
    rv = DisableJSAndPlugins(aWindow);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Tells embedder that startup is in progress.
  mEditorStatus = eEditorCreationInProgress;

  // Adds back web progress listener.
  rv = PrepareForEditing(aWindow);
  NS_ENSURE_SUCCESS(rv, rv);

  // Setup the command controllers again.
  rv = SetupEditorCommandController("@mozilla.org/editor/editingcontroller;1",
                                    aWindow,
                                    static_cast<nsIEditingSession*>(this),
                                    &mBaseCommandControllerId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetupEditorCommandController("@mozilla.org/editor/editordocstatecontroller;1",
                                    aWindow,
                                    static_cast<nsIEditingSession*>(this),
                                    &mDocStateControllerId);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mStateMaintainer)
    mStateMaintainer->Init(aWindow);

  // Get editor
  nsCOMPtr<nsIEditor> editor;
  rv = GetEditorForWindow(aWindow, getter_AddRefs(editor));
  NS_ENSURE_TRUE(editor, NS_ERROR_FAILURE);

  if (!mInteractive)
  {
    // Disable animation of images in this document:
    nsCOMPtr<nsIDOMWindowUtils> utils(do_GetInterface(aWindow));
    NS_ENSURE_TRUE(utils, NS_ERROR_FAILURE);

    rv = utils->GetImageAnimationMode(&mImageAnimationMode);
    NS_ENSURE_SUCCESS(rv, rv);
    utils->SetImageAnimationMode(imgIContainer::kDontAnimMode);
  }

  // The third controller takes an nsIEditor as the context
  rv = SetupEditorCommandController("@mozilla.org/editor/htmleditorcontroller;1",
                                    aWindow, editor,
                                    &mHTMLCommandControllerId);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set context on all controllers to be the editor
  rv = SetEditorOnControllers(aWindow, editor);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  {
    bool isEditable;
    rv = WindowIsEditable(aWindow, &isEditable);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ASSERTION(isEditable, "Window is not editable after reattaching editor.");
  }
#endif // DEBUG

  return NS_OK;
}

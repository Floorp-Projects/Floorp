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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include <stdio.h>

#include "nsEditorShell.h"
#include "nsIWebShell.h"
#include "nsIBaseWindow.h"
#include "nsIContentViewerFile.h"
#include "pratom.h"
#include "prprf.h"
#include "nsIComponentManager.h"

#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIDOMDocument.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDiskDocument.h"
#include "nsIDocument.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsICSSLoader.h"
#include "nsICSSStyleSheet.h"
#include "nsIStyleSheet.h"
#include "nsIStyleSet.h"
#include "nsIContent.h"
#include "nsIHTMLContentContainer.h"
#include "nsIURI.h"
#include "nsNetUtil.h"

#include "nsIScriptGlobalObject.h"
#include "nsIWebNavigation.h"
#include "nsCOMPtr.h"

#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIWidget.h"
#include "nsIWindowMediator.h"
#include "plevent.h"
#include "nsXPIDLString.h"

#include "nsIAppShell.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"

#include "nsIDocumentViewer.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"

#include "nsIFileWidget.h"
#include "nsFileSpec.h"
#include "nsIFindComponent.h"
#include "nsIPrompt.h"
#include "nsICommonDialogs.h"

#include "nsIEditorController.h"
#include "nsEditorController.h"
#include "nsIControllers.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShellTreeNode.h"
#include "nsITransactionManager.h"
#include "nsIDocumentEncoder.h"

#include "nsIRefreshURI.h"
#include "nsIPref.h"

///////////////////////////////////////
// Editor Includes
///////////////////////////////////////
#include "nsIDOMEventCapturer.h"
#include "nsString.h"
#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDocumentLoader.h"

#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsIEditorStyleSheets.h"
#include "nsIEditorMailSupport.h"
#include "nsITableEditor.h"
#include "nsIEditorLogging.h"

#include "nsEditorCID.h"

#include "nsIComponentManager.h"
#include "nsTextServicesCID.h"
#include "nsITextServicesDocument.h"
#include "nsISpellChecker.h"
#include "nsInterfaceState.h"

#include "nsAOLCiter.h"
#include "nsInternetCiter.h"
#include "nsEditorShellMouseListener.h"

///////////////////////////////////////

// Drag & Drop, Clipboard
#include "nsWidgetsCID.h"
#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsISupportsArray.h"

/* Define Class IDs */
static NS_DEFINE_IID(kAppShellServiceCID,       NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kHTMLEditorCID,            NS_HTMLEDITOR_CID);
static NS_DEFINE_CID(kCTextServicesDocumentCID, NS_TEXTSERVICESDOCUMENT_CID);
static NS_DEFINE_IID(kCFileWidgetCID,           NS_FILEWIDGET_CID);
static NS_DEFINE_CID(kCStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kCommonDialogsCID,         NS_CommonDialog_CID );
static NS_DEFINE_CID(kDialogParamBlockCID,      NS_DialogParamBlock_CID);
static NS_DEFINE_CID(kPrefServiceCID,           NS_PREF_CID);

/* Define Interface IDs */
static NS_DEFINE_IID(kISupportsIID,             NS_ISUPPORTS_IID);

#define APP_DEBUG 0 

#define EDITOR_BUNDLE_URL "chrome://editor/locale/editor.properties"

enum {
  eEditorController,
  eComposerController
};

/////////////////////////////////////////////////////////////////////////
// Utility to extract document from a webshell object.
static nsresult
GetDocument(nsIDocShell *aDocShell, nsIDocument **aDoc ) 
{
  // Get content viewer from the web shell.
  nsCOMPtr<nsIContentViewer> contentViewer;
  nsresult res = (aDocShell && aDoc) ? 
   aDocShell->GetContentViewer(getter_AddRefs(contentViewer))
                   : NS_ERROR_NULL_POINTER;

  if ( NS_SUCCEEDED(res) && contentViewer )
  {
    // Up-cast to a document viewer.
    nsCOMPtr<nsIDocumentViewer> docViewer(do_QueryInterface(contentViewer));
    if ( docViewer )
    {
      // Get the document from the doc viewer.
      res = docViewer->GetDocument(*aDoc);
    }
  }
  return res;
}

// Utility get a UI element
static nsresult 
GetChromeElement(nsIDocShell *aShell, const char *aID, nsIDOMElement **aElement)
{
  if (!aElement) return NS_ERROR_NULL_POINTER;
  *aElement = nsnull;

  nsCOMPtr<nsIDocument> doc;
  nsresult rv = GetDocument( aShell, getter_AddRefs(doc) );
  if(NS_SUCCEEDED(rv) && doc)
  {
    nsCOMPtr<nsIDOMDocument> dDoc( do_QueryInterface(doc) );
    if ( dDoc )
    {
        nsCOMPtr<nsIDOMElement> elem;
        rv = dDoc->GetElementById( NS_ConvertASCIItoUCS2(aID), getter_AddRefs(elem) );
        if (elem)
        {
            *aElement = elem.get();
            NS_ADDREF(*aElement);
        }
    }
  }
  return rv;
}

// Utility to set and attribute of a UI element
static nsresult 
SetChromeAttribute(nsIDocShell *aShell, const char *aID, 
                    const char *aName,  const nsString &aValue)
{
  nsCOMPtr<nsIDOMElement> elem;
  nsresult rv = GetChromeElement(aShell, aID, getter_AddRefs(elem));
  if (NS_SUCCEEDED(rv) && elem)
    // Set the text attribute.
    rv = elem->SetAttribute( NS_ConvertASCIItoUCS2(aName), aValue);

  return rv;
}

// Utility to get the treeOwner for a docShell
static nsresult
GetTreeOwner(nsIDocShell* aDocShell, nsIBaseWindow** aBaseWindow)
{
   nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(aDocShell));
   NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

   nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
   docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
   NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(CallQueryInterface(treeOwner, aBaseWindow), 
      NS_ERROR_FAILURE);

   return NS_OK;
}

/////////////////////////////////////////////////////////////////////////
// nsEditorShell
/////////////////////////////////////////////////////////////////////////

nsEditorShell::nsEditorShell()
:  mMailCompose(PR_FALSE)
,  mDisplayMode(eDisplayModeNormal)
,  mHTMLSourceMode(PR_FALSE)
,  mWebShellWindow(nsnull)
,  mContentWindow(nsnull)
,  mParserObserver(nsnull)
,  mStateMaintainer(nsnull)
,  mEditorController(nsnull)
,  mDocShell(nsnull)
,  mContentAreaDocShell(nsnull)
,  mCloseWindowWhenLoaded(PR_FALSE)
,  mCantEditReason(eCantEditNoReason)
,  mEditorType(eUninitializedEditorType)
,  mWrapColumn(0)
,  mSuggestedWordIndex(0)
,  mDictionaryIndex(0)
{
  //TODO:Save last-used display mode in prefs so new window inherits?
  NS_INIT_REFCNT();
}

nsEditorShell::~nsEditorShell()
{
  NS_IF_RELEASE(mStateMaintainer);
  NS_IF_RELEASE(mParserObserver);
  
  // the only other references we hold are in nsCOMPtrs, so they'll take
  // care of themselves.
}

NS_IMPL_ADDREF(nsEditorShell)
NS_IMPL_RELEASE(nsEditorShell)

NS_IMETHODIMP 
nsEditorShell::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if ( aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)((nsIEditorShell *)this));
    AddRef();
    return NS_OK;
  }
  else if ( aIID.Equals(NS_GET_IID(nsIEditorShell)) ) {
    *aInstancePtr = (void*) ((nsIEditorShell*)this);
    AddRef();
    return NS_OK;
  }
  else if ( aIID.Equals(NS_GET_IID(nsIEditorSpellCheck)) ) {
    *aInstancePtr = (void*) ((nsIEditorSpellCheck*)this);
    AddRef();
    return NS_OK;
  }
  else if (aIID.Equals(NS_GET_IID(nsIDocumentLoaderObserver))) {
    *aInstancePtr = (void*) ((nsIDocumentLoaderObserver*)this);
     AddRef();
    return NS_OK;
  }
 
  return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP    
nsEditorShell::Init()
{  
  nsAutoString    editorType; editorType.AssignWithConversion("html");      // default to creating HTML editor
  mEditorTypeString = editorType;
  mEditorTypeString.ToLowerCase();

  // Get pointer to our string bundle
  nsresult res;
  nsCOMPtr<nsIStringBundleService> stringBundleService = do_GetService(kCStringBundleServiceCID, &res);
  if (!stringBundleService) { 
    NS_WARNING("ERROR: Failed to get StringBundle Service instance.\n");
    return res;
  }
  nsILocale* aLocale = nsnull;
  res = stringBundleService->CreateBundle(EDITOR_BUNDLE_URL, aLocale, getter_AddRefs(mStringBundle));

  // XXX: why are we returning NS_OK here rather than res?
  // is it ok to fail to get a string bundle?  if so, it should be documented.

  return NS_OK;
}

NS_IMETHODIMP    
nsEditorShell::Shutdown()
{
  nsresult rv = NS_OK;
  // Remove our document mouse event listener
  if (mMouseListenerP)
  {
    nsCOMPtr<nsIDOMEventReceiver> erP;
    rv = GetDocumentEventReceiver(getter_AddRefs(erP));
    if (NS_SUCCEEDED(rv))
    {
      if (erP)
      {
        erP->RemoveEventListenerByIID(mMouseListenerP, NS_GET_IID(nsIDOMMouseListener));
        mMouseListenerP = nsnull;
      }
      else rv = NS_ERROR_NULL_POINTER;
    }
  }
  return rv;
}

nsresult
nsEditorShell::ResetEditingState()
{
  if (!mEditor) return NS_OK;   // nothing to do

  // Mmm, we have an editor already. That means that someone loaded more than
  // one URL into the content area. Let's tear down what we have, and rip 'em a
  // new one.

  // Unload existing stylesheets
  nsCOMPtr<nsIEditorStyleSheets> styleSheets = do_QueryInterface(mEditor);
  if (styleSheets)
  {
    if (mBaseStyleSheet)
    {
      styleSheets->RemoveOverrideStyleSheet(mBaseStyleSheet);
      mBaseStyleSheet = 0;
    }
    if (mEditModeStyleSheet)
    {
      styleSheets->RemoveOverrideStyleSheet(mEditModeStyleSheet);
      mEditModeStyleSheet = 0;
    }
    if (mAllTagsModeStyleSheet)
    {
      styleSheets->RemoveOverrideStyleSheet(mAllTagsModeStyleSheet);
      mAllTagsModeStyleSheet = 0;
    }
    if (mParagraphMarksStyleSheet)
    {
      styleSheets->RemoveOverrideStyleSheet(mParagraphMarksStyleSheet);
      mParagraphMarksStyleSheet = 0;  
    }
  }
  
  nsresult rv;  
  // now, unregister the selection listener, if there was one
  if (mStateMaintainer)
  {
    nsCOMPtr<nsISelection> domSelection;
    // using a scoped result, because we don't really care if this fails
    rv = GetEditorSelection(getter_AddRefs(domSelection));
    if (NS_SUCCEEDED(rv) && domSelection)
    {
      nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(domSelection));
      selPriv->RemoveSelectionListener(mStateMaintainer);
      NS_IF_RELEASE(mStateMaintainer);
    }
  }

  // Remove our document mouse event listener
  if (mMouseListenerP)
  {
    nsCOMPtr<nsIDOMEventReceiver> erP;
    rv = GetDocumentEventReceiver(getter_AddRefs(erP));
    if (NS_SUCCEEDED(rv) && erP)
    {
      erP->RemoveEventListenerByIID(mMouseListenerP, NS_GET_IID(nsIDOMMouseListener));
      mMouseListenerP = nsnull;
    }
  }

  // clear this editor out of the controller
  if (mEditorController)
  {
    mEditorController->SetCommandRefCon(nsnull);
  }
  
  mEditorType = eUninitializedEditorType;
  mEditor = 0;  // clear out the nsCOMPtr

  // and tell them that they are doing bad things
  NS_WARNING("Multiple loads of the editor's document detected.");
  // Note that if you registered doc state listeners before the second
  // URL load, they don't get transferred to the new editor.

  return NS_OK;
}

nsresult    
nsEditorShell::PrepareDocumentForEditing(nsIDocumentLoader* aLoader, nsIURI *aUrl)
{
  if (!mContentAreaDocShell || !mContentWindow)
    return NS_ERROR_NOT_INITIALIZED;

  NS_ASSERTION(!mEditor, "Should never have an editor here");

  // get the webshell for this loader. Need this, not mContentAreaDocShell, in
  // case we are editing a frameset
  nsCOMPtr<nsISupports> loaderContainer;
  aLoader->GetContainer(getter_AddRefs(loaderContainer));
  nsCOMPtr<nsIDocShell> containerAsDocShell = do_QueryInterface(loaderContainer);
  if (!containerAsDocShell)
  {
    NS_ASSERTION(0, "Failed to get loader container as web shell");
    return NS_ERROR_UNEXPECTED;
  }
  
  // turn off animated GIFs
  nsCOMPtr<nsIPresContext> presContext;
  containerAsDocShell->GetPresContext(getter_AddRefs(presContext));
  if (presContext)
    presContext->SetImageAnimationMode(eImageAnimation_None);

  nsresult rv = DoEditorMode(containerAsDocShell);
  if (NS_FAILED(rv)) return rv;
  
  // transfer the doc state listeners to the editor
  rv = TransferDocumentStateListeners();
  if (NS_FAILED(rv)) return rv;
  
  // make the UI state maintainer
  NS_NEWXPCOM(mStateMaintainer, nsInterfaceState);
  if (!mStateMaintainer) return NS_ERROR_OUT_OF_MEMORY;
  mStateMaintainer->AddRef();      // the owning reference

  // get the Doc from the webshell
  nsCOMPtr<nsIContentViewer> cv;
  rv = mDocShell->GetContentViewer(getter_AddRefs(cv));
  if (NS_FAILED(rv)) return rv;
    
  nsCOMPtr<nsIDocumentViewer> docViewer = do_QueryInterface(cv, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDocument> chromeDoc;
  rv = docViewer->GetDocument(*getter_AddRefs(chromeDoc));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIDOMDocument> dDoc = do_QueryInterface(chromeDoc, &rv);
  if (NS_FAILED(rv)) return rv;


  // now init the state maintainer
  rv = mStateMaintainer->Init(mEditor, dDoc);
  if (NS_FAILED(rv)) return rv;
  
  // set it up as a selection listener
  nsCOMPtr<nsISelection> domSelection;
  rv = GetEditorSelection(getter_AddRefs(domSelection));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(domSelection));
  rv = selPriv->AddSelectionListener(mStateMaintainer);
  if (NS_FAILED(rv)) return rv;

  // and set it up as a doc state listener
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor, &rv);
  if (NS_FAILED(rv)) return rv;
  rv = editor->AddDocumentStateListener(NS_STATIC_CAST(nsIDocumentStateListener*, mStateMaintainer));
  if (NS_FAILED(rv)) return rv;
  
  // and as a transaction listener
  nsCOMPtr<nsITransactionManager> txnMgr;
  editor->GetTransactionManager(getter_AddRefs(txnMgr));
  if (txnMgr)
  {
    txnMgr->AddListener(NS_STATIC_CAST(nsITransactionListener*, mStateMaintainer));
  }
  
  // set the editor in the editor controller  
  if (mEditorController)
  {
    nsCOMPtr<nsISupports> editorAsISupports = do_QueryInterface(editor);
    mEditorController->SetCommandRefCon(editorAsISupports);
  }

  if (mEditorType == eHTMLTextEditorType)
  {
    // get a mouse listener for double click on tags
    // We can't use nsEditor listener because core editor shouldn't call UI commands
    rv = NS_NewEditorShellMouseListener(getter_AddRefs(mMouseListenerP), this);
    if (NS_FAILED(rv))
    {
      mMouseListenerP = nsnull;
      return rv;
    }

    // Add mouse listener to document
    nsCOMPtr<nsIDOMEventReceiver> erP;
    rv = GetDocumentEventReceiver(getter_AddRefs(erP));
    if (NS_FAILED(rv))
    {
      mMouseListenerP = nsnull;
      return rv;
    }

    rv = erP->AddEventListenerByIID(mMouseListenerP, NS_GET_IID(nsIDOMMouseListener));
    if (NS_FAILED(rv)) return rv;
  }

  // now all the listeners are set up, we can call PostCreate
  rv = editor->PostCreate();
  if (NS_FAILED(rv)) return rv;
  
  // get the URL of the page we are editing
  if (aUrl)
  {
    char* pageURLString = nsnull;                                               
    char* pageScheme = nsnull;                                                  
    aUrl->GetScheme(&pageScheme);                                               
    aUrl->GetSpec(&pageURLString);

    // Truncate the URL name at "#" named anchor and "?" query appendages
    char* temp = pageURLString;
    while (temp && *temp)
    {
      if ( *temp == '#' || *temp == '?')
        *temp = '\0';
      else
        temp++;
    }

#if DEBUG
    printf("PrepareDocumentForEditing: Editor is editing %s\n", pageURLString ? pageURLString : "");
#endif

     // only save the file spec if this is a local file, and is not              
     // about:blank                                                              
     if (nsCRT::strncmp(pageScheme, "file", 4) == 0 &&                           
         nsCRT::strncmp(pageURLString,"about:blank", 11) != 0)                   
    {
      // Clutzy method of converting URL to local file format
      // nsIFileSpec is going away --  WE NEED TO REWRITE nsIDiskDocument!
      nsFileURL    pageURL(pageURLString);
      nsFileSpec   pageSpec(pageURL);

      nsCOMPtr<nsIDOMDocument>  domDoc;
      editor->GetDocument(getter_AddRefs(domDoc));
    
      if (domDoc)
      {
        nsCOMPtr<nsIDiskDocument> diskDoc = do_QueryInterface(domDoc);
        if (diskDoc)
          diskDoc->InitDiskDocument(&pageSpec);
      }
    }
    if (pageURLString)
      nsCRT::free(pageURLString);
    if (pageScheme)
      nsCRT::free(pageScheme);
  }
  // Set the editor-specific Window caption
  UpdateWindowTitle();

  nsCOMPtr<nsIEditorStyleSheets> styleSheets = do_QueryInterface(mEditor);
  if (!styleSheets)
    return NS_NOINTERFACE;

  // Load style sheet with settings that should never
  //  change, even in "Browser" mode
  // We won't unload this, so we don't need to be returned the style sheet pointer
  
  
  styleSheets->ApplyOverrideStyleSheet(NS_ConvertASCIItoUCS2("chrome://editor/content/EditorOverride.css"),
                                       getter_AddRefs(mBaseStyleSheet));

  SetDisplayMode(mDisplayMode);

#ifdef DEBUG
  // Activate the debug menu only in debug builds
  // by removing the "hidden" attribute set "true" in XUL
  nsCOMPtr<nsIDOMElement> elem;
  rv = dDoc->GetElementById(NS_ConvertASCIItoUCS2("debugMenu"), getter_AddRefs(elem));
  if (elem)
    elem->RemoveAttribute(NS_ConvertASCIItoUCS2("hidden"));
#endif

  // Force initial focus to the content window except if in mail compose
  // why aren't we doing this in JS?
  if (!mMailCompose)
  {
    if(!mContentWindow)
      return NS_ERROR_NOT_INITIALIZED;
    nsCOMPtr<nsIDOMWindowInternal> cwP = do_QueryReferent(mContentWindow);
    if (!cwP) return NS_ERROR_NOT_INITIALIZED;
      cwP->Focus();

    //mContentWindow->Focus();
    // Collapse the selection to the begining of the document
    // (this also turns on the caret)
    mEditor->SetCaretToDocumentStart();
  }
  return NS_OK;
}

nsresult nsEditorShell::GetDocumentEventReceiver(nsIDOMEventReceiver **aEventReceiver)
{
  if (!aEventReceiver) return NS_ERROR_NULL_POINTER;
  if (!mContentWindow || !mEditor) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDOMDocument> domDoc;

  if(!mContentWindow)
    return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMWindowInternal> cwP = do_QueryReferent(mContentWindow);
  if (!cwP) return NS_ERROR_NOT_INITIALIZED;
    cwP->GetDocument(getter_AddRefs(domDoc));
  //mContentWindow->GetDocument(getter_AddRefs(domDoc));

  if (!domDoc) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDOMElement> rootElement;
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  nsresult rv = editor->GetRootElement(getter_AddRefs(rootElement));

  nsCOMPtr<nsIDOMEventReceiver> erP;
  rv = rootElement->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), getter_AddRefs(erP));

  if (erP)
  {
    *aEventReceiver = erP;
    NS_ADDREF(*aEventReceiver);
  }  
  return rv;
}


NS_IMETHODIMP    
nsEditorShell::SetContentWindow(nsIDOMWindowInternal* aWin)
{
  NS_PRECONDITION(aWin != nsnull, "null ptr");
  if (!aWin)
      return NS_ERROR_NULL_POINTER;

  mContentWindow = getter_AddRefs( NS_GetWeakReference(aWin) );  // weak reference to aWin
  //mContentWindow = aWin;

  nsresult  rv;
  nsCOMPtr<nsIScriptGlobalObject> globalObj = do_QueryReferent(mContentWindow, &rv);
  if (NS_FAILED(rv) || !globalObj)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocShell> docShell;
  globalObj->GetDocShell(getter_AddRefs(docShell));
  if (!docShell)
    return NS_ERROR_FAILURE;
    
  mContentAreaDocShell = docShell;      // dont AddRef
  rv = docShell->SetDocLoaderObserver((nsIDocumentLoaderObserver *)this);
  if (NS_FAILED(rv)) return rv;
    
  // we make two controllers
  nsCOMPtr<nsIControllers> controllers;      
  if(!mContentWindow)
    return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMWindowInternal> cwP = do_QueryReferent(mContentWindow);
  if (!cwP) return NS_ERROR_NOT_INITIALIZED;
    cwP->GetControllers(getter_AddRefs(controllers));
  //rv = mContentWindow->GetControllers(getter_AddRefs(controllers));
  if (NS_FAILED(rv)) return rv;
  
  {
    // the first is an editor controller, and takes an nsIEditor as the refCon
    nsCOMPtr<nsIController> controller = do_CreateInstance("@mozilla.org/editor/editorcontroller;1", &rv);
    if (NS_FAILED(rv)) return rv;  
    nsCOMPtr<nsIEditorController> editorController = do_QueryInterface(controller);
    rv = editorController->Init(nsnull);    // we set the editor later when we have one
    if (NS_FAILED(rv)) return rv;
    
    mEditorController = editorController;   // temp weak link, so we can get it and set the editor later
    
    rv = controllers->InsertControllerAt(eEditorController, controller);
    if (NS_FAILED(rv)) return rv;  
  }
  
  {
    // the second is a composer controller, and takes an nsIEditorShell as the refCon
    nsCOMPtr<nsIController> controller = do_CreateInstance("@mozilla.org/editor/composercontroller;1", &rv);
    if (NS_FAILED(rv)) return rv;  
    nsCOMPtr<nsIEditorController> editorController = do_QueryInterface(controller);
    
    nsCOMPtr<nsISupports> shellAsISupports = do_QueryInterface((nsIEditorShell*)this);
    rv = editorController->Init(shellAsISupports);
    if (NS_FAILED(rv)) return rv;
    
    rv = controllers->InsertControllerAt(eComposerController, controller);
    if (NS_FAILED(rv)) return rv;  
  }

  return NS_OK;
}


NS_IMETHODIMP    
nsEditorShell::GetContentWindow(nsIDOMWindowInternal * *aContentWindow)
{
  NS_ENSURE_ARG_POINTER(aContentWindow);

  if(!mContentWindow)
    return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMWindowInternal> cwP = do_QueryReferent(mContentWindow);
  if (!cwP) 
    return NS_ERROR_NOT_INITIALIZED;
  
  NS_IF_ADDREF(*aContentWindow = cwP);
  return NS_OK;
}


NS_IMETHODIMP    
nsEditorShell::SetWebShellWindow(nsIDOMWindowInternal* aWin)
{
  NS_PRECONDITION(aWin != nsnull, "null ptr");
  if (!aWin)
      return NS_ERROR_NULL_POINTER;

  mWebShellWindow = aWin;   // no addref
  
  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(aWin) );
  if (!globalObj) {
    return NS_ERROR_FAILURE;
  }

  nsresult  rv = NS_OK;
  
  nsCOMPtr<nsIDocShell> docShell;
  globalObj->GetDocShell(getter_AddRefs(docShell));
  if (!docShell)
    return NS_ERROR_NOT_INITIALIZED;

  mDocShell = docShell;

/*
#ifdef APP_DEBUG
  nsXPIDLString name;
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));
  docShellAsItem->GetName(getter_Copies(name));
  nsAutoString str(name);

  char* cstr = str.ToNewCString();
  printf("Attaching to WebShellWindow[%s]\n", cstr);
  nsCRT::free(cstr);
#endif
*/
    
  return rv;
}

NS_IMETHODIMP    
nsEditorShell::GetWebShellWindow(nsIDOMWindowInternal * *aWebShellWindow)
{
  NS_ENSURE_ARG_POINTER(aWebShellWindow);
  NS_IF_ADDREF(*aWebShellWindow = mWebShellWindow);
  return NS_OK;
}

// tell the appcore what type of editor to instantiate
// this must be called before the editor has been instantiated,
// otherwise, an error is returned.
NS_IMETHODIMP
nsEditorShell::SetEditorType(const PRUnichar *editorType)
{  
  if (mEditor)
    return NS_ERROR_ALREADY_INITIALIZED;
    
  nsAutoString  theType(editorType);
  theType.ToLowerCase();

  PRBool textMail = theType.EqualsWithConversion("textmail");
  mMailCompose = theType.EqualsWithConversion("htmlmail") || textMail;

  if (mMailCompose ||theType.EqualsWithConversion("text") || theType.EqualsWithConversion("html") || theType.IsEmpty())
  {
    // We don't store a separate type for textmail
    if (textMail)
      mEditorTypeString = NS_ConvertASCIItoUCS2("text");
    else
      mEditorTypeString = theType;
    return NS_OK;
  }
  else
  {
    NS_WARNING("Editor type not recognized");
    return NS_ERROR_UNEXPECTED;
  }
}

NS_IMETHODIMP
nsEditorShell::GetEditorType(PRUnichar **_retval)
{
  *_retval = nsnull;

  nsresult  err = NS_NOINTERFACE;
  nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor);

  if (editor)
  {
    *_retval = mEditorTypeString.ToNewUnicode();
  }
  return err;
}


nsresult
nsEditorShell::InstantiateEditor(nsIDOMDocument *aDoc, nsIPresShell *aPresShell)
{
  NS_PRECONDITION(aDoc && aPresShell, "null ptr");
  if (!aDoc || !aPresShell)
    return NS_ERROR_NULL_POINTER;

  if (mEditor)
    return NS_ERROR_ALREADY_INITIALIZED;

  nsresult err = NS_OK;
  
  nsCOMPtr<nsIEditor> editor;
  err = nsComponentManager::CreateInstance(kHTMLEditorCID, nsnull, NS_GET_IID(nsIEditor), getter_AddRefs(editor));
  if(!editor)
    err = NS_ERROR_OUT_OF_MEMORY;
  nsCOMPtr<nsISelectionController> selCon = do_QueryInterface(aPresShell);
  
  if (NS_SUCCEEDED(err))
  {
    if (mEditorTypeString.EqualsWithConversion("text"))
    {
      err = editor->Init(aDoc, aPresShell, nsnull, selCon, nsIHTMLEditor::eEditorPlaintextMask);
      mEditorType = ePlainTextEditorType;
    }
    else if (mEditorTypeString.EqualsWithConversion("html") || mEditorTypeString.IsEmpty())  // empty string default to HTML editor
    {
      err = editor->Init(aDoc, aPresShell, nsnull, selCon, 0);
      mEditorType = eHTMLTextEditorType;
    }
    else if (mEditorTypeString.EqualsWithConversion("htmlmail"))  //  HTML editor with special mail rules
    {
      err = editor->Init(aDoc, aPresShell, nsnull, selCon, nsIHTMLEditor::eEditorMailMask);
      mEditorType = eHTMLTextEditorType;
    }
    else
    {
      err = NS_ERROR_INVALID_ARG;    // this is not an editor we know about
#if DEBUG
      nsAutoString  errorMsg; errorMsg.AssignWithConversion("Failed to init editor. Unknown editor type \"");
      errorMsg += mEditorTypeString;
      errorMsg.AppendWithConversion("\"\n");
      char  *errorMsgCString = errorMsg.ToNewCString();
         NS_WARNING(errorMsgCString);
         nsCRT::free(errorMsgCString);
#endif
    }

    if (NS_SUCCEEDED(err) && editor)
    {
      mEditor = do_QueryInterface(editor);    // this does the addref that is the owning reference
    }
  }
    
  return err;
}


nsresult
nsEditorShell::DoEditorMode(nsIDocShell *aDocShell)
{
  nsresult  err = NS_OK;
  
  NS_PRECONDITION(aDocShell, "Need a webshell here");
  if (!aDocShell)
      return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDocument> doc;
  err = GetDocument(aDocShell, getter_AddRefs(doc));
  if (NS_FAILED(err)) return err;
  if (!doc) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMDocument>  domDoc = do_QueryInterface(doc, &err);
  if (NS_FAILED(err)) return err;
  if (!domDoc) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresShell> presShell;
  err = aDocShell->GetPresShell(getter_AddRefs(presShell));
  if (NS_FAILED(err)) return err;
  if (!presShell) return NS_ERROR_FAILURE;
  
  return InstantiateEditor(domDoc, presShell);
}


NS_IMETHODIMP
nsEditorShell::UpdateInterfaceState(const PRUnichar *tagToUpdate)
{
  if (!mStateMaintainer)
    return NS_ERROR_NOT_INITIALIZED;

  return mStateMaintainer->ForceUpdate(tagToUpdate);
}  

// Deletion routines
nsresult
nsEditorShell::ScrollSelectionIntoView()
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (!editor) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsISelectionController> selCon;
  editor->GetSelectionController(getter_AddRefs(selCon));
  if (!selCon)
    return NS_ERROR_UNEXPECTED;
  return selCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL,
                                            nsISelectionController::SELECTION_FOCUS_REGION);
}

NS_IMETHODIMP
nsEditorShell::DeleteCharForward()
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (!editor) return NS_ERROR_NOT_INITIALIZED;
  nsresult rv = editor->DeleteSelection(nsIEditor::eNext);
  ScrollSelectionIntoView();
  return rv;
}

NS_IMETHODIMP
nsEditorShell::DeleteCharBackward()
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (!editor) return NS_ERROR_NOT_INITIALIZED;
  nsresult rv = editor->DeleteSelection(nsIEditor::ePrevious);
  ScrollSelectionIntoView();
  return rv;
}

NS_IMETHODIMP
nsEditorShell::DeleteWordForward()
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (!editor) return NS_ERROR_NOT_INITIALIZED;
  nsresult rv = editor->DeleteSelection(nsIEditor::eNextWord);
  ScrollSelectionIntoView();
  return rv;
}

NS_IMETHODIMP
nsEditorShell::DeleteWordBackward()
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (!editor) return NS_ERROR_NOT_INITIALIZED;
  nsresult rv = editor->DeleteSelection(nsIEditor::ePreviousWord);
  ScrollSelectionIntoView();
  return rv;
}

NS_IMETHODIMP
nsEditorShell::DeleteToEndOfLine()
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (!editor) return NS_ERROR_NOT_INITIALIZED;
  nsresult rv = editor->DeleteSelection(nsIEditor::eToEndOfLine);
  ScrollSelectionIntoView();
  return rv;
}

NS_IMETHODIMP
nsEditorShell::DeleteToBeginningOfLine()
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (!editor) return NS_ERROR_NOT_INITIALIZED;
  nsresult rv = editor->DeleteSelection(nsIEditor::eToBeginningOfLine);
  ScrollSelectionIntoView();
  return rv;
}

// Generic attribute setting and removal
NS_IMETHODIMP    
nsEditorShell::SetAttribute(nsIDOMElement *element, const PRUnichar *attr, const PRUnichar *value)
{
  if (!element || !attr || !value)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (editor) {
    nsAutoString attributeStr(attr);
    nsAutoString valueStr(value);
    result = editor->SetAttribute(element, attributeStr, valueStr); 
  }
  UpdateInterfaceState(nsnull);
  return result;
}

NS_IMETHODIMP    
nsEditorShell::RemoveAttribute(nsIDOMElement *element, const PRUnichar *attr)
{
  if (!element || !attr)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (editor) {
    nsAutoString attributeStr(attr);
    result = editor->RemoveAttribute(element, attributeStr);
  }
  UpdateInterfaceState(nsnull);
  return result;
}

// the name of the attribute here should be the contents of the appropriate
// tag, e.g. 'b' for bold, 'i' for italics.
NS_IMETHODIMP    
nsEditorShell::SetTextProperty(const PRUnichar *prop, const PRUnichar *attr, const PRUnichar *value)
{
  nsresult  err = NS_NOINTERFACE;

  nsCOMPtr<nsIAtom> styleAtom = getter_AddRefs(NS_NewAtom(prop));      /// XXX Hack alert! Look in nsIEditProperty.h for this
  if (! styleAtom) return NS_ERROR_OUT_OF_MEMORY;

  nsAutoString    attributeStr(attr);
  nsAutoString    valueStr(value);
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
        // should we allow this?
    case eHTMLTextEditorType:
      err = mEditor->SetInlineProperty(styleAtom, &attributeStr, &valueStr);
      break;
    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  UpdateInterfaceState(prop);
  return err;
}



nsresult
nsEditorShell::RemoveOneProperty(const nsString& aProp, const nsString &aAttr)
{
  nsresult  err = NS_NOINTERFACE;

  nsCOMPtr<nsIAtom> styleAtom = getter_AddRefs(NS_NewAtom(aProp));      /// XXX Hack alert! Look in nsIEditProperty.h for this
  if (! styleAtom) return NS_ERROR_OUT_OF_MEMORY;

  switch (mEditorType)
  {
    case ePlainTextEditorType:
        // should we allow this?
    case eHTMLTextEditorType:
      err = mEditor->RemoveInlineProperty(styleAtom, &aAttr);
      break;
    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  UpdateInterfaceState(aProp.GetUnicode());
  return err;
}


// the name of the attribute here should be the contents of the appropriate
// tag, e.g. 'b' for bold, 'i' for italics.
NS_IMETHODIMP    
nsEditorShell::RemoveTextProperty(const PRUnichar *prop, const PRUnichar *attr)
{
  // OK, I'm really hacking now. This is just so that we can accept 'all' as input.  
  nsAutoString  allStr(prop);
  nsAutoString  aAttr(attr);
  
  allStr.ToLowerCase();
  PRBool    doingAll = (allStr.EqualsWithConversion("all"));
  nsresult  err = NS_OK;

  if (doingAll)
  {
    err = mEditor->RemoveAllInlineProperties();
  }
  else
  {
    nsAutoString  aProp(prop);
    err = RemoveOneProperty(aProp, aAttr);
  }
  
  return err;
}


NS_IMETHODIMP
nsEditorShell::GetTextProperty(const PRUnichar *prop, const PRUnichar *attr, const PRUnichar *value, PRBool *firstHas, PRBool *anyHas, PRBool *allHas)
{
  nsIAtom    *styleAtom = nsnull;
  nsresult  err = NS_NOINTERFACE;

  styleAtom = NS_NewAtom(prop);      /// XXX Hack alert! Look in nsIEditProperty.h for this

  nsAutoString  aAttr(attr);
  nsAutoString  aValue(value);
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
        // should we allow this?
    case eHTMLTextEditorType:
      err = mEditor->GetInlineProperty(styleAtom, &aAttr, &aValue, *firstHas, *anyHas, *allHas);
      break;
    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_RELEASE(styleAtom);
  return err;
}

NS_IMETHODIMP
nsEditorShell::IncreaseFontSize()
{
  nsresult  err = NS_NOINTERFACE;
  nsCOMPtr<nsIHTMLEditor>  htmlEditor = do_QueryInterface(mEditor);

  if (htmlEditor)
    err = htmlEditor->IncreaseFontSize();
  return err;
}

NS_IMETHODIMP
nsEditorShell::DecreaseFontSize()
{
  nsresult  err = NS_NOINTERFACE;
  nsCOMPtr<nsIHTMLEditor>  htmlEditor = do_QueryInterface(mEditor);

  if (htmlEditor)
    err = htmlEditor->DecreaseFontSize();
  return err;
}


NS_IMETHODIMP 
nsEditorShell::SetBackgroundColor(const PRUnichar *color)
{
  nsresult result = NS_NOINTERFACE;
  
  nsAutoString aColor(color);
  
  result = mEditor->SetBackgroundColor(aColor);

  return result;
}

NS_IMETHODIMP 
nsEditorShell::GetParagraphState(PRBool *aMixed, PRUnichar **_retval)
{
  if (!aMixed || !_retval) return NS_ERROR_NULL_POINTER;
  *_retval = nsnull;
  *aMixed = PR_FALSE;

  nsresult  err = NS_NOINTERFACE;
  nsCOMPtr<nsIHTMLEditor>  htmlEditor = do_QueryInterface(mEditor);

  if (htmlEditor)
  {
    PRBool bMixed;
    nsAutoString state;
    err = htmlEditor->GetParagraphState(bMixed, state);
    if (!bMixed)
      *_retval = state.ToNewUnicode();
  }
  return err;
}

NS_IMETHODIMP 
nsEditorShell::GetListState(PRBool *aMixed, PRUnichar **_retval)
{
  if (!aMixed || !_retval) return NS_ERROR_NULL_POINTER;
  *_retval = nsnull;
  *aMixed = PR_FALSE;

  nsresult  err = NS_NOINTERFACE;
  nsCOMPtr<nsIHTMLEditor>  htmlEditor = do_QueryInterface(mEditor);
  if (htmlEditor)
  {
    PRBool bOL, bUL, bDL;
    err = htmlEditor->GetListState(*aMixed, bOL, bUL, bDL);
    if (NS_SUCCEEDED(err))
    {
      if (!*aMixed)
      {
        nsAutoString tagStr;
        if (bOL) tagStr.AssignWithConversion("ol");
        else if (bUL) tagStr.AssignWithConversion("ul");
        else if (bDL) tagStr.AssignWithConversion("dl");
        *_retval = tagStr.ToNewUnicode();
      }
    }  
  }
  return err;
}

NS_IMETHODIMP 
nsEditorShell::GetListItemState(PRBool *aMixed, PRUnichar **_retval)
{
  if (!aMixed || !_retval) return NS_ERROR_NULL_POINTER;
  *_retval = nsnull;
  *aMixed = PR_FALSE;

  nsresult  err = NS_NOINTERFACE;
  nsCOMPtr<nsIHTMLEditor>  htmlEditor = do_QueryInterface(mEditor);
  if (htmlEditor)
  {
    PRBool bLI,bDT,bDD;
    err = htmlEditor->GetListItemState(*aMixed, bLI, bDT, bDD);
    if (NS_SUCCEEDED(err))
    {
      if (!*aMixed)
      {
        nsAutoString tagStr;
        if (bLI) tagStr.AssignWithConversion("li");
        else if (bDT) tagStr.AssignWithConversion("dt");
        else if (bDD) tagStr.AssignWithConversion("dd");
        *_retval = tagStr.ToNewUnicode();
      }
    }  
  }
  return err;
}

NS_IMETHODIMP 
nsEditorShell::GetAlignment(PRBool *aMixed, PRUnichar **_retval)
{
  if (!aMixed || !_retval) return NS_ERROR_NULL_POINTER;
  *_retval = nsnull;
  *aMixed = PR_FALSE;

  nsresult  err = NS_NOINTERFACE;
  nsCOMPtr<nsIHTMLEditor>  htmlEditor = do_QueryInterface(mEditor);
  if (htmlEditor)
  {
    nsIHTMLEditor::EAlignment firstAlign;
    err = htmlEditor->GetAlignment(*aMixed, firstAlign);
    if (NS_SUCCEEDED(err))
    {
      nsAutoString tagStr;
      if (firstAlign == nsIHTMLEditor::eLeft)        
        tagStr.AssignWithConversion("left");
      else if (firstAlign == nsIHTMLEditor::eCenter) 
        tagStr.AssignWithConversion("center");
      else if (firstAlign == nsIHTMLEditor::eRight)  
        tagStr.AssignWithConversion("right");
      *_retval = tagStr.ToNewUnicode();
    }  
  }
  return err;
}

NS_IMETHODIMP 
nsEditorShell::ApplyStyleSheet(const PRUnichar *url)
{
  nsresult result = NS_NOINTERFACE;
  
  nsAutoString  aURL(url);

  nsCOMPtr<nsIEditorStyleSheets> styleSheets = do_QueryInterface(mEditor);
  if (styleSheets)
    result = styleSheets->ApplyStyleSheet(aURL, nsnull);

  return result;
}

// Note: This is not undoable action (on purpose!)
NS_IMETHODIMP 
nsEditorShell::SetDisplayMode(PRInt32 aDisplayMode)
{
  if (aDisplayMode == eDisplayModeSource)
  {
    // We track only display modes that involve style sheet changes
    //   with mDisplayMode, so use a separate bool for source mode
    mHTMLSourceMode = PR_TRUE;
    // The HTML Source display mode is handled in editor.js
    return NS_OK;
  }
  mHTMLSourceMode = PR_FALSE;

  nsCOMPtr<nsIEditorStyleSheets> styleSheets = do_QueryInterface(mEditor);
  if (!styleSheets) return NS_NOINTERFACE;

  nsCOMPtr<nsIStyleSheet> nsISheet;
  nsresult res = NS_OK;

  // Extra style sheets to explain optimization testing:
  // eDisplayModePreview: No extra style sheets
  // eDisplayModePreview: 1 extra sheet:  mEditModeStyleSheet
  // eDisplayModeAllTags: 2 extra sheets: mEditModeStyleSheet and mAllTagsModeStyleSheet

  if (aDisplayMode == eDisplayModePreview)
  {
    // Disable all extra "edit mode" style sheets 
    if (mEditModeStyleSheet)
    {
      nsISheet = do_QueryInterface(mEditModeStyleSheet);
      res = nsISheet->SetEnabled(PR_FALSE);
      if (NS_FAILED(res)) return res;
    }
    // Disable ShowAllTags mode if that was the previous mode
    if (mDisplayMode == eDisplayModeAllTags && mAllTagsModeStyleSheet)
    {
      nsISheet = do_QueryInterface(mAllTagsModeStyleSheet);
      res = nsISheet->SetEnabled(PR_FALSE);
    }
  }
  else if (aDisplayMode == eDisplayModeNormal)
  {
    // Don't need to activate if AllTags was last mode
    if (mDisplayMode != eDisplayModeAllTags)
    {
      // If loaded before, enable the sheet
      if (mEditModeStyleSheet)
      {
        nsISheet = do_QueryInterface(mEditModeStyleSheet);
        res = nsISheet->SetEnabled(PR_TRUE);
      }
      else
      {
    
        //Load the editmode style sheet
        res = styleSheets->ApplyOverrideStyleSheet(NS_ConvertASCIItoUCS2("chrome://editor/content/EditorContent.css"),
                                                   getter_AddRefs(mEditModeStyleSheet));
      }
      if (NS_FAILED(res)) return res;
    }

    // Disable ShowAllTags mode if that was the previous mode
    if (mDisplayMode == eDisplayModeAllTags && mAllTagsModeStyleSheet)
    {
      nsISheet = do_QueryInterface(mAllTagsModeStyleSheet);
      res = nsISheet->SetEnabled(PR_FALSE);
    }
  }
  else if (aDisplayMode == eDisplayModeAllTags)
  {
    // If loaded before, enable the sheet
    if (mAllTagsModeStyleSheet)
    {
      nsISheet = do_QueryInterface(mAllTagsModeStyleSheet);
      res = nsISheet->SetEnabled(PR_TRUE);
    }
    else
    {
      // else load it
      res = styleSheets->ApplyOverrideStyleSheet(NS_ConvertASCIItoUCS2("chrome://editor/content/EditorAllTags.css"),
                                                 getter_AddRefs(mAllTagsModeStyleSheet));
    }     
    if (NS_FAILED(res)) return res;

    // We don't need to activate "normal" mode if that was the previous mode
    if (mDisplayMode != eDisplayModeNormal)
    {
      if (mEditModeStyleSheet)
      {
        nsISheet = do_QueryInterface(mEditModeStyleSheet);
        res = nsISheet->SetEnabled(PR_TRUE);
      }
      else
      {
        res = styleSheets->ApplyOverrideStyleSheet(NS_ConvertASCIItoUCS2("chrome://editor/content/EditorContent.css"),
                                                   getter_AddRefs(mEditModeStyleSheet));
      }
    }
  }

  // Remember the new mode
  if (NS_SUCCEEDED(res)) mDisplayMode = aDisplayMode;
  return res;
}

NS_IMETHODIMP 
nsEditorShell::GetEditMode(PRInt32 *_retval)
{
  if (mHTMLSourceMode)
    *_retval = eDisplayModeSource;
  else
    *_retval = mDisplayMode;

  return NS_OK;
}
  
NS_IMETHODIMP 
nsEditorShell::IsHTMLSourceMode(PRBool *_retval)
{
  *_retval = mHTMLSourceMode;

  return NS_OK;
}

NS_IMETHODIMP 
nsEditorShell::FinishHTMLSource(void)
{
  if (mHTMLSourceMode)
  {
    // Call the JS command to convert and switch to previous edit mode
    nsAutoString command(NS_LITERAL_STRING("cmd_FinishHTMLSource"));
    return DoControllerCommand(command);
  }
  return NS_OK;
}


NS_IMETHODIMP 
nsEditorShell::DisplayParagraphMarks(PRBool aShowMarks)
{
  nsresult  res = NS_OK;

  nsCOMPtr<nsIEditorStyleSheets> styleSheets = do_QueryInterface(mEditor);
  if (!styleSheets) return NS_NOINTERFACE;
  nsCOMPtr<nsIStyleSheet> nsISheet;
  if (aShowMarks)
  {
    // Check if style sheet is already loaded -- just enable it
    if (mParagraphMarksStyleSheet)
    {
      nsISheet = do_QueryInterface(mParagraphMarksStyleSheet);
      return nsISheet->SetEnabled(PR_TRUE);
    }
    //First time used -- load the style sheet
    nsCOMPtr<nsICSSStyleSheet> styleSheet;
    res = styleSheets->ApplyOverrideStyleSheet(NS_ConvertASCIItoUCS2("chrome://editor/content/EditorParagraphMarks.css"),
                                                getter_AddRefs(mParagraphMarksStyleSheet));
  }
  else if (mParagraphMarksStyleSheet)
  {
    // Disable the style sheet
    nsISheet = do_QueryInterface(mParagraphMarksStyleSheet);
    res = nsISheet->SetEnabled(PR_FALSE);
  }
  
  return res;
}

NS_IMETHODIMP 
nsEditorShell::SetBodyAttribute(const PRUnichar *attr, const PRUnichar *value)
{
  nsresult result = NS_NOINTERFACE;
  
  nsAutoString  aAttr(attr);
  nsAutoString  aValue(value);
  
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
    {
      result = mEditor->SetBodyAttribute(aAttr, aValue);
      break;
    }
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::LoadUrl(const PRUnichar *url)
{
  if(!mContentAreaDocShell)
    return NS_ERROR_NOT_INITIALIZED;

  nsresult rv = ResetEditingState();
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mContentAreaDocShell));
  NS_ENSURE_SUCCESS(webNav->LoadURI(url, nsIWebNavigation::LOAD_FLAGS_NONE), NS_ERROR_FAILURE);

  return NS_OK;
}


NS_IMETHODIMP    
nsEditorShell::RegisterDocumentStateListener(nsIDocumentStateListener *docListener)
{
  nsresult rv = NS_OK;
  
  if (!docListener)
    return NS_ERROR_NULL_POINTER;
    
  // Make the array
  if (!mDocStateListeners)
  {
    rv = NS_NewISupportsArray(getter_AddRefs(mDocStateListeners));
    if (NS_FAILED(rv)) return rv;
  }
  nsCOMPtr<nsISupports> iSupports = do_QueryInterface(docListener, &rv);
  if (NS_FAILED(rv)) return rv;

  PRBool appended = mDocStateListeners->AppendElement(iSupports);
  NS_ASSERTION(appended, "Append failed");
  
  // if we have an editor already, register this right now.
  if (mEditor)
  {
    nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor, &rv);
    if (NS_FAILED(rv)) return rv;
  
    // this checks for duplicates
    rv = editor->AddDocumentStateListener(docListener);
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsEditorShell::UnregisterDocumentStateListener(nsIDocumentStateListener *docListener)
{
  if (!docListener)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;
  
  // remove it from our list
  if (mDocStateListeners)
  {
    nsCOMPtr<nsISupports> iSupports = do_QueryInterface(docListener, &rv);
    if (NS_FAILED(rv)) return rv;

    PRBool removed = mDocStateListeners->RemoveElement(iSupports);
  }
    
  // if we have an editor already, remove it from there too
  if (mEditor)
  {
    nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor, &rv);
    if (NS_FAILED(rv)) return rv;
  
    return editor->RemoveDocumentStateListener(docListener);
  }


  return NS_OK;
}

// called after making an editor. Transfer the nsIDOcumentStateListeners
// that we have been stashing in mDocStateListeners to the editor.
nsresult    
nsEditorShell::TransferDocumentStateListeners()
{
  if (!mDocStateListeners)
    return NS_OK;
   
  if (!mEditor)
    return NS_ERROR_NOT_INITIALIZED;    // called too early.

  nsresult  rv;
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor, &rv);
  if (NS_FAILED(rv)) return rv;
    
  PRUint32 numListeners;  
  mDocStateListeners->Count(&numListeners);

  for (PRUint32 i = 0; i <  numListeners; i ++)
  {
    nsCOMPtr<nsISupports> iSupports = getter_AddRefs(mDocStateListeners->ElementAt(i));
    nsCOMPtr<nsIDocumentStateListener> docStateListener = do_QueryInterface(iSupports);
    if (docStateListener)
    {
      // this checks for duplicates
      rv = editor->AddDocumentStateListener(docStateListener);
      if (NS_FAILED(rv)) break;
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsEditorShell::CheckOpenWindowForURLMatch(const PRUnichar* inFileURL, nsIDOMWindowInternal* inCheckWindow, PRBool *aDidFind)
{
  if (!inCheckWindow) return NS_ERROR_NULL_POINTER;
  *aDidFind = PR_FALSE;
  

  // get an nsFileSpec from the URL
  // This assumes inFileURL is "file://" format
  nsAutoString fileURLString(inFileURL);
  nsFileURL    fileURL(fileURLString);
  nsFileSpec   fileSpec(fileURL);

  nsCOMPtr<nsIDOMWindowInternal> contentWindow;
  inCheckWindow->Get_content(getter_AddRefs(contentWindow));
  if (contentWindow)
  {
    // get the content doc
    nsCOMPtr<nsIDOMDocument> contentDoc;          
    contentWindow->GetDocument(getter_AddRefs(contentDoc));
    if (contentDoc)
    {
      nsCOMPtr<nsIDiskDocument> diskDoc(do_QueryInterface(contentDoc));
      if (diskDoc)
      {
        nsFileSpec docFileSpec;
        if (NS_SUCCEEDED(diskDoc->GetFileSpec(docFileSpec)))
        {
          // is this the filespec we are looking for?
          if (docFileSpec == fileSpec)
          {
            *aDidFind = PR_TRUE;
          }
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsEditorShell::SaveDocument(PRBool saveAs, PRBool saveCopy, PRBool *_retval)
{
  nsresult  res = NS_NOINTERFACE;
  *_retval = PR_FALSE;

  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
    {
      nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
      if (editor)
      {
        // get the document
        nsCOMPtr<nsIDOMDocument> doc;
        res = editor->GetDocument(getter_AddRefs(doc));
        if (NS_FAILED(res)) return res;
        if (!doc) return NS_ERROR_NULL_POINTER;
  
        nsCOMPtr<nsIDiskDocument> diskDoc = do_QueryInterface(doc);
        if (!diskDoc)
          return NS_ERROR_NO_INTERFACE;

        // find out if the doc already has a fileSpec associated with it.
        nsFileSpec    docFileSpec;
        PRBool noFileSpec = (diskDoc->GetFileSpec(docFileSpec) == NS_ERROR_NOT_INITIALIZED);
        PRBool mustShowFileDialog = saveAs || noFileSpec;
        PRBool replacing = !saveAs;

        
        // Get existing document title
        nsAutoString title;
        nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(doc);
        if (!htmlDoc) return NS_ERROR_FAILURE;
        res = htmlDoc->GetTitle(title);
        if (NS_FAILED(res)) return res;
        
        if (mustShowFileDialog)
        {
          // Prompt for title ONLY if existing title is empty
          if (!mMailCompose && (mEditorType == eHTMLTextEditorType) && (title.Length() == 0))
          {
            // Use a "prompt" common dialog to get title string from user
            NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &res); 
            if (NS_SUCCEEDED(res)) 
            { 
              PRUnichar *titleUnicode;
              nsAutoString captionStr, msgStr1, msgStr2;
              
              GetBundleString(NS_ConvertASCIItoUCS2("DocumentTitle"), captionStr);
              GetBundleString(NS_ConvertASCIItoUCS2("NeedDocTitle"), msgStr1); 
              GetBundleString(NS_ConvertASCIItoUCS2("DocTitleHelp"), msgStr2);
              msgStr1 += NS_ConvertASCIItoUCS2("\n") + msgStr2;
              
              PRBool retVal = PR_FALSE;
              if(!mContentWindow)
                return NS_ERROR_NOT_INITIALIZED;
              nsCOMPtr<nsIDOMWindowInternal> cwP = do_QueryReferent(mContentWindow);
              if (!cwP) return NS_ERROR_NOT_INITIALIZED;

              res = dialog->Prompt(cwP, captionStr.GetUnicode(), msgStr1.GetUnicode(),
                                   title.GetUnicode(), &titleUnicode, &retVal); 
              
              if( retVal == PR_FALSE)
              {
                // This indicates Cancel was used -- don't continue saving
                *_retval = PR_FALSE;
                return NS_OK;
              }
              title = titleUnicode;
              nsCRT::free(titleUnicode);
            }
            // This sets title in HTML node
            SetDocumentTitle(title.GetUnicode());
          }

// #if 0 Replace this with nsIFilePicker code 

          nsCOMPtr<nsIFileWidget>  fileWidget;
          res = nsComponentManager::CreateInstance(kCFileWidgetCID, nsnull, 
                                                   NS_GET_IID(nsIFileWidget), 
                                                   getter_AddRefs(fileWidget));
          if (NS_SUCCEEDED(res) && fileWidget)
          {
            nsAutoString  promptString;
            GetBundleString(NS_ConvertASCIItoUCS2("SaveDocumentAs"), promptString);

            nsString* titles = nsnull;
            nsString* filters = nsnull;
            nsString* nextTitle;
            nsString* nextFilter;
            nsAutoString HTMLFiles;
            nsAutoString TextFiles;
            nsAutoString fileName;
            nsFileSpec parentPath;

            titles = new nsString[3];
            if (!titles)
              return NS_ERROR_OUT_OF_MEMORY;

            filters = new nsString[3];
            if (!filters)
            {
              delete [] titles;
              return NS_ERROR_OUT_OF_MEMORY;
            }
            nextTitle = titles;
            nextFilter = filters;
            // The names of the file types are localizable
            GetBundleString(NS_ConvertASCIItoUCS2("HTMLFiles"), HTMLFiles);
            GetBundleString(NS_ConvertASCIItoUCS2("TextFiles"), TextFiles);
            if (! (HTMLFiles.Length() == 0 || TextFiles.Length() == 0))
            {
              nsAutoString allFilesStr;
              GetBundleString(NS_ConvertASCIItoUCS2("AllFiles"), allFilesStr);
              
            *nextTitle++ = HTMLFiles;
            (*nextFilter++).AssignWithConversion("*.htm; *.html; *.shtml");
            *nextTitle++ = TextFiles;
            (*nextFilter++).AssignWithConversion("*.txt");
            *nextTitle++ = allFilesStr;
            (*nextFilter++).AssignWithConversion("*.*");
            fileWidget->SetFilterList(3, titles, filters);
            }
            
            if (noFileSpec)
            {
              // check the current url, use that file name if possible
              nsString urlstring;
              res = htmlDoc->GetURL(urlstring);

     //         ?????
     //         res = htmlDoc->GetSourceDocumentURL(jscx, uri);
     //         do a QI to get an nsIURL and then call GetFileName()

              // if it's not a local file already, grab the current file name
              if ( (urlstring.CompareWithConversion("file", PR_TRUE, 4) != 0 )
                && (urlstring.CompareWithConversion("about:blank", PR_TRUE, -1) != 0) )
              {
                // remove cruft before file name including '/'
                // if the url ends with a '/' then the whole string will be cut
                PRInt32 index = urlstring.RFindChar((PRUnichar)'/', PR_FALSE, -1, -1 );
                if ( index != -1 )
                {
                  urlstring.Cut(0, index + 1);
                  if (urlstring.Length() > 0)
                  {
                    // Then truncate at any existing "#", "?" or "." since we replace with ".html"
                    index = urlstring.RFindChar((PRUnichar)'.', PR_FALSE, -1, -1 );
                    if ( index != -1)
                      urlstring.Truncate(index);
                    if (urlstring.Length() > 0)
                    {
                      index = urlstring.RFindChar((PRUnichar)'#', PR_FALSE, -1, -1 );
                      if ( index != -1)
                        urlstring.Truncate(index);
                      if (urlstring.Length() > 0)
                      {
                        index = urlstring.RFindChar((PRUnichar)'?', PR_FALSE, -1, -1 );
                        if ( index != -1)
                          urlstring.Truncate(index);
                      }
                    }
                    if (urlstring.Length() > 0)
                    {
                      fileName.Assign( urlstring );
                      fileName.AppendWithConversion(".html");
                    }
                  }
                }
              }
              
              // Use page title as suggested name for new document
              if (fileName.Length() == 0 && title.Length() > 0)
              {
                //Replace "bad" filename characteres with "_"
                PRUnichar space = (PRUnichar)' ';
                PRUnichar dot = (PRUnichar)'.';
                PRUnichar bslash = (PRUnichar)'\\';
                PRUnichar fslash = (PRUnichar)'/';
                PRUnichar at = (PRUnichar)'@';
                PRUnichar colon = (PRUnichar)':';
                PRUnichar underscore = (PRUnichar)'_';
                title.ReplaceChar(space, underscore);
                title.ReplaceChar(dot, underscore);
                title.ReplaceChar(bslash, underscore);
                title.ReplaceChar(fslash, underscore);
                title.ReplaceChar(at, underscore);
                title.ReplaceChar(colon, underscore);
                fileName = title;
                fileName.AppendWithConversion(".html");
              }
            } 
            else
            {
              char *leafName = docFileSpec.GetLeafName();
              if (leafName)
              {
                fileName.AssignWithConversion(leafName);
                nsCRT::free(leafName);
              }
              docFileSpec.GetParent(parentPath);

              // TODO: CHANGE TO THE DIRECTORY OF THE PARENT PATH?
            }
            
            if (fileName.Length() > 0)
              fileWidget->SetDefaultString(fileName);

            nsFileDlgResults dialogResult;
            // 1ST PARAM SHOULD BE nsIDOMWindowInternal*, not nsIWidget*
            dialogResult = fileWidget->PutFile(nsnull, promptString, docFileSpec);
            delete [] titles;
            delete [] filters;

            if (dialogResult == nsFileDlgResults_Cancel)
            {
              // Note that *_retval = PR_FALSE at this point
              return NS_OK;
            }
            replacing = (dialogResult == nsFileDlgResults_Replace);
          }
          else
          {
             NS_ASSERTION(0, "Failed to get file widget");
            return res;
          }
//#endif end replace with nsIFilePicker block

          // Set the new URL for the webshell
          nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mContentAreaDocShell));
          if (webShell)
          {
            nsFileURL fileURL(docFileSpec);
            nsAutoString fileURLString; fileURLString.AssignWithConversion(fileURL.GetURLString());
            PRUnichar *fileURLUnicode = fileURLString.ToNewUnicode();
            if (fileURLUnicode)
            {
              webShell->SetURL(fileURLUnicode);
        			Recycle(fileURLUnicode);
            }
          }
        } // mustShowFileDialog

        // TODO: Get the file type (from the extension?) the user set for the file
        // How do we do this in an XP way??? 
        // For now, just save as HTML type
        nsString format;
        format.AssignWithConversion("text/html");
        res = editor->SaveFile(&docFileSpec, replacing, saveCopy, format);
        if (NS_FAILED(res))
        {
          nsAutoString saveDocStr, failedStr;
          GetBundleString(NS_ConvertASCIItoUCS2("SaveDocument"), saveDocStr);
          GetBundleString(NS_ConvertASCIItoUCS2("SaveFileFailed"), failedStr);
          Alert(saveDocStr, failedStr);
        } else {
          // File was saved successfully
          *_retval = PR_TRUE;

          // Update window title to show possibly different filename
          if (mustShowFileDialog)
            UpdateWindowTitle();
        }
      }
      break;
    }
    default:
      res = NS_ERROR_NOT_IMPLEMENTED;
  }
  return res;
}

NS_IMETHODIMP    
nsEditorShell::CloseWindowWithoutSaving()
{
  nsCOMPtr<nsIBaseWindow> baseWindow;
  GetTreeOwner(mDocShell, getter_AddRefs(baseWindow));
  NS_ENSURE_TRUE(baseWindow, NS_ERROR_FAILURE);
  return baseWindow->Destroy();
}

NS_IMETHODIMP    
nsEditorShell::Print()
{ 
  if (!mContentAreaDocShell)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIContentViewer> viewer;
  mContentAreaDocShell->GetContentViewer(getter_AddRefs(viewer));    
  if (nsnull != viewer) 
  {
    nsCOMPtr<nsIContentViewerFile> viewerFile = do_QueryInterface(viewer);
    if (viewerFile) {
      NS_ENSURE_SUCCESS(viewerFile->Print(PR_FALSE,nsnull), NS_ERROR_FAILURE);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsEditorShell::GetLocalFileURL(nsIDOMWindowInternal *parent, const PRUnichar *filterType, PRUnichar **_retval)
{
  nsAutoString FilterType(filterType);
  PRBool htmlFilter = FilterType.EqualsIgnoreCase("html");
  PRBool imgFilter = FilterType.EqualsIgnoreCase("img");

  *_retval = nsnull;
  
  // TODO: DON'T ACCEPT NULL PARENT AFTER WIDGET IS FIXED
  if (/*parent||*/ !(htmlFilter || imgFilter))
    return NS_ERROR_NOT_INITIALIZED;


  nsCOMPtr<nsIFileWidget>  fileWidget;
  nsAutoString HTMLTitle;
  GetBundleString(NS_ConvertASCIItoUCS2("OpenHTMLFile"), HTMLTitle);

  // An empty string should just result in "Open" for the dialog
  nsAutoString title;
  if (htmlFilter)
  {
    title = HTMLTitle;
  } else {
    nsAutoString ImageTitle;
    GetBundleString(NS_ConvertASCIItoUCS2("SelectImageFile"), ImageTitle);

    if (ImageTitle.Length() > 0 && imgFilter)
      title = ImageTitle;
  }

  nsFileSpec fileSpec;
  // TODO: GET THE DEFAULT DIRECTORY FOR DIFFERENT TYPES FROM PREFERENCES
  nsFileSpec aDisplayDirectory;

  nsresult res = nsComponentManager::CreateInstance(kCFileWidgetCID,
                                                    nsnull,
                                                    NS_GET_IID(nsIFileWidget),
                                                    (void**)&fileWidget);
  if (NS_SUCCEEDED(res))
  {
    nsFileDlgResults dialogResult;
    nsString* titles = nsnull;
    nsString* filters = nsnull;
    nsString* nextTitle;
    nsString* nextFilter;

    nsAutoString tempStr;

    if (htmlFilter)
    {
      titles = new nsString[3];
      filters = new nsString[3];
      if (!titles || ! filters)
        return NS_ERROR_OUT_OF_MEMORY;

      nextTitle = titles;
      nextFilter = filters;
      GetBundleString(NS_ConvertASCIItoUCS2("HTMLFiles"), tempStr);
      *nextTitle++ = tempStr;
      GetBundleString(NS_ConvertASCIItoUCS2("TextFiles"), tempStr);
      *nextTitle++ = tempStr;
      (*nextFilter++).AssignWithConversion("*.htm; *.html; *.shtml");
      (*nextFilter++).AssignWithConversion("*.txt");
      fileWidget->SetFilterList(3, titles, filters);
    } else {
      titles = new nsString[2];
      filters = new nsString[2];
      if (!titles || ! filters)
        return NS_ERROR_OUT_OF_MEMORY;
      nextTitle = titles;
      nextFilter = filters;
      GetBundleString(NS_ConvertASCIItoUCS2("IMGFiles"), tempStr);
      *nextTitle++ = tempStr;
      (*nextFilter++).AssignWithConversion("*.gif; *.jpg; *.jpeg; *.png; *.*");
      fileWidget->SetFilterList(2, titles, filters);
    }
    GetBundleString(NS_ConvertASCIItoUCS2("AllFiles"), tempStr);
    *nextTitle++ = tempStr;
    (*nextFilter++).AssignWithConversion("*.*");
    // First param should be Parent window, but type is nsIWidget*
    // Bug is filed to change this to a more suitable window type
    dialogResult = fileWidget->GetFile(/*parent*/ nsnull, title, fileSpec);
    delete [] titles;
    delete [] filters;

    // Do this after we get this from preferences
    //fileWidget->SetDisplayDirectory(aDisplayDirectory);
    
    if (dialogResult != nsFileDlgResults_Cancel) 
    {
      // Get the platform-specific format
      // Convert it to the string version of the URL format
      // NOTE: THIS CRASHES IF fileSpec is empty
      nsFileURL url(fileSpec);
      nsAutoString  returnVal; returnVal.AssignWithConversion(url.GetURLString());
      *_retval = returnVal.ToNewUnicode();
    }
    // TODO: SAVE THIS TO THE PREFS?
    fileWidget->GetDisplayDirectory(aDisplayDirectory);
  }

  return res;
}

nsresult
nsEditorShell::UpdateWindowTitle()
{
  nsresult res = NS_ERROR_NOT_INITIALIZED;

  if (!mContentAreaDocShell || !mEditor)
    return res;

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (!editor)
    return res;

  nsAutoString windowCaption;
  res = GetDocumentTitleString(windowCaption);
  // If title is empty, use "untitled"
  if (windowCaption.Length() == 0)
    GetBundleString(NS_ConvertASCIItoUCS2("untitled"), windowCaption);

  // Append just the 'leaf' filename to the Doc. Title for the window caption
  if (NS_SUCCEEDED(res))
  {
    nsCOMPtr<nsIDOMDocument>  domDoc;
    editor->GetDocument(getter_AddRefs(domDoc));
    if (domDoc)
    {
      nsCOMPtr<nsIDiskDocument> diskDoc = do_QueryInterface(domDoc);
      if (diskDoc)
      {
        // find out if the doc already has a fileSpec associated with it.
        nsFileSpec docFileSpec;
        if (NS_SUCCEEDED(diskDoc->GetFileSpec(docFileSpec)))
        {
          nsAutoString name;
          docFileSpec.GetLeafName(name);
          windowCaption.AppendWithConversion(" [");
          windowCaption += name;
          windowCaption.AppendWithConversion("]");
        }
      }
    }
    nsCOMPtr<nsIBaseWindow> contentAreaAsWin(do_QueryInterface(mContentAreaDocShell));
    NS_ASSERTION(contentAreaAsWin, "This object should implement nsIBaseWindow");
    res = contentAreaAsWin->SetTitle(windowCaption.GetUnicode());
  }
  return res;
}

nsresult
nsEditorShell::GetDocumentTitleString(nsString& title)
{
  nsresult res = NS_ERROR_NOT_INITIALIZED;

  if (!mEditor)
    return res;

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (!editor)
    return res;

  nsCOMPtr<nsIDOMDocument>  domDoc;
  res = editor->GetDocument(getter_AddRefs(domDoc));
  if (NS_SUCCEEDED(res) && domDoc)
  {
    // Get the document title
    nsCOMPtr<nsIDOMHTMLDocument> HTMLDoc = do_QueryInterface(domDoc);
    if (HTMLDoc)
      res = HTMLDoc->GetTitle(title);
  }
  return res;
}

// JavaScript version
NS_IMETHODIMP
nsEditorShell::GetDocumentTitle(PRUnichar **title)
{
  if (!title)
    return NS_ERROR_NULL_POINTER;

  nsAutoString titleStr;
  nsresult res = GetDocumentTitleString(titleStr);
  if (NS_SUCCEEDED(res))
  {
    *title = titleStr.ToNewUnicode();
  } else {
    // Don't fail, just return an empty string    
    nsAutoString empty;
    *title = empty.ToNewUnicode();
    res = NS_OK;
  }
  return res;
}

NS_IMETHODIMP
nsEditorShell::SetDocumentTitle(const PRUnichar *title)
{
  nsresult res = NS_ERROR_NOT_INITIALIZED;

  if (!mEditor && !mContentAreaDocShell)
    return res;

  // This should only be allowed for HTML documents
  if (mEditorType != eHTMLTextEditorType)
    return NS_ERROR_NOT_IMPLEMENTED;

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (!editor)
    return NS_ERROR_FAILURE;

  nsAutoString titleStr(title);
  nsCOMPtr<nsIDOMDocument>  domDoc;
  res = editor->GetDocument(getter_AddRefs(domDoc));
  
  if (domDoc)
  {
    // Get existing document title node
    nsCOMPtr<nsIDOMHTMLDocument> HTMLDoc = do_QueryInterface(domDoc);
    if (HTMLDoc)
    {
      // This sets the window title, and saves the title as a member varialble,
      //  but does NOT insert the <title> node.
      HTMLDoc->SetTitle(titleStr);
      
      nsCOMPtr<nsIDOMNodeList> titleList;
      nsCOMPtr<nsIDOMNode>titleNode;
      nsCOMPtr<nsIDOMNode>headNode;
      nsCOMPtr<nsIDOMNode> resultNode;
      res = domDoc->GetElementsByTagName(NS_ConvertASCIItoUCS2("title"), getter_AddRefs(titleList));
      if (NS_SUCCEEDED(res))
      {
        if(titleList)
        {
          /* I'm tempted to just get the 1st title element in the list
             (there should always be just 1). But in case there's > 1,
             I assume the last one will be used, so this finds that one.
          */
          PRUint32 len = 0;
          titleList->GetLength(&len);
          if (len >= 1)
            titleList->Item(len-1, getter_AddRefs(titleNode));

          if (titleNode)
          {
            //Delete existing children (text) of title node
            nsCOMPtr<nsIDOMNodeList> children;
            res = titleNode->GetChildNodes(getter_AddRefs(children));
            if(NS_SUCCEEDED(res) && children)
            {
              PRUint32 count = 0;
              children->GetLength(&count);
              for( PRUint32 i = 0; i < count; i++)
              {
                nsCOMPtr<nsIDOMNode> child;
                res = children->Item(i,getter_AddRefs(child));
                if(NS_SUCCEEDED(res) && child)
                  titleNode->RemoveChild(child,getter_AddRefs(resultNode));
              }
            }
          }
        }
      }
      // Get the <HEAD> node, create a <TITLE> and insert it under the HEAD
      nsCOMPtr<nsIDOMNodeList> headList;
      res = domDoc->GetElementsByTagName(NS_ConvertASCIItoUCS2("head"),getter_AddRefs(headList));
      if (NS_FAILED(res)) return res;
      if (headList) 
      {
        headList->Item(0, getter_AddRefs(headNode));
        if (headNode) 
        {
          PRBool newTitleNode = PR_FALSE;
          if (!titleNode)
          {
            // Didn't find one above: Create a new one
            nsCOMPtr<nsIDOMElement>titleElement;
            res = domDoc->CreateElement(NS_ConvertASCIItoUCS2("title"), getter_AddRefs(titleElement));
            if (NS_SUCCEEDED(res) && titleElement)
            {
              titleNode = do_QueryInterface(titleElement);
              newTitleNode = PR_TRUE;
            }
            // Note: There should ALWAYS be a <title> in any HTML document,
            //       so we will insert the node and not make it undoable
            res = headNode->AppendChild(titleNode, getter_AddRefs(resultNode));
            if (NS_FAILED(res)) return res;
          }
          // Append a text node under the TITLE
          //  only if the title text isn't empty
          if (titleNode && titleStr.Length() > 0)
          {
            nsCOMPtr<nsIDOMText> textNode;
            res = domDoc->CreateTextNode(titleStr, getter_AddRefs(textNode));
            if (NS_FAILED(res)) return res;
            if (!textNode) return NS_ERROR_FAILURE;
            // Do NOT use editor transaction -- we don't want this to be undoable
            res = titleNode->AppendChild(textNode, getter_AddRefs(resultNode));
            if (NS_FAILED(res)) return res;
          }
        }
      }
    }
  }

  return res;
}

NS_IMETHODIMP
nsEditorShell::CloneAttributes(nsIDOMNode *destNode, nsIDOMNode *sourceNode)
{
  if (!destNode || !sourceNode) { return NS_ERROR_NULL_POINTER; }
  nsresult  rv = NS_NOINTERFACE;
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor);
        if (editor)
          rv = editor->CloneAttributes(destNode, sourceNode);
      }
      break;

    default:
      rv = NS_ERROR_NOT_IMPLEMENTED;
  }

  return rv;
}

NS_IMETHODIMP
nsEditorShell::NodeIsBlock(nsIDOMNode *node, PRBool *_retval)
{
  if (!node || !_retval) { return NS_ERROR_NULL_POINTER; }
  nsresult  rv = NS_NOINTERFACE;
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor);
        if (editor)
          rv = editor->NodeIsBlock(node, *_retval);
      }
      break;

    default:
      rv = NS_ERROR_NOT_IMPLEMENTED;
  }

  return rv;
}

NS_IMETHODIMP    
nsEditorShell::Undo()
{ 
  nsresult  err = NS_NOINTERFACE;
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor);
        if (editor)
          err = editor->Undo(1);
      }
      break;

    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}

NS_IMETHODIMP    
nsEditorShell::Redo()
{  
  nsresult  err = NS_NOINTERFACE;
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor);
        if (editor)
          err = editor->Redo(1);
      }
      break;

    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}

NS_IMETHODIMP    
nsEditorShell::Cut()
{  
  nsresult  err = NS_NOINTERFACE;
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor);
        if (editor)
          err = editor->Cut();
      }
      break;

    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}

NS_IMETHODIMP    
nsEditorShell::Copy()
{  
  nsresult  err = NS_NOINTERFACE;
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor);
        if (editor)
          err = editor->Copy();
      }
      break;

    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}

NS_IMETHODIMP    
nsEditorShell::Paste(PRInt32 aSelectionType)
{  
  nsresult  err = NS_NOINTERFACE;
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor);
        if (editor)
          err = editor->Paste(aSelectionType);
      }
      break;

    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}

NS_IMETHODIMP    
nsEditorShell::PasteAsQuotation(PRInt32 aSelectionType)
{  
  nsresult  err = NS_NOINTERFACE;
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIEditorMailSupport>  mailEditor = do_QueryInterface(mEditor);
        if (mailEditor)
          err = mailEditor->PasteAsQuotation(aSelectionType);
      }
      break;

    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}

NS_IMETHODIMP    
nsEditorShell::PasteAsCitedQuotation(const PRUnichar *cite, PRInt32 aSelectionType)
{  
  nsresult  err = NS_NOINTERFACE;
  
  nsAutoString aCiteString(cite);

  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIEditorMailSupport>  mailEditor = do_QueryInterface(mEditor);
        if (mailEditor)
          err = mailEditor->PasteAsCitedQuotation(aCiteString, aSelectionType);
      }
      break;

    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}

NS_IMETHODIMP    
nsEditorShell::InsertAsQuotation(const PRUnichar *quotedText,
                                 nsIDOMNode** aNodeInserted)
{  
  nsresult  err = NS_NOINTERFACE;
  
  nsAutoString aQuotedText(quotedText);
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIEditorMailSupport>  mailEditor = do_QueryInterface(mEditor);
        if (mailEditor)
          err = mailEditor->InsertAsQuotation(aQuotedText, aNodeInserted);
      }
      break;

    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}

NS_IMETHODIMP    
nsEditorShell::InsertAsCitedQuotation(const PRUnichar *quotedText,
                                      const PRUnichar *cite,
                                      PRBool aInsertHTML,
                                      const PRUnichar *charset,
                                      nsIDOMNode** aNodeInserted)
{  
  nsresult  err = NS_NOINTERFACE;
  
  nsCOMPtr<nsIEditorMailSupport> mailEditor = do_QueryInterface(mEditor);
  if (!mailEditor)
    return NS_NOINTERFACE;

  nsAutoString aQuotedText(quotedText);
  nsAutoString aCiteString(cite);
  nsAutoString aCharset(charset);
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
      err = mailEditor->InsertAsQuotation(aQuotedText, aNodeInserted);
      break;

    case eHTMLTextEditorType:
      err = mailEditor->InsertAsCitedQuotation(aQuotedText, aCiteString,
                                               aInsertHTML,
                                               aCharset, aNodeInserted);
      break;

    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}

// Utility routine to make a new citer.  This addrefs, of course.
static nsICiter* MakeACiter()
{
  // Make a citer of an appropriate type
  nsICiter* citer = 0;
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
  if (NS_FAILED(rv)) return 0;

  char *citationType = 0;
  rv = prefs->CopyCharPref("mail.compose.citationType", &citationType);
                          
  if (NS_SUCCEEDED(rv) && citationType[0])
  {
    if (!strncmp(citationType, "aol", 3))
      citer = new nsAOLCiter;
    else
      citer = new nsInternetCiter;
    PL_strfree(citationType);
  }
  else
    citer = new nsInternetCiter;

  if (citer)
    NS_ADDREF(citer);
  return citer;
}

NS_IMETHODIMP    
nsEditorShell::Rewrap(PRBool aRespectNewlines)
{
  PRInt32 wrapCol;
  nsresult rv = GetWrapColumn(&wrapCol);
  if (NS_FAILED(rv))
    return NS_OK;
#ifdef DEBUG_akkana
  printf("nsEditorShell::Rewrap to %ld columns\n", (long)wrapCol);
#endif

  nsCOMPtr<nsISelection> selection;
  rv = GetEditorSelection(getter_AddRefs(selection));
  if (NS_FAILED(rv)) return rv;

  if (!selection)
    return NS_ERROR_NOT_INITIALIZED;
  PRBool isCollapsed;
  rv = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(rv)) return rv;

  // Variables we'll need either way
  nsAutoString format; format.AssignWithConversion("text/plain");
  nsAutoString current;
  nsString wrapped;
  nsCOMPtr<nsIEditor> nsied (do_QueryInterface(mEditor));
  if (!nsied)
    return NS_ERROR_UNEXPECTED;

  if (isCollapsed)    // rewrap the whole document
  {
    rv = nsied->OutputToString(current, format,
                               nsIDocumentEncoder::OutputFormatted);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsICiter> citer = dont_AddRef(MakeACiter());
    if (NS_FAILED(rv)) return rv;
    if (!citer) return NS_ERROR_UNEXPECTED;

    rv = citer->Rewrap(current, wrapCol, 0, aRespectNewlines, wrapped);
    if (NS_FAILED(rv)) return rv;

    rv = SelectAll();
    if (NS_FAILED(rv)) return rv;

    return mEditor->InsertText(wrapped);
  }
  else                // rewrap only the selection
  {
    rv = nsied->OutputToString(current, format,
                               nsIDocumentEncoder::OutputFormatted
                                | nsIDocumentEncoder::OutputSelectionOnly);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsICiter> citer = dont_AddRef(MakeACiter());
    if (NS_FAILED(rv)) return rv;
    if (!citer) return NS_ERROR_UNEXPECTED;

    PRUint32 firstLineOffset = 0;   // XXX need to get this
    rv = citer->Rewrap(current, wrapCol, firstLineOffset, aRespectNewlines,
                       wrapped);
    if (NS_FAILED(rv)) return rv;

    return mEditor->InsertText(wrapped);
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsEditorShell::StripCites()
{
#ifdef DEBUG_akkana
  printf("nsEditorShell::StripCites()\n");
#endif

  nsCOMPtr<nsISelection> selection;
  nsresult rv = GetEditorSelection(getter_AddRefs(selection));
  if (NS_FAILED(rv)) return rv;

  if (!selection)
    return NS_ERROR_NOT_INITIALIZED;
  PRBool isCollapsed;
  rv = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(rv)) return rv;

  // Variables we'll need either way
  nsAutoString format; format.AssignWithConversion("text/plain");
  nsAutoString current;
  nsString stripped;
  nsCOMPtr<nsIEditor> nsied (do_QueryInterface(mEditor));
  if (!nsied)
    return NS_ERROR_UNEXPECTED;

  if (isCollapsed)    // rewrap the whole document
  {
    rv = nsied->OutputToString(current, format,
                               nsIDocumentEncoder::OutputFormatted);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsICiter> citer = dont_AddRef(MakeACiter());
    if (NS_FAILED(rv)) return rv;
    if (!citer) return NS_ERROR_UNEXPECTED;

    rv = citer->StripCites(current, stripped);
    if (NS_FAILED(rv)) return rv;

    rv = SelectAll();
    if (NS_FAILED(rv)) return rv;

    return mEditor->InsertText(stripped);
  }
  else                // rewrap only the selection
  {
    rv = nsied->OutputToString(current, format,
                               nsIDocumentEncoder::OutputFormatted
                                | nsIDocumentEncoder::OutputSelectionOnly);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsICiter> citer = dont_AddRef(MakeACiter());
    if (NS_FAILED(rv)) return rv;
    if (!citer) return NS_ERROR_UNEXPECTED;

    rv = citer->StripCites(current, stripped);
    if (NS_FAILED(rv)) return rv;

    return mEditor->InsertText(stripped);
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsEditorShell::SelectAll()
{  
  nsresult  err = NS_NOINTERFACE;
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor);
        if (editor)
          err = editor->SelectAll();
      }
      break;

    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}


NS_IMETHODIMP    
nsEditorShell::DeleteSelection(PRInt32 action)
{  
  nsresult  err = NS_NOINTERFACE;
  nsIEditor::EDirection selectionAction;

  switch(action)
  {
    case 1:
      selectionAction = nsIEditor::eNext;
      break;
    case 2:
      selectionAction = nsIEditor::ePrevious;
      break;
    default:
      selectionAction = nsIEditor::eNone;
      break;
  }

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (editor)
    err = editor->DeleteSelection(selectionAction);
  
  return err;
}

/* This routine should only be called when playing back a log */
NS_IMETHODIMP
nsEditorShell::TypedText(const PRUnichar *aTextToInsert, PRInt32 aAction)
{
  nsresult  err = NS_NOINTERFACE;
  
  nsAutoString textToInsert(aTextToInsert);
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIHTMLEditor>  htmlEditor = do_QueryInterface(mEditor);
        if (htmlEditor)
          err = htmlEditor->TypedText(textToInsert, aAction);
      }
      break;

    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}


NS_IMETHODIMP
nsEditorShell::InsertText(const PRUnichar *textToInsert)
{
  nsresult  err = NS_NOINTERFACE;
  
  nsAutoString aTextToInsert(textToInsert);
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIHTMLEditor>  htmlEditor = do_QueryInterface(mEditor);
        if (htmlEditor)
          err = htmlEditor->InsertText(aTextToInsert);
      }
      break;

    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}

NS_IMETHODIMP
nsEditorShell::InsertSource(const PRUnichar *aSourceToInsert)
{
  nsresult  err = NS_NOINTERFACE;
  
  nsAutoString sourceToInsert(aSourceToInsert);
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIHTMLEditor>  htmlEditor = do_QueryInterface(mEditor);
        if (htmlEditor)
          err = htmlEditor->InsertHTML(sourceToInsert);
      }
      break;

    default:
      err = NS_NOINTERFACE;
  }

  return err;
}

NS_IMETHODIMP
nsEditorShell::InsertSourceWithCharset(const PRUnichar *aSourceToInsert,
                                       const PRUnichar *aCharset)
{
  nsresult  err = NS_NOINTERFACE;
  
  nsAutoString sourceToInsert(aSourceToInsert);
  nsAutoString charset(aCharset);
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIHTMLEditor>  htmlEditor = do_QueryInterface(mEditor);
        if (htmlEditor)
          err = htmlEditor->InsertHTMLWithCharset(sourceToInsert, charset);
      }
      break;

    default:
      err = NS_NOINTERFACE;
  }

  return err;
}

NS_IMETHODIMP    
nsEditorShell::RebuildDocumentFromSource(const PRUnichar *aSource)
{
  nsresult  err = NS_NOINTERFACE;
  
  nsAutoString source(aSource);
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIHTMLEditor>  htmlEditor = do_QueryInterface(mEditor);
        if (htmlEditor)
          err = htmlEditor->RebuildDocumentFromSource(source);
      }
      break;

    default:
      err = NS_NOINTERFACE;
  }

  return err;
}

NS_IMETHODIMP
nsEditorShell::InsertBreak()
{
  nsresult  err = NS_NOINTERFACE;
  
  if (mEditor)
    err = mEditor->InsertBreak();
  
  return err;
}

// Both Find and FindNext call through here.
nsresult
nsEditorShell::DoFind(PRBool aFindNext)
{
  if (!mContentAreaDocShell)
    return NS_ERROR_NOT_INITIALIZED;

  PRBool foundIt = PR_FALSE;
  
  // Get find component.
  nsresult rv;
  NS_WITH_SERVICE(nsIFindComponent, findComponent, NS_IFINDCOMPONENT_CONTRACTID, &rv);
  NS_ASSERTION(((NS_SUCCEEDED(rv)) && findComponent), "GetService failed for find component.");
  if (NS_FAILED(rv)) { return rv; }

  // make the search context if we need to
  if (!mSearchContext)
  {
    if(!mContentWindow)
      return NS_ERROR_NOT_INITIALIZED;
    nsCOMPtr<nsIDOMWindowInternal> cwP = do_QueryReferent(mContentWindow);
    if (!cwP) return NS_ERROR_NOT_INITIALIZED;
    rv = findComponent->CreateContext(cwP, nsnull, getter_AddRefs(mSearchContext));
  }
  
  if (NS_SUCCEEDED(rv))
  {
    if (aFindNext)
      rv = findComponent->FindNext(mSearchContext, &foundIt);
    else
      rv = findComponent->Find(mSearchContext, &foundIt);
  }

  return rv;
}

NS_IMETHODIMP
nsEditorShell::Find()
{
  return DoFind(PR_FALSE);
}

NS_IMETHODIMP
nsEditorShell::FindNext()
{
  return DoFind(PR_TRUE);
}

/* Get localized strings for UI from the Editor's string bundle */
// Use this version from JavaScript:
NS_IMETHODIMP
nsEditorShell::GetString(const PRUnichar *name, PRUnichar **_retval)
{
  if (!name || !_retval)
    return NS_ERROR_NULL_POINTER;

  // Don't fail, just return an empty string    
  nsAutoString empty;

  if (mStringBundle)
  {
    if (NS_FAILED(mStringBundle->GetStringFromName(name, _retval)))
      *_retval = empty.ToNewUnicode();

    return NS_OK;
  } else {
    *_retval = empty.ToNewUnicode();
    return NS_ERROR_NOT_INITIALIZED;
  }
}


// Use this version within the shell:
void nsEditorShell::GetBundleString(const nsString& name, nsString &outString)
{
  if (mStringBundle && name.Length() > 0)
  {
    PRUnichar *ptrv = nsnull;
    if (NS_SUCCEEDED(mStringBundle->GetStringFromName(name.GetUnicode(), &ptrv)))
      outString = ptrv;
    else
      outString.SetLength(0);
    
    nsMemory::Free(ptrv);
  }
  else
  {
    outString.SetLength(0);
  }
}

// Utilities to bring up a Yes/No/Cancel dialog.

// For JavaScript:
NS_IMETHODIMP    
nsEditorShell::ConfirmWithTitle(const PRUnichar *aTitle, const PRUnichar *aQuestion,
                                const PRUnichar *aYesButtonText, const PRUnichar *aNoButtonText, PRInt32 *_retval)
{
  if (!aTitle || !aQuestion || !aYesButtonText || !_retval)
    return NS_ERROR_NULL_POINTER;

  nsAutoString title(aTitle);
  nsAutoString question(aQuestion);
  nsAutoString yesString(aYesButtonText);
  nsAutoString noString(aNoButtonText);

  *_retval = ConfirmWithCancel(title, question, &yesString, &noString);
  
  return NS_OK;
}

nsEditorShell::EConfirmResult
nsEditorShell::ConfirmWithCancel(const nsString& aTitle, const nsString& aQuestion, 
                                 const nsString *aYesString, const nsString *aNoString)
{
  nsEditorShell::EConfirmResult result = nsEditorShell::eCancel;
  
  nsIDialogParamBlock* block = NULL; 
  nsresult rv = nsComponentManager::CreateInstance(kDialogParamBlockCID, 0,
                                          NS_GET_IID(nsIDialogParamBlock), 
                                          (void**)&block ); 
  if ( NS_SUCCEEDED(rv) )
  { 
    // Stuff in Parameters 
    block->SetString( nsICommonDialogs::eMsg, aQuestion.GetUnicode()); 
    nsAutoString url; url.AssignWithConversion( "chrome://global/skin/question-icon.gif"  ); 
    block->SetString( nsICommonDialogs::eIconURL, url.GetUnicode()); 

    nsAutoString yesStr, noStr;
    // Default is Yes, No, Cancel
    PRInt32 numberOfButtons = 3;
    if (aYesString)
      yesStr.Assign(*aYesString);
    else
      // We always want a "Yes" string, so supply the default
      GetBundleString(NS_ConvertASCIItoUCS2("Yes"), yesStr);

    if (aNoString && aNoString->Length() > 0)
    {
      noStr.Assign(*aNoString);
      block->SetString( nsICommonDialogs::eButton2Text, noStr.GetUnicode() ); 
    }
    else
    {
      // No string for "No" means we only want Yes, Cancel
      numberOfButtons = 2;
    }    
    block->SetInt( nsICommonDialogs::eNumberButtons, numberOfButtons ); 

    nsAutoString cancelStr;
    GetBundleString(NS_ConvertASCIItoUCS2("Cancel"), cancelStr);

    block->SetString( nsICommonDialogs::eDialogTitle, aTitle.GetUnicode() );
    //Note: "button0" is always Ok or Yes action, "button1" is Cancel
    block->SetString( nsICommonDialogs::eButton0Text, yesStr.GetUnicode() ); 
    block->SetString( nsICommonDialogs::eButton1Text, cancelStr.GetUnicode() ); 

    NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv); 
    if ( NS_SUCCEEDED( rv ) ) 
    { 
      PRInt32 buttonPressed = 0; 
      if(!mContentWindow)
        return result;
      nsCOMPtr<nsIDOMWindowInternal> cwP = do_QueryReferent(mContentWindow);
      if (!cwP) return result;
      rv = dialog->DoDialog( cwP, block, "chrome://global/content/commonDialog.xul" ); 
      block->GetInt( nsICommonDialogs::eButtonPressed, &buttonPressed ); 
      // NOTE: If order of buttons changes in nsICommonDialogs,
      //       then we must change the EConfirmResult enums in nsEditorShell.h
      result = nsEditorShell::EConfirmResult(buttonPressed);
    } 
    NS_IF_RELEASE( block );
  }
  return result;
} 

// Utility to bring up a OK/Cancel dialog.
PRBool    
nsEditorShell::Confirm(const nsString& aTitle, const nsString& aQuestion)
{
  nsresult rv;
  PRBool   result = PR_FALSE;

  NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv); 
  if (NS_SUCCEEDED(rv) && dialog)
  {
    if(!mContentWindow)
      return NS_ERROR_NOT_INITIALIZED;
    nsCOMPtr<nsIDOMWindowInternal> cwP = do_QueryReferent(mContentWindow);
    if (!cwP) return NS_ERROR_NOT_INITIALIZED;
    rv = dialog->Confirm(cwP, aTitle.GetUnicode(), aQuestion.GetUnicode(), &result);
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::AlertWithTitle(const PRUnichar *aTitle, const PRUnichar *aMsg)
{
  if (!aTitle || !aMsg)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_ERROR_FAILURE;
  NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv); 
  if (NS_SUCCEEDED(rv) && dialog)
  {
    if(!mContentWindow)
      return NS_ERROR_NOT_INITIALIZED;
    nsCOMPtr<nsIDOMWindowInternal> cwP = do_QueryReferent(mContentWindow);
    if (!cwP) return NS_ERROR_NOT_INITIALIZED;
    rv = dialog->Alert(cwP, aTitle, aMsg);
  }

  return rv;
}

void
nsEditorShell::Alert(const nsString& aTitle, const nsString& aMsg)
{
  nsresult rv;
  NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv); 
  if (NS_SUCCEEDED(rv) && dialog)
  {
    if(!mContentWindow)
      return;
    nsCOMPtr<nsIDOMWindowInternal> cwP = do_QueryReferent(mContentWindow);
    if (!cwP) return;
    rv = dialog->Alert(cwP, aTitle.GetUnicode(), aMsg.GetUnicode());
  }
}

NS_IMETHODIMP
nsEditorShell::GetDocumentCharacterSet(PRUnichar** characterSet)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);

  if (editor)
    return editor->GetDocumentCharacterSet(characterSet);

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsEditorShell::SetDocumentCharacterSet(const PRUnichar* characterSet)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);

  nsresult res = NS_OK;
  if (editor)
    res = editor->SetDocumentCharacterSet(characterSet);
  
  if(NS_SUCCEEDED(res)) {
    nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryReferent(mContentWindow));
    if (!globalObj) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIDocShell> docShell;
    globalObj->GetDocShell(getter_AddRefs(docShell));
    if (docShell)
    {
      nsCOMPtr<nsIContentViewer> childCV;
      NS_ENSURE_SUCCESS(docShell->GetContentViewer(getter_AddRefs(childCV)), NS_ERROR_FAILURE);
      if (childCV)
      {
        nsCOMPtr<nsIMarkupDocumentViewer> markupCV = do_QueryInterface(childCV);
        if (markupCV) {
          NS_ENSURE_SUCCESS(markupCV->SetDefaultCharacterSet(characterSet), NS_ERROR_FAILURE);
          NS_ENSURE_SUCCESS(markupCV->SetForceCharacterSet(characterSet), NS_ERROR_FAILURE);
        }
      }
    }
  }
  return res;
}

NS_IMETHODIMP
nsEditorShell::GetContentsAs(const PRUnichar *format, PRUint32 flags,
                             PRUnichar **aContentsAs)
{
  nsresult  err = NS_NOINTERFACE;

  nsAutoString aFormat (format);
  nsAutoString contentsAs;

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (editor)
    err = editor->OutputToString(contentsAs, aFormat, flags);

  *aContentsAs = contentsAs.ToNewUnicode();
  
  return err;
}

NS_IMETHODIMP
nsEditorShell::GetHeadContentsAsHTML(PRUnichar **aHeadContents)
{
  nsresult  err = NS_NOINTERFACE;

  nsAutoString headContents;

  nsCOMPtr<nsIHTMLEditor> editor = do_QueryInterface(mEditor);
  if (editor)
    err = editor->GetHeadContentsAsHTML(headContents);

  *aHeadContents = headContents.ToNewUnicode();
  
  return err;
}

NS_IMETHODIMP
nsEditorShell::ReplaceHeadContentsWithHTML(const PRUnichar *aSourceToInsert)
{
  nsresult  err = NS_NOINTERFACE;
  
  nsAutoString sourceToInsert(aSourceToInsert);
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIHTMLEditor>  htmlEditor = do_QueryInterface(mEditor);
        if (htmlEditor)
          err = htmlEditor->ReplaceHeadContentsWithHTML(sourceToInsert);
      }
      break;

    default:
      err = NS_NOINTERFACE;
  }

  return err;
}

NS_IMETHODIMP
nsEditorShell::DumpContentTree()
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (!editor)
    return NS_ERROR_NOT_INITIALIZED;
  return editor->DumpContentTree();
}

NS_IMETHODIMP
nsEditorShell::GetWrapColumn(PRInt32* aWrapColumn)
{
  nsresult  err = NS_NOINTERFACE;
  
  if (!aWrapColumn)
    return NS_ERROR_NULL_POINTER;
  
  // fill result in case of failure
  *aWrapColumn = mWrapColumn;

  // If we don't have an editor yet, say we're not initialized
  // even though mWrapColumn may have a value.
  if (!mEditor)
    return NS_ERROR_NOT_INITIALIZED;

  switch (mEditorType)
  {
    case ePlainTextEditorType:
      {
        nsCOMPtr<nsIEditorMailSupport>  mailEditor = do_QueryInterface(mEditor);
        if (mailEditor)
        {
          PRInt32 wc;
          err = mailEditor->GetBodyWrapWidth(&wc);
          if (NS_SUCCEEDED(err))
            *aWrapColumn = (PRInt32)wc;
        }
      }
      break;
    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}

NS_IMETHODIMP
nsEditorShell::SetWrapColumn(PRInt32 aWrapColumn)
{
  nsresult  err = NS_OK;

  mWrapColumn = aWrapColumn;

  if (mEditor)
  {
    switch (mEditorType)
    {
        case ePlainTextEditorType:
        {
          nsCOMPtr<nsIEditorMailSupport>  mailEditor = do_QueryInterface(mEditor);
          if (mailEditor)
            err = mailEditor->SetBodyWrapWidth(mWrapColumn);
        }
        break;
        default:
          err = NS_ERROR_NOT_IMPLEMENTED;
    }
  }

  return err;
}

NS_IMETHODIMP
nsEditorShell::SetParagraphFormat(const PRUnichar * paragraphFormat)
{
  nsresult  err = NS_NOINTERFACE;
  
  nsAutoString aParagraphFormat(paragraphFormat);
  
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      err = mEditor->SetParagraphFormat(aParagraphFormat);
      break;

    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}

NS_IMETHODIMP
nsEditorShell::GetEditorDocument(nsIDOMDocument** aEditorDocument)
{
  if (mEditor)
  {
    nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor);
    if (editor)
    {
      return editor->GetDocument(aEditorDocument);
    }
  }
  return NS_NOINTERFACE;
}

 
NS_IMETHODIMP
nsEditorShell::GetEditor(nsIEditor** aEditor)
{
  if (mEditor)
    return mEditor->QueryInterface(NS_GET_IID(nsIEditor), (void **)aEditor);		// the QI does the addref

  *aEditor = nsnull;
  return NS_ERROR_NOT_INITIALIZED;
}


NS_IMETHODIMP
nsEditorShell::GetEditorSelection(nsISelection** aEditorSelection)
{
  nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor);
  if (editor)
      return editor->GetSelection(aEditorSelection);
  
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsEditorShell::GetSelectionController(nsISelectionController** aSelectionController)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (!editor)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsISelectionController> selCont;
  nsresult rv = editor->GetSelectionController(getter_AddRefs(selCont));
  if (NS_FAILED(rv))
    return rv;

  if (!selCont)
    return NS_ERROR_NO_INTERFACE;
  *aSelectionController = selCont;
  NS_IF_ADDREF(*aSelectionController);
  return NS_OK;
}

NS_IMETHODIMP
nsEditorShell::GetDocumentModified(PRBool *aDocumentModified)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (editor)
    return editor->GetDocumentModified(aDocumentModified);

  return NS_NOINTERFACE;
}


NS_IMETHODIMP
nsEditorShell::GetDocumentIsEmpty(PRBool *aDocumentIsEmpty)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (editor)
    return editor->GetDocumentIsEmpty(aDocumentIsEmpty);

  return NS_NOINTERFACE;
}


NS_IMETHODIMP
nsEditorShell::GetDocumentEditable(PRBool *aDocumentEditable)
{
  *aDocumentEditable = PR_FALSE;  // default return value
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (!editor) return NS_OK;
  
  PRUint32  editorFlags;
  editor->GetFlags(&editorFlags);
  
  if (editorFlags & nsIHTMLEditor::eEditorReadonlyMask)
    return NS_OK;
  
  nsCOMPtr<nsIDOMDocument> doc;
  editor->GetDocument(getter_AddRefs(doc));
  if (!doc) return NS_OK;

  *aDocumentEditable = PR_TRUE;
  return NS_OK;
}


NS_IMETHODIMP
nsEditorShell::GetDocumentLength(PRInt32 *aDocumentLength)
{
  nsCOMPtr<nsIHTMLEditor> editor = do_QueryInterface(mEditor);
  if (editor)
    return editor->GetDocumentLength(aDocumentLength);

  return NS_NOINTERFACE;
}


NS_IMETHODIMP
nsEditorShell::MakeOrChangeList(const PRUnichar *listType, PRBool entireList)
{
  nsresult err = NS_NOINTERFACE;

  nsAutoString aListType(listType);
  
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      if (aListType.IsEmpty())
      {
        err = mEditor->RemoveList(NS_ConvertASCIItoUCS2("ol"));
        if(NS_SUCCEEDED(err))
        {
          err = mEditor->RemoveList(NS_ConvertASCIItoUCS2("ul"));
          if(NS_SUCCEEDED(err))
            err = mEditor->RemoveList(NS_ConvertASCIItoUCS2("dl"));
        }
      }
      else
        err = mEditor->MakeOrChangeList(aListType, entireList);
      break;

    case ePlainTextEditorType:
    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }
  return err;
}


NS_IMETHODIMP
nsEditorShell::RemoveList(const PRUnichar *listType)
{
  nsresult err = NS_NOINTERFACE;

  
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
    {
      nsAutoString aListType(listType);
      err = mEditor->RemoveList(aListType);
      break;
    }

    case ePlainTextEditorType:
    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}

NS_IMETHODIMP
nsEditorShell::Indent(const PRUnichar *indent)
{
  nsresult err = NS_NOINTERFACE;

  nsAutoString aIndent(indent);
  
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      err = mEditor->Indent(aIndent);
      break;

    case ePlainTextEditorType:
    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}

NS_IMETHODIMP
nsEditorShell::Align(const PRUnichar *align)
{
  nsresult err = NS_NOINTERFACE;

  nsAutoString aAlignType(align);
  
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      err = mEditor->Align(aAlignType);
      break;

    case ePlainTextEditorType:
    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}

NS_IMETHODIMP
nsEditorShell::GetSelectedElement(const PRUnichar *aInTagName, nsIDOMElement **aOutElement)
{
  if (!aInTagName || !aOutElement)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  nsAutoString tagName(aInTagName);
  
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      result = mEditor->GetSelectedElement(tagName, aOutElement);
      // Don't return NS_EDITOR_ELEMENT_NOT_FOUND (passes NS_SUCCEEDED macro)
      //  to JavaScript
      if(NS_SUCCEEDED(result)) return NS_OK;
      break;

    case ePlainTextEditorType:
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }

  return result;
}

NS_IMETHODIMP
nsEditorShell::GetFirstSelectedCell(nsIDOMElement **aOutElement)
{
  if (!aOutElement)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
    {
      nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
      if (tableEditor)
      {
        result = tableEditor->GetFirstSelectedCell(aOutElement, nsnull);
        // Don't return NS_EDITOR_ELEMENT_NOT_FOUND (passes NS_SUCCEEDED macro)
        //  to JavaScript
        if(NS_SUCCEEDED(result)) return NS_OK;
      }
      
      break;
    }

    case ePlainTextEditorType:
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }

  return result;
}

NS_IMETHODIMP 
nsEditorShell::GetFirstSelectedCellInTable(PRInt32 *aRowIndex, PRInt32 *aColIndex, nsIDOMElement **aOutElement)
{
  if (!aOutElement || !aRowIndex || !aColIndex)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
    {
      nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
      if (tableEditor)
      {
        result = tableEditor->GetFirstSelectedCellInTable(aOutElement, aRowIndex, aColIndex);
        // Don't return NS_EDITOR_ELEMENT_NOT_FOUND (passes NS_SUCCEEDED macro)
        //  to JavaScript
        if(NS_SUCCEEDED(result)) return NS_OK;
      }
      
      break;
    }

    case ePlainTextEditorType:
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }

  return result;
}

NS_IMETHODIMP
nsEditorShell::GetNextSelectedCell(nsIDOMElement **aOutElement)
{
  if (!aOutElement)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
    {
      nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
      if (tableEditor)
        result = tableEditor->GetNextSelectedCell(aOutElement, nsnull);
      break;
    }
    case ePlainTextEditorType:
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }

  return result;
}


NS_IMETHODIMP
nsEditorShell::GetElementOrParentByTagName(const PRUnichar *aInTagName, nsIDOMNode *node, nsIDOMElement **aOutElement)
{
  //node can be null -- this signals using the selection anchorNode
  if (!aInTagName || !aOutElement)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  nsAutoString tagName(aInTagName);
  
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      result = mEditor->GetElementOrParentByTagName(tagName, node, aOutElement);
      // Don't return NS_EDITOR_ELEMENT_NOT_FOUND (passes NS_SUCCEEDED macro)
      //  to JavaScript
      if(NS_SUCCEEDED(result)) return NS_OK;
      break;

    case ePlainTextEditorType:
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }

  return result;
}

NS_IMETHODIMP
nsEditorShell::CreateElementWithDefaults(const PRUnichar *aInTagName, nsIDOMElement **aOutElement)
{
  if (!aOutElement)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  nsAutoString tagName(aInTagName);

  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      result = mEditor->CreateElementWithDefaults(tagName, aOutElement);
      break;

    case ePlainTextEditorType:
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }

  return result;
}

NS_IMETHODIMP
nsEditorShell::DeleteElement(nsIDOMElement *element)
{
  if (!element)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (editor) {
    // The nsIEditor::DeleteNode() wants a node
    //   but it actually requires that it is an element!
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(element);
    result = editor->DeleteNode(node);
  }
  return result;
}

NS_IMETHODIMP
nsEditorShell::InsertElement(nsIDOMElement *element, nsIDOMElement *parent, PRInt32 position)
{
  if (!element || !parent)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (editor) {
    // The nsIEditor::InsertNode() wants nodes as params,
    //   but it actually requires that they are elements!
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(element);
    nsCOMPtr<nsIDOMNode> parentNode = do_QueryInterface(parent);
    result = editor->InsertNode(node, parentNode, position);
  }
  return result;
}

NS_IMETHODIMP
nsEditorShell::InsertElementAtSelection(nsIDOMElement *element, PRBool deleteSelection)
{
  if (!element)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      result = mEditor->InsertElementAtSelection(element, deleteSelection);
      break;

    case ePlainTextEditorType:
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }

  return result;
}

NS_IMETHODIMP
nsEditorShell::InsertLinkAroundSelection(nsIDOMElement* aAnchorElement)
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      result = mEditor->InsertLinkAroundSelection(aAnchorElement);
      break;

    case ePlainTextEditorType:
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::SelectElement(nsIDOMElement* aElement)
{
  if (!aElement)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      result = mEditor->SelectElement(aElement);
      break;

    case ePlainTextEditorType:
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }

  return result;
}

NS_IMETHODIMP    
nsEditorShell::SetSelectionAfterElement(nsIDOMElement* aElement)
{
  if (!aElement)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      result = mEditor->SetCaretAfterElement(aElement);
      break;

    case ePlainTextEditorType:
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }

  return result;
}

/* Table Editing */
NS_IMETHODIMP    
nsEditorShell::InsertTableRow(PRInt32 aNumber, PRBool bAfter)
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
          result = tableEditor->InsertTableRow(aNumber,bAfter);
      }
      break;

    case ePlainTextEditorType:
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::InsertTableColumn(PRInt32 aNumber, PRBool bAfter)
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
          result = tableEditor->InsertTableColumn(aNumber,bAfter);
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::InsertTableCell(PRInt32 aNumber, PRBool bAfter)
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
        {
          BeginBatchChanges();
          result = tableEditor->InsertTableCell(aNumber, bAfter);
          if (NS_SUCCEEDED(result))
          {
            // Fix disturbances in table layout because of inserted cells
            result = CheckPrefAndNormalizeTable();
          }
          EndBatchChanges();
          return result;
        }
      }
      break;

    case ePlainTextEditorType:
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::DeleteTable()
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
        {
          result = tableEditor->DeleteTable();
          // Don't return NS_EDITOR_ELEMENT_NOT_FOUND (passes NS_SUCCEEDED macro)
          //  to JavaScript
          if(NS_SUCCEEDED(result)) return NS_OK;
        }
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::DeleteTableCell(PRInt32 aNumber)
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
        {
          BeginBatchChanges();
          result = tableEditor->DeleteTableCell(aNumber);
          if(NS_SUCCEEDED(result))
          {
            // Fix disturbances in table layout because of deleted cells
            result = CheckPrefAndNormalizeTable();
          }
          EndBatchChanges();
          return result;
        }
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::DeleteTableCellContents()
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
        {
          result = tableEditor->DeleteTableCellContents();
          // Don't return NS_EDITOR_ELEMENT_NOT_FOUND (passes NS_SUCCEEDED macro)
          //  to JavaScript
          if(NS_SUCCEEDED(result)) return NS_OK;
        }
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::DeleteTableRow(PRInt32 aNumber)
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
        {
          result = tableEditor->DeleteTableRow(aNumber);
          // Don't return NS_EDITOR_ELEMENT_NOT_FOUND (passes NS_SUCCEEDED macro)
          //  to JavaScript
          if(NS_SUCCEEDED(result)) return NS_OK;
        }
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}


NS_IMETHODIMP    
nsEditorShell::DeleteTableColumn(PRInt32 aNumber)
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
        {
          result = tableEditor->DeleteTableColumn(aNumber);
          // Don't return NS_EDITOR_ELEMENT_NOT_FOUND (passes NS_SUCCEEDED macro)
          //  to JavaScript
          if(NS_SUCCEEDED(result)) return NS_OK;
        }
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP 
nsEditorShell::SwitchTableCellHeaderType(nsIDOMElement *aSourceCell, nsIDOMElement **aNewCell)
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
          result = tableEditor->SwitchTableCellHeaderType(aSourceCell, aNewCell);
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}


NS_IMETHODIMP    
nsEditorShell::JoinTableCells(PRBool aMergeNonContiguousContents)
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
          result = tableEditor->JoinTableCells(aMergeNonContiguousContents);
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::SplitTableCell()
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
          result = tableEditor->SplitTableCell();
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::SelectTableCell()
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
          result = tableEditor->SelectTableCell();
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::SelectBlockOfCells(nsIDOMElement *aStartCell, nsIDOMElement *aEndCell)
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
          result = tableEditor->SelectBlockOfCells(aStartCell, aEndCell);
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}


NS_IMETHODIMP    
nsEditorShell::SelectTableRow()
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
          result = tableEditor->SelectTableRow();
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::SelectTableColumn()
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
          result = tableEditor->SelectTableColumn();
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::SelectTable()
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
          result = tableEditor->SelectTable();
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::SelectAllTableCells()
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
          result = tableEditor->SelectAllTableCells();
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP 
nsEditorShell::NormalizeTable(nsIDOMElement *aTable)
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
          result = tableEditor->NormalizeTable(aTable);
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

// The next four methods are factored to return single items 
//  separately for row and column. 
//  Underlying implementation gets both at the same time for efficiency.

NS_IMETHODIMP    
nsEditorShell::GetRowIndex(nsIDOMElement *cellElement, PRInt32 *_retval)
{
  if (!_retval)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
        {
          // Get both row and column indexes - return just row
          PRInt32 colIndex;
          result = tableEditor->GetCellIndexes(cellElement, *_retval, colIndex);
        }
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::GetColumnIndex(nsIDOMElement *cellElement, PRInt32 *_retval)
{
  if (!_retval)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
        {
          // Get both row and column indexes - return just column
          PRInt32 rowIndex;
          result = tableEditor->GetCellIndexes(cellElement, rowIndex, *_retval);
        }
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }

  return result;
}

NS_IMETHODIMP    
nsEditorShell::GetTableRowCount(nsIDOMElement *tableElement, PRInt32 *_retval)
{
  if (!_retval)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
        {
          // This returns both the number of rows and columns: return just rows
          PRInt32 cols;
          result = tableEditor->GetTableSize(tableElement, *_retval, cols);
        }
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }

  return result;
}

NS_IMETHODIMP    
nsEditorShell::GetTableColumnCount(nsIDOMElement *tableElement, PRInt32 *_retval)
{
  if (!_retval)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
        {
          // This returns both the number of rows and columns: return just columns
          PRInt32 rows;
          result = tableEditor->GetTableSize(tableElement, rows, *_retval);
        }
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }

  return result;
}

NS_IMETHODIMP    
nsEditorShell::GetCellAt(nsIDOMElement *tableElement, PRInt32 rowIndex, PRInt32 colIndex, nsIDOMElement **_retval)
{
  if (!_retval)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
        {
          result = tableEditor->GetCellAt(tableElement, rowIndex, colIndex, *_retval);
          // Don't return NS_EDITOR_ELEMENT_NOT_FOUND (passes NS_SUCCEEDED macro)
          //  to JavaScript
          if(NS_SUCCEEDED(result)) return NS_OK;
        }
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }

  return result;
}

// Note that the return param in the IDL must be the LAST out param here,
//   so order of params is different from nsITableEditor
NS_IMETHODIMP    
nsEditorShell::GetCellDataAt(nsIDOMElement *tableElement, PRInt32 rowIndex, PRInt32 colIndex,
                             PRInt32 *aStartRowIndex, PRInt32 *aStartColIndex, 
                             PRInt32 *aRowSpan, PRInt32 *aColSpan, 
                             PRInt32 *aActualRowSpan, PRInt32 *aActualColSpan, 
                             PRBool *aIsSelected, nsIDOMElement **_retval)
{
  if (!_retval || 
      !aStartRowIndex || !aStartColIndex || 
      !aRowSpan || !aColSpan || 
      !aActualRowSpan || !aActualColSpan ||
      !aIsSelected )
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
    {
      nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
      if (tableEditor)
        result = tableEditor->GetCellDataAt(tableElement, rowIndex, colIndex, *_retval,
                                            *aStartRowIndex, *aStartColIndex, 
                                            *aRowSpan, *aColSpan, 
                                            *aActualRowSpan, *aActualColSpan,
                                            *aIsSelected);
      // Don't return NS_EDITOR_ELEMENT_NOT_FOUND (passes NS_SUCCEEDED macro)
      //  to JavaScript
      if(NS_SUCCEEDED(result)) return NS_OK;
    }
    break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }

  return result;
}

NS_IMETHODIMP
nsEditorShell::GetFirstRow(nsIDOMElement *aTableElement, nsIDOMElement **_retval)
{
  if (!_retval || !aTableElement)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
          result = tableEditor->GetFirstRow(aTableElement, *_retval);
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP
nsEditorShell::GetNextRow(nsIDOMElement *aCurrentRow, nsIDOMElement **_retval)
{
  if (!_retval || !*_retval || !aCurrentRow)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
          result = tableEditor->GetNextRow(aCurrentRow, *_retval);
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP
nsEditorShell::GetSelectedOrParentTableElement(PRUnichar **aTagName, PRInt32 *aSelectedCount, nsIDOMElement **_retval)
{
  if (!_retval || !aTagName || !aSelectedCount)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        nsAutoString TagName(*aTagName);
        if (tableEditor)
          result = tableEditor->GetSelectedOrParentTableElement(*_retval, TagName, *aSelectedCount);
          *aTagName = TagName.ToNewUnicode();
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP
nsEditorShell::GetSelectedCellsType(nsIDOMElement *aElement, PRUint32 *_retval)
{
  if (!_retval)
    return NS_ERROR_NULL_POINTER;

  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
          result = tableEditor->GetSelectedCellsType(aElement, *_retval);
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

/* end of table editing */

NS_IMETHODIMP
nsEditorShell::GetEmbeddedObjects(nsISupportsArray **aObjectArray)
{
  if (!aObjectArray)
    return NS_ERROR_NULL_POINTER;

  nsresult result = NS_NOINTERFACE;

  switch (mEditorType)
  {
    case eHTMLTextEditorType:
    {
      nsCOMPtr<nsIEditorMailSupport> mailEditor = do_QueryInterface(mEditor);
      if (mailEditor)
        result = mailEditor->GetEmbeddedObjects(aObjectArray);
    }
    break;

    default:
      result = NS_NOINTERFACE;
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::InitSpellChecker()
{
  nsresult  result = NS_NOINTERFACE;

   // We can spell check with any editor type
  if (mEditor)
  {
    nsCOMPtr<nsITextServicesDocument>tsDoc;

    result = nsComponentManager::CreateInstance(
                                 kCTextServicesDocumentCID,
                                 nsnull,
                                 NS_GET_IID(nsITextServicesDocument),
                                 (void **)getter_AddRefs(tsDoc));

    if (NS_FAILED(result))
      return result;

    if (!tsDoc)
      return NS_ERROR_NULL_POINTER;

    // Pass the editor to the text services document
    nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor);
    if (!editor)
      return NS_NOINTERFACE;

    result = tsDoc->InitWithEditor(editor);

    if (NS_FAILED(result))
      return result;

    result = nsComponentManager::CreateInstance(NS_SPELLCHECKER_CONTRACTID,
                                                nsnull,
                                                NS_GET_IID(nsISpellChecker),
                                                (void **)getter_AddRefs(mSpellChecker));

    if (NS_FAILED(result))
      return result;

    if (!mSpellChecker)
      return NS_ERROR_NULL_POINTER;

    result = mSpellChecker->SetDocument(tsDoc, PR_FALSE);

    if (NS_FAILED(result))
      return result;

    DeleteSuggestedWordList();
  }

  return result;
}

NS_IMETHODIMP    
nsEditorShell::GetNextMisspelledWord(PRUnichar **aNextMisspelledWord)
{
  nsresult  result = NS_NOINTERFACE;
  nsAutoString nextMisspelledWord;
  
   // We can spell check with any editor type
  if (mEditor && mSpellChecker)
  {
    DeleteSuggestedWordList();
    result = mSpellChecker->NextMisspelledWord(&nextMisspelledWord, &mSuggestedWordList);
  }
  *aNextMisspelledWord = nextMisspelledWord.ToNewUnicode();
  return result;
}

NS_IMETHODIMP    
nsEditorShell::GetSuggestedWord(PRUnichar **aSuggestedWord)
{
  nsresult  result = NS_NOINTERFACE;
  nsAutoString word;
   // We can spell check with any editor type
  if (mEditor)
  {
    if ( mSuggestedWordIndex < mSuggestedWordList.Count())
    {
      mSuggestedWordList.StringAt(mSuggestedWordIndex, word);
      mSuggestedWordIndex++;
    } else {
      // A blank string signals that there are no more strings
      word.SetLength(0);
    }
    result = NS_OK;
  }
  *aSuggestedWord = word.ToNewUnicode();
  return result;
}

NS_IMETHODIMP    
nsEditorShell::CheckCurrentWord(const PRUnichar *aSuggestedWord, PRBool *aIsMisspelled)
{
  nsresult  result = NS_NOINTERFACE;
  nsAutoString suggestedWord(aSuggestedWord);
   // We can spell check with any editor type
  if (mEditor && mSpellChecker)
  {
    DeleteSuggestedWordList();
    result = mSpellChecker->CheckWord(&suggestedWord, aIsMisspelled, &mSuggestedWordList);
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::ReplaceWord(const PRUnichar *aMisspelledWord, const PRUnichar *aReplaceWord, PRBool allOccurrences)
{
  nsresult  result = NS_NOINTERFACE;
  nsAutoString misspelledWord(aMisspelledWord);
  nsAutoString replaceWord(aReplaceWord);
  if (mEditor && mSpellChecker)
  {
    result = mSpellChecker->Replace(&misspelledWord, &replaceWord, allOccurrences);
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::IgnoreWordAllOccurrences(const PRUnichar *aWord)
{
  nsresult  result = NS_NOINTERFACE;
  nsAutoString word(aWord);
  if (mEditor && mSpellChecker)
  {
    result = mSpellChecker->IgnoreAll(&word);
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::GetPersonalDictionary()
{
  nsresult  result = NS_NOINTERFACE;
   // We can spell check with any editor type
  if (mEditor && mSpellChecker)
  {
    mDictionaryList.Clear();
    mDictionaryIndex = 0;
    result = mSpellChecker->GetPersonalDictionary(&mDictionaryList);
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::GetPersonalDictionaryWord(PRUnichar **aDictionaryWord)
{
  nsresult  result = NS_NOINTERFACE;
  nsAutoString word;
  if (mEditor)
  {
    if ( mDictionaryIndex < mDictionaryList.Count())
    {
      mDictionaryList.StringAt(mDictionaryIndex, word);
      mDictionaryIndex++;
    } else {
      // A blank string signals that there are no more strings
      word.SetLength(0);
    }
    result = NS_OK;
  }
  *aDictionaryWord = word.ToNewUnicode();
  return result;
}

NS_IMETHODIMP    
nsEditorShell::AddWordToDictionary(const PRUnichar *aWord)
{
  nsresult  result = NS_NOINTERFACE;
  nsAutoString word(aWord);
  if (mEditor && mSpellChecker)
  {
    result = mSpellChecker->AddWordToPersonalDictionary(&word);
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::RemoveWordFromDictionary(const PRUnichar *aWord)
{
  nsresult  result = NS_NOINTERFACE;
  nsAutoString word(aWord);
  if (mEditor && mSpellChecker)
  {
    result = mSpellChecker->RemoveWordFromPersonalDictionary(&word);
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::GetDictionaryList(PRUnichar ***aDictionaryList, PRUint32 *aCount)
{
  nsresult  result = NS_ERROR_NOT_IMPLEMENTED;

  if (!aDictionaryList || !aCount)
    return NS_ERROR_NULL_POINTER;

  *aDictionaryList = 0;
  *aCount          = 0;

  if (mEditor && mSpellChecker)
  {
    nsStringArray dictList;

    result = mSpellChecker->GetDictionaryList(&dictList);

    if (NS_FAILED(result))
      return result;

    PRUnichar **tmpPtr = 0;

    if (dictList.Count() < 1)
    {
      // If there are no dictionaries, return an array containing
      // one element and a count of one.

      tmpPtr = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *));

      if (!tmpPtr)
        return NS_ERROR_OUT_OF_MEMORY;

      *tmpPtr          = 0;
      *aDictionaryList = tmpPtr;
      *aCount          = 0;

      return NS_OK;
    }

    tmpPtr = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * dictList.Count());

    if (!tmpPtr)
      return NS_ERROR_OUT_OF_MEMORY;

    *aDictionaryList = tmpPtr;
    *aCount          = dictList.Count();

    nsAutoString dictStr;

    PRUint32 i;

    for (i = 0; i < *aCount; i++)
    {
      dictList.StringAt(i, dictStr);
      tmpPtr[i] = dictStr.ToNewUnicode();
    }
  }

  return result;
}

NS_IMETHODIMP    
nsEditorShell::GetCurrentDictionary(PRUnichar **aDictionary)
{
  nsresult  result = NS_ERROR_NOT_INITIALIZED;

  if (!aDictionary)
    return NS_ERROR_NULL_POINTER;

  *aDictionary = 0;

  if (mEditor && mSpellChecker)
  {
    nsAutoString dictStr;
    result = mSpellChecker->GetCurrentDictionary(&dictStr);

    if (NS_FAILED(result))
      return result;

    *aDictionary = dictStr.ToNewUnicode();
  }

  return result;
}

NS_IMETHODIMP    
nsEditorShell::SetCurrentDictionary(const PRUnichar *aDictionary)
{
  nsresult  result = NS_ERROR_NOT_INITIALIZED;

  if (!aDictionary)
    return NS_ERROR_NULL_POINTER;

  if (mEditor && mSpellChecker)
  {
    nsAutoString dictStr(aDictionary);
    result = mSpellChecker->SetCurrentDictionary(&dictStr);
  }

  return result;
}

NS_IMETHODIMP    
nsEditorShell::UninitSpellChecker()
{
  nsresult  result = NS_NOINTERFACE;
   // We can spell check with any editor type
  if (mEditor)
  {
    // Cleanup - kill the spell checker
    DeleteSuggestedWordList();
    mDictionaryList.Clear();
    mDictionaryIndex = 0;
    mSpellChecker = 0;
    result = NS_OK;
  }
  return result;
}

nsresult    
nsEditorShell::DeleteSuggestedWordList()
{
  mSuggestedWordList.Clear();
  mSuggestedWordIndex = 0;
  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsEditorShell::BeginBatchChanges()
{
  nsresult  err = NS_NOINTERFACE;
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor);
        if (editor)
          err = editor->BeginTransaction();
      }
      break;

    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}

NS_IMETHODIMP
nsEditorShell::EndBatchChanges()
{
  nsresult  err = NS_NOINTERFACE;
  
  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor);
        if (editor)
          err = editor->EndTransaction();
      }
      break;

    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}

NS_IMETHODIMP
nsEditorShell::RunUnitTests()
{
  PRInt32  numTests = 0;
  PRInt32  numTestsFailed = 0;
  
  nsresult err = NS_OK;
  nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor);
  if (editor)
    err = editor->DebugUnitTests(&numTests, &numTestsFailed);

#ifdef APP_DEBUG
  printf("\nRan %ld tests, of which %ld failed\n", (long)numTests, (long)numTestsFailed);
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsEditorShell::StartLogging(nsIFileSpec *logFile)
{
  nsresult  err = NS_OK;

  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIEditorLogging>  logger = do_QueryInterface(mEditor);
        if (logger)
          err = logger->StartLogging(logFile);
      }
      break;

    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}

NS_IMETHODIMP
nsEditorShell::StopLogging()
{
  nsresult  err = NS_OK;

  switch (mEditorType)
  {
    case ePlainTextEditorType:
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsIEditorLogging>  logger = do_QueryInterface(mEditor);
        if (logger)
          err = logger->StopLogging();
      }
      break;

    default:
      err = NS_ERROR_NOT_IMPLEMENTED;
  }

  return err;
}

#ifdef XP_MAC
#pragma mark -
#endif

// nsIDocumentLoaderObserver methods
NS_IMETHODIMP
nsEditorShell::OnStartDocumentLoad(nsIDocumentLoader* loader, nsIURI* aURL, const char* aCommand)
{
  // Start the throbber
  // TODO: We should also start/stop it for saving and publishing?
  SetChromeAttribute( mDocShell, "Editor:Throbber", "busy", NS_ConvertASCIItoUCS2("true") );

  // Disable JavaScript in this document:
  nsCOMPtr<nsIScriptGlobalObjectOwner> sgoo (do_QueryInterface(mContentAreaDocShell));
  if (sgoo)
  {
    nsCOMPtr<nsIScriptGlobalObject> sgo;
    sgoo->GetScriptGlobalObject(getter_AddRefs(sgo));
    if (sgo)
    {
      nsCOMPtr<nsIScriptContext> scriptContext;
      sgo->GetContext(getter_AddRefs(scriptContext));
      if (scriptContext)
        scriptContext->SetScriptsEnabled(PR_FALSE);
    }
  }
  
  // set up a parser observer
  if (!mParserObserver)
  {
    mParserObserver = new nsEditorParserObserver();
    if (!mParserObserver) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mParserObserver);
    mParserObserver->RegisterTagToWatch("FRAMESET");
    mParserObserver->Start();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditorShell::OnEndDocumentLoad(nsIDocumentLoader* aLoader, nsIChannel* aChannel, nsresult aStatus)
{
  // for pages with charsets, this gets called the first time with a 
  // non-zero status value. Don't prepare the editor that time.
  // aStatus will be NS_BINDING_ABORTED then.
	if (aStatus == NS_BINDING_ABORTED)
	  return NS_OK;
	
	// note that we continue with other non-success status codes.
	
  // Disable meta-refresh
  nsCOMPtr<nsIRefreshURI> refreshURI = do_QueryInterface(mContentAreaDocShell);
  if (refreshURI)
    refreshURI->CancelRefreshURITimers();

  // can we handle this document?
  if (mParserObserver)
  {
    mParserObserver->End();     // how do we know if this is the last call?
    PRBool cancelEdit;
    mParserObserver->GetBadTagFound(&cancelEdit);
    if (cancelEdit)
    {
      NS_RELEASE(mParserObserver);
      if (mDocShell)
      {
        // where do we pop up a dialog telling the user they can't edit this doc?
        // this next call will close the window, but do we want to do that?  or tell the .js UI to do it?
        mCloseWindowWhenLoaded = PR_TRUE;
        mCantEditReason = eCantEditFramesets;
      }
    }
  }
  
  // Is this a MIME type we can handle?
  if (aChannel)
  {
    char  *contentType;
    aChannel->GetContentType(&contentType);
    if (contentType)
    {
      if ( (strcmp(contentType, "text/html") != 0) &&
           (strcmp(contentType, "text/plain") != 0))
      {
        mCloseWindowWhenLoaded = PR_TRUE;
        mCantEditReason = eCantEditMimeType;
      }
  
      nsMemory::Free(contentType);
    }
  }
  
  PRBool isRootDoc;
  nsresult res = DocumentIsRootDoc(aLoader, isRootDoc);
  if (NS_FAILED(res)) return res;
  
  if (mCloseWindowWhenLoaded && isRootDoc)
  {
    nsAutoString alertLabel, alertMessage;
    GetBundleString(NS_ConvertASCIItoUCS2("Alert"), alertLabel);
    
    nsAutoString  stringID;
    switch (mCantEditReason)
    {
      case eCantEditFramesets:
        stringID.AssignWithConversion("CantEditFramesetMsg");
        break;        
      case eCantEditMimeType:
        stringID.AssignWithConversion("CantEditMimeTypeMsg");
        break;
      case eCantEditOther:
        stringID.AssignWithConversion("CantEditDocumentMsg");
        break;
    }
    
    GetBundleString(stringID, alertMessage);
    Alert(alertLabel, alertMessage);

    nsCOMPtr<nsIBaseWindow> baseWindow;
    GetTreeOwner(mDocShell, getter_AddRefs(baseWindow));
    NS_ENSURE_TRUE(baseWindow, NS_ERROR_ABORT);
    baseWindow->Destroy();

    return NS_ERROR_ABORT;
  }

  // if we're loading a frameset document (which we can't edit yet), don't make an editor.
  // Only make an editor for child documents.
  PRBool docHasFrames;
  if (NS_SUCCEEDED(DocumentContainsFrames(aLoader, docHasFrames)) && !docHasFrames)
  {
    nsCOMPtr<nsIURI>  aUrl;
    aChannel->GetURI(getter_AddRefs(aUrl));
    res = PrepareDocumentForEditing(aLoader, aUrl);
    SetChromeAttribute( mDocShell, "Editor:Throbber", "busy", NS_ConvertASCIItoUCS2("false") );
  }

  nsAutoString doneText;
  GetBundleString(NS_ConvertASCIItoUCS2("LoadingDone"), doneText);
  SetChromeAttribute(mDocShell, "statusText", "value", doneText);

  return res;
}

NS_IMETHODIMP
nsEditorShell::OnStartURLLoad(nsIDocumentLoader* loader,
                              nsIChannel* channel)
{
   return NS_OK;
}

NS_IMETHODIMP
nsEditorShell::OnProgressURLLoad(nsIDocumentLoader* aLoader,
                                    nsIChannel* channel, PRUint32 aProgress, 
                                    PRUint32 aProgressMax)
{
  if (mParserObserver)
  {
    PRBool cancelEdit;
    mParserObserver->GetBadTagFound(&cancelEdit);
    if (cancelEdit)
    {
      /*
      nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(mWebShell);
      if (docShell)
        docShell->StopLoad();
      */
      
      mParserObserver->End();
      NS_RELEASE(mParserObserver);

      if (mDocShell) 
      {
        // where do we pop up a dialog telling the user they can't edit this doc?
        // this next call will close the window, but do we want to do that?  or tell the .js UI to do it?
        mCloseWindowWhenLoaded = PR_TRUE;
        mCantEditReason = eCantEditFramesets;
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsEditorShell::OnStatusURLLoad(nsIDocumentLoader* loader,
                                  nsIChannel* channel, nsString& aMsg)
{

  SetChromeAttribute(mDocShell, "statusText", "value", aMsg);

  return NS_OK;
}

NS_IMETHODIMP
nsEditorShell::OnEndURLLoad(nsIDocumentLoader* loader,
                               nsIChannel* channel, nsresult aStatus)
{
   return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#endif

// does the document being loaded contain subframes?
nsresult
nsEditorShell::DocumentContainsFrames(nsIDocumentLoader* aLoader, PRBool& outHasFrames)
{
  nsresult rv;
  
  outHasFrames = PR_FALSE;
  
  nsCOMPtr<nsISupports> container;
  aLoader->GetContainer(getter_AddRefs(container));
  nsCOMPtr<nsIDocShellTreeNode> docShellAsNode = do_QueryInterface(container, &rv);
  if (NS_FAILED(rv)) return rv;
  if (!docShellAsNode)
    return NS_ERROR_UNEXPECTED;

  PRInt32 numChildren;
  rv = docShellAsNode->GetChildCount(&numChildren);
  if (NS_FAILED(rv)) return rv;
  
  outHasFrames = (numChildren != 0);

  return NS_OK;
}

// is the document being loaded the root of a frameset, or a non-frameset doc?
nsresult
nsEditorShell::DocumentIsRootDoc(nsIDocumentLoader* aLoader, PRBool& outIsRoot)
{
  nsresult rv;
  
  outIsRoot = PR_TRUE;
  
  nsCOMPtr<nsISupports> container;
  aLoader->GetContainer(getter_AddRefs(container));
  nsCOMPtr<nsIDocShellTreeItem> docShellAsTreeItem = do_QueryInterface(container, &rv);
  if (NS_FAILED(rv)) return rv;
  if (!docShellAsTreeItem)
    return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  rv = docShellAsTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(rootItem));
  if (NS_FAILED(rv)) return rv;
  if (!rootItem)
    return NS_ERROR_UNEXPECTED;
  
  outIsRoot = (rootItem.get() == docShellAsTreeItem.get());
  return NS_OK;
}

nsresult
nsEditorShell::CheckPrefAndNormalizeTable()
{
  nsresult  res = NS_NOINTERFACE;
  nsCOMPtr<nsIHTMLEditor>  htmlEditor = do_QueryInterface(mEditor);

  if (htmlEditor)
  {
    NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &res);
    if (NS_FAILED(res)) return NS_OK;

    PRBool normalizeTable = PR_FALSE;
    if (NS_SUCCEEDED(prefs->GetBoolPref("editor.table.maintain_structure", &normalizeTable)) && normalizeTable)
      return NormalizeTable(nsnull);

    return NS_OK;
  }
  return res;
}


NS_IMETHODIMP
nsEditorShell::HandleMouseClickOnElement(nsIDOMElement *aElement, PRInt32 aClickCount,
                                         PRInt32 x, PRInt32 y, PRBool *_retval)
{
  // Guess it's ok if we don't have an element
  if (!aElement) return NS_OK;
  if (!_retval)  return NS_ERROR_NULL_POINTER;

  *_retval = PR_FALSE;

  // We'll only look at single and double-click
  if (aClickCount > 2) return NS_OK;
  
  nsresult rv = NS_OK;

/*
#if DEBUG_cmanske
  nsAutoString TagName;
  aElement->GetTagName(TagName);
  TagName.ToLowerCase();
  char szTagName[64];
  TagName.ToCString(szTagName, 64);
  printf("***** Element clicked on: %s, x=%d, y=%d\n", szTagName, x, y);
#endif
*/
  if (mDisplayMode == eDisplayModeAllTags) 
  {
    // Always select the element in AllTags mode
    //  in other modes, clicking on images, hline, etc 
    //  already selects them correctly
    // Selection here is used to make clicking on the 
    //  background image in ShowAllTags mode select
    //  contents of a element
    // TODO: It would be great if we could use x, y to restrict
    //  where you click for easier caret placement near border with content,
    //  but:
    //  1. We can get x,y, relative to either screen or "widget" (contentWindow)
    //      origin, but not the element clicked on!
    //  2.  we need to get the size of the element!

    rv = SelectElement(aElement);
    if (NS_SUCCEEDED(rv))
      *_retval = PR_TRUE;
  }

  // For double-click, edit element properties
  if (aClickCount == 2)
  {
    nsAutoString commandName;

    // In "All Tags" mode, use AdvancedProperties,
    //  in others use appriate object property dialog
    if (mDisplayMode != eDisplayModeAllTags) 
      commandName = NS_LITERAL_STRING("cmd_objectProperties");
    else
      commandName = NS_LITERAL_STRING("cmd_advancedProperties");
    
    rv = DoControllerCommand(commandName);

    if (NS_SUCCEEDED(rv))
      *_retval = PR_TRUE;
  }

  return rv;
}

nsresult
nsEditorShell::DoControllerCommand(nsString& aCommand)
{
  // Get the list of controllers...
  nsCOMPtr<nsIControllers> controllers;      
  if(!mContentWindow)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDOMWindowInternal> cwP = do_QueryReferent(mContentWindow);
  if (!cwP) return NS_ERROR_NOT_INITIALIZED;
  nsresult rv = cwP->GetControllers(getter_AddRefs(controllers));      
  if (NS_FAILED(rv)) return rv;
  if (!controllers) return NS_ERROR_NULL_POINTER;

  //... then find the specific controller supporting desired command
  nsCOMPtr<nsIController> controller; 

  rv = controllers->GetControllerForCommand(aCommand.GetUnicode(), getter_AddRefs(controller));
  
  if (NS_SUCCEEDED(rv))
  {
    if (!controller) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIEditorController> composerController = do_QueryInterface(controller);
    // Execute the command
    rv = composerController->DoCommand(aCommand.GetUnicode());
  }
  return rv;
}


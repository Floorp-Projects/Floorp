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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsIDOMXULDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDiskDocument.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsICSSLoader.h"
#include "nsICSSStyleSheet.h"
#include "nsIHTMLContentContainer.h"
#include "nsIStyleSet.h"
#include "nsIURI.h"
#include "nsNetUtil.h"

#include "nsIScriptGlobalObject.h"
#include "nsIWebShellWindow.h"
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
#include "nsIDOMSelection.h"

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
#include "nsIDOMEventReceiver.h"
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
static NS_DEFINE_CID(kCSpellCheckerCID,         NS_SPELLCHECKER_CID);
static NS_DEFINE_IID(kCFileWidgetCID,           NS_FILEWIDGET_CID);
static NS_DEFINE_CID(kCStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kCommonDialogsCID,         NS_CommonDialog_CID );
static NS_DEFINE_CID(kDialogParamBlockCID,      NS_DialogParamBlock_CID);
static NS_DEFINE_CID(kPrefServiceCID,           NS_PREF_CID);

/* Define Interface IDs */
static NS_DEFINE_IID(kISupportsIID,             NS_ISUPPORTS_IID);

#define APP_DEBUG 0 

#define EDITOR_BUNDLE_URL "chrome://editor/locale/editor.properties"

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

// Utility to set and attribute of an element (used for throbber)
static nsresult 
SetChromeAttribute( nsIDocShell *shell, const char *id, 
                    const char *name,  const nsString &value )
{
  nsCOMPtr<nsIDocument> doc;
  nsresult rv = GetDocument( shell, getter_AddRefs(doc) );
  if(NS_SUCCEEDED(rv) && doc)
  {
    // Up-cast.
    nsCOMPtr<nsIDOMXULDocument> xulDoc( do_QueryInterface(doc) );
    if ( xulDoc )
    {
      // Find specified element.
      nsCOMPtr<nsIDOMElement> elem;
      rv = xulDoc->GetElementById( NS_ConvertASCIItoUCS2(id), getter_AddRefs(elem) );
      if ( elem )
        // Set the text attribute.
        rv = elem->SetAttribute( NS_ConvertASCIItoUCS2(name), value );
    }
  }
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
:  mToolbarWindow(nsnull)
,  mContentWindow(nsnull)
,  mParserObserver(nsnull)
,  mStateMaintainer(nsnull)
,  mDocShell(nsnull)
,  mContentAreaDocShell(nsnull)
,  mCloseWindowWhenLoaded(PR_FALSE)
,  mEditorType(eUninitializedEditorType)
,  mWrapColumn(0)
,  mSuggestedWordIndex(0)
,  mDictionaryIndex(0)
{
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

nsresult    
nsEditorShell::PrepareDocumentForEditing(nsIDocumentLoader* aLoader, nsIURI *aUrl)
{
  if (!mContentAreaDocShell)
    return NS_ERROR_NOT_INITIALIZED;

  if (mEditor)
  {
    // Mmm, we have an editor already. That means that someone loaded more than
    // one URL into the content area. Let's tear down what we have, and rip 'em a
    // new one.

    // first, unregister the selection listener, if there was one
    if (mStateMaintainer)
    {
      nsCOMPtr<nsIDOMSelection> domSelection;
      // using a scoped result, because we don't really care if this fails
      nsresult result = GetEditorSelection(getter_AddRefs(domSelection));
      if (NS_SUCCEEDED(result) && domSelection)
      {
        domSelection->RemoveSelectionListener(mStateMaintainer);
        NS_IF_RELEASE(mStateMaintainer);
      }
    }
    mEditorType = eUninitializedEditorType;
    mEditor = 0;  // clear out the nsCOMPtr

    // and tell them that they are doing bad things
    NS_WARNING("Multiple loads of the editor's document detected.");
    // Note that if you registered doc state listeners before the second
    // URL load, they don't get transferred to the new editor.
  }
   
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
  
  nsresult rv = DoEditorMode(containerAsDocShell);
  if (NS_FAILED(rv)) return rv;
  
  // transfer the doc state listeners to the editor
  rv = TransferDocumentStateListeners();
  if (NS_FAILED(rv)) return rv;
  
  // make the UI state maintainer
  NS_NEWXPCOM(mStateMaintainer, nsInterfaceState);
  if (!mStateMaintainer) return NS_ERROR_OUT_OF_MEMORY;
  mStateMaintainer->AddRef();      // the owning reference

  // get the XULDoc from the webshell
  nsCOMPtr<nsIContentViewer> cv;
  rv = mDocShell->GetContentViewer(getter_AddRefs(cv));
  if (NS_FAILED(rv)) return rv;
    
  nsCOMPtr<nsIDocumentViewer> docViewer = do_QueryInterface(cv, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDocument> chromeDoc;
  rv = docViewer->GetDocument(*getter_AddRefs(chromeDoc));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIDOMXULDocument> xulDoc = do_QueryInterface(chromeDoc, &rv);
  if (NS_FAILED(rv)) return rv;

  // now init the state maintainer
  rv = mStateMaintainer->Init(mEditor, xulDoc);
  if (NS_FAILED(rv)) return rv;
  
  // set it up as a selection listener
  nsCOMPtr<nsIDOMSelection> domSelection;
  rv = GetEditorSelection(getter_AddRefs(domSelection));
  if (NS_FAILED(rv)) return rv;

  rv = domSelection->AddSelectionListener(mStateMaintainer);
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

  if (NS_SUCCEEDED(rv) && mContentWindow)
  {
    nsCOMPtr<nsIController> controller;
    nsCOMPtr<nsIControllers> controllers;
    rv = nsComponentManager::CreateInstance("component://netscape/editor/editorcontroller",
                                       nsnull,
                                       NS_GET_IID(nsIController),
                                       getter_AddRefs(controller));
    if (NS_SUCCEEDED(rv) && controller)
    {
      rv = mContentWindow->GetControllers(getter_AddRefs(controllers));
      if (NS_SUCCEEDED(rv) && controllers)
      {
        nsCOMPtr<nsIEditorController> editorController = do_QueryInterface(controller);
        rv = editorController->Init();
        if (NS_FAILED(rv)) return rv;
        
        editorController->SetEditor(editor);//weak link

        rv = controllers->InsertControllerAt(0, controller);
      }
    }
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

#if DEBUG
    printf("Editor is editing %s\n", pageURLString ? pageURLString : "");
#endif

     // only save the file spec if this is a local file, and is not              
     // about:blank                                                              
     if (nsCRT::strncmp(pageScheme, "file", 4) == 0 &&                           
         nsCRT::strncmp(pageURLString,"about:blank", 11) != 0)                   
    {
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
  styleSheets->ApplyOverrideStyleSheet(NS_ConvertASCIItoUCS2("chrome://editor/content/EditorOverride.css"), nsnull);

  // Load the edit mode override style sheet
  // This will be remove for "Browser" mode
  SetDisplayMode(eDisplayModeNormal);

#ifdef DEBUG
  // Activate the debug menu only in debug builds
  // by removing the "hidden" attribute set "true" in XUL
  nsCOMPtr<nsIDOMElement> elem;
  rv = xulDoc->GetElementById(NS_ConvertASCIItoUCS2("debugMenu"), getter_AddRefs(elem));
  if (elem)
    elem->RemoveAttribute(NS_ConvertASCIItoUCS2("hidden"));
#endif

  // Force initial focus to the content window -- HOW?
//  mWebShellWin->SetFocus();
  return NS_OK;
}

NS_IMETHODIMP    
nsEditorShell::SetToolbarWindow(nsIDOMWindow* aWin)
{
  NS_PRECONDITION(aWin != nsnull, "null ptr");
  if (!aWin)
      return NS_ERROR_NULL_POINTER;

  mToolbarWindow = aWin;

  return NS_OK;
}

NS_IMETHODIMP    
nsEditorShell::SetContentWindow(nsIDOMWindow* aWin)
{
  NS_PRECONDITION(aWin != nsnull, "null ptr");
  if (!aWin)
      return NS_ERROR_NULL_POINTER;

  mContentWindow = aWin;

  nsresult  rv;
  nsCOMPtr<nsIScriptGlobalObject> globalObj = do_QueryInterface(mContentWindow, &rv);
  if (NS_FAILED(rv) || !globalObj)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocShell> docShell;
  globalObj->GetDocShell(getter_AddRefs(docShell));
  if (!docShell)
    return NS_ERROR_FAILURE;
    
  mContentAreaDocShell = docShell;      // dont AddRef
  return docShell->SetDocLoaderObserver((nsIDocumentLoaderObserver *)this);
}


NS_IMETHODIMP    
nsEditorShell::SetWebShellWindow(nsIDOMWindow* aWin)
{
  NS_PRECONDITION(aWin != nsnull, "null ptr");
  if (!aWin)
      return NS_ERROR_NULL_POINTER;

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
  //NS_ADDREF(mWebShell);

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

// tell the appcore what type of editor to instantiate
// this must be called before the editor has been instantiated,
// otherwise, an error is returned.
NS_METHOD
nsEditorShell::SetEditorType(const PRUnichar *editorType)
{  
  if (mEditor)
    return NS_ERROR_ALREADY_INITIALIZED;
    
  nsAutoString  theType = editorType;
  theType.ToLowerCase();
  if (theType.EqualsWithConversion("text") || theType.EqualsWithConversion("html") || theType.IsEmpty() || theType.EqualsWithConversion("htmlmail"))
  {
    mEditorTypeString = theType;
    return NS_OK;
  }
  else
  {
    NS_WARNING("Editor type not recognized");
    return NS_ERROR_UNEXPECTED;
  }
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
    
  if (NS_SUCCEEDED(err))
  {
    if (mEditorTypeString.EqualsWithConversion("text"))
    {
      err = editor->Init(aDoc, aPresShell, nsIHTMLEditor::eEditorPlaintextMask);
      mEditorType = ePlainTextEditorType;
    }
    else if (mEditorTypeString.EqualsWithConversion("html") || mEditorTypeString.IsEmpty())  // empty string default to HTML editor
    {
      err = editor->Init(aDoc, aPresShell, 0);
      mEditorType = eHTMLTextEditorType;
    }
    else if (mEditorTypeString.EqualsWithConversion("htmlmail"))  //  HTML editor with special mail rules
    {
      err = editor->Init(aDoc, aPresShell, nsIHTMLEditor::eEditorMailMask);
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
nsEditorShell::UpdateInterfaceState(void)
{
  if (!mStateMaintainer)
    return NS_ERROR_NOT_INITIALIZED;

  return mStateMaintainer->ForceUpdate();
}  

// Deletion routines
nsresult
nsEditorShell::ScrollSelectionIntoView()
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (!editor) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> presShell;
  nsresult result = editor->GetPresShell(getter_AddRefs(presShell));
  if (NS_FAILED(result))
    return result;
  if (!presShell)
    return NS_ERROR_NULL_POINTER;

  return presShell->ScrollSelectionIntoView(SELECTION_NORMAL,
                                            SELECTION_FOCUS_REGION);
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
  UpdateInterfaceState();
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
  UpdateInterfaceState();
  return result;
}

// the name of the attribute here should be the contents of the appropriate
// tag, e.g. 'b' for bold, 'i' for italics.
NS_IMETHODIMP    
nsEditorShell::SetTextProperty(const PRUnichar *prop, const PRUnichar *attr, const PRUnichar *value)
{
  nsIAtom    *styleAtom = nsnull;
  nsresult  err = NS_NOINTERFACE;

  styleAtom = NS_NewAtom(prop);      /// XXX Hack alert! Look in nsIEditProperty.h for this

  if (! styleAtom)
    return NS_ERROR_OUT_OF_MEMORY;

  // addref it while we're using it
  NS_ADDREF(styleAtom);

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
  UpdateInterfaceState();
  NS_RELEASE(styleAtom);
  return err;
}



nsresult
nsEditorShell::RemoveOneProperty(const nsString& aProp, const nsString &aAttr)
{
  nsIAtom    *styleAtom = nsnull;
  nsresult  err = NS_NOINTERFACE;

  styleAtom = NS_NewAtom(aProp);      /// XXX Hack alert! Look in nsIEditProperty.h for this

  if (! styleAtom)
    return NS_ERROR_OUT_OF_MEMORY;

  // addref it while we're using it
  NS_ADDREF(styleAtom);

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

  UpdateInterfaceState();
  NS_RELEASE(styleAtom);
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
  nsresult  res = NS_OK;

  nsCOMPtr<nsIEditorStyleSheets> styleSheets = do_QueryInterface(mEditor);
  if (!styleSheets) return NS_NOINTERFACE;

  if (aDisplayMode == eDisplayModeWYSIWYG)
  {
    // Remove all extra "edit mode" style sheets 
    if (mEditModeStyleSheet)
    {
      styleSheets->RemoveOverrideStyleSheet(mEditModeStyleSheet);
      mEditModeStyleSheet = nsnull;
    }
    if (mAllTagsModeStyleSheet)
    {
      styleSheets->RemoveOverrideStyleSheet(mAllTagsModeStyleSheet);
      mAllTagsModeStyleSheet = nsnull;
    }
  }
  else if (aDisplayMode == eDisplayModeNormal)
  {
    // Remove the AllTags sheet
    if (mAllTagsModeStyleSheet)
    {
      styleSheets->RemoveOverrideStyleSheet(mAllTagsModeStyleSheet);
      mAllTagsModeStyleSheet = nsnull;
    }

    // We are already in the requested mode
    if (mEditModeStyleSheet) return NS_OK;
    
    //Load the editmode style sheet
    nsCOMPtr<nsICSSStyleSheet> styleSheet;
    res = styleSheets->ApplyOverrideStyleSheet(NS_ConvertASCIItoUCS2("chrome://editor/content/EditorContent.css"),
                                                getter_AddRefs(styleSheet));
    
    // Save the returned style sheet so we can remove it later
    if (NS_SUCCEEDED(res))
      mEditModeStyleSheet = styleSheet;
    return res;
  }
  else if (aDisplayMode == eDisplayModeAllTags)
  {
    // We are already in the requested mode
    if (mAllTagsModeStyleSheet) return NS_OK;
    
    //Load the normal mode style sheet
    if (!mEditModeStyleSheet)
    {
      // Note: using "@import url(chrome://editor/content/EditorContent.css);"
      //   in EditorAllTags.css doesn't seem to work!?
      nsCOMPtr<nsICSSStyleSheet> styleSheet;
      res = styleSheets->ApplyOverrideStyleSheet(NS_ConvertASCIItoUCS2("chrome://editor/content/EditorContent.css"),
                                                  getter_AddRefs(styleSheet));
    
      // Save the returned style sheet so we can remove it later
      if (NS_SUCCEEDED(res))
        mEditModeStyleSheet = styleSheet;
    }

    //Load the editmode style sheet
    nsCOMPtr<nsICSSStyleSheet> styleSheet;
    res = styleSheets->ApplyOverrideStyleSheet(NS_ConvertASCIItoUCS2("chrome://editor/content/EditorAllTags.css"),
                                                getter_AddRefs(styleSheet));
    
    // Save the returned style sheet so we can remove it later
    if (NS_SUCCEEDED(res))
      mAllTagsModeStyleSheet = styleSheet;
    return res;
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsEditorShell::DisplayParagraphMarks(PRBool aShowMarks)
{
  nsresult  res = NS_OK;

  nsCOMPtr<nsIEditorStyleSheets> styleSheets = do_QueryInterface(mEditor);
  if (!styleSheets) return NS_NOINTERFACE;
  
  if (aShowMarks)
  {
    // Check if style sheet is already loaded
    if (mParagraphMarksStyleSheet) return NS_OK;

    //Load the style sheet
    nsCOMPtr<nsICSSStyleSheet> styleSheet;
    res = styleSheets->ApplyOverrideStyleSheet(NS_ConvertASCIItoUCS2("chrome://editor/content/EditorParagraphMarks.css"),
                                                getter_AddRefs(styleSheet));
    
    // Save the returned style sheet so we can remove it later
    if (NS_SUCCEEDED(res))
      mParagraphMarksStyleSheet = styleSheet;
  }
  else if (mParagraphMarksStyleSheet)
  {
    res = styleSheets->RemoveOverrideStyleSheet(mParagraphMarksStyleSheet);
    mParagraphMarksStyleSheet = nsnull;  
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

   nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mContentAreaDocShell));
   NS_ENSURE_SUCCESS(webNav->LoadURI(url), NS_ERROR_FAILURE);

   return NS_OK;
}


NS_IMETHODIMP    
nsEditorShell::RegisterDocumentStateListener(nsIDocumentStateListener *docListener)
{
  nsresult rv = NS_OK;
  
  if (!docListener)
    return NS_ERROR_NULL_POINTER;
  
  // if we have an editor already, just pass this baby through.
  if (mEditor)
  {
    nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor, &rv);
    if (NS_FAILED(rv))
      return rv;
  
    return editor->AddDocumentStateListener(docListener);
  }
  
  // otherwise, keep it until we create an editor.
  if (!mDocStateListeners)
  {
    rv = NS_NewISupportsArray(getter_AddRefs(mDocStateListeners));
    if (NS_FAILED(rv)) return rv;
  }
  nsCOMPtr<nsISupports> iSupports = do_QueryInterface(docListener, &rv);
  if (NS_FAILED(rv)) return rv;

  // note that this return value is really a PRBool, so be sure to use
  // NS_SUCCEEDED or NS_FAILED to check it.
  return mDocStateListeners->AppendElement(iSupports);
}

NS_IMETHODIMP    
nsEditorShell::UnregisterDocumentStateListener(nsIDocumentStateListener *docListener)
{
  if (!docListener)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;
  
  // if we have an editor already, just pass this baby through.
  if (mEditor)
  {
    nsCOMPtr<nsIEditor>  editor = do_QueryInterface(mEditor, &rv);
    if (NS_FAILED(rv))
      return rv;
  
    return editor->RemoveDocumentStateListener(docListener);
  }

  // otherwise, see if it exists in our list
  if (!mDocStateListeners)
    return (nsresult)PR_FALSE;      // yeah, this sucks, but I'm emulating the behaviour of
                                    // nsISupportsArray::RemoveElement()

  nsCOMPtr<nsISupports> iSupports = do_QueryInterface(docListener, &rv);
  if (NS_FAILED(rv)) return rv;

  // note that this return value is really a PRBool, so be sure to use
  // NS_SUCCEEDED or NS_FAILED to check it.
  return mDocStateListeners->RemoveElement(iSupports);
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
  while (NS_SUCCEEDED(mDocStateListeners->Count(&numListeners)) && numListeners > 0)
  {
    nsCOMPtr<nsISupports> iSupports = getter_AddRefs(mDocStateListeners->ElementAt(0));
    nsCOMPtr<nsIDocumentStateListener> docStateListener = do_QueryInterface(iSupports);
    if (docStateListener)
    {
      // this checks for duplicates
      rv = editor->AddDocumentStateListener(docStateListener);
    }
    
    mDocStateListeners->RemoveElementAt(0);
  }
  
  // free the array
  mDocStateListeners = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsEditorShell::CheckOpenWindowForURLMatch(const PRUnichar* inFileURL, nsIDOMWindow* inCheckWindow, PRBool *aDidFind)
{
  if (!inCheckWindow) return NS_ERROR_NULL_POINTER;
  *aDidFind = PR_FALSE;
  

  // get an nsFileSpec from the URL
  // This assumes inFileURL is "file://" format
  nsFileURL    fileURL(inFileURL);
  nsFileSpec   fileSpec(fileURL);

  nsCOMPtr<nsIDOMWindow> contentWindow;
  inCheckWindow->GetContent(getter_AddRefs(contentWindow));
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
nsEditorShell::CheckAndSaveDocument(const PRUnichar *reasonToSave, PRBool *_retval)
{
  *_retval = PR_FALSE;

  nsCOMPtr<nsIDOMDocument> theDoc;
  nsresult rv = GetEditorDocument(getter_AddRefs(theDoc));
  if (NS_SUCCEEDED(rv) && theDoc)
  {
    nsCOMPtr<nsIDiskDocument> diskDoc = do_QueryInterface(theDoc);
    if (diskDoc)
    {
      PRInt32  modCount = 0;
      diskDoc->GetModCount(&modCount);

      // Return true unless user cancels an action
      *_retval = PR_TRUE;

      if (modCount > 0)
      {
        // Ask user if they want to save current changes
        nsAutoString reasonToSaveStr(reasonToSave);
        nsAutoString tmp1, tmp2, title;
        GetBundleString(NS_ConvertASCIItoUCS2("Save"), tmp1);
        GetBundleString(NS_ConvertASCIItoUCS2("DontSave"), tmp2);
        GetDocumentTitleString(title);
        // If title is empty, use "untitled"
        if (title.Length() == 0)
          GetBundleString(NS_ConvertASCIItoUCS2("untitled"), title);

        nsAutoString saveMsg;
        GetBundleString(NS_ConvertASCIItoUCS2("SaveFilePrompt"), saveMsg);
        saveMsg.ReplaceSubstring(NS_ConvertASCIItoUCS2("%title%"), title);
        saveMsg.ReplaceSubstring(NS_ConvertASCIItoUCS2("%reason%"), reasonToSaveStr);

        nsAutoString saveDocString;
        GetBundleString(NS_ConvertASCIItoUCS2("SaveDocument"), saveDocString);
        EConfirmResult result = ConfirmWithCancel(saveDocString, saveMsg, &tmp1, &tmp2);
        if (result == eCancel)
        {
          *_retval = PR_FALSE;
        } else if (result == eYes)
        {
          // Either save to existing file or prompt for name (as for SaveAs)
          // We don't continue if we failed to save file (_retval is set to FALSE)
          rv = SaveDocument(PR_FALSE, PR_FALSE, _retval);
        }
      }
    }
  }
  return rv;
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
        nsCOMPtr<nsIDOMHTMLDocument> HTMLDoc = do_QueryInterface(doc);
        if (!HTMLDoc) return NS_ERROR_FAILURE;
        res = HTMLDoc->GetTitle(title);
        if (NS_FAILED(res)) return res;
        
        if (mustShowFileDialog)
        {
          // Prompt for title ONLY if existing title is empty
          if ( title.Length() == 0)
          {
            // Use a "prompt" common dialog to get title string from user
            NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &res); 
            if (NS_SUCCEEDED(res)) 
            { 
              PRUnichar *titleUnicode;
              nsAutoString captionStr, msgStr;
              
              GetBundleString(NS_ConvertASCIItoUCS2("DocumentTitle"), captionStr);
              GetBundleString(NS_ConvertASCIItoUCS2("NeedDocTitle"), msgStr); 
              
              PRBool retVal = PR_FALSE;
              res = dialog->Prompt(mContentWindow, captionStr.GetUnicode(), msgStr.GetUnicode(),
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
              res = HTMLDoc->GetURL(urlstring);

     //         ?????
     //         res = HTMLDoc->GetSourceDocumentURL(jscx, uri);
     //         do a QI to get an nsIURL and then call GetFileName()

              // if it's not a local file already, grab the current file name
              if ( (urlstring.CompareWithConversion("file", PR_TRUE, 4) != 0 )
                && (urlstring.CompareWithConversion("about:blank", PR_TRUE, -1) != 0) )
              {
                PRInt32 index = urlstring.RFindChar((PRUnichar)'/', PR_FALSE, -1, -1 );
                if ( index != -1 )
                {
                  // remove cruft before file name including '/'
                  // if the url ends with a '/' then the whole string will be cut
                  urlstring.Cut(0, index + 1);
                  if (urlstring.Length() > 0)
                    fileName.Assign( urlstring );
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
            // 1ST PARAM SHOULD BE nsIDOMWindow*, not nsIWidget*
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
        res = editor->SaveFile(&docFileSpec, replacing, saveCopy, nsIDiskDocument::eSaveFileHTML);
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
nsEditorShell::CloseWindow( PRBool *_retval )
{
  nsAutoString beforeClosingStr;
  GetBundleString(NS_ConvertASCIItoUCS2("BeforeClosing"), beforeClosingStr);
  
  nsresult rv = CheckAndSaveDocument(beforeClosingStr.GetUnicode(), _retval);
 
  // Don't close the window if there was an error saving file or 
  //   user canceled an action along the way
  if (NS_SUCCEEDED(rv) && *_retval)
    {
    nsCOMPtr<nsIBaseWindow> baseWindow;
    GetTreeOwner(mDocShell, getter_AddRefs(baseWindow));
    NS_ENSURE_TRUE(baseWindow, NS_ERROR_FAILURE);
    baseWindow->Destroy();
    }

  return rv;
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
nsEditorShell::GetLocalFileURL(nsIDOMWindow *parent, const PRUnichar *filterType, PRUnichar **_retval)
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
            res = domDoc->CreateElement(NS_ConvertASCIItoUCS2("title"),getter_AddRefs(titleElement));
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
            // Go through the editor API so action is undoable
            res = editor->InsertNode(textNode, titleNode, 0);
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

  nsCOMPtr<nsIDOMSelection> selection;
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

  nsCOMPtr<nsIDOMSelection> selection;
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
  NS_WITH_SERVICE(nsIFindComponent, findComponent, NS_IFINDCOMPONENT_PROGID, &rv);
  NS_ASSERTION(((NS_SUCCEEDED(rv)) && findComponent), "GetService failed for find component.");
  if (NS_FAILED(rv)) { return rv; }

  // make the search context if we need to
  if (!mSearchContext)
  {
    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mContentAreaDocShell));
    rv = findComponent->CreateContext( webShell, nsnull, getter_AddRefs(mSearchContext));
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
    
    nsAllocator::Free(ptrv);
  }
  else
  {
    outString.SetLength(0);
  }
}

// Utility to bring up a Yes/No/Cancel dialog.
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
    block->SetInt( nsICommonDialogs::eNumberButtons,3 ); 
    block->SetString( nsICommonDialogs::eMsg, aQuestion.GetUnicode()); 
    nsAutoString url; url.AssignWithConversion( "chrome://global/skin/question-icon.gif"  ); 
    block->SetString( nsICommonDialogs::eIconURL, url.GetUnicode()); 

    nsAutoString yesStr, noStr;
    if (aYesString)
      yesStr.Assign(*aYesString);
    else
      GetBundleString(NS_ConvertASCIItoUCS2("Yes"), yesStr);

    if (aNoString)
      noStr.Assign(*aNoString);
    else
      GetBundleString(NS_ConvertASCIItoUCS2("No"), noStr);

    nsAutoString cancelStr;
    GetBundleString(NS_ConvertASCIItoUCS2("Cancel"), cancelStr);

    block->SetString( nsICommonDialogs::eDialogTitle, aTitle.GetUnicode() );
    //Note: "button0" is always Ok or Yes action, "button1" is Cancel
    block->SetString( nsICommonDialogs::eButton0Text, yesStr.GetUnicode() ); 
    block->SetString( nsICommonDialogs::eButton1Text, cancelStr.GetUnicode() ); 
    block->SetString( nsICommonDialogs::eButton2Text, noStr.GetUnicode() ); 

    NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv); 
    if ( NS_SUCCEEDED( rv ) ) 
    { 
      PRInt32 buttonPressed = 0; 
      rv = dialog->DoDialog( mContentWindow, block, "chrome://global/content/commonDialog.xul" ); 
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
    rv = dialog->Confirm(mContentWindow, aTitle.GetUnicode(), aQuestion.GetUnicode(), &result);
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
    rv = dialog->Alert(mContentWindow, aTitle, aMsg);

  return rv;
}

void
nsEditorShell::Alert(const nsString& aTitle, const nsString& aMsg)
{
  nsresult rv;
  NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv); 
  if (NS_SUCCEEDED(rv) && dialog)
  {
    rv = dialog->Alert(mContentWindow, aTitle.GetUnicode(), aMsg.GetUnicode());
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

  if (editor)
    return editor->SetDocumentCharacterSet(characterSet);

  return NS_ERROR_FAILURE;

}

NS_IMETHODIMP
nsEditorShell::GetContentsAs(const PRUnichar *format, PRUint32 flags,
                             PRUnichar **contentsAs)
{
  nsresult  err = NS_NOINTERFACE;

  nsAutoString aFormat (format);
  nsAutoString aContentsAs;

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (editor)
    err = editor->OutputToString(aContentsAs, aFormat, flags);

  *contentsAs = aContentsAs.ToNewUnicode();
  
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
nsEditorShell::GetEditorSelection(nsIDOMSelection** aEditorSelection)
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

  nsCOMPtr<nsIPresShell> presShell;
  nsresult rv = editor->GetPresShell(getter_AddRefs(presShell));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsISelectionController> selCont (do_QueryInterface(presShell));
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
nsEditorShell::GetDocumentLength(PRInt32 *aDocumentLength)
{
  nsCOMPtr<nsIHTMLEditor> editor = do_QueryInterface(mEditor);
  if (editor)
    return editor->GetDocumentLength(aDocumentLength);

  return NS_NOINTERFACE;
}


NS_IMETHODIMP
nsEditorShell::MakeOrChangeList(const PRUnichar *listType)
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
        err = mEditor->MakeOrChangeList(aListType);
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
        err = mEditor->RemoveList(aListType);
      break;

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
        result = tableEditor->GetFirstSelectedCell(aOutElement, nsnull);
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
          result = tableEditor->InsertTableCell(aNumber, bAfter);
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
          result = tableEditor->DeleteTable();
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
          result = tableEditor->DeleteTableCell(aNumber);
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
          result = tableEditor->DeleteTableCellContents();
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
          result = tableEditor->DeleteTableRow(aNumber);
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
          result = tableEditor->DeleteTableColumn(aNumber);
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP    
nsEditorShell::JoinTableCells()
{
  nsresult  result = NS_NOINTERFACE;
  switch (mEditorType)
  {
    case eHTMLTextEditorType:
      {
        nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(mEditor);
        if (tableEditor)
          result = tableEditor->JoinTableCells();
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
          result = tableEditor->GetCellAt(tableElement, rowIndex, colIndex, *_retval);
      }
      break;
    default:
      result = NS_ERROR_NOT_IMPLEMENTED;
  }

  return result;
}

// Note that the return param in the IDL must be the LAST out param here,
//   so order of params is different from nsIHTMLEditor
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

  nsresult result;

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
nsEditorShell::StartSpellChecking(PRUnichar **aFirstMisspelledWord)
{
  nsresult  result = NS_NOINTERFACE;
  nsAutoString firstMisspelledWord;
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

    result = nsComponentManager::CreateInstance(kCSpellCheckerCID,
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
    // Return the first misspelled word and initialize the suggested list
    result = mSpellChecker->NextMisspelledWord(&firstMisspelledWord, &mSuggestedWordList);
  }
  *aFirstMisspelledWord = firstMisspelledWord.ToNewUnicode();
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
nsEditorShell::CloseSpellChecking()
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
  nsresult res = NS_OK;
	if (NS_FAILED(aStatus))
	  return NS_OK;
	  
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
      }
    }
  }
  
  PRBool isRootDoc;
  res = DocumentIsRootDoc(aLoader, isRootDoc);
  if (NS_FAILED(res)) return res;
  
  if (mCloseWindowWhenLoaded && isRootDoc)
  {
    nsAutoString alertLabel, alertMessage;
    GetBundleString(NS_ConvertASCIItoUCS2("Alert"), alertLabel);
    GetBundleString(NS_ConvertASCIItoUCS2("CantEditFramesetMsg"), alertMessage);
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



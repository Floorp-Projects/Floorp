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
#include "nslayout.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsISupports.h"
#include "nsIContent.h"
#include "nsIContentViewerContainer.h"
#include "nsIDocumentViewer.h"
#include "nsIDOMWindowInternal.h"

#include "nsIImageGroup.h"
#include "nsIImageObserver.h"

#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsICSSStyleSheet.h"
#include "nsIStyleContext.h"
#include "nsIFrame.h"

#include "nsIScriptGlobalObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsILinkHandler.h"
#include "nsIDOMDocument.h"
#include "nsISelectionListener.h"
#include "nsISelectionPrivate.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMRange.h"
#include "nsLayoutCID.h"

#include "nsViewsCID.h"
#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"
#include "nsIDeviceContext.h"
#include "nsIDeviceContextSpec.h"
#include "nsIDeviceContextSpecFactory.h"
#include "nsIViewManager.h"
#include "nsIView.h"

#include "nsIPref.h"
#include "nsIPageSequenceFrame.h"
#include "nsIURL.h"
#include "nsIWebShell.h"
#include "nsIContentViewerEdit.h"
#include "nsIContentViewerFile.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIInterfaceRequestor.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShell.h"
#include "nsIFrameDebug.h"

#include "nsIChromeRegistry.h"

#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"

//focus
#include "nsIDOMEventReceiver.h" 
#include "nsIDOMFocusListener.h"
#include "nsISelectionController.h"

#ifdef MOZ_XSL
#include "nsITransformMediator.h"
#endif

static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);

#ifdef NS_DEBUG
#undef NOISY_VIEWER
#else
#undef NOISY_VIEWER
#endif


class DocumentViewerImpl;

// a small delegate class used to avoid circular references

#ifdef XP_MAC
#pragma mark ** nsDocViwerSelectionListener **
#endif

class nsDocViwerSelectionListener : public nsISelectionListener
{
public:

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsISelectionListerner interface
  NS_DECL_NSISELECTIONLISTENER  

                       nsDocViwerSelectionListener()
                       : mDocViewer(NULL)
                       , mGotSelectionState(PR_FALSE)
                       , mSelectionWasCollapsed(PR_FALSE)
                       {
                         NS_INIT_REFCNT();
                       }
                       
  virtual              ~nsDocViwerSelectionListener() {}
   
  nsresult             Init(DocumentViewerImpl *aDocViewer);

protected:

  DocumentViewerImpl*  mDocViewer;
  PRPackedBool         mGotSelectionState;
  PRPackedBool         mSelectionWasCollapsed;
  
};


/** editor Implementation of the FocusListener interface
 */
class nsDocViewerFocusListener : public nsIDOMFocusListener 
{
public:
  /** default constructor
   */
  nsDocViewerFocusListener();
  /** default destructor
   */
  virtual ~nsDocViewerFocusListener();


/*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

/*BEGIN implementations of focus event handler interface*/
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent);
  virtual nsresult Focus(nsIDOMEvent* aEvent);
  virtual nsresult Blur(nsIDOMEvent* aEvent);
/*END implementations of focus event handler interface*/
  nsresult             Init(DocumentViewerImpl *aDocViewer);

private:
    DocumentViewerImpl*  mDocViewer;
};



#ifdef XP_MAC
#pragma mark ** DocumentViewerImpl **
#endif

class DocumentViewerImpl : public nsIDocumentViewer,
                           public nsIContentViewerEdit,
                           public nsIContentViewerFile,
                           public nsIMarkupDocumentViewer,
                           public nsIImageGroupObserver
{
  friend class nsDocViwerSelectionListener;
  
public:
  DocumentViewerImpl();
  DocumentViewerImpl(nsIPresContext* aPresContext);
  
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIContentViewer interface...
  NS_IMETHOD Init(nsIWidget* aParentWidget,
                  nsIDeviceContext* aDeviceContext,
                  const nsRect& aBounds);
  NS_IMETHOD BindToDocument(nsISupports* aDoc, const char* aCommand);
  NS_IMETHOD SetContainer(nsISupports* aContainer);
  NS_IMETHOD GetContainer(nsISupports** aContainerResult);
  NS_IMETHOD LoadComplete(nsresult aStatus);
  NS_IMETHOD Destroy(void);
  NS_IMETHOD Stop(void);
  NS_IMETHOD GetDOMDocument(nsIDOMDocument **aResult);
  NS_IMETHOD SetDOMDocument(nsIDOMDocument *aDocument);
  NS_IMETHOD GetBounds(nsRect& aResult);
  NS_IMETHOD SetBounds(const nsRect& aBounds);
  NS_IMETHOD Move(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD Show();
  NS_IMETHOD Hide();
  NS_IMETHOD SetEnableRendering(PRBool aOn);
  NS_IMETHOD GetEnableRendering(PRBool* aResult);

  // nsIDocumentViewer interface...
  NS_IMETHOD SetUAStyleSheet(nsIStyleSheet* aUAStyleSheet);
  NS_IMETHOD GetDocument(nsIDocument*& aResult);
  NS_IMETHOD GetPresShell(nsIPresShell*& aResult);
  NS_IMETHOD GetPresContext(nsIPresContext*& aResult);
  NS_IMETHOD CreateDocumentViewerUsing(nsIPresContext* aPresContext,
                                       nsIDocumentViewer*& aResult);
#ifdef MOZ_XSL
  NS_IMETHOD SetTransformMediator(nsITransformMediator* aMediator);
#endif

  // nsIContentViewerEdit
  NS_DECL_NSICONTENTVIEWEREDIT

  // nsIContentViewerFile
  NS_DECL_NSICONTENTVIEWERFILE

  // nsIMarkupDocumentViewer
  NS_DECL_NSIMARKUPDOCUMENTVIEWER

  typedef void (*CallChildFunc)(nsIMarkupDocumentViewer* aViewer,
                                void* aClosure);
  nsresult CallChildren(CallChildFunc aFunc, void* aClosure);

  // nsIImageGroupObserver interface
  virtual void Notify(nsIImageGroup *aImageGroup,
                      nsImageGroupNotification aNotificationType);
  
protected:
  virtual ~DocumentViewerImpl();

private:
  void ForceRefresh(void);
  nsresult CreateStyleSet(nsIDocument* aDocument, nsIStyleSet** aStyleSet);
  nsresult MakeWindow(nsIWidget* aParentWidget,
                      const nsRect& aBounds);

  nsresult GetDocumentSelection(nsISelection **aSelection);

  //
  // The following three methods are used for printing...
  //
  void DocumentReadyForPrinting();

  static void PR_CALLBACK HandlePLEvent(PLEvent* aEvent);
  static void PR_CALLBACK DestroyPLEvent(PLEvent* aEvent);

protected:
  // IMPORTANT: The ownership implicit in the following member
  // variables has been explicitly checked and set using nsCOMPtr
  // for owning pointers and raw COM interface pointers for weak
  // (ie, non owning) references. If you add any members to this
  // class, please make the ownership explicit (pinkerton, scc).
  
  nsISupports* mContainer; // [WEAK] it owns me!
  nsCOMPtr<nsIDeviceContext> mDeviceContext;   // ??? can't hurt, but...
  nsIView*                 mView;        // [WEAK] cleaned up by view mgr

  // the following six items are explicitly in this order
  // so they will be destroyed in the reverse order (pinkerton, scc)
#ifdef MOZ_XSL
  nsCOMPtr<nsITransformMediator> mTransformMediator;
#endif
  nsCOMPtr<nsIDocument>    mDocument;
  nsCOMPtr<nsIWidget>      mWindow;      // ??? should we really own it?
  nsCOMPtr<nsIViewManager> mViewManager;
  nsCOMPtr<nsIPresContext> mPresContext;
  nsCOMPtr<nsIPresShell>   mPresShell;

  nsCOMPtr<nsIStyleSheet>  mUAStyleSheet;

  nsCOMPtr<nsISelectionListener> mSelectionListener;
  nsCOMPtr<nsIDOMFocusListener> mFocusListener;
  
  PRBool  mEnableRendering;
  PRInt16 mNumURLStarts;
  PRBool  mIsPrinting;



  // printing members
  nsIDeviceContext  *mPrintDC;
  nsIPresContext    *mPrintPC;
  nsIStyleSet       *mPrintSS;
  nsIPresShell      *mPrintPS;
  nsIViewManager    *mPrintVM;
  nsIView           *mPrintView;
  FILE              *mFilePointer;   // a file where information can go to when printing

  nsCOMPtr<nsIPrintListener>  mPrintListener; // An observer for printing...

  // document management data
  //   these items are specific to markup documents (html and xml)
  //   may consider splitting these out into a subclass
  PRBool   mAllowPlugins;
  /* character set member data */
  nsString mDefaultCharacterSet;
  nsString mHintCharset;
  nsCharsetSource mHintCharsetSource;
  nsString mForceCharacterSet;
};

// Class IDs
static NS_DEFINE_CID(kViewManagerCID,       NS_VIEW_MANAGER_CID);
static NS_DEFINE_CID(kScrollingViewCID,     NS_SCROLLING_VIEW_CID);
static NS_DEFINE_CID(kWidgetCID,            NS_CHILD_CID);
static NS_DEFINE_CID(kViewCID,              NS_VIEW_CID);

nsresult
NS_NewDocumentViewer(nsIDocumentViewer** aResult)
{
  NS_PRECONDITION(aResult, "null OUT ptr");
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  DocumentViewerImpl* it = new DocumentViewerImpl();
  if (nsnull == it) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIDocumentViewer), (void**) aResult);
}

// Note: operator new zeros our memory
DocumentViewerImpl::DocumentViewerImpl()
{
  NS_INIT_REFCNT();
  mEnableRendering = PR_TRUE;
  mFilePointer = nsnull;
  mPrintListener = nsnull;
}

DocumentViewerImpl::DocumentViewerImpl(nsIPresContext* aPresContext)
  : mPresContext(dont_QueryInterface(aPresContext))
{
  NS_INIT_REFCNT();
  mHintCharsetSource = kCharsetUninitialized;
  mAllowPlugins = PR_TRUE;
  mEnableRendering = PR_TRUE;
  mFilePointer = nsnull;

}

// ISupports implementation...
NS_IMPL_ADDREF(DocumentViewerImpl)
NS_IMPL_RELEASE(DocumentViewerImpl)

nsresult
DocumentViewerImpl::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(NS_GET_IID(nsIContentViewer))) {
    nsIContentViewer* tmp = this;
    *aInstancePtr = (void*)tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDocumentViewer))) {
    nsIDocumentViewer* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIMarkupDocumentViewer))) {
    nsIMarkupDocumentViewer* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIContentViewerFile))) {
    nsIContentViewerFile* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIContentViewerEdit))) {
    nsIContentViewerEdit* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    nsIContentViewer* tmp1 = this;
    nsISupports* tmp2 = tmp1;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

DocumentViewerImpl::~DocumentViewerImpl()
{
  nsresult rv;

  if (mDocument) {
    // Break global object circular reference on the document created
    // in the DocViewer Init
    nsCOMPtr<nsIScriptGlobalObject> globalObject;
    mDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));
    if (globalObject) {
      globalObject->SetNewDocument(nsnull);
    }
    // out of band cleanup of webshell
    mDocument->SetScriptGlobalObject(nsnull);
    if (mFocusListener) {
      // get the DOM event receiver
      nsCOMPtr<nsIDOMEventReceiver> erP;
      rv = mDocument->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), getter_AddRefs(erP));
      if(NS_SUCCEEDED(rv) && erP)
        erP->RemoveEventListenerByIID(mFocusListener, NS_GET_IID(nsIDOMFocusListener));
    }
 }

  if (mPresContext) {
    mPresContext->SetContainer(nsnull);
    mPresContext->SetLinkHandler(nsnull);
  }

  if (mDeviceContext)
    mDeviceContext->FlushFontCache();

  if (mPresShell) {
    // Break circular reference (or something)
    mPresShell->EndObservingDocument();
    nsCOMPtr<nsISelection> selection;
    rv = GetDocumentSelection(getter_AddRefs(selection));
    nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(selection));
    if (NS_FAILED(rv) || !selPrivate) 
      return;
    if (mSelectionListener)
      selPrivate->RemoveSelectionListener(mSelectionListener);
  }
  
}

/*
 * This method is called by the Document Loader once a document has
 * been created for a particular data stream...  The content viewer
 * must cache this document for later use when Init(...) is called.
 */
NS_IMETHODIMP
DocumentViewerImpl::BindToDocument(nsISupports *aDoc, const char *aCommand)
{
  NS_PRECONDITION(!mDocument, "Viewer is already bound to a document!");

#ifdef NOISY_VIEWER
  printf("DocumentViewerImpl::BindToDocument\n");
#endif

  nsresult rv;
  mDocument = do_QueryInterface(aDoc,&rv);
  return rv;
}

NS_IMETHODIMP
DocumentViewerImpl::SetContainer(nsISupports* aContainer)
{
  mContainer = aContainer;
  if (mPresContext) {
    mPresContext->SetContainer(aContainer);
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetContainer(nsISupports** aResult)
{
   NS_ENSURE_ARG_POINTER(aResult);

   *aResult = mContainer;
   NS_IF_ADDREF(*aResult);

   return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Init(nsIWidget* aParentWidget,
                         nsIDeviceContext* aDeviceContext,
                         const nsRect& aBounds)
{
  nsresult rv;

  if (!mDocument) {
    return NS_ERROR_NULL_POINTER;
  }

  mDeviceContext = dont_QueryInterface(aDeviceContext);

  PRBool makeCX = PR_FALSE;
  if (!mPresContext) {
    // Create presentation context
#if 1 
    rv = NS_NewGalleyContext(getter_AddRefs(mPresContext));
#else // turn on print preview for debugging until print preview is fixed
    rv = NS_NewPrintPreviewContext(getter_AddRefs(mPresContext));
#endif
    if (NS_FAILED(rv)) return rv;

    mPresContext->Init(aDeviceContext); 
    makeCX = PR_TRUE;
  }

  nsCOMPtr<nsIInterfaceRequestor> requestor(do_QueryInterface(mContainer));
  if (requestor) {
    nsCOMPtr<nsILinkHandler> linkHandler;
    requestor->GetInterface(NS_GET_IID(nsILinkHandler), 
       getter_AddRefs(linkHandler));
    mPresContext->SetContainer(mContainer);
    mPresContext->SetLinkHandler(linkHandler);

    // Set script-context-owner in the document
    nsCOMPtr<nsIScriptGlobalObjectOwner> owner;
    requestor->GetInterface(NS_GET_IID(nsIScriptGlobalObjectOwner),
       getter_AddRefs(owner));
    if (nsnull != owner) {
      nsCOMPtr<nsIScriptGlobalObject> global;
      rv = owner->GetScriptGlobalObject(getter_AddRefs(global));
      if (NS_SUCCEEDED(rv) && (nsnull != global)) {
        mDocument->SetScriptGlobalObject(global);
        nsCOMPtr<nsIDOMDocument> domdoc(do_QueryInterface(mDocument));
        if (nsnull != domdoc) {
          global->SetNewDocument(domdoc);
        }
      }
    }
  }

  // Create the ViewManager and Root View...
  rv = MakeWindow(aParentWidget, aBounds);
  if (NS_FAILED(rv)) return rv;

  // Create the style set...
  nsIStyleSet* styleSet;
  rv = CreateStyleSet(mDocument, &styleSet);
  if (NS_FAILED(rv)) return rv;

    // Now make the shell for the document
    rv = mDocument->CreateShell(mPresContext, mViewManager, styleSet,
                                getter_AddRefs(mPresShell));
    NS_RELEASE(styleSet);
  if (NS_FAILED(rv)) return rv;
  mPresShell->BeginObservingDocument();

      // Initialize our view manager
      nsRect bounds;
      mWindow->GetBounds(bounds);
      nscoord width = bounds.width;
      nscoord height = bounds.height;
      float p2t;
      mPresContext->GetPixelsToTwips(&p2t);
      width = NSIntPixelsToTwips(width, p2t);
      height = NSIntPixelsToTwips(height, p2t);
      mViewManager->DisableRefresh();
      mViewManager->SetWindowDimensions(width, height);

      if (!makeCX) {
        // Make shell an observer for next time
        // XXX - we observe the docuement always, see above after preshell is created
        // mPresShell->BeginObservingDocument();

//XXX I don't think this should be done *here*; and why paint nothing
//(which turns out to cause black flashes!)???
        // Resize-reflow this time
        mPresShell->InitialReflow(width, height);

        // Now trigger a refresh
        if (mEnableRendering) {
          mViewManager->EnableRefresh(NS_VMREFRESH_IMMEDIATE);
        }
      }

  // now register ourselves as a selection listener, so that we get called
  // when the selection changes in the window
  nsDocViwerSelectionListener *selectionListener;
  NS_NEWXPCOM(selectionListener, nsDocViwerSelectionListener);
  if (!selectionListener) return NS_ERROR_OUT_OF_MEMORY;
  selectionListener->Init(this);
  
  // this is the owning reference. The nsCOMPtr will take care of releasing
  // our ref to the listener on destruction.
  NS_ADDREF(selectionListener);
  rv = selectionListener->QueryInterface(NS_GET_IID(nsISelectionListener), getter_AddRefs(mSelectionListener));
  NS_RELEASE(selectionListener);
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsISelection> selection;
  rv = GetDocumentSelection(getter_AddRefs(selection));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(selection));
  rv = selPrivate->AddSelectionListener(mSelectionListener);
  if (NS_FAILED(rv)) return rv;
  
  //focus listener
  // now register ourselves as a focus listener, so that we get called
  // when the focus changes in the window
  nsDocViewerFocusListener *focusListener;
  NS_NEWXPCOM(focusListener, nsDocViewerFocusListener);
  if (!focusListener) return NS_ERROR_OUT_OF_MEMORY;
  focusListener->Init(this);
  
  // this is the owning reference. The nsCOMPtr will take care of releasing
  // our ref to the listener on destruction.
  NS_ADDREF(focusListener);
  rv = focusListener->QueryInterface(NS_GET_IID(nsIDOMFocusListener), getter_AddRefs(mFocusListener));
  NS_RELEASE(focusListener);
  if (NS_FAILED(rv)) 
    return rv;

  if(mDocument)
  {
    // get the DOM event receiver
    nsCOMPtr<nsIDOMEventReceiver> erP;
    rv = mDocument->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), getter_AddRefs(erP));
    if(NS_FAILED(rv) || !erP)
      return rv?rv:NS_ERROR_FAILURE;

    rv = erP->AddEventListenerByIID(mFocusListener, NS_GET_IID(nsIDOMFocusListener));
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register focus listener");
  }
  return rv;
}

//
// LoadComplete(aStatus)
//
//   aStatus - The status returned from loading the document.
//
// This method is called by the container when the document has been
// completely loaded.
//
NS_IMETHODIMP
DocumentViewerImpl::LoadComplete(nsresult aStatus)
{
  nsresult rv;

  nsCOMPtr<nsIScriptGlobalObject> global;

  // First, get the script global object from the document...
  if (mDocument) {
    rv = mDocument->GetScriptGlobalObject(getter_AddRefs(global));
  }

  // Fail if no ScriptGlobalObject is available...
  NS_ASSERTION(global, "nsIScriptGlobalObject not set for document!");
  if (!global) return NS_ERROR_NULL_POINTER;

  // Now, fire either an OnLoad or OnError event to the document...
  if(NS_SUCCEEDED(aStatus)) {
    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event;

    event.eventStructType = NS_EVENT;
    event.message = NS_PAGE_LOAD;
    rv = global->HandleDOMEvent(mPresContext, &event, nsnull,
                                NS_EVENT_FLAG_INIT, &status);
  } else {
    // XXX: Should fire error event to the document...
  }

  return rv;
}

NS_IMETHODIMP
DocumentViewerImpl::Destroy(void)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DocumentViewerImpl::Stop(void)
{
  if (mDocument) {
    mDocument->StopDocumentLoad();
  }

  if (mPresContext) {
    mPresContext->Stop();
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetDOMDocument(nsIDOMDocument **aResult)
{
  return CallQueryInterface(mDocument.get(), aResult);
}

NS_IMETHODIMP
DocumentViewerImpl::SetDOMDocument(nsIDOMDocument *aDocument)
{
  // Assumptions:
  //
  // 1) this document viewer has been initialized with a call to Init().
  // 2) the stylesheets associated with the document have been added to the document.

  // XXX Right now, this method assumes that the layout of the current document
  // hasn't started yet.  More cleanup will probably be necessary to make this
  // method work for the case when layout *has* occurred for the current document.
  // That work can happen when and if it is needed.
  
  nsresult rv;
  if (nsnull == aDocument)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDocument> newDoc = do_QueryInterface(aDocument, &rv);
  if (NS_FAILED(rv)) return rv;

  // 0) Replace the old document with the new one
  mDocument = newDoc;

  // 1) Set the script global object on the new document
  nsCOMPtr<nsIInterfaceRequestor> requestor(do_QueryInterface(mContainer));
  if (requestor) {
    nsCOMPtr<nsIScriptGlobalObjectOwner> owner;
    requestor->GetInterface(NS_GET_IID(nsIScriptGlobalObjectOwner),
       getter_AddRefs(owner));
    if (nsnull != owner) {
      nsCOMPtr<nsIScriptGlobalObject> global;
      rv = owner->GetScriptGlobalObject(getter_AddRefs(global));
      if (NS_SUCCEEDED(rv) && (nsnull != global)) {
        mDocument->SetScriptGlobalObject(global);
        global->SetNewDocument(aDocument);
      }
    }
  }  

  // 2) Create a new style set for the document
  nsCOMPtr<nsIStyleSet> styleSet;
  rv = CreateStyleSet(mDocument, getter_AddRefs(styleSet));
  if (NS_FAILED(rv)) return rv;

  // 3) Replace the current pres shell with a new shell for the new document 
  mPresShell->EndObservingDocument();

  mPresShell = nsnull;
  rv = newDoc->CreateShell(mPresContext, mViewManager, styleSet,
                           getter_AddRefs(mPresShell));
  if (NS_FAILED(rv)) return rv;

  mPresShell->BeginObservingDocument();

  // 4) Register the focus listener on the new document
  if(mDocument)
  {    
    nsCOMPtr<nsIDOMEventReceiver> erP;
    rv = mDocument->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), getter_AddRefs(erP));
    if(NS_FAILED(rv) || !erP)
      return rv ? rv : NS_ERROR_FAILURE;

    rv = erP->AddEventListenerByIID(mFocusListener, NS_GET_IID(nsIDOMFocusListener));
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register focus listener");
  }

  return rv;
}

NS_IMETHODIMP
DocumentViewerImpl::SetUAStyleSheet(nsIStyleSheet* aUAStyleSheet)
{
  mUAStyleSheet = dont_QueryInterface(aUAStyleSheet);
  return NS_OK;
}
  
NS_IMETHODIMP
DocumentViewerImpl::GetDocument(nsIDocument*& aResult)
{
  aResult = mDocument;
  NS_IF_ADDREF(aResult);
  return NS_OK;
}
  
NS_IMETHODIMP
DocumentViewerImpl::GetPresShell(nsIPresShell*& aResult)
{
  aResult = mPresShell;
  NS_IF_ADDREF(aResult);
  return NS_OK;
}
  
NS_IMETHODIMP
DocumentViewerImpl::GetPresContext(nsIPresContext*& aResult)
{
  aResult = mPresContext;
  NS_IF_ADDREF(aResult);
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetBounds(nsRect& aResult)
{
  NS_PRECONDITION(mWindow, "null window");
  if (mWindow) {
    mWindow->GetBounds(aResult);
  }
  else {
    aResult.SetRect(0, 0, 0, 0);
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::SetBounds(const nsRect& aBounds)
{
  NS_PRECONDITION(mWindow, "null window");
  if (mWindow) {
    // Don't have the widget repaint. Layout will generate repaint requests
    // during reflow
    mWindow->Resize(aBounds.x, aBounds.y, aBounds.width, aBounds.height,
                    PR_FALSE);
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Move(PRInt32 aX, PRInt32 aY)
{
  NS_PRECONDITION(mWindow, "null window");
  if (mWindow) {
    mWindow->Move(aX, aY);
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Show(void)
{
  NS_PRECONDITION(mWindow, "null window");
  if (mWindow) {
    mWindow->Show(PR_TRUE);
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Hide(void)
{
  NS_PRECONDITION(mWindow, "null window");
  if (mWindow) {
    mWindow->Show(PR_FALSE);
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::PrintContent(nsIWebShell  *aParent,nsIDeviceContext *aDContext)
{
  NS_ENSURE_ARG_POINTER(aParent);
  NS_ENSURE_ARG_POINTER(aDContext);

  nsCOMPtr<nsIStyleSet>       ss;
  nsCOMPtr<nsIViewManager>    vm;
  PRInt32                     width, height;
  nsIView                     *view;
  nsresult                    rv;
  PRInt32                     count,i;
  nsCOMPtr<nsIContentViewer>  viewer;
  nsCOMPtr<nsIDocShellTreeNode> parentAsNode(do_QueryInterface(aParent));

  parentAsNode->GetChildCount(&count);
  if(count> 0) { 
    for(i=0;i<count;i++) {
      nsCOMPtr<nsIDocShellTreeItem> child;
      parentAsNode->GetChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
      childAsShell->GetContentViewer(getter_AddRefs(viewer));
      nsCOMPtr<nsIContentViewerFile> viewerFile(do_QueryInterface(viewer));
      if (viewerFile) {
        nsCOMPtr<nsIWebShell> childWebShell(do_QueryInterface(child));
        NS_ENSURE_SUCCESS(viewerFile->PrintContent(childWebShell,aDContext), NS_ERROR_FAILURE);
      }
    }
  } else {
    aDContext->BeginDocument();
    aDContext->GetDeviceSurfaceDimensions(width, height);

    nsCOMPtr<nsIPresContext> cx;
    rv = NS_NewPrintContext(getter_AddRefs(cx));
    if (NS_FAILED(rv)) {
      return rv;
    }

    cx->Init(aDContext);

    CreateStyleSet(mDocument, getter_AddRefs(ss));

    nsCOMPtr<nsIPresShell> ps;
    rv = NS_NewPresShell(getter_AddRefs(ps));
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = nsComponentManager::CreateInstance(kViewManagerCID,
                                            nsnull,
                                            NS_GET_IID(nsIViewManager),
                                            (void **)getter_AddRefs(vm));
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = vm->Init(aDContext);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsRect tbounds = nsRect(0, 0, width, height);

    // Create a child window of the parent that is our "root view/window"
    rv = nsComponentManager::CreateInstance(kViewCID, nsnull, NS_GET_IID(nsIView), (void **)&view);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = view->Init(vm, tbounds, nsnull);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Setup hierarchical relationship in view manager
    vm->SetRootView(view);

    ps->Init(mDocument, cx, vm, ss);

    nsCompatibility mode;
    mPresContext->GetCompatibilityMode(&mode);
    cx->SetCompatibilityMode(mode);
    cx->SetContainer(aParent);

    //lay it out...
    ps->InitialReflow(width, height);

    // Ask the page sequence frame to print all the pages
    nsIPageSequenceFrame* pageSequence;
    nsPrintOptions        options;

    ps->GetPageSequenceFrame(&pageSequence);
    NS_ASSERTION(nsnull != pageSequence, "no page sequence frame");
    
 
    if (nsnull != mFilePointer) {
      // output the regression test
      nsIFrameDebug* fdbg;
      nsIFrame* root;
      ps->GetRootFrame(&root);

      if (NS_SUCCEEDED(root->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**) &fdbg))) {
        fdbg->DumpRegressionData(cx, mFilePointer, 0);
      }
      fclose(mFilePointer);      
    } else {
      pageSequence->Print(cx, options, nsnull);
    }

    aDContext->EndDocument();

    ps->EndObservingDocument();
  }
  return NS_OK;

}

void DocumentViewerImpl::Notify(nsIImageGroup *aImageGroup,
                                nsImageGroupNotification aNotificationType)
{
  //
  // Image are being loaded...  Set the flag to delay printing until
  // all images are loaded.
  //
  if (aNotificationType == nsImageGroupNotification_kStartedLoading) {
    mIsPrinting = PR_TRUE;
  }
  //
  // All the images have been loaded, so the document is ready to print.
  //
  // However, at this point we are unable to release the resources that
  // were allocated for printing...  This is because ImgLib resources will
  // be deleted and *this* is an ImgLib notification routine.  So, fire an 
  // event to do the actual printing.
  //
  else if(aNotificationType == nsImageGroupNotification_kFinishedLoading) {
    nsresult rv;
    nsCOMPtr<nsIEventQueue> eventQ;

    // Get the event queue of the current thread...
    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
    if (NS_FAILED(rv)) return;

    rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, 
                                            getter_AddRefs(eventQ));
    if (NS_FAILED(rv)) return;

    PRStatus status;
    PLEvent *event = new PLEvent;
  
    if (!event) return;

    //
    // AddRef this because it is being placed in the PLEvent struct.
    // It will be Released when DestroyPLEvent is called...
    //
    NS_ADDREF_THIS();
    PL_InitEvent(event, 
                 this,
                 (PLHandleEventProc)  DocumentViewerImpl::HandlePLEvent,
                 (PLDestroyEventProc) DocumentViewerImpl::DestroyPLEvent);

    status = eventQ->PostEvent(event);
  }
}


NS_IMETHODIMP
DocumentViewerImpl::SetEnableRendering(PRBool aOn)
{
  mEnableRendering = aOn;
  if (mViewManager) {
    if (aOn) {
      mViewManager->EnableRefresh(NS_VMREFRESH_IMMEDIATE);
      nsIView* view; 
      mViewManager->GetRootView(view);   // views are not refCounted 
      if (view) { 
        mViewManager->UpdateView(view, NS_VMREFRESH_IMMEDIATE);
      } 
    }
    else {
      mViewManager->DisableRefresh();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetEnableRendering(PRBool* aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null OUT ptr");
  if (aResult) {
    *aResult = mEnableRendering;
  }
  return NS_OK;
}

void
DocumentViewerImpl::ForceRefresh()
{
  mWindow->Invalidate(PR_TRUE);
}

nsresult
DocumentViewerImpl::CreateStyleSet(nsIDocument* aDocument,
                                   nsIStyleSet** aStyleSet)
{
  // this should eventually get expanded to allow for creating
  // different sets for different media
  nsresult rv;

  if (!mUAStyleSheet) {
    NS_WARNING("unable to load UA style sheet");
  }

  rv = NS_NewStyleSet(aStyleSet);
  if (NS_OK == rv) {
    PRInt32 index = aDocument->GetNumberOfStyleSheets();

    while (0 < index--) {
      nsCOMPtr<nsIStyleSheet> sheet(getter_AddRefs(aDocument->GetStyleSheetAt(index)));

      /*
       * GetStyleSheetAt will return all style sheets in the document but
       * we're only interested in the ones that are enabled.
       */

      PRBool styleEnabled;
      sheet->GetEnabled(styleEnabled);

      if (styleEnabled) {
        (*aStyleSet)->AddDocStyleSheet(sheet, aDocument);
      }
    }

    NS_WITH_SERVICE(nsIChromeRegistry, chromeRegistry, "@mozilla.org/chrome/chrome-registry;1", &rv);
    if (NS_SUCCEEDED(rv) && chromeRegistry) {
      nsCOMPtr<nsISupportsArray> sheets;
      chromeRegistry->GetBackstopSheets(getter_AddRefs(sheets));
      if(sheets){
        nsCOMPtr<nsICSSStyleSheet> sheet;
        PRUint32 count;
        sheets->Count(&count);
        for(PRUint32 i=0; i<count; i++) {
          sheets->GetElementAt(i, getter_AddRefs(sheet));
          (*aStyleSet)->AppendBackstopStyleSheet(sheet);
        }
      }

      // Now handle the user sheets.
      nsCOMPtr<nsIDocShellTreeItem> docShell(do_QueryInterface(mContainer));
      PRInt32 shellType;
      docShell->GetItemType(&shellType);
      PRBool isChrome = (shellType == nsIDocShellTreeItem::typeChrome);
      sheets = nsnull;
      chromeRegistry->GetUserSheets(isChrome, getter_AddRefs(sheets));
      if(sheets){
        nsCOMPtr<nsICSSStyleSheet> sheet;
        PRUint32 count;
        sheets->Count(&count);
        for(PRUint32 i=0; i<count; i++) {
          sheets->GetElementAt(i, getter_AddRefs(sheet));
          // XXX For now, append as backstop until we figure out something
          // better to do.
          (*aStyleSet)->AppendBackstopStyleSheet(sheet);
        }
      }
    }

    if (mUAStyleSheet) {
      (*aStyleSet)->AppendBackstopStyleSheet(mUAStyleSheet);
    }
  }
  return rv;
}

nsresult
DocumentViewerImpl::MakeWindow(nsIWidget* aParentWidget,
                               const nsRect& aBounds)
{
  nsresult rv;

  rv = nsComponentManager::CreateInstance(kViewManagerCID, 
                                          nsnull, 
                                          NS_GET_IID(nsIViewManager), 
                                          getter_AddRefs(mViewManager));

  nsCOMPtr<nsIDeviceContext> dx;
  mPresContext->GetDeviceContext(getter_AddRefs(dx));

 
  nsRect tbounds = aBounds;
  float p2t;
  mPresContext->GetPixelsToTwips(&p2t);
  tbounds *= p2t;

    // Initialize the view manager with an offset. This allows the viewmanager
    // to manage a coordinate space offset from (0,0)
   if ((NS_OK != rv) || (NS_OK != mViewManager->Init(dx, tbounds.x, tbounds.y))) {
    return rv;
  }
   // Reset the bounds offset so the root view is set to 0,0. The offset is
   // specified in nsIViewManager::Init above. 
   // Besides, layout will reset the root view to (0,0) during reflow, 
   // so changing it to 0,0 eliminates placing
   // the root view in the wrong place initially.
  tbounds.x = 0;
  tbounds.y = 0;

  // Create a child window of the parent that is our "root view/window"
  // Create a view
  rv = nsComponentManager::CreateInstance(kViewCID, 
                                          nsnull, 
                                          NS_GET_IID(nsIView), 
                                          (void**)&mView);
  if ((NS_OK != rv) || (NS_OK != mView->Init(mViewManager, 
                                             tbounds,
                                             nsnull))) {
    return rv;
  }

  rv = mView->CreateWidget(kWidgetCID, nsnull, aParentWidget->GetNativeData(NS_NATIVE_WIDGET));

  if (rv != NS_OK)
    return rv;

  // Setup hierarchical relationship in view manager
  mViewManager->SetRootView(mView);

  mView->GetWidget(*getter_AddRefs(mWindow));

  //set frame rate to 25 fps
  mViewManager->SetFrameRate(25);

  // This SetFocus is necessary so the Arrow Key and Page Key events
  // go to the scrolled view as soon as the Window is created instead of going to
  // the browser window (this enables keyboard scrolling of the document)
  // mWindow->SetFocus();

  return rv;
}

nsresult DocumentViewerImpl::GetDocumentSelection(nsISelection **aSelection)
{
  if (!aSelection) return NS_ERROR_NULL_POINTER;
  if (!mPresShell) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsISelectionController> selcon;
  selcon = do_QueryInterface(mPresShell);
  if (selcon) 
    return selcon->GetSelection(nsISelectionController::SELECTION_NORMAL, aSelection);  
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DocumentViewerImpl::CreateDocumentViewerUsing(nsIPresContext* aPresContext,
                                              nsIDocumentViewer*& aResult)
{
  if (!mDocument) {
    // XXX better error
    return NS_ERROR_NULL_POINTER;
  }
  if (nsnull == aPresContext) {
    return NS_ERROR_NULL_POINTER;
  }

  // Create new viewer
  DocumentViewerImpl* viewer = new DocumentViewerImpl(aPresContext);
  if (nsnull == viewer) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(viewer);

  // XXX make sure the ua style sheet is used (for now; need to be
  // able to specify an alternate)
  viewer->SetUAStyleSheet(mUAStyleSheet);

  // Bind the new viewer to the old document
  nsresult rv = viewer->BindToDocument(mDocument, "create");/* XXX verb? */

  aResult = viewer;

  return rv;
}



void PR_CALLBACK DocumentViewerImpl::HandlePLEvent(PLEvent* aEvent)
{
  DocumentViewerImpl *viewer;

  viewer = (DocumentViewerImpl*)PL_GetEventOwner(aEvent);

  NS_ASSERTION(viewer, "The event owner is null.");
  if (viewer) {
    viewer->DocumentReadyForPrinting();
  }
}

void PR_CALLBACK DocumentViewerImpl::DestroyPLEvent(PLEvent* aEvent)
{
  DocumentViewerImpl *viewer;

  viewer = (DocumentViewerImpl*)PL_GetEventOwner(aEvent);
  NS_IF_RELEASE(viewer);

  delete aEvent;
}


void DocumentViewerImpl::DocumentReadyForPrinting()
{
  nsCOMPtr<nsIWebShell> webContainer;

  webContainer = do_QueryInterface(mContainer);
  if(webContainer) {
    //
    // Remove ourselves as an image group observer...
    //
    nsCOMPtr<nsIImageGroup> imageGroup;
    mPrintPC->GetImageGroup(getter_AddRefs(imageGroup));
    if (imageGroup) {
      imageGroup->RemoveObserver(this);
    }
    //
    // Send the document to the printer...
    //
    nsresult rv = PrintContent(webContainer, mPrintDC);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "bad result from PrintConent");

    // printing is complete, clean up now
    mIsPrinting = PR_FALSE;

    mPrintPS->EndObservingDocument();
  
    if (mPrintListener)
      mPrintListener->OnEndPrinting(NS_OK);

    NS_RELEASE(mPrintPS);
    NS_RELEASE(mPrintVM);
    NS_RELEASE(mPrintSS);
    NS_RELEASE(mPrintDC);
  }
}

#ifdef MOZ_XSL
NS_IMETHODIMP 
DocumentViewerImpl::SetTransformMediator(nsITransformMediator* aMediator)
{
  NS_ASSERTION(nsnull == mTransformMediator, "nsXMLDocument::SetTransformMediator(): \
    Cannot set a second transform mediator\n");
  mTransformMediator = aMediator;
  return NS_OK;
}
#endif

#ifdef XP_MAC
#pragma mark -
#endif

/* ========================================================================================
 * nsIContentViewerEdit
 * ======================================================================================== */

NS_IMETHODIMP DocumentViewerImpl::Search()
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentViewerImpl::GetSearchable(PRBool *aSearchable)
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentViewerImpl::ClearSelection()
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentViewerImpl::SelectAll()
{
  // XXX this is a temporary implementation copied from nsWebShell
  // for now. I think nsDocument and friends should have some helper
  // functions to make this easier.
  nsCOMPtr<nsISelection> selection;
  nsresult rv;
  rv = GetDocumentSelection(getter_AddRefs(selection));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMHTMLDocument> htmldoc = do_QueryInterface(mDocument);
  nsCOMPtr<nsIDOMNode> bodyNode;
  
  if (htmldoc)
  {
    nsCOMPtr<nsIDOMHTMLElement>bodyElement;
    rv = htmldoc->GetBody(getter_AddRefs(bodyElement));
    if (NS_FAILED(rv) || !bodyElement) return rv;

    bodyNode = do_QueryInterface(bodyElement);
  }
  else if (mDocument)
  {
    nsCOMPtr<nsIContent> rootContent = getter_AddRefs(mDocument->GetRootContent());
    bodyNode = do_QueryInterface(rootContent);
  }
  if (!bodyNode) return NS_ERROR_FAILURE; 
  
  rv = selection->RemoveAllRanges();
  if (NS_FAILED(rv)) return rv;

  static NS_DEFINE_CID(kCDOMRangeCID,           NS_RANGE_CID);
  nsCOMPtr<nsIDOMRange> range;
  rv = nsComponentManager::CreateInstance(kCDOMRangeCID, nsnull,
                                          NS_GET_IID(nsIDOMRange),
                                          getter_AddRefs(range));

  rv = range->SelectNodeContents(bodyNode);
  if (NS_FAILED(rv)) return rv;

  rv = selection->AddRange(range);
  return rv;
}

NS_IMETHODIMP DocumentViewerImpl::CopySelection()
{
  if (!mPresShell) return NS_ERROR_NOT_INITIALIZED;
  return mPresShell->DoCopy();
}

NS_IMETHODIMP DocumentViewerImpl::GetCopyable(PRBool *aCopyable)
{
  nsCOMPtr<nsISelection> selection;
  nsresult rv;
  rv = GetDocumentSelection(getter_AddRefs(selection));
  if (NS_FAILED(rv)) return rv;
  
  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);
  
  *aCopyable = !isCollapsed;
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::CutSelection()
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentViewerImpl::GetCutable(PRBool *aCutable)
{
  *aCutable = PR_FALSE;  // mm, will this ever be called for an editable document?
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::Paste()
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentViewerImpl::GetPasteable(PRBool *aPasteable)
{
  *aPasteable = PR_FALSE;
  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#endif

/* ========================================================================================
 * nsIContentViewerFile
 * ======================================================================================== */
NS_IMETHODIMP
DocumentViewerImpl::Save()
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
DocumentViewerImpl::GetSaveable(PRBool *aSaveable)
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

static NS_DEFINE_IID(kIDeviceContextSpecFactoryIID, NS_IDEVICE_CONTEXT_SPEC_FACTORY_IID);
static NS_DEFINE_IID(kDeviceContextSpecFactoryCID, NS_DEVICE_CONTEXT_SPEC_FACTORY_CID);


/** ---------------------------------------------------
 *  See documentation above in the nsIContentViewerfile class definition
 *	@update 01/24/00 dwc
 */
NS_IMETHODIMP
DocumentViewerImpl::Print(PRBool aSilent,FILE *aFile, nsIPrintListener *aPrintListener)
{
nsCOMPtr<nsIWebShell>                 webContainer;
nsCOMPtr<nsIDeviceContextSpecFactory> factory;
PRInt32                               width,height;

  nsComponentManager::CreateInstance(kDeviceContextSpecFactoryCID, 
                                     nsnull,
                                     kIDeviceContextSpecFactoryIID,
                                     (void **)getter_AddRefs(factory));

  if (factory) {

#ifdef DEBUG_dcone
    printf("PRINT JOB STARTING\n");
#endif

    nsIDeviceContextSpec *devspec = nsnull;
    nsCOMPtr<nsIDeviceContext> dx;
    mPrintDC = nsnull;
    mFilePointer = aFile;

    factory->CreateDeviceContextSpec(nsnull, devspec, aSilent);
    if (nsnull != devspec) {
      mPresContext->GetDeviceContext(getter_AddRefs(dx));
      nsresult rv = dx->GetDeviceContextFor(devspec, mPrintDC); 
      if (NS_SUCCEEDED(rv)) {

        NS_RELEASE(devspec);

        // Get the webshell for this documentviewer
        webContainer = do_QueryInterface(mContainer);
        if(webContainer) {
          // load the document and do the initial reflow on the entire document
          rv = NS_NewPrintContext(&mPrintPC);
          if(NS_FAILED(rv)){
            return rv;
          }

          mPrintDC->GetDeviceSurfaceDimensions(width,height);
          mPrintPC->Init(mPrintDC);
          mPrintPC->SetContainer(webContainer);
          CreateStyleSet(mDocument,&mPrintSS);

          rv = NS_NewPresShell(&mPrintPS);
          if(NS_FAILED(rv)){
            return rv;
          }
          
          rv = nsComponentManager::CreateInstance(kViewManagerCID, nsnull, NS_GET_IID(nsIViewManager),(void**)&mPrintVM);
          if(NS_FAILED(rv)) {
            return rv;
          }

          rv = mPrintVM->Init(mPrintDC);
          if(NS_FAILED(rv)) {
            return rv;
          }

          rv = nsComponentManager::CreateInstance(kViewCID, nsnull, NS_GET_IID(nsIView),(void**)&mPrintView);
          if(NS_FAILED(rv)) {
            return rv;
          }
          
          nsRect  tbounds = nsRect(0,0,width,height);
          rv = mPrintView->Init(mPrintVM,tbounds,nsnull);
          if(NS_FAILED(rv)) {
            return rv;
          }

          // setup hierarchical relationship in view manager
          mPrintVM->SetRootView(mPrintView);
          mPrintPS->Init(mDocument,mPrintPC,mPrintVM,mPrintSS);

          nsCOMPtr<nsIImageGroup> imageGroup;
          mPrintPC->GetImageGroup(getter_AddRefs(imageGroup));
          if (imageGroup) {
            imageGroup->AddObserver(this);
          }

          mPrintPS->InitialReflow(width,height);

#ifdef DEBUG_dcone
          float   a1,a2;
          PRInt32 i1,i2;

          printf("CRITICAL PRINTING INFORMATION\n");
          printf("PRESSHELL(%x)  PRESCONTEXT(%x)\nVIEWMANAGER(%x) VIEW(%x)\n",
              mPrintPS, mPrintPC,mPrintDC,mPrintVM,mPrintView);
          
          // DEVICE CONTEXT INFORMATION from PresContext
          printf("DeviceContext of Presentation Context(%x)\n",dx);
          dx->GetDevUnitsToTwips(a1);
          dx->GetTwipsToDevUnits(a2);
          printf("    DevToTwips = %f TwipToDev = %f\n",a1,a2);
          dx->GetAppUnitsToDevUnits(a1);
          dx->GetDevUnitsToAppUnits(a2);
          printf("    AppUnitsToDev = %f DevUnitsToApp = %f\n",a1,a2);
          dx->GetCanonicalPixelScale(a1);
          printf("    GetCanonicalPixelScale = %f\n",a1);
          dx->GetScrollBarDimensions(a1, a2);
          printf("    ScrollBar x = %f y = %f\n",a1,a2);
          dx->GetZoom(a1);
          printf("    Zoom = %f\n",a1);
          dx->GetDepth((PRUint32&)i1);
          printf("    Depth = %d\n",i1);
          dx->GetDeviceSurfaceDimensions(i1,i2);
          printf("    DeviceDimension w = %d h = %d\n",i1,i2);


          // DEVICE CONTEXT INFORMATION
          printf("DeviceContext created for print(%x)\n",mPrintDC);
          mPrintDC->GetDevUnitsToTwips(a1);
          mPrintDC->GetTwipsToDevUnits(a2);
          printf("    DevToTwips = %f TwipToDev = %f\n",a1,a2);
          mPrintDC->GetAppUnitsToDevUnits(a1);
          mPrintDC->GetDevUnitsToAppUnits(a2);
          printf("    AppUnitsToDev = %f DevUnitsToApp = %f\n",a1,a2);
          mPrintDC->GetCanonicalPixelScale(a1);
          printf("    GetCanonicalPixelScale = %f\n",a1);
          mPrintDC->GetScrollBarDimensions(a1, a2);
          printf("    ScrollBar x = %f y = %f\n",a1,a2);
          mPrintDC->GetZoom(a1);
          printf("    Zoom = %f\n",a1);
          mPrintDC->GetDepth((PRUint32&)i1);
          printf("    Depth = %d\n",i1);
          mPrintDC->GetDeviceSurfaceDimensions(i1,i2);
          printf("    DeviceDimension w = %d h = %d\n",i1,i2);

#endif
          // Print listener setup...
          if (aPrintListener)
          {
            mPrintListener = aPrintListener;
            mPrintListener->OnStartPrinting();    
          /* RICHIE mPrintListener->OnProgressPrinting(PRUint32 aProgress, PRUint32 aProgressMax); */
          }

          //
          // The mIsPrinting flag is set when the ImageGroup observer is
          // notified that images must be loaded as a result of the 
          // InitialReflow...
          //
          if(!mIsPrinting){
            DocumentReadyForPrinting();
#ifdef DEBUG_dcone
            printf("PRINT JOB ENDING, OBSERVER WAS NOT CALLED\n");
#endif
          } else {
            // use the observer mechanism to finish the printing
#ifdef DEBUG_dcone
            printf("PRINTING OBSERVER STARTED\n");
#endif
          }
        }
      }      
    }
    else
    {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
DocumentViewerImpl::GetPrintable(PRBool *aPrintable) 
{
  NS_ENSURE_ARG_POINTER(aPrintable);

  *aPrintable = PR_TRUE;
  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#endif

//*****************************************************************************
// nsIMarkupDocumentViewer
//*****************************************************************************   

NS_IMETHODIMP DocumentViewerImpl::ScrollToNode(nsIDOMNode* aNode)
{
   NS_ENSURE_ARG(aNode);
   nsCOMPtr<nsIPresShell> presShell;
   NS_ENSURE_SUCCESS(GetPresShell(*(getter_AddRefs(presShell))), NS_ERROR_FAILURE);

   // Get the nsIContent interface, because that's what we need to 
   // get the primary frame
   
   nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
   NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

   // Get the primary frame
   nsIFrame* frame;  // Remember Frames aren't ref-counted.  They are in their 
                     // own special little world.

   NS_ENSURE_SUCCESS(presShell->GetPrimaryFrameFor(content, &frame),
      NS_ERROR_FAILURE);

   // tell the pres shell to scroll to the frame
   NS_ENSURE_SUCCESS(presShell->ScrollFrameIntoView(frame, 
                                                    NS_PRESSHELL_SCROLL_TOP, 
                                                    NS_PRESSHELL_SCROLL_ANYWHERE), 
                     NS_ERROR_FAILURE); 
   return NS_OK; 
}

NS_IMETHODIMP DocumentViewerImpl::GetAllowPlugins(PRBool* aAllowPlugins)
{
   NS_ENSURE_ARG_POINTER(aAllowPlugins);

   *aAllowPlugins = mAllowPlugins;
   return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SetAllowPlugins(PRBool aAllowPlugins)
{
   mAllowPlugins = aAllowPlugins;
   return NS_OK;
}

nsresult
DocumentViewerImpl::CallChildren(CallChildFunc aFunc, void* aClosure)
{
  nsCOMPtr<nsIDocShellTreeNode> docShellNode(do_QueryInterface(mContainer));
  if (docShellNode)
  {
    PRInt32 i;
    PRInt32 n;
    docShellNode->GetChildCount(&n);
    for (i=0; i < n; i++) 
    {
      nsCOMPtr<nsIDocShellTreeItem> child;
      docShellNode->GetChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
      NS_ASSERTION(childAsShell, "null child in docshell");
      if (childAsShell) 
      {
        nsCOMPtr<nsIContentViewer> childCV;
        childAsShell->GetContentViewer(getter_AddRefs(childCV));
        if (childCV) 
        {
          nsCOMPtr<nsIMarkupDocumentViewer> markupCV = do_QueryInterface(childCV);
          if (markupCV) {
            (*aFunc)(markupCV, aClosure);
          }
        }
      }
    }
  }
  return NS_OK;
}

struct TextZoomInfo
{
  float mTextZoom;
};

static void
SetChildTextZoom(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  struct TextZoomInfo* textZoomInfo = (struct TextZoomInfo*) aClosure;
  aChild->SetTextZoom(textZoomInfo->mTextZoom);
}

NS_IMETHODIMP DocumentViewerImpl::SetTextZoom(float aTextZoom)
{
  if (mDeviceContext) {
    mDeviceContext->SetTextZoom(aTextZoom);
    if (mPresContext) {
      mPresContext->RemapStyleAndReflow();
    }
  }

  // now set the text zoom on all children of mContainer
  struct TextZoomInfo textZoomInfo = { aTextZoom };
  return CallChildren(SetChildTextZoom, &textZoomInfo);
}

NS_IMETHODIMP DocumentViewerImpl::GetTextZoom(float* aTextZoom)
{
  NS_ENSURE_ARG_POINTER(aTextZoom);

  if (mDeviceContext) {
    return mDeviceContext->GetTextZoom(*aTextZoom);
  }

  *aTextZoom = 1.0;
  return NS_OK;
}

// XXX: SEMANTIC CHANGE! 
//      returns a copy of the string.  Caller is responsible for freeing result
//      using Recycle(aDefaultCharacterSet)
NS_IMETHODIMP DocumentViewerImpl::GetDefaultCharacterSet(PRUnichar** aDefaultCharacterSet)
{
  NS_ENSURE_ARG_POINTER(aDefaultCharacterSet);
  NS_ENSURE_STATE(mContainer);

  static PRUnichar *gDefCharset = nsnull;    // XXX: memory leak!

  if (0 == mDefaultCharacterSet.Length()) 
  {
    if ((nsnull == gDefCharset) || (nsnull == *gDefCharset)) 
    {
      nsCOMPtr<nsIWebShell> webShell;
      webShell = do_QueryInterface(mContainer);
      if (webShell)
      {
        nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
        if(prefs)
          prefs->GetLocalizedUnicharPref("intl.charset.default", &gDefCharset);
      }
    }
    if ((nsnull == gDefCharset) || (nsnull == *gDefCharset))
      mDefaultCharacterSet.AssignWithConversion("ISO-8859-1");
    else
      mDefaultCharacterSet.Assign(gDefCharset);
  }
  *aDefaultCharacterSet = mDefaultCharacterSet.ToNewUnicode();
  return NS_OK;
}

static void
SetChildDefaultCharacterSet(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  aChild->SetDefaultCharacterSet((PRUnichar*) aClosure);
}

NS_IMETHODIMP DocumentViewerImpl::SetDefaultCharacterSet(const PRUnichar* aDefaultCharacterSet)
{
  mDefaultCharacterSet = aDefaultCharacterSet;  // this does a copy of aDefaultCharacterSet
  // now set the default char set on all children of mContainer
  return CallChildren(SetChildDefaultCharacterSet,
                      (void*) aDefaultCharacterSet);
}

// XXX: SEMANTIC CHANGE! 
//      returns a copy of the string.  Caller is responsible for freeing result
//      using Recycle(aForceCharacterSet)
NS_IMETHODIMP DocumentViewerImpl::GetForceCharacterSet(PRUnichar** aForceCharacterSet)
{
  NS_ENSURE_ARG_POINTER(aForceCharacterSet);

  nsAutoString emptyStr;
  if (mForceCharacterSet.Equals(emptyStr)) {
    *aForceCharacterSet = nsnull;
  }
  else {
    *aForceCharacterSet = mForceCharacterSet.ToNewUnicode();
  }
  return NS_OK;
}

static void
SetChildForceCharacterSet(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  aChild->SetForceCharacterSet((PRUnichar*) aClosure);
}

NS_IMETHODIMP DocumentViewerImpl::SetForceCharacterSet(const PRUnichar* aForceCharacterSet)
{
  mForceCharacterSet = aForceCharacterSet;
  // now set the force char set on all children of mContainer
  return CallChildren(SetChildForceCharacterSet, (void*) aForceCharacterSet);
}

// XXX: SEMANTIC CHANGE! 
//      returns a copy of the string.  Caller is responsible for freeing result
//      using Recycle(aHintCharacterSet)
NS_IMETHODIMP DocumentViewerImpl::GetHintCharacterSet(PRUnichar * *aHintCharacterSet)
{
  NS_ENSURE_ARG_POINTER(aHintCharacterSet);

  if(kCharsetUninitialized == mHintCharsetSource) {
    *aHintCharacterSet = nsnull;
  } else {
    *aHintCharacterSet = mHintCharset.ToNewUnicode();
    // this can't possibly be right.  we can't set a value just because somebody got a related value!
    //mHintCharsetSource = kCharsetUninitialized;
  }
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetHintCharacterSetSource(PRInt32 *aHintCharacterSetSource)
{
  NS_ENSURE_ARG_POINTER(aHintCharacterSetSource);

  *aHintCharacterSetSource = mHintCharsetSource;
  return NS_OK;
}

static void
SetChildHintCharacterSetSource(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  aChild->SetHintCharacterSetSource((PRInt32) aClosure);
}

NS_IMETHODIMP DocumentViewerImpl::SetHintCharacterSetSource(PRInt32 aHintCharacterSetSource)
{
  mHintCharsetSource = (nsCharsetSource)aHintCharacterSetSource;
  // now set the hint char set source on all children of mContainer
  return CallChildren(SetChildHintCharacterSetSource,
                      (void*) aHintCharacterSetSource);
}

static void
SetChildHintCharacterSet(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  aChild->SetHintCharacterSet((PRUnichar*) aClosure);
}

NS_IMETHODIMP DocumentViewerImpl::SetHintCharacterSet(const PRUnichar* aHintCharacterSet)
{
  mHintCharset = aHintCharacterSet;
  // now set the hint char set on all children of mContainer
  return CallChildren(SetChildHintCharacterSet, (void*) aHintCharacterSet);
}

NS_IMETHODIMP DocumentViewerImpl::SizeToContent()
{
   nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mContainer));
   NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

   nsCOMPtr<nsIDocShellTreeItem> docShellParent;
   docShellAsItem->GetSameTypeParent(getter_AddRefs(docShellParent));

   // It's only valid to access this from a top frame.  Doesn't work from 
   // sub-frames.
   NS_ENSURE_TRUE(!docShellParent, NS_ERROR_FAILURE);

   nsCOMPtr<nsIPresShell> presShell;
   GetPresShell(*getter_AddRefs(presShell));
   NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(presShell->ResizeReflow(NS_UNCONSTRAINEDSIZE,
      NS_UNCONSTRAINEDSIZE), NS_ERROR_FAILURE);

   nsCOMPtr<nsIPresContext> presContext;
   GetPresContext(*getter_AddRefs(presContext));
   NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

   nsRect  shellArea;
   PRInt32 width, height;
   float   pixelScale;

   // so how big is it?
   presContext->GetVisibleArea(shellArea);
   presContext->GetTwipsToPixels(&pixelScale);
   width = PRInt32((float)shellArea.width*pixelScale);
   height = PRInt32((float)shellArea.height*pixelScale);

   nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
   docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
   NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(treeOwner->SizeShellTo(docShellAsItem, width, height),
      NS_ERROR_FAILURE);

   return NS_OK;
}



#ifdef XP_MAC
#pragma mark -
#endif

NS_IMPL_ISUPPORTS(nsDocViwerSelectionListener, NS_GET_IID(nsISelectionListener));

nsresult nsDocViwerSelectionListener::Init(DocumentViewerImpl *aDocViewer)
{
  mDocViewer = aDocViewer;
  return NS_OK;
}


NS_IMETHODIMP nsDocViwerSelectionListener::NotifySelectionChanged(nsIDOMDocument *, nsISelection *, short)
{
  NS_ASSERTION(mDocViewer, "Should have doc viewer!");

  // get the selection state
  nsCOMPtr<nsISelection> selection;
  nsresult rv = mDocViewer->GetDocumentSelection(getter_AddRefs(selection));
  if (NS_FAILED(rv)) return rv;

  PRBool selectionCollapsed;
  selection->GetIsCollapsed(&selectionCollapsed);
  // we only call UpdateCommands when the selection changes from collapsed
  // to non-collapsed or vice versa. We might need another update string
  // for simple selection changes, but that would be expenseive.
  if (!mGotSelectionState || mSelectionWasCollapsed != selectionCollapsed)
  {
    nsCOMPtr<nsIDocument> theDoc;
    mDocViewer->GetDocument(*getter_AddRefs(theDoc));  
    if (!theDoc) return NS_ERROR_FAILURE;
    
    nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject;
    theDoc->GetScriptGlobalObject(getter_AddRefs(scriptGlobalObject));

    nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(scriptGlobalObject);
    if (!domWindow) return NS_ERROR_FAILURE;
    
    domWindow->UpdateCommands(NS_ConvertToString("select"));
    mGotSelectionState = PR_TRUE;
    mSelectionWasCollapsed = selectionCollapsed;
  }  
  
  return NS_OK;
}

//nsDocViewerFocusListener
NS_IMPL_ISUPPORTS(nsDocViewerFocusListener, NS_GET_IID(nsIDOMFocusListener));

nsDocViewerFocusListener::nsDocViewerFocusListener()
:mDocViewer(nsnull)
{
  NS_INIT_REFCNT();
}

nsDocViewerFocusListener::~nsDocViewerFocusListener(){}

nsresult
nsDocViewerFocusListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

nsresult
nsDocViewerFocusListener::Focus(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIPresShell> shell;
  if(!mDocViewer)
    return NS_ERROR_FAILURE;

  nsresult result = mDocViewer->GetPresShell(*getter_AddRefs(shell));//deref once cause it take a ptr ref
  if(NS_FAILED(result) || !shell)
    return result?result:NS_ERROR_FAILURE;
  nsCOMPtr<nsISelectionController> selCon;
  selCon = do_QueryInterface(shell);
  PRInt16 selectionStatus;
  selCon->GetDisplaySelection( &selectionStatus);

  //if selection was nsISelectionController::SELECTION_OFF, do nothing
  //otherwise re-enable it. 
  if(selectionStatus == nsISelectionController::SELECTION_DISABLED)
  {
    selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
    selCon->RepaintSelection(nsISelectionController::SELECTION_NORMAL);
  }
  return result;
}
 
nsresult
nsDocViewerFocusListener::Blur(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIPresShell> shell;
  if(!mDocViewer) 
    return NS_ERROR_FAILURE;

  nsresult result = mDocViewer->GetPresShell(*getter_AddRefs(shell));//deref once cause it take a ptr ref
  if(NS_FAILED(result) || !shell)
    return result?result:NS_ERROR_FAILURE;
  nsCOMPtr<nsISelectionController> selCon;
  selCon = do_QueryInterface(shell);
  PRInt16 selectionStatus;
  selCon->GetDisplaySelection(&selectionStatus);
 
  //if selection was nsISelectionController::SELECTION_OFF, do nothing
  //otherwise re-enable it.
  if(selectionStatus == nsISelectionController::SELECTION_ON)
  {
    selCon->SetDisplaySelection(nsISelectionController::SELECTION_DISABLED);
    selCon->RepaintSelection(nsISelectionController::SELECTION_NORMAL);
  }
  return result;
}


nsresult
nsDocViewerFocusListener::Init(DocumentViewerImpl *aDocViewer)
{
  mDocViewer = aDocViewer;
  return NS_OK;
}


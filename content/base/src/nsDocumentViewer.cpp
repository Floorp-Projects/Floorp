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

#include "nsIImageGroup.h"
#include "nsIImageObserver.h"

#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsIFrame.h"

#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsILinkHandler.h"
#include "nsIDOMDocument.h"

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


#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"

static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);

#ifdef NS_DEBUG
#undef NOISY_VIEWER
#else
#undef NOISY_VIEWER
#endif

class DocumentViewerImpl : public nsIDocumentViewer,
                           public nsIContentViewerEdit,
                           public nsIContentViewerFile,
                           public nsIMarkupDocumentViewer,
                           public nsIImageGroupObserver
{
public:
  DocumentViewerImpl();
  DocumentViewerImpl(nsIPresContext* aPresContext);
  
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIContentViewer interface...
  NS_IMETHOD Init(nsNativeWidget aParent,
                  nsIDeviceContext* aDeviceContext,
                  nsIPref* aPrefs,
                  const nsRect& aBounds,
                  nsScrollPreference aScrolling = nsScrollPreference_kAuto);
  NS_IMETHOD BindToDocument(nsISupports* aDoc, const char* aCommand);
  NS_IMETHOD SetContainer(nsISupports* aContainer);
  NS_IMETHOD GetContainer(nsISupports** aContainerResult);
  NS_IMETHOD Stop(void);
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

  // nsIContentViewerEdit
  NS_DECL_NSICONTENTVIEWEREDIT

  // nsIContentViewerFile
  NS_DECL_NSICONTENTVIEWERFILE

  // nsIMarkupDocumentViewer
  NS_DECL_NSIMARKUPDOCUMENTVIEWER

  // nsIImageGroupObserver interface...
  virtual void Notify(nsIImageGroup *aImageGroup,
                      nsImageGroupNotification aNotificationType);

protected:
  virtual ~DocumentViewerImpl();

private:
  void ForceRefresh(void);
  nsresult CreateStyleSet(nsIDocument* aDocument, nsIStyleSet** aStyleSet);
  nsresult MakeWindow(nsNativeWidget aNativeParent,
                      const nsRect& aBounds,
                      nsScrollPreference aScrolling);

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
  nsCOMPtr<nsIDocument>    mDocument;
  nsCOMPtr<nsIWidget>      mWindow;      // ??? should we really own it?
  nsCOMPtr<nsIViewManager> mViewManager;
  nsCOMPtr<nsIPresContext> mPresContext;
  nsCOMPtr<nsIPresShell>   mPresShell;

  nsCOMPtr<nsIStyleSheet>  mUAStyleSheet;

  PRBool mEnableRendering;
  PRInt16 mNumURLStarts;
  PRBool  mIsPrinting;


  // printing members
  nsIDeviceContext  *mPrintDC;
  nsIPresContext    *mPrintPC;
  nsIStyleSet       *mPrintSS;
  nsIPresShell      *mPrintPS;
  nsIViewManager    *mPrintVM;
  nsIView           *mPrintView;

  // document management data
  //   these items are specific to markup documents (html and xml)
  //   may consider splitting these out into a subclass
  PRBool   mAllowPlugins;
  PRPackedBool mIsFrame;
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

// Interface IDs
static NS_DEFINE_IID(kIScriptContextOwnerIID, NS_ISCRIPTCONTEXTOWNER_IID);
static NS_DEFINE_IID(kISupportsIID,         NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDocumentIID,         NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMDocumentIID,      NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIViewManagerIID,      NS_IVIEWMANAGER_IID);
static NS_DEFINE_IID(kIViewIID,             NS_IVIEW_IID);
static NS_DEFINE_IID(kScrollViewIID,        NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kIContentViewerIID,    NS_ICONTENT_VIEWER_IID);
static NS_DEFINE_IID(kIDocumentViewerIID,   NS_IDOCUMENT_VIEWER_IID);
static NS_DEFINE_IID(kILinkHandlerIID,      NS_ILINKHANDLER_IID);

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
  return it->QueryInterface(kIDocumentViewerIID, (void**) aResult);
}

// Note: operator new zeros our memory
DocumentViewerImpl::DocumentViewerImpl()
{
  NS_INIT_REFCNT();
  mEnableRendering = PR_TRUE;

}

DocumentViewerImpl::DocumentViewerImpl(nsIPresContext* aPresContext)
  : mPresContext(dont_QueryInterface(aPresContext))
{
  NS_INIT_REFCNT();
  mHintCharset = "";
  mHintCharsetSource = kCharsetUninitialized;
  mForceCharacterSet = "";
  mAllowPlugins = PR_TRUE;
  mIsFrame = PR_FALSE;
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

  if (aIID.Equals(kIContentViewerIID)) {
    nsIContentViewer* tmp = this;
    *aInstancePtr = (void*)tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDocumentViewerIID)) {
    nsIDocumentViewer* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIMarkupDocumentViewer::GetIID())) {
    nsIMarkupDocumentViewer* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
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
  if (mDocument) {
    // Break global object circular reference on the document created
    // in the DocViewer Init
    nsIScriptContextOwner *mOwner = mDocument->GetScriptContextOwner();
    if (nsnull != mOwner) {
      nsIScriptGlobalObject *mGlobal;
      mOwner->GetScriptGlobalObject(&mGlobal);
      if (nsnull != mGlobal) {
        mGlobal->SetNewDocument(nsnull);
        NS_RELEASE(mGlobal);
      }
      NS_RELEASE(mOwner);

      // out of band cleanup of webshell
      mDocument->SetScriptContextOwner(nsnull);
    }
  }

  if (mDeviceContext)
    mDeviceContext->FlushFontCache();

  if (mPresShell) {
    // Break circular reference (or something)
    mPresShell->EndObservingDocument();
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
DocumentViewerImpl::Init(nsNativeWidget aNativeParent,
                         nsIDeviceContext* aDeviceContext,
                         nsIPref* aPrefs,
                         const nsRect& aBounds,
                         nsScrollPreference aScrolling)
{
  nsresult rv;

  if (!mDocument) {
    return NS_ERROR_NULL_POINTER;
  }

  mDeviceContext = dont_QueryInterface(aDeviceContext);

  PRBool makeCX = PR_FALSE;
  if (!mPresContext) {
    // Create presentation context
    rv = NS_NewGalleyContext(getter_AddRefs(mPresContext));
    if (NS_OK != rv) {
      return rv;
    }

    mPresContext->Init(aDeviceContext, aPrefs); 
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
    nsCOMPtr<nsIScriptContextOwner> owner;
    requestor->GetInterface(NS_GET_IID(nsIScriptContextOwner),
       getter_AddRefs(owner));
    if (nsnull != owner) {
      mDocument->SetScriptContextOwner(owner);
      nsCOMPtr<nsIScriptGlobalObject> global;
      rv = owner->GetScriptGlobalObject(getter_AddRefs(global));
      if (NS_SUCCEEDED(rv) && (nsnull != global)) {
        nsCOMPtr<nsIDOMDocument> domdoc(do_QueryInterface(mDocument));
        if (nsnull != domdoc) {
          global->SetNewDocument(domdoc);
        }
      }
    }
  }

  // Create the ViewManager and Root View...
  MakeWindow(aNativeParent, aBounds, aScrolling);

  // Create the style set...
  nsIStyleSet* styleSet;
  rv = CreateStyleSet(mDocument, &styleSet);
  if (NS_OK == rv) {
    // Now make the shell for the document
    rv = mDocument->CreateShell(mPresContext, mViewManager, styleSet,
                                getter_AddRefs(mPresShell));
    NS_RELEASE(styleSet);
    if (NS_OK == rv) {
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
        mPresShell->BeginObservingDocument();

//XXX I don't think this should be done *here*; and why paint nothing
//(which turns out to cause black flashes!)???
        // Resize-reflow this time
        mPresShell->InitialReflow(width, height);

        // Now trigger a refresh
        if (mEnableRendering) {
          mViewManager->EnableRefresh();
        }
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
DocumentViewerImpl::Stop(void)
{
  if (mPresContext) {
    mPresContext->Stop();
  }
  return NS_OK;
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
  nsCOMPtr<nsIPref>           prefs;
  nsCOMPtr<nsIViewManager>    vm;
  PRInt32                     width, height;
  nsIView                     *view;
  nsresult                    rv;
  PRInt32                     count,i;
  nsCOMPtr<nsIWebShell>       childWebShell;
  nsCOMPtr<nsIContentViewer>  viewer;

  aParent->GetChildCount(count);
  if(count> 0) { 
    for(i=0;i<count;i++) {
      aParent->ChildAt(i, *(getter_AddRefs(childWebShell)));
      childWebShell->GetContentViewer(getter_AddRefs(viewer));
      nsCOMPtr<nsIContentViewerFile> viewerFile = do_QueryInterface(viewer);
      if (viewerFile) {
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

    mPresContext->GetPrefs(getter_AddRefs(prefs));
    cx->Init(aDContext, prefs);

    nsCompatibility mode;
    mPresContext->GetCompatibilityMode(&mode);
    cx->SetCompatibilityMode(mode);

    CreateStyleSet(mDocument, getter_AddRefs(ss));

    nsCOMPtr<nsIPresShell> ps;
    rv = NS_NewPresShell(getter_AddRefs(ps));
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = nsComponentManager::CreateInstance(kViewManagerCID,
                                            nsnull,
                                            kIViewManagerIID,
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
    rv = nsComponentManager::CreateInstance(kViewCID,nsnull,kIViewIID,(void **)&view);
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

    //lay it out...
    //aDContext->BeginDocument();
    ps->InitialReflow(width, height);

    // Ask the page sequence frame to print all the pages
    nsIPageSequenceFrame* pageSequence;
    nsPrintOptions        options;

    ps->GetPageSequenceFrame(&pageSequence);
    NS_ASSERTION(nsnull != pageSequence, "no page sequence frame");
    pageSequence->Print(*cx, options, nsnull);
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

    rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), 
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
      mViewManager->EnableRefresh();
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
    if (mUAStyleSheet) {
      (*aStyleSet)->AppendBackstopStyleSheet(mUAStyleSheet);
    }
  }
  return rv;
}

nsresult
DocumentViewerImpl::MakeWindow(nsNativeWidget aNativeParent,
                               const nsRect& aBounds,
                               nsScrollPreference aScrolling)
{
  nsresult rv;

  rv = nsComponentManager::CreateInstance(kViewManagerCID, 
                                          nsnull, 
                                          kIViewManagerIID, 
                                          getter_AddRefs(mViewManager));

  nsCOMPtr<nsIDeviceContext> dx;
  mPresContext->GetDeviceContext(getter_AddRefs(dx));

  if ((NS_OK != rv) || (NS_OK != mViewManager->Init(dx))) {
    return rv;
  }

  nsRect tbounds = aBounds;
  float p2t;
  mPresContext->GetPixelsToTwips(&p2t);
  tbounds *= p2t;

  // Create a child window of the parent that is our "root view/window"
  // Create a view
  rv = nsComponentManager::CreateInstance(kViewCID, 
                                          nsnull, 
                                          kIViewIID, 
                                          (void**)&mView);
  if ((NS_OK != rv) || (NS_OK != mView->Init(mViewManager, 
                                             tbounds,
                                             nsnull))) {
    return rv;
  }

  rv = mView->CreateWidget(kWidgetCID, nsnull, aNativeParent);

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

    NS_RELEASE(mPrintPS);
    NS_RELEASE(mPrintVM);
    NS_RELEASE(mPrintSS);
    NS_RELEASE(mPrintDC);
  }
}

/* ========================================================================================
 * nsIContentViewerFile
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
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentViewerImpl::CopySelection()
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentViewerImpl::GetCopyable(PRBool *aCopyable)
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentViewerImpl::CutSelection()
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentViewerImpl::GetCutable(PRBool *aCutable)
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentViewerImpl::Paste()
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentViewerImpl::GetPasteable(PRBool *aPasteable)
{
  NS_ASSERTION(0, "NOT IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* ========================================================================================
 * nsIContentViewerEdit
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
 *  See documentation above in the DocumentViewerImpl class definition
 *	@update 07/09/99 dwc
 */
NS_IMETHODIMP
DocumentViewerImpl::Print()
{
nsCOMPtr<nsIWebShell>                 webContainer;
nsCOMPtr<nsIDeviceContextSpecFactory> factory;
PRInt32                               width,height;
nsCOMPtr<nsIPref>                     prefs;

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

    factory->CreateDeviceContextSpec(nsnull, devspec, PR_FALSE);
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
          mPresContext->GetPrefs(getter_AddRefs(prefs));
          mPrintPC->Init(mPrintDC,prefs);
          CreateStyleSet(mDocument,&mPrintSS);

          rv = NS_NewPresShell(&mPrintPS);
          if(NS_FAILED(rv)){
            return rv;
          }
          
          rv = nsComponentManager::CreateInstance(kViewManagerCID,nsnull,kIViewManagerIID,(void**)&mPrintVM);
          if(NS_FAILED(rv)) {
            return rv;
          }

          rv = mPrintVM->Init(mPrintDC);
          if(NS_FAILED(rv)) {
            return rv;
          }

          rv = nsComponentManager::CreateInstance(kViewCID,nsnull,kIViewIID,(void**)&mPrintView);
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

NS_IMETHODIMP DocumentViewerImpl::GetIsFrame(PRBool* aIsFrame)
{
  NS_ENSURE_ARG_POINTER(aIsFrame);

  *aIsFrame = mIsFrame;
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SetIsFrame(PRBool aIsFrame)
{
  mIsFrame = aIsFrame;
  return NS_OK;
}

// XXX: SEMANTIC CHANGE! 
//      returns a copy of the string.  Caller is responsible for freeing result
//      using Recycle(aDefaultCharacterSet)
NS_IMETHODIMP DocumentViewerImpl::GetDefaultCharacterSet(PRUnichar** aDefaultCharacterSet)
{
  NS_ENSURE_ARG_POINTER(aDefaultCharacterSet);
  NS_ENSURE_STATE(mContainer);

  static char *gDefCharset = nsnull;    // XXX: memory leak!

  if (0 == mDefaultCharacterSet.Length()) 
  {
    if ((nsnull == gDefCharset) || (nsnull == *gDefCharset)) 
    {
      nsCOMPtr<nsIWebShell> webShell;
      webShell = do_QueryInterface(mContainer);
      if (webShell)
      {
        nsCOMPtr<nsIPref> prefs;
        NS_ENSURE_SUCCESS(webShell->GetPrefs(*(getter_AddRefs(prefs))), NS_ERROR_FAILURE);
        if(prefs)
          prefs->CopyCharPref("intl.charset.default", &gDefCharset);
      }
    }
    if ((nsnull == gDefCharset) || (nsnull == *gDefCharset))
      mDefaultCharacterSet = "ISO-8859-1";
    else
      mDefaultCharacterSet = gDefCharset;
  }
  *aDefaultCharacterSet = mDefaultCharacterSet.ToNewUnicode();
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SetDefaultCharacterSet(const PRUnichar* aDefaultCharacterSet)
{
  mDefaultCharacterSet = aDefaultCharacterSet;  // this does a copy of aDefaultCharacterSet
  // now set the default char set on all children of mContainer
  nsCOMPtr<nsIWebShell> webShell;
  webShell = do_QueryInterface(mContainer);
  if (webShell)
  {
    nsCOMPtr<nsIWebShell> childWebShell;
    PRInt32 i;
    PRInt32 n;
    webShell->GetChildCount(n);
    for (i=0; i < n; i++) 
    {
      webShell->ChildAt(i, *(getter_AddRefs(childWebShell)));
      NS_WARN_IF_FALSE(childWebShell, "null child in docshell");
      if (childWebShell) 
      {
        nsCOMPtr<nsIContentViewer> childCV;
        childWebShell->GetContentViewer(getter_AddRefs(childCV));
        if (childCV) 
        {
          nsCOMPtr<nsIMarkupDocumentViewer> markupCV = do_QueryInterface(childCV);
          if (markupCV) {
            markupCV->SetDefaultCharacterSet(aDefaultCharacterSet);
          }
        }
      }
    }
  }
  return NS_OK;
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

NS_IMETHODIMP DocumentViewerImpl::SetForceCharacterSet(const PRUnichar* aForceCharacterSet)
{
  mForceCharacterSet = aForceCharacterSet;
  // now set the force char set on all children of mContainer
  nsCOMPtr<nsIWebShell> webShell;
  webShell = do_QueryInterface(mContainer);
  if (webShell)
  {
    nsCOMPtr<nsIWebShell> childWebShell;
    PRInt32 i;
    PRInt32 n;
    webShell->GetChildCount(n);
    for (i=0; i < n; i++) 
    {
      webShell->ChildAt(i, *(getter_AddRefs(childWebShell)));
      NS_WARN_IF_FALSE(childWebShell, "null child in docshell");
      if (childWebShell) 
      {
        nsCOMPtr<nsIContentViewer> childCV;
        childWebShell->GetContentViewer(getter_AddRefs(childCV));
        if (childCV) 
        {
          nsCOMPtr<nsIMarkupDocumentViewer> markupCV = do_QueryInterface(childCV);
          if (markupCV) {
            markupCV->SetForceCharacterSet(aForceCharacterSet);
          }
        }
      }
    }
  }
  return NS_OK;
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


NS_IMETHODIMP DocumentViewerImpl::SetHintCharacterSetSource(PRInt32 aHintCharacterSetSource)
{
  mHintCharsetSource = (nsCharsetSource)aHintCharacterSetSource;
  // now set the force char set on all children of mContainer
  nsCOMPtr<nsIWebShell> webShell;
  webShell = do_QueryInterface(mContainer);
  if (webShell)
  {
    nsCOMPtr<nsIWebShell> childWebShell;
    PRInt32 i;
    PRInt32 n;
    webShell->GetChildCount(n);
    for (i=0; i < n; i++) 
    {
      webShell->ChildAt(i, *(getter_AddRefs(childWebShell)));
      NS_WARN_IF_FALSE(childWebShell, "null child in docshell");
      if (childWebShell) 
      {
        nsCOMPtr<nsIContentViewer> childCV;
        childWebShell->GetContentViewer(getter_AddRefs(childCV));
        if (childCV) 
        {
          nsCOMPtr<nsIMarkupDocumentViewer> markupCV = do_QueryInterface(childCV);
          if (markupCV) {
            markupCV->SetHintCharacterSetSource(aHintCharacterSetSource);
          }
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SetHintCharacterSet(const PRUnichar* aHintCharacterSet)
{
  mHintCharset = aHintCharacterSet;
  // now set the force char set on all children of mContainer
  nsCOMPtr<nsIWebShell> webShell;
  webShell = do_QueryInterface(mContainer);
  if (webShell)
  {
    nsCOMPtr<nsIWebShell> childWebShell;
    PRInt32 i;
    PRInt32 n;
    webShell->GetChildCount(n);
    for (i=0; i < n; i++) 
    {
      webShell->ChildAt(i, *(getter_AddRefs(childWebShell)));
      NS_WARN_IF_FALSE(childWebShell, "null child in docshell");
      if (childWebShell) 
      {
        nsCOMPtr<nsIContentViewer> childCV;
        childWebShell->GetContentViewer(getter_AddRefs(childCV));
        if (childCV) 
        {
          nsCOMPtr<nsIMarkupDocumentViewer> markupCV = do_QueryInterface(childCV);
          if (markupCV) {
            markupCV->SetHintCharacterSet(aHintCharacterSet);
          }
        }
      }
    }
  }
  return NS_OK;
}

// XXX: poor error checking
NS_IMETHODIMP DocumentViewerImpl::SizeToContent()
{

  // XXX: for now, just call the webshell's SizeToContent
  nsCOMPtr<nsIWebShell> webShell;
  webShell = do_QueryInterface(mContainer);
  if (webShell)  
  {
    return webShell->SizeToContent();
  }
  else {
    return NS_ERROR_FAILURE;
  }

#ifdef NEW_DOCSHELL_INTERFACES

  // get the presentation shell
  nsCOMPtr<nsIPresShell> presShell;
  NS_ENSURE_SUCCESS(GetPresShell(*(getter_AddRefs(presShell))), NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  nsRect  shellArea;
  PRInt32 width, height;
  float   pixelScale;
  NS_ENSURE_SUCCESS(presShell->ResizeReflow(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE), 
                    NS_ERROR_FAILURE);

  // so how big is it?
  nsCOMPtr<nsIPresContext> presContext;
  NS_ENSURE_SUCCESS(GetPresContext(*(getter_AddRefs(presContext))), 
                    NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);
  presContext->GetVisibleArea(shellArea);
  presContext->GetTwipsToPixels(&pixelScale);
  width = PRInt32((float)shellArea.width*pixelScale);
  height = PRInt32((float)shellArea.height*pixelScale);

  // if we're the outermost webshell for this window, size the window
  /* XXX: how do we do this now?
  if (mContainer) 
  {
    nsCOMPtr<nsIBrowserWindow> browser = do_QueryInterface(mContainer);
    if (browser) 
    {
      nsCOMPtr<nsIDocShell> browserWebShell;
      PRInt32 oldX, oldY, oldWidth, oldHeight,
              widthDelta, heightDelta;
      nsRect  windowBounds;

      GetBounds(oldX, oldY, oldWidth, oldHeight);
      widthDelta = width - oldWidth;
      heightDelta = height - oldHeight;
      browser->GetWindowBounds(windowBounds);
      browser->SizeWindowTo(windowBounds.width + widthDelta,
                            windowBounds.height + heightDelta);
    }
  }
  */
  NS_ASSERTION(PR_FALSE, "NOT YET IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
  //return NS_OK;

#endif

}

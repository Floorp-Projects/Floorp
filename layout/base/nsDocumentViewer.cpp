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
#include "nsContentCID.h"
#include "nsLayoutCID.h"
#include "nsHTMLParts.h"

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
#include "nsILayoutHistoryState.h"
#include "nsLayoutAtoms.h"
#include "nsIDOMHTMLFrameSetElement.h"
#include "nsIFrameManager.h"
#include "nsIParser.h"
#include "nsIPrintContext.h"

#include "nsIChromeRegistry.h"

#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"

// Timer Includes
#include "nsITimer.h"
#include "nsITimerCallback.h"

#include "nsPIDOMWindow.h"
#include "nsIFocusController.h"

// Print Options
#include "nsIPrintOptions.h"
#include "nsGfxCIID.h"
#include "nsIServiceManager.h"
static NS_DEFINE_CID(kPrintOptionsCID, NS_PRINTOPTIONS_CID);
#include "nsHTMLAtoms.h" // XXX until atoms get factored into nsLayoutAtoms

// Printing Events
#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

// FrameSet
#include "nsINodeInfo.h"
#include "nsIDocument.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContent.h"
#include "nsINameSpaceManager.h"
#include "nsIWebShell.h"

//focus
#include "nsIDOMEventReceiver.h" 
#include "nsIDOMFocusListener.h"
#include "nsISelectionController.h"

#include "nsITransformMediator.h"

static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kPresShellCID, NS_PRESSHELL_CID);
static NS_DEFINE_CID(kGalleyContextCID,  NS_GALLEYCONTEXT_CID);
static NS_DEFINE_CID(kPrintContextCID,  NS_PRINTCONTEXT_CID);
static NS_DEFINE_CID(kStyleSetCID,  NS_STYLESET_CID);

#ifdef NS_DEBUG
#undef NOISY_VIEWER
#else
#undef NOISY_VIEWER
#endif

//#define SPOOL_TO_ONE_DOC 1

class DocumentViewerImpl;
class nsPagePrintTimer;

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
  NS_IMETHOD SetTransformMediator(nsITransformMediator* aMediator);

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

  // Printing Methods
  PRBool   PrintPage(nsIPresContext* aPresContext,
                     nsIPrintOptions* aPrintOptions);
  PRBool   DonePrintingPages();
  
protected:
  virtual ~DocumentViewerImpl();

private:
  void ForceRefresh(void);
  nsresult CreateStyleSet(nsIDocument* aDocument, nsIStyleSet** aStyleSet);
  nsresult MakeWindow(nsIWidget* aParentWidget,
                      const nsRect& aBounds);

  nsresult GetDocumentSelection(nsISelection **aSelection, nsIPresShell * aPresShell = nsnull);
  nsresult FindFrameSetWithIID(nsIContent * aParentContent, const nsIID& aIID);
  PRBool   IsThereASelection(nsIDOMWindowInternal * aDOMWin);
  PRBool   DoesContainFrameSet(nsIWebShell * aParent);
  PRBool   IsThereAnIFrameSelected(nsIWebShell* aWebShell,
                                   nsIDOMWindowInternal * aDOMWin,
                                   PRBool& aDoesContainFrameset);
  PRBool   IsWindowsInOurSubTree(nsIDOMWindowInternal * aDOMWindow);
  void     MakeDocPrintList(nsIDocShellTreeNode * aParentNode, nsVoidArray * aDocList);

  // get the currently infocus frame for the document viewer
  nsIDOMWindowInternal * FindFocusedDOMWindowInternal();
  // get the DOM window for a given document viewer
  nsIDOMWindow * GetDOMWindowForThisDV();

  //
  // The following three methods are used for printing...
  //
  nsresult DocumentReadyForPrinting();
  //nsresult PrintSelection(nsIDeviceContextSpec * aDevSpec);
  nsresult GetSelectionDocument(nsIDeviceContextSpec * aDevSpec, nsIDocument ** aNewDoc);

  PRUnichar* GetWebShellTitle(nsIWebShell * aWebShell);

  static void PR_CALLBACK HandlePLEvent(PLEvent* aEvent);
  static void PR_CALLBACK DestroyPLEvent(PLEvent* aEvent);

  nsresult SetupToPrintContent(nsIWebShell *      aParent,
                               nsIDeviceContext * aDContext,
                               nsIDOMWindow     * aDOMWin);
  nsresult PrintDocContent();


  // Timer Methods
  nsresult StartPagePrintTimer(nsIPresContext * aPresContext, 
                               nsIPrintOptions* aPrintOptions,
                               PRUint32         aDelay);
  void     StopPagePrintTimer();

protected:
  // IMPORTANT: The ownership implicit in the following member
  // variables has been explicitly checked and set using nsCOMPtr
  // for owning pointers and raw COM interface pointers for weak
  // (ie, non owning) references. If you add any members to this
  // class, please make the ownership explicit (pinkerton, scc).
  
  nsISupports* mContainer; // [WEAK] it owns me!
  nsCOMPtr<nsIDeviceContext> mDeviceContext;   // ??? can't hurt, but...
  nsIView*                 mView;        // [WEAK] cleaned up by view mgr

  // the following seven items are explicitly in this order
  // so they will be destroyed in the reverse order (pinkerton, scc)
  nsCOMPtr<nsITransformMediator> mTransformMediator;
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
  nsIPageSequenceFrame* mPageSeqFrame;



  // printing members
  nsIDeviceContext            *mPrintDC;
  nsIPresContext              *mPrintPC;
  nsIStyleSet                 *mPrintSS;
  nsIPresShell                *mPrintPS;
  nsIViewManager              *mPrintVM;
  nsIView                     *mPrintView;
  FILE                        *mFilePointer;    // a file where information can go to when printing

  nsIPresShell                *mPrintPresShell;
  nsIPresContext              *mPrintPresContext;
  nsIStyleSet                 *mPrintStyleSet;
  nsIViewManager              *mPrintViewManager;

  nsPagePrintTimer*           mPagePrintTimer;
  nsCOMPtr<nsIPrintListener>  mPrintListener; // An observer for printing...

  nsVoidArray*                mPrintDocList;
  nsCOMPtr<nsIDeviceContext>  mPrintDocDC;
  nsCOMPtr<nsIDOMWindow>      mPrintDocDW;
  PRBool                      mIsIFrameSelection;
  PRBool                      mDoesContainFrameSet;
  PRBool                      mTimerAlreadyStarted;

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


//---------------------------------------------------
//-- Page Timer Stuff
//---------------------------------------------------
class nsPagePrintTimer : public nsITimerCallback
{
public:

  NS_DECL_ISUPPORTS

  nsPagePrintTimer()
      : mDocViewer(nsnull), mPresContext(nsnull), mPrintOptions(nsnull), mDelay(0)
  {
    NS_INIT_ISUPPORTS();
  }

  virtual ~nsPagePrintTimer()
  {
    if (mTimer) {
      mTimer->Cancel();
    }
  }

  // nsITimerCallback
  NS_IMETHOD_(void) Notify(nsITimer *timer)
  {
    if (mPresContext && mDocViewer) {
      // Check to see if we are done
      // donePrinting will be true if it completed successfully or
      // if the printing was cancelled
      PRBool donePrinting = mDocViewer->PrintPage(mPresContext, mPrintOptions);
      if (donePrinting) {
        // now clean up print or print the next webshell
        if (mDocViewer->DonePrintingPages()) {
          Stop();
        }
      }
    }
  }

  void Init(DocumentViewerImpl* aDocViewerImpl, 
            nsIPresContext*     aPresContext,
            nsIPrintOptions*    aPrintOptions,
            PRUint32            aDelay) 
  {
    mDocViewer       = aDocViewerImpl;
    mPresContext     = aPresContext;
    mPrintOptions    = aPrintOptions;
    mDelay           = aDelay;
  }

  nsresult Start(DocumentViewerImpl* aDocViewerImpl, 
                 nsIPresContext*     aPresContext,
                 nsIPrintOptions*    aPrintOptions,
                 PRUint32            aDelay) 
  {
    Init(aDocViewerImpl, aPresContext, aPrintOptions, aDelay);

    if (!mTimer) {
      nsresult result;
      mTimer = do_CreateInstance("@mozilla.org/timer;1", &result);

      if (NS_FAILED(result))
        return result;
    }

    return mTimer->Init(this, mDelay, NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_SLACK);
  }

  void Stop() { if (mTimer) mTimer->Cancel(); }

private:
  DocumentViewerImpl*  mDocViewer;
  nsIPresContext*      mPresContext;
  nsIPrintOptions*     mPrintOptions;
  nsCOMPtr<nsITimer>   mTimer;
  PRUint32             mDelay;
};

NS_IMPL_ADDREF(nsPagePrintTimer)
NS_IMPL_RELEASE(nsPagePrintTimer)
NS_IMPL_QUERY_INTERFACE1(nsPagePrintTimer, nsITimerCallback)

nsresult NS_NewUpdateTimer(nsPagePrintTimer **aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;

  *aResult = (nsPagePrintTimer*) new nsPagePrintTimer;

  if (!aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);

  return NS_OK;
}
//---------------------------------------------------
//-- DONE: Page Timer Stuff
//---------------------------------------------------




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
  NS_INIT_ISUPPORTS();
  mEnableRendering  = PR_TRUE;
  mFilePointer      = nsnull;
  mPagePrintTimer   = nsnull;
  mPrintDocList     = nsnull;
}

DocumentViewerImpl::DocumentViewerImpl(nsIPresContext* aPresContext)
  : mPresContext(dont_QueryInterface(aPresContext))
{
  NS_INIT_ISUPPORTS();
  mHintCharsetSource = kCharsetUninitialized;
  mAllowPlugins      = PR_TRUE;
  mEnableRendering   = PR_TRUE;
  mFilePointer       = nsnull;
  mPagePrintTimer    = nsnull;
  mPrintDocList      = nsnull;
}

NS_IMPL_ISUPPORTS5(DocumentViewerImpl,
                   nsIContentViewer,
                   nsIDocumentViewer,
                   nsIMarkupDocumentViewer,
                   nsIContentViewerFile,
                   nsIContentViewerEdit)

DocumentViewerImpl::~DocumentViewerImpl()
{
  if (mPagePrintTimer != nsnull) {
    StopPagePrintTimer();
    delete mPagePrintTimer;
  }
      // Revoke pending invalidate events
  NS_ASSERTION(!mDocument, "User did not call nsIContentViewer::Destroy");
  if (mDocument)
    Destroy();

  // clear weak references before we go away
  if (mPresContext) {
    mPresContext->SetContainer(nsnull);
    mPresContext->SetLinkHandler(nsnull);
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
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NULL_POINTER);

  mDeviceContext = dont_QueryInterface(aDeviceContext);

  PRBool makeCX = PR_FALSE;
  if (!mPresContext) {
    // Create presentation context
#if 1 
    mPresContext = do_CreateInstance(kGalleyContextCID,&rv);
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
  nsresult rv = NS_OK;
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIScriptGlobalObject> global;

  // First, get the script global object from the document...
  rv = mDocument->GetScriptGlobalObject(getter_AddRefs(global));

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
DocumentViewerImpl::Destroy()
{
  // All callers are supposed to call destroy to break circular
  // references.  If we do this stuff in the destructor, the
  // destructor might never be called (especially if we're being
  // used from JS.

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

  if (mDeviceContext)
    mDeviceContext->FlushFontCache();

  if (mPresShell) {
    // Break circular reference (or something)
    mPresShell->EndObservingDocument();
    nsCOMPtr<nsISelection> selection;
    rv = GetDocumentSelection(getter_AddRefs(selection));
    nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(selection));
    if (NS_SUCCEEDED(rv) && selPrivate && mSelectionListener) 
      selPrivate->RemoveSelectionListener(mSelectionListener);
  }
  
  mDocument = nsnull;
  
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Stop(void)
{
  NS_ASSERTION(mDocument, "Stop called too early or too late");
  if (mDocument) {
    mDocument->StopDocumentLoad();
  }

  if (mPresContext) {
    // stop everything but the chrome.
    mPresContext->Stop(PR_FALSE);
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetDOMDocument(nsIDOMDocument **aResult)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
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
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
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
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
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
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  NS_PRECONDITION(mWindow, "null window");
  if (mWindow) {
    mWindow->Move(aX, aY);
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Show(void)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  NS_PRECONDITION(mWindow, "null window");
  if (mWindow) {
    mWindow->Show(PR_TRUE);
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Hide(void)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  NS_PRECONDITION(mWindow, "null window");
  if (mWindow) {
    mWindow->Show(PR_FALSE);
  }
  return NS_OK;
}

nsresult
DocumentViewerImpl::FindFrameSetWithIID(nsIContent * aParentContent, const nsIID& aIID)
{
  PRInt32 numChildren;
  aParentContent->ChildCount(numChildren);

  // do a breadth search across all siblings
  PRInt32 inx;
  for (inx=0;inx<numChildren;inx++) {
    nsCOMPtr<nsIContent> child;
    if (NS_SUCCEEDED(aParentContent->ChildAt(inx, *getter_AddRefs(child))) && child) {
      nsCOMPtr<nsISupports> temp;
      if (NS_SUCCEEDED(child->QueryInterface(aIID, (void**)getter_AddRefs(temp)))) {
        return NS_OK;
      }
    }
  }

  return NS_ERROR_FAILURE;
}

//---------------------------------------------------------------
//---------------------------------------------------------------
//-- Debug helper routines
//---------------------------------------------------------------
//---------------------------------------------------------------
#if defined(DEBUG_rods) || defined(DEBUG_dcone)

/** ---------------------------------------------------
 *  Dumps Frames for Printing
 */
static void DumpFrames(FILE*                 out, 
                       nsIPresContext*       aPresContext, 
                       nsIRenderingContext * aRendContext, 
                       nsIFrame *            aFrame, 
                       PRInt32               aLevel)
{
  nsIFrame * child;
  aFrame->FirstChild(aPresContext, nsnull, &child);
  while (child != nsnull) {
    for (PRInt32 i=0;i<aLevel;i++) {
     fprintf(out, "  ");
    }
    nsAutoString tmp;
    nsIFrameDebug*  frameDebug;

    if (NS_SUCCEEDED(child->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
      frameDebug->GetFrameName(tmp);
    }
    fputs(tmp, out);
    nsFrameState state;
    child->GetFrameState(&state);
    PRBool isSelected;
    if (NS_SUCCEEDED(child->IsVisibleForPainting(aPresContext, *aRendContext, PR_TRUE, &isSelected))) {
      fprintf(out, " %p %s", child, isSelected?"VIS":"UVS");
      nsRect rect;
      child->GetRect(rect);
      fprintf(out, "[%d,%d,%d,%d] ", rect.x, rect.y, rect.width, rect.height);
      nsIView * view;
      child->GetView(aPresContext, &view);
      fprintf(out, "v: %p ", view);
      fprintf(out, "\n");
      DumpFrames(out, aPresContext, aRendContext, child, aLevel+1);
      child->GetNextSibling(&child);
    }
  }
}

/** ---------------------------------------------------
 *  Helper function needed for "DumpViews"
 */
static nsIPresShell*
GetPresShellFor(nsIDocShell* aDocShell)
{
  nsIPresShell* shell = nsnull;
  if (nsnull != aDocShell) {
    nsIContentViewer* cv = nsnull;
    aDocShell->GetContentViewer(&cv);
    if (nsnull != cv) {
      nsIDocumentViewer* docv = nsnull;
      cv->QueryInterface(NS_GET_IID(nsIDocumentViewer), (void**) &docv);
      if (nsnull != docv) {
        nsIPresContext* cx;
        docv->GetPresContext(cx);
        if (nsnull != cx) {
          cx->GetShell(&shell);
          NS_RELEASE(cx);
        }
        NS_RELEASE(docv);
      }
      NS_RELEASE(cv);
    }
  }
  return shell;
}

/** ---------------------------------------------------
 *  Dumps the Views from the DocShell
 */
static void
DumpViews(nsIDocShell* aDocShell, FILE* out)
{
  if (nsnull != aDocShell) {
    fprintf(out, "docshell=%p \n", aDocShell);
    nsIPresShell* shell = GetPresShellFor(aDocShell);
    if (nsnull != shell) {
      nsCOMPtr<nsIViewManager> vm;
      shell->GetViewManager(getter_AddRefs(vm));
      if (vm) {
        nsIView* root;
        vm->GetRootView(root);
        if (nsnull != root) {
          root->List(out);
        }
      }
      NS_RELEASE(shell);
    }
    else {
      fputs("null pres shell\n", out);
    }

    // dump the views of the sub documents
    PRInt32 i, n;
    nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(aDocShell));
    docShellAsNode->GetChildCount(&n);
    for (i = 0; i < n; i++) {
      nsCOMPtr<nsIDocShellTreeItem> child;
      docShellAsNode->GetChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
      if (childAsShell) {
        DumpViews(childAsShell, out);
      }
    }
  }
}

/** ---------------------------------------------------
 *  Dumps the Views and Frames
 */
void DumpLayoutData(nsIPresContext*    aPresContext,
                    nsIDeviceContext * aDC,
                    nsIFrame *         aRootFrame,
                    nsIWebShell *      aWebShell)
{
  // Dump all the frames and view to a a file
  FILE * fd = fopen("dump.txt", "w");
  if (fd) {
    fprintf(fd, "--------------- Frames ----------------\n");
    nsCOMPtr<nsIRenderingContext> renderingContext;
    aDC->CreateRenderingContext(*getter_AddRefs(renderingContext));
    DumpFrames(fd, aPresContext, renderingContext, aRootFrame, 0);
    fprintf(fd, "---------------------------------------\n\n");
    fprintf(fd, "--------------- Views From Root Frame----------------\n");
    nsIView * v;
    aRootFrame->GetView(aPresContext, &v);
    if (v) {
      v->List(fd);
    } else {
      printf("View is null!\n");
    }
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aWebShell));
    if (docShell) {
      fprintf(fd, "--------------- All Views ----------------\n");
      DumpViews(docShell, fd);
      fprintf(fd, "---------------------------------------\n\n");
    }
    fclose(fd);
  }
}

#endif
//---------------------------------------------------------------
//---------------------------------------------------------------
//-- End of debug helper routines
//---------------------------------------------------------------
//---------------------------------------------------------------

/** ---------------------------------------------------
 *  Giving a child frame it searches "up" the tree until it 
 *  finds a "Page" frame.
 */
static nsIFrame * GetPageFrame(nsIFrame * aFrame)
{
  nsIFrame * frame = aFrame;
  while (frame != nsnull) {
    nsCOMPtr<nsIAtom> type;
    frame->GetFrameType(getter_AddRefs(type));
    if (type.get() == nsLayoutAtoms::pageFrame) {
      return frame;
    }
    frame->GetParent(&frame);
  }
  return nsnull;
}


/** ---------------------------------------------------
 *  Find by checking content's tag type
 */
static nsIFrame * FindFrameByType(nsIPresContext* aPresContext,
                                  nsIFrame *      aParentFrame,
                                  nsIAtom *       aType,
                                  nsRect&         aRect,
                                  nsRect&         aChildRect) 
{
  nsIFrame * child;
  nsRect rect;
  aParentFrame->GetRect(rect);
  aRect.x += rect.x;
  aRect.y += rect.y;
  aParentFrame->FirstChild(aPresContext, nsnull, &child);
  while (child != nsnull) {
    nsCOMPtr<nsIContent> content;
    child->GetContent(getter_AddRefs(content));
    if (content) {
      nsCOMPtr<nsIAtom> type;
      content->GetTag(*getter_AddRefs(type));
      if (type.get() == aType) {
        nsRect r;
        child->GetRect(r);
        aChildRect.SetRect(aRect.x + r.x, aRect.y + r.y, r.width, r.height);
        aRect.x -= rect.x;
        aRect.y -= rect.y;
        return child;
      }
    }
    nsIFrame * fndFrame = FindFrameByType(aPresContext, child, aType, aRect, aChildRect);
    if (fndFrame != nsnull) {
      return fndFrame;
    }
    child->GetNextSibling(&child);
  }
  aRect.x -= rect.x;
  aRect.y -= rect.y;
  return nsnull;
}

/** ---------------------------------------------------
 *  Find by checking frames type
 */
static nsresult FindSelectionBounds(nsIPresContext* aPresContext,
                                    nsIRenderingContext& aRC,
                                    nsIFrame *      aParentFrame,
                                    nsRect&         aRect,
                                    nsIFrame *&     aStartFrame,
                                    nsRect&         aStartRect,
                                    nsIFrame *&     aEndFrame,
                                    nsRect&         aEndRect)
{
  nsIFrame * child;
  aParentFrame->FirstChild(aPresContext, nsnull, &child);
  nsRect rect;
  aParentFrame->GetRect(rect);
  aRect.x += rect.x;
  aRect.y += rect.y;
  while (child != nsnull) {
    nsFrameState state;
    child->GetFrameState(&state);
    // only leaf frames have this bit flipped
    // then check the hard way
    PRBool isSelected = (state & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT;
    if (isSelected) {
      if (NS_FAILED(child->IsVisibleForPainting(aPresContext, aRC, PR_TRUE, &isSelected))) {
        return NS_ERROR_FAILURE;
      }
    }

    if (isSelected) {
      nsRect r;
      child->GetRect(r);
      if (aStartFrame == nsnull) {
        aStartFrame = child;
        aStartRect.SetRect(aRect.x + r.x, aRect.y + r.y, r.width, r.height);
      } else {
        child->GetRect(r);
        aEndFrame = child;
        aEndRect.SetRect(aRect.x + r.x, aRect.y + r.y, r.width, r.height);
      }
    }
    FindSelectionBounds(aPresContext, aRC, child, aRect, aStartFrame, aStartRect, aEndFrame, aEndRect);
    child->GetNextSibling(&child);
  }
  aRect.x -= rect.x;
  aRect.y -= rect.y;
  return NS_OK;
}

/** ---------------------------------------------------
 *  This method finds the starting and ending page numbers
 *  of the selection and also returns rect for each where
 *  the x,y of the rect is relative to the very top of the
 *  frame tree (absolutely positioned)
 */
static nsresult GetPageRangeForSelection(nsIPresShell *        aPresShell,
                                         nsIPresContext*       aPresContext,
                                         nsIRenderingContext&  aRC,
                                         nsISelection*         aSelection,
                                         nsIPageSequenceFrame* aPageSeqFrame,
                                         nsIFrame**            aStartFrame,
                                         PRInt32&              aStartPageNum,
                                         nsRect&               aStartRect,
                                         nsIFrame**            aEndFrame,
                                         PRInt32&              aEndPageNum,
                                         nsRect&               aEndRect)
{
  nsIFrame * seqFrame;
  if (NS_FAILED(aPageSeqFrame->QueryInterface(NS_GET_IID(nsIFrame), (void **)&seqFrame))) {
    return NS_ERROR_FAILURE;
  }

  nsIFrame * startFrame = nsnull;
  nsIFrame * endFrame   = nsnull;
  nsRect rect;
  seqFrame->GetRect(rect);

  // start out with the sequence frame and search the entire frame tree
  // capturing the the starting and ending child frames of the selection 
  // and their rects
  FindSelectionBounds(aPresContext, aRC, seqFrame, rect, startFrame, aStartRect, endFrame, aEndRect);

#ifdef DEBUG_rods
  printf("Start Frame: %p\n", startFrame);
  printf("End Frame:   %p\n", endFrame);
#endif

  // initial the page numbers here 
  // in case we don't find and frames
  aStartPageNum = -1;
  aEndPageNum   = -1;

  nsIFrame * startPageFrame;
  nsIFrame * endPageFrame;

  // check to make sure we found a starting frame
  if (startFrame != nsnull) {
    // Now search up the tree to find what page the
    // start/ending selections frames are on
    //
    // Check to see if start should be same as end if
    // the end frame comes back null
    if (endFrame == nsnull) {
      // XXX the "GetPageFrame" step could be integrated into 
      // the FindSelectionBounds step, but walking up to find
      // the parent of a child frame isn't expensive and it makes
      // FindSelectionBounds a little easier to understand
      startPageFrame = GetPageFrame(startFrame);
      endPageFrame   = startPageFrame;
      aEndRect       = aStartRect;
    } else {
      startPageFrame = GetPageFrame(startFrame);
      endPageFrame   = GetPageFrame(endFrame);
    }
  } else {
    return NS_ERROR_FAILURE;
  }

#ifdef DEBUG_rods
  printf("Start Page: %p\n", startPageFrame);
  printf("End Page:   %p\n", endPageFrame);

  // dump all the pages and their pointers
  {
  PRInt32 pageNum = 1;
  nsIFrame * child;
  seqFrame->FirstChild(aPresContext, nsnull, &child);
  while (child != nsnull) {
    printf("Page: %d - %p\n", pageNum, child);
    pageNum++;
    child->GetNextSibling(&child);
  }
  }
#endif

  // Now that we have the page frames
  // find out what the page numbers are for each frame
  PRInt32 pageNum = 1;
  nsIFrame * page;
  seqFrame->FirstChild(aPresContext, nsnull, &page);
  while (page != nsnull) {
    if (page == startPageFrame) {
      aStartPageNum = pageNum;
    } 
    if (page == endPageFrame) {
      aEndPageNum = pageNum;
    }
    pageNum++;
    page->GetNextSibling(&page);
  }

#ifdef DEBUG_rods
  printf("Start Page No: %d\n", aStartPageNum);
  printf("End Page No:   %d\n", aEndPageNum);
#endif

  *aStartFrame = startPageFrame;
  *aEndFrame   = endPageFrame;

  return NS_OK;
}

//-------------------------------------------------------
PRBool 
DocumentViewerImpl::DoesContainFrameSet(nsIWebShell * aParent)
{
  // See if if the incoming doc is the root document
  nsCOMPtr<nsIDocShellTreeItem> parentAsItem(do_QueryInterface(aParent));
  nsCOMPtr<nsIDocShellTreeItem> parentsParentDoc;
  parentAsItem->GetParent(getter_AddRefs(parentsParentDoc));

  // When it is the top level document we need to check 
  // to see if it contains a frameset. If it does, then 
  // we only want to print the doc's children and not the document itself
  // For anything else we always print all the children and the document
  // for example, if the doc contains an IFRAME we eant to print the child
  // document (the IFRAME) and then the rest of the document.
  //
  // XXX we really need to search the frame tree, and not the content
  // but there is no way to distinguish between IFRAMEs and FRAMEs
  // with the GetFrameType call.
  // Bug 53459 has been files so we can eventually distinguish 
  // between IFRAME frames and FRAME frames
  PRBool doesContainFrameSet = PR_FALSE;
  // only check to see if there is a frameset if there is 
  // NO parent doc for this doc. meaning this parent is the root doc
  //if (!parentsParentDoc) {
    nsCOMPtr<nsIPresShell> shell;
    mPresContext->GetShell(getter_AddRefs(shell));
    if (shell) {
      nsCOMPtr<nsIDocument> doc;
      shell->GetDocument(getter_AddRefs(doc));
      if (doc) {
        nsCOMPtr<nsIContent> rootContent = getter_AddRefs(doc->GetRootContent());
        if (rootContent) {
          if (NS_SUCCEEDED(FindFrameSetWithIID(rootContent, NS_GET_IID(nsIDOMHTMLFrameSetElement)))) {
            doesContainFrameSet = PR_TRUE;
          }
        }
      }
    }
  //}
  return doesContainFrameSet;
}

//-------------------------------------------------------
PRUnichar*
DocumentViewerImpl::GetWebShellTitle(nsIWebShell * aWebShell)
{
  NS_ASSERTION(aWebShell != nsnull, "Must have a valid webshell!");

  PRUnichar * docTitleStr = nsnull;

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aWebShell));
  if (docShell) {
    nsCOMPtr<nsIPresShell> presShell;
    docShell->GetPresShell(getter_AddRefs(presShell));
    if (presShell) {
      nsCOMPtr<nsIDocument> doc;
      presShell->GetDocument(getter_AddRefs(doc));
      if (doc) {
        const nsString* docTitle = doc->GetDocumentTitle();
        if (docTitle != nsnull) {
          nsAutoString title(docTitle->get());
          if (title.Length() == 0) {
            title = NS_LITERAL_STRING("Document");
          }
          docTitleStr = title.ToNewUnicode();
        }
      }
    }
  }
  return docTitleStr;
}


//-------------------------------------------------------
PRBool
DocumentViewerImpl::DonePrintingPages()
{
  // Clean up 
  // first set of data members could be null
  // if the print job was cancelled
  if (mPrintPresShell != nsnull) {
    mPrintPresShell->EndObservingDocument();
    NS_RELEASE(mPrintPresShell);
  }
  NS_IF_RELEASE(mPrintViewManager);
  NS_IF_RELEASE(mPrintStyleSet);
  NS_IF_RELEASE(mPrintPresContext);

  // printing is complete, clean up now
  mIsPrinting = PR_FALSE;

  mPrintPS->EndObservingDocument();

  // Now check to see if there are anymore webshells
  // that need to be printed.
  if (mPrintDocList && mPrintDocList->Count() > 0) {
    PrintDocContent();
    return PR_FALSE;
  }

  if (mPrintListener) {
    // send the last notification to the listener
    // and then we release the listener
    mPrintListener->OnEndPrinting(NS_OK);
    mPrintListener = do_QueryInterface(nsnull);
  }

  mPrintDC->EndDocument();

  NS_RELEASE(mPrintPS);
  NS_RELEASE(mPrintVM);
  NS_RELEASE(mPrintSS);
  NS_RELEASE(mPrintDC);
  NS_RELEASE(mPrintPC);

  return PR_TRUE;
}

//-------------------------------------------------------
PRBool
DocumentViewerImpl::PrintPage(nsIPresContext* aPresContext,
                              nsIPrintOptions* aPrintOptions)
{
  nsresult rv;
  NS_WITH_SERVICE(nsIPrintOptions, printService, kPrintOptionsCID, &rv);
  if (NS_SUCCEEDED(rv) && printService) {
    PRBool isCancelled;
    printService->GetIsCancelled(&isCancelled);
    if (isCancelled) {
      printService->SetIsCancelled(PR_FALSE);
      return PR_TRUE;
    }
  }

  PRInt32 pageNum;
  PRInt32 curPage;
  PRInt32 endPage;
  mPageSeqFrame->GetCurrentPageNum(&pageNum);

  PRBool donePrinting = PR_FALSE;
  PRBool isDoingPrintRange;
  mPageSeqFrame->IsDoingPrintRange(&isDoingPrintRange);
  if (isDoingPrintRange) {
    PRInt32 fromPage;
    PRInt32 toPage;
    mPageSeqFrame->GetPrintRange(&fromPage, &toPage);
#if defined(DEBUG_rods) || defined(DEBUG_dcone)
    printf("****** Printing Page %d printing from %d to page %d\n", pageNum, fromPage, toPage);
#endif
    donePrinting = pageNum >= toPage;
    curPage = pageNum - fromPage;
    endPage = toPage - fromPage;
  } else {
    PRInt32 numPages;
    mPageSeqFrame->GetNumPages(&numPages);
#if defined(DEBUG_rods) || defined(DEBUG_dcone)
    printf("****** Printing Page %d of %d page(s)\n", pageNum, numPages);
#endif
    donePrinting = pageNum >= numPages;
    curPage = pageNum;
    endPage = numPages;
  }

  if (mPrintListener) {
    // Make sure the Listener gets a "zero" progress at the beginning
    // so it can initialize it's state
    if (curPage == 1) {
      mPrintListener->OnProgressPrinting(0, endPage); 
    }
    mPrintListener->OnProgressPrinting(curPage, endPage); 
  }
  mPageSeqFrame->PrintNextPage(aPresContext, aPrintOptions);

  return donePrinting;
}

//-------------------------------------------------------
// Recursively build a list of of sub documents to be printed
void 
DocumentViewerImpl::MakeDocPrintList(nsIDocShellTreeNode * aParentNode, nsVoidArray * aDocList)
{
  PRInt32 childWebshellCount;
  aParentNode->GetChildCount(&childWebshellCount);

  // If the document is a FrameSet then we don't add the Parent WebShell to the List
  // If it contains an IFrame then we DO add it into the list.
  nsCOMPtr<nsIWebShell> webshell(do_QueryInterface(aParentNode));
  if (!DoesContainFrameSet(webshell) && childWebshellCount > 0) {
    aDocList->AppendElement((void*)webshell);
  }

  if (childWebshellCount > 0) {
    for (PRInt32 i=0;i<childWebshellCount;i++) {
      nsCOMPtr<nsIDocShellTreeItem> child;
      aParentNode->GetChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
      nsCOMPtr<nsIContentViewer>  viewer;
      childAsShell->GetContentViewer(getter_AddRefs(viewer));
      nsCOMPtr<nsIContentViewerFile> viewerFile(do_QueryInterface(viewer));
      if (viewerFile) {
        nsCOMPtr<nsIWebShell> childWebShell(do_QueryInterface(child));
        aDocList->AppendElement((void*)childWebShell);
        nsCOMPtr<nsIDocShellTreeNode> childNode(do_QueryInterface(child));
        MakeDocPrintList(childNode, aDocList);
      }
    }
  }

}

//-------------------------------------------------------
nsresult
DocumentViewerImpl::PrintContent(nsIWebShell *      aParent,
                                 nsIDeviceContext * aDContext,
                                 nsIDOMWindow     * aDOMWin,
                                 PRBool             aIsSubDoc)
{
  // XXX Once we get printing for plugins going we will
  // have to revist this method.
  NS_ASSERTION(0, "Still may be needed for plugins");

  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------
nsresult
DocumentViewerImpl::SetupToPrintContent(nsIWebShell *      aParent,
                                        nsIDeviceContext * aDContext,
                                        nsIDOMWindow     * aDOMWin)
{

  NS_ENSURE_ARG_POINTER(aParent);
  NS_ENSURE_ARG_POINTER(aDContext);

  nsCOMPtr<nsIDocShellTreeNode>  parentAsNode(do_QueryInterface(aParent));
  nsCOMPtr<nsIDOMWindowInternal> domWinIntl(do_QueryInterface(aDOMWin));

  mIsIFrameSelection = IsThereAnIFrameSelected(aParent, domWinIntl, mDoesContainFrameSet);

  PRInt16 printFrameType = nsIPrintOptions::kSelectedFrame; // XXX later this default to kFramesAsIs
  nsresult rv;
  NS_WITH_SERVICE(nsIPrintOptions, printService, kPrintOptionsCID, &rv);
  if (NS_SUCCEEDED(rv) && printService) {
    printService->GetPrintFrameType(&printFrameType);
  }
  if (printFrameType == nsIPrintOptions::kFramesAsIs) {
    // print frameset "as is " here and return
    return NS_OK;
  }

  // Create a list for storing the WebShells that need to be printed
  if (mPrintDocList == nsnull) {
    mPrintDocList = new nsVoidArray();
    if (mPrintDocList == nsnull) {
      return NS_ERROR_FAILURE;
    }
  } else {
    mPrintDocList->Clear();
  }

  // If there are no Sub docs than add "this" webshell to the list
  MakeDocPrintList(parentAsNode, mPrintDocList);
  if (mPrintDocList->Count() == 0) {
    mPrintDocList->AppendElement(aParent);
  }

  mPrintDocDC = aDContext;
  mPrintDocDW = aDOMWin;

  mTimerAlreadyStarted = PR_FALSE;

  // This will print the webshell document
  // when it completes asynchronously in the DonePrintingPages method
  // it will check to see if there are more webshells to be printed and 
  // then PrintDocContent will be called again.

  nsCOMPtr<nsIWebShell> webContainer(do_QueryInterface(mContainer));
  PRUnichar * docTitleStr = GetWebShellTitle(webContainer);

  // BeginDocument may pass back a FAILURE code
  // i.e. On Windows, if you are printing to a file and hit "Cancel" 
  //      to the "File Name" dialog
  rv = mPrintDC->BeginDocument(docTitleStr);

  if (docTitleStr != nsnull) {
    nsMemory::Free(docTitleStr);
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  return PrintDocContent();
}

//-------------------------------------------------------
// Called for each WebShell that needs to be printed
nsresult
DocumentViewerImpl::PrintDocContent()
{
  NS_ASSERTION(mPrintDocList != nsnull, "mPrintDocList to print!");

  nsCOMPtr<nsIDOMWindowInternal> domWinIntl(do_QueryInterface(mPrintDocDW));

  // get the main parent WebShell
  nsCOMPtr<nsIWebShell> webContainer(do_QueryInterface(mContainer));

  //-----------------------------------------------------
  // Now check to see if we can print the WebShells
  //-----------------------------------------------------
  PRInt16 printFrameType = nsIPrintOptions::kSelectedFrame; // XXX later this default to kFramesAsIs
  PRInt16 printHowEnable = nsIPrintOptions::kFrameEnableNone; 
  PRInt16 printRangeType = nsIPrintOptions::kRangeAllPages;
  nsresult rv;
  NS_WITH_SERVICE(nsIPrintOptions, printService, kPrintOptionsCID, &rv);
  if (NS_SUCCEEDED(rv) && printService) {
    printService->GetPrintFrameType(&printFrameType);
    printService->GetHowToEnableFrameUI(&printHowEnable);
    printService->GetPrintRange(&printRangeType);
  }

  nsCOMPtr<nsIDocument>  document;
  nsCOMPtr<nsIPresShell> wsPresShell;
  PRBool isSubDoc            = PR_FALSE;
  PRBool canPrintFrame       = PR_FALSE;
  PRBool printSelectedFrame  = PR_FALSE;
  PRBool printSelectedIFrame = PR_FALSE;

  // Walk the list of webshells to be printed and 
  // find the first one that "can" be printed.
  //
  // For example, if you have a frameset document with three frames and
  // you are printing a selection in the second frame. This will skip
  // the first frame, print the second and then laster clear the list 
  // of webshells to be printed.
  nsIWebShell* webShell = nsnull;

  while (!canPrintFrame && mPrintDocList->Count() > 0) {
    webShell = (nsIWebShell*)mPrintDocList->ElementAt(0);
    NS_ASSERTION(webShell, "The WebShell can't be NULL!");

    // Now locate the nsIDocument for the WebShell
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(webShell));
    NS_ASSERTION(docShell, "The DocShell can't be NULL!");

    docShell->GetPresShell(getter_AddRefs(wsPresShell));
    NS_ASSERTION(wsPresShell, "The PresShell can't be NULL!");

    wsPresShell->GetDocument(getter_AddRefs(document));
    NS_ASSERTION(document, "The Document can't be NULL!");

    // Now see if we are printing a sub document
    isSubDoc = webShell != webContainer.get();

    // Remove the WebShell we are going to print
    mPrintDocList->RemoveElementAt(0);

    // Check to see if printing frames is not on 
    // and that we are printing a range or all pages
    PRBool printAllPages  = (printRangeType == nsIPrintOptions::kRangeAllPages ||
                             printRangeType == nsIPrintOptions::kRangeSpecifiedPageRange) && 
                             printHowEnable == nsIPrintOptions::kFrameEnableNone;

    // See if we are printing the selected frame
    PRBool printEachFrame = printFrameType == nsIPrintOptions::kEachFrameSep;

    // This is for when the current root WebShell is a Frameset
    // and we need to see if the webshell being passed in 
    // is the currently selected frame of the frameset
    //
    // The aDOMWin is the currently selected DOM window, 
    // which could be a frameset frame or a IFrame
    PRBool thisWebShellIsSelected = PR_FALSE;
    nsCOMPtr<nsIScriptGlobalObject> scriptObj(do_QueryInterface(mPrintDocDW));
    if (scriptObj) {
      nsCOMPtr<nsIDocShell> docShell;
      scriptObj->GetDocShell(getter_AddRefs(docShell));
      if (docShell) {
        nsCOMPtr<nsIWebShell> webShellForParent(do_QueryInterface(docShell));
        if (webShellForParent.get() == webShell) {
          thisWebShellIsSelected = PR_TRUE;
        }
      }
    }

    // Check to make we can print the currently selected frame of a frameset
    printSelectedFrame = thisWebShellIsSelected && 
                         printFrameType == nsIPrintOptions::kSelectedFrame;

    // Now see if we can print the selected IFrame
    // When IFrames are selected all the frame UI is turned off
    // and theuser prints it by selected the "Selection" radiobutton
    // which turns on kRangeSelection
    printSelectedIFrame  = PR_FALSE;
    if (printHowEnable == nsIPrintOptions::kFrameEnableNone && 
        printRangeType == nsIPrintOptions::kRangeSelection &&
        thisWebShellIsSelected) {
      printSelectedIFrame = PR_TRUE;
      // check to see if we have a range selection, 
      // as oppose to a insert selection
      // this means if the user just clicked on the IFrame then 
      // there will not be a selection so we want the entire page to print
      //
      // XXX this is sort of a hack right here to make the page 
      // not try to reposition itself when printing selection
      if (!IsThereASelection(domWinIntl)) {
        printRangeType = nsIPrintOptions::kRangeAllPages;
        printService->SetPrintRange(printRangeType);
      }
    }

    canPrintFrame = printAllPages || printEachFrame || printSelectedFrame || printSelectedIFrame;                                                          
  }

  if (webShell != nsnull && (!isSubDoc || (isSubDoc && canPrintFrame))) {

    // If we printing the selected frame and 
    // there are other WebShells in the list then clear the list 
    if (printSelectedFrame) {
      mPrintDocList->Clear();
    }

    PRInt32 width, height;
    mPrintDocDC->GetDeviceSurfaceDimensions(width, height);

    // XXX - Hack Alert
    // OK,  so ther eis a selection, we will print the entire selection 
    // on one page and then crop the page.
    // This means you can never print any selection that is longer than one page
    // put it keeps it from page breaking in the middle of your print of the selection
    // (see also nsSimplePageSequence.cpp)
    if (IsThereASelection(domWinIntl)) {
      height = 0x0FFFFFFF;
    }

    nsCOMPtr<nsIPrintContext> printcon(do_CreateInstance(kPrintContextCID,&rv));
    if (NS_FAILED(rv)) {
      return rv;
    } else {
      rv = printcon->QueryInterface(NS_GET_IID(nsIPresContext), (void**)&mPrintPresContext);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }

    mPrintPresContext->Init(mPrintDocDC);

    CreateStyleSet(document, &mPrintStyleSet);

    rv = nsComponentManager::CreateInstance(kPresShellCID,
                                            nsnull,
                                            NS_GET_IID(nsIPresShell),
                                            (void **)&mPrintPresShell);
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = nsComponentManager::CreateInstance(kViewManagerCID,
                                            nsnull,
                                            NS_GET_IID(nsIViewManager),
                                            (void **)&mPrintViewManager);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = mPrintViewManager->Init(mPrintDocDC);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsRect tbounds = nsRect(0, 0, width, height);

    // Create a child window of the parent that is our "root view/window"
    nsIView* rootView;
    rv = nsComponentManager::CreateInstance(kViewCID, nsnull, NS_GET_IID(nsIView), (void **)&rootView);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = rootView->Init(mPrintViewManager, tbounds, nsnull);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Setup hierarchical relationship in view manager
    mPrintViewManager->SetRootView(rootView);

    mPrintPresShell->Init(document, mPrintPresContext, mPrintViewManager, mPrintStyleSet);

    nsCompatibility mode;
    mPresContext->GetCompatibilityMode(&mode);
    mPrintPresContext->SetCompatibilityMode(mode);
    mPrintPresContext->SetContainer(webShell);


    // get the old history
    nsCOMPtr<nsIPresShell> presShell;
    nsCOMPtr<nsILayoutHistoryState>  layoutState;
    NS_ENSURE_SUCCESS(GetPresShell(*(getter_AddRefs(presShell))), NS_ERROR_FAILURE);
    presShell->CaptureHistoryState(getter_AddRefs(layoutState),PR_TRUE);


    mPrintPresShell->BeginObservingDocument();
    //lay it out...
    mPrintPresShell->InitialReflow(width, height);

    // Transfer Selection Ranges to the new Print PresShell
    nsCOMPtr<nsISelection> selection;
    nsCOMPtr<nsISelection> selectionPS;
    nsresult rvv = GetDocumentSelection(getter_AddRefs(selection), wsPresShell);
    if (NS_SUCCEEDED(rvv) && selection) {
      rvv = GetDocumentSelection(getter_AddRefs(selectionPS), mPrintPresShell);
      if (NS_SUCCEEDED(rvv) && selectionPS) {
        PRInt32 cnt;
        selection->GetRangeCount(&cnt);
        PRInt32 inx;
        for (inx=0;inx<cnt;inx++) {
          nsCOMPtr<nsIDOMRange> range;
          if (NS_SUCCEEDED(selection->GetRangeAt(inx, getter_AddRefs(range)))) {
            selectionPS->AddRange(range);
          }
        }
      }
    }
          
    // update the history from the old presentation shell
    nsCOMPtr<nsIFrameManager> fm;
    rv = mPrintPresShell->GetFrameManager(getter_AddRefs(fm));
    if(NS_SUCCEEDED(rv) && fm) {
      nsIFrame* root;
      mPrintPresShell->GetRootFrame(&root);
      fm->RestoreFrameState(mPrintPresContext, root, layoutState);
    }
    mPrintPresShell->EndObservingDocument();

    // Ask the page sequence frame to print all the pages
    nsIPageSequenceFrame* pageSequence;

    // 
    mPrintPresShell->GetPageSequenceFrame(&pageSequence);
    NS_ASSERTION(nsnull != pageSequence, "no page sequence frame");
    
 
    if (nsnull != mFilePointer) {
      // output the regression test
      nsIFrameDebug* fdbg;
      nsIFrame* root;
      mPrintPresShell->GetRootFrame(&root);

      if (NS_SUCCEEDED(root->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**) &fdbg))) {
        fdbg->DumpRegressionData(mPrintPresContext, mFilePointer, 0, PR_TRUE);
      }
      fclose(mFilePointer);      
    } else {
      nsIFrame* rootFrame;
      mPrintPresShell->GetRootFrame(&rootFrame);

#if defined(DEBUG_rods) || defined(DEBUG_dcone)
      DumpLayoutData(mPrintPresContext, mPrintDocDC, rootFrame, webShell);
#endif

      if (printService) {
        // get the document title
        const nsString* docTitle = document->GetDocumentTitle();
        if( docTitle != nsnull) {
          PRUnichar * docStr = docTitle->ToNewUnicode();
          printService->SetTitle(docStr);
          nsMemory::Free(docStr);
        }

        // Get Document URL String
        nsCOMPtr<nsIURI> url(getter_AddRefs(document->GetDocumentURL()));
        char * urlCStr;
        url->GetSpec(&urlCStr);
        nsAutoString urlStr;
        urlStr.AssignWithConversion(urlCStr);
        PRUnichar * urlUStr = urlStr.ToNewUnicode();
        printService->SetDocURL(urlUStr);
        nsMemory::Free(urlUStr);
        nsMemory::Free(urlCStr);

        if (nsIPrintOptions::kRangeSelection == printRangeType) {
          mPrintPresContext->SetIsRenderingOnlySelection(PR_TRUE);

          // temporarily creating rendering context
          // which is needed to dinf the selection frames
          nsCOMPtr<nsIRenderingContext> rc;
          mPrintDocDC->CreateRenderingContext(*getter_AddRefs(rc));

          // find the starting and ending page numbers
          // via the selection
          nsIFrame* startFrame;
          nsIFrame* endFrame;
          PRInt32   startPageNum;
          PRInt32   endPageNum;
          nsRect    startRect;
          nsRect    endRect;
          rv = GetPageRangeForSelection(mPrintPresShell, mPrintPresContext, *rc, selectionPS, pageSequence, 
                                        &startFrame, startPageNum, startRect, 
                                        &endFrame, endPageNum, endRect);
          if (NS_SUCCEEDED(rv)) {
            printService->SetStartPageRange(startPageNum);
            printService->SetEndPageRange(endPageNum);
            if (startPageNum == endPageNum) {
              nsIFrame * seqFrame;
              if (NS_FAILED(pageSequence->QueryInterface(NS_GET_IID(nsIFrame), (void **)&seqFrame))) {
                return NS_ERROR_FAILURE;
              }
              nsRect rect(0,0,0,0);
              nsRect areaRect;
              nsIFrame * areaFrame = FindFrameByType(mPrintPresContext, startFrame, nsHTMLAtoms::body, rect, areaRect);
              if (areaFrame) {
                areaRect.y = areaRect.y - startRect.y;
                areaFrame->SetRect(mPrintPresContext, areaRect);
              }
            }
          }
        }

        nsIFrame * seqFrame;
        if (NS_FAILED(pageSequence->QueryInterface(NS_GET_IID(nsIFrame), (void **)&seqFrame))) {
          return NS_ERROR_FAILURE;
        }

        nsRect srect;
        seqFrame->GetRect(srect);

        nsRect r;
        rootView->GetBounds(r);
        r.height = srect.height;
        rootView->SetBounds(r);

        rootFrame->GetRect(r);
        r.height = srect.height;
        rootFrame->SetRect(mPrintPresContext, r);

        mPageSeqFrame = pageSequence;
        mPageSeqFrame->StartPrint(mPrintPresContext, printService);

        // Get the delay time in between the printing of each page
        // this gives the user more time to press cancel
        PRInt32 printPageDelay = 500;
        printService->GetPrintPageDelay(&printPageDelay);

        // SetupToPrintContent sets mTimerAlreadyStarted to PR_FALSE in
        // preparation for printing. Start only happens the first time
        // Then we must re-init it with the new mPrintPresContext 
        if (!mTimerAlreadyStarted) {
          mTimerAlreadyStarted = PR_TRUE;
          StartPagePrintTimer(mPrintPresContext, printService, printPageDelay);
        } else {
           mPagePrintTimer->Init(this, mPrintPresContext, printService, printPageDelay);
        }
      } else {
        // not sure what to do here!
        return NS_ERROR_FAILURE;
      }
    }
  }

  if (printSelectedIFrame) {
    printService->SetPrintRange(nsIPrintOptions::kRangeSelection);
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
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
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
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
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

  rv = nsComponentManager::CreateInstance(kStyleSetCID,nsnull,NS_GET_IID(nsIStyleSet),(void**)aStyleSet);
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
  return NS_OK;
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

  // This SetFocus is necessary so the Arrow Key and Page Key events
  // go to the scrolled view as soon as the Window is created instead of going to
  // the browser window (this enables keyboard scrolling of the document)
  // mWindow->SetFocus();

  return rv;
}

nsresult DocumentViewerImpl::GetDocumentSelection(nsISelection **aSelection, nsIPresShell * aPresShell)
{
  if (aPresShell == nsnull) {
    if (!mPresShell) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    aPresShell = mPresShell;
  }
  if (!aSelection) return NS_ERROR_NULL_POINTER;
  if (!aPresShell) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsISelectionController> selcon;
  selcon = do_QueryInterface(aPresShell);
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


nsresult DocumentViewerImpl::DocumentReadyForPrinting()
{
  nsresult rv = NS_ERROR_FAILURE;
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

    // get the focused DOMWindow
    nsCOMPtr<nsIDOMWindowInternal> curFocusDOMWin = getter_AddRefs(FindFocusedDOMWindowInternal());
    nsCOMPtr<nsIDOMWindow> domWin(do_QueryInterface(curFocusDOMWin));
    
    //
    // Send the document to the printer...
    //
    rv = SetupToPrintContent(webContainer, mPrintDC, domWin);
    if (NS_FAILED(rv)) {
      // The print job was canceled or there was a problem
      // So remove all other documents from the print list
      mPrintDocList->Clear();
      DonePrintingPages();
    }
  }
  return rv;
}

NS_IMETHODIMP 
DocumentViewerImpl::SetTransformMediator(nsITransformMediator* aMediator)
{
  NS_ASSERTION(nsnull == mTransformMediator, "nsXMLDocument::SetTransformMediator(): \
    Cannot set a second transform mediator\n");
  mTransformMediator = aMediator;
  return NS_OK;
}

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

static NS_DEFINE_IID(kDeviceContextSpecFactoryCID, NS_DEVICE_CONTEXT_SPEC_FACTORY_CID);

nsresult DocumentViewerImpl::GetSelectionDocument(nsIDeviceContextSpec * aDevSpec, nsIDocument ** aNewDoc)
{
  //NS_ENSURE_ARG_POINTER(*aDevSpec);
  NS_ENSURE_ARG_POINTER(aNewDoc);

    // create document
  nsCOMPtr<nsIDocument> doc;
  nsresult rv = NS_NewHTMLDocument(getter_AddRefs(doc));
  if (NS_FAILED(rv)) { return rv; }
  if (!doc) { return NS_ERROR_NULL_POINTER; }

  nsCOMPtr<nsINodeInfoManager> nimgr;
  rv = doc->GetNodeInfoManager(*getter_AddRefs(nimgr));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nimgr->GetNodeInfo(nsHTMLAtoms::html, nsnull, kNameSpaceID_None,
                     *getter_AddRefs(nodeInfo));

  // create document content
  nsCOMPtr<nsIHTMLContent> htmlElement;
  nsCOMPtr<nsIHTMLContent> headElement;
  nsCOMPtr<nsIHTMLContent> bodyElement;
    // create the root
  rv = NS_NewHTMLHtmlElement(getter_AddRefs(htmlElement), nodeInfo);
  if (NS_FAILED(rv)) { return rv; }
  if (!htmlElement) { return NS_ERROR_NULL_POINTER; }
    // create the head

  nimgr->GetNodeInfo(NS_ConvertASCIItoUCS2("head"), nsnull,
                     kNameSpaceID_None, *getter_AddRefs(nodeInfo));

  rv = NS_NewHTMLHeadElement(getter_AddRefs(headElement), nodeInfo);
  if (NS_FAILED(rv)) { return rv; }
  if (!headElement) { return NS_ERROR_NULL_POINTER; }
  headElement->SetDocument(doc, PR_FALSE, PR_TRUE);
    // create the body

  nimgr->GetNodeInfo(nsHTMLAtoms::body, nsnull, kNameSpaceID_None,
                    *getter_AddRefs(nodeInfo));

  rv = NS_NewHTMLBodyElement(getter_AddRefs(bodyElement), nodeInfo);
  if (NS_FAILED(rv)) { return rv; }
  if (!bodyElement) { return NS_ERROR_NULL_POINTER; }
  bodyElement->SetDocument(doc, PR_FALSE, PR_TRUE);
    // put the head and body into the root
  rv = htmlElement->AppendChildTo(headElement, PR_FALSE);
  if (NS_FAILED(rv)) { return rv; }
  rv = htmlElement->AppendChildTo(bodyElement, PR_FALSE);
  if (NS_FAILED(rv)) { return rv; }
  
  // load the document into the docshell
  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(doc);
  if (!domDoc) { return NS_ERROR_NULL_POINTER; }
  nsCOMPtr<nsIDOMElement> htmlDOMElement = do_QueryInterface(htmlElement);
  if (!htmlDOMElement) { return NS_ERROR_NULL_POINTER; }

  //nsCOMPtr<nsIContent> rootContent(do_QueryInterface(htmlElement));
  //doc->SetRootContent(rootContent);

  *aNewDoc = doc.get();
  NS_ADDREF(*aNewDoc);

  return rv;

}

PRBool 
DocumentViewerImpl::IsThereAnIFrameSelected(nsIWebShell*           aWebShell, 
                                            nsIDOMWindowInternal * aDOMWin,
                                            PRBool&                aDoesContainFrameset)
{
  aDoesContainFrameset = DoesContainFrameSet(aWebShell);
  PRBool iFrameIsSelected = PR_FALSE;
  // First, check to see if we are a frameset
  if (!aDoesContainFrameset) {
    // Check to see if there is a currenlt focused frame
    // if so, it means the selected frame is either the main webshell
    // or an IFRAME
    if (aDOMWin != nsnull) {
      // Get the main webshell's DOMWin to see if it matches 
      // the frame that is selected
      nsIDOMWindow* theDOMWindow = GetDOMWindowForThisDV();                                                
      if (aDOMWin != nsnull && theDOMWindow != aDOMWin) {
        // we have a selected IFRAME
        iFrameIsSelected = PR_TRUE;
      }
      NS_IF_RELEASE(theDOMWindow);
    }
  }
  return iFrameIsSelected;
}


PRBool 
DocumentViewerImpl::IsThereASelection(nsIDOMWindowInternal * aDOMWin)
{
  nsCOMPtr<nsIPresShell> presShell;
  if (aDOMWin != nsnull) {
    nsCOMPtr<nsIScriptGlobalObject> scriptObj(do_QueryInterface(aDOMWin));
    nsCOMPtr<nsIDocShell> docShell;
    scriptObj->GetDocShell(getter_AddRefs(docShell));
    docShell->GetPresShell(getter_AddRefs(presShell));
  }

  // check here to see if there is a range selection
  // so we know whether to turn on the "Selection" radio button
  nsCOMPtr<nsISelection> selection;
  GetDocumentSelection(getter_AddRefs(selection), presShell);
  if (selection) {
    PRInt32 count;
    selection->GetRangeCount(&count);
    if (count == 1) {
      nsCOMPtr<nsIDOMRange> range;
      if (NS_SUCCEEDED(selection->GetRangeAt(0, getter_AddRefs(range)))) {
        // check to make sure it isn't an insertion selection
        PRBool isCollapsed;
        selection->GetIsCollapsed(&isCollapsed);
        return !isCollapsed;
      }
    }
  }
  return PR_FALSE;
}

/** ---------------------------------------------------
 *  See documentation above in the nsIContentViewerfile class definition
 *	@update 01/24/00 dwc
 */
NS_IMETHODIMP
DocumentViewerImpl::Print(PRBool aSilent,FILE *aFile, nsIPrintListener *aPrintListener)
{
  if (aPrintListener) {
    mPrintListener = aPrintListener;
  }

  nsCOMPtr<nsIDOMWindowInternal> curFocusDOMWin = getter_AddRefs(FindFocusedDOMWindowInternal());

  // Get the webshell for this documentviewer
  nsCOMPtr<nsIWebShell> webContainer(do_QueryInterface(mContainer));

  // first check to see if there is a "regular" selection
  PRBool isSelection = IsThereASelection(curFocusDOMWin);

  // Get whether the doc contains a frameset 
  // Also, check to see if the currently focus webshell 
  // is a child of this webshell
  PRBool doesContainFrameSet;
  PRBool isIFrameSelection = IsThereAnIFrameSelected(webContainer, curFocusDOMWin, doesContainFrameSet);

  // Setup print options for UI
  nsresult  rv = NS_ERROR_FAILURE;
  NS_WITH_SERVICE(nsIPrintOptions, printService, kPrintOptionsCID, &rv);
  if (NS_SUCCEEDED(rv) && printService) {
    if (doesContainFrameSet) {
      if (curFocusDOMWin) {
        printService->SetHowToEnableFrameUI(nsIPrintOptions::kFrameEnableAll);
      } else {
        printService->SetHowToEnableFrameUI(nsIPrintOptions::kFrameEnableAsIsAndEach);
      }
    } else {
      printService->SetHowToEnableFrameUI(nsIPrintOptions::kFrameEnableNone);
    }
    // Now determine how to set up the Frame print UI
    printService->SetPrintOptions(nsIPrintOptions::kPrintOptionsEnableSelectionRB, isSelection || isIFrameSelection);
  }

  nsCOMPtr<nsIDeviceContextSpecFactory> factory;
  nsComponentManager::CreateInstance(kDeviceContextSpecFactoryCID, 
                                     nsnull,
                                     NS_GET_IID(nsIDeviceContextSpecFactory),
                                     (void **)getter_AddRefs(factory));

  if (factory) {

#ifdef DEBUG_dcone
    printf("PRINT JOB STARTING\n");
#endif

    nsIDeviceContextSpec *devspec = nsnull;
    nsCOMPtr<nsIDeviceContext> dx;
    mPrintDC = nsnull;
    mFilePointer = aFile;

    factory->CreateDeviceContextSpec(mWindow, devspec, aSilent);
    if (nsnull != devspec) {
      mPresContext->GetDeviceContext(getter_AddRefs(dx));
      rv = dx->GetDeviceContextFor(devspec, mPrintDC); 
      if (NS_SUCCEEDED(rv)) {

        NS_RELEASE(devspec);

        if(webContainer) {
          // load the document and do the initial reflow on the entire document
          nsCOMPtr<nsIPrintContext> printcon(do_CreateInstance(kPrintContextCID,&rv));
          if (NS_FAILED(rv)) {
            return rv;
          } else {
            rv = printcon->QueryInterface(NS_GET_IID(nsIPresContext), (void**)&mPrintPC);
            if (NS_FAILED(rv)) {
              return rv;
            }
          }
          

          PRInt32 width, height;
          mPrintDC->GetDeviceSurfaceDimensions(width,height);
          // XXX - Hack Alert
          // OK,  so ther eis a selection, we will print the entire selection 
          // on one page and then crop the page.
          // This means you can never print any selection that is longer than one page
          // put it keeps it from page breaking in the middle of your print of the selection
          if (isSelection) {
            height = 0x0FFFFFFF;
          }

          mPrintPC->Init(mPrintDC);
          mPrintPC->SetContainer(webContainer);
          CreateStyleSet(mDocument,&mPrintSS);

          rv = nsComponentManager::CreateInstance(kPresShellCID, nsnull, NS_GET_IID(nsIPresShell),(void**)&mPrintPS);
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
          if (mPrintListener) {
            mPrintListener->OnStartPrinting();    
          }

          //
          // The mIsPrinting flag is set when the ImageGroup observer is
          // notified that images must be loaded as a result of the 
          // InitialReflow...
          //
          if(!mIsPrinting){
            rv = DocumentReadyForPrinting();
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
      if (mPrintListener) {
        mPrintListener->OnStartPrinting();    
        mPrintListener->OnEndPrinting(NS_ERROR_FAILURE);    
      }
      rv = NS_ERROR_FAILURE;
    }

  }

  return rv;
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
   NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
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
   NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);

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
    
    domWindow->UpdateCommands(NS_LITERAL_STRING("select"));
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


PRBool 
DocumentViewerImpl::IsWindowsInOurSubTree(nsIDOMWindowInternal * aDOMWindow)
{
  PRBool found = PR_FALSE;
  if(aDOMWindow) {
    // now check to make sure it is in "our" tree of webshells
    nsCOMPtr<nsIScriptGlobalObject> scriptObj(do_QueryInterface(aDOMWindow));
    if (scriptObj) {
      nsCOMPtr<nsIDocShell> docShell;
      scriptObj->GetDocShell(getter_AddRefs(docShell));
      if (docShell) {
        nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));
        if (docShellAsItem) {
          // get this DocViewer webshell
          nsCOMPtr<nsIWebShell> thisDVWebShell(do_QueryInterface(mContainer));
          while (!found) {
            nsCOMPtr<nsIDocShellTreeItem> docShellParent;
            docShellAsItem->GetSameTypeParent(getter_AddRefs(docShellParent));
            if (docShellParent) {
              nsCOMPtr<nsIWebShell> parentWebshell(do_QueryInterface(docShellParent));
              if (parentWebshell) {
                if (parentWebshell.get() == thisDVWebShell.get()) {
                  found = PR_TRUE;
                  break;
                }
              }
            } else {
              break; // at top of tree
            }
            docShellAsItem = docShellParent;
          } // while
        }
      } // docshell
    } // scriptobj
  } // domWindow

  return found;
}

/** ---------------------------------------------------
 *  Get the Focused Frame for a documentviewer
 *  
 */
nsIDOMWindowInternal*
DocumentViewerImpl::FindFocusedDOMWindowInternal()
{
  nsCOMPtr<nsIDOMWindowInternal>  theDOMWin;
  nsCOMPtr<nsIDocument>           theDoc;
  nsCOMPtr<nsIScriptGlobalObject> theSGO;
  nsCOMPtr<nsIFocusController>    focusController;
  nsIDOMWindowInternal *          domWin = nsnull;

  this->GetDocument(*getter_AddRefs(theDoc));  
  if(theDoc){
    theDoc->GetScriptGlobalObject(getter_AddRefs(theSGO));
    if(theSGO){
      nsCOMPtr<nsPIDOMWindow> theDOMWindow = do_QueryInterface(theSGO);
      if(theDOMWindow){
        theDOMWindow->GetRootFocusController(getter_AddRefs(focusController));
        if(focusController){
          focusController->GetFocusedWindow(getter_AddRefs(theDOMWin));
          domWin = theDOMWin.get();
          if(domWin != nsnull) {
            if (IsWindowsInOurSubTree(domWin)){
              NS_ADDREF(domWin);
            } else {
              domWin = nsnull;
            }
          }
        }
      }
    }
  }
  return domWin;
}

/** ---------------------------------------------------
 *  Get DOM Window represented by the document viewer
 *  
 */
nsIDOMWindow*
DocumentViewerImpl::GetDOMWindowForThisDV()
{
  nsCOMPtr<nsIDocument>           theDoc;
  nsCOMPtr<nsIScriptGlobalObject> theSGO;
  nsCOMPtr<nsIFocusController>    focusController;
  nsIDOMWindow *                  domWin = nsnull;

  this->GetDocument(*getter_AddRefs(theDoc)); 
  if (theDoc){ 
    theDoc->GetScriptGlobalObject(getter_AddRefs(theSGO));
    if (theSGO){
      nsCOMPtr<nsIDOMWindow> theDOMWindow = do_QueryInterface(theSGO);
      if(theDOMWindow){
        domWin = theDOMWindow.get();
        NS_ADDREF(domWin);
      }
    }
  }

  return domWin;
}

/*=============== Timer Related Code ======================*/
nsresult
DocumentViewerImpl::StartPagePrintTimer(nsIPresContext * aPresContext, 
                                        nsIPrintOptions* aPrintOptions,
                                        PRUint32         aDelay)
{
  nsresult result;

  if (!mPagePrintTimer) {
    result = NS_NewUpdateTimer(&mPagePrintTimer);

    if (NS_FAILED(result))
      return result;
  }

  return mPagePrintTimer->Start(this, aPresContext, aPrintOptions, aDelay);
}

inline void
DocumentViewerImpl::StopPagePrintTimer()
{
  if (mPagePrintTimer) {
     mPagePrintTimer->Stop();
  }
}


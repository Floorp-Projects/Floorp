/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nslayout.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsISupports.h"
#include "nsIContentViewerContainer.h"
#include "nsIDocumentViewer.h"
#include "nsIDocumentLoaderObserver.h"

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


static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);

#ifdef NS_DEBUG
#undef NOISY_VIEWER
#else
#undef NOISY_VIEWER
#endif

class DocumentViewerImpl : public nsIDocumentViewer, public nsIDocumentLoaderObserver
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
  NS_IMETHOD SetContainer(nsIContentViewerContainer* aContainer);
  NS_IMETHOD GetContainer(nsIContentViewerContainer*& aContainerResult);
  NS_IMETHOD Stop(void);
  NS_IMETHOD GetBounds(nsRect& aResult);
  NS_IMETHOD SetBounds(const nsRect& aBounds);
  NS_IMETHOD Move(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD Show();
  NS_IMETHOD Hide();
  NS_IMETHOD Print(void);
  NS_IMETHOD PrintContent(nsIWebShell  *aParent,nsIDeviceContext *aDContext);
  NS_IMETHOD SetEnableRendering(PRBool aOn);
  NS_IMETHOD GetEnableRendering(PRBool* aResult);

  // nsIDocumentViewer interface...
  NS_IMETHOD SetUAStyleSheet(nsIStyleSheet* aUAStyleSheet);
  NS_IMETHOD GetDocument(nsIDocument*& aResult);
  NS_IMETHOD GetPresShell(nsIPresShell*& aResult);
  NS_IMETHOD GetPresContext(nsIPresContext*& aResult);
  NS_IMETHOD CreateDocumentViewerUsing(nsIPresContext* aPresContext,
                                       nsIDocumentViewer*& aResult);

  // nsIDocumentLoaderObserver interface
  NS_IMETHOD OnStartDocumentLoad(nsIDocumentLoader* loader, nsIURI* aURL, const char* aCommand) {return NS_OK;}

#ifndef NECKO
    NS_IMETHOD OnEndDocumentLoad(nsIDocumentLoader* loader, nsIURI *aUrl, PRInt32 aStatus,nsIDocumentLoaderObserver * aObserver){return NS_OK;}
#else
    NS_IMETHOD OnEndDocumentLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus,nsIDocumentLoaderObserver * aObserver){return NS_OK;}
#endif // NECKO

#ifndef NECKO
    NS_IMETHOD OnStartURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, const char* aContentType,nsIContentViewer* aViewer);
#else
    NS_IMETHOD OnStartURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsIContentViewer* aViewer);
#endif // NECKO

#ifndef NECKO
    NS_IMETHOD OnProgressURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, PRUint32 aProgress,PRUint32 aProgressMax){return NS_OK;}
#else
    NS_IMETHOD OnProgressURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, PRUint32 aProgress,PRUint32 aProgressMax){return NS_OK;}
#endif // NECKO

#ifndef NECKO
    NS_IMETHOD OnStatusURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, nsString& aMsg){return NS_OK;}
#else
    NS_IMETHOD OnStatusURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsString& aMsg){return NS_OK;}
#endif // NECKO

#ifndef NECKO
    NS_IMETHOD OnEndURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, PRInt32 aStatus);
#else
    NS_IMETHOD OnEndURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus);
#endif // NECKO

#ifndef NECKO
    NS_IMETHOD HandleUnknownContentType(nsIDocumentLoader* loader,nsIURI *aURL,const char *aContentType,const char *aCommand ){return NS_OK;}
#else
    NS_IMETHOD HandleUnknownContentType(nsIDocumentLoader* loader,nsIChannel* channel,const char *aContentType,const char *aCommand ){return NS_OK;}
#endif // NECKO

protected:
  virtual ~DocumentViewerImpl();

private:
  void ForceRefresh(void);
  nsresult CreateStyleSet(nsIDocument* aDocument, nsIStyleSet** aStyleSet);
  nsresult MakeWindow(nsNativeWidget aNativeParent,
                      const nsRect& aBounds,
                      nsScrollPreference aScrolling);

protected:
  // IMPORTANT: The ownership implicit in the following member
  // variables has been explicitly checked and set using nsCOMPtr
  // for owning pointers and raw COM interface pointers for weak
  // (ie, non owning) references. If you add any members to this
  // class, please make the ownership explicit (pinkerton, scc).
  
  nsIContentViewerContainer* mContainer; // [WEAK] it owns me!
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
  nsIImageGroup     *mImageGroup;

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
DocumentViewerImpl::SetContainer(nsIContentViewerContainer* aContainer)
{
  mContainer = aContainer;
  if (mPresContext) {
    mPresContext->SetContainer(aContainer);
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetContainer(nsIContentViewerContainer*& aResult)
{
  aResult = mContainer;
  NS_IF_ADDREF(aResult);
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

  if (nsnull != mContainer) {
    nsILinkHandler* linkHandler = nsnull;
    mContainer->QueryCapability(kILinkHandlerIID, (void**)&linkHandler);
    mPresContext->SetContainer(mContainer);
    mPresContext->SetLinkHandler(linkHandler);
    NS_IF_RELEASE(linkHandler);

    // Set script-context-owner in the document
    nsIScriptContextOwner* owner = nsnull;
    mContainer->QueryCapability(kIScriptContextOwnerIID, (void**)&owner);
    if (nsnull != owner) {
      mDocument->SetScriptContextOwner(owner);
      nsIScriptGlobalObject* global;
      rv = owner->GetScriptGlobalObject(&global);
      if (NS_SUCCEEDED(rv) && (nsnull != global)) {
        nsIDOMDocument *domdoc = nsnull;
        mDocument->QueryInterface(kIDOMDocumentIID,
                                  (void**) &domdoc);
        if (nsnull != domdoc) {
          global->SetNewDocument(domdoc);
          NS_RELEASE(domdoc);
        }
        NS_RELEASE(global);
      }
      NS_RELEASE(owner);
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

static NS_DEFINE_IID(kIDeviceContextSpecFactoryIID, NS_IDEVICE_CONTEXT_SPEC_FACTORY_IID);
static NS_DEFINE_IID(kDeviceContextSpecFactoryCID, NS_DEVICE_CONTEXT_SPEC_FACTORY_CID);


/** ---------------------------------------------------
 *  See documentation above in the DocumentViewerImpl class definition
 *	@update 07/09/99 dwc
 */
NS_IMETHODIMP
DocumentViewerImpl::Print(void)
{
nsIContentViewerContainer   *containerResult;
nsIWebShell                 *webContainer;
nsIDeviceContextSpecFactory *factory = nsnull;
PRInt32                     width,height;
nsIPref                     *prefs;
PRBool                      isBusy;

  nsComponentManager::CreateInstance(kDeviceContextSpecFactoryCID, nsnull,kIDeviceContextSpecFactoryIID,(void **)&factory);

  if (nsnull != factory) {
    nsIDeviceContextSpec *devspec = nsnull;
    nsCOMPtr<nsIDeviceContext> dx;
    //nsIDeviceContext *newdx = nsnull;
    mPrintDC = nsnull;
    factory->CreateDeviceContextSpec(nsnull, devspec, PR_FALSE);
    if (nsnull != devspec) {
      mPresContext->GetDeviceContext(getter_AddRefs(dx));
      nsresult rv = dx->GetDeviceContextFor(devspec, mPrintDC); 
      if (NS_SUCCEEDED(rv)) {

        NS_RELEASE(devspec);

        // Get the webshell for this documentviewer
        rv = this->GetContainer(containerResult);
        containerResult->QueryInterface(kIWebShellIID,(void**)&webContainer);
        if(webContainer) {
          webContainer->SetDocLoaderObserver(this);

          // load the document and do the initial reflow on the entire document
          rv = NS_NewPrintContext(&mPrintPC);
          if(NS_FAILED(rv)){
            return rv;
          }

          mPrintDC->GetDeviceSurfaceDimensions(width,height);
          mPresContext->GetPrefs(&prefs);
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
          mPrintPS->InitialReflow(width,height);

          // check NETLIB to see if something was kicked off, 
          webContainer->IsBusy(isBusy);
          
          // if this did not fire any loads, then continue printing
          if(!isBusy){
            PrintContent(webContainer,mPrintDC);
            webContainer->SetDocLoaderObserver(0);
            mPrintPS->EndObservingDocument();
            NS_RELEASE(mPrintPS);
            NS_RELEASE(mPrintVM);
            NS_RELEASE(mPrintSS);
            NS_RELEASE(mPrintDC);
          } else {
            // use the observer mechanism to finish the printing
            mIsPrinting = PR_TRUE;
            mNumURLStarts = 0;
          }
        }
      }
    }
    NS_RELEASE(factory);
  }
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation above in the DocumentViewerImpl class definition
 *	@update 07/09/99 dwc
 */
NS_IMETHODIMP
DocumentViewerImpl::PrintContent(nsIWebShell  *aParent,nsIDeviceContext *aDContext)
{
nsIStyleSet       *ss;
nsIPref           *prefs;
nsIViewManager    *vm;
PRInt32           width, height;
nsIView           *view;
nsresult          rv;
PRInt32           count,i;
nsIWebShell       *childWebShell;
nsIContentViewer  *viewer;
  

  aParent->GetChildCount(count);
  if(count> 0) { 
    for(i=0;i<count;i++) {
      aParent->ChildAt(i,childWebShell);
      childWebShell->GetContentViewer(&viewer);
      viewer->PrintContent(childWebShell,aDContext);
    }
  } else {
    aDContext->BeginDocument();
    aDContext->GetDeviceSurfaceDimensions(width, height);

    nsIPresContext *cx;
    rv = NS_NewPrintContext(&cx);
    if (NS_FAILED(rv)) {
      return rv;
    }
    mPresContext->GetPrefs(&prefs);
    cx->Init(aDContext, prefs);

    nsCompatibility mode;
    mPresContext->GetCompatibilityMode(&mode);
    cx->SetCompatibilityMode(mode);

    CreateStyleSet(mDocument, &ss);

    nsIPresShell *ps;
    rv = NS_NewPresShell(&ps);
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = nsComponentManager::CreateInstance(kViewManagerCID,nsnull,kIViewManagerIID,(void **)&vm);
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
    NS_RELEASE(ps);
    NS_RELEASE(vm);
    NS_RELEASE(ss);
    NS_IF_RELEASE(prefs); // XXX why is the prefs null??
    }
  return NS_OK;

}

/** ---------------------------------------------------
 *  See documentation above in the DocumentViewerImpl class definition
 *	@update 07/09/99 dwc
 */
NS_IMETHODIMP 
#ifdef NECKO
DocumentViewerImpl::OnStartURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsIContentViewer* aViewer)
#else
DocumentViewerImpl::OnStartURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, const char* aContentType,nsIContentViewer* aViewer)
#endif
{
  mNumURLStarts++;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation above in the DocumentViewerImpl class definition
 *	@update 07/09/99 dwc
 */
NS_IMETHODIMP 
#ifdef NECKO
DocumentViewerImpl::OnEndURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus)
#else
DocumentViewerImpl::OnEndURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, PRInt32 aStatus)
#endif // NECKO
{
  nsIContentViewerContainer *containerResult;
  nsIWebShell               *webContainer;
  nsresult                  rv;

    mNumURLStarts--;

    if((mIsPrinting==PR_TRUE) && (mNumURLStarts == 0)) {
      rv = this->GetContainer(containerResult);
      containerResult->QueryInterface(kIWebShellIID,(void**)&webContainer);
      if(webContainer) {
        PrintContent(webContainer,mPrintDC);

        // printing is complete, clean up now
        mIsPrinting = PR_FALSE;
        webContainer->SetDocLoaderObserver(0);

        mPrintPS->EndObservingDocument();
        NS_RELEASE(mPrintPS);
        NS_RELEASE(mPrintVM);
        NS_RELEASE(mPrintSS);
        NS_RELEASE(mPrintDC);
      }
    }

  return NS_OK;
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
        mViewManager->UpdateView(view,nsnull,NS_VMREFRESH_IMMEDIATE); 
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
      nsIStyleSheet* sheet = aDocument->GetStyleSheetAt(index);
      (*aStyleSet)->AddDocStyleSheet(sheet, aDocument);
      NS_RELEASE(sheet);
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

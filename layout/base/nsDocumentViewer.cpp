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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Rosen <dr@netscape.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsISupports.h"
#include "nsIContent.h"
#include "nsIContentViewerContainer.h"
#include "nsIDocumentViewer.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDocumentViewerPrint.h"

#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsICSSStyleSheet.h"
#include "nsIFrame.h"

#include "nsIScriptGlobalObject.h"
#include "nsILinkHandler.h"
#include "nsIDOMDocument.h"
#include "nsISelectionListener.h"
#include "nsISelectionPrivate.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMNSHTMLDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMRange.h"
#include "nsContentCID.h"
#include "nsLayoutCID.h"
#include "nsGenericHTMLElement.h"
#include "nsLayoutStylesheetCache.h"

#include "nsViewsCID.h"
#include "nsWidgetsCID.h"
#include "nsIDeviceContext.h"
#include "nsIDeviceContextSpec.h"
#include "nsIDeviceContextSpecFactory.h"
#include "nsIViewManager.h"
#include "nsIView.h"

#include "nsIPageSequenceFrame.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIChromeEventHandler.h"
#include "nsIContentViewerEdit.h"
#include "nsIContentViewerFile.h"
#include "nsICSSLoader.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShell.h"
#include "nsIBaseWindow.h"
#include "nsIFrameDebug.h"
#include "nsILayoutHistoryState.h"
#include "nsLayoutAtoms.h"
#include "nsIParser.h"
#include "nsIPrintContext.h"
#include "nsGUIEvent.h"
#include "nsHTMLReflowState.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIImageLoadingContent.h"
#include "nsCopySupport.h"
#include "nsIDOMHTMLFrameSetElement.h"
#ifdef MOZ_XUL
#include "nsIXULDocument.h"  // Temporary code for Bug 136185
#endif

#include "nsIClipboardHelper.h"

#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"

#include "nsPIDOMWindow.h"
#include "nsIFocusController.h"

#include "nsIScrollableView.h"
#include "nsIScrollable.h"
#include "nsITimelineService.h"
#include "nsGfxCIID.h"

// Printing
#include "nsIWebBrowserPrint.h"

//--------------------------
// Printing Include
//---------------------------
#ifdef NS_PRINTING

#include "nsPrintEngine.h"

// Print Options
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsService.h"
#include "nsIPrintOptions.h"
#include "nsIServiceManager.h"
#include "nsHTMLAtoms.h" // XXX until atoms get factored into nsLayoutAtoms
#include "nsISimpleEnumerator.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"

// PrintOptions is now implemented by PrintSettingsService
static const char sPrintSettingsServiceContractID[] = "@mozilla.org/gfx/printsettings-service;1";
static const char sPrintOptionsContractID[]         = "@mozilla.org/gfx/printsettings-service;1";

// Printing Events
#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsPrintPreviewListener.h"

#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIPluginDocument.h"

// Print Preview
#include "nsIPrintPreviewContext.h"
#include "imgIContainer.h" // image animation mode constants

// Print Progress
#include "nsIPrintProgress.h"
#include "nsIPrintProgressParams.h"

// Print error dialog
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"
#include "nsIStringBundle.h"

// Printing 
#include "nsPrintEngine.h"
#include "nsPagePrintTimer.h"

#endif // NS_PRINTING

// FrameSet
#include "nsINodeInfo.h"
#include "nsIDocument.h"
#include "nsHTMLAtoms.h"
#include "nsIWebShell.h"

//focus
#include "nsIDOMEventReceiver.h"
#include "nsIDOMFocusListener.h"
#include "nsISelectionController.h"

#include "nsBidiUtils.h"

//paint forcing
#include "prenv.h"
#include <stdio.h>

static NS_DEFINE_CID(kGalleyContextCID,  NS_GALLEYCONTEXT_CID);

static const char kDOMStringBundleURL[] =
  "chrome://communicator/locale/dom/dom.properties";


#ifdef NS_DEBUG

#undef NOISY_VIEWER
#else
#undef NOISY_VIEWER
#endif

//-----------------------------------------------------
// PR LOGGING
#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif

#include "prlog.h"

#ifdef PR_LOGGING

static PRLogModuleInfo * kPrintingLogMod = PR_NewLogModule("printing");
#define PR_PL(_p1)  PR_LOG(kPrintingLogMod, PR_LOG_DEBUG, _p1);

#define PRT_YESNO(_p) ((_p)?"YES":"NO")
#else
#define PRT_YESNO(_p)
#define PR_PL(_p1)
#endif
//-----------------------------------------------------

class DocumentViewerImpl;

// New PrintPreview
static NS_DEFINE_CID(kPrintPreviewContextCID,  NS_PRINT_PREVIEW_CONTEXT_CID);

// a small delegate class used to avoid circular references

#ifdef XP_MAC
#pragma mark ** nsDocViewerSelectionListener **
#endif

class nsDocViewerSelectionListener : public nsISelectionListener
{
public:

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsISelectionListerner interface
  NS_DECL_NSISELECTIONLISTENER

                       nsDocViewerSelectionListener()
                       : mDocViewer(NULL)
                       , mGotSelectionState(PR_FALSE)
                       , mSelectionWasCollapsed(PR_FALSE)
                       {
                       }

  virtual              ~nsDocViewerSelectionListener() {}

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
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
  NS_IMETHOD Focus(nsIDOMEvent* aEvent);
  NS_IMETHOD Blur(nsIDOMEvent* aEvent);
/*END implementations of focus event handler interface*/
  nsresult             Init(DocumentViewerImpl *aDocViewer);

private:
    DocumentViewerImpl*  mDocViewer;
};



#ifdef XP_MAC
#pragma mark ** DocumentViewerImpl **
#endif

//-------------------------------------------------------------
class DocumentViewerImpl : public nsIDocumentViewer,
                           public nsIContentViewerEdit,
                           public nsIContentViewerFile,
                           public nsIMarkupDocumentViewer,
                           public nsIWebBrowserPrint,
                           public nsIDocumentViewerPrint
{
  friend class nsDocViewerSelectionListener;
  friend class nsPagePrintTimer;
  friend class nsPrintEngine;

public:
  DocumentViewerImpl(nsIPresContext* aPresContext);

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIContentViewer interface...
  NS_DECL_NSICONTENTVIEWER

  // nsIDocumentViewer interface...
  NS_IMETHOD SetUAStyleSheet(nsIStyleSheet* aUAStyleSheet);
  NS_IMETHOD GetDocument(nsIDocument** aResult);
  NS_IMETHOD GetPresShell(nsIPresShell** aResult);
  NS_IMETHOD GetPresContext(nsIPresContext** aResult);
  NS_IMETHOD CreateDocumentViewerUsing(nsIPresContext* aPresContext,
                                       nsIDocumentViewer** aResult);

  // nsIContentViewerEdit
  NS_DECL_NSICONTENTVIEWEREDIT

  // nsIContentViewerFile
  NS_DECL_NSICONTENTVIEWERFILE

  // nsIMarkupDocumentViewer
  NS_DECL_NSIMARKUPDOCUMENTVIEWER

  // nsIWebBrowserPrint
  NS_DECL_NSIWEBBROWSERPRINT

  typedef void (*CallChildFunc)(nsIMarkupDocumentViewer* aViewer,
                                void* aClosure);
  nsresult CallChildren(CallChildFunc aFunc, void* aClosure);

  // nsIDocumentViewerPrint Printing Methods
  NS_DECL_NSIDOCUMENTVIEWERPRINT

protected:
  virtual ~DocumentViewerImpl();

private:
  void ForceRefresh(void);
  nsresult MakeWindow(nsIWidget* aParentWidget,
                      const nsRect& aBounds);
  nsresult InitInternal(nsIWidget* aParentWidget,
                        nsIDeviceContext* aDeviceContext,
                        const nsRect& aBounds,
                        PRBool aDoCreation,
                        PRBool aInPrintPreview);
  nsresult InitPresentationStuff(PRBool aDoInitialReflow);

  nsresult GetPopupNode(nsIDOMNode** aNode);
  nsresult GetPopupLinkNode(nsIDOMNode** aNode);
  nsresult GetPopupImageNode(nsIImageLoadingContent** aNode);

  void PrepareToStartLoad(void);

  nsresult SyncParentSubDocMap();

#ifdef NS_PRINTING
  // Called when the DocViewer is notified that the state
  // of Printing or PP has changed
  void SetIsPrintingInDocShellTree(nsIDocShellTreeNode* aParentNode, 
                                   PRBool               aIsPrintingOrPP, 
                                   PRBool               aStartAtTop);
#endif // NS_PRINTING

protected:
  // IMPORTANT: The ownership implicit in the following member
  // variables has been explicitly checked and set using nsCOMPtr
  // for owning pointers and raw COM interface pointers for weak
  // (ie, non owning) references. If you add any members to this
  // class, please make the ownership explicit (pinkerton, scc).

  nsISupports* mContainer; // [WEAK] it owns me!
  nsCOMPtr<nsIDeviceContext> mDeviceContext;   // ??? can't hurt, but...

  // the following six items are explicitly in this order
  // so they will be destroyed in the reverse order (pinkerton, scc)
  nsCOMPtr<nsIDocument>    mDocument;
  nsCOMPtr<nsIWidget>      mWindow;      // ??? should we really own it?
  nsCOMPtr<nsIViewManager> mViewManager;
  nsCOMPtr<nsIPresContext> mPresContext;
  nsCOMPtr<nsIPresShell>   mPresShell;

  nsCOMPtr<nsIStyleSheet>  mUAStyleSheet;

  nsCOMPtr<nsISelectionListener> mSelectionListener;
  nsCOMPtr<nsIDOMFocusListener> mFocusListener;

  nsCOMPtr<nsIContentViewer> mPreviousViewer;

  PRPackedBool  mEnableRendering;
  PRPackedBool  mStopped;
  PRPackedBool  mLoaded;
  PRPackedBool  mDeferredWindowClose;
  PRInt16 mNumURLStarts;
  PRInt16 mDestroyRefCount;     // a second "refcount" for the document viewer's "destroy"

  nsIWidget* mParentWidget;          // purposely won't be ref counted

  PRPackedBool         mInPermitUnload;

#ifdef NS_PRINTING
  PRPackedBool          mClosingWhilePrinting;
  nsPrintEngine*        mPrintEngine;
  nsCOMPtr<nsIDOMWindowInternal> mDialogParentWin;
#if NS_PRINT_PREVIEW
  // These data member support delayed printing when the document is loading
  nsCOMPtr<nsIPrintSettings>       mCachedPrintSettings;
  nsCOMPtr<nsIWebProgressListener> mCachedPrintWebProgressListner;
  PRPackedBool                     mPrintIsPending;
  PRPackedBool                     mPrintDocIsFullyLoaded;
#endif // NS_PRINT_PREVIEW

#ifdef NS_DEBUG
  FILE* mDebugFile;
#endif // NS_DEBUG
#endif // NS_PRINTING

  /* character set member data */
  PRInt32 mHintCharsetSource;
  nsCString mHintCharset;
  nsCString mDefaultCharacterSet;
  nsCString mForceCharacterSet;
  nsCString mPrevDocCharacterSet;
  
  // document management data
  //   these items are specific to markup documents (html and xml)
  //   may consider splitting these out into a subclass
  PRPackedBool mAllowPlugins;
  PRPackedBool mIsSticky;

};

//------------------------------------------------------------------
// DocumentViewerImpl
//------------------------------------------------------------------
// Class IDs
static NS_DEFINE_CID(kViewManagerCID,       NS_VIEW_MANAGER_CID);
static NS_DEFINE_CID(kWidgetCID,            NS_CHILD_CID);
static NS_DEFINE_CID(kViewCID,              NS_VIEW_CID);

//------------------------------------------------------------------
nsresult
NS_NewDocumentViewer(nsIDocumentViewer** aResult)
{
  *aResult = new DocumentViewerImpl(nsnull);
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult);

  return NS_OK;
}

void DocumentViewerImpl::PrepareToStartLoad()
{
  mEnableRendering  = PR_TRUE;
  mStopped          = PR_FALSE;
  mLoaded           = PR_FALSE;
  mDeferredWindowClose = PR_FALSE;

#ifdef NS_PRINTING
  mPrintIsPending        = PR_FALSE;
  mPrintDocIsFullyLoaded = PR_FALSE;
  mClosingWhilePrinting  = PR_FALSE;

  // Make sure we have destroyed it and cleared the data member
  if (mPrintEngine) {
    mPrintEngine->Destroy();
    NS_RELEASE(mPrintEngine);
  }

#ifdef NS_PRINT_PREVIEW
  SetIsPrintPreview(PR_FALSE);
#endif

#ifdef NS_DEBUG
  mDebugFile = nsnull;
#endif

#endif // NS_PRINTING
}

// Note: operator new zeros our memory, so no need to init things to null.
DocumentViewerImpl::DocumentViewerImpl(nsIPresContext* aPresContext)
  : mPresContext(aPresContext),
    mHintCharsetSource(kCharsetUninitialized),
    mAllowPlugins(PR_TRUE),
    mIsSticky(PR_TRUE)
{
  PrepareToStartLoad();
}

NS_IMPL_ISUPPORTS7(DocumentViewerImpl,
                   nsIContentViewer,
                   nsIDocumentViewer,
                   nsIMarkupDocumentViewer,
                   nsIContentViewerFile,
                   nsIContentViewerEdit,
                   nsIWebBrowserPrint,
                   nsIDocumentViewerPrint)

DocumentViewerImpl::~DocumentViewerImpl()
{
  NS_ASSERTION(!mDocument, "User did not call nsIContentViewer::Close");
  if (mDocument) {
    Close();
  }

  NS_ASSERTION(!mPresShell && !mPresContext,
               "User did not call nsIContentViewer::Destroy");
  if (mPresShell || mPresContext) {
    Destroy();
  }

  // XXX(?) Revoke pending invalidate events
}

/*
 * This method is called by the Document Loader once a document has
 * been created for a particular data stream...  The content viewer
 * must cache this document for later use when Init(...) is called.
 *
 * This method is also called when an out of band document.write() happens.
 * In that case, the document passed in is the same as the previous document.
 */
NS_IMETHODIMP
DocumentViewerImpl::LoadStart(nsISupports *aDoc)
{
#ifdef NOISY_VIEWER
  printf("DocumentViewerImpl::LoadStart\n");
#endif

  nsresult rv;
  if (!mDocument) {
    mDocument = do_QueryInterface(aDoc, &rv);
  }
  else if (mDocument == aDoc) {
    // Reset the document viewer's state back to what it was
    // when the document load started.
    PrepareToStartLoad();
  }

  return rv;
}

nsresult
DocumentViewerImpl::SyncParentSubDocMap()
{
  nsCOMPtr<nsIDocShellTreeItem> item(do_QueryInterface(mContainer));
  nsCOMPtr<nsPIDOMWindow> pwin(do_GetInterface(item));
  nsCOMPtr<nsIContent> content;

  if (mDocument && pwin) {
    content = do_QueryInterface(pwin->GetFrameElementInternal());
  }

  if (content) {
    nsCOMPtr<nsIDocShellTreeItem> parent;
    item->GetParent(getter_AddRefs(parent));

    nsCOMPtr<nsIDOMWindow> parent_win(do_GetInterface(parent));

    if (parent_win) {
      nsCOMPtr<nsIDOMDocument> dom_doc;
      parent_win->GetDocument(getter_AddRefs(dom_doc));

      nsCOMPtr<nsIDocument> parent_doc(do_QueryInterface(dom_doc));

      if (parent_doc) {
        return parent_doc->SetSubDocumentFor(content, mDocument);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::SetContainer(nsISupports* aContainer)
{
  mContainer = aContainer;
  if (mPresContext) {
    mPresContext->SetContainer(aContainer);
  }

  // We're loading a new document into the window where this document
  // viewer lives, sync the parent document's frame element -> sub
  // document map

  return SyncParentSubDocMap();
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
  return InitInternal(aParentWidget, aDeviceContext, aBounds, PR_TRUE, PR_FALSE);
}

nsresult
DocumentViewerImpl::InitPresentationStuff(PRBool aDoInitialReflow)
{
  // Create the style set...
  nsStyleSet *styleSet;
  nsresult rv = CreateStyleSet(mDocument, &styleSet);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now make the shell for the document
  rv = mDocument->CreateShell(mPresContext, mViewManager, styleSet,
                              getter_AddRefs(mPresShell));
  if (NS_FAILED(rv)) {
    delete styleSet;
    return rv;
  }

  // We're done creating the style set
  styleSet->EndUpdate();

  if (aDoInitialReflow) {
    // Since InitialReflow() will create frames for *all* items
    // that are currently in the document tree, we need to flush
    // any pending notifications to prevent the content sink from
    // duplicating layout frames for content it has added to the tree
    // but hasn't notified the document about. (Bug 154018)
    //
    // Note that we are flushing before we add mPresShell as an observer
    // to avoid bogus notifications.

    mDocument->FlushPendingNotifications(Flush_ContentAndNotify);
  }

  mPresShell->BeginObservingDocument();

  // Initialize our view manager
  nsRect bounds;
  mWindow->GetBounds(bounds);

  float p2t;

  p2t = mPresContext->PixelsToTwips();

  nscoord width = NSIntPixelsToTwips(bounds.width, p2t);
  nscoord height = NSIntPixelsToTwips(bounds.height, p2t);

  mViewManager->DisableRefresh();
  mViewManager->SetWindowDimensions(width, height);

  // Setup default view manager background color

  // This may be overridden by the docshell with the background color
  // for the last document loaded into the docshell
  mViewManager->SetDefaultBackgroundColor(mPresContext->DefaultBackgroundColor());

  if (aDoInitialReflow) {
    nsCOMPtr<nsIScrollable> sc = do_QueryInterface(mContainer);

    if (sc) {
      nsCOMPtr<nsIDOMHTMLFrameSetElement> frameset(do_QueryInterface(mDocument->GetRootContent()));

      if (frameset) {
        // If this is a frameset (i.e. not a frame) then we never want
        // scrollbars on it, the scrollbars go inside the frames
        // inside the frameset...

        sc->SetCurrentScrollbarPreferences(nsIScrollable::ScrollOrientation_Y,
                                           NS_STYLE_OVERFLOW_HIDDEN);
        sc->SetCurrentScrollbarPreferences(nsIScrollable::ScrollOrientation_X,
                                           NS_STYLE_OVERFLOW_HIDDEN);
      } else {
        sc->ResetScrollbarPreferences();
      }
    }

    // Initial reflow
    mPresShell->InitialReflow(width, height);

    // Now trigger a refresh
    if (mEnableRendering) {
      mViewManager->EnableRefresh(NS_VMREFRESH_IMMEDIATE);
    }
  }

  // now register ourselves as a selection listener, so that we get
  // called when the selection changes in the window
  nsDocViewerSelectionListener *selectionListener =
    new nsDocViewerSelectionListener();
  NS_ENSURE_TRUE(selectionListener, NS_ERROR_OUT_OF_MEMORY);

  selectionListener->Init(this);

  // mSelectionListener is a owning reference
  mSelectionListener = selectionListener;

  nsCOMPtr<nsISelection> selection;
  rv = GetDocumentSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(selection));
  rv = selPrivate->AddSelectionListener(mSelectionListener);
  if (NS_FAILED(rv))
    return rv;

  // Save old listener so we can unregister it
  nsCOMPtr<nsIDOMFocusListener> mOldFocusListener = mFocusListener;

  // focus listener
  //
  // now register ourselves as a focus listener, so that we get called
  // when the focus changes in the window
  nsDocViewerFocusListener *focusListener;
  NS_NEWXPCOM(focusListener, nsDocViewerFocusListener);
  NS_ENSURE_TRUE(focusListener, NS_ERROR_OUT_OF_MEMORY);

  focusListener->Init(this);

  // mFocusListener is a strong reference
  mFocusListener = focusListener;

  // get the DOM event receiver
  nsCOMPtr<nsIDOMEventReceiver> erP(do_QueryInterface(mDocument));
  NS_WARN_IF_FALSE(erP, "No event receiver in document!");

  if (erP) {
    rv = erP->AddEventListenerByIID(mFocusListener,
                                    NS_GET_IID(nsIDOMFocusListener));
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register focus listener");
    if (mOldFocusListener) {
      rv = erP->RemoveEventListenerByIID(mOldFocusListener,
                                      NS_GET_IID(nsIDOMFocusListener));
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to remove focus listener");
    }
  }

  return NS_OK;
}

//-----------------------------------------------
// This method can be used to initial the "presentation"
// The aDoCreation indicates whether it should create
// all the new objects or just initialize the existing ones
nsresult
DocumentViewerImpl::InitInternal(nsIWidget* aParentWidget,
                                 nsIDeviceContext* aDeviceContext,
                                 const nsRect& aBounds,
                                 PRBool aDoCreation,
                                 PRBool aInPrintPreview)
{
  mParentWidget = aParentWidget; // not ref counted

  nsresult rv = NS_OK;
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NULL_POINTER);

  mDeviceContext = aDeviceContext;

#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
  // Clear PrintPreview Alternate Device
  if (mDeviceContext) {
    mDeviceContext->SetAltDevice(nsnull);
    mDeviceContext->SetCanonicalPixelScale(1.0);
  }
#endif

  PRBool makeCX = PR_FALSE;
  if (aDoCreation) {
    if (aParentWidget && !mPresContext) {
      // Create presentation context
      if (GetIsCreatingPrintPreview()) {
        mPresContext = do_CreateInstance(kPrintPreviewContextCID, &rv);
      } else {
        mPresContext = do_CreateInstance(kGalleyContextCID, &rv);
      }
      if (NS_FAILED(rv))
        return rv;

      rv = mPresContext->Init(aDeviceContext); 
      if (NS_FAILED(rv)) {
        mPresContext = nsnull;
        return rv;
      }

#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
      makeCX = !GetIsPrintPreview(); // needs to be true except when we are already in PP
#else
      makeCX = PR_TRUE;
#endif
    }
  }

  if (aDoCreation && mPresContext) {
    // Create the ViewManager and Root View...

    // We must do this before we tell the script global object about
    // this new document since doing that will cause us to re-enter
    // into nsSubDocumentFrame code through reflows caused by
    // FlushPendingNotifications() calls down the road...

    rv = MakeWindow(aParentWidget, aBounds);
    NS_ENSURE_SUCCESS(rv, rv);
    Hide();
  }

  nsCOMPtr<nsIInterfaceRequestor> requestor(do_QueryInterface(mContainer));
  if (requestor) {
    if (mPresContext) {
      nsCOMPtr<nsILinkHandler> linkHandler;
      requestor->GetInterface(NS_GET_IID(nsILinkHandler),
                              getter_AddRefs(linkHandler));
      mPresContext->SetContainer(mContainer);
      mPresContext->SetLinkHandler(linkHandler);
    }

    if (!aInPrintPreview) {
      // Set script-context-owner in the document

      nsCOMPtr<nsIScriptGlobalObject> global;
      requestor->GetInterface(NS_GET_IID(nsIScriptGlobalObject),
                              getter_AddRefs(global));

      if (global) {
        mDocument->SetScriptGlobalObject(global);
        nsCOMPtr<nsIDOMDocument> domdoc(do_QueryInterface(mDocument));

        if (domdoc) {
          global->SetNewDocument(domdoc, PR_TRUE, PR_TRUE);
        }
      }
    }
  }

  if (aDoCreation && mPresContext) {
    // The ViewManager and Root View was created above (in
    // MakeWindow())...

    rv = InitPresentationStuff(!makeCX);
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

  // First, get the script global object from the document...
  nsIScriptGlobalObject *global = mDocument->GetScriptGlobalObject();

  // Fail if no ScriptGlobalObject is available...
  NS_ENSURE_TRUE(global, NS_ERROR_NULL_POINTER);

  mLoaded = PR_TRUE;

  /* We need to protect ourself against auto-destruction in case the
     window is closed while processing the OnLoad event.  See bug
     http://bugzilla.mozilla.org/show_bug.cgi?id=78445 for more
     explanation.
  */
  nsCOMPtr<nsIDocumentViewer> kungFuDeathGrip(this);

  // Now, fire either an OnLoad or OnError event to the document...
  if(NS_SUCCEEDED(aStatus)) {
    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event(NS_PAGE_LOAD);

    rv = global->HandleDOMEvent(mPresContext, &event, nsnull,
                                NS_EVENT_FLAG_INIT, &status);
#ifdef MOZ_TIMELINE
    // if navigator.xul's load is complete, the main nav window is visible
    // mark that point.

    if (mDocument) {
      //printf("DEBUG: getting uri from document (%p)\n", mDocument.get());

      nsIURI *uri = mDocument->GetDocumentURI();

      if (uri) {
        //printf("DEBUG: getting spec fro uri (%p)\n", uri.get());
        nsCAutoString spec;
        uri->GetSpec(spec);
        if (!strcmp(spec.get(), "chrome://navigator/content/navigator.xul")) {
          NS_TIMELINE_MARK("Navigator Window visible now");
        }
      }
    }
#endif /* MOZ_TIMELINE */
  } else {
    // XXX: Should fire error event to the document...
  }

  // Now that the document has loaded, we can tell the presshell
  // to unsuppress painting.
  if (mPresShell && !mStopped) {
    mPresShell->UnsuppressPainting();
  }

  static PRBool forcePaint
    = PR_GetEnv("MOZ_FORCE_PAINT_AFTER_ONLOAD") != nsnull;
  if (forcePaint) {
    mDocument->FlushPendingNotifications(Flush_Display);
    nsIURI *uri = mDocument->GetDocumentURI();
    nsCAutoString spec;
    if (uri) {
      uri->GetSpec(spec);
    }
    printf("GECKO: PAINT FORCED AFTER ONLOAD: %s\n", spec.get());
    fflush(stdout);
  }

#ifdef NS_PRINTING
  // Check to see if someone tried to print during the load
  if (mPrintIsPending) {
    mPrintIsPending        = PR_FALSE;
    mPrintDocIsFullyLoaded = PR_TRUE;
    Print(mCachedPrintSettings, mCachedPrintWebProgressListner);
    mCachedPrintSettings           = nsnull;
    mCachedPrintWebProgressListner = nsnull;
  }
#endif

  return rv;
}

NS_IMETHODIMP
DocumentViewerImpl::PermitUnload(PRBool *aPermitUnload)
{
  *aPermitUnload = PR_TRUE;

  if (!mDocument || mInPermitUnload) {
    return NS_OK;
  }

  // First, get the script global object from the document...
  nsIScriptGlobalObject *global = mDocument->GetScriptGlobalObject();

  if (!global) {
    // This is odd, but not fatal
    NS_WARNING("nsIScriptGlobalObject not set for document!");
    return NS_OK;
  }

  // Now, fire an BeforeUnload event to the document and see if it's ok
  // to unload...
  nsEventStatus status = nsEventStatus_eIgnore;
  nsBeforePageUnloadEvent event(NS_BEFORE_PAGE_UNLOAD);

  // In evil cases we might be destroyed while handling the
  // onbeforeunload event, don't let that happen.
  nsRefPtr<DocumentViewerImpl> kungFuDeathGrip(this);

  mInPermitUnload = PR_TRUE;
  nsresult rv = global->HandleDOMEvent(mPresContext, &event, nsnull,
                                       NS_EVENT_FLAG_INIT, &status);
  mInPermitUnload = PR_FALSE;

  if (NS_SUCCEEDED(rv) && event.flags & NS_EVENT_FLAG_NO_DEFAULT) {
    // Ask the user if it's ok to unload the current page

    nsCOMPtr<nsIPrompt> prompt(do_GetInterface(mContainer));

    if (prompt) {
      nsCOMPtr<nsIStringBundleService>
        stringService(do_GetService(NS_STRINGBUNDLE_CONTRACTID));
      NS_ENSURE_TRUE(stringService, NS_OK);

      nsCOMPtr<nsIStringBundle> bundle;
      stringService->CreateBundle(kDOMStringBundleURL, getter_AddRefs(bundle));
      NS_ENSURE_TRUE(bundle, NS_OK);

      nsXPIDLString preMsg, postMsg;
      nsresult rv;
      rv = bundle->GetStringFromName(NS_LITERAL_STRING("OnBeforeUnloadPreMessage").get(), getter_Copies(preMsg));
      rv |= bundle->GetStringFromName(NS_LITERAL_STRING("OnBeforeUnloadPostMessage").get(), getter_Copies(postMsg));

      // GetStringFromName can succeed, yet give NULL strings back.
      if (NS_FAILED(rv) || !preMsg || !postMsg) {
        NS_ERROR("Failed to get strings from dom.properties!");
        return NS_OK;
      }

      // Limit the length of the text the page can inject into this
      // dialogue to 1024 characters.
      PRInt32 len = PR_MIN(event.text.Length(), 1024);

      nsAutoString msg(preMsg + NS_LITERAL_STRING("\n\n") +
                       StringHead(event.text, len) +
                       NS_LITERAL_STRING("\n\n") + postMsg);

      // This doesn't pass a title, which makes the title be
      // "Confirm", is that ok, or do we want a localizable title for
      // this dialogue?
      if (NS_FAILED(prompt->Confirm(nsnull, msg.get(), aPermitUnload))) {
        *aPermitUnload = PR_TRUE;
      }
    }
  }

  nsCOMPtr<nsIDocShellTreeNode> docShellNode(do_QueryInterface(mContainer));

  if (docShellNode) {
    PRInt32 childCount;
    docShellNode->GetChildCount(&childCount);

    for (PRInt32 i = 0; i < childCount && *aPermitUnload; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> item;
      docShellNode->GetChildAt(i, getter_AddRefs(item));

      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(item));
      nsCOMPtr<nsIContentViewer> cv;

      docShell->GetContentViewer(getter_AddRefs(cv));

      if (cv) {
        cv->PermitUnload(aPermitUnload);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Unload()
{
  mEnableRendering = PR_FALSE;

  if (!mDocument) {
    return NS_ERROR_NULL_POINTER;
  }

  // First, get the script global object from the document...
  nsIScriptGlobalObject *global = mDocument->GetScriptGlobalObject();

  if (!global) {
    // Fail if no ScriptGlobalObject is available...
    NS_ERROR("nsIScriptGlobalObject not set for document!");
    return NS_ERROR_NULL_POINTER;
  }

  // Now, fire an Unload event to the document...
  nsEventStatus status = nsEventStatus_eIgnore;
  nsEvent event(NS_PAGE_UNLOAD);

  return global->HandleDOMEvent(mPresContext, &event, nsnull,
                                NS_EVENT_FLAG_INIT, &status);
}

NS_IMETHODIMP
DocumentViewerImpl::Close()
{
  // All callers are supposed to call close to break circular
  // references.  If we do this stuff in the destructor, the
  // destructor might never be called (especially if we're being
  // used from JS.

  // Close is also needed to disable scripts during paint suppression,
  // since we transfer the existing global object to the new document
  // that is loaded.  In the future, the global object may become a proxy
  // for an object that can be switched in and out so that we don't need
  // to disable scripts during paint suppression.

  if (mDocument) {
#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
    // Turn scripting back on
    // after PrintPreview had turned it off
    if (GetIsPrintPreview() && mPrintEngine) {
      mPrintEngine->TurnScriptingOn(PR_TRUE);
    }
#endif

    // Break global object circular reference on the document created
    // in the DocViewer Init
    nsIScriptGlobalObject* globalObject = mDocument->GetScriptGlobalObject();

    if (globalObject) {
      globalObject->SetNewDocument(nsnull, PR_TRUE, PR_TRUE);
    }

#ifdef NS_PRINTING
    // A Close was called while we were printing
    // so don't clear the ScriptGlobalObject
    // or clear the mDocument below
    // Also, do an extra addref to keep the viewer from going away.
    if (mPrintEngine && !mClosingWhilePrinting) {
      mClosingWhilePrinting = PR_TRUE;
      NS_ADDREF_THIS();
    } else {
      // out of band cleanup of webshell
      mDocument->SetScriptGlobalObject(nsnull);
    }
#else
    mDocument->SetScriptGlobalObject(nsnull);
#endif

    if (mFocusListener) {
      // get the DOM event receiver
      nsCOMPtr<nsIDOMEventReceiver> erP(do_QueryInterface(mDocument));
      NS_WARN_IF_FALSE(erP, "No event receiver in document!");

      if (erP) {
        erP->RemoveEventListenerByIID(mFocusListener,
                                      NS_GET_IID(nsIDOMFocusListener));
      }
    }
  }

#ifdef NS_PRINTING
  // Don't clear the document if we are printing.
  if (!mClosingWhilePrinting) {
    mDocument = nsnull;
  }
#else
    mDocument = nsnull;
#endif

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Destroy()
{
#ifdef NS_PRINTING
  // Here is where we check to see if the docment was still being prepared 
  // for printing when it was asked to be destroy from someone externally
  // This usually happens if the document is unloaded while the user is in the Print Dialog
  //
  // So we flip the bool to remember that the document is going away
  // and we can clean up and abort later after returning from the Print Dialog
  if (mPrintEngine) {
    if (mPrintEngine->CheckBeforeDestroy()) {
      return NS_OK;
    }
  }
#endif

  // Don't let the document get unloaded while we are printing
  // this could happen if we hit the back button during printing
  if (mDestroyRefCount != 0) {
    --mDestroyRefCount;
    return NS_OK;
  }

  // All callers are supposed to call destroy to break circular
  // references.  If we do this stuff in the destructor, the
  // destructor might never be called (especially if we're being
  // used from JS.

#ifdef NS_PRINTING
  if (mPrintEngine) {
    mPrintEngine->Destroy();
    NS_RELEASE(mPrintEngine);
  }
#endif

  // Avoid leaking the old viewer.
  if (mPreviousViewer) {
    mPreviousViewer->Destroy();
    mPreviousViewer = nsnull;
  }

  if (mDeviceContext) {
    mDeviceContext->FlushFontCache();
    mDeviceContext = nsnull;
  }

  if (mPresShell) {
    // Break circular reference (or something)
    mPresShell->EndObservingDocument();

    nsCOMPtr<nsISelection> selection;
    GetDocumentSelection(getter_AddRefs(selection));

    nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(selection));

    if (selPrivate && mSelectionListener)
      selPrivate->RemoveSelectionListener(mSelectionListener);

    mPresShell->Destroy();
    mPresShell = nsnull;
  }

  if (mPresContext) {
    mPresContext->SetContainer(nsnull);
    mPresContext->SetLinkHandler(nsnull);
    mPresContext = nsnull;
  }

  mContainer = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Stop(void)
{
  NS_ASSERTION(mDocument, "Stop called too early or too late");
  if (mDocument) {
    mDocument->StopDocumentLoad();
  }

  if (mEnableRendering && (mLoaded || mStopped) && mPresContext)
    mPresContext->SetImageAnimationMode(imgIContainer::kDontAnimMode);

  mStopped = PR_TRUE;

  if (!mLoaded && mPresShell) {
    // Well, we might as well paint what we have so far.
    mPresShell->UnsuppressPainting();
  }

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetDOMDocument(nsIDOMDocument **aResult)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  return CallQueryInterface(mDocument, aResult);
}

NS_IMETHODIMP
DocumentViewerImpl::SetDOMDocument(nsIDOMDocument *aDocument)
{
  // Assumptions:
  //
  // 1) this document viewer has been initialized with a call to Init().
  // 2) the stylesheets associated with the document have been added
  // to the document.

  // XXX Right now, this method assumes that the layout of the current
  // document hasn't started yet.  More cleanup will probably be
  // necessary to make this method work for the case when layout *has*
  // occurred for the current document.
  // That work can happen when and if it is needed.

  nsresult rv;
  if (!aDocument)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDocument> newDoc = do_QueryInterface(aDocument, &rv);
  if (NS_FAILED(rv)) return rv;

  // 0) Replace the old document with the new one
  mDocument = newDoc;

  // 1) Set the script global object on the new document
  nsCOMPtr<nsIScriptGlobalObject> global(do_GetInterface(mContainer));

  if (global) {
    mDocument->SetScriptGlobalObject(global);
    global->SetNewDocument(aDocument, PR_TRUE, PR_TRUE);

    rv = SyncParentSubDocMap();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // 2) Replace the current pres shell with a new shell for the new document

  if (mPresShell) {
    mPresShell->EndObservingDocument();
    mPresShell->Destroy();

    mPresShell = nsnull;
  }

  // And if we're already given a prescontext...
  if (mPresContext) {
    // 3) Create a new style set for the document

    nsStyleSet *styleSet;
    rv = CreateStyleSet(mDocument, &styleSet);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = newDoc->CreateShell(mPresContext, mViewManager, styleSet,
                             getter_AddRefs(mPresShell));
    if (NS_FAILED(rv)) {
      delete styleSet;
      return rv;
    }

    // We're done creating the style set
    styleSet->EndUpdate();

    // The pres shell owns the style set now.

    mPresShell->BeginObservingDocument();

    // 4) Register the focus listener on the new document

    nsCOMPtr<nsIDOMEventReceiver> erP = do_QueryInterface(mDocument, &rv);
    NS_WARN_IF_FALSE(erP, "No event receiver in document!");

    if (erP) {
      rv = erP->AddEventListenerByIID(mFocusListener,
                                      NS_GET_IID(nsIDOMFocusListener));
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register focus listener");
    }
  }

  return rv;
}

NS_IMETHODIMP
DocumentViewerImpl::SetUAStyleSheet(nsIStyleSheet* aUAStyleSheet)
{
  NS_ASSERTION(aUAStyleSheet, "unexpected null pointer");
  nsCOMPtr<nsICSSStyleSheet> sheet(do_QueryInterface(aUAStyleSheet));
  if (sheet) {
    nsCOMPtr<nsICSSStyleSheet> newSheet;
    sheet->Clone(nsnull, nsnull, nsnull, nsnull, getter_AddRefs(newSheet));
    mUAStyleSheet = newSheet;
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetDocument(nsIDocument** aResult)
{
  NS_IF_ADDREF(*aResult = mDocument);

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetPresShell(nsIPresShell** aResult)
{
  NS_IF_ADDREF(*aResult = mPresShell);

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetPresContext(nsIPresContext** aResult)
{
  NS_IF_ADDREF(*aResult = mPresContext);

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
DocumentViewerImpl::GetPreviousViewer(nsIContentViewer** aViewer)
{
  *aViewer = mPreviousViewer;
  NS_IF_ADDREF(*aViewer);
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::SetPreviousViewer(nsIContentViewer* aViewer)
{
  // NOTE:  |Show| sets |mPreviousViewer| to null without calling this
  // function.

  if (aViewer) {
    NS_ASSERTION(!mPreviousViewer,
                 "can't set previous viewer when there already is one");

    // In a multiple chaining situation (which occurs when running a thrashing
    // test like i-bench or jrgm's tests with no delay), we can build up a
    // whole chain of viewers.  In order to avoid this, we always set our previous
    // viewer to the MOST previous viewer in the chain, and then dump the intermediate
    // link from the chain.  This ensures that at most only 2 documents are alive
    // and undestroyed at any given time (the one that is showing and the one that
    // is loading with painting suppressed).
    nsCOMPtr<nsIContentViewer> prevViewer;
    aViewer->GetPreviousViewer(getter_AddRefs(prevViewer));
    if (prevViewer) {
      aViewer->SetPreviousViewer(nsnull);
      aViewer->Destroy();
      return SetPreviousViewer(prevViewer);
    }
  }

  mPreviousViewer = aViewer;
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::SetBounds(const nsRect& aBounds)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);

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

  // We don't need the previous viewer anymore since we're not
  // displaying it.
  if (mPreviousViewer) {
    // This little dance *may* only be to keep
    // PresShell::EndObservingDocument happy, but I'm not sure.
    nsCOMPtr<nsIContentViewer> prevViewer(mPreviousViewer);
    mPreviousViewer = nsnull;
    prevViewer->Destroy();
  }

  if (mWindow) {
    mWindow->Show(PR_TRUE);
  }

  if (mDocument && !mPresShell && !mWindow) {
    nsresult rv;

    nsCOMPtr<nsIBaseWindow> base_win(do_QueryInterface(mContainer));
    NS_ENSURE_TRUE(base_win, NS_ERROR_UNEXPECTED);

    base_win->GetParentWidget(&mParentWidget);
    NS_ENSURE_TRUE(mParentWidget, NS_ERROR_UNEXPECTED);

    mDeviceContext = dont_AddRef(mParentWidget->GetDeviceContext());

#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
    // Clear PrintPreview Alternate Device
    if (mDeviceContext) {
      mDeviceContext->SetAltDevice(nsnull);
    }
#endif

    // Create presentation context
    if (GetIsCreatingPrintPreview()) {
      NS_ERROR("Whoa, we should not get here!");

      return NS_ERROR_UNEXPECTED;
    }

    NS_ASSERTION(!mPresContext, "Shouldn't have a prescontext if we have no shell!");
    mPresContext = do_CreateInstance(kGalleyContextCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mPresContext->Init(mDeviceContext);
    if (NS_FAILED(rv)) {
      mPresContext = nsnull;
      return rv;
    }

    nsRect tbounds;
    mParentWidget->GetBounds(tbounds);

    rv = MakeWindow(mParentWidget, tbounds);
    if (NS_FAILED(rv))
      return rv;

    if (mPresContext && mContainer) {
      nsCOMPtr<nsILinkHandler> linkHandler(do_GetInterface(mContainer));

      if (linkHandler) {
        mPresContext->SetLinkHandler(linkHandler);
      }

      mPresContext->SetContainer(mContainer);
    }

    if (mPresContext) {
      Hide();

      rv = InitPresentationStuff(PR_TRUE);
    }

    // If we get here the document load has already started and the
    // window is shown because some JS on the page caused it to be
    // shown...

    mPresShell->UnsuppressPainting();
  }

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Hide(void)
{
  PRBool is_in_print_mode = PR_FALSE;

  GetDoingPrint(&is_in_print_mode);

  if (is_in_print_mode) {
    // If we, or one of our parents, is in print mode it means we're
    // right now returning from print and the layout frame that was
    // created for this document is being destroyed. In such a case we
    // ignore the Hide() call.

    return NS_OK;
  }

  GetDoingPrintPreview(&is_in_print_mode);

  if (is_in_print_mode) {
    // If we, or one of our parents, is in print preview mode it means
    // we're right now returning from print preview and the layout
    // frame that was created for this document is being destroyed. In
    // such a case we ignore the Hide() call.

    return NS_OK;
  }

  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  NS_PRECONDITION(mWindow, "null window");
  if (mWindow) {
    mWindow->Show(PR_FALSE);
  }

  if (!mPresShell || GetIsPrintPreview()) {
    return NS_OK;
  }

  NS_ASSERTION(mPresContext, "Can't have a presshell and no prescontext!");

  // Avoid leaking the old viewer.
  if (mPreviousViewer) {
    mPreviousViewer->Destroy();
    mPreviousViewer = nsnull;
  }

  if (mIsSticky) {
    // This window is sticky, that means that it might be shown again
    // and we don't want the presshell n' all that to be thrown away
    // just because the window is hidden.

    return NS_OK;
  }

  if (mDeviceContext) {
    mDeviceContext->FlushFontCache();
  }

  // Break circular reference (or something)
  mPresShell->EndObservingDocument();
  nsCOMPtr<nsISelection> selection;

  GetDocumentSelection(getter_AddRefs(selection));

  nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(selection));

  if (selPrivate && mSelectionListener) {
    selPrivate->RemoveSelectionListener(mSelectionListener);
  }

#ifdef MOZ_XUL
  nsCOMPtr<nsIXULDocument> xul_doc(do_QueryInterface(mDocument));

  if (xul_doc) {
    xul_doc->OnHide();
  }
#endif

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mContainer));
  if (docShell) {
    PRBool saveLayoutState = PR_FALSE;
    docShell->GetShouldSaveLayoutState(&saveLayoutState);
    if (saveLayoutState) {
      nsCOMPtr<nsILayoutHistoryState> layoutState;
      mPresShell->CaptureHistoryState(getter_AddRefs(layoutState), PR_TRUE);
    }
  }

  mPresShell->Destroy();
  // Clear weak refs
  mPresContext->SetContainer(nsnull);
  mPresContext->SetLinkHandler(nsnull);                             

  mPresShell     = nsnull;
  mPresContext   = nsnull;
  mViewManager   = nsnull;
  mWindow        = nsnull;
  mDeviceContext = nsnull;
  mParentWidget  = nsnull;

  nsCOMPtr<nsIBaseWindow> base_win(do_QueryInterface(mContainer));

  if (base_win) {
    base_win->SetParentWidget(nsnull);
  }

  return NS_OK;
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
DocumentViewerImpl::GetSticky(PRBool *aSticky)
{
  *aSticky = mIsSticky;

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::SetSticky(PRBool aSticky)
{
  mIsSticky = aSticky;

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

NS_IMETHODIMP
DocumentViewerImpl::RequestWindowClose(PRBool* aCanClose)
{
#ifdef NS_PRINTING
  if (mPrintIsPending || (mPrintEngine && mPrintEngine->GetIsPrinting())) {
    *aCanClose = PR_FALSE;
    mDeferredWindowClose = PR_TRUE;
  } else
#endif
    *aCanClose = PR_TRUE;

  return NS_OK;
}


void
DocumentViewerImpl::ForceRefresh()
{
  mWindow->Invalidate(PR_TRUE);
}

NS_DEFINE_CID(kCSSLoaderCID, NS_CSS_LOADER_CID);

nsresult
DocumentViewerImpl::CreateStyleSet(nsIDocument* aDocument,
                                   nsStyleSet** aStyleSet)
{
  // this should eventually get expanded to allow for creating
  // different sets for different media
  if (!mUAStyleSheet) {
    NS_WARNING("unable to load UA style sheet");
  }

  nsStyleSet *styleSet = new nsStyleSet();
  if (!styleSet) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  styleSet->BeginUpdate();
  
  // The document will fill in the document sheets when we create the presshell
  
  // Handle the user sheets.
  nsCOMPtr<nsIDocShellTreeItem> docShell(do_QueryInterface(mContainer));
  PRInt32 shellType;
  docShell->GetItemType(&shellType);
  nsICSSStyleSheet* sheet = nsnull;
  if (shellType == nsIDocShellTreeItem::typeChrome) {
    sheet = nsLayoutStylesheetCache::UserChromeSheet();
  }
  else {
    sheet = nsLayoutStylesheetCache::UserContentSheet();
  }

  if (sheet)
    styleSet->PrependStyleSheet(nsStyleSet::eUserSheet, sheet);

  // Append chrome sheets (scrollbars + forms).
  PRBool shouldOverride = PR_FALSE;
  nsCOMPtr<nsIDocShell> ds(do_QueryInterface(mContainer));
  nsCOMPtr<nsIChromeEventHandler> chromeHandler;
  nsCOMPtr<nsICSSLoader> cssLoader( do_GetService(kCSSLoaderCID) );
  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsICSSStyleSheet> csssheet;

  ds->GetChromeEventHandler(getter_AddRefs(chromeHandler));
  if (chromeHandler) {
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(chromeHandler));
    nsCOMPtr<nsIContent> content(do_QueryInterface(elt));
    if (elt && content) {
      nsCOMPtr<nsIURI> baseURI = content->GetBaseURI();

      nsAutoString sheets;
      elt->GetAttribute(NS_LITERAL_STRING("usechromesheets"), sheets);
      if (!sheets.IsEmpty() && baseURI) {
        char *str = ToNewCString(sheets);
        char *newStr = str;
        char *token;
        while ( (token = nsCRT::strtok(newStr, ", ", &newStr)) ) {
          NS_NewURI(getter_AddRefs(uri), nsDependentCString(token), nsnull,
                    baseURI);
          if (!uri) continue;

          cssLoader->LoadAgentSheet(uri, getter_AddRefs(csssheet));
          if (!sheet) continue;

          styleSet->AppendStyleSheet(nsStyleSet::eAgentSheet, csssheet);
          shouldOverride = PR_TRUE;
        }
        nsMemory::Free(str);
      }
    }
  }

  if (!shouldOverride) {
    sheet = nsLayoutStylesheetCache::ScrollbarsSheet();
    if (sheet) {
      styleSet->AppendStyleSheet(nsStyleSet::eAgentSheet, sheet);
    }
  }

  sheet = nsLayoutStylesheetCache::FormsSheet();
  if (sheet) {
    styleSet->AppendStyleSheet(nsStyleSet::eAgentSheet, sheet);
  }

  if (mUAStyleSheet) {
    styleSet->AppendStyleSheet(nsStyleSet::eAgentSheet, mUAStyleSheet);
  }

  // Caller will handle calling EndUpdate, per contract.
  *aStyleSet = styleSet;
  return NS_OK;
}


//-------------------------------------------------------

nsresult
DocumentViewerImpl::MakeWindow(nsIWidget* aParentWidget,
                               const nsRect& aBounds)
{
  nsresult rv;

  mViewManager = do_CreateInstance(kViewManagerCID, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsIDeviceContext *dx = mPresContext->DeviceContext();

  nsRect tbounds = aBounds;
  float p2t;
  p2t = mPresContext->PixelsToTwips();
  tbounds *= p2t;

   // Initialize the view manager with an offset. This allows the viewmanager
   // to manage a coordinate space offset from (0,0)
  rv = mViewManager->Init(dx);
  if (NS_FAILED(rv))
    return rv;

  // Reset the bounds offset so the root view is set to 0,0. The
  // offset is specified in nsIViewManager::Init above.
  // Besides, layout will reset the root view to (0,0) during reflow,
  // so changing it to 0,0 eliminates placing the root view in the
  // wrong place initially.
  tbounds.x = 0;
  tbounds.y = 0;

  // Create a child window of the parent that is our "root view/window"
  // Create a view

  nsIView *view = nsnull;
  rv = CallCreateInstance(kViewCID, &view);
  if (NS_FAILED(rv))
    return rv;

  // if aParentWidget has a view, we'll hook our view manager up to its view tree
  void* clientData;
  nsIView* containerView = nsnull;
  if (NS_SUCCEEDED(aParentWidget->GetClientData(clientData))) {
    nsISupports* data = (nsISupports*)clientData;

    if (data) {
      CallQueryInterface(data, &containerView);
    }
  }

  if (containerView) {
    // see if the containerView has already been hooked into a foreign view manager hierarchy
    // if it has, then we have to hook into the hierarchy too otherwise bad things will happen.
    nsIViewManager* containerVM = containerView->GetViewManager();
    nsIView* pView = containerView;
    do {
      pView = pView->GetParent();
    } while (pView && pView->GetViewManager() == containerVM);

    if (!pView) {
      // OK, so the container is not already hooked up into a foreign view manager hierarchy.
      // That means we can choose not to hook ourselves up.
      //
      // If the parent container is a chrome shell then we won't hook into its view
      // tree. This will improve performance a little bit (especially given scrolling/painting perf bugs)
      // but is really just for peace of mind. This check can be removed if we want to support fancy
      // chrome effects like transparent controls floating over content, transparent Web browsers, and
      // things like that, and the perf bugs are fixed.
      nsCOMPtr<nsIDocShellTreeItem> container(do_QueryInterface(mContainer));
      nsCOMPtr<nsIDocShellTreeItem> parentContainer;
      PRInt32 itemType;
      if (nsnull == container
          || NS_FAILED(container->GetParent(getter_AddRefs(parentContainer)))
          || nsnull == parentContainer
          || NS_FAILED(parentContainer->GetItemType(&itemType))
          || itemType != nsIDocShellTreeItem::typeContent) {
        containerView = nsnull;
      }
    }
  }

  rv = view->Init(mViewManager, tbounds, containerView);
  if (NS_FAILED(rv))
    return rv;

  // pass in a native widget to be the parent widget ONLY if the view hierarchy will stand alone.
  // otherwise the view will find its own parent widget and "do the right thing" to
  // establish a parent/child widget relationship
  rv = view->CreateWidget(kWidgetCID, nsnull,
                          containerView != nsnull ? nsnull : aParentWidget->GetNativeData(NS_NATIVE_WIDGET),
                          PR_TRUE, PR_FALSE);
  if (NS_FAILED(rv))
    return rv;

  // Setup hierarchical relationship in view manager
  mViewManager->SetRootView(view);

  mWindow = view->GetWidget();

  // This SetFocus is necessary so the Arrow Key and Page Key events
  // go to the scrolled view as soon as the Window is created instead of going to
  // the browser window (this enables keyboard scrolling of the document)
  // mWindow->SetFocus();

  return rv;
}

nsresult DocumentViewerImpl::GetDocumentSelection(nsISelection **aSelection,
                                                  nsIPresShell *aPresShell)
{
  if (!aPresShell) {
    if (!mPresShell) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    aPresShell = mPresShell;
  }
  if (!aSelection)
    return NS_ERROR_NULL_POINTER;
  if (!aPresShell)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsISelectionController> selcon;
  selcon = do_QueryInterface(aPresShell);
  if (selcon)
    return selcon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                aSelection);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DocumentViewerImpl::CreateDocumentViewerUsing(nsIPresContext* aPresContext,
                                              nsIDocumentViewer** aResult)
{
  if (!mDocument) {
    // XXX better error
    return NS_ERROR_NULL_POINTER;
  }
  if (!aPresContext) {
    return NS_ERROR_NULL_POINTER;
  }

  // Create new viewer
  DocumentViewerImpl* viewer = new DocumentViewerImpl(aPresContext);
  if (!viewer) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(viewer);

  // XXX make sure the ua style sheet is used (for now; need to be
  // able to specify an alternate)
  viewer->SetUAStyleSheet(mUAStyleSheet);

  // Bind the new viewer to the old document
  nsresult rv = viewer->LoadStart(mDocument);

  *aResult = viewer;

  return rv;
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
  nsresult rv;
  nsCOMPtr<nsISelection> selection;

  rv = GetDocumentSelection(getter_AddRefs(selection));
  if (NS_FAILED(rv)) return rv;

  return selection->CollapseToStart();
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
    bodyNode = do_QueryInterface(mDocument->GetRootContent());
  }
  if (!bodyNode) return NS_ERROR_FAILURE;

  rv = selection->RemoveAllRanges();
  if (NS_FAILED(rv)) return rv;

  rv = selection->SelectAllChildren(bodyNode);
  return rv;
}

NS_IMETHODIMP DocumentViewerImpl::CopySelection()
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);
  return mPresShell->DoCopy();
}

NS_IMETHODIMP DocumentViewerImpl::CopyLinkLocation()
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);
  nsCOMPtr<nsIDOMNode> node;
  GetPopupLinkNode(getter_AddRefs(node));
  // make noise if we're not in a link
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  nsAutoString locationText;
  nsresult rv = mPresShell->GetLinkLocation(node, locationText);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIClipboardHelper> clipboard(do_GetService("@mozilla.org/widget/clipboardhelper;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // copy the href onto the clipboard
  return clipboard->CopyString(locationText);
}

NS_IMETHODIMP DocumentViewerImpl::CopyImageLocation()
{
  nsCOMPtr<nsIImageLoadingContent> node;
  GetPopupImageNode(getter_AddRefs(node));
  // make noise if we're not in an image
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  nsCOMPtr<nsIURI> uri;
  node->GetCurrentURI(getter_AddRefs(uri));
  if (!uri)
    return NS_ERROR_FAILURE;

  nsCAutoString spec;
  uri->GetSpec(spec);

  NS_ConvertUTF8toUTF16 locationText(spec);

  nsresult rv;
  nsCOMPtr<nsIClipboardHelper> clipboard =
    do_GetService("@mozilla.org/widget/clipboardhelper;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // copy the href onto the clipboard
  return clipboard->CopyString(locationText);
}

NS_IMETHODIMP DocumentViewerImpl::CopyImageContents()
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);
  nsCOMPtr<nsIImageLoadingContent> node;
  GetPopupImageNode(getter_AddRefs(node));
  // make noise if we're not in an image
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  return nsCopySupport::ImageCopy(node, nsIClipboard::kGlobalClipboard);
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

/* AString getContents (in string mimeType, in boolean selectionOnly); */
NS_IMETHODIMP DocumentViewerImpl::GetContents(const char *mimeType, PRBool selectionOnly, nsAString& aOutValue)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);
  return mPresShell->DoGetContents(nsDependentCString(mimeType), 0, selectionOnly, aOutValue);
}

/* readonly attribute boolean canGetContents; */
NS_IMETHODIMP DocumentViewerImpl::GetCanGetContents(PRBool *aCanGetContents)
{
  NS_ENSURE_ARG_POINTER(aCanGetContents);

  nsCOMPtr<nsISelection> selection;
  nsresult rv = GetDocumentSelection(getter_AddRefs(selection));
  if (NS_FAILED(rv)) return rv;
  
  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);
  
  *aCanGetContents = !isCollapsed;
  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#endif

/* ========================================================================================
 * nsIContentViewerFile
 * ======================================================================================== */
/** ---------------------------------------------------
 *  See documentation above in the nsIContentViewerfile class definition
 *	@update 01/24/00 dwc
 */
NS_IMETHODIMP
DocumentViewerImpl::Print(PRBool            aSilent,
                          FILE *            aDebugFile,
                          nsIPrintSettings* aPrintSettings)
{
#ifdef NS_PRINTING
  nsCOMPtr<nsIPrintSettings> printSettings;

#ifdef NS_DEBUG
  nsresult rv = NS_ERROR_FAILURE;

  mDebugFile = aDebugFile;
  // if they don't pass in a PrintSettings, then make one
  // it will have all the default values
  printSettings = aPrintSettings;
  nsCOMPtr<nsIPrintOptions> printOptions = do_GetService(sPrintOptionsContractID, &rv);
  if (NS_SUCCEEDED(rv)) {
    // if they don't pass in a PrintSettings, then make one
    if (printSettings == nsnull) {
      printOptions->CreatePrintSettings(getter_AddRefs(printSettings));
    }
    NS_ASSERTION(printSettings, "You can't PrintPreview without a PrintSettings!");
  }
  if (printSettings) printSettings->SetPrintSilent(aSilent);
  if (printSettings) printSettings->SetShowPrintProgress(PR_FALSE);
#endif


  return Print(printSettings, nsnull);
#else
  return NS_ERROR_FAILURE;
#endif
}

/* [noscript] void printWithParent (in nsIDOMWindowInternal aParentWin, in nsIPrintSettings aThePrintSettings, in nsIWebProgressListener aWPListener); */
NS_IMETHODIMP 
DocumentViewerImpl::PrintWithParent(nsIDOMWindowInternal *aParentWin, nsIPrintSettings *aThePrintSettings, nsIWebProgressListener *aWPListener)
{
  mDialogParentWin = aParentWin;
  return Print(aThePrintSettings, aWPListener);
}

// nsIContentViewerFile interface
NS_IMETHODIMP
DocumentViewerImpl::GetPrintable(PRBool *aPrintable)
{
  NS_ENSURE_ARG_POINTER(aPrintable);

  *aPrintable = !GetIsPrinting();

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
   NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(presShell)), NS_ERROR_FAILURE);

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

NS_IMETHODIMP
DocumentViewerImpl::SetTextZoom(float aTextZoom)
{
  if (mDeviceContext) {
    float oldTextZoom = 1.0;  // just in case mDeviceContext doesn't implement
    // Don't reflow if there's no change in the textZoom.
    mDeviceContext->GetTextZoom(oldTextZoom);
    mDeviceContext->SetTextZoom(aTextZoom);
    if (oldTextZoom != aTextZoom && mPresContext) {
      mPresContext->ClearStyleDataAndReflow();
    }
  }

  // now set the text zoom on all children of mContainer (even if our zoom
  // didn't change, our children's zoom may be different, though it would
  // be unusual).
  struct TextZoomInfo textZoomInfo = { aTextZoom };
  return CallChildren(SetChildTextZoom, &textZoomInfo);
}

NS_IMETHODIMP
DocumentViewerImpl::GetTextZoom(float* aTextZoom)
{
  NS_ENSURE_ARG_POINTER(aTextZoom);

  if (mDeviceContext) {
    return mDeviceContext->GetTextZoom(*aTextZoom);
  }

  *aTextZoom = 1.0;
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetDefaultCharacterSet(nsACString& aDefaultCharacterSet)
{
  NS_ENSURE_STATE(mContainer);

  if (mDefaultCharacterSet.IsEmpty())
  {
    const nsAdoptingString& defCharset =
      nsContentUtils::GetLocalizedStringPref("intl.charset.default");

    if (!defCharset.IsEmpty())
      LossyCopyUTF16toASCII(defCharset, mDefaultCharacterSet);
    else
      mDefaultCharacterSet.AssignLiteral("ISO-8859-1");
  }
  aDefaultCharacterSet = mDefaultCharacterSet;
  return NS_OK;
}

static void
SetChildDefaultCharacterSet(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  const nsACString* charset = NS_STATIC_CAST(nsACString*, aClosure);
  aChild->SetDefaultCharacterSet(*charset);
}

NS_IMETHODIMP
DocumentViewerImpl::SetDefaultCharacterSet(const nsACString& aDefaultCharacterSet)
{
  mDefaultCharacterSet = aDefaultCharacterSet;  // this does a copy of aDefaultCharacterSet
  // now set the default char set on all children of mContainer
  return CallChildren(SetChildDefaultCharacterSet,
                      (void*) &aDefaultCharacterSet);
}

// XXX: SEMANTIC CHANGE!
//      returns a copy of the string.  Caller is responsible for freeing result
//      using Recycle(aForceCharacterSet)
NS_IMETHODIMP DocumentViewerImpl::GetForceCharacterSet(nsACString& aForceCharacterSet)
{
  aForceCharacterSet = mForceCharacterSet;
  return NS_OK;
}

static void
SetChildForceCharacterSet(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  const nsACString* charset = NS_STATIC_CAST(nsACString*, aClosure);
  aChild->SetForceCharacterSet(*charset);
}

NS_IMETHODIMP
DocumentViewerImpl::SetForceCharacterSet(const nsACString& aForceCharacterSet)
{
  mForceCharacterSet = aForceCharacterSet;
  // now set the force char set on all children of mContainer
  return CallChildren(SetChildForceCharacterSet, (void*) &aForceCharacterSet);
}

// XXX: SEMANTIC CHANGE!
//      returns a copy of the string.  Caller is responsible for freeing result
//      using Recycle(aHintCharacterSet)
NS_IMETHODIMP DocumentViewerImpl::GetHintCharacterSet(nsACString& aHintCharacterSet)
{

  if(kCharsetUninitialized == mHintCharsetSource) {
    aHintCharacterSet.Truncate();
  } else {
    aHintCharacterSet = mHintCharset;
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


NS_IMETHODIMP DocumentViewerImpl::GetPrevDocCharacterSet(nsACString& aPrevDocCharacterSet)
{
  aPrevDocCharacterSet = mPrevDocCharacterSet;

  return NS_OK;
}

static void
SetChildPrevDocCharacterSet(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  const nsACString* charset = NS_STATIC_CAST(nsACString*, aClosure);
  aChild->SetPrevDocCharacterSet(*charset);
}


NS_IMETHODIMP
DocumentViewerImpl::SetPrevDocCharacterSet(const nsACString& aPrevDocCharacterSet)
{
  mPrevDocCharacterSet = aPrevDocCharacterSet;  
  return CallChildren(SetChildPrevDocCharacterSet,
                      (void*) &aPrevDocCharacterSet);
}


static void
SetChildHintCharacterSetSource(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  aChild->SetHintCharacterSetSource(NS_PTR_TO_INT32(aClosure));
}

NS_IMETHODIMP
DocumentViewerImpl::SetHintCharacterSetSource(PRInt32 aHintCharacterSetSource)
{
  mHintCharsetSource = aHintCharacterSetSource;
  // now set the hint char set source on all children of mContainer
  return CallChildren(SetChildHintCharacterSetSource,
                      (void*) aHintCharacterSetSource);
}

static void
SetChildHintCharacterSet(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  const nsACString* charset = NS_STATIC_CAST(nsACString*, aClosure);
  aChild->SetHintCharacterSet(*charset);
}

NS_IMETHODIMP
DocumentViewerImpl::SetHintCharacterSet(const nsACString& aHintCharacterSet)
{
  mHintCharset = aHintCharacterSet;
  // now set the hint char set on all children of mContainer
  return CallChildren(SetChildHintCharacterSet, (void*) &aHintCharacterSet);
}

static void
SetChildBidiOptions(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  aChild->SetBidiOptions(NS_PTR_TO_INT32(aClosure));
}

NS_IMETHODIMP DocumentViewerImpl::SetBidiTextDirection(PRUint8 aTextDirection)
{
  PRUint32 bidiOptions;

  GetBidiOptions(&bidiOptions);
  SET_BIDI_OPTION_DIRECTION(bidiOptions, aTextDirection);
  SetBidiOptions(bidiOptions);
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetBidiTextDirection(PRUint8* aTextDirection)
{
  PRUint32 bidiOptions;

  if (aTextDirection) {
    GetBidiOptions(&bidiOptions);
    *aTextDirection = GET_BIDI_OPTION_DIRECTION(bidiOptions);
  }
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SetBidiTextType(PRUint8 aTextType)
{
  PRUint32 bidiOptions;

  GetBidiOptions(&bidiOptions);
  SET_BIDI_OPTION_TEXTTYPE(bidiOptions, aTextType);
  SetBidiOptions(bidiOptions);
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetBidiTextType(PRUint8* aTextType)
{
  PRUint32 bidiOptions;

  if (aTextType) {
    GetBidiOptions(&bidiOptions);
    *aTextType = GET_BIDI_OPTION_TEXTTYPE(bidiOptions);
  }
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SetBidiControlsTextMode(PRUint8 aControlsTextMode)
{
  PRUint32 bidiOptions;

  GetBidiOptions(&bidiOptions);
  SET_BIDI_OPTION_CONTROLSTEXTMODE(bidiOptions, aControlsTextMode);
  SetBidiOptions(bidiOptions);
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetBidiControlsTextMode(PRUint8* aControlsTextMode)
{
  PRUint32 bidiOptions;

  if (aControlsTextMode) {
    GetBidiOptions(&bidiOptions);
    *aControlsTextMode = GET_BIDI_OPTION_CONTROLSTEXTMODE(bidiOptions);
  }
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SetBidiNumeral(PRUint8 aNumeral)
{
  PRUint32 bidiOptions;

  GetBidiOptions(&bidiOptions);
  SET_BIDI_OPTION_NUMERAL(bidiOptions, aNumeral);
  SetBidiOptions(bidiOptions);
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetBidiNumeral(PRUint8* aNumeral)
{
  PRUint32 bidiOptions;

  if (aNumeral) {
    GetBidiOptions(&bidiOptions);
    *aNumeral = GET_BIDI_OPTION_NUMERAL(bidiOptions);
  }
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SetBidiSupport(PRUint8 aSupport)
{
  PRUint32 bidiOptions;

  GetBidiOptions(&bidiOptions);
  SET_BIDI_OPTION_SUPPORT(bidiOptions, aSupport);
  SetBidiOptions(bidiOptions);
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetBidiSupport(PRUint8* aSupport)
{
  PRUint32 bidiOptions;

  if (aSupport) {
    GetBidiOptions(&bidiOptions);
    *aSupport = GET_BIDI_OPTION_SUPPORT(bidiOptions);
  }
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SetBidiCharacterSet(PRUint8 aCharacterSet)
{
  PRUint32 bidiOptions;

  GetBidiOptions(&bidiOptions);
  SET_BIDI_OPTION_CHARACTERSET(bidiOptions, aCharacterSet);
  SetBidiOptions(bidiOptions);
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetBidiCharacterSet(PRUint8* aCharacterSet)
{
  PRUint32 bidiOptions;

  if (aCharacterSet) {
    GetBidiOptions(&bidiOptions);
    *aCharacterSet = GET_BIDI_OPTION_CHARACTERSET(bidiOptions);
  }
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SetBidiOptions(PRUint32 aBidiOptions)
{
  if (mPresContext) {
#if 1
    // forcing reflow will cause bug 80352. Temp turn off force reflow and
    // wait for simon@softel.co.il to find the real solution
    mPresContext->SetBidi(aBidiOptions, PR_FALSE);
#else
    mPresContext->SetBidi(aBidiOptions, PR_TRUE); // force reflow
#endif
  }
  // now set bidi on all children of mContainer
  CallChildren(SetChildBidiOptions, (void*) aBidiOptions);
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetBidiOptions(PRUint32* aBidiOptions)
{
  if (aBidiOptions) {
    if (mPresContext) {
      mPresContext->GetBidi(aBidiOptions);
    }
    else
      *aBidiOptions = IBMBIDI_DEFAULT_BIDI_OPTIONS;
  }
  return NS_OK;
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
   GetPresShell(getter_AddRefs(presShell));
   NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

   // Flush out all content and style updates.  Note that we don't need to
   // flush layout since we're about to do a top-level resize reflow, and we
   // shouldn't have any parents to propagate the flush to.
   mDocument->FlushPendingNotifications(Flush_Style);
                                        
   NS_ENSURE_SUCCESS(presShell->ResizeReflow(NS_UNCONSTRAINEDSIZE,
      NS_UNCONSTRAINEDSIZE), NS_ERROR_FAILURE);

   nsCOMPtr<nsIPresContext> presContext;
   GetPresContext(getter_AddRefs(presContext));
   NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

   PRInt32 width, height;
   float   pixelScale;

   // so how big is it?
   nsRect shellArea = presContext->GetVisibleArea();
   pixelScale = presContext->TwipsToPixels();
   width = PRInt32((float)shellArea.width*pixelScale);
   height = PRInt32((float)shellArea.height*pixelScale);

   nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
   docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
   NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

   /* presContext's size was calculated in twips and has already been
      rounded to the equivalent pixels (so the width/height calculation
      we just performed was probably exact, though it was based on
      values already rounded during ResizeReflow). In a surprising
      number of instances, this rounding makes a window which for want
      of one extra pixel's width ends up wrapping the longest line of
      text during actual window layout. This makes the window too short,
      generally clipping the OK/Cancel buttons. Here we add one pixel
      to the calculated width, to circumvent this problem. */
   NS_ENSURE_SUCCESS(treeOwner->SizeShellTo(docShellAsItem, width+1, height),
      NS_ERROR_FAILURE);

   return NS_OK;
}



#ifdef XP_MAC
#pragma mark -
#endif

NS_IMPL_ISUPPORTS1(nsDocViewerSelectionListener, nsISelectionListener)

nsresult nsDocViewerSelectionListener::Init(DocumentViewerImpl *aDocViewer)
{
  mDocViewer = aDocViewer;
  return NS_OK;
}

/*
 * GetPopupNode, GetPopupLinkNode and GetPopupImageNode are helpers
 * for the cmd_copyLink / cmd_copyImageLocation / cmd_copyImageContents family
 * of commands. The focus controller stores the popup node, these retrieve
 * them and munge appropriately. Note that we have to store the popup node
 * rather than retrieving it from EventStateManager::GetFocusedContent because
 * not all content (images included) can receive focus.
 */

nsresult
DocumentViewerImpl::GetPopupNode(nsIDOMNode** aNode)
{
  NS_ENSURE_ARG_POINTER(aNode);

  nsresult rv;

  // get the document
  nsCOMPtr<nsIDocument> document;
  rv = GetDocument(getter_AddRefs(document));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);


  // get the private dom window
  nsCOMPtr<nsPIDOMWindow> privateWin(do_QueryInterface(document->GetScriptGlobalObject(), &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // get the focus controller
  nsIFocusController *focusController = privateWin->GetRootFocusController();
  NS_ENSURE_TRUE(focusController, NS_ERROR_FAILURE);

  // get the popup node
  focusController->GetPopupNode(aNode); // addref happens here

  return rv;
}

// GetPopupLinkNode: return popup link node or fail
nsresult
DocumentViewerImpl::GetPopupLinkNode(nsIDOMNode** aNode)
{
  NS_ENSURE_ARG_POINTER(aNode);

  // you get null unless i say so
  *aNode = nsnull;

  // find popup node
  nsCOMPtr<nsIDOMNode> node;
  nsresult rv = GetPopupNode(getter_AddRefs(node));
  NS_ENSURE_SUCCESS(rv, rv);

  // find out if we have a link in our ancestry
  while (node) {

    // are we an anchor?
    nsCOMPtr<nsIDOMHTMLAnchorElement> anchor(do_QueryInterface(node));
    nsCOMPtr<nsIDOMHTMLAreaElement> area;
    nsCOMPtr<nsIDOMHTMLLinkElement> link;
    nsAutoString xlinkType;
    if (!anchor) {
      // area?
      area = do_QueryInterface(node);
      if (!area) {
        // link?
        link = do_QueryInterface(node);
        if (!link) {
          // XLink?
          nsCOMPtr<nsIDOMElement> element(do_QueryInterface(node));
          if (element) {
            element->GetAttributeNS(NS_LITERAL_STRING("http://www.w3.org/1999/xlink"),NS_LITERAL_STRING("type"),xlinkType);
          }
        }
      }
    }
    if (anchor || area || link || xlinkType.EqualsLiteral("simple")) {
      *aNode = node;
      NS_IF_ADDREF(*aNode); // addref
      return NS_OK;
    }
    else {
      // if not, get our parent and keep trying...
      nsCOMPtr<nsIDOMNode> parentNode;
      node->GetParentNode(getter_AddRefs(parentNode));
      node = parentNode;
    }
  }

  // if we have no node, fail
  return NS_ERROR_FAILURE;
}

// GetPopupLinkNode: return popup image node or fail
nsresult
DocumentViewerImpl::GetPopupImageNode(nsIImageLoadingContent** aNode)
{
  NS_ENSURE_ARG_POINTER(aNode);

  // you get null unless i say so
  *aNode = nsnull;

  // find popup node
  nsCOMPtr<nsIDOMNode> node;
  nsresult rv = GetPopupNode(getter_AddRefs(node));
  NS_ENSURE_SUCCESS(rv, rv);

  CallQueryInterface(node, aNode);
  return NS_OK;
}

/*
 * XXX dr
 * ------
 * These two functions -- GetInLink and GetInImage -- are kind of annoying
 * in that they only get called from the controller (in
 * nsDOMWindowController::IsCommandEnabled). The actual construction of the
 * context menus in communicator (nsContextMenu.js) has its own, redundant
 * tests. No big deal, but good to keep in mind if we ever clean context
 * menus.
 */

NS_IMETHODIMP DocumentViewerImpl::GetInLink(PRBool* aInLink)
{
#ifdef DEBUG_dr
  printf("dr :: DocumentViewerImpl::GetInLink\n");
#endif

  NS_ENSURE_ARG_POINTER(aInLink);

  // we're not in a link unless i say so
  *aInLink = PR_FALSE;

  // get the popup link
  nsCOMPtr<nsIDOMNode> node;
  nsresult rv = GetPopupLinkNode(getter_AddRefs(node));
  if (NS_FAILED(rv)) return rv;
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  // if we made it here, we're in a link
  *aInLink = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetInImage(PRBool* aInImage)
{
#ifdef DEBUG_dr
  printf("dr :: DocumentViewerImpl::GetInImage\n");
#endif

  NS_ENSURE_ARG_POINTER(aInImage);

  // we're not in an image unless i say so
  *aInImage = PR_FALSE;

  // get the popup image
  nsCOMPtr<nsIImageLoadingContent> node;
  nsresult rv = GetPopupImageNode(getter_AddRefs(node));
  if (NS_FAILED(rv)) return rv;
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  // if we made it here, we're in an image
  *aInImage = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsDocViewerSelectionListener::NotifySelectionChanged(nsIDOMDocument *, nsISelection *, short)
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
    mDocViewer->GetDocument(getter_AddRefs(theDoc));
    if (!theDoc) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(theDoc->GetScriptGlobalObject());
    if (!domWindow) return NS_ERROR_FAILURE;

    domWindow->UpdateCommands(NS_LITERAL_STRING("select"));
    mGotSelectionState = PR_TRUE;
    mSelectionWasCollapsed = selectionCollapsed;
  }

  return NS_OK;
}

//nsDocViewerFocusListener
NS_IMPL_ISUPPORTS1(nsDocViewerFocusListener, nsIDOMFocusListener)

nsDocViewerFocusListener::nsDocViewerFocusListener()
:mDocViewer(nsnull)
{
}

nsDocViewerFocusListener::~nsDocViewerFocusListener(){}

nsresult
nsDocViewerFocusListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDocViewerFocusListener::Focus(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIPresShell> shell;
  if(!mDocViewer)
    return NS_ERROR_FAILURE;

  nsresult result = mDocViewer->GetPresShell(getter_AddRefs(shell));
  if(NS_FAILED(result) || !shell)
    return result?result:NS_ERROR_FAILURE;
  nsCOMPtr<nsISelectionController> selCon;
  selCon = do_QueryInterface(shell);
  PRInt16 selectionStatus;
  selCon->GetDisplaySelection( &selectionStatus);

  //if selection was nsISelectionController::SELECTION_OFF, do nothing
  //otherwise re-enable it.
  if(selectionStatus == nsISelectionController::SELECTION_DISABLED ||
     selectionStatus == nsISelectionController::SELECTION_HIDDEN)
  {
    selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
    selCon->RepaintSelection(nsISelectionController::SELECTION_NORMAL);
  }
  return result;
}

NS_IMETHODIMP
nsDocViewerFocusListener::Blur(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIPresShell> shell;
  if(!mDocViewer)
    return NS_ERROR_FAILURE;

  nsresult result = mDocViewer->GetPresShell(getter_AddRefs(shell));
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

/** ---------------------------------------------------
 *  From nsIWebBrowserPrint
 */
NS_IMETHODIMP
DocumentViewerImpl::Print(nsIPrintSettings*       aPrintSettings,
                          nsIWebProgressListener* aWebProgressListener)
{
#ifdef NS_PRINTING
  INIT_RUNTIME_ERROR_CHECKING();

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mContainer));
  NS_ASSERTION(docShell, "This has to be a docshell");

  // Check to see if this document is still busy
  // If it is busy and we aren't already "queued" up to print then
  // Indicate there is a print pending and cache the args for later
  PRUint32 busyFlags = nsIDocShell::BUSY_FLAGS_NONE;
  if ((NS_FAILED(docShell->GetBusyFlags(&busyFlags)) ||
       (busyFlags != nsIDocShell::BUSY_FLAGS_NONE && busyFlags & nsIDocShell::BUSY_FLAGS_PAGE_LOADING)) && 
      !mPrintDocIsFullyLoaded) {
    if (!mPrintIsPending) {
      mCachedPrintSettings           = aPrintSettings;
      mCachedPrintWebProgressListner = aWebProgressListener;
      mPrintIsPending                = PR_TRUE;
    }
    PR_PL(("Printing Stopped - document is still busy!"));
    return NS_ERROR_GFX_PRINTER_DOC_IS_BUSY;
  }

  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));

  if (!presShell) {
    // A frame that's not displayed can't be printed!
    PR_PL(("Printing Stopped - PreShell was NULL!"));
    return NS_OK;
  }

  nsresult rv = NS_ERROR_FAILURE;

  // if we are printing another URL, then exit
  // the reason we check here is because this method can be called while
  // another is still in here (the printing dialog is a good example).
  // the only time we can print more than one job at a time is the regression tests
  if (GetIsPrinting()) {
    // Let the user know we are not ready to print.
    rv = NS_ERROR_NOT_AVAILABLE;
    nsPrintEngine::ShowPrintErrorDialog(rv);
    return rv;
  }

  // If we are hosting a full-page plugin, tell it to print
  // first. It shows its own native print UI.
  nsCOMPtr<nsIPluginDocument> pDoc(do_QueryInterface(mDocument));
  if (pDoc)
    return pDoc->Print();

  if (!mPrintEngine) {
    mPrintEngine = new nsPrintEngine();
    NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_OUT_OF_MEMORY);
    NS_ADDREF(mPrintEngine);

#ifdef NS_DEBUG
    mPrintEngine->Initialize(this, this, mContainer, mDocument, 
                             mDeviceContext, mPresContext, mWindow, mParentWidget, mDebugFile);
#else
    mPrintEngine->Initialize(this, this, mContainer, mDocument, 
                             mDeviceContext, mPresContext, mWindow, mParentWidget, nsnull);
#endif
  }

  rv = mPrintEngine->Print(aPrintSettings, aWebProgressListener);
  if (NS_FAILED(rv)) {
    OnDonePrinting();
  }
  return rv;
#else
  PR_PL(("NS_PRINTING not defined - printing not implemented in this build!"));
  return NS_ERROR_GFX_PRINTING_NOT_IMPLEMENTED;
#endif /* NS_PRINTING */
}

/** ---------------------------------------------------
 *  See documentation above in the nsIContentViewerfile class definition
 *	@update 11/01/01 rods
 *
 *  For a full and detailed understanding of the issues with
 *  PrintPreview: See the design spec that is attached to Bug 107562
 */
NS_IMETHODIMP
DocumentViewerImpl::PrintPreview(nsIPrintSettings* aPrintSettings, 
                                 nsIDOMWindow *aChildDOMWin, 
                                 nsIWebProgressListener* aWebProgressListener)
{
#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
  nsresult rv = NS_OK;

  if (GetIsPrinting()) {
    nsPrintEngine::CloseProgressDialog(aWebProgressListener);
    return NS_ERROR_FAILURE;
  }

  if (!mPrintEngine) {
    mPrintEngine = new nsPrintEngine();
    NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_OUT_OF_MEMORY);
    NS_ADDREF(mPrintEngine);

#ifdef NS_DEBUG
    mPrintEngine->Initialize(this, this, mContainer, mDocument, 
                             mDeviceContext, mPresContext, mWindow, mParentWidget, mDebugFile);
#else
    mPrintEngine->Initialize(this, this, mContainer, mDocument, 
                             mDeviceContext, mPresContext, mWindow, mParentWidget, nsnull);
#endif
  }

  rv = mPrintEngine->PrintPreview(aPrintSettings, aChildDOMWin, aWebProgressListener);
  if (NS_FAILED(rv)) {
    OnDonePrinting();
  }
  return rv;
#else
  return NS_ERROR_FAILURE;
#endif
}

//----------------------------------------------------------------------
NS_IMETHODIMP
DocumentViewerImpl::PrintPreviewNavigate(PRInt16 aType, PRInt32 aPageNum)
{
#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
  if (GetIsPrinting()) return NS_ERROR_FAILURE;

  if (!mPrintEngine) return NS_ERROR_FAILURE;

  nsIScrollableView* scrollableView;
  mViewManager->GetRootScrollableView(&scrollableView);
  if (scrollableView == nsnull) return NS_OK;

  // Check to see if we can short circut scrolling to the top
  if (aType == nsIWebBrowserPrint::PRINTPREVIEW_HOME ||
      (aType == nsIWebBrowserPrint::PRINTPREVIEW_GOTO_PAGENUM && aPageNum == 1)) {
    scrollableView->ScrollTo(0, 0, PR_TRUE);
    return NS_OK;
  }

  // Finds the SimplePageSequencer frame
  // in PP mPrtPreview->mPrintObject->mSeqFrame is null
  nsIFrame* seqFrame  = nsnull;
  PRInt32   pageCount = 0;
  if (NS_FAILED(mPrintEngine->GetSeqFrameAndCountPages(seqFrame, pageCount))) {
    return NS_ERROR_FAILURE;
  }

  // Figure where we are currently scrolled to
  nscoord x;
  nscoord y;
  scrollableView->GetScrollPosition(x, y);

  PRInt32    pageNum = 1;
  nsIFrame * fndPageFrame  = nsnull;
  nsIFrame * currentPage   = nsnull;

  // If it is "End" then just do a "goto" to the last page
  if (aType == nsIWebBrowserPrint::PRINTPREVIEW_END) {
    aType    = nsIWebBrowserPrint::PRINTPREVIEW_GOTO_PAGENUM;
    aPageNum = pageCount;
  }

  // Now, locate the current page we are on and
  // and the page of the page number
  nscoord gap = 0;
  nsIFrame* pageFrame = seqFrame->GetFirstChild(nsnull);
  while (pageFrame != nsnull) {
    nsRect pageRect = pageFrame->GetRect();
    if (pageNum == 1) {
      gap = pageRect.y;
    }
    if (pageRect.Contains(pageRect.x, y)) {
      currentPage = pageFrame;
    }
    if (pageNum == aPageNum) {
      fndPageFrame = pageFrame;
      break;
    }
    pageNum++;
    pageFrame = pageFrame->GetNextSibling();
  }

  if (aType == nsIWebBrowserPrint::PRINTPREVIEW_PREV_PAGE) {
    if (currentPage) {
      currentPage->GetPrevInFlow(&fndPageFrame);
      if (!fndPageFrame) {
        return NS_OK;
      }
    } else {
      return NS_OK;
    }
  } else if (aType == nsIWebBrowserPrint::PRINTPREVIEW_NEXT_PAGE) {
    if (currentPage) {
      currentPage->GetNextInFlow(&fndPageFrame);
      if (!fndPageFrame) {
        return NS_OK;
      }
    } else {
      return NS_OK;
    }
  } else { // If we get here we are doing "GoTo"
    if (aPageNum < 0 || aPageNum > pageCount) {
      return NS_OK;
    }
  }

  if (fndPageFrame && scrollableView) {
    // find offset from view
    nsPoint pnt;
    nsIView * view;
    fndPageFrame->GetOffsetFromView(mPresContext, pnt, &view);

    nscoord deadSpaceGap = 0;
    nsIPageSequenceFrame * sqf;
    if (NS_SUCCEEDED(CallQueryInterface(seqFrame, &sqf))) {
      sqf->GetDeadSpaceValue(&deadSpaceGap);
    }

    // scroll so that top of page (plus the gray area) is at the top of the scroll area
    scrollableView->ScrollTo(0, fndPageFrame->GetPosition().y-deadSpaceGap, PR_TRUE);
  }
  return NS_OK;
#else
  return NS_ERROR_FAILURE;
#endif // NS_PRINT_PREVIEW

}

/* readonly attribute nsIPrintSettings globalPrintSettings; */
NS_IMETHODIMP
DocumentViewerImpl::GetGlobalPrintSettings(nsIPrintSettings * *aGlobalPrintSettings)
{
#ifdef NS_PRINTING
  NS_ENSURE_ARG_POINTER(aGlobalPrintSettings);

  nsPrintEngine printEngine;
  return printEngine.GetGlobalPrintSettings(aGlobalPrintSettings);
#else
  return NS_ERROR_FAILURE;
#endif
}

/* readonly attribute boolean doingPrint; */
NS_IMETHODIMP
DocumentViewerImpl::GetDoingPrint(PRBool *aDoingPrint)
{
#ifdef NS_PRINTING
  NS_ENSURE_ARG_POINTER(aDoingPrint);
  
  *aDoingPrint = PR_FALSE;
  if (mPrintEngine) {
    return mPrintEngine->GetDoingPrintPreview(aDoingPrint);
  } 
  return NS_OK;
#else
  return NS_ERROR_FAILURE;
#endif
}

/* readonly attribute boolean doingPrintPreview; */
NS_IMETHODIMP
DocumentViewerImpl::GetDoingPrintPreview(PRBool *aDoingPrintPreview)
{
#ifdef NS_PRINTING
  NS_ENSURE_ARG_POINTER(aDoingPrintPreview);

  *aDoingPrintPreview = PR_FALSE;
  if (mPrintEngine) {
    return mPrintEngine->GetDoingPrintPreview(aDoingPrintPreview);
  }
  return NS_OK;
#else
  return NS_ERROR_FAILURE;
#endif
}

/* readonly attribute nsIPrintSettings currentPrintSettings; */
NS_IMETHODIMP
DocumentViewerImpl::GetCurrentPrintSettings(nsIPrintSettings * *aCurrentPrintSettings)
{
#ifdef NS_PRINTING
  NS_ENSURE_ARG_POINTER(aCurrentPrintSettings);

  *aCurrentPrintSettings = nsnull;
  NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_FAILURE);

  return mPrintEngine->GetCurrentPrintSettings(aCurrentPrintSettings);
#else
  return NS_ERROR_FAILURE;
#endif
}

/* readonly attribute nsIDOMWindow currentChildDOMWindow; */
NS_IMETHODIMP 
DocumentViewerImpl::GetCurrentChildDOMWindow(nsIDOMWindow * *aCurrentChildDOMWindow)
{
  NS_ENSURE_ARG_POINTER(aCurrentChildDOMWindow);
  *aCurrentChildDOMWindow = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void cancel (); */
NS_IMETHODIMP
DocumentViewerImpl::Cancel()
{
#ifdef NS_PRINTING
  NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_FAILURE);
  return mPrintEngine->Cancelled();
#else
  return NS_ERROR_FAILURE;
#endif
}

/* void exitPrintPreview (); */
NS_IMETHODIMP
DocumentViewerImpl::ExitPrintPreview()
{
#ifdef NS_PRINTING
  if (GetIsPrinting()) return NS_ERROR_FAILURE;
  NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_FAILURE);

  if (GetIsPrintPreview()) {
    ReturnToGalleyPresentation();
  }
  return NS_OK;
#else
  return NS_ERROR_FAILURE;
#endif
}

//----------------------------------------------------------------------------------
// Enumerate all the documents for their titles
NS_IMETHODIMP
DocumentViewerImpl::EnumerateDocumentNames(PRUint32* aCount,
                                           PRUnichar*** aResult)
{
#ifdef NS_PRINTING
  NS_ENSURE_ARG(aCount);
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_FAILURE);

  return mPrintEngine->EnumerateDocumentNames(aCount, aResult);
#else
  return NS_ERROR_FAILURE;
#endif
}

/* readonly attribute boolean isFramesetFrameSelected; */
NS_IMETHODIMP 
DocumentViewerImpl::GetIsFramesetFrameSelected(PRBool *aIsFramesetFrameSelected)
{
#ifdef NS_PRINTING
  *aIsFramesetFrameSelected = PR_FALSE;
  NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_FAILURE);

  return mPrintEngine->GetIsFramesetFrameSelected(aIsFramesetFrameSelected);
#else
  return NS_ERROR_FAILURE;
#endif
}

/* readonly attribute long printPreviewNumPages; */
NS_IMETHODIMP
DocumentViewerImpl::GetPrintPreviewNumPages(PRInt32 *aPrintPreviewNumPages)
{
#ifdef NS_PRINTING
  NS_ENSURE_ARG_POINTER(aPrintPreviewNumPages);
  NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_FAILURE);

  return mPrintEngine->GetPrintPreviewNumPages(aPrintPreviewNumPages);
#else
  return NS_ERROR_FAILURE;
#endif
}

/* readonly attribute boolean isFramesetDocument; */
NS_IMETHODIMP
DocumentViewerImpl::GetIsFramesetDocument(PRBool *aIsFramesetDocument)
{
#ifdef NS_PRINTING
  *aIsFramesetDocument = PR_FALSE;
  NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_FAILURE);

  return mPrintEngine->GetIsFramesetDocument(aIsFramesetDocument);
#else
  return NS_ERROR_FAILURE;
#endif
}

/* readonly attribute boolean isIFrameSelected; */
NS_IMETHODIMP 
DocumentViewerImpl::GetIsIFrameSelected(PRBool *aIsIFrameSelected)
{
#ifdef NS_PRINTING
  *aIsIFrameSelected = PR_FALSE;
  NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_FAILURE);

  return mPrintEngine->GetIsIFrameSelected(aIsIFrameSelected);
#else
  return NS_ERROR_FAILURE;
#endif
}

/* readonly attribute boolean isRangeSelection; */
NS_IMETHODIMP 
DocumentViewerImpl::GetIsRangeSelection(PRBool *aIsRangeSelection)
{
#ifdef NS_PRINTING
  *aIsRangeSelection = PR_FALSE;
  NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_FAILURE);

  return mPrintEngine->GetIsRangeSelection(aIsRangeSelection);
#else
  return NS_ERROR_FAILURE;
#endif
}


#ifdef NS_PRINTING
//----------------------------------------------------------------------------------
// Printing/Print Preview Helpers
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Walks the document tree and tells each DocShell whether Printing/PP is happening
void 
DocumentViewerImpl::SetIsPrintingInDocShellTree(nsIDocShellTreeNode* aParentNode, 
                                                PRBool               aIsPrintingOrPP, 
                                                PRBool               aStartAtTop)
{
  NS_ASSERTION(aParentNode, "Parent can't be NULL!");

  nsCOMPtr<nsIDocShellTreeItem> parentItem(do_QueryInterface(aParentNode));

  // find top of "same parent" tree
  if (aStartAtTop) {
    while (parentItem) {
      nsCOMPtr<nsIDocShellTreeItem> parent;
      parentItem->GetSameTypeParent(getter_AddRefs(parent));
      if (!parent) {
        break;
      }
      parentItem = do_QueryInterface(parent);
    }
  }
  NS_ASSERTION(parentItem, "parentItem can't be null");

  // Check to see if the DocShell's ContentViewer is printing/PP
  nsCOMPtr<nsIContentViewerContainer> viewerContainer(do_QueryInterface(parentItem));
  if (viewerContainer) {
    viewerContainer->SetIsPrinting(aIsPrintingOrPP);
  }

  // Traverse children to see if any of them are printing.
  PRInt32 n;
  aParentNode->GetChildCount(&n);
  for (PRInt32 i=0; i < n; i++) {
    nsCOMPtr<nsIDocShellTreeItem> child;
    aParentNode->GetChildAt(i, getter_AddRefs(child));
    nsCOMPtr<nsIDocShellTreeNode> childAsNode(do_QueryInterface(child));
    NS_ASSERTION(childAsNode, "child isn't nsIDocShellTreeNode");
    if (childAsNode) {
      SetIsPrintingInDocShellTree(childAsNode, aIsPrintingOrPP, PR_FALSE);
    }
  }

}
#endif // NS_PRINTING

//------------------------------------------------------------
PRBool
DocumentViewerImpl::GetIsPrinting()
{
#ifdef NS_PRINTING
  if (mPrintEngine) {
    return mPrintEngine->GetIsPrinting();
  }
#endif
  return PR_FALSE; 
}

//------------------------------------------------------------
// Notification from the PrintEngine of the current Printing status
void
DocumentViewerImpl::SetIsPrinting(PRBool aIsPrinting)
{
#ifdef NS_PRINTING
  // Set all the docShells in the docshell tree to be printing.
  // that way if anyone of them tries to "navigate" it can't
  if (mContainer) {
    nsCOMPtr<nsIDocShellTreeNode> docShellTreeNode(do_QueryInterface(mContainer));
    NS_ASSERTION(docShellTreeNode, "mContainer has to be a nsIDocShellTreeNode");
    SetIsPrintingInDocShellTree(docShellTreeNode, aIsPrinting, PR_TRUE);
  }
#endif
}

//------------------------------------------------------------
// The PrintEngine holds the current value
// this called from inside the DocViewer
PRBool
DocumentViewerImpl::GetIsPrintPreview()
{
#ifdef NS_PRINTING
  if (mPrintEngine) {
    return mPrintEngine->GetIsPrintPreview();
  }
#endif
  return PR_FALSE; 
}

//------------------------------------------------------------
// Notification from the PrintEngine of the current PP status
void
DocumentViewerImpl::SetIsPrintPreview(PRBool aIsPrintPreview)
{
#ifdef NS_PRINTING
  // Set all the docShells in the docshell tree to be printing.
  // that way if anyone of them tries to "navigate" it can't
  if (mContainer) {
    nsCOMPtr<nsIDocShellTreeNode> docShellTreeNode(do_QueryInterface(mContainer));
    NS_ASSERTION(docShellTreeNode, "mContainer has to be a nsIDocShellTreeNode");
    SetIsPrintingInDocShellTree(docShellTreeNode, aIsPrintPreview, PR_TRUE);
  }
#endif
}

//------------------------------------------------------------
// The PrintEngine holds the current value
// this called from inside the DocViewer
PRBool
DocumentViewerImpl::GetIsCreatingPrintPreview()
{
#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
  if (mPrintEngine) {
    return mPrintEngine->GetIsCreatingPrintPreview();
  }
#endif
  return PR_FALSE; 
}

//----------------------------------------------------------------------------------
// nsIDocumentViewerPrint IFace
//----------------------------------------------------------------------------------

//------------------------------------------------------------
void
DocumentViewerImpl::IncrementDestroyRefCount()
{
  ++mDestroyRefCount;
}

//------------------------------------------------------------
void
DocumentViewerImpl::ReturnToGalleyPresentation()
{
#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
  if (!GetIsPrintPreview()) {
    NS_ASSERTION(0, "Wow, we should never get here!");
    return;
  }

  // Get the current size of what is being viewed
  nsRect bounds;
  mWindow->GetBounds(bounds);

  // In case we have focus focus the parent DocShell
  // which in this case should always be chrome
  nsCOMPtr<nsIDocShellTreeItem>  dstParentItem;
  nsCOMPtr<nsIDocShellTreeItem>  dstItem(do_QueryInterface(mContainer));
  if (dstItem) {
    dstItem->GetParent(getter_AddRefs(dstParentItem));
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(dstParentItem));
    if (docShell) {
      docShell->SetHasFocus(PR_TRUE);
    }
  }

  // Start to kill off the old Presentation
  // by cleaning up the PresShell
  if (mPresShell) {
    // Break circular reference (or something)
    mPresShell->EndObservingDocument();
    nsCOMPtr<nsISelection> selection;
    nsresult rv = GetDocumentSelection(getter_AddRefs(selection));
    nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(selection));
    if (NS_SUCCEEDED(rv) && selPrivate && mSelectionListener)
      selPrivate->RemoveSelectionListener(mSelectionListener);
    mPresShell->Destroy();
  }

  // clear weak references before we go away
  if (mPresContext) {
    mPresContext->SetContainer(nsnull);
    mPresContext->SetLinkHandler(nsnull);
  }

  //------------------------------------------------
  // NOTE:
  // Here is why the code below is a little confusing:
  //   1) Scripting needs to be turned back on before 
  //      the print engine is destroyed
  //   2) The PrintEngine must be destroyed BEFORE 
  //      calling InitInternal when caching documents (framesets)
  //      BUT the PrintEngine must be destroyed AFTER 
  //      calling InitInternal when NOT caching documents (no framesets)
  //------------------------------------------------

  // wasCached will be used below to indicate whether the 
  // InitInternal should create all new objects or just
  // initialize the existing ones
  PRBool wasCached = PR_FALSE;

  if (mPrintEngine && mPrintEngine->HasCachedPres()) {

    mPrintEngine->GetCachedPresentation(mPresShell, mPresContext, mViewManager, mWindow);

    // Tell the "real" presshell to start observing the document
    // again.
    mPresShell->BeginObservingDocument();

    mWindow->Show(PR_TRUE);

    wasCached = PR_TRUE;
  } else {
    // Destroy the old Presentation
    mPresShell    = nsnull;
    mPresContext  = nsnull;
    mViewManager  = nsnull;
    mWindow       = nsnull;
  }

  if (mPrintEngine) {
    // Very important! Turn On scripting
    mPrintEngine->TurnScriptingOn(PR_TRUE);

    if (wasCached) {
      mPrintEngine->Destroy();
      NS_RELEASE(mPrintEngine);
    }
  }

  InitInternal(mParentWidget, mDeviceContext, bounds, !wasCached, PR_TRUE);

  if (mPrintEngine && !wasCached) {
    mPrintEngine->Destroy();
    NS_RELEASE(mPrintEngine);
  }

  // this needs to be set here not earlier,
  // because it is needing when re-constructing the Galley Mode)
  SetIsPrintPreview(PR_FALSE);

  mViewManager->EnableRefresh(NS_VMREFRESH_NO_SYNC);

  Show();

#endif // NS_PRINTING && NS_PRINT_PREVIEW
}

//------------------------------------------------------------
void
DocumentViewerImpl::InstallNewPresentation()
{
#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
  // Get the current size of what is being viewed
  nsRect bounds;
  mWindow->GetBounds(bounds);

  // In case we have focus focus the parent DocShell
  // which in this case should always be chrome
  nsCOMPtr<nsIDocShellTreeItem>  dstParentItem;
  nsCOMPtr<nsIDocShellTreeItem>  dstItem(do_QueryInterface(mContainer));
  if (dstItem) {
    dstItem->GetParent(getter_AddRefs(dstParentItem));
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(dstParentItem));
    if (docShell) {
      docShell->SetHasFocus(PR_TRUE);
    }
  }

  // turn off selection painting
  nsCOMPtr<nsISelectionController> selectionController =
    do_QueryInterface(mPresShell);
  if (selectionController) {
    selectionController->SetDisplaySelection(nsISelectionController::SELECTION_OFF);
  }

  // Start to kill off the old Presentation
  // by cleaning up the PresShell
  if (mPresShell) {
    // Break circular reference (or something)
    mPresShell->EndObservingDocument();
    nsCOMPtr<nsISelection> selection;
    nsresult rv = GetDocumentSelection(getter_AddRefs(selection));
    nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(selection));
    if (NS_SUCCEEDED(rv) && selPrivate && mSelectionListener)
      selPrivate->RemoveSelectionListener(mSelectionListener);

    // We need to destroy the PreShell if there is an existing PP
    // or we are not caching the original Presentation
    if (!mPrintEngine->IsCachingPres() || mPrintEngine->IsOldPrintPreviewPres()) {
      mPresShell->Destroy();
    }
  }

  // clear weak references before we go away
  if (mPresContext) {
    mPresContext->SetContainer(nsnull);
    mPresContext->SetLinkHandler(nsnull);
  }

  // See if we are suppose to be caching the old Presentation
  // and then check to see if we already have.
  if (mPrintEngine->IsCachingPres() && !mPrintEngine->HasCachedPres()) {
    // Cach old presentation
    mPrintEngine->CachePresentation(mPresShell, mPresContext, mViewManager, mWindow);
    mWindow->Show(PR_FALSE);
  } else {
    // Destroy the old Presentation
    mPresShell    = nsnull;
    mPresContext  = nsnull;
    mViewManager  = nsnull;
    mWindow       = nsnull;
  }

  mPrintEngine->InstallPrintPreviewListener();

  mPrintEngine->GetNewPresentation(mPresShell, mPresContext, mViewManager, mWindow);

  mPresShell->BeginObservingDocument();

  nscoord width  = bounds.width;
  nscoord height = bounds.height;
  float p2t;
  p2t = mPresContext->PixelsToTwips();
  width = NSIntPixelsToTwips(width, p2t);
  height = NSIntPixelsToTwips(height, p2t);
  mViewManager->DisableRefresh();
  mViewManager->SetWindowDimensions(width, height);

  mDeviceContext->SetUseAltDC(kUseAltDCFor_FONTMETRICS, PR_FALSE);
  mDeviceContext->SetUseAltDC(kUseAltDCFor_CREATERC_PAINT, PR_TRUE);

  mViewManager->EnableRefresh(NS_VMREFRESH_NO_SYNC);

  Show();

  mPrintEngine->ShowDocList(PR_TRUE);
#endif // NS_PRINTING && NS_PRINT_PREVIEW
}

//------------------------------------------------------------
// This called ONLY when printing has completed and the DV
// is being notified that it should get rid of the PrintEngine.
//
// BUT, if we are in Print Preview then we want to ignore the 
// notification (we do not get rid of the PrintEngine)
// 
// One small caveat: 
//   This IS called from two places in this module for cleaning
//   up when an error occurred during the start up printing 
//   and print preview
//
void
DocumentViewerImpl::OnDonePrinting() 
{
#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
  if (mPrintEngine) {
    if (GetIsPrintPreview()) {
      mPrintEngine->DestroyPrintingData();
    } else {
      mPrintEngine->Destroy();
      NS_RELEASE(mPrintEngine);
    }

    // We are done printing, now cleanup 
    if (mDeferredWindowClose) {
      mDeferredWindowClose = PR_FALSE;
      nsCOMPtr<nsIDOMWindowInternal> win = do_GetInterface(mContainer);
      if (win)
        win->Close();
    } else if (mClosingWhilePrinting) {
      if (mDocument) {
        mDocument->SetScriptGlobalObject(nsnull);
        mDocument = nsnull;
      }
      mClosingWhilePrinting = PR_FALSE;
      NS_RELEASE_THIS();
    }
  }
#endif // NS_PRINTING && NS_PRINT_PREVIEW
}
